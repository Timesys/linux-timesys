/*
 *  Driver for Freescale MVF serial ports
 *
 *  Based on drivers/char/imx.c.
 *
 *  Copyright 2012 Freescale Semiconductor, Inc.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#if defined(CONFIG_SERIAL_IMX_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/rational.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>

#include <linux/io.h>
#include <asm/irq.h>
#include <mach/dma.h>
#include <mach/hardware.h>
#include <mach/imx-uart.h>
#include <mach/mxc_uart.h>
#include <mach/mvf.h>
#include <mach/mcf_edma.h>

#define CONFIG_MVF_SERIAL_DMA
/* We've been assigned a range on the "Low-density serial ports" major */
#define SERIAL_IMX_MAJOR        207
#define MINOR_START	        16
#define DEV_NAME		"ttymxc"
#define MAX_INTERNAL_IRQ	MXC_INTERNAL_IRQS

/*
 * This determines how often we check the modem status signals
 * for any change.  They generally aren't connected to an IRQ
 * so we have to poll them.  We also check immediately before
 * filling the TX fifo incase CTS has been dropped.
 */
#define MCTRL_TIMEOUT	(250*HZ/1000)

#define DRIVER_NAME "IMX-uart"

#define UART_NR 6

struct imx_port {
	struct uart_port	port;
	struct timer_list	timer;
	unsigned int		old_status;
	unsigned int		have_rtscts:1;
	unsigned int		use_dcedte:1;
	unsigned int		use_irda:1;
	unsigned int		irda_inv_rx:1;
	unsigned int		irda_inv_tx:1;
	unsigned int		fifo_en:1; /* enable FIFO mode */
	unsigned int		mark_en:1; /* enable Mark address match */
	unsigned int		format_9bits:1; /* 9bits data format */
	unsigned short		trcv_delay; /* transceiver delay */
	unsigned char		ma_addr; /* Match address */
	struct clk			*clk;

	unsigned int		tx_fifo_size, rx_fifo_size;

	/* DMA fields */
	int					enable_dma;
	unsigned long		dma_tx_ch; /* configured eDMA channel */
	struct imx_dma_data	dma_data;
	struct dma_chan		*dma_chan_rx, *dma_chan_tx;
	struct scatterlist	rx_sgl, tx_sgl;
	void				*rx_buf;
	unsigned char		*tx_buf;
	unsigned int		rx_bytes, tx_bytes;
	struct work_struct	tsk_rx, tsk_dma_tx;
	unsigned int		dma_tx_nents;
	bool				dma_is_rxing, dma_is_txing;
	wait_queue_head_t	dma_wait;
};

/*
 * Handle any change of modem status signal since we were last called.
 */
static void imx_mctrl_check(struct imx_port *sport)
{
	unsigned int status, changed;

	status = sport->port.ops->get_mctrl(&sport->port);
	changed = status ^ sport->old_status;

	if (changed == 0)
		return;

	sport->old_status = status;

	if (changed & TIOCM_RI)
		sport->port.icount.rng++;
	if (changed & TIOCM_DSR)
		sport->port.icount.dsr++;
	if (changed & TIOCM_CAR)
		uart_handle_dcd_change(&sport->port, status & TIOCM_CAR);
	if (changed & TIOCM_CTS)
		uart_handle_cts_change(&sport->port, status & TIOCM_CTS);

	wake_up_interruptible(&sport->port.state->port.delta_msr_wait);
}

/*
 * This is our per-port timeout handler, for checking the
 * modem status signals.
 */
static void imx_timeout(unsigned long data)
{
	struct imx_port *sport = (struct imx_port *)data;
	unsigned long flags;

	if (sport->port.state) {
		spin_lock_irqsave(&sport->port.lock, flags);
		imx_mctrl_check(sport);
		spin_unlock_irqrestore(&sport->port.lock, flags);

		mod_timer(&sport->timer, jiffies + MCTRL_TIMEOUT);
	}
}

/*
 * interrupts disabled on entry
 */
static void imx_stop_tx(struct uart_port *port)
{
	struct imx_port *sport = (struct imx_port *)port;
	unsigned char temp;

	temp = readb(sport->port.membase + MXC_UARTCR2);
	writeb(temp & ~(MXC_UARTCR2_TIE | MXC_UARTCR2_TCIE),
			sport->port.membase + MXC_UARTCR2);
}

/*
 * interrupts disabled on entry
 */
static void imx_stop_rx(struct uart_port *port)
{
	struct imx_port *sport = (struct imx_port *)port;
	unsigned char temp;

	/*
	 * We are in SMP now, so if the DMA RX thread is running,
	 * we have to wait for it to finish.
	 */
	if (sport->enable_dma && sport->dma_is_rxing)
		return;

	temp = readb(sport->port.membase + MXC_UARTCR2);
	writeb(temp & ~MXC_UARTCR2_RE, sport->port.membase + MXC_UARTCR2);
}

/*
 * Set the modem control timer to fire immediately.
 */
static void imx_enable_ms(struct uart_port *port)
{
	/*
	 * To Be Implemented
	 */
	return ;
}

static inline void imx_transmit_buffer(struct imx_port *sport)
{
	struct circ_buf *xmit = &sport->port.state->xmit;

	while (!uart_circ_empty(xmit) && (sport->fifo_en == 1 ?
	(readb(sport->port.membase + MXC_UARTTCFIFO) < sport->tx_fifo_size) :
		(readb(sport->port.membase + MXC_UARTSR1) & MXC_UARTSR1_TDRE)
		)) {
		/* send xmit->buf[xmit->tail]
		 * out the port here */
		writeb(xmit->buf[xmit->tail], sport->port.membase + MXC_UARTDR);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		sport->port.icount.tx++;
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
	{
		if (sport->port.state->port.tty)
		{
		uart_write_wakeup(&sport->port);
		}
	}

	if (uart_circ_empty(xmit))
		imx_stop_tx(&sport->port);
}

#ifdef CONFIG_MVF_SERIAL_DMA
/****************************************************************************/
/* Configure and start DMA engine. */
static void tx_uart_dmarun(struct imx_port *sport)
{
	if (sport->dma_tx_nents == 0)
		return;

	mcf_edma_set_tcd_params(sport->dma_tx_ch, /* channel number */
			(u32)sg_phys(&sport->tx_sgl), /* source */
			(u32)(sport->port.mapbase + MXC_UARTDR), /* dest */
			MCF_EDMA_TCD_ATTR_SSIZE_8BIT |
			MCF_EDMA_TCD_ATTR_DSIZE_8BIT, /* attr */
			1, /* soff */
			1, /* nbytes */
			0, /* slast */
			sport->tx_bytes, /* citer */
			0, /* biter */
			0, /* doff */
			0, /* dlast_sga */
			1, /* major_int */
			1, /* disable_req */
			0  /* enabled sg */
			);
	sport->dma_is_txing = 1;
	mcf_edma_enable_transfer(sport->dma_tx_ch);
}

/****************************************************************************/
static irqreturn_t dma_tx_callback(int ch, void *data)
{
	struct imx_port *sport = data;
	struct circ_buf *xmit = &sport->port.state->xmit;

	mcf_edma_stop_transfer(sport->dma_tx_ch);

	dma_unmap_sg(sport->port.dev, &sport->tx_sgl, 1, DMA_TO_DEVICE);

	spin_lock(&sport->port.lock);
	xmit->tail = (xmit->tail + sport->tx_bytes) & (UART_XMIT_SIZE - 1);
	sport->port.icount.tx += sport->tx_bytes;
	spin_unlock(&sport->port.lock);

	sport->dma_is_txing = 0;

	schedule_work(&sport->tsk_dma_tx);

	return IRQ_HANDLED;
}

static void dma_tx_work(struct work_struct *w)
{
	struct imx_port *sport = container_of(w, struct imx_port, tsk_dma_tx);
	struct circ_buf *xmit = &sport->port.state->xmit;
	unsigned long flags;

	if (sport->dma_is_txing)
		return;

	spin_lock_irqsave(&sport->port.lock, flags);
	sport->tx_bytes = uart_circ_chars_pending(xmit);

	if (sport->tx_bytes > 0) {
		sport->dma_tx_nents = 0;
		if (xmit->tail > xmit->head) {
			sg_init_one(&sport->tx_sgl, xmit->buf + xmit->tail,
					UART_XMIT_SIZE - xmit->tail);
			sport->dma_tx_nents = UART_XMIT_SIZE - xmit->tail;
			sport->tx_bytes = sport->dma_tx_nents;
		} else {
			sg_init_one(&sport->tx_sgl, xmit->buf + xmit->tail,
					sport->tx_bytes);
			sport->dma_tx_nents = sport->tx_bytes;
		}

		dma_map_sg(sport->port.dev, &sport->tx_sgl, 1, DMA_TO_DEVICE);
		/* fire it */
		tx_uart_dmarun(sport);
	}

	spin_unlock_irqrestore(&sport->port.lock, flags);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
	{
		if (sport->port.state->port.tty)
		{
			uart_write_wakeup(&sport->port);
		}
	}

	return;
}
#endif

/*
 * interrupts disabled on entry
 */
static void imx_start_tx(struct uart_port *port)
{
	struct imx_port *sport = (struct imx_port *)port;
	unsigned char temp;

	if (!sport->enable_dma) {
		temp = readb(sport->port.membase + MXC_UARTCR2);
		writeb(temp | MXC_UARTCR2_TIE,
				sport->port.membase + MXC_UARTCR2);
	}
	if (sport->enable_dma) {
		schedule_work(&sport->tsk_dma_tx);
		return;
	}

	if (readb(sport->port.membase + MXC_UARTSR1) & MXC_UARTSR1_TDRE)
		imx_transmit_buffer(sport);
}


static irqreturn_t imx_txint(int irq, void *dev_id)
{
	struct imx_port *sport = dev_id;
	struct circ_buf *xmit = &sport->port.state->xmit;
	unsigned long flags;

	spin_lock_irqsave(&sport->port.lock, flags);
	if (sport->port.x_char) {
		/* Send next char */
		writeb(sport->port.x_char, sport->port.membase + MXC_UARTDR);
		goto out;
	}

	if (uart_circ_empty(xmit) || uart_tx_stopped(&sport->port)) {
		imx_stop_tx(&sport->port);
		goto out;
	}

	imx_transmit_buffer(sport);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
	{
		if (sport->port.state->port.tty)
		{
			uart_write_wakeup(&sport->port);
		}
	}

out:
	spin_unlock_irqrestore(&sport->port.lock, flags);
	return IRQ_HANDLED;
}

static void rx_work(struct work_struct *w)
{
	struct imx_port *sport = container_of(w, struct imx_port, tsk_rx);
	struct tty_struct *tty = sport->port.state->port.tty;

	if (sport->rx_bytes) {
		tty_flip_buffer_push(tty);
		sport->rx_bytes = 0;
	}
}

static irqreturn_t imx_rxint(int irq, void *dev_id)
{
	struct imx_port *sport = dev_id;
	unsigned int flg, ignored = 0;
	struct tty_struct *tty = sport->port.state->port.tty;
	unsigned long flags;
	unsigned char r8, rx, sr;

	spin_lock_irqsave(&sport->port.lock, flags);

	while (!(readb(sport->port.membase + MXC_UARTSFIFO) &
				MXC_UARTSFIFO_RXEMPT)) {
		flg = TTY_NORMAL;
		sport->port.icount.rx++;

		/*  To clear the FE, OR, NF, FE, PE flags when set,
		 *  read SR1 then read DR
		 */
		sr = readb(sport->port.membase + MXC_UARTSR1);

		r8 = readb(sport->port.membase + MXC_UARTCR3) & MXC_UARTCR3_R8;
		rx = readb(sport->port.membase + MXC_UARTDR);

		if (sport->mark_en &&
			(sport->format_9bits ? r8 : (rx == sport->ma_addr)))
				continue;

		if (uart_handle_sysrq_char(&sport->port, (unsigned char)rx))
			continue;
		if (sr & (MXC_UARTSR1_PE | MXC_UARTSR1_OR | MXC_UARTSR1_FE)) {
			if (sr & MXC_UARTSR1_PE)
				sport->port.icount.parity++;
			else if (sr & MXC_UARTSR1_FE)
				sport->port.icount.frame++;
			if (sr & MXC_UARTSR1_OR)
				sport->port.icount.overrun++;

			if (sr & sport->port.ignore_status_mask) {
				if (++ignored > 100)
					goto out;
				continue;
			}

			sr &= sport->port.read_status_mask;

			if (sr & MXC_UARTSR1_PE)
				flg = TTY_PARITY;
			else if (sr & MXC_UARTSR1_FE)
				flg = TTY_FRAME;
			if (sr & MXC_UARTSR1_OR)
				flg = TTY_OVERRUN;

#ifdef SUPPORT_SYSRQ
			sport->port.sysrq = 0;
#endif
		}

		/*
		uart_insert_char(sport->port, sr, MXC_UARTSR1_OR, rx, flg);
		*/
		tty_insert_flip_char(tty, rx, flg);
		sport->rx_bytes++;
	}

out:
	spin_unlock_irqrestore(&sport->port.lock, flags);

	if (sport->rx_bytes) {
		tty_flip_buffer_push(tty);
		sport->rx_bytes = 0;
	}
	return IRQ_HANDLED;
}

static irqreturn_t imx_int(int irq, void *dev_id)
{
	struct imx_port *sport = dev_id;
	unsigned int sts;

	sts = readb(sport->port.membase + MXC_UARTSR1);

	if (sts & MXC_UARTSR1_RDRF)
			imx_rxint(irq, dev_id);

	if (sts & MXC_UARTSR1_TDRE &&
		!(readb(sport->port.membase + MXC_UARTCR5) &
			MXC_UARTCR5_TDMAS))
		imx_txint(irq, dev_id);
/*
	if (sts & USR1_AWAKE)
		writel(USR1_AWAKE, sport->port.membase + USR1);
*/
	return IRQ_HANDLED;
}

/*
 * Return TIOCSER_TEMT when transmitter is not busy.
 */
static unsigned int imx_tx_empty(struct uart_port *port)
{
	struct imx_port *sport = (struct imx_port *)port;

	return (readb(sport->port.membase + MXC_UARTSR1) & MXC_UARTSR1_TC) ?
		TIOCSER_TEMT : 0;
}

/*
 * We have a modem side uart, so the meanings of RTS and CTS are inverted.
 */
static unsigned int imx_get_mctrl(struct uart_port *port)
{
	struct imx_port *sport = (struct imx_port *)port;
	unsigned int tmp = 0;

	if (readb(sport->port.membase + MXC_UARTMODEM) & MXC_UARTMODEM_TXCTSE)
		tmp |= TIOCM_CTS;

	if (readb(sport->port.membase + MXC_UARTMODEM) & MXC_UARTMODEM_RXRTSE)
		tmp |= TIOCM_RTS;

	return tmp;
}

static void imx_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct imx_port *sport = (struct imx_port *)port;
	unsigned long temp;

	temp = readb(sport->port.membase + MXC_UARTMODEM) &
					~(MXC_UARTMODEM_RXRTSE | MXC_UARTMODEM_TXCTSE);

	if (mctrl & TIOCM_RTS)
		temp |= MXC_UARTMODEM_RXRTSE;

	if (mctrl & TIOCM_CTS)
		temp |= MXC_UARTMODEM_TXCTSE;

	writeb(temp, sport->port.membase + MXC_UARTMODEM);
}

