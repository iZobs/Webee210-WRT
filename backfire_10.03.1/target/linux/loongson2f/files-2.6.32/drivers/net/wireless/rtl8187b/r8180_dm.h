/* 
       Hardware dynamic mechanism for RTL8187B. 
Notes:	
	This file is ported from RTL8187B Windows driver
*/

#ifndef R8180_DM_H
#define R8180_DM_H

#include "r8187.h"

bool CheckDig(struct net_device *dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
void rtl8180_hw_dig_wq (struct work_struct *work);
#else
void rtl8180_hw_dig_wq(struct net_device *dev);
#endif

bool CheckHighPower(struct net_device *dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
void rtl8180_tx_pw_wq (struct work_struct *work);
#else
void rtl8180_tx_pw_wq(struct net_device *dev);
#endif

//by lzm for antenna
#ifdef SW_ANTE_DIVERSITY
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
void SwAntennaWorkItemCallback(struct work_struct *work);
#else   
void SwAntennaWorkItemCallback(struct net_device *dev);
#endif
void SwAntennaDiversityRxOk8185(struct net_device *dev, u8 SignalStrength);
void SwAntennaDiversityTimerCallback(struct net_device *dev);
#endif
//by lzm for antenna

#endif //R8180_PM_H
