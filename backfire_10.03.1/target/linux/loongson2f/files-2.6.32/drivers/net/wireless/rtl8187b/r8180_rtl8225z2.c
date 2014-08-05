/*
  This is part of the rtl8180-sa2400 driver
  released under the GPL (See file COPYING for details).
  Copyright (c) 2005 Andrea Merello <andreamrl@tiscali.it>
  
  This files contains programming code for the rtl8225 
  radio frontend.
  
  *Many* thanks to Realtek Corp. for their great support!
  
*/



#include "r8180_hw.h"
#include "r8180_rtl8225.h"
#ifdef ENABLE_DOT11D
#include "dot11d.h"
#endif

//2005.11.16
u8 rtl8225z2_threshold[]={
        0x8d, 0x8d, 0x8d, 0x8d, 0x9d, 0xad, 0xbd,
};

//      0xd 0x19 0x1b 0x21
u8 rtl8225z2_gain_bg[]={
	0x23, 0x15, 0xa5, // -82-1dbm
        0x23, 0x15, 0xb5, // -82-2dbm
        0x23, 0x15, 0xc5, // -82-3dbm
        0x33, 0x15, 0xc5, // -78dbm
        0x43, 0x15, 0xc5, // -74dbm
        0x53, 0x15, 0xc5, // -70dbm
        0x63, 0x15, 0xc5, // -66dbm
};

u8 rtl8225z2_gain_a[]={
	0x13,0x27,0x5a,//,0x37,// -82dbm 
	0x23,0x23,0x58,//,0x37,// -82dbm 
	0x33,0x1f,0x56,//,0x37,// -82dbm 
	0x43,0x1b,0x54,//,0x37,// -78dbm 
	0x53,0x17,0x51,//,0x37,// -74dbm 
	0x63,0x24,0x4f,//,0x37,// -70dbm 
	0x73,0x0f,0x4c,//,0x37,// -66dbm 
};
static u32 MAC_REG_TABLE[][3]={ 
	{0xf0, 0x32, 0000}, {0xf1, 0x32, 0000}, {0xf2, 0x00, 0000}, {0xf3, 0x00, 0000}, 
	{0xf4, 0x32, 0000}, {0xf5, 0x43, 0000}, {0xf6, 0x00, 0000}, {0xf7, 0x00, 0000},
	{0xf8, 0x46, 0000}, {0xf9, 0xa4, 0000}, {0xfa, 0x00, 0000}, {0xfb, 0x00, 0000},
	{0xfc, 0x96, 0000}, {0xfd, 0xa4, 0000}, {0xfe, 0x00, 0000}, {0xff, 0x00, 0000}, 

	{0x58, 0x4b, 0001}, {0x59, 0x00, 0001}, {0x5a, 0x4b, 0001}, {0x5b, 0x00, 0001},
	{0x60, 0x4b, 0001}, {0x61, 0x09, 0001}, {0x62, 0x4b, 0001}, {0x63, 0x09, 0001},
	{0xce, 0x0f, 0001}, {0xcf, 0x00, 0001}, {0xe0, 0xff, 0001}, {0xe1, 0x0f, 0001},
	{0xe2, 0x00, 0001}, {0xf0, 0x4e, 0001}, {0xf1, 0x01, 0001}, {0xf2, 0x02, 0001},
	{0xf3, 0x03, 0001}, {0xf4, 0x04, 0001}, {0xf5, 0x05, 0001}, {0xf6, 0x06, 0001},
	{0xf7, 0x07, 0001}, {0xf8, 0x08, 0001}, 

	{0x4e, 0x00, 0002}, {0x0c, 0x04, 0002}, {0x21, 0x61, 0002}, {0x22, 0x68, 0002}, 
	{0x23, 0x6f, 0002}, {0x24, 0x76, 0002}, {0x25, 0x7d, 0002}, {0x26, 0x84, 0002}, 
	{0x27, 0x8d, 0002}, {0x4d, 0x08, 0002}, {0x50, 0x05, 0002}, {0x51, 0xf5, 0002}, 
	{0x52, 0x04, 0002}, {0x53, 0xa0, 0002}, {0x54, 0x1f, 0002}, {0x55, 0x23, 0002}, 
	{0x56, 0x45, 0002}, {0x57, 0x67, 0002}, {0x58, 0x08, 0002}, {0x59, 0x08, 0002}, 
	{0x5a, 0x08, 0002}, {0x5b, 0x08, 0002}, {0x60, 0x08, 0002}, {0x61, 0x08, 0002}, 
	{0x62, 0x08, 0002}, {0x63, 0x08, 0002}, {0x64, 0xcf, 0002}, {0x72, 0x56, 0002}, 
	{0x73, 0x9a, 0002},

	{0x34, 0xf0, 0000}, {0x35, 0x0f, 0000}, {0x5b, 0x40, 0000}, {0x84, 0x88, 0000},
	{0x85, 0x24, 0000}, {0x88, 0x54, 0000}, {0x8b, 0xb8, 0000}, {0x8c, 0x07, 0000},
	{0x8d, 0x00, 0000}, {0x94, 0x1b, 0000}, {0x95, 0x12, 0000}, {0x96, 0x00, 0000},
	{0x97, 0x06, 0000}, {0x9d, 0x1a, 0000}, {0x9f, 0x10, 0000}, {0xb4, 0x22, 0000},
	{0xbe, 0x80, 0000}, {0xdb, 0x00, 0000}, {0xee, 0x00, 0000}, {0x91, 0x01, 0000},
	//lzm mode 0x91 form 0x03->0x01 open GPIO BIT1, 
	//because Polling methord will rurn off Radio
	//the first time when read GPI(0x92).
	//because after 0x91:bit1 form 1->0, there will 
	//be time for 0x92:bit1 form 0->1

	{0x4c, 0x00, 0002}, {0x9f, 0x00, 0003}, {0x8c, 0x01, 0000}, {0x8d, 0x10, 0000},
	{0x8e, 0x08, 0000}, {0x8f, 0x00, 0000}
};

static u8  ZEBRA_AGC[]={
	0,
	0x5e,0x5e,0x5e,0x5e,0x5d,0x5b,0x59,0x57,0x55,0x53,0x51,0x4f,0x4d,0x4b,0x49,0x47,
	0x45,0x43,0x41,0x3f,0x3d,0x3b,0x39,0x37,0x35,0x33,0x31,0x2f,0x2d,0x2b,0x29,0x27,
	0x25,0x23,0x21,0x1f,0x1d,0x1b,0x19,0x17,0x15,0x13,0x11,0x0f,0x0d,0x0b,0x09,0x07,
	0x05,0x03,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
	0x19,0x19,0x19,0x019,0x19,0x19,0x19,0x19,0x19,0x20,0x21,0x22,0x23,0x24,0x25,0x26,
	0x26,0x27,0x27,0x28,0x28,0x29,0x2a,0x2a,0x2a,0x2b,0x2b,0x2b,0x2c,0x2c,0x2c,0x2d,
	0x2d,0x2d,0x2d,0x2e,0x2e,0x2e,0x2e,0x2f,0x2f,0x2f,0x30,0x30,0x31,0x31,0x31,0x31,
	0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31
};

static u32 ZEBRA_RF_RX_GAIN_TABLE[]={	
	0,
	0x0400,0x0401,0x0402,0x0403,0x0404,0x0405,0x0408,0x0409,
	0x040a,0x040b,0x0502,0x0503,0x0504,0x0505,0x0540,0x0541,
	0x0542,0x0543,0x0544,0x0545,0x0580,0x0581,0x0582,0x0583,
	0x0584,0x0585,0x0588,0x0589,0x058a,0x058b,0x0643,0x0644,
	0x0645,0x0680,0x0681,0x0682,0x0683,0x0684,0x0685,0x0688,
	0x0689,0x068a,0x068b,0x068c,0x0742,0x0743,0x0744,0x0745,
	0x0780,0x0781,0x0782,0x0783,0x0784,0x0785,0x0788,0x0789,
	0x078a,0x078b,0x078c,0x078d,0x0790,0x0791,0x0792,0x0793,
	0x0794,0x0795,0x0798,0x0799,0x079a,0x079b,0x079c,0x079d,  
	0x07a0,0x07a1,0x07a2,0x07a3,0x07a4,0x07a5,0x07a8,0x07a9,  
	0x03aa,0x03ab,0x03ac,0x03ad,0x03b0,0x03b1,0x03b2,0x03b3,
	0x03b4,0x03b5,0x03b8,0x03b9,0x03ba,0x03bb,0x03bb
};

// Use the new SD3 given param, by shien chang, 2006.07.14

static u8 OFDM_CONFIG[]={
			// 0x00
	0x10, 0x0d, 0x01, 0x00, 0x14, 0xfb, 0xfb, 0x60, 
	0x00, 0x60, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x00, 
			
			// 0x10
	0x40, 0x00, 0x40, 0x00, 0x00, 0x00, 0xa8, 0x26, 
	0x32, 0x33, 0x07, 0xa5, 0x6f, 0x55, 0xc8, 0xb3, 
			
			// 0x20
	0x0a, 0xe1, 0x2C, 0x8a, 0x86, 0x83, 0x34, 0x0f, 
	0x4f, 0x24, 0x6f, 0xc2, 0x6b, 0x40, 0x80, 0x00, 
			
			// 0x30
	0xc0, 0xc1, 0x58, 0xf1, 0x00, 0xe4, 0x90, 0x3e, 
	0x6d, 0x3c, 0xfb, 0x07//0xc7
		};

//2005.11.16,
u8 ZEBRA2_CCK_OFDM_GAIN_SETTING[]={
        0x00,0x01,0x02,0x03,0x04,0x05,
        0x06,0x07,0x08,0x09,0x0a,0x0b,
        0x0c,0x0d,0x0e,0x0f,0x10,0x11,
        0x12,0x13,0x14,0x15,0x16,0x17,
        0x18,0x19,0x1a,0x1b,0x1c,0x1d,
        0x1e,0x1f,0x20,0x21,0x22,0x23,
};
//-
u16 rtl8225z2_rxgain[]={	
	0x0400, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405, 0x0408, 0x0409,
	0x040a, 0x040b, 0x0502, 0x0503, 0x0504, 0x0505, 0x0540, 0x0541,  
	0x0542, 0x0543, 0x0544, 0x0545, 0x0580, 0x0581, 0x0582, 0x0583,
	0x0584, 0x0585, 0x0588, 0x0589, 0x058a, 0x058b, 0x0643, 0x0644, 
	0x0645, 0x0680, 0x0681, 0x0682, 0x0683, 0x0684, 0x0685, 0x0688,
	0x0689, 0x068a, 0x068b, 0x068c, 0x0742, 0x0743, 0x0744, 0x0745,
	0x0780, 0x0781, 0x0782, 0x0783, 0x0784, 0x0785, 0x0788, 0x0789,
	0x078a, 0x078b, 0x078c, 0x078d, 0x0790, 0x0791, 0x0792, 0x0793,
	0x0794, 0x0795, 0x0798, 0x0799, 0x079a, 0x079b, 0x079c, 0x079d,  
	0x07a0, 0x07a1, 0x07a2, 0x07a3, 0x07a4, 0x07a5, 0x07a8, 0x07a9,  
	0x03aa, 0x03ab, 0x03ac, 0x03ad, 0x03b0, 0x03b1, 0x03b2, 0x03b3,  
	0x03b4, 0x03b5, 0x03b8, 0x03b9, 0x03ba, 0x03bb, 0x03bb

};


