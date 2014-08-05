/*
 *  Driver for YeeLoong laptop extras
 *
 *  Copyright (C) 2009 Lemote Inc.
 *  Author: Wu Zhangjin <wuzj@lemote.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/apm-emulation.h>
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/video_output.h>

#include <asm/bootinfo.h>	/* for arcs_cmdline */

#include <loongson.h>

#include <cs5536/cs5536.h>
#include "ec_kb3310b.h"

#define DRIVER_VERSION "0.1"

#define EC_VER_LEN 64

static int ec_ver_small_than(char *version)
{
	char *p, ec_ver[EC_VER_LEN];

	p = strstr(arcs_cmdline, "EC_VER=");
	if (!p)
		memset(ec_ver, 0, EC_VER_LEN);
	else {
		strncpy(ec_ver, p, EC_VER_LEN);
		p = strstr(ec_ver, " ");
		if (p)
			*p = '\0';
	}

	/* Seems EC(>=PQ1D26) does this job for us, we can not do it again,
	 * otherwise, the brightness will not resume to the normal level! */
	if (strncasecmp(ec_ver, version, 64) < 0)
		return 1;
	return 0;
}
/* backlight subdriver */
#define MAX_BRIGHTNESS 8

static int yeeloong_set_brightness(struct backlight_device *bd)
{
	unsigned int level, current_level;
	static unsigned int old_level;

	level = (bd->props.fb_blank == FB_BLANK_UNBLANK &&
		 bd->props.power == FB_BLANK_UNBLANK) ?
	    bd->props.brightness : 0;

	if (level > MAX_BRIGHTNESS)
		level = MAX_BRIGHTNESS;
	else if (level < 0)
		level = 0;

	/* avoid to modify the brightness when EC is tuning it */
	current_level = ec_read(REG_DISPLAY_BRIGHTNESS);
	if ((old_level == current_level) && (old_level != level))
		ec_write(REG_DISPLAY_BRIGHTNESS, level);
	old_level = level;

	return 0;
}

static int yeeloong_get_brightness(struct backlight_device *bd)
{
	return (int)ec_read(REG_DISPLAY_BRIGHTNESS);
}

static struct backlight_ops backlight_ops = {
	.get_brightness = yeeloong_get_brightness,
	.update_status = yeeloong_set_brightness,
};

static struct backlight_device *yeeloong_backlight_dev;

static int yeeloong_backlight_init(struct device *dev)
{
	int ret;

	yeeloong_backlight_dev = backlight_device_register("backlight0", dev,
			NULL, &backlight_ops);

	if (IS_ERR(yeeloong_backlight_dev)) {
		ret = PTR_ERR(yeeloong_backlight_dev);
		yeeloong_backlight_dev = NULL;
		return ret;
	}

	yeeloong_backlight_dev->props.max_brightness = MAX_BRIGHTNESS;
	yeeloong_backlight_dev->props.brightness =
		yeeloong_get_brightness(yeeloong_backlight_dev);
	backlight_update_status(yeeloong_backlight_dev);

	return 0;
}

static void yeeloong_backlight_exit(void)
{
	if (yeeloong_backlight_dev) {
		backlight_device_unregister(yeeloong_backlight_dev);
		yeeloong_backlight_dev = NULL;
	}
}

/* hwmon subdriver */

#define MIN_FAN_SPEED 0
#define MAX_FAN_SPEED 3

static int get_fan_pwm_enable(void)
{
	int level, mode;

	level = ec_read(REG_FAN_SPEED_LEVEL);
	mode = ec_read(REG_FAN_AUTO_MAN_SWITCH);

	if (level == MAX_FAN_SPEED && mode == BIT_FAN_MANUAL)
		mode = 0;
	else if (mode == BIT_FAN_MANUAL)
		mode = 1;
	else
		mode = 2;

	return mode;
}

static void set_fan_pwm_enable(int mode)
{
	switch (mode) {
	case 0:
		/* fullspeed */
		ec_write(REG_FAN_AUTO_MAN_SWITCH, BIT_FAN_MANUAL);
		ec_write(REG_FAN_SPEED_LEVEL, MAX_FAN_SPEED);
		break;
	case 1:
		ec_write(REG_FAN_AUTO_MAN_SWITCH, BIT_FAN_MANUAL);
		break;
	case 2:
		ec_write(REG_FAN_AUTO_MAN_SWITCH, BIT_FAN_AUTO);
		break;
	default:
		break;
	}
}

static int get_fan_pwm(void)
{
	return ec_read(REG_FAN_SPEED_LEVEL);
}

static void set_fan_pwm(int value)
{
	int status, mode;

	mode = ec_read(REG_FAN_AUTO_MAN_SWITCH);
	if (mode != BIT_FAN_MANUAL)
		return;

	value = SENSORS_LIMIT(value, MIN_FAN_SPEED, MAX_FAN_SPEED);

	/* If value is not ZERO, We should ensure it is on */
	if (value != 0) {
		status = ec_read(REG_FAN_STATUS);
		if (status == 0)
			ec_write(REG_FAN_CONTROL, BIT_FAN_CONTROL_ON);
	}
	ec_write(REG_FAN_SPEED_LEVEL, value);
}

