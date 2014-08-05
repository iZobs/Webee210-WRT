/* linux/driver/media/video/mfc/s3c_mfc_yuv_buf_manager.c
 *
 * C file for Samsung MFC (Multi Function Codec - FIMV) driver 
 * This source file is for managing the YUV buffer.
 *
 * PyoungJae Jung, Jiun Yu, Copyright (c) 2009 Samsung Electronics 
 * http://www.samsungsemi.com/ 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/kernel.h>

#include "s3c_mfc_yuv_buf_manager.h"
#include "s3c_mfc_types.h"
#include "s3c_mfc.h"

/* 
 * The size in bytes of the BUF_SEGMENT. 
 * The buffers are fragemented into the segment unit of this size.
 */
#define S3C_MFC_BUF_SEGMENT_SIZE	1024


typedef struct {
	unsigned char *pBaseAddr;
	int            idx_commit;
} s3c_mfc_segment_info_t;


typedef struct {
	int index_base_seg;
	int num_segs;
} s3c_mfc_commit_info_t;


static s3c_mfc_segment_info_t  *s3c_mfc_segment_info = NULL;
static s3c_mfc_commit_info_t   *s3c_mfc_commit_info  = NULL;

static unsigned char *s3c_mfc_buffer_base  = NULL;
static int            s3c_mfc_buffer_size  = 0;
static int            s3c_mfc_num_segments		= 0;


/* 
 * int s3c_mfc_init_yuvbuf_mgr(unsigned char *pBufBase, int nBufSize) 
 *
 * Description 
 * 		This function initializes the MfcFramBufMgr(Buffer Segment Manager)
 * Parameters
 * 		pBufBase [IN]: pointer to the buffer which will be managed by this MfcFramBufMgr functions. 
 * 		nBufSize [IN]: buffer size in bytes
 * Return Value 
 * 		1 : Success
 * 		0 : Fail
 */
BOOL s3c_mfc_init_yuvbuf_mgr(unsigned char *buffer_base, int buffer_size)
{
	int   i;

	if (buffer_base == NULL || buffer_size == 0)
		return FALSE;

	if ((s3c_mfc_buffer_base != NULL) && (s3c_mfc_buffer_size != 0)) {
		if ((buffer_base == s3c_mfc_buffer_base) && (buffer_size == s3c_mfc_buffer_size))
			return TRUE;

		s3c_mfc_yuv_buffer_mgr_final();
	}

	s3c_mfc_buffer_base = buffer_base;
	s3c_mfc_buffer_size = buffer_size;
	s3c_mfc_num_segments = buffer_size / S3C_MFC_BUF_SEGMENT_SIZE;

	s3c_mfc_segment_info = (s3c_mfc_segment_info_t *)kmalloc(s3c_mfc_num_segments * sizeof(s3c_mfc_segment_info_t),  \
													GFP_KERNEL);
	for (i = 0; i < s3c_mfc_num_segments; i++) {
		s3c_mfc_segment_info[i].pBaseAddr   = buffer_base  +  (i * S3C_MFC_BUF_SEGMENT_SIZE);
		s3c_mfc_segment_info[i].idx_commit  = 0;
	}

	s3c_mfc_commit_info  = (s3c_mfc_commit_info_t *)kmalloc(s3c_mfc_num_segments * sizeof(s3c_mfc_commit_info_t),    \
													GFP_KERNEL);
	for (i = 0; i < s3c_mfc_num_segments; i++) {
		s3c_mfc_commit_info[i].index_base_seg  = -1;
		s3c_mfc_commit_info[i].num_segs        = 0;
	}

	return TRUE;
}


void s3c_mfc_yuv_buffer_mgr_final()
{
	if (s3c_mfc_segment_info != NULL) {
		kfree(s3c_mfc_segment_info);
		s3c_mfc_segment_info = NULL;
	}

	if (s3c_mfc_commit_info != NULL) {
		kfree(s3c_mfc_commit_info);
		s3c_mfc_commit_info = NULL;
	}

	s3c_mfc_buffer_base  = NULL;
	s3c_mfc_buffer_size  = 0;
	s3c_mfc_num_segments = 0;
}

/* 
 * unsigned char *s3c_mfc_commit_yuv_buffer_mgr(int idx_commit, int commit_size)
 *
 * Description
 * 	This function requests the commit for commit_size buffer to be reserved.
 * Parameters
 * 	idx_commit  [IN]: pointer to the buffer which will be managed by this MfcFramBufMgr functions.
 * 	commit_size [IN]: commit size in bytes
 * Return Value
 * 	NULL : Failed to commit (Wrong parameters, commit_size too big, and so on.)
 * 	Otherwise it returns the pointer which was committed.
 */
