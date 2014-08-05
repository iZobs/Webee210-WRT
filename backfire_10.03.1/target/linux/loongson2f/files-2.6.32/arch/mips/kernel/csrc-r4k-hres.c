/*
 * MIPS sched_clock implementation.
 *
 * Copyright (C) 2009 Lemote Inc.
 * Author: Wu Zhangjin, wuzj@lemote.com
 *
 * because cnt32_to_63() needs to be called at least once per half period to
 * work properly, and some of the MIPS frequency is high, perhaps a kernel
 * timer is needed to be set up to ensure this requirement is always met.
 * Please refer to arch/arm/plat-orion/time.c and include/linux/cnt32_to_63.h
 */

#include <linux/clocksource.h>
#include <linux/cnt32_to_63.h>
#include <linux/init.h>
#include <linux/timer.h>

#define CLOCK2NS_SCALE_FACTOR 8

static unsigned long __read_mostly cycle2ns_scale;

unsigned long long notrace sched_clock(void)
{
	unsigned long long v = cnt32_to_63(read_c0_count());
	return (v * cycle2ns_scale) >> CLOCK2NS_SCALE_FACTOR;
}

static struct timer_list cnt32_to_63_keepwarm_timer;

static void cnt32_to_63_keepwarm(unsigned long data)
{
	mod_timer(&cnt32_to_63_keepwarm_timer, round_jiffies(jiffies + data));
	sched_clock();
}

void setup_r4k_sched_clock(struct clocksource cs, unsigned int clock)
{
	unsigned long long v;
	unsigned long data;

	v = NSEC_PER_SEC;
	v <<= CLOCK2NS_SCALE_FACTOR;
	v += clock/2;
	do_div(v, clock);
	/*
	 * We want an even value to automatically clear the top bit
	 * returned by cnt32_to_63() without an additional run time
	 * instruction. So if the LSB is 1 then round it up.
	 */
	if (v & 1)
		v++;
	clock2ns_scale = v;

	data = 0x80000000UL / clock * HZ;
	setup_timer(&cnt32_to_63_keepwarm_timer, cnt32_to_63_keepwarm, data);
	mod_timer(&cnt32_to_63_keepwarm_timer, round_jiffies(jiffies + data));
}
