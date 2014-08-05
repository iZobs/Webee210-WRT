/* linux/drivers/media/video/samsung/jpeg/jpg_opr.c
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

#include "jpg_mem.h"
#include "jpg_misc.h"
#include "jpg_opr.h"
#include "jpg_conf.h"
#include "log_msg.h"

extern int	jpg_irq_reason;

enum{
		UNKNOWN,
		BASELINE = 0xC0,
		EXTENDED_SEQ = 0xC1,
		PROGRESSIVE = 0xC2
}JPG_SOF_MARKER;


/*----------------------------------------------------------------------------
*Function: decode_jpg

*Parameters:	jCTX:
				input_buff:
				input_size:
				output_buff:
				output_size
*Return Value:
*Implementation Notes: 
-----------------------------------------------------------------------------*/
JPG_RETURN_STATUS decode_jpg(s3c6400_jpg_ctx *jCTX,
							JPG_DEC_PROC_PARAM *decParam)
{
	volatile int		ret;
	SAMPLE_MODE_T sampleMode;
	UINT32	width, height, orgwidth, orgheight;
	BOOL	headerFixed = FALSE;

	log_msg(LOG_TRACE, "decode_jpg", "decode_jpg function\n");
	reset_jpg(jCTX);

	/////////////////////////////////////////////////////////
	// Header Parsing									   //
	/////////////////////////////////////////////////////////

	decode_header(jCTX);
	wait_for_interrupt();
	ret = jpg_irq_reason;
		
	if(ret != OK_HD_PARSING){
		log_msg(LOG_ERROR, "\ndecode_jpg", "DD::JPG Header Parsing Error(%d)\r\n", ret);
		return JPG_FAIL;
	}

	sampleMode = get_sample_type(jCTX);
	log_msg(LOG_TRACE, "decode_jpg", "sampleMode : %d\n", sampleMode);
	if(sampleMode == JPG_SAMPLE_UNKNOWN){
		log_msg(LOG_ERROR, "decode_jpg", "DD::JPG has invalid sampleMode\r\n");
		return JPG_FAIL;
	}
	decParam->sampleMode = sampleMode;

	getXY(jCTX, &width, &height);
	log_msg(LOG_TRACE, "decode_jpg", "DD:: width : 0x%x height : 0x%x\n", width, height);
	if(width <= 0 || width > MAX_JPG_WIDTH || height <= 0 || height > MAX_JPG_HEIGHT){
		log_msg(LOG_ERROR, "decode_jpg", "DD::JPG has invalid width/height\n");
		return JPG_FAIL;
	}

	/////////////////////////////////////////////////////////
	// Header Correction								   //
	/////////////////////////////////////////////////////////

	orgwidth = width;
	orgheight = height;
	if(!is_correct_header(sampleMode, &width, &height)){
		rewrite_header(jCTX, decParam->fileSize, width, height);
		headerFixed = TRUE;
	}
	

	/////////////////////////////////////////////////////////
	// Body Decoding									   //
	/////////////////////////////////////////////////////////

	if(headerFixed){
		reset_jpg(jCTX);
		decode_header(jCTX);
		wait_for_interrupt();
		ret = jpg_irq_reason;
				
		if(ret != OK_HD_PARSING){
			log_msg(LOG_ERROR, "decode_jpg", "JPG Header Parsing Error(%d)\r\n", ret);
			return JPG_FAIL;
		}
		
		decode_body(jCTX);
		wait_for_interrupt();
		ret = jpg_irq_reason;
			
		if(ret != OK_ENC_OR_DEC){
			log_msg(LOG_ERROR, "decode_jpg", "JPG Body Decoding Error(%d)\n", ret);
			return JPG_FAIL;
		}

		// for post processor, discard pixel
		if(orgwidth % 4 != 0)  
			orgwidth = (orgwidth/4)*4;

		log_msg(LOG_TRACE, "decode_jpg", "orgwidth : %d orgheight : %d\n", orgwidth, orgheight);
		rewrite_yuv(jCTX, width, orgwidth, height, orgheight);

		// JPEG H/W IP always return YUV422
		decParam->dataSize = getYUVSize(JPG_422, orgwidth, orgheight);
		decParam->width = orgwidth;
		decParam->height = orgheight;
	}
	else{
		decode_body(jCTX);
		wait_for_interrupt();
		ret = jpg_irq_reason;
		
		if(ret != OK_ENC_OR_DEC){
			log_msg(LOG_ERROR, "decode_jpg", "DD::JPG Body Decoding Error(%d)\n", ret);
			return JPG_FAIL;
		}

		// JPEG H/W IP always return YUV422
		decParam->dataSize = getYUVSize(JPG_422, width, height);
		decParam->width = width;
		decParam->height = height;
	}
	
	return JPG_SUCCESS;	
}

