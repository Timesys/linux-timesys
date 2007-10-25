/*
 * Author: MontaVista Software, Inc.
 *       <source@mvista.com>
 *
 * Based on the OMAP devices.c
 *
 * 2005 (c) MontaVista Software, Inc. This file is licensed under the
 * terms of the GNU General Public License version 2. This program is
 * licensed "as is" without any warranty of any kind, whether express
 * or implied.
 *
 * Copyright 2005-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/clk.h>

#include <linux/spi/spi.h>

#include <asm/hardware.h>
#include <asm/arch/pmic_external.h>
#include <asm/arch/pmic_power.h>
#include <asm/arch/mmc.h>

#include <asm/arch/spba.h>
#include "iomux.h"
#include <asm/arch/sdma.h>
#include "sdma_script_code.h"
#include "sdma_script_code_pass2.h"
#include "board-mx31ads.h"

#if 0
int board_device_enable(u32 device_id);
int board_device_disable(u32 device_id);

int mxc_device_enable(u32 device_id)
{
	int ret = 0;

	switch (device_id) {
	default:
		ret = board_device_enable(device_id);
	}

	return ret;
}

int mxc_device_disable(u32 device_id)
{
	int ret = 0;

	switch (device_id) {
	default:
		ret = board_device_disable(device_id);
	}
	return ret;
}

EXPORT_SYMBOL(mxc_device_enable);
EXPORT_SYMBOL(mxc_device_disable);
#endif

#ifndef CONFIG_MXC_DPTC
extern struct dptc_wp dptc_wp_allfreq_26ckih[DPTC_WP_SUPPORTED];
extern struct dptc_wp dptc_wp_allfreq_27ckih[DPTC_WP_SUPPORTED];
extern struct dptc_wp dptc_wp_allfreq_26ckih_TO_2_0[DPTC_WP_SUPPORTED];
/*
 * Clock structures 
 */
static struct clk *ckih_clk;
#endif

