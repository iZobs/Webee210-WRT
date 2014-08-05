/*
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
 * Copyright (c) 2004 Arnaud Patard <arnaud.patard@rtp-net.org>
 * iPAQ H1940 touchscreen support
 *
 * ChangeLog
 *
 * 2004-09-05: Herbert Pötzl <herbert@13thfloor.at>
 *      - Added clock (de-)allocation code
 *
 * 2005-03-06: Arnaud Patard <arnaud.patard@rtp-net.org>
 *      - h1940_ -> s3c2410 (this driver is now also used on the n30
 *        machines :P)
 *      - Debug messages are now enabled with the config option
 *        TOUCHSCREEN_S3C2410_DEBUG
 *      - Changed the way the value are read
 *      - Input subsystem should now work
 *      - Use ioremap and readl/writel
 *
 * 2005-03-23: Arnaud Patard <arnaud.patard@rtp-net.org>
 *      - Make use of some undocumented features of the touchscreen
 *        controller
 *
 * 2007-05-23: Harald Welte <laforge@openmoko.org>
 *      - Add proper support for S32440
 *
 * 2008-06-23: Andy Green <andy@openmoko.com>
 *      - Removed averaging system
 *      - Added generic Touchscreen filter stuff
 *
 * 2008-11-27: Nelson Castillo <arhuaco@freaks-unidos.net>
 *      - Improve interrupt handling
 *
 * 2009-04-09: Nelson Castillo <arhuaco@freaks-unidos.net>
 *      - Use s3c-adc API (Vasily Khoruzhick <anarsoul@gmail.com> provided
 *        a working example for a simpler version of this driver).
 *
 * 2009-09-04: Nelson Castillo <arhuaco@freaks-unidos.net>
 *      - Removed generic Touchscreen filters.
 *      - Added simple filter and module parameters.
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <asm/irq.h>

#include <mach/regs-gpio.h>
#include <mach/ts.h>
#include <mach/hardware.h>
#include <plat/regs-adc.h>
#include <plat/adc.h>

/* For ts.dev.id.version */
#define S3C2410TSVERSION	0x0101

#define WAIT4INT(x)		(((x)<<8) | \
				S3C2410_ADCTSC_YM_SEN | \
				S3C2410_ADCTSC_YP_SEN | \
				S3C2410_ADCTSC_XP_SEN | \
				S3C2410_ADCTSC_XY_PST(3))

