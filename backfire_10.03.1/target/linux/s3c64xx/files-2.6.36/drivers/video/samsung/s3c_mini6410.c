/*
 * drivers/video/s3c/s3cfb_mini6410.c
 *
 * based on s3cfb_lte480wv.c
 *
 * Copyright (C) 2008 Jinsung Yang <jsgood.yang@samsung.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	S3C Frame Buffer Driver
 *	based on skeletonfb.c, sa1100fb.h, s3c2410fb.c
 */

#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <mach/regs-gpio.h>
#include <mach/regs-lcd.h>

#include "s3cfb.h"

/**
 * WARNING: CLKVAL is defined upon 133 MHz HCLK, please update it
 * when HCLK freq changed.
 *   VCLK = 133 MHz / (CLKVAL + 1)
 */

#if defined(CONFIG_FB_S3C_EXT_TFT480272)

#define S3CFB_LCD_TYPE	"N43"
#define S3CFB_VBP		(0x03)	/* back porch */
#define S3CFB_VFP		(0x02)	/* front porch */
#define S3CFB_VSW		(0x02)	/* vsync width */
#define S3CFB_HBP		(0x2d)	/* back porch */
#define S3CFB_HFP		(0x04)	/* front porch */
#define S3CFB_HSW		(0x06)	/* hsync width */

#define S3CFB_HRES		480	/* horizon pixel  x resolition */
#define S3CFB_VRES		272	/* line cnt       y resolution */

#define S3CFB_CLKVAL		11
#define S3CFB_VIDCON1	(S3C_VIDCON1_IVCLK_RISE_EDGE)

#elif defined(CONFIG_FB_S3C_EXT_TFT800480)

#define S3CFB_LCD_TYPE	"A70"
#define S3CFB_VBP		(0x1d)	/* back porch */
#define S3CFB_VFP		(0x11)	/* front porch */
#define S3CFB_VSW		(0x18)	/* vsync width */
#define S3CFB_HBP		(0x28)	/* back porch */
#define S3CFB_HFP		(0x28)	/* front porch */
#define S3CFB_HSW		(0x30)	/* hsync width */

#define S3CFB_HRES		800	/* horizon pixel  x resolition */
#define S3CFB_VRES		480	/* line cnt       y resolution */

#define S3CFB_CLKVAL		3	/* ~33.25 MHz */
#define S3CFB_VIDCON1	(S3C_VIDCON1_IHSYNC_INVERT | S3C_VIDCON1_IVSYNC_INVERT)

#elif defined(CONFIG_FB_S3C_EXT_T240320)

#define S3CFB_LCD_TYPE	"T35"
#define S3CFB_VBP		(0x01)	/* back porch */
#define S3CFB_VFP		(0x01)	/* front porch */
#define S3CFB_VSW		(0x04)	/* vsync width */
#define S3CFB_HBP		(0x01)	/* back porch */
#define S3CFB_HFP		(0x04)	/* front porch */
#define S3CFB_HSW		(0x1E)	/* hsync width */

#define S3CFB_HRES		240	/* horizon pixel  x resolition */
#define S3CFB_VRES		320	/* line cnt       y resolution */

#define S3CFB_CLKVAL		11

#elif defined(CONFIG_FB_S3C_EXT_X240320)
#define S3CFB_LCD_TYPE	"X35"
#define S3CFB_VBP		( 4)	/* back porch */
#define S3CFB_VFP		( 2)	/* front porch */
#define S3CFB_VSW		(10)	/* vsync width */
#define S3CFB_HBP		( 8)	/* back porch */
#define S3CFB_HFP		( 6)	/* front porch */
#define S3CFB_HSW		(18)	/* hsync width */

#define S3CFB_HRES		240	/* horizon pixel  x resolition */
#define S3CFB_VRES		320	/* line cnt       y resolution */

#define S3CFB_CLKVAL		23	/* ~5.542 MHz */
#define S3CFB_VIDCON1	(S3C_VIDCON1_IVCLK_RISE_EDGE | S3C_VIDCON1_IVDEN_INVERT)

#elif defined(CONFIG_FB_S3C_EXT_TFT640480)

