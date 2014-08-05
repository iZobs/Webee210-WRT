/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
 	r8187_led.c
	
Abstract:
 	RTL8187 LED control functions
 	    
Major Change History:
	When        Who      What
	----------    ---------------   -------------------------------
	2006-09-07    Xiong		Created
    
Notes:	
 	
--*/

/*--------------------------Include File------------------------------------*/
#include "ieee80211/ieee80211.h"
#include "r8180_hw.h"
#include "r8187.h"
#include "r8180_93cx6.h" 
#include "r8187_led.h"

/**
*
* Initialization function for Sw Leds controll. 
* 
* \param dev      The net device for this driver.
* \return void.
*
* Note: 
* 
*/

void
InitSwLeds(
	struct net_device *dev
	)
{

	struct r8180_priv *priv = ieee80211_priv(dev);
	u16	usValue;
//	printk("========>%s()\n", __FUNCTION__);

//	priv->CustomerID = RT_CID_87B_DELL; //by lizhaoming for DELL 2008.6.3
	priv->CustomerID = RT_CID_DEFAULT; //just set to default now
	priv->bEnableLedCtrl = 1;
	priv->PsrValue = read_nic_byte(dev, PSR);
	usValue = eprom_read(dev, EEPROM_SW_REVD_OFFSET >> 1);
	priv->EEPROMCustomerID = (u8)( usValue & EEPROM_CID_MASK );
	DMESG("EEPROM Customer ID: %02X", priv->EEPROMCustomerID);

	if(priv->CustomerID == RT_CID_DEFAULT)
	{ // If we have not yet change priv->CustomerID in register, 
	  // we initialzie it from that of EEPROM with proper translation, 2006.07.03, by rcnjko.
		switch(priv->EEPROMCustomerID)
		{
		case EEPROM_CID_RSVD0:
		case EEPROM_CID_RSVD1:
			priv->CustomerID = RT_CID_DEFAULT;
			break;
	
		case EEPROM_CID_ALPHA0:
			priv->CustomerID = RT_CID_8187_ALPHA0;
			break;
	
		case EEPROM_CID_SERCOMM_PS:
			priv->CustomerID = RT_CID_8187_SERCOMM_PS;
			break;
	
		case EEPROM_CID_HW_LED:
			priv->CustomerID = RT_CID_8187_HW_LED;
			break;
			
		case EEPROM_CID_QMI:
			priv->CustomerID = RT_CID_87B_QMI;			
			break;		

		case EEPROM_CID_DELL:
			priv->CustomerID = RT_CID_87B_DELL;			
			break;	
			
		default:
			// Invalid value, so, we use default value instead.
			priv->CustomerID = RT_CID_DEFAULT;
			break;
		}
	}
	switch(priv->CustomerID)
	{
	case RT_CID_DEFAULT: 
		priv->LedStrategy = SW_LED_MODE0;
		break;
	
	case RT_CID_8187_ALPHA0:
		priv->LedStrategy = SW_LED_MODE1;
		break;	

	case RT_CID_8187_SERCOMM_PS:
		priv->LedStrategy = SW_LED_MODE3;
		break;

	case RT_CID_87B_QMI:
		priv->LedStrategy = SW_LED_MODE4;
		break;		

	case RT_CID_87B_DELL:
		priv->LedStrategy = SW_LED_MODE5;
		break;	

	case RT_CID_8187_HW_LED:
		priv->LedStrategy = HW_LED;
		break;

	default:
		priv->LedStrategy = SW_LED_MODE0;
		break;
	}
	
	InitLed8187(dev, 
				&(priv->Gpio0Led), 
				LED_PIN_GPIO0, 
				Gpio0LedBlinkTimerCallback);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	INIT_WORK(&priv->Gpio0LedWorkItem, 
				(void(*)(void*))Gpio0LedWorkItemCallback, dev);

	InitLed8187(dev,
				&(priv->SwLed0), 
				LED_PIN_LED0, 
				SwLed0BlinkTimerCallback);
	INIT_WORK(&priv->SwLed0WorkItem, 
				(void(*)(void*))SwLed0WorkItemCallback, dev);

	InitLed8187(dev,
				&(priv->SwLed1), 
				LED_PIN_LED1, 
				SwLed1BlinkTimerCallback);
	INIT_WORK(&priv->SwLed1WorkItem, 
				(void(*)(void*))SwLed1WorkItemCallback, dev);
#else
INIT_WORK(&priv->Gpio0LedWorkItem, 
				Gpio0LedWorkItemCallback);

	InitLed8187(dev,
				&(priv->SwLed0), 
				LED_PIN_LED0, 
				SwLed0BlinkTimerCallback);
	INIT_WORK(&priv->SwLed0WorkItem, 
				SwLed0WorkItemCallback);

	InitLed8187(dev,
				&(priv->SwLed1), 
				LED_PIN_LED1, 
				SwLed1BlinkTimerCallback);
	INIT_WORK(&priv->SwLed1WorkItem, 
				SwLed1WorkItemCallback);
#endif
}

void
DeInitSwLeds(
	struct net_device *dev
	)
{
	struct r8180_priv *priv = ieee80211_priv(dev);

//	printk("=========>%s In\n", __FUNCTION__);
	DeInitLed8187(dev, &(priv->Gpio0Led));
	DeInitLed8187(dev, &(priv->SwLed0));
	DeInitLed8187(dev, &(priv->SwLed1));
}

void
InitLed8187(
	struct net_device *dev, 
	PLED_8187		pLed,
	LED_PIN_8187		LedPin,
	void	* 		BlinkCallBackFunc)
{
//	printk("=========>%s In\n", __FUNCTION__);
	pLed->LedPin = LedPin;

	pLed->bLedOn = 0;
	pLed->CurrLedState = LED_OFF;

	pLed->bLedBlinkInProgress = 0;
	pLed->BlinkTimes = 0;
	pLed->BlinkingLedState = LED_OFF;

	init_timer(&(pLed->BlinkTimer));
	pLed->BlinkTimer.data = (unsigned long)dev;
	pLed->BlinkTimer.function = BlinkCallBackFunc;
	//PlatformInitializeTimer(dev, &(pLed->BlinkTimer), BlinkCallBackFunc);
}

void
DeInitLed8187(
	struct net_device *dev, 
	PLED_8187			pLed)
{
	//printk("=========>%s In\n", __FUNCTION__);
	//PlatformCancelTimer(dev, &(pLed->BlinkTimer));
	del_timer_sync(&(pLed->BlinkTimer));
	// We should reset bLedBlinkInProgress if we cancel the LedControlTimer, 2005.03.10, by rcnjko.
	pLed->bLedBlinkInProgress = 0;
}

