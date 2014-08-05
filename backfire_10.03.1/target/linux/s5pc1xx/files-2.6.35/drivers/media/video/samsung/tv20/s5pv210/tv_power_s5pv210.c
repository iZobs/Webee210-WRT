/* linux/drivers/media/video/samsung/tv20/s5pv210/tv_power_s5pv210.c
 *
 * power raw ftn  file for Samsung TVOut driver
 *
 * Copyright (c) 2010 Samsung Electronics
 *	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include <mach/map.h>
#include <mach/regs-clock.h>

#include "tv_out_s5pv210.h"

#ifdef S5P_TVOUT_PM_DEBUG
#define TVPMPRINTK(fmt, args...) \
	pr_debug("\t\t[TVPM] %s: " fmt, __func__ , ## args)
#else
#define TVPMPRINTK(fmt, args...)
#endif

/* NORMAL_CFG */
#define TVPWR_SUBSYSTEM_ACTIVE (1<<4)
#define TVPWR_SUBSYSTEM_LP     (0<<4)

/* MTC_STABLE */
#define TVPWR_MTC_COUNTER_CLEAR(a) (((~0xf)<<16)&a)
#define TVPWR_MTC_COUNTER_SET(a)   ((0xf&a)<<16)

/* BLK_PWR_STAT */
#define TVPWR_TV_BLOCK_STATUS(a)    ((0x1<<4)&a)

static unsigned short g_dac_pwr_on;

void s5p_tv_power_init_mtc_stable_counter(unsigned int value)
{
	writel(TVPWR_MTC_COUNTER_CLEAR((readl(S5P_MTC_STABLE) |
		TVPWR_MTC_COUNTER_SET(value))),
		S5P_MTC_STABLE);

	TVPMPRINTK("value(%d), S5P_MTC_STABLE(0x%08x)\n",
		value, readl(S5P_MTC_STABLE));
}

void s5p_tv_power_init_dac_onoff(unsigned short on)
{
	g_dac_pwr_on = on;

	TVPMPRINTK("on(%d), g_dac_pwr_on(0x%08x)\n", on, g_dac_pwr_on);
}

void s5p_tv_power_set_dac_onoff(bool on)
{
	if (on == true)
		writel(S5P_DAC_ENABLE, S5P_DAC_CONTROL);
	else
		writel(S5P_DAC_DISABLE, S5P_DAC_CONTROL);

	TVPMPRINTK("on(%d), S5P_DAC_CONTROL(0x%08x)\n",
		on, readl(S5P_DAC_CONTROL));
}

unsigned short s5p_tv_power_get_power_status(void)
{
	TVPMPRINTK("S5P_BLK_PWR_STAT(0x%08x)\n", readl(S5P_BLK_PWR_STAT));

	return TVPWR_TV_BLOCK_STATUS(readl(S5P_BLK_PWR_STAT)) ? 1 : 0;
}

unsigned short s5p_tv_power_get_dac_power_status(void)
{
	TVPMPRINTK("S5P_DAC_CONTROL(0x%08x)\n", readl(S5P_DAC_CONTROL));

	return (readl(S5P_DAC_CONTROL) & S5P_DAC_ENABLE) ? 1 : 0;
}

void s5p_tv_power_on(void)
{
	int time_out = HDMI_TIME_OUT;

	TVPMPRINTK("S3C_VA_SYS + 0xE804(0x%08x)\n",
		readl(S3C_VA_SYS + 0xE804));

	writel(readl(S3C_VA_SYS + 0xE804) | 0x1, S3C_VA_SYS + 0xE804);

	writel(readl(S5P_NORMAL_CFG) | TVPWR_SUBSYSTEM_ACTIVE, S5P_NORMAL_CFG);

	while (!TVPWR_TV_BLOCK_STATUS(readl(S5P_BLK_PWR_STAT)) && time_out) {
		msleep(1);
		time_out--;
	}

	if (time_out <= 0)
		pr_err("read S5P_BLK_PWR_STAT for TVPWR_TV_BLOCK_STATUS fail\n");

	TVPMPRINTK("S5P_NORMAL_CFG(0x%08x), S5P_BLK_PWR_STAT(0x%08x)\n",
		readl(S5P_NORMAL_CFG),
		readl(S5P_BLK_PWR_STAT));
}

void s5p_tv_power_off(void)
{
	int time_out = HDMI_TIME_OUT;

	s5p_tv_power_set_dac_onoff(false);

	writel(readl(S5P_NORMAL_CFG) & ~TVPWR_SUBSYSTEM_ACTIVE, S5P_NORMAL_CFG);

	while (TVPWR_TV_BLOCK_STATUS(readl(S5P_BLK_PWR_STAT)) && time_out) {
		msleep(1);
		time_out--;
	}

	if (time_out <= 0)
		pr_err("read S5P_BLK_PWR_STAT for TVPWR_TV_BLOCK_STATUS fail\n");

	TVPMPRINTK("0x%08x, 0x%08x)\n", readl(S5P_NORMAL_CFG),
		readl(S5P_BLK_PWR_STAT));
}
