
/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 *
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/hrtimer.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/clkdev.h>
#include <linux/regulator/consumer.h>
#include <asm/div64.h>
#include <mach/hardware.h>
#include <mach/common.h>
#include <mach/clock.h>
#include <mach/mxc_dvfs.h>
#include "crm_regs.h"
#include "regs-anadig.h"

#ifdef CONFIG_CLK_DEBUG
#define __INIT_CLK_DEBUG(n)	.name = #n,
#else
#define __INIT_CLK_DEBUG(n)
#endif


void __iomem *apll_base;
static struct clk pll1_sys_main_clk;
static struct clk pll2_528_bus_main_clk;
static struct clk pll2_pfd2_452M;
static struct clk pll3_usb_otg_main_clk;
static struct clk pll4_audio_main_clk;
static struct clk pll6_video_main_clk;
static struct clk pll5_enet_main_clk;
static struct clk pll1_pfd3_396M;
static struct clk pll1_pfd4_528M;

unsigned long arm_core_clk = 396000000; /* cpu core clk, up to 452MHZ */
unsigned long arm_sys_clk = 396000000; /* ARM_CLK_DIV, system bus clock */
unsigned long platform_bus_clk = 132000000; /* BUS_CLK_DIV, up to 166MHZ */
unsigned long ipg_bus_clk = 66000000; /* IPS clk */

#define SPIN_DELAY	3000000 /* in nanoseconds */

#define AUDIO_VIDEO_MIN_CLK_FREQ	650000000
#define AUDIO_VIDEO_MAX_CLK_FREQ	1300000000

/* We need to check the exp status again after timer expiration,
 * as there might be interrupt coming between the first time exp
 * and the time reading, then the time reading may be several ms
 * after the exp checking due to the irq handle, so we need to
 * check it to make sure the exp return the right value after
 * timer expiration. */
#define WAIT(exp, timeout) \
({ \
	struct timespec nstimeofday; \
	struct timespec curtime; \
	int result = 1; \
	getnstimeofday(&nstimeofday); \
	while (!(exp)) { \
		getnstimeofday(&curtime); \
		if ((curtime.tv_nsec - nstimeofday.tv_nsec) > (timeout)) { \
			if (!(exp)) \
				result = 0; \
			break; \
		} \
	} \
	result; \
})

/* External clock values passed-in by the board code */
static unsigned long external_high_reference, external_low_reference;
static unsigned long oscillator_reference, ckih2_reference;
static unsigned long anaclk_1_reference, anaclk_2_reference;


static int _clk_enable(struct clk *clk)
{
	u32 reg;
	reg = __raw_readl(clk->enable_reg);
	reg |= MXC_CCM_CCGRx_CG_MASK << clk->enable_shift;
	__raw_writel(reg, clk->enable_reg);

	return 0;
}

/* Clock off in all modes */
static void _clk_disable(struct clk *clk)
{
	u32 reg;
	reg = __raw_readl(clk->enable_reg);
	reg &= ~(MXC_CCM_CCGRx_CG_MASK << clk->enable_shift);
	__raw_writel(reg, clk->enable_reg);

}

/* Clock off in wait mode */
static void _clk_disable_inwait(struct clk *clk)
{
	u32 reg;
	reg = __raw_readl(clk->enable_reg);
	reg &= ~(MXC_CCM_CCGRx_CG_MASK << clk->enable_shift);
	reg |= 1 << clk->enable_shift;
	__raw_writel(reg, clk->enable_reg);
}

/*
 * For the 5-to-1 muxed input clock
 */
static inline u32 _get_mux(struct clk *parent, struct clk *m0,
			   struct clk *m1, struct clk *m2,
			   struct clk *m3, struct clk *m4)
{
	if (parent == m0)
		return 0;
	else if (parent == m1)
		return 1;
	else if (parent == m2)
		return 2;
	else if (parent == m3)
		return 3;
	else if (parent == m4)
		return 4;
	else
		BUG();

	return 0;
}

static inline void __iomem *_get_pll_base(struct clk *pll)
{
	if (pll == &pll1_sys_main_clk)
		return PLL1_SYS_BASE_ADDR;
	else if (pll == &pll2_528_bus_main_clk)
		return PLL2_528_BASE_ADDR;
	else if (pll == &pll3_usb_otg_main_clk)
		return PLL3_480_USB1_BASE_ADDR;
	else if (pll == &pll4_audio_main_clk)
		return PLL4_AUDIO_BASE_ADDR;
	else if (pll == &pll5_enet_main_clk)
		return PLL5_ENET_BASE_ADDR;
	else if (pll == &pll6_video_main_clk)
		return PLL6_VIDEO_BASE_ADDR;
	else if (pll == &pll1_pfd3_396M)
		return PLL1_SYS_BASE_ADDR;
	else if (pll == &pll1_pfd4_528M)
		return PLL1_SYS_BASE_ADDR;
	else
		BUG();
	return NULL;
}


/*
 * For the 6-to-1 muxed input clock
 */
static inline u32 _get_mux6(struct clk *parent, struct clk *m0, struct clk *m1,
			    struct clk *m2, struct clk *m3, struct clk *m4,
			    struct clk *m5)
{
	if (parent == m0)
		return 0;
	else if (parent == m1)
		return 1;
	else if (parent == m2)
		return 2;
	else if (parent == m3)
		return 3;
	else if (parent == m4)
		return 4;
	else if (parent == m5)
		return 5;
	else
		BUG();

	return 0;
}
static unsigned long get_high_reference_clock_rate(struct clk *clk)
{
	return external_high_reference;
}

static unsigned long get_low_reference_clock_rate(struct clk *clk)
{
	return external_low_reference;
}

static unsigned long get_oscillator_reference_clock_rate(struct clk *clk)
{
	return oscillator_reference;
}

static unsigned long get_ckih2_reference_clock_rate(struct clk *clk)
{
	return ckih2_reference;
}

static unsigned long _clk_anaclk_1_get_rate(struct clk *clk)
{
	return anaclk_1_reference;
}

static int _clk_anaclk_1_set_rate(struct clk *clk, unsigned long rate)
{
	anaclk_1_reference = rate;
	return 0;
}

static unsigned long _clk_anaclk_2_get_rate(struct clk *clk)
{
	return anaclk_2_reference;
}

static int _clk_anaclk_2_set_rate(struct clk *clk, unsigned long rate)
{
	anaclk_2_reference = rate;
	return 0;
}

/* External high frequency clock */
static struct clk ckih_clk = {
	__INIT_CLK_DEBUG(ckih_clk)
	.get_rate = get_high_reference_clock_rate,
};

static struct clk ckih2_clk = {
	__INIT_CLK_DEBUG(ckih2_clk)
	.get_rate = get_ckih2_reference_clock_rate,
};

static struct clk osc_clk = {
	__INIT_CLK_DEBUG(osc_clk)
	.get_rate = get_oscillator_reference_clock_rate,
};

/* External low frequency (32kHz) clock */
static struct clk ckil_clk = {
	__INIT_CLK_DEBUG(ckil_clk)
	.get_rate = get_low_reference_clock_rate,
};

static struct clk anaclk_1 = {
	__INIT_CLK_DEBUG(anaclk_1)
	.get_rate = _clk_anaclk_1_get_rate,
	.set_rate = _clk_anaclk_1_set_rate,
};

static struct clk anaclk_2 = {
	__INIT_CLK_DEBUG(anaclk_2)
	.get_rate = _clk_anaclk_2_get_rate,
	.set_rate = _clk_anaclk_2_set_rate,
};

static unsigned long pfd_round_rate(struct clk *clk, unsigned long rate)
{
	u32 frac;
	u64 tmp;

	tmp = (u64)clk_get_rate(clk->parent) * 18;
	tmp += rate/2;
	do_div(tmp, rate);
	frac = tmp;
	frac = frac < 12 ? 12 : frac;
	frac = frac > 35 ? 35 : frac;
	tmp = (u64)clk_get_rate(clk->parent) * 18;
	do_div(tmp, frac);
	return tmp;
}

static unsigned long pfd_get_rate(struct clk *clk)
{
	u32 frac;
	u64 tmp;
	tmp = (u64)clk_get_rate(clk->parent) * 18;


	frac = (__raw_readl(clk->enable_reg) >> clk->enable_shift) &
			ANADIG_PFD_FRAC_MASK;

	do_div(tmp, frac);

	return tmp;
}

static int pfd_set_rate(struct clk *clk, unsigned long rate)
{
	u32 reg, frac;
	u64 tmp;
	tmp = (u64)clk_get_rate(clk->parent) * 18;


	/* Round up the divider so that we don't set a rate
	  * higher than what is requested. */
	tmp += rate/2;
	do_div(tmp, rate);
	frac = tmp;
	frac = frac < 12 ? 12 : frac;
	frac = frac > 35 ? 35 : frac;
	/* clear clk frac bits */
	reg = __raw_readl(clk->enable_reg);
	reg &= ~(ANADIG_PFD_FRAC_MASK << clk->enable_shift);
	/* set clk frac bits */
	__raw_writel(reg | (frac << clk->enable_shift),
			clk->enable_reg);

	return 0;
}