/*
 * Interrupts always disabled.
 */
static void imx_break_ctl(struct uart_port *port, int break_state)
{
	struct imx_port *sport = (struct imx_port *)port;
	unsigned long flags;
	unsigned char temp;

	spin_lock_irqsave(&sport->port.lock, flags);
	temp = readb(sport->port.membase + MXC_UARTCR2) & ~MXC_UARTCR2_SBK;

	if (break_state != 0)
		temp |= MXC_UARTCR2_SBK;

	writeb(temp, sport->port.membase + MXC_UARTCR2);

	spin_unlock_irqrestore(&sport->port.lock, flags);
}

#define TXTL 2 /* reset default */
#define RXTL 1 /* reset default */

static int imx_setup_watermark(struct imx_port *sport, unsigned int mode)
{
	unsigned char val, old_cr2, cr2;

	/* set receiver / transmitter trigger level.
	 */
	old_cr2 = cr2 = readb(sport->port.membase + MXC_UARTCR2);
	cr2 &= ~(MXC_UARTCR2_TIE | MXC_UARTCR2_TCIE | MXC_UARTCR2_TE |
			MXC_UARTCR2_RIE | MXC_UARTCR2_RE);
	writeb(cr2, sport->port.membase + MXC_UARTCR2);

	val = TXTL;
	writeb(val, sport->port.membase + MXC_UARTTWFIFO);
	val = RXTL;
	writeb(val, sport->port.membase + MXC_UARTRWFIFO);

	/* Enable Tx and Rx FIFO */
	val = readb(sport->port.membase + MXC_UARTPFIFO);
	sport->tx_fifo_size = 0x1 << (((val >> MXC_UARTPFIFO_TXFIFOSIZE_OFF) &
		MXC_UARTPFIFO_TXFIFOSIZE_MASK) + 1);
	sport->rx_fifo_size = 0x1 << (((val >> MXC_UARTPFIFO_RXFIFOSIZE_OFF) &
		MXC_UARTPFIFO_RXFIFOSIZE_MASK) + 1);
	writeb(val | MXC_UARTPFIFO_TXFE | MXC_UARTPFIFO_RXFE,
			sport->port.membase + MXC_UARTPFIFO);

	/* Flush the Tx and Rx FIFO to a known state */
	writeb(MXC_UARTCFIFO_TXFLUSH | MXC_UARTCFIFO_RXFLUSH,
			sport->port.membase + MXC_UARTCFIFO);

	/* restore CR2 */
	writeb(old_cr2, sport->port.membase + MXC_UARTCR2);
	return 0;
}


