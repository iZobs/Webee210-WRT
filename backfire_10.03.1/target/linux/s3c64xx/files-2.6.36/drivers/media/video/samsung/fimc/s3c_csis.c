/* linux/drivers/media/video/samsung/s3c_csis.c
 *
 * MIPI-CSI2 Support file for FIMC driver
 *
 * Jinsung Yang, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <asm/memory.h>
#include <plat/clock.h>
#include <plat/regs-csis.h>
#include <plat/csis.h>

#include "s3c_csis.h"

static struct s3c_csis_info *s3c_csis;

static struct s3c_platform_csis *to_csis_plat(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return (struct s3c_platform_csis *) pdev->dev.platform_data;
}

static int s3c_csis_set_info(void)
{
	s3c_csis = (struct s3c_csis_info *) \
			kmalloc(sizeof(struct s3c_csis_info), GFP_KERNEL);
	if (!s3c_csis) {
		err("no memory for configuration\n");
		return -ENOMEM;
	}

	strcpy(s3c_csis->name, S3C_CSIS_NAME);
	s3c_csis->nr_lanes = S3C_CSIS_NR_LANES;

	return 0;
}

static void s3c_csis_reset(void)
{
	u32 cfg;

	cfg = readl(s3c_csis->regs + S3C_CSIS_CONTROL);
	cfg |= S3C_CSIS_CONTROL_RESET;
	writel(cfg, s3c_csis->regs + S3C_CSIS_CONTROL);
}

static void s3c_csis_set_nr_lanes(int lanes)
{
	u32 cfg;

	cfg = readl(s3c_csis->regs + S3C_CSIS_CONFIG);
	cfg &= ~S3C_CSIS_CONFIG_NR_LANE_MASK;

	if (lanes == 1)
		cfg |= S3C_CSIS_CONFIG_NR_LANE_1;
	else
		cfg |= S3C_CSIS_CONFIG_NR_LANE_2;

	writel(cfg, s3c_csis->regs + S3C_CSIS_CONFIG);
}

static void s3c_csis_enable_interrupt(void)
{
	u32 cfg = 0;

	/* enable all interrupts */
	cfg |= S3C_CSIS_INTMSK_EVEN_BEFORE_ENABLE | \
		S3C_CSIS_INTMSK_EVEN_AFTER_ENABLE | \
		S3C_CSIS_INTMSK_ODD_BEFORE_ENABLE | \
		S3C_CSIS_INTMSK_ODD_AFTER_ENABLE | \
		S3C_CSIS_INTMSK_ERR_SOT_HS_ENABLE | \
		S3C_CSIS_INTMSK_ERR_ESC_ENABLE | \
		S3C_CSIS_INTMSK_ERR_CTRL_ENABLE | \
		S3C_CSIS_INTMSK_ERR_ECC_ENABLE | \
		S3C_CSIS_INTMSK_ERR_CRC_ENABLE | \
		S3C_CSIS_INTMSK_ERR_ID_ENABLE;

	writel(cfg, s3c_csis->regs + S3C_CSIS_INTMSK);
}

static void s3c_csis_disable_interrupt(void)
{
	/* disable all interrupts */
	writel(0, s3c_csis->regs + S3C_CSIS_INTMSK);
}

static void s3c_csis_system_on(void)
{
	u32 cfg;

	cfg = readl(s3c_csis->regs + S3C_CSIS_CONTROL);
	cfg |= S3C_CSIS_CONTROL_ENABLE;
	writel(cfg, s3c_csis->regs + S3C_CSIS_CONTROL);
}

static void s3c_csis_system_off(void)
{
	u32 cfg;

	cfg = readl(s3c_csis->regs + S3C_CSIS_CONTROL);
	cfg &= ~S3C_CSIS_CONTROL_ENABLE;
	writel(cfg, s3c_csis->regs + S3C_CSIS_CONTROL);
}

static void s3c_csis_phy_on(void)
{
	u32 cfg;

	cfg = readl(s3c_csis->regs + S3C_CSIS_DPHYCTRL);
	cfg |= S3C_CSIS_DPHYCTRL_ENABLE;
	writel(cfg, s3c_csis->regs + S3C_CSIS_DPHYCTRL);
}

