/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * Freescale Faraday Quad SPI driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/spi/spi.h>
#include <mach/spi-mvf.h>

#define	DRIVER_NAME "mvf-qspi"

/* Flash opcodes. */
#define	OPCODE_WREN		0x06	/* Write enable */
#define	OPCODE_RDSR		0x05	/* Read status register */
#define	OPCODE_WRSR		0x01	/* Write status register 1 byte */
#define	OPCODE_NORM_READ	0x03	/* Read data bytes (low frequency) */
#define	OPCODE_FAST_READ	0x0b	/* Read data bytes (high frequency) */
#define	OPCODE_PP		0x02	/* Page program (up to 256 bytes) */
#define	OPCODE_BE_4K		0x20	/* Erase 4KiB block */
#define	OPCODE_BE_32K		0x52	/* Erase 32KiB block */
#define	OPCODE_CHIP_ERASE	0xc7	/* Erase whole flash chip */
#define	OPCODE_SE		0xd8	/* Sector erase (usually 64KiB) */
#define	OPCODE_RDID		0x9f	/* Read JEDEC ID */

/* Used for Spansion flashes only. */
#define	OPCODE_BRWR		0x17	/* Bank register write */

#define endian_change_32bit(A)	((((unsigned int)(A) & 0xff000000) >> 24) | \
				(((unsigned int)(A) & 0x00ff0000) >> 8) | \
				(((unsigned int)(A) & 0x0000ff00) << 8) | \
				(((unsigned int)(A) & 0x000000ff) << 24))

#define PSP_QSPI0_MEMMAP_BASE	(0x20000000)
#define PSP_QSPI1_MEMMAP_BASE	(0x50000000)
#define RX_BUFFER_SIZE		(32 << 2)
#define TX_BUFFER_SIZE		(16 << 2)

/* Field definitions for RBSR */
#define QSPI_RBSR_RDCTR_SHIFT	(16)
#define QSPI_RBSR_RDCTR_MASK	((0x0000FFFF) << (QSPI_RBSR_RDCTR_SHIFT))

#define QSPI_RBSR_RDBFL_SHIFT	(8)
#define QSPI_RBSR_RDBFL_MASK	((0x0000003F) << (QSPI_RBSR_RDBFL_SHIFT))

/* Field definitions for IPCR */
#define QSPI_IPCR_SEQID_SHIFT	(24)
#define QSPI_IPCR_SEQID_MASK	((0x0000000F) << (QSPI_IPCR_SEQID_SHIFT))

#define QSPI_IPCR_PAR_EN_SHIFT	(16)
#define QSPI_IPCR_PAR_EN_MASK	((1) << (QSPI_IPCR_PAR_EN_SHIFT))

#define QSPI_IPCR_IDATSZ_SHIFT	(0)
#define QSPI_IPCR_IDATSZ_MASK	((0x0000FFFF) << (QSPI_IPCR_IDATSZ_SHIFT))

/* Field definitions for MCR */
#define QSPI_MCR_DOZE_SHIFT	(15)
#define QSPI_MCR_DOZE_MASK	((1) << (QSPI_MCR_DOZE_SHIFT))

#define QSPI_MCR_MDIS_SHIFT	(14)
#define QSPI_MCR_MDIS_MASK	((1) << (QSPI_MCR_MDIS_SHIFT))

#define QSPI_MCR_CLR_TXF_SHIFT	(11)
#define QSPI_MCR_CLR_TXF_MASK	((1) << (QSPI_MCR_CLR_TXF_SHIFT))

#define QSPI_MCR_CLR_RXF_SHIFT	(10)
#define QSPI_MCR_CLR_RXF_MASK	((1) << (QSPI_MCR_CLR_RXF_SHIFT))

#define QSPI_MCR_DDR_EN_SHIFT	(7)
#define QSPI_MCR_DDR_EN_MASK	((1) << (QSPI_MCR_DDR_EN_SHIFT))

#define QSPI_MCR_DQS_EN_SHIFT	(6)
#define QSPI_MCR_DQS_EN_MASK	((1) << (QSPI_MCR_DQS_EN_SHIFT))

#define QSPI_MCR_SWRSTHD_SHIFT	(1)
#define QSPI_MCR_SWRSTHD_MASK	((1) << (QSPI_MCR_SWRSTHD_SHIFT))

#define QSPI_MCR_SWRSTSD_SHIFT	(0)
#define QSPI_MCR_SWRSTSD_MASK	((1) << (QSPI_MCR_SWRSTSD_SHIFT))

/* Field definitions for SMPR */
#define QSPI_SMPR_DDRSMP_SHIFT		(16)
#define QSPI_SMPR_DDRSMP_MASK		((0x00000007) << \
					(QSPI_SMPR_DDRSMP_SHIFT))

#define QSPI_SMPR_FSDLY_SHIFT		(6)
#define QSPI_SMPR_FSDLY_MASK		((1) << (QSPI_SMPR_FSDLY_SHIFT))

#define QSPI_SMPR_FSPHS_SHIFT		(5)
#define QSPI_SMPR_FSPHS_MASK		((1) << (QSPI_SMPR_FSPHS_SHIFT))

#define QSPI_SMPR_HSDLY_SHIFT		(2)
#define QSPI_SMPR_HSDLY_MASK		((1) << (QSPI_SMPR_HSDLY_SHIFT))

#define QSPI_SMPR_HSPHS_SHIFT		(1)
#define QSPI_SMPR_HSPHS_MASK		((1) << (QSPI_SMPR_HSPHS_SHIFT))

#define QSPI_SMPR_HSENA_SHIFT		(0)
#define QSPI_SMPR_HSENA_MASK		((1) << (QSPI_SMPR_HSENA_SHIFT))

/* Field definitions for BFGENCR */
#define QSPI_BFGENCR_PAR_EN_SHIFT	(16)
#define QSPI_BFGENCR_PAR_EN_MASK	((1) << (QSPI_BFGENCR_PAR_EN_SHIFT))

#define QSPI_BFGENCR_SEQID_SHIFT	(12)
#define QSPI_BFGENCR_SEQID_MASK		((0xF) << (QSPI_BFGENCR_SEQID_SHIFT))

/* Field definitions for SR */
#define QSPI_SR_DLPSMP_SHIFT		(29)
#define QSPI_SR_DLPSMP_MASK		((0x7) << (QSPI_SR_DLPSMP_SHIFT))

#define QSPI_SR_TXFULL_SHIFT		(27)
#define QSPI_SR_TXFULL_MASK		((1) << (QSPI_SR_TXFULL_SHIFT))

#define QSPI_SR_TXNE_SHIFT		(24)
#define QSPI_SR_TXNE_MASK		((1) << (QSPI_SR_TXNE_SHIFT))

#define QSPI_SR_RXDMA_SHIFT		(23)
#define QSPI_SR_RXDMA_MASK		((1) << (QSPI_SR_RXDMA_SHIFT))

#define QSPI_SR_RXFULL_SHIFT		(19)
#define QSPI_SR_RXFULL_MASK		((1) << (QSPI_SR_RXFULL_SHIFT))

#define QSPI_SR_RXWE_SHIFT		(16)
#define QSPI_SR_RXWE_MASK		((1) << (QSPI_SR_RXWE_SHIFT))

#define QSPI_SR_AHB3FULL_SHIFT		(14)
#define QSPI_SR_AHB3FULL_MASK		((1) << (QSPI_SR_AHB3FULL_SHIFT))

#define QSPI_SR_AHB2FULL_SHIFT		(13)
#define QSPI_SR_AHB2FULL_MASK		((1) << (QSPI_SR_AHB2FULL_SHIFT))

#define QSPI_SR_AHB1FULL_SHIFT		(12)
#define QSPI_SR_AHB1FULL_MASK		((1) << (QSPI_SR_AHB1FULL_SHIFT))

#define QSPI_SR_AHB0FULL_SHIFT		(11)
#define QSPI_SR_AHB0FULL_MASK		((1) << (QSPI_SR_AHB0FULL_SHIFT))

#define QSPI_SR_AHB3NE_SHIFT		(10)
#define QSPI_SR_AHB3NE_MASK		((1) << (QSPI_SR_AHB3NE_SHIFT))

#define QSPI_SR_AHB2NE_SHIFT		(9)
#define QSPI_SR_AHB2NE_MASK		((1) << (QSPI_SR_AHB2NE_SHIFT))

#define QSPI_SR_AHB1NE_SHIFT		(8)
#define QSPI_SR_AHB1NE_MASK		((1) << (QSPI_SR_AHB1NE_SHIFT))

#define QSPI_SR_AHB0NE_SHIFT		(7)
#define QSPI_SR_AHB0NE_MASK		((1) << (QSPI_SR_AHB0NE_SHIFT))

#define QSPI_SR_AHBTRN_SHIFT		(6)
#define QSPI_SR_AHBTRN_MASK		((1) << (QSPI_SR_AHBTRN_SHIFT))

#define QSPI_SR_AHBGNT_SHIFT		(5)
#define QSPI_SR_AHBGNT_MASK		((1) << (QSPI_SR_AHBGNT_SHIFT))

#define QSPI_SR_AHB_ACC_SHIFT		(2)
#define QSPI_SR_AHB_ACC_MASK		((1) << (QSPI_SR_AHB_ACC_SHIFT))

#define QSPI_SR_IP_ACC_SHIFT		(1)
#define QSPI_SR_IP_ACC_MASK		((1) << (QSPI_SR_IP_ACC_SHIFT))

#define QSPI_SR_BUSY_SHIFT		(0)
#define QSPI_SR_BUSY_MASK		((1) << (QSPI_SR_BUSY_SHIFT))

struct mvfqspi {
	void __iomem *iobase;
	int irq;
	struct clk *clk;

	wait_queue_head_t waitq;

	struct work_struct work;
	struct workqueue_struct *workq;
	spinlock_t lock;
	struct list_head msgq;
};

static void set_lut(struct mvfqspi *mvfqspi)
{
	/* Unlock the LUT */
	writel(0x5af05af0, mvfqspi->iobase + QUADSPI_LUTKEY);
	writel(0x2, mvfqspi->iobase + QUADSPI_LCKCR);
	do {} while (!(readl(mvfqspi->iobase + QUADSPI_LCKCR) >> 1));

	/* program required commands */
	/* SEQID 0 - leave as single read default */
#ifdef QUADSPI_ENABLE_32BITS_ADDR
	writel(0x08200413, mvfqspi->iobase + QUADSPI_LUT(0));
#else
	writel(0x08180403, mvfqspi->iobase + QUADSPI_LUT(0));
#endif
	writel(0x1c80, mvfqspi->iobase + QUADSPI_LUT(1));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(2));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(3));

	/* SEQID 1 - Write enable */
	writel(0x406, mvfqspi->iobase + QUADSPI_LUT(4));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(5));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(6));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(7));

	/* SEQID 2 - Bulk Erase */
	writel(0x460, mvfqspi->iobase + QUADSPI_LUT(8));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(9));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(10));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(11));

	/* SEQID 3 - Read Status */
	writel(0x1c010405, mvfqspi->iobase + QUADSPI_LUT(12));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(13));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(14));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(15));

	/* SEQID 4 - Page Program */
#ifdef QUADSPI_ENABLE_32BITS_ADDR
	writel(0x08200412, mvfqspi->iobase + QUADSPI_LUT(16));
#else
	writel(0x08180402, mvfqspi->iobase + QUADSPI_LUT(16));
#endif
	writel(0x2040, mvfqspi->iobase + QUADSPI_LUT(17));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(18));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(19));

	/* SEQID 5 - Write Config/Status */
	writel(0x20020401, mvfqspi->iobase + QUADSPI_LUT(20));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(21));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(22));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(23));

	/* SEQID 6 - Read Config */
	writel(0x1c010435, mvfqspi->iobase + QUADSPI_LUT(24));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(25));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(26));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(27));


	/* SEQID 7 - Sector Erase */
#ifdef QUADSPI_ENABLE_32BITS_ADDR
	writel(0x082004dc, mvfqspi->iobase + QUADSPI_LUT(28));
#else
	writel(0x081804d8, mvfqspi->iobase + QUADSPI_LUT(28));
#endif
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(29));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(30));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(31));


	/* SEQID 8 - Read ID */
	writel(0x1c04049f, mvfqspi->iobase + QUADSPI_LUT(32));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(33));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(34));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(35));

	/* SEQID 9 - Read ID */
	writel(0x08180490, mvfqspi->iobase + QUADSPI_LUT(36));
	writel(0x00001c02, mvfqspi->iobase + QUADSPI_LUT(37));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(38));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(39));

	/* SEQID10 - DDR SINGLE I/O READ */
#ifdef QUADSPI_ENABLE_32BITS_ADDR
	writel(0x2820040e, mvfqspi->iobase + QUADSPI_LUT(40));
#else
	writel(0x2818040d, mvfqspi->iobase + QUADSPI_LUT(40));
#endif
	writel(0x0c022cff, mvfqspi->iobase + QUADSPI_LUT(41));
	writel(0x3880, mvfqspi->iobase + QUADSPI_LUT(42));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(43));

	/* SEQID 11 - SDR QUAD I/O read */
#ifdef QUADSPI_ENABLE_32BITS_ADDR
	writel(0x0a2004ec, mvfqspi->iobase + QUADSPI_LUT(44));
#else
	writel(0x0a1804eb, mvfqspi->iobase + QUADSPI_LUT(44));
#endif
	writel(0x0e0412ff, mvfqspi->iobase + QUADSPI_LUT(45));
	writel(0x1e80, mvfqspi->iobase + QUADSPI_LUT(46));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(47));

	/* SEQID 12 - DDR QUAD I/O read */
#ifdef QUADSPI_ENABLE_32BITS_ADDR
	writel(0x2a2004ee, mvfqspi->iobase + QUADSPI_LUT(48));
#else
	writel(0x2a1804ed, mvfqspi->iobase + QUADSPI_LUT(48));
#endif
	writel(0x0c062eff, mvfqspi->iobase + QUADSPI_LUT(49));
	writel(0x3a80, mvfqspi->iobase + QUADSPI_LUT(50));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(51));

	/* SEQID 13 - QUAD Page Program */
#ifdef QUADSPI_ENABLE_32BITS_ADDR
	writel(0x08200434, mvfqspi->iobase + QUADSPI_LUT(52));
#else
	writel(0x08180432, mvfqspi->iobase + QUADSPI_LUT(52));
#endif
	writel(0x2240, mvfqspi->iobase + QUADSPI_LUT(53));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(54));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(55));

	/* SEQID 14 - Fast READ */
#ifdef QUADSPI_ENABLE_32BITS_ADDR
	writel(0x0820040C, mvfqspi->iobase + QUADSPI_LUT(56));
#else
	writel(0x0818040B, mvfqspi->iobase + QUADSPI_LUT(56));
#endif
	writel(0x1c800c08, mvfqspi->iobase + QUADSPI_LUT(57));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(58));
	writel(0x0, mvfqspi->iobase + QUADSPI_LUT(59));

	/* Lock the LUT */
	writel(0x5af05af0, mvfqspi->iobase + QUADSPI_LUTKEY);
	writel(0x1, mvfqspi->iobase + QUADSPI_LCKCR);
	do {} while (!(readl(mvfqspi->iobase + QUADSPI_LCKCR) & 0x1));
}

static void mvfqspi_transfer_msg(unsigned int opr, unsigned int to_or_from,
	struct mvfqspi *mvfqspi, unsigned count, const u32 *txbuf, u32 *rxbuf)
{
	unsigned int reg, status_reg, tmp, mcr_reg;
	unsigned int i, j, size, tx_size;

	mcr_reg = readl(mvfqspi->iobase + QUADSPI_MCR);
	writel(QSPI_MCR_CLR_RXF_MASK | QSPI_MCR_CLR_TXF_MASK | 0xf0000,
		mvfqspi->iobase + QUADSPI_MCR);
	writel(0x100, mvfqspi->iobase + QUADSPI_RBCT);