/* half the RX buffer size */
#define CTSTL 16

static int imx_startup(struct uart_port *port)
{
	struct imx_port *sport = (struct imx_port *)port;
	int retval;
	unsigned long flags, temp;
	struct tty_struct *tty;
	struct imxuart_platform_data *pdata = port->dev->platform_data;

#ifndef CONFIG_SERIAL_CORE_CONSOLE
	if (sport->fifo_en)
		imx_setup_watermark(sport, 0);
#endif

	/*
	 * Allocate the IRQ(s)
	 * Vybrid chips only have one interrupt.
	 */
		retval = request_irq(sport->port.irq, imx_int, 0,
				DRIVER_NAME, sport);
		if (retval) {
			return retval;
		}

	/* Enable the DMA ops for uart. */
	if (sport->enable_dma) {
		/* request eDMA channel and buffer */
		sport->dma_tx_ch = mcf_edma_request_channel(pdata->dma_req_tx,
			dma_tx_callback, NULL, 1, sport, NULL, "ttymxc");

		sport->dma_is_txing = 0;

		/* enable DMA request generation */
		temp = readb(sport->port.membase + MXC_UARTCR5);
		temp |= MXC_UARTCR5_TDMAS;
		writeb(temp, sport->port.membase + MXC_UARTCR5);

		sport->port.flags |= UPF_LOW_LATENCY;
		INIT_WORK(&sport->tsk_dma_tx, dma_tx_work);
		INIT_WORK(&sport->tsk_rx, rx_work);
		init_waitqueue_head(&sport->dma_wait);

	}

	spin_lock_irqsave(&sport->port.lock, flags);
	/*
	 * Finally enable interrupts and RX/TX
	 */
	temp = readb(sport->port.membase + MXC_UARTCR2);
	temp |= (MXC_UARTCR2_RIE | MXC_UARTCR2_TIE | MXC_UARTCR2_TE | MXC_UARTCR2_RE);
	writeb(temp, sport->port.membase + MXC_UARTCR2);

	spin_unlock_irqrestore(&sport->port.lock, flags);


	tty = sport->port.state->port.tty;

	return 0;
}

static void imx_shutdown(struct uart_port *port)
{
	struct imx_port *sport = (struct imx_port *)port;
	unsigned char temp;
	unsigned long flags;

	if (sport->enable_dma) {
		/* We have to wait for the DMA to finish. */
		if (sport->dma_tx_ch) {
			mcf_edma_stop_transfer(sport->dma_tx_ch);
			mcf_edma_free_channel(sport->dma_tx_ch, sport);
			sport->dma_tx_ch = 0;
		}
	}

	while (!(readb(sport->port.membase + MXC_UARTSR1) & MXC_UARTSR1_TC))
		barrier();
	spin_lock_irqsave(&sport->port.lock, flags);

	/* Disable Rx/Tx and interrupts */
	temp = readb(sport->port.membase + MXC_UARTCR2);
	temp &= ~(MXC_UARTCR2_TE | MXC_UARTCR2_RE |
			MXC_UARTCR2_TIE | MXC_UARTCR2_TCIE | MXC_UARTCR2_RIE);
	writeb(temp, sport->port.membase + MXC_UARTCR2);
	spin_unlock_irqrestore(&sport->port.lock, flags);


	/*
	 * Free the interrupts
	 */

	free_irq(sport->port.irq, sport);

}

