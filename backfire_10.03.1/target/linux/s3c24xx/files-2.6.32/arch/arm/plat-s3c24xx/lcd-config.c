#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>
#include <mach/fb.h>
#include <mach/hardware.h>
#include <mach/dma.h>
#include <mach/irqs.h>
#include <asm/irq.h>

#include <plat/regs-serial.h>
#include <plat/udc.h>

#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/regs-spi.h>


/* Kernel Command Line for LCD Setting. */

/*W35 TFT*/
static struct s3c2410fb_display s3c24xx_lcd_cfg_w35  = {
	.lcdcon5 = S3C2410_LCDCON5_FRM565 |
 				S3C2410_LCDCON5_INVVLINE |
 				S3C2410_LCDCON5_INVVFRAME |
 				S3C2410_LCDCON5_PWREN |
 				S3C2410_LCDCON5_HWSWP,

	.type = S3C2410_LCDCON1_TFT,
	.width = 320,
	.height = 240,
	.pixclock = 170000, /* HCLK 60 MHz, divisor 10 */
	.xres = 320,
	.yres = 240,
	.bpp = 16,
	.left_margin = 21,
	.right_margin = 39,
	.hsync_len = 31,
	.upper_margin = 16,
	.lower_margin = 13,
	.vsync_len = 4,

};

/*X35 TFT*/
static struct s3c2410fb_display s3c24xx_lcd_cfg_x35  = {
	.lcdcon5 = S3C2410_LCDCON5_FRM565 |
 				S3C2410_LCDCON5_INVVLINE |
 				S3C2410_LCDCON5_INVVFRAME |
				S3C2410_LCDCON5_INVVCLK |
				S3C2410_LCDCON5_INVVDEN |
 				S3C2410_LCDCON5_PWREN |
 				S3C2410_LCDCON5_HWSWP,

	.type = S3C2410_LCDCON1_TFT,
	.width = 240,
	.height = 320,
	.pixclock = 170000, /* HCLK 60 MHz, divisor 10 */
	.xres = 240,
	.yres = 320,
	.bpp = 16,

	.left_margin = 1,
	.right_margin = 26,
	.hsync_len = 5,
	.upper_margin = 1,
	.lower_margin = 5,
	.vsync_len = 10,

};

/*T35 TFT*/
static struct s3c2410fb_display s3c24xx_lcd_cfg_t35  = {
	.lcdcon5 = S3C2410_LCDCON5_FRM565 |
 				S3C2410_LCDCON5_INVVLINE |
 				S3C2410_LCDCON5_INVVFRAME |
				S3C2410_LCDCON5_INVVCLK |
				S3C2410_LCDCON5_INVVDEN |
 				S3C2410_LCDCON5_PWREN |
 				S3C2410_LCDCON5_HWSWP,

	.type = S3C2410_LCDCON1_TFT,
	.width = 240,
	.height = 320,
	.pixclock = 170000, /* HCLK 60 MHz, divisor 10 */
	.xres = 240,
	.yres = 320,
	.bpp = 16,

	.left_margin = 1,
	.right_margin = 26,
	.hsync_len = 5,
	.upper_margin = 2,
	.lower_margin = 5,
	.vsync_len = 2,

};

/*N43 TFT*/
static struct s3c2410fb_display s3c24xx_lcd_cfg_n43  = {
	.lcdcon5 = S3C2410_LCDCON5_FRM565 |
 				S3C2410_LCDCON5_INVVLINE |
 				S3C2410_LCDCON5_INVVFRAME |
 				S3C2410_LCDCON5_PWREN |
 				S3C2410_LCDCON5_HWSWP,

	.type = S3C2410_LCDCON1_TFT,
	.width = 480,
	.height = 272,
	.pixclock = 170000, /* HCLK 60 MHz, divisor 10 */
	.xres = 480,
	.yres = 272,
	.bpp = 16,
	.left_margin = 40,
	.right_margin = 40,
	.hsync_len = 48,
	.upper_margin = 29,
	.lower_margin = 13,
	.vsync_len = 3,
};
/* 640*480 VGA driver info */
static struct s3c2410fb_display s3c24xx_lcd_cfg_vga  = {
	
	.lcdcon5 = S3C2410_LCDCON5_FRM565 |
 				S3C2410_LCDCON5_INVVLINE |
 				S3C2410_LCDCON5_INVVFRAME |
 				S3C2410_LCDCON5_PWREN |
 				S3C2410_LCDCON5_HWSWP,

	.type = S3C2410_LCDCON1_TFT,
	.width = 640,
	.height = 480,
	.pixclock = 5000, /* HCLK 60 MHz, divisor 10 */
	.xres = 640,
	.yres = 480,
	.bpp = 16,


	.left_margin = 23,
	.right_margin = 1,
	.hsync_len = 135,
	.upper_margin = 28,
	.lower_margin = 2,
	.vsync_len = 5,
};

static struct s3c2410fb_display s3c24xx_lcd_cfg_a70  = {
	.lcdcon5 = S3C2410_LCDCON5_FRM565 |
 				S3C2410_LCDCON5_INVVLINE |
 				S3C2410_LCDCON5_INVVFRAME |
 				S3C2410_LCDCON5_PWREN |
 				S3C2410_LCDCON5_HWSWP,

	.type = S3C2410_LCDCON1_TFT,
	.width = 800,
	.height = 480,
	.pixclock = 170000, /* HCLK 60 MHz, divisor 10 */
	.xres = 800,
	.yres = 480,
	.bpp = 16,
	.left_margin = 40,
	.right_margin = 40,
	.hsync_len = 48,
	.upper_margin = 29,
	.lower_margin = 13,
	.vsync_len = 3,
};



struct s3c2410fb_mach_info s3c24xx_fb_info __initdata = {
	.displays = &s3c24xx_lcd_cfg_a70,
	.num_displays = 1,
	.default_display = 0,
};

static char s3c_lcd_type[4] = "A70";

static int __init s3c2410_lcd_setup(char *str)
{
//	printk("Setting TFT LCD from kernel command line...\n");
//	strcpy(s3c_lcd_type,str);
//	sprintf(s3c_lcd_type,str);
	s3c_lcd_type[0] = str[0];
	s3c_lcd_type[1] = str[1];
	s3c_lcd_type[2] = str[2];
	
	if(strcmp(s3c_lcd_type,"A70")==0)
	{
	  s3c24xx_fb_info.displays = &s3c24xx_lcd_cfg_a70,
	  printk("LCD Type: A70.\n");
	}
	else if (strcmp(s3c_lcd_type,"N43")==0)
	{
	  s3c24xx_fb_info.displays = &s3c24xx_lcd_cfg_n43,
	  printk("LCD Type: N43.\n");
	}
	else if (strcmp(s3c_lcd_type,"X35")==0)
	{
	  s3c24xx_fb_info.displays = &s3c24xx_lcd_cfg_x35,
	  printk("LCD Type: X35.\n");
	}
	else if (strcmp(s3c_lcd_type,"T35")==0)
	{
	  s3c24xx_fb_info.displays = &s3c24xx_lcd_cfg_t35,
	  printk("LCD Type: T35.\n");
	}
	else if (strcmp(s3c_lcd_type,"W35")==0)
	{
	  s3c24xx_fb_info.displays = &s3c24xx_lcd_cfg_w35,
	  printk("LCD Type: W35.\n");
	}
	else if (strcmp(s3c_lcd_type,"VGA")==0)
	{
	  s3c24xx_fb_info.displays = &s3c24xx_lcd_cfg_vga,
	  printk("Set LCD Type: VGA.\n");
	}
	else
	{
	  s3c24xx_fb_info.displays = &s3c24xx_lcd_cfg_n43,
	  printk("Set LCD Type: Unknow LCD |%s|\n",s3c_lcd_type);
	}	  
	return 1;
}

__setup("lcd_type=", s3c2410_lcd_setup);
