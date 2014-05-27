/*
 * Freescale SSI ALSA SoC Digital Audio Interface (DAI) driver
 *
 * Author: Timur Tabi <timur@freescale.com>
 *
 * Copyright 2007-2010 Freescale Semiconductor, Inc.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 *
 * Some notes why imx-pcm-fiq is used instead of DMA on some boards:
 *
 * The i.MX SSI core has some nasty limitations in AC97 mode. While most
 * sane processor vendors have a FIFO per AC97 slot, the i.MX has only
 * one FIFO which combines all valid receive slots. We cannot even select
 * which slots we want to receive. The WM9712 with which this driver
 * was developed with always sends GPIO status data in slot 12 which
 * we receive in our (PCM-) data stream. The only chance we have is to
 * manually skip this data in the FIQ handler. With sampling rates different
 * from 48000Hz not every frame has valid receive data, so the ratio
 * between pcm data and GPIO status data changes. Our FIQ handler is not
 * able to handle this, hence this driver only works with 48000Hz sampling
 * rate.
 * Reading and writing AC97 registers is another challenge. The core
 * provides us status bits when the read register is updated with *another*
 * value. When we read the same register two times (and the register still
 * contains the same value) these status bits are not set. We work
 * around this by not polling these bits but only wait a fixed delay.
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/dmaengine_pcm.h>

#include "fsl_ssi.h"
#include "imx-pcm.h"

/**
 * FSLSSI_I2S_RATES: sample rates supported by the I2S
 *
 * This driver currently only supports the SSI running in I2S slave mode,
 * which means the codec determines the sample rate.  Therefore, we tell
 * ALSA that we support all rates and let the codec driver decide what rates
 * are really supported.
 */
#define FSLSSI_I2S_RATES (SNDRV_PCM_RATE_5512 | SNDRV_PCM_RATE_8000_192000 | \
			  SNDRV_PCM_RATE_CONTINUOUS)

/**
 * FSLSSI_I2S_FORMATS: audio formats supported by the SSI
 *
 * This driver currently only supports the SSI running in I2S slave mode.
 *
 * The SSI has a limitation in that the samples must be in the same byte
 * order as the host CPU.  This is because when multiple bytes are written
 * to the STX register, the bytes and bits must be written in the same
 * order.  The STX is a shift register, so all the bits need to be aligned
 * (bit-endianness must match byte-endianness).  Processors typically write
 * the bits within a byte in the same order that the bytes of a word are
 * written in.  So if the host CPU is big-endian, then only big-endian
 * samples will be written to STX properly.
 */
#ifdef __BIG_ENDIAN
#define FSLSSI_I2S_FORMATS (SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_BE | \
	 SNDRV_PCM_FMTBIT_S18_3BE | SNDRV_PCM_FMTBIT_S20_3BE | \
	 SNDRV_PCM_FMTBIT_S24_3BE | SNDRV_PCM_FMTBIT_S24_BE)
#else
#define FSLSSI_I2S_FORMATS (SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE | \
	 SNDRV_PCM_FMTBIT_S18_3LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	 SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S24_LE)
#endif

/* SIER bitflag of interrupts to enable */
#define FSLSSI_SIER_DBG_RX_FLAGS (CCSR_SSI_SIER_RFF0_EN | \
		CCSR_SSI_SIER_RLS_EN | CCSR_SSI_SIER_RFS_EN | \
		CCSR_SSI_SIER_ROE0_EN | CCSR_SSI_SIER_RFRC_EN)
#define FSLSSI_SIER_DBG_TX_FLAGS (CCSR_SSI_SIER_TFE0_EN | \
		CCSR_SSI_SIER_TLS_EN | CCSR_SSI_SIER_TFS_EN | \
		CCSR_SSI_SIER_TUE0_EN | CCSR_SSI_SIER_TFRC_EN)

enum fsl_ssi_type {
	FSL_SSI_MCP8610,
	FSL_SSI_MX21,
	FSL_SSI_MX35,
	FSL_SSI_MX51,
};

struct fsl_ssi_reg_val {
	u32 sier;
	u32 srcr;
	u32 stcr;
	u32 scr;
};

struct fsl_ssi_rxtx_reg_val {
	struct fsl_ssi_reg_val rx;
	struct fsl_ssi_reg_val tx;
};
static const struct regmap_config fsl_ssi_regconfig = {
	.max_register = CCSR_SSI_SACCDIS,
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};

struct fsl_ssi_soc_data {
	bool imx;
	bool offline_config;
	u32 sisr_write_mask;
};

/**
 * fsl_ssi_private: per-SSI private data
 *
<<<<<<< HEAD
 * @ssi: pointer to the SSI's registers
=======
 * @reg: Pointer to the regmap registers
 * @irq: IRQ of this SSI
 * @cpu_dai_drv: CPU DAI driver for this device
 *
 * @dai_fmt: DAI configuration this device is currently used with
 * @i2s_mode: i2s and network mode configuration of the device. Is used to
 * switch between normal and i2s/network mode
 * mode depending on the number of channels
 * @use_dma: DMA is used or FIQ with stream filter
 * @use_dual_fifo: DMA with support for both FIFOs used
 * @fifo_deph: Depth of the SSI FIFOs
 * @rxtx_reg_val: Specific register settings for receive/transmit configuration
 *
 * @clk: SSI clock
 * @baudclk: SSI baud clock for master mode
 * @baudclk_streams: Active streams that are using baudclk
 * @bitclk_freq: bitclock frequency set by .set_dai_sysclk
 *
 * @dma_params_tx: DMA transmit parameters
 * @dma_params_rx: DMA receive parameters
>>>>>>> ASoC: fsl-ssi: Use regmap
 * @ssi_phys: physical address of the SSI registers
 * @irq: IRQ of this SSI
 * @first_stream: pointer to the stream that was opened first
 * @second_stream: pointer to second stream
 * @playback: the number of playback streams opened
 * @capture: the number of capture streams opened
 * @cpu_dai: the CPU DAI for this device
 * @dev_attr: the sysfs device attribute structure
 * @stats: SSI statistics
 * @name: name for this device
 */
struct fsl_ssi_private {
<<<<<<< HEAD
	struct ccsr_ssi __iomem *ssi;
	dma_addr_t ssi_phys;
=======
	struct regmap *regs;
>>>>>>> ASoC: fsl-ssi: Use regmap
	unsigned int irq;
	struct snd_pcm_substream *first_stream;
	struct snd_pcm_substream *second_stream;
	unsigned int fifo_depth;
	struct snd_soc_dai_driver cpu_dai_drv;
	struct device_attribute dev_attr;
	struct platform_device *pdev;

	bool new_binding;
	bool ssi_on_imx;
	bool imx_ac97;
	bool use_dma;
	struct clk *clk;
	struct snd_dmaengine_dai_dma_data dma_params_tx;
	struct snd_dmaengine_dai_dma_data dma_params_rx;
	struct imx_dma_data filter_data_tx;
	struct imx_dma_data filter_data_rx;
	struct imx_pcm_fiq_params fiq_params;

	struct {
		unsigned int rfrc;
		unsigned int tfrc;
		unsigned int cmdau;
		unsigned int cmddu;
		unsigned int rxt;
		unsigned int rdr1;
		unsigned int rdr0;
		unsigned int tde1;
		unsigned int tde0;
		unsigned int roe1;
		unsigned int roe0;
		unsigned int tue1;
		unsigned int tue0;
		unsigned int tfs;
		unsigned int rfs;
		unsigned int tls;
		unsigned int rls;
		unsigned int rff1;
		unsigned int rff0;
		unsigned int tfe1;
		unsigned int tfe0;
	} stats;

	char name[1];
};

/**
 * fsl_ssi_isr: SSI interrupt handler
 *
 * Although it's possible to use the interrupt handler to send and receive
 * data to/from the SSI, we use the DMA instead.  Programming is more
 * complicated, but the performance is much better.
 *
 * This interrupt handler is used only to gather statistics.
 *
 * @irq: IRQ of the SSI device
 * @dev_id: pointer to the ssi_private structure for this SSI device
 */
static irqreturn_t fsl_ssi_isr(int irq, void *dev_id)
{
	struct fsl_ssi_private *ssi_private = dev_id;
<<<<<<< HEAD
	struct ccsr_ssi __iomem *ssi = ssi_private->ssi;
	irqreturn_t ret = IRQ_NONE;
=======
	struct regmap *regs = ssi_private->regs;
>>>>>>> ASoC: fsl-ssi: Use regmap
	__be32 sisr;
	__be32 sisr2 = 0;

	/* We got an interrupt, so read the status register to see what we
	   were interrupted for.  We mask it with the Interrupt Enable register
	   so that we only check for events that we're interested in.
	 */
<<<<<<< HEAD
	sisr = read_ssi(&ssi->sisr) & SIER_FLAGS;

	if (sisr & CCSR_SSI_SISR_RFRC) {
		ssi_private->stats.rfrc++;
		sisr2 |= CCSR_SSI_SISR_RFRC;
		ret = IRQ_HANDLED;
	}
=======
	regmap_read(regs, CCSR_SSI_SISR, &sisr);

	sisr2 = sisr & ssi_private->soc->sisr_write_mask;
	/* Clear the bits that we set */
	if (sisr2)
		regmap_write(regs, CCSR_SSI_SISR, sisr2);
>>>>>>> ASoC: fsl-ssi: Use regmap

	if (sisr & CCSR_SSI_SISR_TFRC) {
		ssi_private->stats.tfrc++;
		sisr2 |= CCSR_SSI_SISR_TFRC;
		ret = IRQ_HANDLED;
	}

	if (sisr & CCSR_SSI_SISR_CMDAU) {
		ssi_private->stats.cmdau++;
		ret = IRQ_HANDLED;
	}

<<<<<<< HEAD
	if (sisr & CCSR_SSI_SISR_CMDDU) {
		ssi_private->stats.cmddu++;
		ret = IRQ_HANDLED;
	}

	if (sisr & CCSR_SSI_SISR_RXT) {
		ssi_private->stats.rxt++;
		ret = IRQ_HANDLED;
=======
/*
 * Enable/Disable all rx/tx config flags at once.
 */
static void fsl_ssi_rxtx_config(struct fsl_ssi_private *ssi_private,
		bool enable)
{
	struct regmap *regs = ssi_private->regs;
	struct fsl_ssi_rxtx_reg_val *vals = &ssi_private->rxtx_reg_val;

	if (enable) {
		regmap_update_bits(regs, CCSR_SSI_SIER,
				vals->rx.sier | vals->tx.sier,
				vals->rx.sier | vals->tx.sier);
		regmap_update_bits(regs, CCSR_SSI_SRCR,
				vals->rx.srcr | vals->tx.srcr,
				vals->rx.srcr | vals->tx.srcr);
		regmap_update_bits(regs, CCSR_SSI_STCR,
				vals->rx.stcr | vals->tx.stcr,
				vals->rx.stcr | vals->tx.stcr);
	} else {
		regmap_update_bits(regs, CCSR_SSI_SRCR,
				vals->rx.srcr | vals->tx.srcr, 0);
		regmap_update_bits(regs, CCSR_SSI_STCR,
				vals->rx.stcr | vals->tx.stcr, 0);
		regmap_update_bits(regs, CCSR_SSI_SIER,
				vals->rx.sier | vals->tx.sier, 0);
>>>>>>> ASoC: fsl-ssi: Use regmap
	}

	if (sisr & CCSR_SSI_SISR_RDR1) {
		ssi_private->stats.rdr1++;
		ret = IRQ_HANDLED;
	}

<<<<<<< HEAD
	if (sisr & CCSR_SSI_SISR_RDR0) {
		ssi_private->stats.rdr0++;
		ret = IRQ_HANDLED;
	}
=======
/*
 * Enable/Disable a ssi configuration. You have to pass either
 * ssi_private->rxtx_reg_val.rx or tx as vals parameter.
 */
static void fsl_ssi_config(struct fsl_ssi_private *ssi_private, bool enable,
		struct fsl_ssi_reg_val *vals)
{
	struct regmap *regs = ssi_private->regs;
	struct fsl_ssi_reg_val *avals;
	int nr_active_streams;
	u32 scr_val;
	int keep_active;

	regmap_read(regs, CCSR_SSI_SCR, &scr_val);

	nr_active_streams = !!(scr_val & CCSR_SSI_SCR_TE) +
				!!(scr_val & CCSR_SSI_SCR_RE);

	if (nr_active_streams - 1 > 0)
		keep_active = 1;
	else
		keep_active = 0;
>>>>>>> ASoC: fsl-ssi: Use regmap

	if (sisr & CCSR_SSI_SISR_TDE1) {
		ssi_private->stats.tde1++;
		ret = IRQ_HANDLED;
	}

<<<<<<< HEAD
	if (sisr & CCSR_SSI_SISR_TDE0) {
		ssi_private->stats.tde0++;
		ret = IRQ_HANDLED;
=======
	/* If vals should be disabled, start with disabling the unit */
	if (!enable) {
		u32 scr = fsl_ssi_disable_val(vals->scr, avals->scr,
				keep_active);
		regmap_update_bits(regs, CCSR_SSI_SCR, scr, 0);
>>>>>>> ASoC: fsl-ssi: Use regmap
	}

	if (sisr & CCSR_SSI_SISR_ROE1) {
		ssi_private->stats.roe1++;
		sisr2 |= CCSR_SSI_SISR_ROE1;
		ret = IRQ_HANDLED;
	}

	if (sisr & CCSR_SSI_SISR_ROE0) {
		ssi_private->stats.roe0++;
		sisr2 |= CCSR_SSI_SISR_ROE0;
		ret = IRQ_HANDLED;
	}

<<<<<<< HEAD
	if (sisr & CCSR_SSI_SISR_TUE1) {
		ssi_private->stats.tue1++;
		sisr2 |= CCSR_SSI_SISR_TUE1;
		ret = IRQ_HANDLED;
	}
=======
	/*
	 * Configure single direction units while the SSI unit is running
	 * (online configuration)
	 */
	if (enable) {
		regmap_update_bits(regs, CCSR_SSI_SIER, vals->sier, vals->sier);
		regmap_update_bits(regs, CCSR_SSI_SRCR, vals->srcr, vals->srcr);
		regmap_update_bits(regs, CCSR_SSI_STCR, vals->stcr, vals->stcr);
	} else {
		u32 sier;
		u32 srcr;
		u32 stcr;
>>>>>>> ASoC: fsl-ssi: Use regmap

	if (sisr & CCSR_SSI_SISR_TUE0) {
		ssi_private->stats.tue0++;
		sisr2 |= CCSR_SSI_SISR_TUE0;
		ret = IRQ_HANDLED;
	}

<<<<<<< HEAD
	if (sisr & CCSR_SSI_SISR_TFS) {
		ssi_private->stats.tfs++;
		ret = IRQ_HANDLED;
	}

	if (sisr & CCSR_SSI_SISR_RFS) {
		ssi_private->stats.rfs++;
		ret = IRQ_HANDLED;
	}
=======
		/* These assignments are simply vals without bits set in avals*/
		sier = fsl_ssi_disable_val(vals->sier, avals->sier,
				keep_active);
		srcr = fsl_ssi_disable_val(vals->srcr, avals->srcr,
				keep_active);
		stcr = fsl_ssi_disable_val(vals->stcr, avals->stcr,
				keep_active);

		regmap_update_bits(regs, CCSR_SSI_SRCR, srcr, 0);
		regmap_update_bits(regs, CCSR_SSI_STCR, stcr, 0);
		regmap_update_bits(regs, CCSR_SSI_SIER, sier, 0);
	}

config_done:
	/* Enabling of subunits is done after configuration */
	if (enable)
		regmap_update_bits(regs, CCSR_SSI_SCR, vals->scr, vals->scr);
}
>>>>>>> ASoC: fsl-ssi: Use regmap

	if (sisr & CCSR_SSI_SISR_TLS) {
		ssi_private->stats.tls++;
		ret = IRQ_HANDLED;
	}

	if (sisr & CCSR_SSI_SISR_RLS) {
		ssi_private->stats.rls++;
		ret = IRQ_HANDLED;
	}

	if (sisr & CCSR_SSI_SISR_RFF1) {
		ssi_private->stats.rff1++;
		ret = IRQ_HANDLED;
	}

	if (sisr & CCSR_SSI_SISR_RFF0) {
		ssi_private->stats.rff0++;
		ret = IRQ_HANDLED;
	}

	if (sisr & CCSR_SSI_SISR_TFE1) {
		ssi_private->stats.tfe1++;
		ret = IRQ_HANDLED;
	}

	if (sisr & CCSR_SSI_SISR_TFE0) {
		ssi_private->stats.tfe0++;
		ret = IRQ_HANDLED;
	}

	/* Clear the bits that we set */
	if (sisr2)
		write_ssi(sisr2, &ssi->sisr);

	return ret;
}

static int fsl_ssi_setup(struct fsl_ssi_private *ssi_private)
{
<<<<<<< HEAD
	struct ccsr_ssi __iomem *ssi = ssi_private->ssi;
	u8 i2s_mode;
	u8 wm;
	int synchronous = ssi_private->cpu_dai_drv.symmetric_rates;

	if (ssi_private->imx_ac97)
		i2s_mode = CCSR_SSI_SCR_I2S_MODE_NORMAL | CCSR_SSI_SCR_NET;
	else
		i2s_mode = CCSR_SSI_SCR_I2S_MODE_SLAVE;
=======
	struct regmap *regs = ssi_private->regs;
>>>>>>> ASoC: fsl-ssi: Use regmap

	/*
	 * Section 16.5 of the MPC8610 reference manual says that the SSI needs
	 * to be disabled before updating the registers we set here.
	 */
<<<<<<< HEAD
	write_ssi_mask(&ssi->scr, CCSR_SSI_SCR_SSIEN, 0);
=======
	regmap_write(regs, CCSR_SSI_STCCR,
			CCSR_SSI_SxCCR_WL(17) | CCSR_SSI_SxCCR_DC(13));
	regmap_write(regs, CCSR_SSI_SRCCR,
			CCSR_SSI_SxCCR_WL(17) | CCSR_SSI_SxCCR_DC(13));
>>>>>>> ASoC: fsl-ssi: Use regmap

	/*
	 * Program the SSI into I2S Slave Non-Network Synchronous mode. Also
	 * enable the transmit and receive FIFO.
	 *
	 * FIXME: Little-endian samples require a different shift dir
	 */
	write_ssi_mask(&ssi->scr,
		CCSR_SSI_SCR_I2S_MODE_MASK | CCSR_SSI_SCR_SYN,
		CCSR_SSI_SCR_TFR_CLK_DIS |
		i2s_mode |
		(synchronous ? CCSR_SSI_SCR_SYN : 0));

	write_ssi(CCSR_SSI_STCR_TXBIT0 | CCSR_SSI_STCR_TFEN0 |
		 CCSR_SSI_STCR_TFSI | CCSR_SSI_STCR_TEFS |
		 CCSR_SSI_STCR_TSCKP, &ssi->stcr);

	write_ssi(CCSR_SSI_SRCR_RXBIT0 | CCSR_SSI_SRCR_RFEN0 |
		 CCSR_SSI_SRCR_RFSI | CCSR_SSI_SRCR_REFS |
		 CCSR_SSI_SRCR_RSCKP, &ssi->srcr);
	/*
	 * The DC and PM bits are only used if the SSI is the clock master.
	 */
<<<<<<< HEAD
=======
	regmap_write(regs, CCSR_SSI_SACNT,
			CCSR_SSI_SACNT_AC97EN | CCSR_SSI_SACNT_FV);
	regmap_write(regs, CCSR_SSI_SACCDIS, 0xff);
	regmap_write(regs, CCSR_SSI_SACCEN, 0x300);
>>>>>>> ASoC: fsl-ssi: Use regmap

	/*
	 * Set the watermark for transmit FIFI 0 and receive FIFO 0. We don't
	 * use FIFO 1. We program the transmit water to signal a DMA transfer
	 * if there are only two (or fewer) elements left in the FIFO. Two
	 * elements equals one frame (left channel, right channel). This value,
	 * however, depends on the depth of the transmit buffer.
	 *
	 * We set the watermark on the same level as the DMA burstsize.  For
	 * fiq it is probably better to use the biggest possible watermark
	 * size.
	 */
<<<<<<< HEAD
	if (ssi_private->use_dma)
		wm = ssi_private->fifo_depth - 2;
	else
		wm = ssi_private->fifo_depth;

	write_ssi(CCSR_SSI_SFCSR_TFWM0(wm) | CCSR_SSI_SFCSR_RFWM0(wm) |
		CCSR_SSI_SFCSR_TFWM1(wm) | CCSR_SSI_SFCSR_RFWM1(wm),
		&ssi->sfcsr);

	/*
	 * For ac97 interrupts are enabled with the startup of the substream
	 * because it is also running without an active substream. Normally SSI
	 * is only enabled when there is a substream.
	 */
	if (ssi_private->imx_ac97) {
		/*
		 * Setup the clock control register
		 */
		write_ssi(CCSR_SSI_SxCCR_WL(17) | CCSR_SSI_SxCCR_DC(13),
				&ssi->stccr);
		write_ssi(CCSR_SSI_SxCCR_WL(17) | CCSR_SSI_SxCCR_DC(13),
				&ssi->srccr);

		/*
		 * Enable AC97 mode and startup the SSI
		 */
		write_ssi(CCSR_SSI_SACNT_AC97EN | CCSR_SSI_SACNT_FV,
				&ssi->sacnt);
		write_ssi(0xff, &ssi->saccdis);
		write_ssi(0x300, &ssi->saccen);

		/*
		 * Enable SSI, Transmit and Receive
		 */
		write_ssi_mask(&ssi->scr, 0, CCSR_SSI_SCR_SSIEN |
				CCSR_SSI_SCR_TE | CCSR_SSI_SCR_RE);

		write_ssi(CCSR_SSI_SOR_WAIT(3), &ssi->sor);
	}

	return 0;
=======
	regmap_update_bits(regs, CCSR_SSI_SCR,
			CCSR_SSI_SCR_SSIEN | CCSR_SSI_SCR_TE | CCSR_SSI_SCR_RE,
			CCSR_SSI_SCR_SSIEN | CCSR_SSI_SCR_TE | CCSR_SSI_SCR_RE);

	regmap_write(regs, CCSR_SSI_SOR, CCSR_SSI_SOR_WAIT(3));
>>>>>>> ASoC: fsl-ssi: Use regmap
}


/**
 * fsl_ssi_startup: create a new substream
 *
 * This is the first function called when a stream is opened.
 *
 * If this is the first stream open, then grab the IRQ and program most of
 * the SSI registers.
 */
static int fsl_ssi_startup(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct fsl_ssi_private *ssi_private =
		snd_soc_dai_get_drvdata(rtd->cpu_dai);
	int synchronous = ssi_private->cpu_dai_drv.symmetric_rates;

	/*
	 * If this is the first stream opened, then request the IRQ
	 * and initialize the SSI registers.
	 */
<<<<<<< HEAD
	if (!ssi_private->first_stream) {
		ssi_private->first_stream = substream;
=======
	if (ssi_private->use_dual_fifo)
		snd_pcm_hw_constraint_step(substream->runtime, 0,
				SNDRV_PCM_HW_PARAM_PERIOD_SIZE, 2);

	return 0;
}

/**
 * fsl_ssi_set_bclk - configure Digital Audio Interface bit clock
 *
 * Note: This function can be only called when using SSI as DAI master
 *
 * Quick instruction for parameters:
 * freq: Output BCLK frequency = samplerate * 32 (fixed) * channels
 * dir: SND_SOC_CLOCK_OUT -> TxBCLK, SND_SOC_CLOCK_IN -> RxBCLK.
 */
static int fsl_ssi_set_bclk(struct snd_pcm_substream *substream,
		struct snd_soc_dai *cpu_dai,
		struct snd_pcm_hw_params *hw_params)
{
	struct fsl_ssi_private *ssi_private = snd_soc_dai_get_drvdata(cpu_dai);
	struct regmap *regs = ssi_private->regs;
	int synchronous = ssi_private->cpu_dai_drv.symmetric_rates, ret;
	u32 pm = 999, div2, psr, stccr, mask, afreq, factor, i;
	unsigned long clkrate, baudrate, tmprate;
	u64 sub, savesub = 100000;
	unsigned int freq;
	bool baudclk_is_used;

	/* Prefer the explicitly set bitclock frequency */
	if (ssi_private->bitclk_freq)
		freq = ssi_private->bitclk_freq;
	else
		freq = params_channels(hw_params) * 32 * params_rate(hw_params);

	/* Don't apply it to any non-baudclk circumstance */
	if (IS_ERR(ssi_private->baudclk))
		return -EINVAL;

	baudclk_is_used = ssi_private->baudclk_streams & ~(BIT(substream->stream));

	/* It should be already enough to divide clock by setting pm alone */
	psr = 0;
	div2 = 0;

	factor = (div2 + 1) * (7 * psr + 1) * 2;

	for (i = 0; i < 255; i++) {
		/* The bclk rate must be smaller than 1/5 sysclk rate */
		if (factor * (i + 1) < 5)
			continue;

		tmprate = freq * factor * (i + 2);

		if (baudclk_is_used)
			clkrate = clk_get_rate(ssi_private->baudclk);
		else
			clkrate = clk_round_rate(ssi_private->baudclk, tmprate);

		do_div(clkrate, factor);
		afreq = (u32)clkrate / (i + 1);

		if (freq == afreq)
			sub = 0;
		else if (freq / afreq == 1)
			sub = freq - afreq;
		else if (afreq / freq == 1)
			sub = afreq - freq;
		else
			continue;
>>>>>>> ASoC: fsl-ssi: Use regmap

		/*
		 * fsl_ssi_setup was already called by ac97_init earlier if
		 * the driver is in ac97 mode.
		 */
		if (!ssi_private->imx_ac97)
			fsl_ssi_setup(ssi_private);
	} else {
		if (synchronous) {
			struct snd_pcm_runtime *first_runtime =
				ssi_private->first_stream->runtime;
			/*
			 * This is the second stream open, and we're in
			 * synchronous mode, so we need to impose sample
			 * sample size constraints. This is because STCCR is
			 * used for playback and capture in synchronous mode,
			 * so there's no way to specify different word
			 * lengths.
			 *
			 * Note that this can cause a race condition if the
			 * second stream is opened before the first stream is
			 * fully initialized.  We provide some protection by
			 * checking to make sure the first stream is
			 * initialized, but it's not perfect.  ALSA sometimes
			 * re-initializes the driver with a different sample
			 * rate or size.  If the second stream is opened
			 * before the first stream has received its final
			 * parameters, then the second stream may be
			 * constrained to the wrong sample rate or size.
			 */
			if (first_runtime->sample_bits) {
				snd_pcm_hw_constraint_minmax(substream->runtime,
						SNDRV_PCM_HW_PARAM_SAMPLE_BITS,
				first_runtime->sample_bits,
				first_runtime->sample_bits);
			}
		}

<<<<<<< HEAD
		ssi_private->second_stream = substream;
=======
		/* We are lucky */
		if (savesub == 0)
			break;
	}

	/* No proper pm found if it is still remaining the initial value */
	if (pm == 999) {
		dev_err(cpu_dai->dev, "failed to handle the required sysclk\n");
		return -EINVAL;
	}

	stccr = CCSR_SSI_SxCCR_PM(pm + 1) | (div2 ? CCSR_SSI_SxCCR_DIV2 : 0) |
		(psr ? CCSR_SSI_SxCCR_PSR : 0);
	mask = CCSR_SSI_SxCCR_PM_MASK | CCSR_SSI_SxCCR_DIV2 |
		CCSR_SSI_SxCCR_PSR;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK || synchronous)
		regmap_update_bits(regs, CCSR_SSI_STCCR, mask, stccr);
	else
		regmap_update_bits(regs, CCSR_SSI_SRCCR, mask, stccr);

	if (!baudclk_is_used) {
		ret = clk_set_rate(ssi_private->baudclk, baudrate);
		if (ret) {
			dev_err(cpu_dai->dev, "failed to set baudclk rate\n");
			return -EINVAL;
		}
>>>>>>> ASoC: fsl-ssi: Use regmap
	}

	return 0;
}

/**
 * fsl_ssi_hw_params - program the sample size
 *
 * Most of the SSI registers have been programmed in the startup function,
 * but the word length must be programmed here.  Unfortunately, programming
 * the SxCCR.WL bits requires the SSI to be temporarily disabled.  This can
 * cause a problem with supporting simultaneous playback and capture.  If
 * the SSI is already playing a stream, then that stream may be temporarily
 * stopped when you start capture.
 *
 * Note: The SxCCR.DC and SxCCR.PM bits are only used if the SSI is the
 * clock master.
 */
static int fsl_ssi_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *hw_params, struct snd_soc_dai *cpu_dai)
{
	struct fsl_ssi_private *ssi_private = snd_soc_dai_get_drvdata(cpu_dai);
<<<<<<< HEAD
	struct ccsr_ssi __iomem *ssi = ssi_private->ssi;
	unsigned int sample_size =
		snd_pcm_format_width(params_format(hw_params));
	u32 wl = CCSR_SSI_SxCCR_WL(sample_size);
	int enabled = read_ssi(&ssi->scr) & CCSR_SSI_SCR_SSIEN;
=======
	struct regmap *regs = ssi_private->regs;
	unsigned int channels = params_channels(hw_params);
	unsigned int sample_size =
		snd_pcm_format_width(params_format(hw_params));
	u32 wl = CCSR_SSI_SxCCR_WL(sample_size);
	int ret;
	u32 scr_val;
	int enabled;

	regmap_read(regs, CCSR_SSI_SCR, &scr_val);
	enabled = scr_val & CCSR_SSI_SCR_SSIEN;
>>>>>>> ASoC: fsl-ssi: Use regmap

	/*
	 * If we're in synchronous mode, and the SSI is already enabled,
	 * then STCCR is already set properly.
	 */
	if (enabled && ssi_private->cpu_dai_drv.symmetric_rates)
		return 0;

	/*
	 * FIXME: The documentation says that SxCCR[WL] should not be
	 * modified while the SSI is enabled.  The only time this can
	 * happen is if we're trying to do simultaneous playback and
	 * capture in asynchronous mode.  Unfortunately, I have been enable
	 * to get that to work at all on the P1022DS.  Therefore, we don't
	 * bother to disable/enable the SSI when setting SxCCR[WL], because
	 * the SSI will stop anyway.  Maybe one day, this will get fixed.
	 */

	/* In synchronous mode, the SSI uses STCCR for capture */
	if ((substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ||
	    ssi_private->cpu_dai_drv.symmetric_rates)
		regmap_update_bits(regs, CCSR_SSI_STCCR, CCSR_SSI_SxCCR_WL_MASK,
				wl);
	else
		regmap_update_bits(regs, CCSR_SSI_SRCCR, CCSR_SSI_SxCCR_WL_MASK,
				wl);

<<<<<<< HEAD
=======
	if (!fsl_ssi_is_ac97(ssi_private))
		regmap_update_bits(regs, CCSR_SSI_SCR,
				CCSR_SSI_SCR_NET | CCSR_SSI_SCR_I2S_MODE_MASK,
				channels == 1 ? 0 : ssi_private->i2s_mode);

	return 0;
}

static int fsl_ssi_hw_free(struct snd_pcm_substream *substream,
		struct snd_soc_dai *cpu_dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct fsl_ssi_private *ssi_private =
		snd_soc_dai_get_drvdata(rtd->cpu_dai);

	if (fsl_ssi_is_i2s_master(ssi_private) &&
			ssi_private->baudclk_streams & BIT(substream->stream)) {
		clk_disable_unprepare(ssi_private->baudclk);
		ssi_private->baudclk_streams &= ~BIT(substream->stream);
	}

	return 0;
}

static int _fsl_ssi_set_dai_fmt(struct fsl_ssi_private *ssi_private,
		unsigned int fmt)
{
	struct regmap *regs = ssi_private->regs;
	u32 strcr = 0, stcr, srcr, scr, mask;
	u8 wm;

	ssi_private->dai_fmt = fmt;

	if (fsl_ssi_is_i2s_master(ssi_private) && IS_ERR(ssi_private->baudclk)) {
		dev_err(&ssi_private->pdev->dev, "baudclk is missing which is necessary for master mode\n");
		return -EINVAL;
	}

	fsl_ssi_setup_reg_vals(ssi_private);

	regmap_read(regs, CCSR_SSI_SCR, &scr);
	scr &= ~(CCSR_SSI_SCR_SYN | CCSR_SSI_SCR_I2S_MODE_MASK);
	scr |= CCSR_SSI_SCR_SYNC_TX_FS;

	mask = CCSR_SSI_STCR_TXBIT0 | CCSR_SSI_STCR_TFDIR | CCSR_SSI_STCR_TXDIR |
		CCSR_SSI_STCR_TSCKP | CCSR_SSI_STCR_TFSI | CCSR_SSI_STCR_TFSL |
		CCSR_SSI_STCR_TEFS;
	regmap_read(regs, CCSR_SSI_STCR, &stcr);
	regmap_read(regs, CCSR_SSI_SRCR, &srcr);
	stcr &= ~mask;
	srcr &= ~mask;

	ssi_private->i2s_mode = CCSR_SSI_SCR_NET;
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBS_CFS:
			ssi_private->i2s_mode |= CCSR_SSI_SCR_I2S_MODE_MASTER;
			regmap_update_bits(regs, CCSR_SSI_STCCR,
					CCSR_SSI_SxCCR_DC_MASK,
					CCSR_SSI_SxCCR_DC(2));
			regmap_update_bits(regs, CCSR_SSI_SRCCR,
					CCSR_SSI_SxCCR_DC_MASK,
					CCSR_SSI_SxCCR_DC(2));
			break;
		case SND_SOC_DAIFMT_CBM_CFM:
			ssi_private->i2s_mode |= CCSR_SSI_SCR_I2S_MODE_SLAVE;
			break;
		default:
			return -EINVAL;
		}

		/* Data on rising edge of bclk, frame low, 1clk before data */
		strcr |= CCSR_SSI_STCR_TFSI | CCSR_SSI_STCR_TSCKP |
			CCSR_SSI_STCR_TXBIT0 | CCSR_SSI_STCR_TEFS;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		/* Data on rising edge of bclk, frame high */
		strcr |= CCSR_SSI_STCR_TXBIT0 | CCSR_SSI_STCR_TSCKP;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		/* Data on rising edge of bclk, frame high, 1clk before data */
		strcr |= CCSR_SSI_STCR_TFSL | CCSR_SSI_STCR_TSCKP |
			CCSR_SSI_STCR_TXBIT0 | CCSR_SSI_STCR_TEFS;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		/* Data on rising edge of bclk, frame high */
		strcr |= CCSR_SSI_STCR_TFSL | CCSR_SSI_STCR_TSCKP |
			CCSR_SSI_STCR_TXBIT0;
		break;
	case SND_SOC_DAIFMT_AC97:
		ssi_private->i2s_mode |= CCSR_SSI_SCR_I2S_MODE_NORMAL;
		break;
	default:
		return -EINVAL;
	}
	scr |= ssi_private->i2s_mode;

	/* DAI clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		/* Nothing to do for both normal cases */
		break;
	case SND_SOC_DAIFMT_IB_NF:
		/* Invert bit clock */
		strcr ^= CCSR_SSI_STCR_TSCKP;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		/* Invert frame clock */
		strcr ^= CCSR_SSI_STCR_TFSI;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		/* Invert both clocks */
		strcr ^= CCSR_SSI_STCR_TSCKP;
		strcr ^= CCSR_SSI_STCR_TFSI;
		break;
	default:
		return -EINVAL;
	}

	/* DAI clock master masks */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		strcr |= CCSR_SSI_STCR_TFDIR | CCSR_SSI_STCR_TXDIR;
		scr |= CCSR_SSI_SCR_SYS_CLK_EN;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		scr &= ~CCSR_SSI_SCR_SYS_CLK_EN;
		break;
	default:
		return -EINVAL;
	}

	stcr |= strcr;
	srcr |= strcr;

	if (ssi_private->cpu_dai_drv.symmetric_rates) {
		/* Need to clear RXDIR when using SYNC mode */
		srcr &= ~CCSR_SSI_SRCR_RXDIR;
		scr |= CCSR_SSI_SCR_SYN;
	}

	regmap_write(regs, CCSR_SSI_STCR, stcr);
	regmap_write(regs, CCSR_SSI_SRCR, srcr);
	regmap_write(regs, CCSR_SSI_SCR, scr);

	/*
	 * Set the watermark for transmit FIFI 0 and receive FIFO 0. We don't
	 * use FIFO 1. We program the transmit water to signal a DMA transfer
	 * if there are only two (or fewer) elements left in the FIFO. Two
	 * elements equals one frame (left channel, right channel). This value,
	 * however, depends on the depth of the transmit buffer.
	 *
	 * We set the watermark on the same level as the DMA burstsize.  For
	 * fiq it is probably better to use the biggest possible watermark
	 * size.
	 */
	if (ssi_private->use_dma)
		wm = ssi_private->fifo_depth - 2;
	else
		wm = ssi_private->fifo_depth;

	regmap_write(regs, CCSR_SSI_SFCSR,
			CCSR_SSI_SFCSR_TFWM0(wm) | CCSR_SSI_SFCSR_RFWM0(wm) |
			CCSR_SSI_SFCSR_TFWM1(wm) | CCSR_SSI_SFCSR_RFWM1(wm));

	if (ssi_private->use_dual_fifo) {
		regmap_update_bits(regs, CCSR_SSI_SRCR, CCSR_SSI_SRCR_RFEN1,
				CCSR_SSI_SRCR_RFEN1);
		regmap_update_bits(regs, CCSR_SSI_STCR, CCSR_SSI_STCR_TFEN1,
				CCSR_SSI_STCR_TFEN1);
		regmap_update_bits(regs, CCSR_SSI_SCR, CCSR_SSI_SCR_TCH_EN,
				CCSR_SSI_SCR_TCH_EN);
	}

	if (fmt & SND_SOC_DAIFMT_AC97)
		fsl_ssi_setup_ac97(ssi_private);

	return 0;

}

/**
 * fsl_ssi_set_dai_fmt - configure Digital Audio Interface Format.
 */
static int fsl_ssi_set_dai_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	struct fsl_ssi_private *ssi_private = snd_soc_dai_get_drvdata(cpu_dai);

	return _fsl_ssi_set_dai_fmt(ssi_private, fmt);
}

/**
 * fsl_ssi_set_dai_tdm_slot - set TDM slot number
 *
 * Note: This function can be only called when using SSI as DAI master
 */
static int fsl_ssi_set_dai_tdm_slot(struct snd_soc_dai *cpu_dai, u32 tx_mask,
				u32 rx_mask, int slots, int slot_width)
{
	struct fsl_ssi_private *ssi_private = snd_soc_dai_get_drvdata(cpu_dai);
	struct regmap *regs = ssi_private->regs;
	u32 val;

	/* The slot number should be >= 2 if using Network mode or I2S mode */
	regmap_read(regs, CCSR_SSI_SCR, &val);
	val &= CCSR_SSI_SCR_I2S_MODE_MASK | CCSR_SSI_SCR_NET;
	if (val && slots < 2) {
		dev_err(cpu_dai->dev, "slot number should be >= 2 in I2S or NET\n");
		return -EINVAL;
	}

	regmap_update_bits(regs, CCSR_SSI_STCCR, CCSR_SSI_SxCCR_DC_MASK,
			CCSR_SSI_SxCCR_DC(slots));
	regmap_update_bits(regs, CCSR_SSI_SRCCR, CCSR_SSI_SxCCR_DC_MASK,
			CCSR_SSI_SxCCR_DC(slots));

	/* The register SxMSKs needs SSI to provide essential clock due to
	 * hardware design. So we here temporarily enable SSI to set them.
	 */
	regmap_read(regs, CCSR_SSI_SCR, &val);
	val &= CCSR_SSI_SCR_SSIEN;
	regmap_update_bits(regs, CCSR_SSI_SCR, CCSR_SSI_SCR_SSIEN,
			CCSR_SSI_SCR_SSIEN);

	regmap_write(regs, CCSR_SSI_STMSK, tx_mask);
	regmap_write(regs, CCSR_SSI_SRMSK, rx_mask);

	regmap_update_bits(regs, CCSR_SSI_SCR, CCSR_SSI_SCR_SSIEN, val);

>>>>>>> ASoC: fsl-ssi: Use regmap
	return 0;
}

