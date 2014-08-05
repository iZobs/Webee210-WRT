/*
   This is part of rtl8187 OpenSource driver - v 0.1
   Copyright (C) Andrea Merello 2005  <andreamrl@tiscali.it> 
   Released under the terms of GPL (General Public License)
   
   
   Parts of this driver are based on the rtl8180 driver skeleton 
   from Patric Schenke & Andres Salomon.

   Parts of this driver are based on the Intel Pro Wireless 2*00 GPL drivers.
   
   some ideas might be derived from David Young rtl8180 netbsd driver.
   
   Parts of the usb code are from the r8150.c driver in linux kernel
   
   Some ideas borrowed from the 8139too.c driver included in linux kernel.
   
   We (I?) want to thanks the Authors of those projecs and also the 
   Ndiswrapper's project Authors.
   
   A special big thanks goes also to Realtek corp. for their help in my 
   attempt to add RTL8187 and RTL8225 support, and to David Young also. 

	- Please note that this file is a modified version from rtl8180-sa2400 
	drv. So some other people have contributed to this project, and they are
	thanked in the rtl8180-sa2400 CHANGELOG.
*/

#undef LOOP_TEST
#undef DUMP_RX
#undef DUMP_TX
#undef DEBUG_TX_DESC2
#undef RX_DONT_PASS_UL
#undef DEBUG_EPROM
#undef DEBUG_RX_VERBOSE
#undef DUMMY_RX
#undef DEBUG_ZERO_RX
#undef DEBUG_RX_SKB
#undef DEBUG_TX_FRAG
#undef DEBUG_RX_FRAG
#undef DEBUG_TX_FILLDESC
#undef DEBUG_TX
#undef DEBUG_IRQ
#undef DEBUG_RX
#undef DEBUG_RXALLOC
#undef DEBUG_REGISTERS
#undef DEBUG_RING
#undef DEBUG_IRQ_TASKLET
#undef DEBUG_TX_ALLOC
#undef DEBUG_TX_DESC
#undef CONFIG_SOFT_BEACON
#undef DEBUG_RW_REGISTER

#define CONFIG_RTL8180_IO_MAP
//#define CONFIG_SOFT_BEACON
//#define DEBUG_RW_REGISTER

#include "r8180_hw.h"
#include "r8187.h"
#include "r8180_rtl8225.h" /* RTL8225 Radio frontend */
#include "r8180_93cx6.h"   /* Card EEPROM */
#include "r8180_wx.h"
#include "r8180_dm.h"

#include <linux/dmapool.h>

#include <linux/usb.h>
// FIXME: check if 2.6.7 is ok
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7))
#define usb_kill_urb usb_unlink_urb
#endif

#ifdef CONFIG_RTL8180_PM
#include "r8180_pm.h"
#endif

#ifdef ENABLE_DOT11D
#include "dot11d.h"
#endif

#ifndef USB_VENDOR_ID_REALTEK
#define USB_VENDOR_ID_REALTEK		0x0bda
#endif
#ifndef USB_VENDOR_ID_NETGEAR
#define USB_VENDOR_ID_NETGEAR		0x0846
#endif

#define TXISR_SELECT(priority)	  ((priority == MANAGE_PRIORITY)?rtl8187_managetx_isr:\
	(priority == BEACON_PRIORITY)?rtl8187_beacontx_isr: \
	(priority == VO_PRIORITY)?rtl8187_votx_isr: \
	(priority == VI_PRIORITY)?rtl8187_vitx_isr:\
	(priority == BE_PRIORITY)?rtl8187_betx_isr:rtl8187_bktx_isr)

static struct usb_device_id rtl8187_usb_id_tbl[] = {
	{USB_DEVICE(USB_VENDOR_ID_REALTEK, 0x8187)},
	{USB_DEVICE(USB_VENDOR_ID_REALTEK, 0x8189)},
//	{USB_DEVICE_VER(USB_VENDOR_ID_REALTEK, 0x8187,0x0200,0x0200)},
	{USB_DEVICE(USB_VENDOR_ID_NETGEAR, 0x6100)},
	{USB_DEVICE(USB_VENDOR_ID_NETGEAR, 0x6a00)},
	{USB_DEVICE(USB_VENDOR_ID_REALTEK, 0x8197)},
	{USB_DEVICE(USB_VENDOR_ID_REALTEK, 0x8198)},
	{}
};

static char* ifname = "wlan%d";
#if 0
static int hwseqnum = 0;
static int hwwep = 0;
#endif
static int channels = 0x3fff;
//static int channels = 0x7ff;// change by thomas, use 1 - 11 channel 0907-2007
#define eqMacAddr(a,b)      ( ((a)[0]==(b)[0] && (a)[1]==(b)[1] && (a)[2]==(b)[2] && (a)[3]==(b)[3] && (a)[4]==(b)[4] && (a)[5]==(b)[5]) ? 1:0 )
//by amy for rate adaptive
#define DEFAULT_RATE_ADAPTIVE_TIMER_PERIOD      300
//by amy for rate adaptive
//by amy for ps
#define IEEE80211_WATCH_DOG_TIME    2000
//by amy for ps
MODULE_LICENSE("GPL");
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0))
MODULE_VERSION("V 1.1");
#endif
MODULE_DEVICE_TABLE(usb, rtl8187_usb_id_tbl);
MODULE_AUTHOR("Realsil Wlan");
MODULE_DESCRIPTION("Linux driver for Realtek RTL8187 WiFi cards");

#if 0
MODULE_PARM(ifname,"s");
MODULE_PARM_DESC(devname," Net interface name, wlan%d=default");

MODULE_PARM(hwseqnum,"i");
MODULE_PARM_DESC(hwseqnum," Try to use hardware 802.11 header sequence numbers. Zero=default");

MODULE_PARM(hwwep,"i");
MODULE_PARM_DESC(hwwep," Try to use hardware WEP support. Still broken and not available on all cards");

MODULE_PARM(channels,"i");
MODULE_PARM_DESC(channels," Channel bitmask for specific locales. NYI");
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
module_param(ifname, charp, S_IRUGO|S_IWUSR );
//module_param(hwseqnum,int, S_IRUGO|S_IWUSR);
//module_param(hwwep,int, S_IRUGO|S_IWUSR);
module_param(channels,int, S_IRUGO|S_IWUSR);
#else
MODULE_PARM(ifname, "s");
//MODULE_PARM(hwseqnum,"i");
//MODULE_PARM(hwwep,"i");
MODULE_PARM(channels,"i");
#endif

MODULE_PARM_DESC(devname," Net interface name, wlan%d=default");
//MODULE_PARM_DESC(hwseqnum," Try to use hardware 802.11 header sequence numbers. Zero=default");
//MODULE_PARM_DESC(hwwep," Try to use hardware WEP support. Still broken and not available on all cards");
MODULE_PARM_DESC(channels," Channel bitmask for specific locales. NYI");

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
static int __devinit rtl8187_usb_probe(struct usb_interface *intf,
			 const struct usb_device_id *id);
static void __devexit rtl8187_usb_disconnect(struct usb_interface *intf);
#else
static void *__devinit rtl8187_usb_probe(struct usb_device *udev,unsigned int ifnum,
			 const struct usb_device_id *id);
static void __devexit rtl8187_usb_disconnect(struct usb_device *udev, void *ptr);
#endif
		 

static struct usb_driver rtl8187_usb_driver = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 15)
	.owner		= THIS_MODULE,
#endif
	.name		= RTL8187_MODULE_NAME,	          /* Driver name   */
	.id_table	= rtl8187_usb_id_tbl,	          /* PCI_ID table  */
	.probe		= rtl8187_usb_probe,	          /* probe fn      */
	.disconnect	= rtl8187_usb_disconnect,	  /* remove fn     */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 5, 0)
#ifdef CONFIG_RTL8180_PM
	.suspend	= rtl8187_suspend,	          /* PM suspend fn */
	.resume		= rtl8187_resume,                 /* PM resume fn  */
#else
	.suspend	= NULL,			          /* PM suspend fn */
	.resume      	= NULL,			          /* PM resume fn  */
#endif
#endif
};

#ifdef JOHN_HWSEC
void CAM_mark_invalid(struct net_device *dev, u8 ucIndex)
{
        u32 ulContent=0;
        u32 ulCommand=0;
        u32 ulEncAlgo=CAM_AES;

        // keyid must be set in config field
        ulContent |= (ucIndex&3) | ((u16)(ulEncAlgo)<<2);

        ulContent |= BIT15;
        // polling bit, and No Write enable, and address
        ulCommand= CAM_CONTENT_COUNT*ucIndex;
        ulCommand= ulCommand | BIT31|BIT16;
        // write content 0 is equall to mark invalid

        write_nic_dword(dev, WCAMI, ulContent);  //delay_ms(40);
        //RT_TRACE(COMP_SEC, DBG_LOUD, ("CAM_mark_invalid(): WRITE A4: %x \n",ulContent));
        write_nic_dword(dev, RWCAM, ulCommand);  //delay_ms(40);
        //RT_TRACE(COMP_SEC, DBG_LOUD, ("CAM_mark_invalid(): WRITE A0: %x \n",ulCommand));
}

void CAM_empty_entry(struct net_device *dev, u8 ucIndex)
{
        u32 ulCommand=0;
        u32 ulContent=0;
        u8 i;   
        u32 ulEncAlgo=CAM_AES;

        for(i=0;i<6;i++)
        {

                // filled id in CAM config 2 byte
                if( i == 0)
                {
                        ulContent |=(ucIndex & 0x03) | (ulEncAlgo<<2);
                        ulContent |= BIT15;

                }
                else
                {
                        ulContent = 0;
                }
                // polling bit, and No Write enable, and address
                ulCommand= CAM_CONTENT_COUNT*ucIndex+i;
                ulCommand= ulCommand | BIT31|BIT16;
                // write content 0 is equall to mark invalid
                write_nic_dword(dev, WCAMI, ulContent);  //delay_ms(40);
                //RT_TRACE(COMP_SEC, DBG_LOUD, ("CAM_empty_entry(): WRITE A4: %x \n",ulContent));
                write_nic_dword(dev, RWCAM, ulCommand);  //delay_ms(40);
                //RT_TRACE(COMP_SEC, DBG_LOUD, ("CAM_empty_entry(): WRITE A0: %x \n",ulCommand));
        }
}

void CamResetAllEntry(struct net_device *dev)
{
        u8 ucIndex;

        //2004/02/11  In static WEP, OID_ADD_KEY or OID_ADD_WEP are set before STA associate to AP.
        // However, ResetKey is called on OID_802_11_INFRASTRUCTURE_MODE and MlmeAssociateRequest
        // In this condition, Cam can not be reset because upper layer will not set this static key again.
        //if(Adapter->EncAlgorithm == WEP_Encryption)
        //      return;
	//debug
        //DbgPrint("========================================\n");
        //DbgPrint("          Call ResetAllEntry            \n");
        //DbgPrint("========================================\n\n");

        for(ucIndex=0;ucIndex<TOTAL_CAM_ENTRY;ucIndex++)
                CAM_mark_invalid(dev, ucIndex);
        for(ucIndex=0;ucIndex<TOTAL_CAM_ENTRY;ucIndex++)
                CAM_empty_entry(dev, ucIndex);

}


void write_cam(struct net_device *dev, u8 addr, u32 data)
{
        write_nic_dword(dev, WCAMI, data);
        write_nic_dword(dev, RWCAM, BIT31|BIT16|(addr&0xff) );
}
u32 read_cam(struct net_device *dev, u8 addr)
{
        write_nic_dword(dev, RWCAM, 0x80000000|(addr&0xff) );
        return read_nic_dword(dev, 0xa8);
}
#endif  /*JOHN_HWSEC*/

#ifdef DEBUG_RW_REGISTER
//lzm add for write time out test
void add_into_rw_registers(struct net_device *dev, u32 addr, u32 cont, u8 flag)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	int reg_index = (priv->write_read_register_index % 200) ;

	priv->write_read_registers[reg_index].address = 0;
	priv->write_read_registers[reg_index].content = 0;
	priv->write_read_registers[reg_index].flag = 0;

	priv->write_read_registers[reg_index].address = addr;
	priv->write_read_registers[reg_index].content = cont;
	priv->write_read_registers[reg_index].flag = flag;

	priv->write_read_register_index = (priv->write_read_register_index + 1) % 200;
}

bool print_once = 0;

void print_rw_registers(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	int reg_index = 0;
	int watchdog = 0;
	if(print_once == false)
	{
		print_once = true;
		for(reg_index = ((priv->write_read_register_index + 1) % 200); watchdog <= 199; reg_index++)
		{
			watchdog++;
			printk("====>reg_addr:0x%x, reg_cont:0x%x, read_or_write:0x%d\n", 
					priv->write_read_registers[reg_index].address,
					priv->write_read_registers[reg_index].content,
					priv->write_read_registers[reg_index].flag);
		}
	}
}
//lzm add for write time out test
#endif

#ifdef CPU_64BIT
void write_nic_byte_E(struct net_device *dev, int indx, u8 data)
{
	int status;	
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;
	
	status = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			       RTL8187_REQ_SET_REGS, RTL8187_REQT_WRITE,
			       indx|0xfe00, 0, &data, 1, HZ / 2);
	
//lzm add for write time out test
#ifdef DEBUG_RW_REGISTER
	add_into_rw_registers(dev, indx, data, 2);
#endif

	if (status < 0)
	{
		printk("write_nic_byte_E TimeOut!addr:%x, status:%x\n", indx, status);
#ifdef DEBUG_RW_REGISTER
		print_rw_registers(dev);
#endif
	}	
}

u8 read_nic_byte_E(struct net_device *dev, int indx)
{
	int status;
	u8 data, *buf;
	dma_addr_t dma_handle;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;
	
	buf = dma_pool_alloc(priv->usb_pool, GFP_ATOMIC, &dma_handle);
	if (!buf) {
		printk("read_nic_byte_E out of memory\n");
		return -ENOMEM;
	}
	status = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			       RTL8187_REQ_GET_REGS, RTL8187_REQT_READ,
			       indx|0xfe00, 0, buf, 1, HZ / 2);
//lzm add for write time out test
#ifdef DEBUG_RW_REGISTER
	add_into_rw_registers(dev, indx, buf[0], 1);
#endif

        if (status < 0)
        {
                printk("read_nic_byte_E TimeOut!addr:%x, status:%x\n",indx,  status);
#ifdef DEBUG_RW_REGISTER
		print_rw_registers(dev);
#endif
        }

	data = buf[0];
	dma_pool_free(priv->usb_pool, buf, dma_handle);
	return data;
}

void write_nic_byte(struct net_device *dev, int indx, u8 data)
{
	int status;
	
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;
	
	status = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			       RTL8187_REQ_SET_REGS, RTL8187_REQT_WRITE,
			       (indx&0xff)|0xff00, (indx>>8)&0x03, &data, 1, HZ / 2);

//lzm add for write time out test
#ifdef DEBUG_RW_REGISTER
	add_into_rw_registers(dev, indx, data, 2);
#endif
        if (status < 0)
        {
                printk("write_nic_byte TimeOut!addr:%x, status:%x\n",indx,  status);
#ifdef DEBUG_RW_REGISTER
		print_rw_registers(dev);
#endif
        }


}

void write_nic_word(struct net_device *dev, int indx, u16 data)
{
	
	int status;
	
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;
	
	status = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			       RTL8187_REQ_SET_REGS, RTL8187_REQT_WRITE,
			       (indx&0xff)|0xff00, (indx>>8)&0x03, &data, 2, HZ / 2);

//lzm add for write time out test
#ifdef DEBUG_RW_REGISTER
	add_into_rw_registers(dev, indx, data, 2);

	if(priv->write_read_register_index == 199)
	{
		//print_rw_registers(dev);
	}
#endif
        if (status < 0)
        {
                printk("write_nic_word TimeOut!addr:%x, status:%x\n",indx,  status);
#ifdef DEBUG_RW_REGISTER
		print_rw_registers(dev);
#endif
        }

}

void write_nic_dword(struct net_device *dev, int indx, u32 data)
{

	int status;
	
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;
	
	status = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			       RTL8187_REQ_SET_REGS, RTL8187_REQT_WRITE,
			       (indx&0xff)|0xff00, (indx>>8)&0x03, &data, 4, HZ / 2);
//lzm add for write time out test
#ifdef DEBUG_RW_REGISTER
	add_into_rw_registers(dev, indx, data, 2);
#endif


        if (status < 0)
        {
                printk("write_nic_dword TimeOut!addr:%x, status:%x\n",indx,  status);
#ifdef DEBUG_RW_REGISTER
		print_rw_registers(dev);
#endif
        }

}
 
 u8 read_nic_byte(struct net_device *dev, int indx)
{
	u8 data, *buf;
	int status;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;
	dma_addr_t dma_handle;
	
	buf = dma_pool_alloc(priv->usb_pool, GFP_ATOMIC, &dma_handle);
	if (!buf) {
		printk("read_nic_byte: out of memory\n");
		return -ENOMEM;
	}
	status = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			       RTL8187_REQ_GET_REGS, RTL8187_REQT_READ,
			       (indx&0xff)|0xff00, (indx>>8)&0x03, buf, 1, HZ / 2);
//lzm add for write time out test
#ifdef DEBUG_RW_REGISTER
	add_into_rw_registers(dev, indx, buf[0], 1);
#endif
	
        if (status < 0)
        {
                printk("read_nic_byte TimeOut!addr:%x, status:%x\n",indx,  status);
#ifdef DEBUG_RW_REGISTER
		print_rw_registers(dev);
#endif
        }

	data = buf[0];
	dma_pool_free(priv->usb_pool, buf, dma_handle);
	return data;
}

u16 read_nic_word(struct net_device *dev, int indx)
{
	u16 data, *buf;
	int status;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;
	dma_addr_t dma_handle;
	
	buf = dma_pool_alloc(priv->usb_pool, GFP_ATOMIC, &dma_handle);
	if (!buf) {
		printk("read_nic_word: out of memory\n");
		return -ENOMEM;
	}
	status = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			       RTL8187_REQ_GET_REGS, RTL8187_REQT_READ,
			       (indx&0xff)|0xff00, (indx>>8)&0x03, buf, 2, HZ / 2);
//lzm add for write time out test
#ifdef DEBUG_RW_REGISTER
	add_into_rw_registers(dev, indx, buf[0], 1);
#endif
	
        if (status < 0)
        {
                printk("read_nic_word TimeOut!addr:%x, status:%x\n",indx,  status);
#ifdef DEBUG_RW_REGISTER
		print_rw_registers(dev);
#endif
        }


	data = buf[0];
	dma_pool_free(priv->usb_pool, buf, dma_handle);
	return data;
}

u32 read_nic_dword(struct net_device *dev, int indx)
{
	u32 data, *buf;
	int status;
	dma_addr_t dma_handle;
//	int result;
	
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;
	
	buf = dma_pool_alloc(priv->usb_pool, GFP_ATOMIC, &dma_handle);
	if (!buf){
		printk("read_nic_dword: out of memory\n");
		return -ENOMEM;
	}
	status = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			       RTL8187_REQ_GET_REGS, RTL8187_REQT_READ,
			       (indx&0xff)|0xff00, (indx>>8)&0x03, buf, 4, HZ / 2);
//lzm add for write time out test
#ifdef DEBUG_RW_REGISTER
	add_into_rw_registers(dev, indx, buf[0], 1);
#endif
	
        if (status < 0)
        {
                printk("read_nic_dword TimeOut!addr:%x, status:%x\n",indx, status);
#ifdef DEBUG_RW_REGISTER
		print_rw_registers(dev);
#endif
        }



	data = buf[0];
	dma_pool_free(priv->usb_pool, buf, dma_handle);
	return data;
}
#else
void write_nic_byte_E(struct net_device *dev, int indx, u8 data)
{
	int status;	
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;
	
	status = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			       RTL8187_REQ_SET_REGS, RTL8187_REQT_WRITE,
			       indx|0xfe00, 0, &data, 1, HZ / 2);

	if (status < 0)
	{
		printk("write_nic_byte_E TimeOut!addr:0x%x,val:0x%x, status:%x\n", indx,data,status);
	}	
}

u8 read_nic_byte_E(struct net_device *dev, int indx)
{
	int status;
	u8 data = 0;
	u8 buf[64];
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;
	
	status = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			       RTL8187_REQ_GET_REGS, RTL8187_REQT_READ,
			       indx|0xfe00, 0, buf, 1, HZ / 2);

        if (status < 0)
        {
                printk("read_nic_byte_E TimeOut!addr:0x%x, status:%x\n", indx, status);
        }

	data = *(u8*)buf;
	return data;
}

void write_nic_byte(struct net_device *dev, int indx, u8 data)
{
	int status;
	
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;
	
	status = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			       RTL8187_REQ_SET_REGS, RTL8187_REQT_WRITE,
			       (indx&0xff)|0xff00, (indx>>8)&0x03, &data, 1, HZ / 2);

        if (status < 0)
        {
                printk("write_nic_byte TimeOut!addr:0x%x,val:0x%x, status:%x\n", indx,data, status);
        }


}

void write_nic_word(struct net_device *dev, int indx, u16 data)
{
	
	int status;
	
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;
	
	status = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			       RTL8187_REQ_SET_REGS, RTL8187_REQT_WRITE,
			       (indx&0xff)|0xff00, (indx>>8)&0x03, &data, 2, HZ / 2);

        if (status < 0)
        {
                printk("write_nic_word TimeOut!addr:0x%x,val:0x%x, status:%x\n", indx,data, status);
        }

}

void write_nic_dword(struct net_device *dev, int indx, u32 data)
{

	int status;
	
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;
	
	status = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			       RTL8187_REQ_SET_REGS, RTL8187_REQT_WRITE,
			       (indx&0xff)|0xff00, (indx>>8)&0x03, &data, 4, HZ / 2);


        if (status < 0)
        {
                printk("write_nic_dword TimeOut!addr:0x%x,val:0x%x, status:%x\n", indx,data, status);
        }

}
 
u8 read_nic_byte(struct net_device *dev, int indx)
{
	u8 data = 0;
	u8 buf[64];
	int status;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;
	
	status = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			       RTL8187_REQ_GET_REGS, RTL8187_REQT_READ,
			       (indx&0xff)|0xff00, (indx>>8)&0x03, buf, 1, HZ / 2);
	
        if (status < 0)
        {
                printk("read_nic_byte TimeOut!addr:0x%x,status:%x\n", indx,status);
        }


	data = *(u8*)buf;
	return data;
}

u16 read_nic_word(struct net_device *dev, int indx)
{
	u16 data = 0;
	u8 buf[64];
	int status;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;
	
	status = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			       RTL8187_REQ_GET_REGS, RTL8187_REQT_READ,
			       (indx&0xff)|0xff00, (indx>>8)&0x03, buf, 2, HZ / 2);
	
        if (status < 0)
        {
                printk("read_nic_word TimeOut!addr:0x%x,status:%x\n", indx,status);
        }

	data = *(u16*)buf;
	return data;
}

u32 read_nic_dword(struct net_device *dev, int indx)
{
	u32 data = 0;
	u8 buf[64];
	int status;
	
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;
	
	status = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			       RTL8187_REQ_GET_REGS, RTL8187_REQT_READ,
			       (indx&0xff)|0xff00, (indx>>8)&0x03, buf, 4, HZ / 2);
	
        if (status < 0)
        {
                printk("read_nic_dword TimeOut!addr:0x%x,status:%x\n", indx, status);
        }


	data = *(u32*)buf;
	return data;
}
#endif


u8 read_phy_cck(struct net_device *dev, u8 adr);
u8 read_phy_ofdm(struct net_device *dev, u8 adr);
/* this might still called in what was the PHY rtl8185/rtl8187 common code 
 * plans are to possibilty turn it again in one common code...
 */
inline void force_pci_posting(struct net_device *dev)
{
}


static struct net_device_stats *rtl8180_stats(struct net_device *dev);
void rtl8180_commit(struct net_device *dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
void rtl8180_restart(struct work_struct *work);
#else
void rtl8180_restart(struct net_device *dev);
#endif
/****************************************************************************
   -----------------------------PROCFS STUFF-------------------------
*****************************************************************************/

static struct proc_dir_entry *rtl8180_proc = NULL;
static int proc_get_stats_ap(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee80211;
	struct ieee80211_network *target;
	
	int len = 0;

        list_for_each_entry(target, &ieee->network_list, list) {

		len += snprintf(page + len, count - len,
                "%s ", target->ssid);
                len += snprintf(page + len, count - len,
                "%ld ", (jiffies-target->last_scanned)/HZ);

		

		if(target->wpa_ie_len>0 || target->rsn_ie_len>0){
	                len += snprintf(page + len, count - len,
        	        "WPA\n");
		}
		else{
                        len += snprintf(page + len, count - len,
                        "non_WPA\n");
                }
		 
        }
	
	*eof = 1;
	return len;
}

static int proc_get_registers(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
	
	int len = 0;
	int i,n;
			
	int max=0xff;
	
	/* This dump the current register page */
len += snprintf(page + len, count - len,
                        "\n####################page 0##################\n ");

	for(n=0;n<=max;)
	{
		//printk( "\nD: %2x> ", n);
		len += snprintf(page + len, count - len,
			"\nD:  %2x > ",n);

		for(i=0;i<16 && n<=max;i++,n++)
		len += snprintf(page + len, count - len,
			"%2x ",read_nic_byte(dev,n));

		//	printk("%2x ",read_nic_byte(dev,n));
	}
	len += snprintf(page + len, count - len,"\n");
len += snprintf(page + len, count - len,
                        "\n####################page 1##################\n ");
        for(n=0;n<=max;)
        {
                //printk( "\nD: %2x> ", n);
                len += snprintf(page + len, count - len,
                        "\nD:  %2x > ",n);

                for(i=0;i<16 && n<=max;i++,n++)
                len += snprintf(page + len, count - len,
                        "%2x ",read_nic_byte(dev,0x100|n));

                //      printk("%2x ",read_nic_byte(dev,n));
        }
len += snprintf(page + len, count - len,
                        "\n####################page 2##################\n ");
        for(n=0;n<=max;)
        {
                //printk( "\nD: %2x> ", n);
                len += snprintf(page + len, count - len,
                        "\nD:  %2x > ",n);

                for(i=0;i<16 && n<=max;i++,n++)
                len += snprintf(page + len, count - len,
                        "%2x ",read_nic_byte(dev,0x200|n));

                //      printk("%2x ",read_nic_byte(dev,n));
        }


		
	*eof = 1;
	return len;

}


static int proc_get_cck_reg(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
	
	int len = 0;
	int i,n;
			
	int max = 0x5F;
	
	/* This dump the current register page */
	for(n=0;n<=max;)
	{
		//printk( "\nD: %2x> ", n);
		len += snprintf(page + len, count - len,
			"\nD:  %2x > ",n);

		for(i=0;i<16 && n<=max;i++,n++)
		len += snprintf(page + len, count - len,
			"%2x ",read_phy_cck(dev,n));

		//	printk("%2x ",read_nic_byte(dev,n));
	}
	len += snprintf(page + len, count - len,"\n");

		
	*eof = 1;
	return len;

}


static int proc_get_ofdm_reg(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
	
	int len = 0;
	int i,n;
			
	//int max=0xff;
	int max = 0x40;
	
	/* This dump the current register page */
	for(n=0;n<=max;)
	{
		//printk( "\nD: %2x> ", n);
		len += snprintf(page + len, count - len,
			"\nD:  %2x > ",n);

		for(i=0;i<16 && n<=max;i++,n++)
		len += snprintf(page + len, count - len,
			"%2x ",read_phy_ofdm(dev,n));

		//	printk("%2x ",read_nic_byte(dev,n));
	}
	len += snprintf(page + len, count - len,"\n");


		
	*eof = 1;
	return len;

}


#if 0
static int proc_get_stats_hw(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	
	int len = 0;
	
	len += snprintf(page + len, count - len,
		"NIC int: %lu\n"
		"Total int: %lu\n",
		priv->stats.ints,
		priv->stats.shints);
			
	*eof = 1;
	return len;
}
#endif

static int proc_get_stats_tx(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	
	int len = 0;
	
	len += snprintf(page + len, count - len,
		"TX VI priority ok int: %lu\n"
		"TX VI priority error int: %lu\n"
		"TX VO priority ok int: %lu\n"
		"TX VO priority error int: %lu\n"
		"TX BE priority ok int: %lu\n"
		"TX BE priority error int: %lu\n"
		"TX BK priority ok int: %lu\n"
		"TX BK priority error int: %lu\n"
		"TX MANAGE priority ok int: %lu\n"
		"TX MANAGE priority error int: %lu\n"
		"TX BEACON priority ok int: %lu\n"
		"TX BEACON priority error int: %lu\n"
//		"TX high priority ok int: %lu\n"
//		"TX high priority failed error int: %lu\n"
		"TX queue resume: %lu\n"
		"TX queue stopped?: %d\n"
		"TX fifo overflow: %lu\n"
//		"TX beacon: %lu\n"
		"TX VI queue: %d\n"
		"TX VO queue: %d\n"
		"TX BE queue: %d\n"
		"TX BK queue: %d\n"
		"TX BEACON queue: %d\n"
		"TX MANAGE queue: %d\n"
//		"TX HW queue: %d\n"
		"TX VI dropped: %lu\n"
		"TX VO dropped: %lu\n"
		"TX BE dropped: %lu\n"
		"TX BK dropped: %lu\n"
		"TX total data packets %lu\n",		
//		"TX beacon aborted: %lu\n",
		priv->stats.txviokint,
		priv->stats.txvierr,
		priv->stats.txvookint,
		priv->stats.txvoerr,
		priv->stats.txbeokint,
		priv->stats.txbeerr,
		priv->stats.txbkokint,
		priv->stats.txbkerr,
		priv->stats.txmanageokint,
		priv->stats.txmanageerr,
		priv->stats.txbeaconokint,
		priv->stats.txbeaconerr,
//		priv->stats.txhpokint,
//		priv->stats.txhperr,
		priv->stats.txresumed,
		netif_queue_stopped(dev),
		priv->stats.txoverflow,
//		priv->stats.txbeacon,
		atomic_read(&(priv->tx_pending[VI_PRIORITY])),
		atomic_read(&(priv->tx_pending[VO_PRIORITY])),
		atomic_read(&(priv->tx_pending[BE_PRIORITY])),
		atomic_read(&(priv->tx_pending[BK_PRIORITY])),
		atomic_read(&(priv->tx_pending[BEACON_PRIORITY])),
		atomic_read(&(priv->tx_pending[MANAGE_PRIORITY])),
//		read_nic_byte(dev, TXFIFOCOUNT),
		priv->stats.txvidrop,
		priv->stats.txvodrop,
		priv->stats.txbedrop,
		priv->stats.txbkdrop,
		priv->stats.txdatapkt
//		priv->stats.txbeaconerr
		);
			
	*eof = 1;
	return len;
}		



static int proc_get_stats_rx(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	
	int len = 0;
	
	len += snprintf(page + len, count - len,
		"RX packets: %lu\n"
		"RX urb status error: %lu\n"
		"RX invalid urb error: %lu\n",
		priv->stats.rxok,
		priv->stats.rxstaterr,
		priv->stats.rxurberr);
			
	*eof = 1;
	return len;
}		

#if WIRELESS_EXT < 17
static struct iw_statistics *r8180_get_wireless_stats(struct net_device *dev)
{
       struct r8180_priv *priv = ieee80211_priv(dev);

       return &priv->wstats;
}
#endif

void rtl8180_proc_module_init(void)
{	
	DMESG("Initializing proc filesystem");
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
	rtl8180_proc=create_proc_entry(RTL8187_MODULE_NAME, S_IFDIR, proc_net);
#else
	rtl8180_proc=create_proc_entry(RTL8187_MODULE_NAME, S_IFDIR, init_net.proc_net);
#endif
}


void rtl8180_proc_module_remove(void)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
	remove_proc_entry(RTL8187_MODULE_NAME, proc_net);
#else
	remove_proc_entry(RTL8187_MODULE_NAME, init_net.proc_net);
#endif
}


