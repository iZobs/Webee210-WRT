/* linux/drivers/media/video/samsung/tv20/hpd.c
 *
 * hpd interface ftn file for Samsung TVOut driver
 *
 * Copyright (c) 2010 Samsung Electronics
 *	         http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <linux/kobject.h>
#include <linux/io.h>

#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>

#include "hpd.h"

#ifdef S5P_HPD_DEBUG
#define HPDIFPRINTK(fmt, args...) \
	pr_debug("[HPD_IF] %s: " fmt, __func__ , ## args)
#else
#define HPDIFPRINTK(fmt, args...)
#endif

static struct hpd_struct   g_hpd_struct;
static int                 g_last_hpd_state;
static atomic_t            g_hdmi_status;
static atomic_t            g_poll_state;

static DECLARE_WORK(g_hpd_work, (void *)s5p_tv_base_handle_cable);

int s5p_hpd_get_state(void)
{
       return atomic_read(&g_hpd_struct.state);
}

int s5p_hpd_set_hdmiint(void)
{
	/* EINT -> HDMI */

	set_irq_type(IRQ_EINT13, IRQ_TYPE_NONE);

	if (g_last_hpd_state)
		s5p_hdmi_disable_interrupts(HDMI_IRQ_HPD_UNPLUG);
	else
		s5p_hdmi_disable_interrupts(HDMI_IRQ_HPD_PLUG);

	atomic_set(&g_hdmi_status, HDMI_ON);

	s3c_gpio_cfgpin(S5PV210_GPH1(5), S3C_GPIO_SFN(0x4));
	s3c_gpio_setpull(S5PV210_GPH1(5), S3C_GPIO_PULL_DOWN);
	writel(readl(S5PV210_GPH1DRV)|0x3<<10, S5PV210_GPH1DRV);

	s5p_hdmi_hpd_gen();

	if (s5p_hdmi_get_hpd_status())
		s5p_hdmi_enable_interrupts(HDMI_IRQ_HPD_UNPLUG);
	else
		s5p_hdmi_enable_interrupts(HDMI_IRQ_HPD_PLUG);

	return 0;
}

int s5p_hpd_set_eint(void)
{
	/* HDMI -> EINT */
	atomic_set(&g_hdmi_status, HDMI_OFF);

	s5p_hdmi_clear_pending(HDMI_IRQ_HPD_PLUG);
	s5p_hdmi_clear_pending(HDMI_IRQ_HPD_UNPLUG);

	s5p_hdmi_disable_interrupts(HDMI_IRQ_HPD_PLUG);
	s5p_hdmi_disable_interrupts(HDMI_IRQ_HPD_UNPLUG);

	s3c_gpio_cfgpin(S5PV210_GPH1(5), S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(S5PV210_GPH1(5), S3C_GPIO_PULL_DOWN);
	writel(readl(S5PV210_GPH1DRV)|0x3<<10, S5PV210_GPH1DRV);

	return 0;
}

static int s5p_hpd_open(struct inode *inode, struct file *file)
{
	atomic_set(&g_poll_state, 1);

	return 0;
}

static int s5p_hpd_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t s5p_hpd_read(struct file *file, char __user *buffer, size_t count,
	loff_t *ppos)
{
	ssize_t retval;

	spin_lock_irq(&g_hpd_struct.lock);

	retval = put_user(atomic_read(&g_hpd_struct.state),
		(unsigned int __user *) buffer);

	atomic_set(&g_poll_state, -1);

	spin_unlock_irq(&g_hpd_struct.lock);

	return retval;
}

static unsigned int s5p_hpd_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &g_hpd_struct.waitq, wait);

	if (atomic_read(&g_poll_state) != -1)
		return POLLIN | POLLRDNORM;

	return 0;
}

static const struct file_operations hpd_fops = {
	.owner   = THIS_MODULE,
	.open    = s5p_hpd_open,
	.release = s5p_hpd_release,
	.read    = s5p_hpd_read,
	.poll    = s5p_hpd_poll,
};

static struct miscdevice hpd_misc_device = {
	HPD_MINOR,
	"HPD",
	&hpd_fops,
};

static int irq_eint(int irq)
{
	if (gpio_get_value(S5PV210_GPH1(5))) {
		if (g_last_hpd_state == HPD_HI)
			return IRQ_HANDLED;
		atomic_set(&g_hpd_struct.state, HPD_HI);
		atomic_set(&g_poll_state, 1);

		g_last_hpd_state = HPD_HI;
		wake_up_interruptible(&g_hpd_struct.waitq);
	} else {
		if (g_last_hpd_state == HPD_LO)
			return IRQ_HANDLED;
		atomic_set(&g_hpd_struct.state, HPD_LO);
		atomic_set(&g_poll_state, 1);

		g_last_hpd_state = HPD_LO;
		wake_up_interruptible(&g_hpd_struct.waitq);
	}

	schedule_work(&g_hpd_work);

	HPDIFPRINTK("%s\n", atomic_read(&g_hpd_struct.state) == HPD_HI ?
		"HPD HI" : "HPD LO");

	return IRQ_HANDLED;
}