static int _clk_pfd_enable(struct clk *clk)
{
	u32 reg;

	reg = __raw_readl(clk->enable_reg);
	/* clear clk gate bit */
	__raw_writel(reg & ~(1 << (clk->enable_shift + 7)),
			clk->enable_reg);

	return 0;
}

static void _clk_pfd_disable(struct clk *clk)
{
	u32 reg;

	reg = __raw_readl(clk->enable_reg);
	/* set clk gate bit */
	__raw_writel(reg | (1 << (clk->enable_shift + 7)),
			clk->enable_reg);

}

static int _clk_pll_enable(struct clk *clk)
{
	unsigned int reg;
	void __iomem *pllbase;

	pllbase = _get_pll_base(clk);

	reg = __raw_readl(pllbase);
	reg &= ~ANADIG_PLL_BYPASS;
	reg &= ~ANADIG_PLL_POWER_DOWN;

	/* The 480MHz PLLs have the opposite definition for power bit. */
	if (clk == &pll3_usb_otg_main_clk)
		reg |= (ANADIG_PLL_POWER_DOWN|ANADIG_PLL_480_EN_USB_CLKS);

	__raw_writel(reg, pllbase);

	/* Wait for PLL to lock */
	if (!WAIT(__raw_readl(pllbase) & ANADIG_PLL_LOCK,
				SPIN_DELAY))
		panic("pll enable failed\n");

	/* Enable the PLL output now*/
	reg = __raw_readl(pllbase);
	reg |= ANADIG_PLL_ENABLE;
	__raw_writel(reg, pllbase);

	if (clk == &pll3_usb_otg_main_clk) {
		/* config OTG2 PLL CLK*/
		reg = __raw_readl(PLL3_480_USB2_BASE_ADDR);
		reg &= ~ANADIG_PLL_BYPASS;
		reg &= ~ANADIG_PLL_POWER_DOWN;
		reg |= (ANADIG_PLL_POWER_DOWN|ANADIG_PLL_480_EN_USB_CLKS);

		__raw_writel(reg, PLL3_480_USB2_BASE_ADDR);

		if (!WAIT(__raw_readl(PLL3_480_USB2_BASE_ADDR) &
					ANADIG_PLL_LOCK, SPIN_DELAY))
			panic("pll enable failed\n");

		reg = __raw_readl(PLL3_480_USB2_BASE_ADDR);
		reg |= ANADIG_PLL_ENABLE;
		__raw_writel(reg, PLL3_480_USB2_BASE_ADDR);
	}
	return 0;
}

static void _clk_pll_disable(struct clk *clk)
{
	unsigned int reg;
	void __iomem *pllbase;

	pllbase = _get_pll_base(clk);

	reg = __raw_readl(pllbase);
	reg |= ANADIG_PLL_BYPASS;
	reg &= ~ANADIG_PLL_ENABLE;

	__raw_writel(reg, pllbase);

}

/* PLL sys: 528 or 480 MHz*/
static unsigned long  _clk_pll1_main_get_rate(struct clk *clk)
{
	unsigned int div;
	unsigned long val;

	/* div_sel: 0 -> Fout = Fref x 20; 1 -> Fout = Fref x 22 */
	div = __raw_readl(PLL1_SYS_BASE_ADDR) & 0x1;
	val = (clk_get_rate(clk->parent) * (div ? 22 : 20));
	return val;
}

static int _clk_pll1_main_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int reg, div;

	div = (rate) / clk_get_rate(clk->parent);

	/* Update div */
	reg = __raw_readl(PLL1_SYS_BASE_ADDR) & ~0x1;

	reg |= ((div > 20) ? 1 : 0);
	__raw_writel(reg, PLL1_SYS_BASE_ADDR);

	/* Wait for PLL1 to lock */
	if (!WAIT(__raw_readl(PLL1_SYS_BASE_ADDR) & ANADIG_PLL_LOCK,
				SPIN_DELAY))
		panic("pll1 enable failed\n");

	return 0;
}

static unsigned long _clk_pll1_pfd2_get_rate(struct clk *clk)
{
	return 452000000;
}

static int _clk_pll1_pfd2_set_rate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static unsigned long _clk_pll1_pfd3_get_rate(struct clk *clk)
{
	return 396000000;
}

static int _clk_pll1_pfd3_set_rate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static unsigned long _clk_pll1_pfd4_get_rate(struct clk *clk)
{
	return 528000000;
}

static int _clk_pll1_pfd4_set_rate(struct clk *clk, unsigned long rate)
{
	return 0;
}
static struct clk pll1_sys_main_clk = {
	__INIT_CLK_DEBUG(pll1_sys_main_clk)
	.parent = &osc_clk,
	.get_rate = _clk_pll1_main_get_rate,
	.set_rate = _clk_pll1_main_set_rate,
	.enable = _clk_pll_enable,
	.disable = _clk_pll_disable,
};

static struct clk pll1_pfd2_452M = {
	__INIT_CLK_DEBUG(pll1_pfd2_452M)
	.parent = &osc_clk,
	.enable_reg = (void *)PFD_528SYS_BASE_ADDR,
	.enable_shift = ANADIG_PFD1_FRAC_OFFSET,
	.get_rate = _clk_pll1_pfd2_get_rate,
	.set_rate = _clk_pll1_pfd2_set_rate,
	.enable = _clk_pfd_enable,
	.disable = _clk_pfd_disable,
};

static struct clk pll1_pfd3_396M = {
	__INIT_CLK_DEBUG(pll1_pfd3_396M)
	.parent = &osc_clk,
	.enable_reg = (void *)PFD_528SYS_BASE_ADDR,
	.enable_shift = ANADIG_PFD2_FRAC_OFFSET,
	.get_rate = _clk_pll1_pfd3_get_rate,
	.set_rate = _clk_pll1_pfd3_set_rate,
	.enable = _clk_pfd_enable,
	.disable = _clk_pfd_disable,
};

static struct clk pll1_pfd4_528M = {
	__INIT_CLK_DEBUG(pll1_pfd4_528M)
	.parent = &osc_clk,
	.enable_reg = (void *)PFD_528SYS_BASE_ADDR,
	.enable_shift = ANADIG_PFD3_FRAC_OFFSET,
	.get_rate = _clk_pll1_pfd4_get_rate,
	.set_rate = _clk_pll1_pfd4_set_rate,
	.enable = _clk_pfd_enable,
	.disable = _clk_pfd_disable,
};

/*
 * PLL PFD output select
 * CCM Clock Switcher Register
 */
static int _clk_pll1_sw_set_parent(struct clk *clk, struct clk *parent)
{
	u32 reg;

	reg = __raw_readl(MXC_CCM_CCSR);
	/* enable PLL1 PFD */
	reg |= 0xf00;

	if (parent == &pll1_sys_main_clk) {
		reg &= ~MXC_CCM_CCSR_PLL1_PFD_CLK_SEL_MASK;

	} else if (parent == &pll1_pfd2_452M) {
		reg &= ~MXC_CCM_CCSR_PLL1_PFD_CLK_SEL_MASK;
		reg |= (0x2 << MXC_CCM_CCSR_PLL1_PFD_CLK_SEL_OFFSET);

	} else if (parent == &pll1_pfd3_396M) {
		reg &= ~MXC_CCM_CCSR_PLL1_PFD_CLK_SEL_MASK;
		reg |= (0x3 << MXC_CCM_CCSR_PLL1_PFD_CLK_SEL_OFFSET);
	} else if (parent == &pll1_pfd4_528M) {
		reg &= ~MXC_CCM_CCSR_PLL1_PFD_CLK_SEL_MASK;
		reg |= (0x4 << MXC_CCM_CCSR_PLL1_PFD_CLK_SEL_OFFSET);
	}
	__raw_writel(reg, MXC_CCM_CCSR);
	return 0;
}

static unsigned long _clk_pll1_sw_get_rate(struct clk *clk)
{
	u32 reg;
	reg = __raw_readl(MXC_CCM_CCSR);
	reg &= MXC_CCM_CCSR_PLL1_PFD_CLK_SEL_MASK;
	reg = (reg >> MXC_CCM_CCSR_PLL1_PFD_CLK_SEL_OFFSET);

	if (reg == 0x1)
		return 500000000;
	else if (reg == 0x2)
		return 452000000;
	else if (reg == 0x3)
		return 396000000;
	else
		return 528000000;

}

static struct clk pll1_sw_clk = {
	__INIT_CLK_DEBUG(pll1_sw_clk)
	.parent = &pll1_sys_main_clk,
	.set_parent = _clk_pll1_sw_set_parent,
	.get_rate = _clk_pll1_sw_get_rate,
};