void rtl8180_proc_remove_one(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	if (priv->dir_dev) {
	//	remove_proc_entry("stats-hw", priv->dir_dev);
		remove_proc_entry("stats-tx", priv->dir_dev);
		remove_proc_entry("stats-rx", priv->dir_dev);
	//	remove_proc_entry("stats-ieee", priv->dir_dev);
		remove_proc_entry("stats-ap", priv->dir_dev);
		remove_proc_entry("registers", priv->dir_dev);
		remove_proc_entry("cck-registers",priv->dir_dev);
		remove_proc_entry("ofdm-registers",priv->dir_dev);
		remove_proc_entry(dev->name, rtl8180_proc);
		priv->dir_dev = NULL;
	}
}


void rtl8180_proc_init_one(struct net_device *dev)
{
	struct proc_dir_entry *e;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	priv->dir_dev = create_proc_entry(dev->name, 
					  S_IFDIR | S_IRUGO | S_IXUGO, 
					  rtl8180_proc);
	if (!priv->dir_dev) {
		DMESGE("Unable to initialize /proc/net/rtl8187/%s\n",
		      dev->name);
		return;
	}
	#if 0
	e = create_proc_read_entry("stats-hw", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_stats_hw, dev);
				   
	if (!e) {
		DMESGE("Unable to initialize "
		      "/proc/net/rtl8187/%s/stats-hw\n",
		      dev->name);
	}
	#endif
	e = create_proc_read_entry("stats-rx", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_stats_rx, dev);
				   
	if (!e) {
		DMESGE("Unable to initialize "
		      "/proc/net/rtl8187/%s/stats-rx\n",
		      dev->name);
	}
	
	
	e = create_proc_read_entry("stats-tx", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_stats_tx, dev);
				   
	if (!e) {
		DMESGE("Unable to initialize "
		      "/proc/net/rtl8187/%s/stats-tx\n",
		      dev->name);
	}
	#if 0
	e = create_proc_read_entry("stats-ieee", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_stats_ieee, dev);
				   
	if (!e) {
		DMESGE("Unable to initialize "
		      "/proc/net/rtl8187/%s/stats-ieee\n",
		      dev->name);
	}
	
	#endif
	
	e = create_proc_read_entry("stats-ap", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_stats_ap, dev);
				   
	if (!e) {
		DMESGE("Unable to initialize "
		      "/proc/net/rtl8187/%s/stats-ap\n",
		      dev->name);
	}
	
	e = create_proc_read_entry("registers", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_registers, dev);
	if (!e) {
		DMESGE("Unable to initialize "
		      "/proc/net/rtl8187/%s/registers\n",
		      dev->name);
	}

	e = create_proc_read_entry("cck-registers", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_cck_reg, dev);
	if (!e) {
		DMESGE("Unable to initialize "
		      "/proc/net/rtl8187/%s/cck-registers\n",
		      dev->name);
	}
	
	e = create_proc_read_entry("ofdm-registers", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_ofdm_reg, dev);
	if (!e) {
		DMESGE("Unable to initialize "
		      "/proc/net/rtl8187/%s/ofdm-registers\n",
		      dev->name);
	}

#ifdef _RTL8187_EXT_PATCH_
	if( priv->mshobj && priv->mshobj->ext_patch_create_proc )
		priv->mshobj->ext_patch_create_proc(priv);
#endif

}
/****************************************************************************
   -----------------------------MISC STUFF-------------------------
*****************************************************************************/

/* this is only for debugging */
void print_buffer(u32 *buffer, int len)
{
	int i;
	u8 *buf =(u8*)buffer;
	
	printk("ASCII BUFFER DUMP (len: %x):\n",len);
	
	for(i=0;i<len;i++)
		printk("%c",buf[i]);
		
	printk("\nBINARY BUFFER DUMP (len: %x):\n",len);
	
	for(i=0;i<len;i++)
		printk("%x",buf[i]);

	printk("\n");
}

short check_nic_enought_desc(struct net_device *dev, priority_t priority)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	//int used = atomic_read((priority == NORM_PRIORITY) ? 
	//	&priv->tx_np_pending : &priv->tx_lp_pending);
	int used = atomic_read(&priv->tx_pending[priority]); 
	
	return (used < MAX_TX_URB);
}

void tx_timeout(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	//rtl8180_commit(dev);
        printk("@@@@ Transmit timeout at %ld, latency %ld\n", jiffies,
                        jiffies - dev->trans_start);

	printk("@@@@ netif_queue_stopped = %d\n", netif_queue_stopped(dev));

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0))
	schedule_work(&priv->reset_wq);
#else
	schedule_task(&priv->reset_wq);
#endif
	//DMESG("TXTIMEOUT");
}


/* this is only for debug */
void dump_eprom(struct net_device *dev)
{
	int i;
	for(i=0; i<63; i++)
		DMESG("EEPROM addr %x : %x", i, eprom_read(dev,i));
}

/* this is only for debug */
void rtl8180_dump_reg(struct net_device *dev)
{
	int i;
	int n;
	int max=0xff;
	
	DMESG("Dumping NIC register map");	
	
	for(n=0;n<=max;)
	{
		printk( "\nD: %2x> ", n);
		for(i=0;i<16 && n<=max;i++,n++)
			printk("%2x ",read_nic_byte(dev,n));
	}
	printk("\n");
}

/****************************************************************************
      ------------------------------HW STUFF---------------------------
*****************************************************************************/


void rtl8180_irq_enable(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);	
	//priv->irq_enabled = 1;

	//write_nic_word(dev,INTA_MASK,INTA_RXOK | INTA_RXDESCERR | INTA_RXOVERFLOW | 
	//		     INTA_TXOVERFLOW | INTA_HIPRIORITYDESCERR | INTA_HIPRIORITYDESCOK | 
	//		     INTA_NORMPRIORITYDESCERR | INTA_NORMPRIORITYDESCOK |
	//		     INTA_LOWPRIORITYDESCERR | INTA_LOWPRIORITYDESCOK | INTA_TIMEOUT);

	write_nic_word(dev,INTA_MASK, priv->irq_mask);
}


void rtl8180_irq_disable(struct net_device *dev)
{
	write_nic_word(dev,INTA_MASK,0);
	force_pci_posting(dev);
//	priv->irq_enabled = 0;
}


void rtl8180_set_mode(struct net_device *dev,int mode)
{
	u8 ecmd;
	ecmd=read_nic_byte(dev, EPROM_CMD);
	ecmd=ecmd &~ EPROM_CMD_OPERATING_MODE_MASK;
	ecmd=ecmd | (mode<<EPROM_CMD_OPERATING_MODE_SHIFT);
	ecmd=ecmd &~ (1<<EPROM_CS_SHIFT);
	ecmd=ecmd &~ (1<<EPROM_CK_SHIFT);
	write_nic_byte(dev, EPROM_CMD, ecmd);
}


void rtl8180_update_msr(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	u8 msr;
	
	msr  = read_nic_byte(dev, MSR);
	msr &= ~ MSR_LINK_MASK;
	
	/* do not change in link_state != WLAN_LINK_ASSOCIATED.
	 * msr must be updated if the state is ASSOCIATING. 
	 * this is intentional and make sense for ad-hoc and
	 * master (see the create BSS/IBSS func)
	 */
	if (priv->ieee80211->state == IEEE80211_LINKED){ 
			
		if (priv->ieee80211->iw_mode == IW_MODE_INFRA)
			msr |= (MSR_LINK_MANAGED<<MSR_LINK_SHIFT);
		else if (priv->ieee80211->iw_mode == IW_MODE_ADHOC)
			msr |= (MSR_LINK_ADHOC<<MSR_LINK_SHIFT);
		else if (priv->ieee80211->iw_mode == IW_MODE_MASTER)
			msr |= (MSR_LINK_MASTER<<MSR_LINK_SHIFT);
		
	}else
		msr |= (MSR_LINK_NONE<<MSR_LINK_SHIFT);
		
	write_nic_byte(dev, MSR, msr);
}

void rtl8180_set_chan(struct net_device *dev,short ch)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	u32 tx;

	priv->chan=ch;
	#if 0
	if(priv->ieee80211->iw_mode == IW_MODE_ADHOC || 
		priv->ieee80211->iw_mode == IW_MODE_MASTER){
	
			priv->ieee80211->link_state = WLAN_LINK_ASSOCIATED;	
			priv->ieee80211->master_chan = ch;
			rtl8180_update_beacon_ch(dev); 
		}
	#endif
	
	/* this hack should avoid frame TX during channel setting*/
	tx = read_nic_dword(dev,TX_CONF);
	tx &= ~TX_LOOPBACK_MASK;

#ifndef LOOP_TEST	
	write_nic_dword(dev,TX_CONF, tx |( TX_LOOPBACK_MAC<<TX_LOOPBACK_SHIFT));
	priv->rf_set_chan(dev,priv->chan);
	//mdelay(10); //CPU occupany is too high. LZM 31/10/2008  
	write_nic_dword(dev,TX_CONF,tx | (TX_LOOPBACK_NONE<<TX_LOOPBACK_SHIFT));
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
void rtl8187_rx_isr(struct urb *rx_urb, struct pt_regs *regs);
#else
void rtl8187_rx_isr(struct urb* rx_urb);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
void rtl8187_rx_manage_isr(struct urb *rx_urb, struct pt_regs *regs);
#else
void rtl8187_rx_manage_isr(struct urb* rx_urb);
#endif



void rtl8187_rx_urbsubmit(struct net_device *dev, struct urb* rx_urb)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	int err;
	
	usb_fill_bulk_urb(rx_urb,priv->udev,
			usb_rcvbulkpipe(priv->udev,(NIC_8187 == priv->card_8187)?0x81:0x83), 
                	rx_urb->transfer_buffer,
                	RX_URB_SIZE,
                	rtl8187_rx_isr,
                	dev);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	err = usb_submit_urb(rx_urb, GFP_ATOMIC);	
#else
	err = usb_submit_urb(rx_urb);	
#endif
	if(err && err != -EPERM){
		DMESGE("cannot submit RX command. URB_STATUS %x",rx_urb->status);
	}
}


void rtl8187_rx_manage_urbsubmit(struct net_device *dev, struct urb* rx_urb)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	int err;
#ifdef THOMAS_BEACON
	usb_fill_bulk_urb(rx_urb,priv->udev,
			  usb_rcvbulkpipe(priv->udev,0x09), 
			  rx_urb->transfer_buffer,
			  rx_urb->transfer_buffer_length,
			  rtl8187_rx_manage_isr, dev);
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	err = usb_submit_urb(rx_urb, GFP_ATOMIC);	
#else
	err = usb_submit_urb(rx_urb);	
#endif
	if(err && err != -EPERM){
		DMESGE("cannot submit RX command. URB_STATUS %x",rx_urb->status);
	}
}



void rtl8187_rx_initiate(struct net_device *dev)
{
	int i;
	unsigned long flags;
        struct urb *purb;

        struct sk_buff *pskb;

        struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

        priv->tx_urb_index = 0;

        if ((!priv->rx_urb) || (!priv->pp_rxskb)) {

                DMESGE("Cannot intiate RX urb mechanism");
                return;

        }

        priv->rx_inx = 0;
#ifdef THOMAS_TASKLET
	atomic_set(&priv->irt_counter,0);
#endif
        for(i = 0;i < MAX_RX_URB; i++){

                purb = priv->rx_urb[i] = usb_alloc_urb(0,GFP_KERNEL);

                if(!priv->rx_urb[i])
                        goto destroy;

                pskb = priv->pp_rxskb[i] =  dev_alloc_skb (RX_URB_SIZE);

                if (pskb == NULL)
                        goto destroy;

                purb->transfer_buffer_length = RX_URB_SIZE;
                purb->transfer_buffer = pskb->data;
        }

	spin_lock_irqsave(&priv->irq_lock,flags);//added by thomas

        for(i=0;i<MAX_RX_URB;i++)
                rtl8187_rx_urbsubmit(dev,priv->rx_urb[i]);

	spin_unlock_irqrestore(&priv->irq_lock,flags);//added by thomas
        
	return;

destroy:

        for(i = 0; i < MAX_RX_URB; i++) {

                purb = priv->rx_urb[i];

                if (purb)
                        usb_free_urb(purb);

                pskb = priv->pp_rxskb[i];

                if (pskb)
                        dev_kfree_skb_any(pskb);

        }

        return;
}


void rtl8187_rx_manage_initiate(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	if(!priv->rx_urb)
		DMESGE("Cannot intiate RX urb mechanism");

	rtl8187_rx_manage_urbsubmit(dev,priv->rx_urb[MAX_RX_URB]);
		
}


void rtl8187_set_rxconf(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	u32 rxconf;
	
	rxconf=read_nic_dword(dev,RX_CONF);
	rxconf = rxconf &~ MAC_FILTER_MASK;
	rxconf = rxconf | (1<<ACCEPT_MNG_FRAME_SHIFT);
	rxconf = rxconf | (1<<ACCEPT_DATA_FRAME_SHIFT);
	rxconf = rxconf | (1<<ACCEPT_BCAST_FRAME_SHIFT);
	rxconf = rxconf | (1<<ACCEPT_MCAST_FRAME_SHIFT);
	//rxconf = rxconf | (1<<ACCEPT_CTL_FRAME_SHIFT);	
#ifdef SW_ANTE_DIVERSITY
	rxconf = rxconf | priv->EEPROMCSMethod;//for antenna
#endif

	if (dev->flags & IFF_PROMISC) DMESG ("NIC in promisc mode");
	
	if(priv->ieee80211->iw_mode == IW_MODE_MONITOR || \
	   dev->flags & IFF_PROMISC){
		rxconf = rxconf | (1<<ACCEPT_ALLMAC_FRAME_SHIFT);
	} /*else if(priv->ieee80211->iw_mode == IW_MODE_MASTER){
		rxconf = rxconf | (1<<ACCEPT_ALLMAC_FRAME_SHIFT);
		rxconf = rxconf | (1<<RX_CHECK_BSSID_SHIFT);
	}*/else{
		rxconf = rxconf | (1<<ACCEPT_NICMAC_FRAME_SHIFT);
		rxconf = rxconf | (1<<RX_CHECK_BSSID_SHIFT);
	}
	
	
	if(priv->ieee80211->iw_mode == IW_MODE_MONITOR){
		rxconf = rxconf | (1<<ACCEPT_ICVERR_FRAME_SHIFT);
		rxconf = rxconf | (1<<ACCEPT_PWR_FRAME_SHIFT);
	}
	
	if( priv->crcmon == 1 && priv->ieee80211->iw_mode == IW_MODE_MONITOR)
		rxconf = rxconf | (1<<ACCEPT_CRCERR_FRAME_SHIFT);
	
	
	rxconf = rxconf &~ RX_FIFO_THRESHOLD_MASK;
	rxconf = rxconf | (RX_FIFO_THRESHOLD_NONE<<RX_FIFO_THRESHOLD_SHIFT);
	rxconf = rxconf &~ MAX_RX_DMA_MASK;
	rxconf = rxconf | (MAX_RX_DMA_2048<<MAX_RX_DMA_SHIFT);

	rxconf = rxconf | (1<<RX_AUTORESETPHY_SHIFT);
	rxconf = rxconf | RCR_ONLYERLPKT;
	
	//rxconf = rxconf &~ RCR_CS_MASK;
	//rxconf = rxconf | (1<<RCR_CS_SHIFT);

	write_nic_dword(dev, RX_CONF, rxconf);	
	
	#ifdef DEBUG_RX
	DMESG("rxconf: %x %x",rxconf ,read_nic_dword(dev,RX_CONF));
	#endif
}

void rtl8180_rx_enable(struct net_device *dev)
{
	u8 cmd;

	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	rtl8187_rx_initiate(dev);
	rtl8187_set_rxconf(dev);	

	if(NIC_8187 == priv->card_8187) {
		cmd=read_nic_byte(dev,CMD);
		write_nic_byte(dev,CMD,cmd | (1<<CMD_RX_ENABLE_SHIFT));
	} else {
		//write_nic_dword(dev, RCR, priv->ReceiveConfig);
	}
}


void rtl8180_tx_enable(struct net_device *dev)
{
	u8 cmd;
	u8 byte;
	u32 txconf = 0;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	if(NIC_8187B == priv->card_8187){
		write_nic_dword(dev, TCR, priv->TransmitConfig);	
		byte = read_nic_byte(dev, MSR);
		byte |= MSR_LINK_ENEDCA;
		write_nic_byte(dev, MSR, byte);
#ifdef LOOP_TEST
		txconf= read_nic_dword(dev,TX_CONF);
		txconf = txconf | (TX_LOOPBACK_MAC<<TX_LOOPBACK_SHIFT);
		write_nic_dword(dev,TX_CONF,txconf);
#endif
	} else {
		byte = read_nic_byte(dev,CW_CONF);
		byte &= ~(1<<CW_CONF_PERPACKET_CW_SHIFT);
		byte &= ~(1<<CW_CONF_PERPACKET_RETRY_SHIFT);
		write_nic_byte(dev, CW_CONF, byte);

		byte = read_nic_byte(dev, TX_AGC_CTL);
		byte &= ~(1<<TX_AGC_CTL_PERPACKET_GAIN_SHIFT);
		byte &= ~(1<<TX_AGC_CTL_PERPACKET_ANTSEL_SHIFT);
		byte &= ~(1<<TX_AGC_CTL_FEEDBACK_ANT);
		write_nic_byte(dev, TX_AGC_CTL, byte);

		txconf= read_nic_dword(dev,TX_CONF);


		txconf = txconf &~ TX_LOOPBACK_MASK;

#ifndef LOOP_TEST
		txconf = txconf | (TX_LOOPBACK_NONE<<TX_LOOPBACK_SHIFT);
#else
		txconf = txconf | (TX_LOOPBACK_BASEBAND<<TX_LOOPBACK_SHIFT);
#endif
		txconf = txconf &~ TCR_SRL_MASK;
		txconf = txconf &~ TCR_LRL_MASK;

		txconf = txconf | (priv->retry_data<<TX_LRLRETRY_SHIFT); // long
		txconf = txconf | (priv->retry_rts<<TX_SRLRETRY_SHIFT); // short

		txconf = txconf &~ (1<<TX_NOCRC_SHIFT);

		txconf = txconf &~ TCR_MXDMA_MASK;
		txconf = txconf | (TCR_MXDMA_2048<<TCR_MXDMA_SHIFT);

		txconf = txconf | TCR_DISReqQsize;
		txconf = txconf | TCR_DISCW;
		txconf = txconf &~ TCR_SWPLCPLEN;

		txconf=txconf | (1<<TX_NOICV_SHIFT);

		write_nic_dword(dev,TX_CONF,txconf);

#ifdef DEBUG_TX
		DMESG("txconf: %x %x",txconf,read_nic_dword(dev,TX_CONF));
#endif

		cmd=read_nic_byte(dev,CMD);
		write_nic_byte(dev,CMD,cmd | (1<<CMD_TX_ENABLE_SHIFT));		
	}
}

#if 0
void rtl8180_beacon_tx_enable(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	priv->dma_poll_mask &=~(1<<TX_DMA_STOP_BEACON_SHIFT);
	rtl8180_set_mode(dev,EPROM_CMD_CONFIG);
	write_nic_byte(dev,TX_DMA_POLLING,priv->dma_poll_mask);	
	rtl8180_set_mode(dev,EPROM_CMD_NORMAL);
}


void rtl8180_
_disable(struct net_device *dev) 
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	priv->dma_poll_mask |= (1<<TX_DMA_STOP_BEACON_SHIFT);
	rtl8180_set_mode(dev,EPROM_CMD_CONFIG);
	write_nic_byte(dev,TX_DMA_POLLING,priv->dma_poll_mask);
	rtl8180_set_mode(dev,EPROM_CMD_NORMAL);
}

#endif


void rtl8180_rtx_disable(struct net_device *dev)
{
	u8 cmd;
	int i;
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	cmd=read_nic_byte(dev,CMD);
	write_nic_byte(dev, CMD, cmd &~ \
		       ((1<<CMD_RX_ENABLE_SHIFT)|(1<<CMD_TX_ENABLE_SHIFT)));
	force_pci_posting(dev);
	mdelay(10);

#ifdef THOMAS_BEACON
	{
		int index = priv->rx_inx;//0
		i=0;
		if(priv->rx_urb){
			while(i<MAX_RX_URB){
				if(priv->rx_urb[index]){
					usb_kill_urb(priv->rx_urb[index]);
				}
				if( index == (MAX_RX_URB-1) )
					index=0;
				else
					index=index+1;
				i++;
			}
			if(priv->rx_urb[MAX_RX_URB])
				usb_kill_urb(priv->rx_urb[MAX_RX_URB]);
		}
	}
#endif
}


int alloc_tx_beacon_desc_ring(struct net_device *dev, int count)
{
	#if 0
	int i;
	u32 *tmp;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	
	priv->txbeaconring = (u32*)pci_alloc_consistent(priv->pdev,
					  sizeof(u32)*8*count, 
					  &priv->txbeaconringdma);
	if (!priv->txbeaconring) return -1;
	for (tmp=priv->txbeaconring,i=0;i<count;i++){
		*tmp = *tmp &~ (1<<31); // descriptor empty, owned by the drv 
		/*
		*(tmp+2) = (u32)dma_tmp;
		*(tmp+3) = bufsize;
		*/
		if(i+1<count)
			*(tmp+4) = (u32)priv->txbeaconringdma+((i+1)*8*4);
		else
			*(tmp+4) = (u32)priv->txbeaconringdma;
		
		tmp=tmp+8;
	}
	#endif
	return 0;
}

long NetgearSignalStrengthTranslate(long LastSS,long CurrSS)
{
    long RetSS;

        // Step 1. Scale mapping. 
        if(CurrSS >= 71 && CurrSS <= 100){
                RetSS = 90 + ((CurrSS - 70) / 3);
        }else if(CurrSS >= 41 && CurrSS <= 70){
                RetSS = 78 + ((CurrSS - 40) / 3);
        }else if(CurrSS >= 31 && CurrSS <= 40){
                RetSS = 66 + (CurrSS - 30);
        }else if(CurrSS >= 21 && CurrSS <= 30){
                RetSS = 54 + (CurrSS - 20);
        }else if(CurrSS >= 5 && CurrSS <= 20){
                RetSS = 42 + (((CurrSS - 5) * 2) / 3);
        }else if(CurrSS == 4){
                RetSS = 36;
        }else if(CurrSS == 3){
                RetSS = 27;
        }else if(CurrSS == 2){
                RetSS = 18;
        }else if(CurrSS == 1){
                RetSS = 9;
        }else{
                RetSS = CurrSS;
        }
        //RT_TRACE(COMP_DBG, DBG_LOUD, ("##### After Mapping:  LastSS: %d, CurrSS: %d, RetSS: %d\n", LastSS, CurrSS, RetSS));

        // Step 2. Smoothing.
        if(LastSS > 0){
                RetSS = ((LastSS * 5) + (RetSS)+ 5) / 6;
        }
        //RT_TRACE(COMP_DBG, DBG_LOUD, ("$$$$$ After Smoothing:  LastSS: %d, CurrSS: %d, RetSS: %d\n", LastSS, CurrSS, RetSS));

        return RetSS;
}

extern long TranslateToDbm8187(u8 SignalStrengthIndex);  // 0-100 index.
//long TranslateToDbm8187(u8 SignalStrengthIndex)  // 0-100 index.
//{
 //       long    SignalPower; // in dBm.

        // Translate to dBm (x=0.5y-95).
   //     SignalPower = (long)((SignalStrengthIndex + 1) >> 1);
     //   SignalPower -= 95;

       // return SignalPower;
//}


void rtl8180_reset(struct net_device *dev)
{
	
	struct r8180_priv *priv = ieee80211_priv(dev);
	u8 cr;
	int i;


	/* make sure the analog power is on before
	 * reset, otherwise reset may fail
	 */
	if(NIC_8187 == priv->card_8187) {
		rtl8180_set_anaparam(dev, RTL8225_ANAPARAM_ON);
		rtl8185_set_anaparam2(dev, RTL8225_ANAPARAM2_ON);
		rtl8180_irq_disable(dev);
		mdelay(200);
		write_nic_byte_E(dev,0x18,0x10);
		write_nic_byte_E(dev,0x18,0x11);
		write_nic_byte_E(dev,0x18,0x00);
		mdelay(200);
	}

	
	cr=read_nic_byte(dev,CMD);
	cr = cr & 2;
	cr = cr | (1<<CMD_RST_SHIFT);
	write_nic_byte(dev,CMD,cr);

	//lzm mod for up take too long time 20081201
	//force_pci_posting(dev);	
	//mdelay(200);
	udelay(20);
	
	if(read_nic_byte(dev,CMD) & (1<<CMD_RST_SHIFT)) 
		DMESGW("Card reset timeout!");
	
	if(NIC_8187 == priv->card_8187) {

		//printk("This is RTL8187 Reset procedure\n");
		rtl8180_set_mode(dev,EPROM_CMD_LOAD);
		force_pci_posting(dev);
		mdelay(200);

		/* after the eeprom load cycle, make sure we have
		 * correct anaparams
		 */
		rtl8180_set_anaparam(dev, RTL8225_ANAPARAM_ON);
		rtl8185_set_anaparam2(dev, RTL8225_ANAPARAM2_ON);
	}
	else {
		//printk("This is RTL8187B Reset procedure\n");
		//test pending bug, john 20070815 
		//initialize tx_pending
        	for(i=0;i<0x10;i++)  atomic_set(&(priv->tx_pending[i]), 0);
 		
	}

}

inline u16 ieeerate2rtlrate(int rate)
{
	switch(rate){
	case 10:	
	return 0;
	case 20:
	return 1;
	case 55:
	return 2;
	case 110:
	return 3;
	case 60:
	return 4;
	case 90:
	return 5;
	case 120:
	return 6;
	case 180:
	return 7;
	case 240:
	return 8;
	case 360:
	return 9;
	case 480:
	return 10;
	case 540:
	return 11;
	default:
	return 3;
	
	}
}
static u16 rtl_rate[] = {10,20,55,110,60,90,120,180,240,360,480,540,720};
inline u16 rtl8180_rate2rate(short rate)
{
	if (rate >12) return 10;
	return rtl_rate[rate]; 
}
		
void rtl8180_irq_rx_tasklet(struct r8180_priv *priv);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
void rtl8187_rx_isr(struct urb *rx_urb, struct pt_regs *regs)
#else
void rtl8187_rx_isr(struct urb* rx_urb)
#endif
{
	struct net_device *dev = (struct net_device*)rx_urb->context;
	struct r8180_priv *priv = ieee80211_priv(dev);
	priv->rxurb_task = rx_urb;
	
	
	//DMESGW("David: Rx tasklet start!");

#ifdef THOMAS_TASKLET
	atomic_inc( &priv->irt_counter );	

	//if( likely(priv->irt_counter_head+1 != priv->irt_counter_tail) ){
	//	priv->irt_counter_head = (priv->irt_counter_head+1)&0xffff ;
		tasklet_schedule(&priv->irq_rx_tasklet);
	//} else{
		//DMESG("error: priv->irt_counter_head is going to pass through priv->irt_counter_tail\n");
		/*
		skb = priv->pp_rxskb[priv->rx_inx];
		dev_kfree_skb_any(skb);

		skb = dev_alloc_skb(RX_URB_SIZE);
		if (skb == NULL)
			panic("No Skb For RX!/n");

		rx_urb->transfer_buffer = skb->data;

		priv->pp_rxskb[priv->rx_inx] = skb;
		if(status == 0)
			rtl8187_rx_urbsubmit(dev,rx_urb);
		else {
			priv->pp_rxskb[priv->rx_inx] = NULL;
			dev_kfree_skb_any(skb);
			printk("RX process aborted due to explicit shutdown (%x) ", status);
		}

	       if (*prx_inx == (MAX_RX_URB -1))
			*prx_inx = 0;
	       else
			*prx_inx = *prx_inx + 1;

		*/
	//}
#endif

}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
void rtl8187_rx_manage_isr(struct urb *rx_urb, struct pt_regs *regs)
#else
void rtl8187_rx_manage_isr(struct urb* rx_urb)
#endif
{
	struct net_device *dev = (struct net_device*)rx_urb->context;
	struct r8180_priv *priv = ieee80211_priv(dev);
	int status,cmd;
	struct sk_buff *skb;
	u32 *desc;
	int ret;
	unsigned long flag;

	//DMESG("RX %d ",rx_urb->status);
	status = rx_urb->status;
	if(status == 0){
		
		desc = (u32*)(rx_urb->transfer_buffer);
		cmd = (desc[0] >> 30) & 0x03;
		//printk(KERN_ALERT "buffersize = %d, length = %d, pipe = %p\n", 
		//rx_urb->transfer_buffer_length, rx_urb->actual_length, rx_urb->pipe>>15);				

		if(cmd == 0x00) {//beacon interrupt
			//send beacon packet

                        spin_lock_irqsave(&priv->ieee80211->beaconflag_lock,flag);
                        if(priv->flag_beacon == true){
	                //printk("rtl8187_rx_manage_isr(): CMD_TYPE0_BCN_INTR\n");

			skb = ieee80211_get_beacon(priv->ieee80211);
			if(!skb){ 
				DMESG("not enought memory for allocating beacon");
				return;
			}
			//printk(KERN_WARNING "to send beacon packet through beacon endpoint!\n");
			ret = rtl8180_tx(dev, (u32*)skb->data, skb->len, BEACON_PRIORITY,
					0, ieeerate2rtlrate(priv->ieee80211->basic_rate));
			
			if( ret != 0 ){
				printk(KERN_ALERT "tx beacon packet error : %d !\n", ret);
			}	
			dev_kfree_skb_any(skb);

                 //} else {//0x00
                        //{ log the device information
                        // At present, It is not implemented just now.
                        //}
                //}

                }
                spin_unlock_irqrestore(&priv->ieee80211->beaconflag_lock,flag);
                }
                else if(cmd == 0x01){
                        //printk("rtl8187_rx_manage_isr(): CMD_TYPE1_TX_CLOSE\n");
                        priv->CurrRetryCnt += (u16)desc[0]&0x000000ff;
                       //printk("priv->CurrRetryCnt is %d\n",priv->CurrRetryCnt);
                }
                else
                        printk("HalUsbInCommandComplete8187B(): unknown Type(%#X) !!!\n", cmd);

	}else{
		priv->stats.rxstaterr++;
		priv->ieee80211->stats.rx_errors++;
	}

	
	if( status == 0 )
	//if(status != -ENOENT)
		rtl8187_rx_manage_urbsubmit(dev, rx_urb);
	else 
		;//DMESG("Mangement RX process aborted due to explicit shutdown");
}

#if 0
void rtl8180_tx_queues_stop(struct net_device *dev)
{
	//struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	u8 dma_poll_mask = (1<<TX_DMA_STOP_LOWPRIORITY_SHIFT);
	dma_poll_mask |= (1<<TX_DMA_STOP_HIPRIORITY_SHIFT);
	dma_poll_mask |= (1<<TX_DMA_STOP_NORMPRIORITY_SHIFT);
	dma_poll_mask |= (1<<TX_DMA_STOP_BEACON_SHIFT);

	rtl8180_set_mode(dev,EPROM_CMD_CONFIG);
	write_nic_byte(dev,TX_DMA_POLLING,dma_poll_mask);
	rtl8180_set_mode(dev,EPROM_CMD_NORMAL);
}
#endif

