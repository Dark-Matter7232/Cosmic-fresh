// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2017 Samsung Electronics Co., Ltd.
 */

#include <uapi/linux/sched/types.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/exynos_iovmm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/ion_exynos.h>

#include "g2d.h"
#include "g2d_task.h"
#include "g2d_uapi_process.h"
#include "g2d_command.h"
#include "g2d_fence.h"
#include "g2d_debug.h"
#include "g2d_secure.h"

static void g2d_secure_enable(void)
{
	g2d_smc(SMC_PROTECTION_SET, 0, G2D_ALWAYS_S, 1);
}

static void g2d_secure_disable(void)
{
	g2d_smc(SMC_PROTECTION_SET, 0, G2D_ALWAYS_S, 0);
}

static int g2d_map_cmd_data(struct g2d_task *task)
{
	bool self_prot = task->g2d_dev->caps & G2D_DEVICE_CAPS_SELF_PROTECTION;
	struct scatterlist sgl;
	int prot = IOMMU_READ;

	if (!self_prot && IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION))
		return 0;

	if (device_get_dma_attr(task->g2d_dev->dev) == DEV_DMA_COHERENT)
		prot |= IOMMU_CACHE;

	/* mapping the command data */
	sg_init_table(&sgl, 1);
	sg_set_page(&sgl, task->cmd_page, G2D_CMD_LIST_SIZE, 0);
	task->cmd_addr = iovmm_map(task->g2d_dev->dev, &sgl, 0,
				   G2D_CMD_LIST_SIZE, DMA_TO_DEVICE, prot);

	if (IS_ERR_VALUE(task->cmd_addr)) {
		perrfndev(task->g2d_dev, "Unable to alloc IOVA for cmd data");
		return task->cmd_addr;
	}

	return 0;
}

struct g2d_task *g2d_get_active_task_from_id(struct g2d_device *g2d_dev,
					     unsigned int id)
{
	struct g2d_task *task;

	list_for_each_entry(task, &g2d_dev->tasks_active, node) {
		if (g2d_task_id(task) == id)
			return task;
	}

	perrfndev(g2d_dev, "No active task entry is found for ID %d", id);

	return NULL;
}

static void g2d_task_completion_work(struct kthread_work *work)
{
	struct g2d_task *task = container_of(work, struct g2d_task,
					     completion_work);

	g2d_put_images(task->g2d_dev, task);

	g2d_put_free_task(task->g2d_dev, task);
}

static void __g2d_finish_task(struct g2d_task *task, bool success)
{
	unsigned int i;
	bool need_invalidate = false;

	change_task_state_finished(task);
	if (!success)
		mark_task_state_error(task);

	complete_all(&task->completion);

	if (!(task->flags & G2D_FLAG_NONBLOCK))
		return;

	for (i = 0; i < task->target.num_buffers; i++) {
		if (ion_cached_dmabuf(task->target.buffer[i].dmabuf.dmabuf)) {
			need_invalidate = true;
			break;
		}
	}

	if (task->release_fence && (!need_invalidate ||
	    (device_get_dma_attr(task->g2d_dev->dev) == DEV_DMA_COHERENT))) {
		if (!success)
			dma_fence_set_error(task->release_fence->fence, -EIO);

		dma_fence_signal(task->release_fence->fence);
		fput(task->release_fence->file);

		task->release_fence = NULL;
	}

	kthread_queue_work(task->g2d_dev->completion_workq, &task->completion_work);
}

static void g2d_finish_task(struct g2d_device *g2d_dev,
			    struct g2d_task *task, bool success)
{
	list_del_init(&task->node);

	task->ktime_end = ktime_get();

	del_timer(&task->hw_timer);

	g2d_secure_disable();

	clk_disable(g2d_dev->clock);

	pm_runtime_put(g2d_dev->dev);

	__g2d_finish_task(task, success);
}

void g2d_finish_tasks(struct g2d_device *g2d_dev,
		      unsigned int intflags, bool success)
{
	struct g2d_task *task, *n;

	list_for_each_entry_safe(task, n, &g2d_dev->tasks_active, node) {
		if (!success || ((intflags & BIT(g2d_task_id(task))) != 0)) {
			g2d_finish_task(g2d_dev, task, success);
			intflags &= ~BIT(g2d_task_id(task));
		}
	}

	if (intflags)
		perrfndev(g2d_dev,
			  "Found finished jobs (%#x) of inactive tasks",
			  intflags);
}

