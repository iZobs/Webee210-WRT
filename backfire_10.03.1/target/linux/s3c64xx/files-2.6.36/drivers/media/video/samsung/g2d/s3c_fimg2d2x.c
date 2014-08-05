/* linux/drivers/media/video/samsung/g2d/s3c_fimg2d2x.c
 *
 * Driver file for Samsung 2D Accelerator(FIMG-2D)
 *
 * Jonghun Han, Copyright (c) 2009 Samsung Electronics
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
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/io.h>
#include <mach/map.h>
#include <plat/regs-g2d.h>

#include "s3c_fimg2d2x.h"

static struct clk *s3c_g2d_clock;
static struct clk *h_clk;

static int s3c_g2d_irq_num = NO_IRQ;
static struct resource *s3c_g2d_mem;
static void __iomem *s3c_g2d_base;
static wait_queue_head_t waitq_g2d;

static struct mutex *h_rot_mutex;

static u16 s3c_g2d_poll_flag = 0;

void s3c_g2d_check_fifo(int empty_fifo)
{
	u32 val; 

	val = __raw_readl(s3c_g2d_base + S3C_G2D_FIFO_STAT_REG);
	while( S3C_G2D_FIFO_USED(val) > (FIFO_NUM - empty_fifo));
}


static int s3c_g2d_init_regs(s3c_g2d_params *params)
{
	u32 bpp_mode;
	u32 tmp_reg;
	s3c_g2d_check_fifo(25);


	switch(params->bpp) {
	case ARGB8:
		bpp_mode = S3C_G2D_COLOR_MODE_REG_C0_15BPP;
		break;
		
	case RGB16:
		bpp_mode = S3C_G2D_COLOR_RGB_565;
		break;

	case RGB18:
		bpp_mode = S3C_G2D_COLOR_MODE_REG_C2_18BPP;
		break;

	case RGB24:
		bpp_mode = S3C_G2D_COLOR_XRGB_8888;
		break;

	default:
		bpp_mode = S3C_G2D_COLOR_MODE_REG_C3_24BPP;
		break;
	}

	/*set register for soruce image ===============================*/
	__raw_writel(params->src_base_addr, s3c_g2d_base + S3C_G2D_SRC_BASE_ADDR);
	__raw_writel(params->src_full_width, s3c_g2d_base + S3C_G2D_HORI_RES_REG);
	__raw_writel(params->src_full_height, s3c_g2d_base + S3C_G2D_VERT_RES_REG);
	__raw_writel((S3C_G2D_FULL_V(params->src_full_height) | 
			S3C_G2D_FULL_H(params->src_full_width)), 
			s3c_g2d_base+S3C_G2D_SRC_RES_REG);
	__raw_writel(bpp_mode, s3c_g2d_base + S3C_G2D_SRC_COLOR_MODE);
	
	/*set register for destination image =============================*/
	__raw_writel(params->dst_base_addr, s3c_g2d_base + S3C_G2D_DST_BASE_ADDR);
	__raw_writel(params->dst_full_width, s3c_g2d_base + S3C_G2D_SC_HORI_REG);
	__raw_writel(params->dst_full_height, s3c_g2d_base + S3C_G2D_SC_VERT_REG);
	__raw_writel((S3C_G2D_FULL_V(params->dst_full_height) | 
			S3C_G2D_FULL_H(params->dst_full_width)), 
			s3c_g2d_base+S3C_G2D_SC_RES_REG);
	__raw_writel(bpp_mode, s3c_g2d_base + S3C_G2D_DST_COLOR_MODE);

	/*set register for clipping window===============================*/
	__raw_writel(params->cw_x1, s3c_g2d_base + S3C_G2D_CW_LT_X_REG);
	__raw_writel(params->cw_y1, s3c_g2d_base + S3C_G2D_CW_LT_Y_REG);
	__raw_writel(params->cw_x2, s3c_g2d_base + S3C_G2D_CW_RB_X_REG);
	__raw_writel(params->cw_y2, s3c_g2d_base + S3C_G2D_CW_RB_Y_REG);

	/*set register for color=======================================*/
	__raw_writel(params->color_val[G2D_WHITE], s3c_g2d_base + S3C_G2D_FG_COLOR_REG);	/* set color to both font and foreground color */
	__raw_writel(params->color_val[G2D_BLACK], s3c_g2d_base + S3C_G2D_BG_COLOR_REG);
	__raw_writel(params->color_val[G2D_BLUE], s3c_g2d_base + S3C_G2D_BS_COLOR_REG);		/* Set blue color to blue screen color */

	/*set register for ROP & Alpha==================================*/
	if(params->alpha_mode == TRUE) {
		if(params->alpha_val > ALPHA_VALUE_MAX) {
			printk(KERN_ALERT "s3c g2d dirver error: exceed alpha value range 0~255\n");
 	 		return -ENOENT;
		}

		__raw_writel(S3C_G2D_ROP_REG_OS_FG_COLOR | 
				S3C_G2D_ROP_REG_ABM_REGISTER | 
				S3C_G2D_ROP_REG_T_OPAQUE_MODE | 
				G2D_ROP_SRC_ONLY, 
				s3c_g2d_base + S3C_G2D_ROP_REG);
		__raw_writel(S3C_G2D_ALPHA(params->alpha_val), s3c_g2d_base + S3C_G2D_ALPHA_REG);
	} else {
		__raw_writel(S3C_G2D_ROP_REG_OS_FG_COLOR | 
				S3C_G2D_ROP_REG_ABM_NO_BLENDING | 
				S3C_G2D_ROP_REG_T_OPAQUE_MODE | 
				G2D_ROP_SRC_ONLY, 
				s3c_g2d_base + S3C_G2D_ROP_REG);
		__raw_writel(S3C_G2D_ALPHA(0x00), s3c_g2d_base + S3C_G2D_ALPHA_REG);		
	}

	/*set register for color key====================================*/
	if(params->color_key_mode == TRUE) {
		tmp_reg = __raw_readl(s3c_g2d_base + S3C_G2D_ROP_REG);
		tmp_reg |= S3C_G2D_ROP_REG_T_TRANSP_MODE ;
		__raw_writel(tmp_reg , s3c_g2d_base + S3C_G2D_ROP_REG);
		__raw_writel(params->color_key_val, s3c_g2d_base + S3C_G2D_BS_COLOR_REG); 	
	}

	/*set register for rotation=====================================*/
	__raw_writel(S3C_G2D_ROTATRE_REG_R0_0, s3c_g2d_base + S3C_G2D_ROT_OC_X_REG);
	__raw_writel(S3C_G2D_ROTATRE_REG_R0_0, s3c_g2d_base + S3C_G2D_ROT_OC_Y_REG);
	__raw_writel(S3C_G2D_ROTATRE_REG_R0_0, s3c_g2d_base + S3C_G2D_ROTATE_REG);
	
	return 0;
}


