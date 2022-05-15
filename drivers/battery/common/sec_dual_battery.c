/*
 *  sec_dual_battery.c
 *  Samsung Mobile Charger Driver
 *
 *  Copyright (C) 2018 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG

#include "include/sec_battery.h"
#include "include/sec_dual_battery.h"

#undef pr_info
#undef pr_debug

static enum power_supply_property sec_dual_battery_props[] = {
};

static int sec_dual_check_eoc_status(struct sec_dual_battery_info *battery)
{
	union power_supply_propval value;

	/* check out main battery's eoc status */
	value.intval = SEC_BATTERY_VOLTAGE_MV;
	psy_do_property(battery->pdata->main_limiter_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_BAT_VOLTAGE, value);
	battery->main_voltage_avg = value.intval;

	value.intval = SEC_BATTERY_CURRENT_MA;
	psy_do_property(battery->pdata->main_limiter_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CHG_CURRENT, value);
	battery->main_current_avg = value.intval;

	pr_debug_once("%s %d %d %d %d \n", __func__, battery->main_voltage_avg, battery->pdata->main_full_condition_vcell, battery->main_current_avg, battery->pdata->main_full_condition_eoc);
	if(battery->main_voltage_avg >= battery->pdata->main_full_condition_vcell &&
		battery->main_current_avg <= battery->pdata->main_full_condition_eoc &&
		!(battery->full_total_status & SEC_DUAL_BATTERY_MAIN_CONDITION_DONE)) {
		pr_debug_once("%s Main Batt eoc condtion is done \n", __func__);
		battery->full_total_status |= SEC_DUAL_BATTERY_MAIN_CONDITION_DONE;
		/* main supplement mode enable */
		value.intval = 1;
		psy_do_property(battery->pdata->main_limiter_name, set,
			POWER_SUPPLY_PROP_CHARGE_FULL, value);
	}

	/* check out sub battery's eoc status */
	value.intval = SEC_BATTERY_VOLTAGE_MV;
	psy_do_property(battery->pdata->sub_limiter_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_BAT_VOLTAGE, value);
	battery->sub_voltage_avg = value.intval;

	value.intval = SEC_BATTERY_CURRENT_MA;
	psy_do_property(battery->pdata->sub_limiter_name, get,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CHG_CURRENT, value);
	battery->sub_current_avg = value.intval;

	pr_debug_once("%s %d %d %d %d \n", __func__, battery->sub_voltage_avg, battery->pdata->sub_full_condition_vcell, battery->sub_current_avg, battery->pdata->sub_full_condition_eoc);
	if(battery->sub_voltage_avg >= battery->pdata->sub_full_condition_vcell &&
		battery->sub_current_avg <= battery->pdata->sub_full_condition_eoc &&
		!(battery->full_total_status & SEC_DUAL_BATTERY_SUB_CONDITION_DONE)) {
		pr_debug_once("%s Sub Batt eoc condtion is done \n", __func__);
		battery->full_total_status |= SEC_DUAL_BATTERY_SUB_CONDITION_DONE;
		/* main supplement mode enable */
		value.intval = 1;
		psy_do_property(battery->pdata->sub_limiter_name, set,
			POWER_SUPPLY_PROP_CHARGE_FULL, value);
	}

	pr_debug_once("%s full satus = 0x%x \n", __func__, battery->full_total_status);

	if(battery->full_total_status == (SEC_DUAL_BATTERY_MAIN_CONDITION_DONE | SEC_DUAL_BATTERY_SUB_CONDITION_DONE))
		return POWER_SUPPLY_STATUS_FULL;
	else
		return POWER_SUPPLY_STATUS_CHARGING;
}

