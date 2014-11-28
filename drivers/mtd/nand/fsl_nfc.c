/*
 * Copyright 2009-2012 Freescale Semiconductor, Inc.
 *
 * Description: MPC5125, VF610, M54418 Nand driver.
 * Jason ported to M54418TWR and MVFA5.
 * Authors: Shaohui Xie <b21989@freescale.com>
 *          Jason Jin <Jason.jin@freescale.com>
 *
 * Based on original driver mpc5121_nfc.c.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Limitations:
 * - Untested on MPC5125 and M54418.
 * - DMA not used.
 * - 2K pages or less.
 * - Only 2K page w. 64+OOB and hardware ECC.
 */

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of_mtd.h>

#define	DRV_NAME		"fsl_nfc"

/* Register Offsets */
#define NFC_FLASH_CMD1			0x3F00
#define NFC_FLASH_CMD2			0x3F04
#define NFC_COL_ADDR			0x3F08
#define NFC_ROW_ADDR			0x3F0c
#define NFC_FLASH_COMMAND_REPEAT	0x3F10
#define NFC_ROW_ADDR_INC		0x3F14
#define NFC_FLASH_STATUS1		0x3F18
#define NFC_FLASH_STATUS2		0x3F1c
#define NFC_CACHE_SWAP			0x3F28
#define NFC_SECTOR_SIZE			0x3F2c
#define NFC_FLASH_CONFIG		0x3F30
#define NFC_IRQ_STATUS			0x3F38

/* Addresses for NFC MAIN RAM BUFFER areas */
#define NFC_MAIN_AREA(n)		((n) *  0x1000)

#define PAGE_2K				0x0800
#define OOB_64				0x0040

/* NFC_CMD2[CODE] values. See section:
    - 31.4.7 Flash Command Code Description, Vybrid manual
    - 23.8.6 Flash Command Sequencer, MPC5125 manual

    Briefly these are bitmasks of controller cycles.
*/
#define READ_PAGE_CMD_CODE		0x7EE0
#define PROGRAM_PAGE_CMD_CODE		0x7FC0
#define ERASE_CMD_CODE			0x4EC0
#define READ_ID_CMD_CODE		0x4804
#define RESET_CMD_CODE			0x4040
#define STATUS_READ_CMD_CODE		0x4068

/* NFC ECC mode define */
#define ECC_BYPASS			0
#define ECC_8_BYTE			1
#define ECC_45_BYTE			6

/*** Register Mask and bit definitions */

/* NFC_FLASH_CMD1 Field */
#define CMD_BYTE2_MASK				0xFF000000
#define CMD_BYTE2_SHIFT				24

/* NFC_FLASH_CM2 Field */
#define CMD_BYTE1_MASK				0xFF000000
#define CMD_BYTE1_SHIFT				24
#define CMD_CODE_MASK				0x00FFFF00
#define CMD_CODE_SHIFT				8
#define BUFNO_MASK				0x00000006
#define BUFNO_SHIFT				1
#define START_BIT				(1<<0)

/* NFC_COL_ADDR Field */
#define COL_ADDR_MASK				0x0000FFFF
#define COL_ADDR_SHIFT				0

/* NFC_ROW_ADDR Field */
#define ROW_ADDR_MASK				0x00FFFFFF
#define ROW_ADDR_SHIFT				0
#define ROW_ADDR_CHIP_SEL_RB_MASK		0xF0000000
#define ROW_ADDR_CHIP_SEL_RB_SHIFT		28
#define ROW_ADDR_CHIP_SEL_MASK			0x0F000000
#define ROW_ADDR_CHIP_SEL_SHIFT			24

/* NFC_FLASH_STATUS2 Field */
#define STATUS_BYTE1_MASK			0x000000FF

/* NFC_FLASH_CONFIG Field */
#define CONFIG_ECC_SRAM_ADDR_MASK		0x7FC00000
#define CONFIG_ECC_SRAM_ADDR_SHIFT		22
#define CONFIG_ECC_SRAM_REQ_BIT			(1<<21)
#define CONFIG_DMA_REQ_BIT			(1<<20)
#define CONFIG_ECC_MODE_MASK			0x000E0000
#define CONFIG_ECC_MODE_SHIFT			17
#define CONFIG_FAST_FLASH_BIT			(1<<16)
#define CONFIG_16BIT				(1<<7)
#define CONFIG_BOOT_MODE_BIT			(1<<6)
#define CONFIG_ADDR_AUTO_INCR_BIT		(1<<5)
#define CONFIG_BUFNO_AUTO_INCR_BIT		(1<<4)
#define CONFIG_PAGE_CNT_MASK			0xF
#define CONFIG_PAGE_CNT_SHIFT			0

