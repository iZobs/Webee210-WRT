#################################################
# Makefile for Post Processor 
# 2007 (C) Samsung Electronics 
# Author : SungJun Bae <june.bae@samsung.com>
#################################################

obj-$(CONFIG_VIDEO_TV)	+= s3c-tvenc.o s3c-tvscaler.o

EXTRA_CFLAGS += -Idrivers/media/video

ifeq ($(CONFIG_VIDEO_TV_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif
