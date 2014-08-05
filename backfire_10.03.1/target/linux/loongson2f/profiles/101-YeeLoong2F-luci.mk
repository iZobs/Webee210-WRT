#
# Copyright (C) 2009 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define Profile/YeeLoong2F-luci
  NAME:=YeeLoong2F(8089) LUCI
  PACKAGES:=luci luci-i18n-chinese luci-sgi-uhttpd luci-app-ddns \
	  luci-app-hd-idle luci-app-openvpn luci-app-qos luci-app-samba \
	  luci-app-upnp luci-app-ushare luci-app-wol \
	  openssh-client openssh-client-utils openssh-server
endef

define Profile/YeeLoong2F-luci/Description
	YeeLoong2F (Lemote 2F 8089)
endef
$(eval $(call Profile,YeeLoong2F-luci))

