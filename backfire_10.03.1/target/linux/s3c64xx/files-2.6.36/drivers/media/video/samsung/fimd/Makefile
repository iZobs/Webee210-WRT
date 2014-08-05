obj-$(CONFIG_VIDEO_FIMD)	+= s3c_fimd_core.o s3c_fimd_v4l2.o s3c_fimd_cfg.o
obj-$(CONFIG_ARCH_S5PC1XX)	+= s3c_fimd4x_regs.o

EXTRA_CFLAGS += -Idrivers/media/video

ifeq ($(CONFIG_VIDEO_FIMC_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif
