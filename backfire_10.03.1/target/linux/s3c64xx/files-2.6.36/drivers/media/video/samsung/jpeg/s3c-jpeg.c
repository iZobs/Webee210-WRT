/* linux/drivers/media/video/samsung/jpeg/s3c-jpeg.c
 *
 * Driver file for Samsung JPEG Encoder/Decoder
 *
 * Peter Oh, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/signal.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/kmod.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/page.h>
#include <mach/irqs.h>		
#include <linux/semaphore.h>		
#include <mach/map.h>	
#include <linux/miscdevice.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/platform_device.h>
#include <media/v4l2-dev.h>

#include <linux/version.h>
#include <mach/regs-clock.h>		

#include <linux/time.h>
#include <linux/clk.h>

#include "s3c-jpeg.h"
#include "jpg_mem.h"
#include "jpg_misc.h"
#include "jpg_opr.h"
#include "log_msg.h"


static struct clk		*jpeg_hclk;
static struct clk		*jpeg_sclk;
static struct clk		*post;
static struct resource	*jpeg_mem;
static void __iomem		*jpeg_base;
static s3c6400_jpg_ctx	JPGMem;
static int				irq_no;
static int				instanceNo = 0;
volatile int			jpg_irq_reason;

DECLARE_WAIT_QUEUE_HEAD(WaitQueue_JPEG);

irqreturn_t s3c_jpeg_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned int	intReason;
	unsigned int	status;

	status = JPGMem.v_pJPG_REG->JPGStatus;
	intReason = JPGMem.v_pJPG_REG->JPGIRQStatus;

	if(intReason) {
		intReason &= ((1<<6)|(1<<4)|(1<<3));

		switch(intReason) {
			case 0x08 : 
				jpg_irq_reason = OK_HD_PARSING; 
				break;
			case 0x00 : 
				jpg_irq_reason = ERR_HD_PARSING; 
				break;
			case 0x40 : 
				jpg_irq_reason = OK_ENC_OR_DEC; 
				break;
			case 0x10 : 
				jpg_irq_reason = ERR_ENC_OR_DEC; 
				break;
			default : 
				jpg_irq_reason = ERR_UNKNOWN;
		}
		wake_up_interruptible(&WaitQueue_JPEG);
	}	
	else {
		jpg_irq_reason = ERR_UNKNOWN;
		wake_up_interruptible(&WaitQueue_JPEG);
	}

	return IRQ_HANDLED;
}

static int s3c_jpeg_open(struct inode *inode, struct file *file)
{
	s3c6400_jpg_ctx *JPGRegCtx;
	DWORD	ret;

	clk_enable(jpeg_hclk);
	clk_enable(jpeg_sclk);

	log_msg(LOG_TRACE, "s3c_jpeg_open", "JPG_open \r\n");

	JPGRegCtx = (s3c6400_jpg_ctx *)mem_alloc(sizeof(s3c6400_jpg_ctx));
	memset(JPGRegCtx, 0x00, sizeof(s3c6400_jpg_ctx));

	ret = lock_jpg_mutex();
	if(!ret){
		log_msg(LOG_ERROR, "s3c_jpeg_open", "DD::JPG Mutex Lock Fail\r\n");
		unlock_jpg_mutex();
		return FALSE;
	}

	JPGRegCtx->v_pJPG_REG = JPGMem.v_pJPG_REG;
	JPGRegCtx->v_pJPGData_Buff = JPGMem.v_pJPGData_Buff;

	if (instanceNo > MAX_INSTANCE_NUM){
		log_msg(LOG_ERROR, "s3c_jpeg_open", "DD::Instance Number error-JPEG is running, instance number is %d\n", instanceNo);
		unlock_jpg_mutex();
		return FALSE;
	}

	instanceNo++;

	unlock_jpg_mutex();

	file->private_data = (s3c6400_jpg_ctx *)JPGRegCtx;

	return 0;
}


static int s3c_jpeg_release(struct inode *inode, struct file *file)
{
	DWORD			ret;
	s3c6400_jpg_ctx	*JPGRegCtx;

	log_msg(LOG_TRACE, "s3c_jpeg_release", "JPG_Close\n");

	JPGRegCtx = (s3c6400_jpg_ctx *)file->private_data;
	if(!JPGRegCtx){
		log_msg(LOG_ERROR, "s3c_jpeg_release", "DD::JPG Invalid Input Handle\r\n");
		return FALSE;
	}

	ret = lock_jpg_mutex();
	if(!ret){
		log_msg(LOG_ERROR, "s3c_jpeg_release", "DD::JPG Mutex Lock Fail\r\n");
		return FALSE;
	}

	//if((--instanceNo) < 0)
		instanceNo = 0;

	unlock_jpg_mutex();

	clk_disable(jpeg_hclk);
	clk_disable(jpeg_sclk);

	return 0;
}


static ssize_t s3c_jpeg_write (struct file *file, const char *buf, size_t
		count, loff_t *pos)
{
	return 0;
}

static ssize_t s3c_jpeg_read(struct file *file, char *buf, size_t count, loff_t *pos)
{	
	return 0;
}

static int s3c_jpeg_ioctl(struct file *file, unsigned
		int cmd, unsigned long arg)
{
	static s3c6400_jpg_ctx		*JPGRegCtx;
	JPG_DEC_PROC_PARAM	DecReturn;
	JPG_ENC_PROC_PARAM	EncParam;
	BOOL				result = TRUE;
	DWORD				ret;
	int out;
	

	JPGRegCtx = (s3c6400_jpg_ctx *)file->private_data;
	if(!JPGRegCtx){
		log_msg(LOG_ERROR, "s3c_jpeg_ioctl", "DD::JPG Invalid Input Handle\r\n");
		return FALSE;
	}

	ret = lock_jpg_mutex();
	if(!ret){
		log_msg(LOG_ERROR, "s3c_jpeg_ioctl", "DD::JPG Mutex Lock Fail\r\n");
		return FALSE;
	}

	switch (cmd) 
	{
		case IOCTL_JPG_DECODE:
			
			log_msg(LOG_TRACE, "s3c_jpeg_ioctl", "IOCTL_JPEG_DECODE\n");

			out = copy_from_user(&DecReturn, (JPG_DEC_PROC_PARAM *)arg, sizeof(JPG_DEC_PROC_PARAM));
			result = decode_jpg(JPGRegCtx, &DecReturn);

			log_msg(LOG_TRACE, "s3c_jpeg_ioctl", "width : %d hegiht : %d size : %d\n", 
					DecReturn.width, DecReturn.height, DecReturn.dataSize);

			out = copy_to_user((void *)arg, (void *)&DecReturn, sizeof(JPG_DEC_PROC_PARAM));
			break;

		case IOCTL_JPG_ENCODE:
		
			log_msg(LOG_TRACE, "s3c_jpeg_ioctl", "IOCTL_JPEG_ENCODE\n");

			out = copy_from_user(&EncParam, (JPG_ENC_PROC_PARAM *)arg, sizeof(JPG_ENC_PROC_PARAM));

			log_msg(LOG_TRACE, "s3c_jpeg_ioctl", "width : %d hegiht : %d\n", 
					EncParam.width, EncParam.height);

			result = encode_jpg(JPGRegCtx, &EncParam);

			log_msg(LOG_TRACE, "s3c_jpeg_ioctl", "encoded file size : %d\n", EncParam.fileSize);

			out = copy_to_user((void *)arg, (void *)&EncParam,  sizeof(JPG_ENC_PROC_PARAM));

			break;

		case IOCTL_JPG_GET_STRBUF:
			log_msg(LOG_TRACE, "s3c_jpeg_ioctl", "IOCTL_JPG_GET_STRBUF\n");
			unlock_jpg_mutex();
			return arg;      

		case IOCTL_JPG_GET_THUMB_STRBUF:
			log_msg(LOG_TRACE, "s3c_jpeg_ioctl", "IOCTL_JPG_GET_THUMB_STRBUF\n");
			unlock_jpg_mutex();
			return arg + JPG_STREAM_BUF_SIZE;

		case IOCTL_JPG_GET_FRMBUF:
			log_msg(LOG_TRACE, "s3c_jpeg_ioctl", "IOCTL_JPG_GET_FRMBUF\n");
			unlock_jpg_mutex();
			return arg + JPG_STREAM_BUF_SIZE + JPG_STREAM_THUMB_BUF_SIZE;

		case IOCTL_JPG_GET_THUMB_FRMBUF:
			log_msg(LOG_TRACE, "s3c_jpeg_ioctl", "IOCTL_JPG_GET_THUMB_FRMBUF\n");
			unlock_jpg_mutex();
			return arg + JPG_STREAM_BUF_SIZE + JPG_STREAM_THUMB_BUF_SIZE + JPG_FRAME_BUF_SIZE;

		case IOCTL_JPG_GET_PHY_FRMBUF:
			log_msg(LOG_TRACE, "s3c_jpeg_ioctl", "IOCTL_JPG_GET_PHY_FRMBUF\n");
			unlock_jpg_mutex();
			return jpg_data_base_addr + JPG_STREAM_BUF_SIZE + JPG_STREAM_THUMB_BUF_SIZE;

		case IOCTL_JPG_GET_PHY_THUMB_FRMBUF:
			log_msg(LOG_TRACE, "s3c_jpeg_ioctl", "IOCTL_JPG_GET_PHY_THUMB_FRMBUF\n");
			unlock_jpg_mutex();
			return jpg_data_base_addr + JPG_STREAM_BUF_SIZE + JPG_STREAM_THUMB_BUF_SIZE + JPG_FRAME_BUF_SIZE;

		default : 
			log_msg(LOG_ERROR, "s3c_jpeg_ioctl", "DD::JPG Invalid ioctl : 0x%X\r\n", cmd);
	}

	unlock_jpg_mutex();

	return result;
}


int s3c_jpeg_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long size	= vma->vm_end - vma->vm_start;
	unsigned long maxSize;
	unsigned long pageFrameNo;

	pageFrameNo = __phys_to_pfn(jpg_data_base_addr);

	maxSize = JPG_TOTAL_BUF_SIZE + PAGE_SIZE - (JPG_TOTAL_BUF_SIZE % PAGE_SIZE);

	if(size > maxSize) {
		return -EINVAL;
	}

	vma->vm_flags |= VM_RESERVED | VM_IO;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if( remap_pfn_range(vma, vma->vm_start, pageFrameNo, size,	\
				vma->vm_page_prot) ) {
		log_msg(LOG_ERROR, "s3c_jpeg_mmap", "jpeg remap error");
		return -EAGAIN;
	}

	return 0;
}

static struct file_operations jpeg_fops = {
			owner:		THIS_MODULE,
			open:		s3c_jpeg_open,
			release:	s3c_jpeg_release,
//			ioctl:		s3c_jpeg_ioctl,
			unlocked_ioctl:	s3c_jpeg_ioctl,
			read:		s3c_jpeg_read,
			write:		s3c_jpeg_write,
			mmap:		s3c_jpeg_mmap,
};


static struct miscdevice s3c_jpeg_miscdev = {
minor:		254, 		
			name:		"s3c-jpg",
			fops:		&jpeg_fops
};

static BOOL s3c_jpeg_clock_setup(void)
{
	unsigned int	jpg_clk;
	
	// JPEG clock was set as 66 MHz
	jpg_clk = readl(S3C_CLK_DIV0);
	jpg_clk = (jpg_clk & ~(0xF << 24)) | (3 << 24);
	__raw_writel(jpg_clk, S3C_CLK_DIV0);

	return TRUE;

}

static int s3c_jpeg_probe(struct platform_device *pdev)
{
	struct resource *res;
	static int		size;
	static int		ret;
	HANDLE 			h_Mutex;
	unsigned int	jpg_clk;


	// JPEG clock enable 
	jpeg_hclk	= clk_get(&pdev->dev, "hclk_jpeg");
	if (!jpeg_hclk || IS_ERR(jpeg_hclk)) {
		printk(KERN_ERR "failed to get jpeg hclk source\n");
		return -ENOENT;
	}
	clk_enable(jpeg_hclk);

	jpeg_sclk	= clk_get(&pdev->dev, "sclk_jpeg");
	if (!jpeg_sclk || IS_ERR(jpeg_sclk)) {
		printk(KERN_ERR "failed to get jpeg scllk source\n");
		return -ENOENT;
	}
	clk_enable(jpeg_sclk);
/*
	post	= clk_get(NULL, "post");
	if (!post) {
		printk(KERN_ERR "failed to get post clock source\n");
		return -ENOENT;
	}
	clk_enable(post);
*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		printk(KERN_INFO "failed to get memory region resouce\n");
		return -ENOENT;
	}

	size = (res->end-res->start)+1;
	jpeg_mem = request_mem_region(res->start, size, pdev->name);
	if (jpeg_mem == NULL) {
		printk(KERN_INFO "failed to get memory region\n");
		return -ENOENT;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		printk(KERN_INFO "failed to get irq resource\n");
		return -ENOENT;
	}

	irq_no = res->start;
	ret = request_irq(res->start, s3c_jpeg_irq, 0, pdev->name, pdev);
	if (ret != 0) {
		printk(KERN_INFO "failed to install irq (%d)\n", ret);
		return ret;
	}
/*
	jpeg_base = ioremap(res->start, size);
	if (jpeg_base == 0) {
		printk(KERN_INFO "failed to ioremap() region\n");
		return -EINVAL;
	}
*/
	// JPEG clock was set as 66 MHz
	if (s3c_jpeg_clock_setup() == FALSE)
		return -ENODEV;
	
	log_msg(LOG_TRACE, "s3c_jpeg_probe", "JPG_Init\n");

	// Mutex initialization
	h_Mutex = create_jpg_mutex();
	if (h_Mutex == NULL) 
	{
		log_msg(LOG_ERROR, "s3c_jpeg_probe", "DD::JPG Mutex Initialize error\r\n");
		return FALSE;
	}

	ret = lock_jpg_mutex();
	if (!ret){
		log_msg(LOG_ERROR, "s3c_jpeg_probe", "DD::JPG Mutex Lock Fail\n");
		return FALSE;
	}

	// Memory initialization
	if( !jpeg_mem_mapping(&JPGMem) ){
		log_msg(LOG_ERROR, "s3c_jpeg_probe", "DD::JPEG-HOST-MEMORY Initialize error\r\n");
		unlock_jpg_mutex();
		return FALSE;
	}
	else {
		if (!jpg_buff_mapping(&JPGMem)){
			log_msg(LOG_ERROR, "s3c_jpeg_probe", "DD::JPEG-DATA-MEMORY Initialize error : %d\n");
			unlock_jpg_mutex();
			return FALSE;	
		}
	}

	instanceNo = 0;

	unlock_jpg_mutex();

	ret = misc_register(&s3c_jpeg_miscdev);

	clk_disable(jpeg_hclk);
	clk_disable(jpeg_sclk);

	return 0;
}

