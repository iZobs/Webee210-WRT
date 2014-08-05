/* 
   This is part of rtl8187 OpenSource driver.
   Copyright (C) Andrea Merello 2004-2005  <andreamrl@tiscali.it> 
   Released under the terms of GPL (General Public Licence)
   
   Parts of this driver are based on the GPL part of the 
   official realtek driver
   
   Parts of this driver are based on the rtl8180 driver skeleton 
   from Patric Schenke & Andres Salomon
   
   Parts of this driver are based on the Intel Pro Wireless 2100 GPL driver
   
   We want to tanks the Authors of those projects and the Ndiswrapper 
   project Authors.
*/

#ifndef R8180H
#define R8180H


#define RTL8187_MODULE_NAME "rtl8187"
#define DMESG(x,a...) printk(KERN_INFO RTL8187_MODULE_NAME ": " x "\n", ## a)
#define DMESGW(x,a...) printk(KERN_WARNING RTL8187_MODULE_NAME ": WW:" x "\n", ## a)
#define DMESGE(x,a...) printk(KERN_WARNING RTL8187_MODULE_NAME ": EE:" x "\n", ## a)

#include <linux/module.h>
#include <linux/kernel.h>
//#include <linux/config.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
//#include <linux/pci.h>
#include <linux/usb.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/rtnetlink.h>	//for rtnl_lock()
#include <linux/wireless.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>	// Necessary because we use the proc fs
#include <linux/if_arp.h>
#include <linux/random.h>
#include <linux/version.h>
#include <asm/io.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
#include <asm/semaphore.h>
#endif
#include "ieee80211/ieee80211.h"
#ifdef _RTL8187_EXT_PATCH_
#include "msh_class.h"
#endif
#ifdef LED
#include "r8187_led.h"
#endif

//added for HW security, john.0629
#define FALSE 0
#define TRUE 1
#define MAX_KEY_LEN     61
#define KEY_BUF_SIZE    5

#define BIT0    0x00000001
#define BIT1    0x00000002
#define BIT2    0x00000004
#define BIT3    0x00000008
#define BIT4    0x00000010
#define BIT5    0x00000020
#define BIT6    0x00000040
#define BIT7    0x00000080
#define BIT8    0x00000100
#define BIT9    0x00000200
#define BIT10   0x00000400
#define BIT11   0x00000800
#define BIT12   0x00001000
#define BIT13   0x00002000
#define BIT14   0x00004000
#define BIT15   0x00008000
#define BIT16   0x00010000
#define BIT17   0x00020000
#define BIT18   0x00040000
#define BIT19   0x00080000
#define BIT20   0x00100000
#define BIT21   0x00200000
#define BIT22   0x00400000
#define BIT23   0x00800000
#define BIT24   0x01000000
#define BIT25   0x02000000
#define BIT26   0x04000000
#define BIT27   0x08000000
#define BIT28   0x10000000
#define BIT29   0x20000000
#define BIT30   0x40000000
#define BIT31   0x80000000

//8187B Security
#define RWCAM                   0xA0                    // Software read/write CAM config
#define WCAMI                   0xA4                    // Software write CAM input content
#define RCAMO                   0xA8                    // Output value from CAM according to 0xa0 setting
#define DCAM                    0xAC                    // Debug CAM Interface
#define SECR                    0xB0                    // Security configuration register
#define AESMSK_FC               0xB2    		// AES Mask register for frame control (0xB2~0xB3). Added by Annie, 2006-03-06.
#define AESMSK_SC               0x1FC   		// AES Mask for Sequence Control (0x1FC~0X1FD). Added by Annie, 2006-03-06.
#define AESMSK_QC               0x1CE                   // AES Mask register for QoS Control when computing AES MIC, default = 0x000F. (2 bytes)

#define AESMSK_FC_DEFAULT                       0xC78F  // default value of AES MASK for Frame Control Field. (2 bytes)
#define AESMSK_SC_DEFAULT                       0x000F  // default value of AES MASK for Sequence Control Field. (2 bytes)
#define AESMSK_QC_DEFAULT                       0x000F  // default value of AES MASK for QoS Control Field. (2 bytes)

#define CAM_CONTENT_COUNT       6
#define CFG_DEFAULT_KEY         BIT5
#define CFG_VALID               BIT15

