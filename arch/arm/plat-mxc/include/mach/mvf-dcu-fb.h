/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 *  Freescale DCU Frame Buffer device driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#ifndef __MVF_DCU_FB_H__
#define __MVF_DCU_FB_H__

#include <linux/types.h>
#include <linux/mvf-fb.h>

struct mvf_dcu_platform_data {
	char *mode_str;
	int default_bpp;
	int (*init) (int);
};

struct dfb_chroma_key {
	int enable;
	__u8  red_max;
	__u8  green_max;
	__u8  blue_max;
	__u8  red_min;
	__u8  green_min;
	__u8  blue_min;
};

#define DCU_LCD_ENABLE_PIN	25

#define MFB_SET_CHROMA_KEY	_IOW('M', 1, struct mfb_chroma_key)
#define MFB_SET_BRIGHTNESS	_IOW('M', 3, __u8)

#ifdef __KERNEL__
#include <linux/spinlock.h>

/* DCU registers */
#define DCU_CTRLDESCCURSOR1		0x0000
#define DCU_CTRLDESCCURSOR1_HEIGHT(x)	(x << 16)
#define DCU_CTRLDESCCURSOR1_WIDTH(x)	(x)

#define DCU_CTRLDESCCURSOR2		0x0004
#define DCU_CTRLDESCCURSOR2_POSY(x)	(x << 16)
#define DCU_CTRLDESCCURSOR2_POSX(x)	(x)

#define DCU_CTRLDESCCURSOR3		0x0008
#define DCU_CTRLDESCCURSOR3_CUR_EN(x)	(x << 31)

#define DCU_CTRLDESCCURSOR4		0x000c

#define DCU_DCU_MODE			0x0010
#define DCU_MODE_BLEND_ITER(x)		(x << 20)
#define DCU_MODE_RASTER_EN(x)		(x << 14)
#define DCU_MODE_DCU_MODE(x)		(x)
#define DCU_MODE_DCU_MODE_MASK		0x03

#define DCU_BGND			0x0014
#define DCU_BGND_R(x)			(x << 16)
#define DCU_BGND_G(x)			(x << 8)
#define DCU_BGND_B(x)			(x)

#define DCU_DISP_SIZE			0x0018
#define DCU_DISP_SIZE_DELTA_Y(x)	(x << 16)
#define DCU_DISP_SIZE_DELTA_X(x)	(x)

#define DCU_HSYN_PARA			0x001c
#define DCU_HSYN_PARA_BP(x)		(x << 22)
#define DCU_HSYN_PARA_PW(x)		(x << 11)
#define DCU_HSYN_PARA_FP(x)		(x)

#define DCU_VSYN_PARA			0x0020
#define DCU_VSYN_PARA_BP(x)		(x << 22)
#define DCU_VSYN_PARA_PW(x)		(x << 11)
#define DCU_VSYN_PARA_FP(x)		(x)

#define DCU_SYN_POL			0x0024
#define DCU_SYN_POL_INV_PXCK(x)		(x << 6)
#define DCU_SYN_POL_NEG(x)		(x << 5)
#define DCU_SYN_POL_INV_VS(x)		(x << 1)
#define DCU_SYN_POL_INV_HS(x)		(x)

#define DCU_THRESHOLD			0x0028
#define DCU_THRESHOLD_LS_BF_VS(x)	(x << 16)
#define DCU_THRESHOLD_OUT_BUF_HIGH(x)	(x << 8)
#define DCU_THRESHOLD_OUT_BUF_LOW(x)	(x)

#define DCU_INT_STATUS			0x002C
#define DCU_INT_STATUS_UNDRUN		(1 << 2)
#define DCU_INT_STATUS_LYR_TRANS_FINISH	(1 << 12)

#define DCU_INT_MASK			0x0030
#define DCU_INT_MASK_ALL		0xffffffff

#define DCU_COLBAR_1			0x0034
#define DCU_COLBAR_2			0x0038
#define DCU_COLBAR_3			0x003c
#define DCU_COLBAR_4			0x0040
#define DCU_COLBAR_5			0x0044
#define DCU_COLBAR_6			0x0048
#define DCU_COLBAR_7			0x004c
#define DCU_COLBAR_8			0x0050
#define DCU_DIV_RATIO			0x0054
#define DCU_SIGN_CALC_1			0x0058
#define DCU_SIGN_CALC_2			0x005c
#define DCU_CRC_VAL			0x0060

#define DCU_PARR_ERR_STA_1		0x006c

#define DCU_PARR_ERR_STA_2		0x0070

#define DCU_PARR_ERR_STA_3		0x007c

#define DCU_MSK_PARR_ERR_STA_1		0x0080
#define DCU_MSK_PARR_ERR_ALL		0xffffffff

#define DCU_MSK_PARR_ERR_STA_2		0x0084

#define DCU_MSK_PARR_ERR_STA_3		0x0090

