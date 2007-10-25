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

/*!
 * @file arch-mxc/mx27.h
 * @brief This file contains register definitions.
 *
 * @ingroup MSL_MX27
 */

#ifndef __ASM_ARCH_MXC_MX27_H__
#define __ASM_ARCH_MXC_MX27_H__

#ifndef __ASM_ARCH_MXC_HARDWARE_H__
#error "Do not include directly."
#endif

#include <asm/arch/mx27_pins.h>

/*!
 * defines the OS clock tick rate
 */
#define CLOCK_TICK_RATE         13300000

/*!
 * Register an interrupt handler for the SMN as well as the SCC.  In some
 * implementations, the SMN is not connected at all, and in others, it is
 * on the same interrupt line as the SCM. Comment this line out accordingly
 */
#define USE_SMN_INTERRUPT

/*!
 * UART Chip level Configuration that a user may not have to edit. These
 * configuration vary depending on how the UART module is integrated with
 * the ARM core
 */
#define MXC_UART_NR 6
/*!
 * This option is used to set or clear the RXDMUXSEL bit in control reg 3.
 * Certain platforms need this bit to be set in order to receive Irda data.
 */
#define MXC_UART_IR_RXDMUX      0x0004
/*!
 * This option is used to set or clear the RXDMUXSEL bit in control reg 3.
 * Certain platforms need this bit to be set in order to receive UART data.
 */
#define MXC_UART_RXDMUX         0x0004

/*
 * IRAM
 */
#define IRAM_BASE_ADDR          0xFFFF4C00	/* internal ram */

/*
 *  Register offests.
 */
#define AIPI_BASE_ADDR          0x10000000
#define AIPI_BASE_ADDR_VIRT     0xD4000000
#define AIPI_SIZE               SZ_1M

#define DMA_BASE_ADDR           (AIPI_BASE_ADDR + 0x01000)
#define WDOG1_BASE_ADDR         (AIPI_BASE_ADDR + 0x02000)
#define GPT1_BASE_ADDR          (AIPI_BASE_ADDR + 0x03000)
#define GPT2_BASE_ADDR          (AIPI_BASE_ADDR + 0x04000)
#define GPT3_BASE_ADDR          (AIPI_BASE_ADDR + 0x05000)
#define PWM_BASE_ADDR           (AIPI_BASE_ADDR + 0x06000)
#define RTC_BASE_ADDR           (AIPI_BASE_ADDR + 0x07000)
#define KPP_BASE_ADDR           (AIPI_BASE_ADDR + 0x08000)
#define OWIRE_BASE_ADDR         (AIPI_BASE_ADDR + 0x09000)
#define UART1_BASE_ADDR         (AIPI_BASE_ADDR + 0x0A000)
#define UART2_BASE_ADDR         (AIPI_BASE_ADDR + 0x0B000)
#define UART3_BASE_ADDR         (AIPI_BASE_ADDR + 0x0C000)
#define UART4_BASE_ADDR         (AIPI_BASE_ADDR + 0x0D000)
#define CSPI1_BASE_ADDR         (AIPI_BASE_ADDR + 0x0E000)
#define CSPI2_BASE_ADDR         (AIPI_BASE_ADDR + 0x0F000)
#define SSI1_BASE_ADDR          (AIPI_BASE_ADDR + 0x10000)
#define SSI2_BASE_ADDR          (AIPI_BASE_ADDR + 0x11000)
#define I2C_BASE_ADDR           (AIPI_BASE_ADDR + 0x12000)
#define SDHC1_BASE_ADDR         (AIPI_BASE_ADDR + 0x13000)
#define SDHC2_BASE_ADDR         (AIPI_BASE_ADDR + 0x14000)
#define GPIO_BASE_ADDR          (AIPI_BASE_ADDR + 0x15000)
#define AUDMUX_BASE_ADDR        (AIPI_BASE_ADDR + 0x16000)