static int s3c_jpeg_remove(struct platform_device *dev)
{
	if (jpeg_mem != NULL) {
		release_resource(jpeg_mem);
		kfree(jpeg_mem);
		jpeg_mem = NULL;
	}

	free_irq(irq_no, dev);
	misc_deregister(&s3c_jpeg_miscdev);
	return 0;
}


static struct platform_driver s3c_jpeg_driver = {
	.probe		= s3c_jpeg_probe,
	.remove		= s3c_jpeg_remove,
	.shutdown	= NULL,
	.suspend	= NULL,
	.resume		= NULL,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "s3c-jpeg",
	},
};

static char banner[] __initdata = KERN_INFO "S3C JPEG Driver, (c) 2007 Samsung Electronics\n";

static int __init s3c_jpeg_init(void)
{
	printk(banner);
	return platform_driver_register(&s3c_jpeg_driver);
}

static void __exit s3c_jpeg_exit(void)
{
	DWORD	ret;

	log_msg(LOG_TRACE, "s3c_jpeg_exit", "JPG_Deinit\n");

	ret = lock_jpg_mutex();
	if(!ret){
		log_msg(LOG_ERROR, "s3c_jpeg_exit", "DD::JPG Mutex Lock Fail\r\n");
	}

	jpg_buff_free(&JPGMem);
	jpg_mem_free(&JPGMem);
	unlock_jpg_mutex();

	delete_jpg_mutex();	

	platform_driver_unregister(&s3c_jpeg_driver);	
	printk("S3C JPEG driver module exit\n");
}

module_init(s3c_jpeg_init);
module_exit(s3c_jpeg_exit);

MODULE_AUTHOR("Peter, Oh");
MODULE_DESCRIPTION("S3C JPEG Encoder/Decoder Device Driver");
MODULE_LICENSE("GPL");