#define DCU_THRSHLD_INP_1		0x0094
#define DCU_THRSHLD_INP_2		0x0098
#define DCU_THRSHLD_INP_3		0x009c
#define DCU_LUMA_COMP			0x00a0
#define DCU_RED_CHRM_COMP		0x00a4
#define DCU_GRN_CHRM_COMP		0x00a8
#define DCU_BLUE_CHRM_COMP		0x00ac
#define DCU_CRC_POS			0x00b0
#define DCU_LYR_INTPOL_EN		0x00b4
#define DCU_LYR_LUMA_COMP		0x00b8
#define DCU_LYR_CHRM_RED		0x00bc
#define DCU_LYR_CHRM_GRN		0x00c0
#define DCU_LYR_CHRM_BLUE		0x00c4
#define DCU_COMP_IMSIZE			0x00c8

#define DCU_UPDATE_MODE			0x00cc
#define DCU_UPDATE_MODE_MODE(x)		(x << 31)
#define DCU_UPDATE_MODE_READREG(x)	(x << 30)

#define DCU_UNDERRUN			0x00d0

#define DCU_CTRLDESCLN_0(x)		(0x200 + (x) * 0x40)
#define DCU_CTRLDESCLN_0_HEIGHT(x)	(x << 16)
#define DCU_CTRLDESCLN_0_WIDTH(x)	(x)

#define DCU_CTRLDESCLN_1(x)		(0x204 + (x) * 0x40)
#define DCU_CTRLDESCLN_1_POSY(x)	(x << 16)
#define DCU_CTRLDESCLN_1_POSX(x)	(x)

#define DCU_CTRLDESCLN_2(x)		(0x208 + (x) * 0x40)

#define DCU_CTRLDESCLN_3(x)		(0x20c + (x) * 0x40)
#define DCU_CTRLDESCLN_3_EN(x)		(x << 31)
#define DCU_CTRLDESCLN_3_TILE_EN(x)	(x << 30)
#define DCU_CTRLDESCLN_3_DATA_SEL(x)	(x << 29)
#define DCU_CTRLDESCLN_3_SAFETY_EN(x)	(x << 28)
#define DCU_CTRLDESCLN_3_TRANS(x)	(x << 20)
#define DCU_CTRLDESCLN_3_BPP(x)		(x << 16)
#define DCU_CTRLDESCLN_3_RLE_EN(x)	(x << 15)
#define DCU_CTRLDESCLN_3_LUOFFS(x)	(x << 4)
#define DCU_CTRLDESCLN_3_BB(x)		(x << 2)
#define DCU_CTRLDESCLN_3_AB(x)		(x)

#define DCU_CTRLDESCLN_4(x)		(0x210 + (x) * 0x40)
#define DCU_CTRLDESCLN_4_CKMAX_R(x)	(x << 16)
#define DCU_CTRLDESCLN_4_CKMAX_G(x)	(x << 8)
#define DCU_CTRLDESCLN_4_CKMAX_B(x)	(x)

#define DCU_CTRLDESCLN_5(x)		(0x214 + (x) * 0x40)
#define DCU_CTRLDESCLN_5_CKMIN_R(x)	(x << 16)
#define DCU_CTRLDESCLN_5_CKMIN_G(x)	(x << 8)
#define DCU_CTRLDESCLN_5_CKMIN_B(x)	(x)

#define DCU_CTRLDESCLN_6(x)		(0x218 + (x) * 0x40)
#define DCU_CTRLDESCLN_6_TILE_VER(x)	(x << 16)
#define DCU_CTRLDESCLN_6_TILE_HOR(x)	(x)

#define DCU_CTRLDESCLN_7(x)		(0x21c + (x) * 0x40)
#define DCU_CTRLDESCLN_7_FG_FCOLOR(x)	(x)

#define DCU_CTRLDESCLN_8(x)		(0x220 + (x) * 0x40)
#define DCU_CTRLDESCLN_8_BG_BCOLOR(x)	(x)


struct dcu_addr {
	__u8 __iomem *vaddr;	/* Virtual address */
	dma_addr_t paddr;	/* Physical address */
	__u32	offset;
};

struct dcu_pool {
	struct dcu_addr ad;
	struct dcu_addr pallete;
	struct dcu_addr gamma;
	struct dcu_addr cursor;
};

#define DCU_LAYER_NUM_MAX		6
#define DCU_LAYER_NUM			5

/* Minimum X and Y resolutions */
#define MIN_XRES			64
#define MIN_YRES			64

/* HW cursor parameters */
#define MAX_CURS			32

#define PALLETE_TABLE_SIZE		8192
#define GAMMA_TABLE_SIZE		(1024 * 3)

/* Panels'operation modes */
#define DCU_TYPE_OUTPUT		0	/* Panel output to display */
#define DCU_TYPE_OFF		1	/* Panel off */
#define DCU_TYPE_WB		2	/* Panel written back to memory */
#define DCU_TYPE_TEST		3	/* Panel generate color bar */

#define BPP_1			0
#define BPP_2			1
#define BPP_4			2
#define BPP_8			3
#define BPP_16_RGB565		4
#define BPP_24			5
#define BPP_32_ARGB8888		6
#define BPP_TRANS_4		7
#define BPP_TRANS_8		8
#define BPP_LUM_4		9
#define BPP_LUM_8		0x0a
#define BPP_16_ARGB1555		0x0b
#define BPP_16_ARGB4444		0x0c
#define BPP_16_APAL8		0x0d
#define BPP_YUV422		0x0e

#endif /* __KERNEL__ */
#endif /* __MVF_DCU_FB_H__ */
