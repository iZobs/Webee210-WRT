/*
  This is part of the rtl8180-sa2400 driver
  released under the GPL (See file COPYING for details).
  Copyright (c) 2005 Andrea Merello <andreamrl@tiscali.it>
  
  This files contains programming code for the rtl8225 
  radio frontend.
  
  *Many* thanks to Realtek Corp. for their great support!
  
*/

#ifndef RTL8225H
#define RTL8225H

#include "r8187.h"

#define RTL8225_ANAPARAM_ON  0xa0000a59

// FIXME: OFF ANAPARAM MIGHT BE WRONG!
#define RTL8225_ANAPARAM_OFF 0xa00beb59
#define RTL8225_ANAPARAM2_OFF 0x840dec11

#define RTL8225_ANAPARAM2_ON  0x860c7312

void rtl8225_rf_init(struct net_device *dev);
void rtl8225z2_rf_init(struct net_device *dev);
void rtl8225z2_rf_set_chan(struct net_device *dev, short ch);
short rtl8225_is_V_z2(struct net_device *dev);
void rtl8225_rf_set_chan(struct net_device *dev,short ch);
void rtl8225_rf_close(struct net_device *dev);
short rtl8225_rf_set_sens(struct net_device *dev, short sens);
void rtl8225_host_pci_init(struct net_device *dev);
void rtl8225_host_usb_init(struct net_device *dev);
void write_rtl8225(struct net_device *dev, u8 adr, u16 data);
void rtl8225z2_rf_set_mode(struct net_device *dev) ;
void rtl8185_rf_pins_enable(struct net_device *dev);
void rtl8180_set_mode(struct net_device *dev,int mode);
void UpdateInitialGain(struct net_device *dev);
void UpdateCCKThreshold(struct net_device *dev);
void rtl8225_SetTXPowerLevel(struct net_device *dev, short ch);
void rtl8225z2_SetTXPowerLevel(struct net_device *dev, short ch);

#define RTL8225_RF_MAX_SENS 6
#define RTL8225_RF_DEF_SENS 4

extern inline  char GetTxOfdmHighPowerBias(struct net_device *dev)
{
	//
	// We should always adjust our Tx Power for 8187 and 8187B.
	// It was ever recommended not to adjust Tx Power of 8187B with Atheros AP
	// for throughput by David, but now we found it is not the issue to impact 
	// the Atheros's problem and also no adjustion for Tx Power will cause "low" 
	// throughput. By Bruce, 2007-07-03.
	//
	return 10;
}

//
//	Description:
//		Return Tx power level to minus if we are in high power state. 
//
//	Note: 
//		Adjust it according to RF if required.
//
extern inline char GetTxCckHighPowerBias(struct net_device *dev)
{
	return 7;
}



extern u8 rtl8225_agc[];

extern u32 rtl8225_chan[];

#endif
