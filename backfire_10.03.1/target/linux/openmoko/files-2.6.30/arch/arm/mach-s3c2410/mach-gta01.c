/*
 * linux/arch/arm/mach-s3c2410/mach-gta01.c
 *
 * S3C2410 Machine Support for the FIC Neo1973 GTA01
 *
 * Copyright (C) 2006-2007 by Openmoko, Inc.
 * Author: Harald Welte <laforge@openmoko.org>
 * Copyright (C) 2009, Lars-Peter Clausen <lars@metafoo.de>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/serial_core.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>

#include <linux/mmc/host.h>

#include <linux/mfd/pcf50606/core.h>
#include <linux/mfd/pcf50606/pmic.h>
#include <linux/mfd/pcf50606/mbc.h>
#include <linux/mfd/pcf50606/adc.h>

#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>


#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/hardware.h>
#include <mach/io.h>
#include <asm/mach-types.h>

#include <mach/regs-gpio.h>
#include <mach/gpio-fns.h>
#include <mach/fb.h>

#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>

#include <mach/gta01.h>

#include <plat/regs-serial.h>
#include <plat/nand.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/pm.h>
#include <plat/udc.h>
#include <plat/iic.h>
#include <plat/mci.h>
#include <plat/usb-control.h>
/*#include <mach/neo1973-pm-gsm.h>*/

#include <linux/jbt6k74.h>

#include <linux/input.h>
#include <linux/gpio_keys.h>

#include <linux/pwm_backlight.h>
#include <linux/leds_pwm.h>

#include <linux/gpio.h>

#include <mach/ts.h>
#include <linux/touchscreen/ts_filter_chain.h>
#ifdef CONFIG_TOUCHSCREEN_FILTER
#include <linux/touchscreen/ts_filter_linear.h>
#include <linux/touchscreen/ts_filter_mean.h>
#include <linux/touchscreen/ts_filter_median.h>
#include <linux/touchscreen/ts_filter_group.h>
#endif

static void gta01_pmu_attach_child_devices(struct pcf50606 *pcf);

static struct map_desc gta01_iodesc[] __initdata = {
	{
		.virtual	= 0xe0000000,
		.pfn		= __phys_to_pfn(S3C2410_CS3+0x01000000),
		.length		= SZ_1M,
		.type		= MT_DEVICE
	},
};

#define UCON S3C2410_UCON_DEFAULT
#define ULCON S3C2410_LCON_CS8 | S3C2410_LCON_PNONE | S3C2410_LCON_STOPB
#define UFCON S3C2410_UFCON_RXTRIG8 | S3C2410_UFCON_FIFOMODE
/* UFCON for the gta01 sets the FIFO trigger level at 4, not 8 */
#define UFCON_GTA01_PORT0 S3C2410_UFCON_FIFOMODE

static struct s3c2410_uartcfg gta01_uartcfgs[] = {
	[0] = {
		.hwport	= 0,
		.flags	= 0,
		.ucon	= UCON,
		.ulcon	= ULCON,
		.ufcon	= UFCON_GTA01_PORT0,
	},
	[1] = {
		.hwport	= 1,
		.flags	= 0,
		.ucon	= UCON,
		.ulcon	= ULCON,
		.ufcon	= UFCON,
	},
};

