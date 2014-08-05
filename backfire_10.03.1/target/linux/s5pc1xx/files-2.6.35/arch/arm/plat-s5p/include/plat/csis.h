/* linux/arch/arm/plat-s5p/include/plat/csis.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *	         http://www.samsung.com/
 *
 * S5PV210 - Platform header file for MIPI-CSI2 driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_PLAT_CSIS_H
#define __ASM_PLAT_CSIS_H __FILE__

#define to_csis_plat(d)         (to_platform_device(d)->dev.platform_data)

struct platform_device;

struct s3c_platform_csis {
	const char	srclk_name[16];
	const char	clk_name[16];
	unsigned long	clk_rate;

	void		(*cfg_gpio)(void);
	void		(*cfg_phy_global)(int on);
	int		(*clk_on)(struct platform_device *pdev, struct clk **clk);
	int		(*clk_off)(struct platform_device *pdev, struct clk **clk);

};

extern void s3c_csis_set_platdata(struct s3c_platform_csis *csis);
extern void s3c_csis_cfg_gpio(void);
extern void s3c_csis_cfg_phy_global(int on);
/* platform specific clock functions */
extern int s3c_csis_clk_on(struct platform_device *pdev, struct clk **clk);
extern int s3c_csis_clk_off(struct platform_device *pdev, struct clk **clk);

#endif /* __ASM_PLAT_CSIS_H */