//----------------------------------------------------------------------------
//       8187B WPA Config Register (offset 0xb0, 1 byte)
//----------------------------------------------------------------------------
#define SCR_UseDK                       0x01
#define SCR_TxSecEnable                 0x02
#define SCR_RxSecEnable                 0x04

//----------------------------------------------------------------------------
//       8187B CAM Config Setting (offset 0xb0, 1 byte)
//----------------------------------------------------------------------------
#define CAM_VALID                               0x8000
#define CAM_NOTVALID                    0x0000
#define CAM_USEDK                               0x0020


#define CAM_NONE                                0x0
#define CAM_WEP40                               0x01
#define CAM_TKIP                                0x02
#define CAM_AES                                 0x04
#define CAM_WEP104                              0x05


//#define CAM_SIZE                              16
#define TOTAL_CAM_ENTRY         16
#define CAM_ENTRY_LEN_IN_DW     6                                                                       // 6, unit: in u4byte. Added by Annie, 2006-05-25.
#define CAM_ENTRY_LEN_IN_BYTE   (CAM_ENTRY_LEN_IN_DW*sizeof(u4Byte))    // 24, unit: in u1byte. Added by Annie, 2006-05-25.

#define CAM_CONFIG_USEDK                1
#define CAM_CONFIG_NO_USEDK             0

#define CAM_WRITE                               0x00010000
#define CAM_READ                                0x00000000
#define CAM_POLLINIG                    0x80000000

//=================================================================
//=================================================================

#define EPROM_93c46 0
#define EPROM_93c56 1

#define DEFAULT_FRAG_THRESHOLD 2342U
#define MIN_FRAG_THRESHOLD     256U
#define DEFAULT_BEACONINTERVAL 0x64U
#define DEFAULT_BEACON_ESSID "Rtl8187"

#define DEFAULT_SSID ""
#define DEFAULT_RETRY_RTS 7
#define DEFAULT_RETRY_DATA 7
#define PRISM_HDR_SIZE 64

typedef enum _WIRELESS_MODE {
	WIRELESS_MODE_UNKNOWN = 0x00,
	WIRELESS_MODE_A = 0x01,
	WIRELESS_MODE_B = 0x02,
	WIRELESS_MODE_G = 0x04,
	WIRELESS_MODE_AUTO = 0x08,
} WIRELESS_MODE;

typedef enum _TR_SWITCH_STATE{
	TR_HW_CONTROLLED = 0,
	TR_SW_TX = 1,
}TR_SWITCH_STATE, *PTR_SWITCH_STATE;


#define RTL_IOCTL_WPA_SUPPLICANT		SIOCIWFIRSTPRIV+30

typedef struct buffer
{
	struct buffer *next;
	u32 *buf;
	
} buffer;

typedef struct rtl_reg_debug{
        unsigned int  cmd;
        struct {
                unsigned char type;
                unsigned char addr;
                unsigned char page;
                unsigned char length;
        } head;
        unsigned char buf[0xff];
}rtl_reg_debug;
typedef struct _CHANNEL_LIST{
	u8	Channel[MAX_CHANNEL_NUMBER + 1];
	u8	Len;
}CHANNEL_LIST, *PCHANNEL_LIST;

#define MAX_LD_SLOT_NUM 					10
#define DEFAULT_SLOT_NUM					2
#define KEEP_ALIVE_INTERVAL 				20 // in seconds. 
#define CHECK_FOR_HANG_PERIOD			2 //be equal to watchdog check time
#define DEFAULT_KEEP_ALIVE_LEVEL			1

typedef struct _link_detect_t
{
	u32				RxFrameNum[MAX_LD_SLOT_NUM];	// number of Rx Frame / CheckForHang_period  to determine link status
	u16				SlotNum;	// number of CheckForHang period to determine link status, default is 2
	u16				SlotIndex;

	u32				NumTxOkInPeriod;  //number of packet transmitted during CheckForHang
	u32				NumRxOkInPeriod;  //number of packet received during CheckForHang

	u8				IdleCount;     // (KEEP_ALIVE_INTERVAL / CHECK_FOR_HANG_PERIOD)
	u32				LastNumTxUnicast;
	u32				LastNumRxUnicast;
		
	bool				bBusyTraffic;    //when it is set to 1, UI cann't scan at will.
}link_detect_t, *plink_detect_t;

