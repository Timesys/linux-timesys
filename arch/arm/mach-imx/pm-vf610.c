/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 * Copyright 2014 Toradex AG
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/genalloc.h>
#include <linux/mfd/syscon.h>
#include <linux/mfd/syscon/imx6q-iomuxc-gpr.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/clk.h>
#include <asm/cacheflush.h>
#include <asm/fncpy.h>
#include <asm/proc-fns.h>
#include <asm/suspend.h>
#include <asm/tlb.h>

#include "common.h"

#define CCR				0x0
#define BM_CCR_FIRC_EN			(0x1 << 16)
#define BM_CCR_FXOSC_EN			(0x1 << 12)

#define CCSR				0x8
#define BM_CCSR_DDRC_CLK_SEL		(0x1 << 6)
#define BM_CCSR_FAST_CLK_SEL		(0x1 << 5)
#define BM_CCSR_SLOW_CLK_SEL		(0x1 << 4)
#define BM_CCSR_SYS_CLK_SEL_MASK	(0x7 << 0)

#define CLPCR				0x2c
#define BM_CLPCR_ARM_CLK_DIS_ON_LPM	(0x1 << 5)
#define BM_CLPCR_SBYOS			(0x1 << 6)
#define BM_CLPCR_DIS_REF_OSC		(0x1 << 7)
#define BM_CLPCR_ANADIG_STOP_MODE	(0x1 << 8)
#define BM_CLPCR_FXOSC_BYPSEN		(0x1 << 10)
#define BM_CLPCR_FXOSC_PWRDWN		(0x1 << 11)
#define BM_CLPCR_MASK_CORE0_WFI		(0x1 << 22)
#define BM_CLPCR_MASK_CORE1_WFI		(0x1 << 23)
#define BM_CLPCR_MASK_SCU_IDLE		(0x1 << 24)
#define BM_CLPCR_MASK_L2CC_IDLE		(0x1 << 25)

#define CGPR				0x64
#define BM_CGPR_INT_MEM_CLK_LPM		(0x1 << 17)

#define GPC_PGCR			0x0
#define BM_PGCR_DS_STOP			(0x1 << 7)
#define BM_PGCR_DS_LPSTOP		(0x1 << 6)
#define BM_PGCR_WB_STOP			(0x1 << 4)
#define BM_PGCR_HP_OFF			(0x1 << 3)

#define GPC_LPMR			0x40
#define BM_LPMR_RUN			0x0
#define BM_LPMR_STOP			0x2

#define ANATOP_PLL1_CTRL		0x270
#define ANATOP_PLL2_CTRL		0x30
#define ANATOP_PLL2_PFD			0x100
#define ANATOP_PLL3_CTRL		0x10
#define ANATOP_PLL4_CTRL		0x70
#define ANATOP_PLL5_CTRL		0xe0
#define ANATOP_PLL6_CTRL		0xa0
#define ANATOP_PLL7_CTRL		0x10
#define BM_PLL_POWERDOWN	(0x1 << 12)
#define BM_PLL_ENABLE		(0x1 << 13)
#define BM_PLL_BYPASS		(0x1 << 16)
#define BM_PLL_LOCK		(0x1 << 31)
#define BM_PLL_PFD2_CLKGATE	(0x1 << 15)

#define VF610_SUSPEND_OCRAM_SIZE	0x4000

#define VF_UART0_BASE_ADDR	0x40027000
#define VF_UART1_BASE_ADDR	0x40028000
#define VF_UART2_BASE_ADDR	0x40029000
#define VF_UART3_BASE_ADDR	0x4002a000
#define VF_UART_BASE_ADDR(n)	VF_UART##n##_BASE_ADDR
#define VF_UART_BASE(n)		VF_UART_BASE_ADDR(n)
#define VF_UART_PHYSICAL_BASE	VF_UART_BASE(CONFIG_DEBUG_VF_UART_PORT)

static void __iomem *ccm_base;
static struct vf610_cpu_pm_info *pm_info;

/*
 * suspend ocram space layout:
 * ======================== high address ======================
 *                              .
 *                              .
 *                              .
 *                              ^
 *                              ^
 *                              ^
 *                      vf610_suspend code
 *              PM_INFO structure(vf610_cpu_pm_info)
 * ======================== low address =======================
 */

struct vf610_pm_base {
	phys_addr_t pbase;
	void __iomem *vbase;
};

struct vf610_pm_socdata {
	const char *src_compat;
	const char *gpc_compat;
	const char *anatop_compat;
};

static const struct vf610_pm_socdata vf610_pm_data __initconst = {
	.src_compat = "fsl,vf610-src",
	.gpc_compat = "fsl,vf610-gpc",
	.anatop_compat = "fsl,vf610-anatop",
};

/*
 * This structure is for passing necessary data for low level ocram
 * suspend code(arch/arm/mach-imx/suspend-vf610.S), if this struct
 * definition is changed, the offset definition in
 * arch/arm/mach-imx/suspend-vf610.S must be also changed accordingly,
 * otherwise, the suspend to ocram function will be broken!
 */
struct vf610_cpu_pm_info {
	phys_addr_t pbase; /* The physical address of pm_info. */
	phys_addr_t resume_addr; /* The physical resume address for asm code */
	u32 cpu_type; /* Currently not used, leave it for alignment */
	u32 pm_info_size; /* Size of pm_info. */
	struct vf610_pm_base anatop_base;
	struct vf610_pm_base src_base;
	struct vf610_pm_base ccm_base;
	struct vf610_pm_base gpc_base;
	struct vf610_pm_base l2_base;
	u32 ccm_ccsr;
} __aligned(8);

#ifdef DEBUG
static void uart_reinit(unsigned long int rate, unsigned long int baud)
{
	void __iomem *membase = ioremap(VF_UART_PHYSICAL_BASE, 0x1000);
	u8 tmp;
	u16 sbr, brfa;

	/* UART_C2 */
	__raw_writeb(0, membase + 0x3);

	sbr = (u16) (rate / (baud * 16));
	brfa = (rate / baud) - (sbr * 16);

	tmp = ((sbr & 0x1f00) >> 8);
	__raw_writeb(tmp, membase + 0x0);
	tmp = sbr & 0x00ff;
	__raw_writeb(tmp, membase + 0x1);

	/* UART_C4 */
	__raw_writeb(brfa & 0xf, membase + 0xa);

	/* UART_C2 */
	__raw_writeb(0xac, membase + 0x3);

	iounmap(membase);
}
#else
static void uart_reinit(unsigned long int rate, unsigned long int baud) {}
#endif

static void vf610_set(void __iomem *pll_base, u32 mask)
{
	writel_relaxed(readl_relaxed(pll_base) | mask, pll_base);
}

static void vf610_clr(void __iomem *pll_base, u32 mask)
{
	writel_relaxed(readl_relaxed(pll_base) & ~mask, pll_base);
}

