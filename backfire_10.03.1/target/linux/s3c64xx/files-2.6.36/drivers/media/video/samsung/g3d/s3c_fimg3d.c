/* linux/drivers/video/samsung/g3d/s3c_fimg3d.c
 *
 * Driver file for Samsung 3D Accelerator(FIMG-3D)
 *
 * Jegeon Jung, Copyright (c) 2009 Samsung Electronics
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
#include <linux/mman.h>
#include <linux/vmalloc.h>
#include <asm/io.h>
#include <mach/map.h>

#include "s3c_fimg3d.h"

#define DEBUG_S3C_G3D
#undef	DEBUG_S3C_G3D

#ifdef DEBUG_S3C_G3D
#define DEBUG(fmt,args...) printk(fmt, ##args)
#else
#define DEBUG(fmt,args...) do {} while(0)
#endif

static DWORD	BACKUP_FGHI_CONTROL;
//static DWORD	BACKUP_FGHI_IDXOFFSET;
//static DWORD	BACKUP_FGHI_VBADDR;
static DWORD	BACKUP_FGHI_ATTRIBS[10];
//static DWORD	BACKUP_FGHI_ATTRIBS_VBCTRL[10];
//static DWORD	BACKUP_FGHI_ATTRIBS_VBBASE[10];

static DWORD	BACKUP_FGVS_INSTMEM[2048];
static DWORD	BACKUP_FGVS_CFLOAT[1024];
static DWORD	BACKUP_FGVS_CINT[16];
static DWORD	BACKUP_FGVS_CBOOL;
static DWORD	BACKUP_FGVS_CONFIG;
static DWORD	BACKUP_FGVS_PCRANGE_OUTATTR[8];

static DWORD	BACKUP_FGPE[7];

static DWORD	BACKUP_FGRA_PIXSAMP_YCLIP[7];
static DWORD	BACKUP_FGRA_LODCTL;
static DWORD	BACKUP_FGRA_CLIPX;
static DWORD	BACKUP_FGRA_PWIDTH_LWIDTH[5];

static DWORD	BACKUP_FGPS_INSTMEM[2048];
static DWORD	BACKUP_FGPS_CFLOAT[1024];
static DWORD	BACKUP_FGPS_CINT[16];
static DWORD	BACKUP_FGPS_CBOOL;
static DWORD	BACKUP_FGPS_EXEMODE_ATTRIBUTENUM[5];

static DWORD	BACKUP_FGTU_TEXTURE0[18];
static DWORD	BACKUP_FGTU_TEXTURE1[18];
static DWORD	BACKUP_FGTU_TEXTURE2[18];
static DWORD	BACKUP_FGTU_TEXTURE3[18];
static DWORD	BACKUP_FGTU_TEXTURE4[18];
static DWORD	BACKUP_FGTU_TEXTURE5[18];
static DWORD	BACKUP_FGTU_TEXTURE6[18];
static DWORD	BACKUP_FGTU_TEXTURE7[18];
static DWORD	BACKUP_FGTU_COLORKEYS[6];
static DWORD	BACKUP_FGVTU_STATUSES_VTBADDRS[8];

static DWORD	BACKUP_FGPF[15];


#define G3D_IOCTL_MAGIC			'S'
#define GET_CONFIG			_IO(G3D_IOCTL_MAGIC, 101)
#define WAIT_FOR_FLUSH			_IO(G3D_IOCTL_MAGIC, 100)

#define S3C_3D_MEM_ALLOC		_IOWR(G3D_IOCTL_MAGIC, 310, struct s3c_3d_mem_alloc)
#define S3C_3D_MEM_FREE			_IOWR(G3D_IOCTL_MAGIC, 311, struct s3c_3d_mem_alloc)
#define S3C_3D_SFR_LOCK			_IO(G3D_IOCTL_MAGIC, 312)
#define S3C_3D_SFR_UNLOCK		_IO(G3D_IOCTL_MAGIC, 313)
#define S3C_3D_MEM_ALLOC_SHARE		_IOWR(G3D_IOCTL_MAGIC, 314, struct s3c_3d_mem_alloc)
#define S3C_3D_MEM_SHARE_FREE		_IOWR(G3D_IOCTL_MAGIC, 315, struct s3c_3d_mem_alloc)

#define MEM_ALLOC			1
#define MEM_ALLOC_SHARE			2

#define PFX				"s3c_g3d"
#define G3D_MINOR			249

static wait_queue_head_t waitq;
static struct resource *s3c_g3d_mem;
static void __iomem *s3c_g3d_base;
static int s3c_g3d_irq;
static struct clk *g3d_clock;
static struct clk *h_clk;

static DEFINE_MUTEX(mem_alloc_lock);
static DEFINE_MUTEX(mem_free_lock);
static DEFINE_MUTEX(mem_sfr_lock);

static DEFINE_MUTEX(mem_alloc_share_lock);
static DEFINE_MUTEX(mem_share_free_lock);

void *dma_3d_done;

struct s3c_3d_mem_alloc {
	int		size;
	unsigned int 	vir_addr;
	unsigned int 	phy_addr;
};

static unsigned int mutex_lock_processID = 0;

static int flag = 0;

static unsigned int physical_address;

int interrupt_already_recevied;

unsigned int s3c_g3d_base_physical;


///////////// for check memory leak
//*-------------------------------------------------------------------------*/
typedef struct _memalloc_desc
{
	int		size;
	unsigned int 	vir_addr;
	unsigned int 	phy_addr;	
	struct _memalloc_desc*  next;
} Memalloc_desc;

