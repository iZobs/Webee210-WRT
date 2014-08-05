/* linux/drivers/input/touchscreen/s3c-ts.c
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * a misc driver for mini6410 touch screen
 *  by FriendlyARM 2010
 *
 * Based on following software:
 *
 ** Copyright (c) 2004 Arnaud Patard <arnaud.patard@rtp-net.org>
 ** iPAQ H1940 touchscreen support
 **
 ** ChangeLog
 **
 ** 2004-09-05: Herbert Potzl <herbert@13thfloor.at>
 **	- added clock (de-)allocation code
 **
 ** 2005-03-06: Arnaud Patard <arnaud.patard@rtp-net.org>
 **      - h1940_ -> s3c24xx (this driver is now also used on the n30
 **        machines :P)
 **      - Debug messages are now enabled with the config option
 **        TOUCHSCREEN_S3C_DEBUG
 **      - Changed the way the value are read
 **      - Input subsystem should now work
 **      - Use ioremap and readl/writel
 **
 ** 2005-03-23: Arnaud Patard <arnaud.patard@rtp-net.org>
 **      - Make use of some undocumented features of the touchscreen
 **        controller
 **
 ** 2006-09-05: Ryu Euiyoul <ryu.real@gmail.com>
 **      - added power management suspend and resume code
 *
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <mach/hardware.h>

#include <plat/regs-adc.h>
#include <mach/irqs.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-gpio.h>
#include <mach/gpio-bank-a.h>
#include <mach/ts.h>

#define CONFIG_TOUCHSCREEN_S3C_DEBUG
#undef CONFIG_TOUCHSCREEN_S3C_DEBUG
#define DEBUG_LVL		KERN_DEBUG


#ifdef CONFIG_MINI6410_ADC
DECLARE_MUTEX(ADC_LOCK);

/* Indicate who is using the ADC controller */
#define LOCK_FREE		0
#define LOCK_TS			1
#define LOCK_ADC		2
static int adc_lock_id = LOCK_FREE;

#define	ADC_free()		(adc_lock_id == LOCK_FREE)
#define ADC_locked4TS()	(adc_lock_id == LOCK_TS)

static inline int s3c_ts_adc_lock(int id) {
	int ret;

	ret = down_trylock(&ADC_LOCK);
	if (!ret) {
		adc_lock_id = id;
	}

	return ret;
}

static inline void s3c_ts_adc_unlock(void) {
	adc_lock_id = 0;
	up(&ADC_LOCK);
}
#endif


/* Touchscreen default configuration */
struct s3c_ts_mach_info s3c_ts_default_cfg __initdata = {
	.delay				= 10000,
	.presc				= 49,
	.oversampling_shift = 2,
	.resol_bit			= 10
};

/*
 * Definitions & global arrays.
 */
#define DEVICE_NAME		"touchscreen"
static DECLARE_WAIT_QUEUE_HEAD(ts_waitq);

typedef unsigned		TS_EVENT;
#define NR_EVENTS		64

static TS_EVENT			events[NR_EVENTS];
static int				evt_head, evt_tail;

#define ts_evt_pending()	((volatile u8)(evt_head != evt_tail))
#define ts_evt_get()		(events + evt_tail)
#define ts_evt_pull()		(evt_tail = (evt_tail + 1) & (NR_EVENTS - 1))
#define ts_evt_clear()		(evt_head = evt_tail = 0)

static void ts_evt_add(unsigned x, unsigned y, unsigned down) {
	unsigned ts_event;
	int next_head;

	ts_event = ((x << 16) | (y)) | (down << 31);
	next_head = (evt_head + 1) & (NR_EVENTS - 1);

	if (next_head != evt_tail) {
		events[evt_head] = ts_event;
		evt_head = next_head;
		//printk("====>Add ... [ %4d,  %4d ]%s\n", x, y, down ? "":" ~~~");

		/* wake up any read call */
		if (waitqueue_active(&ts_waitq)) {
			wake_up_interruptible(&ts_waitq);
		}
	} else {
		/* drop the event and try to wakeup readers */
		printk(KERN_WARNING "mini6410-ts: touch event buffer full");
		wake_up_interruptible(&ts_waitq);
	}
}

static unsigned int s3c_ts_poll( struct file *file, struct poll_table_struct *wait)
{
	unsigned int mask = 0;

	poll_wait(file, &ts_waitq, wait);
	if (ts_evt_pending())
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static int s3c_ts_read(struct file *filp, char __user *buff, size_t count, loff_t *offp)
{
	DECLARE_WAITQUEUE(wait, current);
	char *ptr = buff;
	int err = 0;

	add_wait_queue(&ts_waitq, &wait);
	while (count >= sizeof(TS_EVENT)) {
		err = -ERESTARTSYS;
		if (signal_pending(current))
			break;

		if (ts_evt_pending()) {
			TS_EVENT *evt = ts_evt_get();

			err = copy_to_user(ptr, evt, sizeof(TS_EVENT));
			ts_evt_pull();

			if (err)
				break;

			ptr += sizeof(TS_EVENT);
			count -= sizeof(TS_EVENT);
			continue;
		}

		set_current_state(TASK_INTERRUPTIBLE);
		err = -EAGAIN;
		if (filp->f_flags & O_NONBLOCK)
			break;
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&ts_waitq, &wait);

	return ptr == buff ? err : ptr - buff;
}

static int s3c_ts_open(struct inode *inode, struct file *filp) {
	/* flush event queue */
	ts_evt_clear();

	return 0;
}

static struct file_operations dev_fops = {
	.owner				= THIS_MODULE,
	.read				= s3c_ts_read,
	.poll				= s3c_ts_poll,
	.open				= s3c_ts_open,
};

static struct miscdevice misc = {
	.minor				= 180,
	.name				= DEVICE_NAME,
	.fops				= &dev_fops,
};

#define WAIT4INT(x)		(((x) << 8) | \
		S3C_ADCTSC_YM_SEN | S3C_ADCTSC_YP_SEN | S3C_ADCTSC_XP_SEN | \
		S3C_ADCTSC_XY_PST(3))

#define AUTOPST			(S3C_ADCTSC_YM_SEN | S3C_ADCTSC_YP_SEN | S3C_ADCTSC_XP_SEN | \
		S3C_ADCTSC_AUTO_PST | S3C_ADCTSC_XY_PST(0))

static void __iomem 	*ts_base;
static struct resource	*ts_mem;
static struct resource	*ts_irq;
static struct clk		*ts_clock;
static struct s3c_ts_info	*ts;

/**
 * get_down - return the down state of the pen
 * @data0: The data read from ADCDAT0 register.
 * @data1: The data read from ADCDAT1 register.
 *
 * Return non-zero if both readings show that the pen is down.
 */
static inline bool get_down(unsigned long data0, unsigned long data1)
{
	/* returns true if both data values show stylus down */
	return (!(data0 & S3C_ADCDAT0_UPDOWN) && !(data1 & S3C_ADCDAT1_UPDOWN));
}

static void touch_timer_fire(unsigned long data) {
	unsigned long data0;
	unsigned long data1;
	int pendown;

#ifdef CONFIG_MINI6410_ADC
	if (!ADC_locked4TS()) {
		/* Note: pen UP interrupt detected and handled, the lock is released,
		 * so do nothing in the timer which started by ADC ISR. */
		return;
	}
#endif

	data0 = readl(ts_base + S3C_ADCDAT0);
	data1 = readl(ts_base + S3C_ADCDAT1);

	pendown = get_down(data0, data1);

	if (pendown) {
		if (ts->count == (1 << ts->shift)) {
#ifdef CONFIG_TOUCHSCREEN_S3C_DEBUG
			{
				struct timeval tv;
				do_gettimeofday(&tv);
				printk(KERN_INFO "T: %06d, X: %03ld, Y: %03ld\n",
						(int)tv.tv_usec, ts->xp, ts->yp);
			}
#endif

			ts_evt_add((ts->xp >> ts->shift), (ts->yp >> ts->shift), 1);

			ts->xp = 0;
			ts->yp = 0;
			ts->count = 0;
		}

		/* start automatic sequencing A/D conversion */
		writel(S3C_ADCTSC_PULL_UP_DISABLE | AUTOPST, ts_base + S3C_ADCTSC);
		writel(readl(ts_base + S3C_ADCCON) | S3C_ADCCON_ENABLE_START,
				ts_base + S3C_ADCCON);
	} else {
		ts->xp = 0;
		ts->yp = 0;
		ts->count = 0;

		ts_evt_add(0, 0, 0);

		/* PEN is UP, Let's wait the PEN DOWN interrupt */
		writel(WAIT4INT(0), ts_base + S3C_ADCTSC);

#ifdef CONFIG_MINI6410_ADC
		if (ADC_locked4TS()) {
			s3c_ts_adc_unlock();
		}
#endif
	}
}

static DEFINE_TIMER(touch_timer, touch_timer_fire, 0, 0);

static irqreturn_t stylus_updown(int irqno, void *param)
{
#ifdef CONFIG_TOUCHSCREEN_S3C_DEBUG
	unsigned long data0;
	unsigned long data1;
	int is_waiting_up;
	int pendown;
#endif

#ifdef CONFIG_MINI6410_ADC
	if (!ADC_locked4TS()) {
		if (s3c_ts_adc_lock(LOCK_TS)) {
			/* Locking ADC controller failed */
			printk("Lock ADC failed, %d\n", adc_lock_id);
			return IRQ_HANDLED;
		}
	}
#endif

#ifdef CONFIG_TOUCHSCREEN_S3C_DEBUG
	data0 = readl(ts_base + S3C_ADCDAT0);
	data1 = readl(ts_base + S3C_ADCDAT1);

	is_waiting_up = readl(ts_base + S3C_ADCTSC) & (1 << 8);
	pendown = get_down(data0, data1);

	printk("P: %d <--> %c\n", pendown, is_waiting_up ? 'u':'d');
#endif

	if (ts->s3c_adc_con == ADC_TYPE_2) {
		/* Clear ADC and PEN Down/UP interrupt */
		__raw_writel(0x0, ts_base + S3C_ADCCLRWK);
		__raw_writel(0x0, ts_base + S3C_ADCCLRINT);
	}

	/* TODO we should never get an interrupt with pendown set while
	 * the timer is running, but maybe we ought to verify that the
	 * timer isn't running anyways. */

	touch_timer_fire(1);

	return IRQ_HANDLED;
}

static irqreturn_t stylus_action(int irqno, void *param)
{
	unsigned long data0;
	unsigned long data1;

#ifdef CONFIG_MINI6410_ADC
	if (!ADC_locked4TS()) {
		if (ADC_free()) {
			printk("Unexpected\n");

			/* Clear ADC interrupt */
			__raw_writel(0x0, ts_base + S3C_ADCCLRINT);
		}

		return IRQ_HANDLED;
	}
#endif

	data0 = readl(ts_base + S3C_ADCDAT0);
	data1 = readl(ts_base + S3C_ADCDAT1);

	if (ts->resol_bit == 12) {
#if defined(CONFIG_TOUCHSCREEN_NEW)
		ts->yp += S3C_ADCDAT0_XPDATA_MASK_12BIT - (data0 & S3C_ADCDAT0_XPDATA_MASK_12BIT);
		ts->xp += S3C_ADCDAT1_YPDATA_MASK_12BIT - (data1 & S3C_ADCDAT1_YPDATA_MASK_12BIT);
#else 
		ts->xp += data0 & S3C_ADCDAT0_XPDATA_MASK_12BIT;
		ts->yp += data1 & S3C_ADCDAT1_YPDATA_MASK_12BIT;
#endif
	} else {
#if defined(CONFIG_TOUCHSCREEN_NEW)
		ts->yp += S3C_ADCDAT0_XPDATA_MASK - (data0 & S3C_ADCDAT0_XPDATA_MASK);
		ts->xp += S3C_ADCDAT1_YPDATA_MASK - (data1 & S3C_ADCDAT1_YPDATA_MASK);
#else
		ts->xp += data0 & S3C_ADCDAT0_XPDATA_MASK;
		ts->yp += data1 & S3C_ADCDAT1_YPDATA_MASK;
#endif
	}

	ts->count++;

	if (ts->count < (1 << ts->shift)) {
		writel(S3C_ADCTSC_PULL_UP_DISABLE | AUTOPST, ts_base + S3C_ADCTSC);
		writel(readl(ts_base + S3C_ADCCON) | S3C_ADCCON_ENABLE_START, ts_base + S3C_ADCCON);
	} else {
		mod_timer(&touch_timer, jiffies + 1);
		writel(WAIT4INT(1), ts_base + S3C_ADCTSC);
	}

	if (ts->s3c_adc_con == ADC_TYPE_2) {
		/* Clear ADC and PEN Down/UP interrupt */
		__raw_writel(0x0, ts_base + S3C_ADCCLRWK);
		__raw_writel(0x0, ts_base + S3C_ADCCLRINT);
	}

	return IRQ_HANDLED;
}