#define TSPRINTK(fmt, args...) \
	printk(KERN_DEBUG "%s: " fmt, __func__ , ## args)
#ifdef CONFIG_TOUCHSCREEN_S3C2410_DEBUG
#	define DPRINTK TSPRINTK
#else
#	define DPRINTK(fmt, args...)
#endif


MODULE_AUTHOR("Arnaud Patard <arnaud.patard@rtp-net.org>");
MODULE_DESCRIPTION("s3c2410 touchscreen driver");
MODULE_LICENSE("GPL");

static int naverage = 8;
static int maxsquares = 10000;
static int maxattempts = 10;

module_param(naverage, int, 0);
module_param(maxsquares, int, 0);
module_param(maxattempts, int, 0);

/*
 * Definitions & global arrays.
 */

static char *s3c2410ts_name = "s3c2410 TouchScreen";

#define TS_RELEASE_TIMEOUT (HZ >> 7 ? HZ >> 7 : 1) /* 8ms (5ms if HZ is 200) */
#define TS_EVENT_FIFO_SIZE (2 << 6) /* Must be a power of 2. */

/*
 * Basic functions we use to filter noise.
 */

struct filter_data {
	int N;		/* Number of collected points. */
	int attempts;	/* How many times have we tried to get a sample? */
	int *x;		/* X coordinates that will be averaged. */
	int *y;		/* Y coordinates that will be averaged. */
};

static void filter_clear(struct filter_data *f) {
	f->N = 0;
	f->attempts = 0;
}

static int filter_feed(struct filter_data *f, int coord[2]) {
	f->x[f->N] = coord[0];
	f->y[f->N] = coord[1];
	if (++f->N == naverage) {
		int sum_x = 0, sum_y = 0;
		unsigned dist_x = 0, dist_y = 0;
		int i;
		for (i = 0; i < naverage; ++i) {
			sum_x += f->x[i];
			sum_y += f->y[i];
			if (i) {
				dist_x += (f->x[i] - f->x[i - 1]) *
					  (f->x[i] - f->x[i - 1]);
				dist_y += (f->y[i] - f->y[i - 1]) *
					  (f->y[i] - f->y[i - 1]);
			}
		}
		if (max(dist_x, dist_y) > maxsquares) {
			if (++f->attempts > maxattempts)
				return -1; /* Give up. */
			f->N = 0;
			return 0; /* We need more points. */
		}
		coord[0] = (sum_x + (naverage >> 1)) / naverage;
		coord[1] = (sum_y + (naverage >> 1)) / naverage;
		filter_clear(f);
		return 1; /* We have a result. */
	}
	return 0; /* We need more points. */
}

/*
 * Per-touchscreen data.
 */

enum	ts_state {TS_STATE_STANDBY, TS_STATE_PRESSED, TS_STATE_RELEASE_PENDING,
		  TS_STATE_RELEASE};

struct s3c2410ts {
	struct filter_data filter;
	struct input_dev *dev;
	enum ts_state state;
	int is_down;
	struct kfifo *event_fifo;
	struct s3c_adc_client *adc_client;
	unsigned adc_selected;
};

static struct s3c2410ts ts;

static void __iomem *base_addr;

/*
 * A few low level functions.
 */

static inline void s3c2410_ts_connect(void)
{
	s3c2410_gpio_cfgpin(S3C2410_GPG12, S3C2410_GPG12_XMON);
	s3c2410_gpio_cfgpin(S3C2410_GPG13, S3C2410_GPG13_nXPON);
	s3c2410_gpio_cfgpin(S3C2410_GPG14, S3C2410_GPG14_YMON);
	s3c2410_gpio_cfgpin(S3C2410_GPG15, S3C2410_GPG15_nYPON);
}

/*
 * Code that starts ADC conversions.
 */

static void ts_adc_timer_f(unsigned long data);
static struct timer_list ts_adc_timer = TIMER_INITIALIZER(ts_adc_timer_f, 0, 0);

static void ts_adc_timer_f(unsigned long data)
{
	if (s3c_adc_start(ts.adc_client, 0, 1))
		mod_timer(&ts_adc_timer, jiffies + 1);
}

static void s3c2410_ts_start_adc_conversion(void)
{
	if (ts.adc_selected)
		mod_timer(&ts_adc_timer, jiffies + 1);
	else
		ts_adc_timer_f(0);
}

/* Callback for the s3c-adc API. */
void adc_selected_f(unsigned selected)
{
	ts.adc_selected = selected;
}

/*
 * Just send the input events.
 */

enum ts_input_event {IE_DOWN = 0, IE_UP};

static void ts_input_report(int event, int coords[])
{
#ifdef CONFIG_TOUCHSCREEN_S3C2410_DEBUG
	static char *s[] = {"down", "up"};
	struct timeval tv;

	do_gettimeofday(&tv);
#endif

	if (event == IE_DOWN) {
		input_report_abs(ts.dev, ABS_X, coords[0]);
		input_report_abs(ts.dev, ABS_Y, coords[1]);
		input_report_key(ts.dev, BTN_TOUCH, 1);
		input_report_abs(ts.dev, ABS_PRESSURE, 1);

		DPRINTK("T:%06d %6s (X:%03d, Y:%03d)\n",
			(int)tv.tv_usec, s[event], coords[0], coords[1]);
	} else {
		input_report_key(ts.dev, BTN_TOUCH, 0);
		input_report_abs(ts.dev, ABS_PRESSURE, 0);

		DPRINTK("T:%06d %6s\n",
			(int)tv.tv_usec, s[event]);
	}

	input_sync(ts.dev);
}

/*
 * Manage the state of the touchscreen.
 */

static void event_send_timer_f(unsigned long data);

static struct timer_list event_send_timer =
		TIMER_INITIALIZER(event_send_timer_f, 0, 0);

static void event_send_timer_f(unsigned long data)
{
	static int noop_counter;
	int event_type;

	while (__kfifo_get(ts.event_fifo, (unsigned char *)&event_type,
			   sizeof(int))) {
		int buf[2];

		switch (event_type) {
		case 'D':
			if (ts.state == TS_STATE_RELEASE_PENDING)
				/* Ignore short UP event. */
				ts.state = TS_STATE_PRESSED;
			break;

		case 'U':
			ts.state = TS_STATE_RELEASE_PENDING;
			break;

		case 'P':
			if (ts.is_down) /* Stylus_action needs a conversion. */
				s3c2410_ts_start_adc_conversion();

			if (unlikely(__kfifo_get(ts.event_fifo,
						 (unsigned char *)buf,
						 sizeof(int) * 2)
				     != sizeof(int) * 2)) {
				/* This will only happen if we have a bug. */
				TSPRINTK("Invalid packet\n");
				return;
			}

			ts_input_report(IE_DOWN, buf);
			ts.state = TS_STATE_PRESSED;
			break;
		}

		noop_counter = 0;
	}

	if (noop_counter++ >= 1) {
		noop_counter = 0;
		if (ts.state == TS_STATE_RELEASE_PENDING) {
			/*
			 * We delay the UP event for a while to avoid jitter.
			 * If we get a DOWN event we do not send it.
			 */
			ts_input_report(IE_UP, NULL);
			ts.state = TS_STATE_STANDBY;

			filter_clear(&ts.filter);
		}
	} else {
		mod_timer(&event_send_timer, jiffies + TS_RELEASE_TIMEOUT);
	}
}

/*
 * Manage interrupts.
 */

static irqreturn_t stylus_updown(int irq, void *dev_id)
{
	unsigned long data0;
	unsigned long data1;
	int event_type;

	data0 = readl(base_addr + S3C2410_ADCDAT0);
	data1 = readl(base_addr + S3C2410_ADCDAT1);

	ts.is_down = !(data0 & S3C2410_ADCDAT0_UPDOWN) &&
		     !(data1 & S3C2410_ADCDAT0_UPDOWN);

	event_type = ts.is_down ? 'D' : 'U';

	if (unlikely(__kfifo_put(ts.event_fifo, (unsigned char *)&event_type,
		     sizeof(int)) != sizeof(int)))
		/* Only happens if we have a bug. */
		TSPRINTK("FIFO full\n");

	if (ts.is_down)
		s3c2410_ts_start_adc_conversion();
	else
		writel(WAIT4INT(0), base_addr + S3C2410_ADCTSC);

	mod_timer(&event_send_timer, jiffies + 1);

	return IRQ_HANDLED;
}

static void stylus_adc_action(unsigned p0, unsigned p1, unsigned *conv_left)
{
	int buf[3];

	/* TODO: Do we really need this? */
	if (p0 & S3C2410_ADCDAT0_AUTO_PST ||
	    p1 & S3C2410_ADCDAT1_AUTO_PST) {
		*conv_left = 1;
		return;
	}

	buf[1] = p0;
	buf[2] = p1;

	switch (filter_feed(&ts.filter, &buf[1])) {
	case 0:
		/* The filter wants more points. */
		*conv_left = 1;
		return;
	case 1:
		/* We have a point from the filters or no filtering enabled. */
		buf[0] = 'P';
		break;
	default:
		TSPRINTK("invalid return value\n");
	case -1:
		/* Too much noise. Ignore the event. */
		filter_clear(&ts.filter);
		writel(WAIT4INT(1), base_addr + S3C2410_ADCTSC);
		return;
	};

	if (unlikely(__kfifo_put(ts.event_fifo, (unsigned char *)buf,
		     sizeof(int) * 3) != sizeof(int) * 3))
		/* This will only happen if we have a bug. */
		TSPRINTK("FIFO full\n");

	writel(WAIT4INT(1), base_addr + S3C2410_ADCTSC);
	mod_timer(&event_send_timer, jiffies + 1);

	return;
}

/*
 * The functions for inserting/removing us as a module.
 */

static int __init s3c2410ts_probe(struct platform_device *pdev)
{
	int rc;
	struct s3c2410_ts_mach_info *info;
	struct input_dev *input_dev;
	int ret = 0;

	dev_info(&pdev->dev, "Starting\n");

	info = (struct s3c2410_ts_mach_info *)pdev->dev.platform_data;

	if (!info) {
		dev_err(&pdev->dev, "No platform data\n");
		return -EINVAL;
	}

	if (naverage <= 0 || maxsquares <= 0 || maxattempts <= 0) {
		dev_err(&pdev->dev, "Invalid parameters\n");
		return -EINVAL;
	}

	base_addr = ioremap(S3C2410_PA_ADC, 0x20);
	if (base_addr == NULL) {
		dev_err(&pdev->dev, "Failed to remap register block\n");
		ret = -ENOMEM;
		goto bail0;
	}

	/* If we acutally are a S3C2410: Configure GPIOs */
	if (!strcmp(pdev->name, "s3c2410-ts"))
		s3c2410_ts_connect();

	writel(WAIT4INT(0), base_addr + S3C2410_ADCTSC);

	/* Initialise input stuff */
	memset(&ts, 0, sizeof(struct s3c2410ts));

	ts.adc_client =
		s3c_adc_register(pdev, adc_selected_f, stylus_adc_action, 1);
	if (!ts.adc_client) {
		dev_err(&pdev->dev,
			"Unable to register s3c2410_ts as s3_adc client\n");
		iounmap(base_addr);
		ret = -EIO;
		goto bail0;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&pdev->dev, "Unable to allocate the input device\n");
		ret = -ENOMEM;
		goto bail1;
	}

	ts.dev = input_dev;
	ts.dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) |
			   BIT_MASK(EV_ABS);
	ts.dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	input_set_abs_params(ts.dev, ABS_X, 0, 0x3FF, 0, 0);
	input_set_abs_params(ts.dev, ABS_Y, 0, 0x3FF, 0, 0);
	input_set_abs_params(ts.dev, ABS_PRESSURE, 0, 1, 0, 0);

	ts.dev->name = s3c2410ts_name;
	ts.dev->id.bustype = BUS_RS232;
	ts.dev->id.vendor = 0xDEAD;
	ts.dev->id.product = 0xBEEF;
	ts.dev->id.version = S3C2410TSVERSION;
	ts.state = TS_STATE_STANDBY;
	ts.event_fifo = kfifo_alloc(TS_EVENT_FIFO_SIZE, GFP_KERNEL, NULL);
	if (IS_ERR(ts.event_fifo)) {
		ret = -EIO;
		goto bail2;
	}

	ts.filter.x = kmalloc(2 * naverage * sizeof(int), GFP_KERNEL);
	if (!ts.filter.x) {
		iounmap(base_addr);
		goto bail3;
	}

	ts.filter.y = ts.filter.x + naverage;
	filter_clear(&ts.filter);

	/* Get IRQ. */
	if (request_irq(IRQ_TC, stylus_updown, IRQF_SAMPLE_RANDOM,
			"s3c2410_action", ts.dev)) {
		dev_err(&pdev->dev, "Could not allocate ts IRQ_TC !\n");
		iounmap(base_addr);
		ret = -EIO;
		goto bail3;
	}

	dev_info(&pdev->dev, "Successfully loaded\n");

	/* All went ok. Register to the input system. */
	rc = input_register_device(ts.dev);
	if (rc) {
		dev_info(&pdev->dev, "Could not register input device\n");
		ret = -EIO;
		goto bail4;
	}

	return 0;