void mxc_sdma_get_script_info(sdma_script_start_addrs * sdma_script_addr)
{
	if (cpu_is_mx31_rev(CHIP_REV_1_0) == 1) {
		sdma_script_addr->mxc_sdma_app_2_mcu_addr = app_2_mcu_ADDR;
		sdma_script_addr->mxc_sdma_ap_2_ap_addr = ap_2_ap_ADDR;
		sdma_script_addr->mxc_sdma_ap_2_bp_addr = -1;
		sdma_script_addr->mxc_sdma_bp_2_ap_addr = -1;
		sdma_script_addr->mxc_sdma_loopback_on_dsp_side_addr = -1;
		sdma_script_addr->mxc_sdma_mcu_2_app_addr = mcu_2_app_ADDR;
		sdma_script_addr->mxc_sdma_mcu_2_shp_addr = mcu_2_shp_ADDR;
		sdma_script_addr->mxc_sdma_mcu_interrupt_only_addr = -1;
		sdma_script_addr->mxc_sdma_shp_2_mcu_addr = shp_2_mcu_ADDR;
		sdma_script_addr->mxc_sdma_start_addr =
		    (unsigned short *)sdma_code;
		sdma_script_addr->mxc_sdma_uartsh_2_mcu_addr =
		    uartsh_2_mcu_ADDR;
		sdma_script_addr->mxc_sdma_uart_2_mcu_addr = uart_2_mcu_ADDR;
		sdma_script_addr->mxc_sdma_ram_code_size = RAM_CODE_SIZE;
		sdma_script_addr->mxc_sdma_ram_code_start_addr =
		    RAM_CODE_START_ADDR;
		sdma_script_addr->mxc_sdma_dptc_dvfs_addr = dptc_dvfs_ADDR;
		sdma_script_addr->mxc_sdma_firi_2_mcu_addr = firi_2_mcu_ADDR;
		sdma_script_addr->mxc_sdma_firi_2_per_addr = -1;
		sdma_script_addr->mxc_sdma_mshc_2_mcu_addr = mshc_2_mcu_ADDR;
		sdma_script_addr->mxc_sdma_per_2_app_addr = -1;
		sdma_script_addr->mxc_sdma_per_2_firi_addr = -1;
		sdma_script_addr->mxc_sdma_per_2_shp_addr = -1;
		sdma_script_addr->mxc_sdma_mcu_2_ata_addr = mcu_2_ata_ADDR;
		sdma_script_addr->mxc_sdma_mcu_2_firi_addr = mcu_2_firi_ADDR;
		sdma_script_addr->mxc_sdma_mcu_2_mshc_addr = mcu_2_mshc_ADDR;
		sdma_script_addr->mxc_sdma_ata_2_mcu_addr = ata_2_mcu_ADDR;
		sdma_script_addr->mxc_sdma_uartsh_2_per_addr = -1;
		sdma_script_addr->mxc_sdma_shp_2_per_addr = -1;
		sdma_script_addr->mxc_sdma_uart_2_per_addr = -1;
		sdma_script_addr->mxc_sdma_app_2_per_addr = -1;
	} else {
		sdma_script_addr->mxc_sdma_app_2_mcu_addr =
		    app_2_mcu_patched_ADDR_2;
		sdma_script_addr->mxc_sdma_ap_2_ap_addr = ap_2_ap_ADDR_2;
		sdma_script_addr->mxc_sdma_ap_2_bp_addr = ap_2_bp_ADDR_2;
		sdma_script_addr->mxc_sdma_bp_2_ap_addr = bp_2_ap_ADDR_2;
		sdma_script_addr->mxc_sdma_loopback_on_dsp_side_addr = -1;
		sdma_script_addr->mxc_sdma_mcu_2_app_addr = mcu_2_app_ADDR_2;
		sdma_script_addr->mxc_sdma_mcu_2_shp_addr =
		    mcu_2_shp_patched_ADDR_2;
		sdma_script_addr->mxc_sdma_mcu_interrupt_only_addr = -1;
		sdma_script_addr->mxc_sdma_shp_2_mcu_addr =
		    shp_2_mcu_patched_ADDR_2;
		sdma_script_addr->mxc_sdma_start_addr =
		    (unsigned short *)sdma_code_2;
		sdma_script_addr->mxc_sdma_uartsh_2_mcu_addr =
		    uartsh_2_mcu_patched_ADDR_2;
		sdma_script_addr->mxc_sdma_uart_2_mcu_addr =
		    uart_2_mcu_patched_ADDR_2;
		sdma_script_addr->mxc_sdma_ram_code_size = RAM_CODE_SIZE_2;
		sdma_script_addr->mxc_sdma_ram_code_start_addr =
		    RAM_CODE_START_ADDR_2;
		sdma_script_addr->mxc_sdma_dptc_dvfs_addr = -1;
		sdma_script_addr->mxc_sdma_firi_2_mcu_addr = firi_2_mcu_ADDR_2;
		sdma_script_addr->mxc_sdma_firi_2_per_addr = -1;
		sdma_script_addr->mxc_sdma_mshc_2_mcu_addr = -1;
		sdma_script_addr->mxc_sdma_per_2_app_addr = -1;
		sdma_script_addr->mxc_sdma_per_2_firi_addr = -1;
		sdma_script_addr->mxc_sdma_per_2_shp_addr = per_2_shp_ADDR_2;
		sdma_script_addr->mxc_sdma_mcu_2_ata_addr = mcu_2_ata_ADDR_2;
		sdma_script_addr->mxc_sdma_mcu_2_firi_addr = mcu_2_firi_ADDR_2;
		sdma_script_addr->mxc_sdma_mcu_2_mshc_addr = -1;
		sdma_script_addr->mxc_sdma_ata_2_mcu_addr = ata_2_mcu_ADDR_2;
		sdma_script_addr->mxc_sdma_uartsh_2_per_addr = -1;
		sdma_script_addr->mxc_sdma_shp_2_per_addr = shp_2_per_ADDR_2;
		sdma_script_addr->mxc_sdma_uart_2_per_addr = -1;
		sdma_script_addr->mxc_sdma_app_2_per_addr = -1;
	}
}

