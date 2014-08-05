# 
# Copyright (C) 2006-2010 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/image.mk

define Image/Prepare
	$(CP) $(LINUX_DIR)/arch/arm/boot/zImage $(KDIR)/zImage
	$(CP) $(LINUX_DIR)/arch/arm/boot/compressed/vmlinux $(KDIR)/vmlinux
endef

define Image/uImage/uImage
	$(CP) $(LINUX_DIR)/arch/arm/boot/uImage $(KDIR)/uImage
endef

define Image/uImage/zImage
	$(STAGING_DIR_HOST)/bin/mkimage -A arm -O linux -T kernel -C none \
		-n "OpenWrt Linux-$(LINUX_VERSION) Kernel" \
		-a $(1) -e $(2) \
		-d $(KDIR)/zImage $(KDIR)/uImage
endef

define Image/uImage/vmlinux
	$(TARGET_CROSS)objcopy -O binary -R .note -R .comment -S \
		$(KDIR)/vmlinux $(KDIR)/linux.bin

	$(STAGING_DIR_HOST)/bin/mkimage -A arm -O linux -T kernel -C none \
		-n "OpenWrt Linux-$(LINUX_VERSION) Kernel" \
		-a $(1) -e $(2) \
		-d $(KDIR)/linux.bin $(KDIR)/uImage
endef

define Image/uImage/lzma
	$(STAGING_DIR_HOST)/bin/lzma e $(LINUX_DIR)/arch/arm/boot/Image \
		$(KDIR)/vmlinux.bin.lzma

	$(STAGING_DIR_HOST)/bin/mkimage -A arm -O linux -T kernel -C lzma \
		-n "OpenWrt Linux-$(LINUX_VERSION) Kernel" \
		-a $(1) -e $(2) \
		-d $(KDIR)/vmlinux.bin.lzma $(KDIR)/uImage
endef

define Image/uImage/gzip
	gzip -cvn9 $(LINUX_DIR)/arch/arm/boot/Image > $(KDIR)/vmlinux.bin.gz

	$(STAGING_DIR_HOST)/bin/mkimage -A arm -O linux -T kernel -C gzip \
		-n "OpenWrt Linux-$(LINUX_VERSION) Kernel" \
		-a $(1) -e $(2) \
		-d $(KDIR)/vmlinux.bin.gz $(KDIR)/uImage
endef

define Image/uImage/bzip2
	bzip2 -cvs9 $(LINUX_DIR)/arch/arm/boot/Image > $(KDIR)/vmlinux.bin.bz

	$(STAGING_DIR_HOST)/bin/mkimage -A arm -O linux -T kernel -C bzip2 \
		-n "OpenWrt Linux-$(LINUX_VERSION) Kernel" \
		-a $(1) -e $(2) \
		-d $(KDIR)/vmlinux.bin.bz $(KDIR)/uImage
endef

define Image/uImage/7zip
	$(CHIP_PATH)/mktools/7zr a $(KDIR)/vmlinux.bin.7z \
		$(LINUX_DIR)/arch/arm/boot/Image

	$(STAGING_DIR_HOST)/bin/mkimage -A arm -O linux -T kernel -C 7zip \
		-n "OpenWrt Linux-$(LINUX_VERSION) Kernel" \
		-a $(1) -e $(2) \
		-d $(KDIR)/vmlinux.bin.7z $(KDIR)/uImage
endef


UIMAGE_METHOD:=gzip
LOADADDR:=0x8000
STARTADDR:=0x8000
#30008040
define Image/BuildKernel
	$(call Image/uImage/$(UIMAGE_METHOD), $(LOADADDR), $(STARTADDR) )
	$(CP) $(KDIR)/uImage $(BIN_DIR)/$(IMG_PREFIX)-linux-$(LINUX_VERSION)-uImage
	$(CP) $(KDIR)/zImage $(BIN_DIR)/$(IMG_PREFIX)-linux-$(LINUX_VERSION)-zImage
endef

# IN image.mk
#define add_jffs2_mark
#	echo -ne '\xde\xad\xc0\xde' >> $(1)
#endef

# DEFAULT in image.mk: pad to 4k, 8k, 64k, 128k and add jffs2 end-of-filesystem mark
# 这款NAND Flash是16K的Block，因此自动加载jffs2分区的时候会出现2个Block的偏移：
#	jffs2_scan_eraseblock(): End of filesystem marker found at 0x8000
#	Cowardly refusing to erase blocks on filesystem with no valid JFFS2 nodes
#	empty_blocks 44, bad_blocks 0, c->nr_blocks 46
#	jffs2 not ready yet; using ramdisk

# Modified: pad to 4k 8k 16k 32k 64k 128k
define prepare_generic_squashfs_16k
	dd if=$(1) of=$(KDIR)/tmpfile.0 bs=4k conv=sync
	$(call add_jffs2_mark,$(KDIR)/tmpfile.0)
	dd if=$(KDIR)/tmpfile.0 of=$(KDIR)/tmpfile.1 bs=4k conv=sync
	$(call add_jffs2_mark,$(KDIR)/tmpfile.1)
	dd if=$(KDIR)/tmpfile.1 of=$(KDIR)/tmpfile.2 bs=16k conv=sync
	$(call add_jffs2_mark,$(KDIR)/tmpfile.2)
	dd if=$(KDIR)/tmpfile.2 of=$(KDIR)/tmpfile.3 bs=16k conv=sync
	$(call add_jffs2_mark,$(KDIR)/tmpfile.3)
	dd if=$(KDIR)/tmpfile.3 of=$(KDIR)/tmpfile.4 bs=64k conv=sync
	$(call add_jffs2_mark,$(KDIR)/tmpfile.4)
	dd if=$(KDIR)/tmpfile.4 of=$(1) bs=64k conv=sync
	$(call add_jffs2_mark,$(1))
	rm -f $(KDIR)/tmpfile.*
endef


define Image/Build/squashfs
    $(call prepare_generic_squashfs_16k,$(BIN_DIR)/$(IMG_PREFIX)-root.$(1))
endef

define Image/Build
	$(CP) $(KDIR)/root.$(1) $(BIN_DIR)/$(IMG_PREFIX)-root.$(1)
	$(call Image/Build/$(1),$(1))
endef


$(eval $(call BuildImage))
