/* linux/drivers/media/video/samsung/tv20/s5pv210/cec_s5pv210.c
 *
 * cec ftn file for Samsung TVOut driver
 *
 * Copyright (c) 2010 Samsung Electronics
 *	         http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <plat/regs-cec.h>

#include "../cec.h"
#include "tv_out_s5pv210.h"

#ifdef CECDEBUG
#define CECPRINTK(fmt, args...) \
	pr_debug("\t\t[CEC] %s: " fmt, __func__ , ## args)
#else
#define CECPRINTK(fmt, args...)
#endif

static struct resource	*g_cec_mem;
static void __iomem	*g_cec_base;

#define S5P_HDMI_FIN	24000000
#define CEC_DIV_RATIO   320000

void s5p_cec_set_divider(void)
{
	u32 div_ratio, reg, div_val;

	div_ratio  = S5P_HDMI_FIN/CEC_DIV_RATIO - 1;

	reg = readl(S5P_HDMI_PHY_CONTROL);
	reg = (reg & ~(0x3FF<<16)) | (div_ratio << 16);

	writel(reg, S5P_HDMI_PHY_CONTROL);

	div_val = CEC_DIV_RATIO * 0.00005 - 1;

	writeb(0x0, g_cec_base + CEC_DIVISOR_3);
	writeb(0x0, g_cec_base + CEC_DIVISOR_2);
	writeb(0x0, g_cec_base + CEC_DIVISOR_1);
	writeb(div_val, g_cec_base + CEC_DIVISOR_0);

	CECPRINTK("CEC_DIVISOR_3 = 0x%08x\n", readb(g_cec_base + CEC_DIVISOR_3));
	CECPRINTK("CEC_DIVISOR_2 = 0x%08x\n", readb(g_cec_base + CEC_DIVISOR_2));
	CECPRINTK("CEC_DIVISOR_1 = 0x%08x\n", readb(g_cec_base + CEC_DIVISOR_1));
	CECPRINTK("CEC_DIVISOR_0 = 0x%08x\n", readb(g_cec_base + CEC_DIVISOR_0));
}

void s5p_cec_enable_rx(void)
{
	u8 reg;
	reg = readb(g_cec_base + CEC_RX_CTRL);
	reg |= CEC_RX_CTRL_ENABLE;
	writeb(reg, g_cec_base + CEC_RX_CTRL);

	CECPRINTK("CEC_RX_CTRL = 0x%08x\n", readb(g_cec_base + CEC_RX_CTRL));
}

void s5p_cec_mask_rx_interrupts(void)
{
	u8 reg;
	reg = readb(g_cec_base + CEC_IRQ_MASK);
	reg |= CEC_IRQ_RX_DONE;
	reg |= CEC_IRQ_RX_ERROR;
	writeb(reg, g_cec_base + CEC_IRQ_MASK);

	CECPRINTK("CEC_IRQ_MASK = 0x%08x\n", readb(g_cec_base + CEC_IRQ_MASK));
}

void s5p_cec_unmask_rx_interrupts(void)
{
	u8 reg;
	reg = readb(g_cec_base + CEC_IRQ_MASK);
	reg &= ~CEC_IRQ_RX_DONE;
	reg &= ~CEC_IRQ_RX_ERROR;
	writeb(reg, g_cec_base + CEC_IRQ_MASK);

	CECPRINTK("CEC_IRQ_MASK = 0x%08x\n", readb(g_cec_base + CEC_IRQ_MASK));
}

void s5p_cec_mask_tx_interrupts(void)
{
	u8 reg;
	reg = readb(g_cec_base + CEC_IRQ_MASK);
	reg |= CEC_IRQ_TX_DONE;
	reg |= CEC_IRQ_TX_ERROR;
	writeb(reg, g_cec_base + CEC_IRQ_MASK);

	CECPRINTK("CEC_IRQ_MASK = 0x%08x\n", readb(g_cec_base + CEC_IRQ_MASK));

}

void s5p_cec_unmask_tx_interrupts(void)
{
	u8 reg;
	reg = readb(g_cec_base + CEC_IRQ_MASK);
	reg &= ~CEC_IRQ_TX_DONE;
	reg &= ~CEC_IRQ_TX_ERROR;
	writeb(reg, g_cec_base + CEC_IRQ_MASK);

	CECPRINTK("CEC_IRQ_MASK = 0x%08x\n", readb(g_cec_base + CEC_IRQ_MASK));
}

void s5p_cec_reset(void)
{
	writeb(CEC_RX_CTRL_RESET, g_cec_base + CEC_RX_CTRL);
	writeb(CEC_TX_CTRL_RESET, g_cec_base + CEC_TX_CTRL);

	CECPRINTK("CEC_RX_CTRL = 0x%08x\n", readb(g_cec_base + CEC_RX_CTRL));
	CECPRINTK("CEC_TX_CTRL = 0x%08x\n", readb(g_cec_base + CEC_TX_CTRL));
}

void s5p_cec_tx_reset(void)
{
	writeb(CEC_TX_CTRL_RESET, g_cec_base + CEC_TX_CTRL);

	CECPRINTK("CEC_TX_CTRL = 0x%08x\n", readb(g_cec_base + CEC_TX_CTRL));
}

void s5p_cec_rx_reset(void)
{
	writeb(CEC_RX_CTRL_RESET, g_cec_base + CEC_RX_CTRL);
	CECPRINTK("CEC_RX_CTRL = 0x%08x\n", readb(g_cec_base + CEC_RX_CTRL));
}

void s5p_cec_threshold(void)
{
	writeb(CEC_FILTER_THRESHOLD, g_cec_base + CEC_RX_FILTER_TH);
	writeb(0, g_cec_base + CEC_RX_FILTER_CTRL);
	CECPRINTK("CEC_RX_FILTER_TH = 0x%08x\n",
		readb(g_cec_base + CEC_RX_FILTER_TH));
}

void s5p_cec_copy_packet(char *data, size_t count)
{
	int i = 0;
	u8 reg;
	/* copy packet to hardware buffer */
	while (i < count) {
		writeb(data[i], g_cec_base + (CEC_TX_BUFF0 + (i*4)));
		i++;
	}

	/* set number of bytes to transfer */
	writeb(count, g_cec_base + CEC_TX_BYTES);

	s5p_cec_set_tx_state(STATE_TX);

	/* start transfer */
	reg = readb(g_cec_base + CEC_TX_CTRL);

	reg |= CEC_TX_CTRL_START;

	/* if message is broadcast message - set corresponding bit */
	if ((data[0] & CEC_MESSAGE_BROADCAST_MASK) == CEC_MESSAGE_BROADCAST)
		reg |= CEC_TX_CTRL_BCAST;
	else
		reg &= ~CEC_TX_CTRL_BCAST;

	/* set number of retransmissions */
	reg |= 0x50;

	writeb(reg, g_cec_base + CEC_TX_CTRL);
}