/*----------------------------------------------------------------------------
*Function: is_correct_header

*Parameters:	sampleMode:
				width:
				height:
*Return Value:
*Implementation Notes: 
-----------------------------------------------------------------------------*/
BOOL is_correct_header(SAMPLE_MODE_T sampleMode, UINT32 *width, UINT32 *height)
{
	BOOL result = FALSE;

	log_msg(LOG_TRACE, "is_correct_header", "Header is not multiple of MCU\n");

	switch(sampleMode){
		case JPG_400 : 
		case JPG_444 : if((*width % 8 == 0) && (*height % 8 == 0))
						   result = TRUE;
					   if(*width % 8 != 0)
						   *width += 8 - (*width % 8);
					   if(*height % 8 != 0)
						   *height += 8 - (*height % 8);						
						break;
		case JPG_422 : if((*width % 16 == 0) && (*height % 8 == 0))
						   result = TRUE;
					   if(*width % 16 != 0)
						   *width += 16 - (*width % 16);
					   if(*height % 8 != 0)
						   *height += 8 - (*height % 8);						
						break; 
		case JPG_420 : 
		case JPG_411 : if((*width % 16 == 0) && (*height % 16 == 0))
						   result = TRUE;
					   if(*width % 16 != 0)
						   *width += 16 - (*width % 16);
					   if(*height % 16 != 0)
						   *height += 16 - (*height % 16);						
						break;
		default : break;
	}

	log_msg(LOG_TRACE, "is_correct_header", "after error correction : width(%x) height(%x)\n", *width, *height);
	return(result);
}

/*----------------------------------------------------------------------------
*Function: rewrite_header

*Parameters:	jCTX:
				file_size:
				width:
				height:
*Return Value:
*Implementation Notes: 
-----------------------------------------------------------------------------*/
void rewrite_header(s3c6400_jpg_ctx *jCTX, UINT32 file_size, UINT32 width, UINT32 height)
{
	UINT32	i;
	UINT8	*ptr = (UINT8 *)jCTX->v_pJPGData_Buff;
	UINT8	*SOF1 = NULL, *SOF2 = NULL;
	UINT8	*header = NULL;

	log_msg(LOG_TRACE, "rewrite_header", "file size : %d, v_pJPGData_Buff : 0x%X\n", file_size, ptr);
	
	for(i=0; i < file_size; i++){
		if(*ptr++ == 0xFF){
			if((*ptr == BASELINE) || (*ptr == EXTENDED_SEQ) || (*ptr == PROGRESSIVE)){
				log_msg(LOG_TRACE, "rewrite_header", "match FFC0(i : %d)\n", i);
				if(SOF1 == NULL)
					SOF1 = ++ptr;
				else{
					SOF2 = ++ptr;
					break;
				}
			}
		}
	}

	log_msg(LOG_TRACE, "rewrite_header", "start header correction\n");
	if(i <= file_size){
		header = (SOF2 == NULL) ? (SOF1) : (SOF2);
		header += 3; //length(2) + sampling bit(1)
		*header = (height>>8) & 0xFF;
		header++;
		*header = height & 0xFF;
		header++;
		*header = (width>>8) & 0xFF;
		header++;
		*header = (width & 0xFF);

	}
}

/*----------------------------------------------------------------------------
*Function: reset_jpg

*Parameters:	jCTX:
*Return Value:
*Implementation Notes: 
-----------------------------------------------------------------------------*/
void reset_jpg(s3c6400_jpg_ctx *jCTX)
{
	log_msg(LOG_TRACE, "reset_jpg", "reset_jpg function\n");
	jCTX->v_pJPG_REG->JPGSoftReset = 0; //ENABLE
}

/*----------------------------------------------------------------------------
*Function: decode_header

*Parameters:	jCTX:	
*Return Value:
*Implementation Notes: 
-----------------------------------------------------------------------------*/
void decode_header(s3c6400_jpg_ctx *jCTX)
{
	log_msg(LOG_TRACE, "decode_header", "decode_header function\n");
	jCTX->v_pJPG_REG->JPGFileAddr0 = jpg_data_base_addr;
	jCTX->v_pJPG_REG->JPGFileAddr1 = jpg_data_base_addr;
	
	jCTX->v_pJPG_REG->JPGMod = 0x08; //decoding mode
	jCTX->v_pJPG_REG->JPGIRQ = ENABLE_IRQ;
	jCTX->v_pJPG_REG->JPGCntl = DISABLE_HW_DEC;
	jCTX->v_pJPG_REG->JPGMISC = (NORMAL_DEC | YCBCR_MEMORY);
	jCTX->v_pJPG_REG->JPGStart = 1;
}