void
LedControl8187(
	struct net_device *dev,
	LED_CTL_MODE		LedAction
)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
//	printk("=========>%s In\n", __FUNCTION__);
	if( priv->bEnableLedCtrl == 0)
		return;


	if(	priv->eRFPowerState != eRfOn && 
		(LedAction == LED_CTL_TX || LedAction == LED_CTL_RX || 
		 LedAction == LED_CTL_SITE_SURVEY || 
		 LedAction == LED_CTL_LINK || 
		 LedAction == LED_CTL_NO_LINK) )
	{
		return;
	}


	switch(priv->LedStrategy)
	{
	case SW_LED_MODE0:
		SwLedControlMode0(dev, LedAction);
		break;

	case SW_LED_MODE1:
		SwLedControlMode1(dev, LedAction);
		break;

	case SW_LED_MODE2:
		SwLedControlMode2(dev, LedAction);
		break;

	case SW_LED_MODE3:
		SwLedControlMode3(dev, LedAction);
		break;
	case SW_LED_MODE4:
		SwLedControlMode4(dev, LedAction);		
		break;

	case SW_LED_MODE5:
		SwLedControlMode5(dev, LedAction);		
		break;

	default:
		break;
	}
}


//
//	Description:	
//		Implement each led action for SW_LED_MODE0.
//		This is default strategy.
//
void
SwLedControlMode0(
	struct net_device *dev,
	LED_CTL_MODE		LedAction
)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	PLED_8187 pLed = &(priv->Gpio0Led);

//	printk("===+++++++++++++++======>%s In\n", __FUNCTION__);
	// Decide led state
	switch(LedAction)
	{
	case LED_CTL_TX:
	case LED_CTL_RX:
		if( pLed->bLedBlinkInProgress == 0 )
		{
			pLed->CurrLedState = LED_BLINK_NORMAL;
			pLed->BlinkTimes = 2;
	//		printk("===========>LED_CTL_TX/RX \n");
		}
		else
		{
			return;
		}
		break;

	case LED_CTL_SITE_SURVEY:
		if( pLed->bLedBlinkInProgress == 0 )
		{
			pLed->CurrLedState = LED_BLINK_SLOWLY;
		//	pLed->BlinkTimes = 10;
			//printk("===========>LED_CTL_SURVEY \n");
		}
		else
		{
			return;
		}
		break;

	case LED_CTL_LINK:
	//	printk("===========>associate commplite LED_CTL_LINK\n");
		pLed->CurrLedState = LED_ON;
		break;

	case LED_CTL_NO_LINK:
		pLed->CurrLedState = LED_OFF;
		break;
	
	case LED_CTL_POWER_ON:
	//	printk("===========>LED_CTL_POWER_ON\n");
		pLed->CurrLedState = LED_POWER_ON_BLINK;
		break;

	case LED_CTL_POWER_OFF:
		pLed->CurrLedState = LED_OFF;
		break;

	default:
		return;
		break;
	}

	// Change led state.
	switch(pLed->CurrLedState)
	{
	case LED_ON:
		if( pLed->bLedBlinkInProgress == 0 )
		{
			SwLedOn(dev, pLed);
		}
		break;

	case LED_OFF://modified by lizhaoming 2008.6.23
	//	if( pLed->bLedBlinkInProgress == 0 )
	//	{
	//		SwLedOff(dev, pLed);
	//	}
	
	if(pLed->bLedBlinkInProgress )/////////lizhaoming
		{
			del_timer_sync(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = FALSE;
		}
		SwLedOff(dev, pLed);
		break;

	case LED_BLINK_NORMAL:
		if( pLed->bLedBlinkInProgress == 0 )
		{
			pLed->bLedBlinkInProgress = 1;
			if( pLed->bLedOn )
				pLed->BlinkingLedState = LED_OFF; 
			else
				pLed->BlinkingLedState = LED_ON; 
			
			//pLed->BlinkTimer.expires = jiffies + LED_BLINK_NORMAL_INTERVAL;
			//add_timer(&(pLed->BlinkTimer));
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_BLINK_NORMAL_INTERVAL));
			//PlatformSetTimer(dev, &(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
		}
		break;

	case LED_BLINK_SLOWLY:
		if( pLed->bLedBlinkInProgress == 0 )
		{
			//printk("=======>%s SLOWLY\n", __func__);
			pLed->bLedBlinkInProgress = 1;
		//	if( pLed->bLedOn )
				pLed->BlinkingLedState = LED_OFF;//for LED_SHIN is LED on 
		//	else
		//		pLed->BlinkingLedState = LED_ON;
			
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_BLINK_SLOWLY_INTERVAL));
		}
		break;

	case LED_POWER_ON_BLINK:
		SwLedOn(dev, pLed);
#ifdef LED_SHIN
		mdelay(100);
		SwLedOff(dev, pLed);
#endif
		break;

	default:
		break;
	}
}

//
//	Description:	
//		Implement each led action for SW_LED_MODE1.
//		For example, this is applied by ALPHA.
//
void
SwLedControlMode1(
	struct net_device *dev,
	LED_CTL_MODE		LedAction
)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	PLED_8187 pLed0 = &(priv->SwLed0);
	PLED_8187 pLed1 = &(priv->SwLed1);
//	printk("=====++++++++++++++++++++++====>%s In\n", __FUNCTION__);

	switch(LedAction)
	{
	case LED_CTL_TX:
		if( pLed0->bLedBlinkInProgress == 0 )
		{
			pLed0->CurrLedState = LED_BLINK_NORMAL;
			pLed0->BlinkTimes = 2;
			pLed0->bLedBlinkInProgress = 1;
			if( pLed0->bLedOn )
				pLed0->BlinkingLedState = LED_OFF; 
			else
				pLed0->BlinkingLedState = LED_ON; 

			//pLed0->BlinkTimer.expires = jiffies + LED_BLINK_NORMAL_INTERVAL;
			//add_timer(&(pLed0->BlinkTimer));
			mod_timer(&pLed0->BlinkTimer, jiffies + MSECS(LED_BLINK_NORMAL_INTERVAL));
			//PlatformSetTimer(dev, &(pLed0->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
		}
		break;

	case LED_CTL_LINK:
		pLed0->CurrLedState = LED_ON;
		if( pLed0->bLedBlinkInProgress == 0 )
		{
			SwLedOn(dev, pLed0);
		}
		break;

	case LED_CTL_NO_LINK:
		pLed0->CurrLedState = LED_OFF;
		if( pLed0->bLedBlinkInProgress == 0 )
		{
			SwLedOff(dev, pLed0);
		}
		break;
	
	case LED_CTL_POWER_ON:
		pLed0->CurrLedState = LED_OFF;
		SwLedOff(dev, pLed0);

		pLed1->CurrLedState = LED_ON;
		SwLedOn(dev, pLed1);

		break;

	case LED_CTL_POWER_OFF:
		pLed0->CurrLedState = LED_OFF;
		SwLedOff(dev, pLed0);

		pLed1->CurrLedState = LED_OFF;
		SwLedOff(dev, pLed1);
		break;

	case LED_CTL_SITE_SURVEY:
		if( pLed0->bLedBlinkInProgress == 0 )
		{
			pLed0->CurrLedState = LED_BLINK_SLOWLY;;
			pLed0->BlinkTimes = 10;
			pLed0->bLedBlinkInProgress = 1;
			if( pLed0->bLedOn )
				pLed0->BlinkingLedState = LED_OFF; 
			else
				pLed0->BlinkingLedState = LED_ON;

			//pLed0->BlinkTimer.expires = jiffies + LED_BLINK_SLOWLY_INTERVAL;
			//add_timer(&(pLed0->BlinkTimer));
			mod_timer(&pLed0->BlinkTimer, jiffies + MSECS(LED_BLINK_SLOWLY_INTERVAL));
			//PlatformSetTimer(dev, &(pLed0->BlinkTimer), LED_BLINK_SLOWLY_INTERVAL);
		}
		break;

	default:
		break;
	}
}

//
//	Description:	
//		Implement each led action for SW_LED_MODE2, 
//		which is customized for AzWave 8187 minicard.  
//		2006.04.03, by rcnjko.
//
void
SwLedControlMode2(
	struct net_device *dev,
	LED_CTL_MODE		LedAction
)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	PLED_8187 pLed = &(priv->Gpio0Led);

//	printk("====+++++++++++++++++++++=====>%s In\n", __FUNCTION__);
	// Decide led state
	switch(LedAction)
	{
	case LED_CTL_TX:
	case LED_CTL_RX:
		if( pLed->bLedBlinkInProgress == 0 )
		{
			pLed->bLedBlinkInProgress = 1;

			pLed->CurrLedState = LED_BLINK_NORMAL;
			pLed->BlinkTimes = 2;

			if( pLed->bLedOn )
				pLed->BlinkingLedState = LED_OFF; 
			else
				pLed->BlinkingLedState = LED_ON; 

			//pLed->BlinkTimer.expires = jiffies + LED_BLINK_NORMAL_INTERVAL;
			//add_timer(&(pLed->BlinkTimer));
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_BLINK_NORMAL_INTERVAL));
			//PlatformSetTimer(dev, &(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
		}
		break;

	case LED_CTL_SITE_SURVEY:
		if( pLed->bLedBlinkInProgress == 0 )
		{
			pLed->bLedBlinkInProgress = 1;

			//if(	dev->MgntInfo.mAssoc || 
			//	dev->MgntInfo.mIbss )
			//{
				pLed->CurrLedState = LED_SCAN_BLINK;
				pLed->BlinkTimes = 4;
			//}
			//else
			//{
			//	pLed->CurrLedState = LED_NO_LINK_BLINK;
			//	pLed->BlinkTimes = 24;
			//}

			if( pLed->bLedOn )
			{
				pLed->BlinkingLedState = LED_OFF;
				//pLed->BlinkTimer.expires = jiffies + LED_CM2_BLINK_ON_INTERVAL;
				//add_timer(&(pLed->BlinkTimer));
				mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_CM2_BLINK_ON_INTERVAL));
				//PlatformSetTimer(dev, &(pLed->BlinkTimer), LED_CM2_BLINK_ON_INTERVAL);
			}
			else
			{
				pLed->BlinkingLedState = LED_ON; 
				//pLed->BlinkTimer.expires = jiffies + LED_CM2_BLINK_OFF_INTERVAL;
				//add_timer(&(pLed->BlinkTimer));
				mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_CM2_BLINK_OFF_INTERVAL));
				//PlatformSetTimer(dev, &(pLed->BlinkTimer), LED_CM2_BLINK_OFF_INTERVAL);
			}
		}
		else
		{
			if(pLed->CurrLedState != LED_NO_LINK_BLINK)
			{
				pLed->CurrLedState = LED_SCAN_BLINK;
				/*
				if(	dev->MgntInfo.mAssoc || 
					dev->MgntInfo.mIbss )
				{
					pLed->CurrLedState = LED_SCAN_BLINK;
				}
				else
				{
					pLed->CurrLedState = LED_NO_LINK_BLINK;
				}
				*/
			}
		}
		break;

	case LED_CTL_NO_LINK:
		if( pLed->bLedBlinkInProgress == 0 )
		{
			pLed->bLedBlinkInProgress = 1;

			pLed->CurrLedState = LED_NO_LINK_BLINK;
			pLed->BlinkTimes = 24;

			if( pLed->bLedOn )
			{
				pLed->BlinkingLedState = LED_OFF; 
				//pLed->BlinkTimer.expires = jiffies + LED_CM2_BLINK_ON_INTERVAL;
				//add_timer(&(pLed->BlinkTimer));
				mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_CM2_BLINK_ON_INTERVAL));
				//PlatformSetTimer(dev, &(pLed->BlinkTimer), LED_CM2_BLINK_ON_INTERVAL);
			}
			else
			{
				pLed->BlinkingLedState = LED_ON; 
				//pLed->BlinkTimer.expires = jiffies + LED_CM2_BLINK_OFF_INTERVAL;
				//add_timer(&(pLed->BlinkTimer));
				mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_CM2_BLINK_OFF_INTERVAL));
				//PlatformSetTimer(dev, &(pLed->BlinkTimer), LED_CM2_BLINK_OFF_INTERVAL);
			}
		}
		else
		{
			pLed->CurrLedState = LED_NO_LINK_BLINK;
		}
		break;

	case LED_CTL_LINK:
		pLed->CurrLedState = LED_ON;
		if( pLed->bLedBlinkInProgress == 0 )
		{
			SwLedOn(dev, pLed);
		}
		break;

	case LED_CTL_POWER_OFF:
		pLed->CurrLedState = LED_OFF;
		if( pLed->bLedBlinkInProgress == 0 )
		{
			SwLedOff(dev, pLed);
		}
		break;

	default:
		break;
	}
}


//
//	Description:	
//		Implement each led action for SW_LED_MODE3, 
//		which is customized for Sercomm Printer Server case. 
//		2006.04.21, by rcnjko.
//
void
SwLedControlMode3(
	struct net_device *dev,
	LED_CTL_MODE		LedAction
)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	PLED_8187 pLed = &(priv->Gpio0Led);

