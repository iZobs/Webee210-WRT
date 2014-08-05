/* linux/drivers/media/video/samsung/s3c_post.h
 *
 * Header file for Samsung Post Processor driver
 *
 * Jinsung Yang, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _S3C_CAMIF_H
#define _S3C_CAMIF_H

#ifdef __KERNEL__
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <plat/post.h>
#endif

/*
 * P I X E L   F O R M A T   G U I D E
 *
 * The 'x' means 'DO NOT CARE'
 * The '*' means 'POST SPECIFIC'
 * For some post formats, we couldn't find equivalent format in the V4L2 FOURCC.
 *
 * POST TYPE	PLANES	ORDER		V4L2_PIX_FMT
 * ---------------------------------------------------------
 * RGB565	x	x		V4L2_PIX_FMT_RGB565
 * RGB888	x	x		V4L2_PIX_FMT_RGB24
 * YUV420	2	LSB_CBCR	V4L2_PIX_FMT_NV12
 * YUV420	2	LSB_CRCB	V4L2_PIX_FMT_NV21
 * YUV420	2	MSB_CBCR	V4L2_PIX_FMT_NV21X*
 * YUV420	2	MSB_CRCB	V4L2_PIX_FMT_NV12X*
 * YUV420	3	x		V4L2_PIX_FMT_YUV420
 * YUV422	1	YCBYCR		V4L2_PIX_FMT_YUYV
 * YUV422	1	YCRYCB		V4L2_PIX_FMT_YVYU
 * YUV422	1	CBYCRY		V4L2_PIX_FMT_UYVY
 * YUV422	1	CRYCBY		V4L2_PIX_FMT_VYUY*
 * YUV422	2	LSB_CBCR	V4L2_PIX_FMT_NV16*
 * YUV422	2	LSB_CRCB	V4L2_PIX_FMT_NV61*
 * YUV422	2	MSB_CBCR	V4L2_PIX_FMT_NV16X*
 * YUV422	2	MSB_CRCB	V4L2_PIX_FMT_NV61X*
 * YUV422	3	x		V4L2_PIX_FMT_YUV422P
 *
*/

/*
 * C O M M O N   D E F I N I T I O N S
 *
*/
#define S3C_POST_NAME	"s3c-post"

#define info(args...)	do { printk(KERN_INFO S3C_POST_NAME ": " args); } while (0)
#define err(args...)	do { printk(KERN_ERR  S3C_POST_NAME ": " args); } while (0)

#define S3C_POST_MINOR			15
#define S3C_POST_MAX_FRAMES		4

/*
 * E N U M E R A T I O N S
 *
*/
enum s3c_post_order422_in_t {
	IN_ORDER422_CRYCBY = (0 << 4),
	IN_ORDER422_YCRYCB = (1 << 4),
	IN_ORDER422_CBYCRY = (2 << 4),
	IN_ORDER422_YCBYCR = (3 << 4),
};

enum s3c_post_order422_out_t {
	OUT_ORDER422_YCBYCR = (0 << 0),
	OUT_ORDER422_YCRYCB = (1 << 0),
	OUT_ORDER422_CBYCRY = (2 << 0),
	OUT_ORDER422_CRYCBY = (3 << 0),
};

enum s3c_post_2plane_order_t {
	LSB_CBCR = 0,
	LSB_CRCB = 1,
	MSB_CRCB = 2,
	MSB_CBCR = 3,
};

enum s3c_post_scan_t {
	SCAN_TYPE_PROGRESSIVE	= 0,
	SCAN_TYPE_INTERLACE	= 1,
};

enum s3c_post_format_t {
	FORMAT_RGB565,
	FORMAT_RGB666,
	FORMAT_RGB888,
	FORMAT_YCBCR420,
	FORMAT_YCBCR422,
};

enum s3c_post_path_out_t {
	PATH_OUT_DMA,
	PATH_OUT_LCDFIFO,
};

/*
 * P O S T   S T R U C T U R E S
 *
*/