	switch (opr) {
	case OPCODE_PP:
		to_or_from += PSP_QSPI0_MEMMAP_BASE;

		while (count > 0) {
			writel(to_or_from, mvfqspi->iobase + QUADSPI_SFAR);

			status_reg = 0;
			while ((status_reg & 0x02000000) != 0x02000000) {
				writel((1 << QSPI_IPCR_SEQID_SHIFT) | 0,
					mvfqspi->iobase + QUADSPI_IPCR);
				do {} while (readl(mvfqspi->iobase +
					QUADSPI_SR) & QSPI_SR_BUSY_MASK);

				writel((0x3 << QSPI_IPCR_SEQID_SHIFT) | 1,
					mvfqspi->iobase + QUADSPI_IPCR);
				do {} while (readl(mvfqspi->iobase +
					QUADSPI_SR) & QSPI_SR_BUSY_MASK);

				reg = readl(mvfqspi->iobase + QUADSPI_RBSR);
				if (reg & QSPI_RBSR_RDBFL_MASK)
					status_reg =
					readl(mvfqspi->iobase + QUADSPI_RBDR);
				writel(readl(mvfqspi->iobase + QUADSPI_MCR) |
					QSPI_MCR_CLR_RXF_MASK,
					mvfqspi->iobase + QUADSPI_MCR);
			}

			writel(to_or_from, mvfqspi->iobase + QUADSPI_SFAR);
			tx_size = (count > TX_BUFFER_SIZE) ?
				TX_BUFFER_SIZE : count;

			to_or_from += tx_size;
			count -= tx_size;

			size = (tx_size + 3) / 4;
			i = (size > 16) ? 16 : size;

			for (j = 0; j < i; j++) {
				tmp = endian_change_32bit(*txbuf);
				writel(tmp, mvfqspi->iobase + QUADSPI_TBDR);
				txbuf++;
			}

			writel(4 << QSPI_IPCR_SEQID_SHIFT | tx_size,
				mvfqspi->iobase + QUADSPI_IPCR);

			for (j = i; j < size; j++) {
				do {} while (readl(mvfqspi->iobase +
					QUADSPI_SR) & QSPI_SR_TXFULL_MASK);
				tmp = endian_change_32bit(*txbuf);
				writel(tmp, mvfqspi->iobase + QUADSPI_TBDR);
				txbuf++;
			}

			do {} while (readl(mvfqspi->iobase + QUADSPI_SR) &
				QSPI_SR_BUSY_MASK);

			status_reg = 0x01000000;
			while ((status_reg & 0x01000000) == 0x01000000) {
				writel((0x3 << QSPI_IPCR_SEQID_SHIFT) | 1,
					mvfqspi->iobase + QUADSPI_IPCR);
				do {} while (readl(mvfqspi->iobase +
					QUADSPI_SR) & QSPI_SR_BUSY_MASK);

				reg = readl(mvfqspi->iobase + QUADSPI_RBSR);
				if (reg & QSPI_RBSR_RDBFL_MASK)
					status_reg =
					readl(mvfqspi->iobase + QUADSPI_RBDR);
				writel(readl(mvfqspi->iobase + QUADSPI_MCR) |
					QSPI_MCR_CLR_RXF_MASK,
					mvfqspi->iobase + QUADSPI_MCR);
			}
		}
		break;
	case OPCODE_NORM_READ:
		to_or_from += PSP_QSPI0_MEMMAP_BASE;

		while (count > 0) {
			writel(to_or_from, mvfqspi->iobase + QUADSPI_SFAR);
			size = (count > RX_BUFFER_SIZE) ?
				RX_BUFFER_SIZE : count;
			writel(0 << QSPI_IPCR_SEQID_SHIFT | size,
				mvfqspi->iobase + QUADSPI_IPCR);
			do {} while (readl(mvfqspi->iobase + QUADSPI_SR) &
				QSPI_SR_BUSY_MASK);

			to_or_from += size;
			count -= size;

			i = 0;
			while ((RX_BUFFER_SIZE >= size) && (size > 0)) {
				tmp = readl(mvfqspi->iobase + QUADSPI_RBDR +
					i * 4);
				*rxbuf = endian_change_32bit(tmp);
				rxbuf++;
				size -= 4;
				i++;
			}
			writel(readl(mvfqspi->iobase + QUADSPI_MCR) |
				QSPI_MCR_CLR_RXF_MASK,
				mvfqspi->iobase + QUADSPI_MCR);
		}
		break;
	case OPCODE_FAST_READ:
		to_or_from += PSP_QSPI0_MEMMAP_BASE;

		while (count > 0) {
			writel(to_or_from, mvfqspi->iobase + QUADSPI_SFAR);
			size = (count > RX_BUFFER_SIZE) ?
				RX_BUFFER_SIZE : count;
			writel(14 << QSPI_IPCR_SEQID_SHIFT | size,
				mvfqspi->iobase + QUADSPI_IPCR);
			do {} while (readl(mvfqspi->iobase + QUADSPI_SR) &
				QSPI_SR_BUSY_MASK);

			to_or_from += size;
			count -= size;

			i = 0;
			while ((RX_BUFFER_SIZE >= size) && (size > 0)) {
				tmp = readl(mvfqspi->iobase + QUADSPI_RBDR +
					i * 4);
				*rxbuf = endian_change_32bit(tmp);
				rxbuf++;
				size -= 4;
				i++;
			}
			writel(readl(mvfqspi->iobase + QUADSPI_MCR) |
				QSPI_MCR_CLR_RXF_MASK,
				mvfqspi->iobase + QUADSPI_MCR);
		}
		break;
	case OPCODE_SE:
		to_or_from += PSP_QSPI0_MEMMAP_BASE;
		writel(to_or_from, mvfqspi->iobase + QUADSPI_SFAR);

		writel((7 << QSPI_IPCR_SEQID_SHIFT) | 0,
			mvfqspi->iobase + QUADSPI_IPCR);
		do {} while (readl(mvfqspi->iobase + QUADSPI_SR) &
			QSPI_SR_BUSY_MASK);

		status_reg = 0x01000000;
		while ((status_reg & 0x01000000) == 0x01000000) {
			writel((0x3 << QSPI_IPCR_SEQID_SHIFT) | 1,
				mvfqspi->iobase + QUADSPI_IPCR);
			do {} while (readl(mvfqspi->iobase + QUADSPI_SR) &
				QSPI_SR_BUSY_MASK);

			reg = readl(mvfqspi->iobase + QUADSPI_RBSR);
			if (reg & QSPI_RBSR_RDBFL_MASK)
				status_reg = readl(mvfqspi->iobase +
					QUADSPI_RBDR);
			writel(readl(mvfqspi->iobase + QUADSPI_MCR) |
				QSPI_MCR_CLR_RXF_MASK,
				mvfqspi->iobase + QUADSPI_MCR);
		}
		break;
	case OPCODE_CHIP_ERASE:
		writel(PSP_QSPI0_MEMMAP_BASE, mvfqspi->iobase + QUADSPI_SFAR);

		writel((0x2 << QSPI_IPCR_SEQID_SHIFT) | 0,
			mvfqspi->iobase + QUADSPI_IPCR);
		do {} while (readl(mvfqspi->iobase + QUADSPI_SR) &
			QSPI_SR_BUSY_MASK);

		status_reg = 0x01000000;
		while ((status_reg & 0x01000000) == 0x01000000) {
			writel((0x3 << QSPI_IPCR_SEQID_SHIFT) | 1,
				mvfqspi->iobase + QUADSPI_IPCR);
			do {} while (readl(mvfqspi->iobase + QUADSPI_SR) &
				QSPI_SR_BUSY_MASK);

			reg = readl(mvfqspi->iobase + QUADSPI_RBSR);
			if (reg & QSPI_RBSR_RDBFL_MASK)
				status_reg = readl(mvfqspi->iobase +
					QUADSPI_RBDR);
			writel(readl(mvfqspi->iobase + QUADSPI_MCR) |
				QSPI_MCR_CLR_RXF_MASK,
				mvfqspi->iobase + QUADSPI_MCR);
		}
		break;
	case OPCODE_WREN:
		to_or_from += PSP_QSPI0_MEMMAP_BASE;
		writel(to_or_from, mvfqspi->iobase + QUADSPI_SFAR);

		writel((0x1 << QSPI_IPCR_SEQID_SHIFT) | 0,
			mvfqspi->iobase + QUADSPI_IPCR);
		do {} while (readl(mvfqspi->iobase + QUADSPI_SR) &
			QSPI_SR_BUSY_MASK);
		break;
	case OPCODE_RDSR:
		writel(PSP_QSPI0_MEMMAP_BASE, mvfqspi->iobase + QUADSPI_SFAR);

		writel(0x3 << QSPI_IPCR_SEQID_SHIFT | 1,
			mvfqspi->iobase + QUADSPI_IPCR);
		do {} while (readl(mvfqspi->iobase + QUADSPI_SR) &
			QSPI_SR_BUSY_MASK);

		reg = readl(mvfqspi->iobase + QUADSPI_RBSR);
		if (reg & QSPI_RBSR_RDBFL_MASK)
			*rxbuf = readl(mvfqspi->iobase + QUADSPI_RBDR);

		writel(readl(mvfqspi->iobase + QUADSPI_MCR) |
			QSPI_MCR_CLR_RXF_MASK, mvfqspi->iobase + QUADSPI_MCR);
		break;
	case OPCODE_RDID:
		writel(PSP_QSPI0_MEMMAP_BASE, mvfqspi->iobase + QUADSPI_SFAR);

		writel((0x8 << QSPI_IPCR_SEQID_SHIFT) | count,
			mvfqspi->iobase + QUADSPI_IPCR);
		do {} while (readl(mvfqspi->iobase + QUADSPI_SR) &
			QSPI_SR_BUSY_MASK);

		reg = readl(mvfqspi->iobase + QUADSPI_RBSR);
		if (reg & QSPI_RBSR_RDBFL_MASK) {
			i = 0;
			while (count >= 4) {
				tmp = readl(mvfqspi->iobase + QUADSPI_RBDR + i);
				*rxbuf = endian_change_32bit(tmp);
				rxbuf += 1;
				count -= 4;
				i += 4;
			}
			if (count) {
				tmp = readl(mvfqspi->iobase + QUADSPI_RBDR + i);
				tmp = endian_change_32bit(tmp);
				memcpy(rxbuf, &tmp, count);
			}
		}
		break;
	default:
		printk(KERN_ERR "cannot support %x opcode\n", opr);
		break;
	}

	writel(mcr_reg, mvfqspi->iobase + QUADSPI_MCR);
}


static void mvfqspi_work(struct work_struct *work)
{
	struct mvfqspi *mvfqspi = container_of(work, struct mvfqspi, work);
	unsigned long flags;

	spin_lock_irqsave(&mvfqspi->lock, flags);
	while (!list_empty(&mvfqspi->msgq)) {
		struct spi_message *msg;
		struct spi_device *spi;
		struct spi_transfer *xfer;
		int status = 0;
		unsigned int opr = 0, to_or_from = 0;

		msg = container_of(mvfqspi->msgq.next,
			struct spi_message, queue);

		list_del_init(&msg->queue);
		spin_unlock_irqrestore(&mvfqspi->lock, flags);

		spi = msg->spi;
		list_for_each_entry(xfer, &msg->transfers, transfer_list) {
			if (xfer->tx_buf && opr == 0) {
				unsigned char temp_tx_buf =
					*(unsigned char *)xfer->tx_buf;
				unsigned int tx_buf =
					*(unsigned int *)xfer->tx_buf;

				switch (temp_tx_buf) {
				case OPCODE_NORM_READ:
					opr = OPCODE_NORM_READ;
					to_or_from = tx_buf & 0xffffff00;
					msg->actual_length += xfer->len;
					continue;
				case OPCODE_FAST_READ:
					opr = OPCODE_FAST_READ;
					to_or_from = tx_buf & 0xffffff00;
					msg->actual_length += xfer->len;
					continue;
				case OPCODE_PP:
					opr = OPCODE_PP;
					to_or_from = tx_buf & 0xffffff00;
					msg->actual_length += xfer->len;
					continue;
				case OPCODE_RDID:
					opr = OPCODE_RDID;
					msg->actual_length += xfer->len;
					continue;
				case OPCODE_SE:
					opr = OPCODE_SE;
					to_or_from = tx_buf & 0xffffff00;
					break;
				case OPCODE_WREN:
					opr = OPCODE_WREN;
					to_or_from = tx_buf & 0xffffff00;
					break;
				case OPCODE_RDSR:
					opr = OPCODE_RDSR;
					msg->actual_length += xfer->len;
					continue;
				case OPCODE_CHIP_ERASE:
					opr = OPCODE_CHIP_ERASE;
					msg->actual_length += xfer->len;
					break;
				default:
					printk(KERN_ERR "cannot support"
						" %x opcode\n", temp_tx_buf);
					break;
				}
			}

			if (opr != 0) {
				mvfqspi_transfer_msg(opr,
					endian_change_32bit(to_or_from),
					mvfqspi, xfer->len,
					xfer->tx_buf,
					xfer->rx_buf);
				opr = 0;
			}

			if (xfer->delay_usecs)
				udelay(xfer->delay_usecs);
			msg->actual_length += xfer->len;
		}
		msg->status = status;
		msg->complete(msg->context);

		spin_lock_irqsave(&mvfqspi->lock, flags);
	}
	spin_unlock_irqrestore(&mvfqspi->lock, flags);
}