#if 0
static int sec_dual_check_eoc_current(struct sec_dual_battery_info *battery)
{
	union power_supply_propval value;

	psy_do_property(battery->pdata->main_limiter_name, get,
		POWER_SUPPLY_EXT_PROP_CHG_CURRENT, value);
	battery->main_current_avg = value.intval;

	if(battery->main_current_avg <= battery->pdata->main_full_condition_eoc) {
		battery->full_current_status |= SEC_DUAL_BATTERY_MAIN_CONDITION_DONE;
	}

	psy_do_property(battery->pdata->sub_limiter_name, get,
		POWER_SUPPLY_EXT_PROP_CHG_CURRENT, value);
	battery->sub_current_avg = value.intval;

	if(battery->sub_current_avg <= battery->pdata->sub_full_condition_eoc) {
		battery->full_current_status |= SEC_DUAL_BATTERY_MAIN_CONDITION_DONE;
	}

	if(battery->full_current_status == (SEC_DUAL_BATTERY_MAIN_CONDITION_DONE | SEC_DUAL_BATTERY_SUB_CONDITION_DONE))
		return POWER_SUPPLY_STATUS_FULL;
	else
		return POWER_SUPPLY_STATUS_CHARGING;
}
#endif

static int sec_dual_battery_current_avg(struct sec_dual_battery_info *battery, int bat_type, int mode)
{
	union power_supply_propval value;
	int ichg = 0, idis = 0;

	if(bat_type == SEC_DUAL_BATTERY_MAIN) {
		value.intval = SEC_BATTERY_CURRENT_MA;
		psy_do_property(battery->pdata->main_limiter_name, get,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CHG_CURRENT, value);
		ichg = value.intval;
		value.intval = SEC_BATTERY_CURRENT_MA;
		psy_do_property(battery->pdata->main_limiter_name, get,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DISCHG_CURRENT, value);
		idis = value.intval;
	} else {
		value.intval = SEC_BATTERY_CURRENT_MA;
		psy_do_property(battery->pdata->sub_limiter_name, get,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CHG_CURRENT, value);
		ichg = value.intval;
		value.intval = SEC_BATTERY_CURRENT_MA;
		psy_do_property(battery->pdata->sub_limiter_name, get,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DISCHG_CURRENT, value);
		idis = value.intval;
	}

	pr_debug_once("%s: ichg=%d, idis=%d\n", __func__, ichg, idis);

	if((ichg == 0) && (idis == 0))
		return 0;
	else {
		if(ichg != 0) return ichg;
		else return idis * (-1);
	}
}

static int sec_dual_battery_voltage_avg(struct sec_dual_battery_info *battery, int bat_type, int mode)
{
	union power_supply_propval value;
	int vbat = 0;

	if(bat_type == SEC_DUAL_BATTERY_MAIN) {
		value.intval = SEC_BATTERY_VOLTAGE_MV;
		psy_do_property(battery->pdata->main_limiter_name, get,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_BAT_VOLTAGE, value);
		vbat = value.intval;
	} else {
		value.intval = SEC_BATTERY_VOLTAGE_MV;
		psy_do_property(battery->pdata->sub_limiter_name, get,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_BAT_VOLTAGE, value);
		vbat = value.intval;
	}

	pr_debug_once("%s: vbat=%d\n", __func__, vbat);

	return vbat;
}

