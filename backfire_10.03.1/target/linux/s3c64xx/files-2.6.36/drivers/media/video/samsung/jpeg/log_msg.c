/* linux/drivers/media/video/samsung/jpeg/jpg_msg.c
 *
 * Driver file for Samsung JPEG Encoder/Decoder
 *
 * Peter Oh, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdarg.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <asm/param.h>
#include <linux/delay.h>

#include "log_msg.h"

//#define DEBUG

static const LOG_LEVEL log_level = LOG_TRACE;

static const char *modulename = "JPEG_DRV";

static const char *level_str[] = {"TRACE", "WARNING", "ERROR"};

void log_msg(LOG_LEVEL level, const char *func_name, const char *msg, ...)
{
	
	char buf[256];
	va_list argptr;

	if (level < log_level)
		return;

	sprintf(buf, "[%s: %s] %s: ", modulename, level_str[level], func_name);

	va_start(argptr, msg);
	vsprintf(buf + strlen(buf), msg, argptr);

	if(level == LOG_TRACE){
	#ifdef DEBUG
		printk(buf);
	#endif
	} else {
		printk(buf);
	}
	
	va_end(argptr);
}

