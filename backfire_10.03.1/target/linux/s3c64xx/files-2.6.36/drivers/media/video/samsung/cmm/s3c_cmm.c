/* linux/driver/media/video/samsung/cmm/s3c_cmm.c
 *
 * C file for Samsung CMM (Codec Memory Management) driver 
 *
 * Satish, Jiun Yu, Copyright (c) 2009 Samsung Electronics 
 * http://www.samsungsemi.com/ 
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
#include <linux/mutex.h>
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
#include <linux/irq.h>
#include <linux/semaphore.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>

#include <plat/media.h>
#include <linux/bootmem.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <asm/memory.h>
#include <linux/time.h>
#include <linux/clk.h>

#include "s3c_cmm.h"

/*#define CMM_DEBUG */
#ifdef CMM_DEBUG
#define CMM_MSG printk
#else
#define CMM_MSG(format,args...)
#endif

static int			s3c_cmm_instanceNo[S3C_CMM_MAX_INSTANCE_NUM];
static struct mutex	*s3c_cmm_mutex = NULL;

dma_addr_t		s3c_cmm_buffer_start;

s3c_cmm_alloc_mem_t	*s3c_cmm_allocmem_head;
s3c_cmm_alloc_mem_t	*s3c_cmm_allocmem_tail;
s3c_cmm_free_mem_t	*s3c_cmm_freemem_head;
s3c_cmm_free_mem_t	*s3c_cmm_freemem_tail;

unsigned char	*s3c_cmm_cached_vir_addr;
unsigned char	*s3c_cmm_noncached_vir_addr;

static void insertnode_alloclist(s3c_cmm_alloc_mem_t *node, int inst_no);
static void insertnode_freelist(s3c_cmm_free_mem_t *node,  int inst_no);
static void deletenode_alloclist(s3c_cmm_alloc_mem_t *node, int inst_no);
static void deletenode_freelist( s3c_cmm_free_mem_t *node, int inst_no);
static void release_alloc_mem(s3c_cmm_alloc_mem_t *node, 
				s3c_cmm_codec_mem_ctx_t *CodecMem);
static void merge_fragmented_mem(int inst_no);
static unsigned int get_mem_area(int allocSize, int inst_no, char cache_flag);
static s3c_cmm_alloc_mem_t * get_codec_virt_addr(int inst_no, 
				s3c_cmm_codec_mem_alloc_arg_t *in_param);
static int get_instance_num(void);
static void return_instance_num(int inst_no);
static void print_list(void);

static int s3c_cmm_open(struct inode *inode, struct file *file)
{
	s3c_cmm_codec_mem_ctx_t	*CodecMem;
	int			inst_no;


	mutex_lock(s3c_cmm_mutex);

	/* check the number of instance */
	if((inst_no = get_instance_num()) < 0){
		printk(KERN_ERR "\n%s: Instance Number error-too many instance\r\n"
				, __FUNCTION__);
		mutex_unlock(s3c_cmm_mutex);
		return FALSE;
	}
	
	CodecMem = (s3c_cmm_codec_mem_ctx_t *)
			kmalloc(sizeof(s3c_cmm_codec_mem_ctx_t), GFP_KERNEL);
	if(CodecMem == NULL){
		printk(KERN_ERR "\n%s: CodecMem application failed\n", __FUNCTION__);
		mutex_unlock(s3c_cmm_mutex);
		return FALSE;
	}
	
	memset(CodecMem, 0x00, sizeof(s3c_cmm_codec_mem_ctx_t));

	CodecMem->inst_no = inst_no;
	CMM_MSG(KERN_DEBUG "\n****************************\n[CMM_Open]"  
			"s3c_cmm_instanceNo : %d\n****************************\n",
			CodecMem->inst_no);
	print_list();
	
	file->private_data = (s3c_cmm_codec_mem_ctx_t*)CodecMem;

	mutex_unlock(s3c_cmm_mutex);

	return 0;
}


