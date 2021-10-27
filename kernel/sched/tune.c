#include <linux/cgroup.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/percpu.h>
#include <linux/printk.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/ems.h>
#include <linux/ems_service.h>

#include <trace/events/sched.h>
#include <linux/list.h>

#include "sched.h"
#include "tune.h"

bool schedtune_initialized = false;

extern struct reciprocal_value schedtune_spc_rdiv;

#ifdef CONFIG_DYNAMIC_STUNE_BOOST
#define DYNAMIC_BOOST_SLOTS_COUNT 5
static DEFINE_MUTEX(boost_slot_mutex);
static DEFINE_MUTEX(stune_boost_mutex);
struct boost_slot {
	struct list_head list;
	int idx;
};
#endif /* CONFIG_DYNAMIC_STUNE_BOOST */

/* We hold schedtune boost in effect for at least this long */
#define SCHEDTUNE_BOOST_HOLD_NS 50000000ULL

/*
 * EAS scheduler tunables for task groups.

 */

/* SchdTune tunables for a group of tasks */
struct schedtune {
	/* SchedTune CGroup subsystem */
	struct cgroup_subsys_state css;

	/* Boost group allocated ID */
	int idx;

	/* Boost value for tasks on that SchedTune CGroup */
	int boost;

	/* Hint to bias scheduling of tasks on that SchedTune CGroup
	 * towards idle CPUs */
	int prefer_idle;

	/* Hint to bias scheduling of tasks on that SchedTune CGroup
	 * towards high performance CPUs */
	int prefer_perf;

	/* SchedTune util-est */
	int util_est_en;

	/* Hint to group tasks by process */
	int band;

	/* SchedTune ontime migration */
	int ontime_en;
#ifdef CONFIG_DYNAMIC_STUNE_BOOST
	/*
	 * This tracks the default boost value and is used to restore
	 * the value when Dynamic SchedTune Boost is reset.
	 */
	int boost_default;

	/* Sched Boost value for tasks on that SchedTune CGroup */
	int sched_boost;

	/* Number of ongoing boosts for this SchedTune CGroup */
	int boost_count;

	/* Lists of active and available boost slots */
	struct boost_slot active_boost_slots;
	struct boost_slot available_boost_slots;

	/* Array of tracked boost values of each slot */
	int slot_boost[DYNAMIC_BOOST_SLOTS_COUNT];
#endif /* CONFIG_DYNAMIC_STUNE_BOOST */
};

static inline struct schedtune *css_st(struct cgroup_subsys_state *css)
{
	return css ? container_of(css, struct schedtune, css) : NULL;
}

static inline struct schedtune *task_schedtune(struct task_struct *tsk)
{
	return css_st(task_css(tsk, schedtune_cgrp_id));
}

static inline struct schedtune *parent_st(struct schedtune *st)
{
	return css_st(st->css.parent);
}

/*
 * SchedTune root control group
 * The root control group is used to defined a system-wide boosting tuning,
 * which is applied to all tasks in the system.
 * Task specific boost tuning could be specified by creating and
 * configuring a child control group under the root one.
 * By default, system-wide boosting is disabled, i.e. no boosting is applied
 * to tasks which are not into a child control group.
 */
static struct schedtune
root_schedtune = {
	.boost	= 0,


	.prefer_idle = 0,
	.prefer_perf = 0,
	.band = 0,
#ifdef CONFIG_DYNAMIC_STUNE_BOOST
	.boost_default = 0,
	.sched_boost = 0,
	.boost_count = 0,
	.active_boost_slots = {
		.list = LIST_HEAD_INIT(root_schedtune.active_boost_slots.list),
		.idx = 0,
	},
	.available_boost_slots = {
		.list = LIST_HEAD_INIT(root_schedtune.available_boost_slots.list),
		.idx = 0,
	},
	.slot_boost = {0},
#endif /* CONFIG_DYNAMIC_STUNE_BOOST */


};

/*
 * Maximum number of boost groups to support
 * When per-task boosting is used we still allow only limited number of
 * boost groups for two main reasons:
 * 1. on a real system we usually have only few classes of workloads which
 *    make sense to boost with different values (e.g. background vs foreground
 *    tasks, interactive vs low-priority tasks)
 * 2. a limited number allows for a simpler and more memory/time efficient
 *    implementation especially for the computation of the per-CPU boost
 *    value
 */
