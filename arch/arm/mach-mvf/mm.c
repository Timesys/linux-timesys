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

/*
 * Create static mapping between physical to virtual memory.
 */

#include <linux/mm.h>
#include <linux/init.h>

#include <asm/mach/map.h>
#include <mach/iomux-v3.h>

#include <mach/hardware.h>
#include <mach/common.h>
#include <mach/iomux-v3.h>
#include <asm/hardware/cache-l2x0.h>
#include <mach/mvf.h>

/*!
 * This structure defines the MVF memory map.
 */
static struct map_desc mvf_io_desc[] __initdata = {
	{
	.virtual = (unsigned long)MVF_IO_ADDRESS(BOOT_ROM_BASE_ADDR),
	.pfn = __phys_to_pfn(BOOT_ROM_BASE_ADDR),
	.length = ROMCP_SIZE,
	.type = MT_DEVICE},
	{
	.virtual = (unsigned long)MVF_IO_ADDRESS(MVF_AIPS0_BASE_ADDR),
	.pfn = __phys_to_pfn(MVF_AIPS0_BASE_ADDR),
	.length = MVF_AIPS0_SIZE,
	.type = MT_DEVICE},
	{
	.virtual = (unsigned long)MVF_IO_ADDRESS(MVF_AIPS1_BASE_ADDR),
	.pfn = __phys_to_pfn(MVF_AIPS1_BASE_ADDR),
	.length = MVF_AIPS1_SIZE,
	.type = MT_DEVICE},
};
/*!
 * This function initializes the memory map. It is called during the
 * system startup to create static physical to virtual memory map for
 * the IO modules.
 */
void __init mvf_map_io(void)
{
	iotable_init(mvf_io_desc, ARRAY_SIZE(mvf_io_desc));
	mxc_iomux_v3_init(MVF_IO_ADDRESS(MVF_IOMUXC_BASE_ADDR));
	mxc_arch_reset_init(MVF_IO_ADDRESS(MVF_WDOG1_BASE_ADDR));
	mxc_set_cpu_type(MXC_CPU_MVF);
}
#ifdef CONFIG_CACHE_L2X0
int mxc_init_l2x0(void)
{
	unsigned int val;

	//Read the fuse bit in MSCM_CPxCFG1 register to determine if L2 cache present.
	//For the Cortex-A5 core in Vybrid,
	//if L2 is present, then L2WY = 0x08 (8-way set-associative)

	val = readl(MVF_IO_ADDRESS(MVF_MSCM_BASE_ADDR + MVF_MSCM_CPxCFG1));

	if(((val & MVF_MSCM_L2WY) >> 16) != 0x00)
	{
		writel(0x132, MVF_IO_ADDRESS(L2_BASE_ADDR + L2X0_TAG_LATENCY_CTRL));
		writel(0x132, MVF_IO_ADDRESS(L2_BASE_ADDR + L2X0_DATA_LATENCY_CTRL));

		val = readl(MVF_IO_ADDRESS(L2_BASE_ADDR + L2X0_PREFETCH_CTRL));
		val |= 0x40800000;
		writel(val, MVF_IO_ADDRESS(L2_BASE_ADDR + L2X0_PREFETCH_CTRL));
		val = readl(MVF_IO_ADDRESS(L2_BASE_ADDR + L2X0_POWER_CTRL));
		val |= L2X0_DYNAMIC_CLK_GATING_EN;
		val |= L2X0_STNDBY_MODE_EN;
		writel(val, MVF_IO_ADDRESS(L2_BASE_ADDR + L2X0_POWER_CTRL));

		l2x0_init(MVF_IO_ADDRESS(L2_BASE_ADDR), 0x0, ~0x00000000);
		return 0;
	}
	else
	{
		//No L2 cache present, return no such device / address.
		printk("L2x0: L2 cache not present");
		return -ENXIO;
	}
}


arch_initcall(mxc_init_l2x0);
#endif