static int s3c_cmm_release(struct inode *inode, struct file *file)
{
	s3c_cmm_codec_mem_ctx_t	*CodecMem;
	s3c_cmm_alloc_mem_t 	*node, *tmp_node;


	mutex_lock(s3c_cmm_mutex);
	
	CodecMem = (s3c_cmm_codec_mem_ctx_t *)file->private_data;
	CMM_MSG(KERN_DEBUG "\n%s: [%d][CMM Close] \n", __FUNCTION__, 
			CodecMem->inst_no);

	if(!CodecMem){
		printk(KERN_ERR "\n%s: CMM Invalid Input Handle\r\n", __FUNCTION__);
		mutex_unlock(s3c_cmm_mutex);
		return FALSE;
	}

	CMM_MSG(KERN_DEBUG "\n%s: CodecMem->inst_no : %d\n", __FUNCTION__
				, CodecMem->inst_no);

	/* release u_addr and v_addr accoring to inst_no */
	for(node = s3c_cmm_allocmem_head; node != s3c_cmm_allocmem_tail; 
		node = node->next){
		if(node->inst_no == CodecMem->inst_no){
			tmp_node = node;
			node = node->prev;
			release_alloc_mem(tmp_node, CodecMem);
		}
	}

	CMM_MSG(KERN_DEBUG "\n%s: [%d]instance merge_fragmented_mem\n", 
			__FUNCTION__, CodecMem->inst_no);
	merge_fragmented_mem(CodecMem->inst_no);

	return_instance_num(CodecMem->inst_no);

	kfree(CodecMem);
	mutex_unlock(s3c_cmm_mutex);

	return 0;
}


static ssize_t s3c_cmm_write (struct file *file, const char *buf, size_t count, loff_t *pos)
{
	return 0;
}

static ssize_t s3c_cmm_read(struct file *file, char *buf, size_t count, loff_t *pos)
{	
	return 0;
}