/*
 from 0 to 0x23
u8 rtl8225_tx_gain_cck_ofdm[]={
	0x02,0x06,0x0e,0x1e,0x3e,0x7e
};
*/

//-
u8 rtl8225z2_tx_power_ofdm[]={
	0x42,0x00,0x40,0x00,0x40
};


//-
u8 rtl8225z2_tx_power_cck_ch14[]={
	0x36,0x35,0x2e,0x1b,0x00,0x00,0x00,0x00,
	0x30, 0x2f, 0x29, 0x15, 0x00, 0x00, 0x00, 0x00,
	0x30, 0x2f, 0x29, 0x15, 0x00, 0x00, 0x00, 0x00,
	0x30, 0x2f, 0x29, 0x15, 0x00, 0x00, 0x00, 0x00,
};


//-
u8 rtl8225z2_tx_power_cck[]={
	0x36,0x35,0x2e,0x25,0x1c,0x12,0x09,0x04,
	0x30, 0x2f, 0x29, 0x21, 0x19, 0x10, 0x08, 0x03,
	0x2b, 0x2a, 0x25, 0x1e, 0x16, 0x0e, 0x07, 0x03,
	0x26, 0x25, 0x21, 0x1b, 0x14, 0x0d, 0x06, 0x03
};

#ifdef ENABLE_DOT11D
//
//	Description:
//		Map dBm into Tx power index according to 
//		current HW model, for example, RF and PA, and
//		current wireless mode.
//
s8
rtl8187B_DbmToTxPwrIdx(
	struct r8180_priv *priv,
	WIRELESS_MODE	WirelessMode,
	s32			PowerInDbm
	)
{
 	bool bUseDefault = true; 
	s8 TxPwrIdx = 0;

#ifdef CONFIG_RTL818X_S
	//
	// 071011, SD3 SY:
	// OFDM Power in dBm = Index * 0.5 + 0 
	// CCK Power in dBm = Index * 0.25 + 13
	//
	if(priv->card_8185 >= VERSION_8187S_B)
	{
		s32 tmp = 0;

		if(WirelessMode == WIRELESS_MODE_G)
		{
			bUseDefault = false;
			tmp = (2 * PowerInDbm);

			if(tmp < 0) 
				TxPwrIdx = 0; 
			else if(tmp > 40) // 40 means 20 dBm.
				TxPwrIdx = 40;
			else 
				TxPwrIdx = (s8)tmp;
		}
		else if(WirelessMode == WIRELESS_MODE_B)
		{
			bUseDefault = false;
			tmp = (4 * PowerInDbm) - 52;

			if(tmp < 0) 
				TxPwrIdx = 0; 
			else if(tmp > 28) // 28 means 20 dBm.
				TxPwrIdx = 28;
			else 
				TxPwrIdx = (s8)tmp;
		}
	}
#endif

	//	
	// TRUE if we want to use a default implementation.
	// We shall set it to FALSE when we have exact translation formular 
	// for target IC. 070622, by rcnjko.
	//
	if(bUseDefault)
	{
		if(PowerInDbm < 0)
			TxPwrIdx = 0;
		else if(PowerInDbm > 35)
			TxPwrIdx = 35;
		else
			TxPwrIdx = (u8)PowerInDbm;
	}

	return TxPwrIdx;
}
#endif


void rtl8225z2_set_gain(struct net_device *dev, short gain)
{
	u8* rtl8225_gain;
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	u8 mode = priv->ieee80211->mode;
	
	if(mode == IEEE_B || mode == IEEE_G)
		rtl8225_gain = rtl8225z2_gain_bg;
	else
		rtl8225_gain = rtl8225z2_gain_a;
		
	//write_phy_ofdm(dev, 0x0d, rtl8225_gain[gain * 3]);
	//write_phy_ofdm(dev, 0x19, rtl8225_gain[gain * 3 + 1]);
	//write_phy_ofdm(dev, 0x1b, rtl8225_gain[gain * 3 + 2]);
        //2005.11.17, by ch-hsu
        write_phy_ofdm(dev, 0x0b, rtl8225_gain[gain * 3]);
        write_phy_ofdm(dev, 0x1b, rtl8225_gain[gain * 3 + 1]);
        write_phy_ofdm(dev, 0x1d, rtl8225_gain[gain * 3 + 2]);
	write_phy_ofdm(dev, 0x21, 0x37);

}

u32 read_rtl8225(struct net_device *dev, u8 adr)
{
	u32 data2Write = ((u32)(adr & 0x1f)) << 27;
	u32 dataRead;
	u32 mask;
	u16 oval,oval2,oval3,tmp;
//	ThreeWireReg twreg;
//	ThreeWireReg tdata;
	int i;
	short bit, rw;
	
	u8 wLength = 6;
	u8 rLength = 12;
	u8 low2high = 0;

	oval = read_nic_word(dev, RFPinsOutput);
	oval2 = read_nic_word(dev, RFPinsEnable);
	oval3 = read_nic_word(dev, RFPinsSelect);
	write_nic_word(dev, RFPinsEnable, (oval2|0xf));
	write_nic_word(dev, RFPinsSelect, (oval3|0xf));

	dataRead = 0;

	oval &= ~0xf; 

	write_nic_word(dev, RFPinsOutput, oval | BB_HOST_BANG_EN ); udelay(4);

	write_nic_word(dev, RFPinsOutput, oval ); udelay(5);
	
	rw = 0;
	
	mask = (low2high) ? 0x01 : (((u32)0x01)<<(32-1));
	for(i = 0; i < wLength/2; i++)
	{
		bit = ((data2Write&mask) != 0) ? 1 : 0;
		write_nic_word(dev, RFPinsOutput, bit|oval | rw); udelay(1);
		
		write_nic_word(dev, RFPinsOutput, bit|oval | BB_HOST_BANG_CLK | rw); udelay(2);
		write_nic_word(dev, RFPinsOutput, bit|oval | BB_HOST_BANG_CLK | rw); udelay(2);

		mask = (low2high) ? (mask<<1): (mask>>1);

		if(i == 2)
		{
			rw = BB_HOST_BANG_RW;
			write_nic_word(dev, RFPinsOutput, bit|oval | BB_HOST_BANG_CLK | rw); udelay(2);
			write_nic_word(dev, RFPinsOutput, bit|oval | rw); udelay(2);
			break;
		}
		
		bit = ((data2Write&mask) != 0) ? 1: 0;
		
		write_nic_word(dev, RFPinsOutput, oval|bit|rw| BB_HOST_BANG_CLK); udelay(2);
		write_nic_word(dev, RFPinsOutput, oval|bit|rw| BB_HOST_BANG_CLK); udelay(2);

		write_nic_word(dev, RFPinsOutput, oval| bit |rw); udelay(1);

		mask = (low2high) ? (mask<<1) : (mask>>1);
	}

	//twreg.struc.clk = 0;
	//twreg.struc.data = 0;
	write_nic_word(dev, RFPinsOutput, rw|oval); udelay(2);
	mask = (low2high) ? 0x01 : (((u32)0x01) << (12-1));
	
	// We must set data pin to HW controled, otherwise RF can't driver it and 
	// value RF register won't be able to read back properly. 2006.06.13, by rcnjko.
	write_nic_word(dev, RFPinsEnable,((oval2|0xe) & (~0x01)));

	for(i = 0; i < rLength; i++)
	{
		write_nic_word(dev, RFPinsOutput, rw|oval); udelay(1);
		
		write_nic_word(dev, RFPinsOutput, rw|oval|BB_HOST_BANG_CLK); udelay(2);
		write_nic_word(dev, RFPinsOutput, rw|oval|BB_HOST_BANG_CLK); udelay(2);
		write_nic_word(dev, RFPinsOutput, rw|oval|BB_HOST_BANG_CLK); udelay(2);
		tmp = read_nic_word(dev, RFPinsInput);
		
		dataRead |= (tmp & BB_HOST_BANG_CLK ? mask : 0);

		write_nic_word(dev, RFPinsOutput, (rw|oval)); udelay(2);

		mask = (low2high) ? (mask<<1) : (mask>>1);
	}
	
	write_nic_word(dev, RFPinsOutput, BB_HOST_BANG_EN|BB_HOST_BANG_RW|oval); udelay(2);

	write_nic_word(dev, RFPinsEnable, oval2);   
	write_nic_word(dev, RFPinsSelect, oval3);   // Set To SW Switch
	write_nic_word(dev, RFPinsOutput, 0x3a0);

	return dataRead;
	
}
short rtl8225_is_V_z2(struct net_device *dev)
{
	short vz2 = 1;
	//set VCO-PDN pin
//	printk("%s()\n", __FUNCTION__);
	write_nic_word(dev, RFPinsOutput, 0x0080);
	write_nic_word(dev, RFPinsSelect, 0x0080);
	write_nic_word(dev, RFPinsEnable, 0x0080);

	//lzm mod for up take too long time 20081201
	//mdelay(100);
	//mdelay(1000);
	
	/* sw to reg pg 1 */
	write_rtl8225(dev, 0, 0x1b7);
	/* reg 8 pg 1 = 23*/
	if( read_rtl8225(dev, 8) != 0x588)
		vz2 = 0;
	
	else	/* reg 9 pg 1 = 24 */ 
		if( read_rtl8225(dev, 9) != 0x700)
			vz2 = 0;

	/* sw back to pg 0 */	
	write_rtl8225(dev, 0, 0xb7);

	return vz2;
	
}

void rtl8225z2_SetTXPowerLevel(struct net_device *dev, short ch)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	
//	int GainIdx;
//	int GainSetting;
	int i;
	u8 power;
	u8 *cck_power_table;
	u8 max_cck_power_level;
	u8 min_cck_power_level;
	u8 max_ofdm_power_level;
	u8 min_ofdm_power_level;	
	s8 cck_power_level = 0xff & priv->chtxpwr[ch];
	s8 ofdm_power_level = 0xff & priv->chtxpwr_ofdm[ch];
	u8 hw_version = priv->card_8187_Bversion;
	
#ifdef ENABLE_DOT11D
	if(IS_DOT11D_ENABLE(priv->ieee80211) &&
		IS_DOT11D_STATE_DONE(priv->ieee80211) )
	{
		//PRT_DOT11D_INFO pDot11dInfo = GET_DOT11D_INFO(priv->ieee80211);
		u8 MaxTxPwrInDbm = DOT11D_GetMaxTxPwrInDbm(priv->ieee80211, ch);
		u8 CckMaxPwrIdx = rtl8187B_DbmToTxPwrIdx(priv, WIRELESS_MODE_B, MaxTxPwrInDbm);
		u8 OfdmMaxPwrIdx = rtl8187B_DbmToTxPwrIdx(priv, WIRELESS_MODE_G, MaxTxPwrInDbm);

		//printk("Max Tx Power dBm (%d) => CCK Tx power index : %d, OFDM Tx power index: %d\n", MaxTxPwrInDbm, CckMaxPwrIdx, OfdmMaxPwrIdx);

		//printk("EEPROM channel(%d) => CCK Tx power index: %d, OFDM Tx power index: %d\n", 
		//	ch, cck_power_level, ofdm_power_level); 

		if(cck_power_level > CckMaxPwrIdx)
			cck_power_level = CckMaxPwrIdx;
		if(ofdm_power_level > OfdmMaxPwrIdx)
			ofdm_power_level = OfdmMaxPwrIdx;
	}

	//priv->CurrentCckTxPwrIdx = cck_power_level;
	//priv->CurrentOfdmTxPwrIdx = ofdm_power_level;
#endif	

	if (NIC_8187B == priv->card_8187)
	{
		if (hw_version == VERSION_8187B_B)
		{
			min_cck_power_level = 0;
			max_cck_power_level = 15;
			min_ofdm_power_level = 2;
			max_ofdm_power_level = 17;
		}else
		{	
			min_cck_power_level = 7;
			max_cck_power_level = 22;
			min_ofdm_power_level = 10;
			max_ofdm_power_level = 25;
		}

		if( priv->TrSwitchState == TR_SW_TX )	
		{
			//printk("SetTxPowerLevel8187(): Origianl OFDM Tx power level %d, adjust value = %d\n", ofdm_power_level,GetTxOfdmHighPowerBias(dev)); 
			ofdm_power_level -= GetTxOfdmHighPowerBias(dev);
			cck_power_level -= GetTxCckHighPowerBias(dev);
			//printk("SetTxPowerLevel8187(): Adjusted OFDM Tx power level %d for we are in High Power state\n", 
			//		ofdm_power_level); 
			//printk("SetTxPowerLevel8187(): Adjusted CCK Tx power level %d for we are in High Power state\n",
			//		cck_power_level); 
		}		
		/* CCK power setting */
		if(cck_power_level > (max_cck_power_level -min_cck_power_level))
			cck_power_level = max_cck_power_level;
		else
			cck_power_level += min_cck_power_level; 
		cck_power_level += priv->cck_txpwr_base;
		
		if(cck_power_level > 35)
			cck_power_level = 35;
		if(cck_power_level < 0)
			cck_power_level = 0;
			
		if(ch == 14) 
			cck_power_table = rtl8225z2_tx_power_cck_ch14;
		else 
			cck_power_table = rtl8225z2_tx_power_cck;
		if (hw_version == VERSION_8187B_B)
		{
			if (cck_power_level <= 6){
			}
			else if (cck_power_level <=11){
				cck_power_table += 8;
			}
			else{
				cck_power_table += (8*2);
			}
		}else{
			if (cck_power_level<=5){
			}else if(cck_power_level<=11){
				cck_power_table += 8;
			}else if(cck_power_level <= 17){
				cck_power_table += 8*2;
			}else{
				cck_power_table += 8*3;
			}
		}	
			
		
		
		for(i=0;i<8;i++){
		
			power = cck_power_table[i];
			write_phy_cck(dev, 0x44 + i, power);
		}
		
		//write_nic_byte(dev, TX_GAIN_CCK, power);
		//2005.11.17,
		write_nic_byte(dev, CCK_TXAGC, (ZEBRA2_CCK_OFDM_GAIN_SETTING[cck_power_level]*2));
		
//		force_pci_posting(dev);
//		msleep(1);
//in windows the delay was del from 85 to 87, 
//here we mod to sleep, or The CPU occupany is too hight. LZM 31/10/2008  
		
		/* OFDM power setting */
	//  Old:
	//	if(ofdm_power_level > max_ofdm_power_level)
	//		ofdm_power_level = 35;
	//	ofdm_power_level += min_ofdm_power_level;
	//  Latest:
		if(ofdm_power_level > (max_ofdm_power_level - min_ofdm_power_level))
			ofdm_power_level = max_ofdm_power_level;
		else
			ofdm_power_level += min_ofdm_power_level;
		
		ofdm_power_level += priv->ofdm_txpwr_base;
			
		if(ofdm_power_level > 35)
			ofdm_power_level = 35;

		if(ofdm_power_level < 0)
			ofdm_power_level = 0;
		write_nic_byte(dev, OFDM_TXAGC, ZEBRA2_CCK_OFDM_GAIN_SETTING[ofdm_power_level]*2);
		
		if (hw_version == VERSION_8187B_B)
		{
			if(ofdm_power_level<=11){
				write_phy_ofdm(dev, 0x87, 0x60);
				write_phy_ofdm(dev, 0x89, 0x60);
			}
			else{
				write_phy_ofdm(dev, 0x87, 0x5c);
				write_phy_ofdm(dev, 0x89, 0x5c);
			}
		}else{
			if(ofdm_power_level<=11){
				write_phy_ofdm(dev, 0x87, 0x5c);
				write_phy_ofdm(dev, 0x89, 0x5c);
			}
			if(ofdm_power_level<=17){
				write_phy_ofdm(dev, 0x87, 0x54);
				write_phy_ofdm(dev, 0x89, 0x54);
			}
			else{
				write_phy_ofdm(dev, 0x87, 0x50);
				write_phy_ofdm(dev, 0x89, 0x50);
			}
		}
//			force_pci_posting(dev);
//			msleep(1);
//in windows the delay was del from 85 to 87, 
//and here we mod to sleep, or The CPU occupany is too hight. LZM 31/10/2008  
	}else if(NIC_8187 == priv->card_8187) {
			min_cck_power_level = 0;
			max_cck_power_level = 15;
			min_ofdm_power_level = 10;
			max_ofdm_power_level = 25;
			if(cck_power_level > (max_cck_power_level -min_cck_power_level))
				cck_power_level = max_cck_power_level;
			else
				cck_power_level += min_cck_power_level; 
			cck_power_level += priv->cck_txpwr_base;
			
			if(cck_power_level > 35)
				cck_power_level = 35;
				
			if(ch == 14) 
				cck_power_table = rtl8225z2_tx_power_cck_ch14;
			else 
				cck_power_table = rtl8225z2_tx_power_cck;
			for(i=0;i<8;i++){
				power = cck_power_table[i];
				write_phy_cck(dev, 0x44 + i, power);
			}
			
			//write_nic_byte(dev, TX_GAIN_CCK, power);
			//2005.11.17,
			write_nic_byte(dev, CCK_TXAGC, ZEBRA2_CCK_OFDM_GAIN_SETTING[cck_power_level]);
			
//			force_pci_posting(dev);
//			msleep(1);
//in windows the delay was del from 85 to 87, 
//and here we mod to sleep, or The CPU occupany is too hight. LZM 31/10/2008  
			if(ofdm_power_level > (max_ofdm_power_level - min_ofdm_power_level))
				ofdm_power_level = max_ofdm_power_level;
			else
				ofdm_power_level += min_ofdm_power_level;
			
			ofdm_power_level += priv->ofdm_txpwr_base;
				
			if(ofdm_power_level > 35)
				ofdm_power_level = 35;
			write_nic_byte(dev, OFDM_TXAGC, ZEBRA2_CCK_OFDM_GAIN_SETTING[ofdm_power_level]);

			rtl8185_set_anaparam2(dev,RTL8225_ANAPARAM2_ON);

			write_phy_ofdm(dev,2,0x42);
			write_phy_ofdm(dev,5,0);
			write_phy_ofdm(dev,6,0x40);
			write_phy_ofdm(dev,7,0);
			write_phy_ofdm(dev,8,0x40);	
		}

}

void rtl8225z2_rf_set_chan(struct net_device *dev, short ch)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	short gset = (priv->ieee80211->state == IEEE80211_LINKED &&
			ieee80211_is_54g(priv->ieee80211->current_network)) ||
		priv->ieee80211->iw_mode == IW_MODE_MONITOR;
	int eifs_addr;

	down(&priv->set_chan_sem);

	if(NIC_8187 == priv->card_8187) {
		eifs_addr = EIFS_8187;
	} else {
		eifs_addr = EIFS_8187B;
	}	

#ifdef ENABLE_DOT11D    
	if(!IsLegalChannel(priv->ieee80211, ch) )
	{
		printk("channel(%d). is invalide\n",  ch);
		up(&priv->set_chan_sem);
		return;
	}
#endif
        //87B not do it FIXME
	rtl8225z2_SetTXPowerLevel(dev, ch);
	
        //write_nic_byte(dev,0x7,(u8)rtl8225_chan[ch]);
        write_rtl8225(dev, 0x7, rtl8225_chan[ch]);

	force_pci_posting(dev);
	//mdelay(10);
//in windows the delay was del from 85 to 87, 
//and here we mod to sleep, or The CPU occupany is too hight. LZM 31/10/2008  
	if(NIC_8187 == priv->card_8187){	
		write_nic_byte(dev,SIFS,0x22);// SIFS: 0x22

		if(gset)
			write_nic_byte(dev,DIFS,20); //DIFS: 20 
		else
			write_nic_byte(dev,DIFS,0x24); //DIFS: 36 

		if(priv->ieee80211->state == IEEE80211_LINKED &&
				ieee80211_is_shortslot(priv->ieee80211->current_network))
			write_nic_byte(dev,SLOT,0x9); //SLOT: 9

		else
			write_nic_byte(dev,SLOT,0x14); //SLOT: 20 (0x14)


		if(gset){
			write_nic_byte(dev,eifs_addr,91 - 20); // EIFS: 91 (0x5B)
			write_nic_byte(dev,CW_VAL,0x73); //CW VALUE: 0x37
			//DMESG("using G net params");
		}else{
			write_nic_byte(dev,eifs_addr,91 - 0x24); // EIFS: 91 (0x5B)
			write_nic_byte(dev,CW_VAL,0xa5); //CW VALUE: 0x37
			//DMESG("using B net params");
		}
	}

	else {
#ifdef THOMAS_TURBO
		if(priv->ieee80211->current_network.Turbo_Enable && priv->ieee80211->iw_mode == IW_MODE_INFRA){
			write_nic_word(dev,AC_VO_PARAM,0x5114);
			write_nic_word(dev,AC_VI_PARAM,0x5114);
			write_nic_word(dev,AC_BE_PARAM,0x5114);
			write_nic_word(dev,AC_BK_PARAM,0x5114);
		} else {
			write_nic_word(dev,AC_VO_PARAM,0x731c);
			write_nic_word(dev,AC_VI_PARAM,0x731c);
			write_nic_word(dev,AC_BE_PARAM,0x731c);
			write_nic_word(dev,AC_BK_PARAM,0x731c);
		}
#endif
	}

	up(&priv->set_chan_sem);
}
void
MacConfig_87BASIC_HardCode(struct net_device *dev)
{
	//============================================================================
	// MACREG.TXT
	//============================================================================
	int	nLinesRead = 0;
	u32	u4bRegOffset, u4bRegValue, u4bPageIndex;
	int	i;

	nLinesRead=(sizeof(MAC_REG_TABLE)/3)/4;

	for(i = 0; i < nLinesRead; i++)
	{
		u4bRegOffset=MAC_REG_TABLE[i][0]; 
		u4bRegValue=MAC_REG_TABLE[i][1]; 
		u4bPageIndex=MAC_REG_TABLE[i][2]; 

		u4bRegOffset|= (u4bPageIndex << 8);

		write_nic_byte(dev, u4bRegOffset, (u8)u4bRegValue); 
	}
	//============================================================================
}

static void MacConfig_87BASIC(struct net_device *dev)
{
	MacConfig_87BASIC_HardCode(dev);

	//============================================================================

	// Follow TID_AC_MAP of WMac.
	//PlatformEFIOWrite2Byte(dev, TID_AC_MAP, 0xfa50);
	write_nic_word(dev, TID_AC_MAP, 0xfa50);

	// Interrupt Migration, Jong suggested we use set 0x0000 first, 2005.12.14, by rcnjko.
	write_nic_word(dev, INT_MIG, 0x0000);

	// Prevent TPC to cause CRC error. Added by Annie, 2006-06-10.
	write_nic_dword(dev, 0x1F0, 0x00000000);
	write_nic_dword(dev, 0x1F4, 0x00000000);
	write_nic_byte(dev, 0x1F8, 0x00);

	// For WiFi 5.2.2.5 Atheros AP performance. Added by Annie, 2006-06-12.
	// PlatformIOWrite4Byte(dev, RFTiming, 0x0008e00f);
	// Asked for by SD3 CM Lin, 2006.06.27, by rcnjko.
	write_nic_dword(dev, RFTiming, 0x00004001);

#ifdef TODO
	// Asked for by Victor, for 87B B-cut Rx FIFO overflow bug, 2006.06.27, by rcnjko.	
	if(dev->NdisUsbDev.CardInfo.USBIsHigh == FALSE)
	{
		PlatformEFIOWrite1Byte(dev, 0x24E, 0x01);
	}
#endif	
}


//
// Description:
//		Initialize RFE and read Zebra2 version code.
//
//	2005-08-01, by Annie.
//
void
SetupRFEInitialTiming(struct net_device*  dev)
{
	//u32		data8, data9;
        struct r8180_priv *priv = ieee80211_priv(dev);

	// setup initial timing for RFE
	// Set VCO-PDN pin.
	write_nic_word(dev, RFPinsOutput, 0x0480);
	write_nic_word(dev, RFPinsSelect, 0x2488);
	write_nic_word(dev, RFPinsEnable, 0x1FFF);
	
	mdelay(100);
	// Steven recommends: delay 1 sec for setting RF 1.8V. by Annie, 2005-04-28.
	mdelay(1000);

	//
	// TODO: Read Zebra version code if necessary.
	//
	priv->rf_chip = RF_ZEBRA2;
}


void ZEBRA_Config_87BASIC_HardCode(struct net_device* dev)
{
	u32			i;
	u32			addr,data;
	u32			u4bRegOffset, u4bRegValue;


	//=============================================================================
	// RADIOCFG.TXT
	//=============================================================================
	write_rtl8225(dev, 0x00, 0x00b7);			mdelay(1);
	write_rtl8225(dev, 0x01, 0x0ee0);			mdelay(1);
	write_rtl8225(dev, 0x02, 0x044d);			mdelay(1);
	write_rtl8225(dev, 0x03, 0x0441);			mdelay(1);
	write_rtl8225(dev, 0x04, 0x08c3);			mdelay(1);
	write_rtl8225(dev, 0x05, 0x0c72);			mdelay(1);
	write_rtl8225(dev, 0x06, 0x00e6);			mdelay(1);
	write_rtl8225(dev, 0x07, 0x082a);			mdelay(1);
	write_rtl8225(dev, 0x08, 0x003f);			mdelay(1);
	write_rtl8225(dev, 0x09, 0x0335);			mdelay(1);
	write_rtl8225(dev, 0x0a, 0x09d4);			mdelay(1);
	write_rtl8225(dev, 0x0b, 0x07bb);			mdelay(1);
	write_rtl8225(dev, 0x0c, 0x0850);			mdelay(1);
	write_rtl8225(dev, 0x0d, 0x0cdf);			mdelay(1);
	write_rtl8225(dev, 0x0e, 0x002b);			mdelay(1);
	write_rtl8225(dev, 0x0f, 0x0114);			mdelay(1);
	
	write_rtl8225(dev, 0x00, 0x01b7);			mdelay(1);


	for(i=1;i<=95;i++)
	{
		write_rtl8225(dev, 0x01, i);mdelay(1); 
		write_rtl8225(dev, 0x02, ZEBRA_RF_RX_GAIN_TABLE[i]); mdelay(1);
		//DbgPrint("RF - 0x%x =  0x%x\n", i, ZEBRA_RF_RX_GAIN_TABLE[i]);
	}

	write_rtl8225(dev, 0x03, 0x0080);			mdelay(1); 	// write reg 18
	write_rtl8225(dev, 0x05, 0x0004);			mdelay(1);	// write reg 20
	write_rtl8225(dev, 0x00, 0x00b7);			mdelay(1);	// switch to reg0-reg15
	//lzm mod for up take too long time 20081201
#ifdef THOMAS_BEACON
        msleep(1000);// Deay 1 sec. //0xfd
        //msleep(1000);// Deay 1 sec. //0xfd
        //msleep(1000);// Deay 1 sec. //0xfd
        msleep(400);// Deay 1 sec. //0xfd
#else

	mdelay(1000);	
	//mdelay(1000);
	//mdelay(1000);
	mdelay(400);
#endif
	write_rtl8225(dev, 0x02, 0x0c4d);			mdelay(1);
	//lzm mod for up take too long time 20081201
	//mdelay(1000);
	//mdelay(1000);
	msleep(100);// Deay 100 ms. //0xfe
	msleep(100);// Deay 100 ms. //0xfe
	write_rtl8225(dev, 0x02, 0x044d);			mdelay(1); 	
	write_rtl8225(dev, 0x00, 0x02bf);			mdelay(1);	//0x002f disable 6us corner change,  06f--> enable
	
	//=============================================================================
	
	//=============================================================================
	// CCKCONF.TXT
	//=============================================================================
	/*
	u4bRegOffset=0x41;
	u4bRegValue=0xc8;
	
	//DbgPrint("\nCCK- 0x%x = 0x%x\n", u4bRegOffset, u4bRegValue);
	WriteBB(dev, (0x01000080 | (u4bRegOffset & 0x7f) | ((u4bRegValue & 0xff) << 8)));
	*/

	
	//=============================================================================

	//=============================================================================
	// Follow WMAC RTL8225_Config()
	//=============================================================================
//	//
//	// enable EEM0 and EEM1 in 9346CR
//	PlatformEFIOWrite1Byte(dev, CR9346, PlatformEFIORead1Byte(dev, CR9346)|0xc0);
//	// enable PARM_En in Config3
//	PlatformEFIOWrite1Byte(dev, CONFIG3, PlatformEFIORead1Byte(dev, CONFIG3)|0x40);	
//
//	PlatformEFIOWrite4Byte(dev, AnaParm2, ANAPARM2_ASIC_ON); //0x727f3f52
//	PlatformEFIOWrite4Byte(dev, AnaParm, ANAPARM_ASIC_ON); //0x45090658

	// power control
	write_nic_byte(dev, CCK_TXAGC, 0x03);
	write_nic_byte(dev, OFDM_TXAGC, 0x07);
	write_nic_byte(dev, ANTSEL, 0x03);

//	// disable PARM_En in Config3
//	PlatformEFIOWrite1Byte(dev, CONFIG3, PlatformEFIORead1Byte(dev, CONFIG3)&0xbf);
//	// disable EEM0 and EEM1 in 9346CR
//	PlatformEFIOWrite1Byte(dev, CR9346, PlatformEFIORead1Byte(dev, CR9346)&0x3f);
	//=============================================================================

	//=============================================================================
	// AGC.txt
	//=============================================================================
	//write_nic_dword( dev, PhyAddr, 0x00001280);	// Annie, 2006-05-05
	//write_phy_ofdm( dev, 0x00, 0x12);	// David, 2006-08-01
	write_phy_ofdm( dev, 0x80, 0x12);	// David, 2006-08-09

	for (i=0; i<128; i++)
	{
		//DbgPrint("AGC - [%x+1] = 0x%x\n", i, ZEBRA_AGC[i+1]);
		
		data = ZEBRA_AGC[i+1];
		data = data << 8;
		data = data | 0x0000008F;

		addr = i + 0x80; //enable writing AGC table
		addr = addr << 8;
		addr = addr | 0x0000008E;

		write_phy_ofdm(dev,data&0x7f,(data>>8)&0xff);
		write_phy_ofdm(dev,addr&0x7f,(addr>>8)&0xff);
		write_phy_ofdm(dev,0x0E,0x00);
	}

	//write_nic_dword(dev, PhyAddr, 0x00001080);	// Annie, 2006-05-05
	//write_phy_ofdm( dev, 0x00, 0x10);	// David, 2006-08-01
	write_phy_ofdm( dev, 0x80, 0x10);	// David, 2006-08-09

	//=============================================================================
	
	//=============================================================================
	// OFDMCONF.TXT
	//=============================================================================

	for(i=0; i<60; i++)
	{
		u4bRegOffset=i;
		u4bRegValue=OFDM_CONFIG[i];
		//u4bRegValue=OFDM_CONFIG3m82[i];

	//	write_nic_dword(dev,PhyAddr,(0x00000080 | (u4bRegOffset & 0x7f) | ((u4bRegValue & 0xff) << 8)));
		write_phy_ofdm(dev,i,u4bRegValue);
	}


	//=============================================================================
}

