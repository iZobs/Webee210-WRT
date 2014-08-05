/* linux/drivers/media/video/samsung/s3c_post_core.c
 *
 * Core file for Samsung Post Processor driver
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
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <asm/memory.h>
#include <plat/clock.h>
#include <plat/media.h>

#include "s3c_post.h"

struct s3c_post_config s3c_post;

struct s3c_platform_post *to_post_plat(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return (struct s3c_platform_post *) pdev->dev.platform_data;
}

static irqreturn_t s3c_post_irq(int irq, void *dev_id)
{
	struct s3c_post_control *ctrl = (struct s3c_post_control *) dev_id;

	s3c_post_clear_irq(ctrl);

	return IRQ_HANDLED;
}

static
struct s3c_post_control *s3c_post_register_controller(struct platform_device *pdev)
{
	struct s3c_platform_post *pdata;
	struct s3c_post_control *ctrl;
	struct resource *res;
	int id = pdev->id;

	pdata = to_post_plat(&pdev->dev);

	ctrl = &s3c_post.ctrl;
	ctrl->id = id;
	ctrl->dev = &pdev->dev;
	ctrl->vd = &s3c_post_video_device;
	ctrl->vd->minor = S3C_POST_MINOR;
	ctrl->out_frame.nr_frames = pdata->nr_frames;
	ctrl->scaler.line_length = pdata->line_length;

	sprintf(ctrl->name, "%s", S3C_POST_NAME);
	strcpy(ctrl->vd->name, ctrl->name);

	ctrl->open_lcdfifo = s3cfb_enable_local;
	ctrl->close_lcdfifo = s3cfb_enable_dma;

	atomic_set(&ctrl->in_use, 0);
	mutex_init(&ctrl->lock);
	init_waitqueue_head(&ctrl->waitq);

	/* get resource for io memory */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		err("failed to get io memory region\n");
		return NULL;
	}

	/* request mem region */
	res = request_mem_region(res->start, res->end - res->start + 1, pdev->name);
	if (!res) {
		err("failed to request io memory region\n");
		return NULL;
	}

	/* ioremap for register block */
	ctrl->regs = ioremap(res->start, res->end - res->start + 1);
	if (!ctrl->regs) {
		err("failed to remap io region\n");
		return NULL;
	}

	/* irq */
	ctrl->irq = platform_get_irq(pdev, 0);
	if (request_irq(ctrl->irq, s3c_post_irq, IRQF_DISABLED, ctrl->name, ctrl))
		err("request_irq failed\n");

	s3c_post_reset(ctrl);

	return ctrl;
}

static int s3c_post_unregister_controller(struct platform_device *pdev)
{
	struct s3c_post_control *ctrl;
	struct s3c_platform_post *pdata;

	ctrl = &s3c_post.ctrl;

	s3c_post_free_output_memory(&ctrl->out_frame);

	pdata = to_post_plat(ctrl->dev);

	iounmap(ctrl->regs);
	memset(ctrl, 0, sizeof(*ctrl));
	
	return 0;
}

static int s3c_post_mmap(struct file* filp, struct vm_area_struct *vma)
{
	struct s3c_post_control *ctrl = filp->private_data;
	struct s3c_post_out_frame *frame = &ctrl->out_frame;

	u32 size = vma->vm_end - vma->vm_start;
	u32 pfn, total_size = frame->buf_size;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= VM_RESERVED;

	/* page frame number of the address for a source frame to be stored at. */
	pfn = __phys_to_pfn(frame->addr[vma->vm_pgoff].phys_y);

	if (size > total_size) {
		err("the size of mapping is too big\n");
		return -EINVAL;
	}

	if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED)) {
		err("writable mapping must be shared\n");
		return -EINVAL;
	}

	if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot)) {
		err("mmap fail\n");
		return -EINVAL;
	}

	return 0;
}

static u32 s3c_post_poll(struct file *filp, poll_table *wait)
{
	struct s3c_post_control *ctrl = filp->private_data;
	u32 mask = 0;

	poll_wait(filp, &ctrl->waitq, wait);

	/* fixme */
	mask = POLLIN | POLLRDNORM;

	return mask;
}

static
ssize_t s3c_post_read(struct file *filp, char *buf, size_t count, loff_t *pos)
{
	struct s3c_post_control *ctrl = filp->private_data;
	size_t end;

	end = min_t(size_t, ctrl->out_frame.buf_size, count);

	if (copy_to_user(buf, s3c_post_get_current_frame(ctrl), end))
		return -EFAULT;

	return end;
}

static
ssize_t s3c_post_write(struct file *filp, const char *b, size_t c, loff_t *offset)
{
	return 0;
}

