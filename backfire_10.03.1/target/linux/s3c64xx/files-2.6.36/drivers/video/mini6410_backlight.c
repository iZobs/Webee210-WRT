#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/miscdevice.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-gpio.h>
#include <plat/regs-timer.h>

#include <mach/gpio-bank-e.h>
#include <plat/gpio-cfg.h>

#include <linux/cdev.h>

#undef DEBUG
//#define DEBUG
#ifdef DEBUG
#define DPRINTK(x...) {printk(__FUNCTION__"(%d): ",__LINE__);printk(##x);}
#else
#define DPRINTK(x...) (void)(0)
#endif

#define DEVICE_NAME	"backlight"


static unsigned int bl_state;

static inline void set_bl(int state)
{
	bl_state = !!state;
	//printk(DEVICE_NAME ": %s\n", state ? "ON" : "OFF");
	{
		unsigned long tmp;
		tmp = readl(S3C64XX_GPEDAT);
		tmp = (tmp & ~0x1) | (!!state);
		writel(tmp, S3C64XX_GPEDAT);
	}
}

static inline unsigned int get_bl(void)
{
	return bl_state;
}

static ssize_t dev_write(struct file *file, const char *buffer, size_t count, loff_t * ppos)
{
	unsigned char ch;
	int ret;
	if (count == 0) {
		return count;
	}
	ret = copy_from_user(&ch, buffer, sizeof ch) ? -EFAULT : 0;
	if (ret) {
		return ret;
	}

	ch &= 0x01;
	set_bl(ch);
		
	return count;
}

static ssize_t dev_read(struct file *filp, char *buffer, size_t count, loff_t *ppos)
{
	int ret;
	unsigned char str[] = {'0', '1' };

	if (count == 0) {
		return 0;
	}

	ret = copy_to_user(buffer, str + get_bl(), sizeof(unsigned char) ) ? -EFAULT : 0;
	if (ret) {
		return ret;
	}

	return sizeof(unsigned char);
}

static struct file_operations dev_fops = {
	owner:	THIS_MODULE,
	read:	dev_read,	
	write:	dev_write,
};

static struct miscdevice misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &dev_fops,
};

static int __init dev_init(void)
{
	int ret;

	ret = misc_register(&misc);

	printk (DEVICE_NAME"\tinitialized\n");

	{
		unsigned long tmp;
		tmp = readl(S3C64XX_GPECON);
		tmp = (tmp & ~0xF) | 0x1;
		writel(tmp, S3C64XX_GPECON);
	}
	set_bl(1);
	return ret;
}


static void __exit dev_exit(void)
{
	misc_deregister(&misc);
}

module_init(dev_init);
module_exit(dev_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("FriendlyARM Inc.");