#if 0

typedef struct tx_pendingbuf
{
	struct ieee80211_txb *txb;
	short ispending;
	short descfrag;
} tx_pendigbuf;

#endif

typedef struct Stats
{
	unsigned long txrdu;
//	unsigned long rxrdu;
	//unsigned long rxnolast;
	//unsigned long rxnodata;
//	unsigned long rxreset;
//	unsigned long rxwrkaround;
//	unsigned long rxnopointer;
	unsigned long rxok;
	unsigned long rxurberr;
	unsigned long rxstaterr;
	unsigned long txnperr;
	unsigned long txnpdrop;
	unsigned long txresumed;
//	unsigned long rxerr;
//	unsigned long rxoverflow;
//	unsigned long rxint;
	unsigned long txnpokint;
//	unsigned long txhpokint;
//	unsigned long txhperr;
//	unsigned long ints;
//	unsigned long shints;
	unsigned long txoverflow;
//	unsigned long rxdmafail;
//	unsigned long txbeacon;
//	unsigned long txbeaconerr;
	unsigned long txlpokint;
	unsigned long txlpdrop;
	unsigned long txlperr;
	unsigned long txbeokint;
	unsigned long txbedrop;
	unsigned long txbeerr;
	unsigned long txbkokint;
	unsigned long txbkdrop;
	unsigned long txbkerr;
	unsigned long txviokint;
	unsigned long txvidrop;
	unsigned long txvierr;
	unsigned long txvookint;
	unsigned long txvodrop;
	unsigned long txvoerr;
	unsigned long txbeaconokint;
	unsigned long txbeacondrop;
	unsigned long txbeaconerr;
	unsigned long txmanageokint;
	unsigned long txmanagedrop;
	unsigned long txmanageerr;
	unsigned long txdatapkt;
} Stats;

typedef struct 	ChnlAccessSetting {
	u16 SIFS_Timer;
	u16 DIFS_Timer; 
	u16 SlotTimeTimer;
	u16 EIFS_Timer;
	u16 CWminIndex;
	u16 CWmaxIndex;
}*PCHANNEL_ACCESS_SETTING,CHANNEL_ACCESS_SETTING;


