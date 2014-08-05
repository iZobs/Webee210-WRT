/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:
    r8187_led.h

Abstract:
    definitions and stuctures for rtl8187 led control.
    
Major Change History:
      When        Who	What
    ----------	------	----------------------------------------------
    2006-09-07	Xiong	Created

Notes:

--*/

#ifndef R8187_LED_H
#define R8187_LED_H

#include <linux/types.h>
#include <linux/timer.h>


/*--------------------------Define -------------------------------------------*/
//
// 0x7E-0x7F is reserved for SW customization. 2006.04.21, by rcnjko.
//
// BIT[0-7] is for CustomerID where value 0x00 and 0xFF is reserved for Realtek.
#define EEPROM_SW_REVD_OFFSET		0x7E

#define EEPROM_CID_MASK			0x00FF
#define EEPROM_CID_RSVD0			0x00
#define EEPROM_CID_RSVD1			0xFF
#define EEPROM_CID_ALPHA0			0x01
#define EEPROM_CID_SERCOMM_PS	0x02
#define EEPROM_CID_HW_LED			0x03

#define EEPROM_CID_QMI				0x07 	//Added by lizhaoming 2008.6.3
#define EEPROM_CID_DELL				0x08	        //Added by lizhaoming 2008.6.3

#define LED_BLINK_NORMAL_INTERVAL	100	//by lizhaoming 50 -> 100
#define LED_BLINK_SLOWLY_INTERVAL	200

// Customized for AzWave, 2006.04.03, by rcnjko.
#define LED_CM2_BLINK_ON_INTERVAL		250
#define LED_CM2_BLINK_OFF_INTERVAL	4750
//

// Customized for Sercomm Printer Server case, 2006.04.21, by rcnjko.
#define LED_CM3_BLINK_INTERVAL			1500

// by lizhaoming 2008.6.3: Customized for QMI.
//
#define LED_CM4_BLINK_ON_INTERVAL		500
#define LED_CM4_BLINK_OFF_INTERVAL		4500


/*--------------------------Define MACRO--------------------------------------*/


/*------------------------------Define Struct---------------------------------*/
typedef	enum _LED_STATE_8187{
	LED_UNKNOWN = 0,
	LED_ON = 1,
	LED_OFF = 2,
	LED_BLINK_NORMAL = 3,
	LED_BLINK_SLOWLY = 4,
	LED_POWER_ON_BLINK = 5,
	LED_SCAN_BLINK = 6, 	// LED is blinking during scanning period, the # of times to blink is depend on time for scanning.
	LED_NO_LINK_BLINK = 7,	// LED is blinking during no link state.
	LED_BLINK_CM3 = 8, 		// Customzied for Sercomm Printer Server case
}LED_STATE_8187;

typedef enum _RT_CID_TYPE {
	RT_CID_DEFAULT,
	RT_CID_8187_ALPHA0,
	RT_CID_8187_SERCOMM_PS,
	RT_CID_8187_HW_LED,

	RT_CID_87B_QMI ,	//Added by lizhaoming 2008.6.3
	RT_CID_87B_DELL,	//Added by lizhaoming 2008.6.3
	
} RT_CID_TYPE;

typedef	enum _LED_STRATEGY_8187{
	SW_LED_MODE0, // SW control 1 LED via GPIO0. It is default option.
	SW_LED_MODE1, // 2 LEDs, through LED0 and LED1. For ALPHA.
	SW_LED_MODE2, // SW control 1 LED via GPIO0, customized for AzWave 8187 minicard.
	SW_LED_MODE3, // SW control 1 LED via GPIO0, customized for Sercomm Printer Server case.
	SW_LED_MODE4, //added by lizhaoming for bluetooth 2008.6.3
	SW_LED_MODE5, //added by lizhaoming for bluetooth 2008.6.3			
	HW_LED, 		// HW control 2 LEDs, LED0 and LED1 (there are 4 different control modes, see MAC.CONFIG1 for details.)
}LED_STRATEGY_8187, *PLED_STRATEGY_8187;

typedef enum _LED_PIN_8187{
	LED_PIN_GPIO0,
	LED_PIN_LED0,
	LED_PIN_LED1
}LED_PIN_8187;

//by lizhaoming for LED 2008.6.23 into ieee80211.h
//typedef enum _LED_CTL_MODE {
//	LED_CTL_POWER_ON,
//	LED_CTL_POWER_OFF,
//	LED_CTL_LINK,
//	LED_CTL_NO_LINK,
//	LED_CTL_TX,
//	LED_CTL_RX,
//	LED_CTL_SITE_SURVEY,
//} LED_CTL_MODE;

typedef struct _LED_8187{
	LED_PIN_8187		LedPin;	// Identify how to implement this SW led.

	LED_STATE_8187		CurrLedState; // Current LED state.
	u8				bLedOn; // TRUE if LED is ON, FALSE if LED is OFF.

	u8				bLedBlinkInProgress; // TRUE if it is blinking, FALSE o.w..
	u32				BlinkTimes; // Number of times to toggle led state for blinking.
	LED_STATE_8187		BlinkingLedState; // Next state for blinking, either LED_ON or LED_OFF are.
	struct timer_list		BlinkTimer;  // Timer object for led blinking.
} LED_8187, *PLED_8187;



/*------------------------Export global variable------------------------------*/


/*------------------------------Funciton declaration--------------------------*/
void
InitSwLeds(
	struct net_device *dev
	);

void
DeInitSwLeds(
	struct net_device *dev
	);

void
InitLed8187(
	struct net_device *dev, 
	PLED_8187		pLed,
	LED_PIN_8187		LedPin,
	void	* 		BlinkCallBackFunc);

void
DeInitLed8187(
	struct net_device *dev, 
	PLED_8187			pLed);

void
LedControl8187(
	struct net_device *dev,
	LED_CTL_MODE		LedAction
);

void
SwLedControlMode0(
	struct net_device *dev,
	LED_CTL_MODE		LedAction
);

void
SwLedControlMode1(
	struct net_device *dev,
	LED_CTL_MODE		LedAction
);

void
SwLedControlMode2(
	struct net_device *dev,
	LED_CTL_MODE		LedAction
);

void
SwLedControlMode3(
	struct net_device *dev,
	LED_CTL_MODE		LedAction
);


void
SwLedControlMode4(
	struct net_device *dev,
	LED_CTL_MODE		LedAction
);


void
SwLedControlMode5(
	struct net_device *dev,
	LED_CTL_MODE		LedAction
);

void
Gpio0LedBlinkTimerCallback(
	unsigned long		data
	);

void
SwLed0BlinkTimerCallback(
	unsigned long		data
	);

void
SwLed1BlinkTimerCallback(
	unsigned long		data
	);

void
PlatformSwLedBlink(
	struct net_device *dev,
	PLED_8187		pLed
	);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
void
Gpio0LedWorkItemCallback(
	void *			Context
	);

void
SwLed0WorkItemCallback(
	void *			Context
	);

void
SwLed1WorkItemCallback(
	void *			Context
	);
#else
void
Gpio0LedWorkItemCallback(struct work_struct *work);

void
SwLed0WorkItemCallback(struct work_struct *work);

void
SwLed1WorkItemCallback(struct work_struct *work);

#endif
void
SwLedBlink(
	struct net_device *dev, 
	PLED_8187			pLed
	);

void
SwLedCm2Blink(
	struct net_device *dev, 
	PLED_8187			pLed
	);

void
SwLedCm4Blink(
	struct net_device *dev, 
	PLED_8187			pLed
	);

void
SwLedOn(
	struct net_device *dev, 
	PLED_8187			pLed
);

void
SwLedOff(
	struct net_device *dev, 
	PLED_8187			pLed
);


#endif
