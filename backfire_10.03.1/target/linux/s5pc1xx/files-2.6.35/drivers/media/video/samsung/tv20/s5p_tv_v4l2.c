/* linux/drivers/media/video/samsung/tv20/s5p_tv_v4l2.c
 *
 * Video4Linux API ftn. file for Samsung TVOut driver
 *
 * Copyright (c) 2010 Samsung Electronics
 *	         http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>

#include "s5p_tv.h"

#ifdef S5P_V4L2_DEBUG
#define V4L2PRINTK(fmt, args...)\
	pr_debug("[V4L2_IF] %s: " fmt, __func__ , ## args)
#else
#define V4L2PRINTK(fmt, args...)
#endif

/* 0 - hdcp stopped, 1 - hdcp started, 2 - hdcp reset */
u8 g_hdcp_protocol_status;

#define CVBS_S_VIDEO (V4L2_STD_NTSC_M | V4L2_STD_NTSC_M_JP| \
	V4L2_STD_PAL | V4L2_STD_PAL_M | V4L2_STD_PAL_N | V4L2_STD_PAL_Nc | \
	V4L2_STD_PAL_60 | V4L2_STD_NTSC_443)

static struct v4l2_output s5p_tv_outputs[] = {
	{
		.index		= 0,
		.name		= "Analog  COMPOSITE",
		.type		= V4L2_OUTPUT_TYPE_COMPOSITE,
		.audioset	= 0,
		.modulator	= 0,
		.std		= CVBS_S_VIDEO,
	}, {
		.index		= 1,
		.name		= "Analog  SVIDEO",
		.type		= V4L2_OUTPUT_TYPE_SVIDEO,
		.audioset	= 0,
		.modulator	= 0,
		.std		= CVBS_S_VIDEO,
	}, {
		.index		= 2,
		.name		= "Analog COMPONENT_YPBPR_I",
		.type		= V4L2_OUTPUT_TYPE_YPBPR_INERLACED,
		.audioset	= 0,
		.modulator	= 0,
		.std		= V4L2_STD_ALL,
	}, {
		.index		= 3,
		.name		= "Analog COMPONENT_YPBPR_P",
		.type		= V4L2_OUTPUT_TYPE_YPBPR_PROGRESSIVE,
		.audioset	= 0,
		.modulator	= 0,
		.std		= V4L2_STD_ALL,
	}, {
		.index		= 4,
		.name		= "Analog COMPONENT_RGB_P",
		.type		= V4L2_OUTPUT_TYPE_RGB_PROGRESSIVE,
		.audioset	= 0,
		.modulator	= 0,
		.std		= V4L2_STD_ALL,
	}, {
		.index		= 5,
		.name		= "Digital HDMI(YCbCr)",
		.type		= V4L2_OUTPUT_TYPE_HDMI,
		.audioset	= 2,
		.modulator	= 0,
		.std		= V4L2_STD_480P_60_16_9 |
				V4L2_STD_480P_60_16_9 |
				V4L2_STD_720P_60 |
				V4L2_STD_720P_50 |
				V4L2_STD_1080P_60 |
				V4L2_STD_1080P_50 |
				V4L2_STD_1080I_60 |
				V4L2_STD_1080I_50 |
				V4L2_STD_480P_59 |
				V4L2_STD_720P_59 |
				V4L2_STD_1080I_59 |
				V4L2_STD_1080P_59 |
				V4L2_STD_1080P_30,
	}, {
		.index		= 6,
		.name		= "Digital HDMI(RGB)",
		.type		= V4L2_OUTPUT_TYPE_HDMI_RGB,
		.audioset	= 2,
		.modulator	= 0,
		.std		= V4L2_STD_480P_60_16_9 |
				V4L2_STD_480P_60_16_9 |
				V4L2_STD_720P_60 |
				V4L2_STD_720P_50 |
				V4L2_STD_1080P_60 |
				V4L2_STD_1080P_50 |
				V4L2_STD_1080I_60 |
				V4L2_STD_1080I_50 |
				V4L2_STD_480P_59 |
				V4L2_STD_720P_59 |
				V4L2_STD_1080I_59 |
				V4L2_STD_1080P_59 |
				V4L2_STD_1080P_30,
	}, {
		.index		= 7,
		.name		= "Digital DVI",
		.type		= V4L2_OUTPUT_TYPE_DVI,
		.audioset	= 2,
		.modulator	= 0,
		.std		= V4L2_STD_480P_60_16_9 |
				V4L2_STD_480P_60_16_9 |
				V4L2_STD_720P_60 |
				V4L2_STD_720P_50 |
				V4L2_STD_1080P_60 |
				V4L2_STD_1080P_50 |
				V4L2_STD_1080I_60 |
				V4L2_STD_1080I_50 |
				V4L2_STD_480P_59 |
				V4L2_STD_720P_59 |
				V4L2_STD_1080I_59 |
				V4L2_STD_1080P_59 |
				V4L2_STD_1080P_30,
	}
};

const struct v4l2_fmtdesc s5p_tv_o_fmt_desc[] = {
	{
		.index	= 0,
		.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT,
		.description	= "YUV420, NV12 (Video Processor)",
		.pixelformat	= V4L2_PIX_FMT_NV12,
		.flags	= FORMAT_FLAGS_CrCb,
	}
};

const struct v4l2_fmtdesc s5p_tv_o_overlay_fmt_desc[] = {
	{
		.index	= 0,
		.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY,
		.description	= "16bpp RGB, le - RGB[565]",
		.pixelformat	= V4L2_PIX_FMT_RGB565,
		.flags	= FORMAT_FLAGS_PACKED,
	}, {
		.index	= 1,
		.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY,
		.description	= "16bpp RGB, le - ARGB[1555]",
		.pixelformat	= V4L2_PIX_FMT_RGB555,
		.flags	= FORMAT_FLAGS_PACKED,
	}, {
		.index	= 2,
		.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY,
		.description	= "16bpp RGB, le - ARGB[4444]",
		.pixelformat	= V4L2_PIX_FMT_RGB444,
		.flags	= FORMAT_FLAGS_PACKED,
	}, {
		.index	= 3,
		.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY,
		.description	= "32bpp RGB, le - ARGB[8888]",
		.pixelformat	= V4L2_PIX_FMT_RGB32,
		.flags	= FORMAT_FLAGS_PACKED,
	}
};

const struct v4l2_standard s5p_tv_standards[] = {
	{
		.index  = 0,
		.id     = V4L2_STD_NTSC_M,
		.name	= "NTSC_M",
	}, {

		.index	= 1,
		.id	= V4L2_STD_PAL_BDGHI,
		.name	= "PAL_BDGHI",
	}, {
		.index  = 2,
		.id     = V4L2_STD_PAL_M,
		.name	= "PAL_M",
	}, {
		.index  = 3,
		.id     = V4L2_STD_PAL_N,
		.name	= "PAL_N",
	}, {
		.index  = 4,
		.id     = V4L2_STD_PAL_Nc,
		.name	= "PAL_Nc",
	}, {
		.index  = 5,
		.id     = V4L2_STD_PAL_60,
		.name	= "PAL_60",
	}, {
		.index  = 6,
		.id     = V4L2_STD_NTSC_443,
		.name	= "NTSC_443",
	}, {
		.index  = 7,
		.id     = V4L2_STD_480P_60_16_9,
		.name	= "480P_60_16_9",
	}, {
		.index  = 8,
		.id     = V4L2_STD_480P_60_4_3,
		.name	= "480P_60_4_3",
	}, {
		.index  = 9,
		.id     = V4L2_STD_576P_50_16_9,
		.name	= "576P_50_16_9",
	}, {
		.index  = 10,
		.id     = V4L2_STD_576P_50_4_3,
		.name	= "576P_50_4_3",
	}, {
		.index  = 11,
		.id     = V4L2_STD_720P_60,
		.name	= "720P_60",
	}, {
		.index  = 12,
		.id     = V4L2_STD_720P_50,
		.name	= "720P_50",
	}, {
		.index  = 13,
		.id     = V4L2_STD_1080P_60,
		.name	= "1080P_60",
	}, {
		.index  = 14,
		.id     = V4L2_STD_1080P_50,
		.name	= "1080P_50",
	}, {
		.index  = 15,
		.id     = V4L2_STD_1080I_60,
		.name	= "1080I_60",
	}, {
		.index  = 16,
		.id     = V4L2_STD_1080I_50,
		.name	= "1080I_50",
	}, {
		.index  = 17,
		.id     = V4L2_STD_480P_59,
		.name	= "480P_59",
	}, {
		.index  = 18,
		.id     = V4L2_STD_720P_59,
		.name	= "720P_59",
	}, {
		.index  = 19,
		.id     = V4L2_STD_1080I_59,
		.name	= "1080I_59",
	}, {
		.index  = 20,
		.id     = V4L2_STD_1080P_59,
		.name	= "1080I_50",
	}, {
		.index  = 21,
		.id     = V4L2_STD_1080P_30,
		.name	= "1080I_30",
	}
};

