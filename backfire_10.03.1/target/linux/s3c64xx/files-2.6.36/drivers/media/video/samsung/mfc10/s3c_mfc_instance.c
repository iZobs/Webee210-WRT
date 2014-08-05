/* linux/driver/media/video/mfc/s3c_mfc_instance.c
 *
 * C file for Samsung MFC (Multi Function Codec - FIMV) driver 
 *
 * PyoungJae Jung, Jiun Yu, Copyright (c) 2009 Samsung Electronics 
 * http://www.samsungsemi.com/ 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <asm/memory.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>

#include <plat/regs-mfc.h>

#include "s3c_mfc_base.h"
#include "s3c_mfc_instance.h"
#include "s3c_mfc_databuf.h"
#include "s3c_mfc_yuv_buf_manager.h"
#include "s3c_mfc_config.h"
#include "s3c_mfc_sfr.h"
#include "s3c_mfc_bitproc_buf.h"
#include "s3c_mfc_inst_pool.h"
#include "s3c_mfc.h"


extern void __iomem *s3c_mfc_sfr_base_virt_addr;
static s3c_mfc_inst_context_t _mfcinst_ctx[S3C_MFC_NUM_INSTANCES_MAX];


s3c_mfc_inst_context_t *s3c_mfc_inst_get_context(int inst_no)
{
	if ((inst_no < 0) || (inst_no >= S3C_MFC_NUM_INSTANCES_MAX))
		return NULL;

	if (S3C_MFC_INST_STATE(&(_mfcinst_ctx[inst_no])) >= S3C_MFC_INST_STATE_CREATED)
		return &(_mfcinst_ctx[inst_no]);
	else
		return NULL;
}

static void s3c_mfc_get_stream_buffer_addr(s3c_mfc_inst_context_t *ctx)
{
	ctx->stream_buffer = (unsigned char *)(s3c_mfc_get_databuf_virt_addr() + 	\
				(ctx->inst_no * S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE));
	ctx->phys_addr_stream_buffer = (unsigned int)(s3c_mfc_get_databuf_phys_addr() +	\
					(ctx->inst_no * S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE));
	ctx->stream_buffer_size = S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE;

	mfc_debug("ctx->stream_buffer address 0x%08x\n", \
			(unsigned int)ctx->stream_buffer);
	mfc_debug("ctx->phys_addr_stream_buffer address 0x%08x\n", \
					ctx->phys_addr_stream_buffer);
}

static BOOL s3c_mfc_get_yuv_buffer_addr(s3c_mfc_inst_context_t *ctx, int buf_size)
{
	unsigned char	*instance_yuv_buffer;


	instance_yuv_buffer = s3c_mfc_commit_yuv_buffer_mgr(ctx->inst_no, buf_size);
	if (instance_yuv_buffer == NULL) {
		mfc_err("fail to allocate frame buffer\n");
		return FALSE;
	}

	s3c_mfc_print_commit_yuv_buffer_info();

	ctx->yuv_buffer = instance_yuv_buffer; /* virtual address of frame buffer */
	ctx->phys_addr_yuv_buffer = S3C_MFC_BASEADDR_DATA_BUF +					\
				((int)instance_yuv_buffer - (int)s3c_mfc_get_databuf_virt_addr());
	ctx->yuv_buffer_size  = buf_size;
	
	mfc_debug("ctx->inst_no : %d\n", ctx->inst_no);
	mfc_debug("ctx->yuv_buffer : 0x%x\n", (unsigned int)ctx->yuv_buffer);
	mfc_debug("ctx->phys_addr_yuv_buffer : 0x%x\n", ctx->phys_addr_yuv_buffer);

	return TRUE;
}

int s3c_mfc_inst_get_line_buff(s3c_mfc_inst_context_t *ctx, unsigned char **buffer, int *size)
{
	if (S3C_MFC_INST_STATE_CHECK(ctx, S3C_MFC_INST_STATE_DELETED)) {
		mfc_err("mfc instance is deleted\n");
		return S3C_MFC_INST_ERR_STATE_DELETED;
	}

	*buffer = ctx->stream_buffer;
	*size  = S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE;

	return S3C_MFC_INST_RET_OK;
}

int s3c_mfc_inst_get_yuv_buff(s3c_mfc_inst_context_t *ctx, unsigned char **buffer, int *size)
{
	/* checking state */
	if (S3C_MFC_INST_STATE_CHECK(ctx, S3C_MFC_INST_STATE_DELETED)) {
		mfc_err("mfc instance is deleted\n");
		return S3C_MFC_INST_ERR_STATE_DELETED;
	}
	if (S3C_MFC_INST_STATE_CHECK(ctx, S3C_MFC_INST_STATE_CREATED)) {
		mfc_err("mfc instance is not initialized\n");
		return S3C_MFC_INST_ERR_STATE_CHK;
	}

	if (ctx->yuv_buffer == NULL) {
		mfc_err("mfc frame buffer is not internally allocated yet\n");
		return S3C_MFC_INST_ERR_ETC;
	}

	*size  = (ctx->buf_width * ctx->buf_height * 3) >> 1;	/* YUV420 frame size */

	if (ctx->run_index < 0)	/* RET_DEC_PIC_IDX == -3  (No picture to be displayed) */
		*buffer = NULL;
	else {
		*buffer = ctx->yuv_buffer + (ctx->run_index) * (*size);
#if (S3C_MFC_ROTATE_ENABLE == 1)
		if (ctx->post_rotation_mode & 0x0010)
			*buffer = ctx->yuv_buffer + (ctx->yuv_buffer_count) * (*size);
#endif
	}


	return S3C_MFC_INST_RET_OK;
}

/* it returns the instance number of the 6410 mfc instance context */
int s3c_mfc_inst_get_no(s3c_mfc_inst_context_t *ctx)
{
	return ctx->inst_no;
}

/* It returns the virtual address of read pointer and write pointer */
BOOL s3c_mfc_inst_get_stream_buff_rw_ptrs(s3c_mfc_inst_context_t *ctx, unsigned char **read_ptr, \
									unsigned char **write_ptr)
{
	int diff_vir_phy;
	unsigned int read_pointer = 0;
	unsigned int write_pointer = 0;


	if (S3C_MFC_INST_STATE(ctx) < S3C_MFC_INST_STATE_CREATED)
		return FALSE;

	if (S3C_MFC_INST_STATE_CHECK(ctx, S3C_MFC_INST_STATE_CREATED)) {
		/* 
		 * if s3c_mfc_inst_context_t is just created and not initialized by s3c_mfc_instance_init,
		 * then the initial read pointer and write pointer are the start address of stream buffer
		 */
		*read_ptr = ctx->stream_buffer;
		*write_ptr = ctx->stream_buffer;
	} else {
		/* the physical to virtual address conversion of read pointer and write pointer */
		diff_vir_phy = (int)(ctx->stream_buffer - ctx->phys_addr_stream_buffer);
		
		switch(ctx->inst_no) {
		case 0:
			read_pointer = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR0);
			write_pointer = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR0);
			break;
		case 1:
			read_pointer = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR1);
			write_pointer = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR1);
			break;
		case 2:
			read_pointer = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR2);
			write_pointer = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR2);
			break;
		case 3:
			read_pointer = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR3);
			write_pointer = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR3);
			break;
		case 4:
			read_pointer = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR4);
			write_pointer = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR4);
			break;
		case 5:
			read_pointer = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR5);
			write_pointer = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR5);
			break;
		case 6:
			read_pointer = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR6);
			write_pointer = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR6);
			break;
		case 7:
			read_pointer = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR7);
			write_pointer = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR7);
			break;
		}

		*read_ptr = (unsigned char *)(diff_vir_phy + read_pointer);
		*write_ptr = (unsigned char *)(diff_vir_phy + write_pointer);
	}

	return TRUE;
}

unsigned int s3c_mfc_inst_set_post_rotate(s3c_mfc_inst_context_t *ctx, unsigned int post_rotmode)
{
	unsigned int old_post_rotmode;

	old_post_rotmode = ctx->post_rotation_mode;

	if (post_rotmode & 0x0010) {
		ctx->post_rotation_mode = post_rotmode;
	} else
		ctx->post_rotation_mode = 0;


	return old_post_rotmode;
}

s3c_mfc_inst_context_t *s3c_mfc_inst_create(void)
{
	int inst_no;
	s3c_mfc_inst_context_t *ctx;


	/* occupy the 'inst_no' */
	/* if it fails, it returns NULL */
	inst_no = s3c_mfc_occupy_inst_pool();
	if (inst_no == -1)
		return NULL;

	ctx = &(_mfcinst_ctx[inst_no]);

	memset(ctx, 0, sizeof(s3c_mfc_inst_context_t));

	ctx->inst_no     = inst_no;
	S3C_MFC_INST_STATE_TRANSITION(ctx, S3C_MFC_INST_STATE_CREATED);

	s3c_mfc_get_stream_buffer_addr(ctx);

	mfc_debug("state = %d\n", ctx->state_var);

	return ctx;
}

/* it deletes the 6410 mfc instance */
void s3c_mfc_inst_del(s3c_mfc_inst_context_t *ctx)
{
	/* checking state */
	if (S3C_MFC_INST_STATE_CHECK(ctx, S3C_MFC_INST_STATE_DELETED)) {
		mfc_err("mfc instance is already deleted\n");
		return;
	}

	s3c_mfc_release_inst_pool(ctx->inst_no);
	s3c_mfc_free_yuv_buffer_mgr(ctx->inst_no);

	S3C_MFC_INST_STATE_TRANSITION(ctx, S3C_MFC_INST_STATE_DELETED);
}

/* it turns on the flag indicating 6410 mfc's power-off */
void s3c_mfc_inst_pow_off_state(s3c_mfc_inst_context_t *ctx)
{
	S3C_MFC_INST_STATE_PWR_OFF_FLAG_SET(ctx);
}

/* it turns on the flag indicating 6410 mfc's power-off */
void s3c_mfc_inst_pow_on_state(s3c_mfc_inst_context_t *ctx)
{
	S3C_MFC_INST_STATE_PWR_OFF_FLAG_CLEAR(ctx);
}

/*
 * it initializes the 6410 mfc instance with the appropriate config stream
 * the config stream must be copied into stream buffer before this function
 */
int s3c_mfc_inst_init_dec(s3c_mfc_inst_context_t *ctx, s3c_mfc_codec_mode_t codec_mode, unsigned long strm_leng)
{
	int i;
	int yuv_buf_size; /* required size in yuv buffer */
	int frame_size;   /* width * height */
	int frame_need_count;
	unsigned char *param_buf; /* PARAM_BUF in BITPROC_BUF */
	
	
	/* checking state */
	if (!S3C_MFC_INST_STATE_CHECK(ctx, S3C_MFC_INST_STATE_CREATED)) {
		mfc_err("sequence init function was called at an incorrect point\n");
		return S3C_MFC_INST_ERR_STATE_CHK;
	}

	/* codec_mode */
	ctx->codec_mode = codec_mode;

	/* stream size checking */
	if (strm_leng > S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE) {
		mfc_err("Input buffer size is too small to hold the input stream.\n");
		return S3C_MFC_INST_ERR_ETC;
	}

	/* 
	 * Copy the Config Stream into stream buffer
	 * config stream needs to be in the stream buffer a priori.
	 * read pointer and write pointer are set to point the start and end address of stream buffer
	 * if write pointer is set to [start + config_leng] instead of [end address of stream buffer],
	 * then mfc is not initialized when MPEG4 decoding.
	 */
	mfc_debug("strm_leng = %d\n", (int)strm_leng);

	strm_leng = S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE;
	
	switch(ctx->inst_no) {
	case 0:
		writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR0);
		writel(ctx->phys_addr_stream_buffer + strm_leng, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR0);
		break;
	case 1:
		writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR1);
		writel(ctx->phys_addr_stream_buffer + strm_leng, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR1);
		break;
	case 2:
		writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR2);
		writel(ctx->phys_addr_stream_buffer + strm_leng, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR2);
		break;
	case 3:
		writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR3);
		writel(ctx->phys_addr_stream_buffer + strm_leng, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR3);
		break;
	case 4:
		writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR4);
		writel(ctx->phys_addr_stream_buffer + strm_leng, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR4);
		break;
	case 5:
		writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR5);
		writel(ctx->phys_addr_stream_buffer + strm_leng, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR5);
		break;
	case 6:
		writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR6);
		writel(ctx->phys_addr_stream_buffer + strm_leng, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR6);
		break;
	case 7:
		writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR7);
		writel(ctx->phys_addr_stream_buffer + strm_leng, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR7);
		break;
	}
	
	/*
	 * issue the SEQ_INIT command 
	 * width/height of frame will be obtained
	 * set the Parameters for SEQ_INIT command
	 */
	writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_DEC_SEQ_BIT_BUF_ADDR);
	writel(S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE / 1024, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_DEC_SEQ_BIT_BUF_SIZE);
	writel(S3C_MFC_FILEPLAY_ENABLE | S3C_MFC_DYNBUFALLOC_ENABLE, s3c_mfc_sfr_base_virt_addr + 	\
											S3C_MFC_PARAM_DEC_SEQ_OPTION);
	writel(0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_DEC_SEQ_START_BYTE);

	mfc_debug("ctx->inst_no = %d\n", ctx->inst_no);
	mfc_debug("ctx->codec_mode = %d\n", ctx->codec_mode);
	mfc_debug("sequece bit buffer size = %d (kb)\n",			\
		readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_DEC_SEQ_BIT_BUF_SIZE));

	s3c_mfc_set_eos(0);

	/* SEQ_INIT command */
	if (s3c_mfc_issue_command(ctx->inst_no, ctx->codec_mode, SEQ_INIT) == FALSE) {
		mfc_err("sequence init failed\n");
		s3c_mfc_stream_end();
		return S3C_MFC_INST_ERR_DEC_INIT_CMD_FAIL;
	}

	s3c_mfc_stream_end();

	if (readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_SEQ_SUCCESS) == TRUE) {
		mfc_debug("RET_DEC_SEQ_SRC_SIZE = %d\n", 			\
			readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_SEQ_SRC_SIZE));
		mfc_debug("RET_DEC_SEQ_SRC_FRAME_RATE   = %d\n",		\
			readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_SEQ_SRC_FRAME_RATE));
		mfc_debug("RET_DEC_SEQ_FRAME_NEED_COUNT = %d\n",		\
			readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_SEQ_FRAME_NEED_COUNT));
		mfc_debug("RET_DEC_SEQ_FRAME_DELAY = %d\n",			\
			readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_SEQ_FRAME_DELAY));
	} else {
		mfc_err("sequece init failed = %d\n",				\
			readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_SEQ_SUCCESS));
		return S3C_MFC_INST_ERR_DEC_INIT_CMD_FAIL;
	}

	frame_need_count = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_SEQ_FRAME_NEED_COUNT);
	
	/* 
	 * width and height are obtained from return value of SEQ_INIT command
	 * stride value is multiple of 16
	 */
	ctx->height = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_SEQ_SRC_SIZE) & 0x03FF;
	ctx->width = (readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_SEQ_SRC_SIZE) >> 10) & 0x03FF;

	/*
	 * if codec mode is VC1_DEC,
	 * width & height value are not from return value of SEQ_INIT command
	 * but extracting from config stream
	 */
	if (ctx->codec_mode == VC1_DEC) {
		memcpy(&(ctx->height), ctx->stream_buffer + 12, 4);
		memcpy(&(ctx->width), ctx->stream_buffer + 16, 4);
	}
	
	if ((ctx->width & 0x0F) == 0)	/* 16 aligned (ctx->width%16 == 0) */
		ctx->buf_width  = ctx->width;
	else
		ctx->buf_width  = (ctx->width  & 0xFFFFFFF0) + 16;

	if ((ctx->height & 0x0F) == 0)	/* 16 aligned (ctx->height%16 == 0) */
		ctx->buf_height = ctx->height;
	else
		ctx->buf_height = (ctx->height & 0xFFFFFFF0) + 16;

