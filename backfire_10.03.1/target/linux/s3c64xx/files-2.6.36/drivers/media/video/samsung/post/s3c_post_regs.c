/* linux/drivers/media/video/samsung/s3c_post_regs.c
 *
 * Register interface file for Samsung Post Processor driver
 *
 * Jinsung Yang, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <asm/io.h>
#include <mach/map.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-post.h>
#include <plat/post.h>

#include "s3c_post.h"

void s3c_post_clear_irq(struct s3c_post_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CIGCTRL);

	cfg |= S3C_CIGCTRL_IRQ_CLR;

	writel(cfg, ctrl->regs + S3C_CIGCTRL);
}

static void s3c_post_reset_cfg(struct s3c_post_control *ctrl)
{
	int i;
	u32 cfg[][2] = {
		{ 0x018, 0x00000000 }, { 0x01c, 0x00000000 },
		{ 0x020, 0x00000000 }, { 0x024, 0x00000000 },
		{ 0x028, 0x00000000 }, { 0x02c, 0x00000000 },
		{ 0x030, 0x00000000 }, { 0x034, 0x00000000 },
		{ 0x038, 0x00000000 }, { 0x03c, 0x00000000 },
		{ 0x040, 0x00000000 }, { 0x044, 0x00000000 },
		{ 0x048, 0x00000000 }, { 0x04c, 0x00000000 },
		{ 0x050, 0x00000000 }, { 0x054, 0x00000000 },
		{ 0x058, 0x18000000 }, { 0x05c, 0x00000000 },
		{ 0x0c0, 0x00000000 }, { 0x0c4, 0xffffffff },
		{ 0x0d0, 0x00100080 }, { 0x0d4, 0x00000000 },
		{ 0x0d8, 0x00000000 }, { 0x0dc, 0x00000000 },
		{ 0x0f8, 0x00000000 }, { 0x0fc, 0x04000000 },
		{ 0x144, 0x00000000 }, { 0x148, 0x00000000 },
		{ 0x14c, 0x00000000 }, { 0x168, 0x00000000 },
		{ 0x16c, 0x00000000 }, { 0x170, 0x00000000 },
		{ 0x174, 0x00000000 }, { 0x178, 0x00000000 },
		{ 0x17c, 0x00000000 }, { 0x180, 0x00000000 },
		{ 0x184, 0x00000000 }, { 0x188, 0x00000000 },
		{ 0x18c, 0x00000000 },
	};

	for (i = 0; i < sizeof(cfg) / 8; i++)
		writel(cfg[i][1], ctrl->regs + cfg[i][0]);
}

void s3c_post_reset(struct s3c_post_control *ctrl)
{
	u32 cfg;

	/* s/w reset */
	cfg = readl(ctrl->regs + S3C_CIGCTRL);
	cfg |= (S3C_CIGCTRL_SWRST | S3C_CIGCTRL_IRQ_LEVEL);
	writel(cfg, ctrl->regs + S3C_CIGCTRL);
	mdelay(1);

	cfg = readl(ctrl->regs + S3C_CIGCTRL);
	cfg &= ~S3C_CIGCTRL_SWRST;
	writel(cfg, ctrl->regs + S3C_CIGCTRL);

	s3c_post_reset_cfg(ctrl);
}

void s3c_post_set_target_format(struct s3c_post_control *ctrl)
{
	struct s3c_post_out_frame *frame = &ctrl->out_frame;
	u32 cfg = 0;

	switch (frame->format) {
	case FORMAT_RGB565: /* fall through */
	case FORMAT_RGB666: /* fall through */
	case FORMAT_RGB888:
		cfg |= S3C_CITRGFMT_OUTFORMAT_RGB;
		break;

	case FORMAT_YCBCR420:
		cfg |= S3C_CITRGFMT_OUTFORMAT_YCBCR420;
		break;

	case FORMAT_YCBCR422:
		if (frame->planes == 1)
			cfg |= S3C_CITRGFMT_OUTFORMAT_YCBCR422_1PLANE;
		else
			cfg |= S3C_CITRGFMT_OUTFORMAT_YCBCR422;

		break;
	}

	cfg |= S3C_CITRGFMT_TARGETHSIZE(frame->width);
	cfg |= S3C_CITRGFMT_TARGETVSIZE(frame->height);

	writel(cfg, ctrl->regs + S3C_CITRGFMT);

	cfg = S3C_CITAREA_TARGET_AREA(frame->width * frame->height);
	writel(cfg, ctrl->regs + S3C_CITAREA);
}

