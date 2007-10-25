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

/* System Timer Interrupt reconfigured to run in free-run mode.
 * Author: Vitaly Wool
 * Copyright 2004 MontaVista Software Inc.
 */

/*!
 * @defgroup Timers OS Tick Timer
 */
/*!
 * @file plat-mxc/time.c
 * @brief This file contains OS tick timer implementation.
 *
 * This file contains OS tick timer implementation.
 *
 * @ingroup Timers
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <asm/mach/time.h>

#define MXC_GPT_GPTCR		(IO_ADDRESS(GPT1_BASE_ADDR + 0x00))
#define MXC_GPT_GPTPR		(IO_ADDRESS(GPT1_BASE_ADDR + 0x04))
#define MXC_GPT_GPTSR		(IO_ADDRESS(GPT1_BASE_ADDR + 0x08))
#define MXC_GPT_GPTIR		(IO_ADDRESS(GPT1_BASE_ADDR + 0x0C))
#define MXC_GPT_GPTOCR1		(IO_ADDRESS(GPT1_BASE_ADDR + 0x10))
#define MXC_GPT_GPTOCR2		(IO_ADDRESS(GPT1_BASE_ADDR + 0x14))
#define MXC_GPT_GPTOCR3		(IO_ADDRESS(GPT1_BASE_ADDR + 0x18))
#define MXC_GPT_GPTICR1		(IO_ADDRESS(GPT1_BASE_ADDR + 0x1C))
#define MXC_GPT_GPTICR2		(IO_ADDRESS(GPT1_BASE_ADDR + 0x20))
#define MXC_GPT_GPTCNT		(IO_ADDRESS(GPT1_BASE_ADDR + 0x24))

/*!
 * GPT Control register bit definitions
 */
#define GPTCR_FO3			(1<<31)
#define GPTCR_FO2			(1<<30)
#define GPTCR_FO1			(1<<29)

#define GPTCR_OM3_SHIFT			26
#define GPTCR_OM3_MASK			(7<<GPTCR_OM3_SHIFT)
#define GPTCR_OM3_DISCONNECTED		(0<<GPTCR_OM3_SHIFT)
#define GPTCR_OM3_TOGGLE		(1<<GPTCR_OM3_SHIFT)
#define GPTCR_OM3_CLEAR			(2<<GPTCR_OM3_SHIFT)
#define GPTCR_OM3_SET			(3<<GPTCR_OM3_SHIFT)
#define GPTCR_OM3_GENERATE_LOW		(7<<GPTCR_OM3_SHIFT)

#define GPTCR_OM2_SHIFT			23
#define GPTCR_OM2_MASK			(7<<GPTCR_OM2_SHIFT)
#define GPTCR_OM2_DISCONNECTED		(0<<GPTCR_OM2_SHIFT)
#define GPTCR_OM2_TOGGLE		(1<<GPTCR_OM2_SHIFT)
#define GPTCR_OM2_CLEAR			(2<<GPTCR_OM2_SHIFT)
#define GPTCR_OM2_SET			(3<<GPTCR_OM2_SHIFT)
#define GPTCR_OM2_GENERATE_LOW		(7<<GPTCR_OM2_SHIFT)

#define GPTCR_OM1_SHIFT			20
#define GPTCR_OM1_MASK			(7<<GPTCR_OM1_SHIFT)
#define GPTCR_OM1_DISCONNECTED		(0<<GPTCR_OM1_SHIFT)
#define GPTCR_OM1_TOGGLE		(1<<GPTCR_OM1_SHIFT)
#define GPTCR_OM1_CLEAR			(2<<GPTCR_OM1_SHIFT)
#define GPTCR_OM1_SET			(3<<GPTCR_OM1_SHIFT)
#define GPTCR_OM1_GENERATE_LOW		(7<<GPTCR_OM1_SHIFT)

#define GPTCR_IM2_SHIFT			18
#define GPTCR_IM2_MASK			(3<<GPTCR_IM2_SHIFT)
#define GPTCR_IM2_CAPTURE_DISABLE	(0<<GPTCR_IM2_SHIFT)
#define GPTCR_IM2_CAPTURE_RISING	(1<<GPTCR_IM2_SHIFT)
#define GPTCR_IM2_CAPTURE_FALLING	(2<<GPTCR_IM2_SHIFT)
#define GPTCR_IM2_CAPTURE_BOTH		(3<<GPTCR_IM2_SHIFT)

#define GPTCR_IM1_SHIFT			16
#define GPTCR_IM1_MASK			(3<<GPTCR_IM1_SHIFT)
#define GPTCR_IM1_CAPTURE_DISABLE	(0<<GPTCR_IM1_SHIFT)
#define GPTCR_IM1_CAPTURE_RISING	(1<<GPTCR_IM1_SHIFT)
#define GPTCR_IM1_CAPTURE_FALLING	(2<<GPTCR_IM1_SHIFT)
#define GPTCR_IM1_CAPTURE_BOTH		(3<<GPTCR_IM1_SHIFT)

#define GPTCR_SWR			(1<<15)
#define GPTCR_FRR			(1<<9)

#define GPTCR_CLKSRC_SHIFT		6
#define GPTCR_CLKSRC_MASK		(7<<GPTCR_CLKSRC_SHIFT)
#define GPTCR_CLKSRC_NOCLOCK		(0<<GPTCR_CLKSRC_SHIFT)
#define GPTCR_CLKSRC_HIGHFREQ		(2<<GPTCR_CLKSRC_SHIFT)
#define GPTCR_CLKSRC_CLKIN		(3<<GPTCR_CLKSRC_SHIFT)
#define GPTCR_CLKSRC_CLK32K		(7<<GPTCR_CLKSRC_SHIFT)