#define BOOSTGROUPS_COUNT 5



/* Array of configured boostgroups */
static struct schedtune *allocated_group[BOOSTGROUPS_COUNT] = {
	&root_schedtune,
	NULL,
};

/* SchedTune boost groups
 * Keep track of all the boost groups which impact on CPU, for example when a
 * CPU has two RUNNABLE tasks belonging to two different boost groups and thus
 * likely with different boost values.
 * Since on each system we expect only a limited number of boost groups, here
 * we use a simple array to keep track of the metrics required to compute the
 * maximum per-CPU boosting value.
 */
struct boost_groups {
	/* Maximum boost value for all RUNNABLE tasks on a CPU */
	bool idle;
	int boost_max;
	u64 boost_ts;
	struct {
		/* The boost for tasks on that boost group */
		int boost;
		/* Count of RUNNABLE tasks on that boost group */
		unsigned tasks;
		/* Timestamp of boost activation */
		u64 ts;
	} group[BOOSTGROUPS_COUNT];
	/* CPU's boost group locking */
	raw_spinlock_t lock;
};

/* Boost groups affecting each CPU in the system */
DEFINE_PER_CPU(struct boost_groups, cpu_boost_groups);

static inline bool schedtune_boost_timeout(u64 now, u64 ts)
{
	return ((now - ts) > SCHEDTUNE_BOOST_HOLD_NS);
}

static inline bool
schedtune_boost_group_active(int idx, struct boost_groups* bg, u64 now)
{
	if (bg->group[idx].tasks)
		return true;

	return !schedtune_boost_timeout(now, bg->group[idx].ts);
}

static void
schedtune_cpu_update(int cpu, u64 now)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	int boost_max;
	u64 boost_ts;
	int idx;



	/* The root boost group is always active */
	boost_max = bg->group[0].boost;
	boost_ts = now;
	for (idx = 1; idx < BOOSTGROUPS_COUNT; ++idx) {


		/*
		 * A boost group affects a CPU only if it has
		 * RUNNABLE tasks on that CPU or it has hold
		 * in effect from a previous task.
		 */
		if (!schedtune_boost_group_active(idx, bg, now))
			continue;

		/* This boost group is active */
		if (boost_max > bg->group[idx].boost)
			continue;

		boost_max = bg->group[idx].boost;
		boost_ts =  bg->group[idx].ts;
	}
	/* Ensures boost_max is non-negative when all cgroup boost values
	 * are neagtive. Avoids under-accounting of cpu capacity which may cause
	 * task stacking and frequency spikes.*/
	boost_max = max(boost_max, 0);
	bg->boost_max = boost_max;
	bg->boost_ts = boost_ts;
}

static int
schedtune_boostgroup_update(int idx, int boost)
{
	struct boost_groups *bg;
	int cur_boost_max;
	int old_boost;
	int cpu;
	u64 now;

	/* Update per CPU boost groups */
	for_each_possible_cpu(cpu) {
		bg = &per_cpu(cpu_boost_groups, cpu);



		/*
		 * Keep track of current boost values to compute the per CPU
		 * maximum only when it has been affected by the new value of
		 * the updated boost group
		 */
		cur_boost_max = bg->boost_max;
		old_boost = bg->group[idx].boost;

		/* Update the boost value of this boost group */
		bg->group[idx].boost = boost;

		/* Check if this update increase current max */
		now = sched_clock_cpu(cpu);
		if (boost > cur_boost_max &&
			schedtune_boost_group_active(idx, bg, now)) {
			bg->boost_max = boost;
			bg->boost_ts = bg->group[idx].ts;

			trace_sched_tune_boostgroup_update(cpu, 1, bg->boost_max);
			continue;
		}

		/* Check if this update has decreased current max */
		if (cur_boost_max == old_boost && old_boost > boost) {
			schedtune_cpu_update(cpu, now);
			trace_sched_tune_boostgroup_update(cpu, -1, bg->boost_max);
			continue;
		}

		trace_sched_tune_boostgroup_update(cpu, 0, bg->boost_max);
	}

	return 0;
}