static int get_fan_rpm(void)
{
	int value = 0;

	value = FAN_SPEED_DIVIDER /
	    (((ec_read(REG_FAN_SPEED_HIGH) & 0x0f) << 8) |
	     ec_read(REG_FAN_SPEED_LOW));

	return value;
}

static int get_cpu_temp(void)
{
	int value;

	value = ec_read(REG_TEMPERATURE_VALUE);

	if (value & (1 << 7))
		value = (value & 0x7f) - 128;
	else
		value = value & 0xff;

	return value * 1000;
}

static int get_cpu_temp_max(void)
{
	/* 60℃  is the max temp for loongson work normally. */
	return 60 * 1000;
}

static int get_battery_temp(void)
{
	int value;

	value = (ec_read(REG_BAT_TEMPERATURE_HIGH) << 8) |
		(ec_read(REG_BAT_TEMPERATURE_LOW));

	return value * 1000;
}

static int get_battery_current(void)
{
	int value;

	value = (ec_read(REG_BAT_CURRENT_HIGH) << 8) |
		(ec_read(REG_BAT_CURRENT_LOW));

	if (value & 0x8000)
		value = 0xffff - value;

	return value;
}

static int get_battery_voltage(void)
{
	int value;

	value = (ec_read(REG_BAT_VOLTAGE_HIGH) << 8) |
		(ec_read(REG_BAT_VOLTAGE_LOW));

	return value;
}


static int parse_arg(const char *buf, unsigned long count, int *val)
{
	if (!count)
		return 0;
	if (sscanf(buf, "%i", val) != 1)
		return -EINVAL;
	return count;
}

static ssize_t store_sys_hwmon(void (*set) (int), const char *buf, size_t count)
{
	int rv, value;

	rv = parse_arg(buf, count, &value);
	if (rv > 0)
		set(value);
	return rv;
}

static ssize_t show_sys_hwmon(int (*get) (void), char *buf)
{
	return sprintf(buf, "%d\n", get());
}

#define CREATE_SENSOR_ATTR(_name, _mode, _set, _get)		\
	static ssize_t show_##_name(struct device *dev,			\
				    struct device_attribute *attr,	\
				    char *buf)				\
	{								\
		return show_sys_hwmon(_set, buf);			\
	}								\
	static ssize_t store_##_name(struct device *dev,		\
				     struct device_attribute *attr,	\
				     const char *buf, size_t count)	\
	{								\
		return store_sys_hwmon(_get, buf, count);		\
	}								\
	static SENSOR_DEVICE_ATTR(_name, _mode, show_##_name, store_##_name, 0);

CREATE_SENSOR_ATTR(fan1_input, S_IRUGO, get_fan_rpm, NULL);
CREATE_SENSOR_ATTR(pwm1, S_IRUGO | S_IWUSR, get_fan_pwm, set_fan_pwm);
CREATE_SENSOR_ATTR(pwm1_enable, S_IRUGO | S_IWUSR, get_fan_pwm_enable,
		set_fan_pwm_enable);
CREATE_SENSOR_ATTR(temp1_input, S_IRUGO, get_cpu_temp, NULL);
CREATE_SENSOR_ATTR(temp1_max, S_IRUGO, get_cpu_temp_max, NULL);
CREATE_SENSOR_ATTR(temp2_input, S_IRUGO, get_battery_temp, NULL);
CREATE_SENSOR_ATTR(curr1_input, S_IRUGO, get_battery_current, NULL);
CREATE_SENSOR_ATTR(in1_input, S_IRUGO, get_battery_voltage, NULL);

static ssize_t
show_name(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "yeeloong\n");
}

static SENSOR_DEVICE_ATTR(name, S_IRUGO, show_name, NULL, 0);

static struct attribute *hwmon_attributes[] = {
	&sensor_dev_attr_pwm1.dev_attr.attr,
	&sensor_dev_attr_pwm1_enable.dev_attr.attr,
	&sensor_dev_attr_fan1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_max.dev_attr.attr,
	&sensor_dev_attr_temp2_input.dev_attr.attr,
	&sensor_dev_attr_curr1_input.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_name.dev_attr.attr,
	NULL
};

static struct attribute_group hwmon_attribute_group = {
	.attrs = hwmon_attributes
};

struct device *yeeloong_hwmon_dev;

static int yeeloong_hwmon_init(struct device *dev)
{
	int ret;

	yeeloong_hwmon_dev = hwmon_device_register(dev);
	if (IS_ERR(yeeloong_hwmon_dev)) {
		printk(KERN_INFO "unable to register yeeloong hwmon device\n");
		yeeloong_hwmon_dev = NULL;
		return PTR_ERR(yeeloong_hwmon_dev);
	}
	ret = sysfs_create_group(&yeeloong_hwmon_dev->kobj,
				 &hwmon_attribute_group);
	if (ret) {
		sysfs_remove_group(&yeeloong_hwmon_dev->kobj,
				   &hwmon_attribute_group);
		hwmon_device_unregister(yeeloong_hwmon_dev);
		yeeloong_hwmon_dev = NULL;
	}
	/* ensure fan is set to auto mode */
	set_fan_pwm_enable(2);

	return 0;
}