/* TODO: set default format for v, vo0/1 */

const struct v4l2_format s5p_tv_format[] = {
	{
		.type = V4L2_BUF_TYPE_VIDEO_OUTPUT,
	}, {
		.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY,
	}, {
		.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY,
	},
};

#define S5P_TVOUT_MAX_STANDARDS          ARRAY_SIZE(s5p_tv_standards)
#define S5P_TVOUT_MAX_O_TYPES            ARRAY_SIZE(s5p_tv_outputs)
#define S5P_TVOUT_MAX_O_FMT              ARRAY_SIZE(s5p_tv_format)
#define S5P_TVOUT_MAX_O_FMT_DESC         ARRAY_SIZE(s5p_tv_o_fmt_desc)
#define S5P_TVOUT_MAX_O_OVERLAY_FMT_DESC ARRAY_SIZE(s5p_tv_o_overlay_fmt_desc)

/* VIDIOC_QUERYCAP handler */
static int s5p_tv_v4l2_querycap(struct file *file, void *fh,
	struct v4l2_capability *cap)
{
	struct s5p_tv_vo *layer = (struct s5p_tv_vo *)fh;
	u32 index;

	if (layer == NULL) {
		index = 0;
		strcpy(cap->driver, "S5P TV Vid drv");
		cap->capabilities = V4L2_CAP_VIDEO_OUTPUT;
	} else {
		index = layer->index + 1;
		strcpy(cap->driver, "S5P TV Grp drv");
		cap->capabilities = V4L2_CAP_VIDEO_OUTPUT_OVERLAY;
	}

	strlcpy(cap->card, g_s5ptv_status.video_dev[index]->name,
		sizeof(cap->card));

	sprintf(cap->bus_info, "ARM AHB BUS");
	cap->version = KERNEL_VERSION(2, 6, 35);

	return 0;
}

/* VIDIOC_ENUM_FMT handlers */
static int s5p_tv_v4l2_enum_fmt_vid_out(struct file *file, void *fh,
	struct v4l2_fmtdesc *f)
{
	int index = f->index;

	V4L2PRINTK("(%d)++\n", f->index);

	if (index >= S5P_TVOUT_MAX_O_FMT_DESC) {
		/*
		pr_err("%s::index(%d) exceeded S5P_TVOUT_MAX_O_FMT_DESC\n",
			__func__, index);
		*/
		return -EINVAL;
	}

	memcpy(f, &s5p_tv_o_fmt_desc[index], sizeof(struct v4l2_fmtdesc));

	V4L2PRINTK("()--\n");
	return 0;
}

static int s5p_tv_v4l2_enum_fmt_vid_out_overlay(struct file *file,
	void *fh, struct v4l2_fmtdesc *f)
{
	int index = f->index;

	V4L2PRINTK("(%d)++\n", f->index);

	if (index >= S5P_TVOUT_MAX_O_OVERLAY_FMT_DESC) {
		pr_err("%s::index(%d) exceeded S5P_TVOUT_MAX_O_OVERLAY_FMT_DESC\n",
			__func__, index);
		return -EINVAL;
	}

	memcpy(f, &s5p_tv_o_overlay_fmt_desc[index],
		sizeof(struct v4l2_fmtdesc));

	V4L2PRINTK("()--\n");

	return 0;
}

/* VIDIOC_G_FMT handlers */
static int s5p_tv_v4l2_g_fmt_vid_out(struct file *file, void *fh,
	struct v4l2_format *f)
{
	struct v4l2_format *vid_out_fmt = f;

	V4L2PRINTK("(0x%08x)++\n", f->type);

	if (vid_out_fmt->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		struct v4l2_pix_format_s5p_tvout vparam;
		memset(&vparam, 0, sizeof(struct v4l2_pix_format_s5p_tvout));

		vparam.base_y =
			(void *)g_s5ptv_status.vl_basic_param.top_y_address;
		vparam.base_c =
			(void *)g_s5ptv_status.vl_basic_param.top_c_address;
		vparam.pix_fmt.pixelformat =
			g_s5ptv_status.src_color;
		vparam.pix_fmt.width =
			g_s5ptv_status.vl_basic_param.src_width;
		vparam.pix_fmt.height =
			g_s5ptv_status.vl_basic_param.src_height;

		memcpy(vid_out_fmt->fmt.raw_data, &vparam,
			sizeof(struct v4l2_pix_format_s5p_tvout));

		V4L2PRINTK("[type 0x%08x] : addr_y: [0x%08x],\
			addr_c [0x%08x], width[%d], height[%d]\n",
			   f->type,
			   g_s5ptv_status.vl_basic_param.top_y_address,
			   g_s5ptv_status.vl_basic_param.top_c_address,
			   g_s5ptv_status.vl_basic_param.src_width,
			   g_s5ptv_status.vl_basic_param.src_height);
	}

	return 0;
}

static int s5p_tv_v4l2_g_fmt_vid_out_overlay(struct file *file,
	void *fh, struct v4l2_format *f)
{
	struct v4l2_format *vid_out_fmt = f;

	V4L2PRINTK("(0x%08x)++\n", f->type);

	if (vid_out_fmt->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY) {
		struct v4l2_window_s5p_tvout vparam;
		memset(&vparam, 0, sizeof(struct v4l2_window_s5p_tvout));

		if (g_s5ptv_status.vl_basic_param.win_blending) {
			vparam.flags		= V4L2_FBUF_FLAG_CHROMAKEY;
			vparam.capability	= V4L2_FBUF_CAP_CHROMAKEY;
		}

		if (g_s5ptv_status.vl_basic_param.alpha) {
			vparam.flags		= V4L2_FBUF_FLAG_LOCAL_ALPHA;
			vparam.capability	= V4L2_FBUF_CAP_LOCAL_ALPHA;
		}

		vparam.priority	=
			g_s5ptv_status.vl_basic_param.priority;
		vparam.win.w.left =
			g_s5ptv_status.vl_basic_param.src_offset_x;
		vparam.win.w.top =
			g_s5ptv_status.vl_basic_param.src_offset_y;
		vparam.win.w.width =
			g_s5ptv_status.vl_basic_param.src_width;
		vparam.win.w.height =
			g_s5ptv_status.vl_basic_param.src_height;

		memcpy(vid_out_fmt->fmt.raw_data, &vparam,
			sizeof(struct v4l2_window_s5p_tvout));

		V4L2PRINTK("[type 0x%08x] : left: [%d], top [%d],\
			width[%d], height[%d]\n",
			   f->type,
			   g_s5ptv_status.vl_basic_param.src_offset_x,
			   g_s5ptv_status.vl_basic_param.src_offset_y,
			   g_s5ptv_status.vl_basic_param.src_width,
			   g_s5ptv_status.vl_basic_param.src_height);
	}

	V4L2PRINTK("()--\n");

	return 0;
}

/* VIDIOC_S_FMT handlers */
static int s5p_tv_v4l2_s_fmt_vid_out(struct file *file, void *fh,
	struct v4l2_format *f)
{
	struct v4l2_format *vid_out_fmt = f;

	V4L2PRINTK("(0x%08x)++\n", f->type);

	if (vid_out_fmt->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		struct v4l2_pix_format_s5p_tvout vparam;
		memcpy(&vparam, vid_out_fmt->fmt.raw_data,
			sizeof(struct v4l2_pix_format_s5p_tvout));

		g_s5ptv_status.vl_basic_param.img_width =
			vparam.pix_fmt.width;
		g_s5ptv_status.vl_basic_param.img_height =
			vparam.pix_fmt.height;
		g_s5ptv_status.src_color =
			vparam.pix_fmt.pixelformat;

		g_s5ptv_status.vl_basic_param.top_y_address =
			(unsigned int)vparam.base_y;
		g_s5ptv_status.vl_basic_param.top_c_address =
			(unsigned int)vparam.base_c;

		/* check progressive or not */
		if (vparam.pix_fmt.field == V4L2_FIELD_NONE) {
			/* progressive */
			switch (vparam.pix_fmt.pixelformat) {
			case V4L2_PIX_FMT_NV12:
				/* linear */
				g_s5ptv_status.src_color =
				VPROC_SRC_COLOR_NV12;
				break;
			case V4L2_PIX_FMT_NV12T:
				/* tiled */
				g_s5ptv_status.src_color =
				VPROC_SRC_COLOR_TILE_NV12;
				break;
			default:
				pr_err("%s::src img format(%d) not supported\n",
					__func__, vparam.pix_fmt.pixelformat);
				return -1;
			}

			g_s5ptv_status.field_id = VPROC_TOP_FIELD;

			if ((g_s5ptv_status.hpd_status) && g_s5ptv_status.vp_layer_enable) {
				struct s5p_video_img_address temp_addr;
				struct s5p_img_size	img_size;

				temp_addr.y_address =
					(unsigned int)vparam.base_y;
				temp_addr.c_address =
					(unsigned int)vparam.base_c;
				img_size.img_width =
					(unsigned int)vparam.pix_fmt.width;
				img_size.img_height =
					(unsigned int)vparam.pix_fmt.height;

				s5p_vlayer_set_top_address((unsigned long)
					&temp_addr);
				s5p_vlayer_set_img_size((unsigned long)
					&img_size);
			}
		} else if (vparam.pix_fmt.field == V4L2_FIELD_INTERLACED_TB) {
			/* interlaced */
			switch (vparam.pix_fmt.pixelformat) {
			case V4L2_PIX_FMT_NV12:
				/* linear */
				g_s5ptv_status.src_color =
				VPROC_SRC_COLOR_NV12IW;
				break;
			case V4L2_PIX_FMT_NV12T:
				/* tiled */
				g_s5ptv_status.src_color =
				VPROC_SRC_COLOR_TILE_NV12IW;
				break;
			default:
				pr_err("%s::src img format(%d) not supported\n",
					__func__, vparam.pix_fmt.pixelformat);
				return -1;
			}

			if (vparam.pix_fmt.priv == V4L2_FIELD_BOTTOM)
				g_s5ptv_status.field_id = VPROC_BOTTOM_FIELD;
			else
				g_s5ptv_status.field_id = VPROC_TOP_FIELD;

			if ((g_s5ptv_status.hpd_status) && g_s5ptv_status.vp_layer_enable) {
				struct s5p_video_img_address temp_addr;
				struct s5p_img_size	img_size;

				temp_addr.y_address =
					(unsigned int)vparam.base_y;
				temp_addr.c_address =
					(unsigned int)vparam.base_c;
				img_size.img_width =
					(unsigned int)vparam.pix_fmt.width;
				img_size.img_height =
					(unsigned int)vparam.pix_fmt.height;

				s5p_vlayer_set_top_address((unsigned long)
					&temp_addr);
				s5p_vlayer_set_img_size((unsigned long)
					&img_size);
			}

		} else {
			pr_err("%s::this field id not supported\n",
				__func__);
			return -1;
		}
	}

	V4L2PRINTK("[type 0x%08x] : addr_y: [0x%08x], addr_c [0x%08x],\
		width[%d], height[%d]\n",
		   f->type,
		   g_s5ptv_status.vl_basic_param.top_y_address,
		   g_s5ptv_status.vl_basic_param.top_c_address,
		   g_s5ptv_status.vl_basic_param.src_width,
		   g_s5ptv_status.vl_basic_param.src_height);
	V4L2PRINTK("()--\n");

	return 0;
}

static int s5p_tv_v4l2_s_fmt_vid_out_overlay(struct file *file, void *fh,
	struct v4l2_format *f)
{
	struct v4l2_format *vid_out_fmt = f;

	V4L2PRINTK("(0x%08x)++\n", f->type);

	if (vid_out_fmt->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY) {
		struct v4l2_window_s5p_tvout vparam;
		memcpy(&vparam, vid_out_fmt->fmt.raw_data,
			sizeof(struct v4l2_window_s5p_tvout));

		g_s5ptv_status.vl_basic_param.win_blending =
			(vparam.flags & V4L2_FBUF_FLAG_CHROMAKEY) ? 1 : 0;
		g_s5ptv_status.vl_basic_param.alpha =
			(vparam.flags & V4L2_FBUF_FLAG_LOCAL_ALPHA) ? 1 : 0;
		g_s5ptv_status.vl_basic_param.priority =
			vparam.priority;
		g_s5ptv_status.vl_basic_param.src_offset_x =
			vparam.win.w.left;
		g_s5ptv_status.vl_basic_param.src_offset_y =
			vparam.win.w.top;
		g_s5ptv_status.vl_basic_param.src_width =
			vparam.win.w.width;
		g_s5ptv_status.vl_basic_param.src_height =
			vparam.win.w.height;

		V4L2PRINTK("[type 0x%08x] : left: [%d], top [%d],\
			width[%d], height[%d]\n",
			   f->type,
			   g_s5ptv_status.vl_basic_param.src_offset_x ,
			   g_s5ptv_status.vl_basic_param.src_offset_y ,
			   g_s5ptv_status.vl_basic_param.src_width,
			   g_s5ptv_status.vl_basic_param.src_height);

		if ((g_s5ptv_status.hpd_status) && g_s5ptv_status.vp_layer_enable) {
			struct s5p_img_offset	img_offset;
			struct s5p_img_size	img_size;

			img_offset.offset_x = vparam.win.w.left;
			img_offset.offset_y = vparam.win.w.top;
			img_size.img_width = vparam.win.w.width;
			img_size.img_height = vparam.win.w.height;

			s5p_vlayer_set_blending(
				g_s5ptv_status.vl_basic_param.win_blending);
			s5p_vlayer_set_alpha(
				g_s5ptv_status.vl_basic_param.alpha);
			s5p_vlayer_set_priority(vparam.priority);
			s5p_vlayer_set_src_position((unsigned long)
				&img_offset);
			s5p_vlayer_set_src_size((unsigned long)
				&img_size);
		}
	}

	V4L2PRINTK("()--\n");

	return 0;
}

/* start overlay * */
static int s5p_tv_v4l2_overlay(struct file *file, void *fh, unsigned int i)
{
	struct s5p_tv_vo *layer = (struct s5p_tv_vo *)fh;
	int	start = i;
	V4L2PRINTK("(0x%08x)++\n", i);

	/*
	* tv dirver is on suspend mode
	* Just set the status variable on this function
	* overlay will be enabled or disabled on resume or
	* handle_cable function according to this status variable
	*/
	if (g_s5ptv_status.suspend_status == true || !(g_s5ptv_status.hpd_status)) {
		if (start)
			g_s5ptv_status.grp_layer_enable[layer->index] = true;
		else
			g_s5ptv_status.grp_layer_enable[layer->index] = false;

		V4L2PRINTK("()it's on suspend mode or hdmi cable is not inserted--\n");
	} else {
		if (start)
			s5p_grp_start(layer->index);
		else
			s5p_grp_stop(layer->index);
	}

	V4L2PRINTK("()--\n");
	return 0;
}

static int s5p_tv_v4l2_g_fbuf(struct file *file, void *fh,
	struct v4l2_framebuffer *a)
{
	struct v4l2_framebuffer *fbuf = a;
	struct s5p_tv_vo *layer = (struct s5p_tv_vo *)fh;

	fbuf->base = (void *)g_s5ptv_overlay[layer->index].base_addr;
	fbuf->fmt.pixelformat = g_s5ptv_overlay[layer->index].fb.fmt.pixelformat;

	return 0;
}

static int s5p_tv_v4l2_s_fbuf(struct file *file, void *fh,
	struct v4l2_framebuffer *a)
{
	struct v4l2_framebuffer *fbuf = a;
	struct s5p_tv_vo *layer = (struct s5p_tv_vo *)fh;

	g_s5ptv_overlay[layer->index].base_addr = (unsigned int)fbuf->base;

	switch (fbuf->fmt.pixelformat) {
	case V4L2_PIX_FMT_RGB565:
		g_s5ptv_overlay[layer->index].fb.fmt.pixelformat =
			VM_DIRECT_RGB565;
		break;
	case V4L2_PIX_FMT_RGB555:
		g_s5ptv_overlay[layer->index].fb.fmt.pixelformat =
			VM_DIRECT_RGB1555;
		break;
	case V4L2_PIX_FMT_RGB444:
		g_s5ptv_overlay[layer->index].fb.fmt.pixelformat =
			VM_DIRECT_RGB4444;
		break;
	case V4L2_PIX_FMT_RGB32:
		g_s5ptv_overlay[layer->index].fb.fmt.pixelformat =
			VM_DIRECT_RGB8888;
		break;
	default:
		pr_err("%s::unmatched pixelformat fail\n", __func__);
		break;
	}

	return 0;
}

/* Stream on/off */
static int s5p_tv_v4l2_streamon(struct file *file, void *fh,
	enum v4l2_buf_type i)
{
	struct s5p_tv_vo *layer = (struct s5p_tv_vo *)fh;

	V4L2PRINTK("(0x%08x)++\n", i);

	/* tv dirver is on suspend mode or hdmi cable is not inserted
	* Just set the status variable on this function
	* overlay will be enabled or disabled on resume or
	* handle_cable function according to this status variable
	*/
	if (g_s5ptv_status.suspend_status == true || !(g_s5ptv_status.hpd_status)) {
		switch (i) {
		/* Vlayer */
		case V4L2_BUF_TYPE_VIDEO_OUTPUT:
			s5p_vlayer_init_param(0);
			g_s5ptv_status.vp_layer_enable = true;
			break;
		/* GRP0/1 */
		case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
			g_s5ptv_status.grp_layer_enable[layer->index] = true;
			break;
		default:
			break;
		}
		V4L2PRINTK("()it's on suspend mode or hdmi cable is not inserted --\n");
		return 0;
	}

	switch (i) {
	/* Vlayer */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		if (!(g_s5ptv_status.vp_layer_enable)) {
			s5p_vlayer_init_param(0);
			s5p_vlayer_start();
			g_s5ptv_status.vp_layer_enable = true;
		/*
		mdelay(50);
		if (g_s5ptv_status.hdcp_en)
			s5p_start_hdcp();
		*/
		} else
			return -EBUSY;

		break;
	/* GRP0/1 */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		s5p_grp_start(layer->index);
		break;
	default:
		break;
	}

	V4L2PRINTK("()--\n");

	return 0;
}

static int s5p_tv_v4l2_streamoff(struct file *file, void *fh,
	enum v4l2_buf_type i)
{
	struct s5p_tv_vo *layer = (struct s5p_tv_vo *)fh;

	V4L2PRINTK("(0x%08x)++\n", i);

	/* tv driver is on suspend mode or hdmi cable is not inserted
	* Each layer was disabled on suspend function already.
	* Just set the status variable on this function
	* Each layer will be enabled or disabled on resume or
	* handle_cable function according to this status variable
	*/
	if (g_s5ptv_status.suspend_status == true || !(g_s5ptv_status.hpd_status)) {
		switch (i) {
		/* Vlayer */
		case V4L2_BUF_TYPE_VIDEO_OUTPUT:
			g_s5ptv_status.vp_layer_enable = false;
			break;
		/* GRP0/1 */
		case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
			g_s5ptv_status.grp_layer_enable[layer->index] = false;
			break;
		default:
			break;
		}
		V4L2PRINTK("()it's on suspend mode or hdmi cable is not inserted--\n");
		return 0;

	}

	switch (i) {
	/* Vlayer */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		s5p_vlayer_stop();
		break;
	/* GRP0/1 */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		s5p_grp_stop(layer->index);
		break;
	default:
		break;
	}

	V4L2PRINTK("()--\n");

	return 0;
}

/* Standard handling ENUMSTD is handled by videodev.c */
static int s5p_tv_v4l2_g_std(struct file *file, void *fh, v4l2_std_id *norm)
{
	V4L2PRINTK("()++\n");

	*norm = g_s5ptv_status.v4l2.std->id;

	V4L2PRINTK("(%d)++\n", (int)(*norm));

	return 0;
}

static int s5p_tv_v4l2_s_std(struct file *file, void *fh, v4l2_std_id *norm)
{
	unsigned int i = 0;
	v4l2_std_id std_id = *norm;

	V4L2PRINTK("(0x%08Lx)++\n", std_id);

	g_s5ptv_status.v4l2.std = NULL;

	do {
		if (s5p_tv_standards[i].id == std_id) {
			g_s5ptv_status.v4l2.std = (struct v4l2_standard *)
				&s5p_tv_standards[i];
			break;
		}

		i++;
	} while (i < S5P_TVOUT_MAX_STANDARDS);

	if (i >= S5P_TVOUT_MAX_STANDARDS || g_s5ptv_status.v4l2.std == NULL) {
		pr_err("%s::There is no tv-out standards :\
			index = 0x%08Lx\n", __func__, std_id);
		return -EINVAL;
	}

	switch (std_id) {
	case V4L2_STD_NTSC_M:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_NTSC_M;
		break;
	case V4L2_STD_PAL_BDGHI:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_PAL_BDGHI;
		break;
	case V4L2_STD_PAL_M:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_PAL_M;
		break;
	case V4L2_STD_PAL_N:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_PAL_N;
		break;
	case V4L2_STD_PAL_Nc:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_PAL_NC;
		break;
	case V4L2_STD_PAL_60:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_PAL_60;
		break;
	case V4L2_STD_NTSC_443:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_NTSC_443;
		break;
	case V4L2_STD_480P_60_16_9:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_480P_60_16_9;
		break;
	case V4L2_STD_480P_60_4_3:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_480P_60_4_3;
		break;
	case V4L2_STD_480P_59:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_480P_59;
		break;
	case V4L2_STD_576P_50_16_9:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_576P_50_16_9;
		break;
	case V4L2_STD_576P_50_4_3:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_576P_50_4_3;
		break;
	case V4L2_STD_720P_60:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_720P_60;
		break;
	case V4L2_STD_720P_59:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_720P_59;
		break;
	case V4L2_STD_720P_50:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_720P_50;
		break;
	case V4L2_STD_1080I_60:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_1080I_60;
		break;
	case V4L2_STD_1080I_59:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_1080I_59;
		break;
	case V4L2_STD_1080I_50:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_1080I_50;
		break;
	case V4L2_STD_1080P_30:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_1080P_30;
		break;
	case V4L2_STD_1080P_60:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_1080P_60;
		break;
	case V4L2_STD_1080P_59:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_1080P_59;
		break;
	case V4L2_STD_1080P_50:
		g_s5ptv_status.tvout_param.disp_mode = TVOUT_1080P_50;
		break;
	default:
		pr_err("%s::not supported standard id :\
			index = 0x%08Lx\n", __func__, std_id);
		return -EINVAL;
		break;
	}

	V4L2PRINTK("()--\n");

	return 0;
}

/* Output handling */
static int s5p_tv_v4l2_enum_output(struct file *file, void *fh,
	struct v4l2_output *a)
{
	unsigned int index = a->index;
	V4L2PRINTK("(%d)++\n", a->index);

	if (index >= S5P_TVOUT_MAX_O_TYPES) {
		pr_err("%s::index(%d) exceeded supported output!!\n",
			__func__, index);
		return -EINVAL;
	}

	memcpy(a, &s5p_tv_outputs[index], sizeof(struct v4l2_output));

	V4L2PRINTK("()--\n");

	return 0;
}

static int s5p_tv_v4l2_g_output(struct file *file, void *fh, unsigned int *i)
{
	V4L2PRINTK("(%d)++\n", *i);

	*i = g_s5ptv_status.v4l2.output->index;

	V4L2PRINTK("()--\n");
	return 0;
}

static int s5p_tv_v4l2_s_output(struct file *file, void *fh, unsigned int i)
{
	V4L2PRINTK("(%d)++\n", i);

	if (i >= S5P_TVOUT_MAX_O_TYPES) {
		pr_err("%s::index(%d) exceeded supported output!!\n",
			__func__, i);
		return -EINVAL;
	}

	g_s5ptv_status.v4l2.output = &s5p_tv_outputs[i];

	switch (g_s5ptv_status.v4l2.output->type) {
	case V4L2_OUTPUT_TYPE_COMPOSITE:
		g_s5ptv_status.tvout_param.out_mode =
			TVOUT_OUTPUT_COMPOSITE;
		break;
	case V4L2_OUTPUT_TYPE_SVIDEO:
		g_s5ptv_status.tvout_param.out_mode =
			TVOUT_OUTPUT_SVIDEO;
		break;
	case V4L2_OUTPUT_TYPE_YPBPR_INERLACED:
		g_s5ptv_status.tvout_param.out_mode =
			TVOUT_OUTPUT_COMPONENT_YPBPR_INERLACED;
		break;
	case V4L2_OUTPUT_TYPE_YPBPR_PROGRESSIVE:
		g_s5ptv_status.tvout_param.out_mode =
			TVOUT_OUTPUT_COMPONENT_YPBPR_PROGRESSIVE;
		break;
	case V4L2_OUTPUT_TYPE_RGB_PROGRESSIVE:
		g_s5ptv_status.tvout_param.out_mode =
			TVOUT_OUTPUT_COMPOSITE;
		break;
	case V4L2_OUTPUT_TYPE_HDMI:
		g_s5ptv_status.tvout_param.out_mode =
			TVOUT_OUTPUT_HDMI;
		break;
	case V4L2_OUTPUT_TYPE_HDMI_RGB:
		g_s5ptv_status.tvout_param.out_mode =
			TVOUT_OUTPUT_HDMI_RGB;
		break;
	case V4L2_OUTPUT_TYPE_DVI:
		g_s5ptv_status.tvout_param.out_mode =
			TVOUT_OUTPUT_DVI;
		break;
	default:
		break;
	}

	if (g_s5ptv_status.tvout_param.out_mode != TVOUT_OUTPUT_HDMI &&
	    g_s5ptv_status.tvout_param.out_mode != TVOUT_OUTPUT_HDMI_RGB) {
		if (!(g_s5ptv_status.hpd_status))
			s5p_tv_base_clk_gate(true);

		g_s5ptv_status.hpd_status = 1;
	}

	s5p_tv_if_set_disp();

	V4L2PRINTK("()--\n");

	return 0;
};

/* Crop ioctls */

/*
 * Video Format Name	Pixel aspect ratio		Description
 *			STD(4:3)	Anamorphic(16:9)
 * 640x480	4:3				Used on YouTube
 * 720x576	576i	5:4		64:45		Used on D1/DV PAL
 * 704x576	576p	12:11		16:11		Used on EDTV PAL
 * 720x480	480i	8:9		32:27		Used on DV NTSC
 * 720x486	480i	8:9		32:27		Used on D1 NTSC
 * 704x480	480p	10:11		40:33		Used on EDTV NTSC
 */

static int s5p_tv_v4l2_cropcap(struct file *file, void *fh,
	struct v4l2_cropcap *a)
{
	struct v4l2_cropcap *cropcap = a;

	switch (cropcap->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		break;
	default:
		pr_err("%s::unmatched cropcap->type fail\n", __func__);
		return -1;
		break;
	}

	switch (g_s5ptv_status.tvout_param.disp_mode) {
	case TVOUT_NTSC_M:
	case TVOUT_NTSC_443:
	case TVOUT_480P_60_16_9:
	case TVOUT_480P_60_4_3:
	case TVOUT_480P_59:
		cropcap->bounds.top = 0;
		cropcap->bounds.left = 0;
		cropcap->bounds.width = 720;
		cropcap->bounds.height = 480;

		cropcap->defrect.top = 0;
		cropcap->defrect.left = 0;
		cropcap->defrect.width = 720;
		cropcap->defrect.height = 480;
		break;
	case TVOUT_PAL_M:
	case TVOUT_PAL_BDGHI:
	case TVOUT_PAL_N:
	case TVOUT_PAL_NC:
	case TVOUT_PAL_60:
	case TVOUT_576P_50_16_9:
	case TVOUT_576P_50_4_3:
		cropcap->bounds.top = 0;
		cropcap->bounds.left = 0;
		cropcap->bounds.width = 720;
		cropcap->bounds.height = 576;

		cropcap->defrect.top = 0;
		cropcap->defrect.left = 0;
		cropcap->defrect.width = 720;
		cropcap->defrect.height = 576;
		break;
	case TVOUT_720P_60:
	case TVOUT_720P_59:
	case TVOUT_720P_50:
		cropcap->bounds.top = 0;
		cropcap->bounds.left = 0;
		cropcap->bounds.width = 1280;
		cropcap->bounds.height = 720;

		cropcap->defrect.top = 0;
		cropcap->defrect.left = 0;
		cropcap->defrect.width = 1280;
		cropcap->defrect.height = 720;
		break;
	case TVOUT_1080I_60:
	case TVOUT_1080I_59:
	case TVOUT_1080I_50:
	case TVOUT_1080P_60:
	case TVOUT_1080P_59:
	case TVOUT_1080P_50:
	case TVOUT_1080P_30:
		cropcap->bounds.top = 0;
		cropcap->bounds.left = 0;
		cropcap->bounds.width = 1920;
		cropcap->bounds.height = 1080;

		cropcap->defrect.top = 0;
		cropcap->defrect.left = 0;
		cropcap->defrect.width = 1920;
		cropcap->defrect.height = 1080;
		break;
	default:
		pr_err("%s::unmatched disp_mode(%d) fail\n",
			__func__,
			g_s5ptv_status.tvout_param.disp_mode);
		return -1;
		break;

	}

	V4L2PRINTK("[input type 0x%08x] : left: [%d], top [%d],\
		width[%d], height[%d]\n",
		   g_s5ptv_status.tvout_param.disp_mode,
		   cropcap->bounds.top ,
		   cropcap->bounds.left ,
		   cropcap->bounds.width,
		   cropcap->bounds.height);

	return 0;
}

static int s5p_tv_v4l2_g_crop(struct file *file, void *fh, struct v4l2_crop *a)
{
	struct v4l2_crop *crop = a;
	struct s5p_tv_vo *layer = (struct s5p_tv_vo *)fh;

	switch (crop->type) {
	/* Vlayer */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		crop->c.left	= g_s5ptv_status.vl_basic_param.dest_offset_x;
		crop->c.top	= g_s5ptv_status.vl_basic_param.dest_offset_y;
		crop->c.width	= g_s5ptv_status.vl_basic_param.dest_width;
		crop->c.height	= g_s5ptv_status.vl_basic_param.dest_height;
		break;
	/* GRP0/1 */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		crop->c.left	= g_s5ptv_overlay[layer->index].dst_rect.left;
		crop->c.top	= g_s5ptv_overlay[layer->index].dst_rect.top;
		crop->c.width	= g_s5ptv_overlay[layer->index].dst_rect.width;
		crop->c.height	= g_s5ptv_overlay[layer->index].dst_rect.height;
		break;
	default:
		break;
	}

	return 0;
}

static int s5p_tv_v4l2_s_crop(struct file *file, void *fh, struct v4l2_crop *a)
{
	struct v4l2_crop *crop = a;
	struct s5p_tv_vo *layer = (struct s5p_tv_vo *)fh;

	switch (crop->type) {
	/* Vlayer - scaling!! */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		g_s5ptv_status.vl_basic_param.dest_offset_x = crop->c.left;
		g_s5ptv_status.vl_basic_param.dest_offset_y = crop->c.top;
		g_s5ptv_status.vl_basic_param.dest_width = crop->c.width;
		g_s5ptv_status.vl_basic_param.dest_height = crop->c.height;

		if ((g_s5ptv_status.hpd_status) && g_s5ptv_status.vp_layer_enable) {
			struct s5p_img_size img_size;
			struct s5p_img_offset img_offset;
			img_size.img_width  = crop->c.width;
			img_size.img_height = crop->c.height;
			img_offset.offset_x = crop->c.left;
			img_offset.offset_y = crop->c.top;

			s5p_vlayer_set_dest_size((unsigned long)
				&img_size);
			s5p_vlayer_set_dest_position((unsigned long)
				&img_offset);
		}
		break;
	/* GRP0/1 */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		g_s5ptv_overlay[layer->index].dst_rect.left = crop->c.left;
		g_s5ptv_overlay[layer->index].dst_rect.top = crop->c.top;
		g_s5ptv_overlay[layer->index].dst_rect.width = crop->c.width;
		g_s5ptv_overlay[layer->index].dst_rect.height = crop->c.height;
		break;
	default:
		break;
	}

	return 0;
}

/* Stream type-dependent parameter ioctls */
static int s5p_tv_v4l2_g_parm_v(struct file *file, void *fh,
	struct v4l2_streamparm *a)
{
	struct v4l2_streamparm *param = a;

	struct v4l2_window_s5p_tvout vparam;

	if (g_s5ptv_status.vl_basic_param.win_blending) {
		vparam.flags		= V4L2_FBUF_FLAG_GLOBAL_ALPHA;
		vparam.capability	= V4L2_FBUF_FLAG_GLOBAL_ALPHA;
	}

	vparam.priority		= g_s5ptv_status.vl_basic_param.priority;
	vparam.win.w.left	= g_s5ptv_status.vl_basic_param.src_offset_x;
	vparam.win.w.top	= g_s5ptv_status.vl_basic_param.src_offset_y;
	vparam.win.w.width	= g_s5ptv_status.vl_basic_param.src_width;
	vparam.win.w.height	= g_s5ptv_status.vl_basic_param.src_height;
	vparam.win.global_alpha = g_s5ptv_status.vl_basic_param.alpha;

	memcpy(param->parm.raw_data, &vparam,
		sizeof(struct v4l2_window_s5p_tvout));

	return 0;
}

static int s5p_tv_v4l2_s_parm_v(struct file *file, void *fh,
	struct v4l2_streamparm *a)
{
	struct v4l2_streamparm *param = a;

	struct v4l2_window_s5p_tvout vparam;

	memcpy(&vparam, param->parm.raw_data,
		sizeof(struct v4l2_window_s5p_tvout));

	g_s5ptv_status.vl_basic_param.win_blending =
		(vparam.flags & V4L2_FBUF_FLAG_GLOBAL_ALPHA) ? 1 : 0;
	g_s5ptv_status.vl_basic_param.alpha = vparam.win.global_alpha;
	g_s5ptv_status.vl_basic_param.priority = vparam.priority;
	g_s5ptv_status.vl_basic_param.src_offset_x = vparam.win.w.left;
	g_s5ptv_status.vl_basic_param.src_offset_y = vparam.win.w.top;
	g_s5ptv_status.vl_basic_param.src_width = vparam.win.w.width;
	g_s5ptv_status.vl_basic_param.src_height = vparam.win.w.height;

	V4L2PRINTK("[type 0x%08x] : left: [%d], top [%d],\
		width[%d], height[%d]\n",
		   a->type,
		   g_s5ptv_status.vl_basic_param.src_offset_x ,
		   g_s5ptv_status.vl_basic_param.src_offset_y ,
		   g_s5ptv_status.vl_basic_param.src_width,
		   g_s5ptv_status.vl_basic_param.src_height);

	if ((g_s5ptv_status.hpd_status) && g_s5ptv_status.vp_layer_enable) {
		struct s5p_img_offset	img_offset;
		struct s5p_img_size	img_size;

		img_offset.offset_x = vparam.win.w.left;
		img_offset.offset_y = vparam.win.w.top;
		img_size.img_width  = vparam.win.w.width;
		img_size.img_height = vparam.win.w.height;

		s5p_vlayer_set_blending(g_s5ptv_status.vl_basic_param.win_blending);
		s5p_vlayer_set_alpha(g_s5ptv_status.vl_basic_param.alpha);
		s5p_vlayer_set_priority(vparam.priority);
		s5p_vlayer_set_src_position((unsigned long)&img_offset);
		s5p_vlayer_set_src_size((unsigned long)&img_size);
	}

	return 0;
}

static int s5p_tv_v4l2_g_parm_vo(struct file *file, void *fh,
	struct v4l2_streamparm *a)
{
	struct v4l2_streamparm *param = a;

	struct v4l2_window_s5p_tvout vparam;
	struct s5p_tv_vo *layer = (struct s5p_tv_vo *)fh;

	memset(&vparam, 0, sizeof(struct v4l2_window_s5p_tvout));

	V4L2PRINTK("entered\n");

	if (g_s5ptv_overlay[layer->index].win_blending) {
		vparam.flags		= V4L2_FBUF_FLAG_GLOBAL_ALPHA;
		vparam.capability	= V4L2_FBUF_CAP_GLOBAL_ALPHA;
	}

	if (g_s5ptv_overlay[layer->index].blank_change) {
		vparam.flags		|= V4L2_FBUF_FLAG_CHROMAKEY;
		vparam.capability	|= V4L2_FBUF_CAP_CHROMAKEY;
	}

	if (g_s5ptv_overlay[layer->index].pixel_blending) {
		vparam.flags		|= V4L2_FBUF_FLAG_LOCAL_ALPHA;
		vparam.capability	|= V4L2_FBUF_CAP_LOCAL_ALPHA;
	}

	if (g_s5ptv_overlay[layer->index].pre_mul) {
		vparam.flags		|= V4L2_FBUF_FLAG_PRE_MULTIPLY;
		vparam.capability	|= V4L2_FBUF_CAP_PRE_MULTIPLY;
	}

	vparam.priority		= g_s5ptv_overlay[layer->index].priority;

