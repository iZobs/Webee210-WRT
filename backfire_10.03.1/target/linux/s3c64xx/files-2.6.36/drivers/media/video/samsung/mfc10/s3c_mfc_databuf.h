/* linux/driver/media/video/mfc/s3c_mfc_databuf.h
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

#ifndef _S3C_MFC_DATABUF_H
#define _S3C_MFC_DATABUF_H

#include "s3c_mfc_types.h"

BOOL s3c_mfc_memmap_databuf(void);

volatile unsigned char *s3c_mfc_get_databuf_virt_addr(void);
volatile unsigned char *s3c_mfc_get_yuvbuff_virt_addr(void);
unsigned int s3c_mfc_get_databuf_phys_addr(void);
unsigned int s3c_mfc_get_yuvbuff_phys_addr(void);

#endif /* _S3C_MFC_DATABUF_H */
