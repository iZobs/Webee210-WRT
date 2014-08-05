/* Philips PCF50606 ADC Driver
 *
 * (C) 2006-2008 by Openmoko, Inc.
 * Author: Balaji Rao <balajirrao@openmoko.org>
 * All rights reserved.
 *
 * Broken down from monstrous PCF50606 driver mainly by
 * Harald Welte, Andy Green, Werner Almesberger and Matt Hsu
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  NOTE: This driver does not yet support subtractive ADC mode, which means
 *  you can do only one measurement per read request.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/completion.h>

#include <linux/mfd/pcf50606/core.h>
#include <linux/mfd/pcf50606/adc.h>

struct pcf50606_adc_request {
	int mux;
	int result;
	void (*callback)(struct pcf50606 *, void *, int);
	void *callback_param;

	/* Used in case of sync requests */
	struct completion completion;

};

/* code expects this to be a power of two */
#define PCF50606_MAX_ADC_FIFO_DEPTH 8

struct pcf50606_adc {
	struct pcf50606 *pcf;

	/* Private stuff */
	struct pcf50606_adc_request *queue[PCF50606_MAX_ADC_FIFO_DEPTH];
	unsigned int queue_head;
	unsigned int queue_tail;
	struct mutex queue_mutex;
};

static inline struct pcf50606_adc *pcf50606_to_adc(struct pcf50606 *pcf)
{
	return platform_get_drvdata(pcf->adc_pdev);
}

static void adc_setup(struct pcf50606 *pcf, int channel)
{
	channel &= PCF50606_ADCC2_ADCMUX_MASK;

	/* start ADC conversion of selected channel */
	pcf50606_reg_write(pcf, PCF50606_REG_ADCC2, channel |
		    PCF50606_ADCC2_ADCSTART | PCF50606_ADCC2_RES_10BIT);

}

static void trigger_next_adc_job_if_any(struct pcf50606 *pcf)
{
	struct pcf50606_adc *adc = pcf50606_to_adc(pcf);
	unsigned int head;

	mutex_lock(&adc->queue_mutex);

	head = adc->queue_head;

	if (!adc->queue[head])
		goto out;

	adc_setup(pcf, adc->queue[head]->mux);
out:
	mutex_unlock(&adc->queue_mutex);
}

static int
adc_enqueue_request(struct pcf50606 *pcf, struct pcf50606_adc_request *req)
{
	struct pcf50606_adc *adc = pcf50606_to_adc(pcf);
	unsigned int tail;

	mutex_lock(&adc->queue_mutex);
	tail = adc->queue_tail;

	if (adc->queue[tail]) {
		mutex_unlock(&adc->queue_mutex);
		return -EBUSY;
	}

	adc->queue[tail] = req;

	adc->queue_tail =
		(tail + 1) & (PCF50606_MAX_ADC_FIFO_DEPTH - 1);

	mutex_unlock(&adc->queue_mutex);

	trigger_next_adc_job_if_any(pcf);

	return 0;
}

static void
pcf50606_adc_sync_read_callback(struct pcf50606 *pcf, void *param, int result)
{
	struct pcf50606_adc_request *req;

	/*We know here that the passed param is an adc_request object */
	req = (struct pcf50606_adc_request *)param;

	req->result = result;
	complete(&req->completion);
}

int pcf50606_adc_sync_read(struct pcf50606 *pcf, int mux)
{

	struct pcf50606_adc_request *req;
	int result;

	/* req is freed when the result is ready, in irq handler*/
	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	req->mux = mux;
	req->callback =  pcf50606_adc_sync_read_callback;
	req->callback_param = req;
	init_completion(&req->completion);

	adc_enqueue_request(pcf, req);

	if (wait_for_completion_timeout(&req->completion, 5 * HZ) == 5 * HZ) {
		dev_err(pcf->dev, "ADC read timed out \n");
	}

	result = req->result;

	return result;
}
EXPORT_SYMBOL_GPL(pcf50606_adc_sync_read);

int pcf50606_adc_async_read(struct pcf50606 *pcf, int mux,
			     void (*callback)(struct pcf50606 *, void *, int),
			     void *callback_param)
{
	struct pcf50606_adc_request *req;

	/* req is freed when the result is ready, in pcf50606_work*/
	req = kmalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	req->mux = mux;
	req->callback = callback;
	req->callback_param = callback_param;

	adc_enqueue_request(pcf, req);

	return 0;
}
EXPORT_SYMBOL_GPL(pcf50606_adc_async_read);

static int adc_result(struct pcf50606 *pcf)
{
	int ret = (pcf50606_reg_read(pcf, PCF50606_REG_ADCS1) << 2) |
			(pcf50606_reg_read(pcf, PCF50606_REG_ADCS2) & 0x03);

	dev_dbg(pcf->dev, "adc result = %d\n", ret);

	return ret;
}

static void pcf50606_adc_irq(int irq, void *data)
{
	struct pcf50606_adc *adc = data;
	struct pcf50606 *pcf = adc->pcf;
	struct pcf50606_adc_request *req;
	unsigned int head;

	mutex_lock(&adc->queue_mutex);
	head = adc->queue_head;

	req = adc->queue[head];
	if (WARN_ON(!req)) {
		dev_err(pcf->dev, "pcf50606-adc irq: ADC queue empty!\n");
		mutex_unlock(&adc->queue_mutex);
		return;
	}

	adc->queue[head] = NULL;
	adc->queue_head = (head + 1) & (PCF50606_MAX_ADC_FIFO_DEPTH - 1);

	mutex_unlock(&adc->queue_mutex);

	req->callback(pcf, req->callback_param, adc_result(pcf));
	kfree(req);

	trigger_next_adc_job_if_any(pcf);
}

static int __devinit pcf50606_adc_probe(struct platform_device *pdev)
{
	struct pcf50606_adc *adc;

	adc = kzalloc(sizeof(*adc), GFP_KERNEL);
	if (!adc)
		return -ENOMEM;

	adc->pcf = dev_to_pcf50606(pdev->dev.parent);
	platform_set_drvdata(pdev, adc);

	pcf50606_register_irq(adc->pcf, PCF50606_IRQ_ADCRDY,
					pcf50606_adc_irq, adc);

	mutex_init(&adc->queue_mutex);

	return 0;
}

static int __devexit pcf50606_adc_remove(struct platform_device *pdev)
{
	struct pcf50606_adc *adc = platform_get_drvdata(pdev);
	unsigned int i;
	unsigned int head;

	pcf50606_free_irq(adc->pcf, PCF50606_IRQ_ADCRDY);

	mutex_lock(&adc->queue_mutex);
	head = adc->queue_head;

	if (WARN_ON(adc->queue[head]))
		dev_err(adc->pcf->dev,
			"adc driver removed with request pending\n");

	for (i = 0; i < PCF50606_MAX_ADC_FIFO_DEPTH; i++)
		kfree(adc->queue[i]);

	mutex_unlock(&adc->queue_mutex);
	kfree(adc);

	return 0;
}

struct platform_driver pcf50606_adc_driver = {
	.driver = {
		.name = "pcf50606-adc",
		.owner = THIS_MODULE,
	},
	.probe = pcf50606_adc_probe,
	.remove = __devexit_p(pcf50606_adc_remove),
};

static int __init pcf50606_adc_init(void)
{
		return platform_driver_register(&pcf50606_adc_driver);
}
module_init(pcf50606_adc_init);

static void __exit pcf50606_adc_exit(void)
{
		platform_driver_unregister(&pcf50606_adc_driver);
}
module_exit(pcf50606_adc_exit);

MODULE_AUTHOR("Balaji Rao <balajirrao@openmoko.org>");
MODULE_DESCRIPTION("PCF50606 adc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pcf50606-adc");

