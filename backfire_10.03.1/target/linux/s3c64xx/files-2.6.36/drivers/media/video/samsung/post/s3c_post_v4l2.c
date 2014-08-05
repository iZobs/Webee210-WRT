/* linux/drivers/media/video/samsung/s3c_post_v4l2.c
 *
 * V4L2 interface support file for Samsung Post Processor driver
 *
 * Jinsung Yang, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/videodev2.h>
#include <media/v4l2-ioctl.h>

#include "s3c_post.h"

static struct v4l2_input s3c_post_input_types[] = {
	{
		.index		= 0,
		.name		= "Memory Input",
		.type		= V4L2_INPUT_TYPE_MEMORY,
		.audioset	= 2,
		.tuner		= 0,
		.std		= V4L2_STD_PAL_BG | V4L2_STD_NTSC_M,
		.status		= 0,
	}
};

static struct v4l2_output s3c_post_output_types[] = {
	{
		.index		= 0,
		.name		= "Memory Output",
		.type		= V4L2_OUTPUT_TYPE_MEMORY,
		.audioset	= 0,
		.modulator	= 0, 
		.std		= 0,
	}, 
	{
		.index		= 1,
		.name		= "LCD FIFO Output",
		.type		= V4L2_OUTPUT_TYPE_LCDFIFO,
		.audioset	= 0,
		.modulator	= 0,
		.std		= 0,
	} 
};

const static struct v4l2_fmtdesc s3c_post_capture_formats[] = {
	{
		.index		= 0,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_PLANAR,
		.description	= "4:2:0, planar, Y-Cb-Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV420,
	},
	{
		.index		= 1,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_PLANAR,
		.description	= "4:2:2, planar, Y-Cb-Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV422P,

	},	
	{
		.index		= 2,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_PACKED,
		.description	= "4:2:2, packed, YCBYCR",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
	},
	{
		.index		= 3,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_PACKED,
		.description	= "4:2:2, packed, CBYCRY",
		.pixelformat	= V4L2_PIX_FMT_UYVY,
	}
};

#define S3C_POST_MAX_INPUT_TYPES	ARRAY_SIZE(s3c_post_input_types)
#define S3C_POST_MAX_OUTPUT_TYPES	ARRAY_SIZE(s3c_post_output_types)
#define S3C_POST_MAX_CAPTURE_FORMATS	ARRAY_SIZE(s3c_post_capture_formats)

static int s3c_post_v4l2_querycap(struct file *filp, void *fh,
					struct v4l2_capability *cap)
{
	struct s3c_post_control *ctrl = (struct s3c_post_control *) fh;

	strcpy(cap->driver, "Samsung Post Processor Driver");
	strlcpy(cap->card, ctrl->vd->name, sizeof(cap->card));
	sprintf(cap->bus_info, "AHB-bus");

	cap->version = 0;
	cap->capabilities = (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING);

	return 0;
}

static int s3c_post_v4l2_enum_fmt_vid_cap(struct file *filp, void *fh,
					struct v4l2_fmtdesc *f)
{
	struct s3c_post_control *ctrl = (struct s3c_post_control *) fh;
	int index = f->index;

	if (index >= S3C_POST_MAX_CAPTURE_FORMATS)
		return -EINVAL;

	memset(f, 0, sizeof(*f));
	memcpy(f, ctrl->v4l2.fmtdesc + index, sizeof(*f));

	return 0;
}

static int s3c_post_v4l2_g_fmt_vid_cap(struct file *filp, void *fh,
					struct v4l2_format *f)
{
	struct s3c_post_control *ctrl = (struct s3c_post_control *) fh;
	int size = sizeof(struct v4l2_pix_format);

	memset(&f->fmt.pix, 0, size);
	memcpy(&f->fmt.pix, &(ctrl->v4l2.frmbuf.fmt), size);

	return 0;
}

static int s3c_post_v4l2_s_fmt_vid_cap(struct file *filp, void *fh,
					struct v4l2_format *f)
{
	struct s3c_post_control *ctrl = (struct s3c_post_control *) fh;

	ctrl->v4l2.frmbuf.fmt = f->fmt.pix;

	if (f->fmt.pix.priv == V4L2_FMT_IN)
		s3c_post_set_input_frame(ctrl, &f->fmt.pix);
	else
		s3c_post_set_output_frame(ctrl, &f->fmt.pix);

	return 0;
}

static int s3c_post_v4l2_try_fmt_vid_cap(struct file *filp, void *fh,
					  struct v4l2_format *f)
{
	return 0;
}

static int s3c_post_v4l2_g_ctrl(struct file *filp, void *fh,
					struct v4l2_control *c)
{
	struct s3c_post_control *ctrl = (struct s3c_post_control *) fh;
	struct s3c_post_out_frame *frame = &ctrl->out_frame;

	switch (c->id) {
	case V4L2_CID_OUTPUT_ADDR:
		c->value = frame->addr[c->value].phys_y;
		break;

	default:
		err("invalid control id: %d\n", c->id);
		return -EINVAL;
	}
	
	return 0;
}

static int s3c_post_v4l2_s_ctrl(struct file *filp, void *fh,
					struct v4l2_control *c)
{
	struct s3c_post_control *ctrl = (struct s3c_post_control *) fh;

	switch (c->id) {
	case V4L2_CID_NR_FRAMES:
		s3c_post_set_nr_frames(ctrl, c->value);
		break;

	case V4L2_CID_INPUT_ADDR:
		s3c_post_alloc_input_memory(&ctrl->in_frame, \
						(dma_addr_t) c->value);
		s3c_post_set_input_address(ctrl);
		break;

	case V4L2_CID_INPUT_ADDR_Y:
	case V4L2_CID_INPUT_ADDR_RGB:
		s3c_post_alloc_y_memory(&ctrl->in_frame, \
						(dma_addr_t) c->value);
		s3c_post_set_input_address(ctrl);
		break;

	case V4L2_CID_INPUT_ADDR_CB:	/* fall through */
	case V4L2_CID_INPUT_ADDR_CBCR:
		s3c_post_alloc_cb_memory(&ctrl->in_frame, \
						(dma_addr_t) c->value);
		s3c_post_set_input_address(ctrl);
		break;

	case V4L2_CID_INPUT_ADDR_CR:
		s3c_post_alloc_cr_memory(&ctrl->in_frame, \
						(dma_addr_t) c->value);
		s3c_post_set_input_address(ctrl);
		break;

	case V4L2_CID_RESET:
		s3c_post_reset(ctrl);
		break;

	default:
		err("invalid control id: %d\n", c->id);
		return -EINVAL;
	}

	return 0;
}

static int s3c_post_v4l2_streamon(struct file *filp, void *fh,
					enum v4l2_buf_type i)
{
	struct s3c_post_control *ctrl = (struct s3c_post_control *) fh;
	
	if (i != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	s3c_post_start_dma(ctrl);

	return 0;
}

static int s3c_post_v4l2_streamoff(struct file *filp, void *fh,
					enum v4l2_buf_type i)
{
	struct s3c_post_control *ctrl = (struct s3c_post_control *) fh;
	
	if (i != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	s3c_post_stop_dma(ctrl);
	s3c_post_free_output_memory(&ctrl->out_frame);
	s3c_post_set_output_address(ctrl);

	return 0;
}

static int s3c_post_v4l2_g_input(struct file *filp, void *fh,
					unsigned int *i)
{
	struct s3c_post_control *ctrl = (struct s3c_post_control *) fh;

	*i = ctrl->v4l2.input->index;

	return 0;
}

static int s3c_post_v4l2_s_input(struct file *filp, void *fh,
					unsigned int i)
{
	struct s3c_post_control *ctrl = (struct s3c_post_control *) fh;

	if (i >= S3C_POST_MAX_INPUT_TYPES)
		return -EINVAL;

	ctrl->v4l2.input = &s3c_post_input_types[i];

	return 0;
}

static int s3c_post_v4l2_g_output(struct file *filp, void *fh,
					unsigned int *i)
{
	struct s3c_post_control *ctrl = (struct s3c_post_control *) fh;

	*i = ctrl->v4l2.output->index;

	return 0;
}

static int s3c_post_v4l2_s_output(struct file *filp, void *fh,
					unsigned int i)
{
	struct s3c_post_control *ctrl = (struct s3c_post_control *) fh;

	if (i >= S3C_POST_MAX_OUTPUT_TYPES)
		return -EINVAL;

	ctrl->v4l2.output = &s3c_post_output_types[i];

	if (s3c_post_output_types[i].type == V4L2_OUTPUT_TYPE_MEMORY)
		ctrl->out_type = PATH_OUT_DMA;
	else
		ctrl->out_type = PATH_OUT_LCDFIFO;

	return 0;
}

static int s3c_post_v4l2_enum_input(struct file *filp, void *fh,
					struct v4l2_input *i)
{
	if (i->index >= S3C_POST_MAX_INPUT_TYPES)
		return -EINVAL;

