/*
 * Copyright 2004-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-licensisr_locke.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * @defgroup SPI Configurable Serial Peripheral Interface (CSPI) Driver
 */

/*!
 * @file mxc_spi.c
 * @brief This file contains the implementation of the SPI master controller services
 *
 *
 * @ingroup SPI
 */

#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/clk.h>

#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/gpio.h>

#ifdef CONFIG_ARCH_MX27
#include "mxc_spi_mx27.h"
#else
#include "mxc_spi.h"
#endif

#ifdef CONFIG_SPI_MXC_TEST_LOOPBACK
struct spi_chip_info {
	int lb_enable;
};

static struct spi_chip_info lb_chip_info = {
	.lb_enable = 1,
};

static struct spi_board_info loopback_info[] = {
#ifdef CONFIG_SPI_MXC_SELECT1
	{
	 .modalias = "loopback_spi",
	 .controller_data = &lb_chip_info,
	 .irq = 0,
	 .max_speed_hz = 4000000,
	 .bus_num = 1,
	 .chip_select = 4,
	 },
#endif
#ifdef CONFIG_SPI_MXC_SELECT2
	{
	 .modalias = "loopback_spi",
	 .controller_data = &lb_chip_info,
	 .irq = 0,
	 .max_speed_hz = 4000000,
	 .bus_num = 2,
	 .chip_select = 4,
	 },
#endif
#ifdef CONFIG_SPI_MXC_SELECT3
	{
	 .modalias = "loopback_spi",
	 .controller_data = &lb_chip_info,
	 .irq = 0,
	 .max_speed_hz = 4000000,
	 .bus_num = 3,
	 .chip_select = 4,
	 },
#endif
};
#endif

extern void gpio_spi_active(int cspi_mod);
extern void gpio_spi_inactive(int cspi_mod);

static struct mxc_spi_unique_def spi_ver_0_7 = {
	.intr_bit_shift = MXC_CSPIINT_IRQSHIFT_0_7,
	.cs_shift = MXC_CSPICTRL_CSSHIFT_0_7,
	.bc_shift = MXC_CSPICTRL_BCSHIFT_0_7,
	.bc_mask = MXC_CSPICTRL_BCMASK_0_7,
	.drctrl_shift = MXC_CSPICTRL_DRCTRLSHIFT_0_7,
	.xfer_complete = MXC_CSPISTAT_TC_0_7,
	.bc_overflow = MXC_CSPISTAT_BO_0_7,
};

static struct mxc_spi_unique_def spi_ver_0_5 = {
	.intr_bit_shift = MXC_CSPIINT_IRQSHIFT_0_5,
	.cs_shift = MXC_CSPICTRL_CSSHIFT_0_5,
	.bc_shift = MXC_CSPICTRL_BCSHIFT_0_5,
	.bc_mask = MXC_CSPICTRL_BCMASK_0_5,
	.drctrl_shift = MXC_CSPICTRL_DRCTRLSHIFT_0_5,
	.xfer_complete = MXC_CSPISTAT_TC_0_5,
	.bc_overflow = MXC_CSPISTAT_BO_0_5,
};

static struct mxc_spi_unique_def spi_ver_0_4 = {
	.intr_bit_shift = MXC_CSPIINT_IRQSHIFT_0_4,
	.cs_shift = MXC_CSPICTRL_CSSHIFT_0_4,
	.bc_shift = MXC_CSPICTRL_BCSHIFT_0_4,
	.bc_mask = MXC_CSPICTRL_BCMASK_0_4,
	.drctrl_shift = MXC_CSPICTRL_DRCTRLSHIFT_0_4,
	.xfer_complete = MXC_CSPISTAT_TC_0_4,
	.bc_overflow = MXC_CSPISTAT_BO_0_4,
};

static struct mxc_spi_unique_def spi_ver_0_0 = {
	.intr_bit_shift = MXC_CSPIINT_IRQSHIFT_0_0,
	.cs_shift = MXC_CSPICTRL_CSSHIFT_0_0,
	.bc_shift = MXC_CSPICTRL_BCSHIFT_0_0,
	.bc_mask = MXC_CSPICTRL_BCMASK_0_0,
	.drctrl_shift = MXC_CSPICTRL_DRCTRLSHIFT_0_0,
	.xfer_complete = MXC_CSPISTAT_TC_0_0,
	.bc_overflow = MXC_CSPISTAT_BO_0_0,
};

struct mxc_spi;
/*!
 * Structure to group together all the data buffers and functions
 * used in data transfers.
 */
struct mxc_spi_xfer {
	/*!
	 * Transmit buffer.
	 */
	const void *tx_buf;
	/*!
	 * Receive buffer.
	 */
	void *rx_buf;
	/*!
	 * Data transfered count.
	 */
	unsigned int count;

	/*!
	 * Function to read the FIFO data to rx_buf.
	 */
	void (*rx_get) (struct mxc_spi *, u32 val);
	/*!
	 * Function to get the data to be written to FIFO.
	 */
	 u32(*tx_get) (struct mxc_spi *);
};

/*!
 * This structure is a way for the low level driver to define their own
 * \b spi_master structure. This structure includes the core \b spi_master
 * structure that is provided by Linux SPI Framework/driver as an
 * element and has other elements that are specifically required by this
 * low-level driver.
 */
struct mxc_spi {
	/*!
	 * SPI Master and a simple I/O queue runner.
	 */
	struct spi_bitbang mxc_bitbang;
	/*!
	 * Completion flags used in data transfers.
	 */
	struct completion xfer_done;
	/*!
	 * Data transfer structure.
	 */
	struct mxc_spi_xfer transfer;
	/*!
	 * Resource structure, which will maintain base addresses and IRQs.
	 */
	struct resource *res;
	/*!
	 * Base address of CSPI, used in readl and writel.
	 */
	void *base;
	/*!
	 * CSPI IRQ number.
	 */
	int irq;
	/*!
	 * CSPI Clock id.
	 */
	struct clk *clk;
	/*!
	 * CSPI input clock SCLK.
	 */
	unsigned long spi_ipg_clk;
	/*!
	 * CSPI registers' bit pattern.
	 */
	struct mxc_spi_unique_def *spi_ver_def;
};

#define MXC_SPI_BUF_RX(type)	\
void mxc_spi_buf_rx_##type(struct mxc_spi *master_drv_data, u32 val)\
{\
	type *rx = master_drv_data->transfer.rx_buf;\
	*rx++ = (type)val;\
	master_drv_data->transfer.rx_buf = rx;\
}

#define MXC_SPI_BUF_TX(type)    \
u32 mxc_spi_buf_tx_##type(struct mxc_spi *master_drv_data)\
{\
	u32 val;\
	const type *tx = master_drv_data->transfer.tx_buf;\
	val = *tx++;\
	master_drv_data->transfer.tx_buf = tx;\
	return val;\
}
MXC_SPI_BUF_RX(u8)
    MXC_SPI_BUF_TX(u8)
    MXC_SPI_BUF_RX(u16)
    MXC_SPI_BUF_TX(u16)
    MXC_SPI_BUF_RX(u32)
    MXC_SPI_BUF_TX(u32)

/*!
 * This function enables CSPI interrupt(s)
 *
 * @param        master_data the pointer to mxc_spi structure
 * @param        irqs        the irq(s) to set (can be a combination)
 *
 * @return       This function returns 0 if successful, -1 otherwise.
 */
static int spi_enable_interrupt(struct mxc_spi *master_data, unsigned int irqs)
{
	if (irqs & ~((1 << master_data->spi_ver_def->intr_bit_shift) - 1)) {
		return -1;
	}

	__raw_writel((irqs | __raw_readl(master_data->base + MXC_CSPIINT)),
		     master_data->base + MXC_CSPIINT);

	return 0;
}

/*!
 * This function disables CSPI interrupt(s)
 *
 * @param        master_data the pointer to mxc_spi structure
 * @param        irqs        the irq(s) to reset (can be a combination)
 *
 * @return       This function returns 0 if successful, -1 otherwise.
 */
static int spi_disable_interrupt(struct mxc_spi *master_data, unsigned int irqs)
{
	if (irqs & ~((1 << master_data->spi_ver_def->intr_bit_shift) - 1)) {
		return -1;
	}

	__raw_writel((~irqs & __raw_readl(master_data->base + MXC_CSPIINT)),
		     master_data->base + MXC_CSPIINT);
	return 0;
}

