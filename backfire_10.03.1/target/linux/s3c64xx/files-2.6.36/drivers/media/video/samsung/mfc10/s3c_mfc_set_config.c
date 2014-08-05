/* linux/driver/media/video/mfc/s3c_mfc_set_config.c
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

#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <asm/memory.h>
#include <linux/kernel.h>

#include "s3c_mfc_params.h"
#include "s3c_mfc_instance.h"
#include "s3c_mfc_config.h"
#include "s3c_mfc_sfr.h"
#include "s3c_mfc.h"

/* Input arguments for S3C_MFC_IOCTL_MFC_SET_CONFIG */
int s3c_mfc_get_config_params(s3c_mfc_inst_context_t *mfc_inst, s3c_mfc_args_t *args )
{
	int             ret;


	switch (args->get_config.in_config_param) {
	case S3C_MFC_GET_CONFIG_DEC_YUV_NEED_COUNT:
		args->get_config.out_config_value[0] = mfc_inst->yuv_buffer_count;
		ret = S3C_MFC_INST_RET_OK;

		break;

	case S3C_MFC_GET_CONFIG_DEC_MP4ASP_MV:
	case S3C_MFC_GET_CONFIG_DEC_MP4ASP_MBTYPE:
		/* "S3C_MFC_GET_CONFIG_DEC_MP4ASP_MV" and "S3C_MFC_GET_CONFIG_DEC_MP4ASP_MBTYPE" are processed in the upper function. */
		ret = S3C_MFC_INST_RET_OK;

		break;

#if (defined(DIVX_ENABLE) && (DIVX_ENABLE == 1))
	case S3C_MFC_GET_CONFIG_DEC_BYTE_CONSUMED:
		args->get_config.out_config_value[0] = (int)mfc_inst->RET_DEC_PIC_RUN_BAK_BYTE_CONSUMED; 
		mfc_debug("S3C_MFC_GET_CONFIG_DEC_BYTE_CONSUMED = %d\n", \
			(int)mfc_inst->RET_DEC_PIC_RUN_BAK_BYTE_CONSUMED);
		ret = S3C_MFC_INST_RET_OK;
		break;

	case S3C_MFC_GET_CONFIG_DEC_MP4ASP_FCODE:
		args->get_config.out_config_value[0] = (int)mfc_inst->RET_DEC_PIC_RUN_BAK_MP4ASP_FCODE;
		ret = S3C_MFC_INST_RET_OK;
		break;

	case S3C_MFC_GET_CONFIG_DEC_MP4ASP_VOP_TIME_RES:
		args->get_config.out_config_value[0] = (int)mfc_inst->RET_DEC_SEQ_INIT_BAK_MP4ASP_VOP_TIME_RES;
		ret = S3C_MFC_INST_RET_OK;
		break;

	case S3C_MFC_GET_CONFIG_DEC_MP4ASP_TIME_BASE_LAST:
		args->get_config.out_config_value[0] = (int)mfc_inst->RET_DEC_PIC_RUN_BAK_MP4ASP_TIME_BASE_LAST;
		ret = S3C_MFC_INST_RET_OK;
		break;

	case S3C_MFC_GET_CONFIG_DEC_MP4ASP_NONB_TIME_LAST:
		args->get_config.out_config_value[0] = (int)mfc_inst->RET_DEC_PIC_RUN_BAK_MP4ASP_NONB_TIME_LAST;
		ret = S3C_MFC_INST_RET_OK;
		break;

	case S3C_MFC_GET_CONFIG_DEC_MP4ASP_TRD:
		args->get_config.out_config_value[0] = (int)mfc_inst->RET_DEC_PIC_RUN_BAK_MP4ASP_MP4ASP_TRD;
		ret = S3C_MFC_INST_RET_OK;
		break;
#endif

	default:
		ret = -1;
	}


	/* Output arguments for S3C_MFC_IOCTL_MFC_SET_CONFIG */
	args->get_config.ret_code = ret;

	return S3C_MFC_INST_RET_OK;
}

/* Input arguments for S3C_MFC_IOCTL_MFC_SET_CONFIG */
int s3c_mfc_set_config_params(s3c_mfc_inst_context_t *mfc_inst, s3c_mfc_args_t *args)		
{
	int             ret, size;
	unsigned int    param_change_enable = 0, param_change_val;
	unsigned char	*start;
	unsigned int	end, offset;
	
	switch (args->set_config.in_config_param) {
	case S3C_MFC_SET_CONFIG_DEC_ROTATE:
#if (S3C_MFC_ROTATE_ENABLE == 1)
		args->set_config.out_config_value_old[0]
			= s3c_mfc_inst_set_post_rotate(mfc_inst, args->set_config.in_config_value[0]);
#else
		mfc_err("S3C_MFC_IOCTL_MFC_SET_CONFIG with S3C_MFC_SET_CONFIG_DEC_ROTATE is not supported\n");
		mfc_err("please check if S3C_MFC_ROTATE_ENABLE is defined as 1 in MfcConfig.h file\n");
#endif
		ret = S3C_MFC_INST_RET_OK;
		break;

	case S3C_MFC_SET_CONFIG_ENC_H263_PARAM:
		args->set_config.out_config_value_old[0] = mfc_inst->h263_annex;
		mfc_inst->h263_annex = args->set_config.in_config_value[0];
		mfc_debug("parameter = 0x%x\n", mfc_inst->h263_annex);
		ret = S3C_MFC_INST_RET_OK;
		break;

	case S3C_MFC_SET_CONFIG_ENC_SLICE_MODE:
		if (mfc_inst->enc_num_slices) {
			args->set_config.out_config_value_old[0] = 1;
			args->set_config.out_config_value_old[1] = mfc_inst->enc_num_slices;
		} else {
			args->set_config.out_config_value_old[0] = 0;
			args->set_config.out_config_value_old[1] = 0;
		}

		if (args->set_config.in_config_value[0])
			mfc_inst->enc_num_slices = args->set_config.in_config_value[1];
		else
			mfc_inst->enc_num_slices = 0;

		ret = S3C_MFC_INST_RET_OK;
		break;

	case S3C_MFC_SET_CONFIG_ENC_PARAM_CHANGE:

		switch (args->set_config.in_config_value[0]) {
		case S3C_ENC_PARAM_GOP_NUM:
			param_change_enable = (1 << 0);
			break;

		case S3C_ENC_PARAM_INTRA_QP:
			param_change_enable = (1 << 1);
			break;

		case S3C_ENC_PARAM_BITRATE:
			param_change_enable = (1 << 2);
			break;

		case S3C_ENC_PARAM_F_RATE:
			param_change_enable = (1 << 3);
			break;

		case S3C_ENC_PARAM_INTRA_REF:
			param_change_enable = (1 << 4);
			break;

		case S3C_ENC_PARAM_SLICE_MODE:
			param_change_enable = (1 << 5);
			break;

		default:
			break;
		}

		param_change_val  = args->set_config.in_config_value[1];
		ret = s3c_mfc_inst_enc_param_change(mfc_inst, param_change_enable, param_change_val);

		break;

	case S3C_MFC_SET_CONFIG_ENC_CUR_PIC_OPT:

		switch (args->set_config.in_config_value[0]) {
		case S3C_ENC_PIC_OPT_IDR:
			mfc_inst->enc_pic_option ^= (args->set_config.in_config_value[1] << 1);
			break;

		case S3C_ENC_PIC_OPT_SKIP:
			mfc_inst->enc_pic_option ^= (args->set_config.in_config_value[1] << 0);
			break;

		case S3C_ENC_PIC_OPT_RECOVERY:
			mfc_inst->enc_pic_option ^= (args->set_config.in_config_value[1] << 24);
			break;

		default:
			break;
		}

		ret = S3C_MFC_INST_RET_OK;
		break;

	case S3C_MFC_SET_CACHE_CLEAN:
		/* 
		 * in_config_value[0] : start address in user layer 
		 * in_config_value[1] : offset 
		 * in_config_value[2] : start address of stream buffer in user layer
		 */
		offset = args->set_config.in_config_value[0] - args->set_config.in_config_value[2];
		start = mfc_inst->stream_buffer + offset;
		size = args->set_config.in_config_value[1];
		dma_cache_maint(start, size, DMA_TO_DEVICE);
		/*
		offset = args->set_config.in_config_value[0] - args->set_config.in_config_value[2];
		start = (unsigned int)mfc_inst->stream_buffer + offset;
		end   = start + args->set_config.in_config_value[1];
		dmac_clean_range((void *)start, (void *)end);

		start = (unsigned int)mfc_inst->phys_addr_stream_buffer + offset;
		end   = start + args->set_config.in_config_value[1];
		outer_clean_range((unsigned long)start, (unsigned long)end);
		*/
		
		ret = S3C_MFC_INST_RET_OK;
		break;

	case S3C_MFC_SET_CACHE_INVALIDATE:
		/* 
		 * in_config_value[0] : start address in user layer 
		 * in_config_value[1] : offset 
		 * in_config_value[2] : start address of stream buffer in user layer
		 */
		offset = args->set_config.in_config_value[0] - args->set_config.in_config_value[2];
		start = mfc_inst->stream_buffer + offset;
		size = args->set_config.in_config_value[1];
		dma_cache_maint(start, size, DMA_FROM_DEVICE);

		/*
		offset = args->set_config.in_config_value[0] - args->set_config.in_config_value[2];
		start = (unsigned int)mfc_inst->stream_buffer + offset;
		end   = start + args->set_config.in_config_value[1];
		dmac_inv_range((void *)start, (void *)end);

		start = (unsigned int)mfc_inst->phys_addr_stream_buffer + offset;
		end = start + args->set_config.in_config_value[1];
		outer_inv_range((unsigned long)start, (unsigned long)end);
		*/
		ret = S3C_MFC_INST_RET_OK;
		break;

	case S3C_MFC_SET_CACHE_CLEAN_INVALIDATE:
		/* 
		 * in_config_value[0] : start address in user layer 
		 * in_config_value[1] : offset 
		 * in_config_value[2] : start address of stream buffer in user layer
		 */
		offset = args->set_config.in_config_value[0] - args->set_config.in_config_value[2];
		start = mfc_inst->stream_buffer + offset;
		size = args->set_config.in_config_value[1];
		dma_cache_maint(start, size, DMA_BIDIRECTIONAL);

		/*
		offset = args->set_config.in_config_value[0] - args->set_config.in_config_value[2];
		start = (unsigned int)mfc_inst->stream_buffer + offset;
		end   = start + args->set_config.in_config_value[1];
		dmac_flush_range((void *)start, (void *)end);

		start = (unsigned int)mfc_inst->phys_addr_stream_buffer + offset;
		end = start + args->set_config.in_config_value[1];
		outer_flush_range((unsigned long)start, (unsigned long)end);
		*/
		ret = S3C_MFC_INST_RET_OK;
		break;

#if (defined(DIVX_ENABLE) && (DIVX_ENABLE == 1))
	case S3C_MFC_SET_PADDING_SIZE:
		mfc_debug("padding size = %d\n", 		\
			args->set_config.in_config_value[0]);
		mfc_inst->padding_size = args->set_config.in_config_value[0];
		ret = S3C_MFC_INST_RET_OK;
		break;
#endif

	default:
		ret = -1;
	}

	/* Output arguments for S3C_MFC_IOCTL_MFC_SET_CONFIG */
	args->set_config.ret_code = ret;

	return S3C_MFC_INST_RET_OK;
}

