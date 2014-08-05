/* linux/drivers/media/video/samsung/tv20/cec.c
 *
 * cec interface file for Samsung TVOut driver (only s5pv210)
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
#include <linux/io.h>
#include <linux/uaccess.h>

#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>

#include "cec.h"

#ifdef CECDEBUG
#define CECIFPRINTK(fmt, args...) \
	pr_debug("\t[CEC_IF] %s: " fmt, __func__ , ## args)
#else
#define CECIFPRINTK(fmt, args...)
#endif

static struct cec_rx_struct g_cec_rx_struct;
static struct cec_tx_struct g_cec_tx_struct;
static bool                 g_hdmi_on;

/**
 * Change CEC Tx state to state
 * @param state [in] new CEC Tx state.
 */
void s5p_cec_set_tx_state(enum cec_state state)
{
	atomic_set(&g_cec_tx_struct.state, state);
}

/**
 * Change CEC Rx state to @c state.
 * @param state [in] new CEC Rx state.
 */
void s5p_cec_set_rx_state(enum cec_state state)
{
	atomic_set(&g_cec_rx_struct.state, state);
}

static int s5p_cec_open(struct inode *inode, struct file *file)
{
	g_hdmi_on = true;

	s5p_cec_reset();

	s5p_cec_set_divider();

	s5p_cec_threshold();

	s5p_cec_unmask_tx_interrupts();

	s5p_cec_set_rx_state(STATE_RX);
	s5p_cec_unmask_rx_interrupts();
	s5p_cec_enable_rx();

	return 0;
}

static int s5p_cec_release(struct inode *inode, struct file *file)
{
	g_hdmi_on = false;

	s5p_cec_mask_tx_interrupts();
	s5p_cec_mask_rx_interrupts();

	return 0;
}

static ssize_t s5p_cec_read(struct file *file, char __user *buffer,
			    size_t count, loff_t *ppos)
{
	ssize_t retval;

	if (wait_event_interruptible(g_cec_rx_struct.waitq,
		atomic_read(&g_cec_rx_struct.state)
		== STATE_DONE)) {
		return -ERESTARTSYS;
	}

	spin_lock_irq(&g_cec_rx_struct.lock);

	if (g_cec_rx_struct.size > count) {
		spin_unlock_irq(&g_cec_rx_struct.lock);
		return -1;
	}

	if (copy_to_user(buffer, g_cec_rx_struct.buffer,
			 g_cec_rx_struct.size)) {
		spin_unlock_irq(&g_cec_rx_struct.lock);
		pr_err("%s::copy_to_user() failed!\n", __func__);
		return -EFAULT;
	}

	retval = g_cec_rx_struct.size;

	s5p_cec_set_rx_state(STATE_RX);
	spin_unlock_irq(&g_cec_rx_struct.lock);

	return retval;
}

static ssize_t s5p_cec_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *ppos)
{
	char *data;

	/* check data size */
	if (count > CEC_TX_BUFF_SIZE || count == 0)
		return -EFAULT;

	data = kmalloc(count, GFP_KERNEL);
	if (!data) {
		pr_err("%s::kmalloc() failed!\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(data, buffer, count)) {
		pr_err("%s::copy_from_user() failed!\n", __func__);
		kfree(data);
		return -EFAULT;
	}

	s5p_cec_copy_packet(data, count);

	kfree(data);

	/* wait for interrupt */
	if (wait_event_interruptible(g_cec_tx_struct.waitq,
		atomic_read(&g_cec_tx_struct.state) != STATE_TX)) {
		return -ERESTARTSYS;
	}

	if (atomic_read(&g_cec_tx_struct.state) == STATE_ERROR)
		return -EFAULT;

	return count;
}

static int s5p_cec_ioctl(struct inode *inode, struct file *file, u32 cmd,
	unsigned long arg)
{
	u32 laddr;
	int ret = 0;

	switch (cmd) {
	case CEC_IOC_SETLADDR:
		CECIFPRINTK("ioctl(CEC_IOC_SETLADDR)\n");

		if (get_user(laddr, (u32 __user *) arg))
			return -EFAULT;

		CECIFPRINTK("logical address = 0x%02x\n", laddr);

		s5p_cec_set_addr(laddr);
		break;
	default:
		ret = EINVAL;
		break;
	}

	return ret;
}

static u32 s5p_cec_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &g_cec_rx_struct.waitq, wait);

	if (atomic_read(&g_cec_rx_struct.state) == STATE_DONE)
		return POLLIN | POLLRDNORM;

	return 0;
}

static const struct file_operations cec_fops = {
	.owner   = THIS_MODULE,
	.open    = s5p_cec_open,
	.release = s5p_cec_release,
	.read    = s5p_cec_read,
	.write   = s5p_cec_write,
	.ioctl   = s5p_cec_ioctl,
	.poll    = s5p_cec_poll,
};

static struct miscdevice cec_misc_device = {
	.minor = CEC_MINOR,
	.name  = "CEC",
	.fops  = &cec_fops,
};

/**
 * @brief CEC interrupt handler
 *
 * Handles interrupt requests from CEC hardware. \n
 * Action depends on current state of CEC hardware.
 */
