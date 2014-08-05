/* linux/drivers/media/video/samsung/post/s3c_pp_common.c
 *
 * Driver file for Samsung Post processor
 *
 * Jiun Yu, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/errno.h> /* error codes */
#include <asm/div64.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <mach/hardware.h>
#include <asm/uaccess.h>
#include <mach/map.h>
#include <linux/miscdevice.h>

#include <linux/version.h>
#include <plat/regs-pp.h>

#include "s3c_pp_common.h"

#define PFX "s3c_pp"

// setting the source/destination color space
void set_data_format(s3c_pp_instance_context_t *pp_instance)
{
	// set the source color space
	switch(pp_instance->src_color_space) {
		case YC420:
			pp_instance->in_pixel_size	= 1;
			break;
		case YCBYCR:
			pp_instance->in_pixel_size	= 2;
			break;
		case YCRYCB:
			pp_instance->in_pixel_size	= 2;
			break;
		case CBYCRY:
			pp_instance->in_pixel_size	= 2;
			break;
		case CRYCBY:
			pp_instance->in_pixel_size	= 2;
			break;
		case RGB24:
			pp_instance->in_pixel_size	= 4;
			break;
		case RGB16:
			pp_instance->in_pixel_size	= 2;
			break;
		default:
			break;
	}

	// set the destination color space
	if ( DMA_ONESHOT == pp_instance->out_path ) 
    {
		switch(pp_instance->dst_color_space) {
			case YC420:
				pp_instance->out_pixel_size	= 1;
				break;
			case YCBYCR:
				pp_instance->out_pixel_size	= 2;
				break;
			case YCRYCB:
				pp_instance->out_pixel_size	= 2;
				break;
			case CBYCRY:
				pp_instance->out_pixel_size	= 2;
				break;
			case CRYCBY:
				pp_instance->out_pixel_size	= 2;
				break;
			case RGB24:
				pp_instance->out_pixel_size	= 4;
				break;
			case RGB16:
				pp_instance->out_pixel_size	= 2;
				break;
			default:
				break;
		}
	}
	else if ( FIFO_FREERUN == pp_instance->out_path ) 
	{
		if(pp_instance->dst_color_space == RGB30) 
		{
			pp_instance->out_pixel_size	= 4;
		} 
		else if(pp_instance->dst_color_space == YUV444) 
		{
			pp_instance->out_pixel_size	= 4;
		} 
	}

	// setting the register about src/dst data format
	set_data_format_register(pp_instance);
}


void set_src_addr(s3c_pp_instance_context_t *pp_instance)
{
	s3c_pp_buf_addr_t	buf_addr;

	buf_addr.offset_y			= (pp_instance->src_full_width - pp_instance->src_width) * pp_instance->in_pixel_size;
	buf_addr.start_pos_y		= (pp_instance->src_full_width*pp_instance->src_start_y+pp_instance->src_start_x)*pp_instance->in_pixel_size;
	buf_addr.end_pos_y			= pp_instance->src_width*pp_instance->src_height*pp_instance->in_pixel_size + buf_addr.offset_y*(pp_instance->src_height-1);
	buf_addr.src_frm_start_addr	= pp_instance->src_buf_addr_phy;
	buf_addr.src_start_y		= pp_instance->src_buf_addr_phy + buf_addr.start_pos_y;
	buf_addr.src_end_y			= buf_addr.src_start_y + buf_addr.end_pos_y;

	if (pp_instance->src_color_space == YC420) 
    {
		buf_addr.offset_cb		= buf_addr.offset_cr = ((pp_instance->src_full_width - pp_instance->src_width) / 2) * pp_instance->in_pixel_size;
		buf_addr.start_pos_cb	= pp_instance->src_full_width * pp_instance->src_full_height * 1	\
								+ (pp_instance->src_full_width * pp_instance->src_start_y / 2 + pp_instance->src_start_x) /2 * 1;

		buf_addr.end_pos_cb		= pp_instance->src_width/2*pp_instance->src_height/2*pp_instance->in_pixel_size \
					 			+ (pp_instance->src_height/2 -1)*buf_addr.offset_cb;
		buf_addr.start_pos_cr	= pp_instance->src_full_width * pp_instance->src_full_height *1 \
					   			+ pp_instance->src_full_width*pp_instance->src_full_height/4 *1 \
					   			+ (pp_instance->src_full_width*pp_instance->src_start_y/2 + pp_instance->src_start_x)/2*1;
		buf_addr.end_pos_cr		= pp_instance->src_width/2*pp_instance->src_height/2*pp_instance->in_pixel_size \
					 			+ (pp_instance->src_height/2-1)*buf_addr.offset_cr;

		buf_addr.src_start_cb	= pp_instance->src_buf_addr_phy + buf_addr.start_pos_cb;		
		buf_addr.src_end_cb		= buf_addr.src_start_cb + buf_addr.end_pos_cb;

		buf_addr.src_start_cr	= pp_instance->src_buf_addr_phy + buf_addr.start_pos_cr;
		buf_addr.src_end_cr		= buf_addr.src_start_cr + buf_addr.end_pos_cr;
	}

    set_src_addr_register ( &buf_addr, pp_instance );
}

void set_dest_addr(s3c_pp_instance_context_t *pp_instance)
{
	s3c_pp_buf_addr_t	buf_addr;

	if ( DMA_ONESHOT == pp_instance->out_path ) 
    {
		buf_addr.offset_rgb		= (pp_instance->dst_full_width - pp_instance->dst_width)*pp_instance->out_pixel_size;
		buf_addr.start_pos_rgb	= (pp_instance->dst_full_width*pp_instance->dst_start_y + pp_instance->dst_start_x)*pp_instance->out_pixel_size;
		buf_addr.end_pos_rgb	= pp_instance->dst_width*pp_instance->dst_height*pp_instance->out_pixel_size 	\
								+ buf_addr.offset_rgb*(pp_instance->dst_height - 1);
		buf_addr.dst_start_rgb 	= pp_instance->dst_buf_addr_phy + buf_addr.start_pos_rgb;
		buf_addr.dst_end_rgb 	= buf_addr.dst_start_rgb + buf_addr.end_pos_rgb;

		if (pp_instance->dst_color_space == YC420) 
        {
			buf_addr.out_offset_cb		= buf_addr.out_offset_cr = ((pp_instance->dst_full_width - pp_instance->dst_width)/2)*pp_instance->out_pixel_size;
			buf_addr.out_start_pos_cb 	= pp_instance->dst_full_width*pp_instance->dst_full_height*1 \
							  	 		+ (pp_instance->dst_full_width*pp_instance->dst_start_y/2 + pp_instance->dst_start_x)/2*1;
			buf_addr.out_end_pos_cb 	= pp_instance->dst_width/2*pp_instance->dst_height/2*pp_instance->out_pixel_size \
							 			+ (pp_instance->dst_height/2 -1)*buf_addr.out_offset_cr;

			buf_addr.out_start_pos_cr 	= pp_instance->dst_full_width*pp_instance->dst_full_height*1 \
							   			+ (pp_instance->dst_full_width*pp_instance->dst_full_height/4)*1 \
							   			+ (pp_instance->dst_full_width*pp_instance->dst_start_y/2 +pp_instance->dst_start_x)/2*1;
			buf_addr.out_end_pos_cr 	= pp_instance->dst_width/2*pp_instance->dst_height/2*pp_instance->out_pixel_size \
							 			+ (pp_instance->dst_height/2 -1)*buf_addr.out_offset_cb;

			buf_addr.out_src_start_cb 	= pp_instance->dst_buf_addr_phy + buf_addr.out_start_pos_cb;
			buf_addr.out_src_end_cb 	= buf_addr.out_src_start_cb + buf_addr.out_end_pos_cb;
			buf_addr.out_src_start_cr 	= pp_instance->dst_buf_addr_phy + buf_addr.out_start_pos_cr;
			buf_addr.out_src_end_cr 	= buf_addr.out_src_start_cr + buf_addr.out_end_pos_cr;
		}

        set_dest_addr_register ( &buf_addr, pp_instance );
	}
}

void set_src_next_buf_addr(s3c_pp_instance_context_t *pp_instance)
{
	s3c_pp_buf_addr_t	buf_addr;


	buf_addr.offset_y			= (pp_instance->src_full_width - pp_instance->src_width) * pp_instance->in_pixel_size;
	buf_addr.start_pos_y		= (pp_instance->src_full_width*pp_instance->src_start_y+pp_instance->src_start_x)*pp_instance->in_pixel_size;
	buf_addr.end_pos_y			= pp_instance->src_width*pp_instance->src_height*pp_instance->in_pixel_size + buf_addr.offset_y*(pp_instance->src_height-1);
	buf_addr.src_frm_start_addr	= pp_instance->src_next_buf_addr_phy;
	
	buf_addr.src_start_y		= pp_instance->src_next_buf_addr_phy + buf_addr.start_pos_y;
	buf_addr.src_end_y			= buf_addr.src_start_y + buf_addr.end_pos_y;


	if(pp_instance->src_color_space == YC420) {
		buf_addr.offset_cb		= buf_addr.offset_cr = ((pp_instance->src_full_width - pp_instance->src_width) / 2) * pp_instance->in_pixel_size;
		buf_addr.start_pos_cb	= pp_instance->src_full_width * pp_instance->src_full_height * 1	\
								+ (pp_instance->src_full_width * pp_instance->src_start_y / 2 + pp_instance->src_start_x) /2 * 1;

		buf_addr.end_pos_cb		= pp_instance->src_width/2*pp_instance->src_height/2*pp_instance->in_pixel_size \
					 			+ (pp_instance->src_height/2 -1)*buf_addr.offset_cb;
		buf_addr.start_pos_cr	= pp_instance->src_full_width * pp_instance->src_full_height *1 \
					   			+ pp_instance->src_full_width*pp_instance->src_full_height/4 *1 \
					   			+ (pp_instance->src_full_width*pp_instance->src_start_y/2 + pp_instance->src_start_x)/2*1;
		buf_addr.end_pos_cr		= pp_instance->src_width/2*pp_instance->src_height/2*pp_instance->in_pixel_size \
					 			+ (pp_instance->src_height/2-1)*buf_addr.offset_cr;

	
		buf_addr.src_start_cb	= pp_instance->src_next_buf_addr_phy + buf_addr.start_pos_cb;
		buf_addr.src_end_cb		= buf_addr.src_start_cb + buf_addr.end_pos_cb;
		buf_addr.src_start_cr	= pp_instance->src_next_buf_addr_phy + buf_addr.start_pos_cr;
		buf_addr.src_end_cr		= buf_addr.src_start_cr + buf_addr.end_pos_cr;

	}


	set_src_next_addr_register(&buf_addr, pp_instance);	
}