typedef struct _openContext
{
    Memalloc_desc* allocatedList;
} OpenContext;

void grabageCollect(int *newid);
/////////////////////////////////////


irqreturn_t s3c_g3d_isr(int irq, void *dev_id)
{
	__raw_writel(0, s3c_g3d_base + FGGB_INTPENDING);

	interrupt_already_recevied = 1;
	wake_up_interruptible(&waitq);

	return IRQ_HANDLED;
}


int s3c_g3d_open(struct inode *inode, struct file *file)
{
    int *newid;
    newid = (int*)vmalloc(sizeof(OpenContext));
    memset(newid, 0x0, sizeof(OpenContext));
    
    file->private_data = newid;
	return 0;
}

int s3c_g3d_release(struct inode *inode, struct file *file)
{
    int *newid = file->private_data;
    if(mutex_lock_processID != 0 && mutex_lock_processID == (unsigned int)file->private_data)
    {
        mutex_unlock(&mem_sfr_lock);
        printk("Abnormal close of pid # %d\n", task_pid_nr(current));        
    }
    
    grabageCollect(newid);
    vfree((OpenContext*)newid);

	return 0;
}


static int s3c_g3d_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	u32 val;
	DECLARE_COMPLETION_ONSTACK(complete);

	unsigned long *virt_addr;
	struct mm_struct *mm = current->mm;
	struct s3c_3d_mem_alloc param;
	
	Memalloc_desc   *memdesc, *curDesc, *prevDesc;
	OpenContext*    pOpenCtx = (OpenContext*)file->private_data;

	switch (cmd) {
	case WAIT_FOR_FLUSH:

		//if fifo has already been flushed, return;
		val = __raw_readl(s3c_g3d_base+FGGB_PIPESTATE);
		//printk("read pipestate = 0x%x\n",val);
		if((val & arg) ==0) break;

		// enable interrupt
		interrupt_already_recevied = 0;
		__raw_writel(0x0001171f,s3c_g3d_base+FGGB_PIPEMASK);
		__raw_writel(1,s3c_g3d_base+FGGB_INTMASK);

		//printk("wait for flush (arg=0x%lx)\n",arg);


		while(1) {
			wait_event_interruptible(waitq, (interrupt_already_recevied>0));
			__raw_writel(0,s3c_g3d_base+FGGB_INTMASK);
			interrupt_already_recevied = 0;
			//if(interrupt_already_recevied==0)interruptible_sleep_on(&waitq);
			val = __raw_readl(s3c_g3d_base+FGGB_PIPESTATE);
			//printk("in while read pipestate = 0x%x\n",val);
			if(val & arg) {
			} else {
				break;
			}
			__raw_writel(1,s3c_g3d_base+FGGB_INTMASK);
		}
		break;

	case GET_CONFIG:
//		copy_to_user((void *)arg,&g3d_config,sizeof(G3D_CONFIG_STRUCT));
		break;

	case S3C_3D_MEM_ALLOC:		
		mutex_lock(&mem_alloc_lock);
		if(copy_from_user(&param, (struct s3c_3d_mem_alloc *)arg, sizeof(struct s3c_3d_mem_alloc))) {
			mutex_unlock(&mem_alloc_lock);			
			return -EFAULT;
		}
		flag = MEM_ALLOC;

		param.vir_addr = do_mmap(file, 0, param.size, PROT_READ|PROT_WRITE, MAP_SHARED, 0);
		DEBUG("param.vir_addr = %08x\n", param.vir_addr);

		if(param.vir_addr == -EINVAL) {
			printk("S3C_3D_MEM_ALLOC FAILED\n");
			flag = 0;
			mutex_unlock(&mem_alloc_lock);			
			return -EFAULT;
		}
		param.phy_addr = physical_address;

		// printk("alloc %d\n", param.size);
		DEBUG("KERNEL MALLOC : param.phy_addr = 0x%X \t size = %d \t param.vir_addr = 0x%X\n", param.phy_addr, param.size, param.vir_addr);

		if(copy_to_user((struct s3c_3d_mem_alloc *)arg, &param, sizeof(struct s3c_3d_mem_alloc))) {
			flag = 0;
			mutex_unlock(&mem_alloc_lock);
			return -EFAULT;		
		}

		flag = 0;
		
		//////////////////////////////////
		// for memory leak
		memdesc = (Memalloc_desc*)vmalloc(sizeof(Memalloc_desc));
		memdesc->size = param.size;
		memdesc->vir_addr = param.vir_addr;
		memdesc->phy_addr = param.phy_addr;
		memdesc->next = NULL;
		prevDesc = NULL;
		for(curDesc = pOpenCtx->allocatedList; curDesc != NULL; curDesc = curDesc->next) {
		    prevDesc = curDesc;
		}
		
		if(prevDesc == NULL) pOpenCtx->allocatedList = memdesc;
		else prevDesc->next = memdesc;
		//////////////////////////////////
		
		mutex_unlock(&mem_alloc_lock);
		
		break;

	case S3C_3D_MEM_FREE:	
		mutex_lock(&mem_free_lock);
		if(copy_from_user(&param, (struct s3c_3d_mem_alloc *)arg, sizeof(struct s3c_3d_mem_alloc))) {
			mutex_unlock(&mem_free_lock);
			return -EFAULT;
		}

		DEBUG("KERNEL FREE : param.phy_addr = 0x%X \t size = %d \t param.vir_addr = 0x%X\n", param.phy_addr, param.size, param.vir_addr);

		if (do_munmap(mm, param.vir_addr, param.size) < 0) {
			printk("do_munmap() failed !!\n");
			mutex_unlock(&mem_free_lock);
			return -EINVAL;
		}
		virt_addr = (unsigned long *)phys_to_virt(param.phy_addr);
		//printk("KERNEL : virt_addr = 0x%X\n", virt_addr);
		//printk("free %d\n", param.size);

		kfree(virt_addr);
		param.size = 0;
		DEBUG("do_munmap() succeed !!\n");

		if(copy_to_user((struct s3c_3d_mem_alloc *)arg, &param, sizeof(struct s3c_3d_mem_alloc))) {
			mutex_unlock(&mem_free_lock);
			return -EFAULT;
		}
		
		//////////////////////////////////
		// for memory leak
		prevDesc = NULL;
		for(curDesc = pOpenCtx->allocatedList; curDesc != NULL; curDesc = curDesc->next) {
		    if(curDesc->vir_addr == param.vir_addr) break;
		    prevDesc = curDesc;
		}
		
	        if(prevDesc != NULL)
			prevDesc->next = curDesc->next;    
	        else 
			pOpenCtx->allocatedList = NULL;	
        
		vfree(curDesc);
		//////////////////////////////////		
		
		mutex_unlock(&mem_free_lock);
		
		break;

	case S3C_3D_SFR_LOCK:
		mutex_lock(&mem_sfr_lock);
		mutex_lock_processID = (unsigned int)file->private_data;
		DEBUG("s3c_g3d_ioctl() : You got a muxtex lock !!\n");
		break;

	case S3C_3D_SFR_UNLOCK:
		mutex_lock_processID = 0;
		mutex_unlock(&mem_sfr_lock);
		DEBUG("s3c_g3d_ioctl() : The muxtex unlock called !!\n");
		break;

	case S3C_3D_MEM_ALLOC_SHARE:		
		mutex_lock(&mem_alloc_share_lock);
		if(copy_from_user(&param, (struct s3c_3d_mem_alloc *)arg, sizeof(struct s3c_3d_mem_alloc))) {
			mutex_unlock(&mem_alloc_share_lock);
			return -EFAULT;
		}
		flag = MEM_ALLOC_SHARE;

		physical_address = param.phy_addr;
		DEBUG("param.phy_addr = %08x\n", physical_address);

		param.vir_addr = do_mmap(file, 0, param.size, PROT_READ|PROT_WRITE, MAP_SHARED, 0);
		DEBUG("param.vir_addr = %08x\n", param.vir_addr);

		if(param.vir_addr == -EINVAL) {
			printk("S3C_3D_MEM_ALLOC_SHARE FAILED\n");
			flag = 0;
			mutex_unlock(&mem_alloc_share_lock);
			return -EFAULT;
		}

		DEBUG("MALLOC_SHARE : param.phy_addr = 0x%X \t size = %d \t param.vir_addr = 0x%X\n", param.phy_addr, param.size, param.vir_addr);

		if(copy_to_user((struct s3c_3d_mem_alloc *)arg, &param, sizeof(struct s3c_3d_mem_alloc))) {
			flag = 0;
			mutex_unlock(&mem_alloc_share_lock);
			return -EFAULT;		
		}

		flag = 0;
		
		mutex_unlock(&mem_alloc_share_lock);
		
		break;

	case S3C_3D_MEM_SHARE_FREE:	
		mutex_lock(&mem_share_free_lock);
		if(copy_from_user(&param, (struct s3c_3d_mem_alloc *)arg, sizeof(struct s3c_3d_mem_alloc))) {
			mutex_unlock(&mem_share_free_lock);
			return -EFAULT;		
		}

		DEBUG("MEM_SHARE_FREE : param.phy_addr = 0x%X \t size = %d \t param.vir_addr = 0x%X\n", param.phy_addr, param.size, param.vir_addr);

		if (do_munmap(mm, param.vir_addr, param.size) < 0) {
			printk("do_munmap() failed - MEM_SHARE_FREE!!\n");
			mutex_unlock(&mem_share_free_lock);
			return -EINVAL;
		}

		param.vir_addr = 0;
		DEBUG("do_munmap() succeed !! - MEM_SHARE_FREE\n");

		if(copy_to_user((struct s3c_3d_mem_alloc *)arg, &param, sizeof(struct s3c_3d_mem_alloc))) {
			mutex_unlock(&mem_share_free_lock);
			return -EFAULT;		
		}

		mutex_unlock(&mem_share_free_lock);
		
		break;
		
	default:
		DEBUG("s3c_g3d_ioctl() : default !!\n");
		return -EINVAL;
	}
	
	return 0;
}