static int s3c_cmm_ioctl(struct inode *inode, struct file *file, unsigned
		int cmd, unsigned long arg)
{
	int				ret;
	s3c_cmm_codec_mem_ctx_t		*CodecMem;
	s3c_cmm_codec_mem_alloc_arg_t	codec_mem_alloc_arg;
	s3c_cmm_codec_cache_flush_arg_t	codec_cache_flush_arg;
	s3c_cmm_codec_get_phy_addr_arg_t	codec_get_phy_addr_arg;
	BOOL				result = TRUE;
	void				*start, *end;
	s3c_cmm_alloc_mem_t		*node;
	s3c_cmm_codec_mem_free_arg_t	codec_mem_free_arg;

	CodecMem = (s3c_cmm_codec_mem_ctx_t *)file->private_data;
	if (!CodecMem) {
		printk(KERN_ERR "\n%s: CMM Invalid Input Handle\n", __FUNCTION__);
		return FALSE;
	}

	mutex_lock(s3c_cmm_mutex);

	switch (cmd){
	case S3C_CMM_IOCTL_CODEC_MEM_ALLOC:
		CMM_MSG(KERN_DEBUG"\n%s: S3C_CMM_IOCTL_CODEC_MEM_ALLOC\n",
				__FUNCTION__);

		ret = copy_from_user(&codec_mem_alloc_arg, 
					(s3c_cmm_codec_mem_alloc_arg_t *)arg
					, sizeof(s3c_cmm_codec_mem_alloc_arg_t));

		node = get_codec_virt_addr(CodecMem->inst_no, 
						&codec_mem_alloc_arg);
		if(node == NULL){
			result = FALSE;
			break;
		}
		
		ret = copy_to_user((void *)arg, (void *)&codec_mem_alloc_arg
					, sizeof(s3c_cmm_codec_mem_alloc_arg_t));
		break;

	case S3C_CMM_IOCTL_CODEC_MEM_FREE:
		CMM_MSG(KERN_DEBUG "\n%s: S3C_CMM_IOCTL_CODEC_MEM_FREE\n",
				__FUNCTION__);

		ret = copy_from_user(&codec_mem_free_arg, 
					(s3c_cmm_codec_mem_free_arg_t *)arg
					, sizeof(s3c_cmm_codec_mem_free_arg_t));

		for(node = s3c_cmm_allocmem_head; 
			node != s3c_cmm_allocmem_tail; node = node->next) {
			if(node->u_addr == (unsigned char *)codec_mem_free_arg.u_addr)
				break;
		}

		if(node  == s3c_cmm_allocmem_tail){
			CMM_MSG(KERN_DEBUG "\n%s: invalid virtual address(0x%x)\r\n"
					, __FUNCTION__,codec_mem_free_arg.u_addr);
			result = FALSE;
			break;
		}

		release_alloc_mem(node, CodecMem);
		break;


	case S3C_CMM_IOCTL_CODEC_GET_PHY_ADDR:
		ret = copy_from_user(&codec_get_phy_addr_arg, 
					(s3c_cmm_codec_get_phy_addr_arg_t *)arg
					, sizeof(s3c_cmm_codec_get_phy_addr_arg_t));

		for(node = s3c_cmm_allocmem_head;
			node != s3c_cmm_allocmem_tail; node = node->next) {
			if(node->u_addr == 
				(unsigned char *)codec_get_phy_addr_arg.u_addr)
				break;
		}

		if(node  == s3c_cmm_allocmem_tail){
			CMM_MSG(KERN_DEBUG "\n%s: invalid virtual address(0x%x)\r\n"
					, __FUNCTION__, codec_get_phy_addr_arg.u_addr);
			result = FALSE;
			break;
		}

		if(node->cacheFlag)
			codec_get_phy_addr_arg.p_addr = node->cached_p_addr;
		else
			codec_get_phy_addr_arg.p_addr = node->uncached_p_addr;
		
		ret = copy_to_user((void *)arg, (void *)&codec_get_phy_addr_arg
					, sizeof(s3c_cmm_codec_get_phy_addr_arg_t));

		break;
	case S3C_CMM_IOCTL_CODEC_MERGE_FRAGMENTATION:

		merge_fragmented_mem(CodecMem->inst_no);

		break;

	case S3C_CMM_IOCTL_CODEC_CACHE_FLUSH:
	case S3C_CMM_IOCTL_CODEC_CACHE_INVALIDATE:
		CMM_MSG(KERN_DEBUG "\n%s: S3C_CMM_IOCTL_CODEC_CACHE_INVALIDATE\n",
				__FUNCTION__);

		ret = copy_from_user(&codec_cache_flush_arg, 
					(s3c_cmm_codec_cache_flush_arg_t *)arg
					, sizeof(s3c_cmm_codec_cache_flush_arg_t));

		for(node = s3c_cmm_allocmem_head; 
			node != s3c_cmm_allocmem_tail; node = node->next) {
			if(node->u_addr == 
				(unsigned char *)codec_cache_flush_arg.u_addr)
				break;
		}

		if(node  == s3c_cmm_allocmem_tail){
			CMM_MSG(KERN_DEBUG "\n%s: invalid virtual address(0x%x)\r\n"
					, __FUNCTION__,codec_cache_flush_arg.u_addr);
			result = FALSE;
			break;
		}

		start = node->v_addr;
		end = start + codec_cache_flush_arg.size;
		dma_cache_maint(start,codec_cache_flush_arg.size,DMA_FROM_DEVICE);

		break;

	case S3C_CMM_IOCTL_CODEC_CACHE_CLEAN:
		CMM_MSG(KERN_DEBUG "\n%s: S3C_CMM_IOCTL_CODEC_CACHE_CLEAN\n", 
				__FUNCTION__);

		ret = copy_from_user(&codec_cache_flush_arg, 
					(s3c_cmm_codec_cache_flush_arg_t *)arg
					, sizeof(s3c_cmm_codec_cache_flush_arg_t));

		for(node = s3c_cmm_allocmem_head; 
			node != s3c_cmm_allocmem_tail; node = node->next) {
			if(node->u_addr == 
				(unsigned char *)codec_cache_flush_arg.u_addr)
				break;
		}

		if(node  == s3c_cmm_allocmem_tail){
			CMM_MSG(KERN_DEBUG "\n%s: invalid virtual address(0x%x)\r\n"
					, __FUNCTION__, codec_cache_flush_arg.u_addr);
			result = FALSE;
			break;
		}

		start = node->v_addr;
		end = start + codec_cache_flush_arg.size;
		dma_cache_maint(start,codec_cache_flush_arg.size,DMA_TO_DEVICE);
		break;

	case S3C_CMM_IOCTL_CODEC_CACHE_CLEAN_INVALIDATE:
		CMM_MSG(KERN_DEBUG "\n%s: S3C_CMM_IOCTL_CODEC_CACHE_CLEAN_INVALIDATE\n",
				__FUNCTION__);

		ret = copy_from_user(&codec_cache_flush_arg, 
					(s3c_cmm_codec_cache_flush_arg_t *)arg
					, sizeof(s3c_cmm_codec_cache_flush_arg_t));

		for(node = s3c_cmm_allocmem_head; 
			node != s3c_cmm_allocmem_tail; node = node->next) {
			if(node->u_addr == 
				(unsigned char *)codec_cache_flush_arg.u_addr)
				break;
		}

		if(node  == s3c_cmm_allocmem_tail){
			CMM_MSG(KERN_DEBUG "\n%s: invalid virtual address(0x%x)\r\n"
					, __FUNCTION__, codec_cache_flush_arg.u_addr);
			result = FALSE;
			break;
		}

		start = node->v_addr;
		end = start + codec_cache_flush_arg.size;
		dma_cache_maint(start,codec_cache_flush_arg.size,DMA_BIDIRECTIONAL);
		
		break;
		
	default : 
		printk(KERN_ERR "\n%s: DD::CMM Invalid ioctl : 0x%X\r\n",
				__FUNCTION__, cmd);
	}

	mutex_unlock(s3c_cmm_mutex);
	return result;
}