/*
 * struct s3c_post_frame_addr
 * @phys_rgb:	physical start address of rgb buffer
 * @phys_y:	physical start address of y buffer
 * @phys_cb:	physical start address of u buffer
 * @phys_cr:	physical start address of v buffer
 * @virt_y:	virtual start address of y buffer
 * @virt_rgb:	virtual start address of rgb buffer
 * @virt_cb:	virtual start address of u buffer
 * @virt_cr:	virtual start address of v buffer
*/
struct s3c_post_frame_addr {
	union {
		dma_addr_t	phys_rgb;
		dma_addr_t	phys_y;		
	};

	dma_addr_t		phys_cb;
	dma_addr_t		phys_cr;

	union {
		u8		*virt_rgb;
		u8		*virt_y;
	};

	u8			*virt_cb;
	u8			*virt_cr;
};

/*
 * struct s3c_post_dma_offset
 * @y_h:	y value horizontal offset
 * @y_v:	y value vertical offset
 * @cb_h:	cb value horizontal offset
 * @cb_v:	cb value vertical offset
 * @cr_h:	cr value horizontal offset
 * @cr_v:	cr value vertical offset
 *
*/
struct s3c_post_dma_offset {
	int	y_h;
	int	y_v;
	int	cb_h;
	int	cb_v;
	int	cr_h;
	int	cr_v;
};

/*
 * struct s3c_post_scaler
 * @hfactor:		horizontal shift factor to scale up/down
 * @vfactor:		vertical shift factor to scale up/down
 * @pre_hratio:		horizontal ratio for pre-scaler
 * @pre_vratio:		vertical ratio for pre-scaler
 * @pre_dst_width:	destination width for pre-scaler
 * @pre_dst_height:	destination height for pre-scaler
 * @scaleup_h:		1 if we have to scale up for the horizontal
 * @scaleup_v:		1 if we have to scale up for the vertical
 * @main_hratio:	horizontal ratio for main scaler
 * @main_vratio:	vertical ratio for main scaler
 * @real_width:		src_width - offset
 * @real_height:	src_height - offset
 * @line_length:	line buffer length from platform_data
*/
struct s3c_post_scaler {
	u32		hfactor;
	u32		vfactor;
	u32		pre_hratio;
	u32		pre_vratio;
	u32		pre_dst_width;
	u32		pre_dst_height;
	u32		scaleup_h;
	u32		scaleup_v;
	u32		main_hratio;
	u32		main_vratio;
	u32		real_width;
	u32		real_height;
	u32		line_length;
};

/*
 * struct s3c_post_in_frame: abstraction for frame data
 * @addr:		address information of frame data
 * @width:		width
 * @height:		height
 * @offset:		dma offset
 * @format:		pixel format
 * @planes:		YCBCR planes (1, 2 or 3)
 * @order_1p		1plane YCBCR order
 * @order_2p:		2plane YCBCR order
*/
struct s3c_post_in_frame {
	u32				buf_size;
	struct s3c_post_frame_addr	addr;
	int				width;
	int				height;
	struct s3c_post_dma_offset	offset;
	enum s3c_post_format_t		format;
	int				planes;
	enum s3c_post_order422_in_t	order_1p;
	enum s3c_post_2plane_order_t	order_2p;
};

/*
 * struct s3c_post_out_frame: abstraction for frame data
 * @cfn:		current frame number
 * @buf_size:		1 buffer size
 * @addr[]:		address information of frames
 * @nr_frams:		how many output frames used
 * @width:		width
 * @height:		height
 * @offset:		offset for output dma
 * @format:		pixel format
 * @planes:		YCBCR planes (1, 2 or 3)
 * @order_1p		1plane YCBCR order
 * @order_2p:		2plane YCBCR order
 * @scan:		output scan method (progressive, interlace)
*/
struct s3c_post_out_frame {
	int				cfn;
	u32				buf_size;
	struct s3c_post_frame_addr	addr[S3C_POST_MAX_FRAMES];
	int				nr_frames;
	int				width;
	int				height;
	struct s3c_post_dma_offset	offset;
	enum s3c_post_format_t		format;
	int				planes;
	enum s3c_post_order422_out_t	order_1p;
	enum s3c_post_2plane_order_t	order_2p;
	enum s3c_post_scan_t		scan;
};

