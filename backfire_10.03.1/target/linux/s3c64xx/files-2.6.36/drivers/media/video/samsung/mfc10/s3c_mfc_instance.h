/* linux/driver/media/video/mfc/s3c_mfc_instance.h
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

#ifndef _S3C_MFC_INSTANCE_H
#define _S3C_MFC_INSTANCE_H


#include "s3c_mfc_base.h"
#include "s3c_mfc_types.h"


typedef enum
{
	S3C_MFC_INST_STATE_DELETED = 0,			/* instance is deleted */
	S3C_MFC_INST_STATE_CREATED         = 10,	/* instance is created but not initialized */
	S3C_MFC_INST_STATE_DEC_INITIALIZED = 20,	/* instance is initialized for decoding */
	S3C_MFC_INST_STATE_DEC_PIC_RUN_LINE_BUF,
	S3C_MFC_INST_STATE_ENC_INITIALIZED = 30,	/* instance is initialized for encoding */
	S3C_MFC_INST_STATE_ENC_PIC_RUN_LINE_BUF,
} s3c_mfc_instance_state_t;


#define S3C_MFC_INST_STATE_PWR_OFF_FLAG		0x40000000
#define S3C_MFC_INST_STATE_BUF_FILL_REQ		0x80000000

#define S3C_MFC_INST_STATE_TRANSITION(inst_ctx, state)	((inst_ctx)->state_var =  (state))
#define S3C_MFC_INST_STATE(inst_ctx)			((inst_ctx)->state_var & 0x0FFFFFFF)
#define S3C_MFC_INST_STATE_CHECK(inst_ctx, state)	(((inst_ctx)->state_var & 0x0FFFFFFF) == (state))

#define S3C_MFC_INST_STATE_PWR_OFF_FLAG_SET(inst_ctx)	((inst_ctx)->state_var |= S3C_MFC_INST_STATE_PWR_OFF_FLAG)
#define S3C_MFC_INST_STATE_PWR_OFF_FLAG_CLEAR(inst_ctx)	((inst_ctx)->state_var &= ~S3C_MFC_INST_STATE_PWR_OFF_FLAG)
#define S3C_MFC_INST_STATE_PWR_OFF_FLAG_CHECK(inst_ctx)	((inst_ctx)->state_var & S3C_MFC_INST_STATE_PWR_OFF_FLAG)
#define S3C_MFC_INST_STATE_BUF_FILL_REQ_SET(inst_ctx)	((inst_ctx)->state_var |= S3C_MFC_INST_STATE_BUF_FILL_REQ)
#define S3C_MFC_INST_STATE_BUF_FILL_REQ_CLEAR(inst_ctx)	((inst_ctx)->state_var &= ~S3C_MFC_INST_STATE_BUF_FILL_REQ)
#define S3C_MFC_INST_STATE_BUF_FILL_REQ_CHECK(inst_ctx)	((inst_ctx)->state_var & S3C_MFC_INST_STATE_BUF_FILL_REQ)


typedef struct
{
	int		inst_no;

	s3c_mfc_codec_mode_t	codec_mode;

	unsigned char	*stream_buffer;			/* stream buffer pointer (virtual address) */
	unsigned int	phys_addr_stream_buffer;	/* stream buffer physical address */
	unsigned int	stream_buffer_size;		/* stream buffer size */

	unsigned char	*yuv_buffer;			/* yuv buffer pointer (virtual address) */
	unsigned int	phys_addr_yuv_buffer;		/* yuv buffer physical address */
	unsigned int	yuv_buffer_size;		/* yuv buffer size */
	unsigned int	mv_mbyte_addr;       		/* phyaical address of MV and MByte tables */


	int		yuv_buffer_allocated;

	int		width, height;
	int		buf_width, buf_height;		/* buf_width is stride. */
	
	int		yuv_buffer_count;		/* decoding case: RET_DEC_SEQ_FRAME_NEED_COUNT */
	                              			/* encoding case: fixed at 2 (at lease 2 frame buffers) */

	unsigned int    post_rotation_mode;

	unsigned int    dec_pic_option;			/* 0-th bit : MP4ASP FLAG, */
							/* 1-st bit : MV REPORT ENABLE, */
							/* 2-nd bit : MBTYPE REPORT ENABLE */
	/* encoding configuration info */
	int		frame_rate_residual;
	int		frame_rate_division;
	int		gop_number;
	int		bitrate;

	/* encoding configuration info (misc.) */
	int		h263_annex;
	int		enc_num_slices;

	/* encoding picture option */
	unsigned int	enc_pic_option;			/* 0-th bit : S3C_ENC_PIC_OPT_SKIP, */
							/* 1-st bit : S3C_ENC_PIC_OPT_IDR, */
							/* 24-th bit: S3C_ENC_PIC_OPT_RECOVERY */
	unsigned int	enc_change_framerate;  		/* when Frame Rate changing */							
	int		frame_num;			/* DEC_PIC_FRAME_NUM */
	int		run_index;			/* DEC_PIC_RUN_IDX */

	s3c_mfc_instance_state_t   state_var;		/* State Variable */

#if (defined(DIVX_ENABLE) && (DIVX_ENABLE == 1))

	unsigned int	padding_size;

	/* RET_DEC_PIC_RUN_BAK_XXXXX : MP4ASP(DivX) related values that is returned        */
	/*                         on DEC_PIC_RUN command.                                 */
	/* RET_DEC_SEQ_INIT_BAK_XXXXX : MP4ASP(DivX) related values that is returned       */
	/*                         on DEC_SEQ_INIT command.                                */
	/* They are maintained in the context structure variable.                          */
	unsigned int	RET_DEC_SEQ_INIT_BAK_MP4ASP_VOP_TIME_RES;
	unsigned int	RET_DEC_PIC_RUN_BAK_BYTE_CONSUMED;
	unsigned int	RET_DEC_PIC_RUN_BAK_MP4ASP_FCODE;
	unsigned int	RET_DEC_PIC_RUN_BAK_MP4ASP_TIME_BASE_LAST;
	unsigned int	RET_DEC_PIC_RUN_BAK_MP4ASP_NONB_TIME_LAST;
	unsigned int	RET_DEC_PIC_RUN_BAK_MP4ASP_MP4ASP_TRD;
#endif
} s3c_mfc_inst_context_t;


typedef struct {
	int width;
	int height;
	int frame_rate_residual;
	int frame_rate_division;
	int gop_number;
	int bitrate;
} s3c_mfc_enc_info_t;

/* future work
#define MFCINST_MP4_QPMAX	31
#define MFCINST_MP4_QPMIN	1
#define MFCINST_H264_QPMAX	51
#define MFCINST_H264_QPMIN	0

#define MFCINST_GAMMA_FACTOR	0.75
#define MFCINST_GAMMA_FACTEE	32768
*/

s3c_mfc_inst_context_t *s3c_mfc_inst_get_context(int inst_no);
int  s3c_mfc_inst_get_no(s3c_mfc_inst_context_t *ctx);
BOOL s3c_mfc_inst_get_stream_buff_rw_ptrs(s3c_mfc_inst_context_t *ctx, unsigned char **read_ptr, unsigned char **write_ptr);

s3c_mfc_inst_context_t *s3c_mfc_inst_create(void);
void s3c_mfc_inst_del(s3c_mfc_inst_context_t *ctx);

void s3c_mfc_inst_pow_off_state(s3c_mfc_inst_context_t *ctx);
void s3c_mfc_inst_pow_on_state(s3c_mfc_inst_context_t *ctx);

int  s3c_mfc_inst_init_dec(s3c_mfc_inst_context_t *ctx, s3c_mfc_codec_mode_t codec_mode, unsigned long strm_leng);
int  s3c_mfc_inst_dec(s3c_mfc_inst_context_t *ctx, unsigned long arg);

int  s3c_mfc_instance_init_enc(s3c_mfc_inst_context_t *ctx, s3c_mfc_codec_mode_t codec_mode, s3c_mfc_enc_info_t *enc_info);
int  s3c_mfc_inst_enc(s3c_mfc_inst_context_t *ctx, int *enc_data_size, int *header_size);
int  s3c_mfc_inst_enc_header(s3c_mfc_inst_context_t *ctx, int hdr_code, int hdr_num, unsigned int outbuf_physical_addr, int outbuf_size, int *hdr_size);
int  s3c_mfc_inst_enc_param_change(s3c_mfc_inst_context_t *ctx, unsigned int param_change_enable, unsigned int param_change_val);

unsigned int s3c_mfc_inst_set_post_rotate(s3c_mfc_inst_context_t *ctx, unsigned int post_rotmode);

int  s3c_mfc_inst_get_line_buff(s3c_mfc_inst_context_t *ctx, unsigned char **buffer, int *size);
int  s3c_mfc_inst_get_yuv_buff(s3c_mfc_inst_context_t *ctx, unsigned char **buffer, int *size);

#define S3C_MFC_INST_RET_OK				(0)

#define S3C_MFC_INST_ERR_INVALID_PARAM			(-1001)
#define S3C_MFC_INST_ERR_STATE_CHK			(-1002)
#define S3C_MFC_INST_ERR_STATE_DELETED			(-1003)
#define S3C_MFC_INST_ERR_STATE_POWER_OFF		(-1004)
#define S3C_MFC_INST_ERR_WRONG_CODEC_MODE		(-1005)

#define S3C_MFC_INST_ERR_DEC_INIT_CMD_FAIL		(-2001)
#define S3C_MFC_INST_ERR_DEC_PIC_RUN_CMD_FAIL		(-2002)
#define S3C_MFC_INST_ERR_DEC_DECODE_FAIL_ETC		(-2011)
#define S3C_MFC_INST_ERR_DEC_INVALID_STRM		(-2012)
#define S3C_MFC_INST_ERR_DEC_EOS			(-2013)
#define S3C_MFC_INST_ERR_DEC_BUF_FILL_SIZE_WRONG	(-2014)

#define S3C_MFC_INST_ERR_ENC_INIT_CMD_FAIL		(-3001)
#define S3C_MFC_INST_ERR_ENC_PIC_RUN_CMD_FAIL		(-3002)
#define S3C_MFC_INST_ERR_ENC_HEADER_CMD_FAIL		(-3003)
#define S3C_MFC_INST_ERR_ENC_PARAM_CHANGE_INVALID_VALUE	(-3011)

#define S3C_MFC_INST_ERR_MEMORY_ALLOCATION_FAIL		(-4001)

#define S3C_MFC_INST_ERR_ETC				(-9001)


#endif /* _S3C_MFC_INSTANCE_H */