static unsigned long _clk_pll2_main_get_rate(struct clk *clk)
{
	unsigned int div;
	unsigned long val;

	div = __raw_readl(PLL2_528_BASE_ADDR) & ANADIG_PLL_528_DIV_SELECT;

	if (div == 1)
		val = clk_get_rate(clk->parent) * 22;

	else
		val = clk_get_rate(clk->parent) * 20;

	return val;
}

static int _clk_pll2_main_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int reg,  div;

	if (rate == 528000000)
		div = 1;
	else if (rate == 480000000)
		div = 0;
	else
		return -EINVAL;

	reg = __raw_readl(PLL2_528_BASE_ADDR);
	reg &= ~ANADIG_PLL_528_DIV_SELECT;
	reg |= div;
	__raw_writel(reg, PLL2_528_BASE_ADDR);

	return 0;
}

static struct clk pll2_528_bus_main_clk = {
	__INIT_CLK_DEBUG(pll2_528_bus_main_clk)
	.parent = &osc_clk,
	.get_rate = _clk_pll2_main_get_rate,
	.set_rate = _clk_pll2_main_set_rate,
	.enable = _clk_pll_enable,
	.disable = _clk_pll_disable,
};

static struct clk pll2_pfd2_396M = {
	__INIT_CLK_DEBUG(pll2_pfd_396M)
	.parent = &pll2_528_bus_main_clk,
	.enable_reg = (void *)PFD_528_BASE_ADDR,
	.enable_shift = BP_ANADIG_PFD_528_PFD1_FRAC,
	.enable = _clk_pfd_enable,
	.disable = _clk_pfd_disable,
	.get_rate = pfd_get_rate,
	.set_rate = pfd_set_rate,
	.round_rate = pfd_round_rate,
};

static struct clk pll2_pfd3_339M = {
	__INIT_CLK_DEBUG(pll2_pfd_339M)
	.parent = &pll2_528_bus_main_clk,
	.enable_reg = (void *)PFD_528_BASE_ADDR,
	.enable_shift = BP_ANADIG_PFD_528_PFD2_FRAC,
	.enable = _clk_pfd_enable,
	.disable = _clk_pfd_disable,
	.set_rate = pfd_set_rate,
	.get_rate = pfd_get_rate,
	.round_rate = pfd_round_rate,
};

static struct clk pll2_pfd4_413M = {
	__INIT_CLK_DEBUG(pll2_pfd_413M)
	.parent = &pll2_528_bus_main_clk,
	.enable_reg = (void *)PFD_528_BASE_ADDR,
	.enable_shift = BP_ANADIG_PFD_528_PFD3_FRAC,
	.enable = _clk_pfd_enable,
	.disable = _clk_pfd_disable,
	.set_rate = pfd_set_rate,
	.get_rate = pfd_get_rate,
	.round_rate = pfd_round_rate,
};

static unsigned long _clk_pll3_usb_otg_get_rate(struct clk *clk)
{
	unsigned int div;
	unsigned long val;

	div = __raw_readl(PLL3_480_USB1_BASE_ADDR)
		& ANADIG_PLL_480_DIV_SELECT_MASK;

	if (div == 1)
		val = clk_get_rate(clk->parent) * 22;
	else
		val = clk_get_rate(clk->parent) * 20;
	return val;
}

static int _clk_pll3_usb_otg_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int reg,  div;

	if (rate == 528000000)
		div = 1;
	else if (rate == 480000000)
		div = 0;
	else
		return -EINVAL;

	reg = __raw_readl(PLL3_480_USB1_BASE_ADDR);
	reg &= ~ANADIG_PLL_480_DIV_SELECT_MASK;
	reg |= div;
	__raw_writel(reg, PLL3_480_USB1_BASE_ADDR);

	return 0;
}


/* same as pll3_main_clk. These two clocks should always be the same */
static struct clk pll3_usb_otg_main_clk = {
	__INIT_CLK_DEBUG(pll3_usb_otg_main_clk)
	.parent = &osc_clk,
	.enable = _clk_pll_enable,
	.disable = _clk_pll_disable,
	.set_rate = _clk_pll3_usb_otg_set_rate,
	.get_rate = _clk_pll3_usb_otg_get_rate,
};

static unsigned long _clk_usb_get_rate(struct clk *clk)
{
	return 60000000;
}

static struct clk usb_clk = {
	__INIT_CLK_DEBUG(usb_clk)
	.get_rate = _clk_usb_get_rate,
};

/* for USB OTG1 */
static struct clk usb_phy0_clk = {
	__INIT_CLK_DEBUG(usb_phy0_clk)
	.parent = &pll3_usb_otg_main_clk,
	.set_rate = _clk_pll3_usb_otg_set_rate,
	.get_rate = _clk_pll3_usb_otg_get_rate,
};

/* For USB OTG2 */
static struct clk usb_phy1_clk = {
	__INIT_CLK_DEBUG(usb_phy1_clk)
	.parent = &pll3_usb_otg_main_clk,
	.set_rate = _clk_pll3_usb_otg_set_rate,
	.get_rate = _clk_pll3_usb_otg_get_rate,
};


static struct clk pll3_pfd2_396M = {
	__INIT_CLK_DEBUG(pll3_pfd2_396M)
	.parent = &pll3_usb_otg_main_clk,
	.enable_reg = (void *)PFD_480_BASE_ADDR,
	.enable_shift = BP_ANADIG_PFD_480_PFD1_FRAC,
	.enable = _clk_pfd_enable,
	.disable = _clk_pfd_disable,
	.set_rate = pfd_set_rate,
	.get_rate = pfd_get_rate,
	.round_rate = pfd_round_rate,
};

static struct clk pll3_pfd3_308M = {
	__INIT_CLK_DEBUG(pll3_pfd3_309M)
	.parent = &pll3_usb_otg_main_clk,
	.enable_reg = (void *)PFD_480_BASE_ADDR,
	.enable_shift = BP_ANADIG_PFD_480_PFD2_FRAC,
	.enable = _clk_pfd_enable,
	.disable = _clk_pfd_disable,
	.set_rate = pfd_set_rate,
	.get_rate = pfd_get_rate,
	.round_rate = pfd_round_rate,
};

static struct clk pll3_pfd4_320M = {
	__INIT_CLK_DEBUG(pll3_pfd4_320M)
	.parent = &pll3_usb_otg_main_clk,
	.enable_reg = (void *)PFD_480_BASE_ADDR,
	.enable_shift = BP_ANADIG_PFD_480_PFD3_FRAC,
	.enable = _clk_pfd_enable,
	.disable = _clk_pfd_disable,
	.set_rate = pfd_set_rate,
	.get_rate = pfd_get_rate,
	.round_rate = pfd_round_rate,
};

static unsigned long _clk_pll3_sw_get_rate(struct clk *clk)
{
	return clk_get_rate(clk->parent);
}

/* same as pll3_main_clk. These two clocks should always be the same */
static struct clk pll3_sw_clk = {
	__INIT_CLK_DEBUG(pll3_sw_clk)
	.parent = &pll3_usb_otg_main_clk,
	.get_rate = _clk_pll3_sw_get_rate,
};
/*
*/

static unsigned long  _clk_audio_video_get_rate(struct clk *clk)
{
	unsigned int div, mfn, mfd;
	unsigned long rate;
	unsigned int parent_rate = clk_get_rate(clk->parent);
	void __iomem *pllbase;

	unsigned int test_div_sel, control3, post_div = 1;

	if (clk == &pll4_audio_main_clk)
		pllbase = PLL4_AUDIO_BASE_ADDR;
	else
		pllbase = PLL6_VIDEO_BASE_ADDR;


	div = __raw_readl(pllbase) & ANADIG_PLL_SYS_DIV_SELECT_MASK;
	mfn = __raw_readl(pllbase + PLL_NUM_DIV_OFFSET);
	mfd = __raw_readl(pllbase + PLL_DENOM_DIV_OFFSET);

	rate = (parent_rate * div) + ((parent_rate / mfd) * mfn);
	rate = rate / post_div;

	return rate;
}

static int _clk_audio_video_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int reg,  div;
	unsigned int mfn, mfd = 1000000;
	s64 temp64;
	unsigned int parent_rate = clk_get_rate(clk->parent);
	void __iomem *pllbase;
	unsigned long min_clk_rate, pre_div_rate;

	u32 test_div_sel = 2;
	u32 control3 = 0;


	if (clk == &pll4_audio_main_clk)
		min_clk_rate = AUDIO_VIDEO_MIN_CLK_FREQ / 4;
	else
		min_clk_rate = AUDIO_VIDEO_MIN_CLK_FREQ / 16;

	if ((rate < min_clk_rate) || (rate > AUDIO_VIDEO_MAX_CLK_FREQ))
		return -EINVAL;

	if (clk == &pll4_audio_main_clk)
		pllbase = PLL4_AUDIO_BASE_ADDR;
	else
		pllbase = PLL6_VIDEO_BASE_ADDR;

	pre_div_rate = rate;
	div = pre_div_rate / parent_rate;
	temp64 = (u64) (pre_div_rate - (div * parent_rate));
	temp64 *= mfd;
	do_div(temp64, parent_rate);
	mfn = temp64;

	reg = __raw_readl(pllbase)
			& ~ANADIG_PLL_SYS_DIV_SELECT_MASK
			& ~ANADIG_PLL_AV_TEST_DIV_SEL_MASK;
	reg |= div |
		(test_div_sel << ANADIG_PLL_AV_TEST_DIV_SEL_OFFSET);
	__raw_writel(reg, pllbase);
	__raw_writel(mfn, pllbase + PLL_NUM_DIV_OFFSET);
	__raw_writel(mfd, pllbase + PLL_DENOM_DIV_OFFSET);

	return 0;
}