#if (defined(DIVX_ENABLE) && (DIVX_ENABLE == 1))
	ctx->buf_width += 2 * ctx->padding_size;
	ctx->buf_height += 2 * ctx->padding_size;
	ctx->RET_DEC_SEQ_INIT_BAK_MP4ASP_VOP_TIME_RES = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_SEQ_TIME_RES);
#endif
	mfc_debug("width = %d, height = %d, buf_width = %d, buf_height = %d\n",	\
			ctx->width, ctx->height, ctx->buf_width, ctx->buf_height);

	/*
	 * getting yuv buffer for this instance
	 * yuv_buf_size is (YUV420 frame size) * (required frame buffer count)
	 */
#if (S3C_MFC_ROTATE_ENABLE == 1)
	/* If rotation is enabled, one more YUV buffer is required */
	yuv_buf_size = ((ctx->buf_width * ctx->buf_height * 3) >> 1) * (frame_need_count + 1);
#else
	yuv_buf_size = ((ctx->buf_width * ctx->buf_height * 3) >> 1) * frame_need_count;
#endif
	yuv_buf_size += 60000;
	if ( s3c_mfc_get_yuv_buffer_addr(ctx, yuv_buf_size) == FALSE ) {
		mfc_err("mfc instance init failed (required frame buffer size = %d)\n",		\
										yuv_buf_size);
		return S3C_MFC_INST_ERR_ETC;
	}
	
	ctx->yuv_buffer_allocated = 1;
	ctx->yuv_buffer_count = frame_need_count;
	ctx->mv_mbyte_addr = ctx->phys_addr_yuv_buffer + (yuv_buf_size - 60000);

	/*
	 * set the parameters in the parameters buffer for SET_FRAME_BUF command. 
	 * buffer address of y, cb, cr will be set in parameters buffer before issuing SET_FRAME_BUF command
	 */
	param_buf = (unsigned char *)s3c_mfc_get_param_buff_virt_addr();
	frame_size = ctx->buf_width * ctx->buf_height;
#if (defined(DIVX_ENABLE) && (DIVX_ENABLE == 1))
	/* s/w divx use padding area, so mfc frame buffer must have stride */
	for (i=0; i < frame_need_count; i++) {
		*((int *)(param_buf + i * 3 * 4)) = ctx->phys_addr_yuv_buffer + i * ((frame_size * 3) >> 1) + 		\
								(ctx->buf_width)*ctx->padding_size+ ctx->padding_size;
		*((int *)(param_buf + i * 3 * 4 + 4)) = ctx->phys_addr_yuv_buffer + i * ((frame_size * 3) >> 1) + 	\
				frame_size + ((ctx->buf_width) / 2) * (ctx->padding_size / 2) + (ctx->padding_size / 2);
		*((int *)(param_buf + i * 3 * 4 + 8)) = ctx->phys_addr_yuv_buffer + i * ((frame_size * 3) >> 1) + 	\
		frame_size + (frame_size >> 2) + ((ctx->buf_width) / 2) * (ctx->padding_size / 2) + (ctx->padding_size / 2);
	}
#else	
	for (i=0; i < frame_need_count; i++) {
		*((int *)(param_buf + i * 3 * 4)) = ctx->phys_addr_yuv_buffer + i * ((frame_size * 3) >> 1);
		*((int *)(param_buf + i * 3 * 4 + 4)) = ctx->phys_addr_yuv_buffer + i * ((frame_size * 3) >> 1) + frame_size;
		*((int *)(param_buf + i * 3 * 4 + 8)) = ctx->phys_addr_yuv_buffer + i * ((frame_size * 3) >> 1) + 	\
											frame_size + (frame_size >> 2);
	}