//	printk("=====+++++++++++++++++++====>%s In\n", __FUNCTION__);
	// Decide led state
	switch(LedAction)
	{
	case LED_CTL_TX:
	case LED_CTL_RX:
		if( pLed->bLedBlinkInProgress == 0 )
		{
			pLed->bLedBlinkInProgress = 1;

			pLed->CurrLedState = LED_BLINK_CM3;
			pLed->BlinkTimes = 2;

			if( pLed->bLedOn )
				pLed->BlinkingLedState = LED_OFF; 
			else
				pLed->BlinkingLedState = LED_ON; 

			//pLed->BlinkTimer.expires = jiffies + LED_CM3_BLINK_INTERVAL;
			//add_timer(&(pLed->BlinkTimer));
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_CM3_BLINK_INTERVAL));
			//PlatformSetTimer(dev, &(pLed->BlinkTimer), LED_CM3_BLINK_INTERVAL);
		}
		break;

	case LED_CTL_SITE_SURVEY:
		if( pLed->bLedBlinkInProgress == 0 )
		{
			pLed->bLedBlinkInProgress = 1;

			pLed->CurrLedState = LED_BLINK_CM3;
			pLed->BlinkTimes = 10;

			if( pLed->bLedOn )
				pLed->BlinkingLedState = LED_OFF; 
			else
				pLed->BlinkingLedState = LED_ON; 

			//pLed->BlinkTimer.expires = jiffies + LED_CM3_BLINK_INTERVAL;
			//add_timer(&(pLed->BlinkTimer));
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_CM3_BLINK_INTERVAL));
			//PlatformSetTimer(dev, &(pLed->BlinkTimer), LED_CM3_BLINK_INTERVAL);
		}
		break;

	case LED_CTL_LINK:
		pLed->CurrLedState = LED_ON;
		if( pLed->bLedBlinkInProgress == 0 )
		{
			SwLedOn(dev, pLed);
		}
		break;

	case LED_CTL_NO_LINK:
		pLed->CurrLedState = LED_OFF;
		if( pLed->bLedBlinkInProgress == 0 )
		{
			SwLedOff(dev, pLed);
		}
		break;

	case LED_CTL_POWER_ON:
		pLed->CurrLedState = LED_POWER_ON_BLINK;
		SwLedOn(dev, pLed);
		mdelay(100);
		SwLedOff(dev, pLed);
		break;

	case LED_CTL_POWER_OFF:
		pLed->CurrLedState = LED_OFF;
		if( pLed->bLedBlinkInProgress == 0 )
		{
			SwLedOff(dev, pLed);
		}
		break;

	default:
		break;
	}
}

// added by lizhaoming 2008.6.2
//
//	Description:	
//		Implement each led action for SW_LED_MODE4, 
//		which is customized for QMI 8187B minicard. 
//		2008.04.21, by chiyokolin.
//
void
SwLedControlMode4(
	struct net_device *dev,
	LED_CTL_MODE		LedAction
	)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	PLED_8187 pLed = &(priv->Gpio0Led);

	//printk("=====+++++++++++++++++++++====>%s In\n", __FUNCTION__);
	// Decide led state
	switch(LedAction)
	{
	case LED_CTL_TX:
	case LED_CTL_RX:
		//if( pLed->bLedBlinkInProgress == false && !priv->bScanInProgress)//?????
		if( pLed->bLedBlinkInProgress == 0)
		{
			pLed->bLedBlinkInProgress = 1;

			pLed->CurrLedState = LED_BLINK_NORMAL;
			pLed->BlinkTimes = 2;

			if( pLed->bLedOn )
				pLed->BlinkingLedState = LED_OFF; 
			else
				pLed->BlinkingLedState = LED_ON; 

			//pLed->BlinkTimer.expires = jiffies + LED_BLINK_NORMAL_INTERVAL;
			//add_timer(&(pLed->BlinkTimer));
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_BLINK_NORMAL_INTERVAL));
			//PlatformSetTimer(Adapter, &(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
		}
		else
			//printk("----->LED_CTL_RX/TX bLedBlinkInProgress\n");
			
		break;

	case LED_CTL_SITE_SURVEY:
		if( pLed->bLedBlinkInProgress == 0 )
		{

			pLed->bLedBlinkInProgress = 1;			
			//if(	priv->MgntInfo.mAssoc || priv->MgntInfo.mIbss )//////////??????
			//{				
				pLed->CurrLedState = LED_SCAN_BLINK;	
				pLed->BlinkTimes = 10;	

				pLed->BlinkingLedState = LED_ON; 	

				//pLed->BlinkTimer.expires = jiffies + LED_BLINK_NORMAL_INTERVAL;
				//add_timer(&(pLed->BlinkTimer));
				mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_BLINK_NORMAL_INTERVAL));
				//PlatformSetTimer(Adapter, &(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);				
			//}
			//else
			//{
			//	pLed->CurrLedState = LED_NO_LINK_BLINK;
			//	pLed->BlinkTimes = 24;
			//
			//	if( pLed->bLedOn )
			//	{
			//		pLed->BlinkingLedState = LED_OFF; 
			//		
			//		pLed->BlinkTimer.expires = jiffies + LED_CM4_BLINK_ON_INTERVAL;
			//		add_timer(&(pLed->BlinkTimer));
			//		//PlatformSetTimer(Adapter, &(pLed->BlinkTimer), LED_CM4_BLINK_ON_INTERVAL);
			//	}
			//	else
			//	{
			//		pLed->BlinkingLedState = LED_ON; 

			//		pLed->BlinkTimer.expires = jiffies + LED_CM4_BLINK_OFF_INTERVAL;
			//		add_timer(&(pLed->BlinkTimer));
			//		//PlatformSetTimer(Adapter, &(pLed->BlinkTimer), LED_CM4_BLINK_OFF_INTERVAL);
			//	}				
			//}
		}
		else
		{
			if(pLed->CurrLedState != LED_NO_LINK_BLINK)
			{
				//if(	priv->MgntInfo.mAssoc || priv->MgntInfo.mIbss )//???????????
				//{
				//}
				//else
				//{
				//	pLed->CurrLedState = LED_NO_LINK_BLINK;
				//}
			}

			//printk("----->LED_CTL_SITE_SURVEY bLedBlinkInProgress\n");			
		}
		break;

	case LED_CTL_NO_LINK:
		if( pLed->bLedBlinkInProgress == 0 )
		{
			pLed->bLedBlinkInProgress = 1;

			pLed->CurrLedState = LED_NO_LINK_BLINK;
			pLed->BlinkTimes = 24;

			if( pLed->bLedOn )
			{
				pLed->BlinkingLedState = LED_OFF; 

				//pLed->BlinkTimer.expires = jiffies + LED_CM4_BLINK_ON_INTERVAL;
				//add_timer(&(pLed->BlinkTimer));
				mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_CM4_BLINK_ON_INTERVAL));
				//PlatformSetTimer(Adapter, &(pLed->BlinkTimer), LED_CM4_BLINK_ON_INTERVAL);
			}
			else
			{
				pLed->BlinkingLedState = LED_ON; 

				//pLed->BlinkTimer.expires = jiffies + LED_CM4_BLINK_OFF_INTERVAL;
				//add_timer(&(pLed->BlinkTimer));
				mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_CM4_BLINK_OFF_INTERVAL));
				//PlatformSetTimer(Adapter, &(pLed->BlinkTimer), LED_CM4_BLINK_OFF_INTERVAL);
			}
		}
		else
		{
			pLed->CurrLedState = LED_NO_LINK_BLINK;
			//printk("----->LED_CTL_NO_LINK bLedBlinkInProgress\n");						
		}
		break;

	case LED_CTL_LINK:
		pLed->CurrLedState = LED_ON;
		if( pLed->bLedBlinkInProgress == 0)
		{
			SwLedOn(dev, pLed);
		}
		else
			;//printk("----->LED_CTL_LINK bLedBlinkInProgress\n");						
			
		break;

	case LED_CTL_POWER_OFF:
		pLed->CurrLedState = LED_OFF;
		if(pLed->bLedBlinkInProgress)
		{
			printk("----->LED_CTL_POWER_OFF bLedBlinkInProgress\n");						
		
			//PlatformCancelTimer(Adapter, &(pLed->BlinkTimer));
			del_timer_sync(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = 0;
		}
		SwLedOff(dev, pLed);
		break;

	default:
		break;
	}
}



