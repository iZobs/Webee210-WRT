#
# Copyright (C) 2006-2009 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

ARCH:=arm
BOARD:=s3c24xx
BOARDNAME:=Samsung S3C24xx
FEATURES:=jffs2 squashfs
CFLAGS:=-O2 -pipe -march=armv4t -mtune=arm920t -funit-at-a-time
SUBTARGETS:=dev-s3c2440 dev-s3c2410 dev-fs2410 dev-mini2440 dev-gec2410 dev-gec2440 dev-qq2440

LINUX_VERSION:=2.6.32.27
#LINUX_VERSION:=2.6.32.59


define Target/Description
	S3C24xx arm
endef

include $(INCLUDE_DIR)/target.mk
DEFAULT_PACKAGES += wpad-mini kmod-gpio-dev gpioctl\
		kmod-usb-core kmod-usb-storage kmod-usb-ohci \
		kmod-sound-core kmod-sound-soc-s3c24xx-uda134x \
		kmod-mmc-s3c24xx  \
		kmod-i2c-core kmod-i2c-s3c24xx \
		kmod-spi-s3c24xx kmod-spi-s3c24xx-gpio \
		kmod-leds-gpio kmod-ledtrig-netdev kmod-ledtrig-usbdev \
		kmod-button-hotplug kmod-input-gpio-keys \
		kmod-fs-ext2 kmod-fs-ext3 kmod-fs-ext2 kmod-nls-utf8 kmod-nls-cp936 kmod-nls-iso8859-1 kmod-nls-cp437 kmod-fs-vfat

$(eval $(call BuildTarget))
