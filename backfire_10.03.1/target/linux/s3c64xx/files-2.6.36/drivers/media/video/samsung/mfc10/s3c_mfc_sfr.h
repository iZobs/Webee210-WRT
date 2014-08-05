/* linux/driver/media/video/mfc/s3c_mfc_sfr.h
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

#ifndef _S3C_MFC_SFR_H
#define _S3C_MFC_SFR_H

#include "s3c_mfc_types.h"
#include "s3c_mfc_base.h"

int s3c_mfc_sleep(void);
int s3c_mfc_wakeup(void);
BOOL s3c_mfc_issue_command(int inst_no, s3c_mfc_codec_mode_t codec_mode, s3c_mfc_command_t mfc_cmd);
int  s3c_mfc_get_firmware_ver(void);

void s3c_mfc_reset(void);
void s3c_mfc_clear_intr(void);
unsigned int s3c_mfc_intr_reason(void);
void s3c_mfc_set_eos(int buffer_mode);
void s3c_mfc_stream_end(void);
void s3c_mfc_download_boot_firmware(void);
void s3c_mfc_start_bit_processor(void);
void s3c_mfc_stop_bit_processor(void);
void s3c_mfc_config_sfr_bitproc_buffer(void);
void s3c_mfc_config_sfr_ctrl_opts(void);


#endif /* _S3C_MFC_SFR_H */