typedef	enum _RT_RF_POWER_STATE
{
	eRfOn,
	eRfSleep,
	eRfOff
}RT_RF_POWER_STATE;
typedef	enum _RT_PS_MODE	
{
	eActive,	// Active/Continuous access.
	eMaxPs,		// Max power save mode.
	eFastPs		// Fast power save mode.
}RT_PS_MODE;
//
// Three wire mode.
//
#define IC_DEFAULT_THREE_WIRE	 0
#define SW_THREE_WIRE			 1
//RTL818xB
#define SW_THREE_WIRE_BY_8051	 2
#define HW_THREE_WIRE			 3
#define HW_THREE_WIRE_BY_8051 4
//lzm add for write time out test
typedef struct write_read_register
{
	u32 address;
	u32 content;
	u32  flag;
} write_read_register;
//lzm add for write time out test
typedef struct r8180_priv
{
//lzm add for write time out test
	struct write_read_register write_read_registers[200];
	u8 write_read_register_index;
//lzm add for write time out test

	struct usb_device *udev;
	short epromtype;
	int irq;
	struct ieee80211_device *ieee80211;
	
	short card_8187; /* O: rtl8180, 1:rtl8185 V B/C, 2:rtl8185 V D */
	short card_8187_Bversion; /* if TCR reports card V B/C this discriminates */
	short phy_ver; /* meaningful for rtl8225 1:A 2:B 3:C */
	short enable_gpio0;
	enum card_type {PCI,MINIPCI,CARDBUS,USB/*rtl8187*/}card_type;
	short hw_plcp_len;
	short plcp_preamble_mode;
		
	spinlock_t irq_lock;
//	spinlock_t irq_th_lock;
	spinlock_t tx_lock;
//by amy for ps
	spinlock_t rf_ps_lock;
//by amy for ps
	
	u16 irq_mask;
//	short irq_enabled;
	struct net_device *dev;
	short chan;
	short sens;
	short max_sens;
	u8 chtxpwr[15]; //channels from 1 to 14, 0 not used
	u8 chtxpwr_ofdm[15]; //channels from 1 to 14, 0 not used
	u8 cck_txpwr_base;
	u8 ofdm_txpwr_base;
	u8 challow[15]; //channels from 1 to 14, 0 not used
	short up;
	short crcmon; //if 1 allow bad crc frame reception in monitor mode
//	short prism_hdr;
	
//	struct timer_list scan_timer;
	/*short scanpending;
	short stopscan;*/
//	spinlock_t scan_lock;
//	u8 active_probe;
	//u8 active_scan_num;
	struct semaphore wx_sem;
	struct semaphore set_chan_sem;
//	short hw_wep;
		
//	short digphy;
//	short antb;
//	short diversity;
//	u8 cs_treshold;
//	short rcr_csense;
	short rf_chip;
//	u32 key0[4];
	short (*rf_set_sens)(struct net_device *dev,short sens);
	void (*rf_set_chan)(struct net_device *dev,short ch);
	void (*rf_close)(struct net_device *dev);
	void (*rf_init)(struct net_device *dev);
	//short rate;
	short promisc;	
	/*stats*/
	struct Stats stats;
	struct _link_detect_t link_detect;  //added on 1016.2008
	struct iw_statistics wstats;
	struct proc_dir_entry *dir_dev;
	
	/*RX stuff*/
//	u32 *rxring;
//	u32 *rxringtail;
//	dma_addr_t rxringdma;
	struct urb **rx_urb;
#ifdef THOMAS_BEACON
	unsigned long *oldaddr; //lzm for 64bit CPU crash
#endif

#ifdef THOMAS_TASKLET
	atomic_t irt_counter;//count for irq_rx_tasklet
#endif
#ifdef JACKSON_NEW_RX
        struct sk_buff **pp_rxskb;
        int     rx_inx;
#endif

	short  tx_urb_index;
	
	//struct buffer *rxbuffer;
	//struct buffer *rxbufferhead;
	//int rxringcount;
	//u16 rxbuffersize;
	
	//struct sk_buff *rx_skb; 

	//short rx_skb_complete;

	//u32 rx_prevlen;
	//atomic_t tx_lp_pending;
	//atomic_t tx_np_pending;
	atomic_t tx_pending[0x10];//UART_PRIORITY+1

#if 0	
	/*TX stuff*/
	u32 *txlpring;
	u32 *txhpring;
	u32 *txnpring;
	dma_addr_t txlpringdma;
	dma_addr_t txhpringdma;
	dma_addr_t txnpringdma;
	u32 *txlpringtail;
	u32 *txhpringtail;
	u32 *txnpringtail;
	u32 *txlpringhead;
	u32 *txhpringhead;
	u32 *txnpringhead;
	struct buffer *txlpbufs;
	struct buffer *txhpbufs;
	struct buffer *txnpbufs;
	struct buffer *txlpbufstail;
	struct buffer *txhpbufstail;
	struct buffer *txnpbufstail;
	int txringcount;
	int txbuffsize;

	//struct tx_pendingbuf txnp_pending;
	struct tasklet_struct irq_tx_tasklet;
#endif
	struct tasklet_struct irq_rx_tasklet;
	struct urb *rxurb_task;
//	u8 dma_poll_mask;
	//short tx_suspend;
	
	/* adhoc/master mode stuff */
#if 0
	u32 *txbeacontail;
	dma_addr_t txbeaconringdma;
	u32 *txbeaconring;
	int txbeaconcount;
#endif
//	struct ieee_tx_beacon *beacon_buf;
	//char *master_essid;
//	dma_addr_t beacondmabuf;
	//u16 master_beaconinterval;
//	u32 master_beaconsize;
	//u16 beacon_interval;

	//2 Tx Related variables
	u16	ShortRetryLimit;
	u16	LongRetryLimit;
	u32	TransmitConfig;
	u8	RegCWinMin;		// For turbo mode CW adaptive. Added by Annie, 2005-10-27.

	//2 Rx Related variables
	u16	EarlyRxThreshold;
	u32	ReceiveConfig;
	u8	AcmControl;

	u8	RFProgType;
	
	u8 retry_data;
	u8 retry_rts;
	u16 rts;

//by amy
        long            LastSignalStrengthInPercent;
        long            SignalStrength;
        long                SignalQuality;
        u8                      antenna_flag;
        bool                    flag_beacon;
//by amy
//by amy for rate adaptive
    struct timer_list rateadapter_timer;
    u16                                 LastRetryCnt;
        u16                                     LastRetryRate;
        unsigned long           LastTxokCnt;
        unsigned long           LastRxokCnt;
        u16                                     CurrRetryCnt;
        long                            RecvSignalPower;
        unsigned long           LastTxOKBytes;
        u8                                      LastFailTxRate;
        long                            LastFailTxRateSS;
        u8                                      FailTxRateCount;
        u32                             LastTxThroughput;
        unsigned long txokbytestotal;
        //for up rate
        unsigned short          bTryuping;
        u8                                      CurrTxRate;     //the rate before up
        u16                                     CurrRetryRate;
        u16                                     TryupingCount;
        u8                                      TryDownCountLowData;
        u8                                      TryupingCountNoData;

        u8                  CurrentOperaRate;
//by amy for rate adaptive
//by amy for power save
	struct timer_list watch_dog_timer;
	bool bInactivePs;
	bool bSwRfProcessing;
	RT_RF_POWER_STATE	eInactivePowerState;
	RT_RF_POWER_STATE eRFPowerState;
	u32 RfOffReason;
	bool RFChangeInProgress;
	bool bInHctTest;
	bool SetRFPowerStateInProgress;
	//u8   RFProgType;
	bool bLeisurePs;
	RT_PS_MODE dot11PowerSaveMode;
	u32 NumRxOkInPeriod;
	u32 NumTxOkInPeriod;
	u8 RegThreeWireMode;
	bool ps_mode;
//by amy for power save
//by amy for DIG
	bool bDigMechanism;
	bool bCCKThMechanism;
	u8   InitialGain;
	u8   StageCCKTh;
	u8   RegBModeGainStage;
	u8   RegDigOfdmFaUpTh;	 //added by david, 2008.3.6
	u8   DIG_NumberFallbackVote;
	u8   DIG_NumberUpgradeVote;
	u16  CCKUpperTh;
	u16  CCKLowerTh;
	u32  FalseAlarmRegValue; //added by david, 2008.3.6	
//by amy for DIG
//{ added by david for high power, 2008.3.11
	int  UndecoratedSmoothedSS;
	bool bRegHighPowerMechanism;
	bool bToUpdateTxPwr;
	u8   Z2HiPwrUpperTh;
	u8   Z2HiPwrLowerTh;
	u8   Z2RSSIHiPwrUpperTh;
	u8   Z2RSSIHiPwrLowerTh;
	// Current CCK RSSI value to determine CCK high power, asked by SD3 DZ, by Bruce, 2007-04-12.
	u8   CurCCKRSSI;
	bool bCurCCKPkt;
	u32  wMacRegRfPinsOutput;
	u32  wMacRegRfPinsSelect;
	TR_SWITCH_STATE TrSwitchState;
//}
//{added by david for radio on/off
	u8			radion;
//}
	struct 	ChnlAccessSetting  ChannelAccessSetting;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)	
	struct work_struct reset_wq;
#else
	struct tq_struct reset_wq;
#endif

#ifdef _RTL8187_EXT_PATCH_
	struct mshclass			*mshobj;
#endif
	
#ifdef LED
  /* add for led controll */
        u8                      EEPROMCustomerID;
        RT_CID_TYPE     CustomerID;
        LED_8187        Gpio0Led;
        LED_8187        SwLed0;
        LED_8187        SwLed1;
        u8                      bEnableLedCtrl;
        LED_STRATEGY_8187       LedStrategy;
        u8                      PsrValue;
        struct work_struct              Gpio0LedWorkItem;
        struct work_struct              SwLed0WorkItem;
        struct work_struct              SwLed1WorkItem;
#endif 
        u8      driver_upping;
#ifdef CPU_64BIT	
	u8      *usb_buf;
	struct dma_pool *usb_pool;
#endif


#ifdef SW_ANTE_DIVERSITY

//#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)	
//	struct delayed_work SwAntennaWorkItem;
//#else
//	struct work_struct SwAntennaWorkItem;
//#endif	
	
	bool	bAntennaDiversityTimerIssued;
	short antb;
	short diversity;
	bool	AutoloadFailFlag;
	u16	EEPROMVersion;
	u8	EEPROMAntennaDiversity;
	u16	EEPROMCSThreshold;
	u8	EEPROMDefaultAntennaB;
	u8	EEPROMDigitalPhy;
	u32	EEPROMCSMethod;
	u8	EEPROMGEPRFOffState;
	// For HW antenna diversity, added by Roger, 2008.01.30.
	u32	AdMainAntennaRxOkCnt;		// Main antenna Rx OK count. 
	u32	AdAuxAntennaRxOkCnt;		// Aux antenna Rx OK count. 
	bool	bHWAdSwitched;				// TRUE if we has switched default antenna by HW evaluation.
	u8 EEPROMSwAntennaDiversity;
	bool EEPROMDefaultAntenna1;
	u8 RegSwAntennaDiversityMechanism;// 0:default from EEPROM, 1: disable, 2: enable.
	bool bSwAntennaDiverity;
	u8 RegDefaultAntenna;// 0: default from EEPROM, 1: main, 2: aux. Added by Roger, 2007.11.05.
	bool bDefaultAntenna1;
	//long SignalStrength;
	long Stats_SignalStrength;
	//long LastSignalStrengthInPercent; // In percentange, used for smoothing, e.g. Moving Average.
	//long	 SignalQuality; // in 0-100 index. 
	long Stats_SignalQuality;
	//long RecvSignalPower; // in dBm.
	long Stats_RecvSignalPower;
	u8	 LastRxPktAntenna;	// +by amy 080312 Antenn which received the lasted packet. 0: Aux, 1:Main. Added by Roger, 2008.01.25.
	u32 AdRxOkCnt;
	long AdRxSignalStrength; // Rx signal strength for Antenna Diversity, which had been smoothing, its valid range is [0,100].	
	u8 CurrAntennaIndex;			// Index to current Antenna (both Tx and Rx).
	u8 AdTickCount;				// Times of SwAntennaDiversityTimer happened.
	u8 AdCheckPeriod;				// # of period SwAntennaDiversityTimer to check Rx signal strength for SW Antenna Diversity. 
	u8 AdMinCheckPeriod;			// Min value of AdCheckPeriod. 
	u8 AdMaxCheckPeriod;			// Max value of AdCheckPeriod.  
	long AdRxSsThreshold;			// Signal strength threshold to switch antenna.
	long AdMaxRxSsThreshold;			// Max value of AdRxSsThreshold.
	bool bAdSwitchedChecking;		// TRUE if we shall shall check Rx signal strength for last time switching antenna.
	long AdRxSsBeforeSwitched;		// Rx signal strength before we swithed antenna.
	struct timer_list SwAntennaDiversityTimer;
