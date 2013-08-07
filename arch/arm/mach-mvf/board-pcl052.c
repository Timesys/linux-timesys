/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/nodemask.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/smsc911x.h>
#include <linux/spi/spi.h>
#if defined(CONFIG_MTD_M25P80) || defined(CONFIG_MTD_M25P80_MODULE)
#include <linux/spi/flash.h>
#else
#include <linux/mtd/physmap.h>
#endif
#include <linux/i2c.h>
#include <linux/i2c/at24.h>
#include <linux/i2c/pca953x.h>
#include <linux/mfd/stmpe.h>
#include <linux/ata.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/pmic_external.h>
#include <linux/pmic_status.h>
#include <linux/ipu.h>
#include <linux/mxcfb.h>
#include <linux/phy.h> // SP: added for reference to phy_ functions, ex. phy_register_fixup
#include <linux/pwm_backlight.h>
#include <linux/leds_pwm.h>
#include <linux/fec.h>
#include <linux/memblock.h>
#include <linux/gpio.h>
#include <linux/etherdevice.h>
#include <linux/regulator/anatop-regulator.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <sound/pcm.h>

#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/mxc_dvfs.h>
#include <mach/memory.h>
#include <mach/iomux-mvf.h>
#include <mach/imx-uart.h>
#include <mach/spi-mvf.h>
#include <mach/viv_gpu.h>
#include <mach/ahci_sata.h>
#include <mach/ipu-v3.h>
#include <mach/mxc_hdmi.h>
#include <mach/mxc_asrc.h>
#include <mach/mipi_dsi.h>
#include <mach/mipi_csi2.h>
#include <mach/fsl_l2_switch.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>

#include "devices-mvf.h"
#include "usb.h"
#include "crm_regs.h"

#define PCL052_SD1_CD   98	
#define PCL052_TS_IRQ	32

/* FEC1 definitions */
#define FEC1_BUS_ID	"2:01"	// SP: BUS_ID - required to use phy-fixup specifically for the ETH1 PHY

#define KSZ8051_PHY_CTRL2       0x1F     // SP: PHY Control 2 register address
#define KSZ8051_50MHZ_CLK_MODE (1 << 7) // SP: PHY Control 2 register setting for 50 MHz clock mode

#ifdef PCM952_REV0
#undef PCM952_REV0
#endif