static int sec_dual_battery_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct sec_dual_battery_info *battery =
		power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property)psp;
	union power_supply_propval value;

	value.intval = val->intval;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
			val->intval = sec_dual_check_eoc_status(battery);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		if(value.intval == SEC_DUAL_BATTERY_MAIN)
			val->intval = sec_dual_battery_voltage_avg(battery, SEC_DUAL_BATTERY_MAIN, SEC_BATTERY_VOLTAGE_MV);
		else
			val->intval = sec_dual_battery_voltage_avg(battery, SEC_DUAL_BATTERY_SUB, SEC_BATTERY_VOLTAGE_MV);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		if(value.intval == SEC_DUAL_BATTERY_MAIN)
			val->intval = sec_dual_battery_current_avg(battery, SEC_DUAL_BATTERY_MAIN, SEC_BATTERY_CURRENT_MA);
		else
			val->intval = sec_dual_battery_current_avg(battery, SEC_DUAL_BATTERY_SUB, SEC_BATTERY_CURRENT_MA);
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_DUAL_BAT_DET:
			if (value.intval == SEC_DUAL_BATTERY_MAIN) {
				if (battery->pdata->main_bat_con_det_gpio) {
					val->intval = !gpio_get_value(battery->pdata->main_bat_con_det_gpio);
					pr_debug_once("%s : main det(%d) = %d \n", __func__, battery->pdata->main_bat_con_det_gpio, (int)value.intval);
				}
				else
					val->intval = -1;
			} else if (value.intval == SEC_DUAL_BATTERY_SUB) {
				if (battery->pdata->sub_bat_con_det_gpio) {
					val->intval = !gpio_get_value(battery->pdata->sub_bat_con_det_gpio);
					pr_debug_once("%s : sub det(%d) = %d \n", __func__, battery->pdata->sub_bat_con_det_gpio, (int)value.intval);
				}
				else
					val->intval = -1;
			}
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sec_dual_battery_set_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    const union power_supply_propval *val)
{
	struct sec_dual_battery_info *battery =
		power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property)psp;
	union power_supply_propval value;

	//value.intval = val->intval;
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if(val->intval == 0) {
			/* disable main/sub supplement mode */
			value.intval = 0;
			psy_do_property(battery->pdata->main_limiter_name, set,
				POWER_SUPPLY_PROP_CHARGE_FULL, value);
			psy_do_property(battery->pdata->sub_limiter_name, set,
				POWER_SUPPLY_PROP_CHARGE_FULL, value);
			battery->full_total_status = SEC_DUAL_BATTERY_NONE;
		}
		//else {
			//value.intval = 1;
			/* enable main/sub supplement mode */
			//psy_do_property(battery->pdata->main_limiter_name, set,
			//	POWER_SUPPLY_PROP_CHARGE_FULL, value);
			//psy_do_property(battery->pdata->sub_limiter_name, set,
			//	POWER_SUPPLY_PROP_CHARGE_FULL, value);
		//}
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		/* SET PWR OFF MODE 2*/
		if(val->intval == SEC_DUAL_BATTERY_MAIN) {
			value.intval = 1;
			psy_do_property(battery->pdata->main_limiter_name, set,
				POWER_SUPPLY_PROP_ENERGY_NOW, value);
		} else {
			value.intval = 1;
			psy_do_property(battery->pdata->sub_limiter_name, set,
				POWER_SUPPLY_PROP_ENERGY_NOW, value);
		}
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_OF
static int sec_dual_battery_parse_dt(struct device *dev,
		struct sec_dual_battery_info *battery)
{
	struct device_node *np = dev->of_node;
	struct sec_dual_battery_platform_data *pdata = battery->pdata;
	int ret = 0;
	//int len;
	//const u32 *p;

	if (!np) {
		pr_err_once("%s: np NULL\n", __func__);
		return 1;
	} else {
		ret = of_property_read_string(np, "battery,main_current_limiter",
				(char const **)&battery->pdata->main_limiter_name);
		if (ret)
			pr_err_once("%s: main_current_limiter is Empty\n", __func__);

		ret = of_property_read_string(np, "battery,sub_current_limiter",
				(char const **)&battery->pdata->sub_limiter_name);
		if (ret)
			pr_err_once("%s: sub_current_limiter is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,main_full_condition_vcell",
					&pdata->main_full_condition_vcell);
		if (ret < 0) {
			pr_debug_once("%s : main_full_condition_vcell is empty\n", __func__);
			pdata->main_full_condition_vcell = 4250;
		}

		ret = of_property_read_u32(np, "battery,sub_full_condition_vcell",
					&pdata->sub_full_condition_vcell);
		if (ret < 0) {
			pr_debug_once("%s : sub_full_condition_vcell is empty\n", __func__);
			pdata->sub_full_condition_vcell = 4250;
		}

		ret = of_property_read_u32(np, "battery,main_full_condition_eoc",
					&pdata->main_full_condition_eoc);
		if (ret < 0) {
			pr_debug_once("%s : main_full_condition_eoc is empty\n", __func__);
			pdata->main_full_condition_eoc = 100;
		}

		ret = of_property_read_u32(np, "battery,sub_full_condition_eoc",
					&pdata->sub_full_condition_eoc);
		if (ret < 0) {
			pr_debug_once("%s : sub_full_condition_eoc is empty\n", __func__);
			pdata->sub_full_condition_eoc = 100;
		}
		/* MAIN_BATTERY_CON_DET */
		ret = pdata->main_bat_con_det_gpio = of_get_named_gpio(np, "battery,main_bat_con_det_gpio", 0);
		if (ret < 0) {
			pr_debug_once("%s : can't get main_bat_con_det_gpio\n", __func__);
		}
		/* SUB_BATTERY_CON_DET */
		ret = pdata->sub_bat_con_det_gpio = of_get_named_gpio(np, "battery,sub_bat_con_det_gpio", 0);
		if (ret < 0) {
			pr_debug_once("%s : can't get sub_bat_con_det_gpio\n", __func__);
		}
	}
	return 0;
}
#endif

