/* linux/driver/media/video/cmm/s3c_cmm.h
 *
 * Header file for Samsung CMM (Codec Memory Management) driver 
 *
 * Satish, Jiun Yu, Copyright (c) 2009 Samsung Electronics 
 * http://www.samsungsemi.com/ 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _S3C_CMM_H
#define _S3C_CMM_H


#define S3C_CMM_MAX_INSTANCE_NUM			10

#define S3C_CMM_CODEC_CACHED_MEM_SIZE		(4*1024*1024)
#define S3C_CMM_CODEC_NON_CACHED_MEM_SIZE		(4*1024*1024) 
#define S3C_CMM_CODEC_MEM_SIZE			(8*1024*1024)

#define S3C_CMM_IOCTL_CODEC_MEM_ALLOC		0x0000001A	
#define S3C_CMM_IOCTL_CODEC_MEM_FREE		0x0000001B
#define S3C_CMM_IOCTL_CODEC_CACHE_FLUSH		0x0000001C
#define S3C_CMM_IOCTL_CODEC_GET_PHY_ADDR		0x0000001D
#define S3C_CMM_IOCTL_CODEC_MERGE_FRAGMENTATION	0x0000001E
#define S3C_CMM_IOCTL_CODEC_CACHE_INVALIDATE	0x0000001F
#define S3C_CMM_IOCTL_CODEC_CACHE_CLEAN		0x00000020
#define S3C_CMM_IOCTL_CODEC_CACHE_CLEAN_INVALIDATE	0x00000021


typedef enum {FALSE, TRUE} BOOL;

typedef struct {
	char			cacheFlag;	/* Cache or noncache flag */
	int			size;       	/* memory size */
}s3c_cmm_alloc_pram_t;

typedef struct {
	int			inst_no;
	int			callerProcess;
}s3c_cmm_codec_mem_ctx_t;

typedef struct alloc_mem_t {
	struct alloc_mem_t	*prev;
	struct alloc_mem_t	*next;
	union{
		unsigned int	cached_p_addr;  /* physical address of cacheable area */
		unsigned int	uncached_p_addr;/* physical address of non-cacheable area */
	};
	unsigned char		*v_addr;  	/* virtual address in cached area */
	unsigned char		*u_addr;  	/* copyed virtual address for user mode process */
	int			size;       	/* memory size */
	int			inst_no;
	char			cacheFlag;
}s3c_cmm_alloc_mem_t;
	
typedef struct free_mem_t {
	struct free_mem_t	*prev;
	struct free_mem_t	*next;
	unsigned int		startAddr;
	unsigned int		size;
	char			cacheFlag;
}s3c_cmm_free_mem_t;

/* ioctl arguments */
typedef struct {
	char			cacheFlag;
	int			buffSize;
	unsigned int		cached_mapped_addr;
	unsigned int		non_cached_mapped_addr;
	unsigned int		out_addr;
}s3c_cmm_codec_mem_alloc_arg_t;

typedef struct {
	unsigned int	u_addr;
}s3c_cmm_codec_mem_free_arg_t;

typedef struct {
	unsigned int	u_addr;
	int		size;
}s3c_cmm_codec_cache_flush_arg_t;

typedef struct {
	unsigned int 	u_addr;
	unsigned int	p_addr;
}s3c_cmm_codec_get_phy_addr_arg_t;

#endif /* _S3C_CMM_H */