	vparam.win.chromakey	= g_s5ptv_overlay[layer->index].blank_color;
	vparam.win.w.left	= g_s5ptv_overlay[layer->index].dst_rect.left;
	vparam.win.w.top	= g_s5ptv_overlay[layer->index].dst_rect.top;
	vparam.win.w.left	= g_s5ptv_overlay[layer->index].win.w.left;
	vparam.win.w.top	= g_s5ptv_overlay[layer->index].win.w.top;
	vparam.win.w.width	= g_s5ptv_overlay[layer->index].win.w.width;
	vparam.win.w.height	= g_s5ptv_overlay[layer->index].win.w.height;
	vparam.win.global_alpha	= g_s5ptv_overlay[layer->index].win.global_alpha;

	vparam.win.w.width	= g_s5ptv_overlay[layer->index].fb.fmt.bytesperline;

	memcpy(param->parm.raw_data, &vparam,
		sizeof(struct v4l2_window_s5p_tvout));

	return 0;
}

static int s5p_tv_v4l2_s_parm_vo(struct file *file, void *fh,
	struct v4l2_streamparm *a)
{
	struct v4l2_streamparm *param = a;

	struct v4l2_window_s5p_tvout vparam;

	struct s5p_tv_vo *layer = (struct s5p_tv_vo *)fh;
	memcpy(&vparam, param->parm.raw_data,
		sizeof(struct v4l2_window_s5p_tvout));

	g_s5ptv_overlay[layer->index].win_blending =
		(vparam.flags & V4L2_FBUF_FLAG_GLOBAL_ALPHA) ? 1 : 0;
	g_s5ptv_overlay[layer->index].blank_change =
		(vparam.flags & V4L2_FBUF_FLAG_CHROMAKEY) ? 1 : 0;
	g_s5ptv_overlay[layer->index].pixel_blending =
		(vparam.flags & V4L2_FBUF_FLAG_LOCAL_ALPHA) ? 1 : 0;
	g_s5ptv_overlay[layer->index].pre_mul =
		(vparam.flags & V4L2_FBUF_FLAG_PRE_MULTIPLY) ? 1 : 0;
	g_s5ptv_overlay[layer->index].priority =
		vparam.priority;
	g_s5ptv_overlay[layer->index].blank_color =
		vparam.win.chromakey;
	g_s5ptv_overlay[layer->index].dst_rect.left =
		vparam.win.w.left;
	g_s5ptv_overlay[layer->index].dst_rect.top =
		vparam.win.w.top;
	g_s5ptv_overlay[layer->index].win.w.left =
		vparam.win.w.left;
	g_s5ptv_overlay[layer->index].win.w.top =
		vparam.win.w.top;
	g_s5ptv_overlay[layer->index].win.w.width =
		vparam.win.w.width;
	g_s5ptv_overlay[layer->index].win.w.height =
		vparam.win.w.height;
	g_s5ptv_overlay[layer->index].win.global_alpha =
		vparam.win.global_alpha;

	g_s5ptv_overlay[layer->index].fb.fmt.bytesperline =
		vparam.win.w.width;

	return 0;
}

void s5p_tv_v4l2_init_param(void)
{
	g_s5ptv_status.tvout_param.out_mode = TVOUT_OUTPUT_COMPOSITE;
	g_s5ptv_status.v4l2.output = (struct v4l2_output *)&s5p_tv_outputs[0];
	g_s5ptv_status.v4l2.std = (struct v4l2_standard *)&s5p_tv_standards[0];
	g_s5ptv_status.v4l2.fmt_v = (struct v4l2_format *)&s5p_tv_o_fmt_desc[0];
	g_s5ptv_status.v4l2.fmt_vo_0 = (struct v4l2_format *)&s5p_tv_format[1];
	g_s5ptv_status.v4l2.fmt_vo_1 = (struct v4l2_format *)&s5p_tv_format[2];
	g_s5ptv_status.hdmi_audio_type = HDMI_AUDIO_PCM;
}

long s5p_tv_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	switch (cmd) {
	case VIDIOC_HDCP_ENABLE:
		g_s5ptv_status.hdcp_en = (unsigned int) arg;
		V4L2PRINTK("HDCP status is %s\n",
		       g_s5ptv_status.hdcp_en ? "enabled" : "disabled");
		break;
	case VIDIOC_HDCP_STATUS: {
		unsigned int *status = (unsigned int *)&arg;

		*status = 1;

		V4L2PRINTK("HPD status is %s\n",
		       g_s5ptv_status.hpd_status ? "plugged" : "unplugged");
		break;
	}
	case VIDIOC_HDCP_PROT_STATUS: {
		unsigned int *prot = (unsigned int *)&arg;

		*prot = 1;

		V4L2PRINTK("hdcp prot status is %d\n",
		       g_hdcp_protocol_status);
		break;
	}
	case VIDIOC_ENUMSTD: {
		struct v4l2_standard *p = (struct v4l2_standard *)arg;

		if (p->index >= S5P_TVOUT_MAX_STANDARDS) {
			/*
			pr_err("%s::index(%d) exceeded S5P_TVOUT_MAX_STANDARDS\n",
				__func__, p->index);
			*/
			ret = -EINVAL;
		} else {
			memcpy(p, &s5p_tv_standards[p->index],
			sizeof(struct v4l2_standard));
		}
		break;
	}
	default:
		/*
		 * in 2.6.28 version Mauro Carvalho Chehab added for removing inode
		 * but 2.6.29 is not applied. what is it?
		 */
		ret = video_ioctl2(file, cmd, arg);
		break;
	}

	return ret;
}

long s5p_tv_vid_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct video_device *vfd = video_devdata(file);
	const struct v4l2_ioctl_ops *ops = vfd->ioctl_ops;
	long ret = 0;

	switch (cmd) {
	case VIDIOC_S_FMT: {
		struct v4l2_format *f = (struct v4l2_format *)arg;
		void *fh = file->private_data;

		if (ops->vidioc_s_fmt_vid_out)
			ret = ops->vidioc_s_fmt_vid_out(file, fh, f);
		else
			ret = -EINVAL;
	}
	default:		
		/*
		 * in 2.6.28 version Mauro Carvalho Chehab added for removing inode
		 * but 2.6.29 is not applied. what is it?
		 */
		ret = video_ioctl2(file, cmd, arg);
		break;
	}
	return ret;
}

