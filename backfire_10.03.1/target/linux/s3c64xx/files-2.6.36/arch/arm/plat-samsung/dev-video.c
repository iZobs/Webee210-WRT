/* linux/arch/arm/plat-samsung/dev-video.c
 *
 * S3C series device definition for multimedia device
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/platform_device.h>

#include <mach/irqs.h>
#include <mach/map.h>

#include <plat/devs.h>
#include <plat/cpu.h>

/* FIMG-2D controller */
static struct resource s3c_g2d_resource[] = {
	[0] = {
		.start		= S3C64XX_PA_G2D,
		.end		= S3C64XX_PA_G2D + S3C64XX_SZ_G2D - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_2D,
		.end		= IRQ_2D,
		.flags		= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_g2d = {
	.name			= "s3c-g2d",
	.id				= -1,
	.num_resources	= ARRAY_SIZE(s3c_g2d_resource),
	.resource		= s3c_g2d_resource
};
EXPORT_SYMBOL(s3c_device_g2d);

/* FIMG-3D controller */
static struct resource s3c_g3d_resource[] = {
	[0] = {
		.start		= S3C64XX_PA_G3D,
		.end		= S3C64XX_PA_G3D + S3C64XX_SZ_G3D - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_S3C6410_G3D,
		.end		= IRQ_S3C6410_G3D,
		.flags		= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_g3d = {
	.name			= "s3c-g3d",
	.id				= -1,
	.num_resources	= ARRAY_SIZE(s3c_g3d_resource),
	.resource		= s3c_g3d_resource
};
EXPORT_SYMBOL(s3c_device_g3d);

/* VPP controller */
static struct resource s3c_vpp_resource[] = {
	[0] = {
		.start		= S3C6400_PA_VPP,
		.end		= S3C6400_PA_VPP + S3C_SZ_VPP - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_POST0,
		.end		= IRQ_POST0,
		.flags		= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_vpp = {
	.name			= "s3c-vpp",
	.id				= -1,
	.num_resources	= ARRAY_SIZE(s3c_vpp_resource),
	.resource		= s3c_vpp_resource,
};
EXPORT_SYMBOL(s3c_device_vpp);

/* TV encoder */
static struct resource s3c_tvenc_resource[] = {
	[0] = {
		.start		= S3C6400_PA_TVENC,
		.end		= S3C6400_PA_TVENC + S3C_SZ_TVENC - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_TVENC,
		.end		= IRQ_TVENC,
		.flags		= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_tvenc = {
	.name			= "s3c-tvenc",
	.id				= -1,
	.num_resources	= ARRAY_SIZE(s3c_tvenc_resource),
	.resource		= s3c_tvenc_resource,
};

EXPORT_SYMBOL(s3c_device_tvenc);

/* MFC controller */
static struct resource s3c_mfc_resource[] = {
	[0] = {
		.start		= S3C6400_PA_MFC,
		.end		= S3C6400_PA_MFC + S3C_SZ_MFC - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_MFC,
		.end		= IRQ_MFC,
		.flags		= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_mfc = {
	.name			= "s3c-mfc",
	.id				= -1,
	.num_resources	= ARRAY_SIZE(s3c_mfc_resource),
	.resource		= s3c_mfc_resource
};
EXPORT_SYMBOL(s3c_device_mfc);

/* TV scaler */
static struct resource s3c_tvscaler_resource[] = {
	[0] = {
		.start		= S3C6400_PA_TVSCALER,
		.end		= S3C6400_PA_TVSCALER + S3C_SZ_TVSCALER - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_SCALER,
		.end		= IRQ_SCALER,
		.flags		= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_tvscaler = {
	.name			= "s3c-tvscaler",
	.id				= -1,
	.num_resources	= ARRAY_SIZE(s3c_tvscaler_resource),
	.resource		= s3c_tvscaler_resource,
};
EXPORT_SYMBOL(s3c_device_tvscaler);

/* rotator interface */
static struct resource s3c_rotator_resource[] = {
	[0] = {
		.start		= S3C6400_PA_ROTATOR,
		.end		= S3C6400_PA_ROTATOR + S3C_SZ_ROTATOR - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_ROTATOR,
		.end		= IRQ_ROTATOR,
		.flags		= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_rotator = {
	.name			= "s3c-rotator",
	.id				= -1,
	.num_resources	= ARRAY_SIZE(s3c_rotator_resource),
	.resource		= s3c_rotator_resource
};
EXPORT_SYMBOL(s3c_device_rotator);

/* JPEG controller */
static struct resource s3c_jpeg_resource[] = {
	[0] = {
		.start		= S3C6400_PA_JPEG,
		.end		= S3C6400_PA_JPEG + S3C_SZ_JPEG - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_JPEG,
		.end		= IRQ_JPEG,
		.flags		= IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_jpeg = {
	.name			= "s3c-jpeg",
	.id				= -1,
	.num_resources	= ARRAY_SIZE(s3c_jpeg_resource),
	.resource		= s3c_jpeg_resource,
};
EXPORT_SYMBOL(s3c_device_jpeg);