static void mxc_nop_release(struct device *dev)
{
	/* Nothing */
}

#if defined(CONFIG_W1_MASTER_MXC) || defined(CONFIG_W1_MASTER_MXC_MODULE)
static struct mxc_w1_config mxc_w1_data = {
	.search_rom_accelerator = 0,
};

static struct platform_device mxc_w1_devices = {
	.name = "mxc_w1",
	.dev = {
		.release = mxc_nop_release,
		.platform_data = &mxc_w1_data,
		},
	.id = 0
};

static void mxc_init_owire(void)
{
	(void)platform_device_register(&mxc_w1_devices);
}
#else
static inline void mxc_init_owire(void)
{
}
#endif

#if defined(CONFIG_RTC_MXC) || defined(CONFIG_RTC_MXC_MODULE)
static struct resource rtc_resources[] = {
	{
	 .start = RTC_BASE_ADDR,
	 .end = RTC_BASE_ADDR + 0x30,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = INT_RTC,
	 .flags = IORESOURCE_IRQ,
	 },
};
static struct platform_device mxc_rtc_device = {
	.name = "mxc_rtc",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		},
	.num_resources = ARRAY_SIZE(rtc_resources),
	.resource = rtc_resources,
};
static void mxc_init_rtc(void)
{
	(void)platform_device_register(&mxc_rtc_device);
}
#else
static inline void mxc_init_rtc(void)
{
}
#endif

#if defined(CONFIG_MXC_WATCHDOG) || defined(CONFIG_MXC_WATCHDOG_MODULE)

static struct resource wdt_resources[] = {
	{
	 .start = WDOG1_BASE_ADDR,
	 .end = WDOG1_BASE_ADDR + 0x30,
	 .flags = IORESOURCE_MEM,
	 },
};

static struct platform_device mxc_wdt_device = {
	.name = "mxc_wdt",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		},
	.num_resources = ARRAY_SIZE(wdt_resources),
	.resource = wdt_resources,
};

static void mxc_init_wdt(void)
{
	(void)platform_device_register(&mxc_wdt_device);
}
#else
static inline void mxc_init_wdt(void)
{
}
#endif

#if defined(CONFIG_MXC_IPU) || defined(CONFIG_MXC_IPU_MODULE)
static struct mxc_ipu_config mxc_ipu_data = {
	.rev = 1,
};

