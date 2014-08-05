/* linux/drivers/media/video/samsung/jpeg_v2/jpg_opr.c
 *
 * Driver file for Samsung JPEG Encoder/Decoder
 *
 * Peter Oh, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/delay.h>
#include <asm/io.h>

#include "jpg_mem.h"
#include "jpg_misc.h"
#include "jpg_opr.h"
#include "jpg_conf.h"
#include "log_msg.h"

#include "regs-jpeg.h"

extern void __iomem		*s3c_jpeg_base;
extern int			jpg_irq_reason;

enum {
	UNKNOWN,
	BASELINE = 0xC0,
	EXTENDED_SEQ = 0xC1,
	PROGRESSIVE = 0xC2
} jpg_sof_marker;

/*----------------------------------------------------------------------------
*Function: wait_for_interrupt

*Parameters:	void
*Return Value:
*Implementation Notes:
-----------------------------------------------------------------------------*/
jpg_return_status wait_for_interrupt(void)
{
	if (interruptible_sleep_on_timeout(&wait_queue_jpeg, INT_TIMEOUT) == 0) {
		log_msg(LOG_ERROR, "s3c_jpeg_ioctl", "Waiting for interrupt is timeout\n");
	}

	return jpg_irq_reason;
}
/*----------------------------------------------------------------------------
*Function: decode_jpg

*Parameters:	jpg_ctx:
				input_buff:
				input_size:
				output_buff:
				output_size
*Return Value:
*Implementation Notes:
-----------------------------------------------------------------------------*/
jpg_return_status decode_jpg(sspc100_jpg_ctx *jpg_ctx,
			     jpg_dec_proc_param *dec_param)
{
	volatile int		ret;
	sample_mode_t sample_mode;
	UINT32	width, height;
	log_msg(LOG_TRACE, "decode_jpg", "decode_jpg function\n");

	if (jpg_ctx)
		reset_jpg(jpg_ctx);
	else {
		log_msg(LOG_ERROR, "\ndecode_jpg", "DD::JPG CTX is NULL\n");
		return JPG_FAIL;
	}

	////////////////////////////////////////
	// Header Parsing				      //
	////////////////////////////////////////

	decode_header(jpg_ctx, dec_param);
	ret = wait_for_interrupt();

	if (ret != OK_HD_PARSING) {
		log_msg(LOG_ERROR, "\ndecode_jpg", "DD::JPG Header Parsing Error(%d)\r\n", ret);
		return JPG_FAIL;
	}

	sample_mode = get_sample_type(jpg_ctx);
	log_msg(LOG_TRACE, "decode_jpg", "sample_mode : %d\n", sample_mode);

	if (sample_mode == JPG_SAMPLE_UNKNOWN) {
		log_msg(LOG_ERROR, "decode_jpg", "DD::JPG has invalid sample_mode\r\n");
		return JPG_FAIL;
	}

	dec_param->sample_mode = sample_mode;

	get_xy(jpg_ctx, &width, &height);
	log_msg(LOG_TRACE, "decode_jpg", "DD:: width : %d height : %d\n", width, height);

	if (width <= 0 || width > MAX_JPG_WIDTH || height <= 0 || height > MAX_JPG_HEIGHT) {
		log_msg(LOG_ERROR, "decode_jpg", "DD::JPG has invalid width(%d)/height(%d)\n",width, height);
		return JPG_FAIL;
	}

	////////////////////////////////////////
	// Body Decoding									   //
	////////////////////////////////////////

	decode_body(jpg_ctx);

	ret = wait_for_interrupt();

	if (ret != OK_ENC_OR_DEC) {
		log_msg(LOG_ERROR, "decode_jpg", "DD::JPG Body Decoding Error(%d)\n", ret);
		return JPG_FAIL;
	}

	dec_param->data_size = get_yuv_size(dec_param->out_format, width, height);
	dec_param->width = width;
	dec_param->height = height;

	return JPG_SUCCESS;
}

/*----------------------------------------------------------------------------
*Function: reset_jpg

*Parameters:	jpg_ctx:
*Return Value:
*Implementation Notes:
-----------------------------------------------------------------------------*/
void reset_jpg(sspc100_jpg_ctx *jpg_ctx)
{
	log_msg(LOG_TRACE, "reset_jpg", "s3c_jpeg_base 0x%08x \n", s3c_jpeg_base);
	__raw_writel(S3C_JPEG_SW_RESET_REG_ENABLE, s3c_jpeg_base + S3C_JPEG_SW_RESET_REG);

	do {
		__raw_writel(S3C_JPEG_SW_RESET_REG_ENABLE, s3c_jpeg_base + S3C_JPEG_SW_RESET_REG);
	} while (((readl(s3c_jpeg_base + S3C_JPEG_SW_RESET_REG)) & S3C_JPEG_SW_RESET_REG_ENABLE) == S3C_JPEG_SW_RESET_REG_ENABLE);
}

