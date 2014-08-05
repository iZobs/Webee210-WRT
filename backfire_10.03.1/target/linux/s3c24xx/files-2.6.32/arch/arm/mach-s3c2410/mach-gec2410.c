/* linux/arch/arm/mach-s3c2410/mach-gec2410.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * Copyright 2012. Lintel.huang@gmail.com
 *
 *
 */



#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/sysdev.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>
#include <linux/dm9000.h>
#include <linux/mmc/host.h>

#include <linux/gpio_keys.h>
#include <linux/input.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>
#include <asm/irq.h>
#include <asm/mach-types.h>

#include <mach/hardware.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <mach/regs-lcd.h>
#include <mach/leds-gpio.h>
#include <linux/leds.h>
#include <mach/idle.h>
#include <mach/fb.h>

#include <plat/regs-serial.h>
#include <plat/iic.h>
#include <plat/s3c2410.h>
#include <plat/s3c2440.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/nand.h>
#include <plat/pm.h>
#include <plat/mci.h>
#include <plat/lcd-config.h>

#include <plat/usb-control.h> //gec2410 usb ctl
#include <linux/delay.h>

#define CONFIG_FB_S3C2410_VGA1024768 1

#define NAND_FLASH_SIZE 0x000004000000
#include <sound/s3c24xx_uda134x.h>

static struct map_desc gec2410_iodesc[] __initdata = {
	{ 0xe0000000, __phys_to_pfn(S3C2410_CS3+0x01000000), SZ_1M, MT_DEVICE }
};

#define UCON S3C2410_UCON_DEFAULT | S3C2410_UCON_UCLK
#define ULCON S3C2410_LCON_CS8 | S3C2410_LCON_PNONE | S3C2410_LCON_STOPB
#define UFCON S3C2410_UFCON_RXTRIG8 | S3C2410_UFCON_FIFOMODE

static struct s3c2410_uartcfg gec2410_uartcfgs[] __initdata = {
	[0] = {
		.hwport	     = 0,
		.flags	     = 0,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x03,
		.ufcon	     = 0x51,
	},
	[1] = {
		.hwport	     = 1,
		.flags	     = 0,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x03,
		.ufcon	     = 0x51,
	},
	[2] = {
		.hwport	     = 2,
		.flags	     = 0,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x03,
		.ufcon	     = 0x51,
	}
};


static struct s3c24xx_uda134x_platform_data s3c24xx_uda134x_data = {
	.l3_clk = S3C2410_GPB(4),
	.l3_data = S3C2410_GPB(3),
	.l3_mode = S3C2410_GPB(2),
	.model = UDA134X_UDA1341,
};

static struct platform_device s3c24xx_uda134x = {
	.name = "s3c24xx_uda134x",
	.dev = {
		.platform_data    = &s3c24xx_uda134x_data,
	}
};

static struct mtd_partition gec2410_default_nand_part[] = {
	[0] = {
		.name	= "u-boot",
		.size	= SZ_256K+SZ_128K,
		.offset	= 0,
	},
	[1] = {
		.name	= "u-boot-env",
		.offset = SZ_256K+SZ_128K,
		.size	= SZ_128K,
	},
	[2] = {
		.name	= "kernel",
		.offset = ((SZ_256K+SZ_128K)+SZ_128K),
		.size	= (SZ_1M * 5),
	},
	[3] = {
		.name	= "rootfs",
		.offset = (((SZ_256K+SZ_128K)+SZ_128K)+ (SZ_1M * 5)),
		.size	= (SZ_1M * 50),
//		.size	= 0x2000000,
	},
	[4] = {
		.name	= "other-rootfs",
		.offset = MTDPART_OFS_APPEND,
		.size	= MTDPART_SIZ_FULL,
	},
};

static struct s3c2410_nand_set gec2410_nand_sets[] = {
	[0] = {
		.name		= "NAND",
		.nr_chips	= 1,
		.nr_partitions	= ARRAY_SIZE(gec2410_default_nand_part),
		.partitions	= gec2410_default_nand_part,
	},
};

/* choose a set of timings which should suit most 512Mbit
 * chips and beyond.
*/

static struct s3c2410_platform_nand gec2410_nand_info = {
	.tacls		= 20,
	.twrph0		= 60,
	.twrph1		= 20,
	.nr_sets	= ARRAY_SIZE(gec2410_nand_sets),
	.sets		= gec2410_nand_sets,
	.ignore_unset_ecc = 1,
};

/* CS89000A 10M ethernet controller */
#define GEC2410_CS89000A_BASE 0x40000000