void rtl8180_data_hard_stop(struct net_device *dev)
{
	//FIXME !!
	#if 0
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	priv->dma_poll_mask |= (1<<TX_DMA_STOP_LOWPRIORITY_SHIFT);
	rtl8180_set_mode(dev,EPROM_CMD_CONFIG);
	write_nic_byte(dev,TX_DMA_POLLING,priv->dma_poll_mask);
	rtl8180_set_mode(dev,EPROM_CMD_NORMAL);
	#endif
}


void rtl8180_data_hard_resume(struct net_device *dev)
{
	// FIXME !!
	#if 0
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	priv->dma_poll_mask &= ~(1<<TX_DMA_STOP_LOWPRIORITY_SHIFT);
	rtl8180_set_mode(dev,EPROM_CMD_CONFIG);
	write_nic_byte(dev,TX_DMA_POLLING,priv->dma_poll_mask);
	rtl8180_set_mode(dev,EPROM_CMD_NORMAL);
	#endif
}

unsigned int PRI2EP[4] = {0x06,0x07,0x05,0x04};
// this function TX data frames when the ieee80211 stack requires this.
// It checks also if we need to stop the ieee tx queue, eventually do it
void rtl8180_hard_data_xmit(struct sk_buff *skb, struct net_device *dev, int rate)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	
	short morefrag = 0;	
	unsigned long flags;
	struct ieee80211_hdr *h = (struct ieee80211_hdr  *) skb->data;

	unsigned char ep;
	short ret;	//john

	if (le16_to_cpu(h->frame_ctl) & IEEE80211_FCTL_MOREFRAGS)
		morefrag = 1;
	//DMESG("%x %x", h->frame_ctl, h->seq_ctl);
	/*
	* This function doesn't require lock because we make
	* sure it's called with the tx_lock already acquired.
	* this come from the kernel's hard_xmit callback (trought
	* the ieee stack, or from the try_wake_queue (again trought
	* the ieee stack.
	*/
	spin_lock_irqsave(&priv->tx_lock,flags);	

	//lzm mod 20081128 for sometimes wlan down but it still have some pkt to tx
	if((priv->ieee80211->bHwRadioOff)||(!priv->up))
	{
		spin_unlock_irqrestore(&priv->tx_lock,flags);	
	
		return;	
	}	

	if(NIC_8187B == priv->card_8187){
		ep = PRI2EP[skb->priority];
	} else {
		ep = LOW_PRIORITY;
	}
	//if (!check_nic_enought_desc(dev, PRI2EP[skb->priority])){
	if (!check_nic_enought_desc(dev, ep)){
		DMESG("Error: no TX slot ");
		ieee80211_stop_queue(priv->ieee80211);
	}

#ifdef LED_SHIN
	priv->ieee80211->ieee80211_led_contorl(dev,LED_CTL_TX); 
#endif

	ret =	rtl8180_tx(dev, (u32*)skb->data, skb->len, ep, morefrag,ieeerate2rtlrate(rate));
	if(ret!=0) DMESG("Error: rtl8180_tx failed in rtl8180_hard_data_xmit\n");//john

	priv->stats.txdatapkt++;
	
	//if (!check_nic_enought_desc(dev, PRI2EP[skb->priority])){
	if (!check_nic_enought_desc(dev, ep)){
		ieee80211_stop_queue(priv->ieee80211);
	}
		
	spin_unlock_irqrestore(&priv->tx_lock,flags);	
			
}

//This is a rough attempt to TX a frame
//This is called by the ieee 80211 stack to TX management frames.
//If the ring is full packet are dropped (for data frame the queue
//is stopped before this can happen).

int rtl8180_hard_start_xmit(struct sk_buff *skb,struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee80211;
	int ret;
	unsigned long flags;
	spin_lock_irqsave(&priv->tx_lock,flags);

	//lzm mod 20081128 for sometimes wlan down but it still have some pkt to tx
	if((priv->ieee80211->bHwRadioOff)||(!priv->up))
	{
		spin_unlock_irqrestore(&priv->tx_lock,flags);	
		return 0;	
	}
	
	ret = rtl8180_tx(dev, (u32*)skb->data, skb->len, MANAGE_PRIORITY, 0, ieeerate2rtlrate(ieee->basic_rate));

	priv->ieee80211->stats.tx_bytes+=skb->len;
	priv->ieee80211->stats.tx_packets++;
	
	spin_unlock_irqrestore(&priv->tx_lock,flags);	
	
	return ret;
}


#if 0
// longpre 144+48 shortpre 72+24
u16 rtl8180_len2duration(u32 len, short rate,short* ext)
{
	u16 duration;
	u16 drift;
	*ext=0;
	
	switch(rate){
	case 0://1mbps
		*ext=0;
		duration = ((len+4)<<4) /0x2;
		drift = ((len+4)<<4) % 0x2;
		if(drift ==0 ) break;
		duration++;
		break;
		
	case 1://2mbps
		*ext=0;
		duration = ((len+4)<<4) /0x4;
		drift = ((len+4)<<4) % 0x4;
		if(drift ==0 ) break;
		duration++;
		break;
		
	case 2: //5.5mbps
		*ext=0;
		duration = ((len+4)<<4) /0xb;
		drift = ((len+4)<<4) % 0xb;
		if(drift ==0 ) 
			break;
		duration++;
		break;
		
	default:
	case 3://11mbps				
		*ext=0;
		duration = ((len+4)<<4) /0x16;
		drift = ((len+4)<<4) % 0x16;
		if(drift ==0 ) 
			break;
		duration++;
		if(drift > 6) 
			break;
		*ext=1;
		break;
	}
	
	return duration;
}
#endif