long s5p_tv_v_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct video_device *vfd = video_devdata(file);
	const struct v4l2_ioctl_ops *ops = vfd->ioctl_ops;
	long ret = 0;

	switch (cmd) {
	case VIDIOC_INIT_AUDIO: {
		bool flag_audio_enable;

		g_s5ptv_status.hdmi_audio_type = (unsigned int) arg;

		if (g_s5ptv_status.hdmi_audio_type == HDMI_AUDIO_PCM)
			flag_audio_enable = true;
		else
			flag_audio_enable = false;

		s5p_hdmi_set_audio(flag_audio_enable);

		if (g_s5ptv_status.tvout_output_enable == true)
			s5p_hdmi_audio_enable(flag_audio_enable);
		break;
	}
	case VIDIOC_AV_MUTE: {
		bool flag_audio_enable;
		bool flag_video_bs;
		bool flag_audio_mute;

		if ((unsigned int)arg == HDMI_AUDIO_PCM) {
			g_s5ptv_status.hdmi_audio_type = HDMI_AUDIO_NO;
			flag_audio_enable = false;
			flag_video_bs     = true;
			flag_audio_mute   = true;
		} else {
			g_s5ptv_status.hdmi_audio_type = HDMI_AUDIO_PCM;
			flag_audio_enable = true;
			flag_video_bs     = false;
			flag_audio_mute   = false;
		}
		if (g_s5ptv_status.tvout_output_enable) {
			s5p_hdmi_audio_enable(flag_audio_enable);
			s5p_hdmi_video_set_bluescreen(flag_video_bs, 0, 0, 0);
		}

		s5p_hdmi_set_mute(flag_audio_mute);
		break;
	}
	case VIDIOC_G_AVMUTE:
		ret = s5p_hdmi_get_mute();
		break;
	case VIDIOC_S_FMT: {
		struct v4l2_format *f = (struct v4l2_format *)arg;
		void *fh = file->private_data;

		if (ops->vidioc_s_fmt_vid_out)
			ret = ops->vidioc_s_fmt_vid_out(file, fh, f);
		else
			ret = -EINVAL;
		break;
	}
	case VIDIOC_HDCP_ENABLE: {
		unsigned int *hdcp_en = (unsigned int *)arg;
		g_s5ptv_status.hdcp_en = *hdcp_en;
		V4L2PRINTK("HDCP status is %s\n",
		       g_s5ptv_status.hdcp_en ? "enabled" : "disabled");
		break;
	}
	case VIDIOC_HDCP_STATUS: {
		unsigned int *status = (unsigned int *)arg;

		*status = g_s5ptv_status.hpd_status;

		V4L2PRINTK("HPD status is %s\n",
		       g_s5ptv_status.hpd_status ? "plugged" : "unplugged");
		break;
	}
	case VIDIOC_HDCP_PROT_STATUS: {
		unsigned int *prot = (unsigned int *)&arg;

		*prot = 1;

		V4L2PRINTK("hdcp prot status is %d\n",
		       g_hdcp_protocol_status);
		break;
	}
	case VIDIOC_ENUMSTD: {
		struct v4l2_standard *p = (struct v4l2_standard *)arg;

		if (p->index >= S5P_TVOUT_MAX_STANDARDS) {
			/*
			pr_err("%s::index(%d) exceeded S5P_TVOUT_MAX_STANDARDS\n",
				__func__, p->index);
			*/
			ret = -EINVAL;
		} else {
			memcpy(p, &s5p_tv_standards[p->index],
				sizeof(struct v4l2_standard));
		}
		break;
	}
	default:
		/*
		 * in 2.6.28 version Mauro Carvalho Chehab added for removing inode
		 * but 2.6.29 is not applied. what is it?
		 */
		ret = video_ioctl2(file, cmd, arg);
		break;
	}

	return ret;
}

long s5p_tv_vo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void *fh = file->private_data;
	long ret = 0;

	switch (cmd) {
	case VIDIOC_ENUM_FMT: {
		struct v4l2_fmtdesc *f = (struct v4l2_fmtdesc *)arg;
		enum v4l2_buf_type type;
		unsigned int index;

		index = f->index;
		type  = f->type;

		if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY) {
			memset(f, 0, sizeof(*f));
			f->index = index;
			f->type  = type;

			ret = s5p_tv_v4l2_enum_fmt_vid_out_overlay(file,
				fh, f);
		} else {
			/*
			 * in 2.6.28 version Mauro Carvalho Chehab added for removing inode
			 * but 2.6.29 is not applied. what is it?
			 */
			ret = video_ioctl2(file, cmd, arg);
		}
		break;
	}
	case VIDIOC_G_FMT: {
		struct v4l2_format *f = (struct v4l2_format *)arg;
		ret = s5p_tv_v4l2_g_fmt_vid_out_overlay(file, fh, f);
		break;
	}
	case VIDIOC_TV_OUT_BASE_ADDR: {
		unsigned int base_addr = *((unsigned int *)arg);
		ret = s5p_vm_set_grp_base_address(0, base_addr);
		break;
	}
	default:
		/*
		 * in 2.6.28 version Mauro Carvalho Chehab added for removing inode
		 * but 2.6.29 is not applied. what is it?
		 */
		ret = video_ioctl2(file, cmd, arg);
		break;
	}

	return ret;
}

const struct v4l2_ioctl_ops g_s5p_tv_v4l2_ops = {
	.vidioc_querycap		= s5p_tv_v4l2_querycap,
	.vidioc_g_std			= s5p_tv_v4l2_g_std,
	.vidioc_s_std			= s5p_tv_v4l2_s_std,
	.vidioc_enum_output		= s5p_tv_v4l2_enum_output,
	.vidioc_g_output		= s5p_tv_v4l2_g_output,
	.vidioc_s_output		= s5p_tv_v4l2_s_output,
};

const struct v4l2_ioctl_ops g_s5p_tv_v4l2_vid_ops = {
	.vidioc_querycap		= s5p_tv_v4l2_querycap,
	.vidioc_enum_fmt_vid_out	= s5p_tv_v4l2_enum_fmt_vid_out,
	.vidioc_g_fmt_vid_out		= s5p_tv_v4l2_g_fmt_vid_out,
	.vidioc_s_fmt_vid_out		= s5p_tv_v4l2_s_fmt_vid_out,
	.vidioc_streamon		= s5p_tv_v4l2_streamon,
	.vidioc_streamoff		= s5p_tv_v4l2_streamoff,
	.vidioc_cropcap			= s5p_tv_v4l2_cropcap,
	.vidioc_g_crop			= s5p_tv_v4l2_g_crop,
	.vidioc_s_crop			= s5p_tv_v4l2_s_crop,
	.vidioc_g_parm			= s5p_tv_v4l2_g_parm_v,
	.vidioc_s_parm			= s5p_tv_v4l2_s_parm_v,
};

const struct v4l2_ioctl_ops g_s5p_tv_v4l2_v_ops = {
	.vidioc_querycap		= s5p_tv_v4l2_querycap,
	.vidioc_enum_fmt_vid_out	= s5p_tv_v4l2_enum_fmt_vid_out,
	.vidioc_g_fmt_vid_out		= s5p_tv_v4l2_g_fmt_vid_out,
	.vidioc_s_fmt_vid_out		= s5p_tv_v4l2_s_fmt_vid_out,
	.vidioc_streamon		= s5p_tv_v4l2_streamon,
	.vidioc_streamoff		= s5p_tv_v4l2_streamoff,
	.vidioc_g_std			= s5p_tv_v4l2_g_std,
	.vidioc_s_std			= s5p_tv_v4l2_s_std,
	.vidioc_enum_output		= s5p_tv_v4l2_enum_output,
	.vidioc_g_output		= s5p_tv_v4l2_g_output,
	.vidioc_s_output		= s5p_tv_v4l2_s_output,
	.vidioc_cropcap			= s5p_tv_v4l2_cropcap,
	.vidioc_g_crop			= s5p_tv_v4l2_g_crop,
	.vidioc_s_crop			= s5p_tv_v4l2_s_crop,
	.vidioc_g_parm			= s5p_tv_v4l2_g_parm_v,
	.vidioc_s_parm			= s5p_tv_v4l2_s_parm_v,
};

const struct v4l2_ioctl_ops g_s5p_tv_v4l2_vo_ops = {
	.vidioc_querycap		= s5p_tv_v4l2_querycap,
	.vidioc_g_fmt_vid_out_overlay	= s5p_tv_v4l2_g_fmt_vid_out_overlay,
	.vidioc_s_fmt_vid_out_overlay	= s5p_tv_v4l2_s_fmt_vid_out_overlay,
	.vidioc_overlay			= s5p_tv_v4l2_overlay,
	.vidioc_g_fbuf			= s5p_tv_v4l2_g_fbuf,
	.vidioc_s_fbuf			= s5p_tv_v4l2_s_fbuf,
	.vidioc_streamon		= s5p_tv_v4l2_streamon,
	.vidioc_streamoff		= s5p_tv_v4l2_streamoff,
	.vidioc_cropcap			= s5p_tv_v4l2_cropcap,
	.vidioc_g_crop			= s5p_tv_v4l2_g_crop,
	.vidioc_s_crop			= s5p_tv_v4l2_s_crop,
	.vidioc_g_parm			= s5p_tv_v4l2_g_parm_vo,
	.vidioc_s_parm			= s5p_tv_v4l2_s_parm_vo,
};