int s3c_g3d_mmap(struct file* filp, struct vm_area_struct *vma)
{
	unsigned long pageFrameNo, size, phys_addr;
	unsigned long *virt_addr;

	size = vma->vm_end - vma->vm_start;

	switch (flag) { 
	case MEM_ALLOC :
		virt_addr = (unsigned long *)kmalloc(size, GFP_KERNEL);

		if (virt_addr == NULL) {
			printk("kmalloc() failed !\n");
			return -EINVAL;
		}
		DEBUG("MMAP_KMALLOC : virt addr = 0x%p, size = %d\n", virt_addr, size);
		phys_addr = virt_to_phys(virt_addr);
		physical_address = (unsigned int)phys_addr;

		//DEBUG("MMAP_KMALLOC : phys addr = 0x%p\n", phys_addr);
		pageFrameNo = __phys_to_pfn(phys_addr);
		//DEBUG("MMAP_KMALLOC : PFN = 0x%x\n", pageFrameNo);
		break;
		
	case MEM_ALLOC_SHARE :
		DEBUG("MMAP_KMALLOC_SHARE : phys addr = 0x%p\n", physical_address);
		
		// page frame number of the address for the physical_address to be shared.
		pageFrameNo = __phys_to_pfn(physical_address);
		//DEBUG("MMAP_KMALLOC_SHARE: PFN = 0x%x\n", pageFrameNo);
		DEBUG("MMAP_KMALLOC_SHARE : vma->end = 0x%p, vma->start = 0x%p, size = %d\n", vma->vm_end, vma->vm_start, size);
		break;
		
	default :
		// page frame number of the address for a source G2D_SFR_SIZE to be stored at.
		pageFrameNo = __phys_to_pfn(s3c_g3d_base_physical);
		DEBUG("MMAP : vma->end = 0x%p, vma->start = 0x%p, size = %d\n", vma->vm_end, vma->vm_start, size);

		if(size > (s3c_g3d_mem->end-s3c_g3d_mem->start+1)) {
			printk("The size of G3D_SFR_SIZE mapping is too big!\n");
			return -EINVAL;
		}
		break;
	}
	
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED)) {
		printk("s3c_g3d_mmap() : Writable G3D_SFR_SIZE mapping must be shared !\n");
		return -EINVAL;
	}

	if (remap_pfn_range(vma, vma->vm_start, pageFrameNo, size, vma->vm_page_prot)) {
		printk("s3c_g3d_mmap() : remap_pfn_range() failed !\n");
		return -EINVAL;
	}

	return 0;
}