#define CSPI3_BASE_ADDR         (AIPI_BASE_ADDR + 0x17000)
#define MSHC_BASE_ADDR          (AIPI_BASE_ADDR + 0x18000)
#define GPT5_BASE_ADDR          (AIPI_BASE_ADDR + 0x19000)
#define GPT4_BASE_ADDR          (AIPI_BASE_ADDR + 0x1A000)
#define UART5_BASE_ADDR         (AIPI_BASE_ADDR + 0x1B000)
#define UART6_BASE_ADDR         (AIPI_BASE_ADDR + 0x1C000)
#define I2C2_BASE_ADDR          (AIPI_BASE_ADDR + 0x1D000)
#define SDHC3_BASE_ADDR         (AIPI_BASE_ADDR + 0x1E000)
#define GPT6_BASE_ADDR          (AIPI_BASE_ADDR + 0x1F000)

#define LCDC_BASE_ADDR          (AIPI_BASE_ADDR + 0x21000)
#define SLCDC_BASE_ADDR         (AIPI_BASE_ADDR + 0x22000)
#define VPU_BASE_ADDR           (AIPI_BASE_ADDR + 0x23000)
#define USBOTG_BASE_ADDR        (AIPI_BASE_ADDR + 0x24000)
/* for mx27*/
#define OTG_BASE_ADDR           USBOTG_BASE_ADDR
#define SAHARA_BASE_ADDR        (AIPI_BASE_ADDR + 0x25000)
#define EMMA_BASE_ADDR          (AIPI_BASE_ADDR + 0x26400)
#define CCM_BASE_ADDR           (AIPI_BASE_ADDR + 0x27000)
#define SYSCTRL_BASE_ADDR       (AIPI_BASE_ADDR + 0x27800)
#define IIM_BASE_ADDR           (AIPI_BASE_ADDR + 0x28000)

#define RTIC_BASE_ADDR          (AIPI_BASE_ADDR + 0x2A000)
#define FEC_BASE_ADDR           (AIPI_BASE_ADDR + 0x2B000)
#define SCC_BASE_ADDR           (AIPI_BASE_ADDR + 0x2C000)
#define ETB_BASE_ADDR           (AIPI_BASE_ADDR + 0x3B000)
#define ETB_RAM_BASE_ADDR       (AIPI_BASE_ADDR + 0x3C000)

#define JAM_BASE_ADDR           (AIPI_BASE_ADDR + 0x3E000)
#define MAX_BASE_ADDR           (AIPI_BASE_ADDR + 0x3F000)

/*
 * ROMP and AVIC
 */
#define ROMP_BASE_ADDR          0x10041000

#define AVIC_BASE_ADDR          0x10040000

#define SAHB1_BASE_ADDR         0x80000000
#define SAHB1_BASE_ADDR_VIRT    0xD4100000
#define SAHB1_SIZE              SZ_1M

#define CSI_BASE_ADDR           (SAHB1_BASE_ADDR + 0x0000)
#define ATA_BASE_ADDR           (SAHB1_BASE_ADDR + 0x1000)

/*
 * NAND, SDRAM, WEIM, M3IF, EMI controllers
 */
#define X_MEMC_BASE_ADDR        0xD8000000
#define X_MEMC_BASE_ADDR_VIRT   0xD4200000
#define X_MEMC_SIZE             SZ_1M

#define NFC_BASE_ADDR           (X_MEMC_BASE_ADDR)
#define SDRAMC_BASE_ADDR        (X_MEMC_BASE_ADDR + 0x1000)
#define WEIM_BASE_ADDR          (X_MEMC_BASE_ADDR + 0x2000)
#define M3IF_BASE_ADDR          (X_MEMC_BASE_ADDR + 0x3000)
#define PCMCIA_CTL_BASE_ADDR    (X_MEMC_BASE_ADDR + 0x4000)

/*
 * Memory regions and CS
 */
#define SDRAM_BASE_ADDR         0xA0000000
#define CSD1_BASE_ADDR          0xB0000000