/* NFC_IRQ_STATUS Field */
#define IDLE_IRQ_BIT				(1<<29)
#define IDLE_EN_BIT				(1<<20)
#define CMD_DONE_CLEAR_BIT			(1<<18)
#define IDLE_CLEAR_BIT				(1<<17)

#define NFC_TIMEOUT		(HZ)

#define ECC_SRAM_ADDR	((PAGE_2K+256-8) >> 3)
#define ECC_STATUS_MASK	0x80
#define ECC_ERR_COUNT	0x3F

struct nfc_config {
	int hardware_ecc;
	int width;
	int flash_bbt;
	u32 clkrate;
};

struct fsl_nfc {
	struct mtd_info	   mtd;
	struct nand_chip   chip;
	struct device	  *dev;
	void __iomem	  *regs;
	wait_queue_head_t  irq_waitq;
	uint               column;
	int                spareonly;
	int                page;
	/* Status and ID are in alternate locations. */
	int                alt_buf;
#define ALT_BUF_ID   1
#define ALT_BUF_STAT 2
	struct clk        *clk;

	struct nfc_config *cfg;
};
#define mtd_to_nfc(_mtd) container_of(_mtd, struct fsl_nfc, mtd)

static u8 bbt_pattern[] = {'B', 'b', 't', '0' };
static u8 mirror_pattern[] = {'1', 't', 'b', 'B' };

static struct nand_bbt_descr bbt_main_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE |
		   NAND_BBT_2BIT | NAND_BBT_VERSION,
	.offs =	11,
	.len = 4,
	.veroffs = 15,
	.maxblocks = 4,
	.pattern = bbt_pattern,
};

static struct nand_bbt_descr bbt_mirror_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE |
		   NAND_BBT_2BIT | NAND_BBT_VERSION,
	.offs =	11,
	.len = 4,
	.veroffs = 15,
	.maxblocks = 4,
	.pattern = mirror_pattern,
};

static struct nand_ecclayout nfc_ecc45 = {
	.eccbytes = 45,
	.eccpos = {19, 20, 21, 22, 23,
		   24, 25, 26, 27, 28, 29, 30, 31,
		   32, 33, 34, 35, 36, 37, 38, 39,
		   40, 41, 42, 43, 44, 45, 46, 47,
		   48, 49, 50, 51, 52, 53, 54, 55,
		   56, 57, 58, 59, 60, 61, 62, 63},
	.oobfree = {
		{.offset = 8,
		 .length = 11} }
};

static u32 nfc_read(struct mtd_info *mtd, uint reg)
{
	struct fsl_nfc *nfc = mtd_to_nfc(mtd);
	if (reg == NFC_FLASH_STATUS1 ||
	    reg == NFC_FLASH_STATUS2 ||
	    reg == NFC_IRQ_STATUS)
		return __raw_readl(nfc->regs + reg);
	/* Gang read/writes together for most registers. */
	else
		return *(u32 *)(nfc->regs + reg);
}

static void nfc_write(struct mtd_info *mtd, uint reg, u32 val)
{
	struct fsl_nfc *nfc = mtd_to_nfc(mtd);

	if (reg == NFC_FLASH_STATUS1 ||
	    reg == NFC_FLASH_STATUS2 ||
	    reg == NFC_IRQ_STATUS)
		__raw_writel(val, nfc->regs + reg);
	/* Gang read/writes together for most registers. */
	else
		*(u32 *)(nfc->regs + reg) = val;
}

static void nfc_set(struct mtd_info *mtd, uint reg, u32 bits)
{
	nfc_write(mtd, reg, nfc_read(mtd, reg) | bits);
}

static void nfc_clear(struct mtd_info *mtd, uint reg, u32 bits)
{
	nfc_write(mtd, reg, nfc_read(mtd, reg) & ~bits);
}