int vf610_set_lpm(enum vf610_cpu_pwr_mode mode)
{
	u32 ccr = readl_relaxed(ccm_base + CCR);
	u32 ccsr = readl_relaxed(ccm_base + CCSR);
	u32 cclpcr = readl_relaxed(ccm_base + CLPCR);
	void __iomem *gpc_base = pm_info->gpc_base.vbase;
	u32 gpc_pgcr = readl_relaxed(gpc_base + GPC_PGCR);
	void __iomem *anatop = pm_info->anatop_base.vbase;

	switch (mode) {
	case VF610_STOP:
		cclpcr &= ~BM_CLPCR_ANADIG_STOP_MODE;
		cclpcr |= BM_CLPCR_ARM_CLK_DIS_ON_LPM;
		cclpcr &= ~BM_CLPCR_SBYOS;
		writel_relaxed(cclpcr, ccm_base + CLPCR);

		gpc_pgcr |= BM_PGCR_DS_STOP;
		gpc_pgcr |= BM_PGCR_HP_OFF;
		writel_relaxed(gpc_pgcr, gpc_base + GPC_PGCR);

		writel_relaxed(BM_LPMR_STOP, gpc_base + GPC_LPMR);
		/* fall-through */
	case VF610_LP_RUN:
		/* Store clock settings */
		pm_info->ccm_ccsr = ccsr;

		ccr |= BM_CCR_FIRC_EN;
		writel_relaxed(ccr, ccm_base + CCR);

		/* Enable PLL2 for DDR clock */
		vf610_set(anatop + ANATOP_PLL2_CTRL, BM_PLL_ENABLE);
		vf610_clr(anatop + ANATOP_PLL2_CTRL, BM_PLL_POWERDOWN);
		vf610_clr(anatop + ANATOP_PLL2_CTRL, BM_PLL_BYPASS);
		while (!(readl(anatop + ANATOP_PLL2_CTRL) & BM_PLL_LOCK));
		vf610_clr(anatop + ANATOP_PLL2_PFD, BM_PLL_PFD2_CLKGATE);

		/* Switch internal OSC's */
		ccsr &= ~BM_CCSR_FAST_CLK_SEL;
		ccsr &= ~BM_CCSR_SLOW_CLK_SEL;

		/* Select PLL2 as DDR clock */
		ccsr &= ~BM_CCSR_DDRC_CLK_SEL;
		writel_relaxed(ccsr, ccm_base + CCSR);

		ccsr &= ~BM_CCSR_SYS_CLK_SEL_MASK;
		writel_relaxed(ccsr, ccm_base + CCSR);
		uart_reinit(4000000UL, 115200);

		vf610_set(anatop + ANATOP_PLL1_CTRL, BM_PLL_BYPASS);
		vf610_clr(anatop + ANATOP_PLL3_CTRL, BM_PLL_ENABLE);
		vf610_clr(anatop + ANATOP_PLL5_CTRL, BM_PLL_ENABLE);
		vf610_clr(anatop + ANATOP_PLL7_CTRL, BM_PLL_ENABLE);
		break;
	case VF610_RUN:
		vf610_clr(anatop + ANATOP_PLL1_CTRL, BM_PLL_BYPASS);
		vf610_set(anatop + ANATOP_PLL3_CTRL, BM_PLL_ENABLE);
		vf610_set(anatop + ANATOP_PLL5_CTRL, BM_PLL_ENABLE);
		vf610_set(anatop + ANATOP_PLL7_CTRL, BM_PLL_ENABLE);

		/* Restore clock settings */
		writel_relaxed(pm_info->ccm_ccsr, ccm_base + CCSR);

		/* Disable PLL2 if not needed */
		if (pm_info->ccm_ccsr & BM_CCSR_DDRC_CLK_SEL)
			vf610_set(anatop + ANATOP_PLL2_CTRL, BM_PLL_POWERDOWN);

		uart_reinit(83368421UL, 115200);

		writel_relaxed(BM_LPMR_RUN, gpc_base + GPC_LPMR);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int vf610_pm_enter(suspend_state_t state)
{
	switch (state) {
	case PM_SUSPEND_STANDBY:
		vf610_set_lpm(VF610_LP_RUN);
		imx_gpc_pre_suspend(false);

		/* zzZZZzzz */
		cpu_do_idle();

		imx_gpc_post_resume();
		vf610_set_lpm(VF610_RUN);
		break;
	case PM_SUSPEND_MEM:
		vf610_set_lpm(VF610_STOP);
		imx_gpc_pre_suspend(false);

		cpu_do_idle();

		imx_gpc_post_resume();
		vf610_set_lpm(VF610_RUN);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int vf610_pm_valid(suspend_state_t state)
{
	return (state == PM_SUSPEND_STANDBY || state == PM_SUSPEND_MEM);
}

static const struct platform_suspend_ops vf610_pm_ops = {
	.enter = vf610_pm_enter,
	.valid = vf610_pm_valid,
};

void __init vf610_pm_set_ccm_base(void __iomem *base)
{
	ccm_base = base;
}

static int __init imx_pm_get_base(struct vf610_pm_base *base,
				const char *compat)
{
	struct device_node *node;
	struct resource res;
	int ret = 0;

	node = of_find_compatible_node(NULL, NULL, compat);
	if (!node) {
		ret = -ENODEV;
		goto out;
	}

	ret = of_address_to_resource(node, 0, &res);
	if (ret)
		goto put_node;

	base->pbase = res.start;
	base->vbase = ioremap(res.start, resource_size(&res));

	if (!base->vbase)
		ret = -ENOMEM;

put_node:
	of_node_put(node);
out:
	return ret;
}

static int __init vf610_suspend_init(const struct vf610_pm_socdata *socdata)
{
	int ret = 0;

	suspend_set_ops(&vf610_pm_ops);

	if (!socdata) {
		pr_warn("%s: invalid argument!\n", __func__);
		return -EINVAL;
	}

	pm_info = kzalloc(sizeof(*pm_info), GFP_KERNEL);
	pm_info->pm_info_size = sizeof(*pm_info);

	/*
	 * ccm physical address is not used by asm code currently,
	 * so get ccm virtual address directly, as we already have
	 * it from ccm driver.
	 */
	pm_info->ccm_base.vbase = ccm_base;

	ret = imx_pm_get_base(&pm_info->anatop_base, socdata->anatop_compat);
	if (ret) {
		pr_warn("%s: failed to get anatop base %d!\n", __func__, ret);
		goto free_memory;
	}

	ret = imx_pm_get_base(&pm_info->gpc_base, socdata->gpc_compat);
	if (ret) {
		pr_warn("%s: failed to get gpc base %d!\n", __func__, ret);
		goto gpc_map_failed;
	}

	return 0;

gpc_map_failed:
	iounmap(&pm_info->anatop_base.vbase);
free_memory:
	kfree(pm_info);

	return ret;
}

void __init vf610_pm_init(void)
{
	int ret;

	WARN_ON(!ccm_base);

	if (IS_ENABLED(CONFIG_SUSPEND)) {
		ret = vf610_suspend_init(&vf610_pm_data);
		if (ret)
			pr_warn("%s: No DDR LPM support with suspend %d!\n",
				__func__, ret);
	}
}