static void s3c_post_set_output_dma_size(struct s3c_post_control *ctrl)
{
	struct s3c_post_out_frame *frame = &ctrl->out_frame;
	int ofs_h = frame->offset.y_h * 2;
	int ofs_v = frame->offset.y_v * 2;
	u32 cfg = 0;

	cfg |= S3C_ORGOSIZE_HORIZONTAL(frame->width - ofs_h);
	cfg |= S3C_ORGOSIZE_VERTICAL(frame->height - ofs_v);

	writel(cfg, ctrl->regs + S3C_ORGOSIZE);
}

void s3c_post_set_output_dma(struct s3c_post_control *ctrl)
{
	struct s3c_post_out_frame *frame = &ctrl->out_frame;
	u32 cfg;

	/* for offsets */
	cfg = 0;
	cfg |= S3C_CIOYOFF_HORIZONTAL(frame->offset.y_h);
	cfg |= S3C_CIOYOFF_VERTICAL(frame->offset.y_v);
	writel(cfg, ctrl->regs + S3C_CIOYOFF);

	cfg = 0;
	cfg |= S3C_CIOCBOFF_HORIZONTAL(frame->offset.cb_h);
	cfg |= S3C_CIOCBOFF_VERTICAL(frame->offset.cb_v);
	writel(cfg, ctrl->regs + S3C_CIOCBOFF);

	cfg = 0;
	cfg |= S3C_CIOCROFF_HORIZONTAL(frame->offset.cr_h);
	cfg |= S3C_CIOCROFF_VERTICAL(frame->offset.cr_v);
	writel(cfg, ctrl->regs + S3C_CIOCROFF);

	/* for original size */
	s3c_post_set_output_dma_size(ctrl);
	
	/* for output dma control */
	cfg = readl(ctrl->regs + S3C_CIOCTRL);

	cfg &= ~(S3C_CIOCTRL_ORDER2P_MASK | S3C_CIOCTRL_ORDER422_MASK | \
		 S3C_CIOCTRL_YCBCR_PLANE_MASK);

	if (frame->planes == 1)
		cfg |= frame->order_1p;
	else if (frame->planes == 2)
		cfg |= (S3C_CIOCTRL_YCBCR_2PLANE | \
			(frame->order_2p << S3C_CIOCTRL_ORDER2P_SHIFT));
	else if (frame->planes == 3)
		cfg |= S3C_CIOCTRL_YCBCR_3PLANE;

	writel(cfg, ctrl->regs + S3C_CIOCTRL);
}

void s3c_post_set_prescaler(struct s3c_post_control *ctrl)
{
	struct s3c_post_scaler *sc = &ctrl->scaler;
	u32 cfg = 0, shfactor;

	shfactor = 10 - (sc->hfactor + sc->vfactor);

	cfg |= S3C_CISCPRERATIO_SHFACTOR(shfactor);
	cfg |= S3C_CISCPRERATIO_PREHORRATIO(sc->pre_hratio);
	cfg |= S3C_CISCPRERATIO_PREVERRATIO(sc->pre_vratio);

	writel(cfg, ctrl->regs + S3C_CISCPRERATIO);

	cfg = 0;
	cfg |= S3C_CISCPREDST_PREDSTWIDTH(sc->pre_dst_width);
	cfg |= S3C_CISCPREDST_PREDSTHEIGHT(sc->pre_dst_height);

	writel(cfg, ctrl->regs + S3C_CISCPREDST);
}

void s3c_post_set_scaler(struct s3c_post_control *ctrl)
{
	struct s3c_post_scaler *sc = &ctrl->scaler;
	u32 cfg = (S3C_CISCCTRL_CSCR2Y_WIDE | S3C_CISCCTRL_CSCY2R_WIDE);

	if (sc->scaleup_h)
		cfg |= S3C_CISCCTRL_SCALEUP_H;

	if (sc->scaleup_v)
		cfg |= S3C_CISCCTRL_SCALEUP_V;

	if (ctrl->in_frame.format == FORMAT_RGB565)
		cfg |= S3C_CISCCTRL_INRGB_FMT_RGB565;
	else if (ctrl->in_frame.format == FORMAT_RGB666)
		cfg |= S3C_CISCCTRL_INRGB_FMT_RGB666;
	else if (ctrl->in_frame.format == FORMAT_RGB888)
		cfg |= S3C_CISCCTRL_INRGB_FMT_RGB888;

	if (ctrl->out_type == PATH_OUT_DMA) {
		if (ctrl->out_frame.format == FORMAT_RGB565)
			cfg |= S3C_CISCCTRL_OUTRGB_FMT_RGB565;
		else if (ctrl->out_frame.format == FORMAT_RGB666)
			cfg |= S3C_CISCCTRL_OUTRGB_FMT_RGB666;
		else if (ctrl->out_frame.format == FORMAT_RGB888)
			cfg |= S3C_CISCCTRL_OUTRGB_FMT_RGB888;
	} else {
		cfg |= S3C_CISCCTRL_OUTRGB_FMT_RGB888;

		if (ctrl->out_frame.scan == SCAN_TYPE_INTERLACE)
			cfg |= S3C_CISCCTRL_INTERLACE;
		else
			cfg |= S3C_CISCCTRL_PROGRESSIVE;
	}

	cfg |= S3C_CISCCTRL_MAINHORRATIO(sc->main_hratio);
	cfg |= S3C_CISCCTRL_MAINVERRATIO(sc->main_vratio);

	writel(cfg, ctrl->regs + S3C_CISCCTRL);
}

