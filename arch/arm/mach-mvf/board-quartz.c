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
#include <linux/i2c/pca953x.h>
#include <linux/ata.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/pmic_external.h>
#include <linux/pmic_status.h>
#include <linux/ipu.h>
#include <linux/mxcfb.h>
#include <linux/pwm_backlight.h>
#include <linux/leds.h>
#include <linux/fec.h>
#include <linux/memblock.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
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
#include <mach/mxc.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>

#include "devices-mvf.h"
#include "usb.h"
#include "crm_regs.h"

#define MVF600_SD1_CD  126
#define MVF600_SD1_WP  98
#define MVF600_LVDS_TS	119
#define MVF600_RTC_INT	127
#define MVF600_CRTOUCH_INT 68
/* GPIO definitions */
#define GPIO_LED_1		65
#define GPIO_LED_2		64
#define GPIO_BTN_1		70
#define GPIO_BTN_2		69

static iomux_v3_cfg_t mvf600_pads[] = {

	/*SDHC1*/
	MVF600_PAD14_PTA24__SDHC1_CLK,
	MVF600_PAD15_PTA25__SDHC1_CMD,
	MVF600_PAD16_PTA26__SDHC1_DAT0,
	MVF600_PAD17_PTA27__SDHC1_DAT1,
	MVF600_PAD18_PTA28__SDHC1_DAT2,
	MVF600_PAD19_PTA29__SDHC1_DAT3,
	MVF600_PAD98_PTB28__SDHC1_WP,
	MVF600_PAD126_PTE21__SDHC1_CD,

	/*I2C3*/
	MVF600_PAD20_PTA30__I2C3_SCL,
	MVF600_PAD21_PTA31__I2C3_SDA,

	/*I2C2*/
	MVF600_PAD66_PTD28__I2C2_SCL,
	MVF600_PAD67_PTD27__I2C2_SDA,

#if 0
	/*CAN0*/
	MVF600_PAD36_PTB14__CAN0_RX,
	MVF600_PAD37_PTB15__CAN0_TX,
	MVF600_PAD40_PTB18__CAN0_MODE,

	/*CAN1*/
	MVF600_PAD38_PTB16__CAN1_RX,
	MVF600_PAD39_PTB17__CAN1_TX,
	MVF600_PAD41_PTB19__CAN1_MODE,
#endif

	/*DGIO*/
	MVF600_PAD42_PTB20__DGIO_IN1,
	MVF600_PAD43_PTB21__DGIO_IN2,
	MVF600_PAD44_PTB22__DGIO_OUT1,
	MVF600_PAD93_PTB23__DGIO_OUT2,

	/*FEC0*/
	MVF600_PAD0_PTA6__RMII_CLKIN,
	MVF600_PAD45_PTC0__RMII0_MDC,
	MVF600_PAD46_PTC1__RMII0_MDIO,
	MVF600_PAD47_PTC2__RMII0_CRS_DV,
	MVF600_PAD48_PTC3__RMII0_RXD1,
	MVF600_PAD49_PTC4__RMII0_RXD0,
	MVF600_PAD50_PTC5__RMII0_RXER,
	MVF600_PAD51_PTC6__RMII0_TXD1,
	MVF600_PAD52_PTC7__RMII0_TXD0,
	MVF600_PAD53_PTC8__RMII0_TXEN,

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

	/*SAI2*/
	MVF600_PAD6_PTA16_SAI2_TX_BCLK,
	MVF600_PAD8_PTA18_SAI2_TX_DATA,
	MVF600_PAD9_PTA19_SAI2_TX_SYNC,
	MVF600_PAD12_PTA22_SAI2_RX_DATA,

	/*DCU0*/
	MVF600_PAD25_PTB3_LCD_CONTRAST,
	MVF600_PAD105_PTE0_DCU0_HSYNC,
	MVF600_PAD106_PTE1_DCU0_VSYNC,
	MVF600_PAD107_PTE2_DCU0_PCLK,
	MVF600_PAD109_PTE4_DCU0_DE,
	MVF600_PAD112_PTE7_DCU0_R2,
	MVF600_PAD113_PTE8_DCU0_R3,
	MVF600_PAD114_PTE9_DCU0_R4,
	MVF600_PAD115_PTE10_DCU0_R5,
	MVF600_PAD116_PTE11_DCU0_R6,
	MVF600_PAD117_PTE12_DCU0_R7,
	MVF600_PAD120_PTE15_DCU0_G2,
	MVF600_PAD121_PTE16_DCU0_G3,
	MVF600_PAD122_PTE17_DCU0_G4,
	MVF600_PAD123_PTE18_DCU0_G5,
	MVF600_PAD124_PTE19_DCU0_G6,
	MVF600_PAD125_PTE20_DCU0_G7,
	MVF600_PAD128_PTE23_DCU0_B2,
	MVF600_PAD129_PTE24_DCU0_B3,
	MVF600_PAD130_PTE25_DCU0_B4,
	MVF600_PAD131_PTE26_DCU0_B5,
	MVF600_PAD132_PTE27_DCU0_B6,
	MVF600_PAD133_PTE28_DCU0_B7,
	MVF600_PAD108_PTE3_LVDS_ENABLE,
	MVF600_PAD118_PTE13_LVDS_BLT_EN,

	/*UART3*/
	MVF600_PAD10_PTA20_UART3_TX,
	MVF600_PAD11_PTA21_UART3_RX,

	/*UART1*/
	MVF600_PAD26_PTB4_UART1_TX,
	MVF600_PAD27_PTB5_UART1_RX,

	/*UART0*/
	MVF600_PAD32_PTB10_UART0_TX,
	MVF600_PAD33_PTB11_UART0_RX,
	MVF600_PAD34_PTB12_UART0_RTS,
	MVF600_PAD35_PTB13_UART0_CTS,

	/*User LED1-2*/
	MVF600_PAD65_PTD29_FTM3CH2,
	MVF600_PAD64_PTD30_FTM3CH1,

	/* Touch Screen */
	MVF600_PAD68_PTD26_TS_IRQ,
	MVF600_PAD119_PTE14_LVDS_TS_IRQ,

	/*User button support*/
	MVF600_PAD70_PTD24_USER_BTN1,
	MVF600_PAD69_PTD25_USER_BTN2,

	/* RTC */
	MVF600_PAD127_PTE22_RTC_INT,

};

static struct mxc_audio_platform_data mvf_twr_audio_data;

static int mvf_twr_sgtl5000_init(void)
{
	mvf_twr_audio_data.sysclk = 12288000;
	return 0;
}

static struct mvf_sai_platform_data mvf_sai_pdata = {
	.flags = MVF_SAI_DMA | MVF_SAI_TRA_SYN | MVF_SAI_USE_I2S_SLAVE,
};

static struct mxc_audio_platform_data mvf_twr_audio_data = {
	.sai_num = 1,
	.src_port = 2,
	.ext_port = 2,
	.init = mvf_twr_sgtl5000_init,
};

static struct platform_device mvf_twr_audio_device = {
	.name = "mvf-sgtl5000",
};

static struct imxuart_platform_data mvf_uart1_pdata = {
	.flags = IMXUART_FIFO | IMXUART_EDMA,
	.dma_req_rx = DMA_MUX03_UART1_RX,
	.dma_req_tx = DMA_MUX03_UART1_TX,
};

static struct imxuart_platform_data mvf_uart0_pdata = {
	.flags = IMXUART_FIFO | IMXUART_EDMA,
	.dma_req_rx = DMA_MUX03_UART0_RX,
	.dma_req_tx = DMA_MUX03_UART0_TX,
};

static inline void mvf_vf700_init_uart(void)
{
	mvf_add_imx_uart(0, &mvf_uart0_pdata);
	mvf_add_imx_uart(1, &mvf_uart1_pdata);
}

static struct fec_platform_data fec_data __initdata = {
	.phy = PHY_INTERFACE_MODE_RMII,
};

static struct switch_platform_data switch_data __initdata = {
	.phy = PHY_INTERFACE_MODE_RMII,
};

static void vf600_suspend_enter(void)
{
	/* suspend preparation */
}

static void vf600_suspend_exit(void)
{
	/* resmue resore */
}

static const struct pm_platform_data mvf_vf600_pm_data __initconst = {
	.name = "mvf_pm",
	.suspend_enter = vf600_suspend_enter,
	.suspend_exit = vf600_suspend_exit,
};

static struct mvf_dcu_platform_data mvf_dcu_pdata = {
	.mode_str	= "800x480",
	.default_bpp	= 24,
};

static void __init fixup_mxc_board(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
	if (!mi->nr_banks)
		arm_add_memory(PHYS_OFFSET, SZ_256M);
}

static const struct esdhc_platform_data mvfa5_sd1_data __initconst = {
	.cd_gpio = MVF600_SD1_CD,
	.wp_gpio = MVF600_SD1_WP,
};

static struct crtouch_platform_data crtouch_pdata = {
	.irq_gpio = MVF600_CRTOUCH_INT,
};

static struct imxi2c_platform_data mvf600_i2c_data = {
	.bitrate = 100000,
};

static struct i2c_board_info mxc_i2c2_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("crtouch_ts", 0x49),
		.platform_data = &crtouch_pdata,		
	},
	{
		I2C_BOARD_INFO("sgtl5000", 0x0a),
	},
};

static struct i2c_board_info mxc_i2c3_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("egalax_ts", 0x4),
		.irq = gpio_to_irq(MVF600_LVDS_TS),
	},
	{
		I2C_BOARD_INFO("m41t83", 0x68),
		.irq = gpio_to_irq(MVF600_RTC_INT),
	},
};

