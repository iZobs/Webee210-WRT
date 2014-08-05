/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
 	r8180_dig.c
	
Abstract:
 	Hardware dynamic mechanism for RTL8187B
 	    
Major Change History:
	When        	Who      	What
	----------    ---------------   -------------------------------
	2006-11-15     david		Created

Notes:	
	This file is ported from RTL8187B Windows driver.
	
 	
--*/
#include "r8180_dm.h"
#include "r8180_hw.h"
#include "r8180_rtl8225.h"

//================================================================================
//	Local Constant.
//================================================================================
#define Z1_HIPWR_UPPER_TH			99
#define Z1_HIPWR_LOWER_TH			70	
#define Z2_HIPWR_UPPER_TH			99
#define Z2_HIPWR_LOWER_TH			90

bool CheckDig(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee80211;

	if(ieee->state != IEEE80211_LINKED)
		return false;

	if(priv->card_8187 == NIC_8187B) {
		//
		// We need to schedule dig workitem on either of the below mechanisms.
		// By Bruce, 2007-06-01.
		//
		if(!priv->bDigMechanism && !priv->bCCKThMechanism)
			return false;

		if(priv->CurrentOperaRate < 36) // Schedule Dig under all OFDM rates. By Bruce, 2007-06-01.
			return false;
	} else { 
		if(!priv->bDigMechanism)
			return false;

		if(priv->CurrentOperaRate < 48)
			return false;
	}
	return true;
}


//
//	Description:
//		Implementation of DIG for Zebra and Zebra2.	
//
void DIG_Zebra(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	//PHAL_DATA_8187	pHalData = GetHalData8187(Adapter);
	u16			CCKFalseAlarm, OFDMFalseAlarm;
	u16			OfdmFA1, OfdmFA2;
	int			InitialGainStep = 7; // The number of initial gain stages.
	int			LowestGainStage = 4; // The capable lowest stage of performing dig workitem.

//	printk("---------> DIG_Zebra()\n");

	//Read only 1 byte because of HW bug. This is a temporal modification. Joseph
	// Modify by Isaiah 2006-06-27
	if(priv->card_8187_Bversion == VERSION_8187B_B)
	{
		CCKFalseAlarm = 0;
		OFDMFalseAlarm = (u16)(priv->FalseAlarmRegValue);
		OfdmFA1 =  0x01;
		OfdmFA2 = priv->RegDigOfdmFaUpTh; 
	}
	else
	{
		CCKFalseAlarm = (u16)(priv->FalseAlarmRegValue & 0x0000ffff);
		OFDMFalseAlarm = (u16)((priv->FalseAlarmRegValue >> 16) & 0x0000ffff);
		OfdmFA1 =  0x15;
		//OfdmFA2 =  0xC00;
		OfdmFA2 = ((u16)(priv->RegDigOfdmFaUpTh)) << 8;
	}	

//	printk("DIG**********CCK False Alarm: %#X \n",CCKFalseAlarm);
//	printk("DIG**********OFDM False Alarm: %#X \n",OFDMFalseAlarm);



	// The number of initial gain steps is different, by Bruce, 2007-04-13.
	if(priv->card_8187 == NIC_8187) {
		if (priv->InitialGain == 0 ) //autoDIG
		{
			switch( priv->rf_chip)
			{
				case RF_ZEBRA:
					priv->InitialGain = 5; // m74dBm;
					break;
				case RF_ZEBRA2:
					priv->InitialGain = 4; // m78dBm;
					break;
				default:
					priv->InitialGain = 5; // m74dBm;
					break;
			}
		}
		InitialGainStep = 7;
		if(priv->InitialGain > 7)
			priv->InitialGain = 5;
		LowestGainStage = 4;
	} else {
		if (priv->InitialGain == 0 ) //autoDIG
		{ // Advised from SD3 DZ, by Bruce, 2007-06-05.
			priv->InitialGain = 4; // In 87B, m74dBm means State 4 (m82dBm)
		}
		if(priv->card_8187_Bversion != VERSION_8187B_B)
		{ // Advised from SD3 DZ, by Bruce, 2007-06-05.
			OfdmFA1 =  0x20;
		}
		InitialGainStep = 8;
		LowestGainStage = priv->RegBModeGainStage; // Lowest gain stage.
	}

	if (OFDMFalseAlarm > OfdmFA1)
	{
		if (OFDMFalseAlarm > OfdmFA2)
		{
			priv->DIG_NumberFallbackVote++;
			if (priv->DIG_NumberFallbackVote >1)
			{
				//serious OFDM  False Alarm, need fallback
				// By Bruce, 2007-03-29.
				// if (pHalData->InitialGain < 7) // In 87B, m66dBm means State 7 (m74dBm)
				if (priv->InitialGain < InitialGainStep)
				{
					priv->InitialGain = (priv->InitialGain + 1);
					//printk("DIG**********OFDM False Alarm: %#X,  OfdmFA1: %#X, OfdmFA2: %#X\n", OFDMFalseAlarm, OfdmFA1, OfdmFA2);
					//printk("DIG+++++++ fallback OFDM:%d \n", priv->InitialGain);
					UpdateInitialGain(dev); // 2005.01.06, by rcnjko.
				}
				priv->DIG_NumberFallbackVote = 0;
				priv->DIG_NumberUpgradeVote=0;
			}
		}
		else
		{
			if (priv->DIG_NumberFallbackVote)
				priv->DIG_NumberFallbackVote--;
		}
		priv->DIG_NumberUpgradeVote=0;		
	}
	else	//OFDM False Alarm < 0x15
	{
		if (priv->DIG_NumberFallbackVote)
			priv->DIG_NumberFallbackVote--;
		priv->DIG_NumberUpgradeVote++;

		if (priv->DIG_NumberUpgradeVote>9)
		{
			if (priv->InitialGain > LowestGainStage) // In 87B, m78dBm means State 4 (m864dBm)
			{
				priv->InitialGain = (priv->InitialGain - 1);
				//printk("DIG**********OFDM False Alarm: %#X,  OfdmFA1: %#X, OfdmFA2: %#X\n", OFDMFalseAlarm, OfdmFA1, OfdmFA2);
				//printk("DIG--------- Upgrade OFDM:%d \n", priv->InitialGain);
				UpdateInitialGain(dev); // 2005.01.06, by rcnjko.
			}
			priv->DIG_NumberFallbackVote = 0;
			priv->DIG_NumberUpgradeVote=0;
		}
	}

//	printk("DIG+++++++ OFDM:%d\n", priv->InitialGain);	
//	printk("<--------- DIG_Zebra()\n");
}

