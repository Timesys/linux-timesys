/*
 * Copyright 2007 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * MTD primitives for XIP support. Architecture specific functions
 *
 * Do not include this file directly. It's included from linux/mtd/xip.h
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef __ARCH_MXC_MTD_XIP_H__
#define __ARCH_MXC_MTD_XIP_H__

#include <asm/hardware.h>
#include <asm/arch/system.h>
#define xip_irqpending()        \
        (~(__raw_readl(AVIC_NIVECSR) & __raw_readl(AVIC_FIVECSR)))
#define xip_currtime()          get_cycles()
#define xip_elapsed_since(x)    \
        (signed)(((xip_currtime() - (x)) * USEC_PER_SEC) / LATCH)

/*
 * Wait For Interrupt command for XIP kernel to put CPU in Idle mode
 */
#define xip_cpu_idle()  arch_idle()

#endif				/* __ARCH_MXC_MTD_XIP_H__ */
