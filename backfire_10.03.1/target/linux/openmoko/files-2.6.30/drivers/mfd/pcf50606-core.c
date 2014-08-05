/* Philips PCF50606 Power Management Unit (PMU) driver
 *
 * (C) 2006-2008 by Openmoko, Inc.
 * Author: Harald Welte <laforge@openmoko.org>
 *	    Matt Hsu <matt@openmoko.org>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/module.h>

#include <linux/mfd/pcf50606/core.h>

static int __pcf50606_read(struct pcf50606 *pcf, uint8_t reg, int num, uint8_t *data)
{
	int ret;

	ret = i2c_smbus_read_i2c_block_data(pcf->i2c_client, reg,
				num, data);
	if (ret < 0)
		dev_err(pcf->dev, "Error reading %d regs at %d\n", num, reg);

	return ret;
}

static int __pcf50606_write(struct pcf50606 *pcf, uint8_t reg, int num, uint8_t *data)
{
	int ret;

	ret = i2c_smbus_write_i2c_block_data(pcf->i2c_client, reg,
				num, data);
	if (ret < 0)
		dev_err(pcf->dev, "Error writing %d regs at %d\n", num, reg);

	return ret;

}

/* Read a block of upto 32 regs  */
int pcf50606_read_block(struct pcf50606 *pcf, uint8_t reg,
					int nr_regs, uint8_t *data)
{
	int ret;

	mutex_lock(&pcf->lock);
	ret = __pcf50606_read(pcf, reg, nr_regs, data);
	mutex_unlock(&pcf->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(pcf50606_read_block);

/* Write a block of upto 32 regs  */
int pcf50606_write_block(struct pcf50606 *pcf , uint8_t reg,
					int nr_regs, uint8_t *data)
{
	int ret;

	mutex_lock(&pcf->lock);
	ret = __pcf50606_write(pcf, reg, nr_regs, data);
	mutex_unlock(&pcf->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(pcf50606_write_block);

uint8_t pcf50606_reg_read(struct pcf50606 *pcf, uint8_t reg)
{
	uint8_t val;

	mutex_lock(&pcf->lock);
	__pcf50606_read(pcf, reg, 1, &val);
	mutex_unlock(&pcf->lock);

	return val;
}
EXPORT_SYMBOL_GPL(pcf50606_reg_read);

int pcf50606_reg_write(struct pcf50606 *pcf, uint8_t reg, uint8_t val)
{
	int ret;

	mutex_lock(&pcf->lock);
	ret = __pcf50606_write(pcf, reg, 1, &val);
	mutex_unlock(&pcf->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(pcf50606_reg_write);

int pcf50606_reg_set_bit_mask(struct pcf50606 *pcf, uint8_t reg, uint8_t mask, uint8_t val)
{
	int ret;
	uint8_t tmp;

	val &= mask;

	mutex_lock(&pcf->lock);
	ret = __pcf50606_read(pcf, reg, 1, &tmp);
	if (ret < 0)
		goto out;

	tmp &= ~mask;
	tmp |= val;
	ret = __pcf50606_write(pcf, reg, 1, &tmp);

out:
	mutex_unlock(&pcf->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(pcf50606_reg_set_bit_mask);

int pcf50606_reg_clear_bits(struct pcf50606 *pcf, uint8_t reg, uint8_t val)
{
	int ret;
	uint8_t tmp;

	mutex_lock(&pcf->lock);
	ret = __pcf50606_read(pcf, reg, 1, &tmp);
	if (ret < 0)
		goto out;

	tmp &= ~val;
	ret = __pcf50606_write(pcf, reg, 1, &tmp);

out:
	mutex_unlock(&pcf->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(pcf50606_reg_clear_bits);

/* sysfs attributes */
static ssize_t show_dump_regs(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct pcf50606 *pcf = dev_get_drvdata(dev);
	uint8_t dump[16];
	int n, n1, idx = 0;
	char *buf1 = buf;
	static uint8_t address_no_read[] = { /* must be ascending */
		PCF50606_REG_INT1,
		PCF50606_REG_INT2,
		PCF50606_REG_INT3,
		0 /* terminator */
	};

	for (n = 0; n < 256; n += sizeof(dump)) {
		for (n1 = 0; n1 < sizeof(dump); n1++)
			if (n == address_no_read[idx]) {
				idx++;
				dump[n1] = 0x00;
			} else
				dump[n1] = pcf50606_reg_read(pcf, n + n1);

		hex_dump_to_buffer(dump, sizeof(dump), 16, 1, buf1, 128, 0);
		buf1 += strlen(buf1);
		*buf1++ = '\n';
		*buf1 = '\0';
	}

	return buf1 - buf;
}
static DEVICE_ATTR(dump_regs, 0400, show_dump_regs, NULL);

static ssize_t show_resume_reason(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcf50606 *pcf = dev_get_drvdata(dev);
	int n;

	n = sprintf(buf, "%02x%02x%02x\n",
				pcf->resume_reason[0],
				pcf->resume_reason[1],
				pcf->resume_reason[2]);

	return n;
}
static DEVICE_ATTR(resume_reason, 0400, show_resume_reason, NULL);

static struct attribute *pcf_sysfs_entries[] = {
	&dev_attr_dump_regs.attr,
	&dev_attr_resume_reason.attr,
	NULL,
};

static struct attribute_group pcf_attr_group = {
	.name	= NULL,			/* put in device directory */
	.attrs	= pcf_sysfs_entries,
};

int pcf50606_register_irq(struct pcf50606 *pcf, int irq,
			void (*handler) (int, void *), void *data)
{
	if (irq < 0 || irq > PCF50606_NUM_IRQ || !handler)
		return -EINVAL;

	if (WARN_ON(pcf->irq_handler[irq].handler))
		return -EBUSY;

	mutex_lock(&pcf->lock);
	pcf->irq_handler[irq].handler = handler;
	pcf->irq_handler[irq].data = data;
	mutex_unlock(&pcf->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(pcf50606_register_irq);

int pcf50606_free_irq(struct pcf50606 *pcf, int irq)
{
	if (irq < 0 || irq > PCF50606_NUM_IRQ)
		return -EINVAL;

	mutex_lock(&pcf->lock);
	pcf->irq_handler[irq].handler = NULL;
	mutex_unlock(&pcf->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(pcf50606_free_irq);

static int __pcf50606_irq_mask_set(struct pcf50606 *pcf, int irq, uint8_t mask)
{
	uint8_t reg, bits, tmp;
	int ret = 0, idx;

	idx = irq >> 3;
	reg =  PCF50606_REG_INT1M + idx;
	bits = 1 << (irq & 0x07);

	mutex_lock(&pcf->lock);

	if (mask) {
		ret = __pcf50606_read(pcf, reg, 1, &tmp);
		if (ret < 0)
			goto out;

		tmp |= bits;

		ret = __pcf50606_write(pcf, reg, 1, &tmp);
		if (ret < 0)
			goto out;

		pcf->mask_regs[idx] &= ~bits;
		pcf->mask_regs[idx] |= bits;
	} else {
		ret = __pcf50606_read(pcf, reg, 1, &tmp);
		if (ret < 0)
			goto out;

		tmp &= ~bits;

		ret = __pcf50606_write(pcf, reg, 1, &tmp);
		if (ret < 0)
			goto out;

		pcf->mask_regs[idx] &= ~bits;
	}
out:
	mutex_unlock(&pcf->lock);

	return ret;
}

int pcf50606_irq_mask(struct pcf50606 *pcf, int irq)
{
	dev_dbg(pcf->dev, "Masking IRQ %d\n", irq);

	return __pcf50606_irq_mask_set(pcf, irq, 1);
}
EXPORT_SYMBOL_GPL(pcf50606_irq_mask);

int pcf50606_irq_unmask(struct pcf50606 *pcf, int irq)
{
	dev_dbg(pcf->dev, "Unmasking IRQ %d\n", irq);

	return __pcf50606_irq_mask_set(pcf, irq, 0);
}
EXPORT_SYMBOL_GPL(pcf50606_irq_unmask);

int pcf50606_irq_mask_get(struct pcf50606 *pcf, int irq)
{
	uint8_t reg, bits;

	reg =  (irq / 8);
	bits = (1 << (irq % 8));

	return pcf->mask_regs[reg] & bits;
}
EXPORT_SYMBOL_GPL(pcf50606_irq_mask_get);

static void pcf50606_irq_call_handler(struct pcf50606 *pcf,
					int irq)
{
	if (pcf->irq_handler[irq].handler)
		pcf->irq_handler[irq].handler(irq, pcf->irq_handler[irq].data);
}

#define PCF50606_ONKEY1S_TIMEOUT 	8

#define PCF50606_REG_MBCS1		0x2c

static void pcf50606_irq_worker(struct work_struct *work)
{
	int ret;
	struct pcf50606 *pcf;
	uint8_t pcf_int[3], charger_status;
	size_t i, j;

	pcf = container_of(work, struct pcf50606, irq_work);

	/* Read the 3 INT regs in one transaction */
	ret = pcf50606_read_block(pcf, PCF50606_REG_INT1,
						ARRAY_SIZE(pcf_int), pcf_int);
	if (ret != ARRAY_SIZE(pcf_int)) {
		dev_err(pcf->dev, "Error reading INT registers\n");

		/*
		 * If this doesn't ACK the interrupt to the chip, we'll be
		 * called once again as we're level triggered.
		 */
		goto out;
	}

	/* We immediately read the charger status. We thus make sure
	 * only one of CHGINS/CHGRM interrupt handlers are called */
	if (pcf_int[1] & (PCF50606_INT2_CHGINS | PCF50606_INT2_CHGRM)) {
		charger_status = pcf50606_reg_read(pcf, PCF50606_REG_MBCS1);
		if (charger_status & (0x1 << 4))
			pcf_int[1] &= ~PCF50606_INT2_CHGRM;
		else
			pcf_int[1] &= ~PCF50606_INT2_CHGINS;
	}

	dev_dbg(pcf->dev, "INT1=0x%02x INT2=0x%02x INT3=0x%02x\n",
				pcf_int[0], pcf_int[1], pcf_int[2]);

	/* Some revisions of the chip don't have a 8s standby mode on
	 * ONKEY1S press. We try to manually do it in such cases. */
	if (pcf_int[0] & PCF50606_INT1_SECOND && pcf->onkey1s_held) {
		dev_info(pcf->dev, "ONKEY1S held for %d secs\n",
							pcf->onkey1s_held);
		if (pcf->onkey1s_held++ == PCF50606_ONKEY1S_TIMEOUT)
			if (pcf->pdata->force_shutdown)
				pcf->pdata->force_shutdown(pcf);
	}

	if (pcf_int[0] & PCF50606_INT1_ONKEY1S) {
		dev_info(pcf->dev, "ONKEY1S held\n");
		pcf->onkey1s_held = 1 ;

		/* Unmask IRQ_SECOND */
		pcf50606_reg_clear_bits(pcf, PCF50606_REG_INT1M,
						PCF50606_INT1_SECOND);

		/* Unmask IRQ_ONKEYF */
		pcf50606_reg_clear_bits(pcf, PCF50606_REG_INT1M,
						PCF50606_INT1_ONKEYF);
	}

	if ((pcf_int[0] & PCF50606_INT1_ONKEYR) && pcf->onkey1s_held) {
		pcf->onkey1s_held = 0;

		/* Mask SECOND and ONKEYF interrupts */
		if (pcf->mask_regs[0] & PCF50606_INT1_SECOND)
			pcf50606_reg_set_bit_mask(pcf,
					PCF50606_REG_INT1M,
					PCF50606_INT1_SECOND,
					PCF50606_INT1_SECOND);

		if (pcf->mask_regs[0] & PCF50606_INT1_ONKEYF)
			pcf50606_reg_set_bit_mask(pcf,
					PCF50606_REG_INT1M,
					PCF50606_INT1_ONKEYF,
					PCF50606_INT1_ONKEYF);
	}

	/* Have we just resumed ? */
	if (pcf->is_suspended) {

		pcf->is_suspended = 0;

		/* Set the resume reason filtering out non resumers */
		for (i = 0; i < ARRAY_SIZE(pcf_int); i++)
			pcf->resume_reason[i] = pcf_int[i] &
						pcf->pdata->resumers[i];

		/* Make sure we don't pass on ONKEY events to
		 * userspace now */
		pcf_int[1] &= ~(PCF50606_INT1_ONKEYR | PCF50606_INT1_ONKEYF);
	}

	for (i = 0; i < ARRAY_SIZE(pcf_int); i++) {
		/* Unset masked interrupts */
		pcf_int[i] &= ~pcf->mask_regs[i];

		for (j = 0; j < 8 ; j++)
			if (pcf_int[i] & (1 << j))
				pcf50606_irq_call_handler(pcf, (i * 8) + j);
	}

out:
	put_device(pcf->dev);
	enable_irq(pcf->irq);
}

static irqreturn_t pcf50606_irq(int irq, void *data)
{
	struct pcf50606 *pcf = data;

	get_device(pcf->dev);
	disable_irq_nosync(pcf->irq);
	schedule_work(&pcf->irq_work);

	return IRQ_HANDLED;
}

static void
pcf50606_client_dev_register(struct pcf50606 *pcf, const char *name,
						struct platform_device **pdev)
{
	int ret;

	*pdev = platform_device_alloc(name, -1);
	if (!*pdev) {
		dev_err(pcf->dev, "Falied to allocate %s\n", name);
		return;
	}

	(*pdev)->dev.parent = pcf->dev;

	ret = platform_device_add(*pdev);
	if (ret) {
		dev_err(pcf->dev, "Failed to register %s: %d\n", name, ret);
		platform_device_put(*pdev);
		*pdev = NULL;
	}
}

#ifdef CONFIG_PM
static int pcf50606_suspend(struct i2c_client *client, pm_message_t state)
{
	int ret;
	struct pcf50606 *pcf;
	size_t i;
	uint8_t res[3];

	pcf = i2c_get_clientdata(client);

	/* Make sure our interrupt handlers are not called
	 * henceforth */
	disable_irq(pcf->irq);

	/* Make sure that any running IRQ worker has quit */
	cancel_work_sync(&pcf->irq_work);

	/* Save the masks */
	ret = pcf50606_read_block(pcf, PCF50606_REG_INT1M,
				ARRAY_SIZE(pcf->suspend_irq_masks),
						pcf->suspend_irq_masks);
	if (ret < 0) {
		dev_err(pcf->dev, "error saving irq masks\n");
		goto out;
	}

	/* Write wakeup irq masks */
	for (i = 0; i < ARRAY_SIZE(res); i++)
		res[i] = ~pcf->pdata->resumers[i];

	ret = pcf50606_write_block(pcf, PCF50606_REG_INT1M,
					ARRAY_SIZE(res), &res[0]);
	if (ret < 0) {
		dev_err(pcf->dev, "error writing wakeup irq masks\n");
		goto out;
	}

	pcf->is_suspended = 1;

out:
	return ret;
}

static int pcf50606_resume(struct i2c_client *client)
{
	struct pcf50606 *pcf;
	int ret;

	pcf = i2c_get_clientdata(client);

	/* Write the saved mask registers */
	ret = pcf50606_write_block(pcf, PCF50606_REG_INT1M,
				ARRAY_SIZE(pcf->suspend_irq_masks),
					pcf->suspend_irq_masks);
	if (ret < 0)
		dev_err(pcf->dev, "Error restoring saved suspend masks\n");

	get_device(pcf->dev);

	/*
	 * Clear any pending interrupts and set resume reason if any.
	 * This will leave with enable_irq()
	 */
	pcf50606_irq_worker(&pcf->irq_work);

	return 0;
}
#else
#define pcf50606_suspend NULL
#define pcf50606_resume NULL
#endif

static int pcf50606_probe(struct i2c_client *client,
				const struct i2c_device_id *ids)
{
	int ret;
	struct pcf50606 *pcf;
	struct pcf50606_platform_data *pdata = client->dev.platform_data;
	int i;
	uint8_t version, variant;

	if (!client->irq) {
		dev_err(&client->dev, "Missing IRQ\n");
		return -ENOENT;
	}

	pcf = kzalloc(sizeof(*pcf), GFP_KERNEL);
	if (!pcf)
		return -ENOMEM;

	pcf->pdata = pdata;

	mutex_init(&pcf->lock);

	i2c_set_clientdata(client, pcf);
	pcf->dev = &client->dev;
	pcf->i2c_client = client;
	pcf->irq = client->irq;

	INIT_WORK(&pcf->irq_work, pcf50606_irq_worker);

	version = pcf50606_reg_read(pcf, 0);
	variant = pcf50606_reg_read(pcf, 1);

	/* This test is always false, FIX it */
	if (version < 0 || variant < 0) {
		dev_err(pcf->dev, "Unable to probe pcf50606\n");
		ret = -ENODEV;
		goto err;
	}

	dev_info(pcf->dev, "Probed device version %d variant %d\n",
							version, variant);
	/* Enable all inteerupts except RTC SECOND */
	pcf->mask_regs[0] = 0x40;
	pcf50606_reg_write(pcf, PCF50606_REG_INT1M, pcf->mask_regs[0]);
	pcf50606_reg_write(pcf, PCF50606_REG_INT2M, 0x00);
	pcf50606_reg_write(pcf, PCF50606_REG_INT3M, 0x00);

	ret = request_irq(client->irq, pcf50606_irq,
			IRQF_TRIGGER_LOW, "pcf50606", pcf);

	if (ret) {
		dev_err(pcf->dev, "Failed to request IRQ %d\n", ret);
		goto err;
	}

	pcf50606_client_dev_register(pcf, "pcf50606-input",
						&pcf->input_pdev);
	pcf50606_client_dev_register(pcf, "pcf50606-rtc",
						&pcf->rtc_pdev);
	pcf50606_client_dev_register(pcf, "pcf50606-mbc",
						&pcf->mbc_pdev);
	pcf50606_client_dev_register(pcf, "pcf50606-adc",
						&pcf->adc_pdev);
	pcf50606_client_dev_register(pcf, "pcf50606-wdt",
						&pcf->wdt_pdev);
	for (i = 0; i < PCF50606_NUM_REGULATORS; i++) {
		struct platform_device *pdev;

		pdev = platform_device_alloc("pcf50606-regltr", i);
		if (!pdev) {
			dev_err(pcf->dev, "Cannot create regulator %d\n", i);
			continue;
		}

		pdev->dev.parent = pcf->dev;
		platform_device_add_data(pdev, &pdata->reg_init_data[i],
					sizeof(pdata->reg_init_data[i]));
		pcf->regulator_pdev[i] = pdev;

		platform_device_add(pdev);
	}

	if (enable_irq_wake(client->irq) < 0)
		dev_info(pcf->dev, "IRQ %u cannot be enabled as wake-up source"
			"in this hardware revision\n", client->irq);

	ret = sysfs_create_group(&client->dev.kobj, &pcf_attr_group);
	if (ret)
		dev_info(pcf->dev, "error creating sysfs entries\n");

	if (pdata->probe_done)
		pdata->probe_done(pcf);

	return 0;

err:
	i2c_set_clientdata(client, NULL);
	kfree(pcf);
	return ret;
}

static int pcf50606_remove(struct i2c_client *client)
{
	struct pcf50606 *pcf = i2c_get_clientdata(client);
	unsigned int i;

	free_irq(pcf->irq, pcf);

	platform_device_unregister(pcf->input_pdev);
	platform_device_unregister(pcf->rtc_pdev);
	platform_device_unregister(pcf->mbc_pdev);
	platform_device_unregister(pcf->adc_pdev);

	for (i = 0; i < PCF50606_NUM_REGULATORS; i++)
		platform_device_unregister(pcf->regulator_pdev[i]);

	i2c_set_clientdata(client, NULL);
	kfree(pcf);

	return 0;
}

static struct i2c_device_id pcf50606_id_table[] = {
	{"pcf50606", 0x08},
};

static struct i2c_driver pcf50606_driver = {
	.driver = {
		.name	= "pcf50606",
	},
	.id_table = pcf50606_id_table,
	.probe = pcf50606_probe,
	.remove = pcf50606_remove,
	.suspend = pcf50606_suspend,
	.resume	= pcf50606_resume,
};

static int __init pcf50606_init(void)
{
	return i2c_add_driver(&pcf50606_driver);
}
module_init(pcf50606_init);

static void pcf50606_exit(void)
{
	i2c_del_driver(&pcf50606_driver);
}
module_exit(pcf50606_exit);

MODULE_DESCRIPTION("I2C chip driver for NXP PCF50606 PMU");
MODULE_AUTHOR("Harald Welte <laforge@openmoko.org>");
MODULE_LICENSE("GPL");
