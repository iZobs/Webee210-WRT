/*
 * include/plat/goodix_touch.h
 *
 * Copyright (C) 2008 Goodix, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __PLAT_GOODIX_TOUCH_H__
#define __PLAT_GOODIX_TOUCH_H__


struct goodix_i2c_platform_data {
	uint32_t gpio_irq;			// IRQ port
	uint32_t irq_cfg;			// IRQ port config
	uint32_t gpio_shutdown;		// Shutdown port number
	uint32_t shutdown_cfg;		// Shutdown port config
	uint32_t screen_width;		// screen width
	uint32_t screen_height;		// screen height
};

#endif	// __PLAT_GOODIX_TOUCH_H__