static struct resource ipu_resources[] = {
	{
	 .start = IPU_CTRL_BASE_ADDR,
	 .end = IPU_CTRL_BASE_ADDR + SZ_4K,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = INT_IPU_SYN,
	 .flags = IORESOURCE_IRQ,
	 },
	{
	 .start = INT_IPU_ERR,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct platform_device mxc_ipu_device = {
	.name = "mxc_ipu",
	.id = -1,
	.dev = {
		.release = mxc_nop_release,
		.platform_data = &mxc_ipu_data,
		},
	.num_resources = ARRAY_SIZE(ipu_resources),
	.resource = ipu_resources,
};

static void mxc_init_ipu(void)
{
	platform_device_register(&mxc_ipu_device);
}
#else
static inline void mxc_init_ipu(void)
{
}
#endif

#if defined(CONFIG_MXC_FIR) || defined(CONFIG_MXC_FIR_MODULE)
/*!
 * Resource definition for the FIR
 */
static struct resource mxcir_resources[] = {
	[0] = {
	       .start = UART2_BASE_ADDR,
	       .end = UART2_BASE_ADDR + SZ_16K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = INT_UART2,
	       .end = INT_UART2,
	       .flags = IORESOURCE_IRQ,
	       },
	[2] = {
	       .start = FIRI_BASE_ADDR,
	       .end = FIRI_BASE_ADDR + SZ_16K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[3] = {
	       .start = INT_FIRI,
	       .end = INT_FIRI,
	       .flags = IORESOURCE_IRQ,
	       },
	[4] = {
	       .start = INT_UART2,
	       .end = INT_UART2,
	       .flags = IORESOURCE_IRQ,
	       }
};

static struct mxc_ir_platform_data ir_data;

/*! Device Definition for MXC FIR */
static struct platform_device mxcir_device = {
	.name = "mxcir",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		.platform_data = &ir_data,
		},
	.num_resources = ARRAY_SIZE(mxcir_resources),
	.resource = mxcir_resources,
};

static inline void mxc_init_ir(void)
{
	ir_data.uart_ir_mux = 1;
	ir_data.uart_clk = clk_get(NULL, "uart_clk.1");;
	(void)platform_device_register(&mxcir_device);
}
#else
static inline void mxc_init_ir(void)
{
}
#endif
/*!
 * This is platform device structure for adding SCC
 */
#if defined(CONFIG_MXC_SECURITY_SCC) || defined(CONFIG_MXC_SECURITY_SCC_MODULE)
static struct platform_device mxc_scc_device = {
	.name = "mxc_scc",
	.id = 0,
};

static void mxc_init_scc(void)
{
	platform_device_register(&mxc_scc_device);
}
#else
static inline void mxc_init_scc(void)
{
}
#endif
/* MMC device data */

#if defined(CONFIG_MMC_MXC) || defined(CONFIG_MMC_MXC_MODULE)

extern unsigned int sdhc_get_card_det_status(struct device *dev);
extern int sdhc_init_card_det(int id);

static struct mxc_mmc_platform_data mmc_data = {
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30,
	.min_clk = 150000,
	.max_clk = 25000000,
	.card_inserted_state = 1,
	.status = sdhc_get_card_det_status,
};

/*!
 * Resource definition for the SDHC1
 */
static struct resource mxcsdhc1_resources[] = {
	[0] = {
	       .start = MMC_SDHC1_BASE_ADDR,
	       .end = MMC_SDHC1_BASE_ADDR + SZ_16K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = INT_MMC_SDHC1,
	       .end = INT_MMC_SDHC1,
	       .flags = IORESOURCE_IRQ,
	       },
	[2] = {
	       .start = 0,
	       .end = 0,
	       .flags = IORESOURCE_IRQ,
	       },
	[3] = {
	       .start = MXC_SDIO1_CARD_IRQ,
	       .end = MXC_SDIO1_CARD_IRQ,
	       .flags = IORESOURCE_IRQ,
	       },
};

/*!
 * Resource definition for the SDHC2
 */
static struct resource mxcsdhc2_resources[] = {
	[0] = {
	       .start = MMC_SDHC2_BASE_ADDR,
	       .end = MMC_SDHC2_BASE_ADDR + SZ_16K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = INT_MMC_SDHC2,
	       .end = INT_MMC_SDHC2,
	       .flags = IORESOURCE_IRQ,
	       },
	[2] = {
	       .start = 0,
	       .end = 0,
	       .flags = IORESOURCE_IRQ,
	       },
	[3] = {
	       .start = MXC_SDIO2_CARD_IRQ,
	       .end = MXC_SDIO2_CARD_IRQ,
	       .flags = IORESOURCE_IRQ,
	       },
};

/*! Device Definition for MXC SDHC1 */
static struct platform_device mxcsdhc1_device = {
	.name = "mxcmci",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		.platform_data = &mmc_data,
		},
	.num_resources = ARRAY_SIZE(mxcsdhc1_resources),
	.resource = mxcsdhc1_resources,
};

/*! Device Definition for MXC SDHC2 */
static struct platform_device mxcsdhc2_device = {
	.name = "mxcmci",
	.id = 1,
	.dev = {
		.release = mxc_nop_release,
		.platform_data = &mmc_data,
		},
	.num_resources = ARRAY_SIZE(mxcsdhc2_resources),
	.resource = mxcsdhc2_resources,
};

static inline void mxc_init_mmc(void)
{
	int cd_irq;

	cd_irq = sdhc_init_card_det(0);
	if (cd_irq) {
		mxcsdhc1_device.resource[2].start = cd_irq;
		mxcsdhc1_device.resource[2].end = cd_irq;
	}
	cd_irq = sdhc_init_card_det(1);
	if (cd_irq) {
		mxcsdhc2_device.resource[2].start = cd_irq;
		mxcsdhc2_device.resource[2].end = cd_irq;
	}

	spba_take_ownership(SPBA_SDHC1, SPBA_MASTER_A | SPBA_MASTER_C);
	(void)platform_device_register(&mxcsdhc1_device);
	spba_take_ownership(SPBA_SDHC2, SPBA_MASTER_A | SPBA_MASTER_C);
	(void)platform_device_register(&mxcsdhc2_device);
}
#else
static inline void mxc_init_mmc(void)
{
}
#endif

/* SPI controller and device data */
#if defined(CONFIG_SPI_MXC) || defined(CONFIG_SPI_MXC_MODULE)

#ifdef CONFIG_SPI_MXC_SELECT1
/*!
 * Resource definition for the CSPI1
 */
static struct resource mxcspi1_resources[] = {
	[0] = {
	       .start = CSPI1_BASE_ADDR,
	       .end = CSPI1_BASE_ADDR + SZ_4K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = INT_CSPI1,
	       .end = INT_CSPI1,
	       .flags = IORESOURCE_IRQ,
	       },
};

/*! Platform Data for MXC CSPI1 */
static struct mxc_spi_master mxcspi1_data = {
	.maxchipselect = 4,
	.spi_version = 4,
};

/*! Device Definition for MXC CSPI1 */
static struct platform_device mxcspi1_device = {
	.name = "mxc_spi",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		.platform_data = &mxcspi1_data,
		},
	.num_resources = ARRAY_SIZE(mxcspi1_resources),
	.resource = mxcspi1_resources,
};