static int mvfqspi_transfer(struct spi_device *spi, struct spi_message *msg)
{
	struct mvfqspi *mvfqspi;
	struct spi_transfer *xfer;
	unsigned long flags;
	mvfqspi = spi_master_get_devdata(spi->master);

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		if (xfer->bits_per_word && ((xfer->bits_per_word < 8)
			|| (xfer->bits_per_word > 16))) {
			dev_dbg(&spi->dev, "%d bits per word is not "
				"supported\n", xfer->bits_per_word);
				goto fail;
		}
	}
	msg->status = -EINPROGRESS;
	msg->actual_length = 0;

	spin_lock_irqsave(&mvfqspi->lock, flags);
	list_add_tail(&msg->queue, &mvfqspi->msgq);
	queue_work(mvfqspi->workq, &mvfqspi->work);
	spin_unlock_irqrestore(&mvfqspi->lock, flags);

	return 0;
fail:
	msg->status = -EINVAL;
	return -EINVAL;
}

static int mvfqspi_setup(struct spi_device *spi)
{
	unsigned int reg_val, smpr_val, seq_id;
	struct mvfqspi *mvfqspi;

	mvfqspi = spi_master_get_devdata(spi->master);

	if ((spi->bits_per_word < 8) || (spi->bits_per_word > 16)) {
		dev_dbg(&spi->dev, "%d bits per word is not supported\n",
			spi->bits_per_word);
		return -EINVAL;
	}

	if (spi->chip_select >= spi->master->num_chipselect) {
		dev_dbg(&spi->dev, "%d chip select is out of range\n",
			spi->chip_select);
		return -EINVAL;
	}

	writel(0xf0000 | QSPI_MCR_MDIS_MASK, mvfqspi->iobase + QUADSPI_MCR);

	reg_val = readl(mvfqspi->iobase + QUADSPI_SMPR);
	writel(reg_val & ~(QSPI_SMPR_FSDLY_MASK | QSPI_SMPR_FSPHS_MASK |
		QSPI_SMPR_HSENA_MASK), mvfqspi->iobase + QUADSPI_SMPR);
	writel(0xf0000, mvfqspi->iobase + QUADSPI_MCR);

	writel(0x1 << 24 | PSP_QSPI0_MEMMAP_BASE,
		mvfqspi->iobase + QUADSPI_SFA1AD);
	writel(0x1 << 24 | PSP_QSPI0_MEMMAP_BASE,
		mvfqspi->iobase + QUADSPI_SFA2AD);
	writel(0x1 << 25 | PSP_QSPI0_MEMMAP_BASE,
		mvfqspi->iobase + QUADSPI_SFB1AD);
	writel(0x1 << 25 | PSP_QSPI0_MEMMAP_BASE,
		mvfqspi->iobase + QUADSPI_SFB2AD);

	set_lut(mvfqspi);

	reg_val = 0;
	reg_val |= 0xf0000;
	smpr_val = readl(mvfqspi->iobase + QUADSPI_SMPR);
	smpr_val &= ~QSPI_SMPR_DDRSMP_MASK;
	writel(smpr_val, mvfqspi->iobase + QUADSPI_SMPR);
	reg_val &= ~QSPI_MCR_DDR_EN_MASK;
	writel(reg_val, mvfqspi->iobase + QUADSPI_MCR);

	seq_id = 0;
	reg_val = readl(mvfqspi->iobase + QUADSPI_BFGENCR);
	reg_val &= ~QSPI_BFGENCR_SEQID_MASK;
	reg_val |= (seq_id << QSPI_BFGENCR_SEQID_SHIFT);
	reg_val &= ~QSPI_BFGENCR_PAR_EN_MASK;
	writel(reg_val, mvfqspi->iobase + QUADSPI_BFGENCR);

	return 0;
}