static void
imx_set_termios(struct uart_port *port, struct ktermios *termios,
		   struct ktermios *old)
{
	struct imx_port *sport = (struct imx_port *)port;
	unsigned long flags;
	unsigned char cr1, old_cr1, old_cr2, cr4, bdh, modem;
	unsigned int  baud;
	unsigned int old_csize = old ? old->c_cflag & CSIZE : CS8;
	unsigned int sbr, brfa;

	cr1 = old_cr1 = readb(sport->port.membase + MXC_UARTCR1);
	old_cr2 = readb(sport->port.membase + MXC_UARTCR2);
	cr4 = readb(sport->port.membase + MXC_UARTCR4);
	bdh = readb(sport->port.membase + MXC_UARTBDH);
	modem = readb(sport->port.membase + MXC_UARTMODEM);

	/*
	 * We only support CS8 and CS7,but CS7 must enable PE.
	 * supported mode:
	 *  - (7,e/o,1)
	 *  - (8,n,1)
	 *  - (8,m/s,1)
	 *  - (8,e/o,1)
	 */
	while ((termios->c_cflag & CSIZE) != CS8 &&
		(termios->c_cflag & CSIZE) != CS7) {
		termios->c_cflag &= ~CSIZE;
		termios->c_cflag |= old_csize;
		old_csize = CS8;
	}

	if ((termios->c_cflag & CSIZE) == CS8 ||
		(termios->c_cflag & CSIZE) == CS7)
		cr1 = old_cr1 & ~MXC_UARTCR1_M;

	if (termios->c_cflag & CMSPAR) {
		if ((termios->c_cflag & CSIZE) != CS8) {
			termios->c_cflag &= ~CSIZE;
			termios->c_cflag |= CS8;
		}
		cr1 |= MXC_UARTCR1_M;
	}

	if (termios->c_cflag & CRTSCTS) {
		if (sport->have_rtscts)
			modem |= (MXC_UARTMODEM_RXRTSE | MXC_UARTMODEM_TXCTSE);

	} else {
		termios->c_cflag &= ~CRTSCTS;
		modem &= ~(MXC_UARTMODEM_RXRTSE | MXC_UARTMODEM_TXCTSE);
	}

	if (termios->c_cflag & CSTOPB)
		termios->c_cflag &= ~CSTOPB;

	/* parity must enable when CS7 to match 8-bits format */
	if ((termios->c_cflag & CSIZE) == CS7)
		termios->c_cflag |= PARENB;

	if ((termios->c_cflag & PARENB)) {
		if (termios->c_cflag & CMSPAR) {
			cr1 &= ~MXC_UARTCR1_PE;
			cr1 |= MXC_UARTCR1_M;
			sport->format_9bits = 1;
		} else {
			cr1 |= MXC_UARTCR1_PE;
			if ((termios->c_cflag & CSIZE) == CS8)
				cr1 |= MXC_UARTCR1_M;
			if (termios->c_cflag & PARODD)
				cr1 |= MXC_UARTCR1_PT;
			else
				cr1 &= ~MXC_UARTCR1_PT;
		}
	}

	/*
	 * Ask the core to calculate the divisor for us.
	 */
	baud = uart_get_baud_rate(port, termios, old, 50, port->uartclk / 16);

	spin_lock_irqsave(&sport->port.lock, flags);

	sport->port.read_status_mask = 0;
	if (termios->c_iflag & INPCK)
		sport->port.read_status_mask |=
					(MXC_UARTSR1_FE | MXC_UARTSR1_PE);
	if (termios->c_iflag & (BRKINT | PARMRK))
		sport->port.read_status_mask |= MXC_UARTSR1_FE;

	/*
	 * Characters to ignore
	 */
	sport->port.ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		sport->port.ignore_status_mask |= MXC_UARTSR1_PE;
	if (termios->c_iflag & IGNBRK) {
		sport->port.ignore_status_mask |= MXC_UARTSR1_FE;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (termios->c_iflag & IGNPAR)
			sport->port.ignore_status_mask |= MXC_UARTSR1_OR;
	}

	/*
	 * Update the per-port timeout.
	 */
	uart_update_timeout(port, termios->c_cflag, baud);

	/* wait transmit engin complete */
	while (!(readb(sport->port.membase + MXC_UARTSR1) & MXC_UARTSR1_TC))
		barrier();

	/* disable transmit and receive */
	writeb(old_cr2 & ~(MXC_UARTCR2_TE | MXC_UARTCR2_RE),
			sport->port.membase + MXC_UARTCR2);

	sbr = sport->port.uartclk / (16 * baud);
	brfa = ((sport->port.uartclk - (16 * sbr * baud)) * 2)/baud;

	bdh &= ~MXC_UARTBDH_SBR_MASK;
	bdh |= (sbr >> 8) & 0x1F;

	cr4 &= ~MXC_UARTCR4_BRFA_MASK;
	brfa &= MXC_UARTCR4_BRFA_MASK;
	writeb(cr4 | brfa, sport->port.membase + MXC_UARTCR4);
	writeb(bdh, sport->port.membase + MXC_UARTBDH);
	writeb(sbr & 0xFF, sport->port.membase + MXC_UARTBDL);
	writeb(cr1, sport->port.membase + MXC_UARTCR1);
	writeb(modem, sport->port.membase + MXC_UARTMODEM);

	/* restore control register */
	writeb(old_cr2, sport->port.membase + MXC_UARTCR2);

	spin_unlock_irqrestore(&sport->port.lock, flags);
}

