#################################################
# Makefile for Post Processor 
# 2007 (C) Samsung Electronics 
# Author : SungJun Bae <june.bae@samsung.com>
#################################################

ifeq ($(CONFIG_ARCH_S5P64XX),y)
obj-$(CONFIG_VIDEO_POST)	+= s3c_post_core.o s3c_post_v4l2.o s3c_post_cfg.o s3c_post_regs.o
else
obj-$(CONFIG_VIDEO_POST)	+= s3c_pp_common.o s3c_pp_6400.o
endif

EXTRA_CFLAGS += -Idrivers/media/video

ifeq ($(CONFIG_VIDEO_POST_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif
