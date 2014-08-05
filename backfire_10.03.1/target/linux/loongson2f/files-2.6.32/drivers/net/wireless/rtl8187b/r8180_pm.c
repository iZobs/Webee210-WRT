/* 
   Power management interface routines. 
   Written by Mariusz Matuszek.
   This code is currently just a placeholder for later work and 
   does not do anything useful.
   
   This is part of rtl8180 OpenSource driver.
   Copyright (C) Andrea Merello 2004  <andreamrl@tiscali.it> 
   Released under the terms of GPL (General Public Licence)	
*/

#ifdef CONFIG_RTL8180_PM


#include "r8180_hw.h"
#include "r8180_pm.h"
#include "r8187.h"
int rtl8180_save_state (struct pci_dev *dev, u32 state)
{
        printk(KERN_NOTICE "r8180 save state call (state %u).\n", state);
	return(-EAGAIN);
}

//netif_running is set to 0 before system call rtl8180_close, 
//netif_running is set to 1 before system call rtl8180_open,
//if open success it will not change, or it change to 0;
int rtl8187_suspend (struct usb_interface *intf, pm_message_t state)
{
	struct r8180_priv *priv;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	struct net_device *dev = usb_get_intfdata(intf);
#else
	//struct net_device *dev = (struct net_device *)ptr;
#endif

	printk("====>%s \n", __func__);
	priv=ieee80211_priv(dev);

	if(dev) {
		/* save the old rfkill state and then power off it */
		priv->eInactivePowerState = priv->eRFPowerState;
		/* power off the wifi by default */
		r8187b_wifi_change_rfkill_state(dev, eRfOff, 0);

		if (!netif_running(dev)) {
			//printk(KERN_WARNING "UI or other close dev before suspend, go out suspend function\n");
			return 0;
		}

		dev->netdev_ops->ndo_stop(dev);
		netif_device_detach(dev);
	}
	return 0;
}


int rtl8187_resume (struct usb_interface *intf)
{
	struct r8180_priv *priv;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	struct net_device *dev = usb_get_intfdata(intf);
#else
	//struct net_device *dev = (struct net_device *)ptr;
#endif

	printk("====>%s \n", __func__);
	priv=ieee80211_priv(dev);

	if(dev) {
		/* resume the old rfkill state */
		r8187b_wifi_change_rfkill_state(dev, priv->eInactivePowerState, 1);

		if (!netif_running(dev)){
			//printk(KERN_WARNING "UI or other close dev before suspend, go out resume function\n");
			return 0;
		}

		netif_device_attach(dev);
		dev->netdev_ops->ndo_open(dev);
	}

	return 0;
}


int rtl8180_enable_wake (struct pci_dev *dev, u32 state, int enable)
{
		
		//printk(KERN_NOTICE "r8180 enable wake call (state %u, enable %d).\n", 
	//       state, enable);
	return 0;
	//return(-EAGAIN);
}



#endif //CONFIG_RTL8180_PM