static int s3c_post_open(struct inode *inode, struct file *filp)
{
	struct s3c_post_control *ctrl;
	int ret;

	ctrl = &s3c_post.ctrl;

	mutex_lock(&ctrl->lock);

	if (atomic_read(&ctrl->in_use)) {
		ret = -EBUSY;
		goto resource_busy;
	} else {
		atomic_inc(&ctrl->in_use);
		s3c_post_reset(ctrl);
		filp->private_data = ctrl;
	}

	mutex_unlock(&ctrl->lock);

	return 0;

resource_busy:
	mutex_unlock(&ctrl->lock);
	return ret;
}

static int s3c_post_release(struct inode *inode, struct file *filp)
{
	struct s3c_post_control *ctrl;

	ctrl = &s3c_post.ctrl;

	mutex_lock(&ctrl->lock);

	atomic_dec(&ctrl->in_use);
	filp->private_data = NULL;

	mutex_unlock(&ctrl->lock);

	return 0;
}

static const struct file_operations s3c_post_fops = {
	.owner = THIS_MODULE,
	.open = s3c_post_open,
	.release = s3c_post_release,
	.ioctl = video_ioctl2,
	.read = s3c_post_read,
	.write = s3c_post_write,
	.mmap = s3c_post_mmap,
	.poll = s3c_post_poll,
};

static void s3c_post_vdev_release(struct video_device *vdev)
{
	kfree(vdev);
}

struct video_device s3c_post_video_device = {
	.vfl_type = VID_TYPE_CAPTURE | VID_TYPE_CLIPPING | VID_TYPE_SCALES,
	.fops = &s3c_post_fops,
	.ioctl_ops = &s3c_post_v4l2_ops,
	.release  = s3c_post_vdev_release,
};

static int s3c_post_init_global(struct platform_device *pdev)
{
	s3c_post.dma_start = s3c_get_media_memory(S3C_MDEV_POST);
	s3c_post.dma_total = s3c_get_media_memsize(S3C_MDEV_POST);
	s3c_post.dma_current = s3c_post.dma_start;

	return 0;
}

static int s3c_post_probe(struct platform_device *pdev)
{
	struct s3c_platform_post *pdata;
	struct s3c_post_control *ctrl;
	int ret;

	ctrl = s3c_post_register_controller(pdev);
	if (!ctrl) {
		err("cannot register post controller\n");
		goto err_post;
	}

	pdata = to_post_plat(&pdev->dev);
	if (pdata->cfg_gpio)
		pdata->cfg_gpio(pdev);

	/* post clock */
	ctrl->clock = clk_get(&pdev->dev, pdata->clk_name);
	if (IS_ERR(ctrl->clock)) {
		err("failed to get post clock source\n");
		goto err_clk_io;
	}
	/* set clockrate for POST interface block */
	if (ctrl->clock->set_rate)
		ctrl->clock->set_rate(ctrl->clock, pdata->clockrate);

	clk_enable(ctrl->clock);

	/* things to initialize once */
	ret = s3c_post_init_global(pdev);
	if (ret)
		goto err_global;

	ret = video_register_device(ctrl->vd, VFL_TYPE_GRABBER, pdev->id);
	if (ret) {
		err("cannot register video driver\n");
		goto err_video;
	}

	info("registered successfully\n");

	return 0;

err_video:
err_global:
	clk_disable(ctrl->clock);
	clk_put(ctrl->clock);

err_clk_io:
	s3c_post_unregister_controller(pdev);

err_post:
	return -EINVAL;
	
}

static int s3c_post_remove(struct platform_device *pdev)
{
	s3c_post_unregister_controller(pdev);

	return 0;
}

int s3c_post_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

int s3c_post_resume(struct platform_device *dev)
{
	return 0;
}

static struct platform_driver s3c_post_driver = {
	.probe		= s3c_post_probe,
	.remove		= s3c_post_remove,
	.suspend	= s3c_post_suspend,
	.resume		= s3c_post_resume,
	.driver		= {
		.name	= "s3c-post",
		.owner	= THIS_MODULE,
	},
};

static int s3c_post_register(void)
{
	platform_driver_register(&s3c_post_driver);

	return 0;
}

static void s3c_post_unregister(void)
{
	platform_driver_unregister(&s3c_post_driver);
}

module_init(s3c_post_register);
module_exit(s3c_post_unregister);
	
MODULE_AUTHOR("Jinsung, Yang <jsgood.yang@samsung.com>");
MODULE_DESCRIPTION("Samsung Post Processor driver");
MODULE_LICENSE("GPL");
