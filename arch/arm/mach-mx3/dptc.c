/*
 * Copyright 2005-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * @file dptc.c
 *
 * @brief DPTC table for the Freescale Semiconductor MXC DPTC module.
 *
 * @ingroup PM
 */

#include <asm/arch/pmic_power.h>
#include <asm/arch/hardware.h>

struct dptc_wp dptc_wp_allfreq_26ckih[DPTC_WP_SUPPORTED] = {
	/* 532MHz */
	/* dcvr0      dcvr1       dcvr2       dcvr3       regulator voltage */
	/* wp0 */
	{0xffc00000, 0x95c00000, 0xffc00000, 0xe5800000, SW_SW1A, SW1A_1_625V},
	{0xffc00000, 0x95e3e8e4, 0xffc00000, 0xe5b6fda0, SW_SW1A, SW1A_1_6V},
	{0xffc00000, 0x95e3e8e4, 0xffc00000, 0xe5b6fda0, SW_SW1A, SW1A_1_575V},
	{0xffc00000, 0x95e3e8e8, 0xffc00000, 0xe5f70da4, SW_SW1A, SW1A_1_55V},
	{0xffc00000, 0x9623f8e8, 0xffc00000, 0xe6371da8, SW_SW1A, SW1A_1_525V},
	/* wp5 */
	{0xffc00000, 0x966408f0, 0xffc00000, 0xe6b73db0, SW_SW1A, SW1A_1_5V},
	{0xffc00000, 0x96e428f4, 0xffc00000, 0xe7776dbc, SW_SW1A, SW1A_1_475V},
	{0xffc00000, 0x976448fc, 0xffc00000, 0xe8379dc8, SW_SW1A, SW1A_1_45V},
	{0xffc00000, 0x97e46904, 0xffc00000, 0xe977ddd8, SW_SW1A, SW1A_1_425V},
	{0xffc00000, 0x98a48910, 0xffc00000, 0xeab81de8, SW_SW1A, SW1A_1_4V},
	/* wp10 */
	{0xffc00000, 0x9964b918, 0xffc00000, 0xebf86df8, SW_SW1A, SW1A_1_375V},
	{0xffc00000, 0xffe4e924, 0xffc00000, 0xfff8ae08, SW_SW1A, SW1A_1_35V},
	{0xffc00000, 0xffe5192c, 0xffc00000, 0xfff8fe1c, SW_SW1A, SW1A_1_35V},
	{0xffc00000, 0xffe54938, 0xffc00000, 0xfff95e2c, SW_SW1A, SW1A_1_35V},
	{0xffc00000, 0xffe57944, 0xffc00000, 0xfff9ae44, SW_SW1A, SW1A_1_35V},
	/* wp15 */
	{0xffc00000, 0xffe5b954, 0xffc00000, 0xfffa0e58, SW_SW1A, SW1A_1_35V},
	{0xffc00000, 0xffe5e960, 0xffc00000, 0xfffa6e70, SW_SW1A, SW1A_1_35V},
};