/**
 * fsl_ssi_trigger: start and stop the DMA transfer.
 *
 * This function is called by ALSA to start, stop, pause, and resume the DMA
 * transfer of data.
 *
 * The DMA channel is in external master start and pause mode, which
 * means the SSI completely controls the flow of data.
 */
static int fsl_ssi_trigger(struct snd_pcm_substream *substream, int cmd,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct fsl_ssi_private *ssi_private = snd_soc_dai_get_drvdata(rtd->cpu_dai);
<<<<<<< HEAD
	struct ccsr_ssi __iomem *ssi = ssi_private->ssi;
	unsigned int sier_bits;

	/*
	 *  Enable only the interrupts and DMA requests
	 *  that are needed for the channel. As the fiq
	 *  is polling for this bits, we have to ensure
	 *  that this are aligned with the preallocated
	 *  buffers
	 */

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (ssi_private->use_dma)
			sier_bits = SIER_FLAGS;
		else
			sier_bits = CCSR_SSI_SIER_TIE | CCSR_SSI_SIER_TFE0_EN;
	} else {
		if (ssi_private->use_dma)
			sier_bits = SIER_FLAGS;
		else
			sier_bits = CCSR_SSI_SIER_RIE | CCSR_SSI_SIER_RFF0_EN;
	}
=======
	struct regmap *regs = ssi_private->regs;
>>>>>>> ASoC: fsl-ssi: Use regmap

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			write_ssi_mask(&ssi->scr, 0,
				CCSR_SSI_SCR_SSIEN | CCSR_SSI_SCR_TE);
		else
			write_ssi_mask(&ssi->scr, 0,
				CCSR_SSI_SCR_SSIEN | CCSR_SSI_SCR_RE);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			write_ssi_mask(&ssi->scr, CCSR_SSI_SCR_TE, 0);
		else
			write_ssi_mask(&ssi->scr, CCSR_SSI_SCR_RE, 0);

		if (!ssi_private->imx_ac97 && (read_ssi(&ssi->scr) &
					(CCSR_SSI_SCR_TE | CCSR_SSI_SCR_RE)) == 0)
			write_ssi_mask(&ssi->scr, CCSR_SSI_SCR_SSIEN, 0);
		break;

	default:
		return -EINVAL;
	}

<<<<<<< HEAD
	write_ssi(sier_bits, &ssi->sier);
=======
	if (fsl_ssi_is_ac97(ssi_private)) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			regmap_write(regs, CCSR_SSI_SOR, CCSR_SSI_SOR_TX_CLR);
		else
			regmap_write(regs, CCSR_SSI_SOR, CCSR_SSI_SOR_RX_CLR);
	}
