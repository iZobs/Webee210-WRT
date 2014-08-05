/* linux/driver/media/video/mfc/s3c_mfc_bitproc_buf.h
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

#ifndef _S3C_MFC_BITPROC_BUF_H
#define _S3C_MFC_BITPROC_BUF_H

#include "s3c_mfc_types.h"

BOOL s3c_mfc_memmap_bitproc_buff(void);
volatile unsigned char *s3c_mfc_get_bitproc_buff_virt_addr(void);
unsigned char *s3c_mfc_get_param_buff_virt_addr(void);

void s3c_mfc_put_firmware_into_codebuff(void);

#endif /* _S3C_MFC_BITPROC_BUF_H */