static void yeeloong_hwmon_exit(void)
{
	if (yeeloong_hwmon_dev) {
		sysfs_remove_group(&yeeloong_hwmon_dev->kobj,
				   &hwmon_attribute_group);
		hwmon_device_unregister(yeeloong_hwmon_dev);
		yeeloong_hwmon_dev = NULL;
	}
}

/* video output subdriver */

static int lcd_video_output_get(struct output_device *od)
{
	return ec_read(REG_DISPLAY_LCD);
}

static int lcd_video_output_set(struct output_device *od)
{
	unsigned long status = od->request_state;
	int value;

	if (status == BIT_DISPLAY_LCD_ON) {
		/* open LCD */
		outb(0x31, 0x3c4);
		value = inb(0x3c5);
		value = (value & 0xf8) | 0x03;
		outb(0x31, 0x3c4);
		outb(value, 0x3c5);
		/* open backlight */
		ec_write(REG_BACKLIGHT_CTRL, BIT_BACKLIGHT_ON);
	} else {
		/* close backlight */
		ec_write(REG_BACKLIGHT_CTRL, BIT_BACKLIGHT_OFF);
		/* close LCD */
		outb(0x31, 0x3c4);
		value = inb(0x3c5);
		value = (value & 0xf8) | 0x02;
		outb(0x31, 0x3c4);
		outb(value, 0x3c5);
	}

	return 0;
}

static struct output_properties lcd_output_properties = {
	.set_state = lcd_video_output_set,
	.get_status = lcd_video_output_get,
};

static int crt_video_output_get(struct output_device *od)
{
	return ec_read(REG_CRT_DETECT);
}

static int crt_video_output_set(struct output_device *od)
{
	unsigned long status = od->request_state;
	int value;

	if (status == BIT_CRT_DETECT_PLUG) {
		if (ec_read(REG_CRT_DETECT) == BIT_CRT_DETECT_PLUG) {
			/* open CRT */
			outb(0x21, 0x3c4);
			value = inb(0x3c5);
			value &= ~(1 << 7);
			outb(0x21, 0x3c4);
			outb(value, 0x3c5);
		}
	} else {
		/* close CRT */
		outb(0x21, 0x3c4);
		value = inb(0x3c5);
		value |= (1 << 7);
		outb(0x21, 0x3c4);
		outb(value, 0x3c5);
	}

	return 0;
}

static struct output_properties crt_output_properties = {
	.set_state = crt_video_output_set,
	.get_status = crt_video_output_get,
};

struct output_device *lcd_output_dev, *crt_output_dev;

static void lcd_vo_set(int status)
{
	if (ec_ver_small_than("EC_VER=PQ1D27")) {
		lcd_output_dev->request_state = status;
		lcd_video_output_set(lcd_output_dev);
	}
}

static void crt_vo_set(int status)
{
	crt_output_dev->request_state = status;
	crt_video_output_set(crt_output_dev);
}

static int crt_detect_handler(int status)
{
	if (status == BIT_CRT_DETECT_PLUG) {
		crt_vo_set(BIT_CRT_DETECT_PLUG);
		lcd_vo_set(BIT_DISPLAY_LCD_OFF);
	} else {
		lcd_vo_set(BIT_DISPLAY_LCD_ON);
		crt_vo_set(BIT_CRT_DETECT_UNPLUG);
	}
	return status;
}

static int black_screen_handler(int status)
{
	if (ec_ver_small_than("EC_VER=PQ1D26"))
		lcd_vo_set(status);

	return status;
}

static int display_toggle_handler(int status)
{
	static int video_output_status;

	/* only enable switch video output button
	 * when CRT is connected */
	if (ec_read(REG_CRT_DETECT) == BIT_CRT_DETECT_UNPLUG)
		return 0;
	/* 0. no CRT connected: LCD on, CRT off
	 * 1. BOTH on
	 * 2. LCD off, CRT on
	 * 3. BOTH off
	 * 4. LCD on, CRT off
	 */
	video_output_status++;
	if (video_output_status > 4)
		video_output_status = 1;

	switch (video_output_status) {
	case 1:
		lcd_vo_set(BIT_DISPLAY_LCD_ON);
		crt_vo_set(BIT_CRT_DETECT_PLUG);
		break;
	case 2:
		lcd_vo_set(BIT_DISPLAY_LCD_OFF);
		crt_vo_set(BIT_CRT_DETECT_PLUG);
		break;
	case 3:
		lcd_vo_set(BIT_DISPLAY_LCD_OFF);
		crt_vo_set(BIT_CRT_DETECT_UNPLUG);
		break;
	case 4:
		lcd_vo_set(BIT_DISPLAY_LCD_ON);
		crt_vo_set(BIT_CRT_DETECT_UNPLUG);
		break;
	default:
		/* ensure LCD is on */
		lcd_vo_set(BIT_DISPLAY_LCD_ON);
		break;
	}
	return video_output_status;
}

