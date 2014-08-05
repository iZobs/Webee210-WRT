/*
 * mini6410_wm9713.c  --  SoC audio for MINI6410
 *
 * Copyright 2010 Samsung Electronics Co. Ltd.
 * Author: Jaswinder Singh Brar <jassi.brar@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/device.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include "../codecs/wm9713.h"
#include "s3c-dma.h"
#include "s3c-ac97.h"

static struct snd_soc_card mini6410;

/*
 Playback (HeadPhone):-
	$ amixer sset 'Headphone' unmute
	$ amixer sset 'Right Headphone Out Mux' 'Headphone'
	$ amixer sset 'Left Headphone Out Mux' 'Headphone'
	$ amixer sset 'Right HP Mixer PCM' unmute
	$ amixer sset 'Left HP Mixer PCM' unmute

 Capture (LineIn):-
	$ amixer sset 'Right Capture Source' 'Line'
	$ amixer sset 'Left Capture Source' 'Line'
*/

/* Machine dapm widgets */
static const struct snd_soc_dapm_widget mini6410_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Mic (on-board)", NULL),
};

/* audio map */
static struct snd_soc_dapm_route audio_map[] = {
	{ "MIC1", NULL, "Mic Bias" },
	{ "Mic Bias", NULL, "Mic (on-board)" },
};

static int mini6410_ac97_init(struct snd_soc_codec *codec) {
	unsigned short val;

	/* add board specific widgets */
	snd_soc_dapm_new_controls(codec, mini6410_dapm_widgets,
			ARRAY_SIZE(mini6410_dapm_widgets));

	/* setup board specific audio path audio_map */
	snd_soc_dapm_add_routes(codec, audio_map, ARRAY_SIZE(audio_map));

	/* Prepare MIC input */
	val = codec->read(codec, AC97_3D_CONTROL);
	codec->write(codec, AC97_3D_CONTROL, val | 0xc000);

	/* Static setup for now */
	snd_soc_dapm_enable_pin(codec, "Mic (on-board)");
	snd_soc_dapm_sync(codec);

	return 0;
}

static struct snd_soc_dai_link mini6410_dai = {
	.name = "AC97",
	.stream_name = "AC97 PCM",
	.cpu_dai = &s3c_ac97_dai[S3C_AC97_DAI_PCM],
	.codec_dai = &wm9713_dai[WM9713_DAI_AC97_HIFI],
	.init = mini6410_ac97_init,
};

static struct snd_soc_card mini6410 = {
	.name = "MINI6410",
	.platform = &s3c24xx_soc_platform,
	.dai_link = &mini6410_dai,
	.num_links = 1,
};

static struct snd_soc_device mini6410_snd_ac97_devdata = {
	.card = &mini6410,
	.codec_dev = &soc_codec_dev_wm9713,
};

static struct platform_device *mini6410_snd_ac97_device;

static int __init mini6410_init(void)
{
	int ret;

	mini6410_snd_ac97_device = platform_device_alloc("soc-audio", -1);
	if (!mini6410_snd_ac97_device)
		return -ENOMEM;

	platform_set_drvdata(mini6410_snd_ac97_device,
			     &mini6410_snd_ac97_devdata);
	mini6410_snd_ac97_devdata.dev = &mini6410_snd_ac97_device->dev;

	ret = platform_device_add(mini6410_snd_ac97_device);
	if (ret)
		platform_device_put(mini6410_snd_ac97_device);

	return ret;
}

static void __exit mini6410_exit(void)
{
	platform_device_unregister(mini6410_snd_ac97_device);
}

module_init(mini6410_init);
module_exit(mini6410_exit);

/* Module information */
MODULE_AUTHOR("Jaswinder Singh Brar, jassi.brar@samsung.com");
MODULE_DESCRIPTION("ALSA SoC MINI6410+WM9713");
MODULE_LICENSE("GPL");