static int irq_hdmi(int irq)
{
	u8 flag;
	int ret = IRQ_HANDLED;

	/* read flag register */
	flag = s5p_hdmi_get_interrupts();

	if (s5p_hdmi_get_hpd_status())
		s5p_hdmi_clear_pending(HDMI_IRQ_HPD_PLUG);

	else
		s5p_hdmi_clear_pending(HDMI_IRQ_HPD_UNPLUG);

	s5p_hdmi_disable_interrupts(HDMI_IRQ_HPD_UNPLUG);
	s5p_hdmi_disable_interrupts(HDMI_IRQ_HPD_PLUG);

	/* is this our interrupt? */
	if (!(flag & (1 << HDMI_IRQ_HPD_PLUG | 1 << HDMI_IRQ_HPD_UNPLUG))) {
		ret = IRQ_NONE;
		goto out;
	}

	if (flag == (1 << HDMI_IRQ_HPD_PLUG | 1 << HDMI_IRQ_HPD_UNPLUG)) {
		HPDIFPRINTK("HPD_HI && HPD_LO\n");

		if (g_last_hpd_state == HPD_HI && s5p_hdmi_get_hpd_status())
			flag = 1 << HDMI_IRQ_HPD_UNPLUG;
		else
			flag = 1 << HDMI_IRQ_HPD_PLUG;
	}

	if (flag & (1 << HDMI_IRQ_HPD_PLUG)) {
		s5p_hdmi_enable_interrupts(HDMI_IRQ_HPD_UNPLUG);

		atomic_set(&g_hpd_struct.state, HPD_HI);
		atomic_set(&g_poll_state, 1);

		g_last_hpd_state = HPD_HI;
		wake_up_interruptible(&g_hpd_struct.waitq);

		s5p_hdcp_encrypt_stop(true);

		HPDIFPRINTK("HPD_HI\n");
	} else if (flag & (1 << HDMI_IRQ_HPD_UNPLUG)) {
		s5p_hdcp_encrypt_stop(false);

		s5p_hdmi_enable_interrupts(HDMI_IRQ_HPD_PLUG);

		atomic_set(&g_hpd_struct.state, HPD_LO);
		atomic_set(&g_poll_state, 1);

		g_last_hpd_state = HPD_LO;
		wake_up_interruptible(&g_hpd_struct.waitq);

		HPDIFPRINTK("HPD_LO\n");
	}

	schedule_work(&g_hpd_work);

out:
	return IRQ_HANDLED;
}

/*
 * HPD interrupt handler
 *
 * Handles interrupt requests from HPD hardware.
 * Handler changes value of internal variable and notifies waiting thread.
 */
irqreturn_t s5p_hpd_irq_handler(int irq)
{
	int ret = IRQ_HANDLED;

	spin_lock_irq(&g_hpd_struct.lock);

	/* check HDMI status */
	if (atomic_read(&g_hdmi_status)) {
		/* HDMI on */
		ret = irq_hdmi(irq);
		HPDIFPRINTK("HDMI HPD interrupt\n");
	} else {
		/* HDMI off */
		ret = irq_eint(irq);
		HPDIFPRINTK("EINT HPD interrupt\n");
	}

	spin_unlock_irq(&g_hpd_struct.lock);

	return ret;
}

static int __init s5p_hpd_probe(struct platform_device *pdev)
{
	if (misc_register(&hpd_misc_device)) {
		pr_err("%s::Couldn't register device 10, %d.\n", __func__,
			HPD_MINOR);
		return -EBUSY;
	}

	init_waitqueue_head(&g_hpd_struct.waitq);

	spin_lock_init(&g_hpd_struct.lock);

	atomic_set(&g_hpd_struct.state, -1);

	atomic_set(&g_hdmi_status, HDMI_OFF);

	s3c_gpio_cfgpin(S5PV210_GPH1(5), S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(S5PV210_GPH1(5), S3C_GPIO_PULL_DOWN);
	writel(readl(S5PV210_GPH1DRV)|0x3<<10, S5PV210_GPH1DRV);

	if (gpio_get_value(S5PV210_GPH1(5))) {
		atomic_set(&g_hpd_struct.state, HPD_HI);
		g_last_hpd_state = HPD_HI;
	} else {
		atomic_set(&g_hpd_struct.state, HPD_LO);
		g_last_hpd_state = HPD_LO;
	}

	set_irq_type(IRQ_EINT13, IRQ_TYPE_EDGE_BOTH);

	if (request_irq(IRQ_EINT13, s5p_hpd_irq_handler, IRQF_DISABLED,
		"hpd", s5p_hpd_irq_handler)) {
		pr_err("%s::failed to install %s irq\n", __func__, "hpd");
		misc_deregister(&hpd_misc_device);
		return -EIO;
	}

	if (s5p_hdmi_register_isr(s5p_hpd_irq_handler, (u8)HDMI_IRQ_HPD_PLUG) != 0)
		pr_err("%s::s5p_hdmi_register_isr(HDMI_IRQ_HPD_PLUG) fail\n", __func__);

	if (s5p_hdmi_register_isr(s5p_hpd_irq_handler, (u8)HDMI_IRQ_HPD_UNPLUG) != 0)
		pr_err("%s::s5p_hdmi_register_isr(HDMI_IRQ_HPD_UNPLUG) fail\n", __func__);

	return 0;
}

static int s5p_hpd_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver s5p_hpd_driver = {
	.probe		= s5p_hpd_probe,
	.remove		= __devexit_p(s5p_hpd_remove),
	.driver		= {
		.name	= "s5p-hpd",
		.owner	= THIS_MODULE,
	},
};

static char banner[] __initdata =
	"S5PC11X HPD Driver, (c) 2009 Samsung Electronics\n";

int __init s5p_hpd_init(void)
{
	int ret;

	pr_info("%s", banner);

	ret = platform_driver_register(&s5p_hpd_driver);
	if (ret) {
		pr_err("%s::Platform Device Register Failed %d\n",
			__func__, ret);
		return -1;
	}

	return 0;
}

static void __exit s5p_hpd_exit(void)
{
	misc_deregister(&hpd_misc_device);
}

module_init(s5p_hpd_init);
module_exit(s5p_hpd_exit);