#endif

	/*
	 * issue the SET_FRAME_BUF command
	 * 'SET_FRAME_BUF_NUM' must be greater than or equal to RET_DEC_SEQ_FRAME_NEED_COUNT
	 */
	writel(frame_need_count, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_SET_FRAME_BUF_NUM);
	writel(ctx->buf_width, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_SET_FRAME_BUF_STRIDE);

	s3c_mfc_issue_command(ctx->inst_no, ctx->codec_mode, SET_FRAME_BUF);

	/*
	 * changing state
	 * change the state to S3C_MFC_INST_STATE_DEC_INITIALIZED
	 * if the input stream data is less than the 2 PARTUNITs size,
	 * then the state is changed to MFCINST_STATE_DEC_PIC_RUN_RING_BUF_LAST_UNITS
	 */
	S3C_MFC_INST_STATE_TRANSITION(ctx, S3C_MFC_INST_STATE_DEC_INITIALIZED);


	return S3C_MFC_INST_RET_OK;
}


int s3c_mfc_instance_init_enc(s3c_mfc_inst_context_t *ctx, s3c_mfc_codec_mode_t codec_mode, s3c_mfc_enc_info_t *enc_info)
{
	int i;
	int yuv_buffer_size;	/* required size in yuv buffer */
	int frame_size;		/* width * height */
	int num_mbs;		/* number of MBs */
	int slices_mb;		/* MB number of slice (only if S3C_MFC_SLICE_MODE_MULTIPLE is selected.) */
	unsigned char *param_buf;	/* PARAM_BUF in BITPROC_BUF */


	/* check parameters from user application */
	if ((enc_info->width & 0x0F) || (enc_info->height & 0x0F)) {
		mfc_err("source picture width and height must be a multiple of 16. width : %d, height : %d\n", \
										enc_info->width, enc_info->height);

		return S3C_MFC_INST_ERR_INVALID_PARAM;
	}

	if (codec_mode < 0 || codec_mode > 6) {
		mfc_err("mfc encoder supports MPEG4, H.264 and H.263\n");
		return S3C_MFC_INST_ERR_INVALID_PARAM;
	}

	if (enc_info->gop_number > 60) {
		mfc_err("maximum gop number is 60.  GOP number = %d\n", enc_info->gop_number);
		return S3C_MFC_INST_ERR_INVALID_PARAM;
	}

	ctx->width = enc_info->width;
	ctx->height = enc_info->height;
	ctx->frame_rate_residual = enc_info->frame_rate_residual;
	ctx->frame_rate_division = enc_info->frame_rate_division;
	ctx->gop_number = enc_info->gop_number;
	ctx->bitrate = enc_info->bitrate;

	/*
	 * At least 2 frame buffers are needed. 
	 * These buffers are used for input buffer in encoder case
	 */
	ctx->yuv_buffer_count = 2;

	/*
	 * this part is not required since the width and the height are checked to be multiples of 16
	 * in the beginning of this function
	 */
	if ((ctx->width & 0x0F) == 0)	/* 16 aligned (ctx->width%16 == 0) */
		ctx->buf_width = ctx->width;
	else
		ctx->buf_width = (ctx->width & 0xFFFFFFF0) + 16;

	/* codec_mode */
	ctx->codec_mode = codec_mode;

	mfc_debug("ctx->inst_no = %d\n", ctx->inst_no);
	mfc_debug("ctx->codec_mode = %d\n", ctx->codec_mode);

	/*
	 * set stream buffer read/write pointer
	 * At first, stream buffer is empty. so write pointer points start of buffer and read pointer points end of buffer
	 */
	switch(ctx->inst_no) {
	case 0:
		writel(ctx->phys_addr_stream_buffer + S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE, 		\
								s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR0);
		writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR0);
		break;
	case 1:
		writel(ctx->phys_addr_stream_buffer + S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE, 		\
								s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR1);
		writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR1);
		break;
	case 2:
		writel(ctx->phys_addr_stream_buffer + S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE, 		\
								s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR2);
		writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR2);
		break;
	case 3:
		writel(ctx->phys_addr_stream_buffer + S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE, 		\
								s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR3);
		writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR3);
		break;
	case 4:
		writel(ctx->phys_addr_stream_buffer + S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE, 		\
								s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR4);
		writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR4);
		break;
	case 5:
		writel(ctx->phys_addr_stream_buffer + S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE, 		\
								s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR5);
		writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR5);
		break;
	case 6:
		writel(ctx->phys_addr_stream_buffer + S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE, 		\
								s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR6);
		writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR6);
		break;
	case 7:
		writel(ctx->phys_addr_stream_buffer + S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE, 		\
								s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR7);
		writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR7);
		break;
	}

	writel(0x1C, s3c_mfc_sfr_base_virt_addr + S3C_MFC_STRM_BUF_CTRL);

	/*
	 * issue the SEQ_INIT command
	 * frame width/height will be returned
	 * set the parameters for SEQ_INIT command
	 */
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_BIT_BUF_ADDR);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_BIT_BUF_SIZE);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_OPTION);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_COD_STD);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_SRC_SIZE);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_SRC_F_RATE);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_MP4_PARA);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_263_PARA);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_264_PARA);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_SLICE_MODE);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_GOP_NUM);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_RC_PARA);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_RC_BUF_SIZE);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_INTRA_MB);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_FMO);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_INTRA_QP);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_ENC_SEQ_SUCCESS);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_RC_OPTION);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_RC_QP_MAX);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_RC_GAMMA);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_TMP_BUF1);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_TMP_BUF2);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_TMP_BUF3);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_TMP_BUF4);

	writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_BIT_BUF_ADDR);
	writel(S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE / 1024, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_BIT_BUF_SIZE);
	writel(S3C_MFC_MB_BIT_REPORT_DISABLE | S3C_MFC_SLICE_INFO_REPORT_DISABLE | S3C_MFC_AUD_DISABLE | \
	   S3C_MFC_MB_QP_REPORT_DISABLE | S3C_MFC_CONST_QP_DISABLE, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_OPTION);
	writel((ctx->width << 10) | ctx->height, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_SRC_SIZE);
	writel((ctx->frame_rate_division << 16) | ctx->frame_rate_residual, 				 \
						s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_SRC_F_RATE);
	writel(S3C_MFC_SLICE_MODE_ONE, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_SLICE_MODE);
	writel(ctx->gop_number, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_GOP_NUM);
	writel(S3C_MFC_RC_ENABLE | (ctx->bitrate << 1) | (SKIP_ENABLE << 31), 				 \
						s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_RC_PARA);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_INTRA_MB);
	writel(FMO_DISABLE, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_FMO);
	writel(S3C_MFC_USE_GAMMA_DISABLE, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_RC_OPTION);

	switch(ctx->codec_mode) {
		case MP4_ENC:
			writel(S3C_MFC_MPEG4_ENCODE, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_COD_STD);
			writel(S3C_MFC_DATA_PART_DISABLE, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_MP4_PARA);	
			break;

		case H263_ENC:
			writel(S3C_MFC_H263_ENCODE, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_COD_STD);
			writel(ctx->h263_annex, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_263_PARA);
			
			if (ctx->enc_num_slices){
				/*
				 * MB size is 16x16 -> width & height are divided by 16 to get number of MBs
				 * division by 16 == shift right by 4-bit
				 */
				num_mbs = (enc_info->width >> 4) * (enc_info->height >> 4);
				slices_mb = (num_mbs / ctx->enc_num_slices);
				writel(S3C_MFC_SLICE_MODE_MULTIPLE | (1 << 1) | (slices_mb << 2), 	\
							s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_SLICE_MODE);
			} else if (ctx->h263_annex == 0) {
				if (((enc_info->width == 704) && (enc_info->height == 576)) || 		\
					((enc_info->width == 352) && (enc_info->height == 288))|| 	\
					((enc_info->width == 176) && (enc_info->height == 144)) ||	\
					((enc_info->width == 128) && (enc_info->height == 96))) {
					mfc_debug("ENC_SEQ_263_PARA = 0x%X\n", 	\
						readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_263_PARA));
				} else {	
					mfc_err("h.263 encoder supports 4cif, cif, qcif and sub-qcif\n");
					mfc_err("when all Annex were off\n");
					return S3C_MFC_INST_ERR_INVALID_PARAM;
				}
			}

			break;

		case AVC_ENC:
			writel(S3C_MFC_H264_ENCODE, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_COD_STD);
			writel(~(0xFFFF), s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_264_PARA);
			if (ctx->enc_num_slices) {
				/* 
				 * MB size is 16x16 -> width & height are divided by 16 to get number of MBs
				 * division by 16 == shift right by 4-bit
				 */
				num_mbs = (enc_info->width >> 4) * (enc_info->height >> 4);
				slices_mb = (num_mbs / ctx->enc_num_slices);
				writel(S3C_MFC_SLICE_MODE_MULTIPLE | (1 << 1) | (slices_mb<< 2), 		\
							s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_SLICE_MODE);
			}

			break;

		default:
			mfc_err("mfc encoder supports mpeg4, h.264 and h.263\n");
			return S3C_MFC_INST_ERR_INVALID_PARAM;
	}

	writel(USER_QP_MAX_DISABLE, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_SEQ_RC_OPTION);
	
	/* SEQ_INIT command */
	s3c_mfc_issue_command(ctx->inst_no, ctx->codec_mode, SEQ_INIT);

	if (readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_ENC_SEQ_SUCCESS) == TRUE) {
		mfc_debug("encoding sequence init success\n");
	} else {
		mfc_err("fail to encoding sequence init\n");
		return S3C_MFC_INST_ERR_ENC_INIT_CMD_FAIL;
	}

	yuv_buffer_size = ((ctx->width * ctx->height * 3) >> 1) * (ctx->yuv_buffer_count + 1);
	if (s3c_mfc_get_yuv_buffer_addr(ctx, yuv_buffer_size) == FALSE) {
		mfc_err("fail to Initialization of MFC instance\n");
		mfc_err("fail to mfc instance inititialization (required frame buffer size = %d)\n", 	\
											yuv_buffer_size);
		return S3C_MFC_INST_ERR_ETC;
	}
	ctx->yuv_buffer_allocated = 1;

	/*
	 * set the parameters in the parameters buffer for SET_FRAME_BUF command
	 * buffer address of y, cb, cr will be set in parameters buffer
	 */
	param_buf = (unsigned char *)s3c_mfc_get_param_buff_virt_addr();
	frame_size = ctx->width * ctx->height;
	for (i=0; i < ctx->yuv_buffer_count; i++) {
		*((int *)(param_buf + i * 3 * 4)) = ctx->phys_addr_yuv_buffer + (i + 1) * ((frame_size * 3) >> 1);
		*((int *)(param_buf + i * 3 * 4 + 4)) = ctx->phys_addr_yuv_buffer + (i + 1) * ((frame_size * 3) >> 1) + \
														frame_size;
		*((int *)(param_buf + i * 3 * 4 + 8)) = ctx->phys_addr_yuv_buffer + (i + 1) * ((frame_size * 3) >> 1) + \
												frame_size + (frame_size >> 2);
	}

	/*
	 * issue the SET_FRAME_BUF command
	 * 'SET_FRAME_BUF_NUM' must be greater than or equal to RET_DEC_SEQ_FRAME_NEED_COUNT
	 */
	writel(ctx->yuv_buffer_count, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_SET_FRAME_BUF_NUM);
	writel(ctx->buf_width, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_SET_FRAME_BUF_STRIDE);
	
	s3c_mfc_issue_command(ctx->inst_no, ctx->codec_mode, SET_FRAME_BUF);

	/*
	 * changing state
	 * state change to S3C_MFC_INST_STATE_ENC_INITIALIZED
	 */
	S3C_MFC_INST_STATE_TRANSITION(ctx, S3C_MFC_INST_STATE_ENC_INITIALIZED);


	return S3C_MFC_INST_RET_OK;
}

/* this function decodes the input stream and put the decoded frame into the yuv buffer */
int s3c_mfc_inst_dec(s3c_mfc_inst_context_t *ctx, unsigned long strm_leng)
{
#if (S3C_MFC_ROTATE_ENABLE == 1)
	int frame_size;	// width * height
#endif
	int frm_size;

	/* checking state */
	if (S3C_MFC_INST_STATE_CHECK(ctx, S3C_MFC_INST_STATE_DELETED)) {
		mfc_err("mfc instance is deleted\n");
		return S3C_MFC_INST_ERR_STATE_DELETED;
	}
	if (S3C_MFC_INST_STATE_PWR_OFF_FLAG_CHECK(ctx)) {
		mfc_err("mfc instance is in Power-Off state.\n");
		return S3C_MFC_INST_ERR_STATE_POWER_OFF;
	}
	if (S3C_MFC_INST_STATE_CHECK(ctx, S3C_MFC_INST_STATE_CREATED)) {
		mfc_err("mfc instance is not initialized\n");
		return S3C_MFC_INST_ERR_STATE_CHK;
	}

	/*
	 * (strm_leng > 0) means that the video stream is waiting for being decoded in the STRM_LINE_BUF
	 * otherwise, no more video streams are available and the decode command will flush the decoded YUV data
	 * which are postponed because of B-frame (VC-1) or reordering (H.264).
	 */
	if (strm_leng > 0) {		
		switch(ctx->inst_no) {
		case 0:
			writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR0);
			writel(ctx->phys_addr_stream_buffer + strm_leng, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR0);
			break;
		case 1:
			writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR1);
			writel(ctx->phys_addr_stream_buffer + strm_leng, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR1);
			break;
		case 2:
			writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR2);
			writel(ctx->phys_addr_stream_buffer + strm_leng, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR2);
			break;
		case 3:
			writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR3);
			writel(ctx->phys_addr_stream_buffer + strm_leng, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR3);
			break;
		case 4:
			writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR4);
			writel(ctx->phys_addr_stream_buffer + strm_leng, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR4);
			break;
		case 5:
			writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR5);
			writel(ctx->phys_addr_stream_buffer + strm_leng, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR5);
			break;
		case 6:
			writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR6);
			writel(ctx->phys_addr_stream_buffer + strm_leng, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR6);
			break;
		case 7:
			writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_RD_PTR7);
			writel(ctx->phys_addr_stream_buffer + strm_leng, s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR7);
			break;
		}

		/* set the parameters in the parameters buffer for PIC_RUN command */
		writel(ctx->post_rotation_mode, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_DEC_PIC_RUN);
		
#if (S3C_MFC_ROTATE_ENABLE == 1)
		if (ctx->post_rotation_mode & 0x0010) {	/* the bit of 'post_rotataion_enable' is 1 */
			unsigned int dec_pic_rot_addr_y;

			frame_size = ctx->buf_width * ctx->buf_height;

			dec_pic_rot_addr_y = ctx->phys_addr_yuv_buffer + ctx->yuv_buffer_count * ((frame_size * 3) >> 1);
			writel(dec_pic_rot_addr_y, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_DEC_PIC_ROT_ADDR_Y);
			writel(dec_pic_rot_addr_y + frame_size, s3c_mfc_sfr_base_virt_addr + 	\
										S3C_MFC_PARAM_DEC_PIC_ROT_ADDR_CB);
			writel(dec_pic_rot_addr_y + frame_size + (frame_size >> 2), 		\
							s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_DEC_PIC_ROT_ADDR_CR);

			/* rotate angle */
			switch (ctx->post_rotation_mode & 0x0003) {
				case 0:	/* 0   degree counterclockwise rotate */
				case 2:	/* 180 degree counterclockwise rotate */
					writel(ctx->buf_width, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_DEC_PIC_ROT_STRIDE);
					
					break;

				case 1:	/* 90  degree counterclockwise rotate */
				case 3:	/* 270 degree counterclockwise rotate */
					writel(ctx->buf_height, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_DEC_PIC_ROT_STRIDE);
					break;
			}
		}
#endif

#if 1
		/* DEC_PIC_OPTION was newly added for MP4ASP */
		frm_size = ctx->buf_width * ctx->buf_height;
		writel(0x7, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_DEC_PIC_OPTION);
		writel(ctx->mv_mbyte_addr, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_DEC_PIC_MV_ADDR);
		writel(ctx->mv_mbyte_addr + 25920, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_DEC_PIC_MBTYPE_ADDR);
#endif
		writel(ctx->phys_addr_stream_buffer & 0xFFFFFFFC, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_DEC_PIC_BB_START);
		writel(ctx->phys_addr_stream_buffer & 0x00000003, 		\
							s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_DEC_PIC_START_BYTE);
		writel(strm_leng, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_DEC_PIC_CHUNK_SIZE);
	} else {
		writel(strm_leng, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_DEC_PIC_RUN);
		s3c_mfc_set_eos(1);
	}

	/* issue the PIC_RUN command */
	if (!s3c_mfc_issue_command(ctx->inst_no, ctx->codec_mode, PIC_RUN)) {
		return S3C_MFC_INST_ERR_DEC_PIC_RUN_CMD_FAIL;
	}
	
	if (readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_PIC_SUCCESS) != 1) {
		mfc_warn("RET_DEC_PIC_SUCCESS is not value of 1(=SUCCESS) value is %d\n", \
			readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_PIC_SUCCESS));
		
	}
	ctx->run_index = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_PIC_IDX);

	if (ctx->run_index > 30) {
		if (ctx->run_index == 0xFFFFFFFF) {		/* RET_DEC_PIC_IDX == -1 */
			mfc_warn("end of stream\n");
			return S3C_MFC_INST_ERR_DEC_EOS;
		} else if (ctx->run_index == 0xFFFFFFFD) {	/* RET_DEC_PIC_IDX == -3 */
			mfc_debug("no picture to be displayed\n");
		} else {
			mfc_err("fail to decoding, ret = %d\n", ctx->run_index);
			return S3C_MFC_INST_ERR_DEC_DECODE_FAIL_ETC;
		}
	}

#if (defined(DIVX_ENABLE) && (DIVX_ENABLE == 1))
	ctx->RET_DEC_PIC_RUN_BAK_BYTE_CONSUMED = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_PIC_BCNT);
	ctx->RET_DEC_PIC_RUN_BAK_MP4ASP_FCODE = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_PIC_FCODE_FWD);
	ctx->RET_DEC_PIC_RUN_BAK_MP4ASP_TIME_BASE_LAST = 	\
					readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_PIC_TIME_BASE_LAST);
	ctx->RET_DEC_PIC_RUN_BAK_MP4ASP_NONB_TIME_LAST = 	\
					readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_PIC_NONB_TIME_LAST);
	ctx->RET_DEC_PIC_RUN_BAK_MP4ASP_MP4ASP_TRD = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_DEC_PIC_TRD);
#endif

	/* 
	 * changing state 
	 * state change to S3C_MFC_INST_STATE_DEC_PIC_RUN_LINE_BUF
	 */
	S3C_MFC_INST_STATE_TRANSITION(ctx, S3C_MFC_INST_STATE_DEC_PIC_RUN_LINE_BUF);

	return S3C_MFC_INST_RET_OK;
}

int s3c_mfc_inst_enc(s3c_mfc_inst_context_t *ctx, int *enc_data_size, int *header_size)
{
	int hdr_size, hdr_size2;
	int size;
	unsigned int bits_wr_ptr_value = 0;
	unsigned char *hdr_buf_tmp=NULL;
	unsigned char	*start, *end;

	/* checking state */
	if (!S3C_MFC_INST_STATE_CHECK(ctx, S3C_MFC_INST_STATE_ENC_INITIALIZED) && 	\
					!S3C_MFC_INST_STATE_CHECK(ctx, S3C_MFC_INST_STATE_ENC_PIC_RUN_LINE_BUF)) {
		mfc_err("mfc encoder instance is not initialized or not using the line buffer\n");
		return S3C_MFC_INST_ERR_STATE_CHK;
	}

	/* the 1st call of this function (s3c_mfc_inst_enc) will generate the stream header (mpeg4:VOL, h264:SPS/PPS) */
	if (S3C_MFC_INST_STATE_CHECK(ctx, S3C_MFC_INST_STATE_ENC_INITIALIZED)) {
		if (ctx->codec_mode == MP4_ENC) {
			/*  ENC_HEADER command  */
			s3c_mfc_inst_enc_header(ctx, 0, 0, ctx->phys_addr_stream_buffer, ctx->stream_buffer_size, \
												&hdr_size);	// VOL

			/* Backup the stream header in the temporary header buffer */
			hdr_buf_tmp = (unsigned char *)kmalloc(hdr_size, GFP_KERNEL);
			if (hdr_buf_tmp) {
				memcpy(hdr_buf_tmp, ctx->stream_buffer, hdr_size);

				start = ctx->stream_buffer;
				size = hdr_size;
				dma_cache_maint(start, size, DMA_FROM_DEVICE);
			} else {
				return S3C_MFC_INST_ERR_MEMORY_ALLOCATION_FAIL;
			}
		} else if (ctx->codec_mode == AVC_ENC) {
			/*  ENC_HEADER command  */
			s3c_mfc_inst_enc_header(ctx, 0, 0, ctx->phys_addr_stream_buffer, ctx->stream_buffer_size,	\
								&hdr_size); /* SPS */
			s3c_mfc_inst_enc_header(ctx, 1, 0, ctx->phys_addr_stream_buffer + (hdr_size + 3), 		\
								ctx->stream_buffer_size-(hdr_size+3), &hdr_size2); /* PPS */

			/* backup the stream header in the temporary header buffer */
			hdr_buf_tmp = (unsigned char *)kmalloc(hdr_size + 3 + hdr_size2, GFP_KERNEL);
			if (hdr_buf_tmp) {
				memcpy(hdr_buf_tmp, ctx->stream_buffer, hdr_size);

				start = ctx->stream_buffer;
				size = hdr_size;
				dma_cache_maint(start, size, DMA_FROM_DEVICE);				
			
				memcpy(hdr_buf_tmp + hdr_size, (unsigned char *)((unsigned int)(ctx->stream_buffer + 	\
								(hdr_size + 3)) & 0xFFFFFFFC), hdr_size2);

				start = ((unsigned int)(ctx->stream_buffer + (hdr_size + 3)) & 0xFFFFFFFC);
				size = hdr_size2;
				dma_cache_maint(start, size, DMA_FROM_DEVICE);				
	
				hdr_size += hdr_size2;
			} else {
                		return S3C_MFC_INST_ERR_MEMORY_ALLOCATION_FAIL;
            		}
		}
	}

	// Dynamic Change for fps of MPEG4 Encoder
	if ( (ctx->codec_mode == MP4_ENC) && 
		S3C_MFC_INST_STATE_CHECK(ctx, S3C_MFC_INST_STATE_ENC_PIC_RUN_LINE_BUF) &&
		(ctx->enc_change_framerate == 1) ) { 
			
    		//  ENC_HEADER command  //
    		s3c_mfc_inst_enc_header(ctx, 0, 0, ctx->phys_addr_stream_buffer, ctx->stream_buffer_size, \
											&hdr_size);	// VOL

    		// Backup the stream header in the temporary header buffer.
    		hdr_buf_tmp = (unsigned char *)kmalloc(hdr_size, GFP_KERNEL);
    		if (hdr_buf_tmp) {
			memcpy(hdr_buf_tmp, ctx->stream_buffer, hdr_size);

			start = ctx->stream_buffer;
			size = hdr_size;
			dma_cache_maint(start, size, DMA_FROM_DEVICE);	
		} else 
			return S3C_MFC_INST_ERR_MEMORY_ALLOCATION_FAIL;            							
    	}

	/* SEI message with recovery point */
	if ((ctx->enc_pic_option & 0x0F000000) && (ctx->codec_mode == AVC_ENC)) {
		/* ENC_HEADER command */
		s3c_mfc_inst_enc_header(ctx, 4, ((ctx->enc_pic_option & 0x0F000000) >> 24), 	\
						ctx->phys_addr_stream_buffer, ctx->stream_buffer_size, &hdr_size); /* SEI */
		/* Backup the stream header in the temporary header buffer */
		hdr_buf_tmp = (unsigned char *)kmalloc(hdr_size, GFP_KERNEL);
		if (hdr_buf_tmp) {
			memcpy(hdr_buf_tmp, ctx->stream_buffer, hdr_size);

			start = ctx->stream_buffer;
			size = hdr_size;
			dma_cache_maint(start, size, DMA_FROM_DEVICE);
		
		} else {
			return S3C_MFC_INST_ERR_MEMORY_ALLOCATION_FAIL;
		}
	}

	/* Set the address of each component of YUV420 */
	writel(ctx->phys_addr_yuv_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_PIC_SRC_ADDR_Y);
	writel(ctx->phys_addr_yuv_buffer + ctx->buf_width * ctx->height, s3c_mfc_sfr_base_virt_addr + 	\
										S3C_MFC_PARAM_ENC_PIC_SRC_ADDR_CB);
	writel(ctx->phys_addr_yuv_buffer + ((ctx->buf_width * ctx->height * 5) >> 2), 			\
							s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_PIC_SRC_ADDR_CR);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_PIC_ROT_MODE);
	writel((ctx->enc_pic_option & 0x0000FFFF), s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_PIC_OPTION);
	writel(ctx->phys_addr_stream_buffer, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_PIC_BB_START);
	writel(ctx->stream_buffer_size, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_PIC_BB_SIZE);

	if (!s3c_mfc_issue_command(ctx->inst_no, ctx->codec_mode, PIC_RUN)) {
		return S3C_MFC_INST_ERR_ENC_PIC_RUN_CMD_FAIL;
	}

	ctx->enc_pic_option = 0; /* reset the encoding picture option at every picture */
	ctx->run_index = 0;
	ctx->enc_change_framerate = 0;	
	
	switch(ctx->inst_no) {
	case 0:
		bits_wr_ptr_value = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR0);
		break;
	case 1:
		bits_wr_ptr_value = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR1);
		break;
	case 2:
		bits_wr_ptr_value = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR2);
		break;
	case 3:
		bits_wr_ptr_value = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR3);
		break;
	case 4:
		bits_wr_ptr_value = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR4);
		break;
	case 5:
		bits_wr_ptr_value = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR5);
		break;
	case 6:
		bits_wr_ptr_value = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR6);
		break;
	case 7:
		bits_wr_ptr_value = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR7);
		break;
	}

	*enc_data_size = bits_wr_ptr_value - ctx->phys_addr_stream_buffer;	
	*header_size = 0;

	if (hdr_buf_tmp) {
		memmove(ctx->stream_buffer + hdr_size, ctx->stream_buffer, *enc_data_size);

		start = ctx->stream_buffer;
		size = hdr_size + (*enc_data_size);
		dma_cache_maint(start, size, DMA_TO_DEVICE);
		
		memcpy(ctx->stream_buffer, hdr_buf_tmp, hdr_size);

		start = ctx->stream_buffer;
		size = hdr_size;
		dma_cache_maint(start, size, DMA_TO_DEVICE);		

		kfree(hdr_buf_tmp);

		*enc_data_size += hdr_size;
		*header_size    = hdr_size;
	}

	/* changing state */
	/* state change to S3C_MFC_INST_STATE_ENC_PIC_RUN_LINE_BUF */
	S3C_MFC_INST_STATE_TRANSITION(ctx, S3C_MFC_INST_STATE_ENC_PIC_RUN_LINE_BUF);


	return S3C_MFC_INST_RET_OK;
}

/* hdr_code == 0: SPS */
/* hdr_code == 1: PPS */
/* hdr_code == 4: SEI */
int s3c_mfc_inst_enc_header(s3c_mfc_inst_context_t *ctx, int hdr_code, int hdr_num, unsigned int outbuf_physical_addr,\
												int outbuf_size, int *hdr_size)
{
	unsigned int bit_wr_ptr_value = 0;


	if (!S3C_MFC_INST_STATE_CHECK(ctx, S3C_MFC_INST_STATE_ENC_INITIALIZED) && 		\
						!S3C_MFC_INST_STATE_CHECK(ctx, S3C_MFC_INST_STATE_ENC_PIC_RUN_LINE_BUF)) {
		mfc_err("mfc encoder instance is not initialized or not using the line buffer\n");
		return S3C_MFC_INST_ERR_STATE_CHK;
	}

	if ((ctx->codec_mode != MP4_ENC) && (ctx->codec_mode != AVC_ENC)) {
		return S3C_MFC_INST_ERR_WRONG_CODEC_MODE;
	}

	/* Set the address of each component of YUV420 */
	writel(hdr_code, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_HEADER_CODE);
	writel(outbuf_physical_addr & 0xFFFFFFFC, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_HEADER_BB_START);
	writel(outbuf_size, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_HEADER_BB_SIZE);
	
	if (hdr_code == 4) /* SEI recovery point */
		writel(hdr_num, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_HEADER_NUM);

	if (!s3c_mfc_issue_command(ctx->inst_no, ctx->codec_mode, ENC_HEADER)) {
		return S3C_MFC_INST_ERR_ENC_HEADER_CMD_FAIL;
	}

	switch (ctx->inst_no) {
	case 0:
		bit_wr_ptr_value = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR0);
		break;
	case 1:
		bit_wr_ptr_value = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR1);
		break;
	case 2:
		bit_wr_ptr_value = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR2);
		break;
	case 3:
		bit_wr_ptr_value = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR3);
		break;
	case 4:
		bit_wr_ptr_value = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR4);
		break;
	case 5:
		bit_wr_ptr_value = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR5);
		break;
	case 6:
		bit_wr_ptr_value = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR6);
		break;
	case 7:
		bit_wr_ptr_value = readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_BIT_STR_WR_PTR7);
		break;
	}

	*hdr_size = bit_wr_ptr_value - readl(s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_HEADER_BB_START);
	

	return S3C_MFC_INST_RET_OK;
}


int s3c_mfc_inst_enc_param_change(s3c_mfc_inst_context_t *ctx, unsigned int param_change_enable, 	\
											unsigned int param_change_val)
{
	int num_mbs; /* number of MBs */
	int slices_mb; /* MB number of slice (only if S3C_MFC_SLICE_MODE_MULTIPLE is selected) */


	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_ENABLE);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_GOP_NUM);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_INTRA_QP);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_BITRATE);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_F_RATE);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_INTRA_REFRESH);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_SLICE_MODE);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_HEC_MODE);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_RESERVED0);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_RESERVED1);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_RESERVED2);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_RESERVED3);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_RESERVED4);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_RESERVED5);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_RESERVED6);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_RESERVED7);
	writel(0x0, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_RET_ENC_CHANGE_SUCCESS);
	writel(param_change_enable, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_ENABLE);

	if (param_change_enable == (1 << 0)) { /* gop number */
		if (param_change_val > 60) {
			mfc_err("mfc encoder parameter change value is invalid\n");
			return S3C_MFC_INST_ERR_ENC_PARAM_CHANGE_INVALID_VALUE;
		}
		writel(param_change_val, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_GOP_NUM);
	} else if (param_change_enable == (1 << 1)) { /* intra qp */
		if (((ctx->codec_mode == MP4_DEC || ctx->codec_mode == H263_DEC) && 		\
					(param_change_val == 0 || param_change_val > 31))	\
					|| (ctx->codec_mode == AVC_DEC && param_change_val > 51)) {
			mfc_err("mfc encoder parameter change value is invalid\n");
			return S3C_MFC_INST_ERR_ENC_PARAM_CHANGE_INVALID_VALUE;
		}
		writel(param_change_val, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_INTRA_QP);
	} else if (param_change_enable == (1 << 2)) { /* bitrate */
		if (param_change_val > 0x07FFF) {
			mfc_err("mfc encoder parameter change value is invalid\n");
			return S3C_MFC_INST_ERR_ENC_PARAM_CHANGE_INVALID_VALUE;
		}
		writel(param_change_val, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_BITRATE);
	} else if (param_change_enable == (1 << 3)) { /* frame rate */
		writel(param_change_val, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_F_RATE);
		ctx->enc_change_framerate = 1;	
	} else if (param_change_enable == (1 << 4)) { /* intra refresh */
		if (param_change_val > ((ctx->width * ctx->height) >> 8)) {
			mfc_err("mfc encoder parameter change value is invalid\n");
			return S3C_MFC_INST_ERR_ENC_PARAM_CHANGE_INVALID_VALUE;
		}
		writel(param_change_val, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_INTRA_REFRESH);
	} else if (param_change_enable == (1 << 5)) { /* slice mode */
		/* 
		 * MB size is 16x16 -> width & height are divided by 16 to get number of MBs
		 * division by 16 == shift right by 4-bit
		 */
		num_mbs = (ctx->width >> 4) * (ctx->height >> 4);

		if (param_change_val > 256 || param_change_val > num_mbs) {
			mfc_err("mfc encoder parameter change value is invalid\n");
			return S3C_MFC_INST_ERR_ENC_PARAM_CHANGE_INVALID_VALUE;
		}

		if (param_change_val == 0) {
			writel(S3C_MFC_SLICE_MODE_ONE, s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_SLICE_MODE);
		} else {
			slices_mb = (num_mbs / param_change_val);
			ctx->enc_num_slices = param_change_val;

			writel(S3C_MFC_SLICE_MODE_MULTIPLE | (1 << 1) | (slices_mb<< 2), 	\
						s3c_mfc_sfr_base_virt_addr + S3C_MFC_PARAM_ENC_CHANGE_SLICE_MODE);
		}
	}

	if (!s3c_mfc_issue_command(ctx->inst_no, ctx->codec_mode, ENC_PARAM_CHANGE)) {
		return S3C_MFC_INST_ERR_ENC_HEADER_CMD_FAIL;
	}


	return S3C_MFC_INST_RET_OK;
}

