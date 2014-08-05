/*!	\file	msh_class.h
	\brief	msh CLASS extension

	\date 2007/5/2 
	\author	Stanley Chang <chagnsl@cs.nctu.edu.tw>
*/

#ifndef _MESH_CLASS_HDR_H_
#define _MESH_CLASS_HDR_H_

#include <linux/if_ether.h> /* ETH_ALEN */
#include <linux/kernel.h>   /* ARRAY_SIZE */
#include <linux/version.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/sched.h>

#include "ieee80211/ieee80211.h" // for struct ieee80211-xxxx
#include "r8187.h" // for struct r8180-priv

#define MAC_TABLE_SIZE	8

struct mshclass {
	struct r8180_priv *	p8187;
	
	// callback functions
	// ieee80211_softmac.c
	int (*ext_patch_ieee80211_start_protocol) (struct ieee80211_device *ieee); // start special mode

	short (*ext_patch_ieee80211_probe_req_1) (struct ieee80211_device *ieee); // return = 0: no more phases,  >0: another phase
	u8* (*ext_patch_ieee80211_probe_req_2) (struct ieee80211_device *ieee, struct sk_buff *skb, u8 *tag); // return tag	

	void (*ext_patch_ieee80211_association_req_1) (struct ieee80211_assoc_request_frame *hdr);
	u8* (*ext_patch_ieee80211_association_req_2) (struct ieee80211_device *ieee, struct ieee80211_network *pstat, struct sk_buff *skb);
	
	int (*ext_patch_ieee80211_rx_frame_softmac_on_assoc_req) (struct ieee80211_device *ieee, struct sk_buff *skb);
	int (*ext_patch_ieee80211_rx_frame_softmac_on_assoc_rsp) (struct ieee80211_device *ieee, struct sk_buff *skb);
	
	void (*ext_patch_ieee80211_stop_protocol) (struct ieee80211_device *ieee); // stop timer
	
	void (*ext_patch_ieee80211_assoc_resp_by_net_1) (struct ieee80211_assoc_response_frame *assoc);
	u8* (*ext_patch_ieee80211_assoc_resp_by_net_2) (struct ieee80211_device *ieee, struct ieee80211_network *pstat, int pkt_type, struct sk_buff *skb);
	
	int (*ext_patch_ieee80211_ext_stop_scan_wq_set_channel) (struct ieee80211_device *ieee);
	
	struct sk_buff* (*ext_patch_get_beacon_get_probersp)(struct ieee80211_device *ieee, u8 *dest, struct ieee80211_network *net);
	
	int (*ext_patch_ieee80211_softmac_xmit_get_rate) (struct ieee80211_device *ieee, struct sk_buff *skb);
	int (*ext_patch_ieee80211_rx_frame_softmac_on_auth)(struct ieee80211_device *ieee, struct sk_buff *skb, struct ieee80211_rx_stats *rx_stats);
	int (*ext_patch_ieee80211_rx_frame_softmac_on_deauth)(struct ieee80211_device *ieee, struct sk_buff *skb, struct ieee80211_rx_stats *rx_stats);
//by amy for mesh
	void (*ext_patch_ieee80211_start_mesh)(struct ieee80211_device *ieee);
//by amy for mesh	
	/// r8180_wx.c
	int (*ext_patch_r8180_wx_get_meshinfo) (struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
	int (*ext_patch_r8180_wx_enable_mesh) (struct net_device *dev);
	int (*ext_patch_r8180_wx_disable_mesh) (struct net_device *dev);
	int (*ext_patch_r8180_wx_set_meshID) ( struct net_device *dev, char *ext);
//by amy for mesh
	int (*ext_patch_r8180_wx_set_mesh_chan)(struct net_device *dev, unsigned char channel);
//by amy for mesh
	void (*ext_patch_r8180_wx_set_channel) (struct ieee80211_device *ieee, int ch);
		
	int (*ext_patch_r8180_wx_set_add_mac_allow) (struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
	int (*ext_patch_r8180_wx_set_del_mac_allow) (struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
	int (*ext_patch_r8180_wx_set_add_mac_deny) (struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
	int (*ext_patch_r8180_wx_set_del_mac_deny) (struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
	int (*ext_patch_r8180_wx_get_mac_allow) (struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
	int (*ext_patch_r8180_wx_get_mac_deny) (struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
	
	int (*ext_patch_r8180_wx_get_mesh_list) (struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
	int (*ext_patch_r8180_wx_mesh_scan) (struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
	int (*ext_patch_r8180_wx_get_selected_mesh)(struct net_device *dev, int en, char *cho, char* id);
//by amy for networkmanager UI
	int (*ext_patch_r8180_wx_get_selected_mesh_channel)(struct net_device *dev, char* extmeshid, char *cho);
//by amy for networkmanager UI
	/// r8187_core.c
	u8 (*ext_patch_rtl8180_up) (struct mshclass *priv);

	// ieee80211_rx.c
	unsigned int (*ext_patch_ieee80211_process_probe_response_1)  (	struct ieee80211_device *ieee,	struct ieee80211_probe_response *beacon,	struct ieee80211_rx_stats *stats);
	void (*ext_patch_ieee80211_rx_mgt_on_probe_req) ( struct ieee80211_device *ieee, struct ieee80211_probe_request *beacon, struct ieee80211_rx_stats *stats);
	
	void (*ext_patch_ieee80211_rx_mgt_update_expire) ( struct ieee80211_device *ieee, struct sk_buff *skb);
	
	int (*ext_patch_ieee80211_rx_on_rx) (struct ieee80211_device *ieee, struct sk_buff *skb, struct ieee80211_rx_stats *rx_stats, u16 type, u16 stype);
	
	int (*ext_patch_ieee80211_rx_frame_get_hdrlen) (struct ieee80211_device *ieee, struct sk_buff *skb);
	
	int (*ext_patch_ieee80211_rx_is_valid_framectl) (struct ieee80211_device *ieee, u16 fc, u16 type, u16 stype);
	
	// return > 0 is success.  0 when failed
	int (*ext_patch_ieee80211_rx_process_dataframe) (struct ieee80211_device *ieee, struct sk_buff *skb, struct ieee80211_rx_stats *rx_stats);
	
	int (*ext_patch_is_duplicate_packet) (struct ieee80211_device *ieee, struct ieee80211_hdr *header, u16 type, u16 stype);
	/* added by david for setting acl dynamically */
	u8 (*ext_patch_ieee80211_acl_query) (struct ieee80211_device *ieee, u8 *sa);
	
	// r8187_core.c
	int (*ext_patch_rtl8180_ioctl) (struct net_device *dev, struct ifreq *rq, int cmd);
	void (*ext_patch_create_proc) (struct r8180_priv *priv);
	void (*ext_patch_remove_proc) (struct r8180_priv *priv);
	
	// ieee80211_tx.c
	
	// locked by ieee->lock. Call ieee80211_softmac_xmit afterward
	struct ieee80211_txb* (*ext_patch_ieee80211_xmit) (struct sk_buff *skb, struct net_device *dev);
	
	// DO NOT MODIFY ANY STRUCTURE BELOW THIS LINE	
	u8					priv[0]; // mshclass_priv;
};

extern void free_mshobj(struct mshclass **pObj);
extern struct mshclass *alloc_mshobj(struct r8180_priv *caller_priv);


#endif // _MESH_CLASS_HDR_H_