void s3c_g2d_bitblt(u16 src_x1, u16 src_y1, u16 src_x2, u16 src_y2,
 	 u16 dst_x1, u16 dst_y1, u16 dst_x2, u16 dst_y2)
{
	u32 cmd_reg_val;

	s3c_g2d_check_fifo(25);

 	__raw_writel(src_x1, s3c_g2d_base + S3C_G2D_COORD0_X_REG);
	__raw_writel(src_y1, s3c_g2d_base + S3C_G2D_COORD0_Y_REG);
 	__raw_writel(src_x2, s3c_g2d_base + S3C_G2D_COORD1_X_REG);
	__raw_writel(src_y2, s3c_g2d_base + S3C_G2D_COORD1_Y_REG);

 	 __raw_writel(dst_x1, s3c_g2d_base + S3C_G2D_COORD2_X_REG);
 	__raw_writel(dst_y1, s3c_g2d_base + S3C_G2D_COORD2_Y_REG);
 	__raw_writel(dst_x2, s3c_g2d_base + S3C_G2D_COORD3_X_REG);
 	__raw_writel(dst_y2, s3c_g2d_base + S3C_G2D_COORD3_Y_REG);

	cmd_reg_val = readl(s3c_g2d_base + S3C_G2D_CMD1_REG);
	cmd_reg_val = ~(S3C_G2D_CMD1_REG_S|S3C_G2D_CMD1_REG_N);
	cmd_reg_val |= S3C_G2D_CMD1_REG_N;
	__raw_writel(cmd_reg_val, s3c_g2d_base + S3C_G2D_CMD1_REG);
}


