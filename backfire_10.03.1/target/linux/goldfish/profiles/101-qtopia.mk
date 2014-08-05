#
# Copyright (C) 2012 OpenWrt.org.cn
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define Profile/qtopia
  NAME:=(qtopia)$(BOARDNAME)
  PACKAGES:=qtopia libts
endef

define Profile/qtopia/Description
	Packages are set as a Qtopia PDA compatible with the $(BOARDNAME) hardware.
endef
$(eval $(call Profile,qtopia))