/* TODO */
static void gta01_pmu_event_callback(struct pcf50606 *pcf, int irq)
{
	/*TODO : Handle ACD here */
}
#if 0
/* FIXME : Goes away when ACD is handled above */
static int pmu_callback(struct device *dev, unsigned int feature,
			enum pmu_event event)
{
	switch (feature) {
	case PCF50606_FEAT_ACD:
		switch (event) {
		case PMU_EVT_INSERT:
			pcf50606_charge_fast(pcf50606_global, 1);
			break;
		case PMU_EVT_REMOVE:
			pcf50606_charge_fast(pcf50606_global, 0);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return 0;
}
#endif
struct pcf50606 *gta01_pcf;

static struct platform_device gta01_pm_gsm_dev = {
	.name		= "neo1973-pm-gsm",
};

static struct platform_device gta01_pm_bt_dev = {
	.name		= "neo1973-pm-bt",
};
static struct platform_device gta01_pm_gps_dev = {
	.name		= "neo1973-pm-gps",
};

static struct regulator_consumer_supply ioreg_consumers[] = {
	{
		.dev = &gta01_pm_gps_dev.dev,
		.supply = "GPS_2V8",
	},
};

static struct regulator_consumer_supply d1reg_consumers[] = {
	{
		.dev = &gta01_pm_gps_dev.dev,
		.supply = "GPS_3V",
	},
	{
		.dev = &gta01_pm_bt_dev.dev,
		.supply = "BT_3V1",
	},
};

static struct regulator_consumer_supply dcd_consumers[] = {
	{
		.dev = &gta01_pm_gps_dev.dev,
		.supply = "GPS_3V3",
	},
	{
		.dev = &gta01_pm_gps_dev.dev,
		.supply = "GPS_1V5",
	},
};

static struct regulator_consumer_supply d2reg_consumers[] = {
	{
		.dev = &gta01_pm_gps_dev.dev,
		.supply = "GPS_2V5",
	},
	{
		.dev = &s3c_device_sdi.dev,
		.supply = "SD_3V3",
	},
};

#if 0
/* This will come with 2.6.32. Don't forget to uncomment it then. */
static struct regulator_consumer_supply lpreg_consumers[] = {
	REGULATOR_SUPPLY("VDC", "jbt6k74"),
	REGULATOR_SUPPLY("VDDIO", "jbt6k74"),
};
#else
static struct regulator_consumer_supply lpreg_consumers[] = {
	{ .supply = "VDC", },
	{ .supply = "VDDIO", },
};
#endif

#if 0

static int gta01_bat_get_charging_status(void)
{
	struct pcf50606 *pcf = gta01_pcf;
	u8 mbcc1, chgmod;

	mbcc1 = pcf50606_reg_read(pcf, PCF50606_REG_MBCC1);
	chgmod = mbcc1 & PCF50606_MBCC1_CHGMOD_MASK;

	if (chgmod == PCF50606_MBCC1_CHGMOD_IDLE)
		return 0;
	else
		return 1;
}

static int gta01_bat_get_voltage(void)
{
	struct pcf50606 *pcf = gta01_pcf;
	u16 adc, mv = 0;

	adc = pcf50606_adc_sync_read(pcf, PCF50606_ADCMUX_BATVOLT_RES);
	mv = (adc * 6000) / 1024;

	return mv * 1000;
}

static int gta01_bat_get_current(void)
{
	struct pcf50606 *pcf = gta01_pcf;
	u16 adc_battvolt, adc_adcin1;
	s32 res;

	adc_battvolt = pcf50606_adc_sync_read(pcf, PCF50606_ADCMUX_BATVOLT_SUBTR);
	adc_adcin1 = pcf50606_adc_sync_read(pcf, PCF50606_ADCMUX_ADCIN1_SUBTR);
	res = (adc_battvolt - adc_adcin1) * 2400;

	/*rsense is 220 milli */
	return (res * 1000) / (220 * 1024) * 1000;
}

static struct gta01_bat_platform_data gta01_bat_pdata = {
	.get_charging_status = gta01_bat_get_charging_status,
	.get_voltage = gta01_bat_get_voltage,
	.get_current = gta01_bat_get_current,
};

static struct platform_device gta01_bat = {
	.name = "gta01_battery",
	.id = -1,
	.dev = {
		.platform_data = &gta01_bat_pdata,
	}
};
#endif

static void __devinit gta01_pcf_probe_done(struct pcf50606 *pcf)
{
	gta01_pcf = pcf;

	gta01_pmu_attach_child_devices(pcf);
}

static int gps_registered_regulators = 0;

static void gta01_pmu_regulator_registered(struct pcf50606 *pcf, int id)
{
	switch(id) {
		case PCF50606_REGULATOR_D1REG:
			platform_device_register(&gta01_pm_bt_dev);
			gps_registered_regulators++;
			break;
		case PCF50606_REGULATOR_D2REG:
			gps_registered_regulators++;
			break;
		case PCF50606_REGULATOR_IOREG:
		case PCF50606_REGULATOR_DCD:
			gps_registered_regulators++;
			break;
	}

	/* All GPS related regulators registered ? */
	if (gps_registered_regulators == 4)
		platform_device_register(&gta01_pm_gps_dev);
}

static struct pcf50606_platform_data gta01_pcf_pdata = {
	.resumers = {
			[0] = 	PCF50606_INT1_ALARM |
				PCF50606_INT1_ONKEYF |
				PCF50606_INT1_EXTONR,
			[1] = 	PCF50606_INT2_CHGWD10S |
				PCF50606_INT2_CHGPROT |
				PCF50606_INT2_CHGERR,
			[2] =	PCF50606_INT3_LOWBAT |
				PCF50606_INT3_HIGHTMP |
				PCF50606_INT3_ACDINS,
	},
	.mbc_event_callback = gta01_pmu_event_callback,
	.reg_init_data = {
		/* BT, GPS */
		[PCF50606_REGULATOR_D1REG] = {
			.constraints = {
				.min_uV = 3000000,
				.max_uV = 3150000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.apply_uV = 1,
			},
			.num_consumer_supplies = ARRAY_SIZE(d1reg_consumers),
			.consumer_supplies = d1reg_consumers,
		},
		/* GPS */
		[PCF50606_REGULATOR_D2REG] = {
			.constraints = {
				.min_uV = 1650000,
				.max_uV = 3300000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
				.apply_uV = 1,
			},
			.num_consumer_supplies = ARRAY_SIZE(d2reg_consumers),
			.consumer_supplies = d2reg_consumers,

		},
		/* RTC/Standby */
		[PCF50606_REGULATOR_D3REG] = {
			.constraints = {
				.min_uV = 1800000,
				.max_uV = 2100000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.always_on = 1,
				.state_mem = {
					.enabled = 1,
				},
			},
			.num_consumer_supplies = 0,
		},
		/* GPS */
		[PCF50606_REGULATOR_DCD] = {
			.constraints = {
				.min_uV = 1500000,
				.max_uV = 1500000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.apply_uV = 1,
			},
			.num_consumer_supplies = ARRAY_SIZE(dcd_consumers),
			.consumer_supplies = dcd_consumers,
		},

		/* S3C2410 Memory and IO, Vibrator, RAM, NAND, AMP, SD Card */
		[PCF50606_REGULATOR_DCDE] = {
			.constraints = {
				.min_uV = 3300000,
				.max_uV = 3300000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.apply_uV = 1,
				.always_on = 1,
				.state_mem = {
					.enabled = 1,
				},
			},
			.num_consumer_supplies = 0,
		},
		/* SoC */
		[PCF50606_REGULATOR_DCUD] = {
			.constraints = {
				.min_uV = 2100000,
				.max_uV = 2100000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.apply_uV = 1,
				.always_on = 1,
				.state_mem = {
					.enabled = 1,
				},
			},
			.num_consumer_supplies = 0,
		},
		/* Codec, GPS */
		[PCF50606_REGULATOR_IOREG] = {
			.constraints = {
				.min_uV = 3300000,
				.max_uV = 3300000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.apply_uV = 1,
				.always_on = 1,
			},
			.num_consumer_supplies = ARRAY_SIZE(ioreg_consumers),
			.consumer_supplies = ioreg_consumers,

		},
		/* LCM */
		[PCF50606_REGULATOR_LPREG] = {
			.constraints = {
				.min_uV = 3300000,
				.max_uV = 3300000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.apply_uV = 1,
			},
			.num_consumer_supplies = ARRAY_SIZE(lpreg_consumers),
			.consumer_supplies = lpreg_consumers,
		},
	},
	.probe_done = gta01_pcf_probe_done,
	.regulator_registered = gta01_pmu_regulator_registered,
};

static void mangle_pmu_pdata_by_system_rev(void)
{
	struct regulator_init_data *reg_init_data;

	reg_init_data = gta01_pcf_pdata.reg_init_data;

	switch (S3C_SYSTEM_REV_ATAG) {
	case GTA01Bv4_SYSTEM_REV:
		/* FIXME : gta01_pcf_pdata.used_features |= PCF50606_FEAT_ACD; */
		break;
	case GTA01Bv3_SYSTEM_REV:
	case GTA01Bv2_SYSTEM_REV:
		reg_init_data[PCF50606_REGULATOR_D3REG].constraints.state_mem.enabled = 1;
		break;
	default:
		break;
	}
}

static void gta01_power_off(void)
{
	pcf50606_reg_write(gta01_pcf, PCF50606_REG_OOCC1,
			PCF50606_OOCC1_GOSTDBY);
}

/* LCD driver info */

/* Configuration for 480x640 toppoly TD028TTEC1.
 * Do not mark this as __initdata or it will break! */
static struct s3c2410fb_display gta01_displays[] =  {
	{
		.type		= S3C2410_LCDCON1_TFT,
		.width		= 43,
		.height		= 58,
		.xres		= 480,
		.yres		= 640,
		.bpp		= 16,

		.pixclock	= 40000,	/* HCLK/4 */
		.left_margin	= 104,
		.right_margin	= 8,
		.hsync_len	= 8,
		.upper_margin	= 2,
		.lower_margin	= 16,
		.vsync_len	= 2,
		.lcdcon5	= S3C2410_LCDCON5_FRM565 |
				  S3C2410_LCDCON5_INVVCLK |
				  S3C2410_LCDCON5_INVVLINE |
				  S3C2410_LCDCON5_INVVFRAME |
				  S3C2410_LCDCON5_PWREN |
				  S3C2410_LCDCON5_HWSWP,
	},
	{
		.type		= S3C2410_LCDCON1_TFT,
		.width		= 43,
		.height		= 58,
		.xres		= 480,
		.yres		= 640,
		.bpp		= 32,

		.pixclock	= 40000,	/* HCLK/4 */
		.left_margin	= 104,
		.right_margin	= 8,
		.hsync_len	= 8,
		.upper_margin	= 2,
		.lower_margin	= 16,
		.vsync_len	= 2,
		.lcdcon5	= S3C2410_LCDCON5_FRM565 |
				  S3C2410_LCDCON5_INVVCLK |
				  S3C2410_LCDCON5_INVVLINE |
				  S3C2410_LCDCON5_INVVFRAME |
				  S3C2410_LCDCON5_PWREN |
				  S3C2410_LCDCON5_HWSWP,
	},
	{
		.type		= S3C2410_LCDCON1_TFT,
		.width		= 43,
		.height		= 58,
		.xres		= 240,
		.yres		= 320,
		.bpp		= 16,

		.pixclock	= 40000,	/* HCLK/4 */
		.left_margin	= 104,
		.right_margin	= 8,
		.hsync_len	= 8,
		.upper_margin	= 2,
		.lower_margin	= 16,
		.vsync_len	= 2,
		.lcdcon5	= S3C2410_LCDCON5_FRM565 |
				  S3C2410_LCDCON5_INVVCLK |
				  S3C2410_LCDCON5_INVVLINE |
				  S3C2410_LCDCON5_INVVFRAME |
				  S3C2410_LCDCON5_PWREN |
				  S3C2410_LCDCON5_HWSWP,
	},
};

static struct s3c2410fb_mach_info gta01_lcd_cfg __initdata = {
	.displays	= gta01_displays,
	.num_displays	= ARRAY_SIZE(gta01_displays),
	.default_display = 0,

	.lpcsel		= ((0xCE6) & ~7) | 1<<4,
};

static struct s3c2410_nand_set gta01_nand_sets[] = {
	[0] = {
		.name		= "neo1973-nand",
		.nr_chips	= 1,
/*		.flags		= S3C2410_NAND_BBT,*/
	},
};

static struct s3c2410_platform_nand gta01_nand_info = {
	.tacls		= 20,
	.twrph0		= 60,
	.twrph1		= 20,
	.nr_sets	= ARRAY_SIZE(gta01_nand_sets),
	.sets		= gta01_nand_sets,
};

/* MMC */

static void gta01_mmc_set_power(unsigned char power_mode, unsigned short vdd)
{
	printk(KERN_DEBUG "mmc_set_power(power_mode=%u, vdd=%u)\n",
		   power_mode, vdd);

	switch (power_mode) {
	case MMC_POWER_OFF:
			gpio_set_value(GTA01_GPIO_SDMMC_ON, 1);
			break;
	case MMC_POWER_UP:
			gpio_set_value(GTA01_GPIO_SDMMC_ON, 0);
			break;
	}
}

static struct s3c24xx_mci_pdata gta01_mmc_cfg = {
	.gpio_detect	= GTA01_GPIO_nSD_DETECT,
	.set_power	= &gta01_mmc_set_power,
	.ocr_avail	= MMC_VDD_32_33,
};

/* UDC */

static void gta01_udc_command(enum s3c2410_udc_cmd_e cmd)
{
	printk("udc command: %d\n", cmd);
	switch (cmd) {
	case S3C2410_UDC_P_ENABLE:
		gpio_set_value(GTA01_GPIO_USB_PULLUP, 1);
		break;
	case S3C2410_UDC_P_DISABLE:
		gpio_set_value(GTA01_GPIO_USB_PULLUP, 0);
		break;
	default:
		break;
	}
}

/* use a work queue, since I2C API inherently schedules
 * and we get called in hardirq context from UDC driver */

struct vbus_draw {
	struct work_struct work;
	int ma;
};
static struct vbus_draw gta01_udc_vbus_drawer;

static void __gta01_udc_vbus_draw(struct work_struct *work)
{
	/*
	 * This is a fix to work around boot-time ordering problems if the
	 * s3c2410_udc is initialized before the pcf50606 driver has defined
	 * pcf50606_global
	 */
	if (!gta01_pcf)
		return;
#if 0
	if (gta01_udc_vbus_drawer.ma >= 500) {
		/* enable fast charge */
		printk(KERN_DEBUG "udc: enabling fast charge\n");
		pcf50606_charge_fast(gta01_pcf, 1);
	} else {
		/* disable fast charge */
		printk(KERN_DEBUG "udc: disabling fast charge\n");
		pcf50606_charge_fast(gta01_pcf, 0);
	}
#endif
}

static void gta01_udc_vbus_draw(unsigned int ma)
{
	gta01_udc_vbus_drawer.ma = ma;
	schedule_work(&gta01_udc_vbus_drawer.work);
}

static struct s3c2410_udc_mach_info gta01_udc_cfg = {
	.vbus_draw		= gta01_udc_vbus_draw,
	.udc_command	= gta01_udc_command,
};

/* Touchscreen configuration. */


#ifdef CONFIG_TOUCHSCREEN_FILTER
const static struct ts_filter_group_configuration gta01_ts_group = {
	.length = 12,
	.close_enough = 10,
	.threshold = 6,		/* At least half of the points in a group. */
	.attempts = 10,
};

const static struct ts_filter_median_configuration gta01_ts_median = {
	.extent = 20,
	.decimation_below = 3,
	.decimation_threshold = 8 * 3,
	.decimation_above = 4,
};

const static struct ts_filter_mean_configuration gta01_ts_mean = {
	.length = 4,
};

const static struct ts_filter_linear_configuration gta01_ts_linear = {
	.constants = {1, 0, 0, 0, 1, 0, 1},	/* Don't modify coords. */
	.coord0 = 0,
	.coord1 = 1,
};
#endif

const static struct ts_filter_chain_configuration gta01_filter_configuration[] =
{
#ifdef CONFIG_TOUCHSCREEN_FILTER
	{&ts_filter_group_api,		&gta01_ts_group.config},
	{&ts_filter_median_api,		&gta01_ts_median.config},
	{&ts_filter_mean_api,		&gta01_ts_mean.config},
	{&ts_filter_linear_api,		&gta01_ts_linear.config},
#endif
	{NULL, NULL},
};

const static struct s3c2410_ts_mach_info gta01_ts_cfg = {
	.delay = 10000,
	.presc = 0xff, /* slow as we can go */
	.filter_config = gta01_filter_configuration,
};

/* SPI / Display */
static void gta01_jbt6k74_reset(int devidx, int level)
{
	gpio_set_value(GTA01_GPIO_LCD_RESET, level);
}

const struct jbt6k74_platform_data gta01_jbt6k74_pdata = {
	.reset = gta01_jbt6k74_reset,
};

static struct spi_board_info gta01_spi_board_info[] = {
	{
		.modalias	= "jbt6k74",
		.platform_data	= &gta01_jbt6k74_pdata,
		.controller_data = (void*)S3C2410_GPG3,
		.max_speed_hz	= 10 * 1000 * 1000,
		.bus_num = 1,
	},
};

static struct spi_gpio_platform_data spigpio_platform_data = {
	.sck = S3C2410_GPG7,
	.mosi = S3C2410_GPG6,
	.miso = S3C2410_GPG5,
	.num_chipselect = 1,
};

static struct platform_device gta01_lcm_spigpio_device = {
	.name = "spi_gpio",
	.id   = 1,
	.dev = {
		.platform_data = &spigpio_platform_data,
	},
};

/* Backlight */

static struct platform_pwm_backlight_data gta01_bl_pdata = {
	.max_brightness = 0xff,
	.dft_brightness = 0xff,
	.pwm_period_ns = 5000000, /* TODO: Tweak this */
	.pwm_id = 0,
};

static struct platform_device gta01_bl_device = {
	.name 		= "pwm-backlight",
	.id		= -1,
	.dev		= {
		.platform_data = &gta01_bl_pdata,
		.parent = &s3c_device_timer[0].dev,
	},
};

/* Vibrator */
static struct led_pwm gta01_vibrator = {
	.name = "gta01::vibrator",
	.max_brightness = 0xff,
	.pwm_period_ns = 500000, /* TODO: Tweak this */
	.pwm_id = 3,
};

static struct led_pwm_platform_data gta01_vibrator_pdata = {
	.num_leds = 1,
	.leds = &gta01_vibrator,
};

static struct platform_device gta01_vibrator_device_bv4 = {
	.name	= "leds_pwm",
	.id	= -1,
	.dev = {
		.platform_data = &gta01_vibrator_pdata,
		.parent = &s3c_device_timer[3].dev,
	}
};

static struct gpio_led gta01_vibrator_bv2 = {
	.name = "gta01::vibrator",
	.gpio = GTA01Bv2_GPIO_VIBRATOR_ON,
};

static struct gpio_led_platform_data gta01_vibrator_pdata_bv2 = {
	.num_leds = 1,
	.leds = &gta01_vibrator_bv2,
};

static struct platform_device gta01_vibrator_device_bv2 = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev = {
		.platform_data = &gta01_vibrator_pdata_bv2,
	}
};

static struct platform_device *gta01_vibrator_device = &gta01_vibrator_device_bv4;
/* Buttons */

static struct gpio_keys_button gta01_buttons[] = {
	{
		.gpio = GTA01_GPIO_AUX_KEY,
		.code = KEY_PHONE,
		.desc = "Aux",
		.type = EV_KEY,
		.debounce_interval = 100,
	},
	{
		.gpio = GTA01_GPIO_HOLD_KEY,
		.code = KEY_PAUSE,
		.desc = "Hold",
		.type = EV_KEY,
		.debounce_interval = 100,
	},
};

static struct gpio_keys_platform_data gta01_buttons_pdata = {
	.buttons = gta01_buttons,
	.nbuttons = ARRAY_SIZE(gta01_buttons),
};

static struct platform_device gta01_buttons_device = {
	.name = "gpio-keys",
	.id = -1,
	.dev = {
		.platform_data = &gta01_buttons_pdata,
	},
};

/* USB */

static struct s3c2410_hcd_info gta01_usb_info = {
	.port[0]	= {
		.flags	= S3C_HCDFLG_USED,
	},
	.port[1]	= {
		.flags	= 0,
	},
};

static struct platform_device *gta01_devices[] __initdata = {
	&s3c_device_usb,
	&s3c_device_lcd,
	&s3c_device_wdt,
	&s3c_device_i2c0,
	&s3c_device_iis,
	&s3c_device_sdi,
	&s3c_device_usbgadget,
	&s3c_device_nand,
	&s3c_device_adc,
	&s3c_device_ts,
	&s3c_device_timer[0],
	&s3c_device_timer[3],
	&gta01_bl_device,
	&gta01_buttons_device,
	&gta01_pm_gsm_dev,
};

static struct platform_device *gta01_pmu_child_devices[] __devinitdata = {
	&gta01_lcm_spigpio_device,
};

static void __devinit gta01_pmu_attach_child_devices(struct pcf50606 *pcf)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(gta01_pmu_child_devices); ++i)
		gta01_pmu_child_devices[i]->dev.parent = pcf->dev;

	platform_add_devices(gta01_pmu_child_devices,
				 ARRAY_SIZE(gta01_pmu_child_devices));
}
static void __init gta01_map_io(void)
{
	s3c24xx_init_io(gta01_iodesc, ARRAY_SIZE(gta01_iodesc));
	s3c24xx_init_clocks(12*1000*1000);
	s3c24xx_init_uarts(gta01_uartcfgs, ARRAY_SIZE(gta01_uartcfgs));
}

