/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/iram_alloc.h>
#include <linux/interrupt.h>
#include <asm/tlb.h>
#include <asm/mach/map.h>
#include <mach/hardware.h>
#include <asm/hardware/gic.h>
#include <mach/mxc_uart.h>
#include "crm_regs.h"
#include "regs-anadig.h"
#include "regs-pm.h"
#include "regs-src.h"

static struct clk *cpu_clk;
static struct clk *ipg_clk;
static struct clk *periph_clk;

extern void mvf_suspend(suspend_state_t state);

static struct device *pm_dev;
static void __iomem *scu_base;
static void __iomem *gpc_base;
static void __iomem *src_base;
static void __iomem *anadig_base;

static void *suspend_iram_base;
static void (*suspend_in_iram)(suspend_state_t state,
	unsigned long iram_paddr, unsigned long suspend_iram_base) = NULL;
static unsigned long iram_paddr, cpaddr;

static u32 ccm_ccr, ccm_clpcr, ccm_ccsr, scu_ctrl;
static u32 gpc_imr[4], gpc_pgcr, gpc_lpmr;
static u32 ccm_anadig_pfd528, ccm_anadig_pfd480_usb0, ccm_anadig_2p5;
static int g_state;

static void mvf_suspend_store(void)
{
	/* save some settings before suspend */
	ccm_ccr = __raw_readl(MXC_CCM_CCR);
	ccm_clpcr = __raw_readl(MXC_CCM_CLPCR);
	ccm_ccsr = __raw_readl(MXC_CCM_CCSR);
	ccm_anadig_pfd528 = __raw_readl(PFD_528_BASE_ADDR);
	ccm_anadig_pfd480_usb0 = __raw_readl(PFD_480_BASE_ADDR);
	ccm_anadig_2p5 = __raw_readl(ANADIG_2P5_ADDR);

	scu_ctrl = __raw_readl(scu_base + SCU_CTRL_OFFSET);
	gpc_imr[0] = __raw_readl(gpc_base + GPC_IMR1_OFFSET);
	gpc_imr[1] = __raw_readl(gpc_base + GPC_IMR2_OFFSET);
	gpc_imr[2] = __raw_readl(gpc_base + GPC_IMR3_OFFSET);
	gpc_imr[3] = __raw_readl(gpc_base + GPC_IMR4_OFFSET);
	gpc_pgcr = __raw_readl(gpc_base + GPC_PGCR_OFFSET);
	gpc_lpmr = __raw_readl(gpc_base + GPC_LPMR_OFFSET);
}

static void mvf_suspend_restore(void)
{
	/* restore settings after suspend */
	__raw_writel(ccm_anadig_2p5, ANADIG_2P5_ADDR);

	udelay(50);

	__raw_writel(ccm_ccr, MXC_CCM_CCR);
	__raw_writel(ccm_clpcr, MXC_CCM_CLPCR);
	__raw_writel(ccm_anadig_pfd528, PFD_528_BASE_ADDR);
	__raw_writel(ccm_anadig_pfd480_usb0, PFD_480_BASE_ADDR);

	__raw_writel(scu_ctrl, scu_base + SCU_CTRL_OFFSET);
	__raw_writel(gpc_imr[0], gpc_base + GPC_IMR1_OFFSET);
	__raw_writel(gpc_imr[1], gpc_base + GPC_IMR2_OFFSET);
	__raw_writel(gpc_imr[2], gpc_base + GPC_IMR3_OFFSET);
	__raw_writel(gpc_imr[3], gpc_base + GPC_IMR4_OFFSET);
	__raw_writel(gpc_pgcr, gpc_base + GPC_PGCR_OFFSET);
	__raw_writel(gpc_lpmr, gpc_base + GPC_LPMR_OFFSET);

	/* enable PLLs */
	__raw_writel(__raw_readl(PLL3_480_USB1_BASE_ADDR) | ANADIG_PLL_ENABLE,
		PLL3_480_USB1_BASE_ADDR);
	__raw_writel(__raw_readl(PLL3_480_USB2_BASE_ADDR) | ANADIG_PLL_ENABLE,
		PLL3_480_USB2_BASE_ADDR);
	__raw_writel(__raw_readl(PLL4_AUDIO_BASE_ADDR) | ANADIG_PLL_ENABLE,
		PLL4_AUDIO_BASE_ADDR);
	__raw_writel(__raw_readl(PLL6_VIDEO_BASE_ADDR) | ANADIG_PLL_ENABLE,
		PLL6_VIDEO_BASE_ADDR);
	__raw_writel(__raw_readl(PLL5_ENET_BASE_ADDR) | ANADIG_PLL_ENABLE,
		PLL5_ENET_BASE_ADDR);
	__raw_writel(__raw_readl(PLL1_SYS_BASE_ADDR) | ANADIG_PLL_ENABLE,
		PLL1_SYS_BASE_ADDR);

	/* restore system clock */
	__raw_writel(ccm_ccsr, MXC_CCM_CCSR);
}

static void uart_reinit(unsigned long int clkspeed, unsigned long int baud)
{
	void __iomem *membase;
	u8 tmp;
	u16 sbr, brfa;

	membase = MVF_IO_ADDRESS(MVF_UART1_BASE_ADDR);

	__raw_writeb(0, membase + MXC_UARTMODEM);

	/* make sure the transmitter and receiver are
	 * disabled while changing settings */
	tmp = __raw_readb(membase + MXC_UARTCR2);
	tmp &= ~MXC_UARTCR2_RE;
	tmp &= ~MXC_UARTCR2_TE;
	__raw_writeb(tmp, membase + MXC_UARTCR2);

	__raw_writeb(0, membase + MXC_UARTCR1);

	sbr = (u16) ((clkspeed * 1000) / (baud * 16));
	brfa = ((clkspeed * 1000) / baud) - (sbr * 16);

	tmp = __raw_readb(membase + MXC_UARTBDH);
	tmp &= ~MXC_UARTBDH_SBR_MASK;
	tmp |= ((sbr & 0x1f00) >> 8);
	__raw_writeb(tmp, membase + MXC_UARTBDH);
	tmp = sbr & 0x00ff;
	__raw_writeb(tmp, membase + MXC_UARTBDL);

	tmp = __raw_readb(membase + MXC_UARTCR4);
	tmp &= ~MXC_UARTCR4_BRFA_MASK;
	tmp |= (brfa & MXC_UARTCR4_BRFA_MASK);
	__raw_writeb(tmp , membase + MXC_UARTCR4);

	tmp = __raw_readb(membase + MXC_UARTCR2);
	tmp |= MXC_UARTCR2_RE;
	tmp |= MXC_UARTCR2_TE;
	__raw_writeb(tmp, membase + MXC_UARTCR2);
}

static int mvf_suspend_enter(suspend_state_t state)
{
	struct gic_dist_state gds;
	struct gic_cpu_state gcs;
	bool arm_pg = false;

	mvf_suspend_store();

	switch (state) {
	case PM_SUSPEND_MEM:
		mvf_cpu_lp_set(LOW_POWER_STOP);
		g_state = PM_SUSPEND_MEM;
		arm_pg = true;
		break;
	case PM_SUSPEND_STANDBY:
		mvf_cpu_lp_set(LOW_POWER_RUN);
		g_state = PM_SUSPEND_STANDBY;
		arm_pg = true;
		break;
	default:
		return -EINVAL;
	}

	if (state == PM_SUSPEND_MEM || state == PM_SUSPEND_STANDBY) {
		local_flush_tlb_all();
		flush_cache_all();

		if (arm_pg) {
			/* preserve GIC state */
			save_gic_dist_state(0, &gds);
			save_gic_cpu_state(0, &gcs);
		}

		suspend_in_iram(state, (unsigned long)iram_paddr,
			(unsigned long)suspend_iram_base);

		/* reconfigure UART1 when using 24MHz system clock */
		uart_reinit(4000, 115200);

		printk(KERN_DEBUG "Read GPC_PGSR register: %x\n",
			__raw_readl(gpc_base + GPC_PGSR_OFFSET));

		/* mask all interrupts */
		__raw_writel(0xffffffff, gpc_base + GPC_IMR1_OFFSET);
		__raw_writel(0xffffffff, gpc_base + GPC_IMR2_OFFSET);
		__raw_writel(0xffffffff, gpc_base + GPC_IMR3_OFFSET);
		__raw_writel(0xffffffff, gpc_base + GPC_IMR4_OFFSET);

		udelay(80);

		if (arm_pg) {
			/* restore GIC registers */
			restore_gic_dist_state(0, &gds);
			restore_gic_cpu_state(0, &gcs);
		}

		mvf_suspend_restore();

		/* reconfigure UART1 when using 396MHz system clock */
		uart_reinit(66000, 115200);

		__raw_writel(BM_ANADIG_ANA_MISC0_STOP_MODE_CONFIG,
			ANADIG_MISC0_REG);
	} else
		cpu_do_idle();

	return 0;
}