#define GPTCR_STOPEN			(1<<5)
#define GPTCR_DOZEN			(1<<4)
#define GPTCR_WAITEN			(1<<3)
#define GPTCR_DBGEN			(1<<2)

#define GPTCR_ENMOD			(1<<1)
#define GPTCR_ENABLE			(1<<0)

#define	GPTSR_OF1			(1<<0)
#define	GPTSR_OF2			(1<<1)
#define	GPTSR_OF3			(1<<2)
#define	GPTSR_IF1			(1<<3)
#define	GPTSR_IF2			(1<<4)
#define	GPTSR_ROV			(1<<5)

#define	GPTIR_OF1IE			GPTSR_OF1
#define	GPTIR_OF2IE			GPTSR_OF2
#define	GPTIR_OF3IE			GPTSR_OF3
#define	GPTIR_IF1IE			GPTSR_IF1
#define	GPTIR_IF2IE			GPTSR_IF2
#define	GPTIR_ROVIE			GPTSR_ROV

extern unsigned long clk_early_get_timer_rate(void);

/*!
 * This is the timer interrupt service routine to do required tasks.
 * It also services the WDOG timer at the frequency of twice per WDOG
 * timeout value. For example, if the WDOG's timeout value is 4 (2
 * seconds since the WDOG runs at 0.5Hz), it will be serviced once
 * every 2/2=1 second.
 *
 * @param  irq          GPT interrupt source number (not used)
 * @param  dev_id       this parameter is not used
 * @return always returns \b IRQ_HANDLED as defined in
 *         include/linux/interrupt.h.
 */
static irqreturn_t mxc_timer_interrupt(int irq, void *dev_id)
{
	unsigned int next_match;

	write_seqlock(&xtime_lock);

	if (__raw_readl(MXC_GPT_GPTSR) & GPTSR_OF1)
		do {
			mxc_kick_wd();
			timer_tick();
			next_match = __raw_readl(MXC_GPT_GPTOCR1) + LATCH;
			__raw_writel(GPTSR_OF1, MXC_GPT_GPTSR);
			__raw_writel(next_match, MXC_GPT_GPTOCR1);
		} while ((signed long)(next_match -
				       __raw_readl(MXC_GPT_GPTCNT)) <= 0);

	write_sequnlock(&xtime_lock);

	return IRQ_HANDLED;
}

/*!
 * This function is used to obtain the number of microseconds since the last
 * timer interrupt. Note that interrupts is disabled by do_gettimeofday().
 *
 * @return the number of microseconds since the last timer interrupt.
 */
static unsigned long mxc_gettimeoffset(void)
{
	long ticks_to_match, elapsed, usec;

	/* Get ticks before next timer match */
	ticks_to_match =
	    __raw_readl(MXC_GPT_GPTOCR1) - __raw_readl(MXC_GPT_GPTCNT);

	/* We need elapsed ticks since last match */
	elapsed = LATCH - ticks_to_match;

	/* Now convert them to usec */
	usec = (unsigned long)(elapsed * (tick_nsec / 1000)) / LATCH;

	return usec;
}

/*!
 * The OS tick timer interrupt structure.
 */
static struct irqaction timer_irq = {
	.name = "MXC Timer Tick",
	.flags = IRQF_DISABLED | IRQF_TIMER,
	.handler = mxc_timer_interrupt
};

/*!
 * This function is used to initialize the GPT to produce an interrupt
 * every 10 msec. It is called by the start_kernel() during system startup.
 */
void __init mxc_init_time(void)
{
	u32 reg, v;
	reg = __raw_readl(MXC_GPT_GPTCR);
	reg &= ~GPTCR_ENABLE;
	__raw_writel(reg, MXC_GPT_GPTCR);
	reg |= GPTCR_SWR;
	__raw_writel(reg, MXC_GPT_GPTCR);

	while ((__raw_readl(MXC_GPT_GPTCR) & GPTCR_SWR) != 0)
		mb();

	reg = __raw_readl(MXC_GPT_GPTCR);

	reg = 0 * GPTCR_OM3_CLEAR | GPTCR_FRR | GPTCR_CLKSRC_HIGHFREQ;

	__raw_writel(reg, MXC_GPT_GPTCR);

	/* Normal clk api are not yet initialized, so use early verion */
	v = clk_early_get_timer_rate();

	__raw_writel((v / CLOCK_TICK_RATE) - 1, MXC_GPT_GPTPR);

	if ((v % CLOCK_TICK_RATE) != 0) {
		pr_info("\nWARNING: Can't generate CLOCK_TICK_RATE at %d Hz\n",
			CLOCK_TICK_RATE);
	}
	pr_info("Actual CLOCK_TICK_RATE is %d Hz\n",
		v / ((__raw_readl(MXC_GPT_GPTPR) & 0xFFF) + 1));

	reg = __raw_readl(MXC_GPT_GPTCNT);
	reg += LATCH;
	__raw_writel(reg, MXC_GPT_GPTOCR1);

	setup_irq(INT_GPT, &timer_irq);

	reg = __raw_readl(MXC_GPT_GPTCR);
	reg = GPTCR_FRR | GPTCR_CLKSRC_HIGHFREQ |
	    GPTCR_STOPEN |
	    GPTCR_DOZEN | GPTCR_WAITEN | GPTCR_ENMOD | GPTCR_ENABLE;
	__raw_writel(reg, MXC_GPT_GPTCR);

	__raw_writel(GPTIR_OF1IE, MXC_GPT_GPTIR);

}

struct sys_timer mxc_timer = {
	.init = mxc_init_time,
	.offset = mxc_gettimeoffset,
};