static void nfc_set_field(struct mtd_info *mtd, u32 reg,
			  u32 mask, u32 shift, u32 val)
{
	nfc_write(mtd, reg, (nfc_read(mtd, reg) & (~mask)) | val << shift);
}

/* Clear flags for upcoming command */
static void nfc_clear_status(struct mtd_info *mtd)
{
	nfc_set(mtd, NFC_IRQ_STATUS, CMD_DONE_CLEAR_BIT);
	nfc_set(mtd, NFC_IRQ_STATUS, IDLE_CLEAR_BIT);
}

/* Wait for complete operation */
static void nfc_done(struct mtd_info *mtd)
{
	struct fsl_nfc *nfc = mtd_to_nfc(mtd);
	int rv;

	nfc_set(mtd, NFC_IRQ_STATUS, IDLE_EN_BIT);
	/* CMD2 is almost non-volatile, START_BIT is not */
	barrier();
	nfc_set(mtd, NFC_FLASH_CMD2, START_BIT);
	barrier();

	if (!(nfc_read(mtd, NFC_IRQ_STATUS) & IDLE_IRQ_BIT)) {
		rv = wait_event_timeout(nfc->irq_waitq,
			(nfc_read(mtd, NFC_IRQ_STATUS) & IDLE_IRQ_BIT),
					NFC_TIMEOUT);
		if (!rv)
			dev_warn(nfc->dev, "Timeout while waiting for BUSY.\n");
	}
	nfc_clear_status(mtd);
}

static u8 nfc_get_id(struct mtd_info *mtd, int col)
{
	u32 flash_id;

	if (col < 4) {
		flash_id = nfc_read(mtd, NFC_FLASH_STATUS1);
		return (flash_id >> (3-col)*8) & 0xff;
	} else {
		flash_id = nfc_read(mtd, NFC_FLASH_STATUS2);
		return flash_id >> 24;
	}
}

static u8 nfc_get_status(struct mtd_info *mtd)
{
	return nfc_read(mtd, NFC_FLASH_STATUS2) & STATUS_BYTE1_MASK;
}

/* Single command */
static void nfc_send_command(struct mtd_info *mtd, u32 cmd_byte1,
			     u32 cmd_code)
{
	nfc_clear_status(mtd);

	nfc_set_field(mtd, NFC_FLASH_CMD2, CMD_BYTE1_MASK,
			CMD_BYTE1_SHIFT, cmd_byte1);

	nfc_clear(mtd, NFC_FLASH_CMD2, BUFNO_MASK);

	nfc_set_field(mtd, NFC_FLASH_CMD2, CMD_CODE_MASK,
			CMD_CODE_SHIFT, cmd_code);
}

/* Two commands */
static void nfc_send_commands(struct mtd_info *mtd, u32 cmd_byte1,
			      u32 cmd_byte2, u32 cmd_code)
{
	nfc_send_command(mtd, cmd_byte1, cmd_code);

	nfc_set_field(mtd, NFC_FLASH_CMD1, CMD_BYTE2_MASK,
			CMD_BYTE2_SHIFT, cmd_byte2);
}

static irqreturn_t nfc_irq(int irq, void *data)
{
	struct mtd_info *mtd = data;
	struct fsl_nfc *nfc = mtd_to_nfc(mtd);

	nfc_clear(mtd, NFC_IRQ_STATUS, IDLE_EN_BIT);
	wake_up(&nfc->irq_waitq);

	return IRQ_HANDLED;
}

static void nfc_addr_cycle(struct mtd_info *mtd, int column, int page)
{
	if (column != -1) {
		struct fsl_nfc *nfc = mtd_to_nfc(mtd);
		if (nfc->chip.options | NAND_BUSWIDTH_16)
			column = column/2;
		nfc_set_field(mtd, NFC_COL_ADDR, COL_ADDR_MASK,
			      COL_ADDR_SHIFT, column);
	}
	if (page != -1)
		nfc_set_field(mtd, NFC_ROW_ADDR, ROW_ADDR_MASK,
				ROW_ADDR_SHIFT, page);
}

/* Write command to NAND flash */
static void nfc_command(struct mtd_info *mtd, unsigned command,
			int column, int page)
{
	struct fsl_nfc *nfc = mtd_to_nfc(mtd);

	nfc->column     = max(column, 0);
	nfc->spareonly	= 0;
	nfc->alt_buf	= 0;

	switch (command) {
	case NAND_CMD_PAGEPROG:
		nfc->page = -1;
		nfc_send_commands(mtd, NAND_CMD_SEQIN,
			     command, PROGRAM_PAGE_CMD_CODE);
		nfc_addr_cycle(mtd, column, page);
		break;

	case NAND_CMD_RESET:
		nfc_send_command(mtd, command, RESET_CMD_CODE);
		break;
	/*
	 * NFC does not support sub-page reads and writes,
	 * so emulate them using full page transfers.
	 */
	case NAND_CMD_READOOB:
		nfc->spareonly = 1;
	case NAND_CMD_SEQIN: /* Pre-read for partial writes. */
	case NAND_CMD_READ0:
		column = 0;
		/* Already read? */
		if (nfc->page == page)
			return;
		nfc->page = page;
		nfc_send_commands(mtd, NAND_CMD_READ0,
				  NAND_CMD_READSTART, READ_PAGE_CMD_CODE);
		nfc_addr_cycle(mtd, column, page);
		break;

	case NAND_CMD_ERASE1:
		if (nfc->page == page)
			nfc->page = -1;
		nfc_send_commands(mtd, command,
				  NAND_CMD_ERASE2, ERASE_CMD_CODE);
		nfc_addr_cycle(mtd, column, page);
		break;

	case NAND_CMD_READID:
		nfc->alt_buf = ALT_BUF_ID;
		nfc_send_command(mtd, command, READ_ID_CMD_CODE);
		break;

	case NAND_CMD_STATUS:
		nfc->alt_buf = ALT_BUF_STAT;
		nfc_send_command(mtd, command, STATUS_READ_CMD_CODE);
		break;
	default:
		return;
	}

	nfc_done(mtd);
}

static void nfc_read_spare(struct mtd_info *mtd, void *buf, int len)
{
	struct fsl_nfc *nfc = mtd_to_nfc(mtd);

	len = min(mtd->oobsize, (uint)len);
	if (len > 0)
		memcpy(buf, nfc->regs + mtd->writesize, len);
}

/* Read data from NFC buffers */
static void nfc_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	struct fsl_nfc *nfc = mtd_to_nfc(mtd);
	uint c = nfc->column;
	uint l;

	/* Handle main area */
	if (!nfc->spareonly) {

		/*
		 * Handle main area access - limit copy length to prevent
		 * crossing main/spare boundary.
		 */
		l = min((uint)len, mtd->writesize - c);
		nfc->column += l;

		if (!nfc->alt_buf)
			memcpy(buf, nfc->regs + NFC_MAIN_AREA(0) + c, l);
		else
			if (nfc->alt_buf & ALT_BUF_ID)
				*buf = nfc_get_id(mtd, c);
			else
				*buf = nfc_get_status(mtd);
		buf += l;
		len -= l;
	}
	/* Handle spare area access */
	if (len) {
		nfc->column += len;
		nfc_read_spare(mtd, buf, len);
	}
}

/* Write data to NFC buffers */
static void nfc_write_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	struct fsl_nfc *nfc = mtd_to_nfc(mtd);
	uint c = nfc->column;
	uint l;

	l = min((uint)len, mtd->writesize + mtd->oobsize - c);
	nfc->column += l;

	memcpy(nfc->regs + NFC_MAIN_AREA(0) + c, buf, l);
}

/* Read byte from NFC buffers */
static u8 nfc_read_byte(struct mtd_info *mtd)
{
	u8 tmp;
	nfc_read_buf(mtd, &tmp, sizeof(tmp));
	return tmp;
}

/* Read word from NFC buffers */
static u16 nfc_read_word(struct mtd_info *mtd)
{
	u16 tmp;
	nfc_read_buf(mtd, (u_char *)&tmp, sizeof(tmp));
	return tmp;
}

/* If not provided, upper layers apply a fixed delay. */
static int nfc_dev_ready(struct mtd_info *mtd)
{
	/* NFC handles R/B internally; always ready.  */
	return 1;
}

/* Vybrid only.  MPC5125 has full RB and four CS. Assume boot loader
 * has set this register for now.
 */