/*----------------------------------------------------------------------------
*Function: decode_body

*Parameters:	jCTX:
*Return Value:
*Implementation Notes: 
-----------------------------------------------------------------------------*/
void decode_body(s3c6400_jpg_ctx *jCTX)
{
	log_msg(LOG_TRACE, "decode_body", "decode_body function\n");
	jCTX->v_pJPG_REG->JPGYUVAddr0 = jpg_data_base_addr + JPG_STREAM_BUF_SIZE + JPG_STREAM_THUMB_BUF_SIZE;
	jCTX->v_pJPG_REG->JPGYUVAddr1 = jpg_data_base_addr + JPG_STREAM_BUF_SIZE + JPG_STREAM_THUMB_BUF_SIZE;

	jCTX->v_pJPG_REG->JPGCntl = 0;
	jCTX->v_pJPG_REG->JPGMISC = 0;
	jCTX->v_pJPG_REG->JPGReStart = 1;
}

/*----------------------------------------------------------------------------
*Function: rewrite_yuv

*Parameters:	jCTX:
				width:
				orgwidth:
				height:
				orgheight:
*Return Value:
*Implementation Notes: 
-----------------------------------------------------------------------------*/
void rewrite_yuv(s3c6400_jpg_ctx *jCTX, UINT32 width, UINT32 orgwidth, UINT32 height, UINT32 orgheight)
{
	UINT32	src, dst;
	UINT32	i;
	UINT8	*streamPtr;

	log_msg(LOG_TRACE, "rewrite_yuv", "rewrite_yuv function\n");

	streamPtr = (UINT8 *)(jCTX->v_pJPGData_Buff + JPG_STREAM_BUF_SIZE + JPG_STREAM_THUMB_BUF_SIZE);
	src = 2*width;
	dst = 2*orgwidth;
	for(i = 1; i < orgheight; i++){
		mem_move(&streamPtr[dst], &streamPtr[src], 2*orgwidth);
		src += 2*width;
		dst += 2*orgwidth;
	}
}

/*----------------------------------------------------------------------------
*Function:	get_sample_type

*Parameters:	jCTX:
*Return Value:
*Implementation Notes: 
-----------------------------------------------------------------------------*/
SAMPLE_MODE_T get_sample_type(s3c6400_jpg_ctx *jCTX)
{	
	ULONG	jpgMode;
	SAMPLE_MODE_T	sampleMode;	

	jpgMode = jCTX->v_pJPG_REG->JPGMod;

	sampleMode = 
		((jpgMode&0x7) == 0) ? JPG_444 :
		((jpgMode&0x7) == 1) ? JPG_422 :
		((jpgMode&0x7) == 2) ? JPG_420 :
		((jpgMode&0x7) == 3) ? JPG_400 :
		((jpgMode&0x7) == 6) ? JPG_411 : JPG_SAMPLE_UNKNOWN;

	return(sampleMode);
}

/*----------------------------------------------------------------------------
*Function:	getXY

*Parameters:	jCTX:
				x:
				y:
*Return Value:
*Implementation Notes: 
-----------------------------------------------------------------------------*/
void getXY(s3c6400_jpg_ctx *jCTX, UINT32 *x, UINT32 *y)
{
	*x = jCTX->v_pJPG_REG->JPGX;
	*y = jCTX->v_pJPG_REG->JPGY;
}

/*----------------------------------------------------------------------------
*Function: getYUVSize

*Parameters:	sampleMode:
				width:
				height:
*Return Value:
*Implementation Notes: 
-----------------------------------------------------------------------------*/
UINT32 getYUVSize(SAMPLE_MODE_T sampleMode, UINT32 width, UINT32 height)
{
	switch(sampleMode){
		case JPG_444 : return(width*height*3);
		case JPG_422 : return(width*height*2);
		case JPG_420 : 
		case JPG_411 : return((width*height*3)>>1);
		case JPG_400 : return(width*height);
		default : return(0);
	}

}

