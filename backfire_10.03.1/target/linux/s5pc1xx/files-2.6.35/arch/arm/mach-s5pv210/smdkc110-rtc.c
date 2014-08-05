/* linux/arch/arm/mach-s5pv210/smdkc110-rtc.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * SMDKC110/V210 Internal RTC Driver
*/

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/clk.h>
#include <linux/log2.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <asm/irq.h>
#include <asm/mach/time.h>
#include <plat/regs-rtc.h>

static struct resource *smdkc110_rtc_mem;

static void __iomem *smdkc110_rtc_base;
static int smdkc110_rtc_alarmno = NO_IRQ;

static void smdkc110_rtc_enable(struct device *dev, int en);
static int smdkc110_rtc_settime(struct device *dev, struct rtc_time *tm);

/* Update control registers */
static void smdkc110_rtc_setaie(int to)
{
	unsigned int tmp;

	pr_debug("%s: aie=%d\n", __func__, to);

	tmp = readb(smdkc110_rtc_base + S3C2410_RTCALM) & ~S3C2410_RTCALM_ALMEN;

	if (to)
		tmp |= S3C2410_RTCALM_ALMEN;

	writeb(tmp, smdkc110_rtc_base + S3C2410_RTCALM);
}

static int smdkc110_rtc_gettime(struct device *dev, struct rtc_time *rtc_tm)
{
	unsigned int have_retried = 0;
	void __iomem *base = smdkc110_rtc_base;

	smdkc110_rtc_enable(dev, 1);
 retry_get_time:
	rtc_tm->tm_sec  = readb(base + S3C2410_RTCSEC);
	rtc_tm->tm_min  = readb(base + S3C2410_RTCMIN);
	rtc_tm->tm_hour = readb(base + S3C2410_RTCHOUR);
	rtc_tm->tm_mday = readb(base + S3C2410_RTCDATE);
	rtc_tm->tm_mon  = readb(base + S3C2410_RTCMON);
	rtc_tm->tm_year = readl(base + S3C2410_RTCYEAR);

	/* the only way to work out wether the system was mid-update
	 * when we read it is to check the second counter, and if it
	 * is zero, then we re-try the entire read
	 */

	if (rtc_tm->tm_sec == 0 && !have_retried) {
		have_retried = 1;
		goto retry_get_time;
	}
	smdkc110_rtc_enable(dev, 0);

	pr_debug("read time %02x.%02x.%02x %02x/%02x/%02x\n",
		 rtc_tm->tm_year, rtc_tm->tm_mon, rtc_tm->tm_mday,
		 rtc_tm->tm_hour, rtc_tm->tm_min, rtc_tm->tm_sec);

	rtc_tm->tm_sec = bcd2bin(rtc_tm->tm_sec);
	rtc_tm->tm_min = bcd2bin(rtc_tm->tm_min);
	rtc_tm->tm_hour = bcd2bin(rtc_tm->tm_hour);
	rtc_tm->tm_mday = bcd2bin(rtc_tm->tm_mday);
	rtc_tm->tm_mon = bcd2bin(rtc_tm->tm_mon);
	rtc_tm->tm_year = bcd2bin(rtc_tm->tm_year & 0xff) +
			  bcd2bin(rtc_tm->tm_year >> 8) * 100;

	rtc_tm->tm_mon -= 1;

	return 0;
}

static int smdkc110_rtc_settime(struct device *dev, struct rtc_time *tm)
{
	void __iomem *base = smdkc110_rtc_base;
	int year = tm->tm_year;
	int year100;

	pr_debug("set time %02d.%02d.%02d %02d/%02d/%02d\n",
		 tm->tm_year, tm->tm_mon, tm->tm_mday,
		 tm->tm_hour, tm->tm_min, tm->tm_sec);

	/* we get around y2k by simply not supporting it */
	if (year < 0 || year >= 1000) {
		dev_err(dev, "rtc only supports 0~999 years\n");
		return -EINVAL;
	}

	smdkc110_rtc_enable(dev, 1);

	writeb(bin2bcd(tm->tm_sec),  base + S3C2410_RTCSEC);
	writeb(bin2bcd(tm->tm_min),  base + S3C2410_RTCMIN);
	writeb(bin2bcd(tm->tm_hour), base + S3C2410_RTCHOUR);
	writeb(bin2bcd(tm->tm_mday), base + S3C2410_RTCDATE);
	writeb(bin2bcd(tm->tm_mon + 1), base + S3C2410_RTCMON);

	year100 = year/100;
	year = year%100;
	year = bin2bcd(year) | ((bin2bcd(year100)) << 8);
	year = (0x00000fff & year);
	pr_debug("year %x\n", year);
	writel(year, base + S3C2410_RTCYEAR);

	smdkc110_rtc_enable(dev, 0);

	return 0;
}