void ZEBRA_Config_87BASIC(struct net_device *dev)
{
	ZEBRA_Config_87BASIC_HardCode(dev);
}
//by amy for DIG
//
//	Description:
//		Update initial gain into PHY.
//
void
UpdateCCKThreshold(
	struct net_device *dev
	)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	// Update CCK Power Detection(0x41) value.
	switch(priv->StageCCKTh)
	{
	case 0:
//		printk("Update CCK Stage 0: 88 \n");
		write_phy_cck(dev, 0xc1, 0x88);mdelay(1);
		break;
		
	case 1:
//		printk("Update CCK Stage 1: 98 \n");
		write_phy_cck(dev, 0xc1, 0x98);mdelay(1);
		break;

	case 2:
//		printk("Update CCK Stage 2: C8 \n");
		write_phy_cck(dev, 0xc1, 0xC8);mdelay(1);
		break;

	case 3:
//		printk("Update CCK Stage 3: D8 \n");
		write_phy_cck(dev, 0xc1, 0xD8);mdelay(1);
		break;

	default:
//		printk("Update CCK Stage %d ERROR!\n", pHalData->StageCCKTh);
		break;
	}
}
//
//	Description:
//		Update initial gain into PHY.
//
void
UpdateInitialGain(
	struct net_device *dev
	)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	//u8	   u1Tmp=0;

	//printk("UpdateInitialGain(): InitialGain: %d RFChipID: %d\n", priv->InitialGain, priv->rf_chip);

	switch(priv->rf_chip)
	{
		case RF_ZEBRA:
		case RF_ZEBRA2:

			//
			// Note: 
			// 	Whenever we update this gain table, we should be careful about those who call it.
			// 	Functions which call UpdateInitialGain as follows are important:
			// 	(1)StaRateAdaptive87B
			//	(2)DIG_Zebra
			//	(3)ActSetWirelessMode8187 (when the wireless mode is "B" mode, we set the 
			//		OFDM[0x17] = 0x26 to improve the Rx sensitivity).
			//	By Bruce, 2007-06-01.
			//

			//
			// SD3 C.M. Lin Initial Gain Table, by Bruce, 2007-06-01.
			//
			switch(priv->InitialGain)
			{
				case 1: //m861dBm
//					DMESG("RTL8187 + 8225 Initial Gain State 1: -82 dBm ");
					write_phy_ofdm(dev, 0x97, 0x26);	mdelay(1);
					write_phy_ofdm(dev, 0xa4, 0x86);	mdelay(1);
					write_phy_ofdm(dev, 0x85, 0xfa);	mdelay(1);
					break;
					
				case 2: //m862dBm
//					DMESG("RTL8187 + 8225 Initial Gain State 2: -78 dBm ");
					write_phy_ofdm(dev, 0x97, 0x36);	mdelay(1);// Revise 0x26 to 0x36, by Roger, 2007.05.03.
					write_phy_ofdm(dev, 0xa4, 0x86);	mdelay(1);
					write_phy_ofdm(dev, 0x85, 0xfa);	mdelay(1);
					break;

				case 3: //m863dBm
//					DMESG("RTL8187 + 8225 Initial Gain State 3: -78 dBm ");
					write_phy_ofdm(dev, 0x97, 0x36);	mdelay(1);// Revise 0x26 to 0x36, by Roger, 2007.05.03.
					write_phy_ofdm(dev, 0xa4, 0x86);	mdelay(1);
					write_phy_ofdm(dev, 0x85, 0xfb);	mdelay(1);
					break;

				case 4: //m864dBm
//					DMESG("RTL8187 + 8225 Initial Gain State 4: -74 dBm ");
					write_phy_ofdm(dev, 0x97, 0x46);	mdelay(1);// Revise 0x26 to 0x36, by Roger, 2007.05.03.
					write_phy_ofdm(dev, 0xa4, 0x86);	mdelay(1);
					write_phy_ofdm(dev, 0x85, 0xfb);	mdelay(1);
					break;

				case 5: //m82dBm
//					DMESG("RTL8187 + 8225 Initial Gain State 5: -74 dBm ");
					write_phy_ofdm(dev, 0x97, 0x46);	mdelay(1);
					write_phy_ofdm(dev, 0xa4, 0x96);	mdelay(1);
					write_phy_ofdm(dev, 0x85, 0xfb);	mdelay(1);
					break;

				case 6: //m78dBm
//					DMESG("RTL8187 + 8225 Initial Gain State 6: -70 dBm ");
					write_phy_ofdm(dev, 0x97, 0x56);	mdelay(1);					
					write_phy_ofdm(dev, 0xa4, 0x96);	mdelay(1);
					write_phy_ofdm(dev, 0x85, 0xfc);	mdelay(1);
					break;

				case 7: //m74dBm
//					DMESG("RTL8187 + 8225 Initial Gain State 7: -70 dBm ");
					write_phy_ofdm(dev, 0x97, 0x56);	mdelay(1);
					write_phy_ofdm(dev, 0xa4, 0xa6);	mdelay(1);
					write_phy_ofdm(dev, 0x85, 0xfc);	mdelay(1);
					break;

				// By Bruce, 2007-03-29.
				case 8:
					write_phy_ofdm(dev, 0x97, 0x66);	mdelay(1);
					write_phy_ofdm(dev, 0xa4, 0xb6);	mdelay(1);
					write_phy_ofdm(dev, 0x85, 0xfc);	mdelay(1);
					break;

				default:	//MP
//					DMESG("RTL8187 + 8225 Initial Gain State: -82 dBm (default), InitialGain(%d)", priv->InitialGain);
					write_phy_ofdm(dev, 0x97, 0x26);	mdelay(1);
					write_phy_ofdm(dev, 0xa4, 0x86);	mdelay(1);
					write_phy_ofdm(dev, 0x85, 0xfa);	mdelay(1);
					break;	
			}			
			break;

		default:
			break;
	}
}
//by amy for DIG
void PhyConfig8187(struct net_device *dev)
{
        struct r8180_priv *priv = ieee80211_priv(dev);
	u8			btConfig4;

	btConfig4 = read_nic_byte(dev, CONFIG4);
	priv->RFProgType = (btConfig4 & 0x03);



	switch(priv->rf_chip)
	{
	case  RF_ZEBRA2:
		ZEBRA_Config_87BASIC(dev);
		break;
	}
	if(priv->bDigMechanism)
	{
		if(priv->InitialGain == 0)
			priv->InitialGain = 4;
		//DMESG("DIG is enabled, set default initial gain index to %d", priv->InitialGain);
	}

	// By Bruce, 2007-03-29.
	UpdateCCKThreshold(dev);
	// Update initial gain after PhyConfig comleted, asked for by SD3 CMLin.
	UpdateInitialGain(dev);
	return ;
}

u8 GetSupportedWirelessMode8187(struct net_device* dev)
{
	u8 btSupportedWirelessMode;
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	btSupportedWirelessMode = 0;
	
	switch(priv->rf_chip)
	{
		case RF_ZEBRA:
		case RF_ZEBRA2:
			btSupportedWirelessMode = (WIRELESS_MODE_B | WIRELESS_MODE_G);
			break;
		default:
			btSupportedWirelessMode = WIRELESS_MODE_B;
			break;
	}
	return btSupportedWirelessMode;
}