void s3c_post_start_scaler(struct s3c_post_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CISCCTRL);

	cfg |= S3C_CISCCTRL_SCALERSTART;
	writel(cfg, ctrl->regs + S3C_CISCCTRL);

	if (ctrl->out_type == PATH_OUT_LCDFIFO)
		ctrl->open_lcdfifo(ctrl->id, 0, 0);
}

void s3c_post_stop_scaler(struct s3c_post_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CISCCTRL);

	if (ctrl->out_type == PATH_OUT_LCDFIFO)
		ctrl->close_lcdfifo(ctrl->id);

	cfg &= ~S3C_CISCCTRL_SCALERSTART;
	writel(cfg, ctrl->regs + S3C_CISCCTRL);
}

void s3c_post_enable_capture(struct s3c_post_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CIIMGCPT);

	cfg &= ~S3C_CIIMGCPT_CPT_FREN_ENABLE;
	cfg |= (S3C_CIIMGCPT_IMGCPTEN | S3C_CIIMGCPT_IMGCPTEN_SC);

	writel(cfg, ctrl->regs + S3C_CIIMGCPT);
}

void s3c_post_disable_capture(struct s3c_post_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CIIMGCPT);

	cfg &= ~(S3C_CIIMGCPT_IMGCPTEN | S3C_CIIMGCPT_IMGCPTEN_SC);
	writel(cfg, ctrl->regs + S3C_CIIMGCPT);
}

static void s3c_post_set_input_dma_size(struct s3c_post_control *ctrl)
{
	struct s3c_post_in_frame *frame = &ctrl->in_frame;
	int ofs_h = frame->offset.y_h * 2;
	int ofs_v = frame->offset.y_v * 2;
	u32 cfg_o = 0, cfg_r = S3C_CIREAL_ISIZE_AUTOLOAD_ENABLE;

	cfg_o |= S3C_ORGISIZE_HORIZONTAL(frame->width - ofs_h);
	cfg_o |= S3C_ORGISIZE_VERTICAL(frame->height - ofs_v);
	cfg_r |= S3C_CIREAL_ISIZE_WIDTH(frame->width - ofs_h);
	cfg_r |= S3C_CIREAL_ISIZE_HEIGHT(frame->height - ofs_v);

	writel(cfg_o, ctrl->regs + S3C_ORGISIZE);
	writel(cfg_r, ctrl->regs + S3C_CIREAL_ISIZE);
}