void rtl8180_try_wake_queue(struct net_device *dev, int pri);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
void rtl8187_lptx_isr(struct urb *tx_urb, struct pt_regs *regs)
#else
void rtl8187_lptx_isr(struct urb* tx_urb)
#endif
{
	struct net_device *dev = (struct net_device*)tx_urb->context;
	struct r8180_priv *priv = ieee80211_priv(dev);

	if(tx_urb->status == 0){
		dev->trans_start = jiffies;	//john
		priv->stats.txlpokint++;
		priv->txokbytestotal+=tx_urb->actual_length;
	}else{
		priv->stats.txlperr++;
	}

	kfree(tx_urb->transfer_buffer);
	usb_free_urb(tx_urb);

	if(atomic_read(&priv->tx_pending[LOW_PRIORITY]) >= 1)
		atomic_dec(&priv->tx_pending[LOW_PRIORITY]);

	rtl8180_try_wake_queue(dev,LOW_PRIORITY);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
void rtl8187_nptx_isr(struct urb *tx_urb, struct pt_regs *regs)
#else
void rtl8187_nptx_isr(struct urb* tx_urb)
#endif
{
	struct net_device *dev = (struct net_device*)tx_urb->context;
	struct r8180_priv *priv = ieee80211_priv(dev);

	if(tx_urb->status == 0){
		dev->trans_start = jiffies;	     //john
		priv->stats.txnpokint++;
	}else{
		priv->stats.txnperr++;
	}

	kfree(tx_urb->transfer_buffer);
	usb_free_urb(tx_urb);

	if(atomic_read(&priv->tx_pending[NORM_PRIORITY]) >= 1)
		atomic_dec(&priv->tx_pending[NORM_PRIORITY]);
	//rtl8180_try_wake_queue(dev,NORM_PRIORITY);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
void rtl8187_votx_isr(struct urb *tx_urb, struct pt_regs *regs)
#else
void rtl8187_votx_isr(struct urb* tx_urb)
#endif
{
	struct net_device *dev = (struct net_device*)tx_urb->context;
	struct r8180_priv *priv = ieee80211_priv(dev);

	if(tx_urb->status == 0){
		dev->trans_start = jiffies;	     //john
		priv->stats.txvookint++;
		priv->txokbytestotal+=tx_urb->actual_length;
	}else{
		priv->stats.txvoerr++;
	}

	kfree(tx_urb->transfer_buffer);
	usb_free_urb(tx_urb);

	if(atomic_read(&priv->tx_pending[VO_PRIORITY]) >= 1)
		atomic_dec(&priv->tx_pending[VO_PRIORITY]);
	rtl8180_try_wake_queue(dev,VO_PRIORITY);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
void rtl8187_vitx_isr(struct urb *tx_urb, struct pt_regs *regs)
#else
void rtl8187_vitx_isr(struct urb* tx_urb)
#endif
{
	struct net_device *dev = (struct net_device*)tx_urb->context;
	struct r8180_priv *priv = ieee80211_priv(dev);

	if(tx_urb->status == 0){
		dev->trans_start = jiffies;	     //john
		priv->stats.txviokint++;
		priv->txokbytestotal+=tx_urb->actual_length;
	}else{
		priv->stats.txvierr++;
	}

	kfree(tx_urb->transfer_buffer);
	usb_free_urb(tx_urb);

	if(atomic_read(&priv->tx_pending[VI_PRIORITY]) >= 1)
		atomic_dec(&priv->tx_pending[VI_PRIORITY]);
	rtl8180_try_wake_queue(dev,VI_PRIORITY);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
void rtl8187_betx_isr(struct urb *tx_urb, struct pt_regs *regs)
#else
void rtl8187_betx_isr(struct urb* tx_urb)
#endif
{
	struct net_device *dev = (struct net_device*)tx_urb->context;
	struct r8180_priv *priv = ieee80211_priv(dev);

	if(tx_urb->status == 0){
		dev->trans_start = jiffies;	     //john
		priv->stats.txbeokint++;
		priv->txokbytestotal+=tx_urb->actual_length;
	}else{
		priv->stats.txbeerr++;
	}

	kfree(tx_urb->transfer_buffer);
	usb_free_urb(tx_urb);

	if(atomic_read(&priv->tx_pending[BE_PRIORITY]) >= 1)
		atomic_dec(&priv->tx_pending[BE_PRIORITY]);
	rtl8180_try_wake_queue(dev, BE_PRIORITY);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
void rtl8187_bktx_isr(struct urb *tx_urb, struct pt_regs *regs)
#else
void rtl8187_bktx_isr(struct urb* tx_urb)
#endif
{
	struct net_device *dev = (struct net_device*)tx_urb->context;
	struct r8180_priv *priv = ieee80211_priv(dev);

	if(tx_urb->status == 0){
		dev->trans_start = jiffies;	     //john
		priv->stats.txbkokint++;
	}else{
		priv->stats.txbkerr++;
	}

	kfree(tx_urb->transfer_buffer);
	usb_free_urb(tx_urb);

	if(atomic_read(&priv->tx_pending[BK_PRIORITY]) >= 1)
		atomic_dec(&priv->tx_pending[BK_PRIORITY]);
	rtl8180_try_wake_queue(dev,BK_PRIORITY);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
void rtl8187_beacontx_isr(struct urb *tx_urb, struct pt_regs *regs)
#else
void rtl8187_beacontx_isr(struct urb* tx_urb)
#endif
{
	struct net_device *dev = (struct net_device*)tx_urb->context;
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	if(tx_urb->status == 0){
		dev->trans_start = jiffies;	     //john
		priv->stats.txbeaconokint++;
		priv->txokbytestotal+=tx_urb->actual_length;
	}else{
		priv->stats.txbeaconerr++;
	}

	kfree(tx_urb->transfer_buffer);
	usb_free_urb(tx_urb);

	if(atomic_read(&priv->tx_pending[BEACON_PRIORITY]) >= 1)
		atomic_dec(&priv->tx_pending[BEACON_PRIORITY]);
	//rtl8180_try_wake_queue(dev,BEACON_PRIORITY);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
void rtl8187_managetx_isr(struct urb *tx_urb, struct pt_regs *regs)
#else
void rtl8187_managetx_isr(struct urb* tx_urb)
#endif
{
	struct net_device *dev = (struct net_device*)tx_urb->context;
	struct r8180_priv *priv = ieee80211_priv(dev);

	if(tx_urb->status == 0){
		dev->trans_start = jiffies;	     //john
		priv->stats.txmanageokint++;
		priv->txokbytestotal+=tx_urb->actual_length;
	}else{
		priv->stats.txmanageerr++;
	}

	kfree(tx_urb->transfer_buffer);
	usb_free_urb(tx_urb);

	if(atomic_read(&priv->tx_pending[MANAGE_PRIORITY]) >= 1)
		atomic_dec(&priv->tx_pending[MANAGE_PRIORITY]);
//	rtl8180_try_wake_queue(dev,MANAGE_PRIORITY);
}

void rtl8187_beacon_stop(struct net_device *dev)
{
	u8 msr, msrm, msr2;
	struct r8180_priv *priv = ieee80211_priv(dev);
	unsigned long flag;
	msr  = read_nic_byte(dev, MSR);
	msrm = msr & MSR_LINK_MASK;
	msr2 = msr & ~MSR_LINK_MASK;
	if(NIC_8187B == priv->card_8187) {
		spin_lock_irqsave(&priv->ieee80211->beaconflag_lock,flag);
        	priv->flag_beacon = false;
        	spin_unlock_irqrestore(&priv->ieee80211->beaconflag_lock,flag);
	}
	if ((msrm == (MSR_LINK_ADHOC<<MSR_LINK_SHIFT) ||
		(msrm == (MSR_LINK_MASTER<<MSR_LINK_SHIFT)))){
		write_nic_byte(dev, MSR, msr2 | MSR_LINK_NONE);
		write_nic_byte(dev, MSR, msr);	
	}
}


void rtl8187_net_update(struct net_device *dev)
{

	struct r8180_priv *priv = ieee80211_priv(dev);
	struct ieee80211_network *net;
	net = & priv->ieee80211->current_network;
	
	
	write_nic_dword(dev,BSSID,((u32*)net->bssid)[0]);
	write_nic_word(dev,BSSID+4,((u16*)net->bssid)[2]);

	rtl8180_update_msr(dev);
		
	//rtl8180_set_mode(dev,EPROM_CMD_CONFIG);
	write_nic_word(dev, AtimWnd, 2);
	write_nic_word(dev, AtimtrItv, 100);	
	write_nic_word(dev, BEACON_INTERVAL, net->beacon_interval);
	//write_nic_word(dev, BcnIntTime, 100);
	write_nic_word(dev, BcnIntTime, 0x3FF);
	

}

void rtl8187_beacon_tx(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	struct sk_buff *skb;
	int i = 0;
	u8 cr;
	unsigned long flag;
	rtl8187_net_update(dev);

	if(NIC_8187B == priv->card_8187) {
		//Cause TSF timer of MAC reset to 0
		cr=read_nic_byte(dev,CMD);
		cr = cr | (1<<CMD_RST_SHIFT);
		write_nic_byte(dev,CMD,cr);

		//lzm mod 20081201
		//mdelay(200);
		mdelay(20);

		if(read_nic_byte(dev,CMD) & (1<<CMD_RST_SHIFT)) 
			DMESGW("Card reset timeout for ad-hoc!");
		else 
			DMESG("Card successfully reset for ad-hoc");

		write_nic_byte(dev,CMD, (read_nic_byte(dev,CMD)|CR_RE|CR_TE));
		spin_lock_irqsave(&priv->ieee80211->beaconflag_lock,flag);
                priv->flag_beacon = true;
                spin_unlock_irqrestore(&priv->ieee80211->beaconflag_lock,flag);

		//rtl8187_rx_manage_initiate(dev);		
	} else {
		printk(KERN_WARNING "get the beacon!\n");
		skb = ieee80211_get_beacon(priv->ieee80211);
		if(!skb){ 
			DMESG("not enought memory for allocating beacon");
			return;
		}

		write_nic_byte(dev, BQREQ, read_nic_byte(dev, BQREQ) | (1<<7));

		i=0;
		//while(!read_nic_byte(dev,BQREQ & (1<<7)))
		while( (read_nic_byte(dev, BQREQ) & (1<<7)) == 0 )
		{
			msleep_interruptible_rtl(HZ/2);
			if(i++ > 10){
				DMESGW("get stuck to wait HW beacon to be ready");
				return ;
			}
		}
		//tx 	
		rtl8180_tx(dev, (u32*)skb->data, skb->len, NORM_PRIORITY,
				0, ieeerate2rtlrate(priv->ieee80211->basic_rate));
		if(skb)
			dev_kfree_skb_any(skb);
	}
}

#if 0
void rtl8187_nptx_isr(struct urb *tx_urb, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device*)tx_urb->context;
	struct r8180_priv *priv = ieee80211_priv(dev);

	if(tx_urb->status == 0)
		priv->stats.txnpokint++;
	else
		priv->stats.txnperr++;
	kfree(tx_urb->transfer_buffer);
	usb_free_urb(tx_urb);
	atomic_dec(&priv->tx_np_pending);
	//rtl8180_try_wake_queue(dev,NORM_PRIORITY);
}
#endif
inline u8 rtl8180_IsWirelessBMode(u16 rate)
{
	if( ((rate <= 110) && (rate != 60) && (rate != 90)) || (rate == 220) )
		return 1;
	else return 0;
}

u16 N_DBPSOfRate(u16 DataRate);

u16 ComputeTxTime( 
		u16		FrameLength,
		u16		DataRate,
		u8		bManagementFrame,
		u8		bShortPreamble
		)
{
	u16	FrameTime;
	u16	N_DBPS;
	u16	Ceiling;

	if( rtl8180_IsWirelessBMode(DataRate) )
	{
		if( bManagementFrame || !bShortPreamble || DataRate == 10 ){	// long preamble
			FrameTime = (u16)(144+48+(FrameLength*8/(DataRate/10)));		
		}else{	// Short preamble
			FrameTime = (u16)(72+24+(FrameLength*8/(DataRate/10)));
		}
		if( ( FrameLength*8 % (DataRate/10) ) != 0 ) //Get the Ceilling
				FrameTime ++;
	} else {	//802.11g DSSS-OFDM PLCP length field calculation.
		N_DBPS = N_DBPSOfRate(DataRate);
		Ceiling = (16 + 8*FrameLength + 6) / N_DBPS 
				+ (((16 + 8*FrameLength + 6) % N_DBPS) ? 1 : 0);
		FrameTime = (u16)(16 + 4 + 4*Ceiling + 6);
	}
	return FrameTime;
}

u16 N_DBPSOfRate(u16 DataRate)
{
	 u16 N_DBPS = 24;
	 
	 switch(DataRate)
	 {
	 case 60:
	  N_DBPS = 24;
	  break;
	 
	 case 90:
	  N_DBPS = 36;
	  break;
	 
	 case 120:
	  N_DBPS = 48;
	  break;
	 
	 case 180:
	  N_DBPS = 72;
	  break;
	 
	 case 240:
	  N_DBPS = 96;
	  break;
	 
	 case 360:
	  N_DBPS = 144;
	  break;
	 
	 case 480:
	  N_DBPS = 192;
	  break;
	 
	 case 540:
	  N_DBPS = 216;
	  break;
	 
	 default:
	  break;
	 }
	 
	 return N_DBPS;
}
// NOte!!!
// the rate filled in is the rtl_rate.
// while the priv->ieee80211->basic_rate,used in the following code is ieee80211 rate.

#ifdef JUST_FOR_87SEMESH
#define ActionHeadLen 30
#endif
#define sCrcLng         4	
#define sAckCtsLng	112		// bits in ACK and CTS frames
short rtl8180_tx(struct net_device *dev, u32* txbuf, int len, priority_t priority,
		 short morefrag, short rate)
{
	u32 	*tx;
	int 	pend ;
	int 	status;
	struct 	urb *tx_urb;
	int 	urb_len;	
	struct 	r8180_priv *priv = ieee80211_priv(dev);
	struct 	ieee80211_hdr_3addr_QOS *frag_hdr = (struct ieee80211_hdr_3addr_QOS *)txbuf;
	struct 	ieee80211_device *ieee;//added for descriptor
	u8 	dest[ETH_ALEN];

	bool	bUseShortPreamble = false;
	bool	bCTSEnable = false;
	bool	bRTSEnable = false;
	u16 	Duration = 0; 
	u16	RtsDur = 0;
	u16	ThisFrameTime = 0;	
	u16	TxDescDuration = 0;

	ieee = priv->ieee80211;
#if 0
//{added by david for filter the packet listed in the filter table
#ifdef _RTL8187_EXT_PATCH_
	if((ieee->iw_mode == ieee->iw_ext_mode) && (ieee->ext_patch_ieee80211_acl_query))
	{
		if(!ieee->ext_patch_ieee80211_acl_query(ieee, frag_hdr->addr1)) {
			return 0;
		}
	}
#endif
//}
#endif

#ifdef JUST_FOR_87SEMESH
//#ifdef Lawrence_Mesh
	u8* meshtype = (u8*)txbuf;
	if(*meshtype == 0xA8)
	{
		//overflow??
		//memcpy(meshtype+ActionHeadLen+2,meshtype+ActionHeadLen,Len-ActionHeadLen);
		//memcpy(meshtype+ActionHeadLen,0,2);
		u8 actionframe[256];
		memset(actionframe,0,256);
		memcpy(actionframe,meshtype,ActionHeadLen);
		memcpy(actionframe+ActionHeadLen+2,meshtype+ActionHeadLen,len-ActionHeadLen);
		txbuf = (u32*)actionframe;
		len=len+2;
		frag_hdr = (struct ieee80211_hdr_3addr_QOS *)txbuf;			
	}
#endif

	//pend = atomic_read((priority == NORM_PRIORITY)? &priv->tx_np_pending : &priv->tx_lp_pending);
	pend = atomic_read(&priv->tx_pending[priority]);
	/* we are locked here so the two atomic_read and inc are executed without interleaves */
	if( pend > MAX_TX_URB){
		if(NIC_8187 == priv->card_8187) {
			if(priority == NORM_PRIORITY)
				priv->stats.txnpdrop++;
			else
				priv->stats.txlpdrop++;

		} else {
			switch (priority) {
				case VO_PRIORITY:
					priv->stats.txvodrop++;
					break;
				case VI_PRIORITY:
					priv->stats.txvidrop++;
					break;
				case BE_PRIORITY:
					priv->stats.txbedrop++;
					break;
				case MANAGE_PRIORITY: //lzm for MANAGE_PRIORITY pending
					if(priv->commit == 0)
					{
						priv->commit = 1;
						printk(KERN_INFO "manage pkt pending will commit now....\n");
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0))
						schedule_work(&priv->reset_wq);
#else
						schedule_task(&priv->reset_wq);
#endif
					}	
					break;
				default://BK_PRIORITY
					priv->stats.txbkdrop++;
					break;	
			}
		}
		//printk(KERN_INFO "tx_pending: %d > MAX_TX_URB\n", priority);
		return -1;
	}

	urb_len = len + ((NIC_8187 == priv->card_8187)?(4*3):(4*8));
	if((0 == (urb_len&63))||(0 == (urb_len&511))) {
		urb_len += 1;	  
	}

	tx = kmalloc(urb_len, GFP_ATOMIC);
	if(!tx) return -ENOMEM;
	memset(tx, 0, sizeof(u32) * 8);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	tx_urb = usb_alloc_urb(0,GFP_ATOMIC);
#else
	tx_urb = usb_alloc_urb(0);
#endif

	if(!tx_urb){
		kfree(tx);
		return -ENOMEM;
	}

	// Check multicast/broadcast
	if (ieee->iw_mode == IW_MODE_INFRA) {
		/* To DS: Addr1 = BSSID, Addr2 = SA,
		   Addr3 = DA */
		//memcpy(&dest, frag_hdr->addr3, ETH_ALEN);
		memcpy(&dest, frag_hdr->addr1, ETH_ALEN);
	} else if (ieee->iw_mode == IW_MODE_ADHOC) {
		/* not From/To DS: Addr1 = DA, Addr2 = SA,
		   Addr3 = BSSID */
		memcpy(&dest, frag_hdr->addr1, ETH_ALEN);
	}

	if (is_multicast_ether_addr(dest) ||is_broadcast_ether_addr(dest)) 
	{
		Duration = 0;
		RtsDur = 0;
		bRTSEnable = false;
		bCTSEnable = false;

		ThisFrameTime = ComputeTxTime(len + sCrcLng, rtl8180_rate2rate(rate), false, bUseShortPreamble);
		TxDescDuration = ThisFrameTime;
	} else {// Unicast packet
		//u8 AckRate;
		u16 AckTime;

		// Figure out ACK rate according to BSS basic rate and Tx rate, 2006.03.08 by rcnjko.
		//AckRate = ComputeAckRate( pMgntInfo->mBrates, (u1Byte)(pTcb->DataRate) );
		// Figure out ACK time according to the AckRate and assume long preamble is used on receiver, 2006.03.08, by rcnjko.
		//AckTime = ComputeTxTime( sAckCtsLng/8, AckRate, FALSE, FALSE);
		//For simplicity, just use the 1M basic rate
		AckTime = ComputeTxTime(14, 10,false, false);	// AckCTSLng = 14 use 1M bps send
		//AckTime = ComputeTxTime(14, 2,false, false);	// AckCTSLng = 14 use 1M bps send

		if ( ((len + sCrcLng) > priv->rts) && priv->rts ){ // RTS/CTS.
			u16 RtsTime, CtsTime;
			//u16 CtsRate;
			bRTSEnable = true;
			bCTSEnable = false;

			// Rate and time required for RTS.
			RtsTime = ComputeTxTime( sAckCtsLng/8,priv->ieee80211->basic_rate, false, false);
			// Rate and time required for CTS.
			CtsTime = ComputeTxTime(14, 10,false, false);	// AckCTSLng = 14 use 1M bps send

			// Figure out time required to transmit this frame.
			ThisFrameTime = ComputeTxTime(len + sCrcLng, 
					rtl8180_rate2rate(rate), 
					false, 
					bUseShortPreamble);

			// RTS-CTS-ThisFrame-ACK.
			RtsDur = CtsTime + ThisFrameTime + AckTime + 3*aSifsTime;

			TxDescDuration = RtsTime + RtsDur;
		}else {// Normal case.
			bCTSEnable = false;
			bRTSEnable = false;
			RtsDur = 0;

			ThisFrameTime = ComputeTxTime(len + sCrcLng, rtl8180_rate2rate(rate), false, bUseShortPreamble);
			TxDescDuration = ThisFrameTime + aSifsTime + AckTime;
		}

		if(!(frag_hdr->frame_ctl & IEEE80211_FCTL_MOREFRAGS)) { //no more fragment
			// ThisFrame-ACK.
			Duration = aSifsTime + AckTime;
		} else { // One or more fragments remained.
			u16 NextFragTime;
			NextFragTime = ComputeTxTime( len + sCrcLng, //pretend following packet length equal current packet
					rtl8180_rate2rate(rate), 
					false, bUseShortPreamble );

			//ThisFrag-ACk-NextFrag-ACK.
			Duration = NextFragTime + 3*aSifsTime + 2*AckTime;
		}

	} // End of Unicast packet


	//fill the tx desriptor
	tx[0] |= len & 0xfff;
#ifdef JOHN_HWSEC
        if(frag_hdr->frame_ctl & IEEE80211_FCTL_WEP ){
                tx[0] &= 0xffff7fff;
		//group key may be different from pairwise key
		if(	frag_hdr->addr1[0]==0xff &&
			frag_hdr->addr1[0]==0xff &&
 			frag_hdr->addr1[0]==0xff &&
			frag_hdr->addr1[0]==0xff &&
			frag_hdr->addr1[0]==0xff &&
			frag_hdr->addr1[0]==0xff ){
			if(ieee->broadcast_key_type == KEY_TYPE_CCMP) tx[7] |= 0x2;//ccmp
			else tx[7] |= 0x1;//wep and tkip
		}
		else {
			if(ieee->pairwise_key_type == KEY_TYPE_CCMP) tx[7] |= 0x2;//CCMP
			else tx[7] |= 0x1;//WEP and TKIP
		}
        }
        else
#endif /*JOHN_HWSEC*/

	tx[0] |= (1<<15);

	if (priv->ieee80211->current_network.capability&WLAN_CAPABILITY_SHORT_PREAMBLE){
		if (priv->plcp_preamble_mode==1 && rate!=0) {	//  short mode now, not long!
			tx[0] |= (1<<16);	
		}			// enable short preamble mode.
	}

	if(morefrag) tx[0] |= (1<<17);
	//printk(KERN_WARNING "rtl_rate = %d\n", rate);
	tx[0] |= (rate << 24); //TX rate
	frag_hdr->duration_id = Duration;

	if(NIC_8187B == priv->card_8187) {
		if(bCTSEnable) {
			tx[0] |= (1<<18);
		}

		if(bRTSEnable) //rts enable
		{
			tx[0] |= ((ieeerate2rtlrate(priv->ieee80211->basic_rate))<<19);//RTS RATE
			tx[0] |= (1<<23);//rts enable
			tx[1] |= RtsDur;//RTS Duration
		}	
		tx[3] |= (TxDescDuration<<16); //DURATION
		if( WLAN_FC_GET_STYPE(le16_to_cpu(frag_hdr->frame_ctl)) == IEEE80211_STYPE_PROBE_RESP )
			tx[5] |= (1<<8);//(priv->retry_data<<8); //retry lim ;	
		else
			tx[5] |= (11<<8);//(priv->retry_data<<8); //retry lim ;	

		//frag_hdr->duration_id = Duration;
		memcpy(tx+8,txbuf,len);
	} else {
		if ( (len>priv->rts) && priv->rts && priority==LOW_PRIORITY){
			tx[0] |= (1<<23);	//enalbe RTS function
			tx[1] |= RtsDur; 	//Need to edit here!  ----hikaru
		}
		else {
			tx[1]=0;
		}
		tx[0] |= (ieeerate2rtlrate(priv->ieee80211->basic_rate) << 19); /* RTS RATE - should be basic rate */

		tx[2] = 3;  // CW min
		tx[2] |= (7<<4); //CW max
		tx[2] |= (11<<8);//(priv->retry_data<<8); //retry lim

		//	printk("%x\n%x\n",tx[0],tx[1]);

#ifdef DUMP_TX
		int i;
		printk("<Tx pkt>--rate %x---",rate);
		for (i = 0; i < (len + 3); i++)
			printk("%2x", ((u8*)tx)[i]);
		printk("---------------\n");
#endif
		memcpy(tx+3,txbuf,len);
	}

#ifdef JOHN_DUMP_TXDESC
                int i;
                printk("<Tx descriptor>--rate %x---",rate);
                for (i = 0; i < 8; i++)
                        printk("%8x ", tx[i]);
                printk("\n");
#endif
#ifdef JOHN_DUMP_TXPKT
		{
                int j;
                printk("\n---------------------------------------------------------------------\n");
                printk("<Tx packet>--rate %x--urb_len in decimal %d",rate, urb_len);
                for (j = 32; j < (urb_len); j++){
                        if( ( (j-32)%24 )==0 ) printk("\n");
                        printk("%2x ", ((u8*)tx)[j]);
                }
                printk("\n---------------------------------------------------------------------\n");

		}
#endif

	if(NIC_8187 == priv->card_8187) {
		usb_fill_bulk_urb(tx_urb,priv->udev,
				usb_sndbulkpipe(priv->udev,priority), tx,
				urb_len, (priority == LOW_PRIORITY)?rtl8187_lptx_isr:rtl8187_nptx_isr, dev);

	} else {
		//printk(KERN_WARNING "Tx packet use by submit urb!\n");
		/* FIXME check what EP is for low/norm PRI */
		usb_fill_bulk_urb(tx_urb,priv->udev,
				usb_sndbulkpipe(priv->udev,priority), tx,
				urb_len, TXISR_SELECT(priority), dev);
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	status = usb_submit_urb(tx_urb, GFP_ATOMIC);
#else
	status = usb_submit_urb(tx_urb);
#endif

	if (!status){
		//atomic_inc((priority == NORM_PRIORITY)? &priv->tx_np_pending : &priv->tx_lp_pending);
		atomic_inc(&priv->tx_pending[priority]);
		dev->trans_start = jiffies;
		//printk("=====> tx_pending[%d]=%d\n", priority, atomic_read(&priv->tx_pending[priority]));
		return 0;
	}else{
		DMESGE("Error TX URB %d, error pending %d",
				//atomic_read((priority == NORM_PRIORITY)? &priv->tx_np_pending : &priv->tx_lp_pending),
				atomic_read(&priv->tx_pending[priority]),
				status);
		return -1;
	}
}

 short rtl8187_usb_initendpoints(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);

	priv->rx_urb = (struct urb**) kmalloc (sizeof(struct urb*) * (MAX_RX_URB+1), GFP_KERNEL);

        memset(priv->rx_urb, 0, sizeof(struct urb*) * MAX_RX_URB);

#ifdef JACKSON_NEW_RX
        priv->pp_rxskb = (struct sk_buff **)kmalloc(sizeof(struct sk_buff *) * MAX_RX_URB, GFP_KERNEL);
        if (priv->pp_rxskb == NULL)
                goto destroy;

        memset(priv->pp_rxskb, 0, sizeof(struct sk_buff*) * MAX_RX_URB);
#endif
#ifdef THOMAS_BEACON
	{
		int align;	
		unsigned long oldaddr,newaddr; //lzm mod for 64bit cpu crash 20081107
		priv->rx_urb[MAX_RX_URB] = usb_alloc_urb(0, GFP_KERNEL);		
		priv->oldaddr = kmalloc(16, GFP_KERNEL);
		oldaddr = (unsigned long)priv->oldaddr;
		align = oldaddr&3;
		if(align != 0 ){
			newaddr = oldaddr + 4 - align;
			priv->rx_urb[MAX_RX_URB]->transfer_buffer_length = 16-4+align;
		}	
		else{
			newaddr = oldaddr;
			priv->rx_urb[MAX_RX_URB]->transfer_buffer_length = 16;
		}
		priv->rx_urb[MAX_RX_URB]->transfer_buffer = (u32*)newaddr;
	}
#endif
       

        goto _middle;


destroy:

#ifdef JACKSON_NEW_RX
        if (priv->pp_rxskb) {
                kfree(priv->pp_rxskb);
		priv->pp_rxskb = NULL;

        }
#endif 
        if (priv->rx_urb) {
                kfree(priv->rx_urb);
        }
	priv->rx_urb = NULL;

        DMESGE("Endpoint Alloc Failure");
        return -ENOMEM;
	

_middle:

	return 0;
	
}
#ifdef THOMAS_BEACON
void rtl8187_usb_deleteendpoints(struct net_device *dev)
{	
	int i;
	struct r8180_priv *priv = ieee80211_priv(dev);
	
        if( in_interrupt() )
		printk(KERN_ALERT " %ld in interrupt \n",in_interrupt() );
	if(priv->rx_urb){
		for(i=0;i<(MAX_RX_URB+1);i++){
			if(priv->rx_urb[i]) {
				usb_kill_urb(priv->rx_urb[i]);
				usb_free_urb(priv->rx_urb[i]);
			}
		}
		kfree(priv->rx_urb);
		priv->rx_urb = NULL;	
	}	
	if(priv->oldaddr){
		kfree(priv->oldaddr);
		priv->oldaddr = NULL;
	}	
        if (priv->pp_rxskb) {
                kfree(priv->pp_rxskb);
                priv->pp_rxskb = 0;
	}
}
#endif

void rtl8187_set_rate(struct net_device *dev)
{
	int i;
	u16 word;
	int basic_rate,min_rr_rate,max_rr_rate;
	
	//if (ieee80211_is_54g(priv->ieee80211->current_network) && 
	//	priv->ieee80211->state == IEEE80211_LINKED){
	basic_rate = ieeerate2rtlrate(240);
	min_rr_rate = ieeerate2rtlrate(60);
	max_rr_rate = ieeerate2rtlrate(240);
	
	/*
	}else{
		basic_rate = ieeerate2rtlrate(20);
		min_rr_rate = ieeerate2rtlrate(10);
		max_rr_rate = ieeerate2rtlrate(110);
	}
	*/
	
	write_nic_byte(dev, RESP_RATE,
			max_rr_rate<<MAX_RESP_RATE_SHIFT| min_rr_rate<<MIN_RESP_RATE_SHIFT);

	//word  = read_nic_word(dev, BRSR);
	word  = read_nic_word(dev, BRSR_8187);
	word &= ~BRSR_MBR_8185;
		

	for(i=0;i<=basic_rate;i++)
		word |= (1<<i);

	//write_nic_word(dev, BRSR, word);
	write_nic_word(dev, BRSR_8187, word);
}


void rtl8187_link_change(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	//write_nic_word(dev, BintrItv, net->beacon_interval);
	rtl8187_net_update(dev);
	/*update timing params*/
	rtl8180_set_chan(dev, priv->chan);
	rtl8187_set_rxconf(dev);
}

#if LINUX_VERSION_CODE >=KERNEL_VERSION(2,6,20)
void rtl8180_wmm_param_update(struct work_struct* work)
{
	struct ieee80211_device * ieee = container_of(work, struct ieee80211_device,wmm_param_update_wq);
	struct net_device *dev = ieee->dev;
	struct r8180_priv *priv = ieee80211_priv(dev);
#else
void rtl8180_wmm_param_update(struct ieee80211_device *ieee)
{
	struct net_device *dev = ieee->dev;
	struct r8180_priv *priv = ieee80211_priv(dev);
#endif
	u8 *ac_param = (u8 *)(ieee->current_network.wmm_param);
	u8 mode = ieee->current_network.mode;
	AC_CODING	eACI;
	AC_PARAM	AcParam;
	PAC_PARAM	pAcParam;
	u8 i;

	//8187 need not to update wmm param, added by David, 2006.9.8
	if(NIC_8187 == priv->card_8187) {
		return; 
	}

	if(!ieee->current_network.QoS_Enable) 
	{
		//legacy ac_xx_param update
		
		AcParam.longData = 0;
		AcParam.f.AciAifsn.f.AIFSN = 2; // Follow 802.11 DIFS.
		AcParam.f.AciAifsn.f.ACM = 0;
		AcParam.f.Ecw.f.ECWmin = 3; // Follow 802.11 CWmin.
		AcParam.f.Ecw.f.ECWmax = 7; // Follow 802.11 CWmax.
		AcParam.f.TXOPLimit = 0;
		for(eACI = 0; eACI < AC_MAX; eACI++)
		{
			AcParam.f.AciAifsn.f.ACI = (u8)eACI;
			{
				u8		u1bAIFS;
				u32		u4bAcParam;


				pAcParam = (PAC_PARAM)(&AcParam);
				// Retrive paramters to udpate.
				u1bAIFS = pAcParam->f.AciAifsn.f.AIFSN *(((mode&IEEE_G) == IEEE_G)?9:20)  + aSifsTime; 
				u4bAcParam = ((((u32)(pAcParam->f.TXOPLimit)) << AC_PARAM_TXOP_LIMIT_OFFSET)	| 
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
			}
		}

		return;
	}
	//
	for(i = 0; i < AC_MAX; i++){
		pAcParam = (AC_PARAM * )ac_param;
		{
			AC_CODING	eACI;
			u8		u1bAIFS;
			u32		u4bAcParam;

			// Retrive paramters to udpate.
			eACI = pAcParam->f.AciAifsn.f.ACI; 
			//Mode G/A: slotTimeTimer = 9; Mode B: 20
			u1bAIFS = pAcParam->f.AciAifsn.f.AIFSN * (((mode&IEEE_G) == IEEE_G)?9:20) + aSifsTime; 
			u4bAcParam = ((((u32)(pAcParam->f.TXOPLimit)) << AC_PARAM_TXOP_LIMIT_OFFSET)	| 
						  (((u32)(pAcParam->f.Ecw.f.ECWmax)) << AC_PARAM_ECW_MAX_OFFSET)	|  
						  (((u32)(pAcParam->f.Ecw.f.ECWmin)) << AC_PARAM_ECW_MIN_OFFSET)	|  
						  (((u32)u1bAIFS) << AC_PARAM_AIFS_OFFSET));

			switch(eACI)
			{
				case AC1_BK:
					write_nic_dword(dev, AC_BK_PARAM, u4bAcParam);
					//printk(KERN_WARNING "[%04x]:0x%08x\n",AC_BK_PARAM,read_nic_dword(dev, AC_BK_PARAM));
					break;

				case AC0_BE:
					write_nic_dword(dev, AC_BE_PARAM, u4bAcParam);
					//printk(KERN_WARNING "[%04x]:0x%08x\n",AC_BE_PARAM,read_nic_dword(dev, AC_BE_PARAM));
					break;

				case AC2_VI:
					write_nic_dword(dev, AC_VI_PARAM, u4bAcParam);
					//printk(KERN_WARNING "[%04x]:0x%08x\n",AC_VI_PARAM,read_nic_dword(dev, AC_VI_PARAM));
					break;

				case AC3_VO:
					write_nic_dword(dev, AC_VO_PARAM, u4bAcParam);
					//printk(KERN_WARNING "[%04x]:0x%08x\n",AC_VO_PARAM,read_nic_dword(dev, AC_VO_PARAM));
					break;

				default:
					printk(KERN_WARNING "SetHwReg8185(): invalid ACI: %d !\n", eACI);
					break;
			}
		}
		ac_param += (sizeof(AC_PARAM));
	}
}

int IncludedInSupportedRates(struct r8180_priv *priv,  u8 TxRate  )
{
    	u8 rate_len;
        u8 rate_ex_len;
        u8                      RateMask = 0x7F;
        u8                      idx;
        unsigned short          Found = 0;
        u8                      NaiveTxRate = TxRate&RateMask;

    	rate_len = priv->ieee80211->current_network.rates_len;
        rate_ex_len = priv->ieee80211->current_network.rates_ex_len;
		
        for( idx=0; idx< rate_len; idx++ ){
                if( (priv->ieee80211->current_network.rates[idx] & RateMask) == NaiveTxRate ) {
                        Found = 1;
                        goto found_rate;
                }
        }
		
   	for( idx=0; idx< rate_ex_len; idx++ ) {
                if( (priv->ieee80211->current_network.rates_ex[idx] & RateMask) == NaiveTxRate ) {
                        Found = 1;
                        goto found_rate;
                }
        }
	
        return Found;
        found_rate:
        return Found;
}
//
//      Description:
//              Get the Tx rate one degree up form the input rate in the supported rates.
//              Return the upgrade rate if it is successed, otherwise return the input rate.
//      By Bruce, 2007-06-05.
// 
u8 GetUpgradeTxRate(struct net_device *dev, u8 rate)
{
        struct r8180_priv *priv = ieee80211_priv(dev);
        u8                      UpRate;

        // Upgrade 1 degree.
        switch(rate)
        {
        case 108: // Up to 54Mbps.
                UpRate = 108;
                break;

        case 96: // Up to 54Mbps.
                UpRate = 108;
                break;

        case 72: // Up to 48Mbps.
                UpRate = 96;
                break;

        case 48: // Up to 36Mbps.
                UpRate = 72;
                break;

        case 36: // Up to 24Mbps.
                UpRate = 48;
                break;

        case 22: // Up to 18Mbps.
                UpRate = 36;
                break;

        case 11: // Up to 11Mbps.
                UpRate = 22;
                break;

        case 4: // Up to 5.5Mbps.
                UpRate = 11;
                break;

        case 2: // Up to 2Mbps.
                UpRate = 4;
                break;

        default:
                printk("GetUpgradeTxRate(): Input Tx Rate(%d) is undefined!\n", rate);
                return rate;
        }
        // Check if the rate is valid.
        if(IncludedInSupportedRates(priv, UpRate))
        {
//              printk("GetUpgradeTxRate(): GetUpgrade Tx rate(%d) from %d !\n", UpRate, priv->CurrentOperaRate);
                return UpRate;
        }
        else
        {
                printk("GetUpgradeTxRate(): Tx rate (%d) is not in supported rates\n", UpRate);
                return rate;
        }
        return rate;
}
//
//      Description:
//              Get the Tx rate one degree down form the input rate in the supported rates.
//              Return the degrade rate if it is successed, otherwise return the input rate.
//      By Bruce, 2007-06-05.
// 
u8 GetDegradeTxRate( struct net_device *dev, u8 rate)
{
        struct r8180_priv *priv = ieee80211_priv(dev);
        u8                      DownRate;

        // Upgrade 1 degree.
        switch(rate)
        {
        case 108: // Down to 48Mbps.
                DownRate = 96;
                break;

        case 96: // Down to 36Mbps.
                DownRate = 72;
                break;

        case 72: // Down to 24Mbps.
                DownRate = 48;
                break;

        case 48: // Down to 18Mbps.
                DownRate = 36;
                break;

        case 36: // Down to 11Mbps.
                DownRate = 22;
                break;

        case 22: // Down to 5.5Mbps.
                DownRate = 11;
                break;

        case 11: // Down to 2Mbps.
                DownRate = 4;
                break;

        case 4: // Down to 1Mbps.
                DownRate = 2;
                break;

        case 2: // Down to 1Mbps.
                DownRate = 2;
                break;

        default:
                printk("GetDegradeTxRate(): Input Tx Rate(%d) is undefined!\n", rate);
                return rate;
        }
        // Check if the rate is valid.
        if(IncludedInSupportedRates(priv, DownRate)){
//              printk("GetDegradeTxRate(): GetDegrade Tx rate(%d) from %d!\n", DownRate, priv->CurrentOperaRate);
                return DownRate;
        }else{
                printk("GetDegradeTxRate(): Tx rate (%d) is not in supported rates\n", DownRate);
                return rate;
        }
        return rate;
}

//
//      Helper function to determine if specified data rate is 
//      CCK rate.
//      2005.01.25, by rcnjko.
//
bool MgntIsCckRate(u16 rate )
{
        bool bReturn = false;

        if((rate <= 22) && (rate != 12) && (rate != 18)){
                bReturn = true;
        }

        return bReturn;
}
//by amy for rate adaptive
//
//      Description: 
//              Core logic to adjust Tx data rate in STA mode according to 
//              OFDM retry count ratio.
//
//      Note:
//              RTL8187   : pHalData->CurrRetryCnt = TallyCnt
//              RTL8187B : pHalData->CurrRetryCnt = PktRetryCnt in TxClosedCommand 
//
void sta_rateadaptive8187B(struct net_device *dev)
{
        struct r8180_priv *priv = ieee80211_priv(dev);
        unsigned long   CurrTxokCnt;
        u16             CurrRetryCnt;
        u16             CurrRetryRate;
        unsigned long   CurrRxokCnt;
        bool            bTryUp = false;
        bool            bTryDown = false;
        u8              TryUpTh = 1;
        u8              TryDownTh = 2;
        u32             TxThroughput;
        long            CurrSignalStrength;
        bool            bUpdateInitialGain = false;
        CurrRetryCnt	=  priv->CurrRetryCnt;
        CurrTxokCnt     = (priv->stats.txbeaconokint + priv->stats.txmanageokint +
                           priv->stats.txvookint + priv->stats.txviokint + priv->stats.txbeokint)- priv->LastTxokCnt;
        CurrRxokCnt     =  priv->stats.rxok - priv->LastRxokCnt;
        CurrSignalStrength = priv->RecvSignalPower;
        TxThroughput 	   = (u32)(priv->txokbytestotal - priv->LastTxOKBytes);
        priv->LastTxOKBytes = priv->txokbytestotal;
        priv->CurrentOperaRate = priv->ieee80211->rate / 5;
    	//printk("priv->CurrentOperaRate is %d\n",priv->CurrentOperaRate);
    	
#if 1 
        //2 Compute retry ratio.
        if (CurrTxokCnt>0)
        {
                CurrRetryRate = (u16)(CurrRetryCnt*100/CurrTxokCnt);
        }
        else
        { // It may be serious retry. To distinguish serious retry or no packets modified by Bruce
                CurrRetryRate = (u16)(CurrRetryCnt*100/1);
        }
#endif


      	//printk("\n(1) priv->LastRetryRate: %d \n",priv->LastRetryRate);
      	//printk("(2) CurrRetryCnt = %d  \n", CurrRetryCnt);      
      	//printk("(3) TxokCnt = %d \n", CurrTxokCnt);
      	//printk("(4) CurrRetryRate = %d \n", CurrRetryRate);     
      	//printk("(5) SignalStrength = %d \n",priv->RecvSignalPower);

        priv->LastRetryCnt = priv->CurrRetryCnt;
        priv->LastTxokCnt  = (priv->stats.txbeaconokint + priv->stats.txmanageokint +
                                        priv->stats.txvookint + priv->stats.txviokint + priv->stats.txbeokint);
        priv->LastRxokCnt = priv->stats.rxok;
        priv->CurrRetryCnt = 0;
        //2No Tx packets, return to init_rate or not?
        if (CurrRetryRate==0 && CurrTxokCnt == 0)
        {
                //
                // 2007.04.09, by Roger. after 4.5 seconds in this condition, we try to raise rate.
                //
                priv->TryupingCountNoData++;

             	//printk("No Tx packets, TryupingCountNoData(%d)\n", priv->TryupingCountNoData);
              	//printk("(6) priv->CurrentOperaRate =%d\n", priv->CurrentOperaRate);

                if (priv->TryupingCountNoData>15)
                {
                        priv->TryupingCountNoData = 0;
                        priv->CurrentOperaRate = GetUpgradeTxRate(dev, priv->CurrentOperaRate);
                        // Reset Fail Record
                        priv->LastFailTxRate = 0;
                        priv->LastFailTxRateSS = -200;
                        priv->FailTxRateCount = 0;
                }
                goto SetInitialGain;
        }
        else
        {
                priv->TryupingCountNoData=0; //Reset trying up times.
        }

        //
        // For Netgear case, I comment out the following signal strength estimation,
        // which can results in lower rate to transmit when sample is NOT enough (e.g. PING request).  
        // 2007.04.09, by Roger.        
        //      
#if 1
        // If sample is not enough, we use signalstrength.
        if ( CurrTxokCnt<10|| CurrRetryCnt<10)
        {
                //printk("Sample is not enough, we use signalstrength for rate adaptive\n");
                //After 3 sec, and trying up.
                priv->TryupingCountNoData++;
                if (priv->TryupingCountNoData>10)
                {
                        //printk("Sample is not enough and After 3 sec try up\n");
                        priv->TryupingCountNoData=0;
                        
                        //
                        // Added by Roger, 2007.01.04.
                        // Signal strength plus 3 for air link.
                        //
                        
                        if ( CurrSignalStrength>-68 )//&& IncludedInSupportedRates(Adapter, 108) )
			{
				priv->ieee80211->rate = 540;
                                //pMgntInfo->CurrentOperaRate = 108;
                        }
			else if (CurrSignalStrength>-70)//  && IncludedInSupportedRates(Adapter, 96) )
			{
				priv->ieee80211->rate = 480;
                                //pMgntInfo->CurrentOperaRate = 96;
                        }
			else if (CurrSignalStrength>-73)//  && IncludedInSupportedRates(Adapter, 72) )
			{
				priv->ieee80211->rate = 360;
                                //pMgntInfo->CurrentOperaRate = 72;
                        }
			else if (CurrSignalStrength>-79)//  && IncludedInSupportedRates(Adapter, 48) )
			{
				priv->ieee80211->rate = 240;
                                //pMgntInfo->CurrentOperaRate = 48;
                        }
			else if (CurrSignalStrength>-81)//  && IncludedInSupportedRates(Adapter, 36) )
			{
				priv->ieee80211->rate = 180;
                                //pMgntInfo->CurrentOperaRate = 36;
                        }
			else if (CurrSignalStrength>-83)//  && IncludedInSupportedRates(Adapter, 22) )
			{
				priv->ieee80211->rate = 110;
                                //pMgntInfo->CurrentOperaRate = 22;
                        }
			else if (CurrSignalStrength>-85)//  && IncludedInSupportedRates(Adapter, 11) )
			{
				priv->ieee80211->rate = 55;
                                //pMgntInfo->CurrentOperaRate = 11;
                        }
			else if (CurrSignalStrength>-89)//  && IncludedInSupportedRates(Adapter, 4) )
			{
				priv->ieee80211->rate = 20;
                                //pMgntInfo->CurrentOperaRate = 4;
                        }
                        
                
                }

                //2004.12.23 skip record for 0
                //pHalData->LastRetryRate = CurrRetryRate;
                //printk("pMgntInfo->CurrentOperaRate =%d\n",priv->ieee80211->rate);
                return;
        }
        else
        {
                priv->TryupingCountNoData=0;
        }       
#endif
        //
        // Restructure rate adaptive as the following main stages:
        // (1) Add retry threshold in 54M upgrading condition with signal strength.
        // (2) Add the mechanism to degrade to CCK rate according to signal strength 
        //              and retry rate.
        // (3) Remove all Initial Gain Updates over OFDM rate. To avoid the complicated 
        //              situation, Initial Gain Update is upon on DIG mechanism except CCK rate.
        // (4) Add the mehanism of trying to upgrade tx rate.
        // (5) Record the information of upping tx rate to avoid trying upping tx rate constantly.
        // By Bruce, 2007-06-05.
        //      
        //

        // 11Mbps or 36Mbps
        // Check more times in these rate(key rates).
        //
        if(priv->CurrentOperaRate == 22 || priv->CurrentOperaRate == 72)
        {
                TryUpTh += 9;
        }
        //
        // Let these rates down more difficult.
        //
        if(MgntIsCckRate(priv->CurrentOperaRate) || priv->CurrentOperaRate == 36)
        {
                        TryDownTh += 1;
        }

        //1 Adjust Rate.
        if (priv->bTryuping == true)
        {
                //2 For Test Upgrading mechanism
                // Note:
                //      Sometimes the throughput is upon on the capability bwtween the AP and NIC,
                //      thus the low data rate does not improve the performance.
                //      We randomly upgrade the data rate and check if the retry rate is improved.

                // Upgrading rate did not improve the retry rate, fallback to the original rate.
                if ( (CurrRetryRate > 25) && TxThroughput < priv->LastTxThroughput)
                {
                        //Not necessary raising rate, fall back rate.
                        bTryDown = true;
                    	//printk("Not necessary raising rate, fall back rate....\n");
                    	//printk("(7) priv->CurrentOperaRate =%d, TxThroughput = %d, LastThroughput = %d\n",
                         //             priv->CurrentOperaRate, TxThroughput, priv->LastTxThroughput);                  
                }
                else
                {
                        priv->bTryuping = false;
                }
        }
        else if (CurrSignalStrength > -51 && (CurrRetryRate < 100))
        {
                //2For High Power
                //
                // Added by Roger, 2007.04.09.
                // Return to highest data rate, if signal strength is good enough.
                // SignalStrength threshold(-50dbm) is for RTL8186.
                // Revise SignalStrength threshold to -51dbm.   
                //
                // Also need to check retry rate for safety, by Bruce, 2007-06-05.
                if(priv->CurrentOperaRate != 108)
                {
                        bTryUp = true;
                        // Upgrade Tx Rate directly.
                        priv->TryupingCount += TryUpTh;
                    	//printk("StaRateAdaptive87B: Power(%d) is high enough!!. \n", CurrSignalStrength);
                }
        }
        // To avoid unstable rate jumping, comment out this condition, by Bruce, 2007-06-26.
        /*
        else if(CurrSignalStrength < -86 && CurrRetryRate >= 100)
        {       
                //2 For Low Power
                //
                // Low signal strength and high current tx rate may cause Tx rate to degrade too slowly.
                // Update Tx rate to CCK rate directly.
                // By Bruce, 2007-06-05.
                //              
                if(!MgntIsCckRate(pMgntInfo->CurrentOperaRate))
                {
                        if(CurrSignalStrength > -88 && IncludedInSupportedRates(Adapter, 22)) // 11M
                                pMgntInfo->CurrentOperaRate = 22;
                        else if(CurrSignalStrength > -90 && IncludedInSupportedRates(Adapter, 11)) // 5.5M
                                pMgntInfo->CurrentOperaRate = 11;
                        else if(CurrSignalStrength > -92 && IncludedInSupportedRates(Adapter, 4)) // 2M
                                pMgntInfo->CurrentOperaRate = 4;
                        else // 1M
                                pMgntInfo->CurrentOperaRate = 2;
                }
                else if(CurrRetryRate >= 200)
                {
                        pMgntInfo->CurrentOperaRate = GetDegradeTxRate(Adapter, pMgntInfo->CurrentOperaRate);
                }
                RT_TRACE(COMP_RATE, DBG_LOUD, ("RA: Low Power(%d), or High Retry Rate(%d), set rate to CCK rate (%d). \n",
                                        CurrSignalStrength, CurrRetryRate, pMgntInfo->CurrentOperaRate));
                bUpdateInitialGain = TRUE;
                // Reset Fail Record
                pHalData->LastFailTxRate = 0;
                pHalData->LastFailTxRateSS = -200;
                pHalData->FailTxRateCount = 0;
                goto SetInitialGain;
        }
        */
        else if(CurrTxokCnt< 100 && CurrRetryRate >= 600)
        {
                //2 For Serious Retry
                //
                // Traffic is not busy but our Tx retry is serious. 
                //
                bTryDown = true;
                // Let Rate Mechanism to degrade tx rate directly.
                priv->TryDownCountLowData += TryDownTh;
            	//printk("RA: Tx Retry is serious. Degrade Tx Rate to %d directly...\n", priv->CurrentOperaRate); 
        }
        else if ( priv->CurrentOperaRate == 108 )
        {
                //2For 54Mbps
                // if ( (CurrRetryRate>38)&&(pHalData->LastRetryRate>35)) 
                if ( (CurrRetryRate>33)&&(priv->LastRetryRate>32))
                {
                        //(30,25) for cable link threshold. (38,35) for air link.
                        //Down to rate 48Mbps.
                        bTryDown = true;
                }
        }
        else if ( priv->CurrentOperaRate == 96 )
        {
                //2For 48Mbps
                // if ( ((CurrRetryRate>73) && (pHalData->LastRetryRate>72)) && IncludedInSupportedRates(Adapter, 72) )
                if ( ((CurrRetryRate>48) && (priv->LastRetryRate>47)))
                {
                        //(73, 72) for temp used.
                        //(25, 23) for cable link, (60,59) for air link.
                        //CurrRetryRate plus 25 and 26 respectively for air link.
                        //Down to rate 36Mbps.
                        bTryDown = true;
                }
                else if ( (CurrRetryRate<8) && (priv->LastRetryRate<8) ) //TO DO: need to consider (RSSI)
                {
                        bTryUp = true;
                }
        }
        else if ( priv->CurrentOperaRate == 72 )
        {
                //2For 36Mbps
                //if ( (CurrRetryRate>97) && (pHalData->LastRetryRate>97)) 
                if ( (CurrRetryRate>55) && (priv->LastRetryRate>54))
                {
                        //(30,25) for cable link threshold respectively. (103,10) for air link respectively.
                        //CurrRetryRate plus 65 and 69 respectively for air link threshold.
                        //Down to rate 24Mbps.
                        bTryDown = true;
                }
                // else if ( (CurrRetryRate<20) &&  (pHalData->LastRetryRate<20) && IncludedInSupportedRates(Adapter, 96) )//&& (device->LastRetryRate<15) ) //TO DO: need to consider (RSSI)
                else if ( (CurrRetryRate<15) &&  (priv->LastRetryRate<16))//&& (device->LastRetryRate<15) ) //TO DO: need to consider (RSSI)
                {
                        bTryUp = true;
                }
        }
        else if ( priv->CurrentOperaRate == 48 )
        {
                //2For 24Mbps
                // if ( ((CurrRetryRate>119) && (pHalData->LastRetryRate>119) && IncludedInSupportedRates(Adapter, 36)))
                if ( ((CurrRetryRate>63) && (priv->LastRetryRate>62)))
                {
                        //(15,15) for cable link threshold respectively. (119, 119) for air link threshold.
                        //Plus 84 for air link threshold.
                        //Down to rate 18Mbps.
                        bTryDown = true;
                }
                // else if ( (CurrRetryRate<14) && (pHalData->LastRetryRate<15) && IncludedInSupportedRates(Adapter, 72)) //TO DO: need to consider (RSSI)
                else if ( (CurrRetryRate<20) && (priv->LastRetryRate<21)) //TO DO: need to consider (RSSI)
                {
                        bTryUp = true;
                }
        }
        else if ( priv->CurrentOperaRate == 36 )
        {
                //2For 18Mbps
                if ( ((CurrRetryRate>109) && (priv->LastRetryRate>109)))
                {
                        //(99,99) for cable link, (109,109) for air link.
                        //Down to rate 11Mbps.
                        bTryDown = true;
                }
                // else if ( (CurrRetryRate<15) && (pHalData->LastRetryRate<16) && IncludedInSupportedRates(Adapter, 48)) //TO DO: need to consider (RSSI)      
                else if ( (CurrRetryRate<25) && (priv->LastRetryRate<26)) //TO DO: need to consider (RSSI)
                {
                        bTryUp = true;
                }
        }
        else if ( priv->CurrentOperaRate == 22 )
        {
                //2For 11Mbps
                // if (CurrRetryRate>299 && IncludedInSupportedRates(Adapter, 11))
                if (CurrRetryRate>95)
                {
                        bTryDown = true;
                }
                else if (CurrRetryRate<55)//&& (device->LastRetryRate<55) ) //TO DO: need to consider (RSSI)
                        {
                        bTryUp = true;
                        }
                }
        else if ( priv->CurrentOperaRate == 11 )
        {
                //2For 5.5Mbps
                // if (CurrRetryRate>159 && IncludedInSupportedRates(Adapter, 4) ) 
                if (CurrRetryRate>149)
                {
                        bTryDown = true;
                }
                // else if ( (CurrRetryRate<30) && (pHalData->LastRetryRate<30) && IncludedInSupportedRates(Adapter, 22) )
                else if ( (CurrRetryRate<60) && (priv->LastRetryRate < 65))
                        {
                        bTryUp = true;
                        }
                }
        else if ( priv->CurrentOperaRate == 4 )
        {
                //2For 2 Mbps
                if((CurrRetryRate>99) && (priv->LastRetryRate>99))
                {
                        bTryDown = true;
        }
                // else if ( (CurrRetryRate<50) && (pHalData->LastRetryRate<65) && IncludedInSupportedRates(Adapter, 11) )
                else if ( (CurrRetryRate < 65) && (priv->LastRetryRate < 70))
        {
                        bTryUp = true;
                }
        }
        else if ( priv->CurrentOperaRate == 2 )
                {
                //2For 1 Mbps
                // if ( (CurrRetryRate<50) && (pHalData->LastRetryRate<65) && IncludedInSupportedRates(Adapter, 4))
                if ( (CurrRetryRate<70) && (priv->LastRetryRate<75))
                        {
                        bTryUp = true;
                }
        }
        if(bTryUp && bTryDown)
                printk("StaRateAdaptive87B(): Tx Rate tried upping and downing simultaneously!\n");

        //1 Test Upgrading Tx Rate
        // Sometimes the cause of the low throughput (high retry rate) is the compatibility between the AP and NIC.
        // To test if the upper rate may cause lower retry rate, this mechanism randomly occurs to test upgrading tx rate.
        if(!bTryUp && !bTryDown && (priv->TryupingCount == 0) && (priv->TryDownCountLowData == 0)
                && priv->CurrentOperaRate != 108 && priv->FailTxRateCount < 2)
        {
#if 1 
                if(jiffies% (CurrRetryRate + 101) == 0)
                {
                        bTryUp = true;
                        priv->bTryuping = true;
                        printk("======================================================>StaRateAdaptive87B(): Randomly try upgrading...\n");
                }
#endif
        }
        //1 Rate Mechanism
        if(bTryUp)
        {
                priv->TryupingCount++;
                priv->TryDownCountLowData = 0;

                //
                // Check more times if we need to upgrade indeed.
                // Because the largest value of pHalData->TryupingCount is 0xFFFF and 
                // the largest value of pHalData->FailTxRateCount is 0x14,
                // this condition will be satisfied at most every 2 min.
                //
                if((priv->TryupingCount > (TryUpTh + priv->FailTxRateCount * priv->FailTxRateCount)) ||
                        (CurrSignalStrength > priv->LastFailTxRateSS) || priv->bTryuping)
                {
                        priv->TryupingCount = 0;
                        // 
                        // When transfering from CCK to OFDM, DIG is an important issue.
                        //
                        if(priv->CurrentOperaRate == 22)
                                bUpdateInitialGain = true;
                        // (1)To avoid upgrade frequently to the fail tx rate, add the FailTxRateCount into the threshold.
                        // (2)If the signal strength is increased, it may be able to upgrade.
                        priv->CurrentOperaRate = GetUpgradeTxRate(dev, priv->CurrentOperaRate);
                    	//printk("StaRateAdaptive87B(): Upgrade Tx Rate to %d\n", priv->CurrentOperaRate);

                        // Update Fail Tx rate and count.
                        if(priv->LastFailTxRate != priv->CurrentOperaRate)
                        {
                                priv->LastFailTxRate = priv->CurrentOperaRate;
                                priv->FailTxRateCount = 0;
                                priv->LastFailTxRateSS = -200; // Set lowest power.
                        }
                }
        }
        else
        {
                if(priv->TryupingCount > 0)
                        priv->TryupingCount --;
        }

        if(bTryDown)
        {
                priv->TryDownCountLowData++;
                priv->TryupingCount = 0;


                //Check if Tx rate can be degraded or Test trying upgrading should fallback.
                if(priv->TryDownCountLowData > TryDownTh || priv->bTryuping)
                {
                        priv->TryDownCountLowData = 0;
                        priv->bTryuping = false;
                        // Update fail information.
                        if(priv->LastFailTxRate == priv->CurrentOperaRate)
                        {
                                priv->FailTxRateCount ++;
                                // Record the Tx fail rate signal strength.
                                if(CurrSignalStrength > priv->LastFailTxRateSS)
                                {
                                        priv->LastFailTxRateSS = CurrSignalStrength;
                                }
                        }
                        else
                        {
                                priv->LastFailTxRate = priv->CurrentOperaRate;
                                priv->FailTxRateCount = 1;
                                priv->LastFailTxRateSS = CurrSignalStrength;
                        }
                        priv->CurrentOperaRate = GetDegradeTxRate(dev, priv->CurrentOperaRate);
                        //
                        // When it is CCK rate, it may need to update initial gain to receive lower power packets.
                        //
                        if(MgntIsCckRate(priv->CurrentOperaRate))
                        {
                                bUpdateInitialGain = true;
                        }
                     	//printk("StaRateAdaptive87B(): Degrade Tx Rate to %d\n", priv->CurrentOperaRate);
                }
        }
        else
        {
                if(priv->TryDownCountLowData > 0)
                        priv->TryDownCountLowData --;
        }
        // Keep the Tx fail rate count to equal to 0x15 at most.
        // Reduce the fail count at least to 10 sec if tx rate is tending stable.
        if(priv->FailTxRateCount >= 0x15 ||
                (!bTryUp && !bTryDown && priv->TryDownCountLowData == 0 && priv->TryupingCount && priv->FailTxRateCount > 0x6))
        {
                priv->FailTxRateCount --;
        }

        //
        // We need update initial gain when we set tx rate "from OFDM to CCK" or
        // "from CCK to OFDM". 
        //
SetInitialGain:
#if 1    //to be done
        if(bUpdateInitialGain)
        {
                if(MgntIsCckRate(priv->CurrentOperaRate)) // CCK
                {
                        if(priv->InitialGain > priv->RegBModeGainStage)
                        {
                                if(CurrSignalStrength < -85) // Low power, OFDM [0x17] = 26.
                                {
                                        priv->InitialGain = priv->RegBModeGainStage;
                                }
                                else if(priv->InitialGain > priv->RegBModeGainStage + 1)
                                {
                                        priv->InitialGain -= 2;
                                }
                                else
                                {
                                        priv->InitialGain --;
                                }
                                UpdateInitialGain(dev);
                        }
                }
                else // OFDM
                {                       
                        if(priv->InitialGain < 4)
                        {
                                priv->InitialGain ++;
                                UpdateInitialGain(dev);
                        }                                       
                }
        }
#endif
        //Record the related info
        priv->LastRetryRate = CurrRetryRate;
        priv->LastTxThroughput = TxThroughput;
        priv->ieee80211->rate = priv->CurrentOperaRate * 5;
}

#if LINUX_VERSION_CODE >=KERNEL_VERSION(2,6,20)
void rtl8180_rate_adapter(struct work_struct * work)
{
    struct delayed_work *dwork = container_of(work,struct delayed_work,work);
        struct ieee80211_device *ieee = container_of(dwork,struct ieee80211_device,rate_adapter_wq);
        struct net_device *dev = ieee->dev;
#else
void rtl8180_rate_adapter(struct net_device *dev)
{

#endif
        sta_rateadaptive8187B(dev);
}

void timer_rate_adaptive(unsigned long data)
{
    struct r8180_priv* priv = ieee80211_priv((struct net_device *)data);
   	 //DMESG("---->timer_rate_adaptive()\n");
        if(!priv->up)
        {
                //DMESG("<----timer_rate_adaptive():driver is not up!\n");
                return;
        }
        if(     (priv->ieee80211->mode != IEEE_B) &&
                (priv->ieee80211->iw_mode != IW_MODE_MASTER)
                && ((priv->ieee80211->state == IEEE80211_LINKED)||(priv->ieee80211->state == IEEE80211_MESH_LINKED)))
        {
        //DMESG("timer_rate_adaptive():schedule rate_adapter_wq\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
        queue_delayed_work(priv->ieee80211->wq,&priv->ieee80211->rate_adapter_wq, 0);
#else	
        queue_work(priv->ieee80211->wq,&priv->ieee80211->rate_adapter_wq);
#endif
        }

        mod_timer(&priv->rateadapter_timer, jiffies + MSECS(DEFAULT_RATE_ADAPTIVE_TIMER_PERIOD));
	//DMESG("<----timer_rate_adaptive()\n");
}
//by amy for rate adaptive


void rtl8180_irq_rx_tasklet_new(struct r8180_priv *priv);
void rtl8180_irq_rx_tasklet(struct r8180_priv *priv);

//YJ,add,080828,for KeepAlive
#if 0
static void MgntLinkKeepAlive(struct r8180_priv *priv )
{
	if (priv->keepAliveLevel == 0)
		return;

	if(priv->ieee80211->state == IEEE80211_LINKED)
	{
		//
		// Keep-Alive.  
		//
		//printk("LastTx:%d Tx:%d LastRx:%d Rx:%ld Idle:%d\n",priv->link_detect.LastNumTxUnicast,priv->NumTxUnicast, priv->link_detect.LastNumRxUnicast, priv->ieee80211->NumRxUnicast, priv->link_detect.IdleCount);
		
		if ( (priv->keepAliveLevel== 2) ||
			(priv->link_detect.LastNumTxUnicast == priv->NumTxUnicast && 
			priv->link_detect.LastNumRxUnicast == priv->ieee80211->NumRxUnicast )
			)
		{
			priv->link_detect.IdleCount++;	

			//
			// Send a Keep-Alive packet packet to AP if we had been idle for a while.
			//
			if(priv->link_detect.IdleCount >= ((KEEP_ALIVE_INTERVAL / CHECK_FOR_HANG_PERIOD)-1) )
			{
				priv->link_detect.IdleCount = 0;
				ieee80211_sta_ps_send_null_frame(priv->ieee80211, false);
			}
		}
		else
		{
			priv->link_detect.IdleCount = 0;
		}
		priv->link_detect.LastNumTxUnicast = priv->NumTxUnicast; 
		priv->link_detect.LastNumRxUnicast = priv->ieee80211->NumRxUnicast;
	}
}
//YJ,add,080828,for KeepAlive,end
#endif
void InactivePowerSave(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	//
	// This flag "bSwRfProcessing", indicates the status of IPS procedure, should be set if the IPS workitem
	// is really scheduled.
	// The old code, sets this flag before scheduling the IPS workitem and however, at the same time the
	// previous IPS workitem did not end yet, fails to schedule the current workitem. Thus, bSwRfProcessing
	// blocks the IPS procedure of switching RF.
	// By Bruce, 2007-12-25.
	//
	priv->bSwRfProcessing = true;
	MgntActSet_RF_State(dev, priv->eInactivePowerState, RF_CHANGE_BY_IPS);

	//
	// To solve CAM values miss in RF OFF, rewrite CAM values after RF ON. By Bruce, 2007-09-20.
	//
#if 0
	while( index < 4 )
	{
		if( ( pMgntInfo->SecurityInfo.PairwiseEncAlgorithm == WEP104_Encryption ) ||
			(pMgntInfo->SecurityInfo.PairwiseEncAlgorithm == WEP40_Encryption) )
		{
			if( pMgntInfo->SecurityInfo.KeyLen[index] != 0)
			pAdapter->HalFunc.SetKeyHandler(pAdapter, index, 0, FALSE, pMgntInfo->SecurityInfo.PairwiseEncAlgorithm, TRUE, FALSE);

		}
		index++;
	}
#endif
	priv->bSwRfProcessing = false;	
}

//
//	Description:
//		Enter the inactive power save mode. RF will be off
//	2007.08.17, by shien chang.
//
void IPSEnter(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	RT_RF_POWER_STATE rtState;

	if (priv->bInactivePs)
	{
		rtState = priv->eRFPowerState;

		//
		// Added by Bruce, 2007-12-25.
		// Do not enter IPS in the following conditions:
		// (1) RF is already OFF or Sleep
		// (2) bSwRfProcessing (indicates the IPS is still under going)
		// (3) Connectted (only disconnected can trigger IPS)
		// (4) IBSS (send Beacon)
		// (5) AP mode (send Beacon)
		//
		if (rtState == eRfOn && !priv->bSwRfProcessing && (priv->ieee80211->iw_mode != IW_MODE_ADHOC) 
			&& (priv->ieee80211->state != IEEE80211_LINKED ))
		{
#ifdef CONFIG_RADIO_DEBUG
			DMESG("IPSEnter(): Turn off RF.");
#endif
			priv->eInactivePowerState = eRfOff;
			InactivePowerSave(dev);
			//SetRFPowerState(dev, priv->eInactivePowerState);
			//MgntActSet_RF_State(dev, priv->eInactivePowerState, RF_CHANGE_BY_IPS);
		}
	}
	//printk("priv->eRFPowerState is %d\n",priv->eRFPowerState);	
}

void IPSLeave(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	RT_RF_POWER_STATE rtState;
	if (priv->bInactivePs)
	{	
		rtState = priv->eRFPowerState;	
		if (rtState == eRfOff  && (!priv->bSwRfProcessing) && priv->RfOffReason <= RF_CHANGE_BY_IPS)
		{
#ifdef CONFIG_RADIO_DEBUG
			DMESG("ISLeave(): Turn on RF.");
#endif
			priv->eInactivePowerState = eRfOn;
			InactivePowerSave(dev);
		}
	}
//	printk("priv->eRFPowerState is %d\n",priv->eRFPowerState);
}
//by amy for power save

//YJ,add,081230
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
void IPSLeave_wq (struct work_struct *work)
{
        struct ieee80211_device *ieee = container_of(work,struct ieee80211_device,ips_leave_wq);
        struct net_device *dev = ieee->dev;
#else
void IPSLeave_wq(struct net_device *dev)
{
#endif
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	down(&priv->ieee80211->ips_sem);
	IPSLeave(dev);	
	up(&priv->ieee80211->ips_sem);	
}

void ieee80211_ips_leave(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	if(priv->bInactivePs){
		if(priv->eRFPowerState == eRfOff)
		{
			//DMESG("%s", __FUNCTION__);
		        queue_work(priv->ieee80211->wq,&priv->ieee80211->ips_leave_wq);
		}
	}
}
//YJ,add,081230,end

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
void rtl8180_watch_dog_wq (struct work_struct *work)
{
        struct delayed_work *dwork = container_of(work,struct delayed_work,work);
        struct ieee80211_device *ieee = container_of(dwork,struct ieee80211_device,watch_dog_wq);
        struct net_device *dev = ieee->dev;
#else
void rtl8180_watch_dog_wq(struct net_device *dev)
{
#endif
	struct r8180_priv *priv = ieee80211_priv(dev);
	//bool bEnterPS = false;
	//bool bBusyTraffic = false;
	u32 TotalRxNum = 0;
	u16 SlotIndex = 0, i=0;
	//YJ,add,080828,for link state check
	if((priv->ieee80211->state == IEEE80211_LINKED) && (priv->ieee80211->iw_mode == IW_MODE_INFRA)){
		SlotIndex = (priv->link_detect.SlotIndex++) % priv->link_detect.SlotNum;
		priv->link_detect.RxFrameNum[SlotIndex] = priv->ieee80211->NumRxDataInPeriod + priv->ieee80211->NumRxBcnInPeriod;
		for( i=0; i<priv->link_detect.SlotNum; i++ )	
			TotalRxNum+= priv->link_detect.RxFrameNum[i];
#if 0	//for roaming temp del		
		if(TotalRxNum == 0){
			priv->ieee80211->state = IEEE80211_ASSOCIATING;
			printk("=========>turn to another AP\n");
			queue_work(priv->ieee80211->wq, &priv->ieee80211->associate_procedure_wq);
		}
#endif
	}
	priv->link_detect.NumRxOkInPeriod = 0;
	priv->link_detect.NumTxOkInPeriod = 0;	
	priv->ieee80211->NumRxDataInPeriod = 0;
	priv->ieee80211->NumRxBcnInPeriod = 0;
	
#ifdef CONFIG_IPS 
	if(priv->ieee80211->actscanning == false){
		if((priv->ieee80211->iw_mode == IW_MODE_INFRA) && 
		   (priv->ieee80211->state == IEEE80211_NOLINK) && 
		   (priv->eRFPowerState == eRfOn)) 
		{
			//printk("actscanning:%d, state:%d, eRFPowerState:%d\n",
			//	priv->ieee80211->actscanning, 
			//	priv->ieee80211->state, 
			//	priv->eRFPowerState);

			down(&priv->ieee80211->ips_sem);
			IPSEnter(dev);	
			up(&priv->ieee80211->ips_sem);
		}
	}
	//queue_delayed_work(priv->ieee80211->wq,&priv->ieee80211->watch_dog_wq,IEEE80211_WATCH_DOG_TIME);
#endif

	//printk("========================>leave rtl8180_watch_dog_wq()\n");
}

void watch_dog_adaptive(unsigned long data)
{
	struct net_device* dev = (struct net_device*)data;
    struct r8180_priv* priv = ieee80211_priv(dev);
	//DMESG("---->watch_dog_adaptive()\n");
	if(!priv->up){
		//DMESG("<----watch_dog_adaptive():driver is not up!\n");
		return;
	}
	// Tx and Rx High Power Mechanism.
	if(CheckHighPower(dev)){
		//printk("===============================> high power!\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
		queue_delayed_work(priv->ieee80211->wq,&priv->ieee80211->tx_pw_wq, 0);
#else
		queue_work(priv->ieee80211->wq,&priv->ieee80211->tx_pw_wq);
#endif
	}

	// Schedule an workitem to perform DIG
	if(CheckDig(dev) == true){

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
		queue_delayed_work(priv->ieee80211->wq,&priv->ieee80211->hw_dig_wq,0);
#else
		queue_work(priv->ieee80211->wq,&priv->ieee80211->hw_dig_wq);
#endif
	}
	
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
        queue_delayed_work(priv->ieee80211->wq,&priv->ieee80211->watch_dog_wq,0);
#else
        queue_work(priv->ieee80211->wq,&priv->ieee80211->watch_dog_wq);
#endif

 	mod_timer(&priv->watch_dog_timer, jiffies + MSECS(IEEE80211_WATCH_DOG_TIME));
        //DMESG("<----watch_dog_adaptive()\n");
}

#ifdef ENABLE_DOT11D

CHANNEL_LIST Current_tbl;

static CHANNEL_LIST ChannelPlan[] = {
	{{1,2,3,4,5,6,7,8,9,10,11,36,40,44,48,52,56,60,64},19},  		//FCC
	{{1,2,3,4,5,6,7,8,9,10,11},11},                    				//IC
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,36,40,44,48,52,56,60,64},21},  	//ETSI
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,36,40,44,48,52,56,60,64},21},    //Spain. Change to ETSI. 
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,36,40,44,48,52,56,60,64},21},  	//France. Change to ETSI.
	{{14,36,40,44,48,52,56,60,64},9},						//MKK
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,14, 36,40,44,48,52,56,60,64},22},//MKK1
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,36,40,44,48,52,56,60,64},21},	//Israel.
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,34,38,42,46},17},			// For 11a , TELEC
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,14},14},  //For Global Domain. 1-11:active scan, 12-14 passive scan. //+YJ, 080626
	{{1,2,3,4,5,6,7,8,9,10,11,12,13},13} //world wide 13: ch1~ch11 active scan, ch12~13 passive //lzm add 081205
};