static void g2d_execute_task(struct g2d_device *g2d_dev, struct g2d_task *task)
{
	g2d_secure_enable();

	list_move_tail(&task->node, &g2d_dev->tasks_active);
	change_task_state_active(task);

	task->hw_timer.expires =
		jiffies + msecs_to_jiffies(G2D_HW_TIMEOUT_MSEC);
	add_timer(&task->hw_timer);

	/*
	 * g2d_device_run() is not reentrant while g2d_schedule() is
	 * reentrant g2d_device_run() should be protected with
	 * g2d_dev->lock_task from race.
	 */
	g2d_device_run(g2d_dev, task);
}

void g2d_prepare_suspend(struct g2d_device *g2d_dev)
{
	spin_lock_irq(&g2d_dev->lock_task);
	set_bit(G2D_DEVICE_STATE_SUSPEND, &g2d_dev->state);
	spin_unlock_irq(&g2d_dev->lock_task);

	wait_event(g2d_dev->freeze_wait, list_empty(&g2d_dev->tasks_active));
}

void g2d_suspend_finish(struct g2d_device *g2d_dev)
{
	struct g2d_task *task;

	spin_lock_irq(&g2d_dev->lock_task);

	clear_bit(G2D_DEVICE_STATE_SUSPEND, &g2d_dev->state);

	while (!list_empty(&g2d_dev->tasks_prepared)) {

		task = list_first_entry(&g2d_dev->tasks_prepared,
					struct g2d_task, node);
		g2d_execute_task(g2d_dev, task);
	}

	spin_unlock_irq(&g2d_dev->lock_task);
}

static void g2d_schedule_task(struct g2d_task *task)
{
	struct g2d_device *g2d_dev = task->g2d_dev;
	unsigned long flags;
	int ret;

	del_timer(&task->fence_timer);

	if (g2d_task_has_error_fence(task))
		goto err_fence;

	g2d_complete_commands(task);

	/*
	 * Unconditional invocation of pm_runtime_get_sync() has no side effect
	 * in g2d_schedule(). It just increases the usage count of RPM if this
	 * function skips calling g2d_device_run(). The skip only happens when
	 * there is no task to run in g2d_dev->tasks_prepared.
	 * If pm_runtime_get_sync() enabled power, there must be a task in
	 * g2d_dev->tasks_prepared.
	 */
	ret = pm_runtime_get_sync(g2d_dev->dev);
	if (ret < 0) {
		perrfndev(g2d_dev, "Failed to enable power (%d)", ret);
		goto err_pm;
	}

	ret = clk_prepare_enable(g2d_dev->clock);
	if (ret < 0) {
		perrfndev(g2d_dev, "Failed to enable clock (%d)", ret);
		goto err_clk;
	}

	spin_lock_irqsave(&g2d_dev->lock_task, flags);

	list_add_tail(&task->node, &g2d_dev->tasks_prepared);
	change_task_state_prepared(task);

	if (!(g2d_dev->state & (1 << G2D_DEVICE_STATE_SUSPEND)))
		g2d_execute_task(g2d_dev, task);

	spin_unlock_irqrestore(&g2d_dev->lock_task, flags);
	return;
err_clk:
	pm_runtime_put(g2d_dev->dev);
err_pm:
err_fence:
	__g2d_finish_task(task, false);
}

static void g2d_task_schedule_work(struct kthread_work *work)
{
	g2d_schedule_task(container_of(work, struct g2d_task, sched_work));
}

void g2d_queuework_task(struct kref *kref)
{
	struct g2d_task *task = container_of(kref, struct g2d_task, starter);
	struct g2d_device *g2d_dev = task->g2d_dev;

	kthread_queue_work(g2d_dev->schedule_workq, &task->sched_work);
}

static void g2d_task_direct_schedule(struct kref *kref)
{
	struct g2d_task *task = container_of(kref, struct g2d_task, starter);

	g2d_schedule_task(task);
}

void g2d_start_task(struct g2d_task *task)
{
	reinit_completion(&task->completion);

	if (atomic_read(&task->starter.refcount.refs) > 1) {
		task->fence_timer.expires =
			jiffies + msecs_to_jiffies(G2D_FENCE_TIMEOUT_MSEC);
		add_timer(&task->fence_timer);
	}

	task->ktime_begin = ktime_get();

	kref_put(&task->starter, g2d_task_direct_schedule);
}