static unsigned long _clk_audio_video_round_rate(struct clk *clk,
						unsigned long rate)
{
	unsigned long min_clk_rate;
	unsigned int div, post_div = 1;
	unsigned int mfn, mfd = 1000000;
	s64 temp64;
	unsigned int parent_rate = clk_get_rate(clk->parent);
	unsigned long pre_div_rate;
	u32 test_div_sel = 2;
	u32 control3 = 0;
	unsigned long final_rate;

	if (clk == &pll4_audio_main_clk)
		min_clk_rate = AUDIO_VIDEO_MIN_CLK_FREQ / 4;
	else
		min_clk_rate = AUDIO_VIDEO_MIN_CLK_FREQ / 16;

	if (rate < min_clk_rate)
		return min_clk_rate;

	if (rate > AUDIO_VIDEO_MAX_CLK_FREQ)
		return AUDIO_VIDEO_MAX_CLK_FREQ;

	pre_div_rate = rate;

	div = pre_div_rate / parent_rate;
	temp64 = (u64) (pre_div_rate - (div * parent_rate));
	temp64 *= mfd;
	do_div(temp64, parent_rate);
	mfn = temp64;

	final_rate = (parent_rate * div) + ((parent_rate / mfd) * mfn);
	final_rate = final_rate / post_div;

	return final_rate;
}

static int _clk_audio_video_set_parent(struct clk *clk, struct clk *parent)
{
	u32 reg;
	int mux;
	void __iomem *pllbase;

	if (clk == &pll4_audio_main_clk)
		pllbase = PLL4_AUDIO_BASE_ADDR;
	else
		pllbase = PLL6_VIDEO_BASE_ADDR;
#if 0
	reg = __raw_readl(pllbase) & ~ANADIG_PLL_BYPASS_CLK_SRC_MASK;
	mux = _get_mux6(parent, &osc_clk, &anaclk_1, &anaclk_2,
				NULL, NULL, NULL);
	reg |= mux << ANADIG_PLL_BYPASS_CLK_SRC_OFFSET;
	__raw_writel(reg, pllbase);

	/* Set anaclk_x as input */
	if (parent == &anaclk_1) {
		reg = __raw_readl(ANADIG_MISC1_REG);
		reg |= (ANATOP_LVDS_CLK1_IBEN_MASK &
				~ANATOP_LVDS_CLK1_OBEN_MASK);
		__raw_writel(reg, ANADIG_MISC1_REG);
	} else if (parent == &anaclk_2) {
		reg = __raw_readl(ANADIG_MISC1_REG);
		reg |= (ANATOP_LVDS_CLK2_IBEN_MASK &
				~ANATOP_LVDS_CLK2_OBEN_MASK);
		__raw_writel(reg, ANADIG_MISC1_REG);
	}
#endif
	return 0;
}

static struct clk pll4_audio_main_clk = {
	__INIT_CLK_DEBUG(pll4_audio_main_clk)
	.parent = &osc_clk,
	.enable = _clk_pll_enable,
	.disable = _clk_pll_disable,
	.set_rate = _clk_audio_video_set_rate,
	.get_rate = _clk_audio_video_get_rate,
	.round_rate = _clk_audio_video_round_rate,
	.set_parent = _clk_audio_video_set_parent,
};

static struct clk pll6_video_main_clk = {
	__INIT_CLK_DEBUG(pll6_video_main_clk)
	.parent = &osc_clk,
	.enable = _clk_pll_enable,
	.disable = _clk_pll_disable,
	.set_rate = _clk_audio_video_set_rate,
	.get_rate = _clk_audio_video_get_rate,
	.round_rate = _clk_audio_video_round_rate,
	.set_parent = _clk_audio_video_set_parent,
};


static struct clk pll5_enet_main_clk = {
	__INIT_CLK_DEBUG(pll5_enet_main_clk)
	.parent = &osc_clk,
	.enable = _clk_pll_enable,
	.disable = _clk_pll_disable,
};

static unsigned long _clk_arm_get_rate(struct clk *clk)
{
	u32 cacrr, div;

	cacrr = __raw_readl(MXC_CCM_CACRR);
	div = (cacrr & MXC_CCM_CACRR_ARM_CLK_DIV_MASK) + 1;
	return arm_core_clk;
	/*return clk_get_rate(clk->parent) / div;*/
}

static int _clk_arm_set_rate(struct clk *clk, unsigned long rate)
{
	u32 reg;
	reg = __raw_readl(MXC_CCM_CCSR);
	reg |= (1 << MXC_CCM_CCSR_CA5_CLK_SEL_OFFSET);
	__raw_writel(reg, MXC_CCM_CCSR);
	return 0;
}


static struct clk cpu_clk = {
	__INIT_CLK_DEBUG(cpu_clk)
	.parent = &pll1_sw_clk, /* A5 clock from PLL1 pfd3 out 396MHZ */
	.set_rate = _clk_arm_set_rate,
	.get_rate = _clk_arm_get_rate,
};
/*
*/

/* platform bus clock parent, CCM_CCSR.SYS_CLK_SEL */
static int _clk_periph_set_parent(struct clk *clk, struct clk *parent)
{
	u32 reg;
	int mux;

	mux = _get_mux6(parent, &ckih_clk, &ckil_clk,
		&pll2_pfd2_396M, &pll2_528_bus_main_clk,
		&pll1_pfd3_396M, &pll3_usb_otg_main_clk);

		reg = __raw_readl(MXC_CCM_CCSR);
		/*  */
		reg &= ~MXC_CCM_CCSR_SYS_CLK_SEL_MASK;
		reg |= mux;
		__raw_writel(reg, MXC_CCM_CCSR);

		/*
		 * Set the BUS_CLK_DIV to 3, 396/3=132
		 * Set IPG_CLK_DIV to 2, 132/2=66
		 */
		reg = __raw_readl(MXC_CCM_CACRR);
		reg &= ~MXC_CCM_CACRR_BUS_CLK_DIV_MASK;
		reg &= ~MXC_CCM_CACRR_IPG_CLK_DIV_MASK;
		reg |= (2 << MXC_CCM_CACRR_BUS_CLK_DIV_OFFSET);
		reg |= (1 << MXC_CCM_CACRR_IPG_CLK_DIV_OFFSET);
		__raw_writel(reg, MXC_CCM_CACRR);

	return 0;
}

static unsigned long _clk_periph_get_rate(struct clk *clk)
{
	unsigned long val = 132000000;
	return val;
}

static struct clk periph_clk = {
	__INIT_CLK_DEBUG(periph_clk)
	.parent = &pll2_pfd2_396M,
	.set_parent = _clk_periph_set_parent,
	.get_rate = _clk_periph_get_rate,
};



static unsigned long _clk_ipg_get_rate(struct clk *clk)
{
	return 66000000;
}


static struct clk ipg_clk = {
	__INIT_CLK_DEBUG(ipg_clk)
	.parent = &periph_clk,
	.get_rate = _clk_ipg_get_rate,
};


static int _clk_enet_set_parent(struct clk *clk, struct clk *parent)
{
	int mux;
	u32 reg = __raw_readl(MXC_CCM_CSCMR2)
		& ~MXC_CCM_CSCMR2_RMII_CLK_SEL_MASK;

	mux = _get_mux6(parent, NULL, NULL, &pll5_enet_main_clk,
			NULL, NULL, NULL);

	reg |= (mux << MXC_CCM_CSCMR2_RMII_CLK_SEL_OFFSET);

	__raw_writel(reg, MXC_CCM_CSCMR2);

	return 0;
}

static int _clk_enet_enable(struct clk *clk)
{
	unsigned int reg;

	/* Enable ENET ref clock */
	reg = __raw_readl(PLL5_ENET_BASE_ADDR);
	reg &= ~ANADIG_PLL_BYPASS;
	reg |= ANADIG_PLL_ENABLE;
	__raw_writel(reg, PLL5_ENET_BASE_ADDR);

	reg = __raw_readl(MXC_CCM_CSCDR1);
	reg |= MXC_CCM_CSCDR1_RMII_CLK_EN;
	__raw_writel(reg, MXC_CCM_CSCDR1);

	_clk_enable(clk);
	return 0;
}