static iomux_v3_cfg_t pcl052_pads[] = {

	/*SDHC1*/
	MVF600_PAD14_PTA24__SDHC1_CLK,
	MVF600_PAD15_PTA25__SDHC1_CMD,
	MVF600_PAD16_PTA26__SDHC1_DAT0,
	MVF600_PAD17_PTA27__SDHC1_DAT1,
	MVF600_PAD18_PTA28__SDHC1_DAT2,
	MVF600_PAD19_PTA29__SDHC1_DAT3,
	/*set PTB28 as GPIO for sdhc card detecting*/
    // GPIO[98]
	MVF600_PAD98_PTB28__SDHC1_SW_CD,
	/*I2C2*/
	MVF600_PAD12_PTA22__I2C2_SCL,
	MVF600_PAD13_PTA23__I2C2_SDA,

	/*CAN0*/
	MVF600_PAD36_PTB14__CAN0_RX,
	MVF600_PAD37_PTB15__CAN0_TX,

	/*CAN1*/
	MVF600_PAD38_PTB16__CAN1_RX,
	MVF600_PAD39_PTB17__CAN1_TX,

	/*DSPI0*/
	MVF600_PAD41_PTB19__DSPI0_PCS0,
	MVF600_PAD42_PTB20__DSPI0_SIN,
	MVF600_PAD43_PTB21__DSPI0_SOUT,
	MVF600_PAD44_PTB22__DSPI0_SCK,

	/*FEC0 not supported*/
#if defined(CONFIG_FEC1) || defined(CONFIG_FSL_L2_SWITCH)
	/*FEC1*/
	MVF600_PAD54_PTC9__RMII1_MDC,
	MVF600_PAD55_PTC10__RMII1_MDIO,
	MVF600_PAD56_PTC11__RMII1_CRS_DV,
	MVF600_PAD57_PTC12__RMII1_RXD1,
	MVF600_PAD58_PTC13__RMII1_RXD0,
	MVF600_PAD59_PTC14__RMII1_RXER,
	MVF600_PAD60_PTC15__RMII1_TXD1,
	MVF600_PAD61_PTC16__RMII1_TXD0,
	MVF600_PAD62_PTC17__RMII1_TXEN,
#endif
    /*TDA19988x HDMI Encoder*/
    /***Video*/
	MVF600_PAD108_PTE3_LCD_ENABLE,
	MVF600_PAD105_PTE0_DCU0_HSYNC,
	MVF600_PAD106_PTE1_DCU0_VSYNC,
	MVF600_PAD107_PTE2_DCU0_PCLK,
	MVF600_PAD109_PTE4_DCU0_DE,
	MVF600_PAD110_PTE5_DCU0_R0,
	MVF600_PAD111_PTE6_DCU0_R1,
	MVF600_PAD112_PTE7_DCU0_R2,
	MVF600_PAD113_PTE8_DCU0_R3,
	MVF600_PAD114_PTE9_DCU0_R4,
	MVF600_PAD115_PTE10_DCU0_R5,
	MVF600_PAD116_PTE11_DCU0_R6,
	MVF600_PAD117_PTE12_DCU0_R7,
	MVF600_PAD118_PTE13_DCU0_G0,
	MVF600_PAD119_PTE14_DCU0_G1,
	MVF600_PAD120_PTE15_DCU0_G2,
	MVF600_PAD121_PTE16_DCU0_G3,
	MVF600_PAD122_PTE17_DCU0_G4,
	MVF600_PAD123_PTE18_DCU0_G5,
	MVF600_PAD124_PTE19_DCU0_G6,
	MVF600_PAD125_PTE20_DCU0_G7,
	MVF600_PAD126_PTE21_DCU0_B0,
	MVF600_PAD127_PTE22_DCU0_B1,
	MVF600_PAD128_PTE23_DCU0_B2,
	MVF600_PAD129_PTE24_DCU0_B3,
	MVF600_PAD130_PTE25_DCU0_B4,
	MVF600_PAD131_PTE26_DCU0_B5,
	MVF600_PAD132_PTE27_DCU0_B6,
	MVF600_PAD133_PTE28_DCU0_B7,
    /* MVF600_PAD53_PTC8__GPIO_TDA_INT*/
    IOMUX_PAD(0x00D4, 0x00D4, IOMUX_CONFIG_ALT0, 0x0000, 0,
			  PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_SPEED_MED 
              | PAD_CTL_PUS_47K_UP | PAD_CTL_DSE_25ohm 
              | PAD_CTL_ODE | PAD_CTL_IBE_ENABLE),

    /***Audio*/
    /* BCLK ACLK*/
	MVF600_PAD6_PTA16_SAI2_TX_BCLK,
    /* SYNC WS/AP0 */
	MVF600_PAD9_PTA19_SAI2_TX_SYNC,
    /* DATA AXR0/AP1 */
	MVF600_PAD8_PTA18_SAI2_TX_DATA,
    /* PTB10 MCLK OSC_IN/AP3 CKIO1 */
    IOMUX_PAD(0x0080, 0x0080, 6, 0x0000, 0, \
              MVF600_SAI_PAD_CTRL | PAD_CTL_IBE_ENABLE),

	/*UART1*/
	MVF600_PAD26_PTB4_UART1_TX,
	MVF600_PAD27_PTB5_UART1_RX,

	/*USB0/1 VBUS_EN*/
#ifdef PCM952_REV0
	MVF600_PAD6_PTA16__USB0_VBUS_EN,
#else
	MVF600_PAD134_PTA7__USB_VBUS_EN,
#endif
	MVF600_PAD7_PTA17__USB_OC_N,

	/* Quad SPI */
	MVF600_PAD79_PTD0_QSPI0_A_SCK,
	MVF600_PAD80_PTD1_QSPI0_A_CS0,
	MVF600_PAD81_PTD2_QSPI0_A_D3,
	MVF600_PAD82_PTD3_QSPI0_A_D2,
	MVF600_PAD83_PTD4_QSPI0_A_D1,
	MVF600_PAD84_PTD5_QSPI0_A_D0,
	MVF600_PAD86_PTD7_QSPI0_B_SCK,
	MVF600_PAD87_PTD8_QSPI0_B_CS0,
	MVF600_PAD88_PTD9_QSPI0_B_D3,
	MVF600_PAD89_PTD10_QSPI0_B_D2,
	MVF600_PAD90_PTD11_QSPI0_B_D1,
	MVF600_PAD91_PTD12_QSPI0_B_D0,
};

/* TODO: HDMI Audio Initialization */
static struct mxc_audio_platform_data pcl052_audio_data;

static int pcl052_hdmiA_init(void)
{
	pcl052_audio_data.sysclk = 12288000;
	return 0;
}

static struct mvf_sai_platform_data mvf_sai_pdata = {
	.flags = MVF_SAI_DMA | MVF_SAI_TRA_SYN | MVF_SAI_USE_I2S_SLAVE,
};

static struct mxc_audio_platform_data pcl052_audio_data = {
	.sai_num = 1,
	.src_port = 2,
	.ext_port = 2,
	.init = pcl052_hdmiA_init,
};

static struct platform_device pcl052_audio_device = {
	.name = "mvf-hdmiaudiocodec",
};

static struct imxuart_platform_data mvf_uart1_pdata = {
	.flags = IMXUART_FIFO | IMXUART_EDMA,
	.dma_req_rx = DMA_MUX03_UART1_RX,
	.dma_req_tx = DMA_MUX03_UART1_TX,
};

static inline void pcl052_init_uart(void)
{
	mvf_add_imx_uart(1, &mvf_uart1_pdata);
}

static struct fec_platform_data fec_data __initdata = {
	.phy = PHY_INTERFACE_MODE_RMII,
};

static struct switch_platform_data switch_data __initdata = {
	.phy = PHY_INTERFACE_MODE_RMII,
};

/* SP: Added function - mvf_fec1_phy_fixup
 * Additional configuration is required to setup ETH1 on the phyCORE-Vybrid
 * (1) Set to 50 MHz (enable bit 7 in the PHY control 2 register)
        - Write: reg - 0x1F;  data - (0x1 << 7)
*/

static int mvf_fec1_phy_fixup(struct phy_device *phydev)
{
        int regval;
        regval = phy_read(phydev, KSZ8051_PHY_CTRL2); // read current value of PHY Control 2 register
        regval |= KSZ8051_50MHZ_CLK_MODE; // enable 50MHz clock mode
        phy_write(phydev, KSZ8051_PHY_CTRL2, regval); // write to PHY Control 2 register

        return 0;
}

static int pcl052_spi_cs[] = {
	41,
};

static const struct spi_mvf_master pcl052_spi_data __initconst = {
	.bus_num = 0,
	.chipselect = pcl052_spi_cs,
	.num_chipselect = ARRAY_SIZE(pcl052_spi_cs),
	.cs_control = NULL,
};

static int pcl052_qspi_cs[] = {
	80,
	87,
};

static const struct spi_mvf_master pcl052_qspi_data __initconst = {
	.bus_num = 0,
	.chipselect = pcl052_qspi_cs,
	.num_chipselect = ARRAY_SIZE(pcl052_qspi_cs),
	.cs_control = NULL,
};

