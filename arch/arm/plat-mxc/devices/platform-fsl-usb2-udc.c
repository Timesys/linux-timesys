/*
 * Copyright 2011-2012 Freescale Semiconductor, Inc.
 *
 * Copyright (C) 2010 Pengutronix
 * Uwe Kleine-Koenig <u.kleine-koenig@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */
#include <linux/dma-mapping.h>
#include <mach/hardware.h>
#include <mach/devices-common.h>

#define imx_fsl_usb2_udc_data_entry_single(soc)				\
	{								\
		.iobase = soc ## _USB_OTG_BASE_ADDR,			\
		.irq = soc ## _INT_USB_OTG,				\
	}

#define mvf_fsl_usb2_udc_data_entry_single(arch, _id)			\
	{								\
		.iobase = arch ## _USBC ## _id ## _BASE_ADDR,			\
		.irq = arch ## _INT_USB ## _id,				\
	}

#ifdef CONFIG_SOC_IMX25
const struct imx_fsl_usb2_udc_data imx25_fsl_usb2_udc_data __initconst =
	imx_fsl_usb2_udc_data_entry_single(MX25);
#endif /* ifdef CONFIG_SOC_IMX25 */

#ifdef CONFIG_SOC_IMX27
const struct imx_fsl_usb2_udc_data imx27_fsl_usb2_udc_data __initconst =
	imx_fsl_usb2_udc_data_entry_single(MX27);
#endif /* ifdef CONFIG_SOC_IMX27 */

#ifdef CONFIG_SOC_IMX31
const struct imx_fsl_usb2_udc_data imx31_fsl_usb2_udc_data __initconst =
	imx_fsl_usb2_udc_data_entry_single(MX31);
#endif /* ifdef CONFIG_SOC_IMX31 */

#ifdef CONFIG_SOC_IMX35
const struct imx_fsl_usb2_udc_data imx35_fsl_usb2_udc_data __initconst =
	imx_fsl_usb2_udc_data_entry_single(MX35);
#endif /* ifdef CONFIG_SOC_IMX35 */

#ifdef CONFIG_SOC_IMX6Q
const struct imx_fsl_usb2_udc_data imx6q_fsl_usb2_udc_data __initconst =
	imx_fsl_usb2_udc_data_entry_single(MX6Q);
#endif /* ifdef CONFIG_SOC_IMX6Q */

#ifdef CONFIG_ARCH_MVF
#if (defined CONFIG_MACH_PCM052 || defined CONFIG_MACH_PCL052)
const struct imx_fsl_usb2_udc_data mvf_fsl_usb2_udc_data[] __initconst = {
       mvf_fsl_usb2_udc_data_entry_single(MVF, 0),
       mvf_fsl_usb2_udc_data_entry_single(MVF, 1),
#else
const struct imx_fsl_usb2_udc_data mvf_fsl_usb2_udc_data __initconst = {
	.iobase = MVF_USBC0_BASE_ADDR,
	.irq = MVF_INT_USBOTG0,
#endif
};
#endif

struct platform_device *__init imx_add_fsl_usb2_udc(
		const struct imx_fsl_usb2_udc_data *data,
		const struct fsl_usb2_platform_data *pdata)
{
	struct resource res[] = {
		{
			.start = data->iobase,
			.end = data->iobase + SZ_512 - 1,
			.flags = IORESOURCE_MEM,
		}, {
			.start = data->irq,
			.end = data->irq,
			.flags = IORESOURCE_IRQ,
		},
	};
	return imx_add_platform_device_dmamask("fsl-usb2-udc", -1,
			res, ARRAY_SIZE(res),
			pdata, sizeof(*pdata), DMA_BIT_MASK(32));
}