#endif				/* CONFIG_SPI_MXC_SELECT1 */

#ifdef CONFIG_SPI_MXC_SELECT2
/*!
 * Resource definition for the CSPI2
 */
static struct resource mxcspi2_resources[] = {
	[0] = {
	       .start = CSPI2_BASE_ADDR,
	       .end = CSPI2_BASE_ADDR + SZ_4K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = INT_CSPI2,
	       .end = INT_CSPI2,
	       .flags = IORESOURCE_IRQ,
	       },
};

/*! Platform Data for MXC CSPI2 */
static struct mxc_spi_master mxcspi2_data = {
	.maxchipselect = 4,
	.spi_version = 4,
};

/*! Device Definition for MXC CSPI2 */
static struct platform_device mxcspi2_device = {
	.name = "mxc_spi",
	.id = 1,
	.dev = {
		.release = mxc_nop_release,
		.platform_data = &mxcspi2_data,
		},
	.num_resources = ARRAY_SIZE(mxcspi2_resources),
	.resource = mxcspi2_resources,
};
#endif				/* CONFIG_SPI_MXC_SELECT2 */

#ifdef CONFIG_SPI_MXC_SELECT3
/*!
 * Resource definition for the CSPI3
 */
static struct resource mxcspi3_resources[] = {
	[0] = {
	       .start = CSPI3_BASE_ADDR,
	       .end = CSPI3_BASE_ADDR + SZ_4K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = INT_CSPI3,
	       .end = INT_CSPI3,
	       .flags = IORESOURCE_IRQ,
	       },
};

/*! Platform Data for MXC CSPI3 */
static struct mxc_spi_master mxcspi3_data = {
	.maxchipselect = 4,
	.spi_version = 4,
};

/*! Device Definition for MXC CSPI3 */
static struct platform_device mxcspi3_device = {
	.name = "mxc_spi",
	.id = 2,
	.dev = {
		.release = mxc_nop_release,
		.platform_data = &mxcspi3_data,
		},
	.num_resources = ARRAY_SIZE(mxcspi3_resources),
	.resource = mxcspi3_resources,
};
#endif				/* CONFIG_SPI_MXC_SELECT3 */