int s3c_cmm_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long offset	= vma->vm_pgoff << PAGE_SHIFT;
	unsigned long size;
	unsigned long maxSize;
	unsigned long pageFrameNo = 0;


	CMM_MSG(KERN_DEBUG "\n%s: vma->vm_end - vma->vm_start = %d\n",
			__FUNCTION__, offset);

	if(offset == 0) {
		pageFrameNo = __phys_to_pfn(s3c_cmm_buffer_start);
		maxSize = S3C_CMM_CODEC_CACHED_MEM_SIZE + PAGE_SIZE - 
				(S3C_CMM_CODEC_CACHED_MEM_SIZE % PAGE_SIZE);		
		vma->vm_flags |= VM_RESERVED | VM_IO;
		size = S3C_CMM_CODEC_CACHED_MEM_SIZE;
	}
	else if(offset == S3C_CMM_CODEC_CACHED_MEM_SIZE) {
		pageFrameNo = __phys_to_pfn(s3c_cmm_buffer_start + 
						S3C_CMM_CODEC_CACHED_MEM_SIZE);
		maxSize = S3C_CMM_CODEC_NON_CACHED_MEM_SIZE + PAGE_SIZE - 
				(S3C_CMM_CODEC_NON_CACHED_MEM_SIZE % PAGE_SIZE);		
		vma->vm_flags |= VM_RESERVED | VM_IO;
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		size = S3C_CMM_CODEC_NON_CACHED_MEM_SIZE;
	}

	if( remap_pfn_range(vma, vma->vm_start, pageFrameNo, size,	\
				vma->vm_page_prot) ) {
		printk(KERN_ERR "\n%s: cmm remap error \n", __FUNCTION__);
		return -EAGAIN;
	}

	return 0;
}


static struct file_operations s3c_cmm_fops = {
owner:		THIS_MODULE,
			open:		s3c_cmm_open,
			release:		s3c_cmm_release,
			ioctl:		s3c_cmm_ioctl,
			read:		s3c_cmm_read,
			write:		s3c_cmm_write,
			mmap:		s3c_cmm_mmap,
};



static struct miscdevice s3c_cmm_miscdev = {
minor:		250, 		
			name:		"s3c-cmm",
			fops:		&s3c_cmm_fops
};

static char banner[] __initdata = KERN_INFO "S3C CMM Driver, (c) 2008 Samsung Electronics\n";

