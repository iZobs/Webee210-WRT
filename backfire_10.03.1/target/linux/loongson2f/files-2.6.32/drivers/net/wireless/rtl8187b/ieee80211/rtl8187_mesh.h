#ifndef _RTL8187_MESH_H_
#define _RTL8187_MESH_H_

#include "msh_class.h" 	// struct mshclass
#include "mesh.h"		// struct MESH-Neighbor-Entry
#include "ieee80211.h"	// struct ieee80211-network
#include "mesh_8185_util.h" // DOT11-QUEUE
#include "hash_table.h" // hash-table
#include "8185s_pathsel.h"
#include <linux/list.h>

#define GET_MESH_PRIV(x)  ((struct mshclass_priv *)(x->priv))

struct ieee80211_hdr_mesh {
	u16					frame_ctl;
	u16					duration_id;
	u8					addr1[ETH_ALEN];
	u8					addr2[ETH_ALEN];
	u8					addr3[ETH_ALEN];
	u16					seq_ctl;
	u8					addr4[ETH_ALEN];
	unsigned char		mesh_flag;
	INT8				TTL;
	UINT16 				segNum;
	unsigned char		DestMACAddr[ETH_ALEN]; // modify for 6 address
	unsigned char		SrcMACAddr[ETH_ALEN];
} __attribute__ ((packed));

struct myMeshIDNode {
	struct list_head	list;
	char id[MESH_ID_LEN+1];
	short popEN;
	char  tried;
	unsigned long expire;
	struct ieee80211_network mesh_network;
};

struct ieee80211_hdr_mesh_QOS {
	u16					frame_ctl;
	u16					duration_id;
	u8					addr1[ETH_ALEN];
	u8					addr2[ETH_ALEN];
	u8					addr3[ETH_ALEN];
	u16					seq_ctl;
	u8					addr4[ETH_ALEN];
	u16					QOS_ctl;
	unsigned char		mesh_flag;
	INT8				TTL;
	UINT16 				segNum;
	unsigned char		DestMACAddr[ETH_ALEN]; // modify for 6 address
	unsigned char		SrcMACAddr[ETH_ALEN];
} __attribute__ ((packed));


struct mesh_PeerEntry {
	// based on 8185ag.h
	struct list_head			hash_list;
	unsigned int				used;	///< used == TRUE => has been allocated, \n used == FALSE => can be allocated
	unsigned char				hwaddr[MACADDRLEN];	///< hardware address
	
	// struct list_head	mesh_unEstablish_ptr;	// 尚未(或從已連線 -> 未連線) 之 MP  list
	struct list_head			mesh_mp_ptr;		// MP list
	
	/*mesh_neighbor:
	 *	Inited by "Neighbor Discovering"
	 *	cleaned by "Disassociation" or "Expired"
	*/
	struct MESH_Neighbor_Entry	mesh_neighbor_TBL;

	struct ieee80211_network *	pstat; // a backward pointer
	
	// 802.11 seq checking
	u16 last_rxseq; /* rx seq previous per-tid */
	u16 last_rxfrag;/* tx frag previous per-tid */
	unsigned long last_time;
	//
};


struct mshclass_priv {
	
	struct mesh_PeerEntry	*meshEntries;				// 1-to-1 for priv->ieee80211->networks

	spinlock_t				lock_stainfo;					// lock for accessing the data structure of stat info
	spinlock_t				lock_queue;						// lock for DOT11_EnQueue2/DOT11_DeQueue2/enque/dequeue
	spinlock_t				lock_Rreq;						// lock for rreq_retry. Some function like aodv_expire/tx use lock_queue simultaneously
//	spinlock_t				lock_meshlist;	
	
	// struct _DOT11_QUEUE		*pevent_queue;	///< 802.11 QUEUE結構
	// struct hash_table		*pathsel_table; // GANTOE
	//tsananiu
	struct _DOT11_QUEUE		*pathsel_queue;	///< 802.11 QUEUE結構

	//tsananiu end		 
		
	//add by shlu 20070518
	unsigned char RreqMAC[AODV_RREQ_TABLE_SIZE][6];
	unsigned int RreqBegin;
	unsigned int RreqEnd;

#if defined(MESH_ROLE_ROOT) || defined(MESH_ROLE_PORTAL)
#define MAX_SZ_BAD_MAC 3
	unsigned char	BadMac[MAX_SZ_BAD_MAC][MACADDRLEN];
	int				idx_BadMac;
#endif // MESH_ROLE_ROOT || MESH_ROLE_PORTAL
	
	//-------------
	unsigned char root_mac[MACADDRLEN];
	struct mesh_info		dot11MeshInfo; // extrated from wifi_mib (ieee802_mib.h)
	struct hash_table *proxy_table, *mesh_rreq_retry_queue;	//GANTOE	//GANTOE
	struct hash_table *pathsel_table; // add by chuangch 2007.09.13
	// add by Jason
	struct mpp_tb *pann_mpp_tb;
		
	struct timer_list		expire_timer;			// 1sec timer

	struct timer_list		beacon_timer;			// 1sec timer
	struct list_head		stat_hash[MAX_NETWORK_COUNT];	// sta_info hash table (aid_obj)
	
	struct list_head		meshList[MAX_CHANNEL_NUMBER];
	int 					scanMode;
	
	struct {
		struct ieee80211_network	*pstat;
		unsigned char				hwaddr[MACADDRLEN];
	} stainfo_cache;
	
	// The following elements are used by 802.11s. 
	// For copyright-pretection, we use an independent (binary) module.
	// Note that it can also be put either under r8180_priv or ieee80211_device. The adv of put under 
	// r8180_priv is to get "higher encapsulation". On the other hand, r8180_priv was originally designed 
	// for "hardward specific."
	char 				mesh_mac_filter_allow[8][13];
	char 				mesh_mac_filter_deny[8][13];
		
	struct MESH_Share		meshare; // mesh share data
	
	struct {
		
		int						prev_iw_mode; // Save this->iw_mode for r8180_wx->r8180_wx_enable_mesh. No init requirement
		
		struct MESH_Profile		mesh_profile; // contains MESHID
	
		struct mesh_info		dot11MeshInfo; // contains meshMaxAssocNum
		
		struct net_device_stats	mesh_stats; 
	
		UINT8				mesh_Version;				// 使用的版本
		// WLAN Mesh Capability
		INT16				mesh_PeerCAP_cap;			// peer capability-Cap number (有號數)
		UINT8				mesh_PeerCAP_flags;		// peer capability-flags
		UINT8				mesh_PowerSaveCAP;		// Power Save capability
		UINT8				mesh_SyncCAP;				// Synchronization capability
		UINT8				mesh_MDA_CAP;				// MDA capability
		UINT32				mesh_ChannelPrecedence;	// Channel Precedence
	
		// neighbor -> candidate neighbor, if mesh_available_peerlink > 0, page 56, D0.02
		UINT8				mesh_AvailablePeerLink;  // 此是否有需要?(因它等同於 mesh_PeerCAP)=>暫 時保 留

		UINT8				mesh_HeaderFlags;	// mesh header 內的 mesh flags field

		// MKD domain element [MKDDIE]
		UINT8				mesh_MKD_DomainID[6];
		UINT8				mesh_MKDDIE_SecurityConfiguration;
	
		// EMSA Handshake element [EMSAIE]
		UINT8				mesh_EMSAIE_ANonce[32];
		UINT8				mesh_EMSAIE_SNonce[32];
		UINT8				mesh_EMSAIE_MA_ID[6];
		UINT16				mesh_EMSAIE_MIC_Control;
		UINT8				mesh_EMSAIE_MIC[16];

		struct timer_list		mesh_peer_link_timer;	///< 對尚未連 線(與連線退至未連線) MP mesh_unEstablish_hdr 作 peer link time out

//		struct timer_list		mesh_beacon_timer;	
		// mesh_unEstablish_hdr:
		//  It is a list structure, only stores unEstablish (or Establish -> unEstablish [MP_HOLDING])MP entry
		//  Each entry is a pointer pointing to an entry in "stat_info->mesh_mp_ptr"
		//  and removed by successful "Peer link setup" or "Expired"
		struct list_head		mesh_unEstablish_hdr;
	
		// mesh_mp_hdr: 
		//  It is a list of MP/MAP/MPP who has already passed "Peer link setup"
		//  Each entry is a pointer pointing to an entry in "stat_info->mesh_mp_ptr"
		//  Every entry is inserted by "successful peer link setup"
		//  and removed by "Expired"
		struct list_head		mesh_mp_hdr;
		
	} mesh;
	
	int iCurChannel; // remember the working channel
};

// Stanley, 04/23/07
// The following mode is used by ieee80211_device->iw_mode
// Although it is better to put the definition under linux/wireless.h (or wireless_copy.h), it is a system file
// that we shouldn't modify directly.
#define IW_MODE_MESH	11	/* 802.11s mesh mode */

// Default MESHID
#define IEEE80211S_DEFAULT_MESHID "802.11s"

// callback for 802.11s 
extern short rtl8187_patch_ieee80211_probe_req_1 (struct ieee80211_device *ieee);
extern u8* rtl8187_patch_ieee80211_probe_req_2 (struct ieee80211_device *ieee, struct sk_buff *skb, u8 *tag);

// wx
extern int rtl8187_patch_r8180_wx_get_meshinfo(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
extern int rtl8187_patch_r8180_wx_enable_mesh(struct net_device *dev);
extern int rtl8187_patch_r8180_wx_disable_mesh(struct net_device *dev);
extern int rtl8187_patch_r8180_wx_wx_set_meshID(struct net_device *dev, char *ext,unsigned char channel);
extern void rtl8187_patch_r8180_wx_set_channel (struct ieee80211_device *ieee, int ch);
extern int rtl8187_patch_r8180_wx_set_add_mac_allow(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
extern int rtl8187_patch_r8180_wx_set_del_mac_allow(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
extern int rtl8187_patch_r8180_wx_set_add_mac_deny(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
extern int rtl8187_patch_r8180_wx_set_del_mac_deny(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
extern int rtl8187_patch_r8180_wx_get_mac_allow(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
extern int rtl8187_patch_r8180_wx_get_mac_deny(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);

extern int rtl8187_patch_r8180_wx_get_mesh_list(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
extern int rtl8187_patch_r8180_wx_mesh_scan(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra);
extern int rtl8187_patch_r8180_wx_get_selected_mesh(struct net_device *dev, int en, char *cho, char* id);
//by amy for networkmanager UI
extern int rtl8187_patch_r8180_wx_get_selected_mesh_channel(struct net_device *dev, char *extmeshid, char *cho);
//by amy for networkmanager UI
// osdep
extern int rtl8187_patch_ieee80211_start_protocol (struct ieee80211_device *ieee);
extern u8 rtl8187_patch_rtl8180_up(struct mshclass *priv);
extern void rtl8187_patch_ieee80211_stop_protocol(struct ieee80211_device *ieee);

// issue_assocreq_MP
extern void rtl8187_patch_ieee80211_association_req_1 (struct ieee80211_assoc_request_frame *hdr);
extern u8* rtl8187_patch_ieee80211_association_req_2 (struct ieee80211_device *ieee, struct ieee80211_network *pstat, struct sk_buff *skb);

// OnAssocReq_MP
extern int rtl8187_patch_ieee80211_rx_frame_softmac_on_assoc_req (struct ieee80211_device *ieee, struct sk_buff *skb);

// issue_assocrsp_MP
extern void rtl8187_patch_ieee80211_assoc_resp_by_net_1 (struct ieee80211_assoc_response_frame *assoc);
u8* rtl8187_patch_ieee80211_assoc_resp_by_net_2 (struct ieee80211_device *ieee, struct ieee80211_network *pstat, int pkt_type, struct sk_buff *skb);

// OnAssocRsp_MP
extern int rtl8187_patch_ieee80211_rx_frame_softmac_on_assoc_rsp (struct ieee80211_device *ieee, struct sk_buff *skb);
				  

extern int rtl8187_patch_ieee80211_rx_frame_softmac_on_auth(struct ieee80211_device *ieee, struct sk_buff *skb, struct ieee80211_rx_stats *rx_stats);
extern int rtl8187_patch_ieee80211_rx_frame_softmac_on_deauth(struct ieee80211_device *ieee, struct sk_buff *skb, struct ieee80211_rx_stats *rx_stats);
extern unsigned int rtl8187_patch_ieee80211_process_probe_response_1(	struct ieee80211_device *ieee,	struct ieee80211_probe_response *beacon,	struct ieee80211_rx_stats *stats);
extern void rtl8187_patch_ieee80211_rx_mgt_on_probe_req( struct ieee80211_device *ieee, struct ieee80211_probe_request *beacon, struct ieee80211_rx_stats *stats);
extern void rtl8187_patch_ieee80211_rx_mgt_update_expire ( struct ieee80211_device *ieee, struct sk_buff *skb);

// set channel
extern int rtl8187_patch_ieee80211_ext_stop_scan_wq_set_channel (struct ieee80211_device *ieee);

// on rx (rx isr)
extern int rtl8187_patch_ieee80211_rx_on_rx (struct ieee80211_device *ieee, struct sk_buff *skb, struct ieee80211_rx_stats *rx_stats, u16 type, u16 stype);

// r8187_core
// handle ioctl
extern int rtl8187_patch_rtl8180_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
// create proc
extern void rtl8187_patch_create_proc(struct r8180_priv *priv);
extern void rtl8187_patch_remove_proc(struct r8180_priv *priv);

// tx, xmit
// locked by ieee->lock. Call ieee80211_softmac_xmit afterward
extern struct ieee80211_txb* rtl8187_patch_ieee80211_xmit (struct sk_buff *skb, struct net_device *dev);

// given a skb, output header's length
extern int rtl8187_patch_ieee80211_rx_frame_get_hdrlen (struct ieee80211_device *ieee, struct sk_buff *skb);

// check the frame control field, return 0: not accept, 1: accept
extern int rtl8187_patch_ieee80211_rx_is_valid_framectl (struct ieee80211_device *ieee, u16 fc, u16 type, u16 stype);

// process_dataframe
extern int rtl8187_patch_ieee80211_rx_process_dataframe (struct ieee80211_device *ieee, struct sk_buff *skb, struct ieee80211_rx_stats *rx_stats);

extern int rtl8187_patch_is_duplicate_packet (struct ieee80211_device *ieee, struct ieee80211_hdr *header, u16 type, u16 stype);

extern int rtl8187_patch_ieee80211_softmac_xmit_get_rate (struct ieee80211_device *ieee, struct sk_buff *skb);
extern void ieee80211_start_mesh(struct ieee80211_device *ieee);
#endif // _RTL8187_MESH_H_