static irqreturn_t gta01_modem_irq(int irq, void *param)
{
	printk(KERN_DEBUG "GSM wakeup interrupt (IRQ %d)\n", irq);
/*	gta_gsm_interrupts++;*/
	return IRQ_HANDLED;
}

static struct i2c_board_info gta01_i2c_devs[] __initdata = {
	{
		I2C_BOARD_INFO("pcf50606", 0x08),
		.irq = GTA01Bv4_IRQ_PCF50606,
		.platform_data = &gta01_pcf_pdata,
	},
	{
		I2C_BOARD_INFO("lm4857", 0x7c),
	},
	{
		I2C_BOARD_INFO("wm8753", 0x1a),
	},
};

static int __init gta01_init_gpio(void)
{
	int ret;

	ret = gpio_request(GTA01_GPIO_USB_PULLUP, "udc pullup");

	ret = gpio_direction_output(GTA01_GPIO_USB_PULLUP, 0);

	ret = gpio_request(GTA01_GPIO_SDMMC_ON, "SD/MMC power");

	ret = gpio_direction_output(GTA01_GPIO_SDMMC_ON, 1);

	s3c2410_gpio_cfgpin(GTA01_GPIO_BACKLIGHT, S3C2410_GPB0_TOUT0);
	if (S3C_SYSTEM_REV_ATAG == GTA01Bv4_SYSTEM_REV)
		s3c2410_gpio_cfgpin(GTA01Bv4_GPIO_VIBRATOR_ON, S3C2410_GPB3_TOUT3);

	return ret;
}