static void rtl8180_set_channel_map(u8 channel_plan, struct ieee80211_device *ieee)
{
	int i;

	//lzm add 081205
	ieee->MinPassiveChnlNum=MAX_CHANNEL_NUMBER+1;
	ieee->IbssStartChnl=0;
	
	switch (channel_plan)
	{
		case COUNTRY_CODE_FCC:
		case COUNTRY_CODE_IC:
		case COUNTRY_CODE_ETSI:
		case COUNTRY_CODE_SPAIN:
		case COUNTRY_CODE_FRANCE:
		case COUNTRY_CODE_MKK:
		case COUNTRY_CODE_MKK1:
		case COUNTRY_CODE_ISRAEL:
		case COUNTRY_CODE_TELEC:
		{
			Dot11d_Init(ieee);
			ieee->bGlobalDomain = false;
			ieee->bWorldWide13 = false;
			if (ChannelPlan[channel_plan].Len != 0){
				// Clear old channel map
				memset(GET_DOT11D_INFO(ieee)->channel_map, 0, sizeof(GET_DOT11D_INFO(ieee)->channel_map));
				// Set new channel map
				for (i=0;i<ChannelPlan[channel_plan].Len;i++) 
				{
					if(ChannelPlan[channel_plan].Channel[i] <= 14)
						GET_DOT11D_INFO(ieee)->channel_map[ChannelPlan[channel_plan].Channel[i]] = 1;
				}
			}
			break;
		}
		case COUNTRY_CODE_GLOBAL_DOMAIN:
		{
			GET_DOT11D_INFO(ieee)->bEnabled = 0;
			Dot11d_Reset(ieee);
			ieee->bGlobalDomain = true;	
			ieee->bWorldWide13 = false;
			
			//lzm add 081205
			ieee->MinPassiveChnlNum=12;
			ieee->IbssStartChnl= 10;
			
			break;
		}
		case COUNTRY_CODE_WORLD_WIDE_13_INDEX://lzm add 081205
		{
			Dot11d_Init(ieee);
			ieee->bGlobalDomain = false;
			ieee->bWorldWide13 = true;
			
			//lzm add 081205
			ieee->MinPassiveChnlNum=12;
			ieee->IbssStartChnl= 10;
			
			if (ChannelPlan[channel_plan].Len != 0){
				// Clear old channel map
				memset(GET_DOT11D_INFO(ieee)->channel_map, 0, sizeof(GET_DOT11D_INFO(ieee)->channel_map));
				// Set new channel map
				for (i=0;i<ChannelPlan[channel_plan].Len;i++) 
				{
					if(ChannelPlan[channel_plan].Channel[i] <= 11)//ch1~ch11 active scan
						GET_DOT11D_INFO(ieee)->channel_map[ChannelPlan[channel_plan].Channel[i]] = 1;
					else//ch12~13 passive scan
						GET_DOT11D_INFO(ieee)->channel_map[ChannelPlan[channel_plan].Channel[i]] = 2;
				}
			}
			
		break;
		}
		default:
		{
			Dot11d_Init(ieee);
			ieee->bGlobalDomain = false;
			ieee->bWorldWide13 = false;
			memset(GET_DOT11D_INFO(ieee)->channel_map, 0, sizeof(GET_DOT11D_INFO(ieee)->channel_map));
			for (i=1;i<=14;i++) 
			{
				GET_DOT11D_INFO(ieee)->channel_map[i] = 1;
			}
			break;
		}
	}
}
#endif


static void rtl8180_link_detect_init(plink_detect_t plink_detect)
{
	memset(plink_detect, 0, sizeof(link_detect_t));
	plink_detect->SlotNum = DEFAULT_SLOT_NUM;
}

#ifdef SW_ANTE_DIVERSITY
static void rtl8187_antenna_diversity_read(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	u16 usValue;
	
	//2 Read CustomerID
	usValue = eprom_read(dev, EEPROM_SW_REVD_OFFSET>>1);
	priv->EEPROMCustomerID = (u8)( usValue & EEPROM_CID_MASK );
	//DMESG("EEPROM Customer ID: %02X\n", priv->EEPROMCustomerID);

	//2 Read AntennaDiversity
	// SW Antenna Diversity.
	if(	(usValue & EEPROM_SW_AD_MASK) != EEPROM_SW_AD_ENABLE ){
		priv->EEPROMSwAntennaDiversity = false;
		DMESG("EEPROM Disable SW Antenna Diversity");
	}else{
		priv->EEPROMSwAntennaDiversity = true;
		DMESG("EEPROM Enable SW Antenna Diversity");
	}
	// Default Antenna to use.
	if( (usValue & EEPROM_DEF_ANT_MASK) != EEPROM_DEF_ANT_1 ) {
		priv->EEPROMDefaultAntenna1 = false;
		DMESG("EEPROM Default Main Antenna 0");
	}else{
		priv->EEPROMDefaultAntenna1 = false;
		DMESG( "EEPROM Default Aux Antenna 1");
	}

	//
	// Antenna diversity mechanism. Added by Roger, 2007.11.05.
	//
	if( priv->RegSwAntennaDiversityMechanism == 0 ) // Auto //set it to 0 when init
	{// 0: default from EEPROM.
		priv->bSwAntennaDiverity = priv->EEPROMSwAntennaDiversity;
	}else{// 1:disable antenna diversity, 2: enable antenna diversity.
		priv->bSwAntennaDiverity = ((priv->RegSwAntennaDiversityMechanism == 1)? false : true);
	}
	//DMESG("bSwAntennaDiverity = %d\n", priv->bSwAntennaDiverity);


	//
	// Default antenna settings. Added by Roger, 2007.11.05.
	//
	if( priv->RegDefaultAntenna == 0)//set it to 0 when init
	{	// 0: default from EEPROM.
		priv->bDefaultAntenna1 = priv->EEPROMDefaultAntenna1;
	}else{// 1: main, 2: aux.
		priv->bDefaultAntenna1 = ((priv->RegDefaultAntenna== 2) ? true : false);
	}
	//DMESG("bDefaultAntenna1 = %d\n", priv->bDefaultAntenna1);	

//by amy for antenna
}
#endif

short rtl8180_init(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	int i, j;
	u16 word;
	//int ch;
	//u16 version;
	u8 hw_version;
	//u8 config3;
	struct usb_device *udev;
	u16 idProduct;
	u16 bcdDevice;
	//u8 chan_plan_index;
	
	//FIXME: these constants are placed in a bad pleace.

	//priv->txbuffsize = 1024;
	//priv->txringcount = 32;
	//priv->rxbuffersize = 1024;
	//priv->rxringcount = 32; 
	//priv->txbeaconcount = 3;
	//priv->rx_skb_complete = 1;
	//priv->txnp_pending.ispending=0; 
	/* ^^ the SKB does not containt a partial RXed packet (is empty) */

	//memcpy(priv->stats,0,sizeof(struct Stats));
	
	//priv->irq_enabled=0;
        priv->driver_upping = 0;
	priv->commit = 0;
	
	//priv->stats.rxdmafail=0;
	priv->stats.txrdu=0;
	//priv->stats.rxrdu=0;
	//priv->stats.rxnolast=0;
	//priv->stats.rxnodata=0;
	//priv->stats.rxreset=0;
	//priv->stats.rxwrkaround=0;
	//priv->stats.rxnopointer=0;
	priv->stats.txbeerr=0;
	priv->stats.txbkerr=0;
	priv->stats.txvierr=0;
	priv->stats.txvoerr=0;
	priv->stats.txmanageerr=0;
	priv->stats.txbeaconerr=0;
	priv->stats.txresumed=0;
	//priv->stats.rxerr=0;
	//priv->stats.rxoverflow=0;
	//priv->stats.rxint=0;
	priv->stats.txbeokint=0;
	priv->stats.txbkokint=0;
	priv->stats.txviokint=0;
	priv->stats.txvookint=0;
	priv->stats.txmanageokint=0;
	priv->stats.txbeaconokint=0;
	/*priv->stats.txhpokint=0;
	priv->stats.txhperr=0;*/
	priv->stats.rxurberr=0;
	priv->stats.rxstaterr=0;
	priv->stats.txoverflow=0;
	priv->stats.rxok=0;
	//priv->stats.txbeaconerr=0;
	//priv->stats.txlperr=0;
	//priv->stats.txlpokint=0;
//john
	priv->stats.txnpdrop=0;
	priv->stats.txlpdrop =0;
	priv->stats.txbedrop =0;
	priv->stats.txbkdrop =0;
	priv->stats.txvidrop =0;
	priv->stats.txvodrop =0;
	priv->stats.txbeacondrop =0;
	priv->stats.txmanagedrop =0;
	
    	// priv->stats.txokbytestotal =0;
//by amy
        priv->LastSignalStrengthInPercent=0;
        priv->SignalStrength=0;
        priv->SignalQuality=0;
        priv->antenna_flag=0;
        priv->flag_beacon = false;
//by amy
//david
	//radion on defaultly 
	priv->radion = 1;
//david
//by amy for rate adaptive
	priv->CurrRetryCnt=0;
	priv->LastRetryCnt=0;
        priv->LastTxokCnt=0;
        priv->LastRxokCnt=0;
        priv->LastRetryRate=0;
        priv->bTryuping=0;
        priv->CurrTxRate=0;
        priv->CurrRetryRate=0;
        priv->TryupingCount=0;
        priv->TryupingCountNoData=0;
        priv->TryDownCountLowData=0;
        priv->RecvSignalPower=0;
        priv->LastTxOKBytes=0;
        priv->LastFailTxRate=0;
        priv->LastFailTxRateSS=0;
        priv->FailTxRateCount=0;
        priv->LastTxThroughput=0;
        priv->txokbytestotal=0;
//by amy for rate adaptive
//by amy for ps
	priv->RFChangeInProgress = false;
	priv->SetRFPowerStateInProgress = false;
	priv->RFProgType = 0;
	priv->bInHctTest = false;
	priv->bInactivePs = true;//false;
	priv->ieee80211->bInactivePs = priv->bInactivePs;
	priv->eInactivePowerState = eRfOn;//lzm add for IPS and Polling methord
	priv->bSwRfProcessing = false;
	priv->eRFPowerState = eRfOff;
	priv->RfOffReason = 0;
	priv->NumRxOkInPeriod = 0;
	priv->NumTxOkInPeriod = 0;
	priv->bLeisurePs = true;
	priv->dot11PowerSaveMode = eActive;
	priv->RegThreeWireMode=HW_THREE_WIRE_BY_8051;
	priv->ps_mode = false;
//by amy for ps
//by amy for DIG
	priv->bDigMechanism = 1;
	priv->bCCKThMechanism = 0;
	priv->InitialGain = 0;
	priv->StageCCKTh = 0;
	priv->RegBModeGainStage = 2;
//by amy for DIG
// {by david for DIG, 2008.3.6
	priv->RegDigOfdmFaUpTh = 0x0c;
	priv->RegBModeGainStage = 0x02;
	priv->DIG_NumberFallbackVote = 0;
	priv->DIG_NumberUpgradeVote = 0;
	priv->CCKUpperTh = 0x100;
	priv->CCKLowerTh = 0x20;
//}
//{added by david for High tx power, 2008.3.11
	priv->bRegHighPowerMechanism = true;
	priv->bToUpdateTxPwr = false;

	priv->Z2HiPwrUpperTh = 77;
	priv->Z2HiPwrLowerTh = 75;
	priv->Z2RSSIHiPwrUpperTh = 70;
	priv->Z2RSSIHiPwrLowerTh = 20;
	//specify for rtl8187B
	priv->wMacRegRfPinsOutput = 0x0480;
	priv->wMacRegRfPinsSelect = 0x2488;
	//
	// Note that, we just set TrSwState to TR_HW_CONTROLLED here instead of changing 
	// HW setting because we assume it should be inialized as HW controlled. 061010, by rcnjko.
	//
	priv->TrSwitchState = TR_HW_CONTROLLED;
//}
	priv->ieee80211->iw_mode = IW_MODE_INFRA;
//test pending bug, john 20070815
	for(i=0;i<0x10;i++)  atomic_set(&(priv->tx_pending[i]), 0);
//by lizhaoming for LED
#ifdef LED
	priv->ieee80211->ieee80211_led_contorl = LedControl8187;
#endif
#ifdef CONFIG_IPS
	priv->ieee80211->ieee80211_ips_leave = ieee80211_ips_leave;//IPSLeave;
#endif

#ifdef SW_ANTE_DIVERSITY
	priv->antb=0;
	priv->diversity=1;
        priv->LastRxPktAntenna = 0;
	priv->AdMinCheckPeriod = 5;
	priv->AdMaxCheckPeriod = 10;
	// Lower signal strength threshold to fit the HW participation in antenna diversity. +by amy 080312
	priv->AdMaxRxSsThreshold = 30;//60->30
	priv->AdRxSsThreshold = 20;//50->20
	priv->AdCheckPeriod = priv->AdMinCheckPeriod;
	priv->AdTickCount = 0;
	priv->AdRxSignalStrength = -1;
	priv->RegSwAntennaDiversityMechanism = 0;
	priv->RegDefaultAntenna = 0;
	priv->SignalStrength = 0;
	priv->AdRxOkCnt = 0;
	priv->CurrAntennaIndex = 0;
	priv->AdRxSsBeforeSwitched = 0;
	init_timer(&priv->SwAntennaDiversityTimer);
	priv->SwAntennaDiversityTimer.data = (unsigned long)dev;
	priv->SwAntennaDiversityTimer.function = (void *)SwAntennaDiversityTimerCallback;
#endif

	priv->retry_rts = DEFAULT_RETRY_RTS;
	priv->retry_data = DEFAULT_RETRY_DATA;
	priv->ieee80211->rate = 110; //11 mbps
	priv->CurrentOperaRate=priv->ieee80211->rate/5;
	priv->ieee80211->short_slot = 1;
	priv->ieee80211->mode = IEEE_G;
	priv->promisc = (dev->flags & IFF_PROMISC) ? 1:0;

	rtl8180_link_detect_init(&priv->link_detect);
	
	spin_lock_init(&priv->tx_lock);
	spin_lock_init(&priv->irq_lock);//added by thomas
	spin_lock_init(&priv->rf_ps_lock);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
	INIT_WORK(&priv->reset_wq, (void*)rtl8180_restart);
	INIT_WORK(&priv->ieee80211->ips_leave_wq, (void*)IPSLeave_wq);  //YJ,add,081230,for IPS
	INIT_DELAYED_WORK(&priv->ieee80211->rate_adapter_wq,(void*)rtl8180_rate_adapter);
	INIT_DELAYED_WORK(&priv->ieee80211->hw_dig_wq,(void*)rtl8180_hw_dig_wq);
	INIT_DELAYED_WORK(&priv->ieee80211->watch_dog_wq,(void*)rtl8180_watch_dog_wq);
	INIT_DELAYED_WORK(&priv->ieee80211->tx_pw_wq,(void*)rtl8180_tx_pw_wq);

#ifdef SW_ANTE_DIVERSITY
	INIT_DELAYED_WORK(&priv->ieee80211->SwAntennaWorkItem,(void*) SwAntennaWorkItemCallback);
#endif

#else
	INIT_WORK(&priv->reset_wq,(void*) rtl8180_restart,dev);
	INIT_WORK(&priv->ieee80211->ips_leave_wq, (void*)IPSLeave_wq,dev);  //YJ,add,081230,for IPS
	INIT_WORK(&priv->ieee80211->rate_adapter_wq,(void*)rtl8180_rate_adapter,dev);	
	INIT_WORK(&priv->ieee80211->hw_dig_wq,(void*)rtl8180_hw_dig_wq,dev);
	INIT_WORK(&priv->ieee80211->watch_dog_wq,(void*)rtl8180_watch_dog_wq,dev);
	INIT_WORK(&priv->ieee80211->tx_pw_wq,(void*)rtl8180_tx_pw_wq,dev);

#ifdef SW_ANTE_DIVERSITY
	INIT_WORK(&priv->ieee80211->SwAntennaWorkItem,(void*) SwAntennaWorkItemCallback, dev);
#endif

#endif
#else
	tq_init(&priv->reset_wq,(void*) rtl8180_restart,dev);
#endif
	sema_init(&priv->wx_sem,1);
	sema_init(&priv->set_chan_sem,1);
#ifdef THOMAS_TASKLET
	tasklet_init(&priv->irq_rx_tasklet,
		     (void(*)(unsigned long))rtl8180_irq_rx_tasklet_new,
		     (unsigned long)priv);
#else
	tasklet_init(&priv->irq_rx_tasklet,
		     (void(*)(unsigned long))rtl8180_irq_rx_tasklet,
		     (unsigned long)priv);
#endif
//by amy for rate adaptive
	init_timer(&priv->rateadapter_timer);
        priv->rateadapter_timer.data = (unsigned long)dev;
        priv->rateadapter_timer.function = timer_rate_adaptive;
//by amy for rate adaptive
//by amy for ps
	init_timer(&priv->watch_dog_timer);
	priv->watch_dog_timer.data = (unsigned long)dev;
	priv->watch_dog_timer.function = watch_dog_adaptive;
//by amy
//by amy for ps
	priv->ieee80211->current_network.beacon_interval = DEFAULT_BEACONINTERVAL;	
	priv->ieee80211->iw_mode = IW_MODE_INFRA;
	priv->ieee80211->softmac_features  = IEEE_SOFTMAC_SCAN | 
		IEEE_SOFTMAC_ASSOCIATE | IEEE_SOFTMAC_PROBERQ | 
#ifdef CONFIG_SOFT_BEACON
		IEEE_SOFTMAC_BEACONS |  //IEEE_SOFTMAC_SINGLE_QUEUE;
#endif
		IEEE_SOFTMAC_PROBERS | IEEE_SOFTMAC_TX_QUEUE;
	
	priv->ieee80211->active_scan = 1;
	//priv->ieee80211->ch_lock = 0;
	priv->ieee80211->rate = 110; //11 mbps
	priv->ieee80211->modulation = IEEE80211_CCK_MODULATION | IEEE80211_OFDM_MODULATION;
	priv->ieee80211->host_encrypt = 1;
	priv->ieee80211->host_decrypt = 1;
#ifdef CONFIG_SOFT_BEACON
	priv->ieee80211->start_send_beacons = NULL;
	priv->ieee80211->stop_send_beacons = NULL;
#else
	priv->ieee80211->start_send_beacons = rtl8187_beacon_tx;
	priv->ieee80211->stop_send_beacons = rtl8187_beacon_stop;
#endif
	priv->ieee80211->softmac_hard_start_xmit = rtl8180_hard_start_xmit;
	priv->ieee80211->set_chan = rtl8180_set_chan;
	priv->ieee80211->link_change = rtl8187_link_change;
	priv->ieee80211->softmac_data_hard_start_xmit = rtl8180_hard_data_xmit;
	priv->ieee80211->data_hard_stop = rtl8180_data_hard_stop;
	priv->ieee80211->data_hard_resume = rtl8180_data_hard_resume;

#ifdef _RTL8187_EXT_PATCH_
	priv->ieee80211->meshScanMode = 0;
	priv->mshobj = alloc_mshobj(priv);
	if(priv->mshobj)
	{
		priv->ieee80211->ext_patch_ieee80211_start_protocol = priv->mshobj->ext_patch_ieee80211_start_protocol;
		priv->ieee80211->ext_patch_ieee80211_stop_protocol = priv->mshobj->ext_patch_ieee80211_stop_protocol;
//by amy for mesh
		priv->ieee80211->ext_patch_ieee80211_start_mesh = priv->mshobj->ext_patch_ieee80211_start_mesh;
//by amy for mesh
		priv->ieee80211->ext_patch_ieee80211_probe_req_1 = priv->mshobj->ext_patch_ieee80211_probe_req_1;
		priv->ieee80211->ext_patch_ieee80211_probe_req_2 = priv->mshobj->ext_patch_ieee80211_probe_req_2;
		priv->ieee80211->ext_patch_ieee80211_association_req_1 = priv->mshobj->ext_patch_ieee80211_association_req_1;
		priv->ieee80211->ext_patch_ieee80211_association_req_2 = priv->mshobj->ext_patch_ieee80211_association_req_2;
		priv->ieee80211->ext_patch_ieee80211_assoc_resp_by_net_1 = priv->mshobj->ext_patch_ieee80211_assoc_resp_by_net_1;
		priv->ieee80211->ext_patch_ieee80211_assoc_resp_by_net_2 = priv->mshobj->ext_patch_ieee80211_assoc_resp_by_net_2;
		priv->ieee80211->ext_patch_ieee80211_rx_frame_softmac_on_auth = priv->mshobj->ext_patch_ieee80211_rx_frame_softmac_on_auth;
		priv->ieee80211->ext_patch_ieee80211_rx_frame_softmac_on_deauth = priv->mshobj->ext_patch_ieee80211_rx_frame_softmac_on_deauth;
		priv->ieee80211->ext_patch_ieee80211_rx_frame_softmac_on_assoc_req = priv->mshobj->ext_patch_ieee80211_rx_frame_softmac_on_assoc_req;
		priv->ieee80211->ext_patch_ieee80211_rx_frame_softmac_on_assoc_rsp = priv->mshobj->ext_patch_ieee80211_rx_frame_softmac_on_assoc_rsp;
		priv->ieee80211->ext_patch_ieee80211_ext_stop_scan_wq_set_channel = priv->mshobj->ext_patch_ieee80211_ext_stop_scan_wq_set_channel;
		priv->ieee80211->ext_patch_ieee80211_process_probe_response_1 = priv->mshobj->ext_patch_ieee80211_process_probe_response_1;
		priv->ieee80211->ext_patch_ieee80211_rx_mgt_on_probe_req = priv->mshobj->ext_patch_ieee80211_rx_mgt_on_probe_req;
		priv->ieee80211->ext_patch_ieee80211_rx_mgt_update_expire = priv->mshobj->ext_patch_ieee80211_rx_mgt_update_expire;
		priv->ieee80211->ext_patch_get_beacon_get_probersp = priv->mshobj->ext_patch_get_beacon_get_probersp;
		priv->ieee80211->ext_patch_ieee80211_rx_on_rx = priv->mshobj->ext_patch_ieee80211_rx_on_rx;		
		priv->ieee80211->ext_patch_ieee80211_xmit = priv->mshobj->ext_patch_ieee80211_xmit;
		priv->ieee80211->ext_patch_ieee80211_rx_frame_get_hdrlen = priv->mshobj->ext_patch_ieee80211_rx_frame_get_hdrlen;
		priv->ieee80211->ext_patch_ieee80211_rx_is_valid_framectl = priv->mshobj->ext_patch_ieee80211_rx_is_valid_framectl;
		priv->ieee80211->ext_patch_ieee80211_rx_process_dataframe = priv->mshobj->ext_patch_ieee80211_rx_process_dataframe;
		// priv->ieee80211->ext_patch_is_duplicate_packet = priv->mshobj->ext_patch_is_duplicate_packet;
		priv->ieee80211->ext_patch_ieee80211_softmac_xmit_get_rate = priv->mshobj->ext_patch_ieee80211_softmac_xmit_get_rate;
		/* added by david for setting acl dynamically */
		priv->ieee80211->ext_patch_ieee80211_acl_query = priv->mshobj->ext_patch_ieee80211_acl_query;
	}

#endif // _RTL8187_EXT_PATCH_

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0))
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)	
	INIT_WORK(&priv->ieee80211->wmm_param_update_wq,\
                  (void(*)(void*)) rtl8180_wmm_param_update,\
                  priv->ieee80211);
#else
	INIT_WORK(&priv->ieee80211->wmm_param_update_wq,\
                   rtl8180_wmm_param_update);
#endif
#else
	tq_init(&priv->ieee80211->wmm_param_update_wq,\
                (void(*)(void*)) rtl8180_wmm_param_update,\
                priv->ieee80211);
#endif
	priv->ieee80211->init_wmmparam_flag = 0;
	priv->ieee80211->fts = DEFAULT_FRAG_THRESHOLD;
	
	//priv->card_8185 = 2;
	priv->phy_ver = 2;
	priv->card_type = USB;
//{add for 87B	
	priv->ShortRetryLimit = 7;
	priv->LongRetryLimit = 7;
	priv->EarlyRxThreshold = 7;
	
	priv->TransmitConfig = 
		TCR_DurProcMode |	//for RTL8185B, duration setting by HW
		TCR_DISReqQsize |
                (TCR_MXDMA_2048<<TCR_MXDMA_SHIFT)|  // Max DMA Burst Size per Tx DMA Burst, 7: reservied.
		(priv->ShortRetryLimit<<TX_SRLRETRY_SHIFT)|	// Short retry limit
		(priv->LongRetryLimit<<TX_LRLRETRY_SHIFT) |	// Long retry limit
		(false ? TCR_SWPLCPLEN : 0);	// FALSE: HW provies PLCP length and LENGEXT, TURE: SW proiveds them

	priv->ReceiveConfig	=
		RCR_AMF | RCR_ADF |		//accept management/data
		RCR_ACF |			//accept control frame for SW AP needs PS-poll, 2005.07.07, by rcnjko.
		RCR_AB | RCR_AM | RCR_APM |	//accept BC/MC/UC
		//RCR_AICV | RCR_ACRC32 | 	//accept ICV/CRC error packet
		RCR_MXDMA | // Max DMA Burst Size per Rx DMA Burst, 7: unlimited.
		(priv->EarlyRxThreshold<<RX_FIFO_THRESHOLD_SHIFT) | // Rx FIFO Threshold, 7: No Rx threshold.
		(priv->EarlyRxThreshold == 7 ? RCR_ONLYERLPKT:0);

	priv->AcmControl = 0;
//}	
	priv->rf_chip = 0xff & eprom_read(dev,EPROM_RFCHIPID);
	//DMESG("rf_chip:%x\n", priv->rf_chip);
	if (!priv->rf_chip)
	{
		DMESG("Can't get rf chip ID from EEPROM");
		DMESGW("So set rf chip ID to defalut EPROM_RFCHIPID_RTL8225U");
		DMESGW("use it with care and at your own risk and");
		DMESGW("**PLEASE** REPORT SUCCESS/INSUCCESS TO Realtek");
		priv->rf_chip = EPROM_RFCHIPID_RTL8225U;

	}


//{put the card to detect different card here, mainly I/O processing
	udev = priv->udev;
	idProduct = le16_to_cpu(udev->descriptor.idProduct);
	//idProduct = 0x8197;
	bcdDevice = le16_to_cpu(udev->descriptor.bcdDevice);
	DMESG("idProduct:0x%x, bcdDevice:0x%x", idProduct,bcdDevice);
	switch (idProduct) {
		case 0x8189:
		case 0x8197:
		case 0x8198:
			/* check RF frontend chipset */
			priv->card_8187 = NIC_8187B;
			priv->rf_init = rtl8225z2_rf_init;
			priv->rf_set_chan = rtl8225z2_rf_set_chan;
			priv->rf_set_sens = NULL;
			break;

		case 0x8187:
			if(bcdDevice == 0x0200){
			priv->card_8187 = NIC_8187B;
			priv->rf_init = rtl8225z2_rf_init;
			priv->rf_set_chan = rtl8225z2_rf_set_chan;
			priv->rf_set_sens = NULL;
			break;
			}else{
			//0x0100 is for 8187
			//legacy 8187 NIC
			//delet
			;
			}
		default:
			/* check RF frontend chipset */
			priv->ieee80211->softmac_features |= IEEE_SOFTMAC_SINGLE_QUEUE;
			priv->card_8187 = NIC_8187;
			switch (priv->rf_chip) {
				case EPROM_RFCHIPID_RTL8225U:
					DMESG("Card reports RF frontend Realtek 8225");
					DMESGW("This driver has EXPERIMENTAL support for this chipset.");
					DMESGW("use it with care and at your own risk and");
					DMESGW("**PLEASE** REPORT SUCCESS/INSUCCESS TO Realtek");
					if(rtl8225_is_V_z2(dev)){
						priv->rf_init = rtl8225z2_rf_init;
						priv->rf_set_chan = rtl8225z2_rf_set_chan;
						priv->rf_set_sens = NULL;
						DMESG("This seems a new V2 radio");
					}else{
						priv->rf_init = rtl8225_rf_init;
						priv->rf_set_chan = rtl8225_rf_set_chan;
						priv->rf_set_sens = rtl8225_rf_set_sens;
						DMESG("This seems a legacy 1st version radio");
					}
					break;

				case EPROM_RFCHIPID_RTL8225U_VF:
					priv->rf_init = rtl8225z2_rf_init;
					priv->rf_set_chan = rtl8225z2_rf_set_chan;
					priv->rf_set_sens = NULL;
					DMESG("This seems a new V2 radio");
					break;

				default:
					DMESGW("Unknown RF module %x",priv->rf_chip);
					DMESGW("Exiting...");
					return -1;
			}
			break;
	}

	priv->rf_close = rtl8225_rf_close;
	priv->max_sens = RTL8225_RF_MAX_SENS;
	priv->sens = RTL8225_RF_DEF_SENS;

#ifdef ENABLE_DOT11D
#if 0	
	for(i=0;i<0xFF;i++) {
		if(i%16 == 0)
			printk("\n[%x]: ", i/16);
		printk("\t%4.4x", eprom_read(dev,i));
	}	
#endif
	priv->channel_plan = eprom_read(dev,EPROM_CHANNEL_PLAN) & 0xFF;
	//DMESG("EPROM_CHANNEL_PLAN is %x", priv->channel_plan);

	// lzm add 081205 for Toshiba:
	// For Toshiba,reading the value from EEPROM 0x6h bit[7] to determine the priority of 
	// channel plan to be defined.
	//   -If bit[7] is true, then the channel plan is defined by EEPROM but no user defined.
	//   -If bit[7] is false, then the channel plan is defined by user first. 
	//	
	if(priv->channel_plan & 0x80)	{
		DMESG("Toshiba channel plan is defined by EEPROM");
		priv->channel_plan &= 0xf;
	}
	else
		priv->channel_plan = 0;
	
	//if(priv->channel_plan > COUNTRY_CODE_WORLD_WIDE_13_INDEX){
	if(priv->channel_plan > COUNTRY_CODE_GLOBAL_DOMAIN){
		DMESG("rtl8180_init:Error channel plan! Set to default.\n");
		priv->channel_plan = 0;
	}

	//priv->channel_plan = 9;  //Global Domain
	//priv->channel_plan = 2;    //ETSI


	DMESG("Channel plan is %d ",priv->channel_plan);

	//for Toshiba card we read channel plan from ChanPlanBin
	if((idProduct != 0x8197) && (idProduct != 0x8198))
		rtl8180_set_channel_map(priv->channel_plan, priv->ieee80211);
#else
	int ch;
	priv->channel_plan = eprom_read(dev,EPROM_CHANNEL_PLAN) & 0xff;
	if(priv->channel_plan & 0x80) {
		priv->channel_plan &= 0x7f;
		if (ChannelPlan[priv->channel_plan].Len != 0){
			// force channel plan map
			for (i=0;i<ChannelPlan[priv->channel_plan].Len;i++) 
				priv->ieee80211->channel_map[ChannelPlan[priv->channel_plan].Channel[i]] = 1;
		} else {
			DMESG("No channels, aborting");
			return -1;
		}
	} else {
		//default channel plan setting
		if(!channels){
			DMESG("No channels, aborting");
			return -1;
		}
		ch=channels;
		// set channels 1..14 allowed in given locale
		for (i=1; i<=14; i++) {
			(priv->ieee80211->channel_map)[i] = (u8)(ch & 0x01);
			ch >>= 1;
		}
	}
#endif

	if((idProduct == 0x8197) || (idProduct == 0x8198))	{
		// lzm add 081205 for Toshiba:
		// 0x77h bit[0]=1: use GPIO bit[2], bit[0]=0: use GPIO bit[1]
		priv->EEPROMSelectNewGPIO =((u8)((eprom_read(dev,EPROM_SELECT_GPIO) & 0xff00) >> 8)) ? true : false;     
		DMESG("EPROM_SELECT_GPIO:%d", priv->EEPROMSelectNewGPIO);
	} else {
		priv->EEPROMSelectNewGPIO = false;
	}


	if (NIC_8187 == priv->card_8187){
		hw_version =( read_nic_dword(dev, TCR) & TCR_HWVERID_MASK)>>TCR_HWVERID_SHIFT;
		switch (hw_version) {
			case 0x06:
				//priv->card_8187_Bversion = VERSION_8187B_B;
				break;
			case 0x05:
				priv->card_8187_Bversion = VERSION_8187_D;
				break;
			default:
				priv->card_8187_Bversion = VERSION_8187_B;
				break;
		}
	}else{
		hw_version = read_nic_byte(dev, 0xe1); //87B hw version reg
		switch (hw_version){
			case 0x00:
				priv->card_8187_Bversion = VERSION_8187B_B;
				break;
			case 0x01:
				priv->card_8187_Bversion = VERSION_8187B_D;
				break;
			case 0x02:
				priv->card_8187_Bversion = VERSION_8187B_E;
				break;
			default:
				priv->card_8187_Bversion = VERSION_8187B_B; //defalt
				break;
		}
	}
	
	//printk("=====hw_version:%x\n", priv->card_8187_Bversion);
	priv->enable_gpio0 = 0;

	/*the eeprom type is stored in RCR register bit #6 */ 
	if (RCR_9356SEL & read_nic_dword(dev, RCR)){
		priv->epromtype=EPROM_93c56;
		DMESG("Reported EEPROM chip is a 93c56 (2Kbit)");
	}else{
		priv->epromtype=EPROM_93c46;
		DMESG("Reported EEPROM chip is a 93c46 (1Kbit)");
	}

	dev->dev_addr[0]=eprom_read(dev,MAC_ADR) & 0xff;
	dev->dev_addr[1]=(eprom_read(dev,MAC_ADR) & 0xff00)>>8;
	dev->dev_addr[2]=eprom_read(dev,MAC_ADR+1) & 0xff;
	dev->dev_addr[3]=(eprom_read(dev,MAC_ADR+1) & 0xff00)>>8;
	dev->dev_addr[4]=eprom_read(dev,MAC_ADR+2) & 0xff;
	dev->dev_addr[5]=((eprom_read(dev,MAC_ADR+2) & 0xff00)>>8)+1;
	
	DMESG("Card MAC address is "MAC_FMT, MAC_ARG(dev->dev_addr));
	
	for(i=1,j=0; i<6; i+=2,j++){
		
		word = eprom_read(dev,EPROM_TXPW0 + j);
		priv->chtxpwr[i]=word & 0xf;
		priv->chtxpwr_ofdm[i]=(word & 0xf0)>>4;
		priv->chtxpwr[i+1]=(word & 0xf00)>>8;
		priv->chtxpwr_ofdm[i+1]=(word & 0xf000)>>12;
	}
	
	for(i=1,j=0; i<4; i+=2,j++){
			
		word = eprom_read(dev,EPROM_TXPW1 + j);
		priv->chtxpwr[i+6]=word & 0xf;
		priv->chtxpwr_ofdm[i+6]=(word & 0xf0)>>4;
		priv->chtxpwr[i+6+1]=(word & 0xf00)>>8;
		priv->chtxpwr_ofdm[i+6+1]=(word & 0xf000)>>12;
	}
	if (priv->card_8187 == NIC_8187){
		for(i=1,j=0; i<4; i+=2,j++){
			word = eprom_read(dev,EPROM_TXPW2 + j);
			priv->chtxpwr[i+6+4]=word & 0xf;
			priv->chtxpwr_ofdm[i+6+4]=(word & 0xf0)>>4;
			priv->chtxpwr[i+6+4+1]=(word & 0xf00)>>8;
			priv->chtxpwr_ofdm[i+6+4+1]=(word & 0xf000)>>12;
		}
	} else{
		word = eprom_read(dev, 0x1B) & 0xff; //channel 11;
		priv->chtxpwr[11]=word & 0xf;
		priv->chtxpwr_ofdm[11] = (word & 0xf0)>>4;

		word = eprom_read(dev, 0xA) & 0xff; //channel 12;
		priv->chtxpwr[12]=word & 0xf;
		priv->chtxpwr_ofdm[12] = (word & 0xf0)>>4;

		word = eprom_read(dev, 0x1c) ; //channel 13 ,14;
		priv->chtxpwr[13]=word & 0xf;
		priv->chtxpwr_ofdm[13] = (word & 0xf0)>>4;
		priv->chtxpwr[14]=(word & 0xf00)>>8;
		priv->chtxpwr_ofdm[14] = (word & 0xf000)>>12;
	}
	
	word = eprom_read(dev,EPROM_TXPW_BASE);
	priv->cck_txpwr_base = word & 0xf;
	priv->ofdm_txpwr_base = (word>>4) & 0xf;
	
	//DMESG("PAPE from CONFIG2: %x",read_nic_byte(dev,CONFIG2)&0x7);
	
#ifdef SW_ANTE_DIVERSITY
	rtl8187_antenna_diversity_read(dev);
#endif

	if(rtl8187_usb_initendpoints(dev)!=0){ 
		DMESG("Endopoints initialization failed");
		return -ENOMEM;
	}
#ifdef LED
	InitSwLeds(dev);
#endif	
#ifdef DEBUG_EPROM
	dump_eprom(dev);
#endif 
	return 0;

}

void rtl8185_rf_pins_enable(struct net_device *dev)
{
/*	u16 tmp;
	tmp = read_nic_word(dev, RFPinsEnable);*/
	write_nic_word(dev, RFPinsEnable, 0x1ff7);// | tmp);
}


void rtl8185_set_anaparam2(struct net_device *dev, u32 a)
{
	u8 conf3;

	rtl8180_set_mode(dev, EPROM_CMD_CONFIG);

	conf3 = read_nic_byte(dev, CONFIG3);
	write_nic_byte(dev, CONFIG3, conf3 | (1<<CONFIG3_ANAPARAM_W_SHIFT));	

	write_nic_dword(dev, ANAPARAM2, a);

	conf3 = read_nic_byte(dev, CONFIG3);
	write_nic_byte(dev, CONFIG3, conf3 &~(1<<CONFIG3_ANAPARAM_W_SHIFT));

	rtl8180_set_mode(dev, EPROM_CMD_NORMAL);

}


void rtl8180_set_anaparam(struct net_device *dev, u32 a)
{
	u8 conf3;

	rtl8180_set_mode(dev, EPROM_CMD_CONFIG);

	conf3 = read_nic_byte(dev, CONFIG3);
	write_nic_byte(dev, CONFIG3, conf3 | (1<<CONFIG3_ANAPARAM_W_SHIFT));
	
	write_nic_dword(dev, ANAPARAM, a);

	conf3 = read_nic_byte(dev, CONFIG3);
	write_nic_byte(dev, CONFIG3, conf3 &~(1<<CONFIG3_ANAPARAM_W_SHIFT));

	rtl8180_set_mode(dev, EPROM_CMD_NORMAL);
	
}


void rtl8185_tx_antenna(struct net_device *dev, u8 ant)
{
	write_nic_byte(dev, ANTSEL, ant); 
	//lzm mod for up take too long time 20081201
	//mdelay(1);
}	


void rtl8187_write_phy(struct net_device *dev, u8 adr, u32 data)
{
	u32 phyw;
	
	adr |= 0x80;
	 
	phyw= ((data<<8) | adr);
	
	
	
	// Note that, we must write 0xff7c after 0x7d-0x7f to write BB register. 
	write_nic_byte(dev, 0x7f, ((phyw & 0xff000000) >> 24));
	write_nic_byte(dev, 0x7e, ((phyw & 0x00ff0000) >> 16));
	write_nic_byte(dev, 0x7d, ((phyw & 0x0000ff00) >> 8));
	write_nic_byte(dev, 0x7c, ((phyw & 0x000000ff) ));

	//read_nic_dword(dev, PHY_ADR);
#if 0	
	for(i=0;i<10;i++){
		write_nic_dword(dev, PHY_ADR, 0xffffff7f & phyw);
		phyr = read_nic_byte(dev, PHY_READ);
		if(phyr == (data&0xff)) break;
			
	}
#endif
	/* this is ok to fail when we write AGC table. check for AGC table might be
	 * done by masking with 0x7f instead of 0xff */
	//if(phyr != (data&0xff)) DMESGW("Phy write timeout %x %x %x", phyr, data, adr);
	//msdelay(1);//CPU occupany is too high. LZM 31/10/2008  
}

u8 rtl8187_read_phy(struct net_device *dev,u8 adr, u32 data)
{
	u32 phyw;

	adr &= ~0x80;
	phyw= ((data<<8) | adr);
	
	
	// Note that, we must write 0xff7c after 0x7d-0x7f to write BB register. 
	write_nic_byte(dev, 0x7f, ((phyw & 0xff000000) >> 24));
	write_nic_byte(dev, 0x7e, ((phyw & 0x00ff0000) >> 16));
	write_nic_byte(dev, 0x7d, ((phyw & 0x0000ff00) >> 8));
	write_nic_byte(dev, 0x7c, ((phyw & 0x000000ff) ));
	
	return(read_nic_byte(dev,0x7e));

}

u8 read_phy_ofdm(struct net_device *dev, u8 adr)
{
	return(rtl8187_read_phy(dev, adr, 0x000000));
}

u8 read_phy_cck(struct net_device *dev, u8 adr)
{
	return(rtl8187_read_phy(dev, adr, 0x10000));
}

inline void write_phy_ofdm (struct net_device *dev, u8 adr, u32 data)
{
	data = data & 0xff;
	rtl8187_write_phy(dev, adr, data);
}


void write_phy_cck (struct net_device *dev, u8 adr, u32 data)
{
	data = data & 0xff;
	rtl8187_write_phy(dev, adr, data | 0x10000);
}
//
//	Description:	Change ZEBRA's power state.
//
//	Assumption:	This function must be executed in PASSIVE_LEVEL.
//
//	061214, by rcnjko.
//
//#define MAX_DOZE_WAITING_TIMES_87B 1000//500
#define MAX_DOZE_WAITING_TIMES_87B_MOD 500

bool SetZebraRFPowerState8187B(struct net_device *dev,RT_RF_POWER_STATE	eRFPowerState)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	u8			btCR9346, btConfig3;
	bool			bResult = true;
	int				i;
	u16			u2bTFPC = 0;
	u8			u1bTmp;

	// Set EEM0 and EEM1 in 9346CR.
	btCR9346 = read_nic_byte(dev, CR9346);
	write_nic_byte(dev, CR9346, (btCR9346|0xC0) );
	// Set PARM_En in Config3.
	btConfig3 = read_nic_byte(dev, CONFIG3);
	write_nic_byte(dev, CONFIG3, (btConfig3|CONFIG3_PARM_En) );
	// BB and RF related operations:
	switch(eRFPowerState)
	{
	case eRfOn:
#ifdef CONFIG_RADIO_DEBUG
		DMESG("Now Radio ON!");
#endif
		write_nic_dword(dev, ANAPARAM, ANAPARM_ON);
		write_nic_dword(dev, ANAPARAM2, ANAPARM2_ON);
		//write_nic_byte(dev, CONFIG4, (priv->RFProgType));
		
		write_nic_byte(dev, 0x085, 0x24); // 061219, SD3 ED: for minicard CCK power leakage issue.
		write_rtl8225(dev, 0x4, 0x9FF);
		mdelay(1);
		//
		// <Roger_Notes> We reset PLL to reduce power consumption about 30 mA. 2008.01.16.
		//
		{
			u8 Reg62;

			write_nic_byte(dev, 0x61, 0x10);
			Reg62 = read_nic_byte(dev, 0x62);
			write_nic_byte(dev, 0x62, (Reg62 & (~BIT5)) );
			write_nic_byte(dev, 0x62, (Reg62 | BIT5) ); // Enable PLL.
		}

		u1bTmp = read_nic_byte(dev, 0x24E);
		write_nic_byte(dev, 0x24E, (u1bTmp & (~(BIT5|BIT6))) );// 070124 SD1 Alex: turn on CCK and OFDM.
		break;

	case eRfSleep:
#ifdef CONFIG_RADIO_DEBUG
		DMESG("Now Radio Sleep!");
#endif
		for(i = 0; i < MAX_DOZE_WAITING_TIMES_87B_MOD; i++)
		{ // Make sure TX FIFO is empty befor turn off RFE pwoer.
			u2bTFPC = read_nic_word(dev, TFPC);
			if(u2bTFPC == 0){
				//printk("%d times TFPC: %d == 0 before doze...\n", (i+1), u2bTFPC);
				break;
			}else{
				//printk("%d times TFPC: %d != 0 before doze!\n", (i+1), u2bTFPC);
				udelay(10);
			}
		}
		
		if( i == MAX_DOZE_WAITING_TIMES_87B_MOD ){
			//printk("\n\n\n SetZebraRFPowerState8187B(): %d times TFPC: %d != 0 !!!\n\n\n", MAX_DOZE_WAITING_TIMES_87B_MOD, u2bTFPC);
		}

		u1bTmp = read_nic_byte(dev, 0x24E);
		write_nic_byte(dev, 0x24E, (u1bTmp|BIT5|BIT6));// 070124 SD1 Alex: turn off CCK and OFDM.
		
		write_rtl8225(dev, 0x4, 0xDFF); // Turn off RF first to prevent BB lock up, suggested by PJ, 2006.03.03.
		write_nic_byte(dev, 0x085, 0x04); // 061219, SD3 ED: for minicard CCK power leakage issue.

		//write_nic_byte(dev, CONFIG4, (priv->RFProgType|Config4_PowerOff));

		write_nic_dword(dev, ANAPARAM, ANAPARM_OFF);
		write_nic_dword(dev, ANAPARAM2, 0x72303f70); // 070126, by SD1 William.
		break;

	case eRfOff:
#ifdef CONFIG_RADIO_DEBUG
		DMESG("Now Radio OFF!");
#endif
		for(i = 0; i < MAX_DOZE_WAITING_TIMES_87B_MOD; i++)
		{ // Make sure TX FIFO is empty befor turn off RFE pwoer.
			u2bTFPC = read_nic_word(dev, TFPC);
			if(u2bTFPC == 0)			{
				//printk("%d times TFPC: %d == 0 before doze...\n", (i+1), u2bTFPC);
				break;
			}else{
				//printk("%d times TFPC: 0x%x != 0 before doze!\n", (i+1), u2bTFPC);
				udelay(10);
			}
		}

		if( i == MAX_DOZE_WAITING_TIMES_87B_MOD){
			//printk("\n\n\nSetZebraRFPowerState8187B(): %d times TFPC: 0x%x != 0 !!!\n\n\n", MAX_DOZE_WAITING_TIMES_87B_MOD, u2bTFPC);
		}

		u1bTmp = read_nic_byte(dev, 0x24E);
		write_nic_byte(dev, 0x24E, (u1bTmp|BIT5|BIT6));// 070124 SD1 Alex: turn off CCK and OFDM.
		
		write_rtl8225(dev, 0x4,0x1FF); // Turn off RF first to prevent BB lock up, suggested by PJ, 2006.03.03.
		write_nic_byte(dev, 0x085, 0x04); // 061219, SD3 ED: for minicard CCK power leakage issue.

		//write_nic_byte(dev, CONFIG4, (priv->RFProgType|Config4_PowerOff));

		write_nic_dword(dev, ANAPARAM, ANAPARM_OFF);
		write_nic_dword(dev, ANAPARAM2, ANAPARM2_OFF); // 070301, SD1 William: to reduce RF off power consumption to 80 mA.
		break;

	default:
		bResult = false;
		//printk("SetZebraRFPowerState8187B(): unknow state to set: 0x%X!!!\n", eRFPowerState);
		break;
	}

	// Clear PARM_En in Config3.
	btConfig3 &= ~(CONFIG3_PARM_En);
	write_nic_byte(dev, CONFIG3, btConfig3);
	// Clear EEM0 and EEM1 in 9346CR.
	btCR9346 &= ~(0xC0);
	write_nic_byte(dev, CR9346, btCR9346);

	if(bResult){
		// Update current RF state variable.
		priv->eRFPowerState = eRFPowerState;
	}

	return bResult;
}
//by amy for ps
//
//	Description: Chang RF Power State.
//	Note that, only MgntActSet_RF_State() is allowed to set HW_VAR_RF_STATE.
//
//	Assumption:PASSIVE LEVEL.
//
bool SetRFPowerState(struct net_device *dev,RT_RF_POWER_STATE eRFPowerState)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	bool			bResult = false;

	//printk("---------> SetRFPowerState(): eRFPowerState(%d)\n", eRFPowerState);
	if(eRFPowerState == priv->eRFPowerState)
	{
		//printk("<--------- SetRFPowerState(): discard the request for eRFPowerState(%d) is the same.\n", eRFPowerState);
		return bResult;
	}

	switch(priv->rf_chip)
	{
		case RF_ZEBRA2:
			 bResult = SetZebraRFPowerState8187B(dev, eRFPowerState);
			break;

		default:
			printk("SetRFPowerState8185(): unknown RFChipID: 0x%X!!!\n", priv->rf_chip);
			break;;
	}
	//printk("<--------- SetRFPowerState(): bResult(%d)\n", bResult);

	return bResult;
}

bool
MgntActSet_RF_State(struct net_device *dev,RT_RF_POWER_STATE StateToSet,u32 ChangeSource)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	bool				bActionAllowed = false; 
	bool				bConnectBySSID = false;
	RT_RF_POWER_STATE 	rtState;
	u16				RFWaitCounter = 0;
	unsigned long flag;
	// printk("===>MgntActSet_RF_State(): StateToSet(%d), ChangeSource(0x%x)\n",StateToSet, ChangeSource);
	//
	// Prevent the race condition of RF state change. By Bruce, 2007-11-28.
	// Only one thread can change the RF state at one time, and others should wait to be executed.
	//
#if 1 
	while(true)
	{
		//down(&priv->rf_state);
		spin_lock_irqsave(&priv->rf_ps_lock,flag);
		if(priv->RFChangeInProgress)
		{
			//up(&priv->rf_state);
			//RT_TRACE(COMP_RF, DBG_LOUD, ("MgntActSet_RF_State(): RF Change in progress! Wait to set..StateToSet(%d).\n", StateToSet));
			spin_unlock_irqrestore(&priv->rf_ps_lock,flag);			
			// Set RF after the previous action is done. 
			while(priv->RFChangeInProgress)
			{
				RFWaitCounter ++;
				//RT_TRACE(COMP_RF, DBG_LOUD, ("MgntActSet_RF_State(): Wait 1 ms (%d times)...\n", RFWaitCounter));
				udelay(1000); // 1 ms

				// Wait too long, return FALSE to avoid to be stuck here.
				if(RFWaitCounter > 1000) // 1sec
				{
			//		DMESG("MgntActSet_RF_State(): Wait too long to set RF");
					// TODO: Reset RF state?
					return false;
				}
			}
		}
		else
		{
			priv->RFChangeInProgress = true;
//			up(&priv->rf_state);
			spin_unlock_irqrestore(&priv->rf_ps_lock,flag);
			break;
		}
	}
#endif
	rtState = priv->eRFPowerState;	

	switch(StateToSet) 
	{
	case eRfOn:
		//
		// Turn On RF no matter the IPS setting because we need to update the RF state to Ndis under Vista, or
		// the Windows does not allow the driver to perform site survey any more. By Bruce, 2007-10-02. 
		//
		// leave last reasons and kick this reason till priv->RfOffReason = 0;
		// if one reason turn radio off check if off->on reason is the same.if so turn, or reject it.
		// if more than 1 reasons turn radio off we only turn on radio when all reasons turn on radio, 
		// so first turn on trys will reject till priv->RfOffReason = 0;
		priv->RfOffReason &= (~ChangeSource);

		if(! priv->RfOffReason)
		{
			priv->RfOffReason = 0;
			bActionAllowed = true;

			if(rtState == eRfOff && ChangeSource >=RF_CHANGE_BY_HW && !priv->bInHctTest)
			{
				bConnectBySSID = true;
			}
		} else {
			;//lzm must or TX_PENDING 12>MAX_TX_URB
			//printk("Turn Radio On Reject because RfOffReason= 0x%x, ChangeSource=0x%X\n", priv->RfOffReason, ChangeSource);
		}
		break;

	case eRfOff:
		 // 070125, rcnjko: we always keep connected in AP mode.
			if (priv->RfOffReason > RF_CHANGE_BY_IPS)
			{
				//
				// 060808, Annie: 
				// Disconnect to current BSS when radio off. Asked by QuanTa.
				// 

				//
				// Calling MgntDisconnect() instead of MgntActSet_802_11_DISASSOCIATE(),
				// because we do NOT need to set ssid to dummy ones.
				// Revised by Roger, 2007.12.04.
				//
//by amy not supported
				//MgntDisconnect( dev, disas_lv_ss );	
				
				// Clear content of bssDesc[] and bssDesc4Query[] to avoid reporting old bss to UI. 
				// 2007.05.28, by shien chang.
				//PlatformZeroMemory( pMgntInfo->bssDesc, sizeof(RT_WLAN_BSS)*MAX_BSS_DESC );
				//pMgntInfo->NumBssDesc = 0;
				//PlatformZeroMemory( pMgntInfo->bssDesc4Query, sizeof(RT_WLAN_BSS)*MAX_BSS_DESC );
				//pMgntInfo->NumBssDesc4Query = 0;
			}
		
		

		priv->RfOffReason |= ChangeSource;
		bActionAllowed = true;
		//printk("Turn Radio Off RfOffReason= 0x%x, ChangeSource=0x%X\n", priv->RfOffReason, ChangeSource);
		break;

	case eRfSleep:
		priv->RfOffReason |= ChangeSource;
		bActionAllowed = true;
		break;

	default:
		break;
	}

	if(bActionAllowed)
	{
                // Config HW to the specified mode.
		//printk("MgntActSet_RF_State(): Action is allowed.... StateToSet(%d), RfOffReason(%#X)\n", StateToSet, priv->RfOffReason);
		SetRFPowerState(dev, StateToSet);
		// Turn on RF.
		if(StateToSet == eRfOn) 
		{				
			//HalEnableRx8185Dummy(dev);
			if(bConnectBySSID)
			{	
			// by amy not supported
				//MgntActSet_802_11_SSID(Adapter, Adapter->MgntInfo.Ssid.Octet, Adapter->MgntInfo.Ssid.Length, TRUE );
			}
		}
		// Turn off RF.
		else if(StateToSet == eRfOff)
		{		
			//HalDisableRx8185Dummy(dev);
		}		
	}
	else
	{
		//printk("Action is rejected.... StateToSet(%d), ChangeSource(%#X), RfOffReason(%#X)\n", 
		//	StateToSet, ChangeSource, priv->RfOffReason);
	}

	// Release RF spinlock
	//down(&priv->rf_state);
	spin_lock_irqsave(&priv->rf_ps_lock,flag);
	priv->RFChangeInProgress = false;
	//up(&priv->rf_state);
	spin_unlock_irqrestore(&priv->rf_ps_lock,flag);
	return bActionAllowed;
}
//by amy for ps

void rtl8180_adapter_start(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
//	struct ieee80211_device *ieee = priv->ieee80211;
//	u8			InitWirelessMode;
//	u8			SupportedWirelessMode;
//	bool			bInvalidWirelessMode = false;	


	if(NIC_8187 == priv->card_8187) {
		//rtl8180_rtx_disable(dev);
		rtl8180_reset(dev);

		write_nic_byte(dev,0x85,0);
		write_nic_byte(dev,0x91,0);

		/* light blink! */
		write_nic_byte(dev,0x85,4);
		write_nic_byte(dev,0x91,1);
		write_nic_byte(dev,0x90,0);

		//by lizhaoming for LED POWR ON
		//LedControl8187(dev, LED_CTL_POWER_ON);
		
		/*	
			write_nic_byte(dev, CR9346, 0xC0);
		//LED TYPE
		write_nic_byte(dev, CONFIG1,((read_nic_byte(dev, CONFIG1)&0x3f)|0x80));	//turn on bit 5:Clkrun_mode
		write_nic_byte(dev, CR9346, 0x0);	// disable config register write
		*/	
		priv->irq_mask = 0xffff;
		/*
		   priv->dma_poll_mask = 0;
		   priv->dma_poll_mask|= (1<<TX_DMA_STOP_BEACON_SHIFT);
		   */	
		//	rtl8180_beacon_tx_disable(dev);

		rtl8180_set_mode(dev, EPROM_CMD_CONFIG);
		write_nic_dword(dev, MAC0, ((u32*)dev->dev_addr)[0]);
		write_nic_word(dev, MAC4, ((u32*)dev->dev_addr)[1] & 0xffff );

		rtl8180_set_mode(dev, EPROM_CMD_NORMAL);
		rtl8180_update_msr(dev);

		rtl8180_set_mode(dev, EPROM_CMD_CONFIG);

		write_nic_word(dev,0xf4,0xffff);
		write_nic_byte(dev,
				CONFIG1, (read_nic_byte(dev,CONFIG1) & 0x3f) | 0x80);	

		rtl8180_set_mode(dev,EPROM_CMD_NORMAL);

		write_nic_dword(dev,INT_TIMEOUT,0);	

#ifdef DEBUG_REGISTERS
		rtl8180_dump_reg(dev);	
#endif


		write_nic_byte(dev, WPA_CONFIG, 0);	

		write_nic_byte(dev, RATE_FALLBACK, 0x81);
		rtl8187_set_rate(dev);

		priv->rf_init(dev);	

		if(priv->rf_set_sens != NULL) {
			priv->rf_set_sens(dev,priv->sens);	
		}

		write_nic_word(dev,0x5e,1);
		//mdelay(1);
		write_nic_word(dev,0xfe,0x10);
		//	mdelay(1);
		write_nic_byte(dev, TALLY_SEL, 0x80);//Set NQ retry count
		write_nic_byte(dev, 0xff, 0x60);
		write_nic_word(dev,0x5e,0);

		rtl8180_irq_enable(dev);
	} else {
		/**
		 *  IO part migrated from Windows Driver.
		 */
		priv->irq_mask = 0xffff;
		// Enable Config3.PARAM_En.
		write_nic_byte(dev, CR9346, 0xC0);

		write_nic_byte(dev, CONFIG3, (read_nic_byte(dev, CONFIG3)| CONFIG3_PARM_En|CONFIG3_GNTSel));
		// Turn on Analog power.
		// setup A/D D/A parameter for 8185 b2 
		// Asked for by William, otherwise, MAC 3-wire can't work, 2006.06.27, by rcnjko.
		write_nic_dword(dev, ANA_PARAM2, ANAPARM2_ASIC_ON);
		write_nic_dword(dev, ANA_PARAM, ANAPARM_ASIC_ON);
		write_nic_byte(dev, ANA_PARAM3, 0x00);

		//by lizhaoming for LED POWR ON
		//LedControl8187(dev, LED_CTL_POWER_ON);

		{//added for reset PLL 
			u8 bReg62;
			write_nic_byte(dev, 0x61, 0x10);
			bReg62 = read_nic_byte(dev, 0x62);
			write_nic_byte(dev, 0x62, bReg62&(~(0x1<<5)));
			write_nic_byte(dev, 0x62, bReg62|(0x1<<5));
		}
		write_nic_byte(dev, CONFIG3, (read_nic_byte(dev, CONFIG3)&(~CONFIG3_PARM_En)));
		write_nic_byte(dev, CR9346, 0x00);

		//rtl8180_rtx_disable(dev);
		rtl8180_reset(dev);
		write_nic_byte(dev, CR9346, 0xc0);	// enable config register write
                priv->rf_init(dev);                
		// Enable tx/rx

		write_nic_byte(dev, CMD, (CR_RE|CR_TE));// Using HW_VAR_COMMAND instead of writing CMDR directly. Rewrited by Annie, 2006-04-07.

		//add this is for 8187B Rx stall problem

		rtl8180_irq_enable(dev);

		write_nic_byte_E(dev, 0x41, 0xF4);
		write_nic_byte_E(dev, 0x40, 0x00);
		write_nic_byte_E(dev, 0x42, 0x00);
		write_nic_byte_E(dev, 0x42, 0x01);
		write_nic_byte_E(dev, 0x40, 0x0F);
		write_nic_byte_E(dev, 0x42, 0x00);
		write_nic_byte_E(dev, 0x42, 0x01);
	
		// 8187B demo board MAC and AFE power saving parameters from SD1 William, 2006.07.20.
                // Parameter revised by SD1 William, 2006.08.21: 
                //      373h -> 0x4F // Original: 0x0F 
                //      377h -> 0x59 // Original: 0x4F 
                // Victor 2006/8/21: ¬Ù¹q°Ñ¼Æ«ØÄ³µ¥SD3 °ª§C·Å and cable link test OK¤~¥¿¦¡ release,¤£«ØÄ³¤Ó§Ö
                // 2006/9/5, Victor & ED: it is OK to use.
                //if(pHalData->RegBoardType == BT_DEMO_BOARD)
                //{
                        // AFE.
                        //
                        // Revise the content of Reg0x372, 0x374, 0x378 and 0x37a to fix unusual electronic current 
                        // while CTS-To-Self occurs, by DZ's request.
                        // Modified by Roger, 2007.06.22.
                        //
                        write_nic_byte(dev, 0x0DB, (read_nic_byte(dev, 0x0DB)|(BIT2)));
                        write_nic_word(dev, 0x372, 0x59FA); // 0x4FFA-> 0x59FA.
                        write_nic_word(dev, 0x374, 0x59D2); // 0x4FD2-> 0x59D2.
                        write_nic_word(dev, 0x376, 0x59D2);
                        write_nic_word(dev, 0x378, 0x19FA); // 0x0FFA-> 0x19FA.
                        write_nic_word(dev, 0x37A, 0x19FA); // 0x0FFA-> 0x19FA.
                        write_nic_word(dev, 0x37C, 0x00D0);

                        write_nic_byte(dev, 0x061, 0x00);

                        // MAC.
                        write_nic_byte(dev, 0x180, 0x0F);
                        write_nic_byte(dev, 0x183, 0x03);
                        // 061218, lanhsin: for victorh request
                        write_nic_byte(dev, 0x0DA, 0x10);
                //}

                //
                // 061213, rcnjko: 
                // Set MAC.0x24D to 0x00 to prevent cts-to-self Tx/Rx stall symptom.
                // If we set MAC 0x24D to 0x08, OFDM and CCK will turn off 
                // if not in use, and there is a bug about this action when 
                // we try to send CCK CTS and OFDM data together.
                //
                //PlatformEFIOWrite1Byte(Adapter, 0x24D, 0x00);
                // 061218, lanhsin: for victorh request
                write_nic_byte(dev, 0x24D, 0x08);

                //
                // 061215, rcnjko:
                // Follow SD3 RTL8185B_87B_MacPhy_Register.doc v0.4.
                //
                write_nic_dword(dev, HSSI_PARA, 0x0600321B);
		//
                // 061226, rcnjko:
                // Babble found in HCT 2c_simultaneous test, server with 87B might 
                // receive a packet size about 2xxx bytes.
                // So, we restrict RMS to 2048 (0x800), while as IC default value is 0xC00.
                //
                write_nic_word(dev, RMS, 0x0800);

                /*****20070321 Resolve HW page bug on system logo test
                u8 faketemp=read_nic_byte(dev, 0x50);*/
	}
}

/* this configures registers for beacon tx and enables it via
 * rtl8180_beacon_tx_enable(). rtl8180_beacon_tx_disable() might
 * be used to stop beacon transmission
 */
#if 0
void rtl8180_start_tx_beacon(struct net_device *dev)
{
	int i;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	u16 word;	
	DMESG("Enabling beacon TX");
	//write_nic_byte(dev, 0x42,0xe6);// TCR
	//rtl8180_init_beacon(dev);
	//set_nic_txring(dev);
//	rtl8180_prepare_beacon(dev);
	rtl8180_irq_disable(dev);
//	rtl8180_beacon_tx_enable(dev);
	rtl8180_set_mode(dev,EPROM_CMD_CONFIG);
	//write_nic_byte(dev,0x9d,0x20); //DMA Poll
	//write_nic_word(dev,0x7a,0);
	//write_nic_word(dev,0x7a,0x8000);

	
	word  = read_nic_word(dev, BcnItv);
	word &= ~BcnItv_BcnItv; // clear Bcn_Itv
	write_nic_word(dev, BcnItv, word);

	write_nic_word(dev, AtimWnd, 
		       read_nic_word(dev, AtimWnd) &~ AtimWnd_AtimWnd);
	
	word  = read_nic_word(dev, BintrItv);
	word &= ~BintrItv_BintrItv;
	
	//word |= priv->ieee80211->beacon_interval * 
	//	((priv->txbeaconcount > 1)?(priv->txbeaconcount-1):1);
	// FIXME:FIXME check if correct ^^ worked with 0x3e8;
	
	write_nic_word(dev, BintrItv, word);
	
	//write_nic_word(dev,0x2e,0xe002);
	//write_nic_dword(dev,0x30,0xb8c7832e);
	for(i=0; i<ETH_ALEN; i++)
		write_nic_byte(dev, BSSID+i, priv->ieee80211->beacon_cell_ssid[i]);
	
//	rtl8180_update_msr(dev);

	
	//write_nic_byte(dev,CONFIG4,3); /* !!!!!!!!!! */
	
	rtl8180_set_mode(dev, EPROM_CMD_NORMAL);
	
	rtl8180_irq_enable(dev);
	
	/* VV !!!!!!!!!! VV*/
	/*
	rtl8180_set_mode(dev,EPROM_CMD_CONFIG);
	write_nic_byte(dev,0x9d,0x00); 	
	rtl8180_set_mode(dev,EPROM_CMD_NORMAL);
*/
}
#endif
/***************************************************************************
    -------------------------------NET STUFF---------------------------
***************************************************************************/
static struct net_device_stats *rtl8180_stats(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	return &priv->ieee80211->stats;
}

int _rtl8180_up(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	//int i;

        priv->driver_upping = 1;
	priv->up=1;
	
#ifdef LED
	if(priv->ieee80211->bHwRadioOff == false)
		priv->ieee80211->ieee80211_led_contorl(dev,LED_CTL_POWER_ON); 
#endif
	MgntActSet_RF_State(dev, eRfOn, RF_CHANGE_BY_SW);

	rtl8180_adapter_start(dev);
	rtl8180_rx_enable(dev);
	rtl8180_tx_enable(dev);
//by amy for rate adaptive
        timer_rate_adaptive((unsigned long)dev);
//by amy for rate adaptive

#ifdef SW_ANTE_DIVERSITY
        if(priv->bSwAntennaDiverity){
            	//DMESG("SW Antenna Diversity Enable!");
            	SwAntennaDiversityTimerCallback(dev);
        	}
#endif

	ieee80211_softmac_start_protocol(priv->ieee80211);

//by amy for ps
	watch_dog_adaptive((unsigned long)dev);
//by amy for ps

	ieee80211_reset_queue(priv->ieee80211);
	if(!netif_queue_stopped(dev))
		netif_start_queue(dev);
	else
		netif_wake_queue(dev);

#ifndef CONFIG_SOFT_BEACON
	if(NIC_8187B == priv->card_8187)
                rtl8187_rx_manage_initiate(dev);
#endif

#ifdef _RTL8187_EXT_PATCH_
	if(priv->mshobj && priv->mshobj->ext_patch_rtl8180_up )
		priv->mshobj->ext_patch_rtl8180_up(priv->mshobj);
#endif


        priv->driver_upping = 0;
	//DMESG("rtl8187_open process complete");
	return 0;
}


int rtl8180_open(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	int ret;
//changed by lizhaoming for power on/off
	if(priv->ieee80211->bHwRadioOff == false){
		//DMESG("rtl8187_open process ");
		down(&priv->wx_sem);
		ret = rtl8180_up(dev);
		up(&priv->wx_sem);
		return ret;
	}else{
		DMESG("rtl8187_open process failed because radio off");
		return -1;
	}
	
}


int rtl8180_up(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);

	if (priv->up == 1) return -1;
	
	return _rtl8180_up(dev);
}


int rtl8180_close(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	int ret;
	
	if (priv->up == 0) return -1;

	down(&priv->wx_sem);

	//DMESG("rtl8187_close process");
	ret = rtl8180_down(dev);
#ifdef LED
	priv->ieee80211->ieee80211_led_contorl(dev,LED_CTL_POWER_OFF); 
#endif	
	up(&priv->wx_sem);
	
	return ret;

}

int rtl8180_down(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);

	if (priv->up == 0) return -1;
	
	priv->up=0;

/* FIXME */
	if (!netif_queue_stopped(dev))
		netif_stop_queue(dev);
	
	//DMESG("rtl8180_down process");
	rtl8180_rtx_disable(dev);
	rtl8180_irq_disable(dev);
//by amy for rate adaptive
        del_timer_sync(&priv->rateadapter_timer);
        cancel_delayed_work(&priv->ieee80211->rate_adapter_wq);
//by amy for rate adaptive
	del_timer_sync(&priv->watch_dog_timer);
	cancel_delayed_work(&priv->ieee80211->watch_dog_wq);
	cancel_delayed_work(&priv->ieee80211->hw_dig_wq);
	cancel_delayed_work(&priv->ieee80211->tx_pw_wq);

#ifdef SW_ANTE_DIVERSITY
	del_timer_sync(&priv->SwAntennaDiversityTimer);
	cancel_delayed_work(&priv->ieee80211->SwAntennaWorkItem);
#endif

	ieee80211_softmac_stop_protocol(priv->ieee80211);
	MgntActSet_RF_State(dev, eRfOff, RF_CHANGE_BY_SW);
	//amy,081212
	memset(&(priv->ieee80211->current_network),0,sizeof(struct ieee80211_network));
	return 0;
}

bool SetZebraRFPowerState8187B(struct net_device *dev,RT_RF_POWER_STATE  eRFPowerState);

void rtl8180_commit(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	int i;

	if (priv->up == 0) return ;
	printk("==========>%s()\n", __FUNCTION__);

	/* FIXME */
	if (!netif_queue_stopped(dev))
		netif_stop_queue(dev);

//by amy for rate adaptive
        del_timer_sync(&priv->rateadapter_timer);
        cancel_delayed_work(&priv->ieee80211->rate_adapter_wq);
//by amy for rate adaptive
        del_timer_sync(&priv->watch_dog_timer);
        cancel_delayed_work(&priv->ieee80211->watch_dog_wq);
        cancel_delayed_work(&priv->ieee80211->hw_dig_wq);
        cancel_delayed_work(&priv->ieee80211->tx_pw_wq);

#ifdef SW_ANTE_DIVERSITY
	del_timer_sync(&priv->SwAntennaDiversityTimer);
	cancel_delayed_work(&priv->ieee80211->SwAntennaWorkItem);
#endif
	ieee80211_softmac_stop_protocol(priv->ieee80211);
	
#if 0
	if(priv->ieee80211->bHwRadioOff == false){
		MgntActSet_RF_State(dev, eRfOff, RF_CHANGE_BY_HW);
		mdelay(10);
		MgntActSet_RF_State(dev, eRfOn, RF_CHANGE_BY_HW);
	}
#endif

	rtl8180_irq_disable(dev);
	rtl8180_rtx_disable(dev);

	//test pending bug, john 20070815 
	//initialize tx_pending
	for(i=0;i<0x10;i++)  atomic_set(&(priv->tx_pending[i]), 0);

	_rtl8180_up(dev);
	priv->commit = 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
void rtl8180_restart(struct work_struct *work)
{
	struct r8180_priv *priv = container_of(work, struct r8180_priv, reset_wq);
	struct ieee80211_device* ieee = priv->ieee80211;//for commit crash
	struct net_device *dev = ieee->dev;//for commit crash
#else
void rtl8180_restart(struct net_device *dev)
{

	struct r8180_priv *priv = ieee80211_priv(dev);
#endif

	down(&priv->wx_sem);
	
	rtl8180_commit(dev);
	
	up(&priv->wx_sem);
}

static void r8180_set_multicast(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	short promisc;

	//down(&priv->wx_sem);
	
	/* FIXME FIXME */
	
	promisc = (dev->flags & IFF_PROMISC) ? 1:0;
	
	if (promisc != priv->promisc)
	//	rtl8180_commit(dev);
	
	priv->promisc = promisc;
	
	//schedule_work(&priv->reset_wq);
	//up(&priv->wx_sem);
}


int r8180_set_mac_adr(struct net_device *dev, void *mac)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	struct sockaddr *addr = mac;
	
	down(&priv->wx_sem);
	
	memcpy(dev->dev_addr, addr->sa_data, ETH_ALEN);
		
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0))
	schedule_work(&priv->reset_wq);
#else
	schedule_task(&priv->reset_wq);
#endif	
	up(&priv->wx_sem);
	
	return 0;
}

struct ipw_param {
        u32 cmd;
        u8 sta_addr[ETH_ALEN];
        union {
                struct {
                        u8 name;
                        u32 value;
                } wpa_param;
                struct {
                        u32 len;
                        u8 reserved[32];
                        u8 data[0];
                } wpa_ie;
                struct{
                        u32 command;
                        u32 reason_code;
                } mlme;
                struct {
                        u8 alg[16];
                        u8 set_tx;
                        u32 err;
                        u8 idx;
                        u8 seq[8];
                        u16 key_len;
                        u8 key[0];
                } crypt;

        } u;
};


/* based on ipw2200 driver */
int rtl8180_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct iwreq *wrq = (struct iwreq *)rq;
	int ret=-1;

#ifdef JOHN_TKIP
        u8 broadcast_addr[6] = {0xff,0xff,0xff,0xff,0xff,0xff};

        struct ieee80211_device *ieee = priv->ieee80211;
        struct ipw_param *ipw = (struct ipw_param *)wrq->u.data.pointer;
        u32 key[4];

#endif

#ifdef _RTL8187_EXT_PATCH_
	if(priv->mshobj && (priv->ieee80211->iw_mode == priv->ieee80211->iw_ext_mode) && priv->mshobj->ext_patch_rtl8180_ioctl)
	{
		// DO NOT put the belowing function in critical section, due to it uses "spin lock"
		if((ret = priv->mshobj->ext_patch_rtl8180_ioctl(dev, rq, cmd)) != -EOPNOTSUPP)
			return ret;
	}
#endif

	down(&priv->wx_sem);

	switch (cmd) {
	    case RTL_IOCTL_WPA_SUPPLICANT:
#ifdef JOHN_TKIP

//the following code is specified for ipw driver in wpa_supplicant
	    if(  ((u32*)wrq->u.data.pointer)[0]==3 ){

							
		if(    ((u32*)wrq->u.data.pointer)[7] &&		
                  (    ((u32*)wrq->u.data.pointer)[3]==0x504d4343 ||
                       ((u32*)wrq->u.data.pointer)[3]==0x50494b54 )) {//50494b54 tkip and 504d4343 ccmp

                        //enable HW security of TKIP and CCMP
                        write_nic_byte(dev, WPA_CONFIG, SCR_TxSecEnable | SCR_RxSecEnable );

			//copy key from wpa_supplicant ioctl info
			key[0] = ((u32*)wrq->u.data.pointer)[12];
			key[1] = ((u32*)wrq->u.data.pointer)[13];
			key[2] = ((u32*)wrq->u.data.pointer)[14];
			key[3] = ((u32*)wrq->u.data.pointer)[15];
			switch (ieee->pairwise_key_type){
				case KEY_TYPE_TKIP:
					setKey(	dev, 
						0,			//EntryNo
						ipw->u.crypt.idx,	//KeyIndex 
				     		KEY_TYPE_TKIP,		//KeyType
  			    			(u8*)ieee->ap_mac_addr,	//MacAddr
						0,			//DefaultKey
					      	key);			//KeyContent
					break;

				case KEY_TYPE_CCMP:
					setKey(	dev, 
						0,			//EntryNo
						ipw->u.crypt.idx,	//KeyIndex 
				     		KEY_TYPE_CCMP,		//KeyType
  			    			(u8*)ieee->ap_mac_addr,	//MacAddr
						0,			//DefaultKey
					      	key);			//KeyContent
					break
;
				default: 
					printk("error on key_type: %d\n", ieee->pairwise_key_type); 
					break;
			}
		}

		//group key for broadcast
 		if( ((u32*)wrq->u.data.pointer)[9] ) {

			key[0] = ((u32*)wrq->u.data.pointer)[12];
			key[1] = ((u32*)wrq->u.data.pointer)[13];
			key[2] = ((u32*)wrq->u.data.pointer)[14];
			key[3] = ((u32*)wrq->u.data.pointer)[15];

			if( ((u32*)wrq->u.data.pointer)[3]==0x50494b54 ){//50494b54 is the ASCII code of TKIP in reversed order
				setKey(	dev, 
					1,		//EntryNo
					ipw->u.crypt.idx,//KeyIndex 
			     		KEY_TYPE_TKIP,	//KeyType
			            	broadcast_addr,	//MacAddr
					0,		//DefaultKey
				      	key);		//KeyContent
			}
			else if( ((u32*)wrq->u.data.pointer)[3]==0x504d4343 ){//CCMP
				setKey(	dev, 
					1,		//EntryNo
					ipw->u.crypt.idx,//KeyIndex 
			     		KEY_TYPE_CCMP,	//KeyType
			            	broadcast_addr,	//MacAddr
					0,		//DefaultKey
				      	key);		//KeyContent
			}
			else if( ((u32*)wrq->u.data.pointer)[3]==0x656e6f6e ){//none
				//do nothing
			}
			else if( ((u32*)wrq->u.data.pointer)[3]==0x504557 ){//WEP
                                setKey( dev,
                                        1,              //EntryNo
                                        ipw->u.crypt.idx,//KeyIndex 
                                        KEY_TYPE_WEP40,  //KeyType
                                        broadcast_addr, //MacAddr
                                        0,              //DefaultKey
                                        key);           //KeyContent
			}	
			else printk("undefine group key type: %8x\n", ((u32*)wrq->u.data.pointer)[3]);
	    	}

	    }
#endif /*JOHN_TKIP*/


#ifdef JOHN_HWSEC_DEBUG
	{
		int i;
		//john's test 0711
		printk("@@ wrq->u pointer = ");
		for(i=0;i<wrq->u.data.length;i++){
			if(i%10==0) printk("\n");
			printk( "%8x|", ((u32*)wrq->u.data.pointer)[i] );
		}
		printk("\n");
	}
#endif /*JOHN_HWSEC_DEBUG*/
		ret = ieee80211_wpa_supplicant_ioctl(priv->ieee80211, &wrq->u.data);
		break; 

	    default:
		ret = -EOPNOTSUPP;
		break;
	}

	up(&priv->wx_sem);
	
	return ret;
}


struct tx_desc {

#ifdef _LINUX_BYTEORDER_LITTLE_ENDIAN_H


//dword 0
unsigned int    tpktsize:12;
unsigned int    rsvd0:3;
unsigned int    no_encrypt:1;
unsigned int    splcp:1;
unsigned int    morefrag:1;
unsigned int    ctsen:1;
unsigned int    rtsrate:4;
unsigned int    rtsen:1;
unsigned int    txrate:4;
unsigned int	last:1;
unsigned int	first:1;
unsigned int	dmaok:1;
unsigned int	own:1;

//dword 1
unsigned short  rtsdur;
unsigned short  length:15;
unsigned short  l_ext:1;

//dword 2
unsigned int	bufaddr;


//dword 3
unsigned short	rxlen:12;
unsigned short  rsvd1:3;
unsigned short  miccal:1;
unsigned short	dur;

//dword 4
unsigned int	nextdescaddr;

//dword 5
unsigned char	rtsagc;
unsigned char	retrylimit;
unsigned short	rtdb:1;
unsigned short	noacm:1;
unsigned short	pifs:1;
unsigned short	rsvd2:4;
unsigned short	rtsratefallback:4;
unsigned short	ratefallback:5;

//dword 6
unsigned short	delaybound;
unsigned short	rsvd3:4;
unsigned short	agc:8;
unsigned short	antenna:1;
unsigned short	spc:2;
unsigned short	rsvd4:1;

//dword 7
unsigned char	len_adjust:2;
unsigned char	rsvd5:1;
unsigned char	tpcdesen:1;
unsigned char	tpcpolarity:2;
unsigned char	tpcen:1;
unsigned char	pten:1;

unsigned char	bckey:6;
unsigned char	enbckey:1;
unsigned char	enpmpd:1;
unsigned short	fragqsz;


#else

#error "please modify tx_desc to your own\n"

#endif


} __attribute__((packed));

struct rx_desc_rtl8187b {

#ifdef _LINUX_BYTEORDER_LITTLE_ENDIAN_H

//dword 0
unsigned int	rxlen:12;
unsigned int	icv:1;
unsigned int	crc32:1;
unsigned int	pwrmgt:1;
unsigned int	res:1;
unsigned int	bar:1;
unsigned int	pam:1;
unsigned int	mar:1;
unsigned int	qos:1;
unsigned int	rxrate:4;
unsigned int	trsw:1;
unsigned int	splcp:1;
unsigned int	fovf:1;
unsigned int	dmafail:1;
unsigned int	last:1;
unsigned int	first:1;
unsigned int	eor:1;
unsigned int	own:1;


//dword 1
unsigned int	tsftl;


//dword 2
unsigned int	tsfth;


//dword 3
unsigned char	sq;
unsigned char	rssi:7;
unsigned char	antenna:1;

unsigned char	agc;
unsigned char 	decrypted:1;
unsigned char	wakeup:1;
unsigned char	shift:1;
unsigned char	rsvd0:5;

//dword 4
unsigned int	num_mcsi:4;
unsigned int	snr_long2end:6;
unsigned int	cfo_bias:6;

int	pwdb_g12:8;
unsigned int	fot:8;




#else

#error "please modify tx_desc to your own\n"

#endif

}__attribute__((packed));



struct rx_desc_rtl8187 {

#ifdef _LINUX_BYTEORDER_LITTLE_ENDIAN_H

//dword 0
unsigned int    rxlen:12;
unsigned int    icv:1;
unsigned int    crc32:1;
unsigned int    pwrmgt:1;
unsigned int    res:1;
unsigned int    bar:1;
unsigned int    pam:1;
unsigned int    mar:1;
unsigned int    qos:1;
unsigned int    rxrate:4;
unsigned int    trsw:1;
unsigned int    splcp:1;
unsigned int    fovf:1;
unsigned int    dmafail:1;
unsigned int    last:1;
unsigned int    first:1;
unsigned int    eor:1;
unsigned int    own:1;

//dword 1
unsigned char   sq;
unsigned char   rssi:7;
unsigned char   antenna:1;

unsigned char   agc;
unsigned char   decrypted:1;
unsigned char   wakeup:1;
unsigned char   shift:1;
unsigned char   rsvd0:5;

//dword 2
unsigned int tsftl;

//dword 3
unsigned int tsfth;



#else

#error "please modify tx_desc to your own\n"

#endif


}__attribute__((packed));



union rx_desc {

struct	rx_desc_rtl8187b desc_87b;
struct	rx_desc_rtl8187	 desc_87;

}__attribute__((packed));

//
//	Description:
//	Perform signal smoothing for dynamic mechanism.
//	This is different with PerformSignalSmoothing8187 in smoothing fomula.
//	No dramatic adjustion is apply because dynamic mechanism need some degree
//	of correctness. 
//	2007.01.23, by shien chang.
//
void PerformUndecoratedSignalSmoothing8187(struct net_device *dev, struct ieee80211_rx_stats *stats)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	bool bCckRate = rtl8180_IsWirelessBMode(rtl8180_rate2rate(stats->rate));

	if(NIC_8187 == priv->card_8187) {
		if(priv->UndecoratedSmoothedSS >= 0)	{
			priv->UndecoratedSmoothedSS = ((priv->UndecoratedSmoothedSS * 50) + (stats->signalstrength * 11)) / 60;
		}else{
			priv->UndecoratedSmoothedSS = stats->signalstrength;
		}
	} else {
		// Determin the current packet is CCK rate, by Bruce, 2007-04-12.
		priv->bCurCCKPkt = bCckRate;

		// Tesing for SD3 DZ, by Bruce, 2007-04-11.
		if(priv->UndecoratedSmoothedSS >= 0) {	
			priv->UndecoratedSmoothedSS = ((priv->UndecoratedSmoothedSS * 5) + (stats->signalstrength * 10)) / 6;
		}else{
			priv->UndecoratedSmoothedSS = stats->signalstrength * 10;
		}

		//
		// Bacause the AGC parameter is not exactly correct under high power (AGC saturation), we need to record the RSSI value to be
		// referenced by DoRxHighPower. It is not necessary to record this value when this packet is sent by OFDM rate.
		// Advised by SD3 DZ, by Bruce, 2007-04-12.
		// 
		if(bCckRate){
			priv->CurCCKRSSI = stats->signal;
		}else{
			priv->CurCCKRSSI = 0;
		}
	}
	//printk("Sommthing SignalSterngth (%d) => UndecoratedSmoothedSS (%d)\n", stats->signalstrength, priv->UndecoratedSmoothedSS);
}

#ifdef THOMAS_SKB
void rtl8180_irq_rx_tasklet(struct r8180_priv *priv)
{
	int status,len,flen;

#ifdef SW_ANTE_DIVERSITY
	u8 	Antenna = 0;
#endif
	u32 SignalStrength = 0;
	u32 quality = 0;
	bool bCckRate = false;
	char RX_PWDB = 0;
	long RecvSignalPower=0;
	struct sk_buff *skb;	
	struct sk_buff *skb2;//skb for check out of memory
	union   rx_desc *desc;
	//struct urb *rx_urb = priv->rxurb_task;
	struct ieee80211_hdr *hdr;//by amy
	u16 fc,type;
	u8 bHwError=0,bCRC=0,bICV=0;
	long SignalStrengthIndex = 0;
	struct ieee80211_rx_stats stats = {
		.signal = 0,
		.noise = -98,
		.rate = 0,
		//.mac_time = jiffies,
		.freq = IEEE80211_24GHZ_BAND,
	};

	int *prx_inx=&priv->rx_inx;
	struct urb *rx_urb=priv->rx_urb[*prx_inx]; //changed by jackson
	struct net_device *dev = (struct net_device*)rx_urb->context;
	//DMESG("=====>RX %x ",rx_urb->status);

	skb = priv->pp_rxskb[*prx_inx];
	status = rx_urb->status;
	skb2 = dev_alloc_skb(RX_URB_SIZE);

	if (skb2 == NULL){
		printk(KERN_ALERT "No Skb For RX!/n");
		//rx_urb->transfer_buffer = skb->data;
		//priv->pp_rxskb[*prx_inx] = skb;
	} else {

		if(status == 0)
		{
			if(NIC_8187B == priv->card_8187)
			{
				stats.nic_type = NIC_8187B;
				len = rx_urb->actual_length;
				len -= sizeof (struct rx_desc_rtl8187b);
				desc = (union rx_desc *)(rx_urb->transfer_buffer + len);
				flen = desc->desc_87b.rxlen ;

				if( flen <= rx_urb->actual_length){
#if 1
#ifdef SW_ANTE_DIVERSITY
					Antenna = desc->desc_87b.antenna;
#endif
					stats.mac_time[0] = desc->desc_87b.tsftl;
					stats.mac_time[1] = desc->desc_87b.tsfth;

					stats.signal = desc->desc_87b.rssi;
					//stats.noise = desc->desc_87b.sq;
					quality = desc->desc_87b.sq;
					stats.rate = desc->desc_87b.rxrate;
					bCckRate = rtl8180_IsWirelessBMode(rtl8180_rate2rate(stats.rate));

					if(!bCckRate) { // OFDM rate.
						if(desc->desc_87b.pwdb_g12 < -106)
							SignalStrength = 0;
						else
							SignalStrength = desc->desc_87b.pwdb_g12 + 106;
						RX_PWDB = (desc->desc_87b.pwdb_g12)/2 -42;
					} else { // CCK rate.
						if(desc->desc_87b.agc> 174)
							SignalStrength = 0;
						else
							SignalStrength = 174 - desc->desc_87b.agc;
						RX_PWDB = ((desc->desc_87b.agc)/2)*(-1) - 8;
					}

					//lzm mod 081028 based on windows driver
					//compensate SignalStrength when switch TR to SW controled
					if(priv->TrSwitchState == TR_SW_TX) {
						SignalStrength = SignalStrength + 54;
						RX_PWDB = RX_PWDB + 27;
					}

					if(SignalStrength > 100)
						SignalStrength = 100;
					SignalStrength = (SignalStrength * 70) / 100 + 30;

					if(SignalStrength > 50)
						SignalStrength = SignalStrength + 10;
					if(SignalStrength > 100)
						SignalStrength = 100;

					RecvSignalPower = RX_PWDB;
					//printk("SignalStrength = %d \n",SignalStrength);
					bHwError = (desc->desc_87b.fovf | desc->desc_87b.icv | desc->desc_87b.crc32);
					bCRC = desc->desc_87b.crc32;
					bICV = desc->desc_87b.icv;
					priv->wstats.qual.level = (u8)SignalStrength;

					if(!bCckRate){
						if (quality > 127)
							quality = 0;
						else if (quality <27)
							quality = 100;
						else
							quality = 127 - quality;
					} else {
						if(quality > 64)
							quality = 0;
						else
							quality = ((64-quality)*100)/64;
					}


					priv ->wstats.qual.qual = quality;
					priv->wstats.qual.noise = 100 - priv ->wstats.qual.qual;

					stats.signalstrength = (u8)SignalStrength;
					stats.signal = (u8)quality;
					stats.noise = desc->desc_87b.snr_long2end;

					skb_put(skb,flen-4);

					priv->stats.rxok++;
					//by amy
					hdr = (struct ieee80211_hdr *)skb->data;
					fc = le16_to_cpu(hdr->frame_ctl);
					type = WLAN_FC_GET_TYPE(fc);

					if((IEEE80211_FTYPE_CTL != type) &&
					   (eqMacAddr(priv->ieee80211->current_network.bssid, (fc & IEEE80211_FCTL_TODS)? hdr->addr1 : (fc & IEEE80211_FCTL_FROMDS )? hdr->addr2 : hdr->addr3)) && (!bHwError) && (!bCRC)&& (!bICV))
					{
						// Perform signal smoothing for dynamic mechanism on demand.
						// This is different with PerformSignalSmoothing8187 in smoothing fomula.
						// No dramatic adjustion is apply because dynamic mechanism need some degree
						// of correctness. 2007.01.23, by shien chang.
						PerformUndecoratedSignalSmoothing8187(dev, &stats);

						//Update signal strength and realted into private RxStats for UI query.
						SignalStrengthIndex = NetgearSignalStrengthTranslate(priv->LastSignalStrengthInPercent,	priv->wstats.qual.level);
						priv->LastSignalStrengthInPercent = SignalStrengthIndex;
						priv->SignalStrength = TranslateToDbm8187((u8)SignalStrengthIndex);
						priv->SignalQuality = (priv->SignalQuality*5+quality+5)/6;
						priv->RecvSignalPower = (priv->RecvSignalPower * 5 + RecvSignalPower - 1) / 6;
#ifdef SW_ANTE_DIVERSITY
						priv->LastRxPktAntenna = Antenna ? 1:0;
						SwAntennaDiversityRxOk8185(dev, SignalStrength);
#endif
					}
					//by amy
#endif
					if(!ieee80211_rx(priv->ieee80211,skb, &stats)) {
						dev_kfree_skb_any(skb);
					}
				}else {
					priv->stats.rxurberr++;
					printk("URB Error  flen:%d actual_length:%d\n",  flen , rx_urb->actual_length);
					dev_kfree_skb_any(skb);
				}
			} else {
				stats.nic_type = NIC_8187;
				len = rx_urb->actual_length;
				len -= sizeof (struct rx_desc_rtl8187);
				desc = (union rx_desc *)(rx_urb->transfer_buffer + len);
				flen = desc->desc_87.rxlen ;

				if(flen <= rx_urb->actual_length){
					stats.signal = desc->desc_87.rssi;
					stats.noise = desc->desc_87.sq;
					stats.rate = desc->desc_87.rxrate;
					stats.mac_time[0] = desc->desc_87.tsftl;
					stats.mac_time[1] = desc->desc_87.tsfth;
					SignalStrength = (desc->desc_87.agc&0xfe) >> 1;
					if( ((stats.rate <= 22) && (stats.rate != 12) && (stats.rate != 18)) || (stats.rate == 44) )//need to translate to real rate here
						bCckRate= TRUE;
					if (!bCckRate)
					{
						if (SignalStrength > 90) SignalStrength = 90;
						else if (SignalStrength < 25) SignalStrength = 25;
						SignalStrength = ((90 - SignalStrength)*100)/65;
					}
					else
					{
						if (SignalStrength >95) SignalStrength = 95;
						else if (SignalStrength < 30) SignalStrength = 30;
						SignalStrength = ((95 - SignalStrength)*100)/65;
					}
					stats.signalstrength = (u8)SignalStrength;

					skb_put(skb,flen-4);

					priv->stats.rxok++;

					if(!ieee80211_rx(priv->ieee80211,skb, &stats))
						dev_kfree_skb_any(skb);


				}else {
					priv->stats.rxurberr++;
					printk("URB Error  flen:%d actual_length:%d\n",  flen , rx_urb->actual_length);
					dev_kfree_skb_any(skb);
				} 
			}
		}else{

			//printk("RX Status Error!\n");
			priv->stats.rxstaterr++;
			priv->ieee80211->stats.rx_errors++;
			dev_kfree_skb_any(skb);

		}	

		rx_urb->transfer_buffer = skb2->data;

		priv->pp_rxskb[*prx_inx] = skb2;
	}

	if(status != -ENOENT ){
		rtl8187_rx_urbsubmit(dev,rx_urb);
	}  else {
		priv->pp_rxskb[*prx_inx] = NULL;
		dev_kfree_skb_any(skb2);
		//printk("RX process %d aborted due to explicit shutdown (%x)(%d)\n ", *prx_inx, status, status);
	}

	if (*prx_inx == (MAX_RX_URB -1))
		*prx_inx = 0;
	else
		*prx_inx = *prx_inx + 1;
}
#endif

#ifdef THOMAS_TASKLET
void rtl8180_irq_rx_tasklet_new(struct r8180_priv *priv){
	unsigned long flags;
	while( atomic_read( &priv->irt_counter ) ){
		spin_lock_irqsave(&priv->irq_lock,flags);//added by thomas
		rtl8180_irq_rx_tasklet(priv);
		spin_unlock_irqrestore(&priv->irq_lock,flags);//added by thomas
		if(atomic_read(&priv->irt_counter) >= 1)
			atomic_dec( &priv->irt_counter );
	}
}
#endif
/****************************************************************************
     ---------------------------- USB_STUFF---------------------------
*****************************************************************************/

static const struct net_device_ops rtl8187_netdev_ops = {
	.ndo_open 		= rtl8180_open,
	.ndo_stop 		= rtl8180_close,
	.ndo_tx_timeout 	= tx_timeout,
	.ndo_do_ioctl 		= rtl8180_ioctl,
	.ndo_set_multicast_list = r8180_set_multicast,
	.ndo_set_mac_address 	= r8180_set_mac_adr,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_start_xmit		= ieee80211_xmit,
	.ndo_get_stats		= rtl8180_stats,
};

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
static int __devinit rtl8187_usb_probe(struct usb_interface *intf,
			 const struct usb_device_id *id)
#else
static void * __devinit rtl8187_usb_probe(struct usb_device *udev,
			                unsigned int ifnum,
			          const struct usb_device_id *id)
#endif
{
	struct net_device *dev = NULL;
	struct r8180_priv *priv= NULL;


#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	struct usb_device *udev = interface_to_usbdev(intf);
#endif

	dev = alloc_ieee80211(sizeof(struct r8180_priv));
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)	
	SET_MODULE_OWNER(dev);
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	usb_set_intfdata(intf, dev);	
	SET_NETDEV_DEV(dev, &intf->dev);
#endif
	priv = ieee80211_priv(dev);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	priv->ieee80211 = netdev_priv(dev);
#else
	priv->ieee80211 = (struct net_device *)dev->priv;
#endif
	priv->udev=udev;
#ifdef CPU_64BIT	
	priv->usb_buf = kmalloc(0x200, GFP_KERNEL);
	priv->usb_pool = dma_pool_create("rtl8187b", NULL, 64, 64, 0);
#endif
//lzm add for write time out test
#ifdef DEBUG_RW_REGISTER
	{
		int reg_index = 0;
		for(reg_index = 0; reg_index <= 199; reg_index++)
		{
			priv->write_read_registers[reg_index].address = 0;
			priv->write_read_registers[reg_index].content = 0;
			priv->write_read_registers[reg_index].flag = 0;
		}
		priv->write_read_register_index = 0;
	}
#endif
	//init netdev_ops, added by falcon....
	dev->netdev_ops = &rtl8187_netdev_ops;

	dev->wireless_handlers = &r8180_wx_handlers_def;
#if WIRELESS_EXT >= 12
#if WIRELESS_EXT < 17
	dev->get_wireless_stats = r8180_get_wireless_stats;
#endif
	dev->wireless_handlers = (struct iw_handler_def *) &r8180_wx_handlers_def;
#endif
	
	dev->type=ARPHRD_ETHER;
	dev->watchdog_timeo = HZ*3;	//modified by john, 0805

	if (dev_alloc_name(dev, ifname) < 0){
                DMESG("Oops: devname already taken! Trying wlan%%d...\n");
		ifname = "wlan%d";
		dev_alloc_name(dev, ifname);
        }
	
	if(rtl8180_init(dev)!=0){ 
		DMESG("Initialization failed");
		goto fail;
	}
	
	netif_carrier_off(dev);
	netif_stop_queue(dev);
	
	register_netdev(dev);
	
	rtl8180_proc_init_one(dev);
	
	/* init rfkill */
	r8187b_rfkill_init(dev);
	
	DMESG("Driver probe completed");
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	return dev;
#else
	return 0;	
#endif

	
fail:
	free_ieee80211(dev);
		
	DMESG("wlan driver load failed\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)	
	return NULL;
#else
	return -ENODEV;
#endif
	
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0) 
static void __devexit rtl8187_usb_disconnect(struct usb_interface *intf)
#else 
static void __devexit rtl8187_usb_disconnect(struct usb_device *udev, void *ptr)
#endif
{
	struct r8180_priv *priv = NULL;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	struct net_device *dev = usb_get_intfdata(intf);
#else
	struct net_device *dev = (struct net_device *)ptr;
#endif
 	if(dev){
		unregister_netdev(dev);
		
		priv=ieee80211_priv(dev);
		
		MgntActSet_RF_State(dev, eRfOff, RF_CHANGE_BY_SW);

#ifdef _RTL8187_EXT_PATCH_
		if(priv && priv->mshobj)
		{
			if(priv->mshobj->ext_patch_remove_proc)
				priv->mshobj->ext_patch_remove_proc(priv);
			priv->ieee80211->ext_patch_ieee80211_start_protocol = 0;
			priv->ieee80211->ext_patch_ieee80211_stop_protocol = 0;
			priv->ieee80211->ext_patch_ieee80211_probe_req_1 = 0;
			priv->ieee80211->ext_patch_ieee80211_probe_req_2 = 0;
			priv->ieee80211->ext_patch_ieee80211_association_req_1 = 0;
			priv->ieee80211->ext_patch_ieee80211_association_req_2 = 0;
			priv->ieee80211->ext_patch_ieee80211_assoc_resp_by_net_1 = 0;
			priv->ieee80211->ext_patch_ieee80211_assoc_resp_by_net_2 = 0;
			priv->ieee80211->ext_patch_ieee80211_rx_frame_softmac_on_auth =0;
			priv->ieee80211->ext_patch_ieee80211_rx_frame_softmac_on_deauth =0;
			priv->ieee80211->ext_patch_ieee80211_rx_frame_softmac_on_assoc_req = 0;
			priv->ieee80211->ext_patch_ieee80211_rx_frame_softmac_on_assoc_rsp = 0;
			priv->ieee80211->ext_patch_ieee80211_ext_stop_scan_wq_set_channel = 0;
			priv->ieee80211->ext_patch_ieee80211_process_probe_response_1 = 0;
			priv->ieee80211->ext_patch_ieee80211_rx_mgt_on_probe_req = 0;
			priv->ieee80211->ext_patch_ieee80211_rx_mgt_update_expire = 0;
			priv->ieee80211->ext_patch_ieee80211_rx_on_rx = 0;
			priv->ieee80211->ext_patch_get_beacon_get_probersp = 0;
			priv->ieee80211->ext_patch_ieee80211_xmit = 0;
			priv->ieee80211->ext_patch_ieee80211_rx_frame_get_hdrlen = 0;
			priv->ieee80211->ext_patch_ieee80211_rx_is_valid_framectl = 0;
			priv->ieee80211->ext_patch_ieee80211_rx_process_dataframe = 0;
			// priv->ieee80211->ext_patch_is_duplicate_packet = 0;
			priv->ieee80211->ext_patch_ieee80211_softmac_xmit_get_rate = 0;
			free_mshobj(&priv->mshobj);
		}
#endif // _RTL8187_EXT_PATCH_

		rtl8180_proc_remove_one(dev);
		
		rtl8180_down(dev);
		priv->rf_close(dev);

		//rtl8180_rtx_disable(dev);
		rtl8187_usb_deleteendpoints(dev);
#ifdef LED
		DeInitSwLeds(dev);
#endif
		rtl8180_irq_disable(dev);
		rtl8180_reset(dev);
		mdelay(10);

	}

#ifdef CPU_64BIT	
	if(priv->usb_buf)
		kfree(priv->usb_buf);
	if(priv->usb_pool) {
		dma_pool_destroy(priv->usb_pool);
		priv->usb_pool = NULL;
	}
#endif
	free_ieee80211(dev);
	DMESG("wlan driver removed");
}

/* fun with the built-in ieee80211 stack... */
extern int ieee80211_crypto_init(void);
extern void ieee80211_crypto_deinit(void);
extern int ieee80211_crypto_tkip_init(void);
extern void ieee80211_crypto_tkip_exit(void);
extern int ieee80211_crypto_ccmp_init(void);
extern void ieee80211_crypto_ccmp_exit(void);
extern int ieee80211_crypto_wep_init(void);
extern void ieee80211_crypto_wep_exit(void);

static int __init rtl8187_usb_module_init(void)
{
	int ret;

	ret = ieee80211_crypto_init();
	if (ret) {
		printk(KERN_ERR "ieee80211_crypto_init() failed %d\n", ret);
		return ret;
	}
	ret = ieee80211_crypto_tkip_init();
	if (ret) {
		printk(KERN_ERR "ieee80211_crypto_tkip_init() failed %d\n", ret);
		return ret;
	}
	ret = ieee80211_crypto_ccmp_init();
	if (ret) {
		printk(KERN_ERR "ieee80211_crypto_ccmp_init() failed %d\n", ret);
		return ret;
	}
	ret = ieee80211_crypto_wep_init();
	if (ret) {
		printk(KERN_ERR "ieee80211_crypto_wep_init() failed %d\n", ret);
		return ret;
	}

	printk("\nLinux kernel driver for RTL8187/RTL8187B based WLAN cards\n");
	printk("Copyright (c) 2004-2008, Realsil Wlan\n");
	DMESG("Initializing module");
	DMESG("Wireless extensions version %d", WIRELESS_EXT);
	rtl8180_proc_module_init();
	return usb_register(&rtl8187_usb_driver);
}

static void __exit rtl8187_usb_module_exit(void)
{
	r8187b_rfkill_exit();
	usb_deregister(&rtl8187_usb_driver);
	rtl8180_proc_module_remove();
	ieee80211_crypto_tkip_exit();
	ieee80211_crypto_ccmp_exit();
	ieee80211_crypto_wep_exit();
	ieee80211_crypto_deinit();

	DMESG("Exiting\n");
}


void rtl8180_try_wake_queue(struct net_device *dev, int pri)
{
	unsigned long flags;
	short enough_desc;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	
	spin_lock_irqsave(&priv->tx_lock,flags);
	enough_desc = check_nic_enought_desc(dev,pri);
        spin_unlock_irqrestore(&priv->tx_lock,flags);	
	
	if(enough_desc)
		ieee80211_wake_queue(priv->ieee80211);
}

#ifdef JOHN_HWSEC
void EnableHWSecurityConfig8187(struct net_device *dev)
{
        u8 SECR_value = 0x0;
	SECR_value = SCR_TxSecEnable | SCR_RxSecEnable;
        {
                write_nic_byte(dev, WPA_CONFIG,  0x7);//SECR_value |  SCR_UseDK ); 
        }
}

void setKey(struct net_device *dev, 
		u8 EntryNo,
		u8 KeyIndex, 
		u16 KeyType, 
		u8 *MacAddr, 
		u8 DefaultKey, 
		u32 *KeyContent )
{
	u32 TargetCommand = 0;
	u32 TargetContent = 0;
	u16 usConfig = 0;
	int i;
	usConfig |= BIT15 | (KeyType<<2) | (DefaultKey<<5) | KeyIndex;


	for(i=0 ; i<6 ; i++){
		TargetCommand  = i+6*EntryNo;
		TargetCommand |= BIT31|BIT16;

		if(i==0){//MAC|Config
			TargetContent = (u32)(*(MacAddr+0)) << 16|
						   (u32)(*(MacAddr+1)) << 24|
						   (u32)usConfig;

			write_nic_dword(dev, WCAMI, TargetContent); 
			write_nic_dword(dev, RWCAM, TargetCommand);
			//printk("setkey cam =%8x\n", read_cam(dev, i+6*EntryNo));
		} else if(i==1){//MAC
                        TargetContent = (u32)(*(MacAddr+2)) 	 |
                                        	   (u32)(*(MacAddr+3)) <<  8|
                                        	   (u32)(*(MacAddr+4)) << 16|
                                        	   (u32)(*(MacAddr+5)) << 24;
			write_nic_dword(dev, WCAMI, TargetContent); 
			write_nic_dword(dev, RWCAM, TargetCommand);
		} else {	//Key Material
			write_nic_dword(dev, WCAMI, (u32)(*(KeyContent+i-2)) ); 
			write_nic_dword(dev, RWCAM, TargetCommand);
		}
	}

}
#endif

/****************************************************************************
            --------------------------- RF power on/power off -----------------
*****************************************************************************/

/*
 * the interface for changing the rfkill state
 * @dev: the device of r8187b
 * @eRfPowerStateToSet: the state we need to change,
 * 	eRfOn: power on
 * 	eRfOff: power off
 *
 * This function should be called by the SCI interrupt handler when the
 * KEY_WLAN event happen(or install to the notify function of the SCI
 * interrupt) or called in the wifi_set function of the rfkill interface for
 * user-space, and also, it can be called to initialize the wifi state, and
 * called when suspend/resume.
 */

void r8187b_wifi_change_rfkill_state(struct net_device *dev, RT_RF_POWER_STATE eRfPowerStateToSet, bool report_rfkill)
{
	struct r8180_priv *priv = ieee80211_priv(dev);

	if (eRfPowerStateToSet == eRfOn)
		priv->ieee80211->bHwRadioOff = false;
	else
		priv->ieee80211->bHwRadioOff = true;

#ifdef CONFIG_RADIO_DEBUG 
	DMESG("SCI interrupt Methord Will Turn Radio %s",
		(priv->ieee80211->bHwRadioOff == true) ? "Off" : "On");
#endif

#ifdef LED //by lizhaoming
	if (priv->ieee80211->bHwRadioOff)
		priv->ieee80211->ieee80211_led_contorl(dev, LED_CTL_POWER_OFF);
	else
		priv->ieee80211->ieee80211_led_contorl(dev, LED_CTL_POWER_ON);
#endif

	MgntActSet_RF_State(dev, eRfPowerStateToSet, RF_CHANGE_BY_HW);

	/* report the rfkill state to the user-space via uevent interface */
	if (report_rfkill)
		r8187b_wifi_report_state(priv);
}

/***************************************************************************
     ------------------- module init / exit stubs ----------------
****************************************************************************/
module_init(rtl8187_usb_module_init);
module_exit(rtl8187_usb_module_exit);
