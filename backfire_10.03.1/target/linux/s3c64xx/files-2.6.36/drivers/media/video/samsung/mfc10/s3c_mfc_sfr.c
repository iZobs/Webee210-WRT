/* linux/driver/media/video/mfc/s3c_mfc_sfr.c
 *
 * C file for Samsung MFC (Multi Function Codec - FIMV) driver 
 * This source file is for setting the MFC's registers.
 *
 * PyoungJae Jung, Jiun Yu, Copyright (c) 2009 Samsung Electronics 
 * http://www.samsungsemi.com/ 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <asm/io.h>
#include <plat/regs-mfc.h>

#include "s3c_mfc_sfr.h"
#include "s3c_mfc_config.h"
#include "prism_s.h"
#include "s3c_mfc_intr_noti.h"
#include "s3c_mfc.h"

extern wait_queue_head_t	s3c_mfc_wait_queue;
extern unsigned int		s3c_mfc_intr_type;
extern void __iomem		*s3c_mfc_sfr_base_virt_addr;


int s3c_mfc_sleep()
{
	/* Wait until finish executing command. */
	while (readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BUSY_FLAG) != 0)
		udelay(1);

	/* Issue Sleep Command. */
	writel(0x01, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BUSY_FLAG);
	writel(0x0A, s3c_mfc_sfr_base_virt_addr + S3C_MFC_RUN_CMD);

	while (readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BUSY_FLAG) != 0)
		udelay(1);

	return 1;
}

int s3c_mfc_wakeup()
{
	/* Bit processor gets started. */
	writel(0x01, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BUSY_FLAG);
	writel(0x01, s3c_mfc_sfr_base_virt_addr + S3C_MFC_CODE_RUN);

	while (readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BUSY_FLAG) != 0)
		udelay(1);

	/* Bit processor wakes up. */
	writel(0x01, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BUSY_FLAG);
	writel(0x0B, s3c_mfc_sfr_base_virt_addr + S3C_MFC_RUN_CMD);

	while (readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BUSY_FLAG) != 0)
		udelay(1);

	return 1;
}


static char *s3c_mfc_get_cmd_string(s3c_mfc_command_t mfc_cmd)
{
	switch ((int)mfc_cmd) {
	case SEQ_INIT:
		return "SEQ_INIT";

	case SEQ_END:
		return "SEQ_END";

	case PIC_RUN:
		return "PIC_RUN";

	case SET_FRAME_BUF:
		return "SET_FRAME_BUF";

	case ENC_HEADER:
		return "ENC_HEADER";

	case ENC_PARA_SET:
		return "ENC_PARA_SET";

	case DEC_PARA_SET:
		return "DEC_PARA_SET";

	case GET_FW_VER:
		return "GET_FW_VER";

	}

	return "UNDEF CMD";
}

static int s3c_mfc_wait_for_ready(void)
{
	int   i;

	for (i = 0; i < 1000; i++) {
		if (readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BUSY_FLAG) == 0) {
			return TRUE;
		}
		udelay(100);	/* 1/1000 second */
	}

	mfc_debug("timeout in waiting for the bitprocessor available\n");

	return FALSE;
}


int s3c_mfc_get_firmware_ver(void)
{
	unsigned int prd_no, ver_no;

	s3c_mfc_wait_for_ready();

	writel(GET_FW_VER, s3c_mfc_sfr_base_virt_addr + S3C_MFC_RUN_CMD);

	mfc_debug("GET_FW_VER command was issued\n");

	s3c_mfc_wait_for_ready();

	prd_no = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_SEQ_SUCCESS) >> 16;
	ver_no = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_SEQ_SUCCESS) & 0x00FFFF;

	mfc_debug("GET_FW_VER => 0x%x, 0x%x\n", prd_no, ver_no);
	mfc_debug("BUSY_FLAG => %d\n", 					\
		readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BUSY_FLAG));

	return readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_SEQ_SUCCESS);
}


BOOL s3c_mfc_issue_command(int inst_no, s3c_mfc_codec_mode_t codec_mode, s3c_mfc_command_t mfc_cmd)
{
	unsigned int intr_reason;

	writel(inst_no, s3c_mfc_sfr_base_virt_addr + S3C_MFC_RUN_INDEX);

	if (codec_mode == H263_DEC) {
		writel(MP4_DEC, s3c_mfc_sfr_base_virt_addr + S3C_MFC_RUN_COD_STD);
	} else if (codec_mode == H263_ENC) {
		writel(MP4_ENC, s3c_mfc_sfr_base_virt_addr + S3C_MFC_RUN_COD_STD);
	} else {
		writel(codec_mode, s3c_mfc_sfr_base_virt_addr + S3C_MFC_RUN_COD_STD);
	}

	switch (mfc_cmd) {
	case PIC_RUN:
	case SEQ_INIT:
	case SEQ_END:
		writel(mfc_cmd, s3c_mfc_sfr_base_virt_addr + S3C_MFC_RUN_CMD);

		if(interruptible_sleep_on_timeout(&s3c_mfc_wait_queue, 500) == 0) {
			s3c_mfc_stream_end();
			return FALSE; 
		}

		intr_reason = s3c_mfc_intr_type;

		if (intr_reason == S3C_MFC_INTR_REASON_INTRNOTI_TIMEOUT) {
			mfc_err("command = %s, WaitInterruptNotification returns TIMEOUT\n", \
								s3c_mfc_get_cmd_string(mfc_cmd));
			return FALSE;
		}
		if (intr_reason & S3C_MFC_INTR_REASON_BUFFER_EMPTY) {
			mfc_err("command = %s, BUFFER EMPTY interrupt was raised\n", \
							s3c_mfc_get_cmd_string(mfc_cmd));
			return FALSE;
		}
		break;

	default:
		if (s3c_mfc_wait_for_ready() == FALSE) {
			mfc_err("command = %s, bitprocessor is busy before issuing the command\n",	\
									s3c_mfc_get_cmd_string(mfc_cmd));
			return FALSE;
		}

		writel(mfc_cmd, s3c_mfc_sfr_base_virt_addr + S3C_MFC_RUN_CMD);
		s3c_mfc_wait_for_ready();

	} 

	return TRUE;
}


