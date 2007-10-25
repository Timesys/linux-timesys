/*
 * Copyright 2004-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#ifndef __ASM_ARCH_MXC_H__
#define __ASM_ARCH_MXC_H__

#ifndef __ASM_ARCH_MXC_HARDWARE_H__
#error "Do not include directly."
#endif

#ifndef __ASSEMBLY__
#include <linux/types.h>

/*!
 * @ingroup MSL_MX27 MSL_MX31
 */
/*!
 * gpio port structure
 */
struct mxc_gpio_port {
	u32 num;		/*!< gpio port number */
	u32 base;		/*!< gpio port base VA */
#ifdef MXC_GPIO_SPLIT_IRQ_2
	u16 irq_0_15, irq_16_31;
#else
	u16 irq;		/*!< irq number to the core */
#endif
	u16 virtual_irq_start;	/*!< virtual irq start number */
};
/*!
 * This structure is used to define the One wire platform data.
 * It includes search rom accelerator.
 */

struct mxc_w1_config {
	int search_rom_accelerator;
};
/*!
 * This structure is used to define the SPI master controller's platform
 * data. It includes the SPI  bus number and the maximum number of
 * slaves/chips it supports.
 */
struct mxc_spi_master {
	/*!
	 * SPI Master's bus number.
	 */
	unsigned int bus_num;
	/*!
	 * SPI Master's maximum number of chip selects.
	 */
	unsigned int maxchipselect;
	/*!
	 * CSPI Hardware Version.
	 */
	unsigned int spi_version;
};

struct mxc_ipu_config {
	int rev;
};

struct mxc_ir_platform_data {
	int uart_ir_mux;
	int ir_rx_invert;
	int ir_tx_invert;
	struct clk *uart_clk;
};

struct mxc_i2c_platform_data {
	u32 i2c_clk;
};

extern void mxc_wd_reset(void);
extern void mxc_kick_wd(void);
unsigned long board_get_ckih_rate(void);

int mxc_snoop_set_config(u32 num, unsigned long base, int size);
int mxc_snoop_get_status(u32 num, u32 * statl, u32 * stath);

#endif				/* __ASSEMBLY__ */

#define IOMUX_TO_GPIO(pin) 	((((unsigned int)pin >> MUX_IO_P) * GPIO_NUM_PIN) + ((pin >> MUX_IO_I) & ((1 << (MUX_IO_P - MUX_IO_I)) -1)))
#define IOMUX_TO_IRQ(pin)	(MXC_GPIO_BASE + IOMUX_TO_GPIO(pin))
#define GPIO_TO_PORT(n)		(n / GPIO_NUM_PIN)
#define GPIO_TO_INDEX(n)	(n % GPIO_NUM_PIN)

#ifdef CONFIG_MXC_TZIC
/*
 *****************************************
 * TZIC Registers                        *
 *****************************************
 */

#define TZIC_BASE               IO_ADDRESS(TZIC_BASE_ADDR)
#define TZIC_INTCNTL            (TZIC_BASE + 0x0000)	/* control register */
#define TZIC_INTTYPE            (TZIC_BASE + 0x0004)	/* Controller type register */
#define TZIC_IMPID              (TZIC_BASE + 0x0008)	/* Distributor Implementer Identification Register */
#define TZIC_PRIOMASK           (TZIC_BASE + 0x000C)	/* Priority Mask Reg */
#define TZIC_SYNCCTRL           (TZIC_BASE + 0x0010)	/* Synchronizer Control register */
#define TZIC_DSMINT             (TZIC_BASE + 0x0014)	/* DSM interrupt Holdoffregister */
#define TZIC_INTSEC0            (TZIC_BASE + 0x0080)	/* interrupt security register 0 */
#define TZIC_ENSET0             (TZIC_BASE + 0x0100)	/* Enable Set Register 0 */
#define TZIC_ENCLEAR0           (TZIC_BASE + 0x0180)	/* Enable Clear Register 0 */
#define TZIC_SRCSET0            (TZIC_BASE + 0x0200)	/* Source Set Register 0 */
#define TZIC_SRCCLAR0           (TZIC_BASE + 0x0280)	/* Source Clear Register 0 */
#define TZIC_PRIORITY0          (TZIC_BASE + 0x0400)	/* Priority Register 0 */
#define TZIC_PND0               (TZIC_BASE + 0x0D00)	/* Pending Register 0 */
#define TZIC_HIPND0             (TZIC_BASE + 0x0D80)	/* High Priority Pending Register */
#define TZIC_WAKEUP0            (TZIC_BASE + 0x0E00)	/* Wakeup Config Register */
#define TZIC_SWINT              (TZIC_BASE + 0x0F00)	/* Software Interrupt Rigger Register */
#define TZIC_ID0                (TZIC_BASE + 0x0FD0)	/* Indentification Register 0 */

