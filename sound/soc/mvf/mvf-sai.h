/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MVF_SAI_H
#define _MVF_SAI_H

#define SAI_TX_DIV		0

#define SAI_TCSR		0x00
#define SAI_TCSR_TE		(1 << 31)
#define SAI_TCSR_FWF		(1 << 17)
#define SAI_TCSR_FRIE		(1 << 8)
#define SAI_TCSR_FRDE		(1 << 0)

#define SAI_TCR1		0x04

#define SAI_TCR2		0x08
#define SAI_TCR2_SYNC		(1 << 30)
#define SAI_TCR2_MSEL_MASK	(0x3 << 26)
#define SAI_TCR2_MSEL_BUS	(0 << 26)
#define SAI_TCR2_MSEL_MCLK1	(1 << 26)
#define SAI_TCR2_MSEL_MCLK2	(2 << 26)
#define SAI_TCR2_MSEL_MCLK3	(3 << 26)
/* Bit clock is active low with driver outputs on
 * falling edge and sample inputs on rising edge */
#define SAI_TCR2_BCP		(1 << 25)
#define SAI_TCR2_BCD_MSTR	(1 << 24)
#define SAI_TCR2_DIV(x)		(x)
#define SAI_TCR2_DIV_MASK	0xff

#define SAI_TCR3		0x0c
#define SAI_TCR3_TCE		(1 << 16)
#define SAI_TCR3_WDFL(x)	(x)
#define SAI_TCR3_WDFL_MASK	0x1f

#define SAI_TCR4		0x10
#define SAI_TCR4_FRSZ(x)	(x << 16)
#define SAI_TCR4_FRSZ_MASK	(0x1f << 16)
#define SAI_TCR4_SYWD(x)	((x) << 8)
#define SAI_TCR4_SYWD_MASK	(0x1f << 8)
#define SAI_TCR4_MF		(1 << 4)
/* Frame sync is active low */
#define SAI_TCR4_FSE		(1 << 3)
#define SAI_TCR4_FSP		(1 << 1)
#define SAI_TCR4_FSD_MSTR	(1 << 0)

#define SAI_TCR5		0x14
#define SAI_TCR5_WNW(x)		((x) << 24)
#define SAI_TCR5_WNW_MASK	(0x1f << 24)
#define SAI_TCR5_W0W(x)		((x) << 16)
#define SAI_TCR5_W0W_MASK	(0x1f << 16)
#define SAI_TCR5_FBT(x)		((x) << 8)
#define SAI_TCR5_FBT_MASK	(0x1f << 8)

#define SAI_TDR			0x20

#define SAI_TFR			0x40

#define SAI_TMR			0x60


#define SAI_RCSR		0x80
#define SAI_RCSR_RE		(1 << 31)
#define SAI_RCSR_FWF		(1 << 17)
#define SAI_RCSR_FRIE		(1 << 8)
#define SAI_RCSR_FRDE		(1 << 0)

#define SAI_RCR1		0x84

#define SAI_RCR2		0x88
#define SAI_RCR2_MSEL_MASK	(0x3 << 26)
#define SAI_RCR2_MSEL_BUS	(0 << 26)
#define SAI_RCR2_MSEL_MCLK1	(1 << 26)
#define SAI_RCR2_MSEL_MCLK2	(2 << 26)
#define SAI_RCR2_MSEL_MCLK3	(3 << 26)
/* Bit clock is active low with driver outputs on
 * falling edge and sample inputs on rising edge */
#define SAI_RCR2_BCP		(1 << 25)
#define SAI_RCR2_BCD_MSTR	(1 << 24)
#define SAI_RCR2_DIV(x)		(x)
#define SAI_RCR2_DIV_MASK	0xff

#define SAI_RCR3		0x8c
#define SAI_RCR3_TCE		(1 << 16)
#define SAI_RCR3_WDFL(x)	(x)
#define SAI_RCR3_WDFL_MASK	0x1f

#define SAI_RCR4		0x90
/* Frame sync is active low */
#define SAI_RCR4_FRSZ(x)	(x << 16)
#define SAI_RCR4_FRSZ_MASK	(0x1f << 16)
#define SAI_RCR4_SYWD(x)	(x << 8)
#define SAI_RCR4_SYWD_MASK	(0x1f << 8)
#define SAI_RCR4_MF		(1 << 4)
/* Frame sync is active low */
#define SAI_RCR4_FSE		(1 << 3)
#define SAI_RCR4_FSP		(1 << 1)
#define SAI_RCR4_FSD_MSTR	(1 << 0)

#define SAI_RCR5		0x94
#define SAI_RCR5_WNW(x)		(x << 24)
#define SAI_RCR5_WNW_MASK	(0x1f << 24)
#define SAI_RCR5_W0W(x)		(x << 16)
#define SAI_RCR5_W0W_MASK	(0x1f << 16)

#define SAI_RDR			0xa0

#define SAI_RFR			0xc0

#define SAI_RMR			0xe0

/* SAI clock sources */
#define MVF_SAI_BUS_CLK		0
#define MVF_SAI_MAST_CLK1	1
#define MVF_SAI_MAST_CLK2	2
#define MVF_SAI_MAST_CLK3	3

/* SAI audio dividers */
#define MVF_SAI_TX_DIV		0
#define MVF_SAI_RX_DIV		1

#define DRV_NAME "mvf-sai"

#include <linux/dmaengine.h>
#include <mach/dma.h>

struct mvf_sai {
	struct clk *clk;
	void __iomem *base;
	int irq;
	int fiq_enable;
	unsigned int offset;

	unsigned int flags;

	struct imx_pcm_dma_params	dma_params_rx;
	struct imx_pcm_dma_params	dma_params_tx;

	int play_enabled;
	int cap_enabled;

	struct platform_device *soc_platform_pdev;
	struct platform_device *soc_platform_pdev_fiq;
};

int snd_mvf_pcm_mmap(struct snd_pcm_substream *substream,
			struct vm_area_struct *vma);
int mvf_pcm_new(struct snd_card *card, struct snd_soc_dai *dai,
	struct snd_pcm *pcm);
void mvf_pcm_free(struct snd_pcm *pcm);

#define MVF_SAI_DMABUF_SIZE	(32 * 1024)
#define TCD_NUMBER		4

#endif /* _MVF_SAI_H */
