/*
 * rtl8187b specific rfkill support
 *
 * NOTE: we only concern about two states
 *   eRfOff: RFKILL_STATE_SOFT_BLOCKED
 *   eRfOn: RFKILL_STATE_UNBLOCKED
 * TODO: move led controlling source code to rfkill framework
 *
 *  Copyright (C) 2009 Lemote Inc. & Insititute of Computing Technology
 *  Author: Wu Zhangjin <wuzj@lemote.com>
 */

#include <linux/module.h>
#include <linux/rfkill.h>
#include <linux/device.h>

/* LED macros are defined in r8187.h and rfkill.h, we not use any of them here
 * just avoid compiling erros here.
 */
#undef LED

#include "r8187.h"
#include "ieee80211/ieee80211.h"
#include "linux/netdevice.h"

static struct rfkill *r8187b_rfkill;
static struct work_struct r8187b_rfkill_task;
static int initialized;
/* turn off by default */
int r8187b_rfkill_state = RFKILL_USER_STATE_SOFT_BLOCKED;
struct net_device *r8187b_dev = NULL;
RT_RF_POWER_STATE eRfPowerStateToSet;

/* These two mutexes are used to ensure the relative rfkill status are accessed
 * by different tasks exclusively */
DEFINE_MUTEX(statetoset_lock);
DEFINE_MUTEX(state_lock);

static void r8187b_wifi_rfkill_task(struct work_struct *work)
{
	if (r8187b_dev) {
		mutex_lock(&statetoset_lock);
		r8187b_wifi_change_rfkill_state(r8187b_dev, eRfPowerStateToSet, 1);
		mutex_unlock(&statetoset_lock);
	}
}

static int r8187b_wifi_update_rfkill_state(int status)
{
	/* ensure r8187b_rfkill is initialized if dev is not initialized, means
	 * wifi driver is not start, the status is eRfOff be default.
	 */
	if (!r8187b_dev)
		return eRfOff;

	if (initialized == 0) {
		/* init the rfkill work task */
		INIT_WORK(&r8187b_rfkill_task, r8187b_wifi_rfkill_task);
		initialized = 1;
	}

	mutex_lock(&statetoset_lock);
	if (status == 1)
		eRfPowerStateToSet = eRfOn;
	else if (status == 0)
		eRfPowerStateToSet = eRfOff;
	else if (status == 2) {
		/* if the KEY_WLAN is pressed, just switch it! */
		mutex_lock(&state_lock);
		if (r8187b_rfkill_state == RFKILL_USER_STATE_UNBLOCKED)
			eRfPowerStateToSet = eRfOff;
		else if (r8187b_rfkill_state == RFKILL_USER_STATE_SOFT_BLOCKED)
			eRfPowerStateToSet = eRfOn;
		mutex_unlock(&state_lock);
	}
	mutex_unlock(&statetoset_lock);

	schedule_work(&r8187b_rfkill_task);

	return eRfPowerStateToSet;
}

static int r8187b_rfkill_set(void *data, bool blocked)
{
	r8187b_wifi_update_rfkill_state(!blocked);

	return 0;
}

static void r8187b_rfkill_query(struct rfkill *rfkill, void *data)
{
	static bool blocked;

	mutex_lock(&state_lock);
	if (r8187b_rfkill_state == RFKILL_USER_STATE_UNBLOCKED)
		blocked = 0;
	else if (r8187b_rfkill_state == RFKILL_USER_STATE_SOFT_BLOCKED)
		blocked = 1;
	mutex_unlock(&state_lock);

	rfkill_set_hw_state(rfkill, blocked);
}

int r8187b_wifi_report_state(r8180_priv *priv)
{
	mutex_lock(&state_lock);
	r8187b_rfkill_state = RFKILL_USER_STATE_UNBLOCKED;
	if (priv->ieee80211->bHwRadioOff && priv->eRFPowerState == eRfOff)
		r8187b_rfkill_state = RFKILL_USER_STATE_SOFT_BLOCKED;
	mutex_unlock(&state_lock);

	r8187b_rfkill_query(r8187b_rfkill, NULL);

	return 0;
}

static const struct rfkill_ops r8187b_rfkill_ops = {
	.set_block = r8187b_rfkill_set,
	.query = r8187b_rfkill_query,
};

int r8187b_rfkill_init(struct net_device *dev)
{
	int ret;

	/* init the r8187b device */
	r8187b_dev = dev;

	/* init the rfkill struct */
	r8187b_rfkill = rfkill_alloc("r8187b-wifi", &dev->dev,
				     RFKILL_TYPE_WLAN, &r8187b_rfkill_ops,
				     (void *)1);

	if (!r8187b_rfkill) {
		rfkill_destroy(r8187b_rfkill);
		printk(KERN_WARNING "r8187b: Unable to allocate rfkill\n");
		return -ENOMEM;
	}
	ret = rfkill_register(r8187b_rfkill);
	if (ret) {
		rfkill_destroy(r8187b_rfkill);
		r8187b_rfkill = NULL;
		return ret;
	}

	/* The default status is passed to the rfkill module */

	return 0;
}

void r8187b_rfkill_exit(void)
{
	if (r8187b_rfkill) {
		rfkill_unregister(r8187b_rfkill);
		rfkill_destroy(r8187b_rfkill);
	}
	r8187b_rfkill = NULL;
}