static const char *imx_type(struct uart_port *port)
{
	struct imx_port *sport = (struct imx_port *)port;

	return sport->port.type == PORT_IMX ? "IMX" : NULL;
}

/*
 * Release the memory region(s) being used by 'port'.
 */
static void imx_release_port(struct uart_port *port)
{
	struct platform_device *pdev = to_platform_device(port->dev);
	struct resource *mmres;

	mmres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(mmres->start, mmres->end - mmres->start + 1);
}

/*
 * Request the memory region(s) being used by 'port'.
 */
static int imx_request_port(struct uart_port *port)
{
	struct platform_device *pdev = to_platform_device(port->dev);
	struct resource *mmres;
	void *ret;

	mmres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mmres)
		return -ENODEV;

	ret = request_mem_region(mmres->start, mmres->end - mmres->start + 1,
			"imx-uart");

	return  ret ? 0 : -EBUSY;
}

/*
 * Configure/autoconfigure the port.
 */
static void imx_config_port(struct uart_port *port, int flags)
{
	struct imx_port *sport = (struct imx_port *)port;

	if (flags & UART_CONFIG_TYPE &&
	    imx_request_port(&sport->port) == 0)
		sport->port.type = PORT_IMX;
}

/*
 * Verify the new serial_struct (for TIOCSSERIAL).
 * The only change we allow are to the flags and type, and
 * even then only between PORT_IMX and PORT_UNKNOWN
 */
static int
imx_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	struct imx_port *sport = (struct imx_port *)port;
	int ret = 0;

	if (ser->type != PORT_UNKNOWN && ser->type != PORT_IMX)
		ret = -EINVAL;
	if (sport->port.irq != ser->irq)
		ret = -EINVAL;
	if (ser->io_type != UPIO_MEM)
		ret = -EINVAL;
	if (sport->port.uartclk / 16 != ser->baud_base)
		ret = -EINVAL;
	if (sport->port.iobase != ser->port)
		ret = -EINVAL;
	if (ser->hub6 != 0)
		ret = -EINVAL;
	return ret;
}

static int imx_uart_ioctl(struct uart_port *uport, unsigned int cmd,
			unsigned long arg)
{
	struct imx_port *sport = (struct imx_port *)uport;
	unsigned char cr1, cr3, cr4, sr2, ma1;

	switch (cmd) {
	case TIOCSETADDRMODE:
		sport->mark_en = 1;
		sport->ma_addr = (unsigned char) arg;
			/*
			 * Enable Mark/Space mode
			 * Avaliable config for MSB as address bit include:
			 * 1b start + 7b data + 1b addr + 1b stop (8,n,1)
			 * 1b start + 8b data + 1b addr + 1b stop (9,n,1)
			 * 1b start + 8b data + 1b addr + 1b parity + 1b stop
			 *
			 * only 9,n,1 mark/space mode is supported
			 */
		cr4 = readb(sport->port.membase + MXC_UARTCR4);
		cr4 |= MXC_UARTCR4_MAEN1;
		ma1 = (unsigned char) arg;
		/* Set match address for receive comparation */
		writeb(ma1, sport->port.membase + MXC_UARTMAR1);
		/* Enable address match */
		writeb(cr4, sport->port.membase + MXC_UARTCR4);
		cr1 = readb(sport->port.membase + MXC_UARTCR1);
		cr1 |= MXC_UARTCR1_M;
		sport->format_9bits = 1;
		writeb(cr1, sport->port.membase + MXC_UARTCR1);
		return 0;

	case TIOCCLRADDRMODE:
		cr4 = readb(sport->port.membase + MXC_UARTCR4);
		cr4 &= ~MXC_UARTCR4_MAEN1;
		writeb(cr4, sport->port.membase + MXC_UARTCR4);
		sport->mark_en = 0;
		return 0;

	case TIOCSETMARK:
		/* Set Mark flag: following Tx is Address */
		cr3 = readb(sport->port.membase + MXC_UARTCR3);
		cr3 |= MXC_UARTCR3_T8;
		writeb(cr3, sport->port.membase + MXC_UARTCR3);
		return 0;

	case TIOCCLRMARK:
		/* clear Mark flag: following Tx is Data */
		cr3 = readb(sport->port.membase + MXC_UARTCR3);
		cr3 &= ~MXC_UARTCR3_T8;
		writeb(cr3, sport->port.membase + MXC_UARTCR3);
		return 0;

	case TIOCMSBMODE:
		/* MSB first on line */
		sr2 = readb(sport->port.membase + MXC_UARTSR2);
		sr2 |= MXC_UARTSR2_MSBF;
		writeb(sr2, sport->port.membase + MXC_UARTSR2);

		return 0;

	case TIOCLSBMODE:
		/* LSB first on line */
		sr2 = readb(sport->port.membase + MXC_UARTSR2);
		sr2 &= ~MXC_UARTSR2_MSBF;
		writeb(sr2, sport->port.membase + MXC_UARTSR2);

		return 0;

	default:
		break;
	}
	return -ENOIOCTLCMD;

}

#if defined(CONFIG_CONSOLE_POLL)
static int imx_poll_get_char(struct uart_port *port)
{
	struct imx_port *sport = (struct imx_port *)port;
	int ch;

	while (!(readb(sport->port.membase + MXC_UARTSR1) & MXC_UARTSR1_RDRF));

	ch = readb(sport->port.membase + MXC_UARTDR);
	return(ch & 0xff);
}