static int yeeloong_vo_init(struct device *dev)
{
	int ret;

	/* register video output device: lcd, crt */
	lcd_output_dev = video_output_register("LCD", dev, NULL,
			&lcd_output_properties);

	if (IS_ERR(lcd_output_dev)) {
		ret = PTR_ERR(lcd_output_dev);
		lcd_output_dev = NULL;
		return ret;
	}
	/* ensure LCD is on by default */
	lcd_vo_set(1);

	crt_output_dev = video_output_register("CRT", dev, NULL,
			&crt_output_properties);

	if (IS_ERR(crt_output_dev)) {
		ret = PTR_ERR(crt_output_dev);
		crt_output_dev = NULL;
		return ret;
	}
	/* close CRT by default, and will be enabled
	 * when the CRT connectting event reported by SCI */
	crt_vo_set(0);

	/* install event handlers */
	yeeloong_install_sci_handler(EVENT_CRT_DETECT, crt_detect_handler);
	yeeloong_install_sci_handler(EVENT_BLACK_SCREEN, black_screen_handler);
	yeeloong_install_sci_handler(EVENT_DISPLAY_TOGGLE,
			display_toggle_handler);
	return 0;
}

static void yeeloong_vo_exit(void)
{
	/* uninstall event handlers */
	yeeloong_uninstall_sci_handler(EVENT_CRT_DETECT, crt_detect_handler);
	yeeloong_uninstall_sci_handler(EVENT_BLACK_SCREEN,
			black_screen_handler);
	yeeloong_uninstall_sci_handler(EVENT_DISPLAY_TOGGLE,
			display_toggle_handler);

	if (lcd_output_dev) {
		video_output_unregister(lcd_output_dev);
		lcd_output_dev = NULL;
	}
	if (crt_output_dev) {
		video_output_unregister(crt_output_dev);
		crt_output_dev = NULL;
	}
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

	*state = yeeloong_get_brightness(bd);

	return 0;
}

static int video_set_cur_state(struct thermal_cooling_device *cdev, unsigned
			       long state)
{
	static struct backlight_device *bd;

	bd = (struct backlight_device *)cdev->devdata;

	yeeloong_backlight_dev->props.brightness = state;
	backlight_update_status(bd);

	return 0;
}

static struct thermal_cooling_device_ops video_cooling_ops = {
	.get_max_state = video_get_max_state,
	.get_cur_state = video_get_cur_state,
	.set_cur_state = video_set_cur_state,
};

static struct thermal_cooling_device *yeeloong_thermal_cdev;

/* TODO: register fan, cpu as the cooling devices */
static int yeeloong_thermal_init(struct device *dev)
{
	int ret;

	if (!dev)
		return -1;

	yeeloong_thermal_cdev = thermal_cooling_device_register("LCD", dev,
			&video_cooling_ops);

	if (IS_ERR(yeeloong_thermal_cdev)) {
		ret = PTR_ERR(yeeloong_thermal_cdev);
		return ret;
	}

	ret = sysfs_create_link(&dev->kobj,
				&yeeloong_thermal_cdev->device.kobj,
				"thermal_cooling");
	if (ret) {
		printk(KERN_ERR "Create sysfs link\n");
		return ret;
	}
	ret = sysfs_create_link(&yeeloong_thermal_cdev->device.kobj,
				&dev->kobj, "device");
	if (ret) {
		printk(KERN_ERR "Create sysfs link\n");
		return ret;
	}

	return 0;
}

static void yeeloong_thermal_exit(struct device *dev)
{
	if (yeeloong_thermal_cdev) {
		if (dev)
			sysfs_remove_link(&dev->kobj, "thermal_cooling");
		sysfs_remove_link(&yeeloong_thermal_cdev->device.kobj,
				  "device");
		thermal_cooling_device_unregister(yeeloong_thermal_cdev);
		yeeloong_thermal_cdev = NULL;
	}
}

/* hotkey input subdriver */

static struct input_dev *yeeloong_hotkey_dev;
static int event, status;

struct key_entry {
	char type;		/* See KE_* below */
	int event;		/* event from SCI */
	u16 keycode;		/* KEY_* or SW_* */
};

enum { KE_KEY, KE_SW, KE_END };

static struct key_entry yeeloong_keymap[] = {
	{KE_SW, EVENT_LID, SW_LID},
	/*{KE_KEY, EVENT_AC_BAT, KEY_BATTERY},*/
	{KE_KEY, EVENT_CAMERA, KEY_CAMERA},	/* Fn + ESC */
	{KE_KEY, EVENT_SLEEP, KEY_SLEEP},	/* Fn + F1 */
	{KE_KEY, EVENT_DISPLAY_TOGGLE, KEY_SWITCHVIDEOMODE},	/* Fn + F3 */
	{KE_KEY, EVENT_AUDIO_MUTE, KEY_MUTE},	/* Fn + F4 */
	{KE_KEY, EVENT_WLAN, KEY_WLAN},	/* Fn + F5 */
	{KE_KEY, EVENT_DISPLAY_BRIGHTNESS, KEY_BRIGHTNESSUP},	/* Fn + up */
	{KE_KEY, EVENT_DISPLAY_BRIGHTNESS, KEY_BRIGHTNESSDOWN},	/* Fn + down */
	{KE_KEY, EVENT_AUDIO_VOLUME, KEY_VOLUMEUP},	/* Fn + right */
	{KE_KEY, EVENT_AUDIO_VOLUME, KEY_VOLUMEDOWN},	/* Fn + left */
	{KE_END, 0, KEY_UNKNOWN}
};

