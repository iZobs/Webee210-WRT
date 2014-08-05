/* linux/drivers/input/touchscreen/ts-if.c
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
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>

#if defined(CONFIG_FB_S3C_EXT_TFT480272)
#define S3CFB_HRES		480	/* horizon pixel  x resolition */
#define S3CFB_VRES		272	/* line cnt       y resolution */
#elif defined(CONFIG_FB_S3C_EXT_TFT800480)
#define S3CFB_HRES		800	/* horizon pixel  x resolition */
#define S3CFB_VRES		480	/* line cnt       y resolution */
#elif defined(CONFIG_FB_S3C_EXT_X240320)
#define S3CFB_HRES		240	/* horizon pixel  x resolition */
#define S3CFB_VRES		320	/* line cnt       y resolution */
#else
#error mini6410 frame buffer driver not configed
#endif

#define S3C_TSVERSION	0x0101
#define DEBUG_LVL    KERN_DEBUG

static struct input_dev *input_dev;
static char phys[] = "input(ts)";

#define DEVICE_NAME     "ts-if"

static long _ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned is_down;
	is_down = (((unsigned)(arg)) >> 31);
	if (is_down) {
		unsigned x,y;
		x = (arg >> 16) & 0x7FFF;
		y = arg &0x7FFF;
		input_report_abs(input_dev, ABS_X, x);
		input_report_abs(input_dev, ABS_Y, y);

		input_report_key(input_dev, BTN_TOUCH, 1);
		input_report_abs(input_dev, ABS_PRESSURE, 1);
		input_sync(input_dev);
	} else {
		input_report_key(input_dev, BTN_TOUCH, 0);
		input_report_abs(input_dev, ABS_PRESSURE, 0);
		input_sync(input_dev);
	}
	return 0;
}


static struct file_operations dev_fops = {
    .owner   =   THIS_MODULE,
    .unlocked_ioctl   =   _ioctl,
};

static struct miscdevice misc = {
	.minor = 185,
	.name = DEVICE_NAME,
	.fops = &dev_fops,
};

static int __init dev_init(void)
{
	int ret;

	input_dev = input_allocate_device();
	if (!input_dev) {
		ret = -ENOMEM;
		return ret;
	}
	
	input_dev->evbit[0] = input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	input_set_abs_params(input_dev, ABS_X, 0, S3CFB_HRES, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, S3CFB_VRES, 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE, 0, 1, 0, 0);

	input_dev->name = "TouchScreen Pipe";
	input_dev->phys = phys;
	input_dev->id.bustype = BUS_RS232;
	input_dev->id.vendor = 0xDEAD;
	input_dev->id.product = 0xBEEF;
	input_dev->id.version = S3C_TSVERSION;

	/* All went ok, so register to the input system */
	ret = input_register_device(input_dev);
	
	if(ret) {
		printk("s3c_ts.c: Could not register input device(touchscreen)!\n");
		input_free_device(input_dev);
		return ret;
	}

	ret = misc_register(&misc);
	if (ret) {
		input_unregister_device(input_dev);
		input_free_device(input_dev);
		return ret;
	}
	printk (DEVICE_NAME"\tinitialized\n");
    	return ret;
}

static void __exit dev_exit(void)
{
	input_unregister_device(input_dev);
	misc_deregister(&misc);
}

module_init(dev_init);
module_exit(dev_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("FriendlyARM Inc.");
MODULE_DESCRIPTION("S3C6410 Touch Screen Interface Driver");
