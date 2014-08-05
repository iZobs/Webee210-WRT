/* linux/drivers/media/video/samsung/ov965x.c
 *
 * Samsung ov965x CMOS Image Sensor driver
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
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/i2c-id.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/init.h>
#include <asm/io.h>

#include "s3c_fimc.h"
#include "ov965x.h"

/* I2C Device ID table */
static const struct i2c_device_id ov965x_id[] = {
	{ "ov965x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c,ov965x_id);



const static u16 ignore[] = { I2C_CLIENT_END };
static struct i2c_driver ov965x_i2c_driver;

static struct s3c_fimc_camera ov965x_data = {
	.id 		= CONFIG_VIDEO_FIMC_CAM_CH,
	.type		= CAM_TYPE_ITU,
	.mode		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_YCBYCR,//YCRYCB,
	.clockrate	= 24000000,
	.width		= 640,//800,
	.height		= 480,//600,
	.offset		= {
		.h1 = 0,
		.h2 = 0,
		.v1 = 0,
		.v2 = 0,
	},

	.polarity	= {
		.pclk	= 0,
		.vsync	= 1,
		.href	= 0,
		.hsync	= 0,
	},

	.initialized	= 0,
};





static int ov965x_probe(struct i2c_client *c, const struct i2c_device_id *id)
{
	int i;
	int ret;

	ov965x_data.client = c;
	s3c_fimc_register_camera(&ov965x_data);

	for (i = 0; i < OV965X_INIT_REGS; i++) {
		ret = i2c_smbus_write_byte_data(c, OV965X_init_reg[i].subaddr, OV965X_init_reg[i].value);
	}


	return 0;
}
static int ov965x_remove(struct i2c_client *c)
{
	return 0;
}

static struct i2c_driver ov965x_i2c_driver = {
	.driver = {
		.name		= "ov965x",
		.owner		= THIS_MODULE,
	},
	.probe		= ov965x_probe,
	.remove		= ov965x_remove,
	.id_table	= ov965x_id,
};


static __init int ov965x_init(void)
{
	info("info ov965x_init\n");
	return i2c_add_driver(&ov965x_i2c_driver);
}

static __init void ov965x_exit(void)
{
	i2c_del_driver(&ov965x_i2c_driver);
}

module_init(ov965x_init)
module_exit(ov965x_exit)

MODULE_AUTHOR("Jinsung, Yang <jsgood.yang@samsung.com>");
MODULE_DESCRIPTION("Samsung ov965x I2C based CMOS Image Sensor driver");
MODULE_LICENSE("GPL");