//added by lizhaoming 2008.6.3
//
//	Description:	
//		Implement each led action for SW_LED_MODE5, 
//		which is customized for DELL 8187B minicard. 
//		2008.04.24, by chiyokolin.
//
void
SwLedControlMode5(	
	struct net_device *dev,
	LED_CTL_MODE		LedAction
	)
{	
	struct r8180_priv *priv = ieee80211_priv(dev);
	PLED_8187 pLed = &(priv->Gpio0Led);
	
	// Decide led state
	//printk("====++++++++++++++++++++++=====>%s In\n", __FUNCTION__);
	switch(LedAction)
	{
	case LED_CTL_TX:
	case LED_CTL_RX:
	case LED_CTL_SITE_SURVEY:
	case LED_CTL_POWER_ON:		
	case LED_CTL_NO_LINK:
	case LED_CTL_LINK:
		pLed->CurrLedState = LED_ON;
		if( pLed->bLedBlinkInProgress == 0 )
		{
			pLed->bLedBlinkInProgress = 1;
			if(! pLed->bLedOn )
				pLed->BlinkingLedState = LED_ON; 
			else
				break; 
			
			//printk("====++++++++++++++++++++++=====>%s In LED:%d\n", __FUNCTION__, pLed->bLedOn);
			//pLed->BlinkTimer.expires = jiffies + LED_BLINK_NORMAL_INTERVAL;
			//add_timer(&(pLed->BlinkTimer));
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_BLINK_NORMAL_INTERVAL));
		//	SwLedOn(dev, pLed);
		}
		else
			;//printk("----->LED_CTL_LINK bLedBlinkInProgress\n");						
			
		break;

	case LED_CTL_POWER_OFF:
		pLed->CurrLedState = LED_OFF;
	//	printk("<====++++++++++++++++++++++=====%s In LED:%d\n", __FUNCTION__, pLed->bLedOn);
		if(pLed->bLedBlinkInProgress)
		{
		//	printk("----->LED_CTL_POWER_OFF bLedBlinkInProgress\n");						

			//PlatformCancelTimer(Adapter, &(pLed->BlinkTimer));
			del_timer_sync(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = 0;
		}
		SwLedOff(dev, pLed);
		break;

	default:
		break;
	}
}

//
// Callback fuction of the timer, Gpio0Led.BlinkTimer.
//
void
Gpio0LedBlinkTimerCallback(
	unsigned long		data
	)
{
	struct net_device *dev = (struct net_device *)data;
	struct r8180_priv *priv = ieee80211_priv(dev);

//	printk("=========>%s In\n", __FUNCTION__);
	PlatformSwLedBlink(dev, &(priv->Gpio0Led));
}



//
// Callback fuction of the timer, SwLed0.BlinkTimer.
//
void
SwLed0BlinkTimerCallback(
	unsigned long		data
	)
{
	struct net_device *dev = (struct net_device *)data;
	struct r8180_priv *priv = ieee80211_priv(dev);

//	printk("=========>%s In\n", __FUNCTION__);
	PlatformSwLedBlink(dev, &(priv->SwLed0));
}



//
// Callback fuction of the timer, SwLed1.BlinkTimer.
//
void
SwLed1BlinkTimerCallback(
	unsigned long		data
	)
{
	struct net_device *dev = (struct net_device *)data;
	struct r8180_priv *priv = ieee80211_priv(dev);

//	printk("=========>%s In\n", __FUNCTION__);
	PlatformSwLedBlink(dev, &(priv->SwLed1));
}

void
PlatformSwLedBlink(
	struct net_device *dev,
	PLED_8187		pLed
	)
{
	struct r8180_priv *priv = ieee80211_priv(dev);

//	printk("=========>%s In\n", __FUNCTION__);
	switch(pLed->LedPin)
	{
	case LED_PIN_GPIO0:
		schedule_work(&(priv->Gpio0LedWorkItem));
		break;

	case LED_PIN_LED0:
		schedule_work(&(priv->SwLed0WorkItem));
		break;

	case LED_PIN_LED1:
		schedule_work(&(priv->SwLed1WorkItem));
		break;

	default:
		break;
	}
}

