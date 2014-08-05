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
#include <linux/gpio.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <plat/regs-adc.h>
#include <mach/regs-gpio.h>

/* For ts.dev.id.version */
#define S3C2410TSVERSION	0x0101

#define WAIT4INT(x)  (((x)<<8) | \
		     S3C2410_ADCTSC_YM_SEN | S3C2410_ADCTSC_YP_SEN | S3C2410_ADCTSC_XP_SEN | \
		     S3C2410_ADCTSC_XY_PST(3))

#define AUTOPST	     (S3C2410_ADCTSC_YM_SEN | S3C2410_ADCTSC_YP_SEN | S3C2410_ADCTSC_XP_SEN | \
		     S3C2410_ADCTSC_AUTO_PST | S3C2410_ADCTSC_XY_PST(0))

static char *s3c2410ts_name = "s3c2410 TouchScreen";

static	struct input_dev *dev;
static	long xp;
static	long yp;
static	int count;

DECLARE_MUTEX(ADC_LOCK); //extern struct semaphore ADC_LOCK;
static int OwnADC = 0;

static void __iomem *base_addr;

static inline void s3c2410_ts_connect(void)
{
	s3c2410_gpio_cfgpin(S3C2410_GPG(12), S3C2410_GPG12_XMON);
	s3c2410_gpio_cfgpin(S3C2410_GPG(13), S3C2410_GPG13_nXPON);
	s3c2410_gpio_cfgpin(S3C2410_GPG(14), S3C2410_GPG14_YMON);
	s3c2410_gpio_cfgpin(S3C2410_GPG(15), S3C2410_GPG15_nYPON);
}

static void touch_timer_fire(unsigned long data)
{
  	unsigned long data0;
  	unsigned long data1;
	int updown;

  	data0 = ioread32(base_addr+S3C2410_ADCDAT0);
  	data1 = ioread32(base_addr+S3C2410_ADCDAT1);

 	updown = (!(data0 & S3C2410_ADCDAT0_UPDOWN)) && (!(data1 & S3C2410_ADCDAT0_UPDOWN));

 	if (updown) {
 		if (count != 0) {
			long tmp;
                                                                                                 
			tmp = xp;
			xp = yp;
			yp = tmp;
                                                                                                 
                        xp >>= 2;
                        yp >>= 2;

 			input_report_abs(dev, ABS_X, xp);
 			input_report_abs(dev, ABS_Y, yp);

 			input_report_key(dev, BTN_TOUCH, 1);
 			input_report_abs(dev, ABS_PRESSURE, 1);
 			input_sync(dev);
 		}

 		xp = 0;
 		yp = 0;
 		count = 0;

 		iowrite32(S3C2410_ADCTSC_PULL_UP_DISABLE | AUTOPST, base_addr+S3C2410_ADCTSC);
 		iowrite32(ioread32(base_addr+S3C2410_ADCCON) | S3C2410_ADCCON_ENABLE_START, base_addr+S3C2410_ADCCON);
 	} else {
 		count = 0;

 		input_report_key(dev, BTN_TOUCH, 0);
 		input_report_abs(dev, ABS_PRESSURE, 0);
 		input_sync(dev);

 		iowrite32(WAIT4INT(0), base_addr+S3C2410_ADCTSC);
		if (OwnADC) {
			OwnADC = 0;
			up(&ADC_LOCK);
		}
 	}
}

static struct timer_list touch_timer =
		TIMER_INITIALIZER(touch_timer_fire, 0, 0);

static irqreturn_t stylus_updown(int irq, void *dev_id)
{
	unsigned long data0;
	unsigned long data1;
	int updown;

	if (down_trylock(&ADC_LOCK) == 0) {
		OwnADC = 1;
		data0 = ioread32(base_addr+S3C2410_ADCDAT0);
		data1 = ioread32(base_addr+S3C2410_ADCDAT1);

		updown = (!(data0 & S3C2410_ADCDAT0_UPDOWN)) && (!(data1 & S3C2410_ADCDAT0_UPDOWN));

		if (updown) {
			touch_timer_fire(0);
		} else {
			OwnADC = 0;
			up(&ADC_LOCK);
		}
	}

	return IRQ_HANDLED;
}