>>>>>>> ASoC: fsl-ssi: Use regmap

	return 0;
}

/**
 * fsl_ssi_shutdown: shutdown the SSI
 *
 * Shutdown the SSI if there are no other substreams open.
 */
static void fsl_ssi_shutdown(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct fsl_ssi_private *ssi_private = snd_soc_dai_get_drvdata(rtd->cpu_dai);

	if (ssi_private->first_stream == substream)
		ssi_private->first_stream = ssi_private->second_stream;

	ssi_private->second_stream = NULL;
}

static int fsl_ssi_dai_probe(struct snd_soc_dai *dai)
{
	struct fsl_ssi_private *ssi_private = snd_soc_dai_get_drvdata(dai);

	if (ssi_private->ssi_on_imx && ssi_private->use_dma) {
		dai->playback_dma_data = &ssi_private->dma_params_tx;
		dai->capture_dma_data = &ssi_private->dma_params_rx;
	}

	return 0;
}

static const struct snd_soc_dai_ops fsl_ssi_dai_ops = {
	.startup	= fsl_ssi_startup,
	.hw_params	= fsl_ssi_hw_params,
	.shutdown	= fsl_ssi_shutdown,
	.trigger	= fsl_ssi_trigger,
};

/* Template for the CPU dai driver structure */
static struct snd_soc_dai_driver fsl_ssi_dai_template = {
	.probe = fsl_ssi_dai_probe,
	.playback = {
		/* The SSI does not support monaural audio. */
		.channels_min = 2,
		.channels_max = 2,
		.rates = FSLSSI_I2S_RATES,
		.formats = FSLSSI_I2S_FORMATS,
	},
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = FSLSSI_I2S_RATES,
		.formats = FSLSSI_I2S_FORMATS,
	},
	.ops = &fsl_ssi_dai_ops,
};

static const struct snd_soc_component_driver fsl_ssi_component = {
	.name		= "fsl-ssi",
};

/**
 * fsl_ssi_ac97_trigger: start and stop the AC97 receive/transmit.
 *
 * This function is called by ALSA to start, stop, pause, and resume the
 * transfer of data.
 */
static int fsl_ssi_ac97_trigger(struct snd_pcm_substream *substream, int cmd,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct fsl_ssi_private *ssi_private = snd_soc_dai_get_drvdata(
			rtd->cpu_dai);
	struct ccsr_ssi __iomem *ssi = ssi_private->ssi;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			write_ssi_mask(&ssi->sier, 0, CCSR_SSI_SIER_TIE |
					CCSR_SSI_SIER_TFE0_EN);
		else
			write_ssi_mask(&ssi->sier, 0, CCSR_SSI_SIER_RIE |
					CCSR_SSI_SIER_RFF0_EN);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			write_ssi_mask(&ssi->sier, CCSR_SSI_SIER_TIE |
					CCSR_SSI_SIER_TFE0_EN, 0);
		else
			write_ssi_mask(&ssi->sier, CCSR_SSI_SIER_RIE |
					CCSR_SSI_SIER_RFF0_EN, 0);
		break;

	default:
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		write_ssi(CCSR_SSI_SOR_TX_CLR, &ssi->sor);
	else
		write_ssi(CCSR_SSI_SOR_RX_CLR, &ssi->sor);

	return 0;
}

static const struct snd_soc_dai_ops fsl_ssi_ac97_dai_ops = {
	.startup	= fsl_ssi_startup,
	.shutdown	= fsl_ssi_shutdown,
	.trigger	= fsl_ssi_ac97_trigger,
};

static struct snd_soc_dai_driver fsl_ssi_ac97_dai = {
	.ac97_control = 1,
	.playback = {
		.stream_name = "AC97 Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.stream_name = "AC97 Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &fsl_ssi_ac97_dai_ops,
};


static struct fsl_ssi_private *fsl_ac97_data;

static void fsl_ssi_ac97_init(void)
{
	fsl_ssi_setup(fsl_ac97_data);
}

static void fsl_ssi_ac97_write(struct snd_ac97 *ac97, unsigned short reg,
		unsigned short val)
{
	struct regmap *regs = fsl_ac97_data->regs;
	unsigned int lreg;
	unsigned int lval;

	if (reg > 0x7f)
		return;


	lreg = reg <<  12;
	regmap_write(regs, CCSR_SSI_SACADD, lreg);

	lval = val << 4;
	regmap_write(regs, CCSR_SSI_SACDAT, lval);

	regmap_update_bits(regs, CCSR_SSI_SACNT, CCSR_SSI_SACNT_RDWR_MASK,
			CCSR_SSI_SACNT_WR);
	udelay(100);
}

static unsigned short fsl_ssi_ac97_read(struct snd_ac97 *ac97,
		unsigned short reg)
{
	struct regmap *regs = fsl_ac97_data->regs;

	unsigned short val = -1;
	u32 reg_val;
	unsigned int lreg;

	lreg = (reg & 0x7f) <<  12;
	regmap_write(regs, CCSR_SSI_SACADD, lreg);
	regmap_update_bits(regs, CCSR_SSI_SACNT, CCSR_SSI_SACNT_RDWR_MASK,
			CCSR_SSI_SACNT_RD);

	udelay(100);

	regmap_read(regs, CCSR_SSI_SACDAT, &reg_val);
	val = (reg_val >> 4) & 0xffff;

	return val;
}

static struct snd_ac97_bus_ops fsl_ssi_ac97_ops = {
	.read		= fsl_ssi_ac97_read,
	.write		= fsl_ssi_ac97_write,
};

/* Show the statistics of a flag only if its interrupt is enabled.  The
 * compiler will optimze this code to a no-op if the interrupt is not
 * enabled.
 */
#define SIER_SHOW(flag, name) \
	do { \
		if (SIER_FLAGS & CCSR_SSI_SIER_##flag) \
			length += sprintf(buf + length, #name "=%u\n", \
				ssi_private->stats.name); \
	} while (0)


/**
 * fsl_sysfs_ssi_show: display SSI statistics
 *
 * Display the statistics for the current SSI device.  To avoid confusion,
 * we only show those counts that are enabled.
 */
static ssize_t fsl_sysfs_ssi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct fsl_ssi_private *ssi_private =
		container_of(attr, struct fsl_ssi_private, dev_attr);
	ssize_t length = 0;

	SIER_SHOW(RFRC_EN, rfrc);
	SIER_SHOW(TFRC_EN, tfrc);
	SIER_SHOW(CMDAU_EN, cmdau);
	SIER_SHOW(CMDDU_EN, cmddu);
	SIER_SHOW(RXT_EN, rxt);
	SIER_SHOW(RDR1_EN, rdr1);
	SIER_SHOW(RDR0_EN, rdr0);
	SIER_SHOW(TDE1_EN, tde1);
	SIER_SHOW(TDE0_EN, tde0);
	SIER_SHOW(ROE1_EN, roe1);
	SIER_SHOW(ROE0_EN, roe0);
	SIER_SHOW(TUE1_EN, tue1);
	SIER_SHOW(TUE0_EN, tue0);
	SIER_SHOW(TFS_EN, tfs);
	SIER_SHOW(RFS_EN, rfs);
	SIER_SHOW(TLS_EN, tls);
	SIER_SHOW(RLS_EN, rls);
	SIER_SHOW(RFF1_EN, rff1);
	SIER_SHOW(RFF0_EN, rff0);
	SIER_SHOW(TFE1_EN, tfe1);
	SIER_SHOW(TFE0_EN, tfe0);

	return length;
}

/**
 * Make every character in a string lower-case
 */
static void make_lowercase(char *s)
{
	char *p = s;
	char c;

	while ((c = *p)) {
		if ((c >= 'A') && (c <= 'Z'))
			*p = c + ('a' - 'A');
		p++;
	}
}

<<<<<<< HEAD
=======
static int fsl_ssi_imx_probe(struct platform_device *pdev,
		struct fsl_ssi_private *ssi_private, void __iomem *iomem)
{
	struct device_node *np = pdev->dev.of_node;
	u32 dmas[4];
	int ret;

	ssi_private->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(ssi_private->clk)) {
		ret = PTR_ERR(ssi_private->clk);
		dev_err(&pdev->dev, "could not get clock: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(ssi_private->clk);
	if (ret) {
		dev_err(&pdev->dev, "clk_prepare_enable failed: %d\n", ret);
		return ret;
	}

	/* For those SLAVE implementations, we ingore non-baudclk cases
	 * and, instead, abandon MASTER mode that needs baud clock.
	 */
	ssi_private->baudclk = devm_clk_get(&pdev->dev, "baud");
	if (IS_ERR(ssi_private->baudclk))
		dev_dbg(&pdev->dev, "could not get baud clock: %ld\n",
			 PTR_ERR(ssi_private->baudclk));

	/*
	 * We have burstsize be "fifo_depth - 2" to match the SSI
	 * watermark setting in fsl_ssi_startup().
	 */
	ssi_private->dma_params_tx.maxburst = ssi_private->fifo_depth - 2;
	ssi_private->dma_params_rx.maxburst = ssi_private->fifo_depth - 2;
	ssi_private->dma_params_tx.addr = ssi_private->ssi_phys + CCSR_SSI_STX0;
	ssi_private->dma_params_rx.addr = ssi_private->ssi_phys + CCSR_SSI_SRX0;

	ret = !of_property_read_u32_array(np, "dmas", dmas, 4);
	if (ssi_private->use_dma && !ret && dmas[2] == IMX_DMATYPE_SSI_DUAL) {
		ssi_private->use_dual_fifo = true;
		/* When using dual fifo mode, we need to keep watermark
		 * as even numbers due to dma script limitation.
		 */
		ssi_private->dma_params_tx.maxburst &= ~0x1;
		ssi_private->dma_params_rx.maxburst &= ~0x1;
	}

	if (!ssi_private->use_dma) {

		/*
		 * Some boards use an incompatible codec. To get it
		 * working, we are using imx-fiq-pcm-audio, that
		 * can handle those codecs. DMA is not possible in this
		 * situation.
		 */

		ssi_private->fiq_params.irq = ssi_private->irq;
		ssi_private->fiq_params.base = iomem;
		ssi_private->fiq_params.dma_params_rx =
			&ssi_private->dma_params_rx;
		ssi_private->fiq_params.dma_params_tx =
			&ssi_private->dma_params_tx;

		ret = imx_pcm_fiq_init(pdev, &ssi_private->fiq_params);
		if (ret)
			goto error_pcm;
	} else {
		ret = imx_pcm_dma_init(pdev);
		if (ret)
			goto error_pcm;
	}

	return 0;

error_pcm:
	clk_disable_unprepare(ssi_private->clk);

	return ret;
}

static void fsl_ssi_imx_clean(struct platform_device *pdev,
		struct fsl_ssi_private *ssi_private)
{
	if (!ssi_private->use_dma)
		imx_pcm_fiq_exit(pdev);
	clk_disable_unprepare(ssi_private->clk);
}

>>>>>>> ASoC: fsl-ssi: Use regmap
static int fsl_ssi_probe(struct platform_device *pdev)
{
	struct fsl_ssi_private *ssi_private;
	int ret = 0;
	struct device_attribute *dev_attr = NULL;
	struct device_node *np = pdev->dev.of_node;
	const char *p, *sprop;
	const uint32_t *iprop;
	struct resource res;
	void __iomem *iomem;
	char name[64];
	bool shared;
	bool ac97 = false;

	/* SSIs that are not connected on the board should have a
	 *      status = "disabled"
	 * property in their device tree nodes.
	 */
	if (!of_device_is_available(np))
		return -ENODEV;

	/* We only support the SSI in "I2S Slave" mode */
	sprop = of_get_property(np, "fsl,mode", NULL);
	if (!sprop) {
		dev_err(&pdev->dev, "fsl,mode property is necessary\n");
		return -EINVAL;
	}
	if (!strcmp(sprop, "ac97-slave")) {
		ac97 = true;
	} else if (strcmp(sprop, "i2s-slave")) {
		dev_notice(&pdev->dev, "mode %s is unsupported\n", sprop);
		return -ENODEV;
	}

	/* The DAI name is the last part of the full name of the node. */
	p = strrchr(np->full_name, '/') + 1;
	ssi_private = devm_kzalloc(&pdev->dev, sizeof(*ssi_private) + strlen(p),
			      GFP_KERNEL);
	if (!ssi_private) {
		dev_err(&pdev->dev, "could not allocate DAI object\n");
		return -ENOMEM;
	}

	strcpy(ssi_private->name, p);

	ssi_private->use_dma = !of_property_read_bool(np,
			"fsl,fiq-stream-filter");

	if (ac97) {
		memcpy(&ssi_private->cpu_dai_drv, &fsl_ssi_ac97_dai,
				sizeof(fsl_ssi_ac97_dai));

		fsl_ac97_data = ssi_private;
		ssi_private->imx_ac97 = true;

		snd_soc_set_ac97_ops_of_reset(&fsl_ssi_ac97_ops, pdev);
	} else {
		/* Initialize this copy of the CPU DAI driver structure */
		memcpy(&ssi_private->cpu_dai_drv, &fsl_ssi_dai_template,
		       sizeof(fsl_ssi_dai_template));
	}
	ssi_private->cpu_dai_drv.name = ssi_private->name;

	/* Get the addresses and IRQ */
	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		dev_err(&pdev->dev, "could not determine device resources\n");
		return ret;
	}
	ssi_private->ssi_phys = res.start;

	iomem = devm_ioremap(&pdev->dev, res.start, resource_size(&res));
	if (!iomem) {
		dev_err(&pdev->dev, "could not map device resources\n");
		return -ENOMEM;
	}

	ssi_private->regs = devm_regmap_init_mmio(&pdev->dev, iomem,
			&fsl_ssi_regconfig);
	if (IS_ERR(ssi_private->regs)) {
		dev_err(&pdev->dev, "Failed to init register map\n");
		return PTR_ERR(ssi_private->regs);
	}

	ssi_private->irq = irq_of_parse_and_map(np, 0);
	if (!ssi_private->irq) {
		dev_err(&pdev->dev, "no irq for node %s\n", np->full_name);
		return -ENXIO;
	}

	/* Are the RX and the TX clocks locked? */
	if (!of_find_property(np, "fsl,ssi-asynchronous", NULL))
		ssi_private->cpu_dai_drv.symmetric_rates = 1;

	/* Determine the FIFO depth. */
	iprop = of_get_property(np, "fsl,fifo-depth", NULL);
	if (iprop)
		ssi_private->fifo_depth = be32_to_cpup(iprop);
	else
                /* Older 8610 DTs didn't have the fifo-depth property */
		ssi_private->fifo_depth = 8;

	if (of_device_is_compatible(pdev->dev.of_node, "fsl,imx21-ssi")) {
		u32 dma_events[2];
		ssi_private->ssi_on_imx = true;

<<<<<<< HEAD
		ssi_private->clk = devm_clk_get(&pdev->dev, NULL);
		if (IS_ERR(ssi_private->clk)) {
			ret = PTR_ERR(ssi_private->clk);
			dev_err(&pdev->dev, "could not get clock: %d\n", ret);
=======
	if (ssi_private->soc->imx) {
		ret = fsl_ssi_imx_probe(pdev, ssi_private, iomem);
		if (ret)
>>>>>>> ASoC: fsl-ssi: Use regmap
			goto error_irqmap;
		}
		ret = clk_prepare_enable(ssi_private->clk);
		if (ret) {
			dev_err(&pdev->dev, "clk_prepare_enable failed: %d\n",
				ret);
			goto error_irqmap;
		}

		/*
		 * We have burstsize be "fifo_depth - 2" to match the SSI
		 * watermark setting in fsl_ssi_startup().
		 */
		ssi_private->dma_params_tx.maxburst =
			ssi_private->fifo_depth - 2;
		ssi_private->dma_params_rx.maxburst =
			ssi_private->fifo_depth - 2;
		ssi_private->dma_params_tx.addr =
			ssi_private->ssi_phys + offsetof(struct ccsr_ssi, stx0);
		ssi_private->dma_params_rx.addr =
			ssi_private->ssi_phys + offsetof(struct ccsr_ssi, srx0);
		ssi_private->dma_params_tx.filter_data =
			&ssi_private->filter_data_tx;
		ssi_private->dma_params_rx.filter_data =
			&ssi_private->filter_data_rx;
		if (!of_property_read_bool(pdev->dev.of_node, "dmas") &&
				ssi_private->use_dma) {
			/*
			 * FIXME: This is a temporary solution until all
			 * necessary dma drivers support the generic dma
			 * bindings.
			 */
			ret = of_property_read_u32_array(pdev->dev.of_node,
					"fsl,ssi-dma-events", dma_events, 2);
			if (ret && ssi_private->use_dma) {
				dev_err(&pdev->dev, "could not get dma events but fsl-ssi is configured to use DMA\n");
				goto error_clk;
			}
		}

		shared = of_device_is_compatible(of_get_parent(np),
			    "fsl,spba-bus");

		imx_pcm_dma_params_init_data(&ssi_private->filter_data_tx,
			dma_events[0], shared ? IMX_DMATYPE_SSI_SP : IMX_DMATYPE_SSI);
		imx_pcm_dma_params_init_data(&ssi_private->filter_data_rx,
			dma_events[1], shared ? IMX_DMATYPE_SSI_SP : IMX_DMATYPE_SSI);
	} else if (ssi_private->use_dma) {
		/* The 'name' should not have any slashes in it. */
		ret = devm_request_irq(&pdev->dev, ssi_private->irq,
					fsl_ssi_isr, 0, ssi_private->name,
					ssi_private);
		if (ret < 0) {
			dev_err(&pdev->dev, "could not claim irq %u\n",
					ssi_private->irq);
			goto error_irqmap;
		}
	}

	/* Initialize the the device_attribute structure */
	dev_attr = &ssi_private->dev_attr;
	sysfs_attr_init(&dev_attr->attr);
	dev_attr->attr.name = "statistics";
	dev_attr->attr.mode = S_IRUGO;
	dev_attr->show = fsl_sysfs_ssi_show;

	ret = device_create_file(&pdev->dev, dev_attr);
	if (ret) {
		dev_err(&pdev->dev, "could not create sysfs %s file\n",
			ssi_private->dev_attr.attr.name);
		goto error_clk;
	}

	/* Register with ASoC */
	dev_set_drvdata(&pdev->dev, ssi_private);

	ret = snd_soc_register_component(&pdev->dev, &fsl_ssi_component,
					 &ssi_private->cpu_dai_drv, 1);
	if (ret) {
		dev_err(&pdev->dev, "failed to register DAI: %d\n", ret);
		goto error_dev;
	}

	if (ssi_private->ssi_on_imx) {
		if (!ssi_private->use_dma) {

			/*
			 * Some boards use an incompatible codec. To get it
			 * working, we are using imx-fiq-pcm-audio, that
			 * can handle those codecs. DMA is not possible in this
			 * situation.
			 */

			ssi_private->fiq_params.irq = ssi_private->irq;
			ssi_private->fiq_params.base = ssi_private->ssi;
			ssi_private->fiq_params.dma_params_rx =
				&ssi_private->dma_params_rx;
			ssi_private->fiq_params.dma_params_tx =
				&ssi_private->dma_params_tx;

			ret = imx_pcm_fiq_init(pdev, &ssi_private->fiq_params);
			if (ret)
				goto error_dev;
		} else {
			ret = imx_pcm_dma_init(pdev);
			if (ret)
				goto error_dev;
		}
	}

	/*
	 * If codec-handle property is missing from SSI node, we assume
	 * that the machine driver uses new binding which does not require
	 * SSI driver to trigger machine driver's probe.
	 */
	if (!of_get_property(np, "codec-handle", NULL)) {
		ssi_private->new_binding = true;
		goto done;
	}

	/* Trigger the machine driver's probe function.  The platform driver
	 * name of the machine driver is taken from /compatible property of the
	 * device tree.  We also pass the address of the CPU DAI driver
	 * structure.
	 */
	sprop = of_get_property(of_find_node_by_path("/"), "compatible", NULL);
	/* Sometimes the compatible name has a "fsl," prefix, so we strip it. */
	p = strrchr(sprop, ',');
	if (p)
		sprop = p + 1;
	snprintf(name, sizeof(name), "snd-soc-%s", sprop);
	make_lowercase(name);

	ssi_private->pdev =
		platform_device_register_data(&pdev->dev, name, 0, NULL, 0);
	if (IS_ERR(ssi_private->pdev)) {
		ret = PTR_ERR(ssi_private->pdev);
		dev_err(&pdev->dev, "failed to register platform: %d\n", ret);
		goto error_dai;
	}

done:
	if (ssi_private->imx_ac97)
		fsl_ssi_ac97_init();

	return 0;

error_dai:
	if (ssi_private->ssi_on_imx)
		imx_pcm_dma_exit(pdev);
	snd_soc_unregister_component(&pdev->dev);

error_dev:
	device_remove_file(&pdev->dev, dev_attr);

error_clk:
	if (ssi_private->ssi_on_imx)
		clk_disable_unprepare(ssi_private->clk);

error_irqmap:
	irq_dispose_mapping(ssi_private->irq);

	return ret;
}

static int fsl_ssi_remove(struct platform_device *pdev)
{
	struct fsl_ssi_private *ssi_private = dev_get_drvdata(&pdev->dev);

	if (!ssi_private->new_binding)
		platform_device_unregister(ssi_private->pdev);
	if (ssi_private->ssi_on_imx)
		imx_pcm_dma_exit(pdev);
	snd_soc_unregister_component(&pdev->dev);
	device_remove_file(&pdev->dev, &ssi_private->dev_attr);
	if (ssi_private->ssi_on_imx)
		clk_disable_unprepare(ssi_private->clk);
	irq_dispose_mapping(ssi_private->irq);

	return 0;
}

static const struct of_device_id fsl_ssi_ids[] = {
	{ .compatible = "fsl,mpc8610-ssi", },
	{ .compatible = "fsl,imx21-ssi", },
	{}
};
MODULE_DEVICE_TABLE(of, fsl_ssi_ids);

static struct platform_driver fsl_ssi_driver = {
	.driver = {
		.name = "fsl-ssi-dai",
		.owner = THIS_MODULE,
		.of_match_table = fsl_ssi_ids,
	},
	.probe = fsl_ssi_probe,
	.remove = fsl_ssi_remove,
};

module_platform_driver(fsl_ssi_driver);

MODULE_ALIAS("platform:fsl-ssi-dai");
MODULE_AUTHOR("Timur Tabi <timur@freescale.com>");
MODULE_DESCRIPTION("Freescale Synchronous Serial Interface (SSI) ASoC Driver");
MODULE_LICENSE("GPL v2");