static void s3c_g2d_rotate_with_bitblt(s3c_g2d_params *params, ROT_DEG rot_degree)
{
	u16 org_x=0, org_y=0;
	u32 rot_reg_val = 0;
	u32 src_x1, src_y1, src_x2, src_y2, dst_x1, dst_y1, dst_x2, dst_y2;

	src_x1 = params->src_start_x;
	src_y1 = params->src_start_y;
	src_x2 = params->src_start_x + params->src_work_width;
	src_y2 = params->src_start_y + params->src_work_height;
	dst_x1 = params->dst_start_x;
	dst_y1 = params->dst_start_y;
	dst_x2 = params->dst_start_x + params->dst_work_width;
	dst_y2 = params->dst_start_y + params->dst_work_height;
	
	s3c_g2d_check_fifo(25);
	__raw_writel(S3C_G2D_INTEN_REG_CCF, s3c_g2d_base + S3C_G2D_INTEN_REG);	

	s3c_g2d_get_rotation_origin(src_x1, src_y1, src_x2, src_y2, 
					dst_x1, dst_y1, rot_degree, &org_x, &org_y);
	if(rot_degree != (ROT_0||ROT_X_FLIP||ROT_Y_FLIP)){
		org_x += 1;
		org_y += 1;
	}
	__raw_writel(org_x, s3c_g2d_base + S3C_G2D_ROT_OC_X_REG);
	__raw_writel(org_y, s3c_g2d_base + S3C_G2D_ROT_OC_Y_REG);

	switch(rot_degree) {
	case ROT_0:
		rot_reg_val = S3C_G2D_ROTATRE_REG_R0_0;
		break;
		
	case ROT_90:
		rot_reg_val = S3C_G2D_ROTATRE_REG_R1_90;
		break;

	case ROT_180:
		rot_reg_val = S3C_G2D_ROTATRE_REG_R2_180;
		break;

	case ROT_270:
		rot_reg_val = S3C_G2D_ROTATRE_REG_R3_270;
		break;
		
	case ROT_X_FLIP:
		rot_reg_val = S3C_G2D_ROTATRE_REG_FX;
		break;

	case ROT_Y_FLIP:
		rot_reg_val = S3C_G2D_ROTATRE_REG_FY;
		break;

	default:
		printk(KERN_ERR "[%s : %d] Wrong rotation degree\n", __FUNCTION__, __LINE__);
		break;
	}
	__raw_writel(rot_reg_val, s3c_g2d_base + S3C_G2D_ROTATE_REG);

	switch(rot_degree) {
	case ROT_0:		/* fall through */
	case ROT_X_FLIP:	/* fall through */
	case ROT_Y_FLIP:
		s3c_g2d_bitblt(src_x1, src_y1, src_x2, src_y2, 
				dst_x1, dst_y1, dst_x2, dst_y2);
		break;
		
	case ROT_90:
		s3c_g2d_bitblt(src_x1, src_y1, src_x2, src_y2, src_x1, 
				src_y1+1, src_x2, src_y2+1);
		break;

	case ROT_180:
		s3c_g2d_bitblt(src_x1, src_y1, src_x2, src_y2, src_x1+1, 
				src_y1+1, src_x2+1, src_y2+1);
		break;

	case ROT_270:
		s3c_g2d_bitblt(src_x1, src_y1, src_x2, src_y2, src_x1+1, 
				src_y1, src_x2+1, src_y2);
		break;
		
	default:
		break;
	}
}


static void s3c_g2d_get_rotation_origin(u16 src_x1, u16 src_y1, 
					u16 src_x2, u16 src_y2, 
					u16 dst_x1, u16 dst_y1, 
					ROT_DEG rot_degree, 
					u16* org_x, u16* org_y)
{
	s3c_g2d_check_fifo(25);

	switch(rot_degree) {
	case ROT_90:
		*org_x = ((dst_x1 - dst_y1 + src_x1 + src_y2)>>1);
		*org_y = ((dst_x1 + dst_y1 - src_x1 + src_y2)>>1);
		break;

	case ROT_180:		 /* fall through */
	case ROT_X_FLIP:	 /* fall through */
	case ROT_Y_FLIP:
		*org_x = ((dst_x1 + src_x2)>>1);
		*org_y = ((dst_y1 + src_y2)>>1);
		break;

	case ROT_270:
		*org_x = ((dst_x1 + dst_y1 + src_x2 - src_y1)>>1);
		*org_y = ((dst_y1 - dst_x1 + src_x2 + src_y1)>>1);
		break;

	case ROT_0: 		/* fall through */
	default:
		break;
	}
}


static void s3c_g2d_rotator_start(s3c_g2d_params *params, ROT_DEG rot_degree)
{

	s3c_g2d_init_regs(params);
	s3c_g2d_rotate_with_bitblt(params, rot_degree);
}


irqreturn_t s3c_g2d_irq(int irq, void *dev_id)
{
	if(__raw_readl(s3c_g2d_base + S3C_G2D_INTC_PEND_REG) & S3C_G2D_PEND_REG_INTP_CMD_FIN) {
	 	__raw_writel ( S3C_G2D_PEND_REG_INTP_CMD_FIN, s3c_g2d_base + S3C_G2D_INTC_PEND_REG );
		wake_up_interruptible(&waitq_g2d);
		s3c_g2d_poll_flag = 1;
	}

	return IRQ_HANDLED;
}


int s3c_g2d_open(struct inode *inode, struct file *file)
{
	s3c_g2d_params *params;
	params = (s3c_g2d_params *)kmalloc(sizeof(s3c_g2d_params), GFP_KERNEL);
	if(params == NULL) {
		printk(KERN_ERR "Instance memory allocation was failed\n");
		return -1;
	}

	memset(params, 0, sizeof(s3c_g2d_params));

	file->private_data	= (s3c_g2d_params *)params;
	
	printk("s3c_g2d_open() \n");

	return 0;
}


int s3c_g2d_release(struct inode *inode, struct file *file)
{
	s3c_g2d_params *params;

	params = (s3c_g2d_params *)file->private_data;
	if (params == NULL) {
		printk(KERN_ERR "Can't release s3c_rotator!!\n");
		return -1;
	}

	kfree(params);
	
	printk("s3c_g2d_release() \n");

	return 0;
}


int s3c_g2d_mmap(struct file* filp, struct vm_area_struct *vma) 
{
	unsigned long pageFrameNo = 0;
	unsigned long size;
	
	size = vma->vm_end - vma->vm_start;

	// page frame number of the address for a source G2D_SFR_SIZE to be stored at. 
	pageFrameNo = __phys_to_pfn(s3c_g2d_mem->start);
	
	if(size > G2D_SFR_SIZE) {
		printk("The size of G2D_SFR_SIZE mapping is too big!\n");
		return -EINVAL;
	}
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot); 
	
	if((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED)) {
		printk("Writable G2D_SFR_SIZE mapping must be shared !\n");
		return -EINVAL;
	}
	
	if(remap_pfn_range(vma, vma->vm_start, pageFrameNo, size, vma->vm_page_prot))
		return -EINVAL;

	return 0;
}


static int s3c_g2d_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	s3c_g2d_params *params;
	ROT_DEG rot_degree;

	params	= (s3c_g2d_params*)file->private_data;
	if (copy_from_user(params, (s3c_g2d_params*)arg, sizeof(s3c_g2d_params)))
		return -EFAULT;

	mutex_lock(h_rot_mutex);
	
	switch(cmd) {
	case S3C_G2D_ROTATOR_0:
		rot_degree = ROT_0;
		s3c_g2d_rotator_start(params, rot_degree);
		break;
		
	case S3C_G2D_ROTATOR_90:
		rot_degree = ROT_90;
		s3c_g2d_rotator_start(params, rot_degree);
		break;

	case S3C_G2D_ROTATOR_180:
		rot_degree = ROT_180;
		s3c_g2d_rotator_start(params, rot_degree);
		break;
		
	case S3C_G2D_ROTATOR_270:
		rot_degree = ROT_270;
		s3c_g2d_rotator_start(params, rot_degree);
		break;

	case S3C_G2D_ROTATOR_X_FLIP:
		rot_degree = ROT_X_FLIP;
		s3c_g2d_rotator_start(params, rot_degree);
		break;

	case S3C_G2D_ROTATOR_Y_FLIP:
		rot_degree = ROT_Y_FLIP;
		s3c_g2d_rotator_start(params, rot_degree);
		break;
	
	default:
		mutex_unlock(h_rot_mutex);
		return -EINVAL;
	}
	
	if(!(file->f_flags & O_NONBLOCK)) {
		if (interruptible_sleep_on_timeout(&waitq_g2d, G2D_TIMEOUT) == 0) {
			printk(KERN_ERR "\n%s: Waiting for interrupt is timeout\n", __FUNCTION__);
		}
	}

	mutex_unlock(h_rot_mutex);

	return 0;
	
}


static unsigned int s3c_g2d_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(file, &waitq_g2d, wait);
	if(s3c_g2d_poll_flag == 1) {
		mask = POLLOUT|POLLWRNORM;
		s3c_g2d_poll_flag = 0;
	}

	return mask;
}


struct file_operations s3c_g2d_fops = {
	.owner 		= THIS_MODULE,
	.open 		= s3c_g2d_open,
	.release 	= s3c_g2d_release,
	.mmap 		= s3c_g2d_mmap,
	.ioctl		= s3c_g2d_ioctl,
	.poll		= s3c_g2d_poll,
};


static struct miscdevice s3c_g2d_dev = {
	.minor		= G2D_MINOR,
	.name		= "s3c-g2d",
	.fops		= &s3c_g2d_fops,
};


int s3c_g2d_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret;
	printk(KERN_ALERT"s3c_g2d_probe called\n");

	/* find the IRQs */
	s3c_g2d_irq_num = platform_get_irq(pdev, 0);
	if(s3c_g2d_irq_num <= 0) {
		printk(KERN_ERR "failed to get irq resouce\n");
 		return -ENOENT;
	}

	ret = request_irq(s3c_g2d_irq_num, s3c_g2d_irq, IRQF_DISABLED, pdev->name, NULL);
	if (ret) {
		printk("request_irq(g2d) failed.\n");
		return ret;
	}


	/* get the memory region */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(res == NULL) {
		printk(KERN_ERR "failed to get memory region resouce\n");
		return -ENOENT;
	}

	s3c_g2d_mem = request_mem_region(res->start, res->end-res->start+1, pdev->name);
	if(s3c_g2d_mem == NULL) {
		printk(KERN_ERR "failed to reserve memory region\n");
 	return -ENOENT;
	}


	s3c_g2d_base = ioremap(s3c_g2d_mem->start, s3c_g2d_mem->end - res->start + 1);
	if(s3c_g2d_base == NULL) {
		printk(KERN_ERR "failed ioremap\n");
 	return -ENOENT;
	}

	s3c_g2d_clock = clk_get(&pdev->dev, "g2d");
	if(s3c_g2d_clock == NULL) {
		printk(KERN_ERR "failed to find g2d clock source\n");
		return -ENOENT;
	}

	clk_enable(s3c_g2d_clock);

	h_clk = clk_get(&pdev->dev, "hclk");
	if(h_clk == NULL) {
		printk(KERN_ERR "failed to find h_clk clock source\n");
		return -ENOENT;
	}

	init_waitqueue_head(&waitq_g2d);

	ret = misc_register(&s3c_g2d_dev);
	if (ret) {
		printk (KERN_ERR "cannot register miscdev on minor=%d (%d)\n",
			G2D_MINOR, ret);
		return ret;
	}

	h_rot_mutex = (struct mutex *)kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (h_rot_mutex == NULL)
		return -1;
	
	mutex_init(h_rot_mutex);

	printk(KERN_ALERT" s3c_g2d_probe Success\n");

	return 0;
}


static int s3c_g2d_remove(struct platform_device *dev)
{
	printk(KERN_INFO "s3c_g2d_remove called !\n");

	free_irq(s3c_g2d_irq_num, NULL);
	
	if (s3c_g2d_mem != NULL) { 
		printk(KERN_INFO "S3C Rotator Driver, releasing resource\n");
		iounmap(s3c_g2d_base);
		release_resource(s3c_g2d_mem);
		kfree(s3c_g2d_mem);
	}

	misc_deregister(&s3c_g2d_dev);	
	printk(KERN_INFO "s3c_g2d_remove Success !\n");
	return 0;
}


static int s3c_g2d_suspend(struct platform_device *dev, pm_message_t state)
{
	clk_disable(s3c_g2d_clock);
	return 0;
}


static int s3c_g2d_resume(struct platform_device *pdev)
{
	clk_enable(s3c_g2d_clock);
	return 0;
}


static struct platform_driver s3c_g2d_driver = {
	.probe		= s3c_g2d_probe,
	.remove		= s3c_g2d_remove,
	.suspend	= s3c_g2d_suspend,
	.resume		= s3c_g2d_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "s3c-g2d",
	},
};


int __init s3c_g2d_init(void)
{
 	if(platform_driver_register(&s3c_g2d_driver) != 0) {
 		printk("platform device register Failed \n");
 		return -1;
 	}

	printk(" S3C G2D Init : Done\n");
 	return 0;
}


void s3c_g2d_exit(void)
{
	platform_driver_unregister(&s3c_g2d_driver);
	mutex_destroy(h_rot_mutex);
}

module_init(s3c_g2d_init);
module_exit(s3c_g2d_exit);

MODULE_AUTHOR("Jonghun Han <jonghun.han@samsung.com>");
MODULE_DESCRIPTION("S3C FIMG-2D Device Driver");
MODULE_LICENSE("GPL");