static int __init s3c_cmm_init(void)
{
	int			ret;
	s3c_cmm_free_mem_t	*node;
	s3c_cmm_alloc_mem_t	*alloc_node;
	
	printk(banner);

	/* mutex creation and initialization */
	s3c_cmm_mutex = (struct mutex *)kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (s3c_cmm_mutex == NULL) {
		printk(KERN_ERR "\n%s: CMM mutex initialize error\n", __FUNCTION__);
		return -ENOMEM;
	}

	mutex_init(s3c_cmm_mutex);

	mutex_lock(s3c_cmm_mutex);

	ret = misc_register(&s3c_cmm_miscdev);

	s3c_cmm_buffer_start = s3c_get_media_memory(S3C_MDEV_CMM);

	/* First 4MB will use cacheable memory */
	s3c_cmm_cached_vir_addr = (unsigned char *)phys_to_virt(s3c_cmm_buffer_start);
	CMM_MSG(KERN_DEBUG "\n%s: cached_vir_addr 0x%x\n", 
			__FUNCTION__,s3c_cmm_cached_vir_addr);

	/* Second 4MB will use non-cacheable memory */
	s3c_cmm_noncached_vir_addr = (unsigned char *)
					phys_to_virt (s3c_cmm_buffer_start + 
						S3C_CMM_CODEC_CACHED_MEM_SIZE);

	/* init alloc list, if(s3c_cmm_allocmem_head == s3c_cmm_allocmem_tail) 
	 * then, the list is NULL */
	alloc_node = (s3c_cmm_alloc_mem_t *)kmalloc(sizeof(s3c_cmm_alloc_mem_t),
							GFP_KERNEL);
	memset(alloc_node, 0x00, sizeof(s3c_cmm_alloc_mem_t));
	alloc_node->next = alloc_node;
	alloc_node->prev = alloc_node;
	s3c_cmm_allocmem_head = alloc_node;
	s3c_cmm_allocmem_tail = s3c_cmm_allocmem_head;

	/* init free list, if(s3c_cmm_freemem_head == s3c_cmm_freemem_tail) 
	 * then, the list is NULL */
	node = (s3c_cmm_free_mem_t *)kmalloc(sizeof(s3c_cmm_free_mem_t),
						GFP_KERNEL);
	memset(node, 0x00, sizeof(s3c_cmm_free_mem_t));
	node->next = node;
	node->prev = node;
	s3c_cmm_freemem_head = node;
	s3c_cmm_freemem_tail = s3c_cmm_freemem_head;

	node = (s3c_cmm_free_mem_t *)kmalloc(sizeof(s3c_cmm_free_mem_t), GFP_KERNEL);
	memset(node, 0x00, sizeof(s3c_cmm_free_mem_t));
	node->startAddr = s3c_cmm_buffer_start;
	node->cacheFlag = 1;
	node->size = S3C_CMM_CODEC_CACHED_MEM_SIZE;
	insertnode_freelist(node, -1);

	node = (s3c_cmm_free_mem_t *)kmalloc(sizeof(s3c_cmm_free_mem_t), GFP_KERNEL);
	memset(node, 0x00, sizeof(s3c_cmm_free_mem_t));
	node->startAddr = s3c_cmm_buffer_start + S3C_CMM_CODEC_CACHED_MEM_SIZE;
	node->cacheFlag = 0;
	node->size = S3C_CMM_CODEC_NON_CACHED_MEM_SIZE;
	insertnode_freelist(node, -1);

	mutex_unlock(s3c_cmm_mutex);

	return 0;
}

static void __exit s3c_cmm_exit(void)
{
	
	CMM_MSG(KERN_DEBUG "\n%s: CMM_Deinit\n", __FUNCTION__);

	mutex_destroy(s3c_cmm_mutex);

	misc_deregister(&s3c_cmm_miscdev);
	
	printk("S3C CMM driver module exit\n");
}


