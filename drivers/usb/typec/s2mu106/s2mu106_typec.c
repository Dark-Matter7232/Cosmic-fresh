/*
 driver/usbpd/s2mu106.c - S2MU106 USB PD(Power Delivery) device driver
 *
 * Copyright (C) 2020 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/completion.h>

#include <linux/usb/typec/s2mu106/s2mu106_pd.h>
#include <linux/usb/typec/s2mu106/s2mu106_typec.h>
#include <linux/usb/typec/pdic_sysfs.h>

#include <linux/muic/muic.h>
#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_PM_S2MU106)
#if defined(CONFIG_BATTERY_SAMSUNG_V2)
#include "../../../battery_v2/include/s2mu106_pmeter.h"
#elif defined(CONFIG_BATTERY_SAMSUNG_LEGO_STYLE)
#include "../../../battery/charger/s2mu106_pmeter.h"
#endif
#endif
#if defined(CONFIG_BATTERY_SAMSUNG_V2)
#include "../../../battery_v2/include/sec_charging_common.h"
#include "../../../battery_v2/include/sec_battery.h"
#elif defined(CONFIG_BATTERY_SAMSUNG_LEGO_STYLE)
#include "../../../battery/common/include/sec_charging_common.h"
#include "../../../battery/common/include/sec_battery.h"
#else
#include <linux/battery/sec_charging_common.h>
#endif
#if defined(CONFIG_USB_HOST_NOTIFY) || defined(CONFIG_USB_HW_PARAM)
#include <linux/usb_notify.h>
#endif
#include <linux/regulator/consumer.h>
#if defined(CONFIG_BATTERY_NOTIFIER)
#include <linux/battery/battery_notifier.h>
#endif
#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
extern struct pdic_notifier_struct pd_noti;
#endif
#endif
#if defined(CONFIG_CCIC_NOTIFIER)
extern struct device *ccic_device;
#endif

#define I2C_RETRY_CNT	3

/*
*VARIABLE DEFINITION
*/
static usbpd_phy_ops_type s2mu106_ops;

static enum power_supply_property ccic_props[] = {
};

static char *ccic_supplied_to[] = {
	"battery",
};

/*
*FUNCTION DEFINITION
*/
static int s2mu106_usbpd_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
static int s2mu106_usbpd_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf);
static int s2mu106_usbpd_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
static int s2mu106_usbpd_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf);
static int s2mu106_receive_message(void *data);
static int s2mu106_check_port_detect(struct s2mu106_usbpd_data *pdic_data);
static int s2mu106_usbpd_reg_init(struct s2mu106_usbpd_data *_data);
static void s2mu106_dfp(struct i2c_client *i2c);
static void s2mu106_ufp(struct i2c_client *i2c);
static void s2mu106_src(struct i2c_client *i2c);
static void s2mu106_snk(struct i2c_client *i2c);
static void s2mu106_assert_rd(void *_data);
static void s2mu106_assert_rp(void *_data);
static void s2mu106_assert_drp(void *_data);
static void s2mu106_usbpd_check_rid(struct s2mu106_usbpd_data *pdic_data);
static int s2mu106_usbpd_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
static int s2mu106_usbpd_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
static void s2mu106_usbpd_notify_detach(struct s2mu106_usbpd_data *pdic_data);
static void s2mu106_usbpd_detach_init(struct s2mu106_usbpd_data *pdic_data);
static int s2mu106_usbpd_set_cc_control(struct s2mu106_usbpd_data  *pdic_data, int val);
static void s2mu106_usbpd_set_rp_scr_sel(struct s2mu106_usbpd_data *pdic_data,
													CCIC_RP_SCR_SEL scr_sel);
int s2mu106_usbpd_check_msg(void *_data, u64 *val);
static void s2mu106_vbus_short_check(struct s2mu106_usbpd_data *pdic_data);


#if defined(CONFIG_CCIC_NOTIFIER)
static enum ccic_sysfs_property s2mu106_sysfs_properties[] = {
	CCIC_SYSFS_PROP_CHIP_NAME,
	CCIC_SYSFS_PROP_LPM_MODE,
	CCIC_SYSFS_PROP_STATE,
	CCIC_SYSFS_PROP_RID,
	CCIC_SYSFS_PROP_CTRL_OPTION,
	CCIC_SYSFS_PROP_FW_WATER,
	CCIC_SYSFS_PROP_ACC_DEVICE_VERSION,
	CCIC_SYSFS_PROP_USBPD_IDS,
	CCIC_SYSFS_PROP_USBPD_TYPE,
};
#endif

void usbpd_manager_select_pdo_cancel(struct device *dev);

#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
void select_pdo(int num);
void usbpd_manager_select_pdo(int num);
extern void (*fp_select_pdo)(int num);
#if defined(CONFIG_PDIC_PD30)
extern int (*fp_sec_pd_select_pps)(int num, int ppsVol, int ppsCur);
extern int (*fp_sec_pd_get_apdo_max_power)(unsigned int *pdo_pos, unsigned int *taMaxVol, unsigned int *taMaxCur, unsigned int *taMaxPwr);
#endif
#endif
#endif


static void s2mu106_usbpd_test_read(struct s2mu106_usbpd_data *usbpd_data)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	u8 data[10];

	s2mu106_usbpd_read_reg(i2c, 0x1, &data[0]);
	s2mu106_usbpd_read_reg(i2c, 0x18, &data[1]);
	s2mu106_usbpd_read_reg(i2c, 0x27, &data[2]);
	s2mu106_usbpd_read_reg(i2c, 0x28, &data[3]);
	s2mu106_usbpd_read_reg(i2c, 0x40, &data[4]);
	s2mu106_usbpd_read_reg(i2c, 0xe4, &data[5]);
	s2mu106_usbpd_read_reg(i2c, 0xb3, &data[6]);
	s2mu106_usbpd_read_reg(i2c, 0xb4, &data[7]);
	s2mu106_usbpd_read_reg(i2c, 0xb7, &data[8]);

	pr_debug_once("%s, 0x1(%x) 0x18(%x) 0x27(%x) 0x28(%x) 0x40(%x) 0xe4(%x) 0xb3(%x) 0xb4(%x) 0xb7(%X)\n",
			__func__, data[0], data[1], data[2], data[3], data[4],
										data[5], data[6], data[7], data[8]);
}

void s2mu106_rprd_mode_change(struct s2mu106_usbpd_data *usbpd_data, u8 mode)
{
	u8 data = 0;
	struct i2c_client *i2c = usbpd_data->i2c;
	struct device *dev = &i2c->dev;
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	pr_debug_once("%s, mode=0x%x\n", __func__, mode);

	mutex_lock(&usbpd_data->_mutex);
	if (usbpd_data->lpm_mode)
		goto skip;

	pr_debug_once("%s, %d\n", __func__, __LINE__);
	switch (mode) {
	case TYPE_C_ATTACH_DFP: /* SRC */
		s2mu106_usbpd_set_cc_control(usbpd_data, USBPD_CC_OFF);
		s2mu106_usbpd_set_rp_scr_sel(usbpd_data, PLUG_CTRL_RP0);
		s2mu106_assert_rp(pd_data);
		msleep(20);
		s2mu106_usbpd_detach_init(usbpd_data);
		s2mu106_usbpd_notify_detach(usbpd_data);
		msleep(600);
		s2mu106_usbpd_set_rp_scr_sel(usbpd_data, PLUG_CTRL_RP80);
		msleep(S2MU106_ROLE_SWAP_TIME_MS);
		s2mu106_assert_drp(pd_data);
		usbpd_data->status_reg |= 1 << PLUG_ATTACH;
		schedule_delayed_work(&usbpd_data->plug_work, 0);
		break;
	case TYPE_C_ATTACH_UFP: /* SNK */
		s2mu106_usbpd_set_cc_control(usbpd_data, USBPD_CC_OFF);
		s2mu106_assert_rp(pd_data);
		s2mu106_usbpd_set_rp_scr_sel(usbpd_data, PLUG_CTRL_RP0);
		msleep(20);
		s2mu106_usbpd_detach_init(usbpd_data);
		s2mu106_usbpd_notify_detach(usbpd_data);
		msleep(600);
		s2mu106_assert_rd(pd_data);
		s2mu106_usbpd_set_rp_scr_sel(usbpd_data, PLUG_CTRL_RP80);
		msleep(S2MU106_ROLE_SWAP_TIME_MS);
		s2mu106_assert_drp(pd_data);
		usbpd_data->status_reg |= 1 << PLUG_ATTACH;
		schedule_delayed_work(&usbpd_data->plug_work, 0);
		break;
	case TYPE_C_ATTACH_DRP: /* DRP */
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, &data);
		data |= S2MU106_REG_PLUG_CTRL_DRP;
		s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, data);
		break;
	};
skip:
	mutex_unlock(&usbpd_data->_mutex);
}

void s2mu106_vbus_turn_on_ctrl(struct s2mu106_usbpd_data *usbpd_data, bool enable)
{
	struct power_supply *psy_otg;
	union power_supply_propval val;
	int on = !!enable;
	int ret = 0, retry_cnt = 0;
#if 0
	static int reserve_booster = 0;
	struct otg_notify *o_notify = get_otg_notify();
#endif

	pr_debug_once("%s %d, enable=%d\n", __func__, __LINE__, enable);

#if 0
	if (o_notify && o_notify->booting_delay_sec && enable) {
		pr_debug_once("%s %d, is booting_delay_sec. skip to control booster\n",
			__func__, __LINE__);
		reserve_booster = 1;
		send_otg_notify(o_notify, NOTIFY_EVENT_RESERVE_BOOSTER, 1);
		return;
	}

	if (!enable) {
		if (reserve_booster) {
			reserve_booster = 0;
			send_otg_notify(o_notify, NOTIFY_EVENT_RESERVE_BOOSTER, 0);
		}
	}
#endif
	psy_otg = get_power_supply_by_name("otg");

	if (psy_otg) {
		val.intval = enable;
		usbpd_data->is_otg_vboost = enable;
		ret = psy_otg->desc->set_property(psy_otg, POWER_SUPPLY_PROP_ONLINE, &val);
	} else {
		pr_err("%s: Fail to get psy battery\n", __func__);

		return;
	}
	if (ret) {
		pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
			__func__, ret);
	} else {
		if (enable == VBUS_ON) {
			for (retry_cnt = 0; retry_cnt < 5; retry_cnt++) {
				psy_otg->desc->get_property(psy_otg, POWER_SUPPLY_PROP_ONLINE, &val);
				if (val.intval == VBUS_OFF) {
					msleep(100);
					val.intval = enable;
					psy_otg->desc->set_property(psy_otg, POWER_SUPPLY_PROP_ONLINE, &val);
				} else
					break;
			}
		}
		pr_debug_once("otg accessory power = %d\n", on);
	}
}

#ifdef CONFIG_PM_S2MU106
static void s2mu106_usbpd_set_pmeter_mode(struct s2mu106_usbpd_data *pdic_data,
																int mode)
{
	struct power_supply *psy_pm = pdic_data->psy_pm;
	union power_supply_propval val;
	int ret = 0;

	pr_debug_once("%s, mode=%d\n", __func__, mode);

	if (psy_pm) {
		val.intval = mode;
		ret = psy_pm->desc->set_property(psy_pm,
							POWER_SUPPLY_PROP_CO_ENABLE, &val);
	} else {
		pr_err("%s: Fail to get pmeter\n", __func__);
		return;
	}

	if (ret) {
		pr_err("%s: Fail to set pmeter\n", __func__);
		return;
	}
}

static int s2mu106_usbpd_get_pmeter_volt(struct s2mu106_usbpd_data *pdic_data)
{
	struct power_supply *psy_pm = pdic_data->psy_pm;
	union power_supply_propval val;
	int ret = 0;

	if (psy_pm) {
		ret = psy_pm->desc->get_property(psy_pm, POWER_SUPPLY_PROP_VCHGIN, &val);
	} else {
		pr_err("%s: Fail to get pmeter\n", __func__);
		return -1;
	}

	if (ret) {
			pr_err("%s: fail to set power_suppy pmeter property(%d)\n",
		__func__, ret);
		return -1;
	}

	pdic_data->pm_chgin = val.intval;

	return 0;
}

static int s2mu106_usbpd_check_vbus(struct s2mu106_usbpd_data *pdic_data,
												int volt, CCIC_VBUS_SEL mode)
{
	int delay = 20;
	int retry = 100;
	int i = 0;
	int ret = 0;

	if (mode == VBUS_OFF) {
		for (i = 0; i < retry; i++) {
			ret = s2mu106_usbpd_get_pmeter_volt(pdic_data);
			if (ret < 0)
				return ret;

			if (pdic_data->pm_chgin < volt) {
				pr_debug_once("%s chgin volt(%d) finish!\n", __func__,
												pdic_data->pm_chgin);
				return true;
			} else {
				pr_debug_once("%s chgin volt(%d) waiting 730ms!\n",
										__func__, pdic_data->pm_chgin);
				msleep(730);
				return true;
			}
			msleep(delay);
		}
	} else if (mode == VBUS_ON) {
		ret = s2mu106_usbpd_get_pmeter_volt(pdic_data);
		if (ret < 0)
			return ret;
		if (pdic_data->pm_chgin > volt) {
			pr_debug_once("%s vbus volt(%d->%d) mode(%d)!\n",
					__func__, volt, pdic_data->pm_chgin, mode);
			return true;
		} else
			return false;
	}

	pr_debug_once("%s failed check vbus volt(%d->%d) mode(%d)!\n",
			__func__, volt, pdic_data->pm_chgin, mode);

	return false;
}
#endif

static int s2mu106_usbpd_check_accessory(struct s2mu106_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val, cc1_val, cc2_val;
	struct device *dev = pdic_data->dev;
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct usbpd_manager_data *manager = &pd_data->manager;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_MON1, &val);

	cc1_val = val & S2MU106_REG_CTRL_MON_CC1_MASK;
	cc2_val = (val & S2MU106_REG_CTRL_MON_CC2_MASK) >> S2MU106_REG_CTRL_MON_CC2_SHIFT;

	if (cc1_val == USBPD_Rd && cc2_val == USBPD_Rd)
		return -1;
	if (cc1_val == USBPD_Ra && cc2_val == USBPD_Ra){
		manager->acc_type = CCIC_DOCK_UNSUPPORTED_AUDIO;
		usbpd_manager_check_accessory(manager);
		return -1;
	}

	return 0;
}

static void s2mu106_usbpd_get_cc_voltage(struct s2mu106_usbpd_data *usbpd_data)
{
#ifdef CONFIG_PM_S2MU106
	struct power_supply *psy_pm = usbpd_data->psy_pm;
	union power_supply_propval val1, val2;
	int ret = 0;

	if (psy_pm) {
		ret = psy_pm->desc->get_property(psy_pm, POWER_SUPPLY_PROP_VCC1, &val1);
		ret = psy_pm->desc->get_property(psy_pm, POWER_SUPPLY_PROP_VCC2, &val2);
	} else {
		pr_err("%s: Fail to get pmeter\n", __func__);
		return;
	}

	if (ret) {
			pr_err("%s: fail to set power_suppy pmeter property(%d)\n",
		__func__, ret);
	} else {
		usbpd_data->pm_cc1 = val1.intval;
		usbpd_data->pm_cc2 = val2.intval;
	}
	pr_debug_once("%s pm_cc1 : %d, pm_cc2 : %d\n", __func__, val1.intval, val2.intval);
#else
	return;
#endif
}

#if defined(CONFIG_CCIC_NOTIFIER)
static void s2mu106_ccic_event_notifier(struct work_struct *data)
{
	struct ccic_state_work *event_work =
		container_of(data, struct ccic_state_work, ccic_work);
	CC_NOTI_TYPEDEF ccic_noti;

	switch (event_work->dest) {
	case CCIC_NOTIFY_DEV_USB:
		pr_debug_once("usb:%s, dest=%s, id=%s, attach=%s, drp=%s, event_work=%p\n", __func__,
				CCIC_NOTI_DEST_Print[event_work->dest],
				CCIC_NOTI_ID_Print[event_work->id],
				event_work->attach ? "Attached" : "Detached",
				CCIC_NOTI_USB_STATUS_Print[event_work->event],
				event_work);
		break;
	default:
		pr_debug_once("usb:%s, dest=%s, id=%s, attach=%d, event=%d, event_work=%p\n", __func__,
			CCIC_NOTI_DEST_Print[event_work->dest],
			CCIC_NOTI_ID_Print[event_work->id],
			event_work->attach,
			event_work->event,
			event_work);
		break;
	}

	ccic_noti.src = CCIC_NOTIFY_DEV_CCIC;
	ccic_noti.dest = event_work->dest;
	ccic_noti.id = event_work->id;
	ccic_noti.sub1 = event_work->attach;
	ccic_noti.sub2 = event_work->event;
	ccic_noti.sub3 = 0;
#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	ccic_noti.pd = &pd_noti;
#endif
#endif
	ccic_notifier_notify((CC_NOTI_TYPEDEF *)&ccic_noti, NULL, 0);

	kfree(event_work);
}

extern void s2mu106_ccic_event_work(void *data, int dest, int id, int attach, int event)
{
	struct s2mu106_usbpd_data *usbpd_data = data;
	struct ccic_state_work *event_work;
#if defined(CONFIG_TYPEC)
	struct typec_partner_desc desc;
	enum typec_pwr_opmode mode;
#endif

	pr_debug_once("%s : usb: DIAES %d-%d-%d-%d\n",
		__func__, dest, id, attach, event);
	event_work = kmalloc(sizeof(struct ccic_state_work), GFP_ATOMIC);
	//pr_debug_once("usb: %s,event_work(%p)\n", __func__, event_work);
	INIT_WORK(&event_work->ccic_work, s2mu106_ccic_event_notifier);

	event_work->dest = dest;
	event_work->id = id;
	event_work->attach = attach;
	event_work->event = event;
	event_work->sub = 0;

#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	if (id == CCIC_NOTIFY_ID_USB) {
		pr_debug_once("usb: %s, dest=%d, event=%d, data_role_dual=%d, try_state_change=%d\n",
			__func__, dest, event, usbpd_data->data_role_dual, usbpd_data->try_state_change);

		usbpd_data->data_role_dual = event;

		if (usbpd_data->dual_role != NULL)
			dual_role_instance_changed(usbpd_data->dual_role);

		if (usbpd_data->try_state_change &&
			(usbpd_data->data_role_dual != USB_STATUS_NOTIFY_DETACH)) {
			/* Role change try and new mode detected */
			pr_debug_once("usb: %s, reverse_completion\n", __func__);
			complete(&usbpd_data->reverse_completion);
		}
	}
	else if (id == CCIC_NOTIFY_ID_ROLE_SWAP ) {
		if (usbpd_data->dual_role != NULL)
			dual_role_instance_changed(usbpd_data->dual_role);
	}
#elif defined(CONFIG_TYPEC)
	if (id == CCIC_NOTIFY_ID_USB) {
		if (usbpd_data->typec_try_state_change &&
			(event != USB_STATUS_NOTIFY_DETACH)) {
			// Role change try and new mode detected
			pr_debug_once("usb: %s, role_reverse_completion\n", __func__);
			complete(&usbpd_data->role_reverse_completion);
		}
		if (event == USB_STATUS_NOTIFY_ATTACH_UFP) {
			mode = s2mu106_typec_get_pd_support(usbpd_data);
			typec_set_pwr_opmode(usbpd_data->port, mode);
			desc.usb_pd = mode == TYPEC_PWR_MODE_PD;
			desc.accessory = TYPEC_ACCESSORY_NONE; /* XXX: handle accessories */
			desc.identity = NULL;
			usbpd_data->typec_data_role = TYPEC_DEVICE;
			typec_set_data_role(usbpd_data->port, TYPEC_DEVICE);
			usbpd_data->partner = typec_register_partner(usbpd_data->port, &desc);
		} else if (event == USB_STATUS_NOTIFY_ATTACH_DFP) {
			mode = s2mu106_typec_get_pd_support(usbpd_data);
			typec_set_pwr_opmode(usbpd_data->port, mode);
			desc.usb_pd = mode == TYPEC_PWR_MODE_PD;
			desc.accessory = TYPEC_ACCESSORY_NONE; /* XXX: handle accessories */
			desc.identity = NULL;
			usbpd_data->typec_data_role = TYPEC_HOST;
			typec_set_data_role(usbpd_data->port, TYPEC_HOST);
			usbpd_data->partner = typec_register_partner(usbpd_data->port, &desc);
		} else {
			if (!IS_ERR(usbpd_data->partner))
				typec_unregister_partner(usbpd_data->partner);
			usbpd_data->partner = NULL;
		}
	}
#endif

	if (queue_work(usbpd_data->ccic_wq, &event_work->ccic_work) == 0) {
		pr_debug_once("usb: %s, event_work(%p) is dropped\n", __func__, event_work);
		kfree(event_work);
	}
}
#endif

#if defined(CONFIG_DUAL_ROLE_USB_INTF)
static enum dual_role_property fusb_drp_properties[] = {
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
	DUAL_ROLE_PROP_VCONN_SUPPLY,
};