static void
nfc_select_chip(struct mtd_info *mtd, int chip)
{
#ifdef CONFIG_SOC_VF610
	nfc_set_field(mtd, NFC_ROW_ADDR, ROW_ADDR_CHIP_SEL_RB_MASK,
		      ROW_ADDR_CHIP_SEL_RB_SHIFT, 1);

	if (chip == 0)
		nfc_set_field(mtd, NFC_ROW_ADDR, ROW_ADDR_CHIP_SEL_MASK,
			      ROW_ADDR_CHIP_SEL_SHIFT, 1);
	else if (chip == 1)
		nfc_set_field(mtd, NFC_ROW_ADDR, ROW_ADDR_CHIP_SEL_MASK,
			      ROW_ADDR_CHIP_SEL_SHIFT, 2);
	else
		nfc_clear(mtd, NFC_ROW_ADDR, ROW_ADDR_CHIP_SEL_MASK);
#endif
}

/* Count the number of 0's in buff upto max_bits */
static int count_written_bits(uint8_t *buff, int size, int max_bits)
{
	int k, written_bits = 0;

	for (k = 0; k < size; k++) {
		written_bits += hweight8(~buff[k]);
		if (written_bits > max_bits)
			break;
	}

	return written_bits;
}

static int nfc_correct_data(struct mtd_info *mtd, u_char *dat,
				 u_char *read_ecc, u_char *calc_ecc)
{
	struct fsl_nfc *nfc = mtd_to_nfc(mtd);
	u32 ecc_status;
	u8 ecc_count;
	int flip;

	/* Errata: ECC status is stored at NFC_CFG[ECCADD] +4 for
	   little-endian and +7 for big-endian SOC.  Access as 32 bits
	   and use low byte.
	*/
	ecc_status = __raw_readl(nfc->regs + ECC_SRAM_ADDR * 8 + 4);
	ecc_count = ecc_status & ECC_ERR_COUNT;
	if (!(ecc_status & ECC_STATUS_MASK))
		return ecc_count;

	/* If 'ecc_count' zeros or less then buffer is all 0xff or erased. */
	flip = count_written_bits(dat, nfc->chip.ecc.size, ecc_count);

	/* ECC failed. */
	if (flip > ecc_count)
		return -1;

	/* Erased page. */
	memset(dat, 0xff, nfc->chip.ecc.size);
	return 0;
}

static int nfc_calculate_ecc(struct mtd_info *mtd, const u_char *dat,
				  u_char *ecc_code)
{
	return 0;
}

static void nfc_enable_hwecc(struct mtd_info *mtd, int mode)
{
}

