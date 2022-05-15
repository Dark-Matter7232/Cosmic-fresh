/*
 * Copyrights (C) 2017 Samsung Electronics, Inc.
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
 */

#include <linux/notifier.h>

#if defined(CONFIG_BATTERY_NOTIFIER)
#include <linux/battery/battery_notifier.h>
#endif

#undef pr_info
#undef pr_debug

struct pdic_notifier_struct pd_noti;

void (*fp_select_pdo)(int num);
int (*fp_sec_pd_select_pps)(int num, int ppsVol, int ppsCur);
int (*fp_sec_pd_get_apdo_max_power)(unsigned int *pdo_pos, unsigned int *taMaxVol, unsigned int *taMaxCur, unsigned int *taMaxPwr);
int (*fp_pps_enable)(int num, int ppsVol, int ppsCur, int enable);

void select_pdo(int num)
{
	if (fp_select_pdo)
		fp_select_pdo(num);
}

int sec_pd_select_pps(int num, int ppsVol, int ppsCur)
{
	if (fp_sec_pd_select_pps)
		return fp_sec_pd_select_pps(num, ppsVol, ppsCur);

	return 0;
}

int sec_pps_enable(int num, int ppsVol, int ppsCur, int enable)
{
	if (fp_pps_enable)
		return fp_pps_enable(num, ppsVol, ppsCur, enable);

	return 0;
}

int sec_pd_get_apdo_max_power(unsigned int *pdo_pos, unsigned int *taMaxVol, unsigned int *taMaxCur, unsigned int *taMaxPwr)
{
	if (fp_sec_pd_get_apdo_max_power)
		return fp_sec_pd_get_apdo_max_power(pdo_pos, taMaxVol, taMaxCur, taMaxPwr);

	return -ENOTSUPP;
}