#define ENQUEUE_TASK  1
#define DEQUEUE_TASK -1

static inline bool
schedtune_update_timestamp(struct task_struct *p)
{
	if (sched_feat(SCHEDTUNE_BOOST_HOLD_ALL))
		return true;

	return task_has_rt_policy(p);
}

static inline void
schedtune_tasks_update(struct task_struct *p, int cpu, int idx, int task_count)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	int tasks = bg->group[idx].tasks + task_count;

	/* Update boosted tasks count while avoiding to make it negative */
	bg->group[idx].tasks = max(0, tasks);

	/* Update timeout on enqueue */
	if (task_count > 0) {
		u64 now = sched_clock_cpu(cpu);

		if (schedtune_update_timestamp(p))
			bg->group[idx].ts = now;

		/* Boost group activation or deactivation on that RQ */
		if (bg->group[idx].tasks == 1)
			schedtune_cpu_update(cpu, now);
	}

	trace_sched_tune_tasks_update(p, cpu, tasks, idx,
			bg->group[idx].boost, bg->boost_max,
			bg->group[idx].ts);
}

/*
 * NOTE: This function must be called while holding the lock on the CPU RQ
 */
void schedtune_enqueue_task(struct task_struct *p, int cpu)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	unsigned long irq_flags;
	struct schedtune *st;
	int idx;

	if (unlikely(!schedtune_initialized))
		return;



	/*
	 * Boost group accouting is protected by a per-cpu lock and requires
	 * interrupt to be disabled to avoid race conditions for example on
	 * do_exit()::cgroup_exit() and task migration.
	 */
	raw_spin_lock_irqsave(&bg->lock, irq_flags);
	rcu_read_lock();

	st = task_schedtune(p);
	idx = st->idx;

	schedtune_tasks_update(p, cpu, idx, ENQUEUE_TASK);

	rcu_read_unlock();
	raw_spin_unlock_irqrestore(&bg->lock, irq_flags);
}

int schedtune_can_attach(struct cgroup_taskset *tset)
{
	struct task_struct *task;
	struct cgroup_subsys_state *css;
	struct boost_groups *bg;
	struct rq_flags rq_flags;
	unsigned int cpu;
	struct rq *rq;
	int src_bg; /* Source boost group index */
	int dst_bg; /* Destination boost group index */
	int tasks;
	u64 now;

	if (unlikely(!schedtune_initialized))
		return 0;


	cgroup_taskset_for_each(task, css, tset) {

		/*
		 * Lock the CPU's RQ the task is enqueued to avoid race
		 * conditions with migration code while the task is being
		 * accounted
		 */
		rq = task_rq_lock(task, &rq_flags);

		if (!task->on_rq) {
			task_rq_unlock(rq, task, &rq_flags);
			continue;
		}

		/*
		 * Boost group accouting is protected by a per-cpu lock and requires
		 * interrupt to be disabled to avoid race conditions on...
		 */
		cpu = cpu_of(rq);
		bg = &per_cpu(cpu_boost_groups, cpu);
		raw_spin_lock(&bg->lock);

		dst_bg = css_st(css)->idx;
		src_bg = task_schedtune(task)->idx;

		/*
		 * Current task is not changing boostgroup, which can
		 * happen when the new hierarchy is in use.
		 */
		if (unlikely(dst_bg == src_bg)) {
			raw_spin_unlock(&bg->lock);
			task_rq_unlock(rq, task, &rq_flags);
			continue;
		}

		/*
		 * This is the case of a RUNNABLE task which is switching its
		 * current boost group.
		 */

		/* Move task from src to dst boost group */
		tasks = bg->group[src_bg].tasks - 1;
		bg->group[src_bg].tasks = max(0, tasks);
		bg->group[dst_bg].tasks += 1;

		/* Update boost hold start for this group */
		now = sched_clock_cpu(cpu);
		bg->group[dst_bg].ts = now;

		/* Force boost group re-evaluation at next boost check */
		bg->boost_ts = now - SCHEDTUNE_BOOST_HOLD_NS;



		raw_spin_unlock(&bg->lock);
		task_rq_unlock(rq, task, &rq_flags);
	}

	return 0;
}

