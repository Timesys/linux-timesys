/*
 * Copyright (C) 2014-2015 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0+ and/or BSD-3-Clause
 * The GPL-2.0+ license for this file can be found in the COPYING.GPL file
 * included with this distribution or at
 * http://www.gnu.org/licenses/gpl-2.0.html
 * The BSD-3-Clause License for this file can be found in the COPYING.BSD file
 * included with this distribution or at
 * http://opensource.org/licenses/BSD-3-Clause
 */

/*
 * Define the phiscal memory address on A9 and shared M4,
 * This definition should be aligned on both A9 and M4
 */

#ifndef __MCC_VF610__
#define __MCC_VF610__

#include <linux/interrupt.h>

#define MCC_VECTOR_NUMBER_INVALID     (0)
#define VF610_A5_CORE_NUMBER (0)
#define VF610_M4_CORE_NUMBER (1)

// for interrupts
#define VF610_INT_CPU_INT0	(32)
#define VF610_MSCM_BASE_ADDR (0x40001000)
#define VF610_MAX_CPU_TO_CPU_INTERRUPTS (4)
#define VF610_MSCM_IRCPnIR_INT0_MASK          (0x00000001)
#define CPU_LOGICAL_NUMBER                      (MCC_CORE_NUMBER)
#define VF610_MSCM_IRCPnIR    ((CPU_LOGICAL_NUMBER == 0x0) ? 0x800 : 0x804)
#define VF610_MSCM_IRCPGIR_CPUTL_SHIFT (16)
#define VF610_MSCM_WRITE(data, offset)        writel(data, mscm_base + offset)
#define VF610_MSCM_IRCPGIR (0x820)
#define MCC_INTERRUPT(n) ((n == 0 ? 1 : 2) << VF610_MSCM_IRCPGIR_CPUTL_SHIFT)


static __iomem void *mscm_base;

/* Return core num. A5 0, M4 1 */
unsigned int _psp_core_num(void);
unsigned int _psp_node_num(void);
int mcc_wait_for_buffer_freed(MCC_RECEIVE_BUFFER **buffer, unsigned int timeout);
int mcc_wait_for_buffer_queued(MCC_ENDPOINT *endpoint, unsigned int timeout);

unsigned int mcc_get_cpu_to_cpu_vector(unsigned int);
void mcc_clear_cpu_to_cpu_interrupt(void);
int mcc_triger_cpu_to_cpu_interrupt(void);
int imx_mcc_bsp_int_disable(void);
int imx_mcc_bsp_int_enable(void);
irqreturn_t cpu_to_cpu_irq_handler(int irq, void *dev_id);

#endif /* __MCC_VF610__ */