static int yeeloong_lid_update_status(int status)
{
	input_report_switch(yeeloong_hotkey_dev, SW_LID, !status);
	input_sync(yeeloong_hotkey_dev);

	return status;
}

static void yeeloong_hotkey_update_status(int key)
{
	input_report_key(yeeloong_hotkey_dev, key, 1);
	input_sync(yeeloong_hotkey_dev);
	input_report_key(yeeloong_hotkey_dev, key, 0);
	input_sync(yeeloong_hotkey_dev);
}

static int get_event_keycode(void)
{
	struct key_entry *key;

	for (key = yeeloong_keymap; key->type != KE_END; key++) {
		if (key->event != event)
			continue;
		else {
			if (EVENT_DISPLAY_BRIGHTNESS == event) {
				static int old_brightness_status = -1;
				/* current status > old one, means up */
				if ((status < old_brightness_status)
				    || (0 == status))
					key++;
				old_brightness_status = status;
			} else if (EVENT_AUDIO_VOLUME == event) {
				static int old_volume_status = -1;
				if ((status < old_volume_status)
				    || (0 == status))
					key++;
				old_volume_status = status;
			}
			break;
		}
	}
	return key->keycode;
}

void yeeloong_report_key(void)
{
	int keycode;

	keycode = get_event_keycode();
	if (keycode == KEY_UNKNOWN)
		return;

	if (keycode == SW_LID)
		yeeloong_lid_update_status(status);
	else
		yeeloong_hotkey_update_status(keycode);
}

enum { NO_REG, MUL_REG, REG_END };

int event_reg[15] = {
	REG_LID_DETECT,		/*  LID open/close */
	NO_REG,			/*  Fn+F3 for display switch */
	NO_REG,			/*  Fn+F1 for entering sleep mode */
	MUL_REG,		/*  Over-temperature happened */
	REG_CRT_DETECT,		/*  CRT is connected */
	REG_CAMERA_STATUS,	/*  Camera on/off */
	REG_USB2_FLAG,		/*  USB2 Over Current occurred */
	REG_USB0_FLAG,		/*  USB0 Over Current occurred */
	REG_DISPLAY_LCD,	/*  Black screen on/off */
	REG_AUDIO_MUTE,		/*  Mute on/off */
	REG_DISPLAY_BRIGHTNESS,	/*  LCD backlight brightness adjust */
	NO_REG,			/*  AC & Battery relative issue */
	REG_AUDIO_VOLUME,	/*  Volume adjust */
	REG_WLAN,		/*  Wlan on/off */
	REG_END
};

static int ec_get_event_status(void)
{
	int reg;

	reg = event_reg[event - EVENT_LID];

	if (reg == NO_REG)
		return 1;
	else if (reg == MUL_REG) {
		if (event == EVENT_OVERTEMP) {
			return (ec_read(REG_BAT_CHARGE_STATUS) &
				BIT_BAT_CHARGE_STATUS_OVERTEMP) >> 2;
		}
	} else if (reg != REG_END)
		return ec_read(reg);

	return -1;
}

static sci_handler event_handler[15];

int yeeloong_install_sci_handler(int event, sci_handler handler)
{
	if (event_handler[event - EVENT_LID] != NULL) {
		printk(KERN_INFO "There is a handler installed for event: %d\n",
		       event);
		return -1;
	}
	event_handler[event - EVENT_LID] = handler;

	return 0;
}
EXPORT_SYMBOL(yeeloong_install_sci_handler);

int yeeloong_uninstall_sci_handler(int event, sci_handler handler)
{
	if (event_handler[event - EVENT_LID] == NULL) {
		printk(KERN_INFO "There is no handler installed for event: %d\n",
		       event);
		return -1;
	}
	if (event_handler[event - EVENT_LID] != handler) {
		printk(KERN_INFO "You can not uninstall the handler installed by others\n");
		return -1;
	}
	event_handler[event - EVENT_LID] = NULL;

	return 0;
}
EXPORT_SYMBOL(yeeloong_uninstall_sci_handler);

static void yeeloong_event_action(void)
{
	sci_handler handler;

	handler = event_handler[event - EVENT_LID];

	if (handler == NULL)
		return;

	if (event == EVENT_CAMERA)
		status = handler(3);
	else
		status = handler(status);
}

/*
 * SCI(system control interrupt) main interrupt routine
 *
 * we will do the query and get event number together so the interrupt routine
 * should be longer than 120us now at least 3ms elpase for it.
 */