void schedtune_cancel_attach(struct cgroup_taskset *tset)
{
	/* This can happen only if SchedTune controller is mounted with
	 * other hierarchies ane one of them fails. Since usually SchedTune is
	 * mouted on its own hierarcy, for the time being we do not implement
	 * a proper rollback mechanism */
	WARN(1, "SchedTune cancel attach not implemented");
}

static void schedtune_attach(struct cgroup_taskset *tset)
{
	struct task_struct *task;
	struct cgroup_subsys_state *css;

	cgroup_taskset_for_each(task, css, tset)
		sync_band(task, css_st(css)->band);
}

/*
 * NOTE: This function must be called while holding the lock on the CPU RQ
 */
void schedtune_dequeue_task(struct task_struct *p, int cpu)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	unsigned long irq_flags;
	struct schedtune *st;
	int idx;

	if (unlikely(!schedtune_initialized))
		return;

	/*
	 * Boost group accouting is protected by a per-cpu lock and requires
	 * interrupt to be disabled to avoid race conditions on...
	 */
	raw_spin_lock_irqsave(&bg->lock, irq_flags);
	rcu_read_lock();

	st = task_schedtune(p);
	idx = st->idx;

	schedtune_tasks_update(p, cpu, idx, DEQUEUE_TASK);

	rcu_read_unlock();
	raw_spin_unlock_irqrestore(&bg->lock, irq_flags);
}

int schedtune_cpu_boost(int cpu)
{
	struct boost_groups *bg;
	u64 now;

	bg = &per_cpu(cpu_boost_groups, cpu);
	now = sched_clock_cpu(cpu);

	/* Check to see if we have a hold in effect */
	if (schedtune_boost_timeout(now, bg->boost_ts))
		schedtune_cpu_update(cpu, now);

	return bg->boost_max;
}

static inline int schedtune_adj_ta(struct task_struct *p)
{
	struct schedtune *st;
	char name_buf[NAME_MAX + 1];
	int adj = p->signal->oom_score_adj;

	/* We only care about adj == 0 */
	if (adj != 0)
		return 0;

	/* Don't touch kthreads */
	if (p->flags & PF_KTHREAD)
		return 0;

	st = task_schedtune(p);
	cgroup_name(st->css.cgroup, name_buf, sizeof(name_buf));
	if (!strncmp(name_buf, "top-app", strlen("top-app"))) {
		pr_debug("top app is %s with adj %i\n", p->comm, adj);
		return 1;
	}

	return 0;
}

int schedtune_task_boost(struct task_struct *p)
{
	struct schedtune *st;
	int task_boost;

	if (unlikely(!schedtune_initialized))
		return 0;

	/* Get task boost value */
	rcu_read_lock();
	st = task_schedtune(p);
	task_boost = max(st->boost, schedtune_adj_ta(p));
	rcu_read_unlock();

	return task_boost;
}

int schedtune_util_est_en(struct task_struct *p)
{
	struct schedtune *st;
	int util_est_en;

	if (unlikely(!schedtune_initialized))
		return 0;

	/* Get util_est value */
	rcu_read_lock();
	st = task_schedtune(p);
	util_est_en = st->util_est_en;
	rcu_read_unlock();

	return util_est_en;
}

int schedtune_ontime_en(struct task_struct *p)
{
	struct schedtune *st;
	int ontime_en;

	if (unlikely(!schedtune_initialized))
		return 0;

	/* Get ontime value */
	rcu_read_lock();
	st = task_schedtune(p);
	ontime_en = st->ontime_en;
	rcu_read_unlock();

	return ontime_en;

}

int schedtune_prefer_idle(struct task_struct *p)
{
	struct schedtune *st;
	int prefer_idle;

	if (unlikely(!schedtune_initialized))
		return 0;

	/* Get prefer_idle value */
	rcu_read_lock();
	st = task_schedtune(p);
	prefer_idle = st->prefer_idle;
	rcu_read_unlock();

	return prefer_idle;
}

int schedtune_prefer_perf(struct task_struct *p)
{
	struct schedtune *st;
	int prefer_perf;

	if (unlikely(!schedtune_initialized))
		return 0;

	/* Get prefer_perf value */
	rcu_read_lock();
	st = task_schedtune(p);
	prefer_perf = max(st->prefer_perf, kpp_status(st->idx));
	rcu_read_unlock();

	return prefer_perf;
}

