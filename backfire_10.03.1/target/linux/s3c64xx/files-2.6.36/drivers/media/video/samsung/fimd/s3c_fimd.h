/* linux/drivers/media/video/samsung/s3c_fimc.h
 *
 * Header file for Samsung Camera Interface (FIMC) driver
 *
 * Jinsung Yang, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _S3C_FIMD_H
#define _S3C_FIMD_H

#ifdef __KERNEL__
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <plat/fimd.h>
#endif

/*
 * C O M M O N   D E F I N I T I O N S
 *
*/
#define S3C_FIMD_NAME	"s3c-fimd"

#define info(args...)	do { printk(KERN_INFO S3C_FIMD_NAME ": " args); } while (0)
#define err(args...)	do { printk(KERN_ERR  S3C_FIMD_NAME ": " args); } while (0)


/*
 * E N U M E R A T I O N S
 *
*/
enum s3c_fimd_data_path_t {
	DATA_PATH_DMA,
	DATA_PATH_FIFO,
};

enum s3c_fimd_alpha_t {
	PLANE_BLENDING,
	PIXEL_BLENDING,
};

enum s3c_fimd_chroma_dir_t {
	CHROMA_FG,
	CHROMA_BG,
};

enum s3c_fimd_output_t {
	OUTPUT_RGB,
	OUTPUT_ITU,
	OUTPUT_I80LDI0,
	OUTPUT_I80LDI1,
	OUTPUT_TV,
	OUTPUT_TV_RGB,
	OUTPUT_TV_I80LDI0,
	OUTPUT_TV_I80LDI1,
};

enum s3c_fimd_rgb_mode_t {
	MDOE_RGB_P = 0,
	MODE_BGR_P = 1,
	MODE_RGB_S = 2,
	MODE_BGR_S = 3,
};


/*
 * F I M D   S T R U C T U R E S
 *
*/

/*
 * struct s3c_fimd_buffer
 * @width:	horizontal resolution
 * @height:	vertical resolution
 * @ofs_x:	left horizontal offset
 * @ofs_y:	top vertical offset
 * @auto_sel:	if auto buffer change enabled by hardware
 * @vs:		if virtual screen enabled
 * @current:	current activated buffer
 * @paddr:	physical address of frame buffer
*/
struct s3c_fimd_buffer {
	int	width;
	int	height;
	int	ofs_x;
	int	ofs_y;
	int	auto_sel;
	int	vs;
	int	current;
	u8	**paddr;
};

/*
 * struct s3c_fimd_alpha
 * @mode:		blending method (plane/pixel)
 * @channel:		alpha channel (0/1)
 * @multiple:		if multiple alpha blending enabled
 * @alpha_value:	alpha value
*/
struct s3c_fimd_alpha {
	enum 	s3c_fimd_alpha_t mode;
	int	channel;
	int	multiple;
	int	alpha_value;
};

/*
 * struct s3c_fimd_chroma
 * @enabled:		if chroma key function enabled
 * @blended:		if chroma key alpha blending enabled
 * @dir:		chroma key direction (fore/back)
 * @chroma_key:		chroma value to be applied
 * @alpha_value:	alpha value for chroma
 *
*/
struct s3c_fimd_chroma {
	int 	enabled;
	int 	blended;
	u32	comp_key;
	u32	chroma_key;
	u32	alpha_value;
	enum 	s3c_fimd_chroma_dir_t dir;
};

/*
 * struct s3c_fimd_lcd_polarity
 * @vclk:	if VCLK polarity is inversed
 * @hsync:	if HSYNC polarity is inversed
 * @vsync:	if VSYNC polarity is inversed
 * @vden:	if VDEN polarity is inversed
*/
struct s3c_fimd_lcd_polarity {
	int	vclk;
	int	hsync;
	int	vsync;
	int	vden;
};

