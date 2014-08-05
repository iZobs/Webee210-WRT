#
# Copyright (C) 2008 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define Profile/qtopia
  NAME:=(qtopia)$(BOARDNAME)
  PACKAGES:=tslib libsqlite3 zbedic filebrowser \
            konqueror-embedded murphypinyin qtopia sdl-paladin sdl-paladin-desktop \
            starter-dialog title-language
endef

define Profile/qtopia/Description
	Packages are set as a qtopia-2.2.0 PDA compatible with the $(BOARDNAME) hardware.
endef
$(eval $(call Profile,qtopia))