static void s3c_csis_phy_off(void)
{
	u32 cfg;

	cfg = readl(s3c_csis->regs + S3C_CSIS_DPHYCTRL);
	cfg &= ~S3C_CSIS_DPHYCTRL_ENABLE;
	writel(cfg, s3c_csis->regs + S3C_CSIS_DPHYCTRL);
}

static void s3c_csis_start(struct platform_device *pdev)
{
	struct s3c_platform_csis *plat;

	plat = to_csis_plat(&pdev->dev);
	if (plat->cfg_phy_global)
		plat->cfg_phy_global(pdev, 1);

	s3c_csis_reset();
	s3c_csis_set_nr_lanes(S3C_CSIS_NR_LANES);
	s3c_csis_enable_interrupt();
	s3c_csis_system_on();
	s3c_csis_phy_on();
}

static void s3c_csis_stop(struct platform_device *pdev)
{
	struct s3c_platform_csis *plat;

	s3c_csis_disable_interrupt();
	s3c_csis_system_off();
	s3c_csis_phy_off();

	plat = to_csis_plat(&pdev->dev);
	if (plat->cfg_phy_global)
		plat->cfg_phy_global(pdev, 0);
}

static irqreturn_t s3c_csis_irq(int irq, void *dev_id)
{
	u32 cfg;

	/* just clearing the pends */
	cfg = readl(s3c_csis->regs + S3C_CSIS_INTSRC);
	writel(cfg, s3c_csis->regs + S3C_CSIS_INTSRC);

	return IRQ_HANDLED;
}

static int s3c_csis_probe(struct platform_device *pdev)
{
	struct s3c_platform_csis *pdata;
	struct resource *res;

	s3c_csis_set_info();

	pdata = to_csis_plat(&pdev->dev);
	if (pdata->cfg_gpio)
		pdata->cfg_gpio(pdev);

	s3c_csis->clock = clk_get(&pdev->dev, pdata->clk_name);
	if (IS_ERR(s3c_csis->clock)) {
		err("failed to get csis clock source\n");
		return -EINVAL;
	}

	clk_enable(s3c_csis->clock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		err("failed to get io memory region\n");
		return -EINVAL;
	}

	res = request_mem_region(res->start, res->end - res->start + 1, pdev->name);
	if (!res) {
		err("failed to request io memory region\n");
		return -EINVAL;
	}

	/* ioremap for register block */
	s3c_csis->regs = ioremap(res->start, res->end - res->start + 1);
	if (!s3c_csis->regs) {
		err("failed to remap io region\n");
		return -EINVAL;
	}

	/* irq */
	s3c_csis->irq = platform_get_irq(pdev, 0);
	if (request_irq(s3c_csis->irq, s3c_csis_irq, IRQF_DISABLED, \
		s3c_csis->name, s3c_csis))
		err("request_irq failed\n");

	info("Samsung MIPI-CSI2 driver probed successfully\n");

	s3c_csis_start(pdev);
	info("Samsung MIPI-CSI2 operation started\n");
	
	return 0;	
}

static int s3c_csis_remove(struct platform_device *pdev)
{
	s3c_csis_stop(pdev);
	kfree(s3c_csis);

	return 0;
}

int s3c_csis_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

int s3c_csis_resume(struct platform_device *dev)
{
	return 0;
}

static struct platform_driver s3c_csis_driver = {
	.probe		= s3c_csis_probe,
	.remove		= s3c_csis_remove,
	.suspend	= s3c_csis_suspend,
	.resume		= s3c_csis_resume,
	.driver		= {
		.name	= "s3c-csis",
		.owner	= THIS_MODULE,
	},
};

static int s3c_csis_register(void)
{
	platform_driver_register(&s3c_csis_driver);

	return 0;
}

static void s3c_csis_unregister(void)
{
	platform_driver_unregister(&s3c_csis_driver);
}

module_init(s3c_csis_register);
module_exit(s3c_csis_unregister);
	
MODULE_AUTHOR("Jinsung, Yang <jsgood.yang@samsung.com>");
MODULE_DESCRIPTION("MIPI-CSI2 support for FIMC driver");
MODULE_LICENSE("GPL");
