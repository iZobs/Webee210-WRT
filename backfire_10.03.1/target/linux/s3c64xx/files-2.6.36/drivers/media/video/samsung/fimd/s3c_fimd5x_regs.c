/* linux/drivers/media/video/samsung/fimd/s3c_fimd5x_regs.c
 *
 * Register interface file for Samsung Display Controller (FIMD) driver
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
#include <plat/regs-fimd.h>
#include <plat/fimd.h>

#include "s3c_fimd.h"

int s3c_fimd_set_output(struct s3c_fimd_global *ctrl)
{
	u32 cfg;

	cfg = readl(ctrl->regs + S3C_VIDCON0);
	cfg &= ~S3C_VIDCON0_VIDOUT_MASK;

	if (ctrl->output == OUTPUT_RGB)
		cfg |= S3C_VIDCON0_VIDOUT_RGB;
	else if (ctrl->out == OUTPUT_ITU)
		cfg |= S3C_VIDCON0_VIDOUT_ITU;
	else if (ctrl->out == OUTPUT_I80LDI0)
		cfg |= S3C_VIDCON0_VIDOUT_I80LDI0;
	else if (ctrl->out == OUTPUT_I80LDI1)
		cfg |= S3C_VIDCON0_VIDOUT_I80LDI1;
	else {
		err("invalid output type: %d\n", ctrl->output);
		return -EINVAL;
	}

	writel(cfg, ctrl->regs + S3C_VIDCON0);

	return 0;
}

int s3c_fimd_set_display_mode(struct s3c_fimd_global *ctrl)
{
	u32 cfg;

	cfg = readl(ctrl->regs + S3C_VIDCON0);
	cfg &= ~S3C_VIDCON0_PNRMODE_MASK;
	cfg |= (ctrl->rgb_mode << S3C_VIDCON0_PNRMODE_SHIFT);

	return 0;
}

int s3c_fimd_display_on(struct s3c_fimd_global *ctrl)
{
	u32 cfg;

	cfg = readl(ctrl->regs + S3C_VIDCON0);
	cfg |= (S3C_VIDCON0_ENVID_ENABLE | S3C_VIDCON0_ENVID_F_ENABLE);
	writel(cfg, ctrl->regs + S3C_VIDCON0);

	dev_dbg(ctrl->dev, "global display is on\n");

	return 0;
}

int s3c_fimd_display_off(struct s3c_fimd_global *ctrl)
{
	u32 cfg;

	cfg = readl(ctrl->regs + S3C_VIDCON0);
	cfg &= ~S3C_VIDCON0_ENVID_ENABLE;
	writel(cfg, ctrl->regs + S3C_VIDCON0);

	cfg &= ~S3C_VIDCON0_ENVID_F_ENABLE;
	writel(cfg, ctrl->regs + S3C_VIDCON0);

	dev_dbg(ctrl->dev, "global display is off\n");

	return 0;
}

int s3c_fimd_frame_off(struct s3c_fimd_global *ctrl)
{
	u32 cfg;

	cfg = readl(ctrl->regs + S3C_VIDCON0);
	cfg &= ~S3C_VIDCON0_ENVID_F_ENABLE;
	writel(cfg, ctrl->regs + S3C_VIDCON0);

	dev_dbg(ctrl->dev, "current frame display is off\n");

	return 0;
}

int s3c_fimd_set_clock(struct s3c_fimd_global *ctrl)
{
	struct s3c_platform_fimd *plat;
	u32 cfg, maxclk, src_clk, vclk, div;

	plat = to_fimd_plat(ctrl->dev)
	maxclk = 66 * 1000000;
	
	/* fixed clock source: hclk */
	cfg = readl(ctrl->regs + S3C_VIDCON0);
	cfg &= ~(S3C_VIDCON0_CLKSEL_MASK | S3C_VIDCON0_CLKVALUP_MASK | \
		S3C_VIDCON0_VCLKEN_MASK | S3C_VIDCON0_CLKDIR_MASK);
	cfg |= (S3C_VIDCON0_CLKSEL_HCLK | S3C_VIDCON0_CLKVALUP_ALWAYS | \
		S3C_VIDCON0_VCLKEN_NORMAL | S3C_VIDCON0_CLKDIR_DIVIDED);
	
	src_clk = ctrl->clock->parent->rate;
	vclk = plat->clockrate;

	if (vclk > maxclk)
		vclk = maxclk;

	div = (int) (src_clk / vclk);

	cfg |= S3C_VIDCON0_CLKVAL_F(div - 1);
	writel(cfg, ctrl->regs + S3C_VIDCON0);

	return 0;	
}










