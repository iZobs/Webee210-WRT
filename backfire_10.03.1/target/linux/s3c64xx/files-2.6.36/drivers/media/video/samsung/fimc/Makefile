obj-$(CONFIG_VIDEO_FIMC)	+= s3c_fimc_core.o s3c_fimc_v4l2.o s3c_fimc_cfg.o
obj-$(CONFIG_VIDEO_FIMC_MIPI)	+= s3c_csis.o
obj-$(CONFIG_ARCH_S3C64XX)	+= s3c_fimc3x_regs.o
obj-$(CONFIG_ARCH_S5PC1XX)	+= s3c_fimc4x_regs.o
obj-$(CONFIG_S5K3BA)		+= s5k3ba.o
obj-$(CONFIG_S5K4BA)		+= s5k4ba.o
obj-$(CONFIG_OV965X)		+= ov965x.o

EXTRA_CFLAGS += -Idrivers/media/video

ifeq ($(CONFIG_VIDEO_FIMC_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif
