#
# Copyright (C) 2006-2009 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

ARCH:=arm
BOARD:=openmoko
BOARDNAME:=Openmoko SmartPhone
FEATURES:=jffs2 squashfs broken
CFLAGS:=-O2 -pipe -march=armv4t -mtune=arm920t -fno-caller-saves
SUBTARGETS:=openmoko-gta01 openmoko-gta02

LINUX_VERSION:=2.6.30.10

DEVICE_TYPE=phone

define Target/Description
	Openmoko Smartphone with S3C24XX ARM
endef

include $(INCLUDE_DIR)/target.mk

$(eval $(call BuildTarget))
