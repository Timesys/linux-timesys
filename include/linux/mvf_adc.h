/* Copyright 2012 Freescale Semiconductor, Inc.
 *
 * Freescale Faraday Quad ADC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
*/

#ifndef	MVF_ADC_H
#define MVF_ADC_H

#define ADC_CFG			0x14     /* Configuration Register */

/* Input Clock Select  1-0, Divide ratio 6-5 */
#define CLEAR_CLK_BIT		0x63
#define BUSCLK_SEL		0x00
#define BUSCLK2_SEL		0x01
#define ALTCLK_SEL		0x02
#define ADACK_SEL		0x03
#define CLK_DIV2		0x20
#define CLK_DIV4		0x40
#define CLK_DIV8		0x60
/* Conversion RES Mode Selection  3-2 */
#define CLEAR_MODE_BIT		0xC
#define BIT8			0x00
#define BIT10			0x01
#define BIT12			0x10
/* Low-Power Configuration 7 */
#define CLEAR_ADLPC_BIT		0x80
#define ADLPC_EN		0x80
/* Long/Short Sample Time Configuration 4,9-8 */
#define CLEAR_LSTC_BIT		0x310
#define ADLSMP_LONG		0x10
#define ADSTS_SHORT		0x100
#define ADSTS_NORMAL		0x200
#define ADSTS_LONG		0x300
/* High speed operation 10 */
#define CLEAR_ADHSC_BIT		0x400
#define ADHSC_EN		0x400
/* Voltage Reference Selection 12-11 */
#define CLEAR_REFSEL_BIT	0x1800
#define REFSEL_VALT		0x100
#define REFSEL_VBG		0x1000
/* Conversion Trigger Select  13 */
#define CLEAR_ADTRG_BIT		0x2000
#define ADTRG_HARD		0x2000
/* Hardware Average select 15-14 */
#define CLEAR_AVGS_BIT		0xC000
#define AVGS_8			0x4000
#define AVGS_16			0x8000
#define AVGS_32			0xC000
/* Data Overwrite Enable 16 */
#define CLEAR_OVWREN_BIT	0x10000
#define OVWREN			0x10000

#define ADC_GC			0x18	/* General control register */

/* Asynchronous clock output 0 */
#define CLEAR_ADACKEN_BIT	0x1
#define ADACKEN			0x1
/* DMA Enable 1 */
#define CLEAR_DMAEN_BIT		0x2
#define DMAEN		        0x2
/* Compare Function Rang Enable 2*/
#define CLEAR_ACREN_BIT		0x4
#define ACREN			0x4
/* Compare Function Greater Than Enable 3 */
#define CLEAR_ACFGT_BIT		0x8
#define ACFGT			0x8
/* Enable compare function 4 */
#define CLEAR_ACFE_BIT		0x10
#define ACFE			0x10
/* Hardware Average Enable 5 */
#define CLEAR_AVGE_BIT		0x20
#define AVGEN			0x20
/* Continue Conversion Enable 6 */
#define CLEAR_ADCO_BIT		0x40
#define ADCON		        0x40


#define ADC_HC0		0x00/* Control register for hardware triggers 0 */
#define ADC_HC1		0x04/* Control register for hardware triggers 1 */

#define IRQ_EN		0x80
#define ADCHC0(x)	((x)&0xF)
#define AIEN1		0x00000080
#define COCOA		0x00000000

#define ADC_HS		0x08/* Status register for HW triggers */

#define ADC_R0			0x0c/* Data result register for HW triggers  */
#define ADC_R1			0x10/* Data result register for HW triggers  */

#define ADC_GS			0x1c/* General status register (ADC0_GS)  */
#define ADC_CV			0x20/* Compare value register (ADC0_CV)  */
/* Offset correction value register (ADC0_OFS) */
#define ADC_OFS			0x24
#define ADC_CAL			0x28/* Calibration value register (ADC0_CAL)  */
#define ADC_PCTL		0x30/* Pin control register (ADC0_PCTL)  */

/* init */
#define CLECFG16		0x1ffff
#define SETCFG			0x10098

#define CLEHC0			0xff
#define SETHC0			0x85

#define CLEPCTL23		0xffffff
#define SETPCTL5		0x20

#define ADC_INIT		_IO('p', 0xb1)
#define ADC_CONVERT		_IO('p', 0xb2)
#define	ADC_CONFIGURATION	_IOWR('p', 0xb3, struct adc_feature)
#define ADC_REG_CLIENT		_IOWR('p', 0xb4, struct adc_feature)

enum clk_sel {
	ADCIOC_BUSCLK_SET,
	ADCIOC_ALTCLK_SET,
	ADCIOC_ADACK_SET,
};


#define ADCIOC_LPOFF_SET	0
#define ADCIOC_LPON_SET		1

#define ADCIOC_HSON_SET         1
#define ADCIOC_HSOFF_SET        0

enum vol_ref {
	ADCIOC_VR_VREF_SET,
	ADCIOC_VR_VALT_SET,
	ADCIOC_VR_VBG_SET,
};

#define ADCIOC_SOFTTS_SET       0
#define ADCIOC_HARDTS_SET	1

#define ADCIOC_HA_DIS		0
#define ADCIOC_HA_SET		1

#define ADCIOC_DOEON_SET	1
#define ADCIOC_DOEOFF_SET	0

#define ADCIOC_ADACKENON_SET	1
#define ADCIOC_ADACKENOFF_SET	0

#define ADCIDC_DMAON_SET	1
#define ADCIDC_DMAOFF_SET	0

#define ADCIOC_CCEOFF_SET	0
#define ADCIOC_CCEON_SET	1

#define ADCIOC_ACFEON_SET	1
#define ADCIOC_ACFEOFF_SET	0

#define ADCIOC_ACFGTON_SET	1
#define ADCIOC_ACFGTOFF_SET	0

#define ADCIOC_ACRENON_SET	1
#define ADCIOC_ACRENOFF_SET	0

enum adc_channel {
	ADC0,
	ADC1,
	ADC2,
	ADC3,
	ADC4,
	ADC5,
	ADC6,
	ADC7,
	ADC8,
	ADC9,
	ADC10,
	ADC11,
	ADC12,
	ADC13,
	ADC14,
	ADC15,
};

struct adc_feature {
	enum adc_channel channel;
	enum clk_sel clk_sel;/* clock select */
	int clk_div_num;/* clock divide number*/
	int res_mode;/* resolution sample */
	int sam_time;/* sample time  */
	int lp_con;/* low power configuration */
	int hs_oper;/* high speed operation */
	enum vol_ref vol_ref;/* voltage reference */
	int tri_sel;/* trigger select */
	int ha_sel;/* hardware average select */
	int ha_sam;/* hardware average sample */
	int do_ena;/* data overwrite enable */
	int ac_ena;/* Asynchronous clock output enable */
	int dma_ena;/* DMA enable */
	int cc_ena;/* continue converdion enable */
	int compare_func_ena;/* enable compare function */
	int range_ena;/* range enable */
	int greater_ena;/* greater than enable */
	unsigned int result0, result1;
};

#endif
