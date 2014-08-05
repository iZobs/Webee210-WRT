#
# Copyright (C) 2008 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define Profile/qtopia
  NAME:=(qtopia)$(BOARDNAME)
  PACKAGES:=qtopia
endef

define Profile/qtopia/Description
	Packages set compatible with the $(BOARDNAME) hardware as Qtopia PDA.
endef
$(eval $(call Profile,qtopia))