static u64
util_est_en_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	struct schedtune *st = css_st(css);

	return st->util_est_en;
}

static int
util_est_en_write(struct cgroup_subsys_state *css, struct cftype *cft,
	    u64 util_est_en)
{
	struct schedtune *st = css_st(css);
	st->util_est_en = util_est_en;

	return 0;
}

static u64
ontime_en_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	struct schedtune *st = css_st(css);

	return st->ontime_en;
}

static int
ontime_en_write(struct cgroup_subsys_state *css, struct cftype *cft,
		u64 ontime_en)
{
	struct schedtune *st = css_st(css);
	st->ontime_en = ontime_en;

	return 0;
}

static u64
band_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	struct schedtune *st = css_st(css);

	return st->band;
}

static int
band_write(struct cgroup_subsys_state *css, struct cftype *cft,
	    u64 band)
{
	struct schedtune *st = css_st(css);


	st->band = band;


	return 0;
}

static u64
prefer_idle_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	struct schedtune *st = css_st(css);

	return st->prefer_idle;
}

static int
prefer_idle_write(struct cgroup_subsys_state *css, struct cftype *cft,
	    u64 prefer_idle)
{
	struct schedtune *st = css_st(css);
	st->prefer_idle = !!prefer_idle;

	return 0;
}

static u64
prefer_perf_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	struct schedtune *st = css_st(css);

	return st->prefer_perf;
}

static int
prefer_perf_write(struct cgroup_subsys_state *css, struct cftype *cft,
	    u64 prefer_perf)
{
	struct schedtune *st = css_st(css);
	st->prefer_perf = prefer_perf;

	return 0;
}

static s64
boost_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	struct schedtune *st = css_st(css);

	return st->boost;
}

static int
boost_write(struct cgroup_subsys_state *css, struct cftype *cft,
	    s64 boost)
{
	struct schedtune *st = css_st(css);



	if (boost < -100 || boost > 100)
		return -EINVAL;




	st->boost = boost;
#ifdef CONFIG_DYNAMIC_STUNE_BOOST
	st->boost_default = boost;
#endif /* CONFIG_DYNAMIC_STUNE_BOOST */


	/* Update CPU boost */
	schedtune_boostgroup_update(st->idx, st->boost);



	return 0;
}


#ifdef CONFIG_STUNE_ASSIST
static int boost_write_wrapper(struct cgroup_subsys_state *css,
			       struct cftype *cft, s64 boost)
{
	if (!strcmp(current->comm, "init"))
		return 0;

	return boost_write(css, cft, boost);
}

static int prefer_idle_write_wrapper(struct cgroup_subsys_state *css,
				     struct cftype *cft, u64 prefer_idle)
{
	if (!strcmp(current->comm, "init"))
		return 0;

	return prefer_idle_write(css, cft, prefer_idle);
}
#endif

#ifdef CONFIG_DYNAMIC_STUNE_BOOST
static s64
sched_boost_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	struct schedtune *st = css_st(css);

	return st->sched_boost;
}

static int
sched_boost_write(struct cgroup_subsys_state *css, struct cftype *cft,
	    s64 sched_boost)
{
	struct schedtune *st = css_st(css);
	st->sched_boost = sched_boost;

	return 0;
}

static void
boost_slots_init(struct schedtune *st)
{
	int i;
	struct boost_slot *slot;

	/* Initialize boost slots */
	INIT_LIST_HEAD(&st->active_boost_slots.list);
	INIT_LIST_HEAD(&st->available_boost_slots.list);

	/* Populate available_boost_slots */
	for (i = 0; i < DYNAMIC_BOOST_SLOTS_COUNT; ++i) {
		slot = kmalloc(sizeof(*slot), GFP_KERNEL);
		slot->idx = i;
		list_add_tail(&slot->list, &st->available_boost_slots.list);
	}
}

static void
boost_slots_release(struct schedtune *st)
{
	struct boost_slot *slot, *next_slot;

	list_for_each_entry_safe(slot, next_slot,
				 &st->available_boost_slots.list, list) {
		list_del(&slot->list);
		kfree(slot);
	}
	list_for_each_entry_safe(slot, next_slot,
				 &st->active_boost_slots.list, list) {
		list_del(&slot->list);
		kfree(slot);
	}
}
#endif // CONFIG_DYNAMIC_STUNE_BOOST