static void imx_poll_put_char(struct uart_port *port, unsigned char c)
{
	struct imx_port *sport = (struct imx_port *)port;

	while (!(readb(sport->port.membase + MXC_UARTSR1) & MXC_UARTSR1_TDRE))
		barrier();

	writeb(c, sport->port.membase + MXC_UARTDR);
}
#endif

static struct uart_ops imx_pops = {
	.tx_empty	= imx_tx_empty,
	.set_mctrl	= imx_set_mctrl,
	.get_mctrl	= imx_get_mctrl,
	.stop_tx	= imx_stop_tx,
	.start_tx	= imx_start_tx,
	.stop_rx	= imx_stop_rx,
	.enable_ms	= imx_enable_ms,
	.break_ctl	= imx_break_ctl,
	.startup	= imx_startup,
	.shutdown	= imx_shutdown,
	.set_termios	= imx_set_termios,
	.type		= imx_type,
	.release_port	= imx_release_port,
	.request_port	= imx_request_port,
	.config_port	= imx_config_port,
	.verify_port	= imx_verify_port,
	.ioctl		= imx_uart_ioctl,
#if defined(CONFIG_CONSOLE_POLL)
	.poll_get_char  = imx_poll_get_char,
	.poll_put_char  = imx_poll_put_char,
#endif
};

static struct imx_port *imx_ports[UART_NR];

#ifdef CONFIG_SERIAL_IMX_CONSOLE
static void imx_console_putchar(struct uart_port *port, int ch)
{
	struct imx_port *sport = (struct imx_port *)port;

	while (!(readb(sport->port.membase + MXC_UARTSR1) & MXC_UARTSR1_TDRE))
		barrier();

	writeb(ch, sport->port.membase + MXC_UARTDR);
}

/*
 * Interrupts are disabled on entering
 */
static void
imx_console_write(struct console *co, const char *s, unsigned int count)
{
	struct imx_port *sport = imx_ports[co->index];
	unsigned int  old_cr2, cr2;
	unsigned long flags;

	spin_lock_irqsave(&sport->port.lock, flags);

	/*
	 *	First, save UCR1/2 and then disable interrupts
	 */
	cr2 = old_cr2 = readb(sport->port.membase + MXC_UARTCR2);


	cr2 |= (MXC_UARTCR2_TE |  MXC_UARTCR2_RE);
	cr2 &= ~(MXC_UARTCR2_TIE | MXC_UARTCR2_TCIE | MXC_UARTCR2_RIE);

	writeb(cr2, sport->port.membase + MXC_UARTCR2);


	uart_console_write(&sport->port, s, count, imx_console_putchar);

	/*
	 *	Finally, wait for transmitter finish complete
	 *	and restore CR2
	 */
	while (!(readb(sport->port.membase + MXC_UARTSR1) & MXC_UARTSR1_TC))
		barrier();

	writeb(old_cr2, sport->port.membase + MXC_UARTCR2);

	spin_unlock_irqrestore(&sport->port.lock, flags);
}

/*
 * If the port was already initialised (eg, by a boot loader),
 * try to determine the current setup.
 */
static void __init
imx_console_get_options(struct imx_port *sport, int *baud,
			   int *parity, int *bits)
{
	unsigned char cr1, cr2, bdh, bdl, brfa;
	unsigned int sbr, uartclk;
	unsigned int baud_raw;

	cr2 = readb(sport->port.membase + MXC_UARTCR2);
	cr2 &= (MXC_UARTCR2_TE | MXC_UARTCR2_RE);
	if (!cr2)
		return;
	/* ok, the port was enabled */

	cr1 = readb(sport->port.membase + MXC_UARTCR1);

	*parity = 'n';
	if (cr1 & MXC_UARTCR1_PE) {
		if (cr1 & MXC_UARTCR1_PT)
			*parity = 'o';
		else
			*parity = 'e';
	}

	if (cr1 & MXC_UARTCR1_M)
		*bits = 9;
	else
		*bits = 8;

	bdh = readb(sport->port.membase + MXC_UARTBDH) &
					MXC_UARTBDH_SBR_MASK;
	bdl = readb(sport->port.membase + MXC_UARTBDL);
	sbr = bdh;
	sbr <<= 8;
	sbr |= bdl;
	brfa = readb(sport->port.membase + MXC_UARTCR4) &
					MXC_UARTCR4_BRFA_MASK;

	uartclk = clk_get_rate(sport->clk);
	/*
	 * Baud = mod_clk/(16*(sbr[13]+(brfa)/32)
	 */
	baud_raw = uartclk/(16 * (sbr + brfa/32));

	if (*baud != baud_raw)
		printk(KERN_INFO "Serial: Console IMX "
				"rounded baud rate from %d to %d\n",
			baud_raw, *baud);
}

static int __init
imx_console_setup(struct console *co, char *options)
{
	struct imx_port *sport;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	/*
	 * Check whether an invalid uart number has been specified, and
	 * if so, search for the first available port that does have
	 * console support.
	 */
	if (co->index == -1 || co->index >= ARRAY_SIZE(imx_ports))
		co->index = 0;
	sport = imx_ports[co->index];

	if (sport == NULL)
		return -ENODEV;

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	else
		imx_console_get_options(sport, &baud, &parity, &bits);

	if (sport->fifo_en == 1)
		imx_setup_watermark(sport, 0);

	return uart_set_options(&sport->port, co, baud, parity, bits, flow);
}