static const struct power_supply_desc sec_dual_battery_power_supply_desc = {
	.name = "sec-dual-battery",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = sec_dual_battery_props,
	.num_properties = ARRAY_SIZE(sec_dual_battery_props),
	.get_property = sec_dual_battery_get_property,
	.set_property = sec_dual_battery_set_property,
};

static int sec_dual_battery_probe(struct platform_device *pdev)
{
	struct sec_dual_battery_info *battery;
	struct sec_dual_battery_platform_data *pdata = NULL;
	struct power_supply_config dual_battery_cfg = {};
	int ret = 0;

	dev_info(&pdev->dev,
		"%s: SEC Dual Battery Driver Loading\n", __func__);

	battery = kzalloc(sizeof(*battery), GFP_KERNEL);
	if (!battery)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct sec_dual_battery_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_battery_free;
		}

		battery->pdata = pdata;
		if (sec_dual_battery_parse_dt(&pdev->dev, battery)) {
			dev_err(&pdev->dev,
				"%s: Failed to get sec-dual-battery dt\n", __func__);
			ret = -EINVAL;
			goto err_battery_free;
		}
	} else {
		pdata = dev_get_platdata(&pdev->dev);
		battery->pdata = pdata;
	}

	platform_set_drvdata(pdev, battery);
	battery->dev = &pdev->dev;
	dual_battery_cfg.drv_data = battery;

	battery->psy_bat = power_supply_register(&pdev->dev, &sec_dual_battery_power_supply_desc, &dual_battery_cfg);
	if (!battery->psy_bat) {
		dev_err(battery->dev,
			"%s: Failed to Register psy_bat\n", __func__);
		goto err_pdata_free;
	}

	dev_info(battery->dev,
		"%s: SEC Dual Battery Driver Loaded\n", __func__);
	return 0;

err_pdata_free:
	kfree(pdata);
err_battery_free:
	kfree(battery);

	return ret;
}

static int sec_dual_battery_remove(struct platform_device *pdev)
{
	struct sec_dual_battery_info *battery = platform_get_drvdata(pdev);

	power_supply_unregister(battery->psy_bat);

	dev_dbg(battery->dev, "%s: End\n", __func__);

	kfree(battery->pdata);
	kfree(battery);

	return 0;
}

static int sec_dual_battery_suspend(struct device *dev)
{
	return 0;
}

static int sec_dual_battery_resume(struct device *dev)
{
	return 0;
}

static void sec_dual_battery_shutdown(struct platform_device *pdev)
{
}

#ifdef CONFIG_OF
static struct of_device_id sec_dual_battery_dt_ids[] = {
	{ .compatible = "samsung,sec-dual-battery" },
	{ }
};
MODULE_DEVICE_TABLE(of, sec_dual_battery_dt_ids);
#endif /* CONFIG_OF */

static const struct dev_pm_ops sec_dual_battery_pm_ops = {
	.suspend = sec_dual_battery_suspend,
	.resume = sec_dual_battery_resume,
};

static struct platform_driver sec_dual_battery_driver = {
	.driver = {
		.name = "sec-dual-battery",
		.owner = THIS_MODULE,
		.pm = &sec_dual_battery_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = sec_dual_battery_dt_ids,
#endif
	},
	.probe = sec_dual_battery_probe,
	.remove = sec_dual_battery_remove,
	.shutdown = sec_dual_battery_shutdown,
};

static int __init sec_dual_battery_init(void)
{
	pr_debug_once("%s: \n", __func__);
	return platform_driver_register(&sec_dual_battery_driver);
}

static void __exit sec_dual_battery_exit(void)
{
	platform_driver_unregister(&sec_dual_battery_driver);
}

device_initcall_sync(sec_dual_battery_init);
module_exit(sec_dual_battery_exit);

MODULE_DESCRIPTION("Samsung Dual Battery Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
