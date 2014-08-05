/* linux/drivers/media/video/samsung/tv20/ddc.h
 *
 * i2c ddc interface header file for Samsung TVOut driver
 *
 * Copyright (c) 2010 Samsung Electronics
 *	         http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#ifndef _HDMI_DDC_H_
#define _HDMI_DDC_H_

#include "s5p_tv.h"

int ddc_read(u8 subaddr, u8 *data, u16 len);
int ddc_write(u8 *data, u16 len);

#endif /* _HDMI_DDC_H_ */
