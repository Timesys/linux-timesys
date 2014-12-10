/*
 * Copyright 2011-2013 Freescale Semiconductor, Inc.
 * Copyright 2011 Linaro Ltd.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/io.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/irqchip/arm-gic.h>
#include "common.h"

#define IMX6_GPC_IMR1		0x008
#define VF610_GPC_IMR1		0x044
#define GPC_PGC_CPU_PDN		0x2a0

#define IMR_NUM			4

static void __iomem *gpc_base;
static void __iomem *gpc_imr_base;
static bool has_cpu_pdn;
static u32 gpc_wake_irqs[IMR_NUM];
static u32 gpc_saved_imrs[IMR_NUM];

void imx_gpc_pre_suspend(bool arm_power_off)
{
	int i;

	/* Tell GPC to power off ARM core when suspend */
	if (arm_power_off && has_cpu_pdn)
		writel_relaxed(0x1, gpc_base + GPC_PGC_CPU_PDN);

	for (i = 0; i < IMR_NUM; i++) {
		gpc_saved_imrs[i] = readl_relaxed(gpc_imr_base + i * 4);
		writel_relaxed(~gpc_wake_irqs[i], gpc_imr_base + i * 4);
	}
}

void imx_gpc_post_resume(void)
{
	int i;

	/* Keep ARM core powered on for other low-power modes */
	if (has_cpu_pdn)
		writel_relaxed(0x0, gpc_base + GPC_PGC_CPU_PDN);

	for (i = 0; i < IMR_NUM; i++)
		writel_relaxed(gpc_saved_imrs[i], gpc_imr_base + i * 4);
}

static int imx_gpc_irq_set_wake(struct irq_data *d, unsigned int on)
{
	unsigned int idx = d->irq / 32 - 1;
	u32 mask;

	/* Sanity check for SPI irq */
	if (d->irq < 32)
		return -EINVAL;

	mask = 1 << d->irq % 32;
	gpc_wake_irqs[idx] = on ? gpc_wake_irqs[idx] | mask :
				  gpc_wake_irqs[idx] & ~mask;

	return 0;
}

void imx_gpc_mask_all(void)
{
	int i;

	for (i = 0; i < IMR_NUM; i++) {
		gpc_saved_imrs[i] = readl_relaxed(gpc_imr_base + i * 4);
		writel_relaxed(~0, gpc_imr_base + i * 4);
	}

}

void imx_gpc_restore_all(void)
{
	int i;

	for (i = 0; i < IMR_NUM; i++)
		writel_relaxed(gpc_saved_imrs[i], gpc_imr_base + i * 4);
}

void imx_gpc_irq_unmask(struct irq_data *d)
{
	void __iomem *reg;
	u32 val;

	/* Sanity check for SPI irq */
	if (d->irq < 32)
		return;

	reg = gpc_imr_base + (d->irq / 32 - 1) * 4;
	val = readl_relaxed(reg);
	val &= ~(1 << d->irq % 32);
	writel_relaxed(val, reg);
}

void imx_gpc_irq_mask(struct irq_data *d)
{
	void __iomem *reg;
	u32 val;

	/* Sanity check for SPI irq */
	if (d->irq < 32)
		return;

	reg = gpc_imr_base + (d->irq / 32 - 1) * 4;
	val = readl_relaxed(reg);
	val |= 1 << (d->irq % 32);
	writel_relaxed(val, reg);
}

static void __init imx_gpc_init(void)
{
	int i;

	WARN_ON(!gpc_base);

	/* Initially mask all interrupts */
	for (i = 0; i < IMR_NUM; i++)
		writel_relaxed(~0, gpc_imr_base + i * 4);

	/* Register GPC as the secondary interrupt controller behind GIC */
	gic_arch_extn.irq_mask = imx_gpc_irq_mask;
	gic_arch_extn.irq_unmask = imx_gpc_irq_unmask;
	gic_arch_extn.irq_set_wake = imx_gpc_irq_set_wake;
}

void __init imx6_gpc_init(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "fsl,imx6q-gpc");
	gpc_base = of_iomap(np, 0);
	gpc_imr_base = gpc_base + IMX6_GPC_IMR1;
	has_cpu_pdn = true;

	imx_gpc_init();
}

void __init vf610_gpc_init(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "fsl,vf610-gpc");
	gpc_base = of_iomap(np, 0);
	gpc_imr_base = gpc_base + VF610_GPC_IMR1;
	has_cpu_pdn = false;

	imx_gpc_init();
}