static int __devinit mvfqspi_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct mvfqspi *mvfqspi;
	struct resource *res;
	struct spi_mvf_master *pdata;
	int status = 0;

	master = spi_alloc_master(&pdev->dev, sizeof(*mvfqspi));
	if (master == NULL) {
		dev_dbg(&pdev->dev, "spi_alloc_master failed\n");
		return -ENOMEM;
	}

	mvfqspi = spi_master_get_devdata(master);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_dbg(&pdev->dev, "platform_get_resource failed\n");
		status = -ENXIO;
		goto fail0;
	}

	if (!request_mem_region(res->start, resource_size(res), pdev->name)) {
		dev_dbg(&pdev->dev, "request_mem_region failed\n");
		status = -EBUSY;
		goto fail0;
	}

	mvfqspi->iobase = ioremap(res->start, resource_size(res));
	if (!mvfqspi->iobase) {
		dev_dbg(&pdev->dev, "ioremap failed\n");
		status = -ENOMEM;
		goto fail1;
	}

	mvfqspi->clk = clk_get(&pdev->dev, "mvf-qspi.0");
	if (IS_ERR(mvfqspi->clk)) {
		dev_dbg(&pdev->dev, "clk_get failed\n");
		status = PTR_ERR(mvfqspi->clk);
		goto fail3;
	}
	clk_enable(mvfqspi->clk);

	mvfqspi->workq =
		create_singlethread_workqueue(dev_name(master->dev.parent));
	if (!mvfqspi->workq) {
		dev_dbg(&pdev->dev, "create_workqueue failed\n");
		status = -ENOMEM;
		goto fail4;
	}

	INIT_WORK(&mvfqspi->work, mvfqspi_work);
	spin_lock_init(&mvfqspi->lock);
	INIT_LIST_HEAD(&mvfqspi->msgq);
	init_waitqueue_head(&mvfqspi->waitq);

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
		dev_dbg(&pdev->dev, "platform data is missing\n");
		goto fail5;
	}
	master->bus_num = pdata->bus_num;
	master->num_chipselect = pdata->num_chipselect;

	master->setup = mvfqspi_setup;
	master->transfer = mvfqspi_transfer;

	platform_set_drvdata(pdev, master);

	status = spi_register_master(master);
	if (status) {
		dev_dbg(&pdev->dev, "spi_register_master failed\n");
		goto fail6;
	}
	dev_info(&pdev->dev, "QSPI bus driver\n");

	return 0;

fail6:
fail5:
	destroy_workqueue(mvfqspi->workq);
fail4:
	clk_disable(mvfqspi->clk);
	clk_put(mvfqspi->clk);
fail3:
	free_irq(mvfqspi->irq, mvfqspi);
fail1:
	release_mem_region(res->start, resource_size(res));
fail0:
	spi_master_put(master);
	dev_dbg(&pdev->dev, "QSPI probe failed\n");
	return status;
}

static int mvfqspi_remove(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct mvfqspi *mvfqspi = spi_master_get_devdata(master);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	/* disable the hardware*/
	writel(0x0, mvfqspi->iobase + QUADSPI_MCR);

	platform_set_drvdata(pdev, NULL);
	destroy_workqueue(mvfqspi->workq);
	clk_disable(mvfqspi->clk);
	clk_put(mvfqspi->clk);
	free_irq(mvfqspi->irq, mvfqspi);
	iounmap(mvfqspi->iobase);
	release_mem_region(res->start, resource_size(res));
	spi_unregister_master(master);
	spi_master_put(master);

	return 0;
}

#ifdef CONFIG_PM
static int mvfqspi_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	struct mvfqspi *mvfqspi = platform_get_drvdata(pdev);

	clk_disable(mvfqspi->clk);
	return 0;
}

static int mvfqspi_resume(struct platform_device *pdev)
{
	struct mvfqspi *mvfqspi = platform_get_drvdata(pdev);

	clk_enable(mvfqspi->clk);
	return 0;
}
#else
#define	mvfqspi_suspend		NULL
#define mvfqspi_resume		NULL
#endif

static struct platform_driver mvfqspi_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.remove		= mvfqspi_remove,
	.suspend	= mvfqspi_suspend,
	.resume		= mvfqspi_resume,
};

static int __init mvfqspi_init(void)
{
	return platform_driver_probe(&mvfqspi_driver, mvfqspi_probe);
}
module_init(mvfqspi_init);

static void __exit mvfqspi_exit(void)
{
	platform_driver_unregister(&mvfqspi_driver);
}
module_exit(mvfqspi_exit);

MODULE_DESCRIPTION("QSPI Controller Driver");
MODULE_LICENSE("GPL");