/* insert node ahead of s3c_cmm_allocmem_head */
static void insertnode_alloclist(s3c_cmm_alloc_mem_t *node, int inst_no)
{
	CMM_MSG(KERN_DEBUG "\n%s: [%d]instance (cached_p_addr : 0x%08x uncached_p_addr"
				" : 0x%08x size:%ld cacheflag : %d)\n", __FUNCTION__
				,inst_no, node->cached_p_addr, node->uncached_p_addr
				, node->size, node->cacheFlag);
	node->next = s3c_cmm_allocmem_head;
	node->prev = s3c_cmm_allocmem_head->prev;
	s3c_cmm_allocmem_head->prev->next = node;
	s3c_cmm_allocmem_head->prev = node;
	s3c_cmm_allocmem_head = node;
	CMM_MSG(KERN_DEBUG "\n%s: Finished insertnode_alloclist\n", __FUNCTION__);
}

/* insert node ahead of s3c_cmm_freemem_head */
static void insertnode_freelist(s3c_cmm_free_mem_t *node,  int inst_no)
{
	CMM_MSG(KERN_DEBUG "\n%s: [%d]instance(startAddr : 0x%08x size:%ld"
				" cached flag : %d)\n", __FUNCTION__,inst_no
				, node->startAddr, node->size, node->cacheFlag);
	node->next = s3c_cmm_freemem_head;
	node->prev = s3c_cmm_freemem_head->prev;
	s3c_cmm_freemem_head->prev->next = node;
	s3c_cmm_freemem_head->prev = node;
	s3c_cmm_freemem_head = node;

	print_list();
}

static void deletenode_alloclist(s3c_cmm_alloc_mem_t *node, int inst_no)
{
	CMM_MSG(KERN_DEBUG "\n%s: [%d]instance (uncached_p_addr : 0x%08x cached_p_addr"
				" : 0x%08x size:%ld cacheflag : %d)\n", __FUNCTION__
				,inst_no,node->uncached_p_addr, node->cached_p_addr
				, node->size, node->cacheFlag);
	
	if(node == s3c_cmm_allocmem_tail){
		CMM_MSG(KERN_DEBUG "\n%s: InValid node\n", __FUNCTION__);
		return;
	}

	if(node == s3c_cmm_allocmem_head)
		s3c_cmm_allocmem_head = node->next;

	node->prev->next = node->next;
	node->next->prev = node->prev;

	kfree(node);

	print_list();
}

static void deletenode_freelist( s3c_cmm_free_mem_t *node, int inst_no)
{
	CMM_MSG(KERN_DEBUG "\n%s: [%d]deletenode_freelist(startAddr : 0x%08x size:%ld)\n"
				, __FUNCTION__, inst_no, node->startAddr, node->size);
	if(node == s3c_cmm_freemem_tail){
		printk(KERN_ERR "\n%s: InValid node\n", __FUNCTION__);
		return;
	}

	if(node == s3c_cmm_freemem_head)
		s3c_cmm_freemem_head = node->next;

	node->prev->next = node->next;
	node->next->prev = node->prev;

	kfree(node);
}

/* Releae cacheable memory */
static void release_alloc_mem(	s3c_cmm_alloc_mem_t *node,
				s3c_cmm_codec_mem_ctx_t *CodecMem)
{
	s3c_cmm_free_mem_t *free_node;
	

	free_node = (s3c_cmm_free_mem_t	*)kmalloc(sizeof(s3c_cmm_free_mem_t), GFP_KERNEL);

	if(node->cacheFlag) {
		free_node->startAddr = node->cached_p_addr;
		free_node->cacheFlag = 1;
	}
	else {
		free_node->startAddr = node->uncached_p_addr;
		free_node->cacheFlag = 0;
	}
	
	free_node->size = node->size;
	insertnode_freelist(free_node, CodecMem->inst_no);
	
	/* Delete from AllocMem list */
	deletenode_alloclist(node, CodecMem->inst_no);
}