static int smdkc110_rtc_getalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct rtc_time *alm_tm = &alrm->time;
	void __iomem *base = smdkc110_rtc_base;
	unsigned int alm_en;

	smdkc110_rtc_enable(dev, 1);

	alm_tm->tm_sec  = readb(base + S3C2410_ALMSEC);
	alm_tm->tm_min  = readb(base + S3C2410_ALMMIN);
	alm_tm->tm_hour = readb(base + S3C2410_ALMHOUR);
	alm_tm->tm_mon  = readb(base + S3C2410_ALMMON);
	alm_tm->tm_mday = readb(base + S3C2410_ALMDATE);
	alm_tm->tm_year = readl(base + S3C2410_ALMYEAR);
	alm_tm->tm_year = (0x00000fff & alm_tm->tm_year);

	smdkc110_rtc_enable(dev, 0);

	alm_en = readb(base + S3C2410_RTCALM);

	alrm->enabled = (alm_en & S3C2410_RTCALM_ALMEN) ? 1 : 0;

	pr_debug("read alarm %02x %02x.%02x.%02x %02x/%02x/%02x\n",
		 alm_en,
		 alm_tm->tm_year, alm_tm->tm_mon, alm_tm->tm_mday,
		 alm_tm->tm_hour, alm_tm->tm_min, alm_tm->tm_sec);

	/* decode the alarm enable field */

	if (alm_en & S3C2410_RTCALM_SECEN)
		alm_tm->tm_sec = bcd2bin(alm_tm->tm_sec);
	else
		alm_tm->tm_sec = 0xff;

	if (alm_en & S3C2410_RTCALM_MINEN)
		alm_tm->tm_min = bcd2bin(alm_tm->tm_min);
	else
		alm_tm->tm_min = 0xff;

	if (alm_en & S3C2410_RTCALM_HOUREN)
		alm_tm->tm_hour = bcd2bin(alm_tm->tm_hour);
	else
		alm_tm->tm_hour = 0xff;

	if (alm_en & S3C2410_RTCALM_DAYEN)
		alm_tm->tm_mday = bcd2bin(alm_tm->tm_mday);
	else
		alm_tm->tm_mday = 0xff;

	if (alm_en & S3C2410_RTCALM_MONEN) {
		alm_tm->tm_mon = bcd2bin(alm_tm->tm_mon);
		alm_tm->tm_mon -= 1;
	} else {
		alm_tm->tm_mon = 0xff;
	}

	if (alm_en & S3C2410_RTCALM_YEAREN) {
		alm_tm->tm_year = bcd2bin(alm_tm->tm_year & 0xff) +
			  bcd2bin(alm_tm->tm_year >> 8) * 100;
	} else {
		alm_tm->tm_year = 0xffff;
	}

	return 0;
}

static int smdkc110_rtc_setalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct rtc_time *tm = &alrm->time;
	void __iomem *base = smdkc110_rtc_base;
	unsigned int alrm_en;

	int year = tm->tm_year;
	int year100;

	pr_debug("s3c_rtc_setalarm: %d, %02x/%02x/%02x %02x.%02x.%02x\n",
		 alrm->enabled,
		 tm->tm_mday & 0xff, tm->tm_mon & 0xff, tm->tm_year & 0xff,
		 tm->tm_hour & 0xff, tm->tm_min & 0xff, tm->tm_sec);

	smdkc110_rtc_enable(dev, 1);

	alrm_en = readb(base + S3C2410_RTCALM) & S3C2410_RTCALM_ALMEN;
	writeb(0x00, base + S3C2410_RTCALM);

	if (tm->tm_sec < 60 && tm->tm_sec >= 0) {
		alrm_en |= S3C2410_RTCALM_SECEN;
		writeb(bin2bcd(tm->tm_sec), base + S3C2410_ALMSEC);
	}

	if (tm->tm_min < 60 && tm->tm_min >= 0) {
		alrm_en |= S3C2410_RTCALM_MINEN;
		writeb(bin2bcd(tm->tm_min), base + S3C2410_ALMMIN);
	}

	if (tm->tm_hour < 24 && tm->tm_hour >= 0) {
		alrm_en |= S3C2410_RTCALM_HOUREN;
		writeb(bin2bcd(tm->tm_hour), base + S3C2410_ALMHOUR);
	}

	if (tm->tm_mday >= 0) {
		alrm_en |= S3C2410_RTCALM_DAYEN;
		writeb(bin2bcd(tm->tm_mday), base + S3C2410_ALMDATE);
	}

	if (tm->tm_mon < 13 && tm->tm_mon >= 0) {
		alrm_en |= S3C2410_RTCALM_MONEN;
		writeb(bin2bcd(tm->tm_mon + 1), base + S3C2410_ALMMON);
	}

	if (year < 1000 && year >= 0) {
		alrm_en |= S3C2410_RTCALM_YEAREN;
		year100 = year/100;
		year = year%100;
		year = bin2bcd(year) | ((bin2bcd(year100)) << 8);
		year = (0x00000fff & year);
		pr_debug("year %x\n", year);
		writel(year, base + S3C2410_ALMYEAR);
	}

	smdkc110_rtc_enable(dev, 0);

	pr_debug("setting S3C2410_RTCALM to %08x\n", alrm_en);

	writeb(alrm_en, base + S3C2410_RTCALM);

	smdkc110_rtc_setaie(alrm->enabled);

	return 0;
}

static const struct rtc_class_ops smdkc110_rtcops = {
	.read_time	= smdkc110_rtc_gettime,
	.set_time	= smdkc110_rtc_settime,
	.read_alarm	= smdkc110_rtc_getalarm,
	.set_alarm	= smdkc110_rtc_setalarm,
};

static void smdkc110_rtc_enable(struct device *dev, int en)
{
	void __iomem *base = smdkc110_rtc_base;
	unsigned int tmp;

	if (smdkc110_rtc_base == NULL)
		return;

	if (!en) {
		tmp = readw(base + S3C2410_RTCCON);
		writew(tmp & ~(S3C2410_RTCCON_RTCEN), base + S3C2410_RTCCON);
	} else {
		/* re-enable the device, and check it is ok */

		if ((readw(base+S3C2410_RTCCON) & S3C2410_RTCCON_RTCEN) == 0) {
			dev_info(dev, "rtc disabled, re-enabling\n");

			tmp = readw(base + S3C2410_RTCCON);
			writew(tmp|S3C2410_RTCCON_RTCEN, base+S3C2410_RTCCON);
		}

		if ((readw(base + S3C2410_RTCCON) & S3C2410_RTCCON_CNTSEL)) {
			dev_info(dev, "removing RTCCON_CNTSEL\n");

			tmp = readw(base + S3C2410_RTCCON);
			writew(tmp & ~S3C2410_RTCCON_CNTSEL,
				 base+S3C2410_RTCCON);
		}

		if ((readw(base + S3C2410_RTCCON) & S3C2410_RTCCON_CLKRST)) {
			dev_info(dev, "removing RTCCON_CLKRST\n");

			tmp = readw(base + S3C2410_RTCCON);
			writew(tmp & ~S3C2410_RTCCON_CLKRST,
				     base+S3C2410_RTCCON);
		}
	}
}

static int __devexit smdkc110_rtc_remove(struct platform_device *pdev)
{
	struct rtc_device *rtc = platform_get_drvdata(pdev);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	platform_set_drvdata(pdev, NULL);
	rtc_device_unregister(rtc);

	smdkc110_rtc_setaie(0);

	iounmap(smdkc110_rtc_base);
	release_mem_region(res->start, res->end - res->start + 1);

	return 0;
}

static int __devinit smdkc110_rtc_probe(struct platform_device *pdev)
{
	struct rtc_device *rtc;
	struct resource *res;
	int ret;
	struct rtc_time tm;

	pr_debug("%s: probe=%p\n", __func__, pdev);

	/* find the IRQs */
	smdkc110_rtc_alarmno = platform_get_irq(pdev, 0);
	if (smdkc110_rtc_alarmno < 0) {
		dev_err(&pdev->dev, "no irq for alarm\n");
		return -ENOENT;
	}

	pr_debug("s3c2410_rtc: alarm irq %d\n", smdkc110_rtc_alarmno);

	/* get the memory region */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get memory region resource\n");
		return -ENOENT;
	}

	smdkc110_rtc_mem = request_mem_region(res->start,
					res->end - res->start + 1,
					pdev->name);

	if (smdkc110_rtc_mem == NULL) {
		dev_err(&pdev->dev, "failed to reserve memory region\n");
		ret = -ENOENT;
		goto err_nores;
	}

	smdkc110_rtc_base = ioremap(res->start, res->end - res->start + 1);
	if (smdkc110_rtc_base == NULL) {
		dev_err(&pdev->dev, "failed ioremap()\n");
		ret = -EINVAL;
		goto err_nomap;
	}

	pr_debug("s3c2410_rtc: RTCCON=%02x\n",
		readw(smdkc110_rtc_base + S3C2410_RTCCON));

	device_init_wakeup(&pdev->dev, 1);

#ifdef CONFIG_MACH_MINI210
	smdkc110_rtc_gettime(&pdev->dev, &tm);
	if (tm.tm_mon > 12 || tm.tm_mday > 31 || tm.tm_year > 3840) {
#endif
		/* Set the default time. 2012:1:1:12:0:0 */
		tm.tm_year = 112;
		tm.tm_mon = 0;
		tm.tm_mday = 1;
		tm.tm_hour = 12;
		tm.tm_min = 0;
		tm.tm_sec = 0;

		smdkc110_rtc_settime(&pdev->dev, &tm);
#ifdef CONFIG_MACH_MINI210
	}
#endif

	/* register RTC and exit */
	rtc = rtc_device_register("s3c", &pdev->dev, &smdkc110_rtcops,
				  THIS_MODULE);

	if (IS_ERR(rtc)) {
		dev_err(&pdev->dev, "cannot attach rtc\n");
		ret = PTR_ERR(rtc);
		goto err_nortc;
	}

	platform_set_drvdata(pdev, rtc);

	return 0;

 err_nortc:
	iounmap(smdkc110_rtc_base);

 err_nomap:
	release_mem_region(res->start, res->end - res->start + 1);

 err_nores:
	return ret;
}

#ifdef CONFIG_PM

/* RTC Power management control */
static int ticnt_save;

static int smdkc110_rtc_suspend(struct platform_device *pdev, pm_message_t state)
{
	/* save TICNT for anyone using periodic interrupts */
	ticnt_save = readl(smdkc110_rtc_base + S3C2410_TICNT);

	if (device_may_wakeup(&pdev->dev))
		enable_irq_wake(smdkc110_rtc_alarmno);

	return 0;
}

static int smdkc110_rtc_resume(struct platform_device *pdev)
{
	writel(ticnt_save, smdkc110_rtc_base + S3C2410_TICNT);

	if (device_may_wakeup(&pdev->dev))
		disable_irq_wake(smdkc110_rtc_alarmno);

	return 0;
}
#else
#define smdkc110_rtc_suspend NULL
#define smdkc110_rtc_resume  NULL
#endif

static struct platform_driver smdkc110_rtc_driver = {
	.probe		= smdkc110_rtc_probe,
	.remove		= __devexit_p(smdkc110_rtc_remove),
	.suspend	= smdkc110_rtc_suspend,
	.resume		= smdkc110_rtc_resume,
	.driver		= {
		.name	= "smdkc110-rtc",
		.owner	= THIS_MODULE,
	},
};

static char __initdata banner[] = "SMDKC110/V210 RTC, (c) 2010 "
				  "Samsung Electronics\n";

static int __init smdkc110_rtc_init(void)
{
	printk(banner);
	return platform_driver_register(&smdkc110_rtc_driver);
}

static void __exit smdkc110_rtc_exit(void)
{
	platform_driver_unregister(&smdkc110_rtc_driver);
}

module_init(smdkc110_rtc_init);
module_exit(smdkc110_rtc_exit);

MODULE_DESCRIPTION("Samsung SMDKC110/V210 RTC Driver");
MODULE_AUTHOR("samsung");
MODULE_LICENSE("GPL");