void s2mu106_role_swap_check(struct work_struct *wk)
{
	struct delayed_work *delay_work =
		container_of(wk, struct delayed_work, work);
	struct s2mu106_usbpd_data *usbpd_data =
		container_of(delay_work, struct s2mu106_usbpd_data, role_swap_work);
	int mode = 0;

	pr_debug_once("%s: s2mu106_ccic_set_dual_role check again.\n", __func__);
	usbpd_data->try_state_change = 0;

	if (usbpd_data->detach_valid) { /* modify here using pd_state */
		pr_err("%s: s2mu106_ccic_set_dual_role reverse failed, set mode to DRP\n", __func__);
		disable_irq(usbpd_data->irq);
		/* exit from Disabled state and set mode to DRP */
		mode =  TYPE_C_ATTACH_DRP;
		s2mu106_rprd_mode_change(usbpd_data, mode);
		enable_irq(usbpd_data->irq);
	}
}

static int s2mu106_ccic_set_dual_role(struct dual_role_phy_instance *dual_role,
				   enum dual_role_property prop,
				   const unsigned int *val)
{
	struct s2mu106_usbpd_data *usbpd_data = dual_role_get_drvdata(dual_role);
	struct i2c_client *i2c;

	USB_STATUS attached_state;
	int mode;
	int timeout = 0;
	int ret = 0;

	if (!usbpd_data) {
		pr_err("%s : usbpd_data is null \n", __func__);
		return -EINVAL;
	}

	i2c = usbpd_data->i2c;

	/* Get Current Role */
	attached_state = usbpd_data->data_role_dual;
	pr_debug_once("%s : request prop = %d , attached_state = %d\n", __func__, prop, attached_state);

	if (attached_state != USB_STATUS_NOTIFY_ATTACH_DFP
	    && attached_state != USB_STATUS_NOTIFY_ATTACH_UFP) {
		pr_err("%s : current mode : %d - just return \n", __func__, attached_state);
		return 0;
	}

	if (attached_state == USB_STATUS_NOTIFY_ATTACH_DFP
	    && *val == DUAL_ROLE_PROP_MODE_DFP) {
		pr_err("%s : current mode : %d - request mode : %d just return \n",
			__func__, attached_state, *val);
		return 0;
	}

	if (attached_state == USB_STATUS_NOTIFY_ATTACH_UFP
	    && *val == DUAL_ROLE_PROP_MODE_UFP) {
		pr_err("%s : current mode : %d - request mode : %d just return \n",
			__func__, attached_state, *val);
		return 0;
	}

	if (attached_state == USB_STATUS_NOTIFY_ATTACH_DFP) {
		/* Current mode DFP and Source  */
		pr_debug_once("%s: try reversing, from Source to Sink\n", __func__);
		/* turns off VBUS first */
		s2mu106_vbus_turn_on_ctrl(usbpd_data, 0);
#if defined(CONFIG_MUIC_SUPPORT_CCIC_OTG_CTRL)
		muic_disable_otg_detect();
#endif
#if defined(CONFIG_CCIC_NOTIFIER)
		/* muic */
		s2mu106_ccic_event_work(usbpd_data,
			CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_ATTACH, 0/*attach*/, 0/*rprd*/);
#endif
		/* exit from Disabled state and set mode to UFP */
		mode =  TYPE_C_ATTACH_UFP;
		usbpd_data->try_state_change = TYPE_C_ATTACH_UFP;
		s2mu106_rprd_mode_change(usbpd_data, mode);
	} else {
		/* Current mode UFP and Sink  */
		pr_debug_once("%s: try reversing, from Sink to Source\n", __func__);
		/* exit from Disabled state and set mode to UFP */
		mode =  TYPE_C_ATTACH_DFP;
		usbpd_data->try_state_change = TYPE_C_ATTACH_DFP;
		s2mu106_rprd_mode_change(usbpd_data, mode);
	}

	reinit_completion(&usbpd_data->reverse_completion);
	timeout =
	    wait_for_completion_timeout(&usbpd_data->reverse_completion,
					msecs_to_jiffies
					(DUAL_ROLE_SET_MODE_WAIT_MS));

	if (!timeout) {
		usbpd_data->try_state_change = 0;
		pr_err("%s: reverse failed, set mode to DRP\n", __func__);
		disable_irq(usbpd_data->irq);
		/* exit from Disabled state and set mode to DRP */
		mode =  TYPE_C_ATTACH_DRP;
		s2mu106_rprd_mode_change(usbpd_data, mode);
		enable_irq(usbpd_data->irq);
		ret = -EIO;
	} else {
		pr_err("%s: reverse success, one more check\n", __func__);
		schedule_delayed_work(&usbpd_data->role_swap_work, msecs_to_jiffies(DUAL_ROLE_SET_MODE_WAIT_MS));
	}

	dev_dbg_once(&i2c->dev, "%s -> data role : %d\n", __func__, *val);
	return ret;
}

/* Decides whether userspace can change a specific property */
int s2mu106_dual_role_is_writeable(struct dual_role_phy_instance *drp,
				  enum dual_role_property prop)
{
	if (prop == DUAL_ROLE_PROP_MODE)
		return 1;
	else
		return 0;
}

/* Callback for "cat /sys/class/dual_role_usb/otg_default/<property>" */
int s2mu106_dual_role_get_local_prop(struct dual_role_phy_instance *dual_role,
				    enum dual_role_property prop,
				    unsigned int *val)
{
	struct s2mu106_usbpd_data *usbpd_data = dual_role_get_drvdata(dual_role);

	USB_STATUS attached_state;
	int power_role_dual;

	if (!usbpd_data) {
		pr_err("%s : usbpd_data is null : request prop = %d \n", __func__, prop);
		return -EINVAL;
	}
	attached_state = usbpd_data->data_role_dual;
	power_role_dual = usbpd_data->power_role_dual;

	pr_debug_once("%s : request prop = %d , attached_state = %d, power_role_dual = %d\n",
		__func__, prop, attached_state, power_role_dual);

	if (prop == DUAL_ROLE_PROP_VCONN_SUPPLY) {
		if (usbpd_data->vconn_en)
			*val = DUAL_ROLE_PROP_VCONN_SUPPLY_YES;
		else
			*val = DUAL_ROLE_PROP_VCONN_SUPPLY_NO;
		return 0;
	}

	if (attached_state == USB_STATUS_NOTIFY_ATTACH_DFP) {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_DFP;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = power_role_dual;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_HOST;
		else
			return -EINVAL;
	} else if (attached_state == USB_STATUS_NOTIFY_ATTACH_UFP) {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_UFP;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = power_role_dual;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_DEVICE;
		else
			return -EINVAL;
	} else {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_NONE;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = DUAL_ROLE_PROP_PR_NONE;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_NONE;
		else
			return -EINVAL;
	}

	return 0;
}

/* Callback for "echo <value> >
 *                      /sys/class/dual_role_usb/<name>/<property>"
 * Block until the entire final state is reached.
 * Blocking is one of the better ways to signal when the operation
 * is done.
 * This function tries to switch to Attached.SRC or Attached.SNK
 * by forcing the mode into SRC or SNK.
 * On failure, we fall back to Try.SNK state machine.
 */
int s2mu106_dual_role_set_prop(struct dual_role_phy_instance *dual_role,
			      enum dual_role_property prop,
			      const unsigned int *val)
{
	pr_debug_once("%s : request prop = %d , *val = %d \n", __func__, prop, *val);
	if (prop == DUAL_ROLE_PROP_MODE)
		return s2mu106_ccic_set_dual_role(dual_role, prop, val);
	else
		return -EINVAL;
}

int s2mu106_dual_role_init(void *_data)
{
	struct s2mu106_usbpd_data *pdic_data = _data;
	struct dual_role_phy_desc *desc;
	struct dual_role_phy_instance *dual_role;

	desc = devm_kzalloc(pdic_data->dev,
			 sizeof(struct dual_role_phy_desc), GFP_KERNEL);
	if (!desc) {
		pr_err("unable to allocate dual role descriptor\n");
		return -1;
	}

	desc->name = "otg_default";
	desc->supported_modes = DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP;
	desc->get_property = s2mu106_dual_role_get_local_prop;
	desc->set_property = s2mu106_dual_role_set_prop;
	desc->properties = fusb_drp_properties;
	desc->num_properties = ARRAY_SIZE(fusb_drp_properties);
	desc->property_is_writeable = s2mu106_dual_role_is_writeable;
	dual_role =
		devm_dual_role_instance_register(pdic_data->dev, desc);
	dual_role->drv_data = pdic_data;
	pdic_data->dual_role = dual_role;
	pdic_data->desc = desc;
	init_completion(&pdic_data->reverse_completion);
	INIT_DELAYED_WORK(&pdic_data->role_swap_work, s2mu106_role_swap_check);

	return 0;
}
#elif defined(CONFIG_TYPEC)
void s2mu106_typec_role_swap_check(struct work_struct *wk)
{
	struct delayed_work *delay_work =
		container_of(wk, struct delayed_work, work);
	struct s2mu106_usbpd_data *usbpd_data =
		container_of(delay_work, struct s2mu106_usbpd_data, typec_role_swap_work);

	pr_debug_once("%s: s2mu106_ccic_set_dual_role check again.\n", __func__);
	usbpd_data->typec_try_state_change = 0;

	if (usbpd_data->detach_valid) { /* modify here using pd_state */
		pr_err("%s: s2mu106_ccic_set_dual_role reverse failed, set mode to DRP\n", __func__);
		disable_irq(usbpd_data->irq);
		/* exit from Disabled state and set mode to DRP */
		s2mu106_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_DRP);
		enable_irq(usbpd_data->irq);
	}
}

int s2mu106_typec_port_type_set(const struct typec_capability *cap, enum typec_port_type port_type)
{
	struct s2mu106_usbpd_data *usbpd_data = container_of(cap, struct s2mu106_usbpd_data, typec_cap);

	int timeout = 0;

	if (!usbpd_data) {
		pr_err("%s : usbpd_data is null\n", __func__);
		return -EINVAL;
	}

	pr_debug_once("%s : typec_power_role=%d, typec_data_role=%d, port_type=%d\n",
		__func__, usbpd_data->typec_power_role, usbpd_data->typec_data_role, port_type);

	switch (port_type) {
	case TYPEC_PORT_DFP:
		pr_debug_once("%s : try reversing, from UFP(Sink) to DFP(Source)\n", __func__);
		usbpd_data->typec_try_state_change = TYPE_C_ATTACH_DFP;
		s2mu106_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_DFP);

		break;
	case TYPEC_PORT_UFP:
		pr_debug_once("%s : try reversing, from DFP(Source) to UFP(Sink)\n", __func__);
		/* turns off VBUS first */
		s2mu106_vbus_turn_on_ctrl(usbpd_data, 0);
#if defined(CONFIG_CCIC_NOTIFIER)
		s2mu106_ccic_event_work(usbpd_data,
			CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_ATTACH,
			0/*attach*/, 0/*rprd*/);
#endif
		usbpd_data->typec_try_state_change = TYPE_C_ATTACH_UFP;
		s2mu106_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_UFP);

		break;
	case TYPEC_PORT_DRP:
		pr_debug_once("%s : set to DRP (No action)\n", __func__);
		return 0;
	default :
		pr_debug_once("%s : invalid typec_role\n", __func__);
		return -EINVAL;
	}

	if (usbpd_data->typec_try_state_change) {
		reinit_completion(&usbpd_data->role_reverse_completion);
		timeout =
		    wait_for_completion_timeout(&usbpd_data->role_reverse_completion,
						msecs_to_jiffies
						(DUAL_ROLE_SET_MODE_WAIT_MS));

		if (!timeout) {
			pr_err("%s: reverse failed, set mode to DRP\n", __func__);
			disable_irq(usbpd_data->irq);
			/* exit from Disabled state and set mode to DRP */
			usbpd_data->typec_try_state_change = 0;
			s2mu106_rprd_mode_change(usbpd_data, TYPE_C_ATTACH_DRP);

			enable_irq(usbpd_data->irq);
			return -EIO;
		} else {
			pr_err("%s: reverse success, one more check\n", __func__);
			schedule_delayed_work(&usbpd_data->typec_role_swap_work, msecs_to_jiffies(DUAL_ROLE_SET_MODE_WAIT_MS));
		}
	}

	return 0;
}

int s2mu106_typec_pr_set(const struct typec_capability *cap,
										enum typec_role power_role)
{
	struct s2mu106_usbpd_data *usbpd_data = container_of(cap, struct s2mu106_usbpd_data, typec_cap);

	if (!usbpd_data) {
		pr_err("%s : usbpd_data is null\n", __func__);
		return -EINVAL;
	}

	pr_debug_once("%s : typec_power_role=%d, typec_data_role=%d, goto power role=%d\n",
									__func__, usbpd_data->typec_power_role,
									usbpd_data->typec_data_role, power_role);

	switch (power_role) {
	case TYPEC_SOURCE:
		pr_debug_once("%s : try reversing, from Sink to Source\n", __func__);
		usbpd_manager_send_pr_swap(usbpd_data->dev);
		break;
	case TYPEC_SINK:
		pr_debug_once("%s : try reversing, from Source to Sink\n", __func__);
		usbpd_manager_send_pr_swap(usbpd_data->dev);
		break;
	default :
		pr_debug_once("%s : invalid power_role\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int s2mu106_typec_dr_set(const struct typec_capability *cap, enum typec_data_role role)
{

	struct s2mu106_usbpd_data *usbpd_data = container_of(cap, struct s2mu106_usbpd_data, typec_cap);

	int timeout = 0;

	if (!usbpd_data) {
		pr_err("%s : usbpd_data is null\n", __func__);
		return -EINVAL;
	}

	pr_debug_once("%s : typec_power_role=%d, typec_data_role=%d, role=%d\n",
		__func__, usbpd_data->typec_power_role, usbpd_data->typec_data_role, role);

	if (role == TYPEC_DEVICE) {
		pr_debug_once("%s, try reversing, from DFP to UFP\n", __func__);
		usbpd_data->typec_try_state_change = TYPE_C_ATTACH_SNK;
		usbpd_manager_send_dr_swap(usbpd_data->dev);
	} else if (role == TYPEC_HOST) {
		pr_debug_once("%s, try reversing, from UFP to DFP\n", __func__);
		usbpd_data->typec_try_state_change = TYPE_C_ATTACH_SNK;
		usbpd_manager_send_dr_swap(usbpd_data->dev);
	} else {
		pr_debug_once("invalid power role\n");
		return -EIO;
	}

	if (usbpd_data->typec_try_state_change) {
		reinit_completion(&usbpd_data->role_reverse_completion);
		timeout =
		    wait_for_completion_timeout(&usbpd_data->role_reverse_completion,
						msecs_to_jiffies
						(DUAL_ROLE_SET_MODE_WAIT_MS));

		if (!timeout) {
			pr_err("%s: reverse failed\n", __func__);
			disable_irq(usbpd_data->irq);
			/* exit from Disabled state and set mode to DRP */
			usbpd_data->typec_try_state_change = 0;
			return -EIO;
		} else
			pr_err("%s: reverse success\n", __func__);
	}

	return 0;
}

int s2mu106_typec_get_pd_support(void *_data)
{
	struct s2mu106_usbpd_data *pdic_data = _data;
	struct usbpd_data *pd_data = dev_get_drvdata(pdic_data->dev);
	struct policy_data *policy = &pd_data->policy;

	if (policy->pd_support)
		return TYPEC_PWR_MODE_PD;

	return TYPEC_PWR_MODE_USB;
}

int s2mu106_typec_init(void *_data)
{
	struct s2mu106_usbpd_data *pdic_data = _data;

	pdic_data->typec_cap.revision = USB_TYPEC_REV_1_2;
	pdic_data->typec_cap.pd_revision = 0x300;
	pdic_data->typec_cap.prefer_role = TYPEC_NO_PREFERRED_ROLE;
	pdic_data->typec_cap.port_type_set = s2mu106_typec_port_type_set;
	pdic_data->typec_cap.type = TYPEC_PORT_DRP;
	pdic_data->typec_cap.pr_set = s2mu106_typec_pr_set;
	pdic_data->typec_cap.dr_set = s2mu106_typec_dr_set;
	pdic_data->port = typec_register_port(pdic_data->dev, &pdic_data->typec_cap);
	if (IS_ERR(pdic_data->port)) {
		pr_err("%s : unable to register typec_register_port\n", __func__);
		return -1;
	} else
		pr_err("%s : success typec_register_port port=%pK\n", __func__, pdic_data->port);

	init_completion(&pdic_data->role_reverse_completion);
	INIT_DELAYED_WORK(&pdic_data->typec_role_swap_work, s2mu106_typec_role_swap_check);

	return 0;
}
#endif

#if defined(CONFIG_CCIC_NOTIFIER)
static void process_dr_swap(struct s2mu106_usbpd_data *usbpd_data)
{
	struct i2c_client *i2c = usbpd_data->i2c;
	dev_dbg_once(&i2c->dev, "%s : before - is_host : %d, is_client : %d\n",
		__func__, usbpd_data->is_host, usbpd_data->is_client);
	if (usbpd_data->is_host == HOST_ON) {
		s2mu106_ccic_event_work(usbpd_data,
			CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
				0/*attach*/, USB_STATUS_NOTIFY_DETACH/*drp*/);
		s2mu106_ccic_event_work(usbpd_data, CCIC_NOTIFY_DEV_MUIC,
				CCIC_NOTIFY_ID_ATTACH, 1/*attach*/, 0/*rprd*/);
		s2mu106_ccic_event_work(usbpd_data,
			CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
				1/*attach*/, USB_STATUS_NOTIFY_ATTACH_UFP/*drp*/);
		usbpd_data->is_host = HOST_OFF;
		usbpd_data->is_client = CLIENT_ON;
	} else if (usbpd_data->is_client == CLIENT_ON) {
		s2mu106_ccic_event_work(usbpd_data,
			CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
				0/*attach*/, USB_STATUS_NOTIFY_DETACH/*drp*/);
		s2mu106_ccic_event_work(usbpd_data, CCIC_NOTIFY_DEV_MUIC,
				CCIC_NOTIFY_ID_ATTACH, 1/*attach*/, 1/*rprd*/);
		s2mu106_ccic_event_work(usbpd_data,
			CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
				1/*attach*/, USB_STATUS_NOTIFY_ATTACH_DFP/*drp*/);
		usbpd_data->is_host = HOST_ON;
		usbpd_data->is_client = CLIENT_OFF;
	}
	dev_dbg_once(&i2c->dev, "%s : after - is_host : %d, is_client : %d\n",
		__func__, usbpd_data->is_host, usbpd_data->is_client);
}
#endif

static void s2mu106_pr_swap(void *_data, int val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;

	if (val == USBPD_SINK_OFF)  {
		pd_noti.event = PDIC_NOTIFY_EVENT_PD_PRSWAP_SNKTOSRC;
		pd_noti.sink_status.selected_pdo_num = 0;
		pd_noti.sink_status.available_pdo_num = 0;
		pd_noti.sink_status.current_pdo_num = 0;
		s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_BATTERY,
			CCIC_NOTIFY_ID_POWER_STATUS, 0, 0);
	}
	else if (val == USBPD_SOURCE_ON) {
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_SRC;
#elif defined(CONFIG_TYPEC)
		pdic_data->typec_power_role = TYPEC_SOURCE;
		typec_set_pwr_role(pdic_data->port, pdic_data->typec_power_role);
#endif
		s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_MUIC,
			CCIC_NOTIFY_ID_ROLE_SWAP, 1/* source */, 0);
	}
	else if (val == USBPD_SOURCE_OFF) {
		s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_BATTERY,
			CCIC_NOTIFY_ID_POWER_STATUS, 0, 0);
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_SNK;
#elif defined(CONFIG_TYPEC)
		pdic_data->typec_power_role = TYPEC_SINK;
		typec_set_pwr_role(pdic_data->port, pdic_data->typec_power_role);
#endif
		s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_MUIC,
			CCIC_NOTIFY_ID_ROLE_SWAP, 0/* sink */, 0);
	}
}