bail4:
	free_irq(IRQ_TC, ts.dev);
	iounmap(base_addr);
	disable_irq(IRQ_TC);
bail3:
        kfree(ts.filter.x);
	kfifo_free(ts.event_fifo);
bail2:
	input_unregister_device(ts.dev);
bail1:
	iounmap(base_addr);
bail0:

	return ret;
}

static int s3c2410ts_remove(struct platform_device *pdev)
{
	disable_irq(IRQ_TC);
	free_irq(IRQ_TC, ts.dev);

	s3c_adc_release(ts.adc_client);
	input_unregister_device(ts.dev);
	iounmap(base_addr);

	kfifo_free(ts.event_fifo);
	kfree(ts.filter.x);

	return 0;
}

#ifdef CONFIG_PM

#define TSC_SLEEP	(S3C2410_ADCTSC_PULL_UP_DISABLE | \
			 S3C2410_ADCTSC_XY_PST(0))

static int s3c2410ts_suspend(struct platform_device *pdev, pm_message_t state)
{
	writel(TSC_SLEEP, base_addr + S3C2410_ADCTSC);
	writel(readl(base_addr + S3C2410_ADCCON) | S3C2410_ADCCON_STDBM,
	       base_addr + S3C2410_ADCCON);
	disable_irq(IRQ_TC);

	return 0;
}

