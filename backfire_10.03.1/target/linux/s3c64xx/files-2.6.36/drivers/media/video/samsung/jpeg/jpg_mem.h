/* linux/drivers/media/video/samsung/jpeg/jpg_mem.h
 *
 * Driver header file for Samsung JPEG Encoder/Decoder
 *
 * Peter Oh, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __JPG_MEM_H__
#define __JPG_MEM_H__

#include "jpg_misc.h"

#include <linux/version.h>
#include <plat/media.h>

#define JPG_REG_BASE_ADDR			(0x78800000)
#define jpg_data_base_addr			(UINT32)s3c_get_media_memory(S3C_MDEV_JPEG)

#define MAX_JPG_WIDTH				1600
#define MAX_JPG_HEIGHT				1200

#define MAX_JPG_THUMBNAIL_WIDTH		320
#define MAX_JPG_THUMBNAIL_HEIGHT	240

#define JPG_STREAM_BUF_SIZE			(MAX_JPG_WIDTH * MAX_JPG_HEIGHT)
#define JPG_STREAM_THUMB_BUF_SIZE	(MAX_JPG_THUMBNAIL_WIDTH * MAX_JPG_THUMBNAIL_HEIGHT)
#define JPG_FRAME_BUF_SIZE			(MAX_JPG_WIDTH * MAX_JPG_HEIGHT * 3)
#define JPG_FRAME_THUMB_BUF_SIZE	(MAX_JPG_THUMBNAIL_WIDTH * MAX_JPG_THUMBNAIL_HEIGHT * 3)

#define JPG_TOTAL_BUF_SIZE			(JPG_STREAM_BUF_SIZE + JPG_STREAM_THUMB_BUF_SIZE \
									+ JPG_FRAME_BUF_SIZE + JPG_FRAME_THUMB_BUF_SIZE)

#define COEF1_RGB_2_YUV         0x4d971e
#define COEF2_RGB_2_YUV         0x2c5783
#define COEF3_RGB_2_YUV         0x836e13

#define ENABLE_MOTION_ENC       (0x1<<3)
#define DISABLE_MOTION_ENC      (0x0<<3)

#define ENABLE_MOTION_DEC       (0x1<<0)
#define DISABLE_MOTION_DEC      (0x0<<0)

#define ENABLE_HW_DEC           (0x1<<2)
#define DISABLE_HW_DEC          (0x0<<2)

#define INCREMENTAL_DEC         (0x1<<3)
#define NORMAL_DEC              (0x0<<3)
#define YCBCR_MEMORY			(0x1<<5)

#define	ENABLE_IRQ				(0xf<<3)

typedef struct tagS3C6400_JPG_HOSTIF_REG
{
	UINT32		JPGMod;			//0x000
	UINT32		JPGStatus;		//0x004
	UINT32		JPGQTblNo;		//0x008
	UINT32		JPGRSTPos;		//0x00C
	UINT32		JPGY;			//0x010
	UINT32		JPGX;			//0x014
	UINT32		JPGDataSize;	//0x018
	UINT32		JPGIRQ;			//0x01C
	UINT32		JPGIRQStatus;	//0x020
	UINT32		dummy0[247];

	UINT32		JQTBL0[64];		//0x400
	UINT32		JQTBL1[64];		//0x500
	UINT32		JQTBL2[64];		//0x600
	UINT32		JQTBL3[64];		//0x700
	UINT32		JHDCTBL0[16];	//0x800
	UINT32		JHDCTBLG0[12];	//0x840
	UINT32		dummy1[4];
	UINT32		JHACTBL0[16];	//0x880
	UINT32		JHACTBLG0[162];	//0x8c0
	UINT32		dummy2[46];
	UINT32		JHDCTBL1[16];	//0xc00
	UINT32		JHDCTBLG1[12];	//0xc40
	UINT32		dummy3[4];
	UINT32		JHACTBL1[16];	//0xc80
	UINT32		JHACTBLG1[162];	//0xcc0
	UINT32		dummy4[46];

	UINT32		JPGYUVAddr0;	//0x1000
	UINT32		JPGYUVAddr1;	//0x1004
	UINT32		JPGFileAddr0;	//0x1008
	UINT32		JPGFileAddr1;	//0x100c
	UINT32		JPGStart;		//0x1010
	UINT32		JPGReStart;		//0x1014
	UINT32		JPGSoftReset;	//0x1018
	UINT32		JPGCntl;		//0x101c
	UINT32		JPGCOEF1;		//0x1020
	UINT32		JPGCOEF2;		//0x1024
	UINT32		JPGCOEF3;		//0x1028
	UINT32		JPGMISC;		//0x102c
	UINT32		JPGFrameIntv;	//0x1030
}S3C6400_JPG_HOSTIF_REG;

typedef struct tags3c6400_jpg_ctx
{
	volatile S3C6400_JPG_HOSTIF_REG	*v_pJPG_REG;
	volatile UINT8					*v_pJPGData_Buff;
	int								callerProcess;
	unsigned char					*strUserBuf;
	unsigned char					*frmUserBuf;
	unsigned char					*strUserThumbBuf;
	unsigned char					*frmUserThumbBuf;
}s3c6400_jpg_ctx;

//extern UINT32 jpg_data_base_addr;

void *phy_to_vir_addr(UINT32 phy_addr, int mem_size);
BOOL jpeg_mem_mapping(s3c6400_jpg_ctx *base);
void jpg_mem_free(s3c6400_jpg_ctx *base);
BOOL jpg_buff_mapping(s3c6400_jpg_ctx *base);
void jpg_buff_free(s3c6400_jpg_ctx *base);
BOOL HWPostMemMapping(void);
void HWPostMemFree(void);
void *mem_move(void *dst, const void *src, unsigned int size);
void *mem_alloc(unsigned int size);
#endif