static int s2mu106_usbpd_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	int ret, i;
	struct device *dev = &i2c->dev;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	for (i = 0; i < I2C_RETRY_CNT; i++) {
		ret = i2c_smbus_read_byte_data(i2c, reg);
		if (ret >= 0)
			break;
		pr_debug_once("%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
	}
	if (ret < 0) {
		dev_err(dev, "%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		return ret;
	}
	ret &= 0xff;
	*dest = ret;
	return 0;
}

static int s2mu106_usbpd_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	int ret, i;
	struct device *dev = &i2c->dev;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif
#ifdef CONFIG_SEC_FACTORY
	int retry = 0;
#endif

	for (i = 0; i < I2C_RETRY_CNT; i++) {
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
		if (ret >= 0)
			break;
		pr_debug_once("%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
	}
#ifdef CONFIG_SEC_FACTORY
	for (retry = 0; retry < 2; retry++) {
		if (ret < 0) {
			dev_err(dev, "%s reg(0x%x), ret(%d) retry(%d) after now\n",
							__func__, reg, ret, retry);
			msleep(40);
			ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
		} else
			break;
	}

	if (ret < 0) {
		dev_err(dev, "%s failed to read reg, ret(%d)\n", __func__, ret);
#else
	if (ret < 0) {
		dev_err(dev, "%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
#endif

#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		return ret;
	}

	return 0;
}

static int s2mu106_usbpd_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	int ret, i;
	struct device *dev = &i2c->dev;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	for (i = 0; i < I2C_RETRY_CNT; i++) {
		ret = i2c_smbus_write_byte_data(i2c, reg, value);
		if (ret >= 0)
			break;
		pr_debug_once("%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
	}
	if (ret < 0) {
		dev_err(dev, "%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
	}
	return ret;
}

static int s2mu106_usbpd_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	int ret, i;
	struct device *dev = &i2c->dev;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	for (i = 0; i < I2C_RETRY_CNT; i++) {
		ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
		if (ret >= 0)
			break;
		pr_debug_once("%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
	}
	if (ret < 0) {
		dev_err(dev, "%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		return ret;
	}
	return 0;
}

#if defined(CONFIG_UPDATE_BIT_S2MU106)
static int s2mu106_usbpd_update_bit(struct i2c_client *i2c,
			u8 reg, u8 mask, u8 shift, u8 value)
{
	int ret;
	u8 reg_val = 0;

	ret = s2mu106_usbpd_read_reg(i2c, reg, &reg_val);
	if (ret < 0) {
		pr_err("%s: Reg = 0x%X, val = 0x%X, read err : %d\n",
			__func__, reg, reg_val, ret);
	}
	reg_val &= ~mask;
	reg_val |= value << shift;
	ret = s2mu106_usbpd_write_reg(i2c, reg, reg_val);
	if (ret < 0) {
		pr_err("%s: Reg = 0x%X, mask = 0x%X, val = 0x%X, write err : %d\n",
			__func__, reg, mask, value, ret);
	}

	return ret;
}
#endif

static int s2mu106_write_msg_all(struct i2c_client *i2c, int count, u8 *buf)
{
	int ret;

	ret = s2mu106_usbpd_bulk_write(i2c, S2MU106_REG_MSG_TX_HEADER_L,
												2 + (count * 4), buf);

	return ret;
}

static int s2mu106_send_msg(struct i2c_client *i2c)
{
	int ret;
	u8 reg = S2MU106_REG_MSG_SEND_CON;
	u8 val = S2MU106_REG_MSG_SEND_CON_OP_MODE
			| S2MU106_REG_MSG_SEND_CON_SEND_MSG_EN
			| S2MU106_REG_MSG_SEND_CON_HARD_EN;

	s2mu106_usbpd_write_reg(i2c, reg, val);

	ret = s2mu106_usbpd_write_reg(i2c, reg, S2MU106_REG_MSG_SEND_CON_OP_MODE
										| S2MU106_REG_MSG_SEND_CON_HARD_EN);

	return ret;
}

static int s2mu106_read_msg_header(struct i2c_client *i2c, msg_header_type *header)
{
	int ret;

	ret = s2mu106_usbpd_bulk_read(i2c, S2MU106_REG_MSG_RX_HEADER_L, 2, header->byte);

	return ret;
}

static int s2mu106_read_msg_obj(struct i2c_client *i2c, int count, data_obj_type *obj)
{
	int ret = 0;
	int i = 0;
	struct device *dev = &i2c->dev;

	if (count > S2MU106_MAX_NUM_MSG_OBJ) {
		dev_err(dev, "%s, not invalid obj count number\n", __func__);
		ret = -EINVAL; /*TODO: check fail case */
	} else {
		for (i = 0; i < count; i++) {
			ret = s2mu106_usbpd_bulk_read(i2c,
				S2MU106_REG_MSG_RX_OBJECT0_0_L + (4 * i),
							4, obj[i].byte);
		}
	}

	return ret;
}

static void s2mu106_set_irq_enable(struct s2mu106_usbpd_data *_data,
		u8 int0, u8 int1, u8 int2, u8 int3, u8 int4, u8 int5)
{
	u8 int_mask[S2MU106_MAX_NUM_INT_STATUS]
		= {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	int ret = 0;
	int i;
	struct i2c_client *i2c = _data->i2c;
	struct device *dev = &i2c->dev;

	pr_debug_once("%s, enter, en : %d\n", __func__, int0);

	int_mask[0] &= ~int0;
	int_mask[1] &= ~int1;
	int_mask[2] &= ~int2;
	int_mask[3] &= ~int3;
	int_mask[4] &= ~int4;
	int_mask[5] &= ~int5;

	for (i = 0; i < I2C_RETRY_CNT; i++) {
		ret = i2c_smbus_write_i2c_block_data(i2c, S2MU106_REG_INT_MASK0,
				S2MU106_MAX_NUM_INT_STATUS, int_mask);
		if (ret >= 0)
			break;
		pr_debug_once("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, S2MU106_REG_INT_MASK0, ret);
	}
	if (ret < 0)
		dev_err(dev, "err write interrupt mask \n");
}

static void s2mu106_self_soft_reset(struct i2c_client *i2c)
{
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_ETC,
			S2MU106_REG_ETC_SOFT_RESET_EN);
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_ETC,
			S2MU106_REG_ETC_SOFT_RESET_DIS);
}

static void s2mu106_driver_reset(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	int i;

	pdic_data->status_reg = 0;
	data->wait_for_msg_arrived = 0;
	pdic_data->header.word = 0;
	for (i = 0; i < S2MU106_MAX_NUM_MSG_OBJ; i++)
		pdic_data->obj[i].object = 0;

	s2mu106_set_irq_enable(pdic_data, ENABLED_INT_0, ENABLED_INT_1,
			ENABLED_INT_2, ENABLED_INT_3, ENABLED_INT_4, ENABLED_INT_5);
}

static void s2mu106_assert_drp(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, &val);
	val &= ~S2MU106_REG_PLUG_CTRL_FSM_MANUAL_EN;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, val);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, &val);
	val &= ~S2MU106_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, val);
}

static void s2mu106_assert_rd(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	if (pdic_data->cc1_val == 2) {
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, &val);
		val = (val & ~S2MU106_REG_PLUG_CTRL_CC_MANUAL_MASK) |
				S2MU106_REG_PLUG_CTRL_CC1_MANUAL_ON;
		s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, val);

		if (pdic_data->vconn_en) {
			s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, &val);
			val = (val & ~S2MU106_REG_PLUG_CTRL_CC_MANUAL_MASK) |
					S2MU106_REG_PLUG_CTRL_RpRd_CC2_VCONN |
					S2MU106_REG_PLUG_CTRL_VCONN_MANUAL_EN;
			s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, val);
		}
	}

	if (pdic_data->cc2_val == 2) {
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, &val);
		val = (val & ~S2MU106_REG_PLUG_CTRL_CC_MANUAL_MASK) |
				S2MU106_REG_PLUG_CTRL_CC2_MANUAL_ON;
		s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, val);

		if (pdic_data->vconn_en) {
			s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, &val);
			val = (val & ~S2MU106_REG_PLUG_CTRL_CC_MANUAL_MASK) |
					S2MU106_REG_PLUG_CTRL_RpRd_CC1_VCONN |
					S2MU106_REG_PLUG_CTRL_VCONN_MANUAL_EN;
			s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, val);
		}
	}

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, &val);
	val &= ~S2MU106_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	val |= S2MU106_REG_PLUG_CTRL_FSM_ATTACHED_SNK;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, val);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, &val);
	val |= S2MU106_REG_PLUG_CTRL_FSM_MANUAL_EN;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, val);
}

static void s2mu106_assert_rp(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, &val);
	val &= ~S2MU106_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	val |= S2MU106_REG_PLUG_CTRL_FSM_ATTACHED_SRC;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, val);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, &val);
	val |= S2MU106_REG_PLUG_CTRL_FSM_MANUAL_EN;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, val);
}

static unsigned s2mu106_get_status(void *_data, u64 flag)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	u64 one = 1;

	if (pdic_data->status_reg & (one << flag)) {
		pdic_data->status_reg &= ~(one << flag); /* clear the flag */
		return 1;
	} else {
		return 0;
	}
}

static bool s2mu106_poll_status(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	u8 intr[S2MU106_MAX_NUM_INT_STATUS] = {0};
	int ret = 0, retry = 0;
	u64 status_reg_val = 0;

	msg_header_type header;
	int data_obj_num = 0;
	int msg_id = 0;
	u8 reg_val = 0, rd_val = 0, rp_val = 0;

	ret = s2mu106_usbpd_bulk_read(i2c, S2MU106_REG_INT_STATUS0,
			S2MU106_MAX_NUM_INT_STATUS, intr);

	dev_dbg_once(dev, "%s status[0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x]\n",
			__func__, intr[0], intr[1], intr[2], intr[3], intr[4], intr[5], intr[6]);

	if ((intr[0] | intr[1] | intr[2] | intr[3] | intr[4] | intr[5]) == 0)
		goto out;

	if ((intr[2] & S2MU106_REG_INT_STATUS2_WAKEUP) ||
		(intr[4] & S2MU106_REG_INT_STATUS4_CC12_DET_IRQ)) {
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_SET_RD, &rd_val);
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_SET_RP, &rp_val);
		if (rd_val == S2MU106_THRESHOLD_428MV && rp_val == S2MU106_THRESHOLD_1628MV) {
			dev_dbg_once(dev, "%s, watchdog reset occurred! ccoff, 257mV/2057mV\n", __func__);
			/* CC manual OFF */
			s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, &reg_val);
			reg_val &= ~S2MU106_REG_PLUG_CTRL_CC_MANUAL_MASK;
			reg_val |= S2MU106_REG_PLUG_CTRL_CC_MANUAL_EN;
			s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, reg_val);

			/* threshold set 257mV / 2057mV */
			s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_SET_RD, S2MU106_THRESHOLD_257MV | 0x40);
			s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_SET_RP, S2MU106_THRESHOLD_MAX);

			/* Wakeup, set interrupt mask */
			s2mu106_set_normal_mode(pdic_data);

			ret = s2mu106_usbpd_bulk_read(i2c, S2MU106_REG_INT_STATUS0, S2MU106_MAX_NUM_INT_STATUS, intr);
			dev_dbg_once(dev, "%s status[0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x]\n",
					__func__, intr[0], intr[1], intr[2], intr[3], intr[4], intr[5], intr[6]);
		}
	}

	/* when occur detach & attach atomic */
	if (intr[4] & S2MU106_REG_INT_STATUS4_USB_DETACH) {
		status_reg_val |= 1 << PLUG_DETACH;
	}

	mutex_lock(&pdic_data->lpm_mutex);
	if ((intr[4] & S2MU106_REG_INT_STATUS4_PLUG_IRQ) &&
			!pdic_data->lpm_mode && !pdic_data->is_water_detect)
		status_reg_val |= 1 << PLUG_ATTACH;
	else if (pdic_data->lpm_mode &&
				(intr[4] & S2MU106_REG_INT_STATUS4_PLUG_IRQ) &&
									!pdic_data->is_water_detect)
		retry = 1;
	mutex_unlock(&pdic_data->lpm_mutex);

	if (retry) {
		msleep(40);
		mutex_lock(&pdic_data->lpm_mutex);
		if ((intr[4] & S2MU106_REG_INT_STATUS4_PLUG_IRQ) &&
				!pdic_data->lpm_mode && !pdic_data->is_water_detect)
			status_reg_val |= 1 << PLUG_ATTACH;
		mutex_unlock(&pdic_data->lpm_mutex);
	}

	if (intr[5] & S2MU106_REG_INT_STATUS5_HARD_RESET)
		status_reg_val |= 1 << MSG_HARDRESET;

	if (intr[0] & S2MU106_REG_INT_STATUS0_MSG_GOODCRC)
		status_reg_val |= 1 << MSG_GOODCRC;

	if (intr[1] & S2MU106_REG_INT_STATUS1_MSG_PR_SWAP)
		status_reg_val |= 1 << MSG_PR_SWAP;

	if (intr[2] & S2MU106_REG_INT_STATUS2_MSG_SOFTRESET)
		status_reg_val |= 1 << MSG_SOFTRESET;

	if (intr[1] & S2MU106_REG_INT_STATUS1_MSG_DR_SWAP)
		status_reg_val |= 1 << MSG_DR_SWAP;

	if (intr[0] & S2MU106_REG_INT_STATUS0_MSG_ACCEPT)
		status_reg_val |= 1 << MSG_ACCEPT;

	if (intr[1] & S2MU106_REG_INT_STATUS1_MSG_PSRDY)
		status_reg_val |= 1 << MSG_PSRDY;

#if 0
	if (intr[2] & S2MU106_REG_INT_STATUS2_MSG_REQUEST)
		status_reg_val |= 1 << MSG_REQUEST;
#endif

	if (intr[1] & S2MU106_REG_INT_STATUS1_MSG_REJECT)
		status_reg_val |= 1 << MSG_REJECT;

	if (intr[2] & S2MU106_REG_INT_STATUS2_MSG_WAIT)
		status_reg_val |= 1 << MSG_WAIT;

	if (intr[4] & S2MU106_REG_INT_STATUS4_MSG_ERROR)
		status_reg_val |= 1 << MSG_ERROR;

	if (intr[1] & S2MU106_REG_INT_STATUS1_MSG_PING)
		status_reg_val |= 1 << MSG_PING;

	if (intr[2] & S2MU106_REG_INT_STATUS2_MSG_VCONN_SWAP)
		status_reg_val |= 1 << MSG_VCONN_SWAP;

	if (intr[3] & S2MU106_REG_INT_STATUS3_UNS_CMD_DATA) {
		if (pdic_data->detach_valid)
			status_reg_val |= 1 << PLUG_ATTACH;
		status_reg_val |= 1 << MSG_RID;
	}

	/* function that support dp control */
	if (intr[4] & S2MU106_REG_INT_STATUS4_MSG_PASS) {
		if ((intr[3] & S2MU106_REG_INT_STATUS3_UNS_CMD_DATA) == 0) {
			usbpd_protocol_rx(data);
			header = data->protocol_rx.msg_header;
			data_obj_num = header.num_data_objs;
			msg_id = header.msg_id;
			pr_debug_once("%s, prev msg_id =(%d), received msg_id =(%d)\n", __func__,
										data->msg_id, msg_id);

			if (header.num_data_objs > 0 && header.msg_type == USBPD_Alert) {
				if (data->alert_msg_id == msg_id) {
					pr_debug_once("%s, alert ignored by same msg_id(%d)\n", __func__, msg_id);
					goto out;
				}
				else
					data->alert_msg_id = msg_id;
			} else {
				data->alert_msg_id = USBPD_nMessageIDCount + 1;
			}
#if 0
			if (msg_id == data->msg_id)
				goto out;
			else
				data->msg_id = msg_id;
#endif
			s2mu106_usbpd_check_msg(data, &status_reg_val);

#if 0
			if (intr[2] & S2MU106_REG_INT_STATUS2_MSG_REQUEST)
				status_reg_val |= 1 << MSG_REQUEST;
#endif
		}
	}
out:
	pdic_data->status_reg |= status_reg_val;

	return 0;
}

static void s2mu106_soft_reset(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;

	s2mu106_self_soft_reset(i2c);
}

static int s2mu106_hard_reset(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	int ret;
	u8 reg;
	u8 Read_Value = 0;

	pr_debug_once("%s, \n", __func__);

	if (pdic_data->rid != REG_RID_UNDF && pdic_data->rid != REG_RID_MAX)
		return 0;

	reg = S2MU106_REG_MSG_SEND_CON;

	ret = s2mu106_usbpd_write_reg(i2c, reg, S2MU106_REG_MSG_SEND_CON_SOP_HardRST
			| S2MU106_REG_MSG_SEND_CON_OP_MODE);
	if (ret < 0)
		goto fail;

	ret = s2mu106_usbpd_write_reg(i2c, reg, S2MU106_REG_MSG_SEND_CON_SOP_HardRST
			| S2MU106_REG_MSG_SEND_CON_OP_MODE
			| S2MU106_REG_MSG_SEND_CON_SEND_MSG_EN);
	if (ret < 0)
		goto fail;

	usleep_range(1500,1500);

    /* USB PD CC Off*/
	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, &Read_Value);
	Read_Value &= ~S2MU106_REG_PLUG_CTRL_CC_MANUAL_MASK;
	Read_Value |= S2MU106_REG_PLUG_CTRL_CC_MANUAL_EN;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, Read_Value);

	ret = s2mu106_usbpd_write_reg(i2c, reg, S2MU106_REG_MSG_SEND_CON_OP_MODE
										| S2MU106_REG_MSG_SEND_CON_HARD_EN);
	udelay(1);
	ret = s2mu106_usbpd_write_reg(i2c, reg, S2MU106_REG_MSG_SEND_CON_HARD_EN);
	if (ret < 0)
		goto fail;

	usleep_range(2000, 2100);

	Read_Value &= ~S2MU106_REG_PLUG_CTRL_CC_MANUAL_MASK;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, Read_Value);

	s2mu106_self_soft_reset(i2c);

	pdic_data->status_reg = 0;

	return 0;

fail:
	return -EIO;
}

#if defined(CONFIG_PDIC_PD30)
static void s2mu106_send_ocp_info(void *_data)
{
	struct power_supply *psy_battery;
	union power_supply_propval val;

	psy_battery = get_power_supply_by_name("battery");

	if (psy_battery) {
		val.intval = 1;
#if defined(CONFIG_DIRECT_CHARGING)
		psy_battery->desc->set_property(psy_battery,
	(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DIRECT_TA_ALERT, &val);
#endif
	} else
		pr_err("%s: Fail to get psy battery\n", __func__);
}
#endif

static int s2mu106_receive_message(void *data)
{
	struct s2mu106_usbpd_data *pdic_data = data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	int obj_num = 0;
	int ret = 0;

	ret = s2mu106_read_msg_header(i2c, &pdic_data->header);
	if (ret < 0)
		dev_err(dev, "%s read msg header error\n", __func__);

	obj_num = pdic_data->header.num_data_objs;

	if (obj_num > 0) {
		ret = s2mu106_read_msg_obj(i2c,
			obj_num, &pdic_data->obj[0]);
	}

	return ret;
}

static int s2mu106_tx_msg(void *_data,
		msg_header_type *header, data_obj_type *obj)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	int ret = 0;
	int i = 0;
	int count = 0;
	u8 send_msg[30];

	pr_debug_once("%s, \n", __func__);

	/* if there is no attach, skip tx msg */
	if (pdic_data->detach_valid)
		goto done;

#if 0 /* skip to reduce time delay */
	/* using msg id counter at s2mu106 */
	s2mu106_usbpd_read_reg(pdic_data->i2c, S2MU106_REG_ID_MONITOR, &reg_data);
	msg_id = reg_data & S2MU106_REG_ID_MONITOR_MSG_ID_MASK;
	header->msg_id = msg_id;
#endif
	send_msg[0] = header->byte[0];
	send_msg[1] = header->byte[1];

	count = header->num_data_objs;

	for (i = 0; i < count; i++) {
		send_msg[2 + (i * 4)] = obj[i].byte[0];
		send_msg[3 + (i * 4)] = obj[i].byte[1];
		send_msg[4 + (i * 4)] = obj[i].byte[2];
		send_msg[5 + (i * 4)] = obj[i].byte[3];
	}

	ret = s2mu106_write_msg_all(i2c, count, send_msg);
	if (ret < 0)
		goto done;

	s2mu106_send_msg(i2c);

done:
	return ret;
}

static int s2mu106_rx_msg(void *_data,
		msg_header_type *header, data_obj_type *obj)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	int i;
	int count = 0;

	if (!s2mu106_receive_message(pdic_data)) {
		header->word = pdic_data->header.word;
		count = pdic_data->header.num_data_objs;
		if (count > 0) {
			for (i = 0; i < count; i++)
				obj[i].object = pdic_data->obj[i].object;
		}
		pdic_data->header.word = 0; /* To clear for duplicated call */
		return 0;
	} else {
		return -EINVAL;
	}
}

static int s2mu106_set_otg_control(void *_data, int val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;

	mutex_lock(&pdic_data->cc_mutex);
	if (val){
		if (pdic_data->is_killer == 0)
			s2mu106_vbus_turn_on_ctrl(pdic_data, VBUS_ON);
	}else
		s2mu106_vbus_turn_on_ctrl(pdic_data, VBUS_OFF);
	mutex_unlock(&pdic_data->cc_mutex);

	return 0;
}

static int s2mu106_set_chg_lv_mode(void *_data, int voltage)
{
	struct power_supply *psy_charger;
	union power_supply_propval val;
	int ret = 0;

	psy_charger = get_power_supply_by_name("s2mu106-charger");
	if (psy_charger == NULL) {
		pr_err("%s: Fail to get psy charger\n", __func__);
		return -1;
	}

	if (voltage == 5) {
		val.intval = 0;
	} else if (voltage == 9) {
		val.intval = 1;
	} else {
		pr_err("%s: invalid pram:%d\n", __func__, voltage);
		return -1;
	}

	ret = psy_charger->desc->set_property(psy_charger,
		POWER_SUPPLY_PROP_2LV_3LV_CHG_MODE, &val);

	if (ret)
		pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
			__func__, ret);

	return ret;
}

static int s2mu106_set_cc_control(void *_data, int val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	int ret = 0;

	mutex_lock(&pdic_data->cc_mutex);
	ret = s2mu106_usbpd_set_cc_control(pdic_data, val);
	mutex_unlock(&pdic_data->cc_mutex);

	return ret;
}