/*
 * struct s3c_post_v4l2
*/
struct s3c_post_v4l2 {
	struct v4l2_fmtdesc	*fmtdesc;
	struct v4l2_framebuffer	frmbuf;
	struct v4l2_input	*input;
	struct v4l2_output	*output;
	struct v4l2_rect	crop_bounds;
	struct v4l2_rect	crop_defrect;
	struct v4l2_rect	crop_current;
};

/*
 * struct s3c_post_control: abstraction for POST controller
 * @id:		id number (= minor number)
 * @name:	name for video_device
 * @flag:	status, usage, irq flag (S, U, I flag)
 * @lock:	mutex lock
 * @waitq:	waitqueue
 * @pdata:	platform data
 * @clock:	post clock
 * @regs:	virtual address of SFR
 * @in_use:	1 when resource is occupied
 * @irq:	irq number
 * @vd:		video_device
 * @v4l2:	v4l2 info
 * @scaler:	scaler related information
 * @in_frame:	frame structure pointer if input is dma else null
 * @out_type:	type of output
 * @out_frame:	frame structure pointer if output is dma
 *
 * @open_lcdfifo:	function pointer to open lcd fifo path (display driver)
 * @close_lcdfifo:	function pointer to close fifo path (display driver)
*/
struct s3c_post_control {
	/* general */
	int				id;
	char				name[16];
	u32				flag;
	struct mutex			lock;
	wait_queue_head_t		waitq;
	struct device			*dev;
	struct clk			*clock;	
	void __iomem			*regs;
	atomic_t			in_use;
	int				irq;
	struct video_device		*vd;
	struct s3c_post_v4l2		v4l2;
	struct s3c_post_scaler		scaler;

	struct s3c_post_in_frame	in_frame;

	/* output */
	enum s3c_post_path_out_t	out_type;
	struct s3c_post_out_frame	out_frame;

	/* functions */
	void (*open_lcdfifo)(int win, int in_yuv, int sel);
	void (*close_lcdfifo)(int win);
};

/*
 * struct s3c_post_config
*/
struct s3c_post_config {
	struct s3c_post_control	ctrl;
	dma_addr_t		dma_start;
	dma_addr_t		dma_current;
	u32			dma_total;
};


/*
 * V 4 L 2   F I M C   E X T E N S I O N S
 *
*/
#define V4L2_INPUT_TYPE_MEMORY		10
#define V4L2_OUTPUT_TYPE_MEMORY		20
#define V4L2_OUTPUT_TYPE_LCDFIFO	21

#define FORMAT_FLAGS_PACKED		1
#define FORMAT_FLAGS_PLANAR		2

#define V4L2_FMT_IN			0
#define V4L2_FMT_OUT			1

/* FOURCC for POST specific */
#define V4L2_PIX_FMT_NV12X		v4l2_fourcc('N', '1', '2', 'X')
#define V4L2_PIX_FMT_NV21X		v4l2_fourcc('N', '2', '1', 'X')
#define V4L2_PIX_FMT_VYUY		v4l2_fourcc('V', 'Y', 'U', 'Y')
#define V4L2_PIX_FMT_NV16		v4l2_fourcc('N', 'V', '1', '6')
#define V4L2_PIX_FMT_NV61		v4l2_fourcc('N', 'V', '6', '1')
#define V4L2_PIX_FMT_NV16X		v4l2_fourcc('N', '1', '6', 'X')
#define V4L2_PIX_FMT_NV61X		v4l2_fourcc('N', '6', '1', 'X')