#define CS0_BASE_ADDR           0xC0000000
#define CS1_BASE_ADDR           0xC8000000
#define CS2_BASE_ADDR           0xD0000000
#define CS3_BASE_ADDR           0xD2000000
#define CS4_BASE_ADDR           0xD4000000
#define CS4_BASE_ADDR_VIRT      0xEB000000
#define CS4_SIZE                SZ_1M
#define CS5_BASE_ADDR           0xD6000000
#define PCMCIA_MEM_BASE_ADDR    0xDC000000

/*!
 * This macro defines the physical to virtual address mapping for all the
 * peripheral modules. It is used by passing in the physical address as x
 * and returning the virtual address. If the physical address is not mapped,
 * it returns 0xDEADBEEF
 */
#define IO_ADDRESS(x)   \
        (((x >= AIPI_BASE_ADDR) && (x < (AIPI_BASE_ADDR + AIPI_SIZE))) ? AIPI_IO_ADDRESS(x):\
        ((x >= SAHB1_BASE_ADDR) && (x < (SAHB1_BASE_ADDR + SAHB1_SIZE))) ? SAHB1_IO_ADDRESS(x):\
        ((x >= CS4_BASE_ADDR) && (x < (CS4_BASE_ADDR + CS4_SIZE))) ? CS4_IO_ADDRESS(x):\
        ((x >= X_MEMC_BASE_ADDR) && (x < (X_MEMC_BASE_ADDR + X_MEMC_SIZE))) ? X_MEMC_IO_ADDRESS(x):\
        0xDEADBEEF)

#define IS_STATIC_MAPPED(x)						\
        ((((x) >= AIPI_BASE_ADDR_VIRT) && ((x) < (AIPI_BASE_ADDR_VIRT + AIPI_SIZE))) || \
	 (((x) >= SAHB1_BASE_ADDR_VIRT) && ((x) < (SAHB1_BASE_ADDR_VIRT + SAHB1_SIZE))) || \
	 (((x) >= CS4_BASE_ADDR_VIRT) && ((x) < (CS4_BASE_ADDR_VIRT + CS4_SIZE))) || \
	 (((x) >= X_MEMC_BASE_ADDR_VIRT) && ((x) < (X_MEMC_BASE_ADDR_VIRT + X_MEMC_SIZE))))

/*
 * define the address mapping macros: in physical address order
 */

#define AIPI_IO_ADDRESS(x)  \
        (((x) - AIPI_BASE_ADDR) + AIPI_BASE_ADDR_VIRT)

#define AVIC_IO_ADDRESS(x)      AIPI_IO_ADDRESS(x)

#define SAHB1_IO_ADDRESS(x)  \
        (((x) - SAHB1_BASE_ADDR) + SAHB1_BASE_ADDR_VIRT)

#define CS4_IO_ADDRESS(x)  \
        (((x) - CS4_BASE_ADDR) + CS4_BASE_ADDR_VIRT)

#define X_MEMC_IO_ADDRESS(x)  \
        (((x) - X_MEMC_BASE_ADDR) + X_MEMC_BASE_ADDR_VIRT)

#define PCMCIA_IO_ADDRESS(x) \
        (((x) - X_MEMC_BASE_ADDR) + X_MEMC_BASE_ADDR_VIRT)

#define IS_MEM_DEVICE_NONSHARED(x)		0

/*
 *  MX27 ADS Interrupt numbers
 */
