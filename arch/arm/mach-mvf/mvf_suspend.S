/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/linkage.h>
#include <mach/hardware.h>
#include <asm/memory.h>
#include <mach/mvf.h>
#include "regs-src.h"
#include "crm_regs.h"

#define PERIPBASE_VIRT_OFFSET	0xb2000000
#define TTRBIT_MASK 		0xffffc000
#define TABLE_INDEX_MASK 	0xfff00000
#define TABLE_ENTRY 		0x00000c02
#define CACHE_DISABLE_MASK 	0xfffffffb
#define IRAM_SUSPEND_SIZE 	(1 << 15)

/*************************************************************
mvf_suspend:

Suspend the processor (eg, wait for interrupt).

r1: iram_paddr
r2: suspend_iram_base
*************************************************************/

	.macro	mvf_stop_mode_enter

	ldr	r3, =MVF_ANATOP_BASE_ADDR
	add	r3, r3, #PERIPBASE_VIRT_OFFSET
	add	r3, r3, #0x20000

	/* pll7 disable */
	ldr	r4, [r3, #0x20]
	bic	r4, r4, #0x2000
	str	r4, [r3, #0x20]

	/* pll3 disable */
	ldr	r4, [r3, #0x10]
	bic	r4, r4, #0x2000
	str	r4, [r3, #0x10]

	/* pll4 disable */
	ldr	r4, [r3, #0x70]
	bic	r4, r4, #0x2000
	str	r4, [r3, #0x70]

	/* pll6 disable */
	ldr	r4, [r3, #0xa0]
	bic	r4, r4, #0x2000
	str	r4, [r3, #0xa0]

	/* pll5 disable */
	ldr	r4, [r3, #0xe0]
	bic	r4, r4, #0x2000
	str	r4, [r3, #0xe0]

	/* pll1 disable */
	ldr	r4, [r3, #0x270]
	bic	r4, r4, #0x2000
	str	r4, [r3, #0x270]

	/* stop mode is masked to Anatop */
	ldr	r3, =MVF_CCM_BASE_ADDR
	add	r3, r3, #PERIPBASE_VIRT_OFFSET
	add	r3, r3, #0x20000

	ldr	r4, [r3, #0x2c]
	bic	r4, r4, #0x100
	str	r4, [r3, #0x2c]

	ldr	r3, =MVF_GPC_BASE_ADDR
	add	r3, r3, #PERIPBASE_VIRT_OFFSET
	add	r3, r3, #0x20000

	/* ensure power domain 0 */
	ldr	r4, [r3, #0x0]
	bic	r4, r4, #0x01
	str	r4, [r3, #0x0]

	/* enable deep sleep for memories */
	ldr	r4, [r3, #0x0]
	orr	r4, r4, #0x80
	str	r4, [r3, #0x0]

	/* disable well bias */
	ldr	r4, [r3, #0x0]
	bic	r4, r4, #0x10
	str	r4, [r3, #0x0]

	/* turn off HPREG in stop mode */
	ldr	r4, [r3, #0x0]
	orr	r4, r4, #0x08
	str	r4, [r3, #0x0]

	/* gpc_lpmr set stop mode */
	ldr	r4, =0x02
	str	r4, [r3, #0x40]

	.endm

	.macro	mvf_lpstop_mode_enter

	ldr	r3, =MVF_ANATOP_BASE_ADDR
	add	r3, r3, #PERIPBASE_VIRT_OFFSET
	add	r3, r3, #0x20000

	/* pll7 disable */
	ldr	r4, [r3, #0x20]
	bic	r4, r4, #0x2000
	str	r4, [r3, #0x20]

	/* pll3 disable */
	ldr	r4, [r3, #0x10]
	bic	r4, r4, #0x2000
	str	r4, [r3, #0x10]

	/* pll4 disable */
	ldr	r4, [r3, #0x70]
	bic	r4, r4, #0x2000
	str	r4, [r3, #0x70]

	/* pll6 disable */
	ldr	r4, [r3, #0xa0]
	bic	r4, r4, #0x2000
	str	r4, [r3, #0xa0]

	/* pll5 disable */
	ldr	r4, [r3, #0xe0]
	bic	r4, r4, #0x2000
	str	r4, [r3, #0xe0]

	/* pll1 disable */
	ldr	r4, [r3, #0x270]
	bic	r4, r4, #0x2000
	str	r4, [r3, #0x270]

	ldr	r3, =MVF_CCM_BASE_ADDR
	add	r3, r3, #PERIPBASE_VIRT_OFFSET
	add	r3, r3, #0x20000

	ldr	r4, [r3, #0x2c]
	bic	r4, r4, #0x100
	str	r4, [r3, #0x2c]

	ldr	r3, =MVF_GPC_BASE_ADDR
	add	r3, r3, #PERIPBASE_VIRT_OFFSET
	add	r3, r3, #0x20000

	/* enable deep sleep for memories */
	ldr	r4, [r3, #0x0]
	orr	r4, r4, #0x40
	str	r4, [r3, #0x0]

	/* enable LPSTOP3 */
	ldr	r4, [r3, #0x0]
	bic	r4, r4, #0x04
	bic	r4, r4, #0x02
	str	r4, [r3, #0x0]

	/* ensure power domain 1 */
	ldr	r4, [r3, #0x0]
	orr	r4, r4, #0x01
	str	r4, [r3, #0x0]

	/* gpc_lpmr set low-power stop mode */
	ldr	r4, =0x02
	str	r4, [r3, #0x40]

	.endm

/******************************************************************
Invalidate l1 dcache, r0-r4, r6, r7 used
******************************************************************/
	.macro	invalidate_l1_dcache

	mov 	r0, #0
	mcr 	p15, 2, r0, c0, c0, 0
	mrc 	p15, 1, r0, c0, c0, 0

	ldr 	r1, =0x7fff
	and 	r2, r1, r0, lsr #13

	ldr 	r1, =0x3ff

	and 	r3, r1, r0, lsr #3	@ NumWays - 1
	add 	r2, r2, #1		@ NumSets

	and 	r0, r0, #0x7
	add 	r0, r0, #4		@ SetShift

	clz 	r1, r3			@ WayShift
	add 	r4, r3, #1		@ NumWays
1:
	sub 	r2, r2, #1		@ NumSets--
	mov 	r3, r4			@ Temp = NumWays
2:
	subs	r3, r3, #1		@ Temp--
	mov 	r7, r3, lsl r1
	mov 	r6, r2, lsl r0
	orr 	r7, r7, r6
	mcr 	p15, 0, r7, c7, c6, 2
	bgt 	2b
	cmp 	r2, #0
	bgt 	1b
	dsb
	isb

	.endm

/******************************************************************
Flush and disable L1 dcache
******************************************************************/
	.macro	flush_disable_l1_dcache

	/*
	 * Flush all data from the L1 data cache before disabling
	 * SCTLR.C bit.
	 */
	push	{r0-r12, lr}
	ldr r0, =v7_flush_dcache_all
	mov lr, pc
	mov pc, r0
	pop {r0-r12, lr}

	/*
	 * Clear the SCTLR.C bit to prevent further data cache
	 * allocation. Clearing SCTLR.C would make all the data accesses
	 * strongly ordered and would not hit the cache.
	 */
	mrc p15, 0, r0, c1, c0, 0
	bic r0, r0, #(1 << 2)		@ Disable the C bit
	mcr p15, 0, r0, c1, c0, 0
	isb

	/*
	 * Invalidate L1 data cache. Even though only invalidate is
	 * necessary exported flush API is used here. Doing clean
	 * on already clean cache would be almost NOP.
	 */
	push	{r0-r12, lr}
	ldr r0, =v7_flush_dcache_all
	mov lr, pc
	mov pc, r0
	pop {r0-r12, lr}

	/*
	 * Execute an ISB instruction to ensure that all of the
	 * CP15 register changes have been committed.
	 */
	isb

	/*
	 * Execute a barrier instruction to ensure that all cache,
	 * TLB and branch predictor maintenance operations issued
	 * by any CPU in the cluster have completed.
	 */
	dsb
	dmb

	.endm

ENTRY(mvf_suspend)
	stmfd	sp!, {r0-r12}     @ Save registers
/*************************************************************
suspend mode entry
*************************************************************/
	mov	r11, r0

	cmp	r0, #0x1
	bne	dormant

	/* Need to flush and disable L1 dcache*/
	flush_disable_l1_dcache

	wfi

	nop
	nop
	nop
	nop

	/* Invalidate L1 I-cache first */
	mov r1, #0x0
	mcr p15, 0, r1, c7, c5, 0 @ Invalidate I-Cache

	/* Need to invalidate L1 dcache, as the power is dropped */
	invalidate_l1_dcache

	/* Enable L1 dcache first */
	mrc p15, 0, r0, c1, c0, 0
	orr r0, r0, #(1 << 2)	@ Disable the C bit
	mcr p15, 0, r0, c1, c0, 0

/***********************************************************
never run to here
************************************************************/
	b 	out	/* exit standby */
	/* Place the literal pool here so that literals are
	within 16KB range */
	.ltorg

/************************************************************
dormant entry, data save in stack
************************************************************/
dormant:

/************************************************************
saved register and context as below:
	sp
	spsr
	lr
	CPACR
	TTBR0
	TTBR1
	TTBCR
	DACR
	PRRR
	NMRR
	ACTLR
	Context ID
	User r/w thread ID
	Secure or NS VBAR
	CPSR
	SCTLR
************************************************************/
	/* stack is from the tail of iram_suspend base */
	mov	r0, r2 		/* get suspend_iram_base */
	add	r0, r0, #IRAM_SUSPEND_SIZE

	mov	r4, r11			@ Store state entered
	stmfd	r0!, {r4}

	mov	r4, sp			@ Store sp
	mrs	r5, spsr		@ Store spsr
	mov	r6, lr			@ Store lr
	stmfd	r0!, {r4-r6}

	/* c1 and c2 registers */
	mrc	p15, 0, r4, c1, c0, 2	@ CPACR
	mrc	p15, 0, r5, c2, c0, 0	@ TTBR0
	mrc	p15, 0, r6, c2, c0, 1	@ TTBR1
	mrc	p15, 0, r7, c2, c0, 2	@ TTBCR
	stmfd	r0!, {r4-r7}

	/* c3 and c10 registers */
	mrc	p15, 0, r4, c3, c0, 0	@ DACR
	mrc	p15, 0, r5, c10, c2, 0	@ PRRR
	mrc	p15, 0, r6, c10, c2, 1	@ NMRR
	mrc	p15, 0, r7, c1, c0, 1	@ ACTLR
	stmfd	r0!,{r4-r7}

	/* c12, c13 and CPSR registers */
	mrc	p15, 0, r4, c13, c0, 1	@ Context ID
	mrc	p15, 0, r5, c13, c0, 2	@ User r/w thread ID
	mrc	p15, 0, r6, c12, c0, 0	@ Secure or NS VBAR
	mrs	r7, cpsr		@ Store CPSR
	stmfd	r0!, {r4-r7}

	/* c1 control register */
	mrc	p15, 0, r4, c1, c0, 0	@ SCTLR
	stmfd	r0!, {r4}

	/* Need to flush and disable L1 dcache*/
	flush_disable_l1_dcache

	/* Make sure TLBs are primed */
	ldr	r1,  =MVF_IOMUXC_BASE_ADDR
	add	r1, r1, #PERIPBASE_VIRT_OFFSET
	add	r1, r1, #0x20000
	ldr	r0, [r1]
	ldr	r1, =MVF_SRC_BASE_ADDR
	add	r1, r1, #PERIPBASE_VIRT_OFFSET
	add	r1, r1, #0x20000
	ldr	r0, [r1]
	ldr	r1, =MVF_CCM_BASE_ADDR
	add	r1, r1, #PERIPBASE_VIRT_OFFSET
	add	r1, r1, #0x20000
	ldr	r0, [r1]
	ldr	r1, =MVF_GPC_BASE_ADDR
	add	r1, r1, #PERIPBASE_VIRT_OFFSET
	add	r1, r1, #0x20000
	ldr	r0, [r1]
	ldr	r1, =MVF_CCM_BASE_ADDR
	add	r1, r1, #PERIPBASE_VIRT_OFFSET
	add	r1, r1, #0x20000
	ldr	r0, [r1]

	/* Do a DSB to drain the buffers */
	dsb

	mvf_stop_mode_enter

/****************************************************************
execute a wfi instruction to let SOC go into stop mode.
****************************************************************/
	wfi

	nop
	nop
	nop
	nop

/****************************************************************
if go here, means there is a wakeup irq pending, we should resume
system immediately.
****************************************************************/
	mov	r0, r2 		/* get suspend_iram_base */
	add	r0, r0, #IRAM_SUSPEND_SIZE

	ldmea	r0!, {r11}	@ standby or mem

	/* mask all the GPC interrupts */
	ldr	r3, =MVF_GPC_BASE_ADDR
	add	r3, r3, #PERIPBASE_VIRT_OFFSET
	add	r3, r3, #0x20000
	ldr	r4, =0xffffffff
	str	r4, [r3, #0x44]
	str	r4, [r3, #0x48]
	str	r4, [r3, #0x4c]
	str	r4, [r3, #0x50]

	/* pll2 enable */
	ldr	r3, =MVF_ANATOP_BASE_ADDR
	add	r3, r3, #PERIPBASE_VIRT_OFFSET
	add	r3, r3, #0x20000

	ldr	r4, [r3, #0x30]
	orr	r4, r4, #0x2000
	str	r4, [r3, #0x30]

	nop
	nop
	nop
	nop
	nop

	nop
	nop
	nop
	nop
	nop

	nop
	nop
	nop
	nop
	nop

	nop
	nop
	nop
	nop
	nop

	nop
	nop
	nop
	nop
	nop

	mrc	p15, 0, r1, c1, c0, 0
	orr	r1, r1, #(1 << 2)	@ Enable the C bit
	mcr	p15, 0, r1, c1, c0, 0

	b 	out

/************************************************
return back to mvf_suspend_enter for suspend
*************************************************/
out:
	ldmfd	sp!, {r0-r12}
	mov	pc, lr

	.type	mvf_do_suspend, #object
ENTRY(mvf_do_suspend)
	.word	mvf_suspend
	.size	mvf_suspend, . - mvf_suspend