void ActUpdateChannelAccessSetting(struct net_device *dev,
		int WirelessMode, 
		PCHANNEL_ACCESS_SETTING ChnlAccessSetting)
{
	AC_CODING	eACI;
	AC_PARAM	AcParam;
#ifdef TODO	
	PSTA_QOS	pStaQos = Adapter->MgntInfo.pStaQos;
#endif	
	//bool		bFollowLegacySetting = false;


	switch( WirelessMode )
	{
		case WIRELESS_MODE_A:
			ChnlAccessSetting->SIFS_Timer = 0x22;
			ChnlAccessSetting->DIFS_Timer = 34; // 34 = 16 + 2*9. 2006.06.07, by rcnjko.
			ChnlAccessSetting->SlotTimeTimer = 9;
			ChnlAccessSetting->EIFS_Timer = 23;
			ChnlAccessSetting->CWminIndex = 4;
			ChnlAccessSetting->CWmaxIndex = 10;
			break;

		case WIRELESS_MODE_B:
			ChnlAccessSetting->SIFS_Timer = 0x22;
			ChnlAccessSetting->DIFS_Timer = 50; // 50 = 10 + 2*20. 2006.06.07, by rcnjko.
			ChnlAccessSetting->SlotTimeTimer = 20;
			ChnlAccessSetting->EIFS_Timer = 91;
			ChnlAccessSetting->CWminIndex = 5;
			ChnlAccessSetting->CWmaxIndex = 10;
			break;

		case WIRELESS_MODE_G:
			//
			// <RJ_TODO_8185B> 
			// TODO: We still don't know how to set up these registers, just follow WMAC to 
			// verify 8185B FPAG.
			//
			// <RJ_TODO_8185B>
			// Jong said CWmin/CWmax register are not functional in 8185B, 
			// so we shall fill channel access realted register into AC parameter registers,
			// even in nQBss.
			//
			ChnlAccessSetting->SIFS_Timer = 0x22; // Suggested by Jong, 2005.12.08.
			ChnlAccessSetting->SlotTimeTimer = 9; // 2006.06.07, by rcnjko.
			ChnlAccessSetting->DIFS_Timer = 28; // 28 = 10 + 2*9. 2006.06.07, by rcnjko.
			ChnlAccessSetting->EIFS_Timer = 0x5B; // Suggested by wcchu, it is the default value of EIFS register, 2005.12.08.
#ifdef TODO
			switch (Adapter->NdisUsbDev.CWinMaxMin)
#else
			switch (2)				
#endif
			{
				case 0:// 0: [max:7 min:1 ]
					ChnlAccessSetting->CWminIndex = 1;
					ChnlAccessSetting->CWmaxIndex = 7;
					break;
				case 1:// 1: [max:7 min:2 ]
					ChnlAccessSetting->CWminIndex = 2;
					ChnlAccessSetting->CWmaxIndex = 7;
					break;
				case 2:// 2: [max:7 min:3 ]
					ChnlAccessSetting->CWminIndex = 3;
					ChnlAccessSetting->CWmaxIndex = 7;
					break;
				case 3:// 3: [max:9 min:1 ]
					ChnlAccessSetting->CWminIndex = 1;
					ChnlAccessSetting->CWmaxIndex = 9;
					break;
				case 4:// 4: [max:9 min:2 ]
					ChnlAccessSetting->CWminIndex = 2;
					ChnlAccessSetting->CWmaxIndex = 9;
					break;
				case 5:// 5: [max:9 min:3 ]
					ChnlAccessSetting->CWminIndex = 3;
					ChnlAccessSetting->CWmaxIndex = 9;
					break;
				case 6:// 6: [max:A min:5 ]
					ChnlAccessSetting->CWminIndex = 5;
					ChnlAccessSetting->CWmaxIndex = 10;
					break;
				case 7:// 7: [max:A min:4 ]
					ChnlAccessSetting->CWminIndex = 4;
					ChnlAccessSetting->CWmaxIndex = 10;
					break;

				default:
					ChnlAccessSetting->CWminIndex = 1;
					ChnlAccessSetting->CWmaxIndex = 7;
					break;
			}
#ifdef TODO
			if( Adapter->MgntInfo.OpMode == RT_OP_MODE_IBSS)
			{
				ChnlAccessSetting->CWminIndex= 4;
				ChnlAccessSetting->CWmaxIndex= 10;
			}
#endif
			break;
	}


	write_nic_byte(dev, SIFS, ChnlAccessSetting->SIFS_Timer);
//{ update slot time related by david, 2006-7-21
	write_nic_byte(dev, SLOT, ChnlAccessSetting->SlotTimeTimer);	// Rewrited from directly use PlatformEFIOWrite1Byte(), by Annie, 2006-03-29.
#ifdef TODO
	if(pStaQos->CurrentQosMode > QOS_DISABLE)
	{
		for(eACI = 0; eACI < AC_MAX; eACI++)
		{
		Adapter->HalFunc.SetHwRegHandler(Adapter, HW_VAR_AC_PARAM, \
				(pu1Byte)(&(pStaQos->WMMParamEle.AcParam[eACI])) );
		}
	}
	else
#endif		
	{
		u8 u1bAIFS = aSifsTime + (2 * ChnlAccessSetting->SlotTimeTimer );

		write_nic_byte(dev, AC_VO_PARAM, u1bAIFS);
		write_nic_byte(dev, AC_VI_PARAM, u1bAIFS);
		write_nic_byte(dev, AC_BE_PARAM, u1bAIFS);
		write_nic_byte(dev, AC_BK_PARAM, u1bAIFS);
	}
//}

	write_nic_byte(dev, EIFS_8187B, ChnlAccessSetting->EIFS_Timer);
	write_nic_byte(dev, AckTimeOutReg, 0x5B); // <RJ_EXPR_QOS> Suggested by wcchu, it is the default value of EIFS register, 2005.12.08.
#ifdef TODO
	// <RJ_TODO_NOW_8185B> Update ECWmin/ECWmax, AIFS, TXOP Limit of each AC to the value defined by SPEC.
	if( pStaQos->CurrentQosMode > QOS_DISABLE )
	{ // QoS mode.
		if(pStaQos->QBssWirelessMode == WirelessMode)
		{
			// Follow AC Parameters of the QBSS.
			for(eACI = 0; eACI < AC_MAX; eACI++)
			{
				Adapter->HalFunc.SetHwRegHandler(Adapter, HW_VAR_AC_PARAM, (pu1Byte)(&(pStaQos->WMMParamEle.AcParam[eACI])) );
			}
		}
		else
		{
			// Follow Default WMM AC Parameters.
			bFollowLegacySetting = TRUE;
		}
	}
	else
	{ // Legacy 802.11.
		bFollowLegacySetting = TRUE;
	}

	if(bFollowLegacySetting)
#endif		
	if(true)
	{
		//
		// Follow 802.11 seeting to AC parameter, all AC shall use the same parameter.
		// 2005.12.01, by rcnjko.
		//
		AcParam.longData = 0;
		AcParam.f.AciAifsn.f.AIFSN = 2; // Follow 802.11 DIFS.
		AcParam.f.AciAifsn.f.ACM = 0;
		AcParam.f.Ecw.f.ECWmin = ChnlAccessSetting->CWminIndex; // Follow 802.11 CWmin.
		AcParam.f.Ecw.f.ECWmax = ChnlAccessSetting->CWmaxIndex; // Follow 802.11 CWmax.
		AcParam.f.TXOPLimit = 0;
		for(eACI = 0; eACI < AC_MAX; eACI++)
		{
			AcParam.f.AciAifsn.f.ACI = (u8)eACI;
			{
				PAC_PARAM	pAcParam = (PAC_PARAM)(&AcParam);
				AC_CODING	eACI;
				u8		u1bAIFS;
				u32		u4bAcParam;

				// Retrive paramters to udpate.
				eACI = pAcParam->f.AciAifsn.f.ACI; 
				u1bAIFS = pAcParam->f.AciAifsn.f.AIFSN * ChnlAccessSetting->SlotTimeTimer + aSifsTime; 
				u4bAcParam = (	(((u32)(pAcParam->f.TXOPLimit)) << AC_PARAM_TXOP_LIMIT_OFFSET)	| 
						(((u32)(pAcParam->f.Ecw.f.ECWmax)) << AC_PARAM_ECW_MAX_OFFSET)	|  
						(((u32)(pAcParam->f.Ecw.f.ECWmin)) << AC_PARAM_ECW_MIN_OFFSET)	|  
						(((u32)u1bAIFS) << AC_PARAM_AIFS_OFFSET));

				switch(eACI)
				{
					case AC1_BK:
						write_nic_dword(dev, AC_BK_PARAM, u4bAcParam);
						break;

					case AC0_BE:
						write_nic_dword(dev, AC_BE_PARAM, u4bAcParam);
						break;

					case AC2_VI:
						write_nic_dword(dev, AC_VI_PARAM, u4bAcParam);
						break;

					case AC3_VO:
						write_nic_dword(dev, AC_VO_PARAM, u4bAcParam);
						break;

					default:
						printk(KERN_WARNING "SetHwReg8185(): invalid ACI: %d !\n", eACI);
						break;
				}

				// Cehck ACM bit.
				// If it is set, immediately set ACM control bit to downgrading AC for passing WMM testplan. Annie, 2005-12-13.
				//write_nic_byte(dev, ACM_CONTROL, pAcParam->f.AciAifsn);
				{
					PACI_AIFSN	pAciAifsn = (PACI_AIFSN)(&pAcParam->f.AciAifsn);
					AC_CODING	eACI = pAciAifsn->f.ACI;

					//modified Joseph
					//for 8187B AsynIORead issue
#ifdef TODO				
					u8	AcmCtrl = pHalData->AcmControl;
#else
					u8	AcmCtrl = 0;	
#endif
					if( pAciAifsn->f.ACM )
					{ // ACM bit is 1.
						switch(eACI)
						{
							case AC0_BE:
								AcmCtrl |= (BEQ_ACM_EN|BEQ_ACM_CTL|ACM_HW_EN);	// or 0x21
								break;

							case AC2_VI:
								AcmCtrl |= (VIQ_ACM_EN|VIQ_ACM_CTL|ACM_HW_EN);	// or 0x42
								break;

							case AC3_VO:
								AcmCtrl |= (VOQ_ACM_EN|VOQ_ACM_CTL|ACM_HW_EN);	// or 0x84
								break;

							default:
								printk(KERN_WARNING "SetHwReg8185(): [HW_VAR_ACM_CTRL] ACM set\
										failed: eACI is %d\n", eACI );
								break;
						}
					}
					else
					{ // ACM bit is 0.
						switch(eACI)
						{
							case AC0_BE:
								AcmCtrl &= ( (~BEQ_ACM_EN) & (~BEQ_ACM_CTL) & (~ACM_HW_EN) );	// and 0xDE
								break;

							case AC2_VI:
								AcmCtrl &= ( (~VIQ_ACM_EN) & (~VIQ_ACM_CTL) & (~ACM_HW_EN) );	// and 0xBD
								break;

							case AC3_VO:
								AcmCtrl &= ( (~VOQ_ACM_EN) & (~VOQ_ACM_CTL) & (~ACM_HW_EN) );	// and 0x7B
								break;

							default:
								break;
						}
					}

					//printk(KERN_WARNING "SetHwReg8185(): [HW_VAR_ACM_CTRL] Write 0x%X\n", AcmCtrl);

#ifdef TO_DO
					pHalData->AcmControl = AcmCtrl;
#endif				
					write_nic_byte(dev, ACM_CONTROL, AcmCtrl);
				}
			}
		}
	}
}

void ActSetWirelessMode8187(struct net_device* dev, u8	btWirelessMode)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee80211;
	//PMGNT_INFO		pMgntInfo = &(pAdapter->MgntInfo);
	u8			btSupportedWirelessMode = GetSupportedWirelessMode8187(dev);

	if( (btWirelessMode & btSupportedWirelessMode) == 0 )
	{ // Don't switch to unsupported wireless mode, 2006.02.15, by rcnjko.
		printk(KERN_WARNING "ActSetWirelessMode8187(): WirelessMode(%d) is not supported (%d)!\n", 
				 btWirelessMode, btSupportedWirelessMode);
		return;
	}

	// 1. Assign wireless mode to swtich if necessary.
	if( (btWirelessMode == WIRELESS_MODE_AUTO) || 
			(btWirelessMode & btSupportedWirelessMode) == 0 )
	{
		if((btSupportedWirelessMode & WIRELESS_MODE_A))
		{
			btWirelessMode = WIRELESS_MODE_A;
		}
		else if((btSupportedWirelessMode & WIRELESS_MODE_G))
		{
			btWirelessMode = WIRELESS_MODE_G;
		}
		else if((btSupportedWirelessMode & WIRELESS_MODE_B))
		{
			btWirelessMode = WIRELESS_MODE_B;
		}
		else
		{
			printk(KERN_WARNING "MptActSetWirelessMode8187(): No valid wireless mode supported, \
					btSupportedWirelessMode(%x)!!!\n", btSupportedWirelessMode);
			btWirelessMode = WIRELESS_MODE_B;
		}
	}

	// 2. Swtich band.
	switch(priv->rf_chip)
	{
		case RF_ZEBRA:
		case RF_ZEBRA2:
			{
				// Update current wireless mode if we swtich to specified band successfully.
				ieee->mode = (WIRELESS_MODE)btWirelessMode;
			}
			break;

		default:
			printk(KERN_WARNING "MptActSetWirelessMode8187(): unsupported RF: 0x%X !!!\n", priv->rf_chip);
			break;
	}

	// 4. Change related setting.
#if 0
	if( ieee->mode == WIRELESS_MODE_A ){
		DMESG("WIRELESS_MODE_A"); 
	}
	else if(ieee->mode == WIRELESS_MODE_B ){
		DMESG("WIRELESS_MODE_B"); 
	}
	else if( ieee->mode == WIRELESS_MODE_G ){
		DMESG("WIRELESS_MODE_G"); 
	}
#endif
	ActUpdateChannelAccessSetting(dev, ieee->mode, &priv->ChannelAccessSetting );
//by amy 0305
#ifdef TODO	
	if(ieee->mode == WIRELESS_MODE_B && priv->InitialGain > pHalData->RegBModeGainStage)
	{
		pHalData->InitialGain = pHalData->RegBModeGainStage;	// B mode, OFDM[0x17] = 26.
		RT_TRACE(COMP_INIT | COMP_DIG, DBG_LOUD, ("ActSetWirelessMode8187(): update init_gain to index %d for B mode\n",pHalData->InitialGain));
		PlatformScheduleWorkItem( &(pHalData->UpdateDigWorkItem) );
	}
//	pAdapter->MgntInfo.dot11CurrentWirelessMode = pHalData->CurrentWirelessMode;
//	MgntSetRegdot11OperationalRateSet( pAdapter );
#endif	
//by amy 0305
}