static void mvf_suspend_finish(void)
{
	if (g_state == PM_SUSPEND_MEM)
		free_irq(MVF_INT_WKPU0, NULL);
}

static int mvf_pm_valid(suspend_state_t state)
{
	return (state > PM_SUSPEND_ON && state <= PM_SUSPEND_MAX);
}

const struct platform_suspend_ops mvf_suspend_ops = {
	.valid = mvf_pm_valid,
	.enter = mvf_suspend_enter,
	.finish = mvf_suspend_finish,
};

static int __devinit mvf_pm_probe(struct platform_device *pdev)
{
	pm_dev = &pdev->dev;

	return 0;
}

static struct platform_driver mvf_pm_driver = {
	.driver = {
		   .name = "imx_pm",
	},
	.probe = mvf_pm_probe,
};

static int __init pm_init(void)
{
	scu_base = MVF_IO_ADDRESS(MVF_SCUGIC_BASE_ADDR);
	gpc_base = MVF_GPC_BASE;
	anadig_base = MXC_PLL_BASE;
	src_base = MVF_IO_ADDRESS(MVF_SRC_BASE_ADDR);

	pr_info("Static Power Management for Freescale Vybrid\n");

	if (platform_driver_register(&mvf_pm_driver) != 0) {
		printk(KERN_ERR "mvf_pm_driver register failed\n");
		return -ENODEV;
	}

	suspend_set_ops(&mvf_suspend_ops);

	/* move suspend routine into sram */
	cpaddr = (unsigned long)iram_alloc(SZ_32K, &iram_paddr);

	/* need to remap the area here since we want the memory region
	 * to be executable */
	suspend_iram_base = __arm_ioremap(iram_paddr, SZ_32K,
		MT_MEMORY_NONCACHED);
	printk(KERN_DEBUG "cpaddr = %x suspend_iram_base=%x iram_paddr %x\n",
		(unsigned int)cpaddr, (unsigned int)suspend_iram_base,
		(unsigned int)iram_paddr);

	/* need to run the suspend code from sram */
	memcpy((void *)cpaddr, mvf_suspend, SZ_32K);

	suspend_in_iram = (void *)suspend_iram_base;

	cpu_clk = clk_get(NULL, "cpu_clk");
	if (IS_ERR(cpu_clk)) {
		printk(KERN_DEBUG "%s: failed to get cpu_clk\n", __func__);
		return PTR_ERR(cpu_clk);
	}
	ipg_clk = clk_get(NULL, "ipg_clk");
	if (IS_ERR(ipg_clk)) {
		printk(KERN_DEBUG "%s: failed to get ipg_clk\n", __func__);
		return PTR_ERR(ipg_clk);
	}
	periph_clk = clk_get(NULL, "periph_clk");
	if (IS_ERR(periph_clk)) {
		printk(KERN_DEBUG "%s: failed to get periph_clk\n", __func__);
		return PTR_ERR(periph_clk);
	}

	printk(KERN_INFO "PM driver module loaded\n");
	return 0;
}

static void __exit pm_cleanup(void)
{
	platform_driver_unregister(&mvf_pm_driver);
}

module_init(pm_init);
module_exit(pm_cleanup);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("PM driver");
MODULE_LICENSE("GPL");
