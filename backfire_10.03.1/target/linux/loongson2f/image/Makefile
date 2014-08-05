# 
# Copyright (C) 2006-2010 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/image.mk

#$(call Image/Prepare)
#$(call Image/mkfs/prepare)
#$(call Image/BuildKernel)
#$(call Image/mkfs/cpiogz)
#$(call Image/mkfs/targz)
#$(call Image/mkfs/ext2)
#$(call Image/mkfs/iso)
#$(call Image/mkfs/jffs2)
#$(call Image/mkfs/squashfs)
#$(call Image/mkfs/ubifs)
#$(call Image/Checksum)

####### 1
define Image/Prepare
	$(CP) $(LINUX_DIR)/vmlinux $(KDIR)/vmlinux
endef

###### 2
#define Image/Build/Initramfs
#        $(STAGING_DIR_HOST)/bin/trx -o $(BIN_DIR)/$(IMG_PREFIX)-initramfs.trx -f $(KDIR)/loader.gz -f $(KDIR)/vmlinux.lzma
#endef
#
define Image/BuildKernel
	$(CP) $(KDIR)/vmlinux $(BIN_DIR)/loongson2f-openwrt-linux-$(LINUX_VERSION)-vmlinux
	$(CP) ./vmlinux-usbinstall $(BIN_DIR)/ 
endef

###### 3
define mkfs/rootfs
	$(INSTALL_DIR) $(TARGET_DIR)/boot/
	$(CP) $(KDIR)/vmlinux $(TARGET_DIR)/boot/
	$(TAR) -zcf $(BIN_DIR)/loongson2f-openwrt-rootfs-2014.tar.gz --numeric-owner --owner=0 --group=0 -C $(TARGET_DIR)/ .

#	$(TAR) -cf $(BIN_DIR)/$(IMG_PREFIX)-rootfs.tar --numeric-owner --owner=0 --group=0 -C $(TARGET_DIR)/ .
#	$(STAGING_DIR_HOST)/bin/lzma e $(BIN_DIR)/$(IMG_PREFIX)-rootfs.tar $(BIN_DIR)/$(IMG_PREFIX)-rootfs.tar.lzma
endef

define Image/mkfs/cpiogz
endef

define Image/mkfs/targz
	$(call mkfs/rootfs)
endef

define Image/mkfs/ext2
endef

define Image/mkfs/iso
endef

define Image/mkfs/jffs2
endef

define Image/mkfs/squashfs
endef

define Image/mkfs/ubifs
endef

#include $(INCLUDE_DIR)/download.mk
#DL_DIR=$(BIN_DIR)
#define Download/vmlinux_usbinstall
#  URL:=http://www.anheng.com.cn/loongson/install/
#  #URL:=ftp://www.bjlx.org.cn/loongson/install/
#  FILE:=vmlinux
#endef
#$(eval $(call Download,vmlinux_usbinstall))


$(eval $(call BuildImage))