static struct mxc_nand_platform_data mvf_data __initdata = {
	.width = 1,
};

static struct gpio_keys_button mvf_gpio_btn[] = {
	{
		.desc	= "button1",
		.gpio	=  GPIO_BTN_1,
	},
	{
		.desc	= "button2",
		.gpio	= GPIO_BTN_2,
	},
};

static struct gpio_led mvf_led[] = {
	{
		.name   = "led1",
		.gpio   =  GPIO_LED_1,
		.default_trigger = "gpio",
	},
	{
		.name   = "led2",
		.gpio   =  GPIO_LED_2,
		.default_trigger = "gpio",
	},
};

static struct gpio_led_platform_data mvf_led_data = {
	.leds   	= mvf_led,
	.num_leds	= ARRAY_SIZE(mvf_led),
};

static struct gpio_keys_platform_data mvf_gpio_btn_data = {
	.buttons        = mvf_gpio_btn,
	.nbuttons       = ARRAY_SIZE(mvf_gpio_btn),
};

static struct imx_asrc_platform_data imx_asrc_data = {
	.channel_bits = 4,
	.clk_map_ver = 3,
};

static void __init mvf_twr_init_usb(void)
{
	imx_otg_base = MVF_IO_ADDRESS(MVF_USBC0_BASE_ADDR);
	/*mvf_set_otghost_vbus_func(mvf_twr_usbotg_vbus);*/
#ifdef CONFIG_USB_EHCI_ARC
	mvf_usb_dr2_init();
#endif
#ifdef CONFIG_USB_GADGET_ARC
	mvf_usb_dr_init();
#endif
}

static void __init mvf_init_adc(void)
{
	mvf_add_adc(0);
}

/*!
 * Board specific initialization.
 */
static void __init mvf_board_init(void)
{
	mxc_iomux_v3_setup_multiple_pads(mvf600_pads,
					ARRAY_SIZE(mvf600_pads));
	mvf_vf700_init_uart();

#ifdef CONFIG_FEC
	mvf_init_fec(fec_data);
#endif

#ifdef CONFIG_FSL_L2_SWITCH
	mvf_init_switch(switch_data);
#endif

	mvf_add_snvs_rtc();

	mvf_init_adc();

	mvf_add_pm_imx(0, &mvf_vf600_pm_data);

	mvf700_add_caam();

	mvf_add_sdhci_esdhc_imx(1, &mvfa5_sd1_data);

#if 1
	/*FIXME: I2C driver needs a fix for Linux-MQX semaphore
	 * conflict
	 */
	mvf_add_imx_i2c(2, &mvf600_i2c_data);
	i2c_register_board_info(2, mxc_i2c2_board_info,
				ARRAY_SIZE(mxc_i2c2_board_info));
#endif

#if 0
	mvf_add_imx_i2c(3, &mvf600_i2c_data);
	i2c_register_board_info(3, mxc_i2c3_board_info,
				ARRAY_SIZE(mxc_i2c3_board_info));
#endif

	mvfa5_add_dcu(0, &mvf_dcu_pdata);

	mxc_register_device(&mvf_twr_audio_device, &mvf_twr_audio_data);
	mvfa5_add_sai(2, &mvf_sai_pdata);

	mvf_add_wdt(0);

	mvf_twr_init_usb();

	mvf_add_nand(&mvf_data);

	imx_add_platform_device("leds-gpio", -1, NULL,
                0, &mvf_led_data, sizeof(mvf_led_data));
	imx_add_platform_device("gpio-keys", -1, NULL,
		0, &mvf_gpio_btn_data, sizeof(mvf_gpio_btn_data));

	imx_asrc_data.asrc_core_clk = clk_get(NULL, "asrc_clk");
	imx_asrc_data.asrc_audio_clk = clk_get(NULL, "asrc_serial_clk");
	mvf_add_asrc(&imx_asrc_data);
}

static void __init mvf_timer_init(void)
{
	mvf_clocks_init(32768, 24000000, 0, 0);
	{
	struct clk *uart_clk;
	uart_clk = clk_get_sys("mvf-uart.0", NULL);
	early_console_setup(MVF_UART0_BASE_ADDR, uart_clk);
	}
}

static struct sys_timer mxc_timer = {
	.init   = mvf_timer_init,
};

/*
 * initialize __mach_desc_ data structure.
 */
MACHINE_START(MVFA5_TWR_VF700, "Device Solutions QDK Board")
	.boot_params = MVF_PHYS_OFFSET + 0x100,
	.fixup = fixup_mxc_board,
	.map_io = mvf_map_io,
	.init_irq = mvf_init_irq,
	.init_machine = mvf_board_init,
	.timer = &mxc_timer,
MACHINE_END
