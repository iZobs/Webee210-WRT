/* linux/driver/media/video/mfc/s3c_mfc_init_hw.c
 *
 * C file for Samsung MFC (Multi Function Codec - FIMV) driver 
 * This source file is for initializing the MFC's H/W setting.
 *
 * PyoungJae Jung, Jiun Yu, Copyright (c) 2009 Samsung Electronics 
 * http://www.samsungsemi.com/ 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>

#include "s3c_mfc_base.h"
#include "s3c_mfc_sfr.h"
#include "s3c_mfc_bitproc_buf.h"
#include "s3c_mfc_databuf.h"
#include "s3c_mfc_types.h"
#include "s3c_mfc_config.h"
#include "s3c_mfc_yuv_buf_manager.h"
#include "s3c_mfc.h"

BOOL s3c_mfc_setup_memory(void)
{
	BOOL ret_bit, ret_dat;
	unsigned char *pDataBuf;

	/* 
	 * MFC SFR(Special Function Registers), Bitprocessor buffer, Data buffer의 
	 * physical address 를 virtual address로 mapping 한다 
	 */

	ret_bit = s3c_mfc_memmap_bitproc_buff();
	if (ret_bit == FALSE) {
		mfc_err("fail to mapping bitprocessor buffer memory\n");
		return FALSE;
	}

	ret_dat	= s3c_mfc_memmap_databuf();
	if (ret_dat == FALSE) {
		mfc_err("fail to mapping data buffer memory \n");
		return FALSE;
	}

	/* FramBufMgr Module Initialization */
	pDataBuf = (unsigned char *)s3c_mfc_get_databuf_virt_addr();
	s3c_mfc_init_yuvbuf_mgr(pDataBuf + S3C_MFC_STREAM_BUF_SIZE, S3C_MFC_YUV_BUF_SIZE);


	return TRUE;
}


BOOL s3c_mfc_init_hw(void)
{
	/* 
	 * 1. Reset the MFC IP
	 */
	s3c_mfc_reset();

	/*
	 * 2. Download Firmware code into MFC
	 */
	s3c_mfc_put_firmware_into_codebuff();
	s3c_mfc_download_boot_firmware();
	mfc_debug("downloading firmware into bitprocessor\n");

	/* 
	 * 3. Start Bit Processor
	 */
	s3c_mfc_start_bit_processor();

	/* 
	 * 4. Set the Base Address Registers for the following 3 buffers
	 * (CODE_BUF, WORKING_BUF, PARAMETER_BUF)
	 */
	s3c_mfc_config_sfr_bitproc_buffer();

	/* 
	 * 5. Set the Control Registers
	 * 	- STRM_BUF_CTRL
	 * 	- FRME_BUF_CTRL
	 * 	- DEC_FUNC_CTRL
	 * 	- WORK_BUF_CTRL
	 */
	s3c_mfc_config_sfr_ctrl_opts();

	s3c_mfc_get_firmware_ver();

	return TRUE;
}

