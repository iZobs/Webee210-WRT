#
# Copyright (C) 2012 OpenWrt.org.cn
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define Profile/minimal
  NAME:=(minimal)$(BOARDNAME)
  PACKAGES:=
endef

define Profile/minimal/Description
	None any other Packages(not default) set compatible with the $(BOARDNAME) hardware.
endef
$(eval $(call Profile,minimal))