static void __init gta01_machine_init(void)
{
	int rc;

	gta01_init_gpio();

	if (S3C_SYSTEM_REV_ATAG != GTA01Bv4_SYSTEM_REV) {
		gta01_vibrator_device = &gta01_vibrator_device_bv2;
		gta01_i2c_devs[0].irq = GTA01Bv2_IRQ_PCF50606;
	}

	s3c_device_usb.dev.platform_data = &gta01_usb_info;
	s3c_device_nand.dev.platform_data = &gta01_nand_info;
	s3c_device_sdi.dev.platform_data = &gta01_mmc_cfg;

	s3c24xx_fb_set_platdata(&gta01_lcd_cfg);

	INIT_WORK(&gta01_udc_vbus_drawer.work, __gta01_udc_vbus_draw);
	s3c24xx_udc_set_platdata(&gta01_udc_cfg);
	s3c_i2c0_set_platdata(NULL);

	set_s3c2410ts_info(&gta01_ts_cfg);

#if 0
	switch (S3C_SYSTEM_REV_ATAG) {
	case GTA01v3_SYSTEM_REV:
	case GTA01v4_SYSTEM_REV:
		/* just use the default (GTA01_IRQ_PCF50606) */
		break;
	case GTA01Bv2_SYSTEM_REV:
	case GTA01Bv3_SYSTEM_REV:
		/* just use the default (GTA01_IRQ_PCF50606) */
		gta01_led_resources[0].start =
			gta01_led_resources[0].end = GTA01Bv2_GPIO_VIBRATOR_ON;
		break;
	case GTA01Bv4_SYSTEM_REV:
		gta01_i2c_devs[0].irq =	 GTA01Bv4_IRQ_PCF50606;
		gta01_led_resources[0].start =
			gta01_led_resources[0].end = GTA01Bv4_GPIO_VIBRATOR_ON;
		break;
	}
#endif
	mangle_pmu_pdata_by_system_rev();

	i2c_register_board_info(0, gta01_i2c_devs, ARRAY_SIZE(gta01_i2c_devs));
	spi_register_board_info(gta01_spi_board_info,
				ARRAY_SIZE(gta01_spi_board_info));

	platform_add_devices(gta01_devices, ARRAY_SIZE(gta01_devices));
/*	platform_add_devices(gta01_vibrator_device, 1);*/

	s3c_pm_init();

	set_irq_type(GTA01_IRQ_MODEM, IRQ_TYPE_EDGE_RISING);
	rc = request_irq(GTA01_IRQ_MODEM, gta01_modem_irq, IRQF_DISABLED,
			 "modem", NULL);
	enable_irq_wake(GTA01_IRQ_MODEM);

	printk(KERN_DEBUG  "Enabled GSM wakeup IRQ %d (rc=%d)\n",
		   GTA01_IRQ_MODEM, rc);

	pm_power_off = &gta01_power_off;
}

MACHINE_START(NEO1973_GTA01, "GTA01")
	.phys_io	= S3C2410_PA_UART,
	.io_pg_offst	= (((u32)S3C24XX_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S3C2410_SDRAM_PA + 0x100,
	.map_io		= gta01_map_io,
	.init_irq	= s3c24xx_init_irq,
	.init_machine	= gta01_machine_init,
	.timer		= &s3c24xx_timer,
MACHINE_END