void
InitializeExtraRegsOn8185(struct net_device	*dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee80211;
	//RTL8185_TODO: Determine Retrylimit, TxAGC, AutoRateFallback control.
	bool		bUNIVERSAL_CONTROL_RL = false; // Enable per-packet tx retry, 2005.03.31, by rcnjko.
	bool		bUNIVERSAL_CONTROL_AGC = true;//false;
	bool		bUNIVERSAL_CONTROL_ANT = true;//false;
	bool		bAUTO_RATE_FALLBACK_CTL = true;
	u8		val8;

	// Set up ACK rate.
	// Suggested by wcchu, 2005.08.25, by rcnjko.
	// 1. Initialize (MinRR, MaxRR) to (6,24) for A/G.
	// 2. MUST Set RR before BRSR.
	// 3. CCK must be basic rate.
	if((ieee->mode == IEEE_G)||(ieee->mode == IEEE_A))
	{
		write_nic_word(dev, BRSR_8187B, 0x0fff);
	}
	else
	{
		write_nic_word(dev, BRSR_8187B, 0x000f);
	}


	// Retry limit
	val8 = read_nic_byte(dev, CW_CONF);
	if(bUNIVERSAL_CONTROL_RL)
	{
		val8 &= (~CW_CONF_PERPACKET_RETRY_LIMIT); 
	}
	else
	{
		val8 |= CW_CONF_PERPACKET_RETRY_LIMIT; 
	}

	write_nic_byte(dev, CW_CONF, val8);

	// Tx AGC
	val8 = read_nic_byte(dev, TX_AGC_CTL);
	if(bUNIVERSAL_CONTROL_AGC)
	{
		val8 &= (~TX_AGC_CTL_PER_PACKET_TXAGC);
		write_nic_byte(dev, CCK_TXAGC, 128);
		write_nic_byte(dev, OFDM_TXAGC, 128);
	}
	else
	{
		val8 |= TX_AGC_CTL_PER_PACKET_TXAGC;
	}
	write_nic_byte(dev, TX_AGC_CTL, val8);

	// Tx Antenna including Feedback control
	val8 = read_nic_byte(dev, TX_AGC_CTL);

	if(bUNIVERSAL_CONTROL_ANT)
	{
		write_nic_byte(dev, ANTSEL, 0x00);
		val8 &= (~TXAGC_CTL_PER_PACKET_ANT_SEL);
	}
	else
	{
		val8 |= TXAGC_CTL_PER_PACKET_ANT_SEL;
	}
	write_nic_byte(dev, TX_AGC_CTL, val8);

	// Auto Rate fallback control
	val8 = read_nic_byte(dev, RATE_FALLBACK);
	if( bAUTO_RATE_FALLBACK_CTL )
	{
		val8 |= RATE_FALLBACK_CTL_ENABLE | RATE_FALLBACK_CTL_AUTO_STEP0;
		
		// <RJ_TODO_8187B> We shall set up the ARFR according to user's setting.
                write_nic_word(dev, ARFR, 0x0fff); // set 1M ~ 54M
	}
	else
	{
		val8 &= (~RATE_FALLBACK_CTL_ENABLE);
	}
	write_nic_byte(dev, RATE_FALLBACK, val8);

}
///////////////////////////
void rtl8225z2_rf_init(struct net_device *dev) 
{

	struct r8180_priv *priv = ieee80211_priv(dev);
	if (NIC_8187B == priv->card_8187){	
		struct ieee80211_device *ieee = priv->ieee80211;
		u8			InitWirelessMode;
		u8			SupportedWirelessMode;
		bool			bInvalidWirelessMode = false;	
		InitializeExtraRegsOn8185(dev);	

		write_nic_byte(dev, MSR, read_nic_byte(dev,MSR) & 0xf3);	// default network type to 'No  Link'
		//{to avoid tx stall 
		write_nic_byte(dev, MSR, read_nic_byte(dev, MSR)|MSR_LINK_ENEDCA);//should always set ENDCA bit
		write_nic_byte(dev, ACM_CONTROL, priv->AcmControl);	

		write_nic_word(dev, BcnIntv, 100);
		write_nic_word(dev, AtimWnd, 2);
		write_nic_word(dev, FEMR, 0xFFFF);
		//LED TYPE
		{
			write_nic_byte(dev, CONFIG1,((read_nic_byte(dev, CONFIG1)&0x3f)|0x80));	//turn on bit 5:Clkrun_mode
		}
		write_nic_byte(dev, CR9346, 0x0);	// disable config register write

		//{ add some info here
		write_nic_dword(dev, MAC0, ((u32*)dev->dev_addr)[0]);
		write_nic_word(dev, MAC4, ((u32*)dev->dev_addr)[1] & 0xffff );

		write_nic_byte(dev, WPA_CONFIG, 0);	
		//}

		MacConfig_87BASIC(dev);

		// Override the RFSW_CTRL (MAC offset 0x272-0x273), 2006.06.07, by rcnjko.
		write_nic_word(dev, RFSW_CTRL, 0x569a);
#ifdef JOHN_TKIP
		{
			void CamResetAllEntry(struct net_device *dev);
			void EnableHWSecurityConfig8187(struct net_device *dev);
			CamResetAllEntry(dev);
			EnableHWSecurityConfig8187(dev);
			write_nic_word(dev, AESMSK_FC, AESMSK_FC_DEFAULT); mdelay(1);
			write_nic_word(dev, AESMSK_SC, AESMSK_SC_DEFAULT); mdelay(1);
			write_nic_word(dev, AESMSK_QC, AESMSK_QC_DEFAULT); mdelay(1);
		}
#endif
		//-----------------------------------------------------------------------------
		// Set up PHY related. 
		//-----------------------------------------------------------------------------
		// Enable Config3.PARAM_En to revise AnaaParm.
		write_nic_byte(dev, CR9346, 0xC0);
		write_nic_byte(dev, CONFIG3, read_nic_byte(dev,CONFIG3)|CONFIG3_PARM_En);
		write_nic_byte(dev, CR9346, 0x0);

		// Initialize RFE and read Zebra2 version code. Added by Annie, 2005-08-01.
		SetupRFEInitialTiming(dev);
		// PHY config.
		PhyConfig8187(dev);

		// We assume RegWirelessMode has already been initialized before, 
		// however, we has to validate the wireless mode here and provide a reasonble
		// initialized value if necessary. 2005.01.13, by rcnjko.
		SupportedWirelessMode = GetSupportedWirelessMode8187(dev);

		if((ieee->mode != WIRELESS_MODE_B) && 
				(ieee->mode != WIRELESS_MODE_G) &&
				(ieee->mode != WIRELESS_MODE_A) &&
				(ieee->mode != WIRELESS_MODE_AUTO)) 
		{ // It should be one of B, G, A, or AUTO. 
			bInvalidWirelessMode = true;
		}
		else
		{ // One of B, G, A, or AUTO.
			// Check if the wireless mode is supported by RF.
			if( (ieee->mode != WIRELESS_MODE_AUTO) &&
					(ieee->mode & SupportedWirelessMode) == 0 )
			{
				bInvalidWirelessMode = true;
			}
		}

		if(bInvalidWirelessMode || ieee->mode==WIRELESS_MODE_AUTO)
		{ // Auto or other invalid value.
			// Assigne a wireless mode to initialize.
			if((SupportedWirelessMode & WIRELESS_MODE_A))
			{
				InitWirelessMode = WIRELESS_MODE_A;
			}
			else if((SupportedWirelessMode & WIRELESS_MODE_G))
			{

				InitWirelessMode = WIRELESS_MODE_G;
			}
			else if((SupportedWirelessMode & WIRELESS_MODE_B))
			{

				InitWirelessMode = WIRELESS_MODE_B;
			}
			else
			{
				printk(KERN_WARNING 
						"InitializeAdapter8187(): No valid wireless mode supported, SupportedWirelessMode(%x)!!!\n", 
						SupportedWirelessMode);
				InitWirelessMode = WIRELESS_MODE_B;
			}

			// Initialize RegWirelessMode if it is not a valid one.
			if(bInvalidWirelessMode)
			{
				ieee->mode = (WIRELESS_MODE)InitWirelessMode;
			}
		}
		else
		{ // One of B, G, A.
			InitWirelessMode = ieee->mode; 
		}
		ActSetWirelessMode8187(dev, (u8)(InitWirelessMode));
		{//added for init gain 
			write_phy_ofdm(dev, 0x97, 0x46); mdelay(1);
			write_phy_ofdm(dev, 0xa4, 0xb6); mdelay(1);
			write_phy_ofdm(dev, 0x85, 0xfc); mdelay(1);
			write_phy_cck(dev, 0xc1, 0x88); mdelay(1);
		}
	
	}
	else{
		int i;
		short channel = 1;
		u16	brsr;
		u32	data,addr;
		
		priv->chan = channel;

		rtl8180_set_anaparam(dev, RTL8225_ANAPARAM_ON);

		if(priv->card_type == USB)
			rtl8225_host_usb_init(dev);
		else
			rtl8225_host_pci_init(dev);

		write_nic_dword(dev, RF_TIMING, 0x000a8008);
		
		brsr = read_nic_word(dev, BRSR_8187);
		
		write_nic_word(dev, BRSR_8187, 0xffff); 


		write_nic_dword(dev, RF_PARA, 0x100044);
		
		#if 1  //0->1
		rtl8180_set_mode(dev, EPROM_CMD_CONFIG);
		write_nic_byte(dev, CONFIG3, 0x44);
		rtl8180_set_mode(dev, EPROM_CMD_NORMAL);
		#endif
		
		
		rtl8185_rf_pins_enable(dev);

	//		mdelay(1000);

		write_rtl8225(dev, 0x0, 0x2bf); mdelay(1);
		
		
		write_rtl8225(dev, 0x1, 0xee0); mdelay(1);

		write_rtl8225(dev, 0x2, 0x44d); mdelay(1);

		write_rtl8225(dev, 0x3, 0x441); mdelay(1);

		
		write_rtl8225(dev, 0x4, 0x8c3);mdelay(1);
		
		
		
		write_rtl8225(dev, 0x5, 0xc72);mdelay(1);
	//	}
		
		write_rtl8225(dev, 0x6, 0xe6);  mdelay(1);

		write_rtl8225(dev, 0x7, ((priv->card_type == USB)? 0x82a : rtl8225_chan[channel]));  mdelay(1);

		write_rtl8225(dev, 0x8, 0x3f);  mdelay(1);

		write_rtl8225(dev, 0x9, 0x335);  mdelay(1);

		write_rtl8225(dev, 0xa, 0x9d4);  mdelay(1);

		write_rtl8225(dev, 0xb, 0x7bb);  mdelay(1);

		write_rtl8225(dev, 0xc, 0x850);  mdelay(1);


		write_rtl8225(dev, 0xd, 0xcdf);   mdelay(1);

		write_rtl8225(dev, 0xe, 0x2b);  mdelay(1);

		write_rtl8225(dev, 0xf, 0x114); 
		
		
		mdelay(100);
		
		
		//if(priv->card_type != USB) /* maybe not needed even for 8185 */
	//	write_rtl8225(dev, 0x7, rtl8225_chan[channel]); 
		
		write_rtl8225(dev, 0x0, 0x1b7);
		
		for(i=0;i<95;i++){
			write_rtl8225(dev, 0x1, (u8)(i+1));
			/* version B & C & D*/
			write_rtl8225(dev, 0x2, rtl8225z2_rxgain[i]);
		}
		//write_rtl8225(dev, 0x3, 0x80);
		write_rtl8225(dev, 0x3, 0x2);
		write_rtl8225(dev, 0x5, 0x4);

		write_rtl8225(dev, 0x0, 0xb7);

		write_rtl8225(dev, 0x2, 0xc4d);
		
		if(priv->card_type == USB){
		//	force_pci_posting(dev);
			mdelay(200);
			
			write_rtl8225(dev, 0x2, 0x44d);
			
		//	force_pci_posting(dev);
			mdelay(200);
			
		}//End of if(priv->card_type == USB)
		/* FIXME!! rtl8187 we have to check if calibrarion
		 * is successful and eventually cal. again (repeat
		 * the two write on reg 2)
		*/
		// Check for calibration status, 2005.11.17,
		data = read_rtl8225(dev, 6);
		if (!(data&0x00000080))
		{
			write_rtl8225(dev, 0x02, 0x0c4d);
			force_pci_posting(dev); mdelay(200);
			write_rtl8225(dev, 0x02, 0x044d);
			force_pci_posting(dev); mdelay(100);
			data = read_rtl8225(dev, 6);
			if (!(data&0x00000080))
				{
					DMESGW("RF Calibration Failed!!!!\n");
				}
		}
		//force_pci_posting(dev);
		
		mdelay(200); //200 for 8187 
		
		
	//	//if(priv->card_type != USB){
	//		write_rtl8225(dev, 0x2, 0x44d);
	//		write_rtl8225(dev, 0x7, rtl8225_chan[channel]);
	//		write_rtl8225(dev, 0x2, 0x47d);
	//		
	//		force_pci_posting(dev);
	//		mdelay(100);
	//		
	//		write_rtl8225(dev, 0x2, 0x44d);
	//	//}
		
		write_rtl8225(dev, 0x0, 0x2bf);   
		
		if(priv->card_type != USB)
			rtl8185_rf_pins_enable(dev);
		//set up ZEBRA AGC table, 2005.11.17,
		for(i=0;i<128;i++){
			data = rtl8225_agc[i];

			addr = i + 0x80; //enable writing AGC table
			write_phy_ofdm(dev, 0xb, data);

			mdelay(1);
			write_phy_ofdm(dev, 0xa, addr);

			mdelay(1);
		}
			
		force_pci_posting(dev);
		mdelay(1);
		
		write_phy_ofdm(dev, 0x0, 0x1); mdelay(1);
		write_phy_ofdm(dev, 0x1, 0x2); mdelay(1);
		write_phy_ofdm(dev, 0x2, ((priv->card_type == USB)? 0x42 : 0x62)); mdelay(1);
		write_phy_ofdm(dev, 0x3, 0x0); mdelay(1);
		write_phy_ofdm(dev, 0x4, 0x0); mdelay(1);
		write_phy_ofdm(dev, 0x5, 0x0); mdelay(1);
		write_phy_ofdm(dev, 0x6, 0x40); mdelay(1);
		write_phy_ofdm(dev, 0x7, 0x0); mdelay(1);
		write_phy_ofdm(dev, 0x8, 0x40); mdelay(1);
		write_phy_ofdm(dev, 0x9, 0xfe); mdelay(1);

		write_phy_ofdm(dev, 0xa, 0x8); mdelay(1);

		//write_phy_ofdm(dev, 0x18, 0xef); 
		//	}
		//}
		write_phy_ofdm(dev, 0xb, 0x80); mdelay(1);

		write_phy_ofdm(dev, 0xc, 0x1);mdelay(1);

		
		//if(priv->card_type != USB)
		write_phy_ofdm(dev, 0xd, 0x43); 
			
		write_phy_ofdm(dev, 0xe, 0xd3);mdelay(1);
		
		write_phy_ofdm(dev, 0xf, 0x38);mdelay(1);
	/*ver D & 8187*/
	//	}
		
	//	if(priv->card_8185 == 1 && priv->card_8185_Bversion)
	//		write_phy_ofdm(dev, 0x10, 0x04);/*ver B*/
	//	else
		write_phy_ofdm(dev, 0x10, 0x84);mdelay(1);
	/*ver C & D & 8187*/
		
		write_phy_ofdm(dev, 0x11, 0x07);mdelay(1);
	/*agc resp time 700*/

		
	//	if(priv->card_8185 == 2){
		/* Ver D & 8187*/
		write_phy_ofdm(dev, 0x12, 0x20);mdelay(1);

		write_phy_ofdm(dev, 0x13, 0x20);mdelay(1);

		write_phy_ofdm(dev, 0x14, 0x0); mdelay(1);
		write_phy_ofdm(dev, 0x15, 0x40); mdelay(1);
		write_phy_ofdm(dev, 0x16, 0x0); mdelay(1);
		write_phy_ofdm(dev, 0x17, 0x40); mdelay(1);
		
	//	if (priv->card_type == USB)
	//		write_phy_ofdm(dev, 0x18, 0xef);
		
		write_phy_ofdm(dev, 0x18, 0xef);mdelay(1);
	 

		write_phy_ofdm(dev, 0x19, 0x19); mdelay(1);
		write_phy_ofdm(dev, 0x1a, 0x20); mdelay(1);
		write_phy_ofdm(dev, 0x1b, 0x15);mdelay(1);
		
		write_phy_ofdm(dev, 0x1c, 0x4);mdelay(1);

		write_phy_ofdm(dev, 0x1d, 0xc5);mdelay(1); //2005.11.17,
		
		write_phy_ofdm(dev, 0x1e, 0x95);mdelay(1);

		write_phy_ofdm(dev, 0x1f, 0x75);	mdelay(1);

	//	}
		
		write_phy_ofdm(dev, 0x20, 0x1f);mdelay(1);

		write_phy_ofdm(dev, 0x21, 0x17);mdelay(1);
		
		write_phy_ofdm(dev, 0x22, 0x16);mdelay(1);

	//	if(priv->card_type != USB)
		write_phy_ofdm(dev, 0x23, 0x80);mdelay(1); //FIXME maybe not needed // <>
		
		write_phy_ofdm(dev, 0x24, 0x46); mdelay(1);
		write_phy_ofdm(dev, 0x25, 0x00); mdelay(1);
		write_phy_ofdm(dev, 0x26, 0x90); mdelay(1);

		write_phy_ofdm(dev, 0x27, 0x88); mdelay(1);

		
		// <> Set init. gain to m74dBm.
		
		rtl8225z2_set_gain(dev,4);
		//rtl8225z2_set_gain(dev,2);
		
		write_phy_cck(dev, 0x0, 0x98); mdelay(1);
		write_phy_cck(dev, 0x3, 0x20); mdelay(1);
		write_phy_cck(dev, 0x4, 0x7e); mdelay(1);
		write_phy_cck(dev, 0x5, 0x12); mdelay(1);
		write_phy_cck(dev, 0x6, 0xfc); mdelay(1);
		write_phy_cck(dev, 0x7, 0x78);mdelay(1);
	 /* Ver C & D & 8187*/
		write_phy_cck(dev, 0x8, 0x2e);mdelay(1);

		write_phy_cck(dev, 0x9, 0x11);mdelay(1);
		write_phy_cck(dev, 0xa, 0x17);mdelay(1);
		write_phy_cck(dev, 0xb, 0x11);mdelay(1);
		
		write_phy_cck(dev, 0x10, ((priv->card_type == USB) ? 0x9b: 0x93)); mdelay(1);
		write_phy_cck(dev, 0x11, 0x88); mdelay(1);
		write_phy_cck(dev, 0x12, 0x47); mdelay(1);
		write_phy_cck(dev, 0x13, 0xd0); /* Ver C & D & 8187*/
			
		write_phy_cck(dev, 0x19, 0x0); mdelay(1);
		write_phy_cck(dev, 0x1a, 0xa0); mdelay(1);
		write_phy_cck(dev, 0x1b, 0x8); mdelay(1);
		write_phy_cck(dev, 0x1d, 0x0); mdelay(1);
		
		write_phy_cck(dev, 0x40, 0x86); /* CCK Carrier Sense Threshold */ mdelay(1);
		
		write_phy_cck(dev, 0x41, 0x9d);mdelay(1);

		
		write_phy_cck(dev, 0x42, 0x15); mdelay(1);
		write_phy_cck(dev, 0x43, 0x18); mdelay(1);
		
		
		write_phy_cck(dev, 0x44, 0x36); mdelay(1);
		write_phy_cck(dev, 0x45, 0x35); mdelay(1);
		write_phy_cck(dev, 0x46, 0x2e); mdelay(1);
		write_phy_cck(dev, 0x47, 0x25); mdelay(1);
		write_phy_cck(dev, 0x48, 0x1c); mdelay(1);
		write_phy_cck(dev, 0x49, 0x12); mdelay(1);
		write_phy_cck(dev, 0x4a, 0x09); mdelay(1);
		write_phy_cck(dev, 0x4b, 0x04); mdelay(1);
		write_phy_cck(dev, 0x4c, 0x5);mdelay(1);


		write_nic_byte(dev, 0x5b, 0x0d); mdelay(1);

		

	// <>
	//	// TESTR 0xb 8187
	//	write_phy_cck(dev, 0x10, 0x93);// & 0xfb);
	//	
	//	//if(priv->card_type != USB){
	//		write_phy_ofdm(dev, 0x2, 0x62);
	//		write_phy_ofdm(dev, 0x6, 0x0);
	//		write_phy_ofdm(dev, 0x8, 0x0);
	//	//}
		
		rtl8225z2_SetTXPowerLevel(dev, channel);
		
		write_phy_cck(dev, 0x10, 0x9b); mdelay(1); /* Rx ant A, 0xdb for B */
		write_phy_ofdm(dev, 0x26, 0x90); mdelay(1); /* Rx ant A, 0x10 for B */
		
		rtl8185_tx_antenna(dev, 0x3); /* TX ant A, 0x0 for B */
		
		/* switch to high-speed 3-wire 
		 * last digit. 2 for both cck and ofdm
		 */
		if(priv->card_type == USB)
			write_nic_dword(dev, 0x94, 0x3dc00002);
		else{
			write_nic_dword(dev, 0x94, 0x15c00002);
			rtl8185_rf_pins_enable(dev);
		}

	//	if(priv->card_type != USB)
	//	rtl8225_set_gain(dev, 4); /* FIXME this '1' is random */ // <>
	//	 rtl8225_set_mode(dev, 1); /* FIXME start in B mode */ // <>
	//	
	//	/* make sure is waken up! */
	//	write_rtl8225(dev,0x4, 0x9ff);
	//	rtl8180_set_anaparam(dev, RTL8225_ANAPARAM_ON); 
	//	rtl8185_set_anaparam2(dev, RTL8225_ANAPARAM2_ON);
		
		rtl8225_rf_set_chan(dev, priv->chan);

		//write_nic_word(dev,BRSR,brsr);
		
		//rtl8225z2_rf_set_mode(dev);	
	}
}