static struct uart_driver imx_reg;
static struct console imx_console = {
	.name		= DEV_NAME,
	.write		= imx_console_write,
	.device		= uart_console_device,
	.setup		= imx_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &imx_reg,
};

#define IMX_CONSOLE	(&imx_console)
#else
#define IMX_CONSOLE	NULL
#endif

static struct uart_driver imx_reg = {
	.owner          = THIS_MODULE,
	.driver_name    = DRIVER_NAME,
	.dev_name       = DEV_NAME,
	.major          = SERIAL_IMX_MAJOR,
	.minor          = MINOR_START,
	.nr             = ARRAY_SIZE(imx_ports),
	.cons           = IMX_CONSOLE,
};

static int serial_imx_suspend(struct platform_device *dev, pm_message_t state)
{
	struct imx_port *sport = platform_get_drvdata(dev);

	/* Enable UART wakeup */

	if (sport)
		uart_suspend_port(&imx_reg, &sport->port);

	return 0;
}

static int serial_imx_resume(struct platform_device *dev)
{
	struct imx_port *sport = platform_get_drvdata(dev);

	if (sport)
		uart_resume_port(&imx_reg, &sport->port);

	/* Disable UART wakeup */

	return 0;
}

static int serial_imx_probe(struct platform_device *pdev)
{
	struct imx_port *sport;
	struct imxuart_platform_data *pdata;
	void __iomem *base;
	int ret = 0;
	struct resource *res;

	sport = kzalloc(sizeof(*sport), GFP_KERNEL);
	if (!sport)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
		goto free;
	}

	base = ioremap(res->start, PAGE_SIZE);
	if (!base) {
		ret = -ENOMEM;
		goto free;
	}

	sport->port.dev = &pdev->dev;
	sport->port.mapbase = res->start;
	sport->port.membase = base;
	sport->port.type = PORT_IMX,
	sport->port.iotype = UPIO_MEM;
	sport->port.irq = platform_get_irq(pdev, 0);
	sport->port.fifosize = 32;
	sport->port.ops = &imx_pops;
	sport->port.flags = UPF_BOOT_AUTOCONF;
	sport->port.line = pdev->id;

	sport->clk = clk_get(&pdev->dev, "mvf-uart.1");
	if (IS_ERR(sport->clk)) {
		ret = PTR_ERR(sport->clk);
		goto unmap;
	}


	sport->port.uartclk = clk_get_rate(sport->clk);

	imx_ports[pdev->id] = sport;

	pdata = pdev->dev.platform_data;
	if (pdata && (pdata->flags & IMXUART_HAVE_RTSCTS))
	{
		sport->have_rtscts = 1;
		printk("IMX UART RTS/CTS enabled\n");
	}
	if (pdata && (pdata->flags & IMXUART_USE_DCEDTE))
	{
		sport->use_dcedte = 1;
		printk("IMX UART using DCE/DTE\n");
	}
	if (pdata && (pdata->flags & IMXUART_EDMA))
	{
		sport->enable_dma = 1;
		printk("IMX UART EDMA enabled\n");
	}
	if (pdata && (pdata->flags & IMXUART_FIFO))
	{
		sport->fifo_en = 1;
		printk("IMX UART FIFO enabled\n");
	}

	if (pdata && pdata->init) {
		ret = pdata->init(pdev);
		if (ret)
			goto clkput;
	}

	ret = uart_add_one_port(&imx_reg, &sport->port);

	if (ret)
		goto deinit;
	platform_set_drvdata(pdev, &sport->port);

	return 0;
deinit:
	if (pdata && pdata->exit)
		pdata->exit(pdev);
clkput:
	clk_put(sport->clk);
	clk_disable(sport->clk);
unmap:
	iounmap(sport->port.membase);
free:
	kfree(sport);

	return ret;
}

static int serial_imx_remove(struct platform_device *pdev)
{
	struct imxuart_platform_data *pdata;
	struct imx_port *sport = platform_get_drvdata(pdev);

	pdata = pdev->dev.platform_data;

	platform_set_drvdata(pdev, NULL);

	if (sport) {
		uart_remove_one_port(&imx_reg, &sport->port);
		clk_put(sport->clk);
	}

	clk_disable(sport->clk);

	if (pdata && pdata->exit)
		pdata->exit(pdev);

	iounmap(sport->port.membase);
	kfree(sport);

	return 0;
}

static struct platform_driver serial_imx_driver = {
	.probe		= serial_imx_probe,
	.remove		= serial_imx_remove,

	.suspend	= serial_imx_suspend,
	.resume		= serial_imx_resume,
	.driver		= {
		.name	= "imx-uart",
		.owner	= THIS_MODULE,
	},
};

static int __init imx_serial_init(void)
{
	int ret;

	printk(KERN_INFO "Serial: MVF driver\n");

	ret = uart_register_driver(&imx_reg);
	if (ret)
		return ret;

	ret = platform_driver_register(&serial_imx_driver);
	if (ret != 0)
		uart_unregister_driver(&imx_reg);

	return 0;
}

static void __exit imx_serial_exit(void)
{
	platform_driver_unregister(&serial_imx_driver);
	uart_unregister_driver(&imx_reg);
}

module_init(imx_serial_init);
module_exit(imx_serial_exit);

MODULE_AUTHOR("Sascha Hauer");
MODULE_DESCRIPTION("IMX generic serial port driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:imx-uart");