#endif
	u8	commit;

//#ifdef ENABLE_DOT11D
	u8	channel_plan;
//#endif	
	u8	EEPROMSelectNewGPIO;
}r8180_priv;

// for rtl8187
// now mirging to rtl8187B
/*
typedef enum{ 
	LOW_PRIORITY = 0x02,
	NORM_PRIORITY 
	} priority_t;
*/
//for rtl8187B
typedef enum{
	BULK_PRIORITY = 0x01,
	//RSVD0,
	//RSVD1,
	LOW_PRIORITY,
	NORM_PRIORITY, 
	VO_PRIORITY,
	VI_PRIORITY, //0x05
	BE_PRIORITY,
	BK_PRIORITY,
	RSVD2,
	RSVD3,
	BEACON_PRIORITY, //0x0A
	HIGH_PRIORITY,
	MANAGE_PRIORITY,
	RSVD4,
	RSVD5,
	UART_PRIORITY //0x0F
} priority_t;

typedef enum{
	NIC_8187 = 1,
	NIC_8187B
	} nic_t;


typedef u32 AC_CODING;
#define AC0_BE	0		// ACI: 0x00	// Best Effort
#define AC1_BK	1		// ACI: 0x01	// Background
#define AC2_VI	2		// ACI: 0x10	// Video
#define AC3_VO	3		// ACI: 0x11	// Voice
#define AC_MAX	4		// Max: define total number; Should not to be used as a real enum.

//
// ECWmin/ECWmax field.
// Ref: WMM spec 2.2.2: WME Parameter Element, p.13.
//
typedef	union _ECW{
	u8	charData;
	struct
	{
		u8	ECWmin:4;
		u8	ECWmax:4;
	}f;	// Field
}ECW, *PECW;