/* CID extensions */
#define V4L2_CID_NR_FRAMES		(V4L2_CID_PRIVATE_BASE + 1)
#define V4L2_CID_RESET			(V4L2_CID_PRIVATE_BASE + 2)
#define V4L2_CID_OUTPUT_ADDR		(V4L2_CID_PRIVATE_BASE + 10)
#define V4L2_CID_INPUT_ADDR		(V4L2_CID_PRIVATE_BASE + 20)
#define V4L2_CID_INPUT_ADDR_RGB		(V4L2_CID_PRIVATE_BASE + 21)
#define V4L2_CID_INPUT_ADDR_Y		(V4L2_CID_PRIVATE_BASE + 22)
#define V4L2_CID_INPUT_ADDR_CB		(V4L2_CID_PRIVATE_BASE + 23)
#define V4L2_CID_INPUT_ADDR_CBCR	(V4L2_CID_PRIVATE_BASE + 24)
#define V4L2_CID_INPUT_ADDR_CR		(V4L2_CID_PRIVATE_BASE + 25)

/*
 * E X T E R N S
 *
*/
extern struct s3c_post_config s3c_post;
extern const struct v4l2_ioctl_ops s3c_post_v4l2_ops;
extern struct video_device s3c_post_video_device;

extern struct s3c_platform_post *to_post_plat(struct device *dev);
extern int s3c_post_alloc_input_memory(struct s3c_post_in_frame *info, dma_addr_t addr);
extern int s3c_post_alloc_output_memory(struct s3c_post_out_frame *info);
extern int s3c_post_alloc_y_memory(struct s3c_post_in_frame *info, dma_addr_t addr);
extern int s3c_post_alloc_cb_memory(struct s3c_post_in_frame *info, dma_addr_t addr);
extern int s3c_post_alloc_cr_memory(struct s3c_post_in_frame *info, dma_addr_t addr);
extern void s3c_post_free_output_memory(struct s3c_post_out_frame *info);
extern int s3c_post_set_input_frame(struct s3c_post_control *ctrl, struct v4l2_pix_format *fmt);
extern int s3c_post_set_output_frame(struct s3c_post_control *ctrl, struct v4l2_pix_format *fmt);
extern int s3c_post_frame_handler(struct s3c_post_control *ctrl);
extern u8 *s3c_post_get_current_frame(struct s3c_post_control *ctrl);
extern void s3c_post_set_nr_frames(struct s3c_post_control *ctrl, int nr);
extern int s3c_post_set_scaler_info(struct s3c_post_control *ctrl);
extern void s3c_post_start_dma(struct s3c_post_control *ctrl);
extern void s3c_post_stop_dma(struct s3c_post_control *ctrl);
extern void s3c_post_restart_dma(struct s3c_post_control *ctrl);
extern void s3c_post_clear_irq(struct s3c_post_control *ctrl);
extern int s3c_post_check_fifo(struct s3c_post_control *ctrl);
extern void s3c_post_reset(struct s3c_post_control *ctrl);
extern void s3c_post_set_target_format(struct s3c_post_control *ctrl);
extern void s3c_post_set_output_dma(struct s3c_post_control *ctrl);
extern void s3c_post_set_prescaler(struct s3c_post_control *ctrl);
extern void s3c_post_set_scaler(struct s3c_post_control *ctrl);
extern void s3c_post_start_scaler(struct s3c_post_control *ctrl);
extern void s3c_post_stop_scaler(struct s3c_post_control *ctrl);
extern void s3c_post_enable_capture(struct s3c_post_control *ctrl);
extern void s3c_post_disable_capture(struct s3c_post_control *ctrl);
extern void s3c_post_set_input_dma(struct s3c_post_control *ctrl);
extern void s3c_post_start_input_dma(struct s3c_post_control *ctrl);
extern void s3c_post_stop_input_dma(struct s3c_post_control *ctrl);
extern void s3c_post_set_input_path(struct s3c_post_control *ctrl);
extern void s3c_post_set_output_path(struct s3c_post_control *ctrl);
extern void s3c_post_set_input_address(struct s3c_post_control *ctrl);
extern void s3c_post_set_output_address(struct s3c_post_control *ctrl);
extern int s3c_post_get_frame_count(struct s3c_post_control *ctrl);
extern void s3c_post_wait_frame_end(struct s3c_post_control *ctrl);

/* FIMD externs */
extern void s3cfb_enable_local(int win, int in_yuv, int sel);
extern void s3cfb_enable_dma(int win);

#endif /* _S3C_CAMIF_H */
