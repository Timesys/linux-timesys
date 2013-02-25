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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <asm/hardware/gic.h>
#include <mach/hardware.h>

int mvf_register_gpios(void);
unsigned int gpc_wake_irq[4];

static int mvf_gic_irq_set_wake(struct irq_data *d, unsigned int enable)
{
	if ((d->irq < MXC_INT_START) || (d->irq > MXC_INT_END)) {
		printk(KERN_ERR "Invalid irq number!\n");
		return -EINVAL;
	}

	if (enable) {
		gpc_wake_irq[d->irq / 32 - 1] |= 1 << (d->irq % 32);
		printk(KERN_DEBUG "add wake up source irq %d\n", d->irq);
	} else {
		printk(KERN_DEBUG "remove wake up source irq %d\n", d->irq);
		gpc_wake_irq[d->irq / 32 - 1] &= ~(1 << (d->irq % 32));
	}
	return 0;
}
void mvf_init_irq(void)
{
	unsigned int i;
	void __iomem *int_router_base =
				MVF_IO_ADDRESS(MVF_MSCM_INT_ROUTER_BASE);
	struct irq_desc *desc;

	/* start offset if private timer irq id, which is 29.
	 * ID table:
	 * Global timer, PPI -> ID27
	 * A legacy nFIQ, PPI -> ID28
	 * Private timer, PPI -> ID29
	 * Watchdog timers, PPI -> ID30
	 * A legacy nIRQ, PPI -> ID31
	 */
	gic_init(0, 27, MVF_IO_ADDRESS(MVF_INTD_BASE_ADDR),
		MVF_IO_ADDRESS(MVF_SCUGIC_BASE_ADDR + 0x100));

	for (i = MXC_INT_START; i <= MXC_INT_END; i++) {
		desc = irq_to_desc(i);
		desc->irq_data.chip->irq_set_wake = mvf_gic_irq_set_wake;
	}

	mvf_register_gpios();

	for (i = 0; i < 112; i++)
		__raw_writew(1, int_router_base + 0x80 + 2 * i);
}
