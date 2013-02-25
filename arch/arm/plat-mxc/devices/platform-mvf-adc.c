/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <asm/sizes.h>
#include <mach/hardware.h>
#include <mach/devices-common.h>
#include <linux/clk.h>
#include <linux/gpio.h>

#define mvf_adc_data_entry_single(soc, _id, _size)		\
	{							\
		.id = _id,					\
		.iobase = soc ## _ADC ## _id ## _BASE_ADDR,	\
		.iosize = _size,				\
		.irq = soc ## _INT_ADC ## _id,			\
	}
#define mvf_adc_data_entry(soc, _id, _size)	\
	[_id] = mvf_adc_data_entry_single(soc, _id, _size)

#ifdef CONFIG_SOC_MVFA5
const struct mvf_adc_data mvfa5_adc_data[] __initconst = {
	 mvf_adc_data_entry(MVF, 0, SZ_4K),
};
#endif

struct platform_device *__init mvf_add_adcdev(
		const struct mvf_adc_data *data)
{
	struct resource res[] = {
		{
			.start = data->iobase,
			.end = data->iobase + data->iosize - 1,
			.flags = IORESOURCE_MEM,
		},
		{
			.start = data->irq,
			.end = data->irq,
			.flags = IORESOURCE_IRQ,
		},
	};

	return imx_add_platform_device("mvf-adc", data->id, res,
			ARRAY_SIZE(res), NULL, 0);
}
