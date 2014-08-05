/*
 * core.h  -- Core driver for NXP PCF50606
 *
 * (C) 2006-2008 by Openmoko, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __LINUX_MFD_PCF50606_CORE_H
#define __LINUX_MFD_PCF50606_CORE_H

#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/power_supply.h>

struct pcf50606;

#define PCF50606_NUM_REGULATORS	8

struct pcf50606_platform_data {
	struct regulator_init_data reg_init_data[PCF50606_NUM_REGULATORS];

	char **batteries;
	int num_batteries;

	/* Callbacks */
	void (*probe_done)(struct pcf50606 *);
	void (*mbc_event_callback)(struct pcf50606 *, int);
	void (*regulator_registered)(struct pcf50606 *, int);
	void (*force_shutdown)(struct pcf50606 *);

	u8 resumers[3];
};

struct pcf50606_irq {
	void (*handler)(int, void *);
	void *data;
};

int pcf50606_register_irq(struct pcf50606 *pcf, int irq,
			void (*handler) (int, void *), void *data);
int pcf50606_free_irq(struct pcf50606 *pcf, int irq);

int pcf50606_irq_mask(struct pcf50606 *pcf, int irq);
int pcf50606_irq_unmask(struct pcf50606 *pcf, int irq);
int pcf50606_irq_mask_get(struct pcf50606 *pcf, int irq);

int pcf50606_read_block(struct pcf50606 *, u8 reg,
					int nr_regs, u8 *data);
int pcf50606_write_block(struct pcf50606 *pcf, u8 reg,
					int nr_regs, u8 *data);
u8 pcf50606_reg_read(struct pcf50606 *, u8 reg);
int pcf50606_reg_write(struct pcf50606 *pcf, u8 reg, u8 val);

int pcf50606_reg_set_bit_mask(struct pcf50606 *pcf, u8 reg, u8 mask, u8 val);
int pcf50606_reg_clear_bits(struct pcf50606 *pcf, u8 reg, u8 bits);

/* Interrupt registers */

#define PCF50606_REG_INT1	0x02
#define	PCF50606_REG_INT2	0x03
#define	PCF50606_REG_INT3	0x04

#define PCF50606_REG_INT1M	0x05
#define	PCF50606_REG_INT2M	0x06
#define	PCF50606_REG_INT3M	0x07

enum {
	/* Chip IRQs */
	PCF50606_IRQ_ONKEYR,
	PCF50606_IRQ_ONKEYF,
	PCF50606_IRQ_ONKEY1S,
	PCF50606_IRQ_EXTONR,
	PCF50606_IRQ_EXTONF,
	PCF50606_IRQ_RESERVED_1,
	PCF50606_IRQ_SECOND,
	PCF50606_IRQ_ALARM,
	PCF50606_IRQ_CHGINS,
	PCF50606_IRQ_CHGRM,
	PCF50606_IRQ_CHGFOK,
	PCF50606_IRQ_CHGERR,
	PCF50606_IRQ_CHGFRDY,
	PCF50606_IRQ_CHGPROT,
	PCF50606_IRQ_CHGWD10S,
	PCF50606_IRQ_CHGWDEXP,
	PCF50606_IRQ_ADCRDY,
	PCF50606_IRQ_ACDINS,
	PCF50606_IRQ_ACDREM,
	PCF50606_IRQ_TSCPRES,
	PCF50606_IRQ_RESERVED_2,
	PCF50606_IRQ_RESERVED_3,
	PCF50606_IRQ_LOWBAT,
	PCF50606_IRQ_HIGHTMP,

	/* Always last */
	PCF50606_NUM_IRQ,
};

struct pcf50606 {
	struct device *dev;
	struct i2c_client *i2c_client;

	struct pcf50606_platform_data *pdata;
	int irq;
	struct pcf50606_irq irq_handler[PCF50606_NUM_IRQ];
	struct work_struct irq_work;
	struct mutex lock;

	u8 mask_regs[3];

	u8 suspend_irq_masks[3];
	u8 resume_reason[3];
	int is_suspended;

	int onkey1s_held;

	struct platform_device *rtc_pdev;
	struct platform_device *mbc_pdev;
	struct platform_device *adc_pdev;
	struct platform_device *input_pdev;
	struct platform_device *wdt_pdev;
	struct platform_device *regulator_pdev[PCF50606_NUM_REGULATORS];
};

enum pcf50606_reg_int1 {
	PCF50606_INT1_ONKEYR	= 0x01,	/* ONKEY rising edge */
	PCF50606_INT1_ONKEYF	= 0x02,	/* ONKEY falling edge */
	PCF50606_INT1_ONKEY1S	= 0x04,	/* OMKEY at least 1sec low */
	PCF50606_INT1_EXTONR	= 0x08,	/* EXTON rising edge */
	PCF50606_INT1_EXTONF	= 0x10,	/* EXTON falling edge */
	PCF50606_INT1_SECOND	= 0x40,	/* RTC periodic second interrupt */
	PCF50606_INT1_ALARM	= 0x80, /* RTC alarm time is reached */
};

enum pcf50606_reg_int2 {
	PCF50606_INT2_CHGINS	= 0x01, /* Charger inserted */
	PCF50606_INT2_CHGRM	= 0x02, /* Charger removed */
	PCF50606_INT2_CHGFOK	= 0x04,	/* Fast charging OK */
	PCF50606_INT2_CHGERR	= 0x08,	/* Error in charging mode */
	PCF50606_INT2_CHGFRDY	= 0x10,	/* Fast charge completed */
	PCF50606_INT2_CHGPROT	= 0x20,	/* Charging protection interrupt */
	PCF50606_INT2_CHGWD10S	= 0x40,	/* Charger watchdig expires in 10s */
	PCF50606_INT2_CHGWDEXP	= 0x80,	/* Charger watchdog expires */
};

enum pcf50606_reg_int3 {
	PCF50606_INT3_ADCRDY	= 0x01,	/* ADC conversion finished */
	PCF50606_INT3_ACDINS	= 0x02,	/* Accessory inserted */
	PCF50606_INT3_ACDREM	= 0x04, /* Accessory removed */
	PCF50606_INT3_TSCPRES	= 0x08,	/* Touch screen pressed */
	PCF50606_INT3_LOWBAT	= 0x40,	/* Low battery voltage */
	PCF50606_INT3_HIGHTMP	= 0x80, /* High temperature */
};

/* Misc regs */

#define PCF50606_REG_OOCC1 	0x08
#define PCF50606_OOCC1_GOSTDBY	0x01

static inline struct pcf50606 *dev_to_pcf50606(struct device *dev)
{
	return dev_get_drvdata(dev);
}

#endif

