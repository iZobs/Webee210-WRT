#
# Copyright (C) 2008 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define Profile/qtopia
  NAME:=(qtopia)$(BOARDNAME)
  PACKAGES:=Tslib Qtopia Filebrowser MurphyPinyin Konqueror libsqlite3 Zbedic SDLPAL
endef

define Profile/qtopia/Description
	Packages are set as a qtopia-2.2.0 PDA compatible with the $(BOARDNAME) hardware.
endef
$(eval $(call Profile,qtopia))

