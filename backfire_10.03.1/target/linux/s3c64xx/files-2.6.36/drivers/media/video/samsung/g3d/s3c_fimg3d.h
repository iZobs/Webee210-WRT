/* linux/drivers/video/samsung/g3d/s3c_fimg3d.h
 *
 * Driver header file for Samsung 3D Accelerator(FIMG-3D)
 *
 * Jegeon Jung, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _S3C_G3D_DRIVER_H_
#define _S3C_G3D_DRIVER_H_

#define FIMG_PHY_SIZE			0x90000

#define DWORD   unsigned int

#define FGGB_CACHECTL			(0x04)
#define FGGB_HOSTINTERFACE	        (0xc000)
#define FGGB_PIPESTATE		        (0x00)
#define FGHI_HI_CTRL			(0x8008)
#define FGHI_ATTR0			(0x8040)
#define FGVS_INSTMEM_SADDR		(0x10000)
#define FGVS_CFLOAT_SADDR		(0x14000)
#define FGVS_CINT_SADDR			(0x18000)
#define FGVS_CBOOL_SADDR		(0x18400)
#define FGVS_CONFIG			(0x1C800)
#define FGVS_PC_RANGE			(0x20000)
#define FGPE_VTX_CONTEXT		(0x30000)
#define FGRA_PIXEL_SAMPOS		(0x38000)
#define FGRA_LOD_CTRL			(0x3C000)
#define FGRA_POINT_WIDTH		(0x3801C)
#define FGPS_INSTMEM_SADDR		(0x40000)
#define FGPS_CFLOAT_SADDR		(0x44000)
#define FGPS_CINT_SADDR			(0x48000)
#define FGPS_CBOOL_SADDR		(0x48400)
#define FGPS_EXE_MODE			(0x4C800)
#define FGTU_TEX0_CTRL			(0x60000) /* R/W */
#define FGTU_TEX1_CTRL			(0x60050)
#define FGTU_TEX2_CTRL			(0x600A0)
#define FGTU_TEX3_CTRL			(0x600F0)
#define FGTU_TEX4_CTRL			(0x60140)
#define FGTU_TEX5_CTRL			(0x60190)
#define FGTU_TEX6_CTRL			(0x601E0)
#define FGTU_TEX7_CTRL			(0x60230)
#define FGRA_CLIP_XCORD			(0x3C004)
#define FGTU_COLOR_KEY1			(0x60280) /* R/W Color Key1 */
#define FGTU_VTXTEX0_CTRL		(0x602C0)
#define FGPF_SCISSOR_XCORD		(0x70000)
#define FGPF_STENCIL_DEPTH_MASK		(0x70028)
#define FGGB_PIPEMASK			(0x48)
#define FGGB_INTMASK			(0x44)
#define FGGB_INTPENDING			(0x40)
#define FGGB_RST			(0x8)
#define FGGB_VERSION			(0x10)
#define FGGB_PIPEINTSTATE		(0x50)

#endif /*_S3C_G2D_DRIVER_H_*/
