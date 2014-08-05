/* 
   This file contains wireless extension handlers.

   This is part of rtl8180 OpenSource driver.
   Copyright (C) Andrea Merello 2004-2005  <andreamrl@tiscali.it> 
   Released under the terms of GPL (General Public Licence)
   
   Parts of this driver are based on the GPL part 
   of the official realtek driver.
   
   Parts of this driver are based on the rtl8180 driver skeleton 
   from Patric Schenke & Andres Salomon.

   Parts of this driver are based on the Intel Pro Wireless 2100 GPL driver.
   
   We want to tanks the Authors of those projects and the Ndiswrapper 
   project Authors.
*/



#include "r8187.h"
#include "r8180_hw.h"
//added 1117
#include "ieee80211/ieee80211.h"
#ifdef ENABLE_DOT11D
#include "dot11d.h"
#endif


//#define RATE_COUNT 4
u32 rtl8180_rates[] = {1000000,2000000,5500000,11000000,
	6000000,9000000,12000000,18000000,24000000,36000000,48000000,54000000};
#define RATE_COUNT sizeof(rtl8180_rates)/(sizeof(rtl8180_rates[0]))

#ifdef _RTL8187_EXT_PATCH_
#define IW_MODE_MESH 11
static int r8180_wx_join_mesh(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
int r8180_wx_set_channel(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
static int r8180_wx_mesh_scan(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
static int r8180_wx_get_mesh_list(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
#endif

static int r8180_wx_get_freq(struct net_device *dev,
			     struct iw_request_info *a,
			     union iwreq_data *wrqu, char *b)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	return ieee80211_wx_get_freq(priv->ieee80211,a,wrqu,b);
}


#if 0

static int r8180_wx_set_beaconinterval(struct net_device *dev, struct iw_request_info *aa,
			  union iwreq_data *wrqu, char *b)
{
	int *parms = (int *)b;
	int bi = parms[0];
	
	struct r8180_priv *priv = ieee80211_priv(dev);
	if(priv->ieee80211->bHwRadioOff)
		return 0;
	down(&priv->wx_sem);
	DMESG("setting beacon interval to %x",bi);
	
	priv->ieee80211->beacon_interval=bi;
	rtl8180_commit(dev);
	up(&priv->wx_sem);
		
	return 0;	
}


static int r8180_wx_set_forceassociate(struct net_device *dev, struct iw_request_info *aa,
			  union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv=ieee80211_priv(dev);	
	int *parms = (int *)extra;
	if(priv->ieee80211->bHwRadioOff)
		return 0;

	priv->ieee80211->force_associate = (parms[0] > 0);
	

	return 0;
}

#endif
static int r8180_wx_get_mode(struct net_device *dev, struct iw_request_info *a,
			     union iwreq_data *wrqu, char *b)
{
	struct r8180_priv *priv=ieee80211_priv(dev);	

	return ieee80211_wx_get_mode(priv->ieee80211,a,wrqu,b);
}



static int r8180_wx_get_rate(struct net_device *dev, 
			     struct iw_request_info *info, 
			     union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	return ieee80211_wx_get_rate(priv->ieee80211,info,wrqu,extra);
}



static int r8180_wx_set_rate(struct net_device *dev, 
			     struct iw_request_info *info, 
			     union iwreq_data *wrqu, char *extra)
{
	int ret;
	struct r8180_priv *priv = ieee80211_priv(dev);	
	if(priv->ieee80211->bHwRadioOff)
		return 0;

	down(&priv->wx_sem);

	ret = ieee80211_wx_set_rate(priv->ieee80211,info,wrqu,extra);
	
	up(&priv->wx_sem);
	
	return ret;
}
#ifdef JOHN_IOCTL
u16 read_rtl8225(struct net_device *dev, u8 addr);
void write_rtl8225(struct net_device *dev, u8 adr, u16 data);
u32 john_read_rtl8225(struct net_device *dev, u8 adr);
void _write_rtl8225(struct net_device *dev, u8 adr, u16 data);

static int r8180_wx_read_regs(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	u8 addr = 0;
	u16 data1;

	down(&priv->wx_sem);

	
	get_user(addr,(u8*)wrqu->data.pointer);
	data1 = read_rtl8225(dev, addr);
	wrqu->data.length = data1;	

	up(&priv->wx_sem);
	return 0;
	 
}

static int r8180_wx_write_regs(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
        struct r8180_priv *priv = ieee80211_priv(dev);
        u8 addr = 0;

        down(&priv->wx_sem);
             
        get_user(addr, (u8*)wrqu->data.pointer);
	write_rtl8225(dev, addr, wrqu->data.length);

        up(&priv->wx_sem);
	return 0;

}

void rtl8187_write_phy(struct net_device *dev, u8 adr, u32 data);
u8 rtl8187_read_phy(struct net_device *dev,u8 adr, u32 data);

static int r8180_wx_read_bb(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
        struct r8180_priv *priv = ieee80211_priv(dev);
	u8 databb;
#if 0 
	int i;
	for(i=0;i<12;i++) printk("%8x\n", read_cam(dev, i) );
#endif

        down(&priv->wx_sem);

	databb = rtl8187_read_phy(dev, (u8)wrqu->data.length, 0x00000000);
	wrqu->data.length = databb;

	up(&priv->wx_sem);
	return 0;
}

void rtl8187_write_phy(struct net_device *dev, u8 adr, u32 data);
static int r8180_wx_write_bb(struct net_device *dev,
                               struct iw_request_info *info,
                               union iwreq_data *wrqu, char *extra)
{
        struct r8180_priv *priv = ieee80211_priv(dev);
        u8 databb = 0;

        down(&priv->wx_sem);

        get_user(databb, (u8*)wrqu->data.pointer);
        rtl8187_write_phy(dev, wrqu->data.length, databb);

        up(&priv->wx_sem);
        return 0;

}


static int r8180_wx_write_nicb(struct net_device *dev,
                               struct iw_request_info *info,
                               union iwreq_data *wrqu, char *extra)
{
        struct r8180_priv *priv = ieee80211_priv(dev);
        u32 addr = 0;

        down(&priv->wx_sem);
	
        get_user(addr, (u32*)wrqu->data.pointer);
        write_nic_byte(dev, addr, wrqu->data.length);

        up(&priv->wx_sem);
        return 0;

}
static int r8180_wx_read_nicb(struct net_device *dev,
                               struct iw_request_info *info,
                               union iwreq_data *wrqu, char *extra)
{
        struct r8180_priv *priv = ieee80211_priv(dev);
        u32 addr = 0;
        u16 data1;

        down(&priv->wx_sem);

        get_user(addr,(u32*)wrqu->data.pointer);
        data1 = read_nic_byte(dev, addr);
        wrqu->data.length = data1;

        up(&priv->wx_sem);
        return 0;
}

static inline int is_same_network(struct ieee80211_network *src,
                                  struct ieee80211_network *dst,
				  struct ieee80211_device *ieee)
{
        /* A network is only a duplicate if the channel, BSSID, ESSID
         * and the capability field (in particular IBSS and BSS) all match.  
         * We treat all <hidden> with the same BSSID and channel
         * as one network */
        return (((src->ssid_len == dst->ssid_len)||(ieee->iw_mode == IW_MODE_INFRA)) &&  //YJ,mod, 080819,for hidden ap
			//((src->ssid_len == dst->ssid_len) &&
			(src->channel == dst->channel) &&
			!memcmp(src->bssid, dst->bssid, ETH_ALEN) &&
			(!memcmp(src->ssid, dst->ssid, src->ssid_len)||(ieee->iw_mode == IW_MODE_INFRA)) &&  //YJ,mod, 080819,for hidden ap 
			//!memcmp(src->ssid, dst->ssid, src->ssid_len) &&
			((src->capability & WLAN_CAPABILITY_IBSS) ==
			(dst->capability & WLAN_CAPABILITY_IBSS)) &&
			((src->capability & WLAN_CAPABILITY_BSS) ==
			(dst->capability & WLAN_CAPABILITY_BSS)));
}

static int r8180_wx_get_ap_status(struct net_device *dev,
                               struct iw_request_info *info,
                               union iwreq_data *wrqu, char *extra)
{
        struct r8180_priv *priv = ieee80211_priv(dev);
        struct ieee80211_device *ieee = priv->ieee80211;
        struct ieee80211_network *target;
	int name_len;

        down(&priv->wx_sem);

	//count the length of input ssid
	for(name_len=0 ; ((char*)wrqu->data.pointer)[name_len]!='\0' ; name_len++);

	//search for the correspoding info which is received
        list_for_each_entry(target, &ieee->network_list, list) {
                if ( (target->ssid_len == name_len) &&
		     (strncmp(target->ssid, (char*)wrqu->data.pointer, name_len)==0)){
			if( ((jiffies-target->last_scanned)/HZ > 1) && (ieee->state == IEEE80211_LINKED) && (is_same_network(&ieee->current_network,target, ieee)) )
				wrqu->data.length = 999;
			else
				wrqu->data.length = target->SignalStrength;
			if(target->wpa_ie_len>0 || target->rsn_ie_len>0 )
				//set flags=1 to indicate this ap is WPA
				wrqu->data.flags = 1;
			else wrqu->data.flags = 0;
		

		break;
                }
        }

	if (&target->list == &ieee->network_list){
		wrqu->data.flags = 3;
	}
        up(&priv->wx_sem);
        return 0;
}



#endif

static int r8180_wx_set_rawtx(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	int ret;
		
	if(priv->ieee80211->bHwRadioOff)
		return 0;

	down(&priv->wx_sem);
	
	ret = ieee80211_wx_set_rawtx(priv->ieee80211, info, wrqu, extra);
	
	up(&priv->wx_sem);
	
	return ret;
	 
}

static int r8180_wx_set_crcmon(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	int *parms = (int *)extra;
	int enable = (parms[0] > 0);
	short prev = priv->crcmon;
	
	if(priv->ieee80211->bHwRadioOff)
		return 0;

	down(&priv->wx_sem);
	
	if(enable) 
		priv->crcmon=1;
	else 
		priv->crcmon=0;

	DMESG("bad CRC in monitor mode are %s", 
	      priv->crcmon ? "accepted" : "rejected");

	if(prev != priv->crcmon && priv->up){
		rtl8180_down(dev);
		rtl8180_up(dev);
	}
	
	up(&priv->wx_sem);
	
	return 0;
}

static int r8180_wx_set_mode(struct net_device *dev, struct iw_request_info *a,
			     union iwreq_data *wrqu, char *b)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	int ret;	
	if(priv->ieee80211->bHwRadioOff)
		return 0;

#ifdef _RTL8187_EXT_PATCH_
	if (priv->mshobj && (priv->ieee80211->iw_ext_mode==11)) return 0;	
#endif
	down(&priv->wx_sem);

#ifdef CONFIG_IPS
	if(priv->bInactivePs){
		if(wrqu->mode != IW_MODE_INFRA){ 
			down(&priv->ieee80211->ips_sem);
			IPSLeave(dev);	
			up(&priv->ieee80211->ips_sem);	
		}
	}
#endif
	ret = ieee80211_wx_set_mode(priv->ieee80211,a,wrqu,b);
	
	rtl8187_set_rxconf(dev);
	
	up(&priv->wx_sem);
	return ret;
}


//YJ,add,080819,for hidden ap
struct  iw_range_with_scan_capa
{
        /* Informative stuff (to choose between different interface) */
        __u32           throughput;     /* To give an idea... */
        /* In theory this value should be the maximum benchmarked
         * TCP/IP throughput, because with most of these devices the
         * bit rate is meaningless (overhead an co) to estimate how
         * fast the connection will go and pick the fastest one.
         * I suggest people to play with Netperf or any benchmark...
         */

        /* NWID (or domain id) */
        __u32           min_nwid;       /* Minimal NWID we are able to set */
        __u32           max_nwid;       /* Maximal NWID we are able to set */

        /* Old Frequency (backward compat - moved lower ) */
        __u16           old_num_channels;
        __u8            old_num_frequency;

        /* Scan capabilities */
        __u8            scan_capa;       
};
//YJ,add,080819,for hidden ap

static int rtl8180_wx_get_range(struct net_device *dev, 
				struct iw_request_info *info, 
				union iwreq_data *wrqu, char *extra)
{
	struct iw_range *range = (struct iw_range *)extra;
	struct r8180_priv *priv = ieee80211_priv(dev);
	u16 val;
	int i;
	struct iw_range_with_scan_capa* tmp = (struct iw_range_with_scan_capa*)range; //YJ,add,080819,for hidden ap

	wrqu->data.length = sizeof(*range);
	memset(range, 0, sizeof(*range));

	/* Let's try to keep this struct in the same order as in
	 * linux/include/wireless.h
	 */
	
	/* TODO: See what values we can set, and remove the ones we can't
	 * set, or fill them with some default data.
	 */

	/* ~5 Mb/s real (802.11b) */
	range->throughput = 5 * 1000 * 1000;     

	// TODO: Not used in 802.11b?
//	range->min_nwid;	/* Minimal NWID we are able to set */
	// TODO: Not used in 802.11b?
//	range->max_nwid;	/* Maximal NWID we are able to set */
	
        /* Old Frequency (backward compat - moved lower ) */
//	range->old_num_channels; 
//	range->old_num_frequency;
//	range->old_freq[6]; /* Filler to keep "version" at the same offset */
	if(priv->rf_set_sens != NULL)
		range->sensitivity = priv->max_sens;	/* signal level threshold range */
	
	range->max_qual.qual = 100;
	/* TODO: Find real max RSSI and stick here */
	range->max_qual.level = 0;
	range->max_qual.noise = -98;
	range->max_qual.updated = 7; /* Updated all three */

	range->avg_qual.qual = 92; /* > 8% missed beacons is 'bad' */
	/* TODO: Find real 'good' to 'bad' threshol value for RSSI */
	range->avg_qual.level = 20 + -98;
	range->avg_qual.noise = 0;
	range->avg_qual.updated = 7; /* Updated all three */

	range->num_bitrates = RATE_COUNT;
	
	for (i = 0; i < RATE_COUNT && i < IW_MAX_BITRATES; i++) {
		range->bitrate[i] = rtl8180_rates[i];
	}
	
	range->min_frag = MIN_FRAG_THRESHOLD;
	range->max_frag = MAX_FRAG_THRESHOLD;
	
	range->pm_capa = 0;

	range->we_version_compiled = WIRELESS_EXT;
	range->we_version_source = 16;

//	range->retry_capa;	/* What retry options are supported */
//	range->retry_flags;	/* How to decode max/min retry limit */
//	range->r_time_flags;	/* How to decode max/min retry life */
//	range->min_retry;	/* Minimal number of retries */
//	range->max_retry;	/* Maximal number of retries */
//	range->min_r_time;	/* Minimal retry lifetime */
//	range->max_r_time;	/* Maximal retry lifetime */

        range->num_channels = 14;

	for (i = 0, val = 0; i < 14; i++) {
		
		// Include only legal frequencies for some countries
#ifdef ENABLE_DOT11D
		if ((GET_DOT11D_INFO(priv->ieee80211)->channel_map)[i+1]) {
#else
		if ((priv->ieee80211->channel_map)[i+1]) {
#endif
		        range->freq[val].i = i + 1;
			range->freq[val].m = ieee80211_wlan_frequencies[i] * 100000;
			range->freq[val].e = 1;
			val++;
		} else {
			// FIXME: do we need to set anything for channels
			// we don't use ?
		}
		
		if (val == IW_MAX_FREQUENCIES)
		break;
	}

	range->num_frequency = val;
	range->enc_capa = IW_ENC_CAPA_WPA | IW_ENC_CAPA_WPA2 |
                          IW_ENC_CAPA_CIPHER_TKIP | IW_ENC_CAPA_CIPHER_CCMP;

	tmp->scan_capa = 0x01; //YJ,add,080819,for hidden ap
	
	return 0;
}


static int r8180_wx_set_scan(struct net_device *dev, struct iw_request_info *a,
			     union iwreq_data *wrqu, char *b)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device* ieee = priv->ieee80211;
	int ret;
		
	if(priv->ieee80211->bHwRadioOff)
		return 0;
	//printk("==============>%s()\n",__FUNCTION__);
	if(!priv->up) 
		return -1;

	if (wrqu->data.flags & IW_SCAN_THIS_ESSID)
	{
		struct iw_scan_req* req = (struct iw_scan_req*)b;
		if (req->essid_len)
		{
			ieee->current_network.ssid_len = req->essid_len;
			memcpy(ieee->current_network.ssid, req->essid, req->essid_len);
		}
	}

	//set Tr switch to hardware control to scan more bss
	if(priv->TrSwitchState == TR_SW_TX) {
		//YJ,add,080611
		write_nic_byte(dev, RFPinsSelect, (u8)(priv->wMacRegRfPinsSelect));
		write_nic_byte(dev, RFPinsOutput, (u8)(priv->wMacRegRfPinsOutput));
		//YJ,add,080611,end
		priv->TrSwitchState = TR_HW_CONTROLLED;
	}
#ifdef _RTL8187_EXT_PATCH_ 
	if((priv->ieee80211->iw_mode == IW_MODE_MESH) && (priv->ieee80211->iw_ext_mode == IW_MODE_MESH)){
		r8180_wx_mesh_scan(dev,a,wrqu,b);
		ret = 0;
	}
	else
#endif
	{
		down(&priv->wx_sem);
		if(priv->ieee80211->state != IEEE80211_LINKED){
			//printk("===>start no link scan\n");
			//ieee80211_start_scan(priv->ieee80211);	
			//lzm mod 090115 because wq can't scan complete once
			//because after start protocal wq scan is in doing
			//so we should stop it first. 
			ieee80211_stop_scan(priv->ieee80211);
			ieee80211_start_scan_syncro(priv->ieee80211);
			ret = 0;
		} else {
			ret = ieee80211_wx_set_scan(priv->ieee80211,a,wrqu,b);
		}
		up(&priv->wx_sem);
	}
	return ret;
}


static int r8180_wx_get_scan(struct net_device *dev, struct iw_request_info *a,
			     union iwreq_data *wrqu, char *b)
{

	int ret;
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	if(!priv->up) return -1;
#ifdef _RTL8187_EXT_PATCH_
	if((priv->ieee80211->iw_mode == IW_MODE_MESH) && (priv->ieee80211->iw_ext_mode == IW_MODE_MESH)){
		ret = r8180_wx_get_mesh_list(dev, a, wrqu, b);
	}
	else
#endif
	{
	down(&priv->wx_sem);

	ret = ieee80211_wx_get_scan(priv->ieee80211,a,wrqu,b);
		
	up(&priv->wx_sem);
	}
	return ret;
}


static int r8180_wx_set_essid(struct net_device *dev, 
			      struct iw_request_info *a,
			      union iwreq_data *wrqu, char *b)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	int ret;
#ifdef _RTL8187_EXT_PATCH_ 
	struct ieee80211_device *ieee = priv->ieee80211;	
	char ch = 0;
	char tmpmeshid[32];
	char *p;
	int tmpmeshid_len=0;
	int i;
	short proto_started;	
#endif
	if(priv->ieee80211->bHwRadioOff)
		return 0;
	//printk("==========>%s()\n",__FUNCTION__);
	down(&priv->wx_sem);

#ifdef _RTL8187_EXT_PATCH_ 
	if((priv->ieee80211->iw_mode == IW_MODE_MESH) && (priv->ieee80211->iw_ext_mode == IW_MODE_MESH)){
		if (wrqu->essid.length > IW_ESSID_MAX_SIZE){
			ret= -E2BIG;
			goto out;
		}
		if (wrqu->essid.flags && (wrqu->essid.length > 1)) {
			memset(tmpmeshid,0,32);
#if LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,20)  
			tmpmeshid_len=wrqu->essid.length;
#else
			tmpmeshid_len=wrqu->essid.length + 1;
#endif
			p=b+tmpmeshid_len-2;
			for(i=tmpmeshid_len-1;i>0;i--)
			{
				if((*p)=='@')
					break;
				p--;
			}
			if((i == 0) || (i == 1)){
				printk("error:wrong meshid\n");
				ret = -1;
				goto out;
			}	
				
			memcpy(tmpmeshid,b,(i-1));
			p++;
			 if((tmpmeshid_len-1-i)==1)
                        {
                                if(*p > '9'|| *p <= '0'){
                                        goto out;
                                } else {
                                        ch = *p - '0';
                                }
                        }
                        else if((tmpmeshid_len-1-i)==2)
                        {
                                if((*p == '1') && (*(p+1) >= '0') && (*(p+1) <= '9'))
                                        ch = (*p - '0') * 10 + (*(p+1) - '0');
                                else
                                        goto out;
                        }
                        else {
                                ret = 0;
                                goto out;
                        }
                        if(ch > 14)
                        {
                                ret = 0;
                                printk("channel is invalid: %d\n",ch);
                                goto out;
                        }
			ieee->sync_scan_hurryup = 1;

			proto_started = ieee->proto_started;
			if(proto_started)
				ieee80211_stop_protocol(ieee);
	
			printk("==============>tmpmeshid is %s\n",tmpmeshid);
			priv->mshobj->ext_patch_r8180_wx_set_meshID(dev, tmpmeshid);
			priv->mshobj->ext_patch_r8180_wx_set_mesh_chan(dev,ch);
			r8180_wx_set_channel(dev, NULL, NULL, &ch);
			if (proto_started)
			        ieee80211_start_protocol(ieee);
		}
		else{
			printk("BUG:meshid is null\n");
			ret=0;
			goto out;
		}
	
		ret = 0;
	}
	else
#endif
	{
		ret = ieee80211_wx_set_essid(priv->ieee80211,a,wrqu,b);
	}

#ifdef _RTL8187_EXT_PATCH_ 
out:
#endif	
	up(&priv->wx_sem);
	return ret;
}


static int r8180_wx_get_essid(struct net_device *dev, 
			      struct iw_request_info *a,
			      union iwreq_data *wrqu, char *b)
{
	int ret;
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	down(&priv->wx_sem);
	
	ret = ieee80211_wx_get_essid(priv->ieee80211, a, wrqu, b);

	up(&priv->wx_sem);
	
	return ret;
}


static int r8180_wx_set_freq(struct net_device *dev, struct iw_request_info *a,
			     union iwreq_data *wrqu, char *b)
{
	int ret;
	struct r8180_priv *priv = ieee80211_priv(dev);
		
	if(priv->ieee80211->bHwRadioOff)
		return 0;

	down(&priv->wx_sem);
	
	ret = ieee80211_wx_set_freq(priv->ieee80211, a, wrqu, b);
	
	up(&priv->wx_sem);
	return ret;
}

static int r8180_wx_get_name(struct net_device *dev, 
			     struct iw_request_info *info, 
			     union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	return ieee80211_wx_get_name(priv->ieee80211, info, wrqu, extra);
}


static int r8180_wx_set_frag(struct net_device *dev, 
			     struct iw_request_info *info, 
			     union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	if(priv->ieee80211->bHwRadioOff)
		return 0;

	if (wrqu->frag.disabled)
		priv->ieee80211->fts = DEFAULT_FRAG_THRESHOLD;
	else {
		if (wrqu->frag.value < MIN_FRAG_THRESHOLD ||
		    wrqu->frag.value > MAX_FRAG_THRESHOLD)
			return -EINVAL;
		
		priv->ieee80211->fts = wrqu->frag.value & ~0x1;
	}

	return 0;
}


static int r8180_wx_get_frag(struct net_device *dev, 
			     struct iw_request_info *info, 
			     union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);

	wrqu->frag.value = priv->ieee80211->fts;
	wrqu->frag.fixed = 0;	/* no auto select */
	wrqu->frag.disabled = (wrqu->frag.value == DEFAULT_FRAG_THRESHOLD);

	return 0;
}


static int r8180_wx_set_wap(struct net_device *dev,
			 struct iw_request_info *info,
			 union iwreq_data *awrq,
			 char *extra)
{
	int ret;
	struct r8180_priv *priv = ieee80211_priv(dev);	
	if(priv->ieee80211->bHwRadioOff)
		return 0;

	//printk("in function %s\n",__FUNCTION__);	
#ifdef _RTL8187_EXT_PATCH_
        if (priv->mshobj && (priv->ieee80211->iw_ext_mode==11)){
		return 0;
	}
#endif
	down(&priv->wx_sem);
	
	ret = ieee80211_wx_set_wap(priv->ieee80211,info,awrq,extra);
	
	up(&priv->wx_sem);
	return ret;
	
}
	

static int r8180_wx_get_wap(struct net_device *dev, 
			    struct iw_request_info *info, 
			    union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	return ieee80211_wx_get_wap(priv->ieee80211,info,wrqu,extra);
}


static int r8180_wx_get_enc(struct net_device *dev, 
			    struct iw_request_info *info, 
			    union iwreq_data *wrqu, char *key)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	return ieee80211_wx_get_encode(priv->ieee80211, info, wrqu, key);
}

static int r8180_wx_set_enc(struct net_device *dev, 
			    struct iw_request_info *info, 
			    union iwreq_data *wrqu, char *key)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	int ret;
#ifdef 	JOHN_HWSEC
//	struct ieee80211_device *ieee = priv->ieee80211;
//	u32 TargetContent;
	u32 hwkey[4]={0,0,0,0};
	u8 mask=0xff;
	u32 key_idx=0;
	u8 broadcast_addr[6] ={	0xff,0xff,0xff,0xff,0xff,0xff}; 
	u8 zero_addr[4][6] ={	{0x00,0x00,0x00,0x00,0x00,0x00},
				{0x00,0x00,0x00,0x00,0x00,0x01}, 
				{0x00,0x00,0x00,0x00,0x00,0x02}, 
				{0x00,0x00,0x00,0x00,0x00,0x03} };
	int i;

#endif
	
	if(priv->ieee80211->bHwRadioOff)
		return 0;

	down(&priv->wx_sem);
	
	DMESG("Setting SW wep key");
	ret = ieee80211_wx_set_encode(priv->ieee80211,info,wrqu,key);

	up(&priv->wx_sem);

#ifdef	JOHN_HWSEC

	//sometimes, the length is zero while we do not type key value
	if(wrqu->encoding.length!=0){

		for(i=0 ; i<4 ; i++){
			hwkey[i] |=  key[4*i+0]&mask;
			if(i==1&&(4*i+1)==wrqu->encoding.length) mask=0x00;
			if(i==3&&(4*i+1)==wrqu->encoding.length) mask=0x00;
			hwkey[i] |= (key[4*i+1]&mask)<<8;
			hwkey[i] |= (key[4*i+2]&mask)<<16;
			hwkey[i] |= (key[4*i+3]&mask)<<24;
		}

		#define CONF_WEP40  0x4
		#define CONF_WEP104 0x14

		switch(wrqu->encoding.flags){
			case 0:
			case 1:	key_idx = 0; break;
			case 2:	key_idx = 1; break;
			case 3:	key_idx = 2; break;
			case 4:	key_idx	= 3; break;
			default: break;
		}

		if(wrqu->encoding.length==0x5){
			setKey( dev,
				key_idx,                //EntryNo
				key_idx,                //KeyIndex 
				KEY_TYPE_WEP40,         //KeyType
				zero_addr[key_idx],
				0,                      //DefaultKey
				hwkey);                 //KeyContent

			if(key_idx == 0){

				write_nic_byte(dev, WPA_CONFIG, 7);

				setKey( dev,
					4,                      //EntryNo
					key_idx,                      //KeyIndex 
					KEY_TYPE_WEP40,        //KeyType
					broadcast_addr,         //addr
					0,                      //DefaultKey
					hwkey);                 //KeyContent
			}
		}

		else if(wrqu->encoding.length==0xd){
			setKey( dev,
				key_idx,                //EntryNo
				key_idx,                //KeyIndex 
				KEY_TYPE_WEP104,        //KeyType
				zero_addr[key_idx],
				0,                      //DefaultKey
				hwkey);                 //KeyContent
 
			if(key_idx == 0){

				write_nic_byte(dev, WPA_CONFIG, 7);
		
				setKey( dev,
					4,                      //EntryNo
					key_idx,                      //KeyIndex 
					KEY_TYPE_WEP104,        //KeyType
					broadcast_addr,         //addr
					0,                      //DefaultKey
					hwkey);                 //KeyContent
			}
		}
		else printk("wrong type in WEP, not WEP40 and WEP104\n");

	}

	//consider the setting different key index situation
	//wrqu->encoding.flags = 801 means that we set key with index "1"
	if(wrqu->encoding.length==0 && (wrqu->encoding.flags >>8) == 0x8 ){
		
		write_nic_byte(dev, WPA_CONFIG, 7);

		//copy wpa config from default key(key0~key3) to broadcast key(key5)
		//
		key_idx = (wrqu->encoding.flags & 0xf)-1 ;
		write_cam(dev, (4*6),   0xffff0000|read_cam(dev, key_idx*6) );
		write_cam(dev, (4*6)+1, 0xffffffff);
		write_cam(dev, (4*6)+2, read_cam(dev, (key_idx*6)+2) );
		write_cam(dev, (4*6)+3, read_cam(dev, (key_idx*6)+3) );
		write_cam(dev, (4*6)+4, read_cam(dev, (key_idx*6)+4) );
		write_cam(dev, (4*6)+5, read_cam(dev, (key_idx*6)+5) );
	}

#endif /*JOHN_HWSEC*/
	return ret;
}


static int r8180_wx_set_scan_type(struct net_device *dev, struct iw_request_info *aa, union
 iwreq_data *wrqu, char *p){
  
 	struct r8180_priv *priv = ieee80211_priv(dev);
	int *parms=(int*)p;
	int mode=parms[0];
		
	if(priv->ieee80211->bHwRadioOff)
		return 0;

	priv->ieee80211->active_scan = mode;
	
	return 1;
}



static int r8180_wx_set_retry(struct net_device *dev, 
				struct iw_request_info *info, 
				union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	int err = 0;
		
	if(priv->ieee80211->bHwRadioOff)
		return 0;

	down(&priv->wx_sem);
	
	if (wrqu->retry.flags & IW_RETRY_LIFETIME || 
	    wrqu->retry.disabled){
		err = -EINVAL;
		goto exit;
	}
	if (!(wrqu->retry.flags & IW_RETRY_LIMIT)){
		err = -EINVAL;
		goto exit;
	}

	if(wrqu->retry.value > R8180_MAX_RETRY){
		err= -EINVAL;
		goto exit;
	}
	if (wrqu->retry.flags & IW_RETRY_MAX) {
		priv->retry_rts = wrqu->retry.value;
		DMESG("Setting retry for RTS/CTS data to %d", wrqu->retry.value);
	
	}else {
		priv->retry_data = wrqu->retry.value;
		DMESG("Setting retry for non RTS/CTS data to %d", wrqu->retry.value);
	}
	
	/* FIXME ! 
	 * We might try to write directly the TX config register
	 * or to restart just the (R)TX process.
	 * I'm unsure if whole reset is really needed
	 */

 	rtl8180_commit(dev);
	/*
	if(priv->up){
		rtl8180_rtx_disable(dev);
		rtl8180_rx_enable(dev);
		rtl8180_tx_enable(dev);
			
	}
	*/
exit:
	up(&priv->wx_sem);
	
	return err;
}

static int r8180_wx_get_retry(struct net_device *dev, 
				struct iw_request_info *info, 
				union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	

	wrqu->retry.disabled = 0; /* can't be disabled */

	if ((wrqu->retry.flags & IW_RETRY_TYPE) == 
	    IW_RETRY_LIFETIME) 
		return -EINVAL;
	
	if (wrqu->retry.flags & IW_RETRY_MAX) {
		wrqu->retry.flags = IW_RETRY_LIMIT & IW_RETRY_MAX;
		wrqu->retry.value = priv->retry_rts;
	} else {
		wrqu->retry.flags = IW_RETRY_LIMIT & IW_RETRY_MIN;
		wrqu->retry.value = priv->retry_data;
	}
	//DMESG("returning %d",wrqu->retry.value);
	

	return 0;
}

static int r8180_wx_get_sens(struct net_device *dev, 
				struct iw_request_info *info, 
				union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	if(priv->rf_set_sens == NULL) 
		return -1; /* we have not this support for this radio */
	wrqu->sens.value = priv->sens;
	return 0;
}


static int r8180_wx_set_sens(struct net_device *dev, 
				struct iw_request_info *info, 
				union iwreq_data *wrqu, char *extra)
{
	
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	short err = 0;	

	if(priv->ieee80211->bHwRadioOff)
		return 0;

	down(&priv->wx_sem);
	//DMESG("attempt to set sensivity to %ddb",wrqu->sens.value);
	if(priv->rf_set_sens == NULL) {
		err= -1; /* we have not this support for this radio */
		goto exit;
	}
	if(priv->rf_set_sens(dev, wrqu->sens.value) == 0)
		priv->sens = wrqu->sens.value;
	else
		err= -EINVAL;

exit:
	up(&priv->wx_sem);
	
	return err;
}


static int dummy(struct net_device *dev, struct iw_request_info *a,
		 union iwreq_data *wrqu,char *b)
{
	return -1;
}
static int r8180_wx_set_enc_ext(struct net_device *dev,
                                        struct iw_request_info *info,
                                        union iwreq_data *wrqu, char *extra)
{
	
	struct r8180_priv *priv = ieee80211_priv(dev);
	//printk("===>%s()\n", __FUNCTION__);

	int ret=0;
	
	if(priv->ieee80211->bHwRadioOff)
		return 0;

	down(&priv->wx_sem);
	ret = ieee80211_wx_set_encode_ext(priv->ieee80211, info, wrqu, extra);
	up(&priv->wx_sem);
	return ret;	

}
static int r8180_wx_set_auth(struct net_device *dev,
                                        struct iw_request_info *info,
                                        union iwreq_data* data, char *extra)
{
	//printk("====>%s()\n", __FUNCTION__);
	struct r8180_priv *priv = ieee80211_priv(dev);
	int ret=0;
	
	if(priv->ieee80211->bHwRadioOff)
		return 0;

	down(&priv->wx_sem);
	ret = ieee80211_wx_set_auth(priv->ieee80211, info, &(data->param), extra);
	up(&priv->wx_sem);
	return ret;
}

static int r8180_wx_set_mlme(struct net_device *dev,
                                        struct iw_request_info *info,
                                        union iwreq_data *wrqu, char *extra)
{
	//printk("====>%s()\n", __FUNCTION__);

	int ret=0;
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	if(priv->ieee80211->bHwRadioOff)
		return 0;

	down(&priv->wx_sem);
#if 1
	ret = ieee80211_wx_set_mlme(priv->ieee80211, info, wrqu, extra);
#endif
	up(&priv->wx_sem);
	return ret;
}

static int r8180_wx_set_gen_ie(struct net_device *dev,
                                        struct iw_request_info *info,
                                        union iwreq_data* data, char *extra)
{
	   //printk("====>%s(), len:%d\n", __FUNCTION__, data->length);
	int ret=0;
        struct r8180_priv *priv = ieee80211_priv(dev);
	
	if(priv->ieee80211->bHwRadioOff)
		return 0;

        down(&priv->wx_sem);
#if 1
        ret = ieee80211_wx_set_gen_ie(priv->ieee80211, extra, data->data.length);
#endif
        up(&priv->wx_sem);
	//printk("<======%s(), ret:%d\n", __FUNCTION__, ret);
        return ret;


}

#ifdef _RTL8187_EXT_PATCH_
/*
   Output:
     (case 1) Mesh: Enable. MESHID=[%s] (max length of %s is 32 bytes). 
     (case 2) Mesh: Disable.
*/
static int r8180_wx_get_meshinfo(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	if( ! priv->mshobj || !priv->mshobj->ext_patch_r8180_wx_get_meshinfo )
		return 0;
	return priv->mshobj->ext_patch_r8180_wx_get_meshinfo(dev, info, wrqu, extra);
}


static int r8180_wx_enable_mesh(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee80211;

	int ret = 0;
	
	if( ! priv->mshobj || !priv->mshobj->ext_patch_r8180_wx_enable_mesh )
		return 0;

	down(&priv->wx_sem);
	if(priv->mshobj->ext_patch_r8180_wx_enable_mesh(dev))
	{
		union iwreq_data tmprqu;
		tmprqu.mode = ieee->iw_mode;
		ieee->iw_mode = 0;
		ret = ieee80211_wx_set_mode(ieee, info, &tmprqu, extra);
		rtl8187_set_rxconf(dev);
	}

	up(&priv->wx_sem);
	
	return ret;
	
}

static int r8180_wx_disable_mesh(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee80211;

	int ret = 0;
	
	if( ! priv->mshobj || !priv->mshobj->ext_patch_r8180_wx_disable_mesh )
		return 0;

	down(&priv->wx_sem);
	if(priv->mshobj->ext_patch_r8180_wx_disable_mesh(dev))
	{
		union iwreq_data tmprqu;
		tmprqu.mode = ieee->iw_mode;
		ieee->iw_mode = 999;
		ret = ieee80211_wx_set_mode(ieee, info, &tmprqu, extra);
		rtl8187_set_rxconf(dev);
	}

	up(&priv->wx_sem);
	
	return ret;
}


int r8180_wx_set_channel(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	int ch = *extra;
	struct r8180_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee80211;
		
	if(priv->ieee80211->bHwRadioOff)
		return 0;

	// is 11s ?
	if (!priv->mshobj || (ieee->iw_mode != ieee->iw_ext_mode) || !priv->mshobj->ext_patch_r8180_wx_set_channel )
		return 0;	
			
	printk("set channel = %d\n", ch);		
	if ( ch < 0 ) 	
	{
		ieee80211_start_scan(ieee);			// auto
		ieee->meshScanMode =2;
	}
	else	
	{	
//#ifdef NETWORKMANAGER_UI
		if((priv->ieee80211->iw_mode == IW_MODE_MESH) && (priv->ieee80211->iw_ext_mode == IW_MODE_MESH)){
		}
//#else		
		else{
		down(&priv->wx_sem);}
//#endif
		ieee->meshScanMode =0;		
		// ieee->set_chan(dev, ch);
//#ifdef _RTL8187_EXT_PATCH_
		if(priv->mshobj->ext_patch_r8180_wx_set_channel)
		{
			priv->mshobj->ext_patch_r8180_wx_set_channel(ieee, ch);
			priv->mshobj->ext_patch_r8180_wx_set_mesh_chan(dev,ch);
		}
//#endif
		ieee->set_chan(ieee->dev, ch);
		ieee->current_network.channel = ch;
		queue_work(ieee->wq, &ieee->ext_stop_scan_wq);
		ieee80211_ext_send_11s_beacon(ieee);
//#ifdef NETWORKMANAGER_UI
		if((priv->ieee80211->iw_mode == IW_MODE_MESH) && (priv->ieee80211->iw_ext_mode == IW_MODE_MESH)){
		}
//#else		
		else{
		up(&priv->wx_sem);}
//#endif
		//up(&ieee->wx_sem);	
		
		// ieee80211_stop_scan(ieee);			// user set	
		//
	
		/*
		netif_carrier_off(ieee->dev);
		
		if (ieee->data_hard_stop)
			ieee->data_hard_stop(ieee->dev);
		
		ieee->state = IEEE80211_NOLINK;
		ieee->link_change(ieee->dev);
			
		ieee->current_network.channel = fwrq->m;
		ieee->set_chan(ieee->dev, ieee->current_network.channel);
		
			
		if (ieee->data_hard_resume)
			ieee->data_hard_resume(ieee->dev);	
		
		netif_carrier_on(ieee->dev);
		*/
		
	}
		
	return 0;
}

static int r8180_wx_set_meshID(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{	
	struct r8180_priv *priv = ieee80211_priv(dev);	
	
	if(priv->ieee80211->bHwRadioOff)
		return 0;

	if( ! priv->mshobj || !priv->mshobj->ext_patch_r8180_wx_set_meshID )
		return 0;

	//printk("len=%d\n", wrqu->data.length);
	//printk("\nCall setMeshid.");
	return priv->mshobj->ext_patch_r8180_wx_set_meshID(dev, wrqu->data.pointer);	
}


/* reserved for future
static int r8180_wx_add_mac_allow(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	if( ! priv->mshobj || !priv->mshobj->ext_patch_r8180_wx_set_add_mac_allow )
		return 0;

	return priv->mshobj->ext_patch_r8180_wx_set_add_mac_allow(dev, info, wrqu, extra);
}

static int r8180_wx_del_mac_allow(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	if( ! priv->mshobj || !priv->mshobj->ext_patch_r8180_wx_set_del_mac_allow )
		return 0;

	return priv->mshobj->ext_patch_r8180_wx_set_del_mac_allow(dev, info, wrqu, extra);
}
*/
static int r8180_wx_add_mac_deny(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	if( ! priv->mshobj || !priv->mshobj->ext_patch_r8180_wx_set_add_mac_deny )
		return 0;

	return priv->mshobj->ext_patch_r8180_wx_set_add_mac_deny(dev, info, wrqu, extra);
}

static int r8180_wx_del_mac_deny(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	if( ! priv->mshobj || !priv->mshobj->ext_patch_r8180_wx_set_del_mac_deny )
		return 0;

	return priv->mshobj->ext_patch_r8180_wx_set_del_mac_deny(dev, info, wrqu, extra);
}

/* reserved for future
static int r8180_wx_get_mac_allow(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	if( ! priv->mshobj || !priv->mshobj->ext_patch_r8180_wx_get_mac_allow )
		return 0;

	return priv->mshobj->ext_patch_r8180_wx_get_mac_allow(dev, info, wrqu, extra);
}
*/

static int r8180_wx_get_mac_deny(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	if( ! priv->mshobj || !priv->mshobj->ext_patch_r8180_wx_get_mac_deny )
		return 0;

	return priv->mshobj->ext_patch_r8180_wx_get_mac_deny(dev, info, wrqu, extra);
}


static int r8180_wx_get_mesh_list(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	if( ! priv->mshobj || !priv->mshobj->ext_patch_r8180_wx_get_mesh_list )
		return 0;

	return priv->mshobj->ext_patch_r8180_wx_get_mesh_list(dev, info, wrqu, extra);
}

static int r8180_wx_mesh_scan(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	
	if( ! priv->mshobj || !priv->mshobj->ext_patch_r8180_wx_mesh_scan )
		return 0;

	return priv->mshobj->ext_patch_r8180_wx_mesh_scan(dev, info, wrqu, extra);
}

static int r8180_wx_join_mesh(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{	
	struct r8180_priv *priv = ieee80211_priv(dev);	
	int index;
	int ret=0;
	char extmeshid[32];
	int len=0;
	char id[50], ch;
//#ifdef NETWORKMANAGER_UI

	if((priv->ieee80211->iw_mode == IW_MODE_MESH) && (priv->ieee80211->iw_ext_mode == IW_MODE_MESH)){
		printk("join mesh %s\n",extra);
		if (wrqu->essid.length > IW_ESSID_MAX_SIZE){
			ret= -E2BIG;
			goto out;
		}
		//printk("wrqu->essid.length is %d\n",wrqu->essid.length);
		//printk("wrqu->essid.flags is %d\n",wrqu->essid.flags);
		if((wrqu->essid.length == 1) && (wrqu->essid.flags == 1)){
			ret = 0;
			goto out;
		}
		if (wrqu->essid.flags && wrqu->essid.length) {
			if(priv->mshobj->ext_patch_r8180_wx_get_selected_mesh_channel(dev, extra, &ch))
			{
				priv->mshobj->ext_patch_r8180_wx_set_meshID(dev, extra); 
				priv->mshobj->ext_patch_r8180_wx_set_mesh_chan(dev,ch); 
				r8180_wx_set_channel(dev, NULL, NULL, &ch);
			}
			else
				printk("invalid mesh #\n");

		}
#if 0
		else{
			if(priv->mshobj->ext_patch_r8180_wx_get_selected_mesh_channel(dev, 0, &ch))
			{
				priv->mshobj->ext_patch_r8180_wx_set_meshID(dev, extra);
				priv->mshobj->ext_patch_r8180_wx_set_mesh_chan(dev,ch);  
				r8180_wx_set_channel(dev, NULL, NULL, &ch);
			}
			else
				printk("invalid mesh #\n");

		}
#endif
	}
	else{
//#else
	index = *(extra);
//	printk("index=%d\n", index);
	
	if( ! priv->mshobj 
		|| !priv->mshobj->ext_patch_r8180_wx_set_meshID 
		|| !priv->mshobj->ext_patch_r8180_wx_get_selected_mesh )		
		return 0;
	
	if( priv->mshobj->ext_patch_r8180_wx_get_selected_mesh(dev, index, &ch, id) )
	{
				//	printk("ch=%d, id=%s\n", ch, id);
		 priv->mshobj->ext_patch_r8180_wx_set_meshID(dev, id);	
			priv->mshobj->ext_patch_r8180_wx_set_mesh_chan(dev,ch);	
		r8180_wx_set_channel(dev, NULL, NULL, &ch);
	}
	else
		printk("invalid mesh #\n");
	}
//#endif
out:
	return ret;
}

#endif // _RTL8187_EXT_PATCH_


static int r8180_wx_get_radion(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
//	u8 addr;

	down(&priv->wx_sem);
	if(priv->radion == 1) {
		*(int *)extra = 1;
	} else {
			
		*(int *)extra = 0;
	}
	up(&priv->wx_sem);
	return 0;
	 
}

static int r8180_wx_set_radion(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	int radion = *extra;
	struct r8180_priv *priv = ieee80211_priv(dev);
//	struct ieee80211_device *ieee = priv->ieee80211;
	u8     btCR9346, btConfig3;	
	int    i;
	u16    u2bTFPC = 0;
	u8     u1bTmp;

	if(priv->ieee80211->bHwRadioOff)
		return 0;

	down(&priv->wx_sem);
	printk("set radion = %d\n", radion);

#ifdef _RTL8187_EXT_PATCH_
        if(ieee->iw_mode == ieee->iw_ext_mode) {
             printk("mesh mode:: could not set radi on/off = %d\n", radion);
	     up(&priv->wx_sem);
             return 0;
        }
#endif
	// Set EEM0 and EEM1 in 9346CR.
	btCR9346 = read_nic_byte(dev, CR9346);
	write_nic_byte(dev, CR9346, (btCR9346|0xC0) );
	// Set PARM_En in Config3.
	btConfig3 = read_nic_byte(dev, CONFIG3);
	write_nic_byte(dev, CONFIG3, (btConfig3|CONFIG3_PARM_En) );

	if ( radion == 1) 	//radion off
	{	
		printk("==================>RF on\n");
		write_nic_dword(dev, ANAPARAM, ANAPARM_ON);
		write_nic_dword(dev, ANAPARAM2, ANAPARM2_ON);
		write_nic_byte(dev, CONFIG4, (priv->RFProgType));
		
		write_nic_byte(dev, 0x085, 0x24); // 061219, SD3 ED: for minicard CCK power leakage issue.
		write_rtl8225(dev, 0x4, 0x9FF);

		u1bTmp = read_nic_byte(dev, 0x24E);
		write_nic_byte(dev, 0x24E, (u1bTmp & (~(BIT5|BIT6))) );// 070124 SD1 Alex: turn on CCK and OFDM.
		priv->radion = 1; //radion on
	}
	else	
	{	
		printk("==================>RF off\n");
		for(i = 0; i < MAX_DOZE_WAITING_TIMES_87B; i++)
		{ // Make sure TX FIFO is empty befor turn off RFE pwoer.
			u2bTFPC = read_nic_word(dev, TFPC);
			if(u2bTFPC == 0)
			{
				break;
			}
			else
			{
				printk("%d times TFPC: %d != 0 before doze!\n", (i+1), u2bTFPC);
				udelay(10);
			}
		}
		if( i == MAX_DOZE_WAITING_TIMES_87B )
		{
			printk("\n\n\n SetZebraRFPowerState8187B(): %d times TFPC: %d != 0 !!!\n\n\n",\
				      	MAX_DOZE_WAITING_TIMES_87B, u2bTFPC);
		}

		u1bTmp = read_nic_byte(dev, 0x24E);
		write_nic_byte(dev, 0x24E, (u1bTmp|BIT5|BIT6));// 070124 SD1 Alex: turn off CCK and OFDM.
		
		write_rtl8225(dev, 0x4,0x1FF); // Turn off RF first to prevent BB lock up, suggested by PJ, 2006.03.03.
		write_nic_byte(dev, 0x085, 0x04); // 061219, SD3 ED: for minicard CCK power leakage issue.

		write_nic_byte(dev, CONFIG4, (priv->RFProgType|Config4_PowerOff));

		write_nic_dword(dev, ANAPARAM, ANAPARM_OFF);
		write_nic_dword(dev, ANAPARAM2, ANAPARM2_OFF); // 070301, SD1 William: to reduce RF off power consumption to 80 mA.	
		priv->radion = 0; //radion off
	}
	// Clear PARM_En in Config3.
	btConfig3 &= ~(CONFIG3_PARM_En);
	write_nic_byte(dev, CONFIG3, btConfig3);
	// Clear EEM0 and EEM1 in 9346CR.
	btCR9346 &= ~(0xC0);
	write_nic_byte(dev, CR9346, btCR9346);

	up(&priv->wx_sem);

	return 0;
}

static int r8180_wx_set_ratadpt (struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	int ratadapt = *extra;
	struct r8180_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee80211;
		
	if(priv->ieee80211->bHwRadioOff)
		return 0;

	down(&priv->wx_sem);
	printk("Set rate adaptive %s\n", (ratadapt==0)?"on":"off");
	if(ratadapt == 0) {
	        del_timer_sync(&priv->rateadapter_timer);
        	cancel_delayed_work(&priv->ieee80211->rate_adapter_wq);
		priv->rateadapter_timer.function((unsigned long)dev);
	} else {
	        del_timer_sync(&priv->rateadapter_timer);
        	cancel_delayed_work(&priv->ieee80211->rate_adapter_wq);
		printk("force rate to %d\n", ratadapt);
		ieee->rate = ratadapt;
	}
	up(&priv->wx_sem);
	return 0;
}

#ifdef ENABLE_TOSHIBA_CONFIG 
static int r8180_wx_get_tblidx(struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	//extern u8 chan_plan_index;
	//printk("=========>%s(), %x\n", __FUNCTION__, priv->channel_plan);
	down(&priv->wx_sem);
	put_user(priv->channel_plan, (u8*)wrqu->data.pointer);	
	up(&priv->wx_sem);
	return 0;	

}

//This func will be called after probe auto
static int r8180_wx_set_tbl (struct net_device *dev, 
			       struct iw_request_info *info, 
			       union iwreq_data *wrqu, char *extra)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	u8 len = 0; 
	s8 err = -1;
	extern	CHANNEL_LIST Current_tbl;		
	down(&priv->wx_sem);
	if (!wrqu->data.pointer)
	{
		printk("user data pointer is null\n");
		goto exit;
	}
	len = wrqu->data.length;
	//printk("=========>%s(), len:%d\n", __FUNCTION__, len);
	//memset(&Current_tbl, 0, sizeof(CHANNEL_LIST));
	if (copy_from_user((u8*)&Current_tbl, (void*)wrqu->data.pointer, len))
	{
		printk("error copy from user\n");
		goto exit;
	}
	{
		int i;
		Current_tbl.Len = len;
		//printk("%d\n", Current_tbl.Len);

		Dot11d_Init(priv->ieee80211);
		priv->ieee80211->bGlobalDomain = false;
		priv->ieee80211->bWorldWide13 = false;

		//lzm add 081205
		priv->ieee80211->MinPassiveChnlNum=12;
		priv->ieee80211->IbssStartChnl= 10;

		for (i=0; i<Current_tbl.Len; i++){
			//printk("%2d ", Current_tbl.Channel[i]);
			if(priv->channel_plan == COUNTRY_CODE_ETSI)
			{
				if(Current_tbl.Channel[i] <= 11)
				{
#ifdef ENABLE_DOT11D
					GET_DOT11D_INFO(priv->ieee80211)->channel_map[Current_tbl.Channel[i]] = 1;
#else
					priv->ieee80211->channel_map[Current_tbl.Channel[i]] = 1;
#endif
				}
				else if((Current_tbl.Channel[i] >= 11) && (Current_tbl.Channel[i] <= 13))
				{
#ifdef ENABLE_DOT11D
					GET_DOT11D_INFO(priv->ieee80211)->channel_map[Current_tbl.Channel[i]] = 2;
#else
					priv->ieee80211->channel_map[Current_tbl.Channel[i]] = 2;
#endif
				}
			}
			else
			{
				if(Current_tbl.Channel[i] <= 14)
				{
#ifdef ENABLE_DOT11D
					GET_DOT11D_INFO(priv->ieee80211)->channel_map[Current_tbl.Channel[i]] = 1;
#else
					priv->ieee80211->channel_map[Current_tbl.Channel[i]] = 1;
#endif
				}
			}
		}
#if 0
		printk("\n");
		for(i=1; i<MAX_CHANNEL_NUMBER; i++)
		{
#ifdef ENABLE_DOT11D
			printk("%2d ", GET_DOT11D_INFO(priv->ieee80211)->channel_map[i]);
#else
			printk("%2d ", priv->ieee80211->channel_map[i]);
#endif
		}
		printk("\n");

#endif
		if(priv->ieee80211->proto_started)
		{//we need to restart protocol now if it was start before channel map
			ieee80211_softmac_stop_protocol(priv->ieee80211);
			//mdelay(1);
			ieee80211_softmac_start_protocol(priv->ieee80211); 
		}
	}
	err = 0;
exit:
	up(&priv->wx_sem);
	return err;	


}

#endif


static iw_handler r8180_wx_handlers[] =
{
        NULL,                     /* SIOCSIWCOMMIT */
        r8180_wx_get_name,   	  /* SIOCGIWNAME */
        dummy,                    /* SIOCSIWNWID */
        dummy,                    /* SIOCGIWNWID */
        r8180_wx_set_freq,        /* SIOCSIWFREQ */
        r8180_wx_get_freq,        /* SIOCGIWFREQ */
        r8180_wx_set_mode,        /* SIOCSIWMODE */
        r8180_wx_get_mode,        /* SIOCGIWMODE */
        r8180_wx_set_sens,        /* SIOCSIWSENS */
        r8180_wx_get_sens,        /* SIOCGIWSENS */
        NULL,                     /* SIOCSIWRANGE */
        rtl8180_wx_get_range,	  /* SIOCGIWRANGE */
        NULL,                     /* SIOCSIWPRIV */
        NULL,                     /* SIOCGIWPRIV */
        NULL,                     /* SIOCSIWSTATS */
        NULL,                     /* SIOCGIWSTATS */
        dummy,                    /* SIOCSIWSPY */
        dummy,                    /* SIOCGIWSPY */
        NULL,                     /* SIOCGIWTHRSPY */
        NULL,                     /* SIOCWIWTHRSPY */
        r8180_wx_set_wap,      	  /* SIOCSIWAP */
        r8180_wx_get_wap,         /* SIOCGIWAP */
        r8180_wx_set_mlme, //NULL,    /* SIOCSIWMLME*/                 /* -- hole -- */
        dummy,                     /* SIOCGIWAPLIST -- depricated */
        r8180_wx_set_scan,        /* SIOCSIWSCAN */
        r8180_wx_get_scan,        /* SIOCGIWSCAN */
        r8180_wx_set_essid,       /* SIOCSIWESSID */
        r8180_wx_get_essid,       /* SIOCGIWESSID */
        dummy,                    /* SIOCSIWNICKN */
        dummy,                    /* SIOCGIWNICKN */
        NULL,                     /* -- hole -- */
        NULL,                     /* -- hole -- */
        r8180_wx_set_rate,        /* SIOCSIWRATE */
        r8180_wx_get_rate,        /* SIOCGIWRATE */
        dummy,                    /* SIOCSIWRTS */
        dummy,                    /* SIOCGIWRTS */
        r8180_wx_set_frag,        /* SIOCSIWFRAG */
        r8180_wx_get_frag,        /* SIOCGIWFRAG */
        dummy,                    /* SIOCSIWTXPOW */
        dummy,                    /* SIOCGIWTXPOW */
        r8180_wx_set_retry,       /* SIOCSIWRETRY */
        r8180_wx_get_retry,       /* SIOCGIWRETRY */
        r8180_wx_set_enc,         /* SIOCSIWENCODE */
        r8180_wx_get_enc,         /* SIOCGIWENCODE */
        dummy,                    /* SIOCSIWPOWER */
        dummy,                    /* SIOCGIWPOWER */
		NULL,			/*---hole---*/
		NULL, 			/*---hole---*/
		r8180_wx_set_gen_ie,//NULL, 			/* SIOCSIWGENIE */
		NULL, 			/* SIOCSIWGENIE */
		r8180_wx_set_auth,//NULL, 			/* SIOCSIWAUTH */
		NULL,//r8180_wx_get_auth,//NULL, 			/* SIOCSIWAUTH */
		r8180_wx_set_enc_ext, 			/* SIOCSIWENCODEEXT */
		NULL,//r8180_wx_get_enc_ext,//NULL, 			/* SIOCSIWENCODEEXT */
		NULL, 			/* SIOCSIWPMKSA */
		NULL, 			 /*---hole---*/
}; 


static const struct iw_priv_args r8180_private_args[] = { 
	{
		SIOCIWFIRSTPRIV + 0x0, 
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "badcrc" 
	}, 
	
	{
		SIOCIWFIRSTPRIV + 0x1,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "activescan"
	
	},
	{
		SIOCIWFIRSTPRIV + 0x2, 
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rawtx" 
	},
#ifdef JOHN_IOCTL	
	{
		SIOCIWFIRSTPRIV + 0x3,
                IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "readRF"
	}
	,
	{
		SIOCIWFIRSTPRIV + 0x4,
                IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "writeRF"
	}
	,
	{
		SIOCIWFIRSTPRIV + 0x5,
                IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "readBB"
	}
	,
	{
		SIOCIWFIRSTPRIV + 0x6,
                IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "writeBB"
	}
    ,
    {
        SIOCIWFIRSTPRIV + 0x7,
                IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "readnicb"
    }
    ,
    {
        SIOCIWFIRSTPRIV + 0x8,
                IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "writenicb"
    }
    ,
    {
        SIOCIWFIRSTPRIV + 0x9,
                IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "apinfo"
    },
#endif
    {
        SIOCIWFIRSTPRIV + 0xA,
                0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED |1, "getradion"
    },
    {
        SIOCIWFIRSTPRIV + 0xB,
                IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setradion"
    },
    {
        SIOCIWFIRSTPRIV + 0xC,
                IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ratadpt"
    },
#ifdef ENABLE_TOSHIBA_CONFIG
   {
	SIOCIWFIRSTPRIV + 0xD, 
	0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "gettblidx" 
   },
   {
	SIOCIWFIRSTPRIV + 0xE, 
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,0, "settblidx" 
   },

#endif

};

/*
 * Private ioctl interface information
 
struct	iw_priv_args
{
//	__u32		cmd;		
//	__u16		set_args;	
//	__u16		get_args;	
//	char		name[IFNAMSIZ];	
//};
*/
//If get cmd's number is big,there may cause some problemes.
//So modified by Lawrence,071120
 
static iw_handler r8180_private_handler[] = {
//	r8180_wx_set_monitor,  /* SIOCIWFIRSTPRIV */
	r8180_wx_set_crcmon,   /*SIOCIWSECONDPRIV*/
//	r8180_wx_set_forceassociate,
//	r8180_wx_set_beaconinterval,
//	r8180_wx_set_monitor_type,
	r8180_wx_set_scan_type,
	r8180_wx_set_rawtx,
	
#if 0
#ifdef _RTL8187_EXT_PATCH_
	r8180_wx_get_meshinfo,
	r8180_wx_enable_mesh,
	r8180_wx_disable_mesh,
	r8180_wx_set_channel,
	r8180_wx_set_meshID,

//	r8180_wx_add_mac_allow,
//	r8180_wx_get_mac_allow,
//	r8180_wx_del_mac_allow,
	r8180_wx_add_mac_deny,
	r8180_wx_get_mac_deny,
	r8180_wx_del_mac_deny,
	r8180_wx_get_mesh_list,
	r8180_wx_mesh_scan,
	r8180_wx_join_mesh,
#endif	
#endif

#ifdef JOHN_IOCTL
	r8180_wx_read_regs,
	r8180_wx_write_regs,
	r8180_wx_read_bb,
	r8180_wx_write_bb,
    	r8180_wx_read_nicb,
    	r8180_wx_write_nicb,
	r8180_wx_get_ap_status,	
#endif
	r8180_wx_get_radion,
	r8180_wx_set_radion,
	r8180_wx_set_ratadpt,
#ifdef ENABLE_TOSHIBA_CONFIG
	r8180_wx_get_tblidx,
	r8180_wx_set_tbl,
#endif
};

#if WIRELESS_EXT >= 17	
//WB modefied to show signal to GUI on 18-01-2008
static struct iw_statistics *r8180_get_wireless_stats(struct net_device *dev)
{
       struct r8180_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device* ieee = priv->ieee80211;
	struct iw_statistics* wstats = &priv->wstats;
//	struct ieee80211_network* target = NULL;
	int tmp_level = 0;
	int tmp_qual = 0;
	int tmp_noise = 0;
//	unsigned long flag;

	if (ieee->state < IEEE80211_LINKED)
	{
		wstats->qual.qual = 0;
		wstats->qual.level = 0;
		wstats->qual.noise = 0;
		wstats->qual.updated = IW_QUAL_ALL_UPDATED | IW_QUAL_DBM;
		return wstats;
	}	
#if 0
	spin_lock_irqsave(&ieee->lock, flag);
	list_for_each_entry(target, &ieee->network_list, list)
	{
		if (is_same_network(target, &ieee->current_network))
		{
			printk("it's same network:%s\n", target->ssid);
#if 0
			if (!tmp_level)
			{
				tmp_level = target->stats.signalstrength;
				tmp_qual = target->stats.signal;
			}
			else
			{

				tmp_level = (15*tmp_level + target->stats.signalstrength)/16;
				tmp_qual = (15*tmp_qual + target->stats.signal)/16;
			}
#else
			tmp_level = target->stats.signal;
			tmp_qual = target->stats.signalstrength;
			tmp_noise = target->stats.noise;			
			printk("level:%d, qual:%d, noise:%d\n", tmp_level, tmp_qual, tmp_noise);
#endif
			break;
		}
	}
	spin_unlock_irqrestore(&ieee->lock, flag);
#endif
	tmp_level = (&ieee->current_network)->stats.signal;
	tmp_qual = (&ieee->current_network)->stats.signalstrength;
	tmp_noise = (&ieee->current_network)->stats.noise;			
	//printk("level:%d, qual:%d, noise:%d\n", tmp_level, tmp_qual, tmp_noise);

	wstats->qual.level = tmp_level;
	wstats->qual.qual = tmp_qual;
	wstats->qual.noise = tmp_noise;
	wstats->qual.updated = IW_QUAL_ALL_UPDATED| IW_QUAL_DBM;
	return wstats;
}
#endif


struct iw_handler_def  r8180_wx_handlers_def={
	.standard = r8180_wx_handlers,
	.num_standard = sizeof(r8180_wx_handlers) / sizeof(iw_handler),
	.private = r8180_private_handler,
	.num_private = sizeof(r8180_private_handler) / sizeof(iw_handler),
 	.num_private_args = sizeof(r8180_private_args) / sizeof(struct iw_priv_args),
#if WIRELESS_EXT >= 17	
	.get_wireless_stats = r8180_get_wireless_stats,
#endif
	.private_args = (struct iw_priv_args *)r8180_private_args,	
};
#ifdef _RTL8187_EXT_PATCH_
EXPORT_SYMBOL(r8180_wx_set_channel);
#endif