void s3c_post_set_input_dma(struct s3c_post_control *ctrl)
{
	struct s3c_post_in_frame *frame = &ctrl->in_frame;
	u32 cfg;

	/* for offsets */
	cfg = 0;
	cfg |= S3C_CIIYOFF_HORIZONTAL(frame->offset.y_h);
	cfg |= S3C_CIIYOFF_VERTICAL(frame->offset.y_v);
	writel(cfg, ctrl->regs + S3C_CIIYOFF);

	cfg = 0;
	cfg |= S3C_CIICBOFF_HORIZONTAL(frame->offset.cb_h);
	cfg |= S3C_CIICBOFF_VERTICAL(frame->offset.cb_v);
	writel(cfg, ctrl->regs + S3C_CIICBOFF);

	cfg = 0;
	cfg |= S3C_CIICROFF_HORIZONTAL(frame->offset.cr_h);
	cfg |= S3C_CIICROFF_VERTICAL(frame->offset.cr_v);
	writel(cfg, ctrl->regs + S3C_CIICROFF);

	/* for original & real size */
	s3c_post_set_input_dma_size(ctrl);

	/* for input dma control */
	cfg = S3C_MSCTRL_SUCCESSIVE_COUNT(4);

	switch (frame->format) {
	case FORMAT_RGB565: /* fall through */
	case FORMAT_RGB666: /* fall through */
	case FORMAT_RGB888:
		cfg |= S3C_MSCTRL_INFORMAT_RGB;
		break;

	case FORMAT_YCBCR420:
		cfg |= S3C_MSCTRL_INFORMAT_YCBCR420;

		if (frame->planes == 2)
			cfg |= (S3C_MSCTRL_C_INT_IN_2PLANE | \
				(frame->order_2p << S3C_MSCTRL_2PLANE_SHIFT));
		else
			cfg |= S3C_MSCTRL_C_INT_IN_3PLANE;

		break;

	case FORMAT_YCBCR422:
		if (frame->planes == 1)
			cfg |= (frame->order_1p | \
				S3C_MSCTRL_INFORMAT_YCBCR422_1PLANE);
		else {
			cfg |= S3C_MSCTRL_INFORMAT_YCBCR422;

			if (frame->planes == 2)
				cfg |= (S3C_MSCTRL_C_INT_IN_2PLANE | \
					(frame->order_2p << S3C_MSCTRL_2PLANE_SHIFT));
			else
				cfg |= S3C_MSCTRL_C_INT_IN_3PLANE;
		}

		break;
	}

	writel(cfg, ctrl->regs + S3C_MSCTRL);
}

void s3c_post_start_input_dma(struct s3c_post_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_MSCTRL);

	cfg |= S3C_MSCTRL_ENVID;
	writel(cfg, ctrl->regs + S3C_MSCTRL);
}

void s3c_post_stop_input_dma(struct s3c_post_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_MSCTRL);

	cfg &= ~S3C_MSCTRL_ENVID;
	writel(cfg, ctrl->regs + S3C_MSCTRL);
}

void s3c_post_set_output_path(struct s3c_post_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CISCCTRL);

	cfg &= ~S3C_CISCCTRL_LCDPATHEN_FIFO;

	if (ctrl->out_type == PATH_OUT_LCDFIFO)
		cfg |= S3C_CISCCTRL_LCDPATHEN_FIFO;

	writel(cfg, ctrl->regs + S3C_CISCCTRL);
}

void s3c_post_set_input_address(struct s3c_post_control *ctrl)
{
	struct s3c_post_frame_addr *addr = &ctrl->in_frame.addr;
	u32 cfg = 0;

	cfg = readl(ctrl->regs + S3C_CIREAL_ISIZE);
	cfg |= S3C_CIREAL_ISIZE_ADDR_CH_DISABLE;
	writel(cfg, ctrl->regs + S3C_CIREAL_ISIZE);

	writel(addr->phys_y, ctrl->regs + S3C_CIIYSA0);
	writel(addr->phys_cb, ctrl->regs + S3C_CIICBSA0);
	writel(addr->phys_cr, ctrl->regs + S3C_CIICRSA0);

	cfg &= ~S3C_CIREAL_ISIZE_ADDR_CH_DISABLE;
	writel(cfg, ctrl->regs + S3C_CIREAL_ISIZE);
}

void s3c_post_set_output_address(struct s3c_post_control *ctrl)
{
	struct s3c_post_out_frame *frame = &ctrl->out_frame;
	struct s3c_post_frame_addr *addr;
	int i;

	for (i = 0; i < S3C_POST_MAX_FRAMES; i++) {
		addr = &frame->addr[i];
		writel(addr->phys_y, ctrl->regs + S3C_CIOYSA(i));
		writel(addr->phys_cb, ctrl->regs + S3C_CIOCBSA(i));
		writel(addr->phys_cr, ctrl->regs + S3C_CIOCRSA(i));
	}
}

int s3c_post_get_frame_count(struct s3c_post_control *ctrl)
{
	return S3C_CISTATUS_GET_FRAME_COUNT(readl(ctrl->regs + S3C_CISTATUS));
}

void s3c_post_wait_frame_end(struct s3c_post_control *ctrl)
{
        unsigned long timeo = jiffies;
	unsigned int frame_cnt = 0;
	u32 cfg;

        timeo += 20;    /* waiting for 100mS */

	while (time_before(jiffies, timeo)) {
		cfg = readl(ctrl->regs + S3C_CISTATUS);
		
		if (S3C_CISTATUS_GET_FRAME_END(cfg)) {
			cfg &= ~S3C_CISTATUS_FRAMEEND;
			writel(cfg, ctrl->regs + S3C_CISTATUS);

			if (frame_cnt == 2)
				break;
			else
				frame_cnt++;
		}
		cond_resched();
	}
}