void grabageCollect(int* newid)
{
    unsigned long *virt_addr;
    struct mm_struct *mm = current->mm;
    OpenContext*    pOpenCtx = (OpenContext*)newid;
	Memalloc_desc   *curDesc, *nextDesc;    
    
    mutex_lock(&mem_free_lock);
    for(curDesc = pOpenCtx->allocatedList;curDesc != NULL; ) {
        nextDesc = curDesc->next;
        printk("Deleting garbage address=0x%x size=%d\n", curDesc->vir_addr, curDesc->size);
	if (do_munmap(mm, curDesc->vir_addr, curDesc->size) < 0) {
		printk("do_munmap() failed !!\n");
	}
	
	virt_addr = (unsigned long *)phys_to_virt(curDesc->phy_addr);
	kfree(virt_addr);
            
        vfree(curDesc);
        curDesc = 0;            
        
        curDesc = nextDesc;
    }
    
    mutex_unlock(&mem_free_lock);       
}


static struct file_operations s3c_g3d_fops = {
	.owner 	= THIS_MODULE,
	.ioctl 	= s3c_g3d_ioctl,
	.open 	= s3c_g3d_open,
	.release = s3c_g3d_release,
	.mmap	= s3c_g3d_mmap,
};


static struct miscdevice s3c_g3d_dev = {
	.minor		= G3D_MINOR,
	.name		= "s3c-g3d",
	.fops		= &s3c_g3d_fops,
};