/*----------------------------------------------------------------------------
*Function: reset_jpg

*Parameters:	jCTX:
*Return Value:
*Implementation Notes: 
-----------------------------------------------------------------------------*/
JPG_RETURN_STATUS encode_jpg(s3c6400_jpg_ctx *jCTX, 
							JPG_ENC_PROC_PARAM	*EncParam)
{
	UINT	i, ret;

	if(EncParam->width <= 0 || EncParam->width > MAX_JPG_WIDTH
		|| EncParam->height <=0 || EncParam->height > MAX_JPG_HEIGHT){
			log_msg(LOG_ERROR, "encode_jpg", "DD::Encoder : Invalid width/height\r\n");
			return JPG_FAIL;
	}

	reset_jpg(jCTX);

	jCTX->v_pJPG_REG->JPGMod = (EncParam->sampleMode == JPG_422) ? (0x1<<0) : (0x2<<0);
	jCTX->v_pJPG_REG->JPGRSTPos = 2; // MCU inserts RST marker
	jCTX->v_pJPG_REG->JPGQTblNo = (1<<12) | (1<<14);
	jCTX->v_pJPG_REG->JPGX = EncParam->width;
	jCTX->v_pJPG_REG->JPGY = EncParam->height;

	log_msg(LOG_TRACE, "encode_jpg", "EncParam->encType : %d\n", EncParam->encType);
	if(EncParam->encType == JPG_MAIN){
		jCTX->v_pJPG_REG->JPGYUVAddr0 = jpg_data_base_addr + JPG_STREAM_BUF_SIZE + JPG_STREAM_THUMB_BUF_SIZE; // Address of input image
		jCTX->v_pJPG_REG->JPGYUVAddr1 = jpg_data_base_addr + JPG_STREAM_BUF_SIZE + JPG_STREAM_THUMB_BUF_SIZE; // Address of input image
		jCTX->v_pJPG_REG->JPGFileAddr0 = jpg_data_base_addr; // Address of JPEG stream
		jCTX->v_pJPG_REG->JPGFileAddr1 = jpg_data_base_addr; // next address of motion JPEG stream
	}
	else{ // thumbnail encoding
		jCTX->v_pJPG_REG->JPGYUVAddr0 = jpg_data_base_addr + JPG_STREAM_BUF_SIZE + JPG_STREAM_THUMB_BUF_SIZE + JPG_FRAME_BUF_SIZE; // Address of input image
		jCTX->v_pJPG_REG->JPGYUVAddr1 = jpg_data_base_addr + JPG_STREAM_BUF_SIZE + JPG_STREAM_THUMB_BUF_SIZE + JPG_FRAME_BUF_SIZE; // Address of input image
		jCTX->v_pJPG_REG->JPGFileAddr0 = jpg_data_base_addr + JPG_STREAM_BUF_SIZE; // Address of JPEG stream
		jCTX->v_pJPG_REG->JPGFileAddr1 = jpg_data_base_addr + JPG_STREAM_BUF_SIZE; // next address of motion JPEG stream
	}
	jCTX->v_pJPG_REG->JPGCOEF1 = COEF1_RGB_2_YUV; // Coefficient value 1 for RGB to YCbCr
	jCTX->v_pJPG_REG->JPGCOEF2 = COEF2_RGB_2_YUV; // Coefficient value 2 for RGB to YCbCr
	jCTX->v_pJPG_REG->JPGCOEF3 = COEF3_RGB_2_YUV; // Coefficient value 3 for RGB to YCbCr

	jCTX->v_pJPG_REG->JPGMISC = (1<<5) | (0<<2);
	jCTX->v_pJPG_REG->JPGCntl = DISABLE_MOTION_ENC;


	// Quantiazation and Huffman Table setting
	for (i=0; i<64; i++)
		jCTX->v_pJPG_REG->JQTBL0[i] = (UINT32)QTBL_Luminance[EncParam->quality][i];

	for (i=0; i<64; i++)
		jCTX->v_pJPG_REG->JQTBL1[i] = (UINT32)QTBL_Chrominance[EncParam->quality][i];

	for (i=0; i<16; i++)
		jCTX->v_pJPG_REG->JHDCTBL0[i] = (UINT32)HDCTBL0[i];

	for (i=0; i<12; i++)
		jCTX->v_pJPG_REG->JHDCTBLG0[i] = (UINT32)HDCTBLG0[i];

	for (i=0; i<16; i++)
		jCTX->v_pJPG_REG->JHACTBL0[i] = (UINT32)HACTBL0[i];

	for (i=0; i<162; i++)
		jCTX->v_pJPG_REG->JHACTBLG0[i] = (UINT32)HACTBLG0[i];

	jCTX->v_pJPG_REG->JPGStart = 0;

	wait_for_interrupt();
	ret = jpg_irq_reason;

	if(ret != OK_ENC_OR_DEC){
		log_msg(LOG_ERROR, "encode_jpg", "DD::JPG Encoding Error(%d)\n", ret);
		return JPG_FAIL;
	}

	EncParam->fileSize = jCTX->v_pJPG_REG->JPGDataSize;
	return JPG_SUCCESS;

}
