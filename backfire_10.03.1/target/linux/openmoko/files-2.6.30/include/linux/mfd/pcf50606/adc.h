/*
 * adc.h  -- Driver for NXP PCF50606 ADC
 *
 * (C) 2006-2008 by Openmoko, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __LINUX_MFD_PCF50606_ADC_H
#define __LINUX_MFD_PCF50606_ADC_H

#include <linux/mfd/pcf50633/core.h>
#include <linux/platform_device.h>

/* ADC Registers */
#define PCF50606_REG_ADCC1 0x2e
#define PCF50606_REG_ADCC2 0x2f
#define PCF50606_REG_ADCS1 0x30
#define PCF50606_REG_ADCS2 0x31
#define PCF50606_REG_ADCS3 0x32

#define PCF50606_ADCC1_TSCMODACT	 0x01
#define PCF50606_ADCC1_TSCMODSTB	 0x02
#define PCF50606_ADCC1_TRATSET		 0x04
#define PCF50606_ADCC1_NTCSWAPE		 0x08
#define PCF50606_ADCC1_NTCSWAOFF	 0x10
#define PCF50606_ADCC1_EXTSYNCBREAK	 0x20
	/* reserved */
#define PCF50606_ADCC1_TSCINT		 0x80

#define PCF50606_ADCC2_ADCSTART		 0x01
	/* see enum pcf50606_adcc2_adcmux */
#define PCF50606_ADCC2_SYNC_NONE	 0x00
#define PCF50606_ADCC2_SYNC_TXON	 0x20
#define PCF50606_ADCC2_SYNC_PWREN1	 0x40
#define PCF50606_ADCC2_SYNC_PWREN2	 0x60
#define PCF50606_ADCC2_RES_10BIT	 0x00
#define PCF50606_ADCC2_RES_8BIT		 0x80

#define PCF50606_ADCC2_ADCMUX_MASK	(0xf << 1)

#define ADCMUX_SHIFT	1
#define PCF50606_ADCMUX_BATVOLT_RES	 (0x0 << ADCMUX_SHIFT)
#define PCF50606_ADCMUX_BATVOLT_SUBTR	 (0x1 << ADCMUX_SHIFT)
#define PCF50606_ADCMUX_ADCIN1_RES	 (0x2 << ADCMUX_SHIFT)
#define PCF50606_ADCMUX_ADCIN1_SUBTR	 (0x3 << ADCMUX_SHIFT)
#define PCF50606_ADCMUX_BATTEMP		 (0x4 << ADCMUX_SHIFT)
#define PCF50606_ADCMUX_ADCIN2		 (0x5 << ADCMUX_SHIFT)
#define PCF50606_ADCMUX_ADCIN3		 (0x6 << ADCMUX_SHIFT)
#define PCF50606_ADCMUX_ADCIN3_RATIO	 (0x7 << ADCMUX_SHIFT)
#define PCF50606_ADCMUX_XPOS		 (0x8 << ADCMUX_SHIFT)
#define PCF50606_ADCMUX_YPOS		 (0x9 << ADCMUX_SHIFT)
#define PCF50606_ADCMUX_P1		 (0xa << ADCMUX_SHIFT)
#define PCF50606_ADCMUX_P2		 (0xb << ADCMUX_SHIFT)
#define PCF50606_ADCMUX_BATVOLT_ADCIN1	 (0xc << ADCMUX_SHIFT)
#define PCF50606_ADCMUX_XY_SEQUENCE	 (0xe << ADCMUX_SHIFT)
#define PCF50606_P1_P2_RESISTANCE	 (0xf << ADCMUX_SHIFT)

#define PCF50606_ADCS2_ADCRDY		 0x80

extern int
pcf50606_adc_async_read(struct pcf50606 *pcf, int mux,
		void (*callback)(struct pcf50606 *, void *, int),
		void *callback_param);
extern int
pcf50606_adc_sync_read(struct pcf50606 *pcf, int mux);

#endif /* __LINUX_PCF50606_ADC_H */