/* Perform the SW_RESET */
void s3c_mfc_reset(void)
{
	writel(0x00, s3c_mfc_sfr_base_virt_addr + S3C_MFC_SFR_SW_RESET_ADDR);
	writel(0x01, s3c_mfc_sfr_base_virt_addr + S3C_MFC_SFR_SW_RESET_ADDR);

	/* Interrupt is enabled for PIC_RUN command and empty/full STRM_BUF status. */
	writel(S3C_MFC_INTR_ENABLE_RESET, s3c_mfc_sfr_base_virt_addr + S3C_MFC_INT_ENABLE);
	writel(S3C_MFC_INTR_REASON_NULL, s3c_mfc_sfr_base_virt_addr + S3C_MFC_INT_REASON);
	writel(0x1, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BITS_INT_CLEAR);
}

/* 
 * Clear the MFC Interrupt
 * After catching the MFC Interrupt,
 * it is required to call this functions for clearing the interrupt-related register.
 */
void s3c_mfc_clear_intr(void)
{
	writel(0x1, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BITS_INT_CLEAR);
	writel(S3C_MFC_INTR_REASON_NULL, s3c_mfc_sfr_base_virt_addr + S3C_MFC_INT_REASON);
}

/* Check INT_REASON register of MFC (the interrupt reason register) */
unsigned int s3c_mfc_intr_reason(void)
{
	return readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_INT_REASON);
}

/* 
 * Set the MFC's SFR of DEC_FUNC_CTRL to 1.
 * It means that the data will not be added more to the STRM_BUF.
 * It is required in RING_BUF mode (VC-1 DEC).
 */
void s3c_mfc_set_eos(int buffer_mode)
{
	if (buffer_mode == 1) {
		writel(1 << 1, s3c_mfc_sfr_base_virt_addr + S3C_MFC_DEC_FUNC_CTRL);
	} else {
		writel(1, s3c_mfc_sfr_base_virt_addr + S3C_MFC_DEC_FUNC_CTRL);
	}
}

void s3c_mfc_stream_end()
{
	writel(0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_DEC_FUNC_CTRL);
}

void s3c_mfc_download_boot_firmware(void)
{
	unsigned int  i;
	unsigned int  data;

	/* Download the Boot code into MFC's internal memory */
	for (i = 0; i < 512; i++) {
		data = s3c_mfc_bit_code[i];
		writel(((i<<16) | data), s3c_mfc_sfr_base_virt_addr + S3C_MFC_CODE_DN_LOAD);
	}

}

void s3c_mfc_start_bit_processor(void)
{
	writel(0x01, s3c_mfc_sfr_base_virt_addr + S3C_MFC_CODE_RUN);
}


void s3c_mfc_stop_bit_processor(void)
{
	writel(0x00, s3c_mfc_sfr_base_virt_addr + S3C_MFC_CODE_RUN);
}

void s3c_mfc_config_sfr_bitproc_buffer(void)
{
	unsigned int code;

	/* 
	 * CODE BUFFER ADDRESS (BASE + 0x100)
	 * 	: Located from the Base address of the BIT PROCESSOR'S Firmware code segment
	 */
	writel(S3C_MFC_BASEADDR_BITPROC_BUF, s3c_mfc_sfr_base_virt_addr + S3C_MFC_CODE_BUF_ADDR);


	/* 
	 * WORKING BUFFER ADDRESS (BASE + 0x104)
	 * 	: Located from the next to the BIT PROCESSOR'S Firmware code segment
	 */
	code = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_CODE_BUF_ADDR);
	writel(code + S3C_MFC_CODE_BUF_SIZE, s3c_mfc_sfr_base_virt_addr + S3C_MFC_WORK_BUF_ADDR);


	/* PARAMETER BUFFER ADDRESS (BASE + 0x108)
	 * 	: Located from the next to the WORKING BUFFER
	 */
	code = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_WORK_BUF_ADDR);
	writel(code + S3C_MFC_WORK_BUF_SIZE, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARA_BUF_ADDR);
}

void s3c_mfc_config_sfr_ctrl_opts(void)
{
	unsigned int  uRegData;

	/* BIT STREAM BUFFER CONTROL (BASE + 0x10C) */
	uRegData = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_STRM_BUF_CTRL);
	writel((uRegData & ~(0x03)) | S3C_MFC_BUF_STATUS_FULL_EMPTY_CHECK_BIT | S3C_MFC_STREAM_ENDIAN_LITTLE, 	\
								s3c_mfc_sfr_base_virt_addr + S3C_MFC_STRM_BUF_CTRL);

	/* FRAME MEMORY CONTROL  (BASE + 0x110) */
	writel(S3C_MFC_YUV_MEM_ENDIAN_LITTLE, s3c_mfc_sfr_base_virt_addr + S3C_MFC_FRME_BUF_CTRL);


	/* DECODER FUNCTION CONTROL (BASE + 0x114) */
	writel(0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_DEC_FUNC_CTRL);

	/* WORK BUFFER CONTROL (BASE + 0x11C) */
	writel(0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_WORK_BUF_CTRL);
}

