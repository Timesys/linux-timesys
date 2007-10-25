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
 * @file mxc_nd.h
 *
 * @brief This file contains the NAND Flash Controller register information.
 *
 *
 * @ingroup NAND_MTD
 */

#ifndef __MXC_ND2_H__
#define __MXC_ND2_H__

#include <asm/hardware.h>

#define IS_2K_PAGE_NAND	  (mtd->writesize == NAND_PAGESIZE_2KB)
#define NAND_PAGESIZE_2KB NAND_MAX_PAGESIZE

#ifdef CONFIG_ARCH_MXC_HAS_NFC_V3
/*
 * For V3 NFC registers Definition
 */

/* AXI Bus Mapped */
#define NFC_AXI_BASE_ADDR      		NFC_BASE_ADDR_AXI
#define NFC_FLASH_ADDR_CMD      	(nfc_axi_base + 0xE00)
#define NFC_CONFIG1             	(nfc_axi_base + 0xE04)
#define NFC_ECC_STATUS_RESULT   	(nfc_axi_base + 0xE08)
#define LAUNCH_NFC              	(nfc_axi_base + 0xE0C)

/* IP Bus Mapped */
#define NFC_WRPROT              	(nfc_ip_base + 0x00)
#define NFC_WRPROT_UNLOCK_BLK_ADD0      (nfc_ip_base + 0x04)
#define NFC_WRPROT_UNLOCK_BLK_ADD1      (nfc_ip_base + 0x08)
#define NFC_WRPROT_UNLOCK_BLK_ADD2      (nfc_ip_base + 0x0C)
#define NFC_WRPROT_UNLOCK_BLK_ADD3      (nfc_ip_base + 0x10)
#define NFC_CONFIG2             	(nfc_ip_base + 0x14)
#define NFC_IPC				(nfc_ip_base + 0x18)
#define NFC_AXI_ERR_ADD			(nfc_ip_base + 0x1C)

/*!
 * Addresses for NFC RAM BUFFER Main area 0
 */
#define MAIN_AREA0        		(volatile u16 *)IO_ADDRESS(NFC_BASE_ADDR_AXI + 0x000)
#define MAIN_AREA1        		(volatile u16 *)IO_ADDRESS(NFC_BASE_ADDR_AXI + 0x200)
#define MAIN_AREA2        		(volatile u16 *)IO_ADDRESS(NFC_BASE_ADDR_AXI + 0x400)
#define MAIN_AREA3        		(volatile u16 *)IO_ADDRESS(NFC_BASE_ADDR_AXI + 0x600)

/*!
 * Addresses for NFC SPARE BUFFER Spare area 0
 */
#define SPARE_AREA0       		(volatile u16 *)IO_ADDRESS(NFC_BASE_ADDR_AXI + 0x800)
#define SPARE_AREA1       		(volatile u16 *)IO_ADDRESS(NFC_BASE_ADDR_AXI + 0x810)
#define SPARE_AREA2       		(volatile u16 *)IO_ADDRESS(NFC_BASE_ADDR_AXI + 0x820)
#define SPARE_AREA3       		(volatile u16 *)IO_ADDRESS(NFC_BASE_ADDR_AXI + 0x830)

/* read column 464-465 byte but only 464 for bad block marker */
#define BAD_BLK_MARKER_464      IO_ADDRESS(NFC_BASE_ADDR_AXI + 0x600 + 464)
/* read column 0-1 byte, but only 1 is used for swapped main area data */
#define BAD_BLK_MARKER_SP_0     IO_ADDRESS(NFC_BASE_ADDR_AXI + 0x800)

/*!
 * Set 1 to specific operation bit, rest to 0 in LAUNCH_NFC Register for 
 * Specific operation
 */
#define NFC_CMD            		0x1
#define NFC_ADDR           		0x2
#define NFC_INPUT          		0x4
#define NFC_OUTPUT         		0x8
#define NFC_ID             		0x10
#define NFC_STATUS         		0x20

/* Bit Definitions */
#define NFC_OPS_STAT			(1 << 31)
#define NFC_INT_MSK			(1 << 4)
#define NFC_BIG				(1 << 5)
#define NFC_FLASH_ADDR_SHIFT		16
#define NFC_UNLOCK_END_ADDR_SHIFT	16
#define RBA_BUFFER0			(0 << 4)
#define RBA_BUFFER1			(1 << 4)
#define RBA_BUFFER2			(2 << 4)
#define RBA_BUFFER3			(3 << 4)
#define RBA_RESET			~(3 << 4)
#define NFC_RB				(1 << 29)
#define NFC_ECC_EN			(1 << 3)
#define NFC_CE				(1 << 1)
#define NFC_RST				(1 << 6)
#define NFC_PPB_32			(0 << 7)
#define NFC_PPB_64			(1 << 7)
#define NFC_PPB_128			(2 << 7)
#define NFC_PPB_256			(3 << 7)
#define NFC_PPB_RESET			~(3 << 7)
#define NFC_SP_EN			(1)
#define NFC_BLS_LOCKED			(0 << 16)
#define NFC_BLS_LOCKED_DEFAULT		(1 << 16)
#define NFC_BLS_UNLCOKED		(2 << 16)
#define NFC_BLS_RESET			~(3 << 16)
#define NFC_WPC_LOCK_TIGHT		(1)
#define NFC_WPC_LOCK			(1  << 1)
#define NFC_WPC_UNLOCK			(1  << 2)
#define NFC_WPC_RESET			~(7)

