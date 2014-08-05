/*
 * linux/drivers/char/mini210_pwm.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>

#include <mach/gpio.h>
#include <mach/regs-gpio.h>
#include <plat/gpio-cfg.h>


#define DEVICE_NAME				"pwm"

#define PWM_IOCTL_SET_FREQ		1
#define PWM_IOCTL_STOP			0

#define NS_IN_1HZ				(1000000000UL)


#define BUZZER_PWM_ID			0
#define BUZZER_PMW_GPIO			S5PV210_GPD0(0)

static struct pwm_device *pwm4buzzer;

static struct semaphore lock;


static void pwm_set_freq(unsigned long freq) {
	int period_ns = NS_IN_1HZ / freq;

	pwm_config(pwm4buzzer, period_ns / 2, period_ns);
	pwm_enable(pwm4buzzer);
}

static void pwm_stop(void) {
	pwm_config(pwm4buzzer, 0, NS_IN_1HZ / 100);
	pwm_disable(pwm4buzzer);
}


static int mini210_pwm_open(struct inode *inode, struct file *file) {
	if (!down_trylock(&lock))
		return 0;
	else
		return -EBUSY;
}

static int mini210_pwm_close(struct inode *inode, struct file *file) {
	up(&lock);
	return 0;
}

static long mini210_pwm_ioctl(struct file *filep, unsigned int cmd,
		unsigned long arg)
{
	switch (cmd) {
		case PWM_IOCTL_SET_FREQ:
			if (arg == 0)
				return -EINVAL;
			pwm_set_freq(arg);
			break;

		case PWM_IOCTL_STOP:
		default:
			pwm_stop();
			break;
	}

	return 0;
}


static struct file_operations mini210_pwm_ops = {
	.owner			= THIS_MODULE,
	.open			= mini210_pwm_open,
	.release		= mini210_pwm_close, 
	.unlocked_ioctl	= mini210_pwm_ioctl,
};

static struct miscdevice mini210_misc_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &mini210_pwm_ops,
};

static int __init mini210_pwm_dev_init(void) {
	int ret;

	ret = gpio_request(BUZZER_PMW_GPIO, DEVICE_NAME);
	if (ret) {
		printk("request GPIO %d for pwm failed\n", BUZZER_PMW_GPIO);
		return ret;
	}

	gpio_set_value(BUZZER_PMW_GPIO, 0);
	s3c_gpio_cfgpin(BUZZER_PMW_GPIO, S3C_GPIO_OUTPUT);

	pwm4buzzer = pwm_request(BUZZER_PWM_ID, DEVICE_NAME);
	if (IS_ERR(pwm4buzzer)) {
		printk("request pwm %d for %s failed\n", BUZZER_PWM_ID, DEVICE_NAME);
		return -ENODEV;
	}

	pwm_stop();

	s3c_gpio_cfgpin(BUZZER_PMW_GPIO, S3C_GPIO_SFN(2));
	gpio_free(BUZZER_PMW_GPIO);

	init_MUTEX(&lock);
	ret = misc_register(&mini210_misc_dev);

	printk(DEVICE_NAME "\tinitialized\n");

	return ret;
}

static void __exit mini210_pwm_dev_exit(void) {
	pwm_stop();

	misc_deregister(&mini210_misc_dev);
}

module_init(mini210_pwm_dev_init);
module_exit(mini210_pwm_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("FriendlyARM Inc.");
MODULE_DESCRIPTION("S5PV210 PWM Driver");