static void _clk_enet_disable(struct clk *clk)
{
	unsigned int reg;

	_clk_disable(clk);

	/* Enable ENET ref clock */
	reg = __raw_readl(PLL5_ENET_BASE_ADDR);
	reg |= ANADIG_PLL_BYPASS;
	reg &= ~ANADIG_PLL_ENABLE;
	__raw_writel(reg, PLL5_ENET_BASE_ADDR);
}

static int _clk_enet_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int reg, div = 1;

	switch (rate) {
	case 25000000:
		div = 0;
		break;
	case 50000000:
		div = 1;
		break;
	case 100000000:
		div = 2;
		break;
	case 125000000:
		div = 3;
		break;
	default:
		return -EINVAL;
	}
	reg = __raw_readl(PLL5_ENET_BASE_ADDR);
	reg &= ~ANADIG_PLL_ENET_DIV_SELECT_MASK;
	reg |= (div << ANADIG_PLL_ENET_DIV_SELECT_OFFSET);
	__raw_writel(reg, PLL5_ENET_BASE_ADDR);

	return 0;
}

static unsigned long _clk_enet_get_rate(struct clk *clk)
{
	unsigned int div;

	div = (__raw_readl(PLL5_ENET_BASE_ADDR))
		& ANADIG_PLL_ENET_DIV_SELECT_MASK;

	switch (div) {
	case 0:
		div = 20;
		break;
	case 1:
		div = 10;
		break;
	case 3:
		div = 5;
		break;
	case 4:
		div = 4;
		break;
	}

	return 500000000 / div;
}

static struct clk enet_clk[] = {
	{
	__INIT_CLK_DEBUG(enet_clk)
	 .id = 0,
	 .parent = &pll5_enet_main_clk,
	 .enable_reg = MXC_CCM_CCGR1,
	 .enable_shift = MXC_CCM_CCGRx_CG5_OFFSET,
	 .set_parent = _clk_enet_set_parent,
	 .enable = _clk_enet_enable,
	 .disable = _clk_enet_disable,
	 .set_rate = _clk_enet_set_rate,
	 .get_rate = _clk_enet_get_rate,
	.secondary = &enet_clk[1],
	.flags = AHB_HIGH_SET_POINT | CPU_FREQ_TRIG_UPDATE,
	},
};



static unsigned long _clk_uart_round_rate(struct clk *clk,
						unsigned long rate)
{
	u32 div;
	u32 parent_rate = clk_get_rate(clk->parent);

	div = parent_rate / rate;

	/* Make sure rate is not greater than the maximum value for the clock.
	 * Also prevent a div of 0.
	 */
	if (div == 0)
		div++;

	if (div > 64)
		div = 64;

	return parent_rate / div;
}
/*
*/
static unsigned long _clk_uart_get_rate(struct clk *clk)
{

	return clk_get_rate(clk->parent);
}

static struct clk uart_clk[] = {
	{
	__INIT_CLK_DEBUG(uart_clk)
	 .id = 0,
	 .parent = &ipg_clk,
	 .enable_reg = MXC_CCM_CCGR0,
	 .enable_shift = MXC_CCM_CCGRx_CG7_OFFSET,
	 .enable = _clk_enable,
	 .disable = _clk_disable,
	 .secondary = &uart_clk[1],
	 .get_rate = _clk_uart_get_rate,
	 .round_rate = _clk_uart_round_rate,
	},
	{
	__INIT_CLK_DEBUG(uart_serial_clk)
	 .id = 1,
	 .enable_reg = MXC_CCM_CCGR0,
	 .enable_shift = MXC_CCM_CCGRx_CG8_OFFSET,
	 .enable = _clk_enable,
	 .disable = _clk_disable,
	},
};

static struct clk dspi_clk[] = {
	{
	__INIT_CLK_DEBUG(dspi0_clk)
	.id = 0,
	.parent = &ipg_clk,
	.enable_reg = MXC_CCM_CCGR0,
	.enable_shift = MXC_CCM_CCGRx_CG12_OFFSET,
	.enable = _clk_enable,
	.disable = _clk_disable,
	},
};

static int _clk_esdhc1_set_parent(struct clk *clk, struct clk *parent)
{
	int mux;
	u32 reg = __raw_readl(MXC_CCM_CSCMR1)
		& ~MXC_CCM_CSCMR1_ESDHC1_CLK_SEL_MASK;

	mux = _get_mux6(parent, &pll3_usb_otg_main_clk, &pll3_pfd3_308M,
			&pll1_pfd3_396M, NULL, NULL, NULL);

	reg |= (mux << MXC_CCM_CSCMR1_ESDHC1_CLK_SEL_OFFSET);

	__raw_writel(reg, MXC_CCM_CSCMR1);

	return 0;
}

static unsigned long _clk_esdhc1_get_rate(struct clk *clk)
{
	u32 reg, div;

	reg = __raw_readl(MXC_CCM_CSCDR2);
	div = ((reg & MXC_CCM_CSCDR2_ESDHC1_DIV_MASK) >>
			MXC_CCM_CSCDR2_ESDHC1_DIV_OFFSET) + 1;

	return clk_get_rate(clk->parent) / div;
}

static int _clk_esdhc1_set_rate(struct clk *clk, unsigned long rate)
{
	u32 reg, div;
	u32 parent_rate = clk_get_rate(clk->parent);

	div = (parent_rate + rate - 1) / rate;
	if (div == 0)
		div++;
	if (((parent_rate / div) > rate) || (div > 8))
		return -EINVAL;

	reg = __raw_readl(MXC_CCM_CSCDR2);
	reg &= ~MXC_CCM_CSCDR2_ESDHC1_DIV_MASK;
	reg |= (div - 1) << MXC_CCM_CSCDR2_ESDHC1_DIV_OFFSET;
	reg |= MXC_CCM_CSCDR2_ESDHC1_EN;
	__raw_writel(reg, MXC_CCM_CSCDR2);

	return 0;
}

static unsigned long _clk_esdhc_round_rate(struct clk *clk, unsigned long rate)
{
	u32 div;
	u32 parent_rate = clk_get_rate(clk->parent);

	div = parent_rate / rate;

	/* Make sure rate is not greater than the maximum value for the clock.
	 * Also prevent a div of 0.
	 */
	if (div == 0)
		div++;

	if (div > 8)
		div = 8;

	return parent_rate / div;
}

static struct clk esdhc1_clk = {
	__INIT_CLK_DEBUG(esdhc1_clk)
	.id = 1,
	.parent = &pll1_pfd3_396M,
	.enable_reg = MXC_CCM_CCGR7,
	.enable_shift = MXC_CCM_CCGRx_CG2_OFFSET,
	.enable = _clk_enable,
	.disable = _clk_disable,
	.set_parent = _clk_esdhc1_set_parent,
	.round_rate = _clk_esdhc_round_rate,
	.set_rate = _clk_esdhc1_set_rate,
	.get_rate = _clk_esdhc1_get_rate,
};

static int _clk_dcu0_set_parent(struct clk *clk, struct clk *parent)
{
	int mux;
	u32 reg = __raw_readl(MXC_CCM_CSCMR1)
		& ~MXC_CCM_CSCMR1_DCU0_CLK_SEL_MASK;

	mux = _get_mux6(parent, &pll1_pfd2_452M, &pll3_usb_otg_main_clk,
			NULL, NULL, NULL, NULL);

	reg |= (mux << MXC_CCM_CSCMR1_DCU0_CLK_SEL_OFFSET);

	__raw_writel(reg, MXC_CCM_CSCMR1);

	return 0;
}

static int _clk_dcu_enable(struct clk *clk)
{
	u32 reg;

	reg = __raw_readl(MXC_CCM_CSCDR3);
	reg |= MXC_CCM_CSCDR3_DCU0_EN;
	__raw_writel(reg, MXC_CCM_CSCDR3);

	return 0;
}

static void _clk_dcu_disable(struct clk *clk)
{
	u32 reg;

	reg = __raw_readl(MXC_CCM_CSCDR3);
	reg &= ~MXC_CCM_CSCDR3_DCU0_EN;
	__raw_writel(reg, MXC_CCM_CSCDR3);

	return 0;
}

static unsigned long _clk_dcu0_get_rate(struct clk *clk)
{
	u32 reg, div;

	reg = __raw_readl(MXC_CCM_CSCDR3);
	div = ((reg & MXC_CCM_CSCDR3_DCU0_DIV_MASK) >>
			MXC_CCM_CSCDR3_DCU0_DIV_OFFSET) + 1;

	return clk_get_rate(clk->parent) / div;
}

