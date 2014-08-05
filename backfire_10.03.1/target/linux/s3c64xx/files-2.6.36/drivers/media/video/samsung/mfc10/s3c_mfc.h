/* linux/driver/media/video/mfc/s3c_mfc.h
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

#ifndef _S3C_MFC_H
#define _S3C_MFC_H

#include <linux/wait.h>
#include <linux/interrupt.h>

#define S3C_MFC_IOCTL_MFC_MPEG4_DEC_INIT		(0x00800001)
#define S3C_MFC_IOCTL_MFC_MPEG4_ENC_INIT		(0x00800002)
#define S3C_MFC_IOCTL_MFC_MPEG4_DEC_EXE			(0x00800003)
#define S3C_MFC_IOCTL_MFC_MPEG4_ENC_EXE			(0x00800004)

#define S3C_MFC_IOCTL_MFC_H264_DEC_INIT			(0x00800005)
#define S3C_MFC_IOCTL_MFC_H264_ENC_INIT			(0x00800006)
#define S3C_MFC_IOCTL_MFC_H264_DEC_EXE			(0x00800007)
#define S3C_MFC_IOCTL_MFC_H264_ENC_EXE			(0x00800008)

#define S3C_MFC_IOCTL_MFC_H263_DEC_INIT			(0x00800009)
#define S3C_MFC_IOCTL_MFC_H263_ENC_INIT			(0x0080000A)
#define S3C_MFC_IOCTL_MFC_H263_DEC_EXE			(0x0080000B)
#define S3C_MFC_IOCTL_MFC_H263_ENC_EXE			(0x0080000C)

#define S3C_MFC_IOCTL_MFC_VC1_DEC_INIT			(0x0080000D)
#define S3C_MFC_IOCTL_MFC_VC1_DEC_EXE			(0x0080000E)

#define S3C_MFC_IOCTL_MFC_GET_LINE_BUF_ADDR		(0x0080000F)
#define S3C_MFC_IOCTL_MFC_GET_RING_BUF_ADDR		(0x00800010)
#define S3C_MFC_IOCTL_MFC_GET_YUV_BUF_ADDR		(0x00800011)
#define S3C_MFC_IOCTL_MFC_GET_POST_BUF_ADDR		(0x00800012)
#define S3C_MFC_IOCTL_MFC_GET_PHY_FRAM_BUF_ADDR		(0x00800013)
#define S3C_MFC_IOCTL_MFC_GET_CONFIG			(0x00800016)
#define S3C_MFC_IOCTL_MFC_GET_MPEG4_ASP_PARAM		(0x00800017)

#define S3C_MFC_IOCTL_MFC_SET_H263_MULTIPLE_SLICE	(0x00800014)
#define S3C_MFC_IOCTL_MFC_SET_CONFIG			(0x00800015)

#define S3C_MFC_IOCTL_MFC_SET_DISP_CONFIG		(0x00800111)
#define S3C_MFC_IOCTL_MFC_GET_YUV_SIZE			(0x00800112)
#define S3C_MFC_IOCTL_MFC_SET_PP_DISP_SIZE		(0x00800113)
#define S3C_MFC_IOCTL_MFC_SET_DEC_INBUF_TYPE		(0x00800114)

#define S3C_MFC_IOCTL_VIRT_TO_PHYS			0x12345678

#if (defined(DIVX_ENABLE) && (DIVX_ENABLE == 1))
#define S3C_MFC_IOCTL_CACHE_FLUSH_B_YUV			(0x00800115)
#define S3C_MFC_IOCTL_MFC_GET_PHY_B_YUV_BUF_ADDR	(0x00800116)
#define S3C_MFC_IOCTL_MFC_GET_B_YUV_BUF_ADDR		(0x00800117)
#endif

typedef struct
{
	int  rotate;
	int  deblockenable;
} s3c_mfc_decode_options_t;

#define S3C_MFC_DRV_RET_OK				(0)
#define S3C_MFC_DRV_RET_ERR_INVALID_PARAM		(-1001)
#define S3C_MFC_DRV_RET_ERR_HANDLE_INVALIDATED		(-1004)
#define S3C_MFC_DRV_RET_ERR_OTHERS			(-9001)

/* '25920' is the maximum MV size (=45*36*16) */
#define S3C_MFC_MAX_MV_SIZE				(45 * 36 * 16)
/* '1620' is the maximum MBTYE size (=45*36*1) */
#define S3C_MFC_MAX_MBYTE_SIZE				(45 * 36 * 1)

/* debug macros */
#define MFC_DEBUG(fmt, ...)					\
	do {							\
		printk(KERN_DEBUG				\
			"%s: " fmt, __func__, ##__VA_ARGS__);	\
	} while(0)

#define MFC_ERROR(fmt, ...)					\
	do {							\
		printk(KERN_ERR					\
			"%s: " fmt, __func__, ##__VA_ARGS__);	\
	} while (0)

#define MFC_NOTICE(fmt, ...)					\
	do {							\
		printk(KERN_NOTICE				\
			"%s: " fmt, __func__, ##__VA_ARGS__);	\
	} while (0)

#define MFC_INFO(fmt, ...)					\
	do {							\
		printk(KERN_INFO				\
			fmt, ##__VA_ARGS__);			\
	} while (0)

#define MFC_WARN(fmt, ...)					\
	do {							\
		printk(KERN_WARNING				\
			fmt, ##__VA_ARGS__);			\
	} while (0)


#ifdef VIDEO_MFC_DEBUG
#define mfc_debug(fmt, ...)		MFC_DEBUG(fmt, ##__VA_ARGS__)
#else
#define mfc_debug(fmt, ...)
#endif

#define mfc_err(fmt, ...)		MFC_ERROR(fmt, ##__VA_ARGS__)
#define mfc_notice(fmt, ...)		MFC_NOTICE(fmt, ##__VA_ARGS__)
#define mfc_info(fmt, ...)		MFC_INFO(fmt, ##__VA_ARGS__)
#define mfc_warn(fmt, ...)		MFC_WARN(fmt, ##__VA_ARGS__)

#endif /* _S3C_MFC_H */
