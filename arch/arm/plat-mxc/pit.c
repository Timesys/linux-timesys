/*
 *  linux/arch/arm/plat-mxc/pit.c
 *
 *  Copyright 2012 Freescale Semiconductor, Inc.
 *  based on linux/arch/arm/plat-mxc/epit.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
#include <asm/sched_clock.h>


#define PITMCR		0x00
#define PITLTMR64H	0xE0
#define PITLTMR64L      0xE4

#define PITOFFSET	0x120
#define PITLDVAL	0x00
#define PITCVAL		0x04
#define PITTCTRL	0x08
#define PITTFLG		0x0C

/*
 * Total 8 pit timer, each memory map occupy 0x10 Bytes
 * get base offset for pit(n)
 */
#define PITOFFSETx(n)	(PITOFFSET + 0x10*n)

/* bit definitation */
#define PITMCR_MDIS		(1 << 1)
#define PITMCR_FRZ		(1 << 0)

#define PITTCTRL_TEN		(1 << 0)
#define PITTCTRL_TIE		(1 << 1)
#define	PITCTRL_CHN		(1 << 2)

#define PITTFLG_TIF		(1 << 0)

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clockchips.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <mach/hardware.h>
#include <asm/mach/time.h>
#include <mach/common.h>

static cycle_t pit_cnt;
static struct clock_event_device clockevent_pit;
static enum clock_event_mode clockevent_mode = CLOCK_EVT_MODE_UNUSED;

static void __iomem *timer_base;
static unsigned long pit_cycle_per_jiffy;

static inline void pit_irq_disable(void)
{
	u32 val;

	val = __raw_readl(timer_base + PITOFFSET + PITTCTRL);
	val &= ~PITTCTRL_TIE;
	__raw_writel(val, timer_base + PITOFFSET + PITTCTRL);
}

static inline void pit_irq_enable(void)
{
	u32 val;

	val = __raw_readl(timer_base + PITOFFSET + PITTCTRL);
	val |= PITTCTRL_TIE;
	__raw_writel(val, timer_base + PITOFFSET + PITTCTRL);
}

static void pit_irq_acknowledge(void)
{
	__raw_writel(PITTFLG_TIF, timer_base + PITOFFSET + PITTFLG);
}

static cycle_t pit_read_clk(struct clocksource *cs);

static DEFINE_CLOCK_DATA(cd);
static void __iomem *sched_clock_reg;
unsigned long long notrace sched_clock(void)
{
	cycle_t cyc = sched_clock_reg ? ((u32)~0
			 - __raw_readl(sched_clock_reg)) : 0;
	return cyc_to_sched_clock(&cd, cyc, (u32)~0);
}

static void notrace mvf_update_sched_clock(void)
{
	cycle_t cyc = sched_clock_reg ? __raw_readl(sched_clock_reg) : 0;
	update_sched_clock(&cd, cyc, (u32)~0);
}
static int __init pit_clocksource_init(struct clk *timer_clk)
{
	unsigned int c = clk_get_rate(timer_clk);
	void __iomem *reg = timer_base + PITOFFSET + PITCVAL;

	sched_clock_reg = reg;

	init_sched_clock(&cd, mvf_update_sched_clock, 32, c);
	return clocksource_mmio_init(timer_base + PITOFFSET + PITCVAL, "pit",
			c, 200, 32,
			pit_read_clk/*clocksource_mmio_readl_down*/);
}

/* clock event */

static int pit_set_next_event(unsigned long evt,
			      struct clock_event_device *unused)
{
	return 0;
}

static void pit_set_mode(enum clock_event_mode mode,
				struct clock_event_device *evt)
{
	unsigned long flags;

	/*
	 * The timer interrupt generation is disabled at least
	 * for enough time to call epit_set_next_event()
	 */
	local_irq_save(flags);

	/* Disable interrupt in PIT module */
	pit_irq_disable();

	if (mode != clockevent_mode) {
		/* Set event time into far-far future */

		/* Clear pending interrupt */
		pit_irq_acknowledge();
	}

	/* Remember timer mode */
	clockevent_mode = mode;
	local_irq_restore(flags);

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:

		__raw_writel(0, timer_base + PITOFFSET + PITTCTRL);
		__raw_writel(pit_cycle_per_jiffy,
				timer_base + PITOFFSET + PITLDVAL);
		__raw_writel(PITTCTRL_TEN, timer_base + PITOFFSET + PITTCTRL);

		pit_irq_enable();

		break;
	case CLOCK_EVT_MODE_ONESHOT:
	/*
	 * Do not put overhead of interrupt enable/disable into
	 * epit_set_next_event(), the core has about 4 minutes
	 * to call epit_set_next_event() or shutdown clock after
	 * mode switching
	 */
		local_irq_save(flags);
		pit_irq_enable();
		local_irq_restore(flags);
		break;
	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_RESUME:
		/* Left event sources disabled, no more interrupts appear */
		break;
	}
}

/*
 * IRQ handler for the timer
 */
static irqreturn_t pit_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = &clockevent_pit;

	pit_irq_acknowledge();

	pit_cnt += pit_cycle_per_jiffy;

	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static cycle_t pit_read_clk(struct clocksource *cs)
{
	unsigned long flags;
	cycle_t cycles;
	u32 pcntr;

	local_irq_save(flags);
	pcntr = __raw_readl(timer_base + PITOFFSET + PITCVAL);
	cycles = pit_cnt;
	local_irq_restore(flags);

	return cycles + pit_cycle_per_jiffy - pcntr;
}


static struct irqaction pit_timer_irq = {
	.name		= "MVF PIT Timer Tick",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler	= pit_timer_interrupt,
};

static struct clock_event_device clockevent_pit = {
	.name		= "pit",
	.features	= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.shift		= 32,
	.set_mode	= pit_set_mode,
	.set_next_event	= pit_set_next_event,
	.rating		= 100,
};

static int __init pit_clockevent_init(struct clk *timer_clk)
{
	unsigned int c = clk_get_rate(timer_clk);

	clockevent_pit.mult = div_sc(c, NSEC_PER_SEC,
					clockevent_pit.shift);
	clockevent_pit.max_delta_ns =
			clockevent_delta2ns(0xfffffffe, &clockevent_pit);
	clockevent_pit.min_delta_ns =
			clockevent_delta2ns(0x800, &clockevent_pit);
	clockevent_pit.cpumask = cpumask_of(0);
	clockevents_register_device(&clockevent_pit);

	return 0;
}

void __init pit_timer_init(struct clk *timer_clk, void __iomem *base, int irq)
{

	timer_base = base;

	pit_cycle_per_jiffy = clk_get_rate(timer_clk)/(HZ);

	/*
	 * Initialise to a known state (all timers off, and timing reset)
	 */
	__raw_writel(0x0, timer_base + PITMCR);

	__raw_writel(0, timer_base + PITOFFSET + PITTCTRL);
	__raw_writel(0xffffffff, timer_base + PITOFFSET + PITLDVAL);
	__raw_writel(PITTCTRL_TEN, timer_base + PITOFFSET + PITTCTRL);

	/* init and register the timer to the framework */
	pit_clocksource_init(timer_clk);

	pit_clockevent_init(timer_clk);

	/* Make irqs happen */
	setup_irq(irq, &pit_timer_irq);
}