#define S3CFB_LCD_TYPE	"L80"
#define S3CFB_VBP		(0x01)	/* back porch */
#define S3CFB_VFP		(0x01)	/* front porch */
#define S3CFB_VSW		(0x01)	/* vsync width */
#define S3CFB_HBP		(0x03)	/* back porch */
#define S3CFB_HFP		(0x03)	/* front porch */
#define S3CFB_HSW		(0x28)	/* hsync width */

#define S3CFB_HRES		640	/* horizon pixel  x resolition */
#define S3CFB_VRES		480	/* line cnt       y resolution */

#define S3CFB_CLKVAL		3

#elif defined(CONFIG_FB_S3C_EXT_VGA1024768)

#define S3CFB_LCD_TYPE	"XGA"
#define S3CFB_VBP		(0x02)	/* back porch */
#define S3CFB_VFP		(0x02)	/* front porch */
#define S3CFB_VSW		(0x10)	/* vsync width */
#define S3CFB_HBP		(0x02)	/* back porch */
#define S3CFB_HFP		(0x02)	/* front porch */
#define S3CFB_HSW		(0x2A)	/* hsync width */

#define S3CFB_HRES		1024	/* horizon pixel  x resolition */
#define S3CFB_VRES		768	/* line cnt       y resolution */

#define S3CFB_CLKVAL		5

#elif defined(CONFIG_FB_S3C_EXT_VGA800600)

#define S3CFB_LCD_TYPE	"SVGA"
#define S3CFB_VBP		(0x02)	/* back porch */
#define S3CFB_VFP		(0x02)	/* front porch */
#define S3CFB_VSW		(0x10)	/* vsync width */
#define S3CFB_HBP		(0x02)	/* back porch */
#define S3CFB_HFP		(0x02)	/* front porch */
#define S3CFB_HSW		(0x2A)	/* hsync width */

#define S3CFB_HRES		800	/* horizon pixel  x resolition */
#define S3CFB_VRES		600	/* line cnt       y resolution */

#define S3CFB_CLKVAL		5

#elif defined(CONFIG_FB_S3C_EXT_VGA640480)

#define S3CFB_LCD_TYPE	"VGA"
#define S3CFB_VBP		(0x02)	/* back porch */
#define S3CFB_VFP		(0x02)	/* front porch */
#define S3CFB_VSW		(0x10)	/* vsync width */
#define S3CFB_HBP		(0x02)	/* back porch */
#define S3CFB_HFP		(0x02)	/* front porch */
#define S3CFB_HSW		(0x2A)	/* hsync width */

#define S3CFB_HRES		640	/* horizon pixel  x resolition */
#define S3CFB_VRES		480	/* line cnt       y resolution */

#define S3CFB_CLKVAL		5

#elif defined(CONFIG_FB_S3C_EXT_EZVGA800600)

#define S3CFB_LCD_TYPE	"EZVGA"
#define S3CFB_VBP		(0x02)	/* back porch */
#define S3CFB_VFP		(0x02)	/* front porch */
#define S3CFB_VSW		(0x10)	/* vsync width */
#define S3CFB_HBP		(0xA8)	/* back porch */
#define S3CFB_HFP		(0x11)	/* front porch */
#define S3CFB_HSW		(0x2A)	/* hsync width */

#define S3CFB_HRES		800	/* horizon pixel  x resolition */
#define S3CFB_VRES		600	/* line cnt       y resolution */

#define S3CFB_CLKVAL		2

#else
#error mini2440 frame buffer driver not configed
#endif

#define S3CFB_HRES_VIRTUAL	S3CFB_HRES	/* horizon pixel  x resolition */
#define S3CFB_VRES_VIRTUAL	S3CFB_VRES	/* line cnt       y resolution */

#define S3CFB_HRES_OSD		S3CFB_HRES	/* horizon pixel  x resolition */
#define S3CFB_VRES_OSD		S3CFB_VRES	/* line cnt       y resolution */

#define S3CFB_PIXEL_CLOCK	S3CFB_CLKVAL