/* NFC Register Mapping */
#define REG_NFC_OPS_STAT		NFC_IPC
#define REG_NFC_INTRRUPT		NFC_CONFIG2
#define REG_NFC_FLASH_ADDR		NFC_FLASH_ADDR_CMD
#define REG_NFC_FLASH_CMD		NFC_FLASH_ADDR_CMD
#define REG_NFC_OPS			LAUNCH_NFC
#define REG_NFC_SET_RBA			NFC_CONFIG1
#define REG_NFC_RB			NFC_IPC
#define REG_NFC_ECC_EN			NFC_CONFIG2
#define REG_NFC_ECC_STATUS_RESULT	NFC_ECC_STATUS_RESULT
#define REG_NFC_CE			NFC_CONFIG1
#define REG_NFC_RST			NFC_CONFIG2
#define REG_NFC_PPB			NFC_CONFIG2
#define REG_NFC_SP_EN			NFC_CONFIG1
#define REG_NFC_BLS			NFC_WRPROT
#define REG_UNLOCK_BLK_ADD0		NFC_WRPROT_UNLOCK_BLK_ADD0
#define REG_UNLOCK_BLK_ADD1		NFC_WRPROT_UNLOCK_BLK_ADD1
#define REG_UNLOCK_BLK_ADD2		NFC_WRPROT_UNLOCK_BLK_ADD2
#define REG_UNLOCK_BLK_ADD3		NFC_WRPROT_UNLOCK_BLK_ADD3
#define REG_NFC_WPC			NFC_WRPROT

/* NFC V3 Specific MACRO functions definitions */
#define raw_write(v,a)                  __raw_writel(v,a)
#define raw_read(a)			__raw_readl(a)

/* Explcit ack ops status (if any), before issue of any command  */
#define ACK_OPS         	raw_write((raw_read(REG_NFC_OPS_STAT) & ~NFC_OPS_STAT), REG_NFC_OPS_STAT);

/* NFC buffer 0 to 3 are used for page read/write, starting with buffer0 */
/* Set RBA bits for BUFFER0 */
#define NFC_SET_RBA(val, buf_id)       \
	val = ((raw_read(REG_NFC_SET_RBA) & RBA_RESET) | buf_id);

#define UNLOCK_ADDR(start_addr,end_addr)     \
	 raw_write(start_addr | (end_addr << NFC_UNLOCK_END_ADDR_SHIFT), REG_UNLOCK_BLK_ADD0);

#define NFC_SET_BLS(val) ((raw_read(REG_NFC_BLS) & NFC_BLS_RESET) | val )
#define NFC_SET_WPC(val) ((raw_read(REG_NFC_WPC) & NFC_WPC_RESET) | val )
#define CHECK_NFC_RB     raw_read(REG_NFC_RB) & NFC_RB

#define READ_2K_PAGE	send_read_page(0);
#define PROG_2K_PAGE    send_prog_page(0);

#elif  CONFIG_ARCH_MXC_HAS_NFC_V2

/*
 * For V1/V2 NFC registers Definition
 */

#define NFC_AXI_BASE_ADDR      	0x00
/*
 * Addresses for NFC registers
 */
#define NFC_BUF_SIZE            (nfc_ip_base + 0xE00)
#define NFC_BUF_ADDR            (nfc_ip_base + 0xE04)
#define NFC_FLASH_ADDR          (nfc_ip_base + 0xE06)
#define NFC_FLASH_CMD           (nfc_ip_base + 0xE08)
#define NFC_CONFIG              (nfc_ip_base + 0xE0A)
#define NFC_ECC_STATUS_RESULT   (nfc_ip_base + 0xE0C)
#define NFC_RSLTMAIN_AREA       (nfc_ip_base + 0xE0E)
#define NFC_RSLTSPARE_AREA      (nfc_ip_base + 0xE10)
#define NFC_WRPROT              (nfc_ip_base + 0xE12)
#define NFC_UNLOCKSTART_BLKADDR (nfc_ip_base + 0xE14)
#define NFC_UNLOCKEND_BLKADDR   (nfc_ip_base + 0xE16)
#define NFC_NF_WRPRST           (nfc_ip_base + 0xE18)
#define NFC_CONFIG1             (nfc_ip_base + 0xE1A)
#define NFC_CONFIG2             (nfc_ip_base + 0xE1C)