static int s3c2410ts_resume(struct platform_device *pdev)
{
	filter_clear(&ts.filter);
	enable_irq(IRQ_TC);
	writel(WAIT4INT(0), base_addr + S3C2410_ADCTSC);

	return 0;
}

#else
#define s3c2410ts_suspend NULL
#define s3c2410ts_resume  NULL
#endif

static struct platform_driver s3c2410ts_driver = {
	.driver = {
		.name	= "s3c2410-ts",
		.owner	= THIS_MODULE,
	},
	.probe		= s3c2410ts_probe,
	.remove		= s3c2410ts_remove,
	.suspend	= s3c2410ts_suspend,
	.resume		= s3c2410ts_resume,
};

static struct platform_driver s3c2440ts_driver = {
	.driver = {
		.name	= "s3c2440-ts",
		.owner	= THIS_MODULE,
	},
	.probe		= s3c2410ts_probe,
	.remove		= s3c2410ts_remove,
	.suspend	= s3c2410ts_suspend,
	.resume		= s3c2410ts_resume,
};

static int __init s3c2410ts_init(void)
{
	int rc;

	rc = platform_driver_register(&s3c2410ts_driver);
	if (rc < 0)
		return rc;

	rc = platform_driver_register(&s3c2440ts_driver);
	if (rc < 0)
		platform_driver_unregister(&s3c2410ts_driver);

	return rc;
}

static void __exit s3c2410ts_exit(void)
{
	platform_driver_unregister(&s3c2440ts_driver);
	platform_driver_unregister(&s3c2410ts_driver);
}

module_init(s3c2410ts_init);
module_exit(s3c2410ts_exit);