#else
/*
 *****************************************
 * AVIC Registers                        *
 *****************************************
 */
#define AVIC_BASE		IO_ADDRESS(AVIC_BASE_ADDR)
#define AVIC_INTCNTL		(AVIC_BASE + 0x00)	/* int control reg */
#define AVIC_NIMASK		(AVIC_BASE + 0x04)	/* int mask reg */
#define AVIC_INTENNUM		(AVIC_BASE + 0x08)	/* int enable number reg */
#define AVIC_INTDISNUM		(AVIC_BASE + 0x0C)	/* int disable number reg */
#define AVIC_INTENABLEH		(AVIC_BASE + 0x10)	/* int enable reg high */
#define AVIC_INTENABLEL		(AVIC_BASE + 0x14)	/* int enable reg low */
#define AVIC_INTTYPEH		(AVIC_BASE + 0x18)	/* int type reg high */
#define AVIC_INTTYPEL		(AVIC_BASE + 0x1C)	/* int type reg low */
#define AVIC_NIPRIORITY7	(AVIC_BASE + 0x20)	/* norm int priority lvl7 */
#define AVIC_NIPRIORITY6	(AVIC_BASE + 0x24)	/* norm int priority lvl6 */
#define AVIC_NIPRIORITY5	(AVIC_BASE + 0x28)	/* norm int priority lvl5 */
#define AVIC_NIPRIORITY4	(AVIC_BASE + 0x2C)	/* norm int priority lvl4 */
#define AVIC_NIPRIORITY3	(AVIC_BASE + 0x30)	/* norm int priority lvl3 */
#define AVIC_NIPRIORITY2	(AVIC_BASE + 0x34)	/* norm int priority lvl2 */
#define AVIC_NIPRIORITY1	(AVIC_BASE + 0x38)	/* norm int priority lvl1 */
#define AVIC_NIPRIORITY0	(AVIC_BASE + 0x3C)	/* norm int priority lvl0 */
#define AVIC_NIVECSR		(AVIC_BASE + 0x40)	/* norm int vector/status */
#define AVIC_FIVECSR		(AVIC_BASE + 0x44)	/* fast int vector/status */
#define AVIC_INTSRCH		(AVIC_BASE + 0x48)	/* int source reg high */
#define AVIC_INTSRCL		(AVIC_BASE + 0x4C)	/* int source reg low */
#define AVIC_INTFRCH		(AVIC_BASE + 0x50)	/* int force reg high */
#define AVIC_INTFRCL		(AVIC_BASE + 0x54)	/* int force reg low */
#define AVIC_NIPNDH		(AVIC_BASE + 0x58)	/* norm int pending high */
#define AVIC_NIPNDL		(AVIC_BASE + 0x5C)	/* norm int pending low */
#define AVIC_FIPNDH		(AVIC_BASE + 0x60)	/* fast int pending high */
#define AVIC_FIPNDL		(AVIC_BASE + 0x64)	/* fast int pending low */
#endif				/* MXC_SUPPORT_TZIC */

/*
 *****************************************
 * EDIO Registers                        *
 *****************************************
 */
#ifdef EDIO_BASE_ADDR
#define EDIO_EPPAR		(IO_ADDRESS(EDIO_BASE_ADDR) + 0x00)
#define EDIO_EPDDR		(IO_ADDRESS(EDIO_BASE_ADDR) + 0x02)
#define EDIO_EPDR		(IO_ADDRESS(EDIO_BASE_ADDR) + 0x04)
#define EDIO_EPFR		(IO_ADDRESS(EDIO_BASE_ADDR) + 0x06)
#endif

/* DMA driver defines */
#define MXC_IDE_DMA_WATERMARK	32	/* DMA watermark level in bytes */
#define MXC_IDE_DMA_BD_NR	(512/3/4)	/* Number of BDs per channel */

#define DPTC_WP_SUPPORTED	17

#ifndef IS_MEM_DEVICE_NONSHARED
/* all peripherals on MXC so far are below 0x80000000 but leave L2CC alone */
#define IS_MEM_DEVICE_NONSHARED(x)  ((x) < 0x80000000 && (x) != L2CC_BASE_ADDR)
#endif

#ifndef __ASSEMBLY__
#include <linux/types.h>
struct dptc_wp {
	u32 dcvr0;
	u32 dcvr1;
	u32 dcvr2;
	u32 dcvr3;
	u32 regulator;
	u32 voltage;
};
struct cpu_wp {
	u32 pll_reg;
	u32 pll_rate;
	u32 cpu_rate;
	u32 pdr0_reg;
};

struct cpu_wp *get_cpu_wp(int *wp);

/*!
 * This function is called to put the DPTC in a low power state.
 *
 */
void dptc_disable(void);

/*!
 * This function is called to resume the DPTC from a low power state.
 *
 */
void dptc_enable(void);

#endif

#endif				/*  __ASM_ARCH_MXC_H__ */
