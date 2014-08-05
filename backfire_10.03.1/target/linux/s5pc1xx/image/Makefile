
# 
# Copyright (C) 2010 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/image.mk


UIMAGE:=$(BIN_DIR)/$(IMG_PREFIX)-uImage

 
define MkuImage
	mkimage -A arm -O linux -T kernel -a 0x30008000 -C none \
		-e 0x30008000 -n 'Webee210 OpenWrt Linux-$(LINUX_VERSION)' \
		-d $(1) $(2)
endef
 
define Image/Prepare
    cp $(LINUX_DIR)/arch/arm/boot/zImage $(KDIR)/zImage
endef

define Image/BuildKernel
	$(call MkuImage,$(KDIR)/zImage,$(UIMAGE))
	cp $(KDIR)/zImage $(BIN_DIR)/$(IMG_PREFIX)-zImage
endef

define Image/Build
	$(call Image/Build/$(1),$(1))
endef

define Image/Build/squashfs
	$(call prepare_generic_squashfs,$(KDIR)/root.squashfs)
	( \
		dd if=$(KDIR)/uImage bs=1024k conv=sync; \
		dd if=$(KDIR)/root.$(1) bs=64k conv=sync; \
	) > $(BIN_DIR)/openwrt-$(BOARD)-$(1).img
	$(STAGING_DIR_HOST)/bin/mkyaffs2image-page2k $(STAGING_DIR_ROOT)/ $(BIN_DIR)/openwrt-$(BOARD)-yaffs2-128k.img
endef

#define Image/Build/yaffs2-128k
#	$(STAGING_DIR_HOST)/bin/mkyaffs2image-page2k $(STAGING_DIR_ROOT)/ $(BIN_DIR)/openwrt-$(BOARD)-$(1).yaffs2
#endef

$(eval $(call BuildImage))
