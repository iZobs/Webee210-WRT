#
# Copyright (C) 2008 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define Profile/paroli
  NAME:=(paroli)$(BOARDNAME)
  PACKAGES:=xserver-xorg xf86-video-glamo xf86-input-tslib enlightenment fso-gsm0710muxd fso-frameworkd fso-gpsd paroli wireless-tools block-hotplug uhttpd wpad wpa-supplicant python python-gzip python-openssl python-shutil python-sqlite3 python-gdbm python-doc
endef

define Profile/paroli/Description
	Package set with accelerated x11-environment and phone-suite (paroli) in the $(BOARDNAME) hardware.
endef
$(eval $(call Profile,paroli))