static irqreturn_t sci_irq_handler(int irq, void *dev_id)
{
	int ret;

	if (SCI_IRQ_NUM != irq) {
		printk(KERN_ERR "%s: spurious irq.\n", __func__);
		return IRQ_NONE;
	}

	/* query the event number */
	ret = ec_query_event_num();
	if (ret < 0) {
		printk(KERN_ERR "%s: return: %d\n", __func__, ret);
		return IRQ_NONE;
	}

	event = ec_get_event_num();
	if (event < 0) {
		printk(KERN_ERR "%s: return: %d\n", __func__, event);
		return IRQ_NONE;
	}

	if ((event != 0x00) && (event != 0xff)) {
		/* get status of current event */
		status = ec_get_event_status();
		if (status == -1)
			return IRQ_NONE;
		/* execute corresponding actions */
		yeeloong_event_action();
		/* report current key */
		yeeloong_report_key();
	}
	return IRQ_HANDLED;
}

/*
 * config and init some msr and gpio register properly.
 */
static int sci_irq_init(void)
{
	u32 hi, lo;
	u32 gpio_base;
	int ret = 0;
	unsigned long flags;

	/* get gpio base */
	_rdmsr(DIVIL_MSR_REG(DIVIL_LBAR_GPIO), &hi, &lo);
	gpio_base = lo & 0xff00;

	/* filter the former kb3310 interrupt for security */
	ret = ec_query_event_num();
	if (ret) {
		printk(KERN_ERR "%s: failed.\n", __func__);
		return ret;
	}

	/* for filtering next number interrupt */
	udelay(10000);

	/* set gpio native registers and msrs for GPIO27 SCI EVENT PIN
	 * gpio :
	 *      input, pull-up, no-invert, event-count and value 0,
	 *      no-filter, no edge mode
	 *      gpio27 map to Virtual gpio0
	 * msr :
	 *      no primary and lpc
	 *      Unrestricted Z input to IG10 from Virtual gpio 0.
	 */
	local_irq_save(flags);
	_rdmsr(0x80000024, &hi, &lo);
	lo &= ~(1 << 10);
	_wrmsr(0x80000024, hi, lo);
	_rdmsr(0x80000025, &hi, &lo);
	lo &= ~(1 << 10);
	_wrmsr(0x80000025, hi, lo);
	_rdmsr(0x80000023, &hi, &lo);
	lo |= (0x0a << 0);
	_wrmsr(0x80000023, hi, lo);
	local_irq_restore(flags);

	/* set gpio27 as sci interrupt
	 *
	 * input, pull-up, no-fliter, no-negedge, invert
	 * the sci event is just about 120us
	 */
	asm(".set noreorder\n");
	/*  input enable */
	outl(0x00000800, (gpio_base | 0xA0));
	/*  revert the input */
	outl(0x00000800, (gpio_base | 0xA4));
	/*  event-int enable */
	outl(0x00000800, (gpio_base | 0xB8));
	asm(".set reorder\n");

	return 0;
}

struct irqaction sci_irqaction = {
	.handler = sci_irq_handler,
	.name = "sci",
	.flags = IRQF_SHARED,
};

static int setup_sci(void)
{
	sci_irq_init();

	setup_irq(SCI_IRQ_NUM, &sci_irqaction);

	return 0;
}

static int camera_set(int status)
{
	int value;
	static int camera_status;

	if (status == 2)
		/* resume the old camera status */
		camera_set(camera_status);
	else if (status == 3) {
		/* revert the camera status */
		value = ec_read(REG_CAMERA_CONTROL);
		ec_write(REG_CAMERA_CONTROL, value | (1 << 1));
	} else {/* status == 0 or status == 1 */
		status = !!status;
		camera_status = ec_read(REG_CAMERA_STATUS);
		if (status != camera_status)
			camera_set(3);
	}
	return ec_read(REG_CAMERA_STATUS);
}

static void yeeloong_hotkey_exit(void)
{
	/* free irq */
	remove_irq(SCI_IRQ_NUM, &sci_irqaction);

#ifdef CONFIG_SUSPEND
	/* uninstall the real yeeloong_report_lid_status for pm.c */
	yeeloong_report_lid_status = NULL;
#endif
	/* uninstall event handler */
	yeeloong_uninstall_sci_handler(EVENT_CAMERA, camera_set);

	if (yeeloong_hotkey_dev) {
		input_unregister_device(yeeloong_hotkey_dev);
		yeeloong_hotkey_dev = NULL;
	}
}

static int yeeloong_hotkey_init(struct device *dev)
{
	int ret;
	struct key_entry *key;

	/* setup the system control interface */
	ret = setup_sci();
	if (ret)
		return -EFAULT;

	yeeloong_hotkey_dev = input_allocate_device();

	if (!yeeloong_hotkey_dev) {
		yeeloong_hotkey_exit();
		return -ENOMEM;
	}

	yeeloong_hotkey_dev->name = "HotKeys";
	yeeloong_hotkey_dev->phys = "button/input0";
	yeeloong_hotkey_dev->id.bustype = BUS_HOST;
	yeeloong_hotkey_dev->dev.parent = dev;

	for (key = yeeloong_keymap; key->type != KE_END; key++) {
		switch (key->type) {
		case KE_KEY:
			set_bit(EV_KEY, yeeloong_hotkey_dev->evbit);
			set_bit(key->keycode, yeeloong_hotkey_dev->keybit);
			break;
		case KE_SW:
			set_bit(EV_SW, yeeloong_hotkey_dev->evbit);
			set_bit(key->keycode, yeeloong_hotkey_dev->swbit);
			break;
		}
	}

	ret = input_register_device(yeeloong_hotkey_dev);
	if (ret) {
		input_free_device(yeeloong_hotkey_dev);
		return ret;
	}

	if (ret) {
		input_unregister_device(yeeloong_hotkey_dev);
		yeeloong_hotkey_dev = NULL;
	}
	/* update the current status of lid */
	yeeloong_lid_update_status(BIT_LID_DETECT_ON);

#ifdef CONFIG_SUSPEND
	/* install the real yeeloong_report_lid_status for pm.c */
	yeeloong_report_lid_status = yeeloong_lid_update_status;
#endif
	/* install event handler */
	yeeloong_install_sci_handler(EVENT_CAMERA, camera_set);

	return 0;
}