void rtl8225z2_rf_set_mode(struct net_device *dev) 
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	if(priv->ieee80211->mode == IEEE_A)
	{
		write_rtl8225(dev, 0x5, 0x1865);
		write_nic_dword(dev, RF_PARA, 0x10084);
		write_nic_dword(dev, RF_TIMING, 0xa8008);
		write_phy_ofdm(dev, 0x0, 0x0);
		write_phy_ofdm(dev, 0xa, 0x6);
		write_phy_ofdm(dev, 0xb, 0x99);
		write_phy_ofdm(dev, 0xf, 0x20);
		write_phy_ofdm(dev, 0x11, 0x7);
		
		rtl8225z2_set_gain(dev,4);
		
		write_phy_ofdm(dev,0x15, 0x40);
		write_phy_ofdm(dev,0x17, 0x40);
	
		write_nic_dword(dev, 0x94,0x10000000);
	}else{
	
		write_rtl8225(dev, 0x5, 0x1864);
		write_nic_dword(dev, RF_PARA, 0x10044);
		write_nic_dword(dev, RF_TIMING, 0xa8008);
		write_phy_ofdm(dev, 0x0, 0x1);
		write_phy_ofdm(dev, 0xa, 0x6);
		write_phy_ofdm(dev, 0xb, 0x99);
		write_phy_ofdm(dev, 0xf, 0x20);
		write_phy_ofdm(dev, 0x11, 0x7);
		
		rtl8225z2_set_gain(dev,4);
		
		write_phy_ofdm(dev,0x15, 0x40);
		write_phy_ofdm(dev,0x17, 0x40);
	
		write_nic_dword(dev, 0x94,0x04000002);
	}
}