static int s3c_g3d_remove(struct platform_device *dev)
{
	//clk_disable(g3d_clock);
	printk(KERN_INFO "s3c_g3d_remove called !\n");

	free_irq(s3c_g3d_irq, NULL);

	if (s3c_g3d_mem != NULL) {
		pr_debug("s3c_g3d: releasing s3c_post_mem\n");
		iounmap(s3c_g3d_base);
		release_resource(s3c_g3d_mem);
		kfree(s3c_g3d_mem);
	}

	misc_deregister(&s3c_g3d_dev);
	printk(KERN_INFO "s3c_g3d_remove Success !\n");
	return 0;
}

int s3c_g3d_probe(struct platform_device *pdev)
{
	struct resource *res;

	int ret;
	int i;

	DEBUG("s3c_g3d probe() called\n");

	s3c_g3d_irq = platform_get_irq(pdev, 0);
	if(s3c_g3d_irq <= 0) {
		printk(KERN_ERR PFX "failed to get irq resouce\n");
		return -ENOENT;
	}

	ret = request_irq(s3c_g3d_irq, s3c_g3d_isr, IRQF_DISABLED, pdev->name, NULL);
	if (ret) {
		printk("request_irq(S3D) failed.\n");
		return ret;
	}

	/* get the memory region for the post processor driver */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(res == NULL) {
		printk(KERN_ERR PFX "failed to get memory region resouce\n");
		return -ENOENT;
	}

	s3c_g3d_base_physical = (unsigned int)res->start;

	s3c_g3d_mem = request_mem_region(res->start, res->end-res->start+1, pdev->name);
	if(s3c_g3d_mem == NULL) {
		printk(KERN_ERR PFX "failed to reserve memory region\n");
		return -ENOENT;
	}


	s3c_g3d_base = ioremap(s3c_g3d_mem->start, s3c_g3d_mem->end - res->start + 1);
	if(s3c_g3d_base == NULL) {
		printk(KERN_ERR PFX "failed ioremap\n");
		return -ENOENT;
	}

	g3d_clock = clk_get(&pdev->dev, "post");
	if(g3d_clock == NULL) {
		printk(KERN_ERR PFX "failed to find post clock source\n");
		return -ENOENT;
	}

	clk_enable(g3d_clock);

	h_clk = clk_get(&pdev->dev, "hclk");
	if(h_clk == NULL) {
		printk(KERN_ERR PFX "failed to find h_clk clock source\n");
		return -ENOENT;
	}

	init_waitqueue_head(&waitq);

	ret = misc_register(&s3c_g3d_dev);
	if (ret) {
		printk (KERN_ERR "cannot register miscdev on minor=%d (%d)\n",
				G3D_MINOR, ret);
		return ret;
	}

	// device reset
	__raw_writel(1,s3c_g3d_base+FGGB_RST);
	for(i=0;i<1000;i++);
	__raw_writel(0,s3c_g3d_base+FGGB_RST);
	for(i=0;i<1000;i++);

	printk("s3c_g3d version : 0x%x\n",__raw_readl(s3c_g3d_base + FGGB_VERSION));

	/* check to see if everything is setup correctly */
	return 0;
}

