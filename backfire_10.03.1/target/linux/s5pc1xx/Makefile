#
# Copyright (C) 2013 OpenWrt.org.cn
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

ARCH:=arm
BOARD:=s5pc1xx
BOARDNAME:=Samsung S5PC1xx
FEATURES:=jffs2 squashfs
CFLAGS:=-O2 -pipe -march=armv7-a -mtune=cortex-a8 -funit-at-a-time -mfpu=vfpv3-d16 -mfloat-abi=softfp
SUBTARGETS:=tiny210 mini210 webee210

LINUX_VERSION:=2.6.35.7

DEVICE_TYPE:=developerboard
MAINTAINER:=Richard Nee <openme@openwrt.org.cn>

define Target/Description
	SAMSUNG S5PC1XX
endef

include $(INCLUDE_DIR)/target.mk
DEFAULT_PACKAGES += kmod-gpio-dev gpioctl

$(eval $(call BuildTarget))