static void s2mu106_send_pd_info(void *_data, int attach)
{
#if defined(CONFIG_CCIC_NOTIFIER)
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;

	if (attach)
		s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_BATTERY,
								CCIC_NOTIFY_ID_POWER_STATUS, 1, 0);
	else
		s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_BATTERY,
								CCIC_NOTIFY_ID_POWER_STATUS, 0, 0);
#endif
}

#if defined(CONFIG_TYPEC)
static void s2mu106_set_pwr_opmode(void *_data, int mode)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;

	typec_set_pwr_opmode(pdic_data->port, mode);
}
#endif

static int s2mu106_set_rp_control(void *_data, int val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;

	mutex_lock(&pdic_data->cc_mutex);
	s2mu106_usbpd_set_rp_scr_sel(pdic_data, val);
	mutex_unlock(&pdic_data->cc_mutex);

	return 0;
}

static int  s2mu106_cc_instead_of_vbus(void *_data, int enable)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val;

	if(pdic_data->cc_instead_of_vbus == enable)
		return 0;

	pdic_data->cc_instead_of_vbus = enable;

	//Setting for CC Detection with VBUS
	//It is recognized that VBUS falls when CC line falls.
	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_VBUS_MUX, &val);
	val &= ~S2MU106_REG_RD_OR_VBUS_MUX_SEL;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_VBUS_MUX, val);
	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL, &val);
	if (enable)
		val |= S2MU106_REG_PLUG_CTRL_REG_UFP_ATTACH_OPT_EN;
	else
		val &= ~S2MU106_REG_PLUG_CTRL_REG_UFP_ATTACH_OPT_EN;

	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL, val);

	return 0;
}

static int  s2mu106_op_mode_clear(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;

	u8 reg = S2MU106_REG_MSG_SEND_CON;
	u8 val = 0;

	val &= ~S2MU106_REG_MSG_SEND_CON_OP_MODE;

	s2mu106_usbpd_write_reg(i2c, reg, val);

	return 0;
}

static int s2mu106_vbus_on_check(void *_data)
{
#ifdef CONFIG_PM_S2MU106
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;

	return s2mu106_usbpd_check_vbus(pdic_data, 4300, VBUS_ON);
#else
	return 0;
#endif
}

#if defined(CONFIG_CHECK_CTYPE_SIDE) || defined(CONFIG_CCIC_SYSFS)
static int s2mu106_get_side_check(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val, cc1_val, cc2_val;

	s2mu106_usbpd_test_read(pdic_data);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_MON1, &val);

	cc1_val = val & S2MU106_REG_CTRL_MON_CC1_MASK;
	cc2_val = (val & S2MU106_REG_CTRL_MON_CC2_MASK) >> S2MU106_REG_CTRL_MON_CC2_SHIFT;

	if (cc1_val == USBPD_Rd)
		return USBPD_UP_SIDE;
	else if (cc2_val == USBPD_Rd)
		return USBPD_DOWN_SIDE;
	else
		return USBPD_UNDEFFINED_SIDE;
}
#endif
static int s2mu106_set_vconn_source(void *_data, int val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 reg_data = 0, reg_val = 0, cc1_val = 0, cc2_val = 0;

	if (!pdic_data->vconn_en) {
		pr_err("%s, not support vconn source\n", __func__);
		return -1;
	}

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_MON1, &reg_val);
	cc1_val = (reg_val & S2MU106_REG_CTRL_MON_CC1_MASK) >> S2MU106_REG_CTRL_MON_CC1_SHIFT;
	cc2_val = (reg_val & S2MU106_REG_CTRL_MON_CC2_MASK) >> S2MU106_REG_CTRL_MON_CC2_SHIFT;

	if (val == USBPD_VCONN_ON) {
		if (cc1_val == USBPD_Rd) {
			if (cc2_val == USBPD_Ra) {
				s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, &reg_data);
				reg_data &= ~S2MU106_REG_PLUG_CTRL_RpRd_VCONN_MASK;
				reg_data |= (S2MU106_REG_PLUG_CTRL_RpRd_CC2_VCONN |
						S2MU106_REG_PLUG_CTRL_VCONN_MANUAL_EN);
				s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, reg_data);
			}
		}
		if (cc2_val == USBPD_Rd) {
			if (cc1_val == USBPD_Ra) {
				s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, &reg_data);
				reg_data &= ~S2MU106_REG_PLUG_CTRL_RpRd_VCONN_MASK;
				reg_data |= (S2MU106_REG_PLUG_CTRL_RpRd_CC1_VCONN |
						S2MU106_REG_PLUG_CTRL_VCONN_MANUAL_EN);
				s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, reg_data);
			}
		}
	} else if (val == USBPD_VCONN_OFF) {
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, &reg_data);
				reg_data &= ~S2MU106_REG_PLUG_CTRL_RpRd_VCONN_MASK;
		reg_data |= S2MU106_REG_PLUG_CTRL_VCONN_MANUAL_EN;
		s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, reg_data);
	} else
		return(-1);

	pdic_data->vconn_source = val;
	return 0;
}

static void s2mu106_usbpd_set_vconn_manual(struct s2mu106_usbpd_data *pdic_data, bool enable)
{
	u8 reg_data = 0;

	s2mu106_usbpd_read_reg(pdic_data->i2c, S2MU106_REG_PLUG_CTRL_RpRd, &reg_data);
	reg_data &= ~S2MU106_REG_PLUG_CTRL_RpRd_VCONN_MASK;

	if (enable)
		reg_data |= S2MU106_REG_PLUG_CTRL_VCONN_MANUAL_EN;

	s2mu106_usbpd_write_reg(pdic_data->i2c, S2MU106_REG_PLUG_CTRL_RpRd, reg_data);
}

static int s2mu106_get_vconn_source(void *_data, int *val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;

	/* TODO
		set s2mu106 pdic register control */

	if (pdic_data->vconn_source != *val) {
		dev_dbg_once(pdic_data->dev, "%s, vconn_source(%d) != gpio val(%d)\n",
				__func__, pdic_data->vconn_source, *val);
		pdic_data->vconn_source = *val;
	}

	return 0;
}

/* val : sink(0) or source(1) */
static int s2mu106_set_power_role(void *_data, int val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;

	pr_debug_once("%s, power_role(%d)\n", __func__, val);

	if (val == USBPD_SINK) {
		pdic_data->is_pr_swap = true;
		s2mu106_assert_rd(data);
		s2mu106_snk(pdic_data->i2c);
	} else if (val == USBPD_SOURCE) {
		pdic_data->is_pr_swap = true;
		s2mu106_assert_rp(data);
		s2mu106_src(pdic_data->i2c);
	} else if (val == USBPD_DRP) {
		pdic_data->is_pr_swap = false;
		s2mu106_assert_drp(data);
		return 0;
	} else
		return(-1);

	pdic_data->power_role = val;
	return 0;
}

static int s2mu106_get_power_role(void *_data, int *val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	*val = pdic_data->power_role;
	return 0;
}

static int s2mu106_set_data_role(void *_data, int val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val_port, data_role;

	/* DATA_ROLE (0x18[2])
	 * 0 : UFP
	 * 1 : DFP
	 */
	if (val == USBPD_UFP) {
		data_role = S2MU106_REG_MSG_DATA_ROLE_UFP;
		s2mu106_ufp(i2c);
	} else {/* (val == USBPD_DFP) */
		data_role = S2MU106_REG_MSG_DATA_ROLE_DFP;
		s2mu106_dfp(i2c);
	}

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_MSG, &val_port);
	val_port = (val_port & ~S2MU106_REG_MSG_DATA_ROLE_MASK) | data_role;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_MSG, val_port);

	pdic_data->data_role = val;

#if defined(CONFIG_CCIC_NOTIFIER)
	process_dr_swap(pdic_data);
#endif
	return 0;
}

static int s2mu106_get_data_role(void *_data, int *val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	*val = pdic_data->data_role;
	return 0;
}

static void s2mu106_get_vbus_short_check(void *_data, bool *val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;
	*val = pdic_data->vbus_short;
}

static void s2mu106_pd_vbus_short_check(void *_data)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	struct s2mu106_usbpd_data *pdic_data = data->phy_driver_data;

	if (pdic_data->pd_vbus_short_check)
		return;

	pdic_data->vbus_short_check = false;

	s2mu106_vbus_short_check(pdic_data);

	pdic_data->pd_vbus_short_check = true;
}

static void s2mu106_usbpd_set_threshold(struct s2mu106_usbpd_data *pdic_data,
			CCIC_RP_RD_SEL port_sel, CCIC_THRESHOLD_SEL threshold_sel)
{
	struct i2c_client *i2c = pdic_data->i2c;

	if (threshold_sel > S2MU106_THRESHOLD_MAX) {
		dev_err(pdic_data->dev, "%s : threshold overflow!!\n", __func__);
		return;
	} else {
		if (port_sel == PLUG_CTRL_RD)
			s2mu106_usbpd_write_reg(i2c,
				S2MU106_REG_PLUG_CTRL_SET_RD, threshold_sel | 0x40);
		else if (port_sel == PLUG_CTRL_RP)
			s2mu106_usbpd_write_reg(i2c,
				S2MU106_REG_PLUG_CTRL_SET_RP, threshold_sel);
	}
}

static int s2mu106_usbpd_check_abnormal_attach(struct s2mu106_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data = 0;

	s2mu106_usbpd_set_threshold(pdic_data, PLUG_CTRL_RP,
										S2MU106_THRESHOLD_1628MV);
	msleep(20);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_MON2, &data);
	if ((data & S2MU106_PR_MASK) == S2MU106_PDIC_SOURCE)
		return true;
	else
		return false;
}

static void s2mu106_usbpd_set_rp_scr_sel(struct s2mu106_usbpd_data *pdic_data,
							CCIC_RP_SCR_SEL scr_sel)
{
	struct i2c_client *i2c = pdic_data->i2c;

	u8 data = 0;
	pr_debug_once("%s: prev_sel(%d), scr_sel : (%d)\n", __func__,
										pdic_data->rp_lvl, scr_sel);

	if (pdic_data->detach_valid) {
		dev_dbg_once(pdic_data->dev, "%s, ignore rp control\n", __func__);
		return;
	}

	if (pdic_data->rp_lvl == scr_sel)
		return;
	pdic_data->rp_lvl = scr_sel;

	switch (scr_sel) {
	case PLUG_CTRL_RP0:
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, &data);
		data &= ~S2MU106_REG_PLUG_CTRL_RP_SEL_MASK;
		data |= S2MU106_REG_PLUG_CTRL_RP0;
		s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, data);
		break;
	case PLUG_CTRL_RP80:
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, &data);
		data &= ~S2MU106_REG_PLUG_CTRL_RP_SEL_MASK;
		data |= S2MU106_REG_PLUG_CTRL_RP80;
		s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, data);
		s2mu106_usbpd_set_threshold(pdic_data, PLUG_CTRL_RD,
						S2MU106_THRESHOLD_214MV);
#if 0
		s2mu106_usbpd_set_threshold(pdic_data, PLUG_CTRL_RP,
						S2MU106_THRESHOLD_1628MV);
#endif
		break;
	case PLUG_CTRL_RP180:
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, &data);
		data &= ~S2MU106_REG_PLUG_CTRL_RP_SEL_MASK;
		data |= S2MU106_REG_PLUG_CTRL_RP180;
		s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, data);
		s2mu106_usbpd_set_threshold(pdic_data, PLUG_CTRL_RD,
						S2MU106_THRESHOLD_428MV);
#if 0
		s2mu106_usbpd_set_threshold(pdic_data, PLUG_CTRL_RP,
						S2MU106_THRESHOLD_2057MV);
#endif
		break;
	case PLUG_CTRL_RP330:
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, &data);
		data &= ~S2MU106_REG_PLUG_CTRL_RP_SEL_MASK;
		data |= S2MU106_REG_PLUG_CTRL_RP330;
		s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, data);
		s2mu106_usbpd_set_threshold(pdic_data, PLUG_CTRL_RD,
						S2MU106_THRESHOLD_814MV);
#if 0
		s2mu106_usbpd_set_threshold(pdic_data, PLUG_CTRL_RP,
						S2MU106_THRESHOLD_2057MV);
#endif
		break;
	default:
		break;
	}
	return;
}

int s2mu106_usbpd_check_msg(void *_data, u64 *val)
{
	struct usbpd_data *data = (struct usbpd_data *) _data;
	int data_type = 0;
	int msg_type = 0;
	int vdm_type = 0;
	int vdm_command = 0;
	u64 shift = 0;

	u64 one = 1;

	dev_dbg_once(data->dev, "%s\n", __func__);

	if (data->protocol_rx.msg_header.num_data_objs == 0)
		data_type = USBPD_CTRL_MSG;
	else if (data->protocol_rx.msg_header.extended == 0)
		data_type = USBPD_DATA_MSG;
	else if (data->protocol_rx.msg_header.extended == 1)
		data_type = USBPD_EXTENDED_MSG;

	msg_type = data->protocol_rx.msg_header.msg_type;

	/* Control Message */
	if (data_type == USBPD_CTRL_MSG) {
		switch (msg_type) {
		case USBPD_Get_Sink_Cap:
			shift = MSG_GET_SNK_CAP;
			*val |=one << shift;
			break;
		case USBPD_Get_Source_Cap:
			shift = MSG_GET_SRC_CAP;
			*val |=one << shift;
			break;
		case USBPD_Ping:
			shift = MSG_PING;
			*val |=one << shift;
			break;
		case USBPD_VCONN_Swap:
			shift = MSG_VCONN_SWAP;
			*val |=one << shift;
			break;
		case USBPD_Wait:
			shift = MSG_WAIT;
			*val |=one << shift;
			break;
		case USBPD_Soft_Reset:
			shift = MSG_SOFTRESET;
			*val |=one << shift;
			break;
		case USBPD_Not_Supported:
			shift = MSG_NOT_SUPPORTED;
			*val |=one << shift;
			break;
		case USBPD_Get_Source_Cap_Extended:
			shift = MSG_GET_SOURCE_CAP_EXTENDED;
			*val |=one << shift;
			break;
		case USBPD_Get_Status:
			shift = MSG_GET_STATUS;
			*val |=one << shift;
			break;
		case USBPD_FR_Swap:
			/* Accept bit Clear */
			shift = MSG_FR_SWAP;
			*val |= one << shift;
			break;
		case USBPD_Get_PPS_Status:
			shift = MSG_GET_PPS_STATUS;
			*val |=one << shift;
			break;
		case USBPD_Get_Country_Codes:
			shift = MSG_GET_COUNTRY_CODES;
			*val |=one << shift;
			break;
		case USBPD_Get_Sink_Cap_Extended:
			shift = MSG_GET_SINK_CAP_EXTENDED;
			*val |=one << shift;
			break;
		case 14:
		case 15:
		case 23 ... 31: /* Reserved */
			shift = MSG_RESERVED;
			*val |=one << shift;
			break;
		}
	}

	/* Data Message */
	if (data_type == USBPD_DATA_MSG) {
		switch (msg_type) {
		case USBPD_Source_Capabilities:
			*val |= one << MSG_SRC_CAP;
			break;
		case USBPD_Request:
			*val |= one << MSG_REQUEST;
			break;
		case USBPD_BIST:
			*val |= one << MSG_BIST;
			break;
		case USBPD_Sink_Capabilities:
			*val |= one << MSG_SNK_CAP;
			break;
		case USBPD_Battery_Status:
			shift = MSG_BATTERY_STATUS;
			*val |= one << shift;
			break;
		case USBPD_Alert:
			shift = MSG_ALERT;
			*val |= one << shift;
			break;
		case USBPD_Get_Country_Info:
			shift = MSG_GET_COUNTRY_INFO;
			*val |= one << shift;
			break;
		case USBPD_Vendor_Defined:
			vdm_command = data->protocol_rx.data_obj[0].structured_vdm.command;
			vdm_type = data->protocol_rx.data_obj[0].structured_vdm.vdm_type;

			if (vdm_type == Unstructured_VDM) {
				if(data->protocol_rx.data_obj[0].unstructured_vdm.vendor_id!= SAMSUNG_VENDOR_ID){
					*val |= one << MSG_RESERVED;
					break;
				}
				dev_dbg_once(data->dev, "%s : uvdm msg received!\n", __func__);
				*val |= one << UVDM_MSG;
				break;
			}
			switch (vdm_command) {
				case DisplayPort_Status_Update:
					*val |= one << VDM_DP_STATUS_UPDATE;
					break;
				case DisplayPort_Configure:
					*val |= one << VDM_DP_CONFIGURE;
					break;
				case Attention:
					*val |= one << VDM_ATTENTION;
					break;
				case Exit_Mode:
					*val |= one << VDM_EXIT_MODE;
					break;
				case Enter_Mode:
					*val |= one << VDM_ENTER_MODE;
					break;
				case Discover_Modes:
					*val |= one << VDM_DISCOVER_MODE;
					break;
				case Discover_SVIDs:
					*val |= one << VDM_DISCOVER_SVID;
					break;
				case Discover_Identity:
					*val |= one << VDM_DISCOVER_IDENTITY;
					break;
				default:
					break;
			}
			break;
		case 0: /* Reserved */
		case 8 ... 0xe:
			shift = MSG_RESERVED;
			*val |= one << shift;
			break;
		}
	}

	/* Extended Message */
	if (data_type == USBPD_EXTENDED_MSG) {
		//MQP : PROT-SNK3-PPS
		if ((data->protocol_rx.data_obj[0].extended_msg_header_type.chunked)
			&& (data->protocol_rx.data_obj[0].extended_msg_header_type.data_size > 24)) {
			shift = MSG_RESERVED;
			*val |= one << shift;
			return 0;
		}
		switch (msg_type){
		case USBPD_Source_Capabilities_Extended:
			shift = MSG_SOURCE_CAPABILITIES_EXTENDED;
			*val |= one << shift;
			break;
		case USBPD_Status:
			shift = MSG_STATUS;
			*val |= one << shift;
			break;
		case USBPD_Get_Battery_Cap:
			shift = MSG_GET_BATTERY_CAP;
			*val |= one << shift;
			break;
		case USBPD_Get_Battery_Status:
			shift = MSG_GET_BATTERY_STATUS;
			*val |= one << shift;
			break;
		case USBPD_Battery_Capabilities:
			shift = MSG_BATTERY_CAPABILITIES;
			*val |= one << shift;
			break;
		case USBPD_Get_Manufacturer_Info:
			shift = MSG_GET_MANUFACTURER_INFO;
			*val |= one << shift;
			break;
		case USBPD_Manufacturer_Info:
			shift = MSG_MANUFACTURER_INFO;
			*val |= one << shift;
			break;
		case USBPD_Security_Request:
			shift = MSG_SECURITY_REQUEST;
			*val |= one << shift;
			break;
		case USBPD_Security_Response:
			shift = MSG_SECURITY_RESPONSE;
			*val |= one << shift;
			break;
		case USBPD_Firmware_Update_Request:
			shift = MSG_FIRMWARE_UPDATE_REQUEST;
			*val |= one << shift;
			break;
		case USBPD_Firmware_Update_Response:
			shift = MSG_FIRMWARE_UPDATE_RESPONSE;
			*val |= one << shift;
			break;
		case USBPD_PPS_Status:
			shift = MSG_PPS_STATUS;
			*val |= one << shift;
			break;
		case USBPD_Country_Info:
			shift = MSG_COUNTRY_INFO;
			*val |= one << shift;
			break;
		case USBPD_Country_Codes:
			shift = MSG_COUNTRY_CODES;
			*val |= one << shift;
			break;
		case USBPD_Sink_Capabilities_Extended:
			shift = MSG_SINK_CAPABILITIES_EXTENDED;
			*val |= one << shift;
			break;
		default: /* Reserved */
			shift = MSG_RESERVED;
			*val |= one << shift;
			break;
		}
	}

	dev_dbg_once(data->dev, "%s: msg status(%llu)\n", __func__, *val);

	return 0;
}

static int s2mu106_usbpd_set_cc_control(struct s2mu106_usbpd_data  *pdic_data, int val)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data = 0;

	dev_dbg_once(pdic_data->dev, "%s, (%d)\n", __func__, val);

	if (pdic_data->detach_valid) {
		dev_dbg_once(pdic_data->dev, "%s, ignore cc control\n", __func__);
		return 0;
	}

	if (val) {
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL, &data);
		data |= S2MU106_REG_PLUG_CTRL_ECO_SRC_CAP_RDY;
		s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL, data);
	} else {
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL, &data);
		data &= ~S2MU106_REG_PLUG_CTRL_ECO_SRC_CAP_RDY;
		s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL, data);
	}

	return 0;
}

static void s2mu106_dfp(struct i2c_client *i2c)
{
	u8 data;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_MSG, &data);
	data |= S2MU106_REG_MSG_DATA_ROLE_MASK;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_MSG, data);
}

static void s2mu106_ufp(struct i2c_client *i2c)
{
	u8 data;
	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_MSG, &data);
	data &= ~S2MU106_REG_MSG_DATA_ROLE_MASK;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_MSG, data);
}

static void s2mu106_src(struct i2c_client *i2c)
{
	u8 data;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_MSG, &data);
	data = (data & ~S2MU106_REG_MSG_POWER_ROLE_MASK) | S2MU106_REG_MSG_POWER_ROLE_SOURCE;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_MSG, data);
}

static void s2mu106_snk(struct i2c_client *i2c)
{
	u8 data;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_MSG, &data);
	data = (data & ~S2MU106_REG_MSG_POWER_ROLE_MASK) | S2MU106_REG_MSG_POWER_ROLE_SINK;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_MSG, data);
}

#if defined(CONFIG_CCIC_NOTIFIER)
void s2mu106_control_option_command (struct s2mu106_usbpd_data *pdic_data, int cmd) {
	struct usbpd_data *_data = dev_get_drvdata(pdic_data->dev);
	int pd_cmd = cmd & 0x0f;

/* 0x1 : Vconn control option command ON
 * 0x2 : Vconn control option command OFF
 * 0x3 : Water Detect option command ON
 * 0x4 : Water Detect option command OFF
 */
	switch (pd_cmd) {
	case 1:
		s2mu106_set_vconn_source(_data, USBPD_VCONN_ON);
		break;
	case 2:
		s2mu106_set_vconn_source(_data, USBPD_VCONN_OFF);
		break;
	case 3:
	case 4:
		pr_err("%s : not implement water control\n", __func__);
		break;
	default:
		break;
	}
}

static int s2mu106_sysfs_get_prop(struct _ccic_data_t *pccic_data,
					enum ccic_sysfs_property prop,
					char *buf)
{
	int retval = -ENODEV;
	struct s2mu106_usbpd_data *usbpd_data =
			(struct s2mu106_usbpd_data *)pccic_data->drv_data;
	struct usbpd_data *pd_data;
	struct usbpd_manager_data *manager;

	if (!usbpd_data) {
		pr_debug_once("%s : usbpd_data is null\n", __func__);
		return retval;
	}
	pd_data = dev_get_drvdata(usbpd_data->dev);
	if (!pd_data) {
		pr_err("%s : pd_data is null\n", __func__);
		return retval;
	}
	manager = &pd_data->manager;
	if (!manager) {
		pr_err("%s : manager is null\n", __func__);
		return retval;
	}

	switch (prop) {
	case CCIC_SYSFS_PROP_LPM_MODE:
		retval = sprintf(buf, "%d\n", usbpd_data->lpm_mode);
		pr_debug_once("%s : CCIC_SYSFS_PROP_LPM_MODE : %s", __func__, buf);
		break;
	case CCIC_SYSFS_PROP_RID:
		retval = sprintf(buf, "%d\n", usbpd_data->rid == REG_RID_MAX ?
				REG_RID_OPEN : usbpd_data->rid);
		pr_debug_once("%s : CCIC_SYSFS_PROP_RID : %s", __func__, buf);
		break;
	case CCIC_SYSFS_PROP_FW_WATER:
		retval = sprintf(buf, "%d\n", usbpd_data->is_water_detect);
		pr_debug_once("%s : CCIC_SYSFS_PROP_FW_WATER : %s", __func__, buf);
		break;
	case CCIC_SYSFS_PROP_STATE:
		retval = sprintf(buf, "%d\n", (int)pd_data->policy.plug_valid);
		pr_debug_once("%s : CCIC_SYSFS_PROP_STATE : %s", __func__, buf);
		break;
	case CCIC_SYSFS_PROP_ACC_DEVICE_VERSION:
		retval = sprintf(buf, "%04x\n", manager->Device_Version);
		pr_debug_once("%s : CCIC_SYSFS_PROP_ACC_DEVICE_VERSION : %s",
				__func__, buf);
		break;
	case CCIC_SYSFS_PROP_USBPD_IDS:
		retval = sprintf(buf, "%04x:%04x\n",
				le16_to_cpu(manager->Vendor_ID),
				le16_to_cpu(manager->Product_ID));
		pr_debug_once("%s : CCIC_SYSFS_PROP_USBPD_IDS : %s", __func__, buf);
		break;
	case CCIC_SYSFS_PROP_USBPD_TYPE:
		retval = sprintf(buf, "%d\n", manager->acc_type);
		pr_debug_once("%s : CCIC_SYSFS_PROP_USBPD_TYPE : %s", __func__, buf);
		break;
	default:
		pr_debug_once("%s : prop read not supported prop (%d)\n",
				__func__, prop);
		retval = -ENODATA;
		break;
	}
	return retval;
}

static ssize_t s2mu106_sysfs_set_prop(struct _ccic_data_t *pccic_data,
				enum ccic_sysfs_property prop,
				const char *buf, size_t size)
{
	ssize_t retval = size;
	struct s2mu106_usbpd_data *usbpd_data =
			(struct s2mu106_usbpd_data *)pccic_data->drv_data;
	int rv;
	int mode = 0;

	if (!usbpd_data) {
		pr_debug_once("%s : usbpd_data is null : request prop = %d\n",
				__func__, prop);
		return -ENODEV;
	}

	switch (prop) {
	case CCIC_SYSFS_PROP_LPM_MODE:
		rv = sscanf(buf, "%d", &mode);
		pr_debug_once("%s : CCIC_SYSFS_PROP_LPM_MODE mode=%d\n",
				__func__, mode);
		mutex_lock(&usbpd_data->lpm_mutex);
#ifdef CONFIG_SEC_FACTORY
	if (mode != 1 && mode != 2)
		s2mu106_set_normal_mode(usbpd_data);
#else
	if (mode == 1 || mode == 2)
		s2mu106_set_lpm_mode(usbpd_data);
	else
		s2mu106_set_normal_mode(usbpd_data);
#endif
		mutex_unlock(&usbpd_data->lpm_mutex);
		break;
	case CCIC_SYSFS_PROP_CTRL_OPTION:
		rv = sscanf(buf, "%d", &mode);
		pr_debug_once("%s : CCIC_SYSFS_PROP_CTRL_OPTION mode=%d\n",
				__func__, mode);
		s2mu106_control_option_command(usbpd_data, mode);
		break;
	default:
		pr_debug_once("%s : prop write not supported prop (%d)\n",
				__func__, prop);
		retval = -ENODATA;
		return retval;
	}
	return size;
}

static int s2mu106_sysfs_is_writeable(struct _ccic_data_t *pccic_data,
				enum ccic_sysfs_property prop)
{
	switch (prop) {
	case CCIC_SYSFS_PROP_LPM_MODE:
	case CCIC_SYSFS_PROP_CTRL_OPTION:
		return 1;
	default:
		return 0;
	}
}
#endif

#if defined(CONFIG_CCIC_MANUAL_QBAT) && !defined(CONFIG_SEC_FACTORY)
static void s2mu106_manual_qbat_control(struct s2mu106_usbpd_data *pdic_data, int rid)
{
	struct power_supply *psy_charger;
	union power_supply_propval val;
	int ret = 0;

	pr_debug_once("%s, rid=%d\n", __func__, rid);
	psy_charger = get_power_supply_by_name("s2mu106-charger");

	if (psy_charger == NULL) {
		pr_err("%s: Fail to get psy charger\n", __func__);
		return;
	}

	switch (rid) {
	case REG_RID_255K:
	case REG_RID_301K:
	case REG_RID_523K:
		val.intval = 1;
		break;
	default:
		val.intval = 0;
		break;
	}

	ret = psy_charger->desc->set_property(psy_charger,
			POWER_SUPPLY_PROP_FACTORY_MODE, &val);

	if (ret)
		pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
			__func__, ret);
}
#endif
static void s2mu106_notify_pdic_rid(struct s2mu106_usbpd_data *pdic_data, int rid)
{
#if defined(CONFIG_CCIC_NOTIFIER)
	pdic_data->is_factory_mode = false;
	if (rid == RID_523K)
		pdic_data->is_factory_mode = true;

#if defined(CONFIG_CCIC_MANUAL_QBAT) && !defined(CONFIG_SEC_FACTORY)
	s2mu106_manual_qbat_control(pdic_data, rid);
#endif
	/* rid */
	s2mu106_ccic_event_work(pdic_data,
		CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_RID, rid/*rid*/, 0);

	if (rid == REG_RID_523K || rid == REG_RID_619K || rid == REG_RID_OPEN) {
		s2mu106_ccic_event_work(pdic_data,
			CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB, 0/*attach*/, USB_STATUS_NOTIFY_DETACH);
		pdic_data->is_host = HOST_OFF;
		pdic_data->is_client = CLIENT_OFF;
	} else if (rid == REG_RID_301K) {
		s2mu106_ccic_event_work(pdic_data,
			CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
				1/*attach*/, USB_STATUS_NOTIFY_ATTACH_UFP/*drp*/);
		pdic_data->is_host = HOST_OFF;
		pdic_data->is_client = CLIENT_ON;
	}
#else
	muic_attached_dev_t new_dev;
	pdic_data->is_factory_mode = false;
	switch (rid) {
	case REG_RID_255K:
		new_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
		break;
	case REG_RID_301K:
		new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
		break;
	case REG_RID_523K:
		new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
		pdic_data->is_factory_mode = true;
		break;
	case REG_RID_619K:
		new_dev = ATTACHED_DEV_JIG_UART_ON_MUIC;
		break;
	default:
		new_dev = ATTACHED_DEV_NONE_MUIC;
		return;
	}
	s2mu106_pdic_notifier_attach_attached_jig_dev(new_dev);
#endif
	dev_dbg_once(pdic_data->dev, "%s : attached rid state(%d)", __func__, rid);
}

static void s2mu106_usbpd_check_rid(struct s2mu106_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 rid;
	int prev_rid = pdic_data->rid;

	usleep_range(5000, 6000);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_ADC_STATUS, &rid);
	rid = (rid & S2MU106_PDIC_RID_MASK) >> S2MU106_PDIC_RID_SHIFT;

	dev_dbg_once(pdic_data->dev, "%s : attached rid state(%d)", __func__, rid);

	if (rid) {
		if (pdic_data->rid != rid) {
			pdic_data->rid = rid;
			if (prev_rid >= REG_RID_OPEN && rid >= REG_RID_OPEN)
				dev_err(pdic_data->dev,
				  "%s : rid is not changed, skip notify(%d)", __func__, rid);
			else
				s2mu106_notify_pdic_rid(pdic_data, rid);
		}

		if (rid >= REG_RID_MAX) {
			dev_err(pdic_data->dev, "%s : overflow rid value", __func__);
			return;
		}
	}
}

int s2mu106_set_normal_mode(struct s2mu106_usbpd_data *pdic_data)
{
	u8 data;
	u8 data_lpm;
	int ret = 0;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, &data);
	data &= ~(S2MU106_REG_PLUG_CTRL_MODE_MASK | S2MU106_REG_PLUG_CTRL_RP_SEL_MASK);
	data |= S2MU106_REG_PLUG_CTRL_DRP | S2MU106_REG_PLUG_CTRL_RP80;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PD_CTRL, &data_lpm);
	data_lpm &= ~S2MU106_REG_LPM_EN;

	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, data);
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PD_CTRL, data_lpm);

	pdic_data->lpm_mode = false;

	s2mu106_set_irq_enable(pdic_data, ENABLED_INT_0, ENABLED_INT_1,
			ENABLED_INT_2, ENABLED_INT_3, ENABLED_INT_4, ENABLED_INT_5);

	dev_dbg_once(dev, "%s s2mu106 exit lpm mode\n", __func__);

	return ret;
}

int s2mu106_usbpd_lpm_check(struct s2mu106_usbpd_data *pdic_data)
{
	u8 data_lpm = 0;
	struct i2c_client *i2c = pdic_data->i2c;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PD_CTRL, &data_lpm);

	return (data_lpm & S2MU106_REG_LPM_EN);
}

void s2mu106_usbpd_set_mode(struct s2mu106_usbpd_data *pdic_data,
													CCIC_LPM_MODE_SEL mode)
{
	u8 data_lpm = 0;
	struct i2c_client *i2c = pdic_data->i2c;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PD_CTRL, &data_lpm);
	if (mode == PD_LPM_MODE)
		data_lpm |= S2MU106_REG_LPM_EN;
	else if (mode == PD_NORMAL_MODE)
		data_lpm &= ~S2MU106_REG_LPM_EN;
	else {
		pr_debug_once("%s mode val(%d) is invalid\n", __func__, mode);
		return;
	}

	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PD_CTRL, data_lpm);
}

void s2mu106_usbpd_set_vbus_wakeup(struct s2mu106_usbpd_data *pdic_data,
													CCIC_VBUS_WAKEUP_SEL sel)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data = 0;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PD_TRIM, &data);
	if (sel == VBUS_WAKEUP_ENABLE)
		data &= ~S2MU106_REG_VBUS_WAKEUP_DIS;
	else if (sel == VBUS_WAKEUP_DISABLE)
		data |= S2MU106_REG_VBUS_WAKEUP_DIS;
	else {
		pr_debug_once("%s sel val(%d) is invalid\n", __func__, sel);
		return;
	}

	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PD_TRIM, data);
}

int s2mu106_get_plug_monitor(struct s2mu106_usbpd_data *pdic_data, u8 *data)
{
	u8 reg_val;
	int ret = 0;
	struct i2c_client *i2c = pdic_data->i2c;

	if (&data[0] == NULL || &data[1] == NULL) {
		pr_err("%s NULL point data\n", __func__);
		return -1;
	}

	ret = s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_MON1, &reg_val);
	if (ret < 0) {
		pr_err("%s: S2MU106_REG_PLUG_MON1 Read err : %d\n",	__func__, ret);
		return ret;
	}

	data[0] = reg_val & S2MU106_REG_CTRL_MON_CC1_MASK;
	data[1] = (reg_val & S2MU106_REG_CTRL_MON_CC2_MASK) >> S2MU106_REG_CTRL_MON_CC2_SHIFT;
	pr_debug_once("%s, water cc mon cc1 : 0x%X, cc2 : 0x%X\n", __func__, data[0], data[1]);

	return ret;
}

int s2mu106_set_cable_detach_lpm_mode(struct s2mu106_usbpd_data *pdic_data)
{
	u8 data, data_lpm;
	int ret = 0;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	u8 intr[S2MU106_MAX_NUM_INT_STATUS] = {0};

	pdic_data->lpm_mode = true;
	pdic_data->vbus_short_check_cnt = 0;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, &data);
	data &= ~(S2MU106_REG_PLUG_CTRL_MODE_MASK | S2MU106_REG_PLUG_CTRL_RP_SEL_MASK);
	data |= S2MU106_REG_PLUG_CTRL_DFP | S2MU106_REG_PLUG_CTRL_RP0;
	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PD_CTRL, &data_lpm);
	data_lpm |= S2MU106_REG_LPM_EN;

	s2mu106_set_irq_enable(pdic_data, 0, 0, 0, 0, 0, 0);

	ret = s2mu106_usbpd_bulk_read(i2c, S2MU106_REG_INT_STATUS0,
			S2MU106_MAX_NUM_INT_STATUS, intr);

	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, data);
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PD_CTRL, data_lpm);

	dev_dbg_once(dev, "%s enter.\n", __func__);

	return ret;
}

int s2mu106_set_lpm_mode(struct s2mu106_usbpd_data *pdic_data)
{
	u8 data, data_lpm;
	int ret = 0;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	u8 intr[S2MU106_MAX_NUM_INT_STATUS] = {0};

	pdic_data->lpm_mode = true;
	pdic_data->vbus_short_check_cnt = 0;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, &data);
	data &= ~(S2MU106_REG_PLUG_CTRL_MODE_MASK | S2MU106_REG_PLUG_CTRL_RP_SEL_MASK);
	data |= S2MU106_REG_PLUG_CTRL_DFP | S2MU106_REG_PLUG_CTRL_RP0;
	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PD_CTRL, &data_lpm);
	data_lpm |= S2MU106_REG_LPM_EN;

#if	(!defined(CONFIG_SEC_FACTORY) && defined(CONFIG_CCIC_MODE_BY_MUIC))
	s2mu106_usbpd_set_vbus_wakeup(pdic_data, VBUS_WAKEUP_DISABLE);
#endif

	s2mu106_set_irq_enable(pdic_data, 0, 0, 0, 0, 0, 0);

	ret = s2mu106_usbpd_bulk_read(i2c, S2MU106_REG_INT_STATUS0,
			S2MU106_MAX_NUM_INT_STATUS, intr);

	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, data);
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PD_CTRL, data_lpm);

	if (pdic_data->detach_valid == false) {
		s2mu106_usbpd_detach_init(pdic_data);
		s2mu106_usbpd_notify_detach(pdic_data);
	}

	dev_dbg_once(dev, "%s s2mu106 enter lpm mode\n", __func__);

	return ret;
}

void _s2mu106_set_water_detect_pre_cond(struct s2mu106_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, &data);
	data &= ~(S2MU106_REG_PLUG_CTRL_MODE_MASK | S2MU106_REG_PLUG_CTRL_RP_SEL_MASK);
	data |= S2MU106_REG_PLUG_CTRL_DFP | S2MU106_REG_PLUG_CTRL_RP0;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, data);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PD_CTRL, &data);
	data &= ~S2MU106_REG_LPM_EN;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PD_CTRL, data);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_ANALOG_OTP_04, &data);
	data |= S2MU106_REG_CC1_RS_SW_ON_MASK | S2MU106_REG_CC2_RS_SW_ON_MASK;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_ANALOG_OTP_04, data);

	msleep(300);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_ANALOG_OTP_04, &data);
	data &= ~(S2MU106_REG_OTP_CC_PUB_MASK | S2MU106_REG_CC_PU_LPM_CTRL_DIS_MASK
			| S2MU106_REG_CC1_RS_SW_ON_MASK | S2MU106_REG_CC2_RS_SW_ON_MASK);
	data |= S2MU106_REG_CC_PU_LPM_CTRL_DIS_MASK;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_ANALOG_OTP_04, data);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_ANALOG_OTP_08, &data);
	data &= ~S2MU106_REG_LPMPUI_SEL_MASK;
	data |= S2MU106_REG_LPMPUI_SEL_1UA_MASK;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_ANALOG_OTP_08, data);
}

void _s2mu106_set_water_detect_post_cond(struct s2mu106_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_ANALOG_OTP_04, &data);
	data &= ~(S2MU106_REG_OTP_CC_PUB_MASK | S2MU106_REG_CC_PU_LPM_CTRL_DIS_MASK);
	data |= S2MU106_REG_OTP_CC_PUB_MASK;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_ANALOG_OTP_04, data);
}

static void _s2mu106_pdic_handle_water_detection(struct s2mu106_usbpd_data *pdic_data)
{
#if defined(CONFIG_CCIC_NOTIFIER)
	s2mu106_ccic_event_work(pdic_data,
		CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_WATER, CCIC_NOTIFY_ATTACH, 0);
#else
	s2mu106_pdic_notifier_attach_attached_jig_dev(ATTACHED_DEV_WATER_MUIC);
#endif
	pdic_data->is_water_detect = true;
}

static void _s2mu106_pdic_transfer_to_water(struct s2mu106_usbpd_data *pdic_data)
{
#if defined(CONFIG_USB_HW_PARAM) && !defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct otg_notify *o_notify = get_otg_notify();
#endif
	pr_debug_once("%s: CCIC water detected", __func__);
	s2mu106_set_irq_enable(pdic_data, 0, 0, 0, 0, 0, 0);
	s2mu106_vbus_turn_on_ctrl(pdic_data, VBUS_OFF);
	s2mu106_ccic_event_work(pdic_data,
	CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
		0/*attach*/, USB_STATUS_NOTIFY_DETACH);
	_s2mu106_pdic_handle_water_detection(pdic_data);
#if defined(CONFIG_USB_HW_PARAM) && !defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	if (o_notify)
		inc_hw_param(o_notify, USB_CCIC_WATER_INT_COUNT);
#endif
}

static void s2mu106_pdic_water_detect_handler(struct work_struct *work)
{
	struct s2mu106_usbpd_data *pdic_data =
		container_of(work, struct s2mu106_usbpd_data, water_detect_handler.work);
	int i = 0;

	pr_debug_once("%s enter", __func__);
	mutex_lock(&pdic_data->water_mutex);
	s2mu106_set_irq_enable(pdic_data, 0, 0, 0, 0, 0, 0);
	/*
	 * Cancel the detect handler,
	 * in case the muic notifies cable attach or dry signal,
	 * or the water chk cnt is over,
	 * or ccic already detected the water.
	 */
	if (!pdic_data->is_muic_water_detect
		|| pdic_data->is_water_detect) {
		pr_debug_once("%s: detect handler is canceled", __func__);
		goto WATER_OUT;
	}

	for (i = 0; i < 3; i++) {
		_s2mu106_set_water_detect_pre_cond(pdic_data);
		msleep(200);
		s2mu106_usbpd_get_cc_voltage(pdic_data);
		_s2mu106_set_water_detect_post_cond(pdic_data);
		if (IS_CC_WATER(pdic_data->pm_cc1, pdic_data->pm_cc2)) {
			_s2mu106_pdic_transfer_to_water(pdic_data);
			goto WATER_OUT;
		}
	}

	if (!pdic_data->is_water_detect) {
		msleep(50);
		s2mu106_usbpd_get_cc_voltage(pdic_data);
		if (IS_CC_WATER_POST(pdic_data->pm_cc1, pdic_data->pm_cc2)) {
			_s2mu106_pdic_transfer_to_water(pdic_data);
			goto WATER_OUT;
		}
#if defined(CONFIG_CCIC_NOTIFIER)
		s2mu106_ccic_event_work(pdic_data,
			CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_WATER, CCIC_NOTIFY_DETACH, 0);
#endif
	}

	pr_debug_once("%s: water is not detected in CC.", __func__);
	if (pdic_data->is_water_detect) {
		pr_debug_once("%s: Mutex Conflict occurred", __func__);
		goto WATER_OUT;
	}
#if defined(CONFIG_CCIC_MODE_BY_MUIC)
	s2mu106_set_lpm_mode(pdic_data);
#else
	s2mu106_set_normal_mode(pdic_data);
	msleep(50);
	s2mu106_set_irq_enable(pdic_data, ENABLED_INT_0, ENABLED_INT_1,
				ENABLED_INT_2, ENABLED_INT_3, ENABLED_INT_4, ENABLED_INT_5);
#endif
WATER_OUT:
	mutex_unlock(&pdic_data->water_mutex);
	return;
}

static void s2mu106_pdic_water_dry_handler(struct work_struct *work)
{
	struct s2mu106_usbpd_data *pdic_data =
		container_of(work, struct s2mu106_usbpd_data, water_dry_handler.work);
	int i = 0;
	int vcc1[2] = {0,};
	int vcc2[2] = {0,};

	mutex_lock(&pdic_data->water_mutex);
	pr_debug_once("%s enter", __func__);
	s2mu106_set_irq_enable(pdic_data, 0, 0, 0, 0, 0, 0);

	if (!pdic_data->is_water_detect) {
		pr_debug_once("%s is canceled : already dried", __func__);
		goto done;
	}

	for (i = 0; i < 3; i++) {
		/* Detect Curr Src */
		_s2mu106_set_water_detect_pre_cond(pdic_data);
		msleep(200);

		/* 1st Measure */
		s2mu106_usbpd_get_cc_voltage(pdic_data);
		vcc1[0] = pdic_data->pm_cc1;
		vcc2[0] = pdic_data->pm_cc2;

		/* Discharging */
		_s2mu106_set_water_detect_post_cond(pdic_data);
		msleep(40);

		/* 2nd Measure : Potential Power */
		s2mu106_usbpd_get_cc_voltage(pdic_data);
		vcc1[1] = pdic_data->pm_cc1;
		vcc2[1] = pdic_data->pm_cc2;

		/* Compensation */
		pdic_data->pm_cc1 = vcc1[0] - vcc1[1];
		pdic_data->pm_cc2 = vcc2[0] - vcc2[1];

		if (IS_CC_DRY(pdic_data->pm_cc1, pdic_data->pm_cc2)) {
			pr_debug_once("%s: CCIC dry detected", __func__);
#if defined(CONFIG_CCIC_NOTIFIER)
			s2mu106_ccic_event_work(pdic_data,
				CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_WATER, CCIC_NOTIFY_DETACH, 0);
#endif
#if defined(CONFIG_CCIC_MODE_BY_MUIC)
			s2mu106_set_lpm_mode(pdic_data);
#endif
			goto done;
		}
		_s2mu106_set_water_detect_post_cond(pdic_data);
		usleep_range(10000, 11000);
	}
	pr_debug_once("%s : CC is not dried yet", __func__);
	_s2mu106_pdic_transfer_to_water(pdic_data);
done:
	mutex_unlock(&pdic_data->water_mutex);
	return;
}

static void s2mu106_usbpd_otg_attach(struct s2mu106_usbpd_data *pdic_data)
{
#if defined(CONFIG_USB_HOST_NOTIFY)
	 struct otg_notify *o_notify = get_otg_notify();
#endif
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;

	/* otg */
	pdic_data->is_host = HOST_ON;
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_SRC;
#elif defined(CONFIG_TYPEC)
	pdic_data->typec_power_role = TYPEC_SOURCE;
	typec_set_pwr_role(pdic_data->port, pdic_data->typec_power_role);
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
	send_otg_notify(o_notify, NOTIFY_EVENT_POWER_SOURCE, 1);
#endif

	/* USB */
	s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
			1/*attach*/, USB_STATUS_NOTIFY_ATTACH_DFP/*drp*/);
	/* add to turn on external 5V */
#if defined(CONFIG_USB_HOST_NOTIFY)
	if (!is_blocked(o_notify, NOTIFY_BLOCK_TYPE_HOST)) {
#ifdef CONFIG_PM_S2MU106
		s2mu106_usbpd_check_vbus(pdic_data, 800, VBUS_OFF);
#endif
		s2mu106_vbus_turn_on_ctrl(pdic_data, VBUS_ON);
	}
#endif
	usbpd_manager_acc_handler_cancel(dev);
}

#if defined(CONFIG_MUIC_NOTIFIER)
static int s2mu106_type3_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
#if defined(CONFIG_CCIC_NOTIFIER)
	CC_NOTI_ATTACH_TYPEDEF *p_noti = (CC_NOTI_ATTACH_TYPEDEF *)data;
	muic_attached_dev_t attached_dev = p_noti->cable_type;
#else
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
#endif
	struct s2mu106_usbpd_data *pdic_data =
		container_of(nb, struct s2mu106_usbpd_data,
			     type3_nb);
#if !defined(CONFIG_SEC_FACTORY) && defined(CONFIG_USB_HOST_NOTIFY) && \
	(defined(CONFIG_DUAL_ROLE_USB_INTF) || defined(CONFIG_TYPEC))
	struct i2c_client *i2c = pdic_data->i2c;
	u8 reg_data = 0;
#endif

#if (defined(CONFIG_USB_HW_PARAM) && !defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)) || \
	(!defined(CONFIG_SEC_FACTORY) && defined(CONFIG_USB_HOST_NOTIFY))
	struct otg_notify *o_notify = get_otg_notify();
#endif
	mutex_lock(&pdic_data->lpm_mutex);
	pr_debug_once("%s action:%d, attached_dev:%d, lpm:%d, pdic_data->is_otg_vboost:%d, pdic_data->is_otg_reboost:%d\n",
		__func__, (int)action, (int)attached_dev, pdic_data->lpm_mode,
		(int)pdic_data->is_otg_vboost, (int)pdic_data->is_otg_reboost);

	if ((action == MUIC_PDIC_NOTIFY_CMD_ATTACH) &&
		(attached_dev == ATTACHED_DEV_TYPE3_MUIC)) {
		pdic_data->is_muic_water_detect = false;
		if (pdic_data->lpm_mode) {
			pr_debug_once("%s try to exit lpm mode-->\n", __func__);
			s2mu106_set_normal_mode(pdic_data);
			pr_debug_once("%s after exit lpm mode<--\n", __func__);
		}
	} else if ((action == MUIC_PDIC_NOTIFY_CMD_ATTACH) &&
		attached_dev == ATTACHED_DEV_CHK_WATER_REQ) {
		pr_debug_once("%s, ATTACH : MUIC REQUESTED WATER CHECK\n", __func__);
		s2mu106_set_irq_enable(pdic_data, 0, 0, 0, 0, 0, 0);
		s2mu106_usbpd_set_vconn_manual(pdic_data, true);
		pdic_data->is_muic_water_detect = true;
		pdic_data->is_water_detect = false;
		cancel_delayed_work(&pdic_data->water_detect_handler);
		schedule_delayed_work(&pdic_data->water_detect_handler, msecs_to_jiffies(100));
	} else if ((action == MUIC_PDIC_NOTIFY_CMD_ATTACH) &&
		attached_dev == ATTACHED_DEV_CHK_WATER_DRY_REQ) {
		pr_debug_once("%s, ATTACH : MUIC REQUESTED WATER DRY CHECK\n", __func__);
		s2mu106_set_irq_enable(pdic_data, 0, 0, 0, 0, 0, 0);
		cancel_delayed_work(&pdic_data->water_dry_handler);
		schedule_delayed_work(&pdic_data->water_dry_handler, msecs_to_jiffies(100));
	} else if ((action == MUIC_PDIC_NOTIFY_CMD_ATTACH) &&
		attached_dev == ATTACHED_DEV_ABNORMAL_OTG_MUIC) {
		pdic_data->is_killer = true;
	} else if ((action == MUIC_PDIC_NOTIFY_CMD_ATTACH) &&
		attached_dev == ATTACHED_DEV_OTG_MUIC) {
		s2mu106_usbpd_otg_attach(pdic_data);
	} else if ((action == MUIC_PDIC_NOTIFY_CMD_DETACH) &&
		attached_dev == ATTACHED_DEV_UNDEFINED_RANGE_MUIC) {
		pr_debug_once("%s, DETACH : ATTACHED_DEV_UNDEFINED_RANGE_MUIC(Water DRY)\n", __func__);
		//s2mu106_set_cable_detach_lpm_mode(pdic_data);
#if !defined(CONFIG_CCIC_MODE_BY_MUIC)
		s2mu106_set_normal_mode(pdic_data);
#endif
#if defined(CONFIG_USB_HW_PARAM) && !defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_DRY_INT_COUNT);
#endif
#if defined(CONFIG_CCIC_NOTIFIER)
		s2mu106_ccic_event_work(pdic_data,
			CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_WATER, CCIC_NOTIFY_DETACH, 0);
#endif
		msleep(50);
		s2mu106_set_irq_enable(pdic_data, ENABLED_INT_0, ENABLED_INT_1,
			ENABLED_INT_2, ENABLED_INT_3, ENABLED_INT_4, ENABLED_INT_5);
		msleep(50);
		pdic_data->is_muic_water_detect = false;
		pdic_data->is_water_detect = false;
	} else if (action == MUIC_PDIC_NOTIFY_CMD_DETACH) {
		if (!pdic_data->lpm_mode) {
			pr_debug_once("%s try to enter lpm mode-->\n", __func__);
			s2mu106_set_lpm_mode(pdic_data);
			pr_debug_once("%s after enter lpm mode<--\n", __func__);
		}
	}
#if !defined(CONFIG_SEC_FACTORY) && defined(CONFIG_USB_HOST_NOTIFY) && \
	(defined(CONFIG_DUAL_ROLE_USB_INTF) || defined(CONFIG_TYPEC))
	else if ((action == MUIC_PDIC_NOTIFY_CMD_ATTACH)
			&& (attached_dev == ATTACHED_DEV_CHECK_OCP)
			&& pdic_data->is_otg_vboost
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
			&& pdic_data->data_role_dual == USB_STATUS_NOTIFY_ATTACH_DFP
#elif defined(CONFIG_TYPEC)
			&& pdic_data->typec_data_role == TYPEC_HOST
#endif
	) {
		if (o_notify) {
			if (is_blocked(o_notify, NOTIFY_BLOCK_TYPE_HOST)) {
				pr_debug_once("%s, upsm mode, skip OCP handling\n", __func__);
				goto EOH;
			}
		}
		if (pdic_data->is_otg_reboost) {
			/* todo : over current event to platform */
			pr_debug_once("%s, CHECK_OCP, Can't afford it(OVERCURRENT)\n", __func__);
			if (o_notify)
				send_otg_notify(o_notify, NOTIFY_EVENT_OVERCURRENT, 0);
			goto EOH;
		}
		s2mu106_ccic_event_work(pdic_data,
			CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_ATTACH, 1/*attach*/, 1/*rprd*/);

		pr_debug_once("%s, CHECK_OCP, start OCP W/A\n", __func__);
		pdic_data->is_otg_reboost = true;
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC_HOLD, &reg_data);
		reg_data |= S2MU106_REG_PLUG_CTRL_CC_HOLD_BIT;
		s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC_HOLD, reg_data);

		s2mu106_usbpd_set_rp_scr_sel(pdic_data, PLUG_CTRL_RP80);
		s2mu106_vbus_turn_on_ctrl(pdic_data, VBUS_OFF);
		s2mu106_vbus_turn_on_ctrl(pdic_data, VBUS_ON);

		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC_HOLD, &reg_data);
		reg_data &= ~S2MU106_REG_PLUG_CTRL_CC_HOLD_BIT;
		s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC_HOLD, reg_data);
	}
EOH:
#endif
	mutex_unlock(&pdic_data->lpm_mutex);

	return 0;
}
#endif

static void s2mu106_usbpd_prevent_watchdog_reset(
						struct s2mu106_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	u8 val = 0;

	mutex_lock(&pdic_data->lpm_mutex);
	if (!pdic_data->lpm_mode) {
		if (s2mu106_usbpd_lpm_check(pdic_data) == 0) {
			msleep(30);
			s2mu106_usbpd_read_reg(i2c, S2MU106_REG_INT_STATUS2, &val);
			s2mu106_usbpd_set_vbus_wakeup(pdic_data, VBUS_WAKEUP_DISABLE);
			pr_debug_once("%s force to lpm mode\n", __func__);
			s2mu106_usbpd_set_mode(pdic_data, PD_LPM_MODE);
			/* enable wakeup to check prevent function */
			s2mu106_set_irq_enable(pdic_data, ENABLED_INT_0, ENABLED_INT_1,
						ENABLED_INT_2_WAKEUP, ENABLED_INT_3, ENABLED_INT_4,
															ENABLED_INT_5);
			s2mu106_usbpd_set_vbus_wakeup(pdic_data, VBUS_WAKEUP_ENABLE);
			usleep_range(1000, 1200);
			s2mu106_usbpd_read_reg(i2c, S2MU106_REG_INT_STATUS2, &val);
			if (val & S2MU106_REG_INT_STATUS2_WAKEUP)
				pr_debug_once("%s auto wakeup success\n", __func__);
			else {
				msleep(22);
				s2mu106_usbpd_set_vbus_wakeup(pdic_data, VBUS_WAKEUP_DISABLE);
				usleep_range(1000, 1200);
				s2mu106_usbpd_set_vbus_wakeup(pdic_data, VBUS_WAKEUP_ENABLE);
				usleep_range(1000, 1200);
				s2mu106_usbpd_read_reg(i2c, S2MU106_REG_INT_STATUS2, &val);
				if (val & S2MU106_REG_INT_STATUS2_WAKEUP)
					pr_debug_once("%s auto wakeup success\n", __func__);
				else
					s2mu106_usbpd_set_mode(pdic_data, PD_NORMAL_MODE);
			}

			s2mu106_set_irq_enable(pdic_data, ENABLED_INT_0, ENABLED_INT_1,
								ENABLED_INT_2, ENABLED_INT_3, ENABLED_INT_4,
																ENABLED_INT_5);
		}
	}
	mutex_unlock(&pdic_data->lpm_mutex);
}

static void s2mu106_vbus_short_check(struct s2mu106_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = pdic_data->dev;
	u8 val = 0;
	u8 cc1_val = 0, cc2_val = 0;
	u8 rp_currentlvl = 0;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	if (pdic_data->vbus_short_check)
		return;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_FSM_MON, &val);

	cc1_val = val & S2MU106_REG_CTRL_MON_CC1_MASK;
	cc2_val = (val & S2MU106_REG_CTRL_MON_CC2_MASK) >> S2MU106_REG_CTRL_MON_CC2_SHIFT;

	dev_dbg_once(dev, "%s, 10k check : cc1_val(%x), cc2_val(%x)\n",
					__func__, cc1_val, cc2_val);

	if (cc1_val == USBPD_10k || cc2_val == USBPD_10k)
		rp_currentlvl = RP_CURRENT_LEVEL3;
	else if (cc1_val == USBPD_22k || cc2_val == USBPD_22k)
		rp_currentlvl = RP_CURRENT_LEVEL2;
	else if (cc1_val == USBPD_56k || cc2_val == USBPD_56k)
		rp_currentlvl = RP_CURRENT_LEVEL_DEFAULT;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_MON1, &val);

	cc1_val = val & S2MU106_REG_CTRL_MON_CC1_MASK;
	cc2_val = (val & S2MU106_REG_CTRL_MON_CC2_MASK) >> S2MU106_REG_CTRL_MON_CC2_SHIFT;

	dev_dbg_once(dev, "%s, vbus short check : cc1_val(%x), cc2_val(%x)\n",
					__func__, cc1_val, cc2_val);

	if (cc1_val == USBPD_Rp || cc2_val == USBPD_Rp) {
		pdic_data->vbus_short = true;
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_VBUS_CC_SHORT_COUNT);
#endif
	} else {
		pdic_data->vbus_short = false;
#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
		pd_noti.sink_status.rp_currentlvl = rp_currentlvl;
		pd_noti.event = PDIC_NOTIFY_EVENT_CCIC_ATTACH;
		s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_BATTERY, CCIC_NOTIFY_ID_POWER_STATUS, 0, 0);
#endif
#endif
		if (rp_currentlvl == RP_CURRENT_LEVEL_DEFAULT)
			s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_MUIC,
				CCIC_NOTIFY_ID_TA, 1/*attach*/, 0/*rprd*/);
	}

	pdic_data->vbus_short_check = true;
}

#if !defined(CONFIG_SEC_FACTORY) && IS_ENABLED(CONFIG_S2MU106_TYPEC_WATER)
static void s2mu106_power_off_water_check(struct s2mu106_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = pdic_data->dev;
	u8 val, prev_val, data_lpm = 0;
	u8 cc1_val, cc2_val;
	int retry = 0;

	mutex_lock(&pdic_data->_mutex);
	mutex_lock(&pdic_data->lpm_mutex);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, &val);
	prev_val = val;
	val &= ~(S2MU106_REG_PLUG_CTRL_MODE_MASK | S2MU106_REG_PLUG_CTRL_RP_SEL_MASK);
	val |= S2MU106_REG_PLUG_CTRL_RP0 | S2MU106_REG_PLUG_CTRL_DRP;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, val);

	if (pdic_data->lpm_mode) {
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PD_CTRL, &data_lpm);
		data_lpm &= ~S2MU106_REG_LPM_EN;
		s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PD_CTRL, data_lpm);
	}

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, &val);
	val &= ~S2MU106_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	val |= S2MU106_REG_PLUG_CTRL_FSM_ATTACHED_SNK;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, val);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, &val);
	val |= S2MU106_REG_PLUG_CTRL_FSM_MANUAL_EN;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, val);
	msleep(50);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, &val);
	val &= ~S2MU106_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	val |= S2MU106_REG_PLUG_CTRL_FSM_ATTACHED_SRC;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, val);

	usleep_range(1000, 1100);

	for (retry = 0; retry < 3; retry++) {
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_MON1, &val);

		cc1_val = val & S2MU106_REG_CTRL_MON_CC1_MASK;
		cc2_val = (val & S2MU106_REG_CTRL_MON_CC2_MASK) >> S2MU106_REG_CTRL_MON_CC2_SHIFT;

		dev_dbg_once(dev, "%s, vbus short check(%d) : cc1_val(%x), cc2_val(%x)\n",
						__func__, retry, cc1_val, cc2_val);

		if (cc1_val == USBPD_Ra || cc2_val == USBPD_Ra)
			break;
		else if (retry == 2) {
			pdic_data->lpcharge_water = true;
			pdic_data->is_water_detect = true;
			pdic_data->water_detect_cnt = 0;
			s2mu106_set_irq_enable(pdic_data, 0, 0, 0, 0, 0, 0);
			s2mu106_usbpd_notify_detach(pdic_data);
			s2mu106_ccic_event_work(pdic_data,
				CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_WATER, CCIC_NOTIFY_ATTACH, 1);
			s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_BATTERY, CCIC_NOTIFY_ID_WATER, 1, 0);
		}
		udelay(5);
	}

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, &val);
	val &= ~S2MU106_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	val |= S2MU106_REG_PLUG_CTRL_FSM_ATTACHED_SNK;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, val);

	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, prev_val);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, &val);
	val &= ~S2MU106_REG_PLUG_CTRL_FSM_MANUAL_EN;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, val);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, &val);
	val &= ~S2MU106_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, val);

	if (pdic_data->lpm_mode) {
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PD_CTRL, &data_lpm);
		data_lpm |= S2MU106_REG_LPM_EN;
		s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PD_CTRL, data_lpm);
	}

	mutex_unlock(&pdic_data->lpm_mutex);
	mutex_unlock(&pdic_data->_mutex);
}
#endif

#if defined(CONFIG_SEC_FACTORY)
int s2mu106_sys_power_off_water_check(struct s2mu106_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = pdic_data->dev;
	u8 val, prev_val, data_lpm = 0;
	u8 cc1_val, cc2_val;
	int retry = 0;
	int ret = true;

	mutex_lock(&pdic_data->_mutex);
	mutex_lock(&pdic_data->lpm_mutex);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, &val);
	prev_val = val;
	val &= ~(S2MU106_REG_PLUG_CTRL_MODE_MASK | S2MU106_REG_PLUG_CTRL_RP_SEL_MASK);
	val |= S2MU106_REG_PLUG_CTRL_RP0 | S2MU106_REG_PLUG_CTRL_DRP;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, val);

	if (pdic_data->lpm_mode) {
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PD_CTRL, &data_lpm);
		data_lpm &= ~S2MU106_REG_LPM_EN;
		s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PD_CTRL, data_lpm);
	}

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, &val);
	val &= ~S2MU106_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	val |= S2MU106_REG_PLUG_CTRL_FSM_ATTACHED_SNK;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, val);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, &val);
	val |= S2MU106_REG_PLUG_CTRL_FSM_MANUAL_EN;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, val);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, &val);
	val &= ~S2MU106_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	val |= S2MU106_REG_PLUG_CTRL_FSM_ATTACHED_SRC;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, val);

	msleep(300);

	for (retry = 0; retry < 3; retry++) {
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_MON1, &val);

		cc1_val = val & S2MU106_REG_CTRL_MON_CC1_MASK;
		cc2_val = (val & S2MU106_REG_CTRL_MON_CC2_MASK) >> S2MU106_REG_CTRL_MON_CC2_SHIFT;

		dev_dbg_once(dev, "%s, vbus short check(%d) : cc1_val(%x), cc2_val(%x)\n",
						__func__, retry, cc1_val, cc2_val);

		if (cc1_val == USBPD_Ra || cc2_val == USBPD_Ra)
			break;
		else if (retry == 2)
			ret = false;
		udelay(5);
	}

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, &val);
	val &= ~S2MU106_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	val |= S2MU106_REG_PLUG_CTRL_FSM_ATTACHED_SNK;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, val);

	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, prev_val);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, &val);
	val &= ~S2MU106_REG_PLUG_CTRL_FSM_MANUAL_EN;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, val);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, &val);
	val &= ~S2MU106_REG_PLUG_CTRL_FSM_MANUAL_INPUT_MASK;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, val);

	if (pdic_data->lpm_mode) {
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PD_CTRL, &data_lpm);
		data_lpm |= S2MU106_REG_LPM_EN;
		s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PD_CTRL, data_lpm);
	}

	mutex_unlock(&pdic_data->lpm_mutex);
	mutex_unlock(&pdic_data->_mutex);

	return ret;
}
#endif

static void s2mu106_usbpd_detach_init(struct s2mu106_usbpd_data *pdic_data)
{
	struct device *dev = pdic_data->dev;
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct i2c_client *i2c = pdic_data->i2c;
	int ret = 0;
	u8 rid = 0;
	u8 reg_data = 0;

	dev_dbg_once(dev, "%s\n", __func__);

	mutex_lock(&pdic_data->cc_mutex);
	s2mu106_usbpd_set_cc_control(pdic_data, USBPD_CC_OFF);
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	if (pdic_data->power_role_dual == DUAL_ROLE_PROP_PR_SRC)
		s2mu106_vbus_turn_on_ctrl(pdic_data, VBUS_OFF);

#elif defined(CONFIG_TYPEC)
	if (pdic_data->typec_power_role == TYPEC_SOURCE)
		s2mu106_vbus_turn_on_ctrl(pdic_data, VBUS_OFF);
#endif
	s2mu106_usbpd_set_rp_scr_sel(pdic_data, PLUG_CTRL_RP80);
	pdic_data->detach_valid = true;
	mutex_unlock(&pdic_data->cc_mutex);

	usbpd_manager_plug_detach(dev, 0);

	/* wait flushing policy engine work */
	usbpd_cancel_policy_work(dev);

	pdic_data->status_reg = 0;
	usbpd_reinit(dev);
	/* for ccic hw detect */
	ret = s2mu106_usbpd_write_reg(i2c, S2MU106_REG_MSG_SEND_CON, S2MU106_REG_MSG_SEND_CON_HARD_EN);
	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_ADC_STATUS, &rid);
	rid = (rid & S2MU106_PDIC_RID_MASK) >> S2MU106_PDIC_RID_SHIFT;
	if (!rid) {
		s2mu106_self_soft_reset(i2c);
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, &reg_data);
		if ((reg_data & S2MU106_REG_PLUG_CTRL_MODE_MASK) !=
			S2MU106_REG_PLUG_CTRL_DRP) {
			reg_data |= S2MU106_REG_PLUG_CTRL_DRP;
			s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_PORT, reg_data);
		}
	}
	s2mu106_snk(i2c);
	s2mu106_ufp(i2c);
	pdic_data->rid = REG_RID_MAX;
	pdic_data->is_factory_mode = false;
	pdic_data->is_pr_swap = false;
	pdic_data->vbus_short_check = false;
	pdic_data->pd_vbus_short_check = false;
	pdic_data->vbus_short = false;
	pdic_data->is_killer = false;
	pdic_data->cc_instead_of_vbus = 0;
	if (pdic_data->regulator_en)
		ret = regulator_disable(pdic_data->regulator);
#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	pd_noti.sink_status.current_pdo_num = 0;
	pd_noti.sink_status.selected_pdo_num = 0;
	pd_noti.sink_status.rp_currentlvl = RP_CURRENT_LEVEL_NONE;
#endif
#endif
	s2mu106_usbpd_reg_init(pdic_data);
	s2mu106_set_vconn_source(pd_data, USBPD_VCONN_OFF);
}

static void s2mu106_usbpd_notify_detach(struct s2mu106_usbpd_data *pdic_data)
{
	struct device *dev = pdic_data->dev;
#if defined(CONFIG_CCIC_NOTIFIER)
#if defined(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
#endif
	/* MUIC */
	s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_ATTACH,
							0/*attach*/, 0/*rprd*/);

	s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_RID,
							REG_RID_OPEN/*rid*/, 0);

	if (pdic_data->is_host > HOST_OFF || pdic_data->is_client > CLIENT_OFF) {
		usbpd_manager_acc_detach(dev);

		/* usb or otg */
		dev_dbg_once(dev, "%s %d: is_host = %d, is_client = %d\n", __func__,
				__LINE__, pdic_data->is_host, pdic_data->is_client);
		pdic_data->is_host = HOST_OFF;
		pdic_data->is_client = CLIENT_OFF;
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_NONE;
#elif defined(CONFIG_TYPEC)
		pdic_data->typec_power_role = TYPEC_SINK;
		typec_set_pwr_role(pdic_data->port, TYPEC_SINK);
		pdic_data->typec_data_role = TYPEC_DEVICE;
		typec_set_data_role(pdic_data->port, TYPEC_DEVICE);
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
		send_otg_notify(o_notify, NOTIFY_EVENT_POWER_SOURCE, 0);
#endif
		/* USB */
		s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
					0/*attach*/, USB_STATUS_NOTIFY_DETACH/*drp*/);
	}
#else
	usbpd_manager_plug_detach(dev, 1);
#endif
}

static void s2mu106_usbpd_check_host(struct s2mu106_usbpd_data *pdic_data,
							CCIC_HOST_REASON host)
{
#if defined(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	if (host == HOST_ON && pdic_data->is_host == HOST_ON) {
		dev_dbg_once(pdic_data->dev, "%s %d: turn off host\n", __func__, __LINE__);
		s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_MUIC,
				CCIC_NOTIFY_ID_ATTACH, 0/*attach*/, 1/*rprd*/);
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_NONE;
#elif defined(CONFIG_TYPEC)
		pdic_data->typec_power_role = TYPEC_SINK;
		typec_set_pwr_role(pdic_data->port, pdic_data->typec_power_role);
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
		send_otg_notify(o_notify, NOTIFY_EVENT_POWER_SOURCE, 0);
#endif
		/* add to turn off external 5V */
		s2mu106_vbus_turn_on_ctrl(pdic_data, VBUS_OFF);

		s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
					0/*attach*/, USB_STATUS_NOTIFY_DETACH/*drp*/);
		pdic_data->is_host = HOST_OFF;
		msleep(300);
	} else if (host == HOST_OFF && pdic_data->is_host == HOST_OFF) {
		/* muic */
		s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_MUIC,
				CCIC_NOTIFY_ID_OTG, 1/*attach*/, 0/*rprd*/);
	}
}

static void s2mu106_usbpd_check_client(struct s2mu106_usbpd_data *pdic_data,
							CCIC_DEVICE_REASON client)
{
	if (client == CLIENT_ON && pdic_data->is_client == CLIENT_ON) {
		dev_dbg_once(pdic_data->dev, "%s %d: turn off client\n", __func__, __LINE__);
		s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_MUIC,
				CCIC_NOTIFY_ID_ATTACH, 0/*attach*/, 0/*rprd*/);
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_NONE;
#elif defined(CONFIG_TYPEC)
		pdic_data->typec_power_role = TYPEC_SINK;
		typec_set_pwr_role(pdic_data->port, pdic_data->typec_power_role);
#endif
		s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
					0/*attach*/, USB_STATUS_NOTIFY_DETACH/*drp*/);
		pdic_data->is_client = CLIENT_OFF;
	}
}

static int s2mu106_check_port_detect(struct s2mu106_usbpd_data *pdic_data)
{
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	u8 data, val;
	u8 cc1_val = 0, cc2_val = 0;
	int ret = 0;

	ret = s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_MON2, &data);
	if (ret < 0)
		dev_err(dev, "%s, i2c read PLUG_MON2 error\n", __func__);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_MON1, &val);

	cc1_val = val & S2MU106_REG_CTRL_MON_CC1_MASK;
	cc2_val = (val & S2MU106_REG_CTRL_MON_CC2_MASK) >> S2MU106_REG_CTRL_MON_CC2_SHIFT;

	pdic_data->cc1_val = cc1_val;
	pdic_data->cc2_val = cc2_val;

	dev_dbg_once(dev, "%s, attach cc pin check cc1_val(%x), cc2_val(%x)\n",
					__func__, cc1_val, cc2_val);

	if ((data & S2MU106_PR_MASK) == S2MU106_PDIC_SINK) {
		dev_dbg_once(dev, "SINK\n");
		pdic_data->detach_valid = false;
		pdic_data->power_role = PDIC_SINK;
		pdic_data->data_role = USBPD_UFP;
		s2mu106_snk(i2c);
		s2mu106_ufp(i2c);
		s2mu106_usbpd_prevent_watchdog_reset(pdic_data);
		usbpd_policy_reset(pd_data, PLUG_EVENT);
#if defined(CONFIG_CCIC_NOTIFIER)
		dev_dbg_once(&i2c->dev, "%s %d: is_host = %d, is_client = %d\n", __func__,
					__LINE__, pdic_data->is_host, pdic_data->is_client);
		if (pdic_data->regulator_en) {
			ret = regulator_enable(pdic_data->regulator);
			if (ret)
				dev_err(&i2c->dev, "Failed to enable vconn LDO: %d\n", ret);
		}

		s2mu106_usbpd_check_host(pdic_data, HOST_ON);
		/* muic */
		s2mu106_ccic_event_work(pdic_data,
			CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_ATTACH, 1/*attach*/, 0/*rprd*/);
		if (!(pdic_data->rid == REG_RID_523K || pdic_data->rid == REG_RID_619K)) {
			if (pdic_data->is_client == CLIENT_OFF && pdic_data->is_host == HOST_OFF) {
				/* usb */
				pdic_data->is_client = CLIENT_ON;
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
				pdic_data->power_role_dual = DUAL_ROLE_PROP_PR_SNK;
#elif defined(CONFIG_TYPEC)
				pdic_data->typec_power_role = TYPEC_SINK;
				typec_set_pwr_role(pdic_data->port, pdic_data->typec_power_role);
#endif
				s2mu106_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
						1/*attach*/, USB_STATUS_NOTIFY_ATTACH_UFP/*drp*/);
			}
		}
#endif
		s2mu106_vbus_short_check(pdic_data);
	} else if ((data & S2MU106_PR_MASK) == S2MU106_PDIC_SOURCE) {
		ret = s2mu106_usbpd_check_abnormal_attach(pdic_data);
		if (ret == false) {
			dev_err(&i2c->dev, "%s, abnormal attach\n", __func__);
			return -1;
		}
		s2mu106_usbpd_set_threshold(pdic_data, PLUG_CTRL_RP,
											S2MU106_THRESHOLD_MAX);
		dev_dbg_once(dev, "SOURCE\n");
		ret = s2mu106_usbpd_check_accessory(pdic_data);
		if (ret < 0) {
			dev_dbg_once(&i2c->dev, "%s attach accessory\n", __func__);
			return -1;
		}
		pdic_data->detach_valid = false;
		pdic_data->power_role = PDIC_SOURCE;
		pdic_data->data_role = USBPD_DFP;
		s2mu106_dfp(i2c);
		s2mu106_src(i2c);
		usbpd_policy_reset(pd_data, PLUG_EVENT);
#if defined(CONFIG_CCIC_NOTIFIER)
		dev_dbg_once(&i2c->dev, "%s %d: is_host = %d, is_client = %d\n", __func__,
					__LINE__, pdic_data->is_host, pdic_data->is_client);
		s2mu106_usbpd_check_client(pdic_data, CLIENT_ON);
		s2mu106_usbpd_check_host(pdic_data, HOST_OFF);
#else
		usbpd_manager_plug_attach(dev, ATTACHED_DEV_TYPE3_ADAPTER_MUIC);
#endif
		if (pdic_data->regulator_en) {
			ret = regulator_enable(pdic_data->regulator);
			if (ret)
				dev_err(&i2c->dev, "Failed to enable vconn LDO: %d\n", ret);
		}

		s2mu106_set_vconn_source(pd_data, USBPD_VCONN_ON);

//		msleep(tTypeCSinkWaitCap); /* dont over 310~620ms(tTypeCSinkWaitCap) */
		msleep(100); /* dont over 310~620ms(tTypeCSinkWaitCap) */
	} else {
		dev_err(dev, "%s, PLUG Error\n", __func__);
		return -1;
	}

	pdic_data->detach_valid = false;

	s2mu106_set_irq_enable(pdic_data, ENABLED_INT_0, ENABLED_INT_1,
				ENABLED_INT_2, ENABLED_INT_3, ENABLED_INT_4, ENABLED_INT_5);

	return ret;
}

static int s2mu106_check_init_port(struct s2mu106_usbpd_data *pdic_data)
{
	u8 data;
	int ret = 0;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;

	ret = s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_MON2, &data);
	if (ret < 0)
		dev_err(dev, "%s, i2c read PLUG_MON2 error\n", __func__);

	if ((data & S2MU106_PR_MASK) == S2MU106_PDIC_SOURCE)
		return PDIC_SOURCE;
	else if ((data & S2MU106_PR_MASK) == S2MU106_PDIC_SINK)
		return PDIC_SINK;

	return -1;
}

#if defined(CONFIG_SEC_FACTORY)
static int s2mu106_usbpd_check_619k(struct s2mu106_usbpd_data *pdic_data)
{
	u8 rid = 0;

	if (pdic_data->rid != REG_RID_619K)
		return false;

	msleep(250);
	s2mu106_usbpd_read_reg(pdic_data->i2c, S2MU106_REG_ADC_STATUS, &rid);
	rid = (rid & S2MU106_PDIC_RID_MASK) >> S2MU106_PDIC_RID_SHIFT;
	dev_dbg_once(pdic_data->dev, "%s %d: Detached, check if still 619K? => 0x%X\n",
					__func__, __LINE__, rid);

	return (rid == REG_RID_619K) ? true : false;
}
#endif

static irqreturn_t s2mu106_irq_thread(int irq, void *data)
{
	struct s2mu106_usbpd_data *pdic_data = data;
	struct i2c_client *i2c = pdic_data->i2c;
	struct device *dev = &i2c->dev;
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	int ret = 0;
	unsigned attach_status = 0, rid_status = 0;

	dev_dbg_once(dev, "%s\n", __func__);

	mutex_lock(&pd_data->accept_mutex);
	mutex_unlock(&pd_data->accept_mutex);

	mutex_lock(&pdic_data->_mutex);

	s2mu106_poll_status(pd_data);

#ifndef CONFIG_SEC_FACTORY
	if (pdic_data->lpcharge_water)
		goto out;
#endif

	if (s2mu106_get_status(pd_data, PLUG_DETACH)) {
#if defined(CONFIG_SEC_FACTORY)
		ret = s2mu106_usbpd_check_619k(pdic_data);
		if (ret)
			goto skip_detach;
#endif /* CONFIG_SEC_FACTORY */
		s2mu106_usbpd_set_rp_scr_sel(pdic_data, PLUG_CTRL_RP80);
		attach_status = s2mu106_get_status(pd_data, PLUG_ATTACH);
		rid_status = s2mu106_get_status(pd_data, MSG_RID);
		s2mu106_usbpd_detach_init(pdic_data);
		s2mu106_usbpd_notify_detach(pdic_data);
		if (attach_status) {
			ret = s2mu106_check_port_detect(pdic_data);
			if (ret >= 0) {
				if (rid_status) {
					s2mu106_usbpd_check_rid(pdic_data);
				}
				goto hard_reset;
			}
		}

		goto out;
	}

	if (s2mu106_get_status(pd_data, MSG_HARDRESET)) {
		mutex_lock(&pdic_data->cc_mutex);
		s2mu106_usbpd_set_cc_control(pdic_data, USBPD_CC_OFF);
		mutex_unlock(&pdic_data->cc_mutex);
		s2mu106_self_soft_reset(i2c);
		pdic_data->status_reg = 0;
		if (pdic_data->power_role == PDIC_SOURCE)
			s2mu106_dfp(i2c);
		else
			s2mu106_ufp(i2c);
		usbpd_rx_hard_reset(dev);
		usbpd_kick_policy_work(dev);
		goto out;
	}

#if defined(CONFIG_SEC_FACTORY)
skip_detach:
#endif /* CONFIG_SEC_FACTORY */
	if (s2mu106_get_status(pd_data, PLUG_ATTACH) && !pdic_data->is_pr_swap) {
		if (s2mu106_check_port_detect(data) < 0)
			goto out;
	}

	if (s2mu106_get_status(pd_data, MSG_RID)) {
		s2mu106_usbpd_check_rid(pdic_data);
	}

	if (s2mu106_get_status(pd_data, MSG_NONE))
		goto out;
hard_reset:
	mutex_lock(&pdic_data->lpm_mutex);
	if (!pdic_data->lpm_mode)
		usbpd_kick_policy_work(dev);
	mutex_unlock(&pdic_data->lpm_mutex);
out:
	mutex_unlock(&pdic_data->_mutex);

	return IRQ_HANDLED;
}

static void s2mu106_usbpd_plug_work(struct work_struct *work)
{
	struct s2mu106_usbpd_data *pdic_data =
		container_of(work, struct s2mu106_usbpd_data, plug_work.work);

	s2mu106_irq_thread(-1, pdic_data);
}

#ifdef S2MU106_TA_DEBUG
static void s2mu106_usbpd_poll_check(struct work_struct *work)
{
	struct s2mu106_usbpd_data *pdic_data =
		container_of(work, struct s2mu106_usbpd_data, s2mu106_poll_check.work);

	s2mu106_usbpd_test_read(pdic_data);

	schedule_delayed_work(&pdic_data->s2mu106_poll_check, msecs_to_jiffies(30000));
}
#endif

static int s2mu106_usbpd_reg_init(struct s2mu106_usbpd_data *_data)
{
	struct i2c_client *i2c = _data->i2c;
	u8 data = 0;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PHY_CTRL_IFG, &data);
	data |= S2MU106_PHY_IFG_35US << S2MU106_REG_IFG_SHIFT;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PHY_CTRL_IFG, data);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_MSG_SEND_CON, &data);
	data |= S2MU106_REG_MSG_SEND_CON_HARD_EN;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_MSG_SEND_CON, data);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_VBUS_MUX, &data);
	data &= ~(S2MU106_REG_RD_OR_VBUS_MUX_SEL);
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_VBUS_MUX, data);

	/* for SMPL issue */
	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_ANALOG_OTP_0A, &data);
	data |= S2MU106_REG_OVP_ON;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_ANALOG_OTP_0A, data);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PD_CTRL_2, &data);
	data &= ~S2MU106_REG_CC_OCP_MASK;
	data |= S2MU106_CC_OCP_575MV << S2MU106_REG_CC_OCP_SHIFT;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PD_CTRL_2, data);

	/* enable Rd monitor status when cc is attached at sink */
	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_SET_MON, &data);
	data |= S2MU106_REG_PLUG_CTRL_SET_MON_RD;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_SET_MON, data);

	/* disable rd or vbus mux */
	/* Setting for CC Detection with VBUS */
	/* It is recognized that VBUS falls when CC line falls */
	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_VBUS_MUX, &data);
	data &= ~S2MU106_REG_RD_OR_VBUS_MUX_SEL;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_VBUS_MUX, data);

	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PHY_CTRL_00, 0x80);
	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_BMC_CTRL, &data);
	data |= 0x01 << 2;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_BMC_CTRL, data);

	/* set debounce time */
	/* 0F3C = 3900/300 = 13ms */
	s2mu106_usbpd_write_reg(i2c, 0x20, 0x3C);
	s2mu106_usbpd_write_reg(i2c, 0x21, 0x0F);

	/* enable support acc */
	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC_HOLD, &data);
	data |= 0x80;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC_HOLD, data);

	data = 0;
	data |= (S2MU106_REG_PLUG_CTRL_SSM_DISABLE |
					S2MU106_REG_PLUG_CTRL_VDM_DISABLE |
						S2MU106_REG_PLUG_CTRL_REG_UFP_ATTACH_OPT_EN);
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL, data);

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, &data);
	data &= ~S2MU106_REG_PLUG_CTRL_CC_MANUAL_MASK;
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_CC12, data);

	/* set Rd threshold to 400mV */
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_SET_RD_2, S2MU106_THRESHOLD_600MV);
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_SET_RP_2, S2MU106_THRESHOLD_1200MV);
#ifdef CONFIG_SEC_FACTORY
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_SET_RD, S2MU106_THRESHOLD_342MV | 0x40);
#else
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_SET_RD, S2MU106_THRESHOLD_257MV | 0x40);
#endif
	s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_SET_RP, S2MU106_THRESHOLD_MAX);

	if (_data->vconn_en) {
		/* Off Manual Rd setup & On Manual Vconn setup */
		s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, &data);
		data &= ~(S2MU106_REG_PLUG_CTRL_RpRd_MANUAL_EN_MASK);
		data |= S2MU106_REG_PLUG_CTRL_VCONN_MANUAL_EN;
		s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_RpRd, data);
	}
#ifdef CONFIG_PM_S2MU106
	s2mu106_usbpd_set_pmeter_mode(_data, PM_TYPE_VCHGIN);
#endif
	s2mu106_usbpd_set_vconn_manual(_data, true);

	return 0;
}

static irqreturn_t s2mu106_irq_isr(int irq, void *data)
{
	return IRQ_WAKE_THREAD;
}

static int s2mu106_usbpd_irq_init(struct s2mu106_usbpd_data *_data)
{
	struct i2c_client *i2c = _data->i2c;
	struct device *dev = &i2c->dev;
	int ret = 0;

	if (!_data->irq_gpio) {
		dev_err(dev, "%s No interrupt specified\n", __func__);
		return -ENXIO;
	}

	i2c->irq = gpio_to_irq(_data->irq_gpio);

	if (i2c->irq) {
		ret = request_threaded_irq(i2c->irq, s2mu106_irq_isr,
				s2mu106_irq_thread,
				(IRQF_TRIGGER_LOW | IRQF_ONESHOT | IRQF_NO_SUSPEND),
				"s2mu106-usbpd", _data);
		if (ret < 0) {
			dev_err(dev, "%s failed to request irq(%d)\n",
					__func__, i2c->irq);
			return ret;
		}

		ret = enable_irq_wake(i2c->irq);
		if (ret < 0)
			dev_err(dev, "%s failed to enable wakeup src\n",
					__func__);
	}

	if (_data->lpm_mode)
		s2mu106_set_irq_enable(_data, 0, 0, 0, 0, 0, 0);
	else
		s2mu106_set_irq_enable(_data, ENABLED_INT_0, ENABLED_INT_1,
			ENABLED_INT_2, ENABLED_INT_3, ENABLED_INT_4, ENABLED_INT_5);

	return ret;
}

static void s2mu106_usbpd_init_configure(struct s2mu106_usbpd_data *_data)
{
	struct i2c_client *i2c = _data->i2c;
	struct device *dev = _data->dev;
	u8 rid = 0;
	int pdic_port = 0;

	s2mu106_usbpd_read_reg(i2c, S2MU106_REG_ADC_STATUS, &rid);

	rid = (rid & S2MU106_PDIC_RID_MASK) >> S2MU106_PDIC_RID_SHIFT;

	_data->rid = rid;

	_data->detach_valid = false;

	/* if there is rid, assume that booted by normal mode */
	if (rid) {
		_data->lpm_mode = false;
		_data->is_factory_mode = false;
		s2mu106_usbpd_set_rp_scr_sel(_data, PLUG_CTRL_RP80);
#if defined(CONFIG_SEC_FACTORY)
		if (factory_mode) {
			if (rid != REG_RID_523K) {
				dev_err(dev, "%s : In factory mode, but RID is not 523K\n", __func__);
			} else {
				dev_err(dev, "%s : In factory mode, but RID is 523K OK\n", __func__);
				_data->is_factory_mode = true;
			}
		}
#endif
		s2mu106_usbpd_set_cc_control(_data, USBPD_CC_ON);
	} else {
		dev_err(dev, "%s : Initial abnormal state to LPM Mode\n",
								__func__);
		s2mu106_usbpd_test_read(_data);
		s2mu106_usbpd_set_vbus_wakeup(_data, VBUS_WAKEUP_DISABLE);
		s2mu106_usbpd_set_vbus_wakeup(_data, VBUS_WAKEUP_ENABLE);
		usleep_range(1000, 1100);
		pdic_port = s2mu106_check_init_port(_data);
		s2mu106_set_normal_mode(_data);
		msleep(25);
		_data->detach_valid = true;
		s2mu106_set_lpm_mode(_data);
		_data->detach_valid = false;
		s2mu106_usbpd_set_cc_control(_data, USBPD_CC_OFF);
		_data->lpm_mode = true;
		msleep(150); /* for abnormal PD TA */
		_data->is_factory_mode = false;
#if	(!defined(CONFIG_SEC_FACTORY) && defined(CONFIG_CCIC_MODE_BY_MUIC))
		if (pdic_port == PDIC_SOURCE)
			s2mu106_set_normal_mode(_data);
#else
		s2mu106_set_normal_mode(_data);
		_data->lpm_mode = false;
#endif
	}
}

static int s2mu106_usbpd_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_AUTHENTIC:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mu106_usbpd_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct s2mu106_usbpd_data *pdic_data =
		power_supply_get_drvdata(psy);
	struct i2c_client *i2c = pdic_data->i2c;
	u8 data = 0;

	switch (psp) {
		case POWER_SUPPLY_PROP_AUTHENTIC:
			s2mu106_usbpd_read_reg(i2c, S2MU106_REG_PLUG_CTRL_VBUS_MUX, &data);
			data &= ~(S2MU106_REG_RD_OR_VBUS_MUX_SEL);
			s2mu106_usbpd_write_reg(i2c, S2MU106_REG_PLUG_CTRL_VBUS_MUX, data);
			break;
		case POWER_SUPPLY_PROP_USBPD_RESET:
			s2mu106_usbpd_set_vbus_wakeup(pdic_data, VBUS_WAKEUP_DISABLE);
			s2mu106_usbpd_set_vbus_wakeup(pdic_data, VBUS_WAKEUP_ENABLE);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

int s2mu106_usbpd_psy_init(struct s2mu106_usbpd_data *_data, struct device *parent)
{
	struct power_supply_config psy_cfg = {};
	int ret = 0;

	if (_data == NULL || parent == NULL) {
		pr_err("%s NULL data\n", __func__);
		return -1;
	}

	_data->ccic_desc.name           = "s2mu106-usbpd";
	_data->ccic_desc.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	_data->ccic_desc.get_property   = s2mu106_usbpd_get_property;
	_data->ccic_desc.set_property   = s2mu106_usbpd_set_property;
	_data->ccic_desc.properties     = ccic_props;
	_data->ccic_desc.num_properties = ARRAY_SIZE(ccic_props);

	psy_cfg.drv_data = _data;
	psy_cfg.supplied_to = ccic_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(ccic_supplied_to);

	_data->psy_ccic = power_supply_register(parent, &_data->ccic_desc, &psy_cfg);
	if (IS_ERR(_data->psy_ccic)) {
		ret = (int)PTR_ERR(_data->psy_ccic);
		pr_err("%s: Failed to Register psy_ccic, ret : %d\n", __func__, ret);
	}
	return ret;
}

static void s2mu106_usbpd_pdic_data_init(struct s2mu106_usbpd_data *_data)
{
	_data->vconn_source = USBPD_VCONN_OFF;
	_data->rid = REG_RID_MAX;
	_data->is_host = 0;
	_data->is_client = 0;
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	_data->data_role_dual = 0;
	_data->power_role_dual = 0;
#elif defined(CONFIG_TYPEC)
	_data->typec_power_role = TYPEC_SINK;
	_data->typec_data_role = TYPEC_DEVICE;
#endif
	_data->is_water_detect = false;
	_data->is_muic_water_detect = false;
	_data->detach_valid = true;
	_data->is_otg_vboost = false;
	_data->is_otg_reboost = false;
	_data->is_pr_swap = false;
	_data->rp_lvl = PLUG_CTRL_RP80;
	_data->vbus_short = false;
	_data->vbus_short_check = false;
	_data->pd_vbus_short_check = false;
	_data->vbus_short_check_cnt = 0;
	_data->lpcharge_water = false;
	_data->pm_cc1 = 0;
	_data->pm_cc2 = 0;
	_data->is_killer = 0;
}

static int of_s2mu106_dt(struct device *dev,
			struct s2mu106_usbpd_data *_data)
{
	struct device_node *np_usbpd = dev->of_node;
	int ret = 0;

	if (np_usbpd == NULL) {
		dev_err(dev, "%s np NULL\n", __func__);
		return -EINVAL;
	} else {
		_data->irq_gpio = of_get_named_gpio(np_usbpd,
							"usbpd,usbpd_int", 0);
		if (_data->irq_gpio < 0) {
			dev_err(dev, "error reading usbpd irq = %d\n",
						_data->irq_gpio);
			_data->irq_gpio = 0;
		}
		if (of_find_property(np_usbpd, "vconn-en", NULL))
			_data->vconn_en = true;
		else
			_data->vconn_en = false;

		if (of_find_property(np_usbpd, "regulator-en", NULL))
			_data->regulator_en = true;
		else
			_data->regulator_en = false;
	}

	return ret;
}

static int s2mu106_usbpd_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(i2c->dev.parent);
	struct s2mu106_usbpd_data *pdic_data;
	struct usbpd_data *pd_data;
	struct device *dev = &i2c->dev;
	int ret = 0;
#if defined(CONFIG_CCIC_NOTIFIER)
	pccic_data_t pccic_data;
	pccic_sysfs_property_t pccic_sysfs_prop;
#endif

	dev_dbg_once(dev, "%s called\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "%s: i2c functionality check error\n", __func__);
		ret = -EIO;
		goto err_return;
	}

	pdic_data = kzalloc(sizeof(struct s2mu106_usbpd_data), GFP_KERNEL);
	if (!pdic_data) {
		dev_err(dev, "%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_return;
	}

	/* save platfom data for gpio control functions */
	pdic_data->dev = &i2c->dev;
	pdic_data->i2c = i2c;
	i2c_set_clientdata(i2c, pdic_data);

	ret = of_s2mu106_dt(&i2c->dev, pdic_data);
	if (ret < 0)
		dev_err(dev, "%s: not found dt!\n", __func__);

	mutex_init(&pdic_data->_mutex);
	mutex_init(&pdic_data->lpm_mutex);
	mutex_init(&pdic_data->cc_mutex);
	mutex_init(&pdic_data->water_mutex);

	s2mu106_usbpd_reg_init(pdic_data);
	s2mu106_usbpd_init_configure(pdic_data);
	s2mu106_usbpd_pdic_data_init(pdic_data);

	if (pdic_data->regulator_en) {
		pdic_data->regulator = devm_regulator_get(dev, "vconn");
		if (IS_ERR(pdic_data->regulator)) {
			dev_err(dev, "%s: not found regulator vconn\n", __func__);
			pdic_data->regulator_en = false;
		} else
			ret = regulator_disable(pdic_data->regulator);
	}

	ret = usbpd_init(dev, pdic_data);
	if (ret < 0) {
		dev_err(dev, "failed on usbpd_init\n");
		goto err_return;
	}

	pd_data = dev_get_drvdata(dev);
	pd_data->ip_num = S2MU106_USBPD_IP;
	usbpd_set_ops(dev, &s2mu106_ops);

	pdic_data->pdic_queue =
	    alloc_workqueue(dev_name(dev), WQ_MEM_RECLAIM, 1);
	if (!pdic_data->pdic_queue) {
		dev_err(dev,
			"%s: Fail to Create Workqueue\n", __func__);
		goto err_return;
	}

#if defined(CONFIG_CCIC_NOTIFIER)
	/* Create a work queue for the ccic irq thread */
	pdic_data->ccic_wq
		= create_singlethread_workqueue("ccic_irq_event");
	if (!pdic_data->ccic_wq) {
		pr_err("%s failed to create work queue for ccic notifier\n",
			__func__);
		goto err_return;
	}
	if (pdic_data->rid == REG_RID_UNDF)
		pdic_data->rid = REG_RID_MAX;
	pccic_data = kzalloc(sizeof(ccic_data_t), GFP_KERNEL);
	pccic_sysfs_prop = kzalloc(sizeof(ccic_sysfs_property_t), GFP_KERNEL);
	pccic_sysfs_prop->get_property = s2mu106_sysfs_get_prop;
	pccic_sysfs_prop->set_property = s2mu106_sysfs_set_prop;
	pccic_sysfs_prop->property_is_writeable = s2mu106_sysfs_is_writeable;
	pccic_sysfs_prop->properties = s2mu106_sysfs_properties;
	pccic_sysfs_prop->num_properties = ARRAY_SIZE(s2mu106_sysfs_properties);
	pccic_data->ccic_syfs_prop = pccic_sysfs_prop;
	pccic_data->drv_data = pdic_data;
	pccic_data->name = "s2mu106";
	ccic_core_register_chip(pccic_data);
	ccic_misc_init(pccic_data);
	pccic_data->misc_dev->uvdm_read = samsung_uvdm_in_request_message;
	pccic_data->misc_dev->uvdm_write = samsung_uvdm_out_request_message;
	pccic_data->misc_dev->uvdm_ready = samsung_uvdm_ready;
	pccic_data->misc_dev->uvdm_close = samsung_uvdm_close;
#endif

#if defined(CONFIG_TYPEC)
	ret = s2mu106_typec_init(pdic_data);
	if (ret < 0) {
		pr_err("failed to init typec\n");
		goto err_return;
	}
#endif

	INIT_DELAYED_WORK(&pdic_data->water_detect_handler, s2mu106_pdic_water_detect_handler);
	INIT_DELAYED_WORK(&pdic_data->water_dry_handler, s2mu106_pdic_water_dry_handler);
	INIT_DELAYED_WORK(&pdic_data->plug_work, s2mu106_usbpd_plug_work);
	ret = s2mu106_usbpd_irq_init(pdic_data);
	if (ret) {
		dev_err(dev, "%s: failed to init irq(%d)\n", __func__, ret);
		goto fail_init_irq;
	}

	if (pdic_data->detach_valid) {
		mutex_lock(&pdic_data->_mutex);
		s2mu106_check_port_detect(pdic_data);
		s2mu106_usbpd_check_rid(pdic_data);
		mutex_unlock(&pdic_data->_mutex);
	}

#if defined(CONFIG_BATTERY_SAMSUNG)
#if !defined(CONFIG_SEC_FACTORY) && IS_ENABLED(CONFIG_S2MU106_TYPEC_WATER)
	if (lpcharge)
		s2mu106_power_off_water_check(pdic_data);
#endif
#endif

	s2mu106_irq_thread(-1, pdic_data);

#if defined(CONFIG_MUIC_NOTIFIER)
	muic_ccic_notifier_register(&pdic_data->type3_nb,
			       s2mu106_type3_handle_notification,
			       MUIC_NOTIFY_DEV_PDIC);
#endif
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	ret = s2mu106_dual_role_init(pdic_data);
	if (ret < 0) {
		pr_err("unable to allocate dual role descriptor\n");
		goto fail_init_irq;
	}
#endif

	pdic_data->psy_pm = get_power_supply_by_name("s2mu106_pmeter");
	if (!pdic_data->psy_pm) {
		pr_err("%s: Fail to get pmeter\n", __func__);
	}

	ret = s2mu106_usbpd_psy_init(pdic_data, &i2c->dev);
	if (ret < 0) {
		pr_err("faled to register the ccic psy.\n");
	}

	dev_dbg_once(dev, "%s s2mu106 usbpd driver uploaded!\n", __func__);

#ifdef S2MU106_TA_DEBUG
	INIT_DELAYED_WORK(&pdic_data->s2mu106_poll_check, s2mu106_usbpd_poll_check);
	schedule_delayed_work(&pdic_data->s2mu106_poll_check, msecs_to_jiffies(30000));
#endif

	return 0;

fail_init_irq:
	if (i2c->irq)
		free_irq(i2c->irq, pdic_data);
err_return:
	return ret;
}

#if defined CONFIG_PM
static int s2mu106_usbpd_suspend(struct device *dev)
{
	struct usbpd_data *_data = dev_get_drvdata(dev);
	struct s2mu106_usbpd_data *pdic_data = _data->phy_driver_data;

	if (device_may_wakeup(dev))
		enable_irq_wake(pdic_data->i2c->irq);

#ifndef CONFIG_SEC_FACTORY
	disable_irq(pdic_data->i2c->irq);
#endif
	return 0;
}

static int s2mu106_usbpd_resume(struct device *dev)
{
	struct usbpd_data *_data = dev_get_drvdata(dev);
	struct s2mu106_usbpd_data *pdic_data = _data->phy_driver_data;

	if (device_may_wakeup(dev))
		disable_irq_wake(pdic_data->i2c->irq);

#ifndef CONFIG_SEC_FACTORY
	enable_irq(pdic_data->i2c->irq);
#endif
	return 0;
}
#else
#define s2mu106_muic_suspend NULL
#define s2mu106_muic_resume NULL
#endif

static int s2mu106_usbpd_remove(struct i2c_client *i2c)
{
	struct s2mu106_usbpd_data *_data = i2c_get_clientdata(i2c);
	struct usbpd_data *pd_data = dev_get_drvdata(&i2c->dev);

	if (_data) {
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
		devm_dual_role_instance_unregister(_data->dev,
						_data->dual_role);
		devm_kfree(_data->dev, _data->desc);
#elif defined(CONFIG_TYPEC)
		typec_unregister_port(_data->port);
#endif
#if defined(CONFIG_CCIC_NOTIFIER)
		ccic_register_switch_device(0);
		ccic_misc_exit();
#endif
		disable_irq_wake(_data->i2c->irq);
		free_irq(_data->i2c->irq, _data);
		mutex_destroy(&_data->_mutex);
		mutex_destroy(&_data->water_mutex);
		i2c_set_clientdata(_data->i2c, NULL);
		kfree(_data);
	}
	if (pd_data) {
		wake_lock_destroy(&pd_data->policy_wake);
		mutex_destroy(&pd_data->accept_mutex);
	}

	return 0;
}

static const struct i2c_device_id s2mu106_usbpd_i2c_id[] = {
	{ S2MU106_USBPD_DEV_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, s2mu106_i2c_id);

static struct of_device_id s2mu106_usbpd_i2c_dt_ids[] = {
	{ .compatible = "s2mu106-usbpd" },
	{ }
};

static void s2mu106_usbpd_shutdown(struct i2c_client *i2c)
{
	struct s2mu106_usbpd_data *_data = i2c_get_clientdata(i2c);
	if (!_data->i2c)
		return;
}

static usbpd_phy_ops_type s2mu106_ops = {
	.tx_msg			= s2mu106_tx_msg,
	.rx_msg			= s2mu106_rx_msg,
	.hard_reset		= s2mu106_hard_reset,
	.soft_reset		= s2mu106_soft_reset,
	.set_power_role		= s2mu106_set_power_role,
	.get_power_role		= s2mu106_get_power_role,
	.set_data_role		= s2mu106_set_data_role,
	.get_data_role		= s2mu106_get_data_role,
	.set_vconn_source	= s2mu106_set_vconn_source,
	.get_vconn_source	= s2mu106_get_vconn_source,
	.get_status			= s2mu106_get_status,
	.poll_status		= s2mu106_poll_status,
	.driver_reset		= s2mu106_driver_reset,
	.set_otg_control	= s2mu106_set_otg_control,
	.get_vbus_short_check	= s2mu106_get_vbus_short_check,
	.pd_vbus_short_check	= s2mu106_pd_vbus_short_check,
	.set_cc_control		= s2mu106_set_cc_control,
	.send_pd_info		= s2mu106_send_pd_info,
	.set_chg_lv_mode	= s2mu106_set_chg_lv_mode,
#if defined(CONFIG_CHECK_CTYPE_SIDE) || defined(CONFIG_CCIC_SYSFS)
	.get_side_check		= s2mu106_get_side_check,
#endif
	.pr_swap			= s2mu106_pr_swap,
	.vbus_on_check		= s2mu106_vbus_on_check,
	.set_rp_control		= s2mu106_set_rp_control,
	.cc_instead_of_vbus = s2mu106_cc_instead_of_vbus,
	.op_mode_clear		= s2mu106_op_mode_clear,
#if defined(CONFIG_TYPEC)
	.set_pwr_opmode		= s2mu106_set_pwr_opmode,
#endif
#if defined(CONFIG_PDIC_PD30)
	.send_ocp_info		= s2mu106_send_ocp_info,
#endif
};

#if defined CONFIG_PM
const struct dev_pm_ops s2mu106_usbpd_pm = {
	.suspend = s2mu106_usbpd_suspend,
	.resume = s2mu106_usbpd_resume,
};
#endif

static struct i2c_driver s2mu106_usbpd_driver = {
	.driver		= {
		.name	= S2MU106_USBPD_DEV_NAME,
		.of_match_table	= s2mu106_usbpd_i2c_dt_ids,
#if defined CONFIG_PM
		.pm	= &s2mu106_usbpd_pm,
#endif /* CONFIG_PM */
	},
	.probe		= s2mu106_usbpd_probe,
	.remove		= s2mu106_usbpd_remove,
	.shutdown	= s2mu106_usbpd_shutdown,
	.id_table	= s2mu106_usbpd_i2c_id,
};

static int __init s2mu106_usbpd_init(void)
{
	return i2c_add_driver(&s2mu106_usbpd_driver);
}
late_initcall(s2mu106_usbpd_init);

static void __exit s2mu106_usbpd_exit(void)
{
	i2c_del_driver(&s2mu106_usbpd_driver);
}
module_exit(s2mu106_usbpd_exit);

MODULE_DESCRIPTION("S2MU106 USB PD driver");
MODULE_LICENSE("GPL");
