/*
 * sound/soc/mvf-sgtl5000.c --  SoC audio for Faraday TWR-AUDIO-SGTL boards
 *				with sgtl5000 codec
 *
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/fsl_devices.h>
#include <linux/gpio.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <asm/mach-types.h>

#include "../codecs/sgtl5000.h"
#include "mvf-sai.h"


static struct mvf_sgtl5000_priv {
	int sysclk;
	int hw;
	struct platform_device *pdev;
} card_priv;

static struct snd_soc_card mvf_sgtl5000;

static int sgtl5000_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	u32 dai_format;
	int ret;
	unsigned int channels = params_channels(params);

	snd_soc_dai_set_sysclk(codec_dai, SGTL5000_SYSCLK, card_priv.sysclk, 1);

	dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
		SND_SOC_DAIFMT_CBM_CFM;

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, dai_format);
	if (ret < 0)
		return ret;

	/* TODO: The SAI driver should figure this out for us */
	switch (channels) {
	case 2:
		snd_soc_dai_set_tdm_slot(cpu_dai, 0xfffffffc, 0xfffffffc, 2, 0);
		break;
	case 1:
		snd_soc_dai_set_tdm_slot(cpu_dai, 0xfffffffe, 0xfffffffe, 1, 0);
		break;
	default:
		return -EINVAL;
	}

	/* set cpu DAI configuration */
	dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_IF |
		SND_SOC_DAIFMT_CBM_CFM;
	ret = snd_soc_dai_set_fmt(cpu_dai, dai_format);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops mvf_sgtl5000_hifi_ops = {
	.hw_params = sgtl5000_params,
};

static int sgtl5000_jack_func;
static int sgtl5000_spk_func;
static int sgtl5000_line_in_func;

static const char * const jack_function[] = { "off", "on"};

static const char * const spk_function[] = { "off", "on" };

static const char * const line_in_function[] = { "off", "on" };

static const struct soc_enum sgtl5000_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, jack_function),
	SOC_ENUM_SINGLE_EXT(2, spk_function),
	SOC_ENUM_SINGLE_EXT(2, line_in_function),
};

static int sgtl5000_get_jack(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = sgtl5000_jack_func;
	return 0;
}

static int sgtl5000_set_jack(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	if (sgtl5000_jack_func == ucontrol->value.enumerated.item[0])
		return 0;

	sgtl5000_jack_func = ucontrol->value.enumerated.item[0];
	if (sgtl5000_jack_func)
		snd_soc_dapm_enable_pin(&codec->dapm, "Headphone Jack");
	else
		snd_soc_dapm_disable_pin(&codec->dapm, "Headphone Jack");

	snd_soc_dapm_sync(&codec->dapm);
	return 1;
}

static int sgtl5000_get_spk(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = sgtl5000_spk_func;
	return 0;
}

static int sgtl5000_set_spk(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	if (sgtl5000_spk_func == ucontrol->value.enumerated.item[0])
		return 0;

	sgtl5000_spk_func = ucontrol->value.enumerated.item[0];
	if (sgtl5000_spk_func)
		snd_soc_dapm_enable_pin(&codec->dapm, "Ext Spk");
	else
		snd_soc_dapm_disable_pin(&codec->dapm, "Ext Spk");

	snd_soc_dapm_sync(&codec->dapm);
	return 1;
}

static int sgtl5000_get_line_in(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = sgtl5000_line_in_func;
	return 0;
}

static int sgtl5000_set_line_in(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	if (sgtl5000_line_in_func == ucontrol->value.enumerated.item[0])
		return 0;

	sgtl5000_line_in_func = ucontrol->value.enumerated.item[0];
	if (sgtl5000_line_in_func)
		snd_soc_dapm_enable_pin(&codec->dapm, "Line In Jack");
	else
		snd_soc_dapm_disable_pin(&codec->dapm, "Line In Jack");

	snd_soc_dapm_sync(&codec->dapm);
	return 1;
}

static const struct snd_soc_dapm_widget mvf_twr_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_LINE("Line In Jack", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
};

static const struct snd_kcontrol_new sgtl5000_machine_controls[] = {
	SOC_ENUM_EXT("Jack Function", sgtl5000_enum[0], sgtl5000_get_jack,
		     sgtl5000_set_jack),
	SOC_ENUM_EXT("Speaker Function", sgtl5000_enum[1], sgtl5000_get_spk,
		     sgtl5000_set_spk),
	SOC_ENUM_EXT("Line In Function", sgtl5000_enum[1], sgtl5000_get_line_in,
		     sgtl5000_set_line_in),
};

static const struct snd_soc_dapm_route audio_map[] = {

	/* Mic Jack --> MIC_IN (with automatic bias) */
	{"MIC_IN", NULL, "Mic Jack"},

	/* Line in Jack --> LINE_IN */
	{"LINE_IN", NULL, "Line In Jack"},

	/* HP_OUT --> Headphone Jack */
	{"Headphone Jack", NULL, "HP_OUT"},

	/* LINE_OUT --> Ext Speaker */
	{"Ext Spk", NULL, "LINE_OUT"},
};

static int mvf_twr_sgtl5000_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	int ret;

	ret = snd_soc_add_controls(codec, sgtl5000_machine_controls,
			ARRAY_SIZE(sgtl5000_machine_controls));
	if (ret)
		return ret;

	/* Add mvf_twr specific widgets */
	snd_soc_dapm_new_controls(&codec->dapm, mvf_twr_dapm_widgets,
				  ARRAY_SIZE(mvf_twr_dapm_widgets));

	/* Set up mvf_twr specific audio path audio_map */
	snd_soc_dapm_add_routes(&codec->dapm, audio_map, ARRAY_SIZE(audio_map));

	snd_soc_dapm_disable_pin(&codec->dapm, "Line In Jack");
	snd_soc_dapm_enable_pin(&codec->dapm, "Headphone Jack");
	snd_soc_dapm_sync(&codec->dapm);

	return 0;
}

static struct snd_soc_dai_link mvf_sgtl5000_dai[] = {
	{
		.name		= "HiFi",
		.stream_name	= "HiFi",
		.codec_dai_name	= "sgtl5000",
		.codec_name	= "sgtl5000.2-000a",
		.cpu_dai_name	= "mvf-sai.0",
		.platform_name	= "mvf-pcm-audio.0",
		.init		= mvf_twr_sgtl5000_init,
		.ops		= &mvf_sgtl5000_hifi_ops,
	},
};

static struct snd_soc_card mvf_sgtl5000 = {
	.name		= "sgtl5000-sai",
	.dai_link	= mvf_sgtl5000_dai,
	.num_links	= ARRAY_SIZE(mvf_sgtl5000_dai),
};

static struct platform_device *mvf_sgtl5000_snd_device;

static int __devinit mvf_sgtl5000_probe(struct platform_device *pdev)
{
	struct mxc_audio_platform_data *plat = pdev->dev.platform_data;

	card_priv.pdev = pdev;

	if (plat->init && plat->init())
		return -EINVAL;

	card_priv.sysclk =  plat->sysclk;;

	return 0;
}

static int mvf_sgtl5000_remove(struct platform_device *pdev)
{
	struct mxc_audio_platform_data *plat = pdev->dev.platform_data;

	if (plat->finit)
		plat->finit();

	return 0;
}

static struct platform_driver mvf_sgtl5000_audio_driver = {
	.probe = mvf_sgtl5000_probe,
	.remove = mvf_sgtl5000_remove,
	.driver = {
		   .name = "mvf-sgtl5000",
		   .owner = THIS_MODULE,
		   },
};

static int __init mvf_sgtl5000_init(void)
{
	int ret;

	ret = platform_driver_register(&mvf_sgtl5000_audio_driver);
	if (ret)
		return -ENOMEM;

	mvf_sgtl5000_dai[0].codec_name = "sgtl5000.2-000a";

	mvf_sgtl5000_snd_device = platform_device_alloc("soc-audio", 1);
	if (!mvf_sgtl5000_snd_device)
		return -ENOMEM;

	platform_set_drvdata(mvf_sgtl5000_snd_device, &mvf_sgtl5000);

	ret = platform_device_add(mvf_sgtl5000_snd_device);

	if (ret) {
		printk(KERN_ERR "ASoC: Platform device allocation failed\n");
		platform_device_put(mvf_sgtl5000_snd_device);
	}

	return ret;
}

static void __exit mvf_sgtl5000_exit(void)
{
	platform_driver_unregister(&mvf_sgtl5000_audio_driver);
	platform_device_unregister(mvf_sgtl5000_snd_device);
}

module_init(mvf_sgtl5000_init);
module_exit(mvf_sgtl5000_exit);

MODULE_AUTHOR("Alison Wang, <b18965@freescale.com>");
MODULE_DESCRIPTION("PhyCORE ALSA SoC driver");
MODULE_LICENSE("GPL");