static inline void mxc_init_spi(void)
{
	/* SPBA configuration for CSPI2 - MCU is set */
	spba_take_ownership(SPBA_CSPI2, SPBA_MASTER_A);
#ifdef CONFIG_SPI_MXC_SELECT1
	if (platform_device_register(&mxcspi1_device) < 0)
		printk("Error: Registering the SPI Controller_1\n");
#endif				/* CONFIG_SPI_MXC_SELECT1 */
#ifdef CONFIG_SPI_MXC_SELECT2
	if (platform_device_register(&mxcspi2_device) < 0)
		printk("Error: Registering the SPI Controller_2\n");
#endif				/* CONFIG_SPI_MXC_SELECT2 */
#ifdef CONFIG_SPI_MXC_SELECT3
	if (platform_device_register(&mxcspi3_device) < 0)
		printk("Error: Registering the SPI Controller_3\n");
#endif				/* CONFIG_SPI_MXC_SELECT3 */
}
#else
static inline void mxc_init_spi(void)
{
}
#endif

/* I2C controller and device data */
#if defined(CONFIG_I2C_MXC) || defined(CONFIG_I2C_MXC_MODULE)

#ifdef CONFIG_I2C_MXC_SELECT1
/*!
 * Resource definition for the I2C1
 */
static struct resource mxci2c1_resources[] = {
	[0] = {
	       .start = I2C_BASE_ADDR,
	       .end = I2C_BASE_ADDR + SZ_4K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = INT_I2C,
	       .end = INT_I2C,
	       .flags = IORESOURCE_IRQ,
	       },
};

/*! Platform Data for MXC I2C */
static struct mxc_i2c_platform_data mxci2c1_data = {
	.i2c_clk = 100000,
};
#endif

#ifdef CONFIG_I2C_MXC_SELECT2
/*!
 * Resource definition for the I2C2
 */
static struct resource mxci2c2_resources[] = {
	[0] = {
	       .start = I2C2_BASE_ADDR,
	       .end = I2C2_BASE_ADDR + SZ_4K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = INT_I2C2,
	       .end = INT_I2C2,
	       .flags = IORESOURCE_IRQ,
	       },
};

/*! Platform Data for MXC I2C */
static struct mxc_i2c_platform_data mxci2c2_data = {
	.i2c_clk = 100000,
};
#endif

#ifdef CONFIG_I2C_MXC_SELECT3
/*!
 * Resource definition for the I2C3
 */
static struct resource mxci2c3_resources[] = {
	[0] = {
	       .start = I2C3_BASE_ADDR,
	       .end = I2C3_BASE_ADDR + SZ_4K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = INT_I2C3,
	       .end = INT_I2C3,
	       .flags = IORESOURCE_IRQ,
	       },
};

/*! Platform Data for MXC I2C */
static struct mxc_i2c_platform_data mxci2c3_data = {
	.i2c_clk = 100000,
};
#endif

/*! Device Definition for MXC I2C1 */
static struct platform_device mxci2c_devices[] = {
#ifdef CONFIG_I2C_MXC_SELECT1
	{
	 .name = "mxc_i2c",
	 .id = 0,
	 .dev = {
		 .release = mxc_nop_release,
		 .platform_data = &mxci2c1_data,
		 },
	 .num_resources = ARRAY_SIZE(mxci2c1_resources),
	 .resource = mxci2c1_resources,},
#endif
#ifdef CONFIG_I2C_MXC_SELECT2
	{
	 .name = "mxc_i2c",
	 .id = 1,
	 .dev = {
		 .release = mxc_nop_release,
		 .platform_data = &mxci2c2_data,
		 },
	 .num_resources = ARRAY_SIZE(mxci2c2_resources),
	 .resource = mxci2c2_resources,},
#endif
#ifdef CONFIG_I2C_MXC_SELECT3
	{
	 .name = "mxc_i2c",
	 .id = 2,
	 .dev = {
		 .release = mxc_nop_release,
		 .platform_data = &mxci2c3_data,
		 },
	 .num_resources = ARRAY_SIZE(mxci2c3_resources),
	 .resource = mxci2c3_resources,},
#endif
};