/* battery subdriver: APM emulated support */

static void get_fixed_battery_info(void)
{
	int design_cap, full_charged_cap, design_vol, vendor, cell_count;

	design_cap = (ec_read(REG_BAT_DESIGN_CAP_HIGH) << 8)
	    | ec_read(REG_BAT_DESIGN_CAP_LOW);
	full_charged_cap = (ec_read(REG_BAT_FULLCHG_CAP_HIGH) << 8)
	    | ec_read(REG_BAT_FULLCHG_CAP_LOW);
	design_vol = (ec_read(REG_BAT_DESIGN_VOL_HIGH) << 8)
	    | ec_read(REG_BAT_DESIGN_VOL_LOW);
	vendor = ec_read(REG_BAT_VENDOR);
	cell_count = ec_read(REG_BAT_CELL_COUNT);

	if (vendor != 0) {
		printk(KERN_INFO
		       "battery vendor(%s), cells count(%d), "
		       "with designed capacity(%d),designed voltage(%d),"
		       " full charged capacity(%d)\n",
		       (vendor ==
			FLAG_BAT_VENDOR_SANYO) ? "SANYO" : "SIMPLO",
		       (cell_count == FLAG_BAT_CELL_3S1P) ? 3 : 6,
		       design_cap, design_vol,
		       full_charged_cap);
	}
}

#define APM_CRITICAL		5

static void __maybe_unused yeeloong_apm_get_power_status(struct apm_power_info
		*info)
{
	unsigned char bat_status;

	info->battery_status = APM_BATTERY_STATUS_UNKNOWN;
	info->battery_flag = APM_BATTERY_FLAG_UNKNOWN;
	info->units = APM_UNITS_MINS;

	info->battery_life = (ec_read(REG_BAT_RELATIVE_CAP_HIGH) << 8) |
		(ec_read(REG_BAT_RELATIVE_CAP_LOW));

	info->ac_line_status = (ec_read(REG_BAT_POWER) & BIT_BAT_POWER_ACIN) ?
		APM_AC_ONLINE : APM_AC_OFFLINE;

	bat_status = ec_read(REG_BAT_STATUS);

	if (!(bat_status & BIT_BAT_STATUS_IN)) {
		/* no battery inserted */
		info->battery_status = APM_BATTERY_STATUS_NOT_PRESENT;
		info->battery_flag = APM_BATTERY_FLAG_NOT_PRESENT;
		info->time = 0x00;
		return;
	}

	/* adapter inserted */
	if (info->ac_line_status == APM_AC_ONLINE) {
		if (!(bat_status & BIT_BAT_STATUS_FULL)) {
			/* battery is not fully charged */
			info->battery_status = APM_BATTERY_STATUS_CHARGING;
			info->battery_flag = APM_BATTERY_FLAG_CHARGING;
		} else {
			/* battery is fully charged */
			info->battery_status = APM_BATTERY_STATUS_HIGH;
			info->battery_flag = APM_BATTERY_FLAG_HIGH;
			info->battery_life = 100;
		}
	} else {
		/* battery is too low */
		if (bat_status & BIT_BAT_STATUS_LOW) {
			info->battery_status = APM_BATTERY_STATUS_LOW;
			info->battery_flag = APM_BATTERY_FLAG_LOW;
			if (info->battery_life <= APM_CRITICAL) {
				/* we should power off the system now */
				info->battery_status =
					APM_BATTERY_STATUS_CRITICAL;
				info->battery_flag = APM_BATTERY_FLAG_CRITICAL;
			}
		} else {
			/* assume the battery is high enough. */
			info->battery_status = APM_BATTERY_STATUS_HIGH;
			info->battery_flag = APM_BATTERY_FLAG_HIGH;
		}
	}
	info->time = ((info->battery_life - 3) * 54 + 142) / 60;
}

static int yeeloong_apm_init(void)
{
	/* print fixed information of battery */
	get_fixed_battery_info();

#ifdef CONFIG_APM_EMULATION
	apm_get_power_status = yeeloong_apm_get_power_status;
#endif
	return 0;
}

static void yeeloong_apm_exit(void)
{
#ifdef CONFIG_APM_EMULATION
	if (apm_get_power_status == yeeloong_apm_get_power_status)
		apm_get_power_status = NULL;
#endif
}

/* platform subdriver */
static void __maybe_unused usb_ports_set(int status)
{
	status = !!status;

	ec_write(REG_USB0_FLAG, status);
	ec_write(REG_USB1_FLAG, status);
	ec_write(REG_USB2_FLAG, status);
}