#ifdef CONFIG_MINI6410_ADC
static unsigned int _adccon, _adctsc, _adcdly;

int mini6410_adc_acquire_io(void) {
	int ret;

	ret = s3c_ts_adc_lock(LOCK_ADC);
	if (!ret) {
		_adccon = readl(ts_base + S3C_ADCCON);
		_adctsc = readl(ts_base + S3C_ADCTSC);
		_adcdly = readl(ts_base + S3C_ADCDLY);
	}

	return ret;
}
EXPORT_SYMBOL(mini6410_adc_acquire_io);

void mini6410_adc_release_io(void) {
	writel(_adccon, ts_base + S3C_ADCCON);
	writel(_adctsc, ts_base + S3C_ADCTSC);
	writel(_adcdly, ts_base + S3C_ADCDLY);
	writel(WAIT4INT(0), ts_base + S3C_ADCTSC);

	s3c_ts_adc_unlock();
}

EXPORT_SYMBOL(mini6410_adc_release_io);
#endif


static struct s3c_ts_mach_info *s3c_ts_get_platdata(struct device *dev)
{
	if (dev->platform_data != NULL)
		return (struct s3c_ts_mach_info *)dev->platform_data;

	return &s3c_ts_default_cfg;
}

/*
 * The functions for inserting/removing us as a module.
 */
static int __init s3c_ts_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev;
	struct s3c_ts_mach_info * s3c_ts_cfg;
	int ret, size;

	dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev,"no memory resource specified\n");
		return -ENOENT;
	}

	size = (res->end - res->start) + 1;
	ts_mem = request_mem_region(res->start, size, pdev->name);
	if (ts_mem == NULL) {
		dev_err(dev, "failed to get memory region\n");
		ret = -ENOENT;
		goto err_req;
	}

	ts_base = ioremap(res->start, size);
	if (ts_base == NULL) {
		dev_err(dev, "failed to ioremap() region\n");
		ret = -EINVAL;
		goto err_map;
	}

	ts_clock = clk_get(&pdev->dev, "adc");
	if (IS_ERR(ts_clock)) {
		dev_err(dev, "failed to find watchdog clock source\n");
		ret = PTR_ERR(ts_clock);
		goto err_clk;
	}

	clk_enable(ts_clock);

	s3c_ts_cfg = s3c_ts_get_platdata(&pdev->dev);

	if ((s3c_ts_cfg->presc & 0xff) > 0)
		writel(S3C_ADCCON_PRSCEN | S3C_ADCCON_PRSCVL(s3c_ts_cfg->presc & 0xff),
				ts_base + S3C_ADCCON);
	else
		writel(0, ts_base + S3C_ADCCON);

	/* Initialise registers */
	if ((s3c_ts_cfg->delay & 0xffff) > 0)
		writel(s3c_ts_cfg->delay & 0xffff, ts_base + S3C_ADCDLY);

	if (s3c_ts_cfg->resol_bit == 12) {
		switch(s3c_ts_cfg->s3c_adc_con) {
			case ADC_TYPE_2:
				writel(readl(ts_base + S3C_ADCCON) | S3C_ADCCON_RESSEL_12BIT,
						ts_base + S3C_ADCCON);
				break;

			case ADC_TYPE_1:
				writel(readl(ts_base + S3C_ADCCON) | S3C_ADCCON_RESSEL_12BIT_1,
						ts_base + S3C_ADCCON);
				break;

			default:
				dev_err(dev, "Touchscreen over this type of AP isn't supported !\n");
				break;
		}
	}

	writel(WAIT4INT(0), ts_base + S3C_ADCTSC);

	ts = kzalloc(sizeof(struct s3c_ts_info), GFP_KERNEL);

	ts->shift = s3c_ts_cfg->oversampling_shift;
	ts->resol_bit = s3c_ts_cfg->resol_bit;
	ts->s3c_adc_con = s3c_ts_cfg->s3c_adc_con;

	/* For IRQ_PENDUP */
	ts_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (ts_irq == NULL) {
		dev_err(dev, "no irq resource specified\n");
		ret = -ENOENT;
		goto err_irq;
	}

	ret = request_irq(ts_irq->start, stylus_updown, IRQF_SAMPLE_RANDOM, "s3c_updown", ts);
	if (ret != 0) {
		dev_err(dev,"s3c_ts.c: Could not allocate ts IRQ_PENDN !\n");
		ret = -EIO;
		goto err_irq;
	}

	/* For IRQ_ADC */
	ts_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	if (ts_irq == NULL) {
		dev_err(dev, "no irq resource specified\n");
		ret = -ENOENT;
		goto err_irq;
	}

	ret = request_irq(ts_irq->start, stylus_action, IRQF_SAMPLE_RANDOM | IRQF_SHARED,
			"s3c_action", ts);
	if (ret != 0) {
		dev_err(dev, "s3c_ts.c: Could not allocate ts IRQ_ADC !\n");
		ret = -EIO;
		goto err_irq;
	}

	printk(KERN_INFO "%s got loaded successfully : %d bits\n", DEVICE_NAME, s3c_ts_cfg->resol_bit);

	ret = misc_register(&misc);
	if (ret) {
		dev_err(dev, "s3c_ts.c: Could not register device(mini6410 touchscreen)!\n");
		ret = -EIO;
		goto fail;
	}

	return 0;