//
//	Description:
//		Dispatch DIG implementation according to RF. 	
//
void DynamicInitGain(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	switch(priv->rf_chip)
	{
		case RF_ZEBRA:
		case RF_ZEBRA2:  // [AnnieWorkaround] For Zebra2, 2005-08-01.
		//case RF_ZEBRA4:
			DIG_Zebra(dev);
			break;
		
		default:
			printk("DynamicInitGain(): unknown RFChipID(%d) !!!\n", priv->rf_chip);
			break;
	}
}

// By Bruce, 2007-03-29.
//
//	Description:
//		Dispatch CCK Power Detection implementation according to RF.	
//
void DynamicCCKThreshold(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	u16			CCK_Up_Th;
	u16			CCK_Lw_Th;
	u16			CCKFalseAlarm;

	printk("=====>DynamicCCKThreshold()\n");

	CCK_Up_Th = priv->CCKUpperTh;
	CCK_Lw_Th = priv->CCKLowerTh;	
	CCKFalseAlarm = (u16)((priv->FalseAlarmRegValue & 0x0000ffff) >> 8); // We only care about the higher byte.	
	printk("DynamicCCKThreshold(): CCK Upper Threshold: 0x%02X, Lower Threshold: 0x%02X, CCKFalseAlarmHighByte: 0x%02X\n", CCK_Up_Th, CCK_Lw_Th, CCKFalseAlarm);

	if(priv->StageCCKTh < 3 && CCKFalseAlarm >= CCK_Up_Th)
	{
		priv->StageCCKTh ++;
		UpdateCCKThreshold(dev);
	}
	else if(priv->StageCCKTh > 0 && CCKFalseAlarm <= CCK_Lw_Th)
	{
		priv->StageCCKTh --;
		UpdateCCKThreshold(dev);
	}
	
	printk("<=====DynamicCCKThreshold()\n");
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
void rtl8180_hw_dig_wq (struct work_struct *work)
{
        struct delayed_work *dwork = container_of(work,struct delayed_work,work);
        struct ieee80211_device *ieee = container_of(dwork,struct ieee80211_device,hw_dig_wq);
        struct net_device *dev = ieee->dev;
#else
void rtl8180_hw_dig_wq(struct net_device *dev)
{
	// struct r8180_priv *priv = ieee80211_priv(dev);
#endif
	struct r8180_priv *priv = ieee80211_priv(dev);

	// Read CCK and OFDM False Alarm.
	if(priv->card_8187_Bversion == VERSION_8187B_B) {
		// Read only 1 byte because of HW bug. This is a temporal modification. Joseph
		// Modify by Isaiah 2006-06-27
		priv->FalseAlarmRegValue = (u32)read_nic_byte(dev, (OFDM_FALSE_ALARM+1));
	} else {
		priv->FalseAlarmRegValue = read_nic_dword(dev, CCK_FALSE_ALARM);
	}

	// Adjust Initial Gain dynamically.
	if(priv->bDigMechanism) {
		DynamicInitGain(dev);
	}

	//
	// Move from DynamicInitGain to be independent of the OFDM DIG mechanism, by Bruce, 2007-06-01.
	//
	if(priv->card_8187 == NIC_8187B) {
		// By Bruce, 2007-03-29.
		// Dynamically update CCK Power Detection Threshold.
		if(priv->bCCKThMechanism)
		{
			DynamicCCKThreshold(dev);
		}
	}
}

void SetTxPowerLevel8187(struct net_device *dev, short chan)
{
	struct r8180_priv *priv = ieee80211_priv(dev);

	switch(priv->rf_chip)
	{
	case  RF_ZEBRA:
		rtl8225_SetTXPowerLevel(dev,chan);
		break;

	case RF_ZEBRA2:
	//case RF_ZEBRA4:
		rtl8225z2_SetTXPowerLevel(dev,chan);
		break;
	}
}

//
//	Description:
//		Check if input power signal strength exceeds maximum input power threshold 
//		of current HW. 
//		If yes, we set our HW to high input power state:
//			RX: always force TR switch to SW Tx mode to reduce input power. 
//			TX: turn off smaller Tx output power (see RtUsbCheckForHang).
//
//		If no, we restore our HW to normal input power state:
///			RX: restore TR switch to HW controled mode.
//			TX: restore TX output power (see RtUsbCheckForHang).
//
//	TODO: 
//		1. Tx power control shall not be done in Platform-dependent timer (e.g. RtUsbCheckForHang). 
//		2. Allow these threshold adjustable by RF SD.
//
void DoRxHighPower(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	TR_SWITCH_STATE TrSwState;
	u16		HiPwrUpperTh = 0;
	u16		HiPwrLowerTh = 0;
	u16		RSSIHiPwrUpperTh = 0;
	u16		RSSIHiPwrLowerTh = 0;	

	//87S remove TrSwitch mechanism
	if((priv->card_8187 == NIC_8187B)||(priv->card_8187 == NIC_8187)) {

		//printk("----> DoRxHighPower()\n");

		//
		// Get current TR switch setting.
		//
		//Adapter->HalFunc.GetHwRegHandler(Adapter, HW_VAR_TR_SWITCH, (pu1Byte)(&TrSwState));
		TrSwState = priv->TrSwitchState;

		//
		// Determine threshold according to RF type.
		//
		switch(priv->rf_chip)
		{
			case RF_ZEBRA:
				HiPwrUpperTh = Z1_HIPWR_UPPER_TH;
				HiPwrLowerTh = Z1_HIPWR_LOWER_TH;
				printk("DoRxHighPower(): RF_ZEBRA, Upper Threshold: %d LOWER Threshold: %d\n", 
						HiPwrUpperTh, HiPwrLowerTh);
				break;	

			case RF_ZEBRA2:
				if((priv->card_8187 == NIC_8187)) {
					HiPwrUpperTh = Z2_HIPWR_UPPER_TH;
					HiPwrLowerTh = Z2_HIPWR_LOWER_TH;
				} else {
					// By Bruce, 2007-04-11.
					// HiPwrUpperTh = Z2_HIPWR_UPPER_TH;
					// HiPwrLowerTh = Z2_HIPWR_LOWER_TH;

					HiPwrUpperTh = priv->Z2HiPwrUpperTh;
					HiPwrLowerTh = priv->Z2HiPwrLowerTh;
					HiPwrUpperTh = HiPwrUpperTh * 10;
					HiPwrLowerTh = HiPwrLowerTh * 10;

					RSSIHiPwrUpperTh = priv->Z2RSSIHiPwrUpperTh;
					RSSIHiPwrLowerTh = priv->Z2RSSIHiPwrLowerTh;
					//printk("DoRxHighPower(): RF_ZEBRA2, Upper Threshold: %d LOWER Threshold: %d, RSSI Upper Th: %d, RSSI Lower Th: %d\n",HiPwrUpperTh, HiPwrLowerTh, RSSIHiPwrUpperTh, RSSIHiPwrLowerTh);
				}
				break;	

			default:
				printk("DoRxHighPower(): Unknown RFChipID(%d), UndecoratedSmoothedSS(%d), TrSwState(%d)!!!\n", 
						priv->rf_chip, priv->UndecoratedSmoothedSS, TrSwState);
				return;
				break;	
		}

		/*printk(">>>>>>>>>>Set TR switch to software control, UndecoratedSmoothedSS:%d, CurCCKRSSI = %d\n",\
					priv->UndecoratedSmoothedSS, priv->CurCCKRSSI);
	*/
		if((priv->card_8187 == NIC_8187)) {
			//
			// Perform Rx part High Power Mechanism by UndecoratedSmoothedSS.
			//
			if (priv->UndecoratedSmoothedSS > HiPwrUpperTh)
			{ //  High input power state.
				if( priv->TrSwitchState == TR_HW_CONTROLLED )	
				{
				/*	printk(">>>>>>>>>>Set TR switch to software control, UndecoratedSmoothedSS:%d \n", \
							priv->UndecoratedSmoothedSS);
				//	printk(">>>>>>>>>> TR_SW_TX\n");
				*/
					write_nic_byte(dev, RFPinsSelect, 
							(u8)(priv->wMacRegRfPinsSelect | TR_SW_MASK_8187 ));
					write_nic_byte(dev, RFPinsOutput, 
						(u8)((priv->wMacRegRfPinsOutput&(~TR_SW_MASK_8187))|TR_SW_MASK_TX_8187));
					priv->TrSwitchState = TR_SW_TX;
					priv->bToUpdateTxPwr = true;
				}
			}
			else if (priv->UndecoratedSmoothedSS < HiPwrLowerTh)
			{ // Normal input power state. 
				if( priv->TrSwitchState == TR_SW_TX)	
				{
				/*	printk("<<<<<<<<<<<Set TR switch to hardware control UndecoratedSmoothedSS:%d \n", \
							priv->UndecoratedSmoothedSS);
				//	printk("<<<<<<<<<< TR_HW_CONTROLLED\n");
				*/
					write_nic_byte(dev, RFPinsOutput, (u8)(priv->wMacRegRfPinsOutput));
					write_nic_byte(dev, RFPinsSelect, (u8)(priv->wMacRegRfPinsSelect));
					priv->TrSwitchState = TR_HW_CONTROLLED;
					priv->bToUpdateTxPwr = true;
				}
			}
		}else {
			/*printk("=====>TrSwState = %s\n", (TrSwState==TR_HW_CONTROLLED)?"TR_HW_CONTROLLED":"TR_SW_TX");
			//printk("UndecoratedSmoothedSS:%d, CurCCKRSSI = %d\n",priv->UndecoratedSmoothedSS, priv->CurCCKRSSI); */
			// Asked by SD3 DZ, by Bruce, 2007-04-12.
			if(TrSwState == TR_HW_CONTROLLED)
			{
				if((priv->UndecoratedSmoothedSS > HiPwrUpperTh) ||
						(priv->bCurCCKPkt && (priv->CurCCKRSSI > RSSIHiPwrUpperTh)))
				{
					//printk("===============================> high power!\n");
					write_nic_byte(dev, RFPinsSelect, (u8)(priv->wMacRegRfPinsSelect|TR_SW_MASK_8187 ));
					write_nic_byte(dev, RFPinsOutput, 
						(u8)((priv->wMacRegRfPinsOutput&(~TR_SW_MASK_8187))|TR_SW_MASK_TX_8187));
					priv->TrSwitchState = TR_SW_TX;
					priv->bToUpdateTxPwr = true;
				}
			}
			else
			{
				if((priv->UndecoratedSmoothedSS < HiPwrLowerTh) &&
						(!priv->bCurCCKPkt || priv->CurCCKRSSI < RSSIHiPwrLowerTh))
				{
					write_nic_byte(dev, RFPinsOutput, (u8)(priv->wMacRegRfPinsOutput));
					write_nic_byte(dev, RFPinsSelect, (u8)(priv->wMacRegRfPinsSelect));
					priv->TrSwitchState = TR_HW_CONTROLLED;
					priv->bToUpdateTxPwr = true;
				}
			}
			//printk("<=======TrSwState = %s\n", (TrSwState==TR_HW_CONTROLLED)?"TR_HW_CONTROLLED":"TR_SW_TX");
		}
		//printk("<---- DoRxHighPower()\n");
	}
}


//
//	Description:
//		Callback function of UpdateTxPowerWorkItem.
//		Because of some event happend, e.g. CCX TPC, High Power Mechanism, 
//		We update Tx power of current channel again. 
//
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
void rtl8180_tx_pw_wq (struct work_struct *work)
{
        struct delayed_work *dwork = container_of(work,struct delayed_work,work);
        struct ieee80211_device *ieee = container_of(dwork,struct ieee80211_device,tx_pw_wq);
        struct net_device *dev = ieee->dev;
#else
void rtl8180_tx_pw_wq(struct net_device *dev)
{
	// struct r8180_priv *priv = ieee80211_priv(dev);
#endif

	struct r8180_priv *priv = ieee80211_priv(dev);

	//printk("----> UpdateTxPowerWorkItemCallback()\n");
	
	if(priv->bToUpdateTxPwr)
	{
		//printk("DoTxHighPower(): schedule UpdateTxPowerWorkItem......\n");
		priv->bToUpdateTxPwr = false;
		SetTxPowerLevel8187(dev, priv->chan);
	}
	
	DoRxHighPower(dev);	
	//printk("<---- UpdateTxPowerWorkItemCallback()\n");
}

//
//	Description:
//		Return TRUE if we shall perform High Power Mecahnism, FALSE otherwise.	
//
bool CheckHighPower(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee80211;

	if(!priv->bRegHighPowerMechanism)
	{
		return false;
	}
		
	if((ieee->state == IEEE80211_LINKED_SCANNING)||(ieee->state == IEEE80211_MESH_SCANNING))
	{
		return false;
	}

	return true;
}

#ifdef SW_ANTE_DIVERSITY

#define ANTENNA_DIVERSITY_TIMER_PERIOD          1000 // 1000 m

void
SwAntennaDiversityRxOk8185(
	struct net_device *dev, 
	u8 SignalStrength
	)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	//printk("+SwAntennaDiversityRxOk8185: RxSs: %d\n", SignalStrength);

	priv->AdRxOkCnt++;

	if( priv->AdRxSignalStrength != -1)
	{
		priv->AdRxSignalStrength = ((priv->AdRxSignalStrength*7) + (SignalStrength*3)) / 10;
	}
	else
	{ // Initialization case.
		priv->AdRxSignalStrength = SignalStrength;
	}
        
        //printk("====>pkt rcvd by %d\n", priv->LastRxPktAntenna);
	if( priv->LastRxPktAntenna ) //Main antenna.	
		priv->AdMainAntennaRxOkCnt++;	
	else	 // Aux antenna.
		priv->AdAuxAntennaRxOkCnt++;
	//printk("-SwAntennaDiversityRxOk8185: AdRxOkCnt: %d AdRxSignalStrength: %d\n", priv->AdRxOkCnt, priv->AdRxSignalStrength);
}

//
//	Description: Change Antenna Switch.
//
bool
SetAntenna8185(
	struct net_device *dev,
	u8		u1bAntennaIndex
	)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	bool bAntennaSwitched = false;

//	printk("+SetAntenna8185(): Antenna is switching to: %d \n", u1bAntennaIndex);

	switch(u1bAntennaIndex)
	{
	case 0://main antenna
		switch(priv->rf_chip)
		{
		case RF_ZEBRA:
		case RF_ZEBRA2:
		//case RF_ZEBRA4:
			// Tx Antenna.
			write_nic_byte(dev, ANTSEL, 0x03); // Config TX antenna.
			
			//PlatformEFIOWrite4Byte(Adapter, BBAddr, 0x01009b90); // Config CCK RX antenna.
			//PlatformEFIOWrite4Byte(Adapter, BBAddr, 0x5c8D); // Config OFDM RX antenna.
			
			// Rx CCK .
			write_nic_byte(dev, 0x7f, ((0x01009b90 & 0xff000000) >> 24)); 
			write_nic_byte(dev, 0x7e, ((0x01009b90 & 0x00ff0000) >> 16)); 
			write_nic_byte(dev, 0x7d, ((0x01009b90 & 0x0000ff00) >> 8)); 
			write_nic_byte(dev, 0x7c, ((0x01009b90 & 0x000000ff) >> 0)); 

			// Rx OFDM.
			write_nic_byte(dev, 0x7f, ((0x00005c8D & 0xff000000) >> 24)); 
			write_nic_byte(dev, 0x7e, ((0x00005c8D & 0x00ff0000) >> 16)); 
			write_nic_byte(dev, 0x7d, ((0x00005c8D & 0x0000ff00) >> 8)); 
			write_nic_byte(dev, 0x7c, ((0x00005c8D & 0x000000ff) >> 0)); 

                        bAntennaSwitched = true;
			break;

		default:
			printk("SetAntenna8185: unkown RFChipID(%d)\n", priv->rf_chip);
			break;
		}
		break;

	case 1:
		switch(priv->rf_chip)
		{
		case RF_ZEBRA:
		case RF_ZEBRA2:
		//case RF_ZEBRA4:
			// Tx Antenna.
			write_nic_byte(dev, ANTSEL, 0x00); // Config TX antenna.
			
			//PlatformEFIOWrite4Byte(Adapter, BBAddr, 0x0100bb90); // Config CCK RX antenna.
			//PlatformEFIOWrite4Byte(Adapter, BBAddr, 0x548D); // Config OFDM RX antenna.
			
			// Rx CCK.
			write_nic_byte(dev, 0x7f, ((0x0100bb90 & 0xff000000) >> 24)); 
			write_nic_byte(dev, 0x7e, ((0x0100bb90 & 0x00ff0000) >> 16)); 
			write_nic_byte(dev, 0x7d, ((0x0100bb90 & 0x0000ff00) >> 8)); 
			write_nic_byte(dev, 0x7c, ((0x0100bb90 & 0x000000ff) >> 0)); 

			// Rx OFDM.
			write_nic_byte(dev, 0x7f, ((0x0000548D & 0xff000000) >> 24)); 
			write_nic_byte(dev, 0x7e, ((0x0000548D & 0x00ff0000) >> 16)); 
			write_nic_byte(dev, 0x7d, ((0x0000548D & 0x0000ff00) >> 8)); 
			write_nic_byte(dev, 0x7c, ((0x0000548D & 0x000000ff) >> 0)); 

			bAntennaSwitched = true;
			break;

		default:
			printk("SetAntenna8185: unkown RFChipID(%d)\n", priv->rf_chip);
			break;
		}
		break;

	default:
		printk("SetAntenna8185: unkown u1bAntennaIndex(%d)\n", u1bAntennaIndex);
		break;
	}

	if(bAntennaSwitched)
	{
		priv->CurrAntennaIndex = u1bAntennaIndex;
	}

//	printk("-SetAntenna8185(): return (%#X)\n", bAntennaSwitched);

	return bAntennaSwitched;
}

//
//	Description: Toggle Antenna switch.
//
bool SwitchAntenna(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	bool		bResult = false;

	if(priv->CurrAntennaIndex == 0)
	{
            bResult = SetAntenna8185(dev, 1);
            if(priv->ieee80211->state == IEEE80211_LINKED)
                printk("Switching to Aux antenna 1 \n");
	}
	else
	{
            bResult = SetAntenna8185(dev, 0);
            if(priv->ieee80211->state == IEEE80211_LINKED)
                printk("Switching to Main antenna 0 \n");
	}

	return bResult;
}

//
//	Description:
//		Engine of SW Antenna Diversity mechanism.
//		Since 8187 has no Tx part information, 
//		this implementation is only dependend on Rx part information. 
//
//	2006.04.17, by rcnjko.
//
void SwAntennaDiversity(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	//bool   bSwCheckSS=false;
	bool   bSwCheckSS=true;//open the SignalStrength check if not switched by rx ok pkt.

//	printk("+SwAntennaDiversity(): CurrAntennaIndex: %d\n", priv->CurrAntennaIndex);

//by amy 080312
	if(bSwCheckSS){
		priv->AdTickCount++;
	
		//printk("(1) AdTickCount: %d, AdCheckPeriod: %d\n", priv->AdTickCount, priv->AdCheckPeriod);
		//printk("(2) AdRxSignalStrength: %ld, AdRxSsThreshold: %ld\n", priv->AdRxSignalStrength, priv->AdRxSsThreshold);
	}
//	priv->AdTickCount++;//-by amy 080312
	
	// Case 1. No Link.
	if(priv->ieee80211->state != IEEE80211_LINKED){
		//printk("SwAntennaDiversity(): Case 1. No Link.\n");

		priv->bAdSwitchedChecking = false;
		// I switch antenna here to prevent any one of antenna is broken before link established, 2006.04.18, by rcnjko..
		SwitchAntenna(dev);
	}
	// Case 2. Linked but no packet received.
	else if(priv->AdRxOkCnt == 0){
		printk("SwAntennaDiversity(): Case 2. Linked but no packet received.\n");

		priv->bAdSwitchedChecking = false;
		SwitchAntenna(dev);
	}
	// Case 3. Evaluate last antenna switch action in case4. and undo it if necessary.
	else if(priv->bAdSwitchedChecking == true){
		//printk("SwAntennaDiversity(): Case 3. Evaluate last antenna switch action.\n");

		priv->bAdSwitchedChecking = false;

		// Adjust Rx signal strength threashold.
		priv->AdRxSsThreshold = (priv->AdRxSignalStrength + priv->AdRxSsBeforeSwitched) / 2;

		priv->AdRxSsThreshold = (priv->AdRxSsThreshold > priv->AdMaxRxSsThreshold) ?	
					priv->AdMaxRxSsThreshold: priv->AdRxSsThreshold;
		if(priv->AdRxSignalStrength < priv->AdRxSsBeforeSwitched){ 
			// Rx signal strength is not improved after we swtiched antenna. => Swich back.
			printk("SwAntennaDiversity(): Rx Signal Strength is not improved, CurrRxSs: %ld, LastRxSs: %ld\n", priv->AdRxSignalStrength, priv->AdRxSsBeforeSwitched);
                        
                        //by amy 080312
			// Increase Antenna Diversity checking period due to bad decision.
			priv->AdCheckPeriod *= 2;
                        //by amy 080312
                        //
			// Increase Antenna Diversity checking period.
			if(priv->AdCheckPeriod > priv->AdMaxCheckPeriod)
				priv->AdCheckPeriod = priv->AdMaxCheckPeriod;
	
			// Wrong deceision => switch back.
			SwitchAntenna(dev);
		}else{ // Rx Signal Strength is improved. 
			printk("SwAntennaDiversity(): Rx Signal Strength is improved, CurrRxSs: %ld, LastRxSs: %ld\n", priv->AdRxSignalStrength, priv->AdRxSsBeforeSwitched);

			// Reset Antenna Diversity checking period to its min value.
			priv->AdCheckPeriod = priv->AdMinCheckPeriod;
		}

		//printk("SwAntennaDiversity(): AdRxSsThreshold: %ld, AdCheckPeriod: %d\n",
		//	priv->AdRxSsThreshold, priv->AdCheckPeriod);
	}
	// Case 4. Evaluate if we shall switch antenna now.
	// Cause Table Speed is very fast in TRC Dell Lab, we check it every time. 
	else// if(priv->AdTickCount >= priv->AdCheckPeriod)//-by amy 080312
	{
		//printk("SwAntennaDiversity(): Case 4. Evaluate if we shall switch antenna now.\n");

		priv->AdTickCount = 0;

		//
		// <Roger_Notes> We evaluate RxOk counts for each antenna first and than 
		// evaluate signal strength. 
		// The following operation can overcome the disability of CCA on both two antennas
		// When signal strength was extremely low or high.
		// 2008.01.30.
		// 
		
		//
		// Evaluate RxOk count from each antenna if we shall switch default antenna now.
		// Added by Roger, 2008.02.21.
                
                //{by amy 080312
		if((priv->AdMainAntennaRxOkCnt < priv->AdAuxAntennaRxOkCnt) && (priv->CurrAntennaIndex == 0)){ 
			// We set Main antenna as default but RxOk count was less than Aux ones.

			printk("SwAntennaDiversity(): Main antenna %d RxOK is poor, AdMainAntennaRxOkCnt: %d, AdAuxAntennaRxOkCnt: %d\n",priv->CurrAntennaIndex, priv->AdMainAntennaRxOkCnt, priv->AdAuxAntennaRxOkCnt);
			
			// Switch to Aux antenna.
			SwitchAntenna(dev);	
			priv->bHWAdSwitched = true;
		}else if((priv->AdAuxAntennaRxOkCnt < priv->AdMainAntennaRxOkCnt) && (priv->CurrAntennaIndex == 1)){ 
			// We set Aux antenna as default but RxOk count was less than Main ones.

			printk("SwAntennaDiversity(): Aux antenna %d RxOK is poor, AdMainAntennaRxOkCnt: %d, AdAuxAntennaRxOkCnt: %d\n",priv->CurrAntennaIndex, priv->AdMainAntennaRxOkCnt, priv->AdAuxAntennaRxOkCnt);
			
			// Switch to Main antenna.
			SwitchAntenna(dev);
			priv->bHWAdSwitched = true;
		}else{// Default antenna is better.

			printk("SwAntennaDiversity(): Current Antenna %d is better., AdMainAntennaRxOkCnt: %d, AdAuxAntennaRxOkCnt: %d\n",priv->CurrAntennaIndex, priv->AdMainAntennaRxOkCnt, priv->AdAuxAntennaRxOkCnt);

			// Still need to check current signal strength.
			priv->bHWAdSwitched = false;	
		}
		//
		// <Roger_Notes> We evaluate Rx signal strength ONLY when default antenna 
		// didn't changed by HW evaluation. 
		// 2008.02.27.
		//
		// [TRC Dell Lab] SignalStrength is inaccuracy. Isaiah 2008-03-05 
		// For example, Throughput of aux is better than main antenna(about 10M v.s 2M), 
		// but AdRxSignalStrength is less than main. 
		// Our guess is that main antenna have lower throughput and get many change 
		// to receive more CCK packets(ex.Beacon) which have stronger SignalStrength.
		//
		if( (!priv->bHWAdSwitched) && (bSwCheckSS)){
                //by amy 080312}

                // Evaluate Rx signal strength if we shall switch antenna now.
                if(priv->AdRxSignalStrength < priv->AdRxSsThreshold){ 
		    // Rx signal strength is weak => Switch Antenna.
                    printk("SwAntennaDiversity(): Rx Signal Strength is weak, CurrRxSs: %ld, RxSsThreshold: %ld\n", priv->AdRxSignalStrength, priv->AdRxSsThreshold);	

                    priv->AdRxSsBeforeSwitched = priv->AdRxSignalStrength; 
                    priv->bAdSwitchedChecking = true;

                    SwitchAntenna(dev);
                }else{ // Rx signal strength is OK. 
			printk("SwAntennaDiversity(): Rx Signal Strength is OK, CurrRxSs: %ld, RxSsThreshold: %ld\n", priv->AdRxSignalStrength, priv->AdRxSsThreshold);

			priv->bAdSwitchedChecking = false;
			// Increase Rx signal strength threashold if necessary.
			if(	(priv->AdRxSignalStrength > (priv->AdRxSsThreshold + 10)) && // Signal is much stronger than current threshold
				priv->AdRxSsThreshold <= priv->AdMaxRxSsThreshold) // Current threhold is not yet reach upper limit.
			{
				priv->AdRxSsThreshold = (priv->AdRxSsThreshold + priv->AdRxSignalStrength) / 2;
				priv->AdRxSsThreshold = (priv->AdRxSsThreshold > priv->AdMaxRxSsThreshold) ?
							priv->AdMaxRxSsThreshold: priv->AdRxSsThreshold;//+by amy 080312
			}

			// Reduce Antenna Diversity checking period if possible. 
			if( priv->AdCheckPeriod > priv->AdMinCheckPeriod )
			{
				priv->AdCheckPeriod /= 2; 
			}
		}
		}
	}
//by amy 080312
	// Reset antenna diversity Rx related statistics.
	priv->AdRxOkCnt = 0;
	priv->AdMainAntennaRxOkCnt = 0;
	priv->AdAuxAntennaRxOkCnt = 0;
//by amy 080312

//	priv->AdRxOkCnt = 0;//-by amy 080312

	//printk("-SwAntennaDiversity()\n");
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
void SwAntennaWorkItemCallback(struct work_struct *work)
{
	struct ieee80211_device *ieee = container_of(work, struct ieee80211_device, SwAntennaWorkItem.work);
	struct net_device *dev = ieee->dev;
#else
void SwAntennaWorkItemCallback(struct net_device *dev)
{
#endif
        //printk("==>%s \n", __func__);
	SwAntennaDiversity(dev);
}

//
//	Description: Timer callback function of SW Antenna Diversity.
//
void SwAntennaDiversityTimerCallback(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	RT_RF_POWER_STATE rtState;
	
	//printk("+SwAntennaDiversityTimerCallback()\n");

	//
	// We do NOT need to switch antenna while RF is off.
	// 2007.05.09, added by Roger.
	//
	rtState = priv->eRFPowerState;
	do{
		if (rtState == eRfOff){	
//			printk("SwAntennaDiversityTimer - RF is OFF.\n");
			break;
		}else if (rtState == eRfSleep){	
			// Don't access BB/RF under Disable PLL situation.
			//RT_TRACE((COMP_RF|COMP_ANTENNA), DBG_LOUD, ("SwAntennaDiversityTimerCallback(): RF is Sleep => skip it\n"));
			break;
		}  
		
	        queue_work(priv->ieee80211->wq,(void *)&priv->ieee80211->SwAntennaWorkItem);

	}while(false);

	if(priv->up){
		//priv->SwAntennaDiversityTimer.expires = jiffies + MSECS(ANTENNA_DIVERSITY_TIMER_PERIOD);
		//add_timer(&priv->SwAntennaDiversityTimer);
		mod_timer(&priv->SwAntennaDiversityTimer, jiffies + MSECS(ANTENNA_DIVERSITY_TIMER_PERIOD));
	}

}
#endif