static int __maybe_unused yeeloong_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	printk(KERN_INFO "yeeloong specific suspend\n");

	/* close LCD */
	lcd_vo_set(BIT_DISPLAY_LCD_OFF);
	/* close CRT */
	crt_vo_set(BIT_CRT_DETECT_UNPLUG);
	/* power off camera */
	camera_set(BIT_CAMERA_CONTROL_OFF);
	/* poweroff three usb ports */
	usb_ports_set(BIT_USB_FLAG_OFF);
	/* minimize the speed of FAN */
	set_fan_pwm_enable(1);
	set_fan_pwm(1);

	return 0;
}

static int __maybe_unused yeeloong_resume(struct platform_device *pdev)
{
	printk(KERN_INFO "yeeloong specific resume\n");

	/* resume the status of lcd & crt */
	lcd_vo_set(BIT_DISPLAY_LCD_ON);
	crt_vo_set(BIT_CRT_DETECT_PLUG);

	/* power on three usb ports */
	usb_ports_set(BIT_USB_FLAG_ON);

	/* resume the camera status */
	camera_set(2);

	/* resume fan to auto mode */
	set_fan_pwm_enable(2);

	return 0;
}

static struct platform_device *yeeloong_pdev;

static int __devinit yeeloong_probe(struct platform_device *dev)
{
	yeeloong_pdev = dev;

	return 0;
}

static struct platform_device_id platform_device_ids[] = {
	{
		.name = "yeeloong_laptop",
	},
	{}
};

MODULE_DEVICE_TABLE(platform, platform_device_ids);

static struct platform_driver platform_driver = {
	.probe = yeeloong_probe,
	.driver = {
		   .name = "yeeloong_laptop",
		   .owner = THIS_MODULE,
		   },
	.id_table = platform_device_ids,
#ifdef CONFIG_PM
	.suspend = yeeloong_suspend,
	.resume = yeeloong_resume,
#endif
};

static int yeeloong_pdev_init(void)
{
	int ret;

	/* Register platform stuff */
	ret = platform_driver_register(&platform_driver);
	if (ret)
		return ret;

	return 0;
}

static void yeeloong_pdev_exit(void)
{
	platform_driver_unregister(&platform_driver);
}

static int __init yeeloong_init(void)
{
	int ret;

	if (mips_machtype != MACH_LEMOTE_YL2F89) {
		printk(KERN_INFO "This Driver is for YeeLoong netbook, You"
				" can not use it on the other Machines\n");
		return -EFAULT;
	}

	printk(KERN_INFO "Load YeeLoong Platform Driver %s.\n",
			DRIVER_VERSION);

	ret = yeeloong_pdev_init();
	if (ret) {
		yeeloong_pdev_exit();
		printk(KERN_INFO "Fail to init yeeloong platform driver.\n");
		return ret;
	}
	ret = yeeloong_hotkey_init(&yeeloong_pdev->dev);
	if (ret) {
		yeeloong_hotkey_exit();
		printk(KERN_INFO "Fail init yeeloong hotkey driver.\n");
		return ret;
	}
	ret = yeeloong_apm_init();
	if (ret) {
		yeeloong_apm_exit();
		printk(KERN_INFO "Fail to init yeeloong apm driver.\n");
		return ret;
	}
	ret = yeeloong_backlight_init(&yeeloong_pdev->dev);
	if (ret) {
		yeeloong_backlight_exit();
		printk(KERN_INFO "Fail to init yeeloong backlight driver.\n");
		return ret;
	}
	ret = yeeloong_thermal_init(&yeeloong_backlight_dev->dev);
	if (ret) {
		yeeloong_thermal_exit(&yeeloong_backlight_dev->dev);
		printk(KERN_INFO
		       "Fail to init yeeloong thermal cooling device.\n");
		return ret;
	}
	ret = yeeloong_hwmon_init(&yeeloong_pdev->dev);
	if (ret) {
		yeeloong_hwmon_exit();
		printk(KERN_INFO "Fail to init yeeloong hwmon driver.\n");
		return ret;
	}
	ret = yeeloong_vo_init(&yeeloong_pdev->dev);
	if (ret) {
		yeeloong_vo_exit();
		printk(KERN_INFO "Fail to init yeeloong video output driver.\n");
		return ret;
	}
	return 0;
}

static void __exit yeeloong_exit(void)
{
	yeeloong_vo_exit();
	yeeloong_hwmon_exit();
	yeeloong_thermal_exit(&yeeloong_backlight_dev->dev);
	yeeloong_backlight_exit();
	yeeloong_apm_exit();
	yeeloong_hotkey_exit();
	yeeloong_pdev_exit();

	printk(KERN_INFO "Unload YeeLoong Platform Driver %s\n",
			DRIVER_VERSION);
}

module_init(yeeloong_init);
module_exit(yeeloong_exit);

MODULE_AUTHOR("Wu Zhangjin <wuzj@lemote.com>");
MODULE_DESCRIPTION("YeeLoong laptop driver");
MODULE_LICENSE("GPL");
