#
# Copyright (C) 2008 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define Profile/router
  NAME:=(router)$(BOARDNAME)
  PACKAGES:=ucitrigger udev wireless-tools  block-hotplug uhttpd wpad wpa-supplicant \
          kmod-usb-core kmod-usb-ohci  kmod-usb-storage kmod-usb-storage-extras kmod-usb2 \
          kmod-fs-ext4 kmod-fs-ext3 kmod-fs-ext2  kmod-fs-vfat \
          kmod-nls-utf8 kmod-nls-cp437  kmod-nls-iso8859-1 \
          openssh-client openssh-client-utils openssh-server \
          luci luci-i18n-chinese luci-sgi-uhttpd luci-app-ddns \
          luci-app-hd-idle luci-app-openvpn luci-app-qos luci-app-samba \
          luci-app-upnp luci-app-ushare luci-app-wol
endef

define Profile/router/Description
	Package set compatible with the $(BOARDNAME) hardware as a Router.
endef
$(eval $(call Profile,router))

