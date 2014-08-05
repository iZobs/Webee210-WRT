/* linux/drivers/media/video/samsung/jpeg/jpg_opr.h
 *
 * Driver header file for Samsung JPEG Encoder/Decoder
 *
 * Peter Oh, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __JPG_OPR_H__
#define __JPG_OPR_H__


typedef enum tagJPG_RETURN_STATUS{
	JPG_FAIL,
	JPG_SUCCESS,
	OK_HD_PARSING,
	ERR_HD_PARSING,
	OK_ENC_OR_DEC,
	ERR_ENC_OR_DEC,
	ERR_UNKNOWN
}JPG_RETURN_STATUS;

typedef enum tagIMAGE_TYPE_T{
	JPG_RGB16,
	JPG_YCBYCR,
	JPG_TYPE_UNKNOWN
}IMAGE_TYPE_T;

typedef enum tagSAMPLE_MODE_T{
	JPG_444,
	JPG_422,
	JPG_420, 
	JPG_411,
	JPG_400,
	JPG_SAMPLE_UNKNOWN
}SAMPLE_MODE_T;

typedef enum tagENCDEC_TYPE_T{
	JPG_MAIN,
	JPG_THUMBNAIL
}ENCDEC_TYPE_T;

typedef enum tagIMAGE_QUALITY_TYPE_T{
	JPG_QUALITY_LEVEL_1 = 0, /*high quality*/
	JPG_QUALITY_LEVEL_2,
	JPG_QUALITY_LEVEL_3,
	JPG_QUALITY_LEVEL_4     /*low quality*/
}IMAGE_QUALITY_TYPE_T;

typedef struct tagJPG_DEC_PROC_PARAM{
	SAMPLE_MODE_T	sampleMode;
	ENCDEC_TYPE_T	decType;
	UINT32	width;
	UINT32	height;
	UINT32	dataSize;
	UINT32	fileSize;
} JPG_DEC_PROC_PARAM;

typedef struct tagJPG_ENC_PROC_PARAM{
	SAMPLE_MODE_T	sampleMode;
	ENCDEC_TYPE_T	encType;
	IMAGE_QUALITY_TYPE_T quality;
	UINT32	width;
	UINT32	height;
	UINT32	dataSize;
	UINT32	fileSize;
} JPG_ENC_PROC_PARAM;

JPG_RETURN_STATUS decode_jpg(s3c6400_jpg_ctx *jCTX, JPG_DEC_PROC_PARAM *decParam);
void reset_jpg(s3c6400_jpg_ctx *jCTX);
void decode_header(s3c6400_jpg_ctx *jCTX);
void decode_body(s3c6400_jpg_ctx *jCTX);
SAMPLE_MODE_T get_sample_type(s3c6400_jpg_ctx *jCTX);
void getXY(s3c6400_jpg_ctx *jCTX, UINT32 *x, UINT32 *y);
UINT32 getYUVSize(SAMPLE_MODE_T sampleMode, UINT32 width, UINT32 height);
BOOL is_correct_header(SAMPLE_MODE_T sampleMode, UINT32 *width, UINT32 *height);
void rewrite_header(s3c6400_jpg_ctx *jCTX, UINT32 file_size, UINT32 width, UINT32 height);
void rewrite_yuv(s3c6400_jpg_ctx *jCTX, UINT32 width, UINT32 orgwidth, UINT32 height, UINT32 orgheight);
JPG_RETURN_STATUS encode_jpg(s3c6400_jpg_ctx *jCTX, JPG_ENC_PROC_PARAM	*EncParam);

#endif