// 
// Callback fucntion of the workitem for SW LEDs.
// 2006.03.01, by rcnjko.
//

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
void Gpio0LedWorkItemCallback(struct work_struct *work)
{
	struct r8180_priv *priv = container_of(work, struct r8180_priv,Gpio0LedWorkItem);
	struct net_device *dev = priv->ieee80211->dev;
#else
void
Gpio0LedWorkItemCallback(
	void *			Context
	)
{
	struct net_device *dev = (struct net_device *)Context;
	struct r8180_priv *priv = ieee80211_priv(dev);
#endif
	PLED_8187	pLed = &(priv->Gpio0Led); 
	if (priv == NULL || dev == NULL){
//	printk("=========>%s In\n", __FUNCTION__);
	//printk("ft=====================>%s()\n", __FUNCTION__);
	}
	
#if 0 // by lizahoming 2008.6.3
	if(priv->LedStrategy == SW_LED_MODE2)
		SwLedCm2Blink(dev, pLed);	
	else
		SwLedBlink(dev, pLed);
#endif

#if 1 // by lizahoming 2008.6.3
	switch(priv->LedStrategy)
	{
	case SW_LED_MODE2:
		SwLedCm2Blink(dev, pLed);
		break;
	case SW_LED_MODE4:
		SwLedCm4Blink(dev, pLed);
		break;		
	default:
		SwLedBlink(dev, pLed);
		break;
	}
#endif

	//LeaveCallbackOfRtWorkItem( &(usbdevice->Gpio0LedWorkItem) );
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
void SwLed0WorkItemCallback(struct work_struct *work)
{
	//struct r8180_priv *priv = container_of(work, struct r8180_priv, SwLed0WorkItem);
	//struct net_device *dev = priv->dev;
#else
void  SwLed0WorkItemCallback(void *	Context)
{
	//struct net_device *dev = (struct net_device *)Context;
	//struct r8180_priv *priv = ieee80211_priv(dev);
#endif
	//SwLedBlink(dev, &(priv->SwLed0));
//	printk("=========>%s In\n", __FUNCTION__);

	//LeaveCallbackOfRtWorkItem( &(usbdevice->SwLed0WorkItem) );
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
void SwLed1WorkItemCallback(struct work_struct *work)
{
	//struct r8180_priv *priv = container_of(work, struct r8180_priv, SwLed1WorkItem);
//	struct net_device *dev = priv->dev;
#else
void
SwLed1WorkItemCallback(
	void *			Context
	)
{
	//struct net_device *dev = (struct net_device *)Context;
	//struct r8180_priv *priv = ieee80211_priv(dev);
#endif
//	printk("=========>%s In\n", __FUNCTION__);
	//SwLedBlink(dev, &(priv->SwLed1));

	//LeaveCallbackOfRtWorkItem( &(usbdevice->SwLed1WorkItem) );
}

//
//	Implementation of LED blinking behavior.
//	It toggle off LED and schedule corresponding timer if necessary.
//
void
SwLedBlink(
	struct net_device *dev, 
	PLED_8187			pLed
	)
{
	u8 bStopBlinking = 0;

	//printk("=========>%s In state:%d\n", __FUNCTION__, pLed->CurrLedState);
	// Change LED according to BlinkingLedState specified.
	if( pLed->BlinkingLedState == LED_ON ) 
	{
		SwLedOn(dev, pLed);
//		printk("Blinktimes (%d): turn on\n", pLed->BlinkTimes);
	}	
	else 
	{
		SwLedOff(dev, pLed);
//		printk("Blinktimes (%d): turn off\n", pLed->BlinkTimes);
	}

	// Determine if we shall change LED state again.
//by lizhaoming for LED BLINK SLOWLY
	if(pLed->CurrLedState == LED_BLINK_SLOWLY)
	{
		bStopBlinking = 0;
	} else {
		pLed->BlinkTimes--;
		if(	pLed->BlinkTimes == 0 )
		{
			bStopBlinking = 1;
		}
		else
		{
			if(	pLed->CurrLedState != LED_BLINK_NORMAL &&
					pLed->CurrLedState != LED_BLINK_SLOWLY &&
					pLed->CurrLedState != LED_BLINK_CM3	)
			{
				bStopBlinking = 1;
			}
		}
	}

	if(bStopBlinking)
	{
		if(	pLed->CurrLedState == LED_ON && pLed->bLedOn == 0)
		{
			SwLedOn(dev, pLed);
		}
		else if(pLed->CurrLedState == LED_OFF && pLed->bLedOn == 1)
		{
			SwLedOff(dev, pLed);
		}

		pLed->BlinkTimes = 0;
		pLed->bLedBlinkInProgress = 0;	
	}
	else
	{
		// Assign LED state to toggle.
		if( pLed->BlinkingLedState == LED_ON ) 
			pLed->BlinkingLedState = LED_OFF;
		else 
			pLed->BlinkingLedState = LED_ON;

		// Schedule a timer to toggle LED state. 
		switch( pLed->CurrLedState )
		{
		case LED_BLINK_NORMAL:
			//printk("LED_BLINK_NORMAL:Blinktimes (%d): turn off\n", pLed->BlinkTimes+1);
			//pLed->BlinkTimer.expires = jiffies + LED_BLINK_NORMAL_INTERVAL;
			//add_timer(&(pLed->BlinkTimer));		
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_BLINK_NORMAL_INTERVAL));
			//PlatformSetTimer(dev, &(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
			break;

		case LED_BLINK_SLOWLY:
			if( pLed->bLedOn == 1 ) 
			{
			//printk("LED_BLINK_SLOWLY:turn off\n");
			//pLed->BlinkTimer.expires = jiffies + LED_BLINK_SLOWLY_INTERVAL+50;//for pcie mini card spec page 33, 250ms
			//add_timer(&(pLed->BlinkTimer));			
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_BLINK_SLOWLY_INTERVAL+50));
			pLed->BlinkingLedState = LED_OFF;			
			} else {
			//printk("LED_BLINK_SLOWLY:turn on\n");
			//pLed->BlinkTimer.expires = jiffies + 5000;//for pcie mini card spec page 33, 5s
			//add_timer(&(pLed->BlinkTimer));
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(5000));
			pLed->BlinkingLedState = LED_ON;			
			}	
			break;

		case LED_BLINK_CM3:
			//printk("LED_BLINK_CM3:Blinktimes (%d): turn off\n", pLed->BlinkTimes+1);
			//pLed->BlinkTimer.expires = jiffies + LED_CM3_BLINK_INTERVAL;
			//add_timer(&(pLed->BlinkTimer));			
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_CM3_BLINK_INTERVAL));
			//PlatformSetTimer(dev, &(pLed->BlinkTimer), LED_CM3_BLINK_INTERVAL);
			break;

		default:
			//printk("LED_BLINK_default:Blinktimes (%d): turn off\n", pLed->BlinkTimes+1);
			//pLed->BlinkTimer.expires = jiffies + LED_BLINK_SLOWLY_INTERVAL;
			//add_timer(&(pLed->BlinkTimer));			
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_BLINK_SLOWLY_INTERVAL));
			//PlatformSetTimer(dev, &(pLed->BlinkTimer), LED_BLINK_SLOWLY_INTERVAL);
			break;
		}
	}
}