/* Remove Fragmentation in FreeMemList */
static void merge_fragmented_mem(int inst_no)
{
	s3c_cmm_free_mem_t *node1, *node2;

	node1 = s3c_cmm_freemem_head;

	while(node1 != s3c_cmm_freemem_tail){
		node2 = s3c_cmm_freemem_head;
		while(node2 != s3c_cmm_freemem_tail){
			if( (node1->startAddr + node1->size == node2->startAddr) 
				&& (node1->cacheFlag == node2->cacheFlag) ){
				node1->size += node2->size;
				CMM_MSG(KERN_DEBUG "\n%s: find merge area !! ( node1->startAddr +"
						" node1->size == node2->startAddr)\n", __FUNCTION__);
				deletenode_freelist(node2, inst_no);
				break;
			}
			else if( (node1->startAddr == node2->startAddr + node2->size) 
					&& (node1->cacheFlag == node2->cacheFlag) ){
				CMM_MSG(KERN_DEBUG "\n%s: find merge area !! ( node1->startAddr == "
						"node2->startAddr + node2->size)\n", __FUNCTION__);
				node1->startAddr = node2->startAddr;
				node1->size += node2->size;
				deletenode_freelist(node2, inst_no);
				break;
			}
			node2 = node2->next;
		}
		node1 = node1->next;
	}
}

static unsigned int get_mem_area(int allocSize, int inst_no, char cache_flag)
{
	s3c_cmm_free_mem_t		*node, *match_node = NULL;
	unsigned int	allocAddr = 0;


	CMM_MSG(KERN_DEBUG "\n%s: request Size : %ld\n", __FUNCTION__, allocSize);
	
	if(s3c_cmm_freemem_head == s3c_cmm_freemem_tail){
		printk(KERN_ERR "\n%s: all memory is gone\n", __FUNCTION__);
		return(allocAddr);
	}

	/* find best chunk of memory */
	for(node = s3c_cmm_freemem_head; 
		node != s3c_cmm_freemem_tail; node = node->next)	{
		if(match_node != NULL)
		{
			if(cache_flag)
			{
				if( (node->size >= allocSize) && 
					(node->size < match_node->size) 
					&& (node->cacheFlag) )
					match_node = node;
			}
			else
			{
				if( (node->size >= allocSize) && 
					(node->size < match_node->size) 
					&& (!node->cacheFlag) )
					match_node = node;
			}
		}
		else
		{
			if(cache_flag)
			{
				if( (node->size >= allocSize) && (node->cacheFlag) )
					match_node = node;
			}
			else
			{
				if( (node->size >= allocSize) && (!node->cacheFlag) )
					match_node = node;
			}
		}
	}

	if(match_node != NULL) {
		CMM_MSG(KERN_DEBUG "\n%s: match : startAddr(0x%08x) size(%ld) cache flag(%d)\n"
					, __FUNCTION__,match_node->startAddr
					,match_node->size, match_node->cacheFlag);
	}
	
	/* rearange FreeMemArea */
	if(match_node != NULL){
		allocAddr = match_node->startAddr;
		match_node->startAddr += allocSize;
		match_node->size -= allocSize;
		
		if(match_node->size == 0)          /* delete match_node */
		 	deletenode_freelist(match_node, inst_no);

		return(allocAddr);
	}else {
		printk("there is no suitable chunk\n");
		return 0;
	}

	return(allocAddr);
}


