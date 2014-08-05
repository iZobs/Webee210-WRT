/* linux/driver/media/video/mfc/s3c_mfc_base.h
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

#ifndef _S3C_MFC_BASE_H
#define _S3C_MFC_BASE_H


/* SDRAM buffer control options */
#define S3C_MFC_STREAM_ENDIAN_LITTLE              (0<<0)
#define S3C_MFC_STREAM_ENDIAN_BIG                 (1<<0)
#define S3C_MFC_BUF_STATUS_FULL_EMPTY_CHECK_BIT   (0<<1)
#define S3C_MFC_BUF_STATUS_NO_CHECK_BIT           (1<<1)

/* FRAME_BUF_CTRL (0x110) */
#define S3C_MFC_YUV_MEM_ENDIAN_LITTLE           (0<<0)
#define S3C_MFC_YUV_MEM_ENDIAN_BIG              (1<<0)

/*
 *    PRiSM-CX Video Codec IP's Register
 *    V178
 */

/* 
 * DEC_SEQ_INIT Parameter Register 
 * DEC_SEQ_OPTION (0x18c)
 */
#define S3C_MFC_MP4_DBK_DISABLE                   (0<<0)
#define S3C_MFC_MP4_DBK_ENABLE                    (1<<0)
#define S3C_MFC_REORDER_DISABLE                   (0<<1)
#define S3C_MFC_REORDER_ENABLE                    (1<<1)
#define S3C_MFC_FILEPLAY_ENABLE                   (1<<2)
#define S3C_MFC_FILEPLAY_DISABLE                  (0<<2)
#define S3C_MFC_DYNBUFALLOC_ENABLE                (1<<3)
#define S3C_MFC_DYNBUFALLOC_DISABLE               (0<<3)

/* 
 * ENC_SEQ_INIT Parameter Register 
 * ENC_SEQ_OPTION (0x188)
 */
#define S3C_MFC_MB_BIT_REPORT_DISABLE             (0<<0)
#define S3C_MFC_MB_BIT_REPORT_ENABLE              (1<<0)
#define S3C_MFC_SLICE_INFO_REPORT_DISABLE         (0<<1)
#define S3C_MFC_SLICE_INFO_REPORT_ENABLE          (1<<1)
#define S3C_MFC_AUD_DISABLE                       (0<<2)
#define S3C_MFC_AUD_ENABLE                        (1<<2)
#define S3C_MFC_MB_QP_REPORT_DISABLE              (0<<3)
#define S3C_MFC_MB_QP_REPORT_ENBLE                (1<<3)
#define S3C_MFC_CONST_QP_DISABLE                  (0<<5)
#define S3C_MFC_CONST_QP_ENBLE                    (1<<5)

/* ENC_SEQ_COD_STD (0x18C) */
#define S3C_MFC_MPEG4_ENCODE                      0
#define S3C_MFC_H263_ENCODE                       1
#define S3C_MFC_H264_ENCODE                       2

/* ENC_SEQ_MP4_PARA (0x198) */
#define S3C_MFC_DATA_PART_DISABLE                 (0<<0)
#define S3C_MFC_DATA_PART_ENABLE                  (1<<0)

/* ENC_SEQ_263_PARA (0x19C) */
#define S3C_MFC_ANNEX_T_OFF                       (0<<0)
#define S3C_MFC_ANNEX_T_ON                        (1<<0)
#define S3C_MFC_ANNEX_K_OFF                       (0<<1)
#define S3C_MFC_ANNEX_K_ON                        (1<<1)
#define S3C_MFC_ANNEX_J_OFF                       (0<<2)
#define S3C_MFC_ANNEX_J_ON                        (1<<2)
#define S3C_MFC_ANNEX_I_OFF                       (0<<3)
#define S3C_MFC_ANNEX_I_ON                        (1<<3)

/* ENC_SEQ_SLICE_MODE (0x1A4) */
#define S3C_MFC_SLICE_MODE_ONE                    (0<<0)
#define S3C_MFC_SLICE_MODE_MULTIPLE               (1<<0)

/* ENC_SEQ_RC_PARA (0x1AC) */
#define S3C_MFC_RC_DISABLE                        (0<<0)    /* RC means rate control */
#define S3C_MFC_RC_ENABLE                         (1<<0)
#define S3C_MFC_SKIP_DISABLE                      (1<<31)
#define S3C_MFC_SKIP_ENABLE                       (0<<31)

/* ENC_SEQ_FMO (0x1B8) */
#define S3C_MFC_FMO_DISABLE                       (0<<0)
#define S3C_MFC_FMO_ENABLE                        (1<<0)

/* ENC_SEQ_RC_OPTION (0x1C4) */
#define S3C_MFC_USER_QP_MAX_DISABLE               (0<<0)
#define S3C_MFC_USER_QP_MAX_ENABLE                (1<<0)
#define S3C_MFC_USE_GAMMA_DISABLE                 (0<<1)
#define S3C_MFC_USE_GAMMA_ENABLE                  (1<<1)


typedef enum __MFC_CODEC_MODE {
	MP4_DEC    = 0,
	MP4_ENC    = 1,
	AVC_DEC    = 2,
	AVC_ENC    = 3,
	VC1_DEC    = 4,
	H263_DEC   = 5,
	H263_ENC   = 6
} s3c_mfc_codec_mode_t;

typedef enum __MFC_COMMAND {
	SEQ_INIT         = 0x01,
	SEQ_END          = 0x02,
	PIC_RUN          = 0x03,
	SET_FRAME_BUF    = 0x04,
	ENC_HEADER       = 0x05,
	ENC_PARA_SET     = 0x06,
	DEC_PARA_SET     = 0x07,
	ENC_PARAM_CHANGE = 0x09,
	SLEEP            = 0x0A,
	WAKEUP           = 0x0B,
	GET_FW_VER       = 0x0F
} s3c_mfc_command_t;

/* 
 * Because SW_RESET register is located apart(address 0xe00), unlike other MFC_SFR registers, 
 * I have excluded it in S3C6400_MFC_SFR struct and defined relative address only.  
 * When do virtual memory mapping in setting up memory, we have to map until this SW_RESET register.  
 *
 * #define S3C6400_MFC_SFR_SW_RESET_ADDR    (0x0e00)
 * #define S3C6400_MFC_SFR_SIZE             (0x0e00)
 */


#endif /* _S3C_MFC_BASE_H */