//
//	Implementation of LED blinking behavior for SwLedControlMode2. 
//
void
SwLedCm2Blink(
	struct net_device *dev, 
	PLED_8187			pLed
	)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	//PMGNT_INFO priv = &(dev->MgntInfo);
	u8 bStopBlinking = 0;
	
	//printk("========+++++++++++++=>%s In\n", __FUNCTION__);
	//To avoid LED blinking when rf is off, add by lizhaoming 2008.6.2
	if((priv->eRFPowerState == eRfOff) && (priv->RfOffReason>RF_CHANGE_BY_IPS))
	{
		SwLedOff(dev, pLed);

		//pLed->BlinkTimer.expires = jiffies + LED_CM2_BLINK_ON_INTERVAL;
		//add_timer(&(pLed->BlinkTimer));
		mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_CM2_BLINK_ON_INTERVAL));
		//PlatformSetTimer(Adapter, &(pLed->BlinkTimer), LED_CM2_BLINK_ON_INTERVAL);
		//printk(" Hw/Soft Radio Off, turn off Led\n");
		return;
	}
	
	// Change LED according to BlinkingLedState specified.
	if( pLed->BlinkingLedState == LED_ON ) 
	{
		SwLedOn(dev, pLed);
		//DMESG("Blinktimes (%d): turn on\n", pLed->BlinkTimes);
	}	
	else 
	{
		SwLedOff(dev, pLed);
		//DMESG("Blinktimes (%d): turn off\n", pLed->BlinkTimes);
	}

	//Add by lizhaoming for avoid BlinkTimers <0, 2008.6.2
	if(pLed->BlinkTimes > 0)
	{//by lizhaoming 2008.6.2		
	// Determine if we shall change LED state again.
	pLed->BlinkTimes--;	
	}//by lizhaoming 2008.6.2	
		
	switch(pLed->CurrLedState)
	{
	case LED_BLINK_NORMAL: 
		if(pLed->BlinkTimes == 0)
		{
			bStopBlinking = 1;
		}
		break;
/* CM2 scan blink and no link blind now not be supported 
	case LED_SCAN_BLINK:
		if( (priv->mAssoc || priv->mIbss) &&  // Linked.
			(!priv->bScanInProgress) && // Not in scan stage.
			(pLed->BlinkTimes % 2 == 0)) // Even
		{
			bStopBlinking = 1;
		}
		break;

	case LED_NO_LINK_BLINK:
		//Revised miniCard Ad-hoc mode "Slow Blink" by Isaiah 2006-08-03	
		//if( (priv->mAssoc || priv->mIbss) ) // Linked.
		if( priv->mAssoc) 
		{
			bStopBlinking = 1;
		}
		else if(priv->mIbss && priv->bMediaConnect )
		{
			bStopBlinking = 1;
		}
		break;
*/
	default:
		bStopBlinking = 1;
		break;
	}

	if(bStopBlinking)
	{
/*
		if( priv->eRFPowerState != eRfOn )
		{
			SwLedOff(dev, pLed);
		}
		else if( priv->bMediaConnect == 1 && pLed->bLedOn == 0)
		{
			SwLedOn(dev, pLed);
		}
		else if( priv->bMediaConnect == 0 &&  pLed->bLedOn == 1)
		{
			SwLedOff(dev, pLed);
		}
*/
		pLed->BlinkTimes = 0;
		pLed->bLedBlinkInProgress = 0;	
	}
	else
	{
		// Assign LED state to toggle.
		if( pLed->BlinkingLedState == LED_ON ) 
			pLed->BlinkingLedState = LED_OFF;
		else 
			pLed->BlinkingLedState = LED_ON;

		// Schedule a timer to toggle LED state. 
		switch( pLed->CurrLedState )
		{
		case LED_BLINK_NORMAL:
			//pLed->BlinkTimer.expires = jiffies + LED_BLINK_NORMAL_INTERVAL;
			//add_timer(&(pLed->BlinkTimer));			
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_BLINK_NORMAL_INTERVAL));
			//PlatformSetTimer(dev, &(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
			break;

		case LED_BLINK_SLOWLY:
			//pLed->BlinkTimer.expires = jiffies + LED_BLINK_SLOWLY_INTERVAL;
			//add_timer(&(pLed->BlinkTimer));			
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_BLINK_SLOWLY_INTERVAL));
			//PlatformSetTimer(dev, &(pLed->BlinkTimer), LED_BLINK_SLOWLY_INTERVAL);
			break;

		case LED_SCAN_BLINK:
		case LED_NO_LINK_BLINK:
			if( pLed->bLedOn ) {
				//pLed->BlinkTimer.expires = jiffies + LED_CM2_BLINK_ON_INTERVAL;
				//add_timer(&(pLed->BlinkTimer));				
				mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_CM2_BLINK_ON_INTERVAL));
				//PlatformSetTimer(dev, &(pLed->BlinkTimer), LED_CM2_BLINK_ON_INTERVAL);
			} else {
				//pLed->BlinkTimer.expires = jiffies + LED_CM2_BLINK_OFF_INTERVAL;
				//add_timer(&(pLed->BlinkTimer));		
				mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_CM2_BLINK_OFF_INTERVAL));
				//PlatformSetTimer(dev, &(pLed->BlinkTimer), LED_CM2_BLINK_OFF_INTERVAL);
			}
			break;

		default:
			//RT_ASSERT(0, ("SwLedCm2Blink(): unexpected state!\n"));
			//pLed->BlinkTimer.expires = jiffies + LED_BLINK_SLOWLY_INTERVAL;
			//add_timer(&(pLed->BlinkTimer));
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_BLINK_SLOWLY_INTERVAL));
			//PlatformSetTimer(dev, &(pLed->BlinkTimer), LED_BLINK_SLOWLY_INTERVAL);
			break;
		}
	}
}

