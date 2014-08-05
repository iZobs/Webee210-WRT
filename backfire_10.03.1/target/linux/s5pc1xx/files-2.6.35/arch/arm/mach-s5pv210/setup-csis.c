/* linux/arch/arm/plat-s5p/setup-csis.c
 *
 * Copyright (c) 2009 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * S5P - Base MIPI-CSI2 gpio configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-clock.h>
#include <plat/csis.h>

struct platform_device; /* don't need the contents */

void s3c_csis_cfg_gpio(void) { }

void s3c_csis_cfg_phy_global(int on)
{
	u32 cfg;

	if (on) {
		/* MIPI D-PHY Power Enable */
		cfg = __raw_readl(S5P_MIPI_CONTROL);
		cfg |= S5P_MIPI_DPHY_EN;
		__raw_writel(cfg, S5P_MIPI_CONTROL);
	} else {
		/* MIPI Power Disable */
		cfg = __raw_readl(S5P_MIPI_CONTROL);
		cfg &= ~S5P_MIPI_DPHY_EN;
		__raw_writel(cfg, S5P_MIPI_CONTROL);
	}
}

int s3c_csis_clk_on(struct platform_device *pdev, struct clk **clk)
{
	struct clk *sclk_csis = NULL;
	struct clk *parent = NULL;
	struct clk *mout_csis = NULL;
	struct s3c_platform_csis *pdata;

	pdata = to_csis_plat(&pdev->dev);

	/* mout_mpll */
	parent = clk_get(&pdev->dev, pdata->srclk_name);
	if (IS_ERR(parent)) {
		dev_err(&pdev->dev, "failed to get parent clock(%s) for csis\n",
			pdata->srclk_name);
		goto err_clk1;
	}

	/* mout_csis */
	mout_csis = clk_get(&pdev->dev, "mout_csis");
	if (IS_ERR(mout_csis)) {
		dev_err(&pdev->dev, "failed to get mout_csis clock source\n");
		goto err_clk2;
	}
	/* sclk_csis */
	sclk_csis = clk_get(&pdev->dev, pdata->clk_name);
	if (IS_ERR(sclk_csis)) {
		dev_err(&pdev->dev, "failed to get csis(%s) clock source\n",
			pdata->clk_name);
		goto err_clk3;
	}

	clk_set_parent(mout_csis, parent);
	clk_set_parent(sclk_csis, mout_csis);

	clk_enable(sclk_csis);

	return 0;
err_clk3:
	clk_put(mout_csis);

err_clk2:
	clk_put(parent);

err_clk1:
	return -EINVAL;
}

int s3c_csis_clk_off(struct platform_device *pdev, struct clk **clk)
{
	struct clk *sclk_csis = NULL;
	struct s3c_platform_csis *pdata;

	pdata = to_csis_plat(&pdev->dev);

	/* sclk_csis */
	sclk_csis = clk_get(&pdev->dev, pdata->clk_name);
	if (IS_ERR(sclk_csis)) {
		dev_err(&pdev->dev, "failed to get csis clock(%s) source\n",
			pdata->srclk_name);
		return -EINVAL;
	}

	/* clock disable for csis */
	clk_disable(sclk_csis);

	return 0;
}

