/* linux/drivers/media/video/samsung/tv20/s5pv210/vprocessor_s5pv210.c
 *
 * Video Processor raw ftn  file for Samsung TVOut driver
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
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <plat/clock.h>

#include "tv_out_s5pv210.h"

#include <plat/regs-vprocessor.h>
#include "vp_coeff_s5pv210.h"

#ifdef S5P_VP_DEBUG
#define VPPRINTK(fmt, args...)\
	pr_debug("\t\t[VP] %s: " fmt, __func__ , ## args)
#else
#define VPPRINTK(fmt, args...)
#endif

static struct resource *g_vp_mem;
static unsigned int     g_vp_contrast_brightness;
static void __iomem    *g_vp_base;

/*
 * set
 *  - set functions are only called under running video processor
 *  - after running set functions, it is need to run s5p_vp_update() function
 *    for update shadow registers
 */
void s5p_vp_set_field_id(enum s5p_vp_field mode)
{
	writel((mode == VPROC_TOP_FIELD) ?
		g_vp_base + S5P_VP_FIELD_ID_TOP :
		g_vp_base + S5P_VP_FIELD_ID_BOTTOM,
		g_vp_base + S5P_VP_FIELD_ID);

	VPPRINTK("mode(%d), 0x%08x\n", mode, readl(g_vp_base + S5P_VP_FIELD_ID));
}

enum s5p_tv_vp_err s5p_vp_set_top_field_address(u32 top_y_addr,
						u32 top_c_addr)
{
	if (VP_PTR_ILLEGAL(top_y_addr) || VP_PTR_ILLEGAL(top_c_addr)) {
		pr_err(" address is not double word align = 0x%x, 0x%x\n",
			top_y_addr, top_c_addr);
		return S5P_TV_VP_ERR_BASE_ADDRESS_MUST_DOUBLE_WORD_ALIGN;
	}

	writel(top_y_addr, g_vp_base + S5P_VP_TOP_Y_PTR);
	writel(top_c_addr, g_vp_base + S5P_VP_TOP_C_PTR);

	VPPRINTK("top_y_addr(0x%x), top_c_addr(0x%x), \
		S5P_VP_TOP_Y_PTR(0x%x), S5P_VP_TOP_C_PTR(0x%x)\n",
		top_y_addr, top_c_addr,
		readl(g_vp_base + S5P_VP_TOP_Y_PTR),
		readl(g_vp_base + S5P_VP_TOP_C_PTR));

	return VPROC_NO_ERROR;
}

enum s5p_tv_vp_err s5p_vp_set_bottom_field_address(u32 bottom_y_addr,
						u32 bottom_c_addr)
{
	if (VP_PTR_ILLEGAL(bottom_y_addr) || VP_PTR_ILLEGAL(bottom_c_addr)) {
		pr_err(" address is not double word align = 0x%x, 0x%x\n",
			bottom_y_addr, bottom_c_addr);
		return S5P_TV_VP_ERR_BASE_ADDRESS_MUST_DOUBLE_WORD_ALIGN;
	}

	writel(bottom_y_addr, g_vp_base + S5P_VP_BOT_Y_PTR);
	writel(bottom_c_addr, g_vp_base + S5P_VP_BOT_C_PTR);

	VPPRINTK("bottom_y_addr(0x%x), bottom_c_addr(0x%x)\n",
		bottom_y_addr, bottom_c_addr);

	VPPRINTK("S5P_VP_BOT_Y_PTR(0x%x), S5P_VP_BOT_C_PTR(0x%x)\n",
		readl(g_vp_base + S5P_VP_BOT_Y_PTR),
		readl(g_vp_base + S5P_VP_BOT_C_PTR));

	return VPROC_NO_ERROR;
}

enum s5p_tv_vp_err s5p_vp_set_img_size(u32 img_width, u32 img_height)
{
	if (VP_IMG_SIZE_ILLEGAL(img_width) || VP_IMG_SIZE_ILLEGAL(img_height)) {
		pr_err(" image full size is not double word align = %d, %d\n",
			img_width, img_height);
		return S5P_TV_VP_ERR_BASE_ADDRESS_MUST_DOUBLE_WORD_ALIGN;
	}

	writel(VP_IMG_HSIZE(img_width) | VP_IMG_VSIZE(img_height),
		g_vp_base + S5P_VP_IMG_SIZE_Y);

	writel(VP_IMG_HSIZE(img_width) | VP_IMG_VSIZE(img_height / 2),
		g_vp_base + S5P_VP_IMG_SIZE_C);

	VPPRINTK("img_width(%d), img_height(%d), \
		S5P_VP_IMG_SIZE_Y(0x%x), S5P_VP_IMG_SIZE_C(0x%x)\n",
		img_width, img_height,
		readl(g_vp_base + S5P_VP_IMG_SIZE_Y),
		readl(g_vp_base + S5P_VP_IMG_SIZE_C));

	return VPROC_NO_ERROR;
}

void s5p_vp_set_src_position(u32 src_off_x,
				u32 src_x_fract_step,
				u32 src_off_y)
{
	writel(VP_SRC_H_POSITION(src_off_x) | VP_SRC_X_FRACT_STEP(src_x_fract_step),
					g_vp_base + S5P_VP_SRC_H_POSITION);
	writel(VP_SRC_V_POSITION(src_off_y), g_vp_base + S5P_VP_SRC_V_POSITION);

	VPPRINTK("src_off_x(%d), src_x_fract_step(%d), src_off_y(%d), \
		S5P_VP_SRC_H_POSITION(0x%x), \
		S5P_VP_SRC_V_POSITION(0x%x)\n",
		src_off_x, src_x_fract_step, src_off_y,
		readl(g_vp_base + S5P_VP_SRC_H_POSITION),
		readl(g_vp_base + S5P_VP_SRC_V_POSITION));
}

void s5p_vp_set_dest_position(u32 dst_off_x,
				u32 dst_off_y)
{
	writel(VP_DST_H_POSITION(dst_off_x), g_vp_base + S5P_VP_DST_H_POSITION);
	writel(VP_DST_V_POSITION(dst_off_y), g_vp_base + S5P_VP_DST_V_POSITION);

	VPPRINTK("dst_off_x(%d), dst_off_y(%d), \
		S5P_VP_DST_H_POSITION(0x%x), \
		S5P_VP_DST_V_POSITION(0x%x)\n",
		dst_off_x, dst_off_y,
		readl(g_vp_base + S5P_VP_DST_H_POSITION),
		readl(g_vp_base + S5P_VP_DST_V_POSITION));
}

void s5p_vp_set_src_dest_size(u32 src_width,
				u32 src_height,
				u32 dst_width,
				u32 dst_height,
				bool ipc_2d)
{
	u32 h_ratio = (src_width << 16) / dst_width;
	u32 v_ratio = (ipc_2d) ?
			((src_height << 17) / dst_height) :
			((src_height << 16) / dst_height);

	writel(VP_SRC_WIDTH(src_width),   g_vp_base + S5P_VP_SRC_WIDTH);
	writel(VP_SRC_HEIGHT(src_height), g_vp_base + S5P_VP_SRC_HEIGHT);
	writel(VP_DST_WIDTH(dst_width),   g_vp_base + S5P_VP_DST_WIDTH);
	writel(VP_DST_HEIGHT(dst_height), g_vp_base + S5P_VP_DST_HEIGHT) ;
	writel(VP_H_RATIO(h_ratio),       g_vp_base + S5P_VP_H_RATIO);
	writel(VP_V_RATIO(v_ratio),       g_vp_base + S5P_VP_V_RATIO);

	writel((ipc_2d) ?
		(readl(g_vp_base + S5P_VP_MODE) | VP_2D_IPC_ON) :
		(readl(g_vp_base + S5P_VP_MODE) & ~VP_2D_IPC_ON),
		g_vp_base + S5P_VP_MODE);

	VPPRINTK("src_width(%d), src_height(%d), dst_width(%d), dst_height(%d), \
		S5P_VP_SRC_WIDTH(%d), S5P_VP_SRC_HEIGHT(%d), \
		S5P_VP_DST_WIDTH(%d), S5P_VP_DST_HEIGHT(%d), \
		S5P_VP_H_RATIO(0x%x), S5P_VP_V_RATIO(0x%x)\n",
		src_width, src_height, dst_width, dst_height,
		readl(g_vp_base + S5P_VP_SRC_WIDTH),
		readl(g_vp_base + S5P_VP_SRC_HEIGHT),
		readl(g_vp_base + S5P_VP_DST_WIDTH),
		readl(g_vp_base + S5P_VP_DST_HEIGHT),
		readl(g_vp_base + S5P_VP_H_RATIO),
		readl(g_vp_base + S5P_VP_V_RATIO));
}

enum s5p_tv_vp_err s5p_vp_set_poly_filter_coef(
	enum s5p_vp_poly_coeff poly_coeff,
	signed char ch0,
	signed char ch1,
	signed char ch2,
	signed char ch3)
{
	if (poly_coeff > VPROC_POLY4_C1_HH || poly_coeff < VPROC_POLY8_Y0_LL ||
		(poly_coeff > VPROC_POLY8_Y3_HH &&
		poly_coeff < VPROC_POLY4_Y0_LL)) {

		pr_err("invaild poly_coeff parameter poly_coeff(%d)\n", poly_coeff);
		return S5P_TV_VP_ERR_INVALID_PARAM;
	}

	writel((((0xff&ch0) << 24) | ((0xff&ch1) << 16) | ((0xff&ch2) << 8) | (0xff&ch3)),
		g_vp_base + S5P_VP_POLY8_Y0_LL + (poly_coeff * 4));

	VPPRINTK("poly_coeff(%d), ch0(%d), ch1(%d), ch2(%d), ch3(%d), \
		0x%08x, 0x%08x\n",
		poly_coeff, ch0, ch1, ch2, ch3,
		readl(g_vp_base + S5P_VP_POLY8_Y0_LL + poly_coeff * 4),
		g_vp_base + S5P_VP_POLY8_Y0_LL + poly_coeff * 4);

	return VPROC_NO_ERROR;
}

void s5p_vp_set_poly_filter_coef_default(u32 h_ratio, u32 v_ratio)
{
	enum s5p_tv_vp_filter_h_pp e_h_filter;
	enum s5p_tv_vp_filter_v_pp e_v_filter;
	u8 *poly_flt_coeff;
	int i, j;

	/*
	 * For the real interlace mode, the vertical ratio should be
	 * used after divided by 2. Because in the interlace mode, all
	 * the VP output is used for SDOUT display and it should be the
	 * same as one field of the progressive mode. Therefore the same
	 * filter coefficients should be used for the same the final
	 * output video. When half of the interlace V_RATIO is same as
	 * the progressive V_RATIO, the final output video scale is same.
	 */

	/* Horizontal Y 8tap */
	/* Horizontal C 4tap */
	if (h_ratio <= (0x1 << 16))		/* 720->720 or zoom in */
		e_h_filter = VPROC_PP_H_NORMAL;
	else if (h_ratio <= (0x9 << 13))	/* 720->640 */
		e_h_filter = VPROC_PP_H_8_9;
	else if (h_ratio <= (0x1 << 17))	/* 2->1 */
		e_h_filter = VPROC_PP_H_1_2;
	else if (h_ratio <= (0x3 << 16))	/* 2->1 */
		e_h_filter = VPROC_PP_H_1_3;
	else
		e_h_filter = VPROC_PP_H_1_4;	/* 4->1 */

	/* Vertical Y 4tap */
	if (v_ratio <= (0x1 << 16))		/* 720->720 or zoom in*/
		e_v_filter = VPROC_PP_V_NORMAL;
	else if (v_ratio <= (0x5 << 14))	/* 4->3*/
		e_v_filter = VPROC_PP_V_3_4;
	else if (v_ratio <= (0x3 << 15))	/*6->5*/
		e_v_filter = VPROC_PP_V_5_6;
	else if (v_ratio <= (0x1 << 17))	/* 2->1*/
		e_v_filter = VPROC_PP_V_1_2;
	else if (v_ratio <= (0x3 << 16))	/* 3->1*/
		e_v_filter = VPROC_PP_V_1_3;
	else
		e_v_filter = VPROC_PP_V_1_4;

	poly_flt_coeff = (u8 *)(g_s_vp8tap_coef_y_h + e_h_filter * 16 * 8);

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			s5p_vp_set_poly_filter_coef(
				VPROC_POLY8_Y0_LL + (i*4) + j,
				*(poly_flt_coeff + 4*j*8 + (7 - i)),
				*(poly_flt_coeff + (4*j + 1)*8 + (7 - i)),
				*(poly_flt_coeff + (4*j + 2)*8 + (7 - i)),
				*(poly_flt_coeff + (4*j + 3)*8 + (7 - i)));
		}
	}

	poly_flt_coeff = (u8 *)(g_s_vp4tap_coef_c_h + e_h_filter * 16 * 4);

	for (i = 0; i < 2; i++) {
		for (j = 0; j < 4; j++) {
			s5p_vp_set_poly_filter_coef(
				VPROC_POLY4_C0_LL + (i*4) + j,
				*(poly_flt_coeff + 4*j*4 + (3 - i)),
				*(poly_flt_coeff + (4*j + 1)*4 + (3 - i)),
				*(poly_flt_coeff + (4*j + 2)*4 + (3 - i)),
				*(poly_flt_coeff + (4*j + 3)*4 + (3 - i)));
		}
	}

	poly_flt_coeff = (u8 *)(g_s_vp4tap_coef_y_v + e_v_filter * 16 * 4);

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			s5p_vp_set_poly_filter_coef(
				VPROC_POLY4_Y0_LL + (i*4) + j,
				*(poly_flt_coeff + 4*j*4 + (3 - i)),
				*(poly_flt_coeff + (4*j + 1)*4 + (3 - i)),
				*(poly_flt_coeff + (4*j + 2)*4 + (3 - i)),
				*(poly_flt_coeff + (4*j + 3)*4 + (3 - i)));
		}
	}

	VPPRINTK("h_ratio(%d), v_ratio(%d), e_h_filter(%d), e_v_filter(%d)\n",
		h_ratio, v_ratio, e_h_filter, e_v_filter);
}

