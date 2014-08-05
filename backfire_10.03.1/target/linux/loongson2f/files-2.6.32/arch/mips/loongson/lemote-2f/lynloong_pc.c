/*
 *  Driver for LynLoong pc extras
 *
 *  Copyright (C) 2009 Lemote Inc.
 *  Author: Xiang Yu <xiangy@lemote.com>
 *          Wu Zhangjin <wuzj@lemote.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/thermal.h>

#include <asm/bootinfo.h>

#include <loongson.h>

#include <cs5536/cs5536.h>
#include <cs5536/cs5536_pci.h>
#include <cs5536/cs5536_mfgpt.h>

static u32 gpio_base, smb_base, mfgpt_base;

/* gpio operations */
static void set_gpio_reg_high(int gpio, int reg)
{
	u32 val;

	val = inl(gpio_base + reg);
	val |= (1 << gpio);
	val &= ~(1 << (16 + gpio));
	outl(val, gpio_base + reg);
	mmiowb();
}

static void set_gpio_reg_low(int gpio, int reg)
{
	u32 val;

	val = inl(gpio_base + reg);
	val |= (1 << (16 + gpio));
	val &= ~(1 << gpio);
	outl(val, gpio_base + reg);
	mmiowb();
}

static void set_gpio_output_low(int gpio)
{
	set_gpio_reg_high(gpio, GPIOL_OUT_EN);
	set_gpio_reg_low(gpio, GPIOL_OUT_VAL);
}

static void set_gpio_output_high(int gpio)
{
	set_gpio_reg_high(gpio, GPIOL_OUT_EN);
	set_gpio_reg_high(gpio, GPIOL_OUT_VAL);
}

/* backlight subdriver */

#define MAX_BRIGHTNESS 100
#define DEFAULT_BRIGHTNESS 50
#define MIN_BRIGHTNESS 0
static uint level;

DEFINE_SPINLOCK(backlight_lock);
/* tune the brightness */
static void setup_mfgpt2(void)
{
	unsigned long flags;

	spin_lock_irqsave(&backlight_lock, flags);
	/* set MFGPT2 comparator 1,2 */
	outw(MAX_BRIGHTNESS-level, MFGPT2_CMP1);
	outw(MAX_BRIGHTNESS, MFGPT2_CMP2);
	/* clear MFGPT2 UP COUNTER */
	outw(0, MFGPT2_CNT);
	/* enable counter, compare mode, 32k */
	outw(0x8280, MFGPT2_SETUP);
	spin_unlock_irqrestore(&backlight_lock, flags);
}

static int lynloong_set_brightness(struct backlight_device *bd)
{
	uint i;

	level = (bd->props.fb_blank == FB_BLANK_UNBLANK &&
		 bd->props.power == FB_BLANK_UNBLANK) ?
	    bd->props.brightness : 0;

	if (level > MAX_BRIGHTNESS)
		level = MAX_BRIGHTNESS;
	else if (level < MIN_BRIGHTNESS)
		level = MIN_BRIGHTNESS;

	if (level == 0) {
		/* turn off the backlight */
		set_gpio_output_low(11);
		for (i = 0; i < 0x500; i++)
			delay();
		/* turn off the LCD */
		set_gpio_output_high(8);
	} else {
		/* turn on the LCD */
		set_gpio_output_low(8);
		for (i = 0; i < 0x500; i++)
			delay();
		/* turn on the backlight */
		set_gpio_output_high(11);
	}

	setup_mfgpt2();

	return 0;
}

static int lynloong_get_brightness(struct backlight_device *bd)
{
	return level;
}

static struct backlight_ops backlight_ops = {
	.get_brightness = lynloong_get_brightness,
	.update_status = lynloong_set_brightness,
};

static struct backlight_device *lynloong_backlight_dev;

static void lynloong_backlight_exit(void)
{
	if (lynloong_backlight_dev) {
		backlight_device_unregister(lynloong_backlight_dev);
		lynloong_backlight_dev = NULL;
	}
	/* disable brightness controlling */
	set_gpio_output_low(7);

	printk(KERN_INFO "exit from LingLoong Backlight Driver");
}