static struct cftype files[] = {
	{
		.name = "boost",
		.read_s64 = boost_read,
		.write_s64 = boost_write_wrapper,
	},
	{
		.name = "prefer_idle",
		.read_u64 = prefer_idle_read,
		.write_u64 = prefer_idle_write_wrapper,
	},
	{
		.name = "prefer_perf",
		.read_u64 = prefer_perf_read,
		.write_u64 = prefer_perf_write,
	},
	{
		.name = "band",
		.read_u64 = band_read,
		.write_u64 = band_write,
	},
	{
		.name = "util_est_en",
		.read_u64 = util_est_en_read,
		.write_u64 = util_est_en_write,
	},
	{
		.name = "ontime_en",
		.read_u64 = ontime_en_read,
		.write_u64 = ontime_en_write,
	},
#ifdef CONFIG_DYNAMIC_STUNE_BOOST
	{
		.name = "sched_boost",
		.read_s64 = sched_boost_read,
		.write_s64 = sched_boost_write,
	},
#endif // CONFIG_DYNAMIC_STUNE_BOOST
	{ }	/* terminate */
};

static int
schedtune_boostgroup_init(struct schedtune *st)
{
	struct boost_groups *bg;
	int cpu;

#ifdef CONFIG_DYNAMIC_STUNE_BOOST
	boost_slots_init(st);
#endif // CONFIG_DYNAMIC_STUNE_BOOST

	/* Keep track of allocated boost groups */
	allocated_group[st->idx] = st;

	/* Initialize the per CPU boost groups */
	for_each_possible_cpu(cpu) {
		bg = &per_cpu(cpu_boost_groups, cpu);
		bg->group[st->idx].boost = 0;
		bg->group[st->idx].tasks = 0;
		bg->group[st->idx].ts = 0;
	}

	return 0;
}

#ifdef CONFIG_STUNE_ASSIST
struct st_data {
	char *name;
	int boost;
	bool prefer_idle;
	bool colocate;
	bool no_override;
};

static void write_default_values(struct cgroup_subsys_state *css)
{
	static struct st_data st_targets[] = {
		{ "audio-app",	0, 0, 0, 0 },
		{ "background",	-25, 0, 0, 0 },
		{ "foreground",	1, 1, 0, 0 },
		{ "rt",		0, 0, 0, 0 },
		{ "top-app",	2, 1, 0, 0 },
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(st_targets); i++) {
		struct st_data tgt = st_targets[i];

		if (!strcmp(css->cgroup->kn->name, tgt.name)) {
			pr_info("stune_assist: setting values for %s: boost=%d prefer_idle=%d colocate=%d no_override=%d\n",
				tgt.name, tgt.boost, tgt.prefer_idle,
				tgt.colocate, tgt.no_override);

			boost_write(css, NULL, tgt.boost);
			prefer_idle_write(css, NULL, tgt.prefer_idle);
		}
	}
}
#endif

static struct cgroup_subsys_state *
schedtune_css_alloc(struct cgroup_subsys_state *parent_css)
{
	struct schedtune *st;
	int idx;

	if (!parent_css)
		return &root_schedtune.css;

	/* Allow only single level hierachies */
	if (parent_css != &root_schedtune.css) {
		pr_err("Nested SchedTune boosting groups not allowed\n");
		return ERR_PTR(-ENOMEM);
	}


	for (idx = 1; idx < BOOSTGROUPS_COUNT; ++idx) {
		if (!allocated_group[idx])
			break;
#ifdef CONFIG_STUNE_ASSIST
		write_default_values(&allocated_group[idx]->css);

#endif
	}
	if (idx == BOOSTGROUPS_COUNT) {
		pr_err("Trying to create more than %d SchedTune boosting groups\n",
		       BOOSTGROUPS_COUNT);
		return ERR_PTR(-ENOSPC);
	}

	st = kzalloc(sizeof(*st), GFP_KERNEL);
	if (!st)
		goto out;

	/* Initialize per CPUs boost group support */
	st->idx = idx;
	if (schedtune_boostgroup_init(st))
		goto release;

	return &st->css;

release:
	kfree(st);
out:
	return ERR_PTR(-ENOMEM);
}

static void
schedtune_boostgroup_release(struct schedtune *st)
{
#ifdef CONFIG_DYNAMIC_STUNE_BOOST
	/* Free dynamic boost slots */
	boost_slots_release(st);
#endif // CONFIG_DYNAMIC_STUNE_BOOST

	/* Reset this boost group */
	schedtune_boostgroup_update(st->idx, 0);

	/* Keep track of allocated boost groups */
	allocated_group[st->idx] = NULL;
}

static void
schedtune_css_free(struct cgroup_subsys_state *css)
{
	struct schedtune *st = css_st(css);


	schedtune_boostgroup_release(st);
	kfree(st);
}

struct cgroup_subsys schedtune_cgrp_subsys = {
	.css_alloc	= schedtune_css_alloc,
	.css_free	= schedtune_css_free,
	.can_attach     = schedtune_can_attach,
	.cancel_attach  = schedtune_cancel_attach,
	.attach		= schedtune_attach,
	.legacy_cftypes	= files,
	.early_init	= 1,
};

static inline void
schedtune_init_cgroups(void)
{
	struct boost_groups *bg;
	int cpu;

	/* Initialize the per CPU boost groups */
	for_each_possible_cpu(cpu) {
		bg = &per_cpu(cpu_boost_groups, cpu);
		memset(bg, 0, sizeof(struct boost_groups));

		raw_spin_lock_init(&bg->lock);
	}

	pr_info("schedtune: configured to support %d boost groups\n",
		BOOSTGROUPS_COUNT);

	schedtune_initialized = true;
}

#ifdef CONFIG_DYNAMIC_STUNE_BOOST
static struct schedtune *stune_get_by_name(char *st_name)
{
	int idx;

	for (idx = 0; idx < BOOSTGROUPS_COUNT; ++idx) {
		char name_buf[NAME_MAX + 1];
		struct schedtune *st = allocated_group[idx];

		if (!st) {
			pr_warn("schedtune: could not find %s\n", st_name);
			break;
		}

		cgroup_name(st->css.cgroup, name_buf, sizeof(name_buf));
		if (strncmp(name_buf, st_name, strlen(st_name)) == 0)
			return st;
	}

	return NULL;
}

static int dynamic_boost(struct schedtune *st, int boost)
{
	int ret;
	/* Backup boost_default */
	int boost_default_backup = st->boost_default;

	ret = boost_write(&st->css, NULL, boost);

	/* Restore boost_default */
	st->boost_default = boost_default_backup;

	return ret;
}

static inline bool is_valid_boost_slot(int slot)
{
	return slot >= 0 && slot < DYNAMIC_BOOST_SLOTS_COUNT;
}

static int activate_boost_slot(struct schedtune *st, int boost, int *slot)
{
	int ret = 0;
	struct boost_slot *curr_slot;
	struct list_head *head;
	*slot = -1;

	mutex_lock(&boost_slot_mutex);

	/* Check for slots in available_boost_slots */
	if (list_empty(&st->available_boost_slots.list)) {
		ret = -EINVAL;
		goto exit;
	}

	/*
	 * Move one slot from available_boost_slots to active_boost_slots
	 */

	/* Get first slot from available_boost_slots */
	head = &st->available_boost_slots.list;
	curr_slot = list_first_entry(head, struct boost_slot, list);

	/* Store slot value and boost value*/
	*slot = curr_slot->idx;
	st->slot_boost[*slot] = boost;

	/* Delete slot from available_boost_slots */
	list_del(&curr_slot->list);
	kfree(curr_slot);

	/* Create new slot with same value at tail of active_boost_slots */
	curr_slot = kmalloc(sizeof(*curr_slot), GFP_KERNEL);
	curr_slot->idx = *slot;
	list_add_tail(&curr_slot->list, &st->active_boost_slots.list);

exit:
	mutex_unlock(&boost_slot_mutex);
	return ret;
}