void s5p_vp_set_src_dest_size_with_default_poly_filter_coef(u32 src_width,
						u32 src_height,
						u32 dst_width,
						u32 dst_height,
						bool ipc_2d)
{
	u32 h_ratio = (src_width << 16) / dst_width;
	u32 v_ratio = (ipc_2d) ? ((src_height << 17) / dst_height) :
				  ((src_height << 16) / dst_height);

	s5p_vp_set_src_dest_size(src_width, src_height,
				dst_width, dst_height,
				ipc_2d);

	s5p_vp_set_poly_filter_coef_default(h_ratio, v_ratio);
}

enum s5p_tv_vp_err s5p_vp_set_brightness_contrast_control(
	enum s5p_vp_line_eq eq_num,
	u32 intc,
	u32 slope)
{
	if (eq_num > VProc_LINE_EQ_7 || eq_num < VProc_LINE_EQ_0) {
		pr_err("invaild eq_num parameter(%d)\n", eq_num);
		return S5P_TV_VP_ERR_INVALID_PARAM;
	}

	writel(VP_LINE_INTC(intc) | VP_LINE_SLOPE(slope),
		g_vp_base + S5P_PP_LINE_EQ0 + eq_num*4);

	VPPRINTK("eq_num(%d), intc(%d), slope(%d), 0x%08x, 0x%08x\n",
		eq_num, intc, slope,
		readl(g_vp_base + S5P_PP_LINE_EQ0 + eq_num * 4),
		g_vp_base + S5P_PP_LINE_EQ0 + eq_num * 4);

	return VPROC_NO_ERROR;
}

void s5p_vp_set_brightness(bool brightness)
{
	unsigned short i;

	g_vp_contrast_brightness =
		VP_LINE_INTC_CLEAR(g_vp_contrast_brightness) |
		VP_LINE_INTC(brightness);

	for (i = 0; i < 8; i++)
		writel(g_vp_contrast_brightness,
			g_vp_base + S5P_PP_LINE_EQ0 + i * 4);

	VPPRINTK("brightness(%d), g_vp_contrast_brightness(%d)\n",
		brightness, g_vp_contrast_brightness);
}

void s5p_vp_set_contrast(u8 contrast)
{
	unsigned short i;

	g_vp_contrast_brightness =
		VP_LINE_SLOPE_CLEAR(g_vp_contrast_brightness) |
		VP_LINE_SLOPE(contrast);

	for (i = 0; i < 8; i++)
		writel(g_vp_contrast_brightness,
			g_vp_base + S5P_PP_LINE_EQ0 + i * 4);

	VPPRINTK("contrast(%d), g_vp_contrast_brightness(%d)\n",
		contrast, g_vp_contrast_brightness);
}

enum s5p_tv_vp_err s5p_vp_update(void)
{
	writel(readl(g_vp_base + S5P_VP_SHADOW_UPDATE) |
		S5P_VP_SHADOW_UPDATE_ENABLE,
		g_vp_base + S5P_VP_SHADOW_UPDATE);

	return VPROC_NO_ERROR;
}

/*
* get  - get info
*/
enum s5p_vp_field s5p_vp_get_field_id(void)
{
	return (readl(g_vp_base + S5P_VP_FIELD_ID) == S5P_VP_FIELD_ID_BOTTOM) ?
		VPROC_BOTTOM_FIELD : VPROC_TOP_FIELD;
}

/*
* etc
*/
unsigned short s5p_vp_get_update_status(void)
{
	return readl(g_vp_base + S5P_VP_SHADOW_UPDATE)
		& S5P_VP_SHADOW_UPDATE_ENABLE;
}

void s5p_vp_init_field_id(enum s5p_vp_field mode)
{
	s5p_vp_set_field_id(mode);
}

void s5p_vp_init_op_mode(bool line_skip,
			enum s5p_vp_mem_mode mem_mode,
			enum s5p_vp_chroma_expansion chroma_exp,
			enum s5p_vp_filed_id_toggle toggle_id)
{
	u32 temp_reg;

	temp_reg = (line_skip) ? VP_LINE_SKIP_ON : VP_LINE_SKIP_OFF;
	temp_reg |= (mem_mode == VPROC_2D_TILE_MODE) ?
		    VP_MEM_2D_MODE : VP_MEM_LINEAR_MODE;
	temp_reg |= (chroma_exp == VPROC_USING_C_TOP_BOTTOM) ?
		    VP_CHROMA_USE_TOP_BOTTOM : VP_CHROMA_USE_TOP;
	temp_reg |= (toggle_id == S5P_TV_VP_FILED_ID_TOGGLE_VSYNC) ?
		    VP_FIELD_ID_TOGGLE_VSYNC : VP_FIELD_ID_TOGGLE_USER;

	writel(temp_reg, g_vp_base + S5P_VP_MODE);

	VPPRINTK("line_skip(%d), mem_mode(%d), chroma_exp(%d), toggle_id(%d)\n",
		line_skip, mem_mode, chroma_exp, toggle_id);
	VPPRINTK("S5P_VP_MODE(0x%08x)\n", readl(g_vp_base + S5P_VP_MODE));
}

void s5p_vp_init_pixel_rate_control(enum s5p_vp_pxl_rate rate)
{
	writel(VP_PEL_RATE_CTRL(rate), g_vp_base + S5P_VP_PER_RATE_CTRL);

	VPPRINTK("rate(%d), S5P_VP_PER_RATE_CTRL(0x%08x)\n",
		rate, readl(g_vp_base + S5P_VP_PER_RATE_CTRL));
}

enum s5p_tv_vp_err s5p_vp_init_layer(
	u32 top_y_addr,
	u32 top_c_addr,
	u32 bottom_y_addr,
	u32 bottom_c_addr,
	enum s5p_endian_type src_img_endian,
	u32 img_width,
	u32 img_height,
	u32 src_off_x,
	u32 src_x_fract_step,
	u32 src_off_y,
	u32 src_width,
	u32 src_height,
	u32 dst_off_x,
	u32 dst_off_y,
	u32 dst_width,
	u32 dst_height,
	bool ipc_2d)
{
	enum s5p_tv_vp_err ret = VPROC_NO_ERROR;

	VPPRINTK("src_img_endian(%d)\n", src_img_endian);

	writel(1, g_vp_base + S5P_VP_ENDIAN_MODE);

	ret = s5p_vp_set_top_field_address(top_y_addr, top_c_addr);
	if (ret != VPROC_NO_ERROR) {
		pr_err("s5p_vp_set_top_field_address(%d, %d) fail\n",
			top_y_addr, top_c_addr);
		return ret;
	}

	ret = s5p_vp_set_bottom_field_address(bottom_y_addr, bottom_c_addr);
	if (ret != VPROC_NO_ERROR) {
		pr_err("s5p_vp_set_bottom_field_address(%d, %d) fail\n",
			bottom_y_addr, bottom_c_addr);
		return ret;
	}

	ret = s5p_vp_set_img_size(img_width, img_height);
	if (ret != VPROC_NO_ERROR) {
		pr_err("s5p_vp_set_img_size(%d, %d) fail\n",
			img_width, img_height);
		return ret;
	}

	s5p_vp_set_src_position(src_off_x, src_x_fract_step, src_off_y);
	s5p_vp_set_dest_position(dst_off_x, dst_off_y);
	s5p_vp_set_src_dest_size(src_width, src_height, dst_width,
				dst_height, ipc_2d);

	VPPRINTK("S5P_VP_ENDIAN_MODE(0x%08x)\n", readl(g_vp_base + S5P_VP_ENDIAN_MODE));

	return ret;
}

enum s5p_tv_vp_err s5p_vp_init_layer_def_poly_filter_coef(
	u32 top_y_addr,
	u32 top_c_addr,
	u32 bottom_y_addr,
	u32 bottom_c_addr,
	enum s5p_endian_type src_img_endian,
	u32 img_width,
	u32 img_height,
	u32 src_off_x,
	u32 src_x_fract_step,
	u32 src_off_y,
	u32 src_width,
	u32 src_height,
	u32 dst_off_x,
	u32 dst_off_y,
	u32 dst_width,
	u32 dst_height,
	bool ipc_2d)
{
	u32 h_ratio = (src_width << 16) / dst_width;
	u32 v_ratio = (ipc_2d) ? ((src_height << 17) / dst_height) :
				((src_height << 16) / dst_height);

	s5p_vp_set_poly_filter_coef_default(h_ratio, v_ratio);

	return s5p_vp_init_layer(top_y_addr,
				top_c_addr,
				bottom_y_addr,
				bottom_c_addr,
				src_img_endian,
				img_width,
				img_height,
				src_off_x,
				src_x_fract_step,
				src_off_y,
				src_width,
				src_height,
				dst_off_x,
				dst_off_y,
				dst_width,
				dst_height,
				ipc_2d);
}

enum s5p_tv_vp_err s5p_vp_init_poly_filter_coef(
	enum s5p_vp_poly_coeff poly_coeff,
	signed char ch0,
	signed char ch1,
	signed char ch2,
	signed char ch3)
{
	return s5p_vp_set_poly_filter_coef(poly_coeff, ch0, ch1, ch2, ch3);
}

void s5p_vp_init_bypass_post_process(bool bypass)
{
	writel((bypass) ? VP_BY_PASS_ENABLE : VP_BY_PASS_DISABLE,
		g_vp_base + S5P_PP_BYPASS);

	VPPRINTK("bypass(%d), S5P_PP_BYPASS(0x%08x)\n",
		bypass, readl(g_vp_base + S5P_PP_BYPASS));
}

enum s5p_tv_vp_err s5p_vp_init_csc_coef(
	enum s5p_vp_csc_coeff csc_coeff, u32 coeff)
{
	VPPRINTK("csc_coeff(%d), coeff(%d)\n", csc_coeff, coeff);

	if (csc_coeff > VPROC_CSC_CR2CR_COEF ||
		csc_coeff < VPROC_CSC_Y2Y_COEF) {
		pr_err("invaild csc_coeff parameter(%d)\n", csc_coeff);
		return S5P_TV_VP_ERR_INVALID_PARAM;
	}

	writel(VP_CSC_COEF(coeff),
		g_vp_base + S5P_PP_CSC_Y2Y_COEF + csc_coeff * 4);

	VPPRINTK("0x%08x\n",
		readl(g_vp_base + S5P_PP_CSC_Y2Y_COEF + csc_coeff * 4));

	return VPROC_NO_ERROR;
}

void s5p_vp_init_saturation(u32 sat)
{
	VPPRINTK("%d\n", sat);

	writel(VP_SATURATION(sat), g_vp_base + S5P_PP_SATURATION);

	VPPRINTK("0x%08x\n", readl(g_vp_base + S5P_PP_SATURATION));
}

void s5p_vp_init_sharpness(u32 th_h_noise,
	enum s5p_vp_sharpness_control sharpness)
{
	VPPRINTK("%d, %d\n", th_h_noise, sharpness);

	writel(VP_TH_HNOISE(th_h_noise) | VP_SHARPNESS(sharpness),
		g_vp_base + S5P_PP_SHARPNESS);

	VPPRINTK("0x%08x\n", readl(g_vp_base + S5P_PP_SHARPNESS));
}

enum s5p_tv_vp_err s5p_vp_init_brightness_contrast_control(
	enum s5p_vp_line_eq eq_num,
	u32 intc,
	u32 slope)
{
	return s5p_vp_set_brightness_contrast_control(eq_num, intc, slope);
}

void s5p_vp_init_brightness(bool brightness)
{
	s5p_vp_set_brightness(brightness);
}

void s5p_vp_init_contrast(u8 contrast)
{
	s5p_vp_set_contrast(contrast);
}

void s5p_vp_init_brightness_offset(u32 offset)
{
	writel(VP_BRIGHT_OFFSET(offset), g_vp_base + S5P_PP_BRIGHT_OFFSET);

	VPPRINTK("offset(%d), S5P_PP_BRIGHT_OFFSET(0x%08x)\n",
		offset,
		readl(g_vp_base + S5P_PP_BRIGHT_OFFSET));
}

void s5p_vp_init_csc_control(bool sub_y_offset_en, bool csc_en)
{
	u32 temp_reg;

	temp_reg = (sub_y_offset_en) ? VP_SUB_Y_OFFSET_ENABLE :
		   VP_SUB_Y_OFFSET_DISABLE;
	temp_reg |= (csc_en) ? VP_CSC_ENABLE : VP_CSC_DISABLE;

	writel(temp_reg, g_vp_base + S5P_PP_CSC_EN);

	VPPRINTK("sub_y_offset_en(%d), csc_en(%d), S5P_PP_CSC_EN(0x%08x)\n",
		sub_y_offset_en, csc_en, readl(g_vp_base + S5P_PP_CSC_EN));
}

enum s5p_tv_vp_err s5p_vp_init_csc_coef_default(enum s5p_vp_csc_type csc_type)
{
	switch (csc_type) {
	case VPROC_CSC_SD_HD:
		writel(Y2Y_COEF_601_TO_709,   g_vp_base + S5P_PP_CSC_Y2Y_COEF);
		writel(CB2Y_COEF_601_TO_709,  g_vp_base + S5P_PP_CSC_CB2Y_COEF);
		writel(CR2Y_COEF_601_TO_709,  g_vp_base + S5P_PP_CSC_CR2Y_COEF);
		writel(Y2CB_COEF_601_TO_709,  g_vp_base + S5P_PP_CSC_Y2CB_COEF);
		writel(CB2CB_COEF_601_TO_709, g_vp_base + S5P_PP_CSC_CB2CB_COEF);
		writel(CR2CB_COEF_601_TO_709, g_vp_base + S5P_PP_CSC_CR2CB_COEF);
		writel(Y2CR_COEF_601_TO_709,  g_vp_base + S5P_PP_CSC_Y2CR_COEF);
		writel(CB2CR_COEF_601_TO_709, g_vp_base + S5P_PP_CSC_CB2CR_COEF);
		writel(CR2CR_COEF_601_TO_709, g_vp_base + S5P_PP_CSC_CR2CR_COEF);
		break;
	case VPROC_CSC_HD_SD:
		writel(Y2Y_COEF_709_TO_601,   g_vp_base + S5P_PP_CSC_Y2Y_COEF);
		writel(CB2Y_COEF_709_TO_601,  g_vp_base + S5P_PP_CSC_CB2Y_COEF);
		writel(CR2Y_COEF_709_TO_601,  g_vp_base + S5P_PP_CSC_CR2Y_COEF);
		writel(Y2CB_COEF_709_TO_601,  g_vp_base + S5P_PP_CSC_Y2CB_COEF);
		writel(CB2CB_COEF_709_TO_601, g_vp_base + S5P_PP_CSC_CB2CB_COEF);
		writel(CR2CB_COEF_709_TO_601, g_vp_base + S5P_PP_CSC_CR2CB_COEF);
		writel(Y2CR_COEF_709_TO_601,  g_vp_base + S5P_PP_CSC_Y2CR_COEF);
		writel(CB2CR_COEF_709_TO_601, g_vp_base + S5P_PP_CSC_CB2CR_COEF);
		writel(CR2CR_COEF_709_TO_601, g_vp_base + S5P_PP_CSC_CR2CR_COEF);
		break;
	default:
		pr_err("invalid csc_type parameter = %d\n", csc_type);
		return S5P_TV_VP_ERR_INVALID_PARAM;
		break;
	}

