/* linux/driver/media/video/mfc/s3c_mfc_yuv_buf_manager.h
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

#ifndef _S3C_MFC_YUV_BUF_MANAGER_H
#define _S3C_MFC_YUV_BUF_MANAGER_H

#include "s3c_mfc_types.h"

BOOL            s3c_mfc_init_yuvbuf_mgr(unsigned char *pBufBase, int nBufSize);
void            s3c_mfc_yuv_buffer_mgr_final(void);

unsigned char  *s3c_mfc_commit_yuv_buffer_mgr(int idx_commit, int commit_size);
void            s3c_mfc_free_yuv_buffer_mgr(int idx_commit);

unsigned char  *s3c_mfc_get_yuv_buffer(int idx_commit);
int             s3c_mfc_get_yuv_buffer_size(int idx_commit);

void            s3c_mfc_print_commit_yuv_buffer_info(void);


#endif /* _S3C_MFC_YUV_BUF_MANAGER_H */
