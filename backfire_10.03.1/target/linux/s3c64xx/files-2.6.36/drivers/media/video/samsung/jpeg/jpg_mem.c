/* linux/drivers/media/video/samsung/jpeg/jpg_mem.c
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

#include <asm/io.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/types.h>

#include "jpg_mem.h"
#include "jpg_misc.h"
#include "log_msg.h"


/*----------------------------------------------------------------------------
*Function: phy_to_vir_addr

*Parameters: 		dwContext		:
*Return Value:		True/False
*Implementation Notes: memory mapping from physical addr to virtual addr 
-----------------------------------------------------------------------------*/
void *phy_to_vir_addr(UINT32 phy_addr, int mem_size)
{
	void	*reserved_mem;

printk("phy_addr = %x..mem_size = %x\n",phy_addr,mem_size);

	reserved_mem = (void *)ioremap( (unsigned long)phy_addr, (int)mem_size );		

	if (reserved_mem == NULL) {
		log_msg(LOG_ERROR, "phy_to_vir_addr", "DD::Phyical to virtual memory mapping was failed!\r\n");
		return NULL;
	}

	return reserved_mem;
}

/*----------------------------------------------------------------------------
*Function: jpeg_mem_mapping

*Parameters: 		dwContext		:
*Return Value:		True/False
*Implementation Notes: JPG register mapping from physical addr to virtual addr 
-----------------------------------------------------------------------------*/
BOOL jpeg_mem_mapping(s3c6400_jpg_ctx *base)
{
	// JPG HOST Register
	base->v_pJPG_REG = (volatile S3C6400_JPG_HOSTIF_REG *)phy_to_vir_addr(JPG_REG_BASE_ADDR, sizeof(S3C6400_JPG_HOSTIF_REG));
	if (base->v_pJPG_REG == NULL)
	{
		log_msg(LOG_ERROR, "jpeg_mem_mapping", "DD::v_pJPG_REG: VirtualAlloc failed!\r\n");
		return FALSE;
	}
	
	return TRUE;
}


void jpg_mem_free(s3c6400_jpg_ctx *base)
{
	iounmap((void *)base->v_pJPG_REG);
	base->v_pJPG_REG = NULL;
}

/*----------------------------------------------------------------------------
*Function: jpg_buff_mapping

*Parameters: 		dwContext		:
*Return Value:		True/False
*Implementation Notes: JPG Buffer mapping from physical addr to virtual addr 
-----------------------------------------------------------------------------*/
BOOL jpg_buff_mapping(s3c6400_jpg_ctx *base)
{    	
    	// JPG Data Buffer
	base->v_pJPGData_Buff = (UINT8 *)phy_to_vir_addr(jpg_data_base_addr, JPG_TOTAL_BUF_SIZE);

	if (base->v_pJPGData_Buff == NULL)
	{
		log_msg(LOG_ERROR, "jpg_buff_mapping", "DD::v_pJPGData_Buff: VirtualAlloc failed!\r\n");
		return FALSE;
	}

	return TRUE;
}

void jpg_buff_free(s3c6400_jpg_ctx *base)
{
	iounmap( (void *)base->v_pJPGData_Buff );
	base->v_pJPGData_Buff = NULL;
}

void *mem_move(void *dst, const void *src, unsigned int size)
{
	return memmove(dst, src, size);
}

void *mem_alloc(unsigned int size)
{
	void	*alloc_mem;

	alloc_mem = (void *)kmalloc((int)size, GFP_KERNEL);
	if (alloc_mem == NULL) {
		log_msg(LOG_ERROR, "Mem_Alloc", "memory allocation failed!\r\n");
		return NULL;
	}

	return alloc_mem;
}