/*!
 * This function sets the baud rate for the SPI module.
 *
 * @param        master_data the pointer to mxc_spi structure
 * @param        baud        the baud rate
 *
 * @return       This function returns the baud rate divisor.
 */
static unsigned int spi_find_baudrate(struct mxc_spi *master_data,
				      unsigned int baud)
{
	unsigned int divisor;
	unsigned int shift = 0;

	/* Calculate required divisor (rounded) */
	divisor = (master_data->spi_ipg_clk + baud / 2) / baud;
	while (divisor >>= 1)
		shift++;
	MXC_CSPICTRL_ADJUST_SHIFT(shift);
	if (shift > MXC_CSPICTRL_MAXDATRATE)
		shift = MXC_CSPICTRL_MAXDATRATE;

	return (shift << MXC_CSPICTRL_DATASHIFT);
}

/*!
 * This function gets the received data.
 *
 * @param        base   the CSPI base address
 *
 * @return       This function returns Rx FIFO data read.
 */
static unsigned int spi_get_rx_data(void *base)
{
	return __raw_readl(base + MXC_CSPIRXDATA);
}

/*!
 * This function loads the transmit fifo.
 *
 * @param        base   the CSPI base address
 * @param        val    the data to put in the TxFIFO
 */
static void spi_put_tx_data(void *base, unsigned int val)
{
	unsigned int ctrl_reg;

	__raw_writel(val, base + MXC_CSPITXDATA);

	ctrl_reg = __raw_readl(base + MXC_CSPICTRL);

	ctrl_reg |= MXC_CSPICTRL_XCH;

	__raw_writel(ctrl_reg, base + MXC_CSPICTRL);

	return;
}

/*!
 * This function configures the hardware CSPI for the current SPI device.
 * It sets the word size, transfer mode, data rate for this device.
 *
 * @param       spi     	the current SPI device
 * @param	is_active 	indicates whether to active/deactivate the current device
 */
void mxc_spi_chipselect(struct spi_device *spi, int is_active)
{
	struct mxc_spi *master_drv_data;
	struct mxc_spi_xfer *ptransfer;
	struct mxc_spi_unique_def *spi_ver_def;
	unsigned int ctrl_reg;
	unsigned int ctrl_mask;
	unsigned int xfer_len;

	if (is_active == BITBANG_CS_INACTIVE) {
		/*Need to deselect the slave */
		return;
	}

	/* Get the master controller driver data from spi device's master */

	master_drv_data = spi_master_get_devdata(spi->master);
	spi_ver_def = master_drv_data->spi_ver_def;

	xfer_len = spi->bits_per_word;

	/* Control Register Settings for transfer to this slave */

	ctrl_reg = __raw_readl(master_drv_data->base + MXC_CSPICTRL);

	ctrl_mask =
	    (MXC_CSPICTRL_LOWPOL | MXC_CSPICTRL_PHA | MXC_CSPICTRL_HIGHSSPOL |
	     MXC_CSPICTRL_CSMASK << spi_ver_def->cs_shift |
	     MXC_CSPICTRL_DATAMASK << MXC_CSPICTRL_DATASHIFT |
	     spi_ver_def->bc_mask << spi_ver_def->bc_shift);
	ctrl_reg &= ~ctrl_mask;

	ctrl_reg |=
	    ((spi->chip_select & MXC_CSPICTRL_CSMASK) << spi_ver_def->cs_shift);
	ctrl_reg |= spi_find_baudrate(master_drv_data, spi->max_speed_hz);
	ctrl_reg |=
	    (((xfer_len - 1) & spi_ver_def->bc_mask) << spi_ver_def->bc_shift);
	if (spi->mode & SPI_CPHA)
		ctrl_reg |= MXC_CSPICTRL_PHA;
	if (!(spi->mode & SPI_CPOL))
		ctrl_reg |= MXC_CSPICTRL_LOWPOL;
	if (spi->mode & SPI_CS_HIGH)
		ctrl_reg |= MXC_CSPICTRL_HIGHSSPOL;

	__raw_writel(ctrl_reg, master_drv_data->base + MXC_CSPICTRL);

	/* Initialize the functions for transfer */
	ptransfer = &master_drv_data->transfer;
	if (xfer_len <= 8) {
		ptransfer->rx_get = mxc_spi_buf_rx_u8;
		ptransfer->tx_get = mxc_spi_buf_tx_u8;
	} else if (xfer_len <= 16) {
		ptransfer->rx_get = mxc_spi_buf_rx_u16;
		ptransfer->tx_get = mxc_spi_buf_tx_u16;
	} else if (xfer_len <= 32) {
		ptransfer->rx_get = mxc_spi_buf_rx_u32;
		ptransfer->tx_get = mxc_spi_buf_tx_u32;
	}
#ifdef CONFIG_SPI_MXC_TEST_LOOPBACK
	{
		struct spi_chip_info *lb_chip =
		    (struct spi_chip_info *)spi->controller_data;
		if (!lb_chip)
			__raw_writel(0, master_drv_data->base + MXC_CSPITEST);
		else if (lb_chip->lb_enable)
			__raw_writel(MXC_CSPITEST_LBC,
				     master_drv_data->base + MXC_CSPITEST);
	}
#endif
	return;
}

/*!
 * This function is called when an interrupt occurs on the SPI modules.
 * It is the interrupt handler for the SPI modules.
 *
 * @param        irq        the irq number
 * @param        dev_id     the pointer on the device
 *
 * @return       The function returns IRQ_HANDLED when handled.
 */
static irqreturn_t mxc_spi_isr(int irq, void *dev_id)
{
	struct mxc_spi *master_drv_data = dev_id;
	irqreturn_t ret = IRQ_NONE;
	unsigned int status;

	/* Read the interrupt status register to determine the source */
	status = __raw_readl(master_drv_data->base + MXC_CSPISTAT);

	/* Rx is given higher priority - Handle it first */
	if (status & MXC_CSPISTAT_RR) {
		u32 rx_tmp = spi_get_rx_data(master_drv_data->base);

		if (master_drv_data->transfer.rx_buf)
			master_drv_data->transfer.rx_get(master_drv_data,
							 rx_tmp);

		ret = IRQ_HANDLED;
	}

	(master_drv_data->transfer.count)--;
	/* Handle the Tx now */
	if (master_drv_data->transfer.count) {
		if (master_drv_data->transfer.tx_buf) {
			u32 tx_tmp =
			    master_drv_data->transfer.tx_get(master_drv_data);

			spi_put_tx_data(master_drv_data->base, tx_tmp);
		}
	} else {
		complete(&master_drv_data->xfer_done);
	}

	/* Clear the interrupt status */
	//__raw_writel(spi_ver_def->spi_status_transfer_complete,
	//           master_drv_data->base + MXC_CSPISTAT);

	return ret;
}

/*!
 * This function initialize the current SPI device.
 *
 * @param        spi     the current SPI device.
 *
 */
int mxc_spi_setup(struct spi_device *spi)
{
	struct mxc_spi *master_data = spi_master_get_devdata(spi->master);

	if ((spi->max_speed_hz < 0)
	    || (spi->max_speed_hz > (master_data->spi_ipg_clk / 4)))
		return -EINVAL;

	if (!spi->bits_per_word)
		spi->bits_per_word = 8;

	pr_debug("%s: mode %d, %u bpw, %d hz\n", __FUNCTION__,
		 spi->mode, spi->bits_per_word, spi->max_speed_hz);

	return 0;
}

/*!
 * This function is called when the data has to transfer from/to the
 * current SPI device. It enables the Rx interrupt, initiates the transfer.
 * When Rx interrupt occurs, the completion flag is set. It then disables
 * the Rx interrupt.
 *
 * @param        spi        the current spi device
 * @param        t          the transfer request - read/write buffer pairs
 *
 * @return       Returns 0 on success -1 on failure.
 */
int mxc_spi_transfer(struct spi_device *spi, struct spi_transfer *t)
{
	struct mxc_spi *master_drv_data = NULL;

	/* Get the master controller driver data from spi device's master */

	master_drv_data = spi_master_get_devdata(spi->master);

	/* Modify the Tx, Rx, Count */

	master_drv_data->transfer.tx_buf = t->tx_buf;
	master_drv_data->transfer.rx_buf = t->rx_buf;
	master_drv_data->transfer.count = t->len;
	INIT_COMPLETION(master_drv_data->xfer_done);

	/* Enable the Rx Interrupts */

	spi_enable_interrupt(master_drv_data, MXC_CSPIINT_RREN);

	/* Perform single Tx transaction */

	spi_put_tx_data(master_drv_data->base,
			master_drv_data->transfer.tx_get(master_drv_data));

	/* Wait for transfer completion */

	wait_for_completion(&master_drv_data->xfer_done);

	/* Disable the Rx Interrupts */

	spi_disable_interrupt(master_drv_data, MXC_CSPIINT_RREN);

	return (t->len - master_drv_data->transfer.count);
}

/*!
 * This function releases the current SPI device's resources.
 *
 * @param        spi     the current SPI device.
 *
 */
void mxc_spi_cleanup(struct spi_device *spi)
{
}

/*!
 * This function is called during the driver binding process. Based on the CSPI
 * hardware module that is being probed this function adds the appropriate SPI module
 * structure in the SPI core driver.
 *
 * @param   pdev  the device structure used to store device specific
 *                information that is used by the suspend, resume and remove
 *                functions.
 *
 * @return  The function returns 0 on successful registration and initialization
 *          of CSPI module. Otherwise returns specific error code.
 */
static int mxc_spi_probe(struct platform_device *pdev)
{
	struct mxc_spi_master *mxc_platform_info;
	struct spi_master *master;
	struct mxc_spi *master_drv_data = NULL;
	unsigned int spi_ver;
	int ret = -ENODEV;

	/* Get the platform specific data for this master device */

	mxc_platform_info = (struct mxc_spi_master *)pdev->dev.platform_data;
	if (!mxc_platform_info) {
		dev_err(&pdev->dev, "can't get the platform data for CSPI\n");
		return -EINVAL;
	}

	/* Allocate SPI master controller */

	master = spi_alloc_master(&pdev->dev, sizeof(struct mxc_spi));
	if (!master) {
		dev_err(&pdev->dev, "can't alloc for spi_master\n");
		return -ENOMEM;
	}

	/* Set this device's driver data to master */

	platform_set_drvdata(pdev, master);

	/* Set this master's data from platform_info */

	master->bus_num = pdev->id + 1;
	master->num_chipselect = mxc_platform_info->maxchipselect;

	/* Set the master controller driver data for this master */

	master_drv_data = spi_master_get_devdata(master);
	master_drv_data->mxc_bitbang.master = spi_master_get(master);

	/* Set the master bitbang data */

	master_drv_data->mxc_bitbang.chipselect = mxc_spi_chipselect;
	master_drv_data->mxc_bitbang.txrx_bufs = mxc_spi_transfer;
	master_drv_data->mxc_bitbang.master->setup = mxc_spi_setup;
	master_drv_data->mxc_bitbang.master->cleanup = mxc_spi_cleanup;

	/* Initialize the completion object */

	init_completion(&master_drv_data->xfer_done);

	/* Set the master controller register addresses and irqs */

	master_drv_data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!master_drv_data->res) {
		dev_err(&pdev->dev, "can't get platform resource for CSPI%d\n",
			master->bus_num);
		ret = -ENOMEM;
		goto err;
	}

	if (!request_mem_region(master_drv_data->res->start,
				master_drv_data->res->end -
				master_drv_data->res->start + 1, pdev->name)) {
		dev_err(&pdev->dev, "request_mem_region failed for CSPI%d\n",
			master->bus_num);
		ret = -ENOMEM;
		goto err;
	}

	master_drv_data->base = (void *)IO_ADDRESS(master_drv_data->res->start);
	if (!master_drv_data->base) {
		dev_err(&pdev->dev, "invalid base address for CSPI%d\n",
			master->bus_num);
		ret = -EINVAL;
		goto err1;
	}

	master_drv_data->irq = platform_get_irq(pdev, 0);
	if (!master_drv_data->irq) {
		dev_err(&pdev->dev, "can't get IRQ for CSPI%d\n",
			master->bus_num);
		ret = -EINVAL;
		goto err1;
	}

	/* Register for SPI Interrupt */

	ret = request_irq(master_drv_data->irq, mxc_spi_isr,
			  0, "CSPI_IRQ", master_drv_data);
	if (ret != 0) {
		dev_err(&pdev->dev, "request_irq failed for CSPI%d\n",
			master->bus_num);
		goto err1;
	}

	/* Setup any GPIO active */

	gpio_spi_active(master->bus_num - 1);

	/* Identify SPI version */

	spi_ver = mxc_platform_info->spi_version;
	if (spi_ver == 7) {
		master_drv_data->spi_ver_def = &spi_ver_0_7;
	} else if (spi_ver == 5) {
		master_drv_data->spi_ver_def = &spi_ver_0_5;
	} else if (spi_ver == 4) {
		master_drv_data->spi_ver_def = &spi_ver_0_4;
	} else if (spi_ver == 0) {
		master_drv_data->spi_ver_def = &spi_ver_0_0;
	}

	dev_dbg(&pdev->dev, "SPI_REV 0.%d\n", spi_ver);

	/* Enable the CSPI Clock, CSPI Module, set as a master */

	master_drv_data->clk = clk_get(&pdev->dev, "cspi_clk");
	clk_enable(master_drv_data->clk);
	master_drv_data->spi_ipg_clk = clk_get_rate(master_drv_data->clk);

	__raw_writel(MXC_CSPIRESET_START,
		     master_drv_data->base + MXC_CSPIRESET);
	udelay(1);
	__raw_writel(MXC_CSPICTRL_ENABLE | MXC_CSPICTRL_MASTER,
		     master_drv_data->base + MXC_CSPICTRL);
	__raw_writel(MXC_CSPIPERIOD_32KHZ,
		     master_drv_data->base + MXC_CSPIPERIOD);
	__raw_writel(0, master_drv_data->base + MXC_CSPIINT);

	/* Start the SPI Master Controller driver */

	ret = spi_bitbang_start(&master_drv_data->mxc_bitbang);
	if (ret != 0)
		goto err2;

	printk(KERN_INFO "CSPI: %s-%d probed\n", pdev->name, pdev->id);

#ifdef CONFIG_SPI_MXC_TEST_LOOPBACK
	{
		int i;
		struct spi_board_info *bi = &loopback_info[0];
		for (i = 0; i < ARRAY_SIZE(loopback_info); i++, bi++) {
			if (bi->bus_num != master->bus_num)
				continue;

			dev_info(&pdev->dev,
				 "registering loopback device '%s'\n",
				 bi->modalias);

			spi_new_device(master, bi);
		}
	}
#endif
	return ret;

      err2:
	gpio_spi_inactive(master->bus_num - 1);
	clk_disable(master_drv_data->clk);
	clk_put(master_drv_data->clk);
	free_irq(master_drv_data->irq, master_drv_data);
      err1:
	release_mem_region(pdev->resource[0].start,
			   pdev->resource[0].end - pdev->resource[0].start + 1);
      err:
	spi_master_put(master);
	kfree(master);
	platform_set_drvdata(pdev, NULL);
	return ret;
}

/*!
 * Dissociates the driver from the SPI master controller. Disables the CSPI module.
 * It handles the release of SPI resources like IRQ, memory,..etc.
 *
 * @param   pdev  the device structure used to give information on which SPI
 *                to remove
 *
 * @return  The function always returns 0.
 */
static int mxc_spi_remove(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);

	if (master) {
		struct mxc_spi *master_drv_data =
		    spi_master_get_devdata(master);

		gpio_spi_inactive(master->bus_num - 1);
		clk_disable(master_drv_data->clk);

		/* Disable the CSPI module */

		__raw_writel(MXC_CSPICTRL_DISABLE,
			     master_drv_data->base + MXC_CSPICTRL);

		/* Unregister for SPI Interrupt */

		free_irq(master_drv_data->irq, master_drv_data);

		release_mem_region(master_drv_data->res->start,
				   master_drv_data->res->end -
				   master_drv_data->res->start + 1);

		/* Stop the SPI Master Controller driver */

		spi_bitbang_stop(&master_drv_data->mxc_bitbang);

		spi_master_put(master);
	}

	printk(KERN_INFO "CSPI: %s-%d removed\n", pdev->name, pdev->id);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int suspend_devices(struct device *dev, void *pm_message)
{
	pm_message_t *state = pm_message;

	if (dev->power.power_state.event != state->event) {
		dev_warn(dev, "mismatch in pm state request\n");
		return -1;
	}

	return 0;
}

static int spi_bitbang_suspend(struct spi_bitbang *bitbang)
{
	unsigned long flags;
	unsigned limit = 500;

	spin_lock_irqsave(&bitbang->lock, flags);
	while (!list_empty(&bitbang->queue) && limit--) {
		spin_unlock_irqrestore(&bitbang->lock, flags);

		dev_dbg(bitbang->master->cdev.dev, "wait for queue\n");
		msleep(10);

		spin_lock_irqsave(&bitbang->lock, flags);
	}
	if (!list_empty(&bitbang->queue)) {
		dev_err(bitbang->master->cdev.dev, "queue didn't empty\n");
		return -EBUSY;
	}
	spin_unlock_irqrestore(&bitbang->lock, flags);

	return 0;
}

static void spi_bitbang_resume(struct spi_bitbang *bitbang)
{
	spin_lock_init(&bitbang->lock);
	INIT_LIST_HEAD(&bitbang->queue);

	bitbang->busy = 0;
}

/*!
 * This function puts the SPI master controller in low-power mode/state.
 *
 * @param   pdev  the device structure used to give information on which SDHC
 *                to suspend
 * @param   state the power state the device is entering
 *
 * @return  The function always returns 0.
 */
static int mxc_spi_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct mxc_spi *master_drv_data = spi_master_get_devdata(master);
	int ret = 0;

	if (device_for_each_child(&pdev->dev, &state, suspend_devices) != 0) {
		dev_warn(&pdev->dev, "suspend aborted\n");
		return -EINVAL;
	}

	spi_bitbang_suspend(&master_drv_data->mxc_bitbang);
	__raw_writel(MXC_CSPICTRL_DISABLE,
		     master_drv_data->base + MXC_CSPICTRL);

	clk_disable(master_drv_data->clk);
	gpio_spi_inactive(master->bus_num - 1);

	return ret;
}

/*!
 * This function brings the SPI master controller back from low-power state.
 *
 * @param   pdev  the device structure used to give information on which SDHC
 *                to resume
 *
 * @return  The function always returns 0.
 */
static int mxc_spi_resume(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct mxc_spi *master_drv_data = spi_master_get_devdata(master);

	gpio_spi_active(master->bus_num - 1);
	clk_enable(master_drv_data->clk);

	spi_bitbang_resume(&master_drv_data->mxc_bitbang);
	__raw_writel(MXC_CSPICTRL_ENABLE | MXC_CSPICTRL_MASTER,
		     master_drv_data->base + MXC_CSPICTRL);

	return 0;
}
#else
#define mxc_spi_suspend  NULL
#define mxc_spi_resume   NULL
#endif				/* CONFIG_PM */

/*!
 * This structure contains pointers to the power management callback functions.
 */
static struct platform_driver mxc_spi_driver = {
	.driver = {
		   .name = "mxc_spi",
		   .bus = &platform_bus_type,
		   .owner = THIS_MODULE,
		   },
	.probe = mxc_spi_probe,
	.remove = mxc_spi_remove,
	.suspend_late = mxc_spi_suspend,
	.resume_early = mxc_spi_resume,
};

/*!
 * This function implements the init function of the SPI device.
 * It is called when the module is loaded. It enables the required
 * clocks to CSPI module(if any) and activates necessary GPIO pins.
 *
 * @return       This function returns 0.
 */
static int __init mxc_spi_init(void)
{
	pr_debug("Registering the SPI Controller Driver\n");
	return platform_driver_register(&mxc_spi_driver);
}

/*!
 * This function implements the exit function of the SPI device.
 * It is called when the module is unloaded. It deactivates the
 * the GPIO pin associated with CSPI hardware modules.
 *
 */
static void __exit mxc_spi_exit(void)
{
	pr_debug("Unregistering the SPI Controller Driver\n");
	platform_driver_unregister(&mxc_spi_driver);
}

subsys_initcall(mxc_spi_init);
module_exit(mxc_spi_exit);

MODULE_DESCRIPTION("SPI Master Controller driver");
MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_LICENSE("GPL");