static int s3c_g3d_suspend(struct platform_device *dev, pm_message_t state)
{
	// backup Registers 
	// backup host interface registers.

	BACKUP_FGHI_CONTROL = __raw_readl(s3c_g3d_base + FGHI_HI_CTRL);
//	READREGP(FGHI_IDX_OFFSET, BACKUP_FGHI_IDXOFFSET);  // currently not used
//	READREGP(FGHI_VTXBUF_ADDR, BACKUP_FGHI_VBADDR);	 // currently not used
	memcpy(BACKUP_FGHI_ATTRIBS, (DWORD*)(s3c_g3d_base + FGHI_ATTR0), sizeof(DWORD)*10);
//	memcpy(BACKUP_FGHI_ATTRIBS_VBCTRL, (DWORD*)FGHI_VTXBUF_CTRL0, sizeof(DWORD)*10);	// currently not used
//	memcpy(BACKUP_FGHI_ATTRIBS_VBBASE, (DWORD*)BACKUP_FGHI_ATTRIBS_VBBASE, sizeof(DWORD)*10);	// currently not used


	// backup vertex shader registers
	memcpy(BACKUP_FGVS_INSTMEM, (DWORD*)(s3c_g3d_base + FGVS_INSTMEM_SADDR), sizeof(DWORD)*2048);	
	memcpy(BACKUP_FGVS_CFLOAT, (DWORD*)(s3c_g3d_base + FGVS_CFLOAT_SADDR), sizeof(DWORD)*1024);	
	memcpy(BACKUP_FGVS_CINT, (DWORD*)(s3c_g3d_base + FGVS_CINT_SADDR), sizeof(DWORD)*16);		
	BACKUP_FGVS_CBOOL = __raw_readl(s3c_g3d_base + FGVS_CBOOL_SADDR);		
	BACKUP_FGVS_CONFIG = __raw_readl(s3c_g3d_base + FGVS_CONFIG);
	memcpy(BACKUP_FGVS_PCRANGE_OUTATTR, (DWORD*)(s3c_g3d_base + FGVS_PC_RANGE), sizeof(DWORD)*8);

	
	// backup primitive engine registers
	memcpy(BACKUP_FGPE, (DWORD*)(s3c_g3d_base + FGPE_VTX_CONTEXT), sizeof(DWORD)*7);


	// backup raster engine registers
	memcpy(BACKUP_FGRA_PIXSAMP_YCLIP, (DWORD*)(s3c_g3d_base + FGRA_PIXEL_SAMPOS), sizeof(DWORD)*7);
	BACKUP_FGRA_LODCTL = __raw_readl(s3c_g3d_base + FGRA_LOD_CTRL);		
	BACKUP_FGRA_CLIPX = __raw_readl(s3c_g3d_base + FGRA_CLIP_XCORD);
	memcpy(BACKUP_FGRA_PWIDTH_LWIDTH, (DWORD*)(s3c_g3d_base + FGRA_POINT_WIDTH), sizeof(DWORD)*5);

	// backup pixel shader registers
	memcpy(BACKUP_FGPS_INSTMEM, (DWORD*)(s3c_g3d_base + FGPS_INSTMEM_SADDR), sizeof(DWORD)*2048);
	memcpy(BACKUP_FGPS_CFLOAT, (DWORD*)(s3c_g3d_base + FGPS_CFLOAT_SADDR), sizeof(DWORD)*1024);	
	memcpy(BACKUP_FGPS_CINT, (DWORD*)(s3c_g3d_base + FGPS_CINT_SADDR), sizeof(DWORD)*16);		
	BACKUP_FGPS_CBOOL = __raw_readl((s3c_g3d_base + FGPS_CBOOL_SADDR));	
	memcpy(BACKUP_FGPS_EXEMODE_ATTRIBUTENUM, (DWORD*)(s3c_g3d_base + FGPS_EXE_MODE), sizeof(DWORD)*5);

	// backup texture unit registers
	memcpy(BACKUP_FGTU_TEXTURE0, (DWORD*)(s3c_g3d_base + FGTU_TEX0_CTRL), sizeof(DWORD)*18);
	memcpy(BACKUP_FGTU_TEXTURE1, (DWORD*)(s3c_g3d_base + FGTU_TEX1_CTRL), sizeof(DWORD)*18);
	memcpy(BACKUP_FGTU_TEXTURE2, (DWORD*)(s3c_g3d_base + FGTU_TEX2_CTRL), sizeof(DWORD)*18);
	memcpy(BACKUP_FGTU_TEXTURE3, (DWORD*)(s3c_g3d_base + FGTU_TEX3_CTRL), sizeof(DWORD)*18);
	memcpy(BACKUP_FGTU_TEXTURE4, (DWORD*)(s3c_g3d_base + FGTU_TEX4_CTRL), sizeof(DWORD)*18);
	memcpy(BACKUP_FGTU_TEXTURE5, (DWORD*)(s3c_g3d_base + FGTU_TEX5_CTRL), sizeof(DWORD)*18);
	memcpy(BACKUP_FGTU_TEXTURE6, (DWORD*)(s3c_g3d_base + FGTU_TEX6_CTRL), sizeof(DWORD)*18);
	memcpy(BACKUP_FGTU_TEXTURE7, (DWORD*)(s3c_g3d_base + FGTU_TEX7_CTRL), sizeof(DWORD)*18);

	memcpy(BACKUP_FGTU_COLORKEYS, (DWORD*)(s3c_g3d_base + FGTU_COLOR_KEY1), sizeof(DWORD)*6);
	memcpy(BACKUP_FGVTU_STATUSES_VTBADDRS, (DWORD*)(s3c_g3d_base + FGTU_VTXTEX0_CTRL), sizeof(DWORD)*8);


	// backup per-fragment unit registers	
	memcpy(BACKUP_FGPF, (DWORD*)(s3c_g3d_base + FGPF_SCISSOR_XCORD), sizeof(DWORD)*15);	

	return 0;
}
static int s3c_g3d_resume(struct platform_device *pdev)
{
	//clk_enable(g3d_clock);
	int i;

	// restore host interface registers.

	__raw_writel(0, s3c_g3d_base+FGHI_HI_CTRL);

//	WRITEREG(FGHI_IDX_OFFSET, BACKUP_FGHI_IDXOFFSET);  // currently not used
//	WRITEREG(FGHI_VTXBUF_ADDR, BACKUP_FGHI_VBADDR);	 // currently not used
	memcpy((DWORD*)(s3c_g3d_base+FGHI_ATTR0), BACKUP_FGHI_ATTRIBS, sizeof(DWORD)*10);
//	memcpy((DWORD*)FGHI_VTXBUF_CTRL0, BACKUP_FGHI_ATTRIBS_VBCTRL, sizeof(DWORD)*10);	// currently not used
//	memcpy((DWORD*)BACKUP_FGHI_ATTRIBS_VBBASE, BACKUP_FGHI_ATTRIBS_VBBASE, sizeof(DWORD)*10);	// currently not used


	// restore vertex shader registers
	memcpy((DWORD*)(s3c_g3d_base+FGVS_INSTMEM_SADDR), BACKUP_FGVS_INSTMEM, sizeof(DWORD)*2048);	
	memcpy((DWORD*)(s3c_g3d_base+FGVS_CFLOAT_SADDR), BACKUP_FGVS_CFLOAT, sizeof(DWORD)*1024);	
	memcpy((DWORD*)(s3c_g3d_base+FGVS_CINT_SADDR), BACKUP_FGVS_CINT, sizeof(DWORD)*16);		
	__raw_writel(BACKUP_FGVS_CBOOL, s3c_g3d_base+FGVS_CBOOL_SADDR);		
	__raw_writel(BACKUP_FGVS_CONFIG, s3c_g3d_base+FGVS_CONFIG);
	memcpy((DWORD*)(s3c_g3d_base+FGVS_PC_RANGE), BACKUP_FGVS_PCRANGE_OUTATTR, sizeof(DWORD)*8);

	
	// restore primitive engine registers
	memcpy((DWORD*)(s3c_g3d_base+FGPE_VTX_CONTEXT), BACKUP_FGPE, sizeof(DWORD)*7);


	// restore raster engine registers
	memcpy((DWORD*)(s3c_g3d_base+FGRA_PIXEL_SAMPOS), BACKUP_FGRA_PIXSAMP_YCLIP, sizeof(DWORD)*7);
	__raw_writel(BACKUP_FGRA_LODCTL, s3c_g3d_base+FGRA_LOD_CTRL);		
	__raw_writel(BACKUP_FGRA_CLIPX, s3c_g3d_base+FGRA_CLIP_XCORD);
	memcpy((DWORD*)(s3c_g3d_base+FGRA_POINT_WIDTH), BACKUP_FGRA_PWIDTH_LWIDTH, sizeof(DWORD)*5);

	// restore pixel shader registers
	memcpy((DWORD*)(s3c_g3d_base+FGPS_INSTMEM_SADDR), BACKUP_FGPS_INSTMEM, sizeof(DWORD)*2048);
	memcpy((DWORD*)(s3c_g3d_base+FGPS_CFLOAT_SADDR), BACKUP_FGPS_CFLOAT, sizeof(DWORD)*1024);	
	memcpy((DWORD*)(s3c_g3d_base+FGPS_CINT_SADDR), BACKUP_FGPS_CINT, sizeof(DWORD)*16);		
	__raw_writel(BACKUP_FGPS_CBOOL, s3c_g3d_base+FGPS_CBOOL_SADDR);	
	memcpy((DWORD*)(s3c_g3d_base+FGPS_EXE_MODE), BACKUP_FGPS_EXEMODE_ATTRIBUTENUM, sizeof(DWORD)*5);


	// backup texture unit registers
	memcpy((DWORD*)(s3c_g3d_base+FGTU_TEX0_CTRL), BACKUP_FGTU_TEXTURE0, sizeof(DWORD)*18);
	memcpy((DWORD*)(s3c_g3d_base+FGTU_TEX1_CTRL), BACKUP_FGTU_TEXTURE1, sizeof(DWORD)*18);
	memcpy((DWORD*)(s3c_g3d_base+FGTU_TEX2_CTRL), BACKUP_FGTU_TEXTURE2, sizeof(DWORD)*18);
	memcpy((DWORD*)(s3c_g3d_base+FGTU_TEX3_CTRL), BACKUP_FGTU_TEXTURE3, sizeof(DWORD)*18);
	memcpy((DWORD*)(s3c_g3d_base+FGTU_TEX4_CTRL), BACKUP_FGTU_TEXTURE4, sizeof(DWORD)*18);
	memcpy((DWORD*)(s3c_g3d_base+FGTU_TEX5_CTRL), BACKUP_FGTU_TEXTURE5, sizeof(DWORD)*18);
	memcpy((DWORD*)(s3c_g3d_base+FGTU_TEX6_CTRL), BACKUP_FGTU_TEXTURE6, sizeof(DWORD)*18);
	memcpy((DWORD*)(s3c_g3d_base+FGTU_TEX7_CTRL), BACKUP_FGTU_TEXTURE7, sizeof(DWORD)*18);

	memcpy((DWORD*)(s3c_g3d_base+FGTU_COLOR_KEY1), BACKUP_FGTU_COLORKEYS, sizeof(DWORD)*6);
	memcpy((DWORD*)(s3c_g3d_base+FGTU_VTXTEX0_CTRL), BACKUP_FGVTU_STATUSES_VTBADDRS, sizeof(DWORD)*8);



	// backup per-fragment unit registers	
	memcpy((DWORD*)(s3c_g3d_base+FGPF_SCISSOR_XCORD), BACKUP_FGPF, sizeof(DWORD)*15);	


	__raw_writel(1,s3c_g3d_base+FGGB_RST);
	for(i=0;i<1000;i++);
	__raw_writel(0,s3c_g3d_base+FGGB_RST);
	for(i=0;i<1000;i++);

	
	return 0;
}

static struct platform_driver s3c_g3d_driver = {
	.probe          = s3c_g3d_probe,
	.remove         = s3c_g3d_remove,
	.suspend        = s3c_g3d_suspend,
	.resume         = s3c_g3d_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "s3c-g3d",
	},
};

static char banner[] __initdata = KERN_INFO "S3C G3D Driver, (c) 2007-2009 Samsung Electronics\n";

static char init_error[] __initdata = KERN_ERR "Intialization of S3C G3D driver is failed\n";

int __init  s3c_g3d_init(void)
{
	printk(banner);

	if(platform_driver_register(&s3c_g3d_driver)!=0) {
		printk(init_error);
		return -1;
	}

	printk(" S3C G3D Init : Done\n");
	return 0;
}

void  s3c_g3d_exit(void)
{
	platform_driver_unregister(&s3c_g3d_driver);

	printk("S3C G3D module exit\n");
}

module_init(s3c_g3d_init);
module_exit(s3c_g3d_exit);

MODULE_AUTHOR("jegeon.jung@samsung.com");
MODULE_DESCRIPTION("S3C G3D Device Driver");
MODULE_LICENSE("GPL");