static inline void mxc_init_i2c(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mxci2c_devices); i++) {
		if (platform_device_register(&mxci2c_devices[i]) < 0)
			dev_err(&mxci2c_devices[i].dev,
				"Unable to register I2C device\n");
	}
}
#else
static inline void mxc_init_i2c(void)
{
}
#endif

struct mxc_gpio_port mxc_gpio_ports[GPIO_PORT_NUM] = {
	{
	 .num = 0,
	 .base = IO_ADDRESS(GPIO1_BASE_ADDR),
	 .irq = INT_GPIO1,
	 .virtual_irq_start = MXC_GPIO_BASE,
	 },
	{
	 .num = 1,
	 .base = IO_ADDRESS(GPIO2_BASE_ADDR),
	 .irq = INT_GPIO2,
	 .virtual_irq_start = MXC_GPIO_BASE + GPIO_NUM_PIN,
	 },
	{
	 .num = 2,
	 .base = IO_ADDRESS(GPIO3_BASE_ADDR),
	 .irq = INT_GPIO3,
	 .virtual_irq_start = MXC_GPIO_BASE + GPIO_NUM_PIN * 2,
	 },
};

#if defined(CONFIG_PCMCIA_MX31ADS) || defined(CONFIG_PCMCIA_MX31ADS_MODULE)

static struct platform_device mx31ads_device = {
	.name = "Mx31ads_pcmcia_socket",
	.id = 0,
	.dev.release = mxc_nop_release,
};
static inline void mxc_init_pcmcia(void)
{
	platform_device_register(&mx31ads_device);
}
#else
static inline void mxc_init_pcmcia(void)
{
}
#endif

#if defined(CONFIG_MXC_HMP4E) || defined(CONFIG_MXC_HMP4E_MODULE)
static struct platform_device hmp4e_device = {
	.name = "mxc_hmp4e",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		}
};

static inline void mxc_init_hmp4e(void)
{
	if (cpu_is_mx32())
		return;

	/* override fuse for Hantro HW clock */
	if (readl(IO_ADDRESS(IIM_BASE_ADDR + 0x808)) == 0x4) {
		if (!(readl(IO_ADDRESS(IIM_BASE_ADDR + 0x800)) & (1 << 5))) {
			writel(readl(IO_ADDRESS(IIM_BASE_ADDR + 0x808)) &
			       0xfffffffb, IO_ADDRESS(IIM_BASE_ADDR + 0x808));
		}
	}

	platform_device_register(&hmp4e_device);
}
#else
static inline void mxc_init_hmp4e(void)
{
}
#endif

static struct platform_device mxc_dma_device = {
	.name = "mxc_dma",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		},
};

static inline void mxc_init_dma(void)
{
	(void)platform_device_register(&mxc_dma_device);
}

#ifndef CONFIG_MXC_DPTC
/*! Device Definition for DPTC */
static struct platform_device mxc_dptc_device = {
	.name = "mxc_dptc",
	.dev = {
		.release = mxc_nop_release,
		.platform_data = &dptc_wp_allfreq_26ckih,
		},
};

static inline void mxc_init_dptc(void)
{
	if (clk_get_rate(ckih_clk) == 27000000
	    && mxc_cpu_is_rev(CHIP_REV_2_0) < 0) {
		mxc_dptc_device.dev.platform_data = &dptc_wp_allfreq_27ckih;
	} else if (clk_get_rate(ckih_clk) == 26000000
		   && mxc_cpu_is_rev(CHIP_REV_2_0)) {
		mxc_dptc_device.dev.platform_data =
		    &dptc_wp_allfreq_26ckih_TO_2_0;
	}

	(void)platform_device_register(&mxc_dptc_device);
}
#endif

#ifdef	CONFIG_MXC_VPU
static struct resource vpu_resources[] = {
	{
	 .start = VL2CC_BASE_ADDR,
	 .end = VL2CC_BASE_ADDR + SZ_8K - 1,
	 .flags = IORESOURCE_MEM,
	 },
};