static struct resource gec2410_cs89x0_resources[] = {
	[0] = {
		.start	= GEC2410_CS89000A_BASE,
		.end	= GEC2410_CS89000A_BASE + 16,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_EINT9,
		.end	= IRQ_EINT9,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device gec2410_cs89x0 = {
	.name		= "cirrus-cs89x0",
	.num_resources	= ARRAY_SIZE(gec2410_cs89x0_resources),
	.resource	= gec2410_cs89x0_resources,
};


/*
usb set unknow
*/
static struct s3c2410_hcd_info usb_gec2410_info = {
 	.port[0] = {
	.flags = S3C_HCDFLG_USED ,
	}
};
int usb_gec2410_init(void)
{
	unsigned long upllvalue = (0x78<<12)|(0x02<<4)|(0x03);
	printk("USB Control, (c) 2009 gec2410\n");
	s3c_device_usb.dev.platform_data = &usb_gec2410_info;
	while(upllvalue!=__raw_readl(S3C2410_UPLLCON))
	{
 		__raw_writel(upllvalue,S3C2410_UPLLCON);
	 	mdelay(1);
 	}
 	return 0;
}
/* MMC/SD  */
static struct s3c24xx_mci_pdata gec2410_mmc_cfg = {
   .gpio_detect   = S3C2410_GPG(10),
   .gpio_wprotect = S3C2410_GPH(0),
   .set_power     = NULL,
   .ocr_avail     = MMC_VDD_32_33|MMC_VDD_33_34,
};

//LED
static struct gpio_led gec2410_led_pins[] = {
	{
		.name		= "LED1",
		.gpio		= S3C2410_GPE(11),
		.active_low	= true,
	},
	{
		.name		= "LED2",
		.gpio		= S3C2410_GPE(12) ,
		.active_low	= true,
	},
	{
		.name		= "LED3",
		.gpio		= S3C2410_GPH(4),
		.active_low	= true,
	},
	{
		.name		= "LED4",
		.gpio		= S3C2410_GPH(6),
		.active_low	= true,
	},
};

static struct gpio_led_platform_data gec2410_led_data = {
	.num_leds		= ARRAY_SIZE(gec2410_led_pins),
	.leds			= gec2410_led_pins,
};

static struct platform_device gec2410_leds = {
	.name			= "leds-gpio",
	.id			= -1,
	.dev.platform_data	= &gec2410_led_data,
};
static struct gpio_keys_button gec2410_buttons[] = {
	{
		.gpio		= S3C2410_GPG(3),
		.code		= BTN_0,
		.desc		= "BTN0",
		.active_low	= 0,
	},
};
static struct gpio_keys_platform_data gec2410_button_data = {
	.buttons	= gec2410_buttons,
	.nbuttons	= ARRAY_SIZE(gec2410_buttons),
};

static struct platform_device gec2410_button_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.num_resources	= 0,
	.dev		= {
		.platform_data	= &gec2410_button_data,
	}
};

static struct resource gpiodev_resource = {
	.start			= 0xFFFFFFFF,
};


/* devices we initialise */

static struct platform_device *gec2410_devices[] __initdata = {
	&s3c_device_usb,
	&s3c_device_rtc,
	&s3c_device_lcd,
	&s3c_device_wdt,
	&s3c_device_i2c0,
	&s3c_device_iis,
	&s3c24xx_uda134x,
	&s3c_device_nand,
	&s3c_device_sdi,
	&s3c_device_usbgadget,
	&gec2410_leds,
	&gec2410_button_device,
};


static void __init gec2410_map_io(void)
{
	s3c24xx_init_io(gec2410_iodesc, ARRAY_SIZE(gec2410_iodesc));
	s3c24xx_init_clocks(12000000);
	s3c24xx_init_uarts(gec2410_uartcfgs, ARRAY_SIZE(gec2410_uartcfgs));
	usb_gec2410_init();
}

extern  struct s3c2410fb_mach_info s3c24xx_fb_info __initdata;

static void __init gec2410_machine_init(void)
{

	s3c24xx_fb_set_platdata(&s3c24xx_fb_info);

	s3c_i2c0_set_platdata(NULL);

	s3c2410_gpio_cfgpin(S3C2410_GPC(0), S3C2410_GPC0_LEND);
	s3c_device_usb.dev.platform_data = &usb_gec2410_info;
	s3c_device_nand.dev.platform_data = &gec2410_nand_info;
	s3c_device_sdi.dev.platform_data = &gec2410_mmc_cfg;
	platform_add_devices(gec2410_devices, ARRAY_SIZE(gec2410_devices));
	platform_device_register_simple("GPIODEV", 0, &gpiodev_resource, 1); //GPIO resource MAP
	s3c_pm_init();
}

MACHINE_START(GEC2410, "GEC2410Bv1.1 development board")
	.phys_io	= S3C2410_PA_UART,
	.io_pg_offst	= (((u32)S3C24XX_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S3C2410_SDRAM_PA + 0x100,

	.init_irq	= s3c24xx_init_irq,
	.map_io		= gec2410_map_io,
	.init_machine	= gec2410_machine_init,
	.timer		= &s3c24xx_timer,
MACHINE_END