#if defined(CONFIG_MTD_M25P80) || defined(CONFIG_MTD_M25P80_MODULE)
static struct mtd_partition at26df081a_partitions[] = {
	{
		.name = "at26df081a",
		.size = (1024 * 64 * 16),
		.offset = 0x00000000,
		.mask_flags = 0,
	}
};

static struct flash_platform_data at26df081a_platform_data = {
	.name = "Atmel at26df081a SPI Flash chip",
	.parts = at26df081a_partitions,
	.nr_parts = ARRAY_SIZE(at26df081a_partitions),
	.type = "at26df081a",
};

static struct spi_mvf_chip at26df081a_chip_info = {
	.mode = SPI_MODE_3,
	.bits_per_word = 8,
	.void_write_data = 0,
	.dbr = 0,
	.pbr = 0,
	.br = 0,
	.pcssck = 0,
	.pasc = 0,
	.pdt = 0,
	.cssck = 0,
	.asc = 0,
	.dt = 0,
};

static struct mtd_partition s25fl256s_partitions[] = {
	{
		.name = "s25fl256s",
		.size = (1024 * 64 * 256),
		.offset = 0x00000000,
		.mask_flags = 0,
	}
};

static struct flash_platform_data s25fl256s_spi_flash_data = {
	.name = "Spansion s25fl128s SPI Flash chip",
	.parts = s25fl256s_partitions,
	.nr_parts = ARRAY_SIZE(s25fl256s_partitions),
	.type = "s25fl128s",
};
#endif

static struct spi_board_info mvf_spi_board_info[] __initdata = {
#if defined(CONFIG_MTD_M25P80)
#if defined(CONFIG_SPI_MVF_QSPI)
	{
		/* The modalias must be the same as spi device driver name */
		.modalias = "m25p80",
		.max_speed_hz = 20000000,
		.bus_num = 0,
		.chip_select = 0,
		.platform_data = &s25fl256s_spi_flash_data,
	},
#endif
#if defined(CONFIG_SPI_MVF)
	{
		/* The modalias must be the same as spi device driver name */
		.modalias = "m25p80",
		.max_speed_hz = 16000000,
		.bus_num = 0,
		.chip_select = 0,
		.platform_data = &at26df081a_platform_data,
		.controller_data = &at26df081a_chip_info
	},
#endif
#endif
};

static void spi_device_init(void)
{
	spi_register_board_info(mvf_spi_board_info,
				ARRAY_SIZE(mvf_spi_board_info));
}

#if 1
static void pcl052_suspend_enter(void)
{
	/* suspend preparation */
}

static void pcl052_suspend_exit(void)
{
	/* resmue resore */
}
static const struct pm_platform_data pcl052_pm_data __initconst = {
	.name = "mvf_pm",
	.suspend_enter = pcl052_suspend_enter,
	.suspend_exit = pcl052_suspend_exit,
};
#endif

static struct mvf_dcu_platform_data mvf_dcu_pdata = {
	.mode_str	= "640x480p_hdmipc", /* works with HDMI PC, not HDMI TV */
   // .mode_str	= "720x480p",
	.default_bpp	= 24,
};

static void __init fixup_mxc_board(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
}
/*
 * Not defined the cd/wp so far, set it always present for debug*/
static const struct esdhc_platform_data pcl052_sd1_data __initconst = {
	.cd_gpio = PCL052_SD1_CD,
	.wp_gpio = -1,
};

/* SP: Replicate the AM335x solution for phy_fixup
 * 	- Call a phy_register_fixup to make the required changes for FEC1/ETH1 - see 'mvf_fec1_phy_fixup'
 * 	- 'pcl052_setup' will be assigned  to .setup in EEPROM platform data (board_eeprom)
 */ 
static void pcl052_setup(struct memory_accessor *mem_acc, void *context)
{
    phy_register_fixup_for_id(FEC1_BUS_ID, mvf_fec1_phy_fixup); // SP: run mvf_fec1_phy_fixup to setup ETH1
        return;
}