/*! Platform Data for MXC VPU */
static struct platform_device mxcvpu_device = {
	.name = "mxc_vpu",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		},
	.num_resources = ARRAY_SIZE(vpu_resources),
	.resource = vpu_resources,
};

static inline void mxc_init_vpu(void)
{
	if (cpu_is_mx32()) {
		if (platform_device_register(&mxcvpu_device) < 0)
			printk(KERN_ERR "Error: Registering the VPU.\n");
	}
}
#else
static inline void mxc_init_vpu(void)
{
}
#endif

#if defined(CONFIG_PATA_FSL) || defined(CONFIG_PATA_FSL_MODULE)
static struct clk *ata_clk;
extern void gpio_ata_active(void);
extern void gpio_ata_inactive(void);

static int ata_init(struct platform_device *pdev)
{
	/* Select group B pins and enable the external MUX */

	__raw_writew(PBC_BCTRL2_ATA_SEL, PBC_BASE_ADDRESS + PBC_BCTRL2_SET);
	__raw_writew(PBC_BCTRL2_ATA_EN, PBC_BASE_ADDRESS + PBC_BCTRL2_CLEAR);

	/* Configure the pins */

	gpio_ata_active();

	/* Enable the clock */

	ata_clk = clk_get(&pdev->dev, "ata_clk.0");
	clk_enable(ata_clk);

	return 0;
}

static void ata_exit(void)
{
	/* Disable the clock */

	clk_disable(ata_clk);
	clk_put(ata_clk);
	ata_clk = NULL;

	/* Disable the MUX */

	__raw_writew(PBC_BCTRL2_ATA_EN, PBC_BASE_ADDRESS + PBC_BCTRL2_SET);

	/* Free the pins */

	gpio_ata_inactive();
}

static int ata_get_clk_rate(void)
{
	return clk_get_rate(ata_clk);
}

static struct fsl_ata_platform_data ata_data = {
	.udma_mask        = 0x0F, /* board can handle up to UDMA3 */
	.fifo_alarm       = MXC_IDE_DMA_WATERMARK / 2,
	.max_sg           = MXC_IDE_DMA_BD_NR,
	.init             = ata_init,
	.exit             = ata_exit,
	.get_clk_rate     = ata_get_clk_rate,
};

static struct resource pata_fsl_resources[] = {
	[0] = {		/* I/O */
		.start		= IO_ADDRESS(ATA_BASE_ADDR + 0x00),
		.end		= IO_ADDRESS(ATA_BASE_ADDR + 0xD8),
		.flags		= IORESOURCE_MEM,
	},
	[2] = {		/* IRQ */
		.start		= INT_ATA,
		.end		= INT_ATA,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device pata_fsl_device = {
	.name			= "pata_fsl",
	.id			= -1,
	.num_resources		= ARRAY_SIZE(pata_fsl_resources),
	.resource		= pata_fsl_resources,
	.dev			= {
		.platform_data	= &ata_data,
		.coherent_dma_mask = ~0,	/* $$$ REVISIT */
	},
};

static inline void mxc_init_pata(void)
{
	(void)platform_device_register(&pata_fsl_device);
}
#else /* CONFIG_PATA_FSL */
static inline void mxc_init_pata(void)
{
}
#endif /* CONFIG_PATA_FSL */

static int __init mxc_init_devices(void)
{
	mxc_init_wdt();
	mxc_init_ipu();
	mxc_init_mmc();
	mxc_init_ir();
	mxc_init_spi();
	mxc_init_i2c();
	mxc_init_rtc();
	mxc_init_owire();
	mxc_init_pcmcia();
	mxc_init_scc();
	mxc_init_hmp4e();
	mxc_init_dma();
#ifndef CONFIG_MXC_DPTC
	ckih_clk = clk_get(NULL, "ckih");
	mxc_init_dptc();
#endif
	mxc_init_vpu();
	mxc_init_pata();

	/* SPBA configuration for SSI2 - SDMA and MCU are set */
	spba_take_ownership(SPBA_SSI2, SPBA_MASTER_C | SPBA_MASTER_A);
	return 0;
}

arch_initcall(mxc_init_devices);