void s5p_cec_set_addr(u32 addr)
{
	writeb(addr & 0x0F, g_cec_base + CEC_LOGIC_ADDR);
}

u32 s5p_cec_get_status(void)
{
	u32 status = 0;

	status = readb(g_cec_base + CEC_STATUS_0);
	status |= readb(g_cec_base + CEC_STATUS_1) << 8;
	status |= readb(g_cec_base + CEC_STATUS_2) << 16;
	status |= readb(g_cec_base + CEC_STATUS_3) << 24;

	CECPRINTK("status = 0x%x!\n", status);

	return status;
}

void s5p_clr_pending_tx(void)
{
	/* clear interrupt pending bit */
	writeb(CEC_IRQ_TX_DONE | CEC_IRQ_TX_ERROR, g_cec_base + CEC_IRQ_CLEAR);
}

void s5p_clr_pending_rx(void)
{
	/* clear interrupt pending bit */
	writeb(CEC_IRQ_RX_DONE | CEC_IRQ_RX_ERROR, g_cec_base + CEC_IRQ_CLEAR);
}

void s5p_cec_get_rx_buf(u32 size, u8 *buffer)
{
	u32 i = 0;

	while (i < size) {
		buffer[i] = readb(g_cec_base + CEC_RX_BUFF0 + (i * 4));
		i++;
	}
}

int __init s5p_cec_probe_core(struct platform_device *pdev)
{
	struct resource *res;
	size_t	size;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev,
			"failed to get memory region resource for cec\n");
		goto err_platform_get_resource_fail;
	}

	size = (res->end - res->start) + 1;

	g_cec_mem = request_mem_region(res->start, size, pdev->name);
	if (g_cec_mem == NULL) {
		dev_err(&pdev->dev,
			"failed to get memory region for cec\n");

		goto err_request_mem_region_fail;
	}

	g_cec_base = ioremap(res->start, size);
	if (g_cec_base == NULL) {
		dev_err(&pdev->dev,
			"failed to ioremap address region for cec\n");
		goto err_ioremap_fail;
	}

	return 0;

err_ioremap_fail:
	release_resource(g_cec_mem);
	kfree(g_cec_mem);
	g_cec_mem = NULL;
err_request_mem_region_fail:
err_platform_get_resource_fail:
	return -ENOENT;
}

int __init s5p_cec_release_core(struct platform_device *pdev)
{
	if (g_cec_base) {
		iounmap(g_cec_base);
		g_cec_base = NULL;
	}

	/* remove memory region */
	if (g_cec_mem) {
		if (release_resource(g_cec_mem))
			dev_err(&pdev->dev,
				"Can't remove tvout drv !!\n");

		kfree(g_cec_mem);
		g_cec_mem = NULL;
	}

	return 0;
}
