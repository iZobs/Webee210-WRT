obj-$(CONFIG_VIDEO_MFC10) += prism_s_v137.o s3c_mfc_bitproc_buf.o s3c_mfc.o s3c_mfc_databuf.o s3c_mfc_init_hw.o s3c_mfc_instance.o s3c_mfc_inst_pool.o s3c_mfc_set_config.o s3c_mfc_sfr.o s3c_mfc_yuv_buf_manager.o

EXTRA_CFLAGS += -DLINUX
EXTRA_CFLAGS += -DDIVX_ENABLE

ifeq ($(CONFIG_VIDEO_MFC_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif
