/*
 * Copyright (C) 2011-2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */
#include <mach/hardware.h>
#include <mach/devices-common.h>

#define imx_imx_asrc_data_entry(soc, _id,  _size)			\
	[_id] = {							\
		.id = _id,						\
		.iobase = soc ## _ASRC ## _BASE_ADDR,			\
		.iosize = _size,					\
		.irq = soc ## _INT_ASRC,				\
		.dmatx1 = soc ## _DMA_REQ_ASRC## _TX1,			\
		.dmarx1 = soc ## _DMA_REQ_ASRC## _RX1,			\
		.dmatx2 = soc ## _DMA_REQ_ASRC## _TX2,			\
		.dmarx2 = soc ## _DMA_REQ_ASRC## _RX2,			\
		.dmatx3 = soc ## _DMA_REQ_ASRC## _TX3,			\
		.dmarx3 = soc ## _DMA_REQ_ASRC## _RX3,			\
	}

#ifdef CONFIG_SOC_IMX53
const struct imx_imx_asrc_data imx53_imx_asrc_data[] __initconst = {
#define imx53_imx_asrc_data_entry(_id)				\
	imx_imx_asrc_data_entry(MX53, _id, SZ_4K)
	imx53_imx_asrc_data_entry(0),
};
#endif /* ifdef CONFIG_SOC_IMX6Q */

#ifdef CONFIG_SOC_IMX6Q
const struct imx_imx_asrc_data imx6q_imx_asrc_data[] __initconst = {
#define imx6q_imx_asrc_data_entry(_id)				\
	imx_imx_asrc_data_entry(MX6Q, _id, SZ_4K)
	imx6q_imx_asrc_data_entry(0),
};
#endif /* ifdef CONFIG_SOC_IMX6Q */

#ifdef CONFIG_SOC_MVFA5
const struct imx_imx_asrc_data mvf_imx_asrc_data[] __initconst = {
	[0] = {
		.id = 0,
		.iobase = MVF_ASRC_BASE_ADDR,
		.iosize = SZ_4K,
		.irq = MVF_INT_ASRC,
		.dmatx1 = DMA_MUX12_ASRC0_TX + 64,
		.dmarx1 = DMA_MUX12_ASRC0_RX + 64,
		.dmatx2 = DMA_MUX12_ASRC1_TX + 64,
		.dmarx2 = DMA_MUX12_ASRC1_RX + 64,
		.dmatx3 = DMA_MUX12_ASRC2_TX + 64,
		.dmarx3 = DMA_MUX12_ASRC2_RX + 64,
	},
};
#endif
struct platform_device *__init imx_add_imx_asrc(
		const struct imx_imx_asrc_data *data,
		const struct imx_asrc_platform_data *pdata)
{
	struct resource res[] = {
		{
			.start = data->iobase,
			.end = data->iobase + data->iosize - 1,
			.flags = IORESOURCE_MEM,
		}, {
			.start = data->irq,
			.end = data->irq,
			.flags = IORESOURCE_IRQ,
		},
#define DMARES(_name) {							\
	.name = #_name,							\
	.start = data->dma ## _name,					\
	.end = data->dma ## _name,					\
	.flags = IORESOURCE_DMA,					\
}
		DMARES(tx1),
		DMARES(rx1),
		DMARES(tx2),
		DMARES(rx2),
		DMARES(tx3),
		DMARES(rx3),
	};

	return imx_add_platform_device("mxc_asrc", data->id,
			res, ARRAY_SIZE(res),
			pdata, sizeof(*pdata));
}
