/*
 * Copyright 2004-2007 Freescale Semiconductor, Inc. All Rights Reserved.
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
 * @file sensor_clock.c
 *
 * @brief camera clock function
 *
 * @ingroup Camera
 */
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/clk.h>

/*
 * set_mclk_rate
 *
 * @param       p_mclk_freq  mclk frequence
 *
 */
void set_mclk_rate(uint32_t * p_mclk_freq)
{
	struct clk *clk;
	int i;
	uint32_t freq = 0;
	uint32_t step = *p_mclk_freq / 8;

	clk = clk_get(NULL, "csi_clk");

	for (i = 0; i <= 8; i++) {
		freq = clk_round_rate(clk, *p_mclk_freq - (i * step));
		if (freq <= *p_mclk_freq)
			break;
	}
	clk_set_rate(clk, freq);

	*p_mclk_freq = freq;

	clk_put(clk);
	pr_debug("mclk frequency = %d\n", *p_mclk_freq);
}

/* Exported symbols for modules. */
EXPORT_SYMBOL(set_mclk_rate);