	memcpy(i, &s3c_post_input_types[i->index], sizeof(struct v4l2_input));

	return 0;
}

static int s3c_post_v4l2_enum_output(struct file *filp, void *fh,
					struct v4l2_output *o)
{
	if ((o->index) >= S3C_POST_MAX_OUTPUT_TYPES)
		return -EINVAL;

	memcpy(o, &s3c_post_output_types[o->index], sizeof(struct v4l2_output));

	return 0;
}

static int s3c_post_v4l2_reqbufs(struct file *filp, void *fh,
					struct v4l2_requestbuffers *b)
{
	if (b->memory != V4L2_MEMORY_MMAP) {
		err("V4L2_MEMORY_MMAP is only supported\n");
		return -EINVAL;
	}

	if (b->count > 4)
		b->count = 4;
	else if (b->count < 1)
		b->count = 1;

	return 0;
}

static int s3c_post_v4l2_querybuf(struct file *filp, void *fh,
					struct v4l2_buffer *b)
{
	struct s3c_post_control *ctrl = (struct s3c_post_control *) fh;
	
	if (b->type != V4L2_BUF_TYPE_VIDEO_OVERLAY && \
		b->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	if (b->memory != V4L2_MEMORY_MMAP)
		return -EINVAL;

	b->length = ctrl->out_frame.buf_size;

	/*
	 * NOTE: we use the m.offset as an index for multiple frames out.
	 * Because all frames are not contiguous, we cannot use it as
	 * original purpose.
	 * The index value used to find out which frame user wants to mmap.
	 */
	b->m.offset = b->index * PAGE_SIZE;

	return 0;
}

static int s3c_post_v4l2_qbuf(struct file *filp, void *fh,
				struct v4l2_buffer *b)
{
	return 0;
}

static int s3c_post_v4l2_dqbuf(struct file *filp, void *fh,
				struct v4l2_buffer *b)
{
	struct s3c_post_control *ctrl = (struct s3c_post_control *) fh;
	struct s3c_post_out_frame *frame = &ctrl->out_frame;

	ctrl->out_frame.cfn = s3c_post_get_frame_count(ctrl);
	b->index = (frame->cfn + 2) % frame->nr_frames;

	return 0;
}

const struct v4l2_ioctl_ops s3c_post_v4l2_ops = {
	.vidioc_querycap		= s3c_post_v4l2_querycap,
	.vidioc_enum_fmt_vid_cap	= s3c_post_v4l2_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap		= s3c_post_v4l2_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap		= s3c_post_v4l2_s_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap		= s3c_post_v4l2_try_fmt_vid_cap,
	.vidioc_g_ctrl			= s3c_post_v4l2_g_ctrl,
	.vidioc_s_ctrl			= s3c_post_v4l2_s_ctrl,
	.vidioc_streamon		= s3c_post_v4l2_streamon,
	.vidioc_streamoff		= s3c_post_v4l2_streamoff,
	.vidioc_g_input			= s3c_post_v4l2_g_input,
	.vidioc_s_input			= s3c_post_v4l2_s_input,
	.vidioc_g_output		= s3c_post_v4l2_g_output,
	.vidioc_s_output		= s3c_post_v4l2_s_output,
	.vidioc_enum_input		= s3c_post_v4l2_enum_input,
	.vidioc_enum_output		= s3c_post_v4l2_enum_output,
	.vidioc_reqbufs			= s3c_post_v4l2_reqbufs,
	.vidioc_querybuf		= s3c_post_v4l2_querybuf,
	.vidioc_qbuf			= s3c_post_v4l2_qbuf,
	.vidioc_dqbuf			= s3c_post_v4l2_dqbuf,
};
