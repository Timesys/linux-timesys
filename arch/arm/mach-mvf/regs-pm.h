/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef __ARCH_ARM_MACH_MVF_REGS_PM_H__
#define __ARCH_ARM_MACH_MVF_REGS_PM_H__

/* GPC registers */
#define GPC_PGCR_OFFSET			(0x00)
#define GPC_PGCR_DS_STOP		(0x000000080)
#define GPC_PGCR_DS_LPSTOP		(0x000000040)
#define GPC_PGCR_WB_STOP		(0x000000010)
#define GPC_PGCR_HP_OFF			(0x000000008)
#define GPC_PGCR_PG_48K			(0x000000004)
#define GPC_PGCR_PG_16K			(0x000000002)
#define GPC_PGCR_PG_PD1			(0x000000001)

#define GPC_PGSR_OFFSET			(0x0c)

#define GPC_LPMR_OFFSET			(0x40)
#define GPC_LPMR_CLPCR_RUN		(0x00000000)
#define GPC_LPMR_CLPCR_WAIT		(0x00000001)
#define GPC_LPMR_CLPCR_STOP		(0x00000002)

#define GPC_IMR1_OFFSET			(0x44)
#define GPC_IMR2_OFFSET			(0x48)
#define GPC_IMR3_OFFSET			(0x4c)
#define GPC_IMR4_OFFSET			(0x50)
#define GPC_ISR1_OFFSET			(0x54)
#define GPC_ISR2_OFFSET			(0x58)
#define GPC_ISR3_OFFSET			(0x5c)
#define GPC_ISR4_OFFSET			(0x60)

/* VREG registers */
#define VREG_CTRL_OFFSET		(0x00)
#define VREG_CTRL_PORPU			(0x00010000)
#define VREG_CTRL_HVDMASK		(0x00000001)
#define VREG_STAT_OFFSET		(0x04)

/* WKPU registers */
#define WKPU_NSR_OFFSET			(0x00)

#define WKPU_NCR_OFFSET			(0x08)
#define WKPU_NCR_NLOCK0			(0x80000000)
#define WKPU_NCR_NWRE0			(0x10000000)
#define WKPU_NCR_NREE0			(0x04000000)
#define WKPU_NCR_NFEE0			(0x02000000)
#define WKPU_NCR_NFE0			(0x01000000)

#define WKPU_WISR_OFFSET		(0x14)

#define WKPU_IRER_OFFSET		(0x18)

#define WKPU_WRER_OFFSET		(0x1c)

#define WKPU_WIREER_OFFSET		(0x28)

#define WKPU_WIFEER_OFFSET		(0x2c)

#define WKPU_WIFER_OFFSET		(0x30)

#define WKPU_WIPUER_OFFSET		(0x34)

#define RISING_EDGE_ENABLED		(0x9e)
#define FALLING_EDGE_ENABLED		(0xfe)

/* SCU registers */
#define SCU_CTRL_OFFSET			(0x00)


#endif				/* __ARCH_ARM_MACH_MVF_REGS_PM_H__ */