void g2d_cancel_task(struct g2d_task *task)
{
	__g2d_finish_task(task, false);
}

void g2d_fence_callback(struct dma_fence *fence, struct dma_fence_cb *cb)
{
	struct g2d_layer *layer = container_of(cb, struct g2d_layer, fence_cb);
	unsigned long flags;

	g2d_stamp_task(layer->task, G2D_STAMP_STATE_FENCE, (u64)fence);

	spin_lock_irqsave(&layer->task->fence_timeout_lock, flags);
	/* @fence is released in g2d_put_image() */
	kref_put(&layer->task->starter, g2d_queuework_task);
	spin_unlock_irqrestore(&layer->task->fence_timeout_lock, flags);
}

static bool block_on_contension;
module_param(block_on_contension, bool, 0644);

static int max_queued;
module_param(max_queued, int, 0644);

static int g2d_queued_task_count(struct g2d_device *g2d_dev)
{
	struct g2d_task *task;
	int num_queued = 0;

	list_for_each_entry(task, &g2d_dev->tasks_active, node)
		num_queued++;

	list_for_each_entry(task, &g2d_dev->tasks_prepared, node)
		num_queued++;

	return num_queued;
}

struct g2d_task *g2d_get_free_task(struct g2d_device *g2d_dev,
				    struct g2d_context *g2d_ctx, bool hwfc)
{
	struct g2d_task *task;
	struct list_head *taskfree;
	unsigned long flags;
	int num_queued = 0;
	ktime_t ktime_pending;

	if ((g2d_dev->caps & G2D_DEVICE_CAPS_HWFC) && hwfc) {
		perrfndev(g2d_dev, "HWFC is not supported");
		return ERR_PTR(-EINVAL);
	}

	if (hwfc)
		taskfree = &g2d_dev->tasks_free_hwfc;
	else
		taskfree = &g2d_dev->tasks_free;

	spin_lock_irqsave(&g2d_dev->lock_task, flags);

	while (list_empty(taskfree) ||
	       ((num_queued = g2d_queued_task_count(g2d_dev)) >= max_queued)) {

		spin_unlock_irqrestore(&g2d_dev->lock_task, flags);

		if (list_empty(taskfree))
			perrfndev(g2d_dev,
				  "no free task slot found (hwfc %d)", hwfc);
		else
			perrfndev(g2d_dev, "queued %d >= max %d",
				  num_queued, max_queued);

		if (!block_on_contension)
			return ERR_PTR(-EBUSY);

		ktime_pending = ktime_get();

		wait_event(g2d_dev->queued_wait,
			   !list_empty(taskfree) &&
			   (g2d_queued_task_count(g2d_dev) < max_queued));

		perrfndev(g2d_dev,
			  "wait to resolve contension for %d us",
			  (int)ktime_us_delta(ktime_get(), ktime_pending));

		spin_lock_irqsave(&g2d_dev->lock_task, flags);
	}

	task = list_first_entry(taskfree, struct g2d_task, node);
	list_del_init(&task->node);

	init_task_state(task);
	task->sec.priority = g2d_ctx->priority;

	g2d_init_commands(task);

	spin_unlock_irqrestore(&g2d_dev->lock_task, flags);

	/*
	 * Inherit qos of device to guarantee while task runs
	 *
	 * However, task doesn't get the qos of device atomically
	 * with mutex because it is only hint to ensure the performance
	 * of task. Also, The request of performance update and task execution
	 * doesn't occur at the same time in normal situation.
	 */
	task->taskqos = g2d_dev->qos;

	return task;
}

void g2d_put_free_task(struct g2d_device *g2d_dev, struct g2d_task *task)
{
	unsigned long flags;

	task->taskqos.rbw = task->taskqos.wbw = 0;
	task->taskqos.devfreq = 0;

	spin_lock_irqsave(&g2d_dev->lock_task, flags);

	task->bufidx = -1;

	clear_task_state(task);

	if (IS_HWFC(task->flags)) {
		/* hwfc job id will be set from repeater driver info */
		g2d_task_set_id(task, G2D_MAX_JOBS);
		list_add(&task->node, &g2d_dev->tasks_free_hwfc);
	} else {
		list_add(&task->node, &g2d_dev->tasks_free);
	}

	spin_unlock_irqrestore(&g2d_dev->lock_task, flags);

	wake_up(&g2d_dev->queued_wait);
}