/*!
 * Addresses for NFC RAM BUFFER Main area 0
 */
#define MAIN_AREA0        (volatile u16 *)IO_ADDRESS(NFC_BASE_ADDR + 0x000)
#define MAIN_AREA1        (volatile u16 *)IO_ADDRESS(NFC_BASE_ADDR + 0x200)
#define MAIN_AREA2        (volatile u16 *)IO_ADDRESS(NFC_BASE_ADDR + 0x400)
#define MAIN_AREA3        (volatile u16 *)IO_ADDRESS(NFC_BASE_ADDR + 0x600)

/*!
 * Addresses for NFC SPARE BUFFER Spare area 0
 */
#define SPARE_AREA0       (volatile u16 *)IO_ADDRESS(NFC_BASE_ADDR + 0x800)
#define SPARE_AREA1       (volatile u16 *)IO_ADDRESS(NFC_BASE_ADDR + 0x810)
#define SPARE_AREA2       (volatile u16 *)IO_ADDRESS(NFC_BASE_ADDR + 0x820)
#define SPARE_AREA3       (volatile u16 *)IO_ADDRESS(NFC_BASE_ADDR + 0x830)

/* read column 464-465 byte but only 464 for bad block marker */
#define BAD_BLK_MARKER_464      IO_ADDRESS(NFC_BASE_ADDR + 0x600 + 464)
/* read column 0-1 byte, but only 1 is used for swapped main area data */
#define BAD_BLK_MARKER_SP_0     IO_ADDRESS(NFC_BASE_ADDR + 0x800)

/*!
 * Set INT to 0, Set 1 to specific operation bit, rest to 0 in LAUNCH_NFC Register for 
 * Specific operation
 */
#define NFC_CMD            		0x1
#define NFC_ADDR           		0x2
#define NFC_INPUT          		0x4
#define NFC_OUTPUT         		0x8
#define NFC_ID             		0x10
#define NFC_STATUS         		0x20

/* Bit Definitions */
#define NFC_OPS_STAT			(1 << 15)
#define NFC_SP_EN           		(1 << 2)
#define NFC_ECC_EN          		(1 << 3)
#define NFC_INT_MSK         		(1 << 4)
#define NFC_BIG             		(1 << 5)
#define NFC_RST             		(1 << 6)
#define NFC_CE              		(1 << 7)
#define NFC_ONE_CYCLE       		(1 << 8)
#define NFC_BLS_LOCKED			0
#define NFC_BLS_LOCKED_DEFAULT		1
#define NFC_BLS_UNLCOKED		2
#define NFC_WPC_LOCK_TIGHT		(1)
#define NFC_WPC_LOCK			(1  << 1)
#define NFC_WPC_UNLOCK			(1  << 2)
#define NFC_FLASH_ADDR_SHIFT 		0
#define NFC_UNLOCK_END_ADDR_SHIFT	0

/* NFC Register Mapping */
#define REG_NFC_OPS_STAT		NFC_CONFIG2
#define REG_NFC_INTRRUPT		NFC_CONFIG1
#define REG_NFC_FLASH_ADDR		NFC_FLASH_ADDR
#define REG_NFC_FLASH_CMD		NFC_FLASH_CMD
#define REG_NFC_OPS			NFC_CONFIG2
#define REG_NFC_SET_RBA			NFC_BUF_ADDR
#define REG_NFC_ECC_EN			NFC_CONFIG1
#define REG_NFC_ECC_STATUS_RESULT  	NFC_ECC_STATUS_RESULT
#define REG_NFC_CE			NFC_CONFIG1
#define REG_NFC_SP_EN			NFC_CONFIG1
#define REG_NFC_BLS			NFC_CONFIG
#define REG_NFC_WPC			NFC_WRPROT
#define REG_START_BLKADDR      		NFC_UNLOCKSTART_BLKADDR
#define REG_END_BLKADDR        		NFC_UNLOCKEND_BLKADDR
#define REG_NFC_RST			NFC_CONFIG1

/* NFC V1/V2 Specific MACRO functions definitions */

#define raw_write(v,a)                  __raw_writew(v,a)
#define raw_read(a)                     __raw_readw(a)

#define NFC_SET_BLS(val)  		val

#define UNLOCK_ADDR(start_addr,end_addr)     \
						raw_write(start_addr,REG_START_BLKADDR);\
						raw_write(end_addr,REG_END_BLKADDR);

#define NFC_SET_WPC(val)                val

/* NULL Definitions */
#define ACK_OPS
#define NFC_SET_RBA(val,buf_id)

#define READ_2K_PAGE           	send_read_page(0);\
				send_read_page(1);\
				send_read_page(2);\
				send_read_page(3);

#define PROG_2K_PAGE            send_prog_page(0);\
				send_prog_page(1);\
				send_prog_page(2);\
				send_prog_page(3);

#define CHECK_NFC_RB                1

#endif

#endif				/* MXCND_H */
