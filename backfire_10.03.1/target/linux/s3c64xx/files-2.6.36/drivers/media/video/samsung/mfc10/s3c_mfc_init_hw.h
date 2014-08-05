/* linux/driver/media/video/mfc/s3c_mfc_init_hw.h
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

#ifndef _S3C_MFC_HW_INIT_H
#define _S3C_MFC_HW_INIT_H

#include "s3c_mfc_types.h"

BOOL s3c_mfc_setup_memory(void);
BOOL s3c_mfc_init_hw(void);

#endif /* _S3C_MFC_HW_INIT_H */