static irqreturn_t stylus_action(int irq, void *dev_id)
{
	unsigned long data0;
	unsigned long data1;

	if (OwnADC) {
		data0 = ioread32(base_addr+S3C2410_ADCDAT0);
		data1 = ioread32(base_addr+S3C2410_ADCDAT1);

		xp += data0 & S3C2410_ADCDAT0_XPDATA_MASK;
		yp += data1 & S3C2410_ADCDAT1_YPDATA_MASK;
		count++;

	    if (count < (1<<2)) {
			iowrite32(S3C2410_ADCTSC_PULL_UP_DISABLE | AUTOPST, base_addr+S3C2410_ADCTSC);
			iowrite32(ioread32(base_addr+S3C2410_ADCCON) | S3C2410_ADCCON_ENABLE_START, base_addr+S3C2410_ADCCON);
		} else {
			mod_timer(&touch_timer, jiffies+1);
			iowrite32(WAIT4INT(1), base_addr+S3C2410_ADCTSC);
		}
	}

	return IRQ_HANDLED;
}

static struct clk	*adc_clock;

static int __init s3c2410ts_init(void)
{
	struct input_dev *input_dev;

	adc_clock = clk_get(NULL, "adc");
	if (!adc_clock) {
		printk(KERN_ERR "failed to get adc clock source\n");
		return -ENOENT;
	}
	clk_enable(adc_clock);

	base_addr=ioremap(S3C2410_PA_ADC,0x20);
	if (base_addr == NULL) {
		printk(KERN_ERR "Failed to remap register block\n");
		return -ENOMEM;
	}

	/* Configure GPIOs */
	s3c2410_ts_connect();

	iowrite32(S3C2410_ADCCON_PRSCEN | S3C2410_ADCCON_PRSCVL(0xFF),\
		     base_addr+S3C2410_ADCCON);
	iowrite32(0xffff,  base_addr+S3C2410_ADCDLY);
	iowrite32(WAIT4INT(0), base_addr+S3C2410_ADCTSC);

	/* Initialise input stuff */
	input_dev = input_allocate_device();

	if (!input_dev) {
		printk(KERN_ERR "Unable to allocate the input device !!\n");
		return -ENOMEM;
	}

	dev = input_dev;
	dev->evbit[0] = BIT(EV_SYN) | BIT(EV_KEY) | BIT(EV_ABS);
	dev->keybit[BITS_TO_LONGS(BTN_TOUCH)] = BIT(BTN_TOUCH);
	input_set_abs_params(dev, ABS_X, 0, 0x3FF, 0, 0);
	input_set_abs_params(dev, ABS_Y, 0, 0x3FF, 0, 0);
	input_set_abs_params(dev, ABS_PRESSURE, 0, 1, 0, 0);

	dev->name = s3c2410ts_name;
	dev->id.bustype = BUS_RS232;
	dev->id.vendor = 0xDEAD;
	dev->id.product = 0xBEEF;
	dev->id.version = S3C2410TSVERSION;


	/* Get irqs */
	if (request_irq(IRQ_ADC, stylus_action, IRQF_SHARED|IRQF_SAMPLE_RANDOM,
		"s3c2410_action", dev)) {
		printk(KERN_ERR "s3c2410_ts.c: Could not allocate ts IRQ_ADC !\n");
		iounmap(base_addr);
		return -EIO;
	}
	if (request_irq(IRQ_TC, stylus_updown, IRQF_SAMPLE_RANDOM,
			"s3c2410_action", dev)) {
		printk(KERN_ERR "s3c2410_ts.c: Could not allocate ts IRQ_TC !\n");
		iounmap(base_addr);
		return -EIO;
	}

	printk(KERN_INFO "%s successfully loaded\n", s3c2410ts_name);

	/* All went ok, so register to the input system */
	input_register_device(dev);

	return 0;
}

static void __exit s3c2410ts_exit(void)
{
	disable_irq(IRQ_ADC);
	disable_irq(IRQ_TC);
	free_irq(IRQ_TC,dev);
	free_irq(IRQ_ADC,dev);

	if (adc_clock) {
		clk_disable(adc_clock);
		clk_put(adc_clock);
		adc_clock = NULL;
	}

	input_unregister_device(dev);
	iounmap(base_addr);
}


EXPORT_SYMBOL(ADC_LOCK);
module_init(s3c2410ts_init);
module_exit(s3c2410ts_exit);