// added by lizhaoming 2008.6.2
//
//	Description:
//		Implement LED blinking behavior for SW_LED_MODE4. 
//
void
SwLedCm4Blink(
	struct net_device *dev, 
	PLED_8187			pLed
	)
{	
	struct r8180_priv *priv = ieee80211_priv(dev);	
	u8 bStopBlinking = 0;

	printk("======++++++++++++++++++======>%s In\n", __FUNCTION__);
	//To avoid LED blinking when rf is off, add by Maddest 20080307
	if((priv->eRFPowerState == eRfOff) && (priv->RfOffReason>RF_CHANGE_BY_IPS))
	{
		SwLedOff(dev, pLed);

		//pLed->BlinkTimer.expires = jiffies + LED_CM4_BLINK_ON_INTERVAL;
		//add_timer(&(pLed->BlinkTimer));		
		mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_CM4_BLINK_ON_INTERVAL));
		//PlatformSetTimer(Adapter, &(pLed->BlinkTimer), LED_CM4_BLINK_ON_INTERVAL);
		printk(" Hw/Soft Radio Off, turn off Led\n");
		return;
	}
	// Change LED according to BlinkingLedState specified.
	if( pLed->BlinkingLedState == LED_ON ) 
	{
		if(!pLed->bLedOn)
		{
		SwLedOn(dev, pLed);
		}
		printk("Blinktimes (%d): turn on\n", pLed->BlinkTimes);
	}	
	else 
	{
		SwLedOff(dev, pLed);
		printk("Blinktimes (%d): turn off\n", pLed->BlinkTimes);
	}

	//Add by Maddest for avoid BlinkTimers <0, 20080307;
	if(pLed->BlinkTimes > 0)
	{
		// Determine if we shall change LED state again.
		pLed->BlinkTimes--;
	}
	printk("pLed->CurrLedState   %d  pLed->BlinkTimes  %d\n", pLed->CurrLedState,pLed->BlinkTimes);
	switch(pLed->CurrLedState)
	{
	case LED_BLINK_NORMAL: 
		if(pLed->BlinkTimes == 0)
		{
			bStopBlinking = 1;
		}
		break;

/* CM2 scan blink and no link blind now not be supported 
	case LED_SCAN_BLINK:
		if( (priv->mAssoc || priv->mIbss) &&  // Linked.//????????????
			(!priv->bScanInProgress) && // Not in scan stage.//????????????
			(pLed->BlinkTimes % 2 == 0)) // Even
		{
			bStopBlinking = 1;
		}
		break;

	case LED_NO_LINK_BLINK:
		//Revised miniCard Ad-hoc mode "Slow Blink" by Isaiah 2006-08-03	
		//if( (pMgntInfo->mAssoc || pMgntInfo->mIbss) ) // Linked.
		if( priv->mAssoc) //????????????
		{
			bStopBlinking = 1;
		}
		else if(priv->mIbss && priv->bMediaConnect )//????????????
		{
			bStopBlinking = 1;
		}
		break;
*/

	default:
		bStopBlinking = 1;
		break;
	}

	if(bStopBlinking)
	{
	/*
		if( priv->eRFPowerState != eRfOn )
		{
			SwLedOff(dev, pLed);
		}
		else if( priv->bMediaConnect == true && pLed->bLedOn == false)//????????????
		{
			SwLedOn(dev, pLed);
		}
		else if( priv->bMediaConnect == false &&  pLed->bLedOn == true)//????????????
		{
			SwLedOff(dev, pLed);
		}
	*/

		pLed->BlinkTimes = 0;
		pLed->bLedBlinkInProgress = 0;	
	}
	else
	{
		// Assign LED state to toggle.
		if( pLed->BlinkingLedState == LED_ON ) 
			pLed->BlinkingLedState = LED_OFF;
		else 
			pLed->BlinkingLedState = LED_ON;

		// Schedule a timer to toggle LED state. 
		switch( pLed->CurrLedState )
		{
		case LED_BLINK_NORMAL:
			//pLed->BlinkTimer.expires = jiffies + LED_BLINK_NORMAL_INTERVAL;
			//add_timer(&(pLed->BlinkTimer));	
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_BLINK_NORMAL_INTERVAL));
			//PlatformSetTimer(Adapter, &(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
			break;

		case LED_BLINK_SLOWLY:
			//pLed->BlinkTimer.expires = jiffies + LED_BLINK_SLOWLY_INTERVAL;
			//add_timer(&(pLed->BlinkTimer));	
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_BLINK_SLOWLY_INTERVAL));
			//PlatformSetTimer(Adapter, &(pLed->BlinkTimer), LED_BLINK_SLOWLY_INTERVAL);
			break;

		case LED_SCAN_BLINK:
			pLed->BlinkingLedState = LED_ON;
			//pLed->BlinkTimer.expires = jiffies + LED_BLINK_NORMAL_INTERVAL;
			//add_timer(&(pLed->BlinkTimer));	
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_BLINK_NORMAL_INTERVAL));
			//PlatformSetTimer(Adapter, &(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
			
		case LED_NO_LINK_BLINK:
			if( pLed->bLedOn ){
				//pLed->BlinkTimer.expires = jiffies + LED_CM4_BLINK_ON_INTERVAL;
				//add_timer(&(pLed->BlinkTimer));	
				mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_CM4_BLINK_ON_INTERVAL));
			//PlatformSetTimer(Adapter, &(pLed->BlinkTimer), LED_CM4_BLINK_ON_INTERVAL);
			}else{
				//pLed->BlinkTimer.expires = jiffies + LED_CM4_BLINK_OFF_INTERVAL;
				//add_timer(&(pLed->BlinkTimer));	
				mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_CM4_BLINK_OFF_INTERVAL));
				//PlatformSetTimer(Adapter, &(pLed->BlinkTimer), LED_CM4_BLINK_OFF_INTERVAL);
			}
			break;

		default:
			printk("SwLedCm2Blink(): unexpected state!\n");
			//pLed->BlinkTimer.expires = jiffies + LED_BLINK_SLOWLY_INTERVAL;
			//add_timer(&(pLed->BlinkTimer));	
			mod_timer(&pLed->BlinkTimer, jiffies + MSECS(LED_BLINK_SLOWLY_INTERVAL));
			//PlatformSetTimer(Adapter, &(pLed->BlinkTimer), LED_BLINK_SLOWLY_INTERVAL);
			break;
		}		
	}
}

void
SwLedOn(
	struct net_device *dev, 
	PLED_8187			pLed
)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
//	printk("=========>%s(), pin:%d\n", __FUNCTION__, pLed->LedPin);
	switch(pLed->LedPin)
	{
	case LED_PIN_GPIO0:
		write_nic_byte(dev,0x0091,0x01);
		write_nic_byte(dev,0x0090,0x00);	// write 0 : LED on
		break;

	case LED_PIN_LED0:
		priv->PsrValue &= ~(0x01 << 4);
		write_nic_byte(dev, PSR, priv->PsrValue);
		break;

	case LED_PIN_LED1:
		priv->PsrValue &= ~(0x01 << 5);
		write_nic_byte(dev, PSR, priv->PsrValue);
		break;

	default:
		break;
	}

	pLed->bLedOn = 1;
}

void
SwLedOff(
	struct net_device *dev, 
	PLED_8187			pLed
)
{
	struct r8180_priv *priv = ieee80211_priv(dev);


	//printk("=========>%s(), pin:%d\n", __FUNCTION__, pLed->LedPin);
	switch(pLed->LedPin)
	{
	case LED_PIN_GPIO0:
		write_nic_byte(dev,0x0091,0x01);
		write_nic_byte(dev,0x0090,0x01);	// write 1 : LED off
		break;

	case LED_PIN_LED0:
		priv->PsrValue |= (0x01 << 4);
		write_nic_byte(dev, PSR, priv->PsrValue);
		break;

	case LED_PIN_LED1:
		priv->PsrValue |= (0x01 << 5);
		write_nic_byte(dev, PSR, priv->PsrValue);
		break;

	default:
		break;
	}

	pLed->bLedOn = 0;
}	

