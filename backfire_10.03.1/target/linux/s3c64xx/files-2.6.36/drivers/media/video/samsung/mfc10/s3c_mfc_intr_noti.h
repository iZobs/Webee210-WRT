/* linux/driver/media/video/mfc/s3c_mfc_intr_noti.h
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

#ifndef _S3C_MFC_INTR_NOTI_H
#define _S3C_MFC_INTR_NOTI_H

#define S3C_MFC_INTR_NOTI_TIMEOUT    1000

/*
 * MFC Interrupt Enable Macro Definition
 */
#define S3C_MFC_INTR_ENABLE_ALL    0xCCFF
#define S3C_MFC_INTR_ENABLE_RESET  0xC00E

/*
 * MFC Interrupt Reason Macro Definition
 */
#define S3C_MFC_INTR_REASON_NULL		0x0000
#define S3C_MFC_INTR_REASON_BUFFER_EMPTY        0xC000
#define S3C_MFC_INTR_REASON_INTRNOTI_TIMEOUT    (-99)


#endif /* _S3C_MFC_INTR_NOTI_H */