#ifdef CONFIG_OF_MTD
static struct of_device_id nfc_dt_ids[] = {
	{ .compatible = "fsl,vf610-nfc" },
	{ .compatible = "fsl,mpc5125-nfc" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, nfc_dt_ids);

static int __init nfc_probe_dt(struct device *dev, struct nfc_config *cfg)
{
	struct device_node *np = dev->of_node;
	int buswidth;
	u32 clkrate;

	if (!np)
		return 1;

	if (of_get_nand_ecc_mode(np) >= NAND_ECC_HW)
		cfg->hardware_ecc = 1;

	cfg->flash_bbt = of_get_nand_on_flash_bbt(np);

	if (!of_property_read_u32(np, "clock-frequency", &clkrate))
		cfg->clkrate = clkrate;

	buswidth = of_get_nand_bus_width(np);
	if (buswidth < 0)
		return buswidth;

	cfg->width = buswidth;

	return 0;
}
#else
static int __init nfc_probe_dt(struct device *dev, struct nfc_config *cfg)
{
	cfg.flash_bbt = 1;
	return 1;
}
#endif

static int nfc_init_controller(struct mtd_info *mtd, struct nfc_config *cfg, int page_sz)
{
	if (cfg->width == 16)
		nfc_set(mtd, NFC_FLASH_CONFIG, CONFIG_16BIT);
	else
		nfc_clear(mtd, NFC_FLASH_CONFIG, CONFIG_16BIT);

	/* Set configuration register. */
	nfc_clear(mtd, NFC_FLASH_CONFIG, CONFIG_ADDR_AUTO_INCR_BIT);
	nfc_clear(mtd, NFC_FLASH_CONFIG, CONFIG_BUFNO_AUTO_INCR_BIT);
	nfc_clear(mtd, NFC_FLASH_CONFIG, CONFIG_BOOT_MODE_BIT);
	nfc_clear(mtd, NFC_FLASH_CONFIG, CONFIG_DMA_REQ_BIT);
	nfc_set(mtd, NFC_FLASH_CONFIG, CONFIG_FAST_FLASH_BIT);

	/* PAGE_CNT = 1 */
	nfc_set_field(mtd, NFC_FLASH_CONFIG, CONFIG_PAGE_CNT_MASK,
			CONFIG_PAGE_CNT_SHIFT, 1);

	/* Set ECC_STATUS offset */
	nfc_set_field(mtd, NFC_FLASH_CONFIG,
		      CONFIG_ECC_SRAM_ADDR_MASK,
		      CONFIG_ECC_SRAM_ADDR_SHIFT, ECC_SRAM_ADDR);

	nfc_write(mtd, NFC_SECTOR_SIZE, page_sz);

	if (cfg->hardware_ecc) {
		/* set ECC mode to 45 bytes OOB with 24 bits correction */
		nfc_set_field(mtd, NFC_FLASH_CONFIG,
				CONFIG_ECC_MODE_MASK,
				CONFIG_ECC_MODE_SHIFT, ECC_45_BYTE);

		/* Enable ECC_STATUS */
		nfc_set(mtd, NFC_FLASH_CONFIG, CONFIG_ECC_SRAM_REQ_BIT);
	}

	return 0;
}

static int nfc_probe(struct platform_device *pdev)
{
	struct fsl_nfc *nfc;
	struct resource *res;
	struct mtd_info *mtd;
	struct nand_chip *chip;
	struct nfc_config *cfg;
	int err = 0;
	int page_sz;
	int irq;

	nfc = devm_kzalloc(&pdev->dev, sizeof(*nfc), GFP_KERNEL);
	if (!nfc)
		return -ENOMEM;

	nfc->cfg = devm_kzalloc(&pdev->dev, sizeof(*nfc), GFP_KERNEL);
	if (!nfc->cfg)
		return -ENOMEM;
	cfg = nfc->cfg;

	nfc->dev = &pdev->dev;
	nfc->page = -1;
	mtd = &nfc->mtd;
	chip = &nfc->chip;

	mtd->priv = chip;
	mtd->owner = THIS_MODULE;
	mtd->dev.parent = nfc->dev;
	mtd->name = DRV_NAME;

	err = nfc_probe_dt(nfc->dev, cfg);
	if (err)
		return -ENODEV;

	irq = platform_get_irq(pdev, 0);
	if (irq <= 0)
		return -EINVAL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	nfc->regs = devm_ioremap_resource(nfc->dev, res);
	if (IS_ERR(nfc->regs))
		return PTR_ERR(nfc->regs);

	nfc->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(nfc->clk))
		return PTR_ERR(nfc->clk);

	if (cfg->clkrate && clk_set_rate(nfc->clk, cfg->clkrate)) {
		dev_err(nfc->dev, "Clock rate not set.");
		return -EINVAL;
	}

	err = clk_prepare_enable(nfc->clk);
	if (err) {
		dev_err(nfc->dev, "Unable to enable clock!\n");
		return err;
	}

	if (cfg->width == 16)
		chip->options |= NAND_BUSWIDTH_16;
	else
		chip->options &= ~NAND_BUSWIDTH_16;

	chip->dev_ready = nfc_dev_ready;
	chip->cmdfunc = nfc_command;
	chip->read_byte = nfc_read_byte;
	chip->read_word = nfc_read_word;
	chip->read_buf = nfc_read_buf;
	chip->write_buf = nfc_write_buf;
	chip->select_chip = nfc_select_chip;

	/* Bad block options. */
	if (cfg->flash_bbt)
		chip->bbt_options = NAND_BBT_USE_FLASH | NAND_BBT_CREATE;

	chip->bbt_td = &bbt_main_descr;
	chip->bbt_md = &bbt_mirror_descr;

	init_waitqueue_head(&nfc->irq_waitq);

	err = devm_request_irq(nfc->dev, irq, nfc_irq, 0, DRV_NAME, mtd);
	if (err) {
		dev_err(nfc->dev, "Error requesting IRQ!\n");
		goto error;
	}