/*----------------------------------------------------------------------------
*Function: decode_header

*Parameters:	jpg_ctx:
*Return Value:
*Implementation Notes:
-----------------------------------------------------------------------------*/
void decode_header(sspc100_jpg_ctx *jpg_ctx, jpg_dec_proc_param *dec_param)
{
	log_msg(LOG_TRACE, "decode_header", "decode_header function\n");
	__raw_writel(jpg_ctx->jpg_data_addr, s3c_jpeg_base + S3C_JPEG_JPGADR_REG);

	__raw_writel(__raw_readl(s3c_jpeg_base + S3C_JPEG_MOD_REG) | (S3C_JPEG_MOD_REG_PROC_DEC), s3c_jpeg_base + S3C_JPEG_MOD_REG);	//decoding mode
	__raw_writel(__raw_readl(s3c_jpeg_base + S3C_JPEG_CMOD_REG) & (~S3C_JPEG_CMOD_REG_MOD_HALF_EN_HALF), s3c_jpeg_base + S3C_JPEG_CMOD_REG);
	__raw_writel(__raw_readl(s3c_jpeg_base + S3C_JPEG_CLKCON_REG) | (S3C_JPEG_CLKCON_REG_POWER_ON_ACTIVATE), s3c_jpeg_base + S3C_JPEG_CLKCON_REG);
	__raw_writel(__raw_readl(s3c_jpeg_base + S3C_JPEG_INTSE_REG) & ~(0x7f << 0), s3c_jpeg_base + S3C_JPEG_INTSE_REG);
	__raw_writel(__raw_readl(s3c_jpeg_base + S3C_JPEG_INTSE_REG) | (S3C_JPEG_INTSE_REG_ERR_INT_EN	| S3C_JPEG_INTSE_REG_HEAD_INT_EN_ENABLE | S3C_JPEG_INTSE_REG_INT_EN), s3c_jpeg_base + S3C_JPEG_INTSE_REG);
	__raw_writel(__raw_readl(s3c_jpeg_base + S3C_JPEG_OUTFORM_REG) & ~(S3C_JPEG_OUTFORM_REG_YCBCY420), s3c_jpeg_base + S3C_JPEG_OUTFORM_REG);
	__raw_writel(__raw_readl(s3c_jpeg_base + S3C_JPEG_OUTFORM_REG) | (dec_param->out_format << 0), s3c_jpeg_base + S3C_JPEG_OUTFORM_REG);
	__raw_writel(__raw_readl(s3c_jpeg_base + S3C_JPEG_DEC_STREAM_SIZE_REG) & ~(S3C_JPEG_DEC_STREAM_SIZE_REG_PROHIBIT), s3c_jpeg_base + S3C_JPEG_DEC_STREAM_SIZE_REG);
	//__raw_writel(dec_param->file_size, s3c_jpeg_base + S3C_JPEG_DEC_STREAM_SIZE_REG);
	__raw_writel(__raw_readl(s3c_jpeg_base + S3C_JPEG_JSTART_REG) | S3C_JPEG_JSTART_REG_ENABLE, s3c_jpeg_base + S3C_JPEG_JSTART_REG);
}

/*----------------------------------------------------------------------------
*Function: decode_body

*Parameters:	jpg_ctx:
*Return Value:
*Implementation Notes:
-----------------------------------------------------------------------------*/
void decode_body(sspc100_jpg_ctx *jpg_ctx)
{
	log_msg(LOG_TRACE, "decode_body", "decode_body function\n");
	__raw_writel(jpg_ctx->img_data_addr, s3c_jpeg_base + S3C_JPEG_IMGADR_REG);
	__raw_writel(__raw_readl(s3c_jpeg_base + S3C_JPEG_JRSTART_REG) | S3C_JPEG_JRSTART_REG_ENABLE, s3c_jpeg_base + S3C_JPEG_JRSTART_REG);
}

/*----------------------------------------------------------------------------
*Function:	get_sample_type

*Parameters:	jpg_ctx:
*Return Value:
*Implementation Notes:
-----------------------------------------------------------------------------*/
sample_mode_t get_sample_type(sspc100_jpg_ctx *jpg_ctx)
{
	ULONG		jpgMode;
	sample_mode_t   sample_mode = JPG_SAMPLE_UNKNOWN;

	jpgMode = __raw_readl(s3c_jpeg_base + S3C_JPEG_MOD_REG);

	sample_mode =
		((jpgMode & JPG_SMPL_MODE_MASK) == JPG_444) ? JPG_444 :
		((jpgMode & JPG_SMPL_MODE_MASK) == JPG_422) ? JPG_422 :
		((jpgMode & JPG_SMPL_MODE_MASK) == JPG_420) ? JPG_420 :
		((jpgMode & JPG_SMPL_MODE_MASK) == JPG_400) ? JPG_400 :
		((jpgMode & JPG_SMPL_MODE_MASK) == JPG_411) ? JPG_411 : JPG_SAMPLE_UNKNOWN;

	return(sample_mode);
}

/*----------------------------------------------------------------------------
*Function:	get_xy

*Parameters:	jpg_ctx:
				x:
				y:
*Return Value:
*Implementation Notes:
-----------------------------------------------------------------------------*/
void get_xy(sspc100_jpg_ctx *jpg_ctx, UINT32 *x, UINT32 *y)
{
	*x = __raw_readl(s3c_jpeg_base + S3C_JPEG_X_REG);
	*y = __raw_readl(s3c_jpeg_base + S3C_JPEG_Y_REG);
}

/*----------------------------------------------------------------------------
*Function: get_yuv_size

*Parameters:	sample_mode:
				width:
				height:
*Return Value:
*Implementation Notes:
-----------------------------------------------------------------------------*/
UINT32 get_yuv_size(out_mode_t out_format, UINT32 width, UINT32 height)
{
	switch (out_format) {
	case YCBCR_422 :

		if (width % 16 != 0)
			width += 16 - (width % 16);

		if (height % 8 != 0)
			height += 8 - (height % 8);

		break;

	case YCBCR_420 :

		if (width % 16 != 0)
			width += 16 - (width % 16);

		if (height % 16 != 0)
			height += 16 - (height % 16);

		break;

	case YCBCR_SAMPLE_UNKNOWN:
		break;
	}

	log_msg(LOG_TRACE, "get_yuv_size", " width(%d) height(%d)\n", width, height);

	switch (out_format) {
	case YCBCR_422 :
		return(width*height*2);
	case YCBCR_420 :
		return((width*height) + (width*height >> 1));
	default :
		return(0);
	}
}

/*----------------------------------------------------------------------------
*Function: reset_jpg

*Parameters:	jpg_ctx:
*Return Value:
*Implementation Notes:
-----------------------------------------------------------------------------*/
jpg_return_status encode_jpg(sspc100_jpg_ctx *jpg_ctx,
			     jpg_enc_proc_param	*enc_param)
{

	UINT	i, ret;
	UINT32	cmd_val;

	if (enc_param->width <= 0 || enc_param->width > MAX_JPG_WIDTH
	    || enc_param->height <= 0 || enc_param->height > MAX_JPG_HEIGHT) {
		log_msg(LOG_ERROR, "encode_jpg", "DD::Encoder : width: %d, height: %d \n", enc_param->width, enc_param->height);
		log_msg(LOG_ERROR, "encode_jpg", "DD::Encoder : Invalid width/height \n");
		return JPG_FAIL;
	}

	reset_jpg(jpg_ctx);
	cmd_val = (enc_param->sample_mode == JPG_422) ? (S3C_JPEG_MOD_REG_SUBSAMPLE_422) : (S3C_JPEG_MOD_REG_SUBSAMPLE_420);
	__raw_writel(cmd_val, s3c_jpeg_base + S3C_JPEG_MOD_REG);
	__raw_writel(__raw_readl(s3c_jpeg_base + S3C_JPEG_CMOD_REG)& (~S3C_JPEG_CMOD_REG_MOD_HALF_EN_HALF), s3c_jpeg_base + S3C_JPEG_CMOD_REG);
	__raw_writel(__raw_readl(s3c_jpeg_base + S3C_JPEG_CLKCON_REG) | (S3C_JPEG_CLKCON_REG_POWER_ON_ACTIVATE), s3c_jpeg_base + S3C_JPEG_CLKCON_REG);
	__raw_writel(__raw_readl(s3c_jpeg_base + S3C_JPEG_CMOD_REG) | (enc_param->in_format << JPG_MODE_SEL_BIT), s3c_jpeg_base + S3C_JPEG_CMOD_REG);
	__raw_writel(JPG_RESTART_INTRAVEL, s3c_jpeg_base + S3C_JPEG_DRI_REG);
	__raw_writel(enc_param->width, s3c_jpeg_base + S3C_JPEG_X_REG);
	__raw_writel(enc_param->height, s3c_jpeg_base + S3C_JPEG_Y_REG);

	log_msg(LOG_TRACE, "encode_jpg", "enc_param->enc_type : %d\n", enc_param->enc_type);

	if (enc_param->enc_type == JPG_MAIN) {
		log_msg(LOG_TRACE, "encode_jpg", "encode image size width: %d, height: %d\n", enc_param->width, enc_param->height);
		__raw_writel(jpg_ctx->img_data_addr, s3c_jpeg_base + S3C_JPEG_IMGADR_REG);
		__raw_writel(jpg_ctx->jpg_data_addr, s3c_jpeg_base + S3C_JPEG_JPGADR_REG);
	} else { // thumbnail encoding
		log_msg(LOG_TRACE, "encode_jpg", "thumb image size width: %d, height: %d\n", enc_param->width, enc_param->height);
		__raw_writel(jpg_ctx->img_thumb_data_addr, s3c_jpeg_base + S3C_JPEG_IMGADR_REG);
		__raw_writel(jpg_ctx->jpg_thumb_data_addr, s3c_jpeg_base + S3C_JPEG_JPGADR_REG);
	}

	__raw_writel(COEF1_RGB_2_YUV, s3c_jpeg_base + S3C_JPEG_COEF1_REG); 	// Coefficient value 1 for RGB to YCbCr
	__raw_writel(COEF2_RGB_2_YUV, s3c_jpeg_base + S3C_JPEG_COEF2_REG); 	// Coefficient value 2 for RGB to YCbCr
	__raw_writel(COEF3_RGB_2_YUV, s3c_jpeg_base + S3C_JPEG_COEF3_REG);	// Coefficient value 3 for RGB to YCbCr

	// Quantiazation and Huffman Table setting
	for (i = 0; i < 64; i++)
		__raw_writel((UINT32)qtbl_luminance[enc_param->quality][i], s3c_jpeg_base + S3C_JPEG_QTBL0_REG + (i*0x04));

	for (i = 0; i < 64; i++)
		__raw_writel((UINT32)qtbl_chrominance[enc_param->quality][i], s3c_jpeg_base + S3C_JPEG_QTBL1_REG + (i*0x04));

	for (i = 0; i < 16; i++)
		__raw_writel((UINT32)hdctbl0[i], s3c_jpeg_base + S3C_JPEG_HDCTBL0_REG + (i*0x04));

	for (i = 0; i < 12; i++)
		__raw_writel((UINT32)hdctblg0[i], s3c_jpeg_base + S3C_JPEG_HDCTBLG0_REG + (i*0x04));

	for (i = 0; i < 16; i++)
		__raw_writel((UINT32)hactbl0[i], s3c_jpeg_base + S3C_JPEG_HACTBL0_REG + (i*0x04));

	for (i = 0; i < 162; i++)
		__raw_writel((UINT32)hactblg0[i], s3c_jpeg_base + S3C_JPEG_HACTBLG0_REG + (i*0x04));

	__raw_writel(S3C_JPEG_QHTBL_REG_QT_NUM2 | S3C_JPEG_QHTBL_REG_QT_NUM3, s3c_jpeg_base + S3C_JPEG_QHTBL_REG);

	__raw_writel(__raw_readl(s3c_jpeg_base + S3C_JPEG_JSTART_REG) | S3C_JPEG_JSTART_REG_ENABLE, s3c_jpeg_base + S3C_JPEG_JSTART_REG);
	ret = wait_for_interrupt();

	if (ret != OK_ENC_OR_DEC) {
		log_msg(LOG_ERROR, "encode_jpg", "DD::JPG Encoding Error(%d)\n", ret);
		return JPG_FAIL;
	}

	enc_param->file_size = __raw_readl(s3c_jpeg_base + S3C_JPEG_CNT_REG);
	log_msg(LOG_TRACE, "encode_jpg", "encoded file size : %d\n", enc_param->file_size);
	return JPG_SUCCESS;

}