//
// ACI/AIFSN Field.
// Ref: WMM spec 2.2.2: WME Parameter Element, p.12.
//
typedef	union _ACI_AIFSN{
	u8	charData;
	
	struct
	{
		u8	AIFSN:4;
		u8	ACM:1;
		u8	ACI:2;
		u8	Reserved:1;
	}f;	// Field
}ACI_AIFSN, *PACI_AIFSN;

//
// AC Parameters Record Format.
// Ref: WMM spec 2.2.2: WME Parameter Element, p.12.
//
typedef	union _AC_PARAM{
	u32	longData;
	u8	charData[4];

	struct
	{
		ACI_AIFSN	AciAifsn;
		ECW		Ecw;
		u16		TXOPLimit;
	}f;	// Field
}AC_PARAM, *PAC_PARAM;

#ifdef JOHN_HWSEC
struct ssid_thread {
	struct net_device *dev;
       	u8 name[IW_ESSID_MAX_SIZE + 1];
};
#endif

short rtl8180_tx(struct net_device *dev,u32* skbuf, int len,priority_t priority,short morefrag,short rate);

#ifdef JOHN_TKIP
u32 read_cam(struct net_device *dev, u8 addr);
void write_cam(struct net_device *dev, u8 addr, u32 data);
#endif
u8 read_nic_byte(struct net_device *dev, int x);
u8 read_nic_byte_E(struct net_device *dev, int x);
u32 read_nic_dword(struct net_device *dev, int x);
u16 read_nic_word(struct net_device *dev, int x) ;
void write_nic_byte(struct net_device *dev, int x,u8 y);
void write_nic_byte_E(struct net_device *dev, int x,u8 y);
void write_nic_word(struct net_device *dev, int x,u16 y);
void write_nic_dword(struct net_device *dev, int x,u32 y);
void force_pci_posting(struct net_device *dev);

void rtl8180_rtx_disable(struct net_device *);
void rtl8180_rx_enable(struct net_device *);
void rtl8180_tx_enable(struct net_device *);

void rtl8180_disassociate(struct net_device *dev);
//void fix_rx_fifo(struct net_device *dev);
void rtl8185_set_rf_pins_enable(struct net_device *dev,u32 a);

void rtl8180_set_anaparam(struct net_device *dev,u32 a);
void rtl8185_set_anaparam2(struct net_device *dev,u32 a);
void rtl8180_update_msr(struct net_device *dev);
int rtl8180_down(struct net_device *dev);
int rtl8180_up(struct net_device *dev);
void rtl8180_commit(struct net_device *dev);
void rtl8180_set_chan(struct net_device *dev,short ch);
void write_phy(struct net_device *dev, u8 adr, u8 data);
void write_phy_cck(struct net_device *dev, u8 adr, u32 data);
void write_phy_ofdm(struct net_device *dev, u8 adr, u32 data);
void rtl8185_tx_antenna(struct net_device *dev, u8 ant);
void rtl8187_set_rxconf(struct net_device *dev);
bool MgntActSet_RF_State(struct net_device *dev,RT_RF_POWER_STATE StateToSet,u32 ChangeSource);
void IPSEnter(struct net_device *dev);
void IPSLeave(struct net_device *dev);
int r8187b_rfkill_init(struct net_device *dev);
void r8187b_rfkill_exit(void);
int r8187b_wifi_report_state(r8180_priv *priv);
void r8187b_wifi_change_rfkill_state(struct net_device *dev, RT_RF_POWER_STATE eRfPowerStateToSet, bool report_rfkill);
bool SetRFPowerState(struct net_device *dev,RT_RF_POWER_STATE eRFPowerState);
void rtl8180_patch_ieee80211_wx_sync_scan_wq(struct ieee80211_device *ieee);
#ifdef _RTL8187_EXT_PATCH_
extern int r8180_wx_set_channel(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
#endif
#ifdef JOHN_TKIP
void EnableHWSecurityConfig8187(struct net_device *dev);
void setKey(struct net_device *dev, u8 EntryNo, u8 KeyIndex, u16 KeyType, u8 *MacAddr, u8 DefaultKey, u32 *KeyContent );

#endif 

#endif
