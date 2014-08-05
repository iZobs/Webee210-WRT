/* linux/arch/arm/mach-s5pv210/pmu.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * S5PV210 - PMU IRQ registration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/platform_device.h>
#include <asm/pmu.h>
#include <mach/irqs.h>

static struct resource pmu_resource = {
	.start  = IRQ_PMU,
	.end    = IRQ_PMU,
	.flags  = IORESOURCE_IRQ,
};

static struct platform_device pmu_device = {
	.name		= "arm-pmu",
	.id		= ARM_PMU_DEVICE_CPU,
	.resource	= &pmu_resource,
	.num_resources	= 1,
};

static int __init s5pv210_pmu_init(void)
{
	platform_device_register(&pmu_device);
	return 0;
}
arch_initcall(s5pv210_pmu_init);
