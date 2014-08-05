/*
 * mbc.h  -- Driver for NXP PCF50606 Main Battery Charger
 *
 * (C) 2006-2008 by Openmoko, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __LINUX_MFD_PCF50606_MBC_H
#define __LINUX_MFD_PCF50606_MBC_H

#include <linux/mfd/pcf50606/core.h>
#include <linux/platform_device.h>

#define PCF50606_REG_OOCS	0x01

/* Charger OK */
#define PCF50606_OOCS_CHGOK	0x20

#define PCF50606_REG_MBCC1	0x29
#define PCF50606_REG_MBCC2	0x2a
#define PCF50606_REG_MBCC3	0x2b
#define PCF50606_REG_MBCS1	0x2c

#define PCF50606_MBCC1_CHGAPE		0x01
#define PCF50606_MBCC1_AUTOFST		0x02
#define PCF50606_MBCC1_CHGMOD_MASK	0x1c
#define PCF50606_MBCC1_CHGMOD_QUAL	0x00
#define PCF50606_MBCC1_CHGMOD_PRE	0x04
#define PCF50606_MBCC1_CHGMOD_TRICKLE	0x08
#define PCF50606_MBCC1_CHGMOD_FAST_CCCV	0x0c
#define PCF50606_MBCC1_CHGMOD_FAST_NOCC	0x10
#define PCF50606_MBCC1_CHGMOD_FAST_NOCV	0x14
#define PCF50606_MBCC1_CHGMOD_FAST_SW	0x18
#define PCF50606_MBCC1_CHGMOD_IDLE	0x1c
#define PCF50606_MBCC1_DETMOD_LOWCHG	0x20
#define PCF50606_MBCC1_DETMOD_WDRST	0x40

#define PCF50606_MBCC1_CHGMOD_SHIFT	2

/* Charger status */
#define PCF50606_MBC_CHARGER_ONLINE	0x01
#define PCF50606_MBC_CHARGER_ACTIVE	0x02

int pcf50606_charge_fast(struct pcf50606 *pcf, int on);

#endif