static s3c_cmm_alloc_mem_t * get_codec_virt_addr(int inst_no, 
					s3c_cmm_codec_mem_alloc_arg_t *in_param)
{

	unsigned int			p_startAddr;
	s3c_cmm_alloc_mem_t 			*p_allocMem;
	

	/* if user request cachable area, allocate from reserved area */
	/* if user request uncachable area, allocate dynamically */	
	p_startAddr = get_mem_area((int)in_param->buffSize, 
					inst_no, in_param->cacheFlag);

	if(!p_startAddr){
		CMM_MSG(KERN_DEBUG "\n%s: There is no more memory\n\r",
				__FUNCTION__);
		in_param->out_addr = -1;
		return NULL;
	}

	p_allocMem = (s3c_cmm_alloc_mem_t *)kmalloc(sizeof(s3c_cmm_alloc_mem_t), GFP_KERNEL);
	memset(p_allocMem, 0x00, sizeof(s3c_cmm_alloc_mem_t));
		

	if(in_param->cacheFlag) {
		p_allocMem->cached_p_addr = p_startAddr;
		p_allocMem->v_addr = s3c_cmm_cached_vir_addr + 
					(p_allocMem->cached_p_addr - 
					s3c_cmm_buffer_start);
		p_allocMem->u_addr = (unsigned char *)(in_param->cached_mapped_addr + 
					(p_allocMem->cached_p_addr - s3c_cmm_buffer_start));
		
		if (p_allocMem->v_addr == NULL) {
			CMM_MSG(KERN_DEBUG "\n%s: Mapping Failed [PA:0x%08x]\n\r"
					, __FUNCTION__, p_allocMem->cached_p_addr);
			return NULL;
		}
	}
	else {
		p_allocMem->uncached_p_addr = p_startAddr;
		p_allocMem->v_addr = s3c_cmm_noncached_vir_addr + 
					(p_allocMem->uncached_p_addr - s3c_cmm_buffer_start 
					- S3C_CMM_CODEC_CACHED_MEM_SIZE);
		p_allocMem->u_addr = (unsigned char *)(in_param->non_cached_mapped_addr + 
					(p_allocMem->uncached_p_addr - s3c_cmm_buffer_start 
					- S3C_CMM_CODEC_CACHED_MEM_SIZE));
		
		if (p_allocMem->v_addr == NULL)
		{
			CMM_MSG(KERN_DEBUG "\n%s: Mapping Failed [PA:0x%08x]\n\r"
						, __FUNCTION__, p_allocMem->uncached_p_addr);
			return NULL;
		}
	}

	in_param->out_addr = (unsigned int)p_allocMem->u_addr;
	CMM_MSG(KERN_DEBUG "\n%s: u_addr : 0x%x v_addr : 0x%x cached_p_addr : 0x%x,"
			" uncached_p_addr : 0x%x\n", __FUNCTION__,p_allocMem->u_addr
			, p_allocMem->v_addr, p_allocMem->cached_p_addr
			, p_allocMem->uncached_p_addr);
		

	p_allocMem->size = (int)in_param->buffSize;
	p_allocMem->inst_no = inst_no;
	p_allocMem->cacheFlag = in_param->cacheFlag;
	
	insertnode_alloclist(p_allocMem, inst_no);

	return(p_allocMem);
}

static int get_instance_num(void)
{
	int	i;

	for(i = 0; i < S3C_CMM_MAX_INSTANCE_NUM; i++)
	{
		if(s3c_cmm_instanceNo[i] == FALSE){
			s3c_cmm_instanceNo[i] = TRUE;
			return i;
		}
	}
	
	if(i == S3C_CMM_MAX_INSTANCE_NUM)
		return -1;

	return i;
}


static void return_instance_num(int inst_no)
{
	s3c_cmm_instanceNo[inst_no] = FALSE;
}


static void print_list()
{
	s3c_cmm_alloc_mem_t		*node1;
	s3c_cmm_free_mem_t		*node2;
	int 			count = 0;
	unsigned int	p_addr;

	for(node1 = s3c_cmm_allocmem_head; 
		node1 != s3c_cmm_allocmem_tail; node1 = node1->next){
		if(node1->cacheFlag)
			p_addr = node1->cached_p_addr;
		else
			p_addr = (unsigned int)node1->uncached_p_addr;
		
		CMM_MSG(KERN_DEBUG "\n%s: [AllocList][%d] inst_no : %d p_addr : 0x%08x "
				"v_addr:0x%08x size:%ld cacheflag : %d\n", __FUNCTION__
				,count++, node1->inst_no,  p_addr, node1->v_addr
				, node1->size, node1->cacheFlag);
	}
				
	count = 0;
	for(node2 = s3c_cmm_freemem_head; 
		node2 != s3c_cmm_freemem_tail; node2 = node2->next){
		CMM_MSG(KERN_DEBUG "\n%s: [FreeList][%d] startAddr : 0x%08x size:%ld\n"
				, __FUNCTION__, count++, node2->startAddr , node2->size);

	}
}

module_init(s3c_cmm_init);
module_exit(s3c_cmm_exit);

MODULE_AUTHOR("Jiun, Yu");
MODULE_LICENSE("GPL");