	VPPRINTK("csc_type(%d)\n", csc_type);
	VPPRINTK("0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x,\
		0x%08x, 0x%08x\n",
		readl(g_vp_base + S5P_PP_CSC_Y2Y_COEF),
		readl(g_vp_base + S5P_PP_CSC_CB2Y_COEF),
		readl(g_vp_base + S5P_PP_CSC_CR2Y_COEF),
		readl(g_vp_base + S5P_PP_CSC_Y2CB_COEF),
		readl(g_vp_base + S5P_PP_CSC_CB2CB_COEF),
		readl(g_vp_base + S5P_PP_CSC_CR2CB_COEF),
		readl(g_vp_base + S5P_PP_CSC_Y2CR_COEF),
		readl(g_vp_base + S5P_PP_CSC_CB2CR_COEF),
		readl(g_vp_base + S5P_PP_CSC_CR2CR_COEF));

	return VPROC_NO_ERROR;
}

/*
 * start  - start functions are only called under stopping video processor
 */
enum s5p_tv_vp_err s5p_vp_start(void)
{
	writel(VP_ON_ENABLE, g_vp_base + S5P_VP_ENABLE);

	return s5p_vp_update();
}

/*
 * stop  - stop functions are only called under running video processor
 */
enum s5p_tv_vp_err s5p_vp_stop(void)
{
	enum s5p_tv_vp_err ret = VPROC_NO_ERROR;
	int time_out = HDMI_TIME_OUT;

	writel((readl(g_vp_base + S5P_VP_ENABLE) & ~VP_ON_ENABLE),
		g_vp_base + S5P_VP_ENABLE);

	ret = s5p_vp_update();

	while (!(readl(g_vp_base + S5P_VP_ENABLE) & VP_POWER_DOWN_RDY) && time_out) {
		msleep(1);
		time_out--;
	}

	if (time_out <= 0) {
		pr_err("readl S5P_VP_ENABLE for VP_POWER_DOWN_RDY fail\n");
		ret = S5P_TV_VP_ERR_INVALID_PARAM;
	}

	return ret;
}

/*
 * reset  - reset function
 */
void s5p_vp_sw_reset(void)
{
	int time_out = HDMI_TIME_OUT;

	writel((readl(g_vp_base + S5P_VP_SRESET) | VP_SOFT_RESET),
		g_vp_base + S5P_VP_SRESET);

	while ((readl(g_vp_base + S5P_VP_SRESET) & VP_SOFT_RESET) && time_out) {
		msleep(10);
		time_out--;
	}

	if (time_out <= 0)
		pr_err("readl S5P_VP_SRESET for VP_SOFT_RESET fail\n");
}

int __init s5p_vp_probe(struct platform_device *pdev, u32 res_num)
{
	struct resource *res;
	size_t	size;

	res = platform_get_resource(pdev, IORESOURCE_MEM, res_num);
	if (res == NULL) {
		dev_err(&pdev->dev,
			"failed to get memory region resource\n");
		goto error;

	}

	size = (res->end - res->start) + 1;

	g_vp_mem = request_mem_region(res->start, size, pdev->name);
	if (g_vp_mem == NULL) {
		dev_err(&pdev->dev,
			"failed to get memory region\n");
		goto error;
	}

	g_vp_base = ioremap(res->start, size);
	if (g_vp_base == NULL) {
		dev_err(&pdev->dev,
			"failed to ioremap address region\n");
		goto error_ioremap_fail;
	}

	return 0;

error_ioremap_fail:
	if (g_vp_mem) {
		release_resource(g_vp_mem);
		kfree(g_vp_mem);
		g_vp_mem = NULL;
	}
error:
	return -ENOENT;

}

int __init s5p_vp_release(struct platform_device *pdev)
{
	if (g_vp_base) {
		iounmap(g_vp_base);
		g_vp_base = NULL;
	}

	/* remove memory region */
	if (g_vp_mem) {
		if (release_resource(g_vp_mem))
			dev_err(&pdev->dev,
				"Can't remove tvout drv !!\n");

		kfree(g_vp_mem);
		g_vp_mem = NULL;
	}

	return 0;
}
