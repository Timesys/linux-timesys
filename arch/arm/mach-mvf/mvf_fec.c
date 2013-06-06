/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <linux/fec.h>
#include <linux/etherdevice.h>

#include <mach/common.h>
#include <mach/hardware.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include "devices-mvf.h"

#define ENET_PALR	0x0e4
#define ENET_PAUR	0x0e8
static unsigned char default_mac[ETH_ALEN] = {
			0x00, 0xe0, 0x0c, 0xbc, 0xe5, 0x60};
/* FEC mac registers set by bootloader */
static int fec_get_mac_addr(unsigned char *mac)
{
	unsigned int value;

	value = readl(MVF_IO_ADDRESS(MVF_FEC_BASE_ADDR) + ENET_PALR);
	mac[3] = value & 0xff;
	mac[2] = (value >> 8) & 0xff;
	mac[1] = (value >> 16) & 0xff;
	mac[0] = (value >> 24) & 0xff;
	value = readl(MVF_IO_ADDRESS(MVF_FEC_BASE_ADDR) + ENET_PAUR);
	mac[5] = (value >> 16) & 0xff;
	mac[4] = (value >> 24) & 0xff;

	return 0;
}

void __init mvf_init_fec(struct fec_platform_data fec_data)
{
	fec_get_mac_addr(fec_data.mac);
	if (!is_valid_ether_addr(fec_data.mac))
		memcpy(fec_data.mac, default_mac, ETH_ALEN);

	mvf_add_fec(0, &fec_data);
#ifdef CONFIG_FEC1
	mvf_add_fec(1, &fec_data);
#endif
}