fail:
	free_irq(ts_irq->start, ts->dev);
	free_irq(ts_irq->end, ts->dev);

err_irq:
	kfree(ts);

	clk_disable(ts_clock);
	clk_put(ts_clock);

err_clk:
	iounmap(ts_base);

err_map:
	release_resource(ts_mem);
	kfree(ts_mem);

err_req:
	return ret;
}

static int s3c_ts_remove(struct platform_device *dev)
{
	printk(KERN_INFO "s3c_ts_remove() of TS called !\n");

	disable_irq(IRQ_ADC);
	disable_irq(IRQ_PENDN);

	free_irq(IRQ_PENDN, ts->dev);
	free_irq(IRQ_ADC, ts->dev);

	if (ts_clock) {
		clk_disable(ts_clock);
		clk_put(ts_clock);
		ts_clock = NULL;
	}

	misc_deregister(&misc);
	iounmap(ts_base);

	return 0;
}

#ifdef CONFIG_PM
static unsigned int adccon, adctsc, adcdly;

static int s3c_ts_suspend(struct platform_device *dev, pm_message_t state)
{
	adccon = readl(ts_base + S3C_ADCCON);
	adctsc = readl(ts_base + S3C_ADCTSC);
	adcdly = readl(ts_base + S3C_ADCDLY);

	disable_irq(IRQ_ADC);
	disable_irq(IRQ_PENDN);

	clk_disable(ts_clock);

	return 0;
}

static int s3c_ts_resume(struct platform_device *pdev)
{
	clk_enable(ts_clock);

	writel(adccon, ts_base + S3C_ADCCON);
	writel(adctsc, ts_base + S3C_ADCTSC);
	writel(adcdly, ts_base + S3C_ADCDLY);
	writel(WAIT4INT(0), ts_base + S3C_ADCTSC);

	enable_irq(IRQ_ADC);
	enable_irq(IRQ_PENDN);
	return 0;
}
#else
#define s3c_ts_suspend	NULL
#define s3c_ts_resume	NULL
#endif

static struct platform_driver s3c_ts_driver = {
	.probe			= s3c_ts_probe,
	.remove			= s3c_ts_remove,
	.suspend		= s3c_ts_suspend,
	.resume			= s3c_ts_resume,
	.driver			= {
		.owner			= THIS_MODULE,
		.name			= "s3c-ts",
	},
};

static char banner[] __initdata = KERN_INFO "S3C Touchscreen driver, (c) 2010 FriendlyARM\n";

static int __init s3c_ts_init(void)
{
	printk(banner);
	return platform_driver_register(&s3c_ts_driver);
}

static void __exit s3c_ts_exit(void)
{
	platform_driver_unregister(&s3c_ts_driver);
}

module_init(s3c_ts_init);
module_exit(s3c_ts_exit);

MODULE_AUTHOR("FriendlyARM Inc.");
MODULE_LICENSE("GPL");