static int deactivate_boost_slot(struct schedtune *st, int slot)
{
	int ret = 0;
	struct boost_slot *curr_slot, *next_slot;

	mutex_lock(&boost_slot_mutex);

	if (!is_valid_boost_slot(slot)) {
		ret = -EINVAL;
		goto exit;
	}

	/* Delete slot from active_boost_slots */
	list_for_each_entry_safe(curr_slot, next_slot,
				 &st->active_boost_slots.list, list) {
		if (curr_slot->idx == slot) {
			st->slot_boost[slot] = 0;
			list_del(&curr_slot->list);
			kfree(curr_slot);

			/* Create same slot at tail of available_boost_slots */
			curr_slot = kmalloc(sizeof(*curr_slot), GFP_KERNEL);
			curr_slot->idx = slot;
			list_add_tail(&curr_slot->list,
				      &st->available_boost_slots.list);

			goto exit;
		}
	}

	/* Reaching here means that we did not find the slot to delete */
	ret = -EINVAL;

exit:
	mutex_unlock(&boost_slot_mutex);
	return ret;
}

static int max_active_boost(struct schedtune *st)
{
	struct boost_slot *slot;
	int max_boost;

	mutex_lock(&boost_slot_mutex);
	mutex_lock(&stune_boost_mutex);

	/* Set initial value to default boost */
	max_boost = st->boost_default;

	/* Check for active boosts */
	if (list_empty(&st->active_boost_slots.list)) {
		goto exit;
	}

	/* Get largest boost value */
	list_for_each_entry(slot, &st->active_boost_slots.list, list) {
		int boost = st->slot_boost[slot->idx];
		if (boost > max_boost)
			max_boost = boost;
	}

exit:
	mutex_unlock(&stune_boost_mutex);
	mutex_unlock(&boost_slot_mutex);

	return max_boost;
}

static int _do_stune_boost(struct schedtune *st, int boost, int *slot)
{
	int ret = 0;

	/* Try to obtain boost slot */
	ret = activate_boost_slot(st, boost, slot);

	/* Check if boost slot obtained successfully */
	if (ret)
		return -EINVAL;

	/* Boost if new value is greater than current */
	mutex_lock(&stune_boost_mutex);
	if (boost > st->boost)
		ret = dynamic_boost(st, boost);
	mutex_unlock(&stune_boost_mutex);

	return ret;
}

int reset_stune_boost(char *st_name, int slot)
{
	int ret = 0;
	int boost = 0;
	struct schedtune *st = stune_get_by_name(st_name);

	if (!st)
		return -EINVAL;

	ret = deactivate_boost_slot(st, slot);
	if (ret)
		return -EINVAL;

	/* Find next largest active boost or reset to default */
	boost = max_active_boost(st);

	mutex_lock(&stune_boost_mutex);
	/* Boost only if value changed */
	if (boost != st->boost)
		ret = dynamic_boost(st, boost);
	mutex_unlock(&stune_boost_mutex);

	return ret;
}

int do_stune_sched_boost(char *st_name, int *slot)
{
	struct schedtune *st = stune_get_by_name(st_name);

	if (!st)
		return -EINVAL;

	return _do_stune_boost(st, st->sched_boost, slot);
}

int do_stune_boost(char *st_name, int boost, int *slot)
{
	struct schedtune *st = stune_get_by_name(st_name);

	if (!st)
		return -EINVAL;

	return _do_stune_boost(st, boost, slot);
}

int set_stune_boost(char *st_name, int boost, int *boost_default)
{
	struct schedtune *st = stune_get_by_name(st_name);
	int ret;

	if (!st)
		return -EINVAL;

	mutex_lock(&stune_boost_mutex);
	if (boost_default)
		*boost_default = st->boost_default;
	ret = boost_write(&st->css, NULL, boost);
	mutex_unlock(&stune_boost_mutex);

	return ret;
}

int do_prefer_idle(char *st_name, u64 prefer_idle)
{
	struct schedtune *st = stune_get_by_name(st_name);

	if (!st)
		return -EINVAL;

	return prefer_idle_write(&st->css, NULL, prefer_idle);
}

#endif /* CONFIG_DYNAMIC_STUNE_BOOST */



/*
 * Initialize the cgroup structures



 */
static int
schedtune_init(void)
{


	schedtune_spc_rdiv = reciprocal_value(100);
	schedtune_init_cgroups();

	return 0;


}
postcore_initcall(schedtune_init);