	page_sz = PAGE_2K + OOB_64;
	page_sz += cfg->width == 16 ? 1 : 0;

	nfc_init_controller(mtd, cfg, page_sz);

	/* Always use software ECC for flash ID. */
	nfc_set_field(mtd, NFC_FLASH_CONFIG,
		      CONFIG_ECC_MODE_MASK,
		      CONFIG_ECC_MODE_SHIFT, ECC_BYPASS);

	/* first scan to find the device and get the page size */
	if (nand_scan_ident(mtd, 1, NULL)) {
		err = -ENXIO;
		goto error;
	}

	chip->ecc.mode = NAND_ECC_SOFT; /* default */

	page_sz = mtd->writesize + mtd->oobsize;

	/* Single buffer only, max 256 OOB minus ECC status */
	if (page_sz > PAGE_2K + 256 - 8) {
		dev_err(nfc->dev, "Unsupported flash size\n");
		err = -ENXIO;
		goto error;
	}
	page_sz += cfg->width == 16 ? 1 : 0;
	nfc_write(mtd, NFC_SECTOR_SIZE, page_sz);

	if (cfg->hardware_ecc) {
		if (mtd->writesize != PAGE_2K && mtd->oobsize < 64) {
			dev_err(nfc->dev, "Unsupported flash with hwecc\n");
			err = -ENXIO;
			goto error;
		}

		chip->ecc.layout = &nfc_ecc45;

		/* propagate ecc.layout to mtd_info */
		mtd->ecclayout = chip->ecc.layout;
		chip->ecc.calculate = nfc_calculate_ecc;
		chip->ecc.hwctl = nfc_enable_hwecc;
		chip->ecc.correct = nfc_correct_data;
		chip->ecc.mode = NAND_ECC_HW;

		chip->ecc.bytes = 45;
		chip->ecc.size = PAGE_2K;
		chip->ecc.strength = 24;

		/* set ECC mode to 45 bytes OOB with 24 bits correction */
		nfc_set_field(mtd, NFC_FLASH_CONFIG,
				CONFIG_ECC_MODE_MASK,
				CONFIG_ECC_MODE_SHIFT, ECC_45_BYTE);
	}

	/* second phase scan */
	if (nand_scan_tail(mtd)) {
		err = -ENXIO;
		goto error;
	}

	/* Register device in MTD */
	mtd_device_parse_register(mtd, NULL,
		&(struct mtd_part_parser_data){
			.of_node = pdev->dev.of_node,
		},
		NULL, 0);

	platform_set_drvdata(pdev, mtd);

	return 0;

error:
	clk_disable_unprepare(nfc->clk);
	return err;
}



static int nfc_remove(struct platform_device *pdev)
{
	struct mtd_info *mtd = platform_get_drvdata(pdev);
	struct fsl_nfc *nfc = mtd_to_nfc(mtd);

	nand_release(mtd);
	clk_disable_unprepare(nfc->clk);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int nfc_suspend(struct device *dev)
{
	struct mtd_info *mtd = dev_get_drvdata(dev);
	struct fsl_nfc *nfc = mtd_to_nfc(mtd);

	clk_disable_unprepare(nfc->clk);
	return 0;
}

static int nfc_resume(struct device *dev)
{
	struct mtd_info *mtd = dev_get_drvdata(dev);
	struct fsl_nfc *nfc = mtd_to_nfc(mtd);
	int page_sz;

	pinctrl_pm_select_default_state(dev);
	if (nfc->cfg->clkrate && clk_set_rate(nfc->clk, nfc->cfg->clkrate)) {
		dev_err(nfc->dev, "Clock rate not set.");
		return -EINVAL;
	}

	clk_prepare_enable(nfc->clk);

	page_sz = mtd->writesize + mtd->oobsize;
	page_sz += nfc->cfg->width == 16 ? 1 : 0;

	nfc_init_controller(mtd, nfc->cfg, page_sz);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(nfc_pm_ops, nfc_suspend, nfc_resume);

static struct platform_driver nfc_driver = {
	.driver		= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = nfc_dt_ids,
		.pm	= &nfc_pm_ops,
	},
	.probe		= nfc_probe,
	.remove		= nfc_remove,
};

module_platform_driver(nfc_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("FSL NFC MTD driver");
MODULE_LICENSE("GPL");