static int __init lynloong_backlight_init(struct device *dev)
{
	int ret;

	/* select for mfgpt */
	set_gpio_reg_high(7, GPIOL_OUT_AUX1_SEL);
	/* enable brightness controlling */
	set_gpio_output_high(7);

	lynloong_backlight_dev =
	    backlight_device_register("backlight0", dev, NULL,
				      &backlight_ops);

	if (IS_ERR(lynloong_backlight_dev)) {
		ret = PTR_ERR(lynloong_backlight_dev);
		return ret;
	}

	lynloong_backlight_dev->props.max_brightness = MAX_BRIGHTNESS;
	lynloong_backlight_dev->props.brightness = DEFAULT_BRIGHTNESS;
	backlight_update_status(lynloong_backlight_dev);

	return 0;
}

/* Thermal cooling devices subdriver */

static int video_get_max_state(struct thermal_cooling_device *cdev, unsigned
			       long *state)
{
	*state = MAX_BRIGHTNESS;
	return 0;
}

static int video_get_cur_state(struct thermal_cooling_device *cdev, unsigned
			       long *state)
{
	static struct backlight_device *bd;

	bd = (struct backlight_device *)cdev->devdata;

	*state = lynloong_get_brightness(bd);

	return 0;
}

static int video_set_cur_state(struct thermal_cooling_device *cdev, unsigned
			       long state)
{
	static struct backlight_device *bd;

	bd = (struct backlight_device *)cdev->devdata;

	lynloong_backlight_dev->props.brightness = state;
	backlight_update_status(bd);

	return 0;
}

static struct thermal_cooling_device_ops video_cooling_ops = {
	.get_max_state = video_get_max_state,
	.get_cur_state = video_get_cur_state,
	.set_cur_state = video_set_cur_state,
};

static struct thermal_cooling_device *lynloong_thermal_cdev;

/* TODO: register cpu as the cooling device */
static int lynloong_thermal_init(struct device *dev)
{
	int ret;

	if (!dev)
		return -1;

	lynloong_thermal_cdev = thermal_cooling_device_register("LCD", dev,
			&video_cooling_ops);

	if (IS_ERR(lynloong_thermal_cdev)) {
		ret = PTR_ERR(lynloong_thermal_cdev);
		return ret;
	}

	ret = sysfs_create_link(&dev->kobj,
				&lynloong_thermal_cdev->device.kobj,
				"thermal_cooling");
	if (ret) {
		printk(KERN_ERR "Create sysfs link\n");
		return ret;
	}
	ret = sysfs_create_link(&lynloong_thermal_cdev->device.kobj,
				&dev->kobj, "device");
	if (ret) {
		printk(KERN_ERR "Create sysfs link\n");
		return ret;
	}

	return 0;
}

static void lynloong_thermal_exit(struct device *dev)
{
	if (lynloong_thermal_cdev) {
		if (dev)
			sysfs_remove_link(&dev->kobj, "thermal_cooling");
		sysfs_remove_link(&lynloong_thermal_cdev->device.kobj,
				  "device");
		thermal_cooling_device_unregister(lynloong_thermal_cdev);
		lynloong_thermal_cdev = NULL;
	}
}

/* platform subdriver */

/* I2C operations */

static int i2c_wait(void)
{
	char c;
	int i;

	udelay(1000);
	for (i = 0; i < 20; i++) {
		c = inb(smb_base | SMB_STS);
		if (c & (SMB_STS_BER | SMB_STS_NEGACK))
			return -1;
		if (c & SMB_STS_SDAST)
			return 0;
		udelay(100);
	}
	return -2;
}

