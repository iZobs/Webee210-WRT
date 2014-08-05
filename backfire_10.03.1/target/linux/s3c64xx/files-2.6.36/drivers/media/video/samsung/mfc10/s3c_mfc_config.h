/* linux/driver/media/video/mfc/s3c_mfc_config.h
 *
 * Header file for Samsung MFC (Multi Function Codec - FIMV) driver 
 *
 * PyoungJae Jung, Jiun Yu, Copyright (c) 2009 Samsung Electronics 
 * http://www.samsungsemi.com/ 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _S3C_MFC_CONFIG_H
#define _S3C_MFC_CONFIG_H

#include <asm/types.h>

/* Physical Base Address for the MFC Host I/F Registers */
/* #define S3C6400_BASEADDR_MFC_SFR			0x7e002000 */
/* #define MFC_RESERVED_MEM_START			0x57800000 */

#ifndef S3C_MFC_PHYS_BUFFER_SET
extern dma_addr_t s3c_mfc_phys_buffer;
#endif

#define MFC_RESERVED_MEM_START			s3c_mfc_phys_buffer

/* 
 * Physical Base Address for the MFC Shared Buffer 
 * Shared Buffer = {CODE_BUF, WORK_BUF, PARA_BUF}
 */
#define S3C_MFC_BASEADDR_BITPROC_BUF	MFC_RESERVED_MEM_START

/* Physical Base Address for the MFC Data Buffer 
 * Data Buffer = {STRM_BUF, FRME_BUF}
 */
#define S3C_MFC_BASEADDR_DATA_BUF		(MFC_RESERVED_MEM_START + 0x116000)

/* Physical Base Address for the MFC Host I/F Registers */
#define S3C_MFC_BASEADDR_POST_SFR			0x77000000


/* 
 * MFC BITPROC_BUF
 *
 * the following three buffers have fixed size
 * firware buffer is to download boot code and firmware code
 */
#define S3C_MFC_CODE_BUF_SIZE	81920	/* It is fixed depending on the MFC'S FIRMWARE CODE (refer to 'Prism_S_V133.h' file) */

/* working buffer is uded for video codec operations by MFC */
#define S3C_MFC_WORK_BUF_SIZE	1048576 /* 1024KB = 1024 * 1024 */


/* Parameter buffer is allocated to store yuv frame address of output frame buffer. */
#define S3C_MFC_PARA_BUF_SIZE	8192  /* Stores the base address of Y , Cb , Cr for each decoded frame */

#define S3C_MFC_BITPROC_BUF_SIZE			 \
				(S3C_MFC_CODE_BUF_SIZE + \
				 S3C_MFC_PARA_BUF_SIZE + \
				 S3C_MFC_WORK_BUF_SIZE)


/* 
 * MFC DATA_BUF
 */
#define S3C_MFC_NUM_INSTANCES_MAX	4//2	/* MFC Driver supports 4 instances MAX. */

/* 
 * Determine if 'Post Rotate Mode' is enabled.
 * If it is enabled, the memory size of SD YUV420(720x576x1.5 bytes) is required more.
 * In case of linux driver, reserved buffer size will be changed.
 */
	
#define S3C_MFC_ROTATE_ENABLE		0

/*
 * stream buffer size must be a multiple of 512bytes 
 * becasue minimun data transfer unit between stream buffer and internal bitstream handling block 
 * in MFC core is 512bytes
 */
#define S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE		(614400)


#define S3C_MFC_LINE_BUF_SIZE		(S3C_MFC_NUM_INSTANCES_MAX * S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE)

#define S3C_MFC_YUV_BUF_SIZE		(720*480*3*4)

#define S3C_MFC_STREAM_BUF_SIZE		(S3C_MFC_LINE_BUF_SIZE)
#define S3C_MFC_DATA_BUF_SIZE		(S3C_MFC_STREAM_BUF_SIZE + S3C_MFC_YUV_BUF_SIZE)

#endif /* _S3C_MFC_CONFIG_H */
