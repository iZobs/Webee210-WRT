#
# Copyright (C) 2009 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define Profile/YeeLoong2F-wifi
  NAME:=YeeLoong2F(8089) wifi
  PACKAGES:=kmod-usb-core kmod-usb-ohci kmod-usb2 kmod-usb-storage \
	kmod-fs-ext2 kmod-nls-cp437 kmod-nls-iso8859-1 kmod-fs-vfat \
	kmod-loop e2fsprogs kmod-spi-bitbang \
	kmod-ipt-nathelper-extra kmod-input-gpio-buttons \
	kmod-eeprom-93cx6 kmod-rtl8187 kmod-madwifi \
	hostapd wpa-supplicant iw
endef

define Profile/YeeLoong2F-wifi/Description
	YeeLoong2F (Lemote 2F 8089)
endef
$(eval $(call Profile,YeeLoong2F-wifi))

