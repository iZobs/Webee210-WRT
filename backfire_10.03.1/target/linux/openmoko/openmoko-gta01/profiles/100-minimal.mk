#
# Copyright (C) 2008 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define Profile/openmoko-gta01-minimal
  NAME:=Openmoko GTA-01 (minimal)
  PACKAGES:=
endef

define Profile/openmoko-gta01-minimal/Description
	minimal Package set compatible with the Openmoko GTA-01 hardware
endef
$(eval $(call Profile,openmoko-gta01-minimal))