static void i2c_read_single(int addr, int regNo, char *value)
{
	unsigned char c;

	/* Start condition */
	c = inb(smb_base | SMB_CTRL1);
	outb(c | SMB_CTRL1_START, smb_base | SMB_CTRL1);
	i2c_wait();

	/* Send slave address */
	outb(addr & 0xfe, smb_base | SMB_SDA);
	i2c_wait();

	/* Acknowledge smbus */
	c = inb(smb_base | SMB_CTRL1);
	outb(c | SMB_CTRL1_ACK, smb_base | SMB_CTRL1);

	/* Send register index */
	outb(regNo, smb_base | SMB_SDA);
	i2c_wait();

	/* Acknowledge smbus */
	c = inb(smb_base | SMB_CTRL1);
	outb(c | SMB_CTRL1_ACK, smb_base | SMB_CTRL1);

	/* Start condition again */
	c = inb(smb_base | SMB_CTRL1);
	outb(c | SMB_CTRL1_START, smb_base | SMB_CTRL1);
	i2c_wait();

	/* Send salve address again */
	outb(1 | addr, smb_base | SMB_SDA);
	i2c_wait();

	/* Acknowledge smbus */
	c = inb(smb_base | SMB_CTRL1);
	outb(c | SMB_CTRL1_ACK, smb_base | SMB_CTRL1);

	/* Read data */
	*value = inb(smb_base | SMB_SDA);

	/* Stop condition */
	outb(SMB_CTRL1_STOP, smb_base | SMB_CTRL1);
	i2c_wait();
}

static void i2c_write_single(int addr, int regNo, char value)
{
	unsigned char c;

	/* Start condition */
	c = inb(smb_base | SMB_CTRL1);
	outb(c | SMB_CTRL1_START, smb_base | SMB_CTRL1);
	i2c_wait();
	/* Send slave address */
	outb(addr & 0xfe, smb_base | SMB_SDA);
	i2c_wait();;

	/* Send register index */
	outb(regNo, smb_base | SMB_SDA);
	i2c_wait();

	/* Write data */
	outb(value, smb_base | SMB_SDA);
	i2c_wait();
	/* Stop condition */
	outb(SMB_CTRL1_STOP, smb_base | SMB_CTRL1);
	i2c_wait();
}

static void stop_clock(int clk_reg, int clk_sel)
{
	u8 value;

	i2c_read_single(0xd3, clk_reg, &value);
	value &= ~(1 << clk_sel);
	i2c_write_single(0xd2, clk_reg, value);
}

static void enable_clock(int clk_reg, int clk_sel)
{
	u8 value;

	i2c_read_single(0xd3, clk_reg, &value);
	value |= (1 << clk_sel);
	i2c_write_single(0xd2, clk_reg, value);
}

static char cached_clk_freq;
static char cached_pci_fixed_freq;

static void decrease_clk_freq(void)
{
	char value;

	i2c_read_single(0xd3, 1, &value);
	cached_clk_freq = value;

	/* select frequency by software */
	value |= (1 << 1);
	/* CPU, 3V66, PCI : 100, 66, 33(1) */
	value |= (1 << 2);
	i2c_write_single(0xd2, 1, value);

	/* cache the pci frequency */
	i2c_read_single(0xd3, 14, &value);
	cached_pci_fixed_freq = value;

	/* enable PCI fix mode */
	value |= (1 << 5);
	/* 3V66, PCI : 64MHz, 32MHz */
	value |= (1 << 3);
	i2c_write_single(0xd2, 14, value);

}

static void resume_clk_freq(void)
{
	i2c_write_single(0xd2, 1, cached_clk_freq);
	i2c_write_single(0xd2, 14, cached_pci_fixed_freq);
}

static void stop_clocks(void)
{
	/* CPU Clock Register */
	stop_clock(2, 5);	/* not used */
	stop_clock(2, 6);	/* not used */
	stop_clock(2, 7);	/* not used */

	/* PCI Clock Register */
	stop_clock(3, 1);	/* 8100 */
	stop_clock(3, 5);	/* SIS */
	stop_clock(3, 0);	/* not used */
	stop_clock(3, 6);	/* not used */

	/* PCI 48M Clock Register */
	stop_clock(4, 6);	/* USB grounding */
	stop_clock(4, 5);	/* REF(5536_14M) */

	/* 3V66 Control Register */
	stop_clock(5, 0);	/* VCH_CLK..., grounding */
}
static void enable_clocks(void)
{
	enable_clock(3, 1);	/* 8100 */
	enable_clock(3, 5);	/* SIS */

	enable_clock(4, 6);
	enable_clock(4, 5);	/* REF(5536_14M) */

	enable_clock(5, 0);	/* VCH_CLOCK, grounding */
}

static struct platform_device *lynloong_pdev;