static int _clk_dcu0_set_rate(struct clk *clk, unsigned long rate)
{
	u32 reg, div;
	u32 parent_rate = clk_get_rate(clk->parent);

	div = (parent_rate + rate - 1) / rate;
	if (div == 0)
		div++;
	if (((parent_rate / div) != rate) || (div > 8))
		return -EINVAL;

	reg = __raw_readl(MXC_CCM_CSCDR3);
	reg &= ~MXC_CCM_CSCDR3_DCU0_DIV_MASK;
	reg |= (div - 1) << MXC_CCM_CSCDR3_DCU0_DIV_OFFSET;
	__raw_writel(reg, MXC_CCM_CSCDR3);

	return 0;
}

static unsigned long _clk_dcu0_round_rate(struct clk *clk, unsigned long rate)
{
	u32 div;
	u32 parent_rate = clk_get_rate(clk->parent);

	div = parent_rate / rate;

	/* Make sure rate is not greater than the maximum value for the clock.
	 * Also prevent a div of 0.
	 */
	if (div == 0)
		div++;

	if (div > 8)
		div = 8;

	return parent_rate / div;
}

static struct clk dcu0_clk = {
	__INIT_CLK_DEBUG(dcu0_clk)
	.parent = &pll1_pfd2_452M,
	.enable_reg = MXC_CCM_CCGR3,
	.enable_shift = MXC_CCM_CCGRx_CG8_OFFSET,
	.enable = _clk_dcu_enable,
	.disable = _clk_dcu_disable,
	.set_parent = _clk_dcu0_set_parent,
	.round_rate = _clk_dcu0_round_rate,
	.set_rate = _clk_dcu0_set_rate,
	.get_rate = _clk_dcu0_get_rate,
};

static unsigned long get_audio_external_clock_rate(struct clk *clk)
{
	return 24576000;
}

static struct clk audio_external_clk = {
	__INIT_CLK_DEBUG(audio_external_clk)
	.get_rate = get_audio_external_clock_rate,
};

static int _clk_sai2_set_parent(struct clk *clk, struct clk *parent)
{
	int mux;
	u32 reg = __raw_readl(MXC_CCM_CSCMR1)
		& ~MXC_CCM_CSCMR1_SAI2_CLK_SEL_MASK;

	mux = _get_mux6(parent, &audio_external_clk, NULL,
			NULL, &pll4_audio_main_clk, NULL, NULL);

	reg |= (mux << MXC_CCM_CSCMR1_SAI2_CLK_SEL_OFFSET);

	__raw_writel(reg, MXC_CCM_CSCMR1);

	return 0;
}

static unsigned long _clk_sai2_get_rate(struct clk *clk)
{
	u32 reg, div;

	reg = __raw_readl(MXC_CCM_CSCDR1);
	div = ((reg & MXC_CCM_CSCDR1_SAI2_DIV_MASK) >>
			MXC_CCM_CSCDR1_SAI2_DIV_OFFSET) + 1;

	return clk_get_rate(clk->parent) / div;
}

static int _clk_sai2_set_rate(struct clk *clk, unsigned long rate)
{
	u32 reg, div;
	u32 parent_rate = clk_get_rate(clk->parent);

	div = parent_rate / rate;
	if (div == 0)
		div++;
	if (((parent_rate / div) != rate) || (div > 16))
		return -EINVAL;

	reg = __raw_readl(MXC_CCM_CSCDR1);
	reg &= ~MXC_CCM_CSCDR1_SAI2_DIV_MASK;
	reg |= (div - 1) << MXC_CCM_CSCDR1_SAI2_DIV_OFFSET;
	reg |= MXC_CCM_CSCDR1_SAI2_EN;
	__raw_writel(reg, MXC_CCM_CSCDR1);

	return 0;
}

static int _clk_sai2_enable(struct clk *clk)
{
	u32 reg;

	reg = __raw_readl(MXC_CCM_CSCDR1);
	reg |= MXC_CCM_CSCDR1_SAI2_EN;
	__raw_writel(reg, MXC_CCM_CSCDR1);

	return 0;
}

static void _clk_sai2_disable(struct clk *clk)
{
	u32 reg;

	reg = __raw_readl(MXC_CCM_CSCDR1);
	reg &= ~MXC_CCM_CSCDR1_SAI2_EN;
	__raw_writel(reg, MXC_CCM_CSCDR1);

	return 0;
}

static unsigned long _clk_sai_round_rate(struct clk *clk,
						unsigned long rate)
{
	u32 div;
	u32 parent_rate = clk_get_rate(clk->parent);

	div = parent_rate / rate;

	/* Make sure rate is not greater than the maximum value for the clock.
	 * Also prevent a div of 0.
	 */
	if (div == 0)
		div++;

	if (div > 8)
		div = 8;

	return parent_rate / div;
}

static struct clk sai2_clk = {
	__INIT_CLK_DEBUG(sai2_clk)
	.parent = &audio_external_clk,
	.enable_reg = MXC_CCM_CCGR1,
	.enable_shift = MXC_CCM_CCGRx_CG1_OFFSET,
	.enable = _clk_sai2_enable,
	.disable = _clk_sai2_disable,
	.set_parent = _clk_sai2_set_parent,
	.round_rate = _clk_sai_round_rate,
	.set_rate = _clk_sai2_set_rate,
	.get_rate = _clk_sai2_get_rate,
};

static int _clk_enable1(struct clk *clk)
{
	u32 reg;
	reg = __raw_readl(clk->enable_reg);
	reg |= 1 << clk->enable_shift;
	__raw_writel(reg, clk->enable_reg);

	return 0;
}

static void _clk_disable1(struct clk *clk)
{
	u32 reg;
	reg = __raw_readl(clk->enable_reg);
	reg &= ~(1 << clk->enable_shift);
	__raw_writel(reg, clk->enable_reg);
}

static int _clk_clko_set_parent(struct clk *clk, struct clk *parent)
{
	u32 sel, reg;

	if (parent == &pll3_usb_otg_main_clk)
		sel = 0;
	else if (parent == &pll2_528_bus_main_clk)
		sel = 1;
	else if (parent == &pll1_sys_main_clk)
		sel = 2;
	else if (parent == &pll6_video_main_clk)
		sel = 3;
	else if (parent == &pll4_audio_main_clk)
		sel = 15;
	else
		return -EINVAL;
	return 0;
}

static unsigned long _clk_clko_get_rate(struct clk *clk)
{
	return 0;
}

static int _clk_clko_set_rate(struct clk *clk, unsigned long rate)
{
	u32 reg;
	u32 parent_rate = clk_get_rate(clk->parent);
	u32 div = parent_rate / rate;

	if (div == 0)
		div++;
	if (((parent_rate / div) != rate) || (div > 8))
		return -EINVAL;

	return 0;
}

static unsigned long _clk_clko_round_rate(struct clk *clk,
						unsigned long rate)
{
	u32 parent_rate = clk_get_rate(clk->parent);
	u32 div = parent_rate / rate;

	/* Make sure rate is not greater than the maximum value for the clock.
	 * Also prevent a div of 0.
	 */
	if (div == 0)
		div++;
	if (div > 8)
		div = 8;
	return parent_rate / div;
}

static int _clk_clko2_set_parent(struct clk *clk, struct clk *parent)
{
	u32 sel, reg;
	return 0;
}

static unsigned long _clk_clko2_get_rate(struct clk *clk)
{
	return 0;
}

static int _clk_clko2_set_rate(struct clk *clk, unsigned long rate)
{
	u32 reg;
	u32 parent_rate = clk_get_rate(clk->parent);
	u32 div = parent_rate / rate;

	if (div == 0)
		div++;
	if (((parent_rate / div) != rate) || (div > 8))
		return -EINVAL;
	return 0;
}

static struct clk clko_clk = {
	__INIT_CLK_DEBUG(clko_clk)
	.parent = &pll2_528_bus_main_clk,
	.enable = _clk_enable1,
	.disable = _clk_disable1,
	.set_parent = _clk_clko_set_parent,
	.set_rate = _clk_clko_set_rate,
	.get_rate = _clk_clko_get_rate,
	.round_rate = _clk_clko_round_rate,
};

static struct clk clko2_clk = {
	__INIT_CLK_DEBUG(clko2_clk)
	.enable = _clk_enable1,
	.disable = _clk_disable1,
	.set_parent = _clk_clko2_set_parent,
	.set_rate = _clk_clko2_set_rate,
	.get_rate = _clk_clko2_get_rate,
	.round_rate = _clk_clko_round_rate,
};
static struct clk caam_clk = {
	__INIT_CLK_DEBUG(caam_clk)
	.parent = &ipg_clk,
	.enable_reg = MXC_CCM_CCGR11,
	.enable_shift = MXC_CCM_CCGRx_CG0_OFFSET,
	.enable = _clk_enable,
	.disable = _clk_disable,
};

static struct clk pit_clk = {
	__INIT_CLK_DEBUG(pit_clk)
	 .parent = &ipg_clk,
#if 0
	.enable_reg = MXC_CCM_CCGR1,
	.enable_shift = MXC_CCM_CCGRx_CG7_OFFSET,
	.enable = _clk_enable,
	.disable = _clk_disable,
#endif
	 .get_rate = _clk_uart_get_rate,
};