static struct at24_platform_data board_eeprom = {
	.byte_len = 4096,
	.page_size = 32,
	.flags = AT24_FLAG_ADDR16,
    .setup = pcl052_setup, // SP: added to replicate the am335x board file
	.context = (void *)NULL, // SP: added to replicate the am335x board file
};

static struct imxi2c_platform_data pcl052_i2c_data = {
	.bitrate = 100000,
};

static struct i2c_board_info pcl052_i2c2_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("tda998x", 0x70),
	},
	{
		I2C_BOARD_INFO("tda99Xcec", 0x34),
	},
};

static struct mxc_nand_platform_data mvf_data __initdata = {
	.width = 2,
};

static struct led_pwm mvf_led __initdata = {
	.name = "mvf_leds",
	.pwm_id = 1,
	.active_low = 0,
	.max_brightness = 6,
	.pwm_period_ns = 100000000,
};

static struct led_pwm_platform_data mvf_led_data __initdata = {
	.num_leds = 1,
	.leds = &mvf_led,
};

#define USB_VBUS_ENABLE_PIN	134
static void __init pcl052_init_usb(void)
{
	gpio_request_one(USB_VBUS_ENABLE_PIN, GPIOF_OUT_INIT_LOW, "VBUS_EN");
	msleep(2);
	gpio_set_value(USB_VBUS_ENABLE_PIN, 1);

#ifdef CONFIG_USB_EHCI_ARC
	mvf_usb_dr_init();
	mvf_usb_dr2_init();
#endif
}

/*!
 * Board specific initialization.
 */
static void __init pcl052_board_init(void)
{
	mxc_iomux_v3_setup_multiple_pads(pcl052_pads,
					ARRAY_SIZE(pcl052_pads));
	pcl052_init_uart();

#ifdef CONFIG_FEC
	mvf_init_fec(fec_data);
#endif

#ifdef CONFIG_FSL_L2_SWITCH
	mvf_init_switch(switch_data);
#endif

	mvf_add_snvs_rtc();

	mvf_add_sdhci_esdhc_imx(1, &pcl052_sd1_data);

	mvf_add_imx_i2c(2, &pcl052_i2c_data);
	i2c_register_board_info(2, pcl052_i2c2_board_info,
			ARRAY_SIZE(pcl052_i2c2_board_info));

    
	mvf_add_dspi(0, &pcl052_spi_data);
	mvf_add_qspi(0, &pcl052_qspi_data);
	spi_device_init();

	mvfa5_add_dcu(0, &mvf_dcu_pdata);

	mxc_register_device(&pcl052_audio_device, &pcl052_audio_data);
	mvfa5_add_sai(2, &mvf_sai_pdata);

//	mvf_add_wdt(0);

	pcl052_init_usb();

	mvf_add_nand(&mvf_data);

//      mvf_add_mxc_pwm(0);
//      mvf_add_pwm_leds(&mvf_led_data);

}

static void __init mvf_timer_init(void)
{
#if 0
	struct clk *uart_clk;
	uart_clk = clk_get_sys("mvf-uart.0", NULL);
	early_console_setup(MVF_UART1_BASE_ADDR, uart_clk);
#endif
	mvf_clocks_init(32768, 24000000, 0, 0);
}

static struct sys_timer pcl052_timer = {
	.init   = mvf_timer_init,
};

/*
 * initialize __mach_desc_ data structure.
 */
MACHINE_START(PCL052, "PHYTEC Cosmic Board")
	/* Maintainer: Freescale Semiconductor, Inc. */
	.boot_params = MVF_PHYS_OFFSET + 0x100,
	.fixup = fixup_mxc_board,
	.map_io = mvf_map_io,
	.init_irq = mvf_init_irq,
	.init_machine = pcl052_board_init,
	.timer = &pcl052_timer,
MACHINE_END
