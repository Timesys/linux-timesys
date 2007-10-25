/*
 *  Copyright 2004-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <asm/hardware.h>

	@ this macro disables fast irq (not implemented)
	.macro	disable_fiq
	.endm

        .macro  get_irqnr_preamble, base, tmp
        .endm

        .macro  arch_ret_to_user, tmp1, tmp2
        .endm

	@ this macro checks which interrupt occured
	@ and returns its number in irqnr
	@ and returns if an interrupt occured in irqstat
	.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
	@ Can't use IO_ADDRESS macro. Otherwise compilation error
#ifdef CONFIG_MXC_TZIC
	ldr	\irqstat, =TZIC_IO_ADDRESS(TZIC_BASE_ADDR)
	@ Load offset & priority of the highest priority
	@ interrupt pending.
        @ 0xD80 is HIPND0 register
        ldr     \irqnr, =0
        ldr     \base, =0x0D80
1000:
        ldr     \tmp,   [\irqstat, \base]
        cmp     \tmp, #0
        bne     1001f
        addeq   \irqnr, \irqnr, #32
        addeq   \base, \base, #4
        cmp     \irqnr, #128
        blo     1000b
        b       2001f
1001:   ldr     \base, =1
1002:   tst     \tmp, \base
        bne     2002f
        movs    \base, \base, lsl #1
        addne   \irqnr, \irqnr, #1
        bne     1002b
2001:
        ldr  \irqnr, =0
2002:
        movs \irqnr, \irqnr
#else
	ldr	\irqstat, =AVIC_IO_ADDRESS(AVIC_BASE_ADDR)
	@ Load offset & priority of the highest priority
	@ interrupt pending.
	ldr	\irqnr, [\irqstat, #0x40]	@ this is AVIC_NIVECSR
	@ Shift off the priority leaving the offset or
	@ "interrupt number"
	mov	\irqnr, \irqnr, lsr #16
	ldr	\irqstat, =1	@ dummy compare
	ldr	\base, =0xFFFF	// invalid interrupt
	cmp	\irqnr, \base
	bne	1001f
	ldr	\irqstat, =0
	1001:
	tst	\irqstat, #1	@ to make the condition code = TRUE
#endif  /*MXC_SUPPORT_TZIC*/
	.endm

	@ irq priority table (not used)
	.macro	irq_prio_table
	.endm