static struct clk adc_clk[] = {
	{
		__INIT_CLK_DEBUG(adc_clk)
		.id = 0,
		.parent = &ipg_clk,
		.enable_reg = MXC_CCM_CCGR1,
		.enable_shift = MXC_CCM_CCGRx_CG11_OFFSET,
		.enable = _clk_enable,
		.disable = _clk_disable,
	},
};

static struct clk i2c_clk[] = {
	{
		__INIT_CLK_DEBUG(i2c_clk_0)
		.id = 0,
		.parent = &ipg_clk,
		.enable_reg = MXC_CCM_CCGR4,
		.enable_shift = MXC_CCM_CCGRx_CG6_OFFSET,
		.enable = _clk_enable,
		.disable = _clk_disable,
	},
	{
		__INIT_CLK_DEBUG(i2c_clk_1)
		.id = 1,
		.parent = &ipg_clk,
		.enable_reg = MXC_CCM_CCGR4,
		.enable_shift = MXC_CCM_CCGRx_CG7_OFFSET,
		.enable = _clk_enable,
		.disable = _clk_disable,
	},
	{
		__INIT_CLK_DEBUG(i2c_clk_2)
		.id = 2,
		.parent = &ipg_clk,
		.enable_reg = MXC_CCM_CCGR10,
		.enable_shift = MXC_CCM_CCGRx_CG6_OFFSET,
		.enable = _clk_enable,
		.disable = _clk_disable,
	},
	{
		__INIT_CLK_DEBUG(i2c_clk_3)
		.id = 3,
		.parent = &ipg_clk,
		.enable_reg = MXC_CCM_CCGR10,
		.enable_shift = MXC_CCM_CCGRx_CG7_OFFSET,
		.enable = _clk_enable,
		.disable = _clk_disable,
	},
};

static struct clk wdt_clk[] = {
	{
		__INIT_CLK_DEBUG(wdt_clk)
		.id = 0,
		.parent = &ipg_clk,
		.enable_reg = MXC_CCM_CCGR1,
		.enable_shift = MXC_CCM_CCGRx_CG7_OFFSET,
		.enable = _clk_enable,
		.disable = _clk_disable,
	},
};

static int ftm_pwm_clk_enable(struct clk *pwm_clk)
{
	u32 reg;
	/* enable FTM fixed and external clk */
	reg = __raw_readl(MXC_CCM_CSCDR1);
	reg |= (0x0F << 25);
	__raw_writel(reg, MXC_CCM_CSCDR1);

	return 0;
}
static void ftm_pwm_clk_disable(struct clk *pwm_clk)
{
	u32 reg;
	reg = __raw_readl(MXC_CCM_CSCDR1);
	reg &= ~(0x0F << 25);
	__raw_writel(reg, MXC_CCM_CSCDR1);

}

static struct clk ftm_pwm_clk = {
	__INIT_CLK_DEBUG(ftm_pwm_clk)
	.parent = &ipg_clk,
	.enable = ftm_pwm_clk_enable,
	.disable = ftm_pwm_clk_disable,

};

static int _clk_qspi0_set_parent(struct clk *clk, struct clk *parent)
{
	int mux;
	u32 reg = __raw_readl(MXC_CCM_CSCMR1)
		& ~MXC_CCM_CSCMR1_QSPI0_CLK_SEL_MASK;

	mux = _get_mux6(parent, &pll3_usb_otg_main_clk, &pll3_pfd4_320M,
		&pll2_pfd4_413M, &pll1_pfd4_528M, NULL, NULL);

	reg |= (mux << MXC_CCM_CSCMR1_QSPI0_CLK_SEL_OFFSET);

	__raw_writel(reg, MXC_CCM_CSCMR1);

	return 0;
}

static int _clk_qspi1_set_parent(struct clk *clk, struct clk *parent)
{
	int mux;
	u32 reg = __raw_readl(MXC_CCM_CSCMR1)
		& ~MXC_CCM_CSCMR1_QSPI1_CLK_SEL_MASK;

	mux = _get_mux6(parent, &pll3_usb_otg_main_clk, &pll3_pfd4_320M ,
		&pll2_pfd4_413M, &pll1_pfd4_528M, NULL, NULL);

	reg |= (mux << MXC_CCM_CSCMR1_QSPI1_CLK_SEL_OFFSET);

	__raw_writel(reg, MXC_CCM_CSCMR1);

	return 0;
}

static unsigned long _clk_qspi0_get_rate(struct clk *clk)
{
	u32 reg, div4, div2, div;

	reg = __raw_readl(MXC_CCM_CSCDR3);
	div4 = ((reg & MXC_CCM_CSCDR3_QSPI0_X4_DIV_MASK) >>
			MXC_CCM_CSCDR3_QSPI0_X4_DIV_OFFSET) + 1;

	div2 = ((reg & MXC_CCM_CSCDR3_QSPI0_X2_DIV_MASK) >>
			MXC_CCM_CSCDR3_QSPI0_X2_DIV_OFFSET) + 1;

	div = ((reg & MXC_CCM_CSCDR3_QSPI0_DIV_MASK) >>
			MXC_CCM_CSCDR3_QSPI0_DIV_OFFSET) + 1;

	return clk_get_rate(clk->parent) / div4 / div2 / div;
}

static unsigned long _clk_qspi1_get_rate(struct clk *clk)
{
	u32 reg, div4, div2, div;

	reg = __raw_readl(MXC_CCM_CSCDR3);
	div4 = ((reg & MXC_CCM_CSCDR3_QSPI1_X4_DIV_MASK) >>
			MXC_CCM_CSCDR3_QSPI1_X4_DIV_OFFSET) + 1;

	div2 = ((reg & MXC_CCM_CSCDR3_QSPI1_X2_DIV_MASK) >>
			MXC_CCM_CSCDR3_QSPI1_X2_DIV_OFFSET) + 1;

	div = ((reg & MXC_CCM_CSCDR3_QSPI1_DIV_MASK) >>
			MXC_CCM_CSCDR3_QSPI1_DIV_OFFSET) + 1;

	return clk_get_rate(clk->parent) / div4 / div2 / div;
}

static int _clk_qspi0_set_rate(struct clk *clk, unsigned long rate)
{
	u32 reg, div;
	u32 parent_rate = clk_get_rate(clk->parent);

	reg = __raw_readl(MXC_CCM_CSCDR3);
	reg &= ~MXC_CCM_CSCDR3_QSPI0_X4_DIV_MASK;
	reg |= 0x1 << MXC_CCM_CSCDR3_QSPI0_X4_DIV_OFFSET;

	reg &= ~MXC_CCM_CSCDR3_QSPI0_X2_DIV_MASK;
	reg |= 0x01 << MXC_CCM_CSCDR3_QSPI0_X2_DIV_OFFSET;

	reg &= ~MXC_CCM_CSCDR3_QSPI0_DIV_MASK;
	reg |= 0x01 << MXC_CCM_CSCDR3_QSPI0_DIV_OFFSET;

	reg |= MXC_CCM_CSCDR3_QSPI0_EN;
	__raw_writel(reg, MXC_CCM_CSCDR3);

	return 0;
}

static int _clk_qspi1_set_rate(struct clk *clk, unsigned long rate)
{
	u32 reg, div;
	u32 parent_rate = clk_get_rate(clk->parent);

	reg = __raw_readl(MXC_CCM_CSCDR3);
	reg &= ~MXC_CCM_CSCDR3_QSPI1_X4_DIV_MASK;
	reg |= 0x1 << MXC_CCM_CSCDR3_QSPI1_X4_DIV_OFFSET;

	reg &= ~MXC_CCM_CSCDR3_QSPI1_X2_DIV_MASK;
	reg |= 0x01 << MXC_CCM_CSCDR3_QSPI1_X2_DIV_OFFSET;

	reg &= ~MXC_CCM_CSCDR3_QSPI1_DIV_MASK;
	reg |= 0x01 << MXC_CCM_CSCDR3_QSPI1_DIV_OFFSET;

	reg |= MXC_CCM_CSCDR3_QSPI1_EN;
	__raw_writel(reg, MXC_CCM_CSCDR3);

	return 0;
}

static unsigned long _clk_qspi_round_rate(struct clk *clk, unsigned long rate)
{
	return 66000000;
}

static struct clk qspi0_clk = {
	__INIT_CLK_DEBUG(qspi0_clk)
	.id = 0,
	.parent = &pll1_pfd4_528M,
	.enable_reg = MXC_CCM_CCGR2,
	.enable_shift = MXC_CCM_CCGRx_CG4_OFFSET,
	.enable = _clk_enable,
	.disable = _clk_disable,
	.set_parent = _clk_qspi0_set_parent,
	.round_rate = _clk_qspi_round_rate,
	.set_rate = _clk_qspi0_set_rate,
	.get_rate = _clk_qspi0_get_rate,
};