unsigned char *s3c_mfc_commit_yuv_buffer_mgr(int idx_commit, int commit_size)
{
	int  i, j;
	int  num_yuv_buf_seg;

	if (s3c_mfc_segment_info == NULL || s3c_mfc_commit_info == NULL) {
		return NULL;
	}

	/* check parameters */
	if (idx_commit < 0 || idx_commit >= s3c_mfc_num_segments)
		return NULL;
	if (commit_size <= 0 || commit_size > s3c_mfc_buffer_size)
		return NULL;

	if (s3c_mfc_commit_info[idx_commit].index_base_seg != -1)
		return NULL;


	if ((commit_size % S3C_MFC_BUF_SEGMENT_SIZE) == 0)
		num_yuv_buf_seg = commit_size / S3C_MFC_BUF_SEGMENT_SIZE;
	else
		num_yuv_buf_seg = (commit_size / S3C_MFC_BUF_SEGMENT_SIZE)  +  1;

	for (i=0; i<(s3c_mfc_num_segments - num_yuv_buf_seg); i++) {
		if (s3c_mfc_segment_info[i].idx_commit != 0)
			continue;

		for (j=0; j<num_yuv_buf_seg; j++) {
			if (s3c_mfc_segment_info[i+j].idx_commit != 0)
				break;
		}

		if (j == num_yuv_buf_seg) {

			for (j=0; j<num_yuv_buf_seg; j++) {
				s3c_mfc_segment_info[i+j].idx_commit = 1;
			}

			s3c_mfc_commit_info[idx_commit].index_base_seg  = i;
			s3c_mfc_commit_info[idx_commit].num_segs        = num_yuv_buf_seg;

			return s3c_mfc_segment_info[i].pBaseAddr;
		} else {
			i = i + j - 1;
		}
	}

	return NULL;
}


/*
 * void s3c_mfc_free_yuv_buffer_mgr(int idx_commit)
 *
 * Description
 * 	This function frees the committed region of buffer.
 * Parameters
 * 	idx_commit  [IN]: pointer to the buffer which will be managed by this MfcFramBufMgr functions.
 * Return Value
 * 	None
 */
void s3c_mfc_free_yuv_buffer_mgr(int idx_commit)
{
	int  i;

	int  index_base_seg;
	int  num_yuv_buf_seg;

	if (s3c_mfc_segment_info == NULL || s3c_mfc_commit_info == NULL)
		return;

	if (idx_commit < 0 || idx_commit >= s3c_mfc_num_segments)
		return;

	if (s3c_mfc_commit_info[idx_commit].index_base_seg == -1)
		return;


	index_base_seg    =  s3c_mfc_commit_info[idx_commit].index_base_seg;
	num_yuv_buf_seg  =  s3c_mfc_commit_info[idx_commit].num_segs;

	for (i = 0; i < num_yuv_buf_seg; i++) {
		s3c_mfc_segment_info[index_base_seg + i].idx_commit = 0;
	}


	s3c_mfc_commit_info[idx_commit].index_base_seg  =  -1;
	s3c_mfc_commit_info[idx_commit].num_segs        =  0;

}

/* 
 * unsigned char *s3c_mfc_get_yuv_buffer(int idx_commit)
 *
 * Description
 * 	This function obtains the committed buffer of 'idx_commit'.
 * Parameters
 * 	idx_commit  [IN]: commit index of the buffer which will be obtained
 * Return Value
 * 	NULL : Failed to get the indicated buffer (Wrong parameters, not committed, and so on.)
 * 	Otherwise it returns the pointer which was committed.
 */
unsigned char *s3c_mfc_get_yuv_buffer(int idx_commit)
{
	int index_base_seg;

	if (s3c_mfc_segment_info == NULL || s3c_mfc_commit_info == NULL)
		return NULL;

	if (idx_commit < 0 || idx_commit >= s3c_mfc_num_segments)
		return NULL;

	if (s3c_mfc_commit_info[idx_commit].index_base_seg == -1)
		return NULL;

	index_base_seg  =  s3c_mfc_commit_info[idx_commit].index_base_seg;

	return s3c_mfc_segment_info[index_base_seg].pBaseAddr;
}

/* 
 * int s3c_mfc_get_yuv_buffer_size(int idx_commit)
 *
 * Description
 * 	This function obtains the size of the committed buffer of 'idx_commit'.
 * Parameters
 * 	idx_commit  [IN]: commit index of the buffer which will be obtained
 * Return Value
 * 	0 : Failed to get the size of indicated buffer (Wrong parameters, not committed, and so on.)
 * 	Otherwise it returns the size of the buffer.
 * 	Note that the size is multiples of the S3C_MFC_BUF_SEGMENT_SIZE.
 */
int s3c_mfc_get_yuv_buffer_size(int idx_commit)
{
	if (s3c_mfc_segment_info == NULL || s3c_mfc_commit_info == NULL)
		return 0;

	if (idx_commit < 0 || idx_commit >= s3c_mfc_num_segments)
		return 0;

	if (s3c_mfc_commit_info[idx_commit].index_base_seg == -1)
		return 0;

	return (s3c_mfc_commit_info[idx_commit].num_segs * S3C_MFC_BUF_SEGMENT_SIZE);
}

/*
 * void s3c_mfc_print_commit_yuv_buffer_info()
 *
 * Description
 * 	This function prints the commited information on the console screen.
 * Parameters
 * 	None
 * Return Value
 * 	None
 */
void s3c_mfc_print_commit_yuv_buffer_info()
{
	int  i;

	if (s3c_mfc_segment_info == NULL || s3c_mfc_commit_info == NULL) {
		mfc_err("fram buffer manager is not initialized\n");
		return;
	}


	for (i = 0; i < s3c_mfc_num_segments; i++) {
		if (s3c_mfc_commit_info[i].index_base_seg != -1)  {
			mfc_debug("commit index = %03d, base segment index = %d\n",	\
						i, s3c_mfc_commit_info[i].index_base_seg);
			mfc_debug("commit index = %03d, number of segment = %d\n", \
						i, s3c_mfc_commit_info[i].num_segs);
		}
	}
}