static void s3cfb_set_fimd_info(void)
{
#ifdef S3CFB_VIDCON1
	s3cfb_fimd.vidcon1 = S3CFB_VIDCON1;
#else
	s3cfb_fimd.vidcon1 = S3C_VIDCON1_IHSYNC_INVERT | S3C_VIDCON1_IVSYNC_INVERT | S3C_VIDCON1_IVDEN_NORMAL;
#endif

#if defined(CONFIG_FB_S3C_EXT_VGA1024768) || \
	defined(CONFIG_FB_S3C_EXT_VGA640480) || \
	defined(CONFIG_FB_S3C_EXT_VGA800600)
	s3cfb_fimd.vidcon1 = 0;
#endif

	s3cfb_fimd.vidtcon0 = S3C_VIDTCON0_VBPD(S3CFB_VBP - 1) | S3C_VIDTCON0_VFPD(S3CFB_VFP - 1) | S3C_VIDTCON0_VSPW(S3CFB_VSW - 1);
	s3cfb_fimd.vidtcon1 = S3C_VIDTCON1_HBPD(S3CFB_HBP - 1) | S3C_VIDTCON1_HFPD(S3CFB_HFP - 1) | S3C_VIDTCON1_HSPW(S3CFB_HSW - 1);
	s3cfb_fimd.vidtcon2 = S3C_VIDTCON2_LINEVAL(S3CFB_VRES - 1) | S3C_VIDTCON2_HOZVAL(S3CFB_HRES - 1);

	s3cfb_fimd.vidosd0a = S3C_VIDOSDxA_OSD_LTX_F(0) | S3C_VIDOSDxA_OSD_LTY_F(0);
	s3cfb_fimd.vidosd0b = S3C_VIDOSDxB_OSD_RBX_F(S3CFB_HRES - 1) | S3C_VIDOSDxB_OSD_RBY_F(S3CFB_VRES - 1);

	s3cfb_fimd.vidosd1a = S3C_VIDOSDxA_OSD_LTX_F(0) | S3C_VIDOSDxA_OSD_LTY_F(0);
	s3cfb_fimd.vidosd1b = S3C_VIDOSDxB_OSD_RBX_F(S3CFB_HRES_OSD - 1) | S3C_VIDOSDxB_OSD_RBY_F(S3CFB_VRES_OSD - 1);

	s3cfb_fimd.width = S3CFB_HRES;
	s3cfb_fimd.height = S3CFB_VRES;
	s3cfb_fimd.xres = S3CFB_HRES;
	s3cfb_fimd.yres = S3CFB_VRES;

#if defined(CONFIG_FB_S3C_EXT_VIRTUAL_SCREEN)
	s3cfb_fimd.xres_virtual = S3CFB_HRES_VIRTUAL;
	s3cfb_fimd.yres_virtual = S3CFB_VRES_VIRTUAL;
#else
	s3cfb_fimd.xres_virtual = S3CFB_HRES;
	s3cfb_fimd.yres_virtual = S3CFB_VRES;
#endif

	s3cfb_fimd.osd_width = S3CFB_HRES_OSD;
	s3cfb_fimd.osd_height = S3CFB_VRES_OSD;
	s3cfb_fimd.osd_xres = S3CFB_HRES_OSD;
	s3cfb_fimd.osd_yres = S3CFB_VRES_OSD;

	s3cfb_fimd.osd_xres_virtual = S3CFB_HRES_OSD;
	s3cfb_fimd.osd_yres_virtual = S3CFB_VRES_OSD;

	s3cfb_fimd.pixclock = S3CFB_PIXEL_CLOCK;

	s3cfb_fimd.hsync_len = S3CFB_HSW;
	s3cfb_fimd.vsync_len = S3CFB_VSW;
	s3cfb_fimd.left_margin = S3CFB_HBP;
	s3cfb_fimd.upper_margin = S3CFB_VBP;
	s3cfb_fimd.right_margin = S3CFB_HFP;
	s3cfb_fimd.lower_margin = S3CFB_VFP;
}

void s3cfb_init_hw(void)
{
	printk(KERN_INFO "LCD TYPE :: %s will be initialized\n", S3CFB_LCD_TYPE);

	s3cfb_set_fimd_info();
	s3cfb_set_gpio();
}

