/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/iram_alloc.h>
#include <linux/delay.h>

#include <mach/hardware.h>
#include <asm/io.h>
#include <asm/mach/map.h>
#include <mach/mvf.h>
#include "crm_regs.h"


struct cpu_op *(*get_cpu_op)(int *op);
bool enable_wait_mode;

void __iomem *gpc_base;
void __iomem *ccm_base;

static int cpu_silicon_rev = -1;
#define SI_REV_OFFSET	0x80

static int get_mvf_srev(void)
{
	void __iomem *romcp = ioremap(BOOT_ROM_BASE_ADDR, SZ_8K);
	u32 rev;

	if (!romcp) {
		cpu_silicon_rev = -EINVAL;
		return 0;
	}

	rev = __raw_readl(romcp + SI_REV_OFFSET);
	rev &= 0xff;

	iounmap(romcp);
	if (rev == 0x10)
		return IMX_CHIP_REVISION_1_0;
	else if (rev == 0x11)
		return IMX_CHIP_REVISION_1_1;
	else if (rev == 0x20)
		return IMX_CHIP_REVISION_2_0;
	return 0;
}

/*
 * Returns:
 *	the silicon revision of the cpu
 *	-EINVAL - not a mvf
 */
int mvf_revision(void)
{

	if (cpu_silicon_rev == -1)
		cpu_silicon_rev = get_mvf_srev();

	return cpu_silicon_rev;

}
EXPORT_SYMBOL(mvf_revision);

static int __init post_cpu_init(void)
{
	iram_init(MVF_IRAM_BASE_ADDR, MVF_IRAM_SIZE);

	/* Move wait routine into iRAM */
	ccm_base = MVF_IO_ADDRESS(MVF_CCM_BASE_ADDR);

	return 0;
}
postcore_initcall(post_cpu_init);