/*
 * struct s3c_fimd_lcd_timing
 * @h_fp:	horizontal front porch
 * @h_fpe:	horizontal front porch for even field
 * @h_bp:	horizontal back porch
 * @h_sw:	horizontal sync width
 * @v_fp:	vertical front porch
 * @v_fpe:	vertical front porch for even field
 * @v_bp:	vertical back porch
 * @v_bpe:	vertical back porch for even field
*/
struct s3c_fimd_lcd_timing {
	int	h_fp;
	int	h_fpe;
	int	h_bp;
	int	h_sw;
	int	v_fp;
	int	v_fpe;
	int	v_bp;
	int	v_bpe;
};

/*
 * struct s3c_fimd_lcd
 * @width:		horizontal resolution
 * @height:		vertical resolution
 * @bpp:		bits per pixel
 * @freq:		vframe frequency
 * @initialized:	if initialized
 * @timing:		timing values
 * @polarity:		polarity settings
 * @init_ldi:		pointer to LDI init function
 *
*/
struct s3c_fimd_lcd {
	int	width;
	int	height;
	int	bpp;
	int	freq;
	int	initialized;
	struct 	s3c_fimd_lcd_timing timing;
	struct 	s3c_fimd_lcd_polarity polarity;

	void 	(*init_ldi)(void);
};

/*
 * struct s3c_fimd_window
 * @id:			window id
 * @enabled:		if enabled
 * @shadow_lock:	current lock status for updating next frame
 * @path:		data path (dma/fifo)
 * @bit_swap:		if bit swap enabled
 * @byte_swap:		if byte swap enabled
 * @halfword_swap:	if halfword swap enabled
 * @word_swap:		if word swap enabled
 * @dma_burst:		dma burst length (4/8/16)
 * @bpp:		bits per pixel
 * @unpacked:		if unpacked format is
 * @buffer:		frame buffer structure
 * @alpha:		alpha blending structure
 * @chroma:		chroma key structure
*/
struct s3c_fimd_window {
	int	id;
	int	enabled;
	int	shadow_lock;
	enum 	s3c_fimd_data_path_t path;
	int	bit_swap;
	int	byte_swap;
	int	halfword_swap;
	int	word_swap;
	int	dma_burst;
	int	bpp;
	int	unpacked;
	struct	s3c_fimd_buffer buffer;
	struct	s3c_fimd_alpha alpha;
	struct	s3c_fimd_chroma chroma;
};

/*
 * struct s3c_fimd_v4l2
*/
struct s3c_fimd_v4l2 {
	struct v4l2_fmtdesc	*fmtdesc;
	struct v4l2_framebuffer	frmbuf;
	struct v4l2_input	*input;
	struct v4l2_output	*output;
	struct v4l2_rect	crop_bounds;
	struct v4l2_rect	crop_defrect;
	struct v4l2_rect	crop_current;
};

/*
 * struct s3c_fimd_global
 *
 * @enabled:		if signal output enabled
 * @dsi:		if mipi-dsim enabled
 * @interlace:		if interlace format is used
 * @output:		output path (RGB/I80/Etc)
 * @rgb_serial:		if RGB signal output is serialized
 * @vclk_freerun:	if vclk is free-run mode
 * @alps_enabled:	if ALPS enabled (over FIMD 6.0)
 * @gamma_enabled:	if gamma control enabled (over FIMD 6.0)
 * @gain_enabled:	if gain control enabled (over FIMD 6.0)
 * @win:		pointer to window structure
 * @lcd:		pointer to lcd structure
*/
struct s3c_fimd_global {
	/* general */
	void __iomem			*regs;
	struct mutex			lock;
	struct device			*dev;
	struct clk			*clock;
	int				irq;
	struct video_device		*vd;
	struct s3c_fimd_v4l2		v4l2;

	/* fimd */
	int				enabled;
	int				dsi;
	int				interlace;
	enum s3c_fimd_output_t 		output;
	enum s3c_fimd_rgb_mode_t	rgb_mode;
	int				vclk_freerun;
	int				alps_enabled;
	int				gamma_enabled;
	int				gain_enabled;
	struct s3c_fimd_window 		**win;
	struct s3c_fimd_lcd 		*lcd;
};


/*
 * E X T E R N S
 *
*/

#endif /* _S3C_FIMD_H */
