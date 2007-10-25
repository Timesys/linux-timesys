/* REVISIT Doxygen fixups */
/*
 * DPM for Freescale MXC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Copyright (C) 2002-2004, MontaVista Software <source@mvista.com>
 *
 * Initially based on include/asm-arm/arch-omap/omap_dpm.h
 */

#ifndef __ASM_ARCH_MXC_DPM_H__
#define __ASM_ARCH_MXC_DPM_H__

#include <asm/io.h>
#include <asm/timex.h>

/*!
 * @file arch-mxc/dpm.h
 *
 * @brief This file provides DPM support hooks for the Freescale MXC
 *
 * @ingroup DPM_MX27 DPM_MX31
 */

/*!
 * machine dependent operating state
 *
 * An operating state is a cpu execution state that has implications for power
 * management. The DPM will select operating points based largely on the
 * current operating state.
 *
 * DPM_STATES is the number of supported operating states. Valid operating
 * states are from 0 to DPM_STATES-1 but when setting an operating state the
 * kernel should only specify a state from the set of "base states" and should
 * do so by name.  During the context switch the new operating state is simply
 * extracted from current->dpm_state.
 *
 * task states:
 *
 * APIs that reference task states use the range -(DPM_TASK_STATE_LIMIT + 1)
 * through +DPM_TASK_STATE_LIMIT.  This value is added to DPM_TASK_STATE to
 * obtain the downward or upward adjusted task state value. The
 * -(DPM_TASK_STATE_LIMIT + 1) value is interpreted specially, and equates to
 * DPM_NO_STATE.
 *
 * Tasks inherit their task operating states across calls to
 * fork(). DPM_TASK_STATE is the default operating state for all tasks, and is
 * inherited from init.  Tasks can change (or have changed) their tasks states
 * using the DPM_SET_TASK_STATE variant of the sys_dpm() system call.
 */
#define DPM_IDLE_TASK_STATE  0
#define DPM_IDLE_STATE       1
#define DPM_SLEEP_STATE      2
#define DPM_BASE_STATES      3

#define DPM_TASK_STATE_LIMIT 4
#define DPM_TASK_STATE       (DPM_BASE_STATES + DPM_TASK_STATE_LIMIT)
#define DPM_STATES           (DPM_TASK_STATE + DPM_TASK_STATE_LIMIT + 1)
#define DPM_TASK_STATES      (DPM_STATES - DPM_BASE_STATES)

#define DPM_STATE_NAMES                  \
{ "idle-task", "idle", "sleep",          \
  "task-4", "task-3", "task-2", "task-1",\
  "task",                                \
  "task+1", "task+2", "task+3", "task+4" \
}

#define DPM_PARAM_NAMES				\
{ "core", "ahb",     \
  "ip", "mode" }

/*!
 * MD operating point parameters
 */
#define DPM_CORE_FREQ         0
#define DPM_AHB_FREQ          1
#define DPM_IP_FREQ           2
#define DPM_MODE              3

#define DPM_PP_NBR 4

#ifndef __ASSEMBLER__

#include <linux/types.h>
#include <linux/proc_fs.h>

#define dpm_time()	get_cycles()

extern unsigned long clock_to_usecs(unsigned long);
#define dpm_time_to_usec(ticks) (clock_to_usecs(ticks))

/*!
 * The register values only include the bits manipulated by the DPM
 * system - other bits that also happen to reside in these registers are
 * not represented here.
 */
struct dpm_regs {
};

/*!
 * Instances of this structure define valid MXC operating points for DPM.
 * Voltages are represented in mV, and frequencies are represented in KHz.
 */
struct dpm_md_opt {
	unsigned int cpu;	/* in KHz */
	unsigned int ahb;	/* in KHz */
	unsigned int ip;	/* in KHz */
	unsigned int mode;
	struct dpm_regs regs;	/* Register values */
};

#ifdef CONFIG_MACH_MX27ADS
#define ARM_MAX 400000000
#define ARM_MIN 33250000
#define AHB_MAX 133000000
#define AHB_MIN 33250000
#define IPG_MAX 66500000
#define IPG_MIN 16625000
#else
#define ARM_MAX 532000000
#define ARM_MIN 133000000
#define AHB_MAX AHB_FREQ
#define AHB_MIN AHB_FREQ
#define IPG_MAX IPG_FREQ
#define IPG_MIN IPG_FREQ
#endif

#define DPM_MODE_SLEEP 0
#define DPM_MODE_RUN 1
#define DPM_MODE_WAIT 2
#define DPM_MODE_STOP 3
#endif				/* __ASSEMBLER__ */
#endif				/* __ASM_ARCH_MXC_DPM_H__ */