static int __maybe_unused lynloong_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	int i;

	printk(KERN_INFO "lynloong specific suspend\n");

	/* disable AMP */
	set_gpio_output_high(6);
	/* disable the brightness control */
	set_gpio_output_low(7);
	/* disable the backlight output */
	set_gpio_output_low(11);

	/* stop the clocks of some devices */
	stop_clocks();

	/* decrease the external clock frequency */
	decrease_clk_freq();

	/* turn off the LCD */
	for (i = 0; i < 0x600; i++)
		delay();
	set_gpio_output_high(8);

	return 0;
}

static int __maybe_unused lynloong_resume(struct platform_device *pdev)
{
	int i;

	printk(KERN_INFO "lynloong specific resume\n");

	/* turn on the LCD */
	set_gpio_output_low(8);
	for (i = 0; i < 0x1000; i++)
		delay();

	/* resume clock frequency, enable the relative clocks */
	resume_clk_freq();
	enable_clocks();

	/* enable the backlight output */
	set_gpio_output_high(11);
	/* enable the brightness control */
	set_gpio_output_high(7);
	/* enable AMP */
	set_gpio_output_low(6);

	return 0;
}

static struct platform_device *lynloong_pdev;

static struct platform_device_id platform_device_ids[] = {
	{
		.name = "lynloong_pc",
	},
	{}
};

MODULE_DEVICE_TABLE(platform, platform_device_ids);

static struct platform_driver platform_driver = {
	.driver = {
		   .name = "lynloong_pc",
		   .owner = THIS_MODULE,
		   },
	.id_table = platform_device_ids,
#ifdef CONFIG_PM
	.suspend = lynloong_suspend,
	.resume = lynloong_resume,
#endif
};

static int lynloong_pdev_init(void)
{
	int ret;

	/* Register platform stuff */
	ret = platform_driver_register(&platform_driver);
	if (ret)
		return ret;

	return 0;
}

static void lynloong_pdev_exit(void)
{
	platform_driver_unregister(&platform_driver);
}

static int __init lynloong_init(void)
{
	int ret;
	u32 hi;

	if (mips_machtype != MACH_LEMOTE_LL2F) {
		printk(KERN_INFO "This Driver is for LynLoong(Allinone) PC, You"
				" can not use it on the other Machines\n");
		return -EFAULT;
	}

	printk(KERN_INFO "Load LynLoong Platform Driver\n");

	/* get mfgpt_base */
	_rdmsr(DIVIL_MSR_REG(DIVIL_LBAR_MFGPT), &hi, &mfgpt_base);
	/* get gpio_base */
	_rdmsr(DIVIL_MSR_REG(DIVIL_LBAR_GPIO), &hi, &gpio_base);
	/* get smb base */
	_rdmsr(DIVIL_MSR_REG(DIVIL_LBAR_SMB), &hi, &smb_base);

	ret = lynloong_pdev_init();
	if (ret) {
		lynloong_pdev_exit();
		printk(KERN_INFO "init lynloong platform driver failure\n");
		return ret;
	}

	ret = lynloong_backlight_init(&lynloong_pdev->dev);
	if (ret) {
		lynloong_backlight_exit();
		printk(KERN_INFO "init lynloong backlight driver failure\n");
		return ret;
	}
	ret = lynloong_thermal_init(&lynloong_backlight_dev->dev);
	if (ret) {
		lynloong_thermal_exit(&lynloong_backlight_dev->dev);
		printk(KERN_INFO
		       "init lynloong thermal cooling device failure\n");
		return ret;
	}

	return 0;
}

static void __exit lynloong_exit(void)
{
	lynloong_pdev_exit();
	lynloong_thermal_exit(&lynloong_backlight_dev->dev);
	lynloong_backlight_exit();

	printk(KERN_INFO "Unload LynLoong Platform Driver\n");
}

module_init(lynloong_init);
module_exit(lynloong_exit);

MODULE_AUTHOR("Xiang Yu <xiangy@lemote.com>; Wu Zhangjin <wuzj@lemote.com>");
MODULE_DESCRIPTION("LynLoong Platform Specific Driver");
MODULE_LICENSE("GPL");