// setting the scaling information(source/destination size)
void set_scaler(s3c_pp_instance_context_t *pp_instance)
{
	s3c_pp_scaler_info_t	scaler_info;

	
	if (pp_instance->src_width >= (pp_instance->dst_width<<6)) {
		printk(KERN_ERR "Out of PreScalar range !!!\n");
		return;
	}
	if(pp_instance->src_width >= (pp_instance->dst_width<<5)) {
		scaler_info.pre_h_ratio = 32;
		scaler_info.h_shift = 5;		
	} else if(pp_instance->src_width >= (pp_instance->dst_width<<4)) {
		scaler_info.pre_h_ratio = 16;
		scaler_info.h_shift = 4;		
	} else if(pp_instance->src_width >= (pp_instance->dst_width<<3)) {
		scaler_info.pre_h_ratio = 8;
		scaler_info.h_shift = 3;		
	} else if(pp_instance->src_width >= (pp_instance->dst_width<<2)) {
		scaler_info.pre_h_ratio = 4;
		scaler_info.h_shift = 2;		
	} else if(pp_instance->src_width >= (pp_instance->dst_width<<1)) {
		scaler_info.pre_h_ratio = 2;
		scaler_info.h_shift = 1;		
	} else {
		scaler_info.pre_h_ratio = 1;
		scaler_info.h_shift = 0;		
	}

	scaler_info.pre_dst_width = pp_instance->src_width / scaler_info.pre_h_ratio;
	scaler_info.dx = (pp_instance->src_width<<8) / (pp_instance->dst_width<<scaler_info.h_shift);


	if (pp_instance->src_height >= (pp_instance->dst_height<<6)) {
		printk(KERN_ERR "Out of PreScalar range !!!\n");
		return;
	}
	if(pp_instance->src_height >= (pp_instance->dst_height<<5)) {
		scaler_info.pre_v_ratio = 32;
		scaler_info.v_shift = 5;		
	} else if(pp_instance->src_height >= (pp_instance->dst_height<<4)) {
		scaler_info.pre_v_ratio = 16;
		scaler_info.v_shift = 4;		
	} else if(pp_instance->src_height >= (pp_instance->dst_height<<3)) {
		scaler_info.pre_v_ratio = 8;
		scaler_info.v_shift = 3;		
	} else if(pp_instance->src_height >= (pp_instance->dst_height<<2)) {
		scaler_info.pre_v_ratio = 4;
		scaler_info.v_shift = 2;		
	} else if(pp_instance->src_height >= (pp_instance->dst_height<<1)) {
		scaler_info.pre_v_ratio = 2;
		scaler_info.v_shift = 1;		
	} else {
		scaler_info.pre_v_ratio = 1;
		scaler_info.v_shift = 0;		
	}	

	scaler_info.pre_dst_height = pp_instance->src_height / scaler_info.pre_v_ratio;
	scaler_info.dy = (pp_instance->src_height<<8) / (pp_instance->dst_height<<scaler_info.v_shift);
	scaler_info.sh_factor = 10 - (scaler_info.h_shift + scaler_info.v_shift);


	// setting the register about scaling information
	set_scaler_register(&scaler_info, pp_instance);
}

int cal_data_size(s3c_color_space_t color_space, unsigned int width, unsigned int height)
{
	switch(color_space) {
		case YC420:
			return (width * height * 3) >> 1;
		case YCBYCR:
			return (width * height * 2);
		case YCRYCB:
			return (width * height * 2);
		case CBYCRY:
			return (width * height * 2);
		case CRYCBY:
			return (width * height * 2);
		case RGB24:
			return (width * height * 4);
		case RGB16:
			return (width * height * 2);
		default:
			printk(KERN_ERR "Input parameter is wrong\n");
			return -EINVAL;
	}
}

int get_src_data_size(s3c_pp_instance_context_t *pp_instance)
{
    return cal_data_size ( pp_instance->src_color_space, pp_instance->src_full_width, pp_instance->src_full_height );
}

int get_dest_data_size(s3c_pp_instance_context_t *pp_instance)
{
    return cal_data_size ( pp_instance->dst_color_space, pp_instance->dst_full_width, pp_instance->dst_full_height );
}