static irqreturn_t s5p_cec_irq_handler(int irq, void *dev_id)
{
	u32 status = 0;

	status = s5p_cec_get_status();

	if (status & CEC_STATUS_TX_DONE) {
		if (status & CEC_STATUS_TX_ERROR) {
			CECIFPRINTK(" CEC_STATUS_TX_ERROR!\n");
			s5p_cec_set_tx_state(STATE_ERROR);
		} else {
			CECIFPRINTK(" CEC_STATUS_TX_DONE!\n");
			s5p_cec_set_tx_state(STATE_DONE);
		}

		s5p_clr_pending_tx();

		wake_up_interruptible(&g_cec_tx_struct.waitq);
	}

	if (status & CEC_STATUS_RX_DONE) {
		if (status & CEC_STATUS_RX_ERROR) {
			CECIFPRINTK(" CEC_STATUS_RX_ERROR!\n");
			s5p_cec_rx_reset();

		} else {
			u32 size;

			CECIFPRINTK(" CEC_STATUS_RX_DONE!\n");

			/* copy data from internal buffer */
			size = status >> 24;

			spin_lock(&g_cec_rx_struct.lock);

			s5p_cec_get_rx_buf(size, g_cec_rx_struct.buffer);

			g_cec_rx_struct.size = size;

			s5p_cec_set_rx_state(STATE_DONE);

			spin_unlock(&g_cec_rx_struct.lock);

			s5p_cec_enable_rx();
		}

		/* clear interrupt pending bit */
		s5p_clr_pending_rx();

		wake_up_interruptible(&g_cec_rx_struct.waitq);
	}

	return IRQ_HANDLED;
}

static int __init s5p_cec_probe(struct platform_device *pdev)
{
	u8 *buffer;
	int irq_num;
	int ret = 0;

	s3c_gpio_cfgpin(S5PV210_GPH1(4), S3C_GPIO_SFN(0x4));
	s3c_gpio_setpull(S5PV210_GPH1(4), S3C_GPIO_PULL_NONE);

	/* get ioremap addr */
	ret = s5p_cec_probe_core(pdev);
	if (ret != 0) {
		pr_err("%s::s5p_cec_probe_core() fail\n", __func__);
		goto err_s5p_cec_probe_core;
	}

	if (misc_register(&cec_misc_device)) {
		pr_err("%s::Couldn't register device 10, %d.\n",
			__func__, CEC_MINOR);
		ret =  -EBUSY;
		goto err_misc_reg;
	}

	irq_num = platform_get_irq(pdev, 0);
	if (irq_num < 0) {
		pr_err("%s::failed to get %s irq resource\n",
			__func__, "cec");
		ret = -ENOENT;
		goto err_req_fw;
	}

	ret = request_irq(irq_num, s5p_cec_irq_handler, IRQF_DISABLED,
		pdev->name, &pdev->id);
	if (ret != 0) {
		pr_err("%s::failed to install %s irq (%d)\n",
			__func__, "cec", ret);
		goto err_req_fw;
	}

	init_waitqueue_head(&g_cec_rx_struct.waitq);
	spin_lock_init(&g_cec_rx_struct.lock);
	init_waitqueue_head(&g_cec_tx_struct.waitq);

	buffer = kmalloc(CEC_TX_BUFF_SIZE, GFP_KERNEL);
	if (!buffer) {
		pr_err("%s::kmalloc(CEC_TX_BUFF_SIZE %d) failed!\n",
			__func__, CEC_TX_BUFF_SIZE);

		ret = -EIO;
		goto err_kmalloc;
	}

	g_cec_rx_struct.buffer = buffer;
	g_cec_rx_struct.size   = 0;

	return 0;

err_kmalloc:
	free_irq(irq_num, NULL);
err_req_fw:
	misc_deregister(&cec_misc_device);
err_misc_reg:
	s5p_cec_release_core(pdev);
err_s5p_cec_probe_core:
	return ret;
}

static int s5p_cec_remove(struct platform_device *pdev)
{
	int irq_num;

	irq_num = platform_get_irq(pdev, 0);
	if (0 <= irq_num)
		free_irq(irq_num, NULL);

	misc_deregister(&cec_misc_device);

	s5p_cec_release_core(pdev);
	return 0;
}

#ifdef CONFIG_PM
static int s5p_cec_suspend(struct platform_device *dev, pm_message_t state)
{
	if (g_hdmi_on)
		s5p_tv_base_clk_gate(false);

	return 0;
}

static int s5p_cec_resume(struct platform_device *dev)
{
	if (g_hdmi_on)
		s5p_tv_base_clk_gate(true);

	return 0;
}
#else
#define s5p_cec_suspend NULL
#define s5p_cec_resume NULL
#endif

static struct platform_driver s5p_cec_driver = {
	.probe		= s5p_cec_probe,
	.remove		= __devexit_p(s5p_cec_remove),
	.suspend	= s5p_cec_suspend,
	.resume		= s5p_cec_resume,
	.driver		= {
		.name	= "s5p-cec",
		.owner	= THIS_MODULE,
	},
};

static char banner[] __initdata =
	"S5PC11X CEC Driver, (c) 2009 Samsung Electronics\n";

int __init s5p_cec_init(void)
{
	int ret;

	pr_info("%s", banner);

	ret = platform_driver_register(&s5p_cec_driver);
	if (ret) {
		pr_err("%s::Platform Device Register Failed %d\n",
			__func__, ret);
		return -1;
	}

	return 0;
}

static void __exit s5p_cec_exit(void)
{
	kfree(g_cec_rx_struct.buffer);

	platform_driver_unregister(&s5p_cec_driver);
}

module_init(s5p_cec_init);
module_exit(s5p_cec_exit);

MODULE_AUTHOR("SangPil Moon");
MODULE_DESCRIPTION("SS5PC11X CEC driver");
MODULE_LICENSE("GPL");