struct dptc_wp dptc_wp_allfreq_26ckih_TO_2_0[DPTC_WP_SUPPORTED] = {
	/* Mx31 TO 2.0  Offset table */
	/* 532MHz  */
	/* dcvr0      dcvr1       dcvr2       dcvr3       regulator voltage */
	/* wp0 */
	{0xffc00000, 0x9DE63974, 0xffc00000, 0xE3F72D9C, SW_SW1A, SW1A_1_625V},
	{0xffc00000, 0x9E265978, 0xffc00000, 0xE4371D9C, SW_SW1A, SW1A_1_6V},
	{0xffc00000, 0x9EE6697C, 0xffc00000, 0xE4772DA0, SW_SW1A, SW1A_1_575V},
	{0xffc00000, 0x9F265978, 0xffc00000, 0xE4B73DA0, SW_SW1A, SW1A_1_55V},
	{0xffc00000, 0x9F265978, 0xffc00000, 0xE4F73DA0, SW_SW1A, SW1A_1_525V},
	/* wp5 */
	{0xffc00000, 0x9F26797C, 0xffc00000, 0xE5B75DA4, SW_SW1A, SW1A_1_5V},
	{0xffc00000, 0x9FE6797C, 0xffc00000, 0xE6375DA4, SW_SW1A, SW1A_1_475V},
	{0xffc00000, 0xA026797C, 0xffc00000, 0xE6B76DA8, SW_SW1A, SW1A_1_45V},
	{0xffc00000, 0xA0E69980, 0xffc00000, 0xE6F76DA8, SW_SW1A, SW1A_1_425V},
	{0xffc00000, 0xA166A980, 0xffc00000, 0xE7778DA8, SW_SW1A, SW1A_1_4V},
	/* wp10 */
	{0xffc00000, 0xA1669980, 0xffc00000, 0xE8379DAC, SW_SW1A, SW1A_1_375V},
	{0xffc00000, 0xA1A6A984, 0xffc00000, 0xE8F7ADB0, SW_SW1A, SW1A_1_35V},
	{0xffc00000, 0xA1E6A980, 0xffc00000, 0xE9B7BDB0, SW_SW1A, SW1A_1_325V},
	{0xffc00000, 0xA266C984, 0xffc00000, 0xEA77CDB4, SW_SW1A, SW1A_1_3V},
	{0xffc00000, 0xA2E6C984, 0xffc00000, 0xEB77FDB8, SW_SW1A, SW1A_1_275V},
	/* wp15 */
	{0xffc00000, 0xA3A6D988, 0xffc00000, 0xECF82DBC, SW_SW1A, SW1A_1_25V},
	{0xffc00000, 0xA466F98C, 0xffc00000, 0xEDF81DBC, SW_SW1A, SW1A_1_225V},
};

/* DPTC table for 27MHz */
struct dptc_wp dptc_wp_allfreq_27ckih[DPTC_WP_SUPPORTED] = {
	/* 532MHz */
	/* dcvr0      dcvr1       dcvr2       dcvr3       regulator voltage */
	/* wp0 */
	{0xffc00000, 0x90400000, 0xffc00000, 0xdd000000, SW_SW1A, SW1A_1_625V},
	{0xffc00000, 0x90629890, 0xffc00000, 0xdd34ed20, SW_SW1A, SW1A_1_6V},
	{0xffc00000, 0x90629890, 0xffc00000, 0xdd34ed20, SW_SW1A, SW1A_1_575V},
	{0xffc00000, 0x90629894, 0xffc00000, 0xdd74fd24, SW_SW1A, SW1A_1_55V},
	{0xffc00000, 0x90a2a894, 0xffc00000, 0xddb50d28, SW_SW1A, SW1A_1_525V},
	/* wp5 */
	{0xffc00000, 0x90e2b89c, 0xffc00000, 0xde352d30, SW_SW1A, SW1A_1_5V},
	{0xffc00000, 0x9162d8a0, 0xffc00000, 0xdef55d38, SW_SW1A, SW1A_1_475V},
	{0xffc00000, 0x91e2f8a8, 0xffc00000, 0xdfb58d44, SW_SW1A, SW1A_1_45V},
	{0xffc00000, 0x926308b0, 0xffc00000, 0xe0b5cd54, SW_SW1A, SW1A_1_425V},
	{0xffc00000, 0x92e328bc, 0xffc00000, 0xe1f60d64, SW_SW1A, SW1A_1_4V},
	/* wp10 */
	{0xffc00000, 0x93a358c0, 0xffc00000, 0xe3365d74, SW_SW1A, SW1A_1_375V},
	{0xffc00000, 0xf66388cc, 0xffc00000, 0xf6768d84, SW_SW1A, SW1A_1_35V},
	{0xffc00000, 0xf663b8d4, 0xffc00000, 0xf676dd98, SW_SW1A, SW1A_1_325V},
	{0xffc00000, 0xf663e8e0, 0xffc00000, 0xf6773da4, SW_SW1A, SW1A_1_3V},
	{0xffc00000, 0xf66418ec, 0xffc00000, 0xf6778dbc, SW_SW1A, SW1A_1_275V},
	/* wp15 */
	{0xffc00000, 0xf66458fc, 0xffc00000, 0xf677edd0, SW_SW1A, SW1A_1_25V},
	{0xffc00000, 0xf6648908, 0xffc00000, 0xf6783de8, SW_SW1A, SW1A_1_2V},
};