void g2d_destroy_tasks(struct g2d_device *g2d_dev)
{
	struct g2d_task *task, *next;
	unsigned long flags;

	spin_lock_irqsave(&g2d_dev->lock_task, flags);

	task = g2d_dev->tasks;
	while (task != NULL) {
		next = task->next;

		list_del(&task->node);

		iovmm_unmap(g2d_dev->dev, task->cmd_addr);

		__free_pages(task->cmd_page, get_order(G2D_CMD_LIST_SIZE));

		kfree(task->source);
		kfree(task);

		task = next;
	}

	spin_unlock_irqrestore(&g2d_dev->lock_task, flags);

	kthread_destroy_worker(g2d_dev->completion_workq);
	kthread_destroy_worker(g2d_dev->schedule_workq);
}

static struct g2d_task *g2d_create_task(struct g2d_device *g2d_dev, int id)
{
	struct g2d_task *task;
	int i, ret = 0;

	task = kzalloc(sizeof(*task), GFP_KERNEL);
	if (!task)
		return ERR_PTR(-ENOMEM);

	task->source = kcalloc(g2d_dev->max_layers, sizeof(*task->source),
			       GFP_KERNEL);
	if (!task->source) { /* g2d_dev->max_layes is not zero */
		ret = -ENOMEM;
		goto err_alloc;
	}

	INIT_LIST_HEAD(&task->node);

	task->cmd_page = alloc_pages(GFP_KERNEL, get_order(G2D_CMD_LIST_SIZE));
	if (!task->cmd_page) {
		ret = -ENOMEM;
		goto err_page;
	}

	g2d_task_set_id(task, id);
	task->bufidx = -1;
	task->g2d_dev = g2d_dev;

	ret = g2d_map_cmd_data(task);
	if (ret)
		goto err_map;

	task->sec.cmd_paddr = (unsigned long)page_to_phys(task->cmd_page);

	for (i = 0; i < g2d_dev->max_layers; i++)
		task->source[i].task = task;
	task->target.task = task;

	init_completion(&task->completion);
	spin_lock_init(&task->fence_timeout_lock);

	setup_timer(&task->hw_timer,
		    g2d_hw_timeout_handler, (unsigned long)task);
	setup_timer(&task->fence_timer,
		    g2d_fence_timeout_handler, (unsigned long)task);

	kthread_init_work(&task->completion_work, g2d_task_completion_work);
	kthread_init_work(&task->sched_work, g2d_task_schedule_work);

	return task;

err_map:
	__free_pages(task->cmd_page, get_order(G2D_CMD_LIST_SIZE));
err_page:
	kfree(task->source);
err_alloc:
	kfree(task);

	return ERR_PTR(ret);
}

int g2d_create_tasks(struct g2d_device *g2d_dev)
{
	struct sched_param param[] = {
		{.sched_priority =  MAX_RT_PRIO / 4 - 1 },
		{.sched_priority =  MAX_RT_PRIO / 2 }
	};
	struct g2d_task *task;
	unsigned int i;

	g2d_dev->completion_workq = kthread_create_worker(0, "g2d_completion");
	if (IS_ERR(g2d_dev->completion_workq))
		return PTR_ERR(g2d_dev->completion_workq);

	sched_setscheduler_nocheck(g2d_dev->completion_workq->task, SCHED_FIFO, &param[0]);

	g2d_dev->schedule_workq = kthread_create_worker(0, "g2d_scheduler");
	if (IS_ERR(g2d_dev->schedule_workq))
		return PTR_ERR(g2d_dev->schedule_workq);

	sched_setscheduler_nocheck(g2d_dev->schedule_workq->task, SCHED_FIFO, &param[1]);

	for (i = 0; i < G2D_MAX_JOBS; i++) {
		task = g2d_create_task(g2d_dev, i);

		if (IS_ERR(task)) {
			g2d_destroy_tasks(g2d_dev);
			return PTR_ERR(task);
		}

		task->next = g2d_dev->tasks;
		g2d_dev->tasks = task;

		/* MAX_SHARED_BUF_NUM is defined in media/exynos_repeater.h */
		if ((g2d_dev->caps & G2D_DEVICE_CAPS_HWFC) &&
		    (i < MAX_SHARED_BUF_NUM))
			list_add(&task->node, &g2d_dev->tasks_free_hwfc);
		else
			list_add(&task->node, &g2d_dev->tasks_free);
	}

	max_queued = G2D_MAX_JOBS;

	return 0;
}
