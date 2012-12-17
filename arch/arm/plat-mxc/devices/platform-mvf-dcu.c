/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <mach/hardware.h>
#include <mach/devices-common.h>
#include <mach/mvf-dcu-fb.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#define mvf_dcu_data_entry_single(soc, id, size, dcu_init)		\
	{								\
		.iobase = soc ## _DCU ## id ## _BASE_ADDR,		\
		.iosize = size,						\
		.irq = soc ## _INT_DCU ## id,				\
		.init = dcu_init,					\
	}

int __init mvf_dcu_init(int id)
{
	int ret = 0;

	ret = gpio_request_one(DCU_LCD_ENABLE_PIN, GPIOF_OUT_INIT_LOW, "DCU");
	if (ret)
		printk(KERN_ERR "DCU: failed to request GPIO 25\n");

	msleep(2);
	gpio_set_value(DCU_LCD_ENABLE_PIN, 0);

	writel(0x20000000, MVF_IO_ADDRESS(MVF_TCON0_BASE_ADDR));
	return ret;
}

const struct mvf_dcu_data mvfa5_dcu_data[] __initconst = {
	mvf_dcu_data_entry_single(MVF, 0, SZ_8K, mvf_dcu_init),
	mvf_dcu_data_entry_single(MVF, 1, SZ_8K, mvf_dcu_init),
};

struct platform_device *__init mvf_add_dcu(
		const int id,
		const struct mvf_dcu_data *data,
		struct mvf_dcu_platform_data *pdata)
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
	};

	pdata->init = data->init;

	return imx_add_platform_device("mvf-dcu", id, res, ARRAY_SIZE(res),
			pdata, sizeof(*pdata));
}