static struct clk qspi1_clk = {
	__INIT_CLK_DEBUG(quadspi1_clk)
	.id = 1,
	.parent = &pll1_pfd4_528M,
	.enable_reg = MXC_CCM_CCGR8,
	.enable_shift = MXC_CCM_CCGRx_CG4_OFFSET,
	.enable = _clk_enable,
	.disable = _clk_disable,
	.set_parent = _clk_qspi1_set_parent,
	.round_rate = _clk_qspi_round_rate,
	.set_rate = _clk_qspi1_set_rate,
	.get_rate = _clk_qspi1_get_rate,
};

static int _clk_asrc_serial_set_rate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static struct clk asrc_clk[] = {
	{
		__INIT_CLK_DEBUG(asrc_clk)
		.id = 0,
		.parent = &ipg_clk,
		.enable_reg = MXC_CCM_CCGR4,
		.enable_shift = MXC_CCM_CCGRx_CG1_OFFSET,
		.enable = _clk_enable,
		.disable = _clk_disable,
	},
	{
		__INIT_CLK_DEBUG(asrc_serial_clk)
		.id = 1,
		.parent = &audio_external_clk,
		.set_rate = _clk_asrc_serial_set_rate,
	},
};

static struct clk dummy_clk = {
	.id = 0,
};

#define _REGISTER_CLOCK(d, n, c) \
	{ \
		.dev_id = d, \
		.con_id = n, \
		.clk = &c, \
	}


static struct clk_lookup lookups[] = {
	_REGISTER_CLOCK(NULL, "osc", osc_clk),
	_REGISTER_CLOCK(NULL, "ckih", ckih_clk),
	_REGISTER_CLOCK(NULL, "ckih2", ckih2_clk),
	_REGISTER_CLOCK(NULL, "ckil", ckil_clk),
	_REGISTER_CLOCK(NULL, "pll1_main_clk", pll1_sys_main_clk),
	_REGISTER_CLOCK(NULL, "pll1_pfd2_452M", pll1_pfd2_452M),
	_REGISTER_CLOCK(NULL, "pll1_pfd3_396M", pll1_pfd3_396M),
	_REGISTER_CLOCK(NULL, "pll1_pfd4_528M", pll1_pfd4_528M),
	_REGISTER_CLOCK(NULL, "pll1_sw_clk", pll1_sw_clk), /*PLL1 pfd out clk*/
	_REGISTER_CLOCK(NULL, "pll2_main_clk", pll2_528_bus_main_clk),
	_REGISTER_CLOCK(NULL, "pll2_pfd2_396M", pll2_pfd2_396M),
	_REGISTER_CLOCK(NULL, "pll2_pfd3_339M", pll2_pfd3_339M),
	_REGISTER_CLOCK(NULL, "pll2_pfd4_413M", pll2_pfd4_413M),
	_REGISTER_CLOCK(NULL, "pll3_main_clk", pll3_usb_otg_main_clk),
	_REGISTER_CLOCK(NULL, "pll3_pfd2_396M", pll3_pfd2_396M),
	_REGISTER_CLOCK(NULL, "pll3_pfd3_308M", pll3_pfd3_308M),
	_REGISTER_CLOCK(NULL, "pll3_pfd4_320M", pll3_pfd4_320M),
	_REGISTER_CLOCK(NULL, "pll4", pll4_audio_main_clk),
	_REGISTER_CLOCK(NULL, "pll5", pll6_video_main_clk),
	_REGISTER_CLOCK(NULL, "pll6", pll5_enet_main_clk),
	_REGISTER_CLOCK(NULL, "cpu_clk", cpu_clk), /* arm core clk */
	_REGISTER_CLOCK(NULL, "periph_clk", periph_clk), /* platform bus clk */
	_REGISTER_CLOCK(NULL, "ipg_clk", ipg_clk),
//	_REGISTER_CLOCK(NULL, "audio ext clk", audio_external_clk),
	_REGISTER_CLOCK(NULL, "mvf-uart.0", uart_clk[0]),
	_REGISTER_CLOCK(NULL, "mvf-uart.1", uart_clk[0]),
	_REGISTER_CLOCK(NULL, "mvf-uart.2", uart_clk[0]),
	_REGISTER_CLOCK(NULL, "mvf-uart.3", uart_clk[0]),
	_REGISTER_CLOCK("mvf-dspi.0", NULL, dspi_clk[0]),
	_REGISTER_CLOCK("pit", NULL, pit_clk),
	_REGISTER_CLOCK("fec.0", NULL, enet_clk[0]),
	_REGISTER_CLOCK("fec.1", NULL, enet_clk[1]),
	_REGISTER_CLOCK("mvf-adc.0", NULL, adc_clk[0]),
	_REGISTER_CLOCK("switch.0", NULL, enet_clk[0]),
	_REGISTER_CLOCK("imx2-wdt.0", NULL, dummy_clk),
	_REGISTER_CLOCK("sdhci-esdhc-imx.1", NULL, esdhc1_clk),
	_REGISTER_CLOCK("mvf-dcu.0", NULL, dcu0_clk),
//	_REGISTER_CLOCK("mvf-sai.0", NULL, sai2_clk),
//	_REGISTER_CLOCK(NULL, "i2c_clk", i2c_clk[2]),
	_REGISTER_CLOCK("imx-i2c.2", NULL, i2c_clk[2]),
	_REGISTER_CLOCK(NULL, "usb-clk", usb_clk),
	_REGISTER_CLOCK(NULL, "mvf-usb.0", usb_phy0_clk),
	_REGISTER_CLOCK(NULL, "mvf-usb.1", usb_phy1_clk),
	_REGISTER_CLOCK(NULL, "pwm", ftm_pwm_clk),
	_REGISTER_CLOCK("mvf-qspi.0", NULL, qspi0_clk),
	_REGISTER_CLOCK(NULL, "asrc_clk", asrc_clk[0]),
	_REGISTER_CLOCK(NULL, "asrc_serial_clk", asrc_clk[1]),
 	_REGISTER_CLOCK(NULL, "caam_clk", caam_clk),
};

static void clk_tree_init(void)
{
	unsigned int reg = 0xffffffff;

	/* enable all ips clock by Clock Gating Register*/
	__raw_writel(reg, MXC_CCM_CCGR0);
	__raw_writel(reg, MXC_CCM_CCGR1);
	__raw_writel(reg, MXC_CCM_CCGR2);
	__raw_writel(reg, MXC_CCM_CCGR3);
	__raw_writel(reg, MXC_CCM_CCGR4);
	__raw_writel(reg, MXC_CCM_CCGR5);
	__raw_writel(reg, MXC_CCM_CCGR6);
	__raw_writel(reg, MXC_CCM_CCGR7);
	__raw_writel(reg, MXC_CCM_CCGR8);
	__raw_writel(reg, MXC_CCM_CCGR9);
	__raw_writel(reg, MXC_CCM_CCGR10);
	__raw_writel(reg, MXC_CCM_CCGR11);
}


int __init mvf_clocks_init(unsigned long ckil, unsigned long osc,
	unsigned long ckih1, unsigned long ckih2)
{
	__iomem void *base;
	int i;

	external_low_reference = ckil;
	external_high_reference = ckih1;
	ckih2_reference = ckih2;
	oscillator_reference = osc;

	apll_base = MVF_IO_ADDRESS(MVF_ANATOP_BASE_ADDR);

	for (i = 0; i < ARRAY_SIZE(lookups); i++)
		clkdev_add(&lookups[i]);

	clk_tree_init();

	/* Disable un-necessary PFDs & PLLs */

	/* keep correct count. */
	cpu_clk.usecount++;
	pll1_sys_main_clk.usecount += 5;
	pll2_528_bus_main_clk.usecount += 5;
	periph_clk.usecount++;
	ipg_clk.usecount++;
#if 0
	clk_set_parent(&periph_clk, &pll2_pfd2_396M);
	clk_enable(&periph_clk); /* platform bus clk */
	clk_enable(&ipg_clk); /* ips bus clk */
#endif
	clk_enable(&pll3_usb_otg_main_clk);

	base = MVF_IO_ADDRESS(MVF_PIT_BASE_ADDR);

	pit_timer_init(&pit_clk, base, MVF_INT_PIT);

	/*clk_set_parent(&enet_clk, &pll5_enet_main_clk);*/

	clk_set_parent(&esdhc1_clk, &pll1_pfd3_396M);
	clk_set_rate(&esdhc1_clk, 200000000);

	clk_set_parent(&dcu0_clk, &pll1_pfd2_452M);
	clk_set_rate(&dcu0_clk, 113000000);

	clk_set_parent(&sai2_clk, &audio_external_clk);
	clk_set_rate(&sai2_clk, 24576000);

	clk_set_parent(&qspi0_clk, &pll1_pfd4_528M);
	clk_set_rate(&qspi0_clk, 66000000);
	return 0;

}
