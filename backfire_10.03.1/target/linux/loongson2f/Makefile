#
# Copyright (C) 2008-2010 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

ARCH:=mipsel
BOARD:=loongson2f
BOARDNAME:=LoongSon 2F
FEATURES:=targz
CFLAGS=-O2 -pipe -march=loongson2f -mabi=32 -funit-at-a-time
#-march=loongson2f -march=r4600 -mabi=n32

LINUX_VERSION:=2.6.32.33

define Target/Description
	LoongSon 2F( Lemote 2F 8089 )
endef

include $(INCLUDE_DIR)/target.mk

DEFAULT_PACKAGES += 

$(eval $(call BuildTarget))
