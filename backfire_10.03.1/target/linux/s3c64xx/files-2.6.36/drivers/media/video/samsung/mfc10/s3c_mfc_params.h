/* linux/driver/media/video/mfc/s3c_mfc_params.h
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

#ifndef _S3C_MFC_PARAMS_H
#define _S3C_MFC_PARAMS_H

typedef struct {
	int ret_code;		/* [OUT] Return code */
	int in_width;		/* [IN]  width  of YUV420 frame to be encoded */
	int in_height;		/* [IN]  height of YUV420 frame to be encoded */
	int in_bitrate;		/* [IN]  Encoding parameter: Bitrate (kbps) */
	int in_gopNum;		/* [IN]  Encoding parameter: GOP Number (interval of I-frame) */
	int in_frameRateRes;	/* [IN]  Encoding parameter: Frame rate (Res) */
	int in_frameRateDiv;	/* [IN]  Encoding parameter: Frame rate (Divider) */
	int in_intraqp;         /* [IN] Encoding Parameter: Intra Quantization Parameter */
	int in_qpmax;           /* [IN] Encoding Paramter: Maximum Quantization Paramter */
	float in_gamma;         /* [IN] Encoding Paramter: Gamma Factor for Motion Estimation */
} s3c_mfc_enc_init_arg_t;

typedef struct {
	int ret_code;		/* [OUT] Return code */
	int out_encoded_size;	/* [OUT] Length of Encoded video stream */
	int out_header_size;	/* [OUT] Length of video stream header */
} s3c_mfc_enc_exe_arg_t;

typedef struct {
	int ret_code;		/* [OUT] Return code */
	int in_strmSize;	/* [IN]  Size of video stream filled in STRM_BUF */
	int out_width;		/* [OUT] width  of YUV420 frame */
	int out_height;		/* [OUT] height of YUV420 frame */
	int out_buf_width;	/* [OUT] buffer's width of YUV420 frame */
	int out_buf_height;	/* [OUT] buffer's height of YUV420 frame */
} s3c_mfc_dec_init_arg_t;

typedef struct {
	int ret_code;		/* [OUT] Return code */
	int in_strmSize;	/* [IN]  Size of video stream filled in STRM_BUF */
} s3c_mfc_dec_exe_arg_t;

typedef struct {
	int ret_code;		/* [OUT] Return code */
	int in_usr_data;	/* [IN]  User data for translating Kernel-mode address to User-mode address */
	int in_usr_data2;
	int out_buf_addr;	/* [OUT] Buffer address */
	int out_buf_size;	/* [OUT] Size of buffer address */
} s3c_mfc_get_buf_addr_arg_t;

typedef struct {
	int ret_code;			/* [OUT] Return code */
	int in_config_param;		/* [IN]  Configurable parameter type */
	int in_config_param2;
	int out_config_value[2];	/* [IN]  Values to get for the configurable parameter. */
	/*       Maximum two integer values can be obtained; */
} s3c_mfc_get_config_arg_t;

typedef struct {
	int ret_code;			/* [OUT] Return code */
	int in_config_param;		/* [IN]  Configurable parameter type */
	int in_config_value[3];		/* [IN]  Values to be set for the configurable parameter. */
	/*       Maximum two integer values can be set. */
	int out_config_value_old[2];	/* [OUT] Old values of the configurable parameters */
} s3c_mfc_set_config_arg_t;

typedef struct {
	int		in_usr_mapped_addr;
	int   		ret_code;            /* [OUT] Return code */
	int   		mv_addr;
	int		mb_type_addr;
	unsigned int  	mv_size;
	unsigned int  	mb_type_size;
	unsigned int  	mp4asp_vop_time_res;
	unsigned int  	byte_consumed;
	unsigned int  	mp4asp_fcode;
	unsigned int  	mp4asp_time_base_last;
	unsigned int  	mp4asp_nonb_time_last;
	unsigned int  	mp4asp_trd;
} s3c_mfc_get_mpeg4asp_arg_t;


typedef union {
	s3c_mfc_enc_init_arg_t		enc_init;
	s3c_mfc_enc_exe_arg_t		enc_exe;
	s3c_mfc_dec_init_arg_t		dec_init;
	s3c_mfc_dec_exe_arg_t		dec_exe;
	s3c_mfc_get_buf_addr_arg_t		get_buf_addr;
	s3c_mfc_get_config_arg_t		get_config;
	s3c_mfc_set_config_arg_t		set_config;
	s3c_mfc_get_mpeg4asp_arg_t		mpeg4_asp_param;
} s3c_mfc_args_t;


#define S3C_MFC_GET_CONFIG_DEC_YUV_NEED_COUNT           (0x0AA0C001)
#define S3C_MFC_GET_CONFIG_DEC_MP4ASP_MV                (0x0AA0C002)
#define S3C_MFC_GET_CONFIG_DEC_MP4ASP_MBTYPE            (0x0AA0C003)

#if (defined(DIVX_ENABLE) && (DIVX_ENABLE == 1))
#define S3C_MFC_GET_CONFIG_DEC_MP4ASP_FCODE             (0x0AA0C011)
#define S3C_MFC_GET_CONFIG_DEC_MP4ASP_VOP_TIME_RES      (0x0AA0C012)
#define S3C_MFC_GET_CONFIG_DEC_MP4ASP_TIME_BASE_LAST    (0x0AA0C013)
#define S3C_MFC_GET_CONFIG_DEC_MP4ASP_NONB_TIME_LAST    (0x0AA0C014)
#define S3C_MFC_GET_CONFIG_DEC_MP4ASP_TRD               (0x0AA0C015)
#define S3C_MFC_GET_CONFIG_DEC_BYTE_CONSUMED            (0x0AA0C016)
#endif

#define S3C_MFC_SET_CONFIG_DEC_ROTATE                   (0x0ABDE001)
#define S3C_MFC_SET_CONFIG_DEC_OPTION                   (0x0ABDE002)

#define S3C_MFC_SET_CONFIG_ENC_H263_PARAM               (0x0ABDC001)
#define S3C_MFC_SET_CONFIG_ENC_SLICE_MODE               (0x0ABDC002)
#define S3C_MFC_SET_CONFIG_ENC_PARAM_CHANGE             (0x0ABDC003)
#define S3C_MFC_SET_CONFIG_ENC_CUR_PIC_OPT              (0x0ABDC004)

#define S3C_MFC_SET_CACHE_CLEAN                         (0x0ABDD001)
#define S3C_MFC_SET_CACHE_INVALIDATE                    (0x0ABDD002)
#define S3C_MFC_SET_CACHE_CLEAN_INVALIDATE              (0x0ABDD003)

#define S3C_MFC_SET_PADDING_SIZE                        (0x0ABDE003)

#define S3C_ENC_PARAM_GOP_NUM                           (0x7000A001)
#define S3C_ENC_PARAM_INTRA_QP                          (0x7000A002)
#define S3C_ENC_PARAM_BITRATE                           (0x7000A003)
#define S3C_ENC_PARAM_F_RATE                            (0x7000A004)
#define S3C_ENC_PARAM_INTRA_REF                         (0x7000A005)
#define S3C_ENC_PARAM_SLICE_MODE                        (0x7000A006)

#define S3C_ENC_PIC_OPT_IDR                             (0x7000B001)
#define S3C_ENC_PIC_OPT_SKIP                            (0x7000B002)
#define S3C_ENC_PIC_OPT_RECOVERY                        (0x7000B003)

#define S3C_MFC_DEC_PIC_OPT_MP4ASP                      (0x7000C001)


#endif /* _S3C_MFC_PARAMS_H */