#define INT_CCM                 63
#define INT_IIM                 62
#define INT_LCDC                61
#define INT_SLCDC               60
#define INT_SAHARA              59
#define INT_SCC_SCM             58
#define INT_SCC_SMN             57
#define INT_USB3                56
#define INT_USB2                55
#define INT_USB1                54
#define INT_VPU                53
#define INT_EMMAPP              52
#define INT_EMMAPRP             51
#define INT_FEC                 50
#define INT_UART5               49
#define INT_UART6               48
#define INT_DMACH15             47
#define INT_DMACH14             46
#define INT_DMACH13             45
#define INT_DMACH12             44
#define INT_DMACH11             43
#define INT_DMACH10             42
#define INT_DMACH9              41
#define INT_DMACH8              40
#define INT_DMACH7              39
#define INT_DMACH6              38
#define INT_DMACH5              37
#define INT_DMACH4              36
#define INT_DMACH3              35
#define INT_DMACH2              34
#define INT_DMACH1              33
#define INT_DMACH0              32
#define INT_CSI                 31
#define INT_ATA                 30
#define INT_NANDFC              29
#define INT_PCMCIA              28
#define INT_WDOG                27
#define INT_GPT1                26
#define INT_GPT2                25
#define INT_GPT3                24
#define INT_GPT                 INT_GPT1
#define INT_PWM                 23
#define INT_RTC                 22
#define INT_KPP                 21
#define INT_UART1               20
#define INT_UART2               19
#define INT_UART3               18
#define INT_UART4               17
#define INT_CSPI1               16
#define INT_CSPI2               15
#define INT_SSI1                14
#define INT_SSI2                13
#define INT_I2C                 12
#define INT_SDHC1               11
#define INT_SDHC2               10
#define INT_SDHC3               9
#define INT_GPIO                8
#define INT_SDHC                7
#define INT_CSPI3               6
#define INT_RTIC                5
#define INT_GPT4                4
#define INT_GPT5                3
#define INT_GPT6                2
#define INT_I2C2                1

#define MXC_MAX_INT_LINES       64
#define MXC_MAX_EXT_LINES       0

#define MXC_MUX_GPIO_INTERRUPTS 1
#define MXC_GPIO_BASE           (MXC_MAX_INT_LINES)

/*!
 * Number of GPIO port as defined in the IC Spec
 */
#define GPIO_PORT_NUM           6
/*!
 * Number of GPIO pins per port
 */
#define GPIO_NUM_PIN            32

#define DMA_REQ_NFC             37
#define DMA_REQ_SDHC3           36
#define DMA_REQ_UART6_RX        35
#define DMA_REQ_UART6_TX        34
#define DMA_REQ_UART5_RX        33
#define DMA_REQ_UART5_TX        32
#define DMA_REQ_CSI_RX          31
#define DMA_REQ_CSI_STAT        30
#define DMA_REQ_ATA_RCV         29
#define DMA_REQ_ATA_TX          28
#define DMA_REQ_UART1_TX        27
#define DMA_REQ_UART1_RX        26
#define DMA_REQ_UART2_TX        25
#define DMA_REQ_UART2_RX        24
#define DMA_REQ_UART3_TX        23
#define DMA_REQ_UART3_RX        22
#define DMA_REQ_UART4_TX        21
#define DMA_REQ_UART4_RX        20
#define DMA_REQ_CSPI1_TX        19
#define DMA_REQ_CSPI1_RX        18
#define DMA_REQ_CSPI2_TX        17
#define DMA_REQ_CSPI2_RX        16
#define DMA_REQ_SSI1_TX1        15
#define DMA_REQ_SSI1_RX1        14
#define DMA_REQ_SSI1_TX0        13
#define DMA_REQ_SSI1_RX0        12
#define DMA_REQ_SSI2_TX1        11
#define DMA_REQ_SSI2_RX1        10
#define DMA_REQ_SSI2_TX0        9
#define DMA_REQ_SSI2_RX0        8
#define DMA_REQ_SDHC1           7
#define DMA_REQ_SDHC2           6
#define DMA_REQ_MSHC            4
#define DMA_REQ_EXT             3
#define DMA_REQ_CSPI3_TX        2
#define DMA_REQ_CSPI3_RX        1

#define MXC_TIMER_GPT1          1
#define MXC_TIMER_GPT2          2
#define MXC_TIMER_GPT3          3
#define MXC_TIMER_GPT4          4
#define MXC_TIMER_GPT5          5
#define MXC_TIMER_GPT6          6

/*!
 * NFMS bit in FMCR register for pagesize of nandflash
 */
#define NFMS (*((volatile u32 *)IO_ADDRESS(SYSCTRL_BASE_ADDR+0x14)))

#define NFMS_BIT 5

/*
 * GPT clock source mask and offset bit definition
 */
#define GPT_CTRL_MASK           0xFFFFFFF1
#define GPT_CTRL_OFFSET	    	1

#endif				/* __ASM_ARCH_MXC_MX27_H__ */
