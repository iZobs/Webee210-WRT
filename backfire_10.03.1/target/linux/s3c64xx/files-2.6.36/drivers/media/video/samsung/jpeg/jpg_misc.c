/* linux/drivers/media/video/samsung/jpeg/jpg_misc.c
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
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/version.h>
#include <mach/regs-lcd.h>	

#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/wait.h>

#include "jpg_misc.h"
#include "jpg_mem.h"

static HANDLE hMutex	= NULL;
extern wait_queue_head_t	WaitQueue_JPEG;

/*----------------------------------------------------------------------------
*Function: create_jpg_mutex
*Implementation Notes: Create Mutex handle 
-----------------------------------------------------------------------------*/
HANDLE create_jpg_mutex(void)
{
	hMutex = (HANDLE)kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (hMutex == NULL)
		return NULL;
	
	mutex_init(hMutex);
	
	return hMutex;
}

/*----------------------------------------------------------------------------
*Function: lock_jpg_mutex
*Implementation Notes: lock mutex 
-----------------------------------------------------------------------------*/
DWORD lock_jpg_mutex(void)
{
    mutex_lock(hMutex);  
    return 1;
}

/*----------------------------------------------------------------------------
*Function: unlock_jpg_mutex
*Implementation Notes: unlock mutex
-----------------------------------------------------------------------------*/
DWORD unlock_jpg_mutex(void)
{
	mutex_unlock(hMutex);
	
    return 1;
}

/*----------------------------------------------------------------------------
*Function: delete_jpg_mutex
*Implementation Notes: delete mutex handle 
-----------------------------------------------------------------------------*/
void delete_jpg_mutex(void)
{
	if (hMutex == NULL)
		return;

	mutex_destroy(hMutex);
}

unsigned int get_fb0_addr(void)
{
	return readl(S3C_VIDW00ADD0B0);
}

void get_lcd_size(int *width, int *height)
{
	unsigned int	tmp;
	
	tmp		= readl(S3C_VIDTCON2);
	*height	= ((tmp >> 11) & 0x7FF) + 1;
	*width	= (tmp & 0x7FF) + 1;
}

void wait_for_interrupt(void)
{
	interruptible_sleep_on_timeout(&WaitQueue_JPEG, 100);
}

