/*
 *  L2 switch Controller (Ethernet switch) driver
 *  for Freescale M5441x and Vybrid.
 *
 *  Copyright 2010-2012 Freescale Semiconductor, Inc.
 *    Alison Wang (b18965@freescale.com)
 *    Jason Jin (Jason.jin@freescale.com)
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/bitops.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/fsl_devices.h>
#include <linux/phy.h>
#include <linux/kthread.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/signal.h>
#include <linux/clk.h>

#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/cacheflush.h>
#include <linux/version.h>
#if 0 /* LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0) */
#include <mach/hardware.h>
#include <mach/fsl_l2_switch.h>
#else
#include <linux/fsl_l2_switch.h>
#endif

#define	SWITCH_MAX_PORTS	1
#define CONFIG_FEC_SHARED_PHY

static unsigned char macaddr[ETH_ALEN];
module_param_array(macaddr, byte, NULL, 0);
MODULE_PARM_DESC(macaddr, "FEC Ethernet MAC address");

/* Interrupt events/masks */
#define FEC_ENET_HBERR	((uint)0x80000000)	/* Heartbeat error */
#define FEC_ENET_BABR	((uint)0x40000000)	/* Babbling receiver */
#define FEC_ENET_BABT	((uint)0x20000000)	/* Babbling transmitter */
#define FEC_ENET_GRA	((uint)0x10000000)	/* Graceful stop complete */
#define FEC_ENET_TXF	((uint)0x08000000)	/* Full frame transmitted */
#define FEC_ENET_TXB	((uint)0x04000000)	/* A buffer was transmitted */
#define FEC_ENET_RXF	((uint)0x02000000)	/* Full frame received */
#define FEC_ENET_RXB	((uint)0x01000000)	/* A buffer was received */
#define FEC_ENET_MII	((uint)0x00800000)	/* MII interrupt */
#define FEC_ENET_EBERR	((uint)0x00400000)	/* SDMA bus error */

static int switch_enet_open(struct net_device *dev);
static int switch_enet_start_xmit(struct sk_buff *skb, struct net_device *dev);
static irqreturn_t switch_enet_interrupt(int irq, void *dev_id);
static void switch_enet_tx(struct net_device *dev);
static void switch_enet_rx(struct net_device *dev);
static int switch_enet_close(struct net_device *dev);
#ifdef UNUSED
static void set_multicast_list(struct net_device *dev);
#endif
static void switch_restart(struct net_device *dev, int duplex);
static void switch_stop(struct net_device *dev);
static void switch_get_mac_address(struct net_device *dev);

#define		NMII	20

/* Make MII read/write commands for the FEC */
#define mk_mii_read(REG)	(0x60020000 | ((REG & 0x1f) << 18))
#define mk_mii_write(REG, VAL)	(0x50020000 | ((REG & 0x1f) << 18) | \
						(VAL & 0xffff))
/* MII MMFR bits definition */
#define ESW_MMFR_ST		(1 << 30)
#define ESW_MMFR_OP_READ	(2 << 28)
#define ESW_MMFR_OP_WRITE	(1 << 28)
#define ESW_MMFR_PA(v)		((v & 0x1f) << 23)
#define ESW_MMFR_RA(v)		((v & 0x1f) << 18)
#define ESW_MMFR_TA		(2 << 16)
#define ESW_MMFR_DATA(v)	(v & 0xffff)

#define ESW_MII_TIMEOUT		30 /* ms */

/* Transmitter timeout.*/
#define TX_TIMEOUT (2*HZ)

/*last read entry from learning interface*/
eswPortInfo g_info;
/* switch ports status */
struct port_status ports_link_status;

/* the user space pid, used to send the link change to user space */
long user_pid = 1;

/* ----------------------------------------------------------------*/
/*
 * Calculate Galois Field Arithmetic CRC for Polynom x^8+x^2+x+1.
 * It omits the final shift in of 8 zeroes a "normal" CRC would do
 * (getting the remainder).
 *
 *  Examples (hexadecimal values):<br>
 *   10-11-12-13-14-15  => CRC=0xc2
 *   10-11-cc-dd-ee-00  => CRC=0xe6
 *
 *   param: pmacaddress
 *          A 6-byte array with the MAC address.
 *          The first byte is the first byte transmitted
 *   return The 8-bit CRC in bits 7:0
 */
int crc8_calc(unsigned char *pmacaddress)
{
	/* byte index */
	int byt;
	/* bit index */
	int bit;
	int inval;
	int crc;
	/* preset */
	crc   = 0x12;
	for (byt = 0; byt < 6; byt++) {
		inval = (((int)pmacaddress[byt]) & 0xff);
		/*
		 * shift bit 0 to bit 8 so all our bits
		 * travel through bit 8
		 * (simplifies below calc)
		 */
		inval <<= 8;

		for (bit = 0; bit < 8; bit++) {
			/* next input bit comes into d7 after shift */
			crc |= inval & 0x100;
			if (crc & 0x01)
				/* before shift  */
				crc ^= 0x1c0;

			crc >>= 1;
			inval >>= 1;
		}

	}
	/* upper bits are clean as we shifted in zeroes! */
	return crc;
}

void read_atable(struct switch_enet_private *fep,
	int index, unsigned long *read_lo, unsigned long *read_hi)
{
	unsigned long atable_base = (unsigned long)fep->macbase;

	*read_lo = readl((void *)(atable_base + (index << 3)));
	*read_hi = readl((void *)(atable_base + (index << 3) + 4));
}

void write_atable(struct switch_enet_private *fep,
	int index, unsigned long write_lo, unsigned long write_hi)
{
	unsigned long atable_base = (unsigned long)fep->macbase;

	writel(write_lo, (void *)atable_base + (index << 3));
	writel(write_hi, (void *)atable_base + (index<<3) + 4);
}

/* Check if the Port Info FIFO has data available
 * for reading. 1 valid, 0 invalid*/
int esw_portinfofifo_status(struct switch_enet_private *fep)
{
	return readl(fep->membase + FEC_ESW_LSR);
}

/* Initialize the Port Info FIFO. */
void esw_portinfofifo_initialize(struct switch_enet_private *fep)
{
	unsigned long tmp;

	/* disable all learn */
	tmp = readl(fep->membase + FEC_ESW_IMR);
	tmp &= (~FSL_ESW_IMR_LRN);
	writel(tmp, fep->membase + FEC_ESW_IMR);

	/* remove all entries from FIFO */
	while (esw_portinfofifo_status(fep)) {
		/* read one data word */
		tmp = readl(fep->membase + FEC_ESW_LREC0);
		tmp = readl(fep->membase + FEC_ESW_LREC1);
	}

}

/* Read one element from the HW receive FIFO (Queue)
 * if available and return it.
 * return ms_HwPortInfo or null if no data is available
 */
eswPortInfo *esw_portinfofifo_read(struct switch_enet_private *fep)
{
	unsigned long tmp;

	/* check learning record valid */
	if (!(readl(fep->membase + FEC_ESW_LSR)))
		return NULL;

	/*read word from FIFO*/
	g_info.maclo = readl(fep->membase + FEC_ESW_LREC0);

	/*but verify that we actually did so
	 * (0=no data available)*/
	if (g_info.maclo == 0)
		return NULL;

	/* read 2nd word from FIFO */
	tmp = readl(fep->membase + FEC_ESW_LREC1);
	g_info.machi = tmp & 0xffff;
	g_info.hash  = (tmp >> 16) & 0xff;
	g_info.port  = (tmp >> 24) & 0xf;

	return &g_info;
}

/* Clear complete MAC Look Up Table */
void esw_clear_atable(struct switch_enet_private *fep)
{
	int index;
	for (index = 0; index < 2048; index++)
		write_atable(fep, index, 0, 0);
}

void esw_dump_atable(struct switch_enet_private *fep)
{
	int index;
	unsigned long read_lo, read_hi;
	for (index = 0; index < 2048; index++)
		read_atable(fep, index, &read_lo, &read_hi);
}

/*
 * pdates MAC address lookup table with a static entry
 * Searches if the MAC address is already there in the block and replaces
 * the older entry with new one. If MAC address is not there then puts a
 * new entry in the first empty slot available in the block
 *
 * mac_addr Pointer to the array containing MAC address to
 *          be put as static entry
 * port     Port bitmask numbers to be added in static entry,
 *          valid values are 1-7
 * priority Priority for the static entry in table
 *
 * return 0 for a successful update else -1  when no slot available
 */
int esw_update_atable_static(unsigned char *mac_addr,
	unsigned int port, unsigned int priority,
	struct switch_enet_private *fep)
{
	unsigned long block_index, entry, index_end;
	unsigned long read_lo, read_hi;
	unsigned long write_lo, write_hi;

	write_lo = (unsigned long)((mac_addr[3] << 24) |
			(mac_addr[2] << 16) |
			(mac_addr[1] << 8) |
			mac_addr[0]);
	write_hi = (unsigned long)(0 |
			(port << AT_SENTRY_PORTMASK_shift) |
			(priority << AT_SENTRY_PRIO_shift) |
			(AT_ENTRY_TYPE_STATIC << AT_ENTRY_TYPE_shift) |
			(AT_ENTRY_RECORD_VALID << AT_ENTRY_VALID_shift) |
			(mac_addr[5] << 8) | (mac_addr[4]));

	block_index = GET_BLOCK_PTR(crc8_calc(mac_addr));
	index_end = block_index + ATABLE_ENTRY_PER_SLOT;
	/* Now search all the entries in the selected block */
	for (entry = block_index; entry < index_end; entry++) {
		read_atable(fep, entry, &read_lo, &read_hi);
		/*
		 * MAC address matched, so update the
		 * existing entry
		 * even if its a dynamic one
		 */
		if ((read_lo == write_lo) && ((read_hi & 0x0000ffff) ==
			 (write_hi & 0x0000ffff))) {
			write_atable(fep, entry, write_lo, write_hi);
			return 0;
		} else if (!(read_hi & (1 << 16))) {
			/*
			 * Fill this empty slot (valid bit zero),
			 * assuming no holes in the block
			 */
			write_atable(fep, entry, write_lo, write_hi);
			fep->atCurrEntries++;
			return 0;
		}
	}

	/* No space available for this static entry */
	return -1;
}

/* lookup entry in given Address Table slot and
 * insert (learn) it if it is not found.
 * return 0 if entry was found and updated.
 *        1 if entry was not found and has been inserted (learned).
 */
int esw_update_atable_dynamic(unsigned char *mac_addr, unsigned int port,
		unsigned int currTime, struct switch_enet_private *fep)
{
	unsigned long block_index, entry, index_end;
	unsigned long read_lo, read_hi;
	unsigned long write_lo, write_hi;
	unsigned long tmp;
	int time, timeold, indexold;

	/* prepare update port and timestamp */
	write_hi = (mac_addr[5] << 8) | (mac_addr[4]);
	write_lo = (unsigned long)((mac_addr[3] << 24) |
			(mac_addr[2] << 16) |
			(mac_addr[1] << 8) |
			mac_addr[0]);
	tmp = AT_ENTRY_RECORD_VALID << AT_ENTRY_VALID_shift;
	tmp |= AT_ENTRY_TYPE_DYNAMIC << AT_ENTRY_TYPE_shift;
	tmp |= currTime << AT_DENTRY_TIME_shift;
	tmp |= port << AT_DENTRY_PORT_shift;
	tmp |= write_hi;

	/*
	 * linear search through all slot
	 * entries and update if found
	 */
	block_index = GET_BLOCK_PTR(crc8_calc(mac_addr));
	index_end = block_index + ATABLE_ENTRY_PER_SLOT;
	 /* Now search all the entries in the selected block */
	for (entry = block_index; entry < index_end; entry++) {
		read_atable(fep, entry, &read_lo, &read_hi);

		if ((read_lo == write_lo) &&
			((read_hi & 0x0000ffff) ==
			(write_hi & 0x0000ffff))) {
			/* found correct address,
			 * update timestamp. */
			write_atable(fep, entry, write_lo, tmp);
			return 0;
		} else if (!(read_hi & (1 << 16))) {
			/* slot is empty, then use it
			 * for new entry
			 * Note: There are no holes,
			 * therefore cannot be any
			 * more that need to be compared.
			 */
			write_atable(fep, entry, write_lo, tmp);
			/* statistics (we do it between writing
			 * .hi an .lo due to
			 * hardware limitation...
			 */
			fep->atCurrEntries++;
			/* newly inserted */
			return 1;
		}
	}

	/*
	 * no more entry available in blockk ...
	 * overwrite oldest
	 */
	timeold = 0;
	indexold = 0;
	for (entry = block_index; entry < index_end; entry++) {
		read_atable(fep, entry, &read_lo, &read_hi);
		time = AT_EXTRACT_TIMESTAMP(read_hi);
		time = TIMEDELTA(currTime, time);
		if (time > timeold) {
			/* is it older ?*/
			timeold = time;
			indexold = entry;
		}
	}

	write_atable(fep, indexold, write_lo, tmp);
	/* Statistics (do it inbetween
	 * writing to .lo and .hi*/
	fep->atBlockOverflows++;
	/* newly inserted */
	return 1;
}

int esw_update_atable_dynamic1(unsigned long write_lo, unsigned long write_hi,
		int block_index, unsigned int port, unsigned int currTime,
		struct switch_enet_private *fep)
{
	unsigned long entry, index_end;
	unsigned long read_lo, read_hi;
	unsigned long tmp;
	int time, timeold, indexold;

	/* prepare update port and timestamp */
	tmp = AT_ENTRY_RECORD_VALID << AT_ENTRY_VALID_shift;
	tmp |= AT_ENTRY_TYPE_DYNAMIC << AT_ENTRY_TYPE_shift;
	tmp |= currTime << AT_DENTRY_TIME_shift;
	tmp |= port << AT_DENTRY_PORT_shift;
	tmp |= write_hi;

	/*
	* linear search through all slot
	* entries and update if found
	*/
	index_end = block_index + ATABLE_ENTRY_PER_SLOT;
	/* Now search all the entries in the selected block */
	for (entry = block_index; entry < index_end; entry++) {
		read_atable(fep, entry, &read_lo, &read_hi);
		if ((read_lo == write_lo) &&
			((read_hi & 0x0000ffff) ==
			(write_hi & 0x0000ffff))) {
			/* found correct address,
			 * update timestamp. */
			write_atable(fep, entry, write_lo, tmp);
			return 0;
		} else if (!(read_hi & (1 << 16))) {
			/* slot is empty, then use it
			* for new entry
			* Note: There are no holes,
			* therefore cannot be any
			* more that need to be compared.
			*/
			write_atable(fep, entry, write_lo, tmp);
			/* statistics (we do it between writing
			*  .hi an .lo due to
			* hardware limitation...
			*/
			fep->atCurrEntries++;
			/* newly inserted */
			return 1;
		}
	}

	/*
	* no more entry available in block ...
	* overwrite oldest
	*/
	timeold = 0;
	indexold = 0;
	for (entry = block_index; entry < index_end; entry++) {
		read_atable(fep, entry, &read_lo, &read_hi);
		time = AT_EXTRACT_TIMESTAMP(read_hi);
		time = TIMEDELTA(currTime, time);
		if (time > timeold) {
			/* is it older ?*/
			timeold = time;
			indexold = entry;
		}
	}

	write_atable(fep, indexold, write_lo, tmp);
	/* Statistics (do it inbetween
	* writing to .lo and .hi*/
	fep->atBlockOverflows++;
	/* newly inserted */
	return 1;
}

/*
 * Delete one dynamic entry within the given block
 * of 64-bit entries.
 * return number of valid entries in the block after deletion.
 */
int esw_del_atable_dynamic(struct switch_enet_private *fep,
	int blockidx, int entryidx)
{
	unsigned long index_start, index_end;
	int i;
	unsigned long read_lo, read_hi;

	/* the entry to delete */
	index_start = blockidx + entryidx;
	/* one after last */
	index_end = blockidx + ATABLE_ENTRY_PER_SLOT;
	/* Statistics */
	fep->atCurrEntries--;

	if (entryidx == (ATABLE_ENTRY_PER_SLOT - 1)) {
		/* if it is the very last entry,
		* just delete it without further efford*/
		write_atable(fep, index_start, 0, 0);
		/*number of entries left*/
		i = ATABLE_ENTRY_PER_SLOT - 1;
		return i;
	} else {
		/*not the last in the block, then
		 * shift all that follow the one
		 * that is deleted to avoid "holes".
		 */
		for (i = index_start; i < (index_end - 1); i++) {
			read_atable(fep, i + 1, &read_lo, &read_hi);
			/* move it down */
			write_atable(fep, i, read_lo, read_hi);
			if (!(read_hi & (1 << 16))) {
				/* stop if we just copied the last */
				return i - blockidx;
			}
		}

		/*moved all entries up to the last.
		 * then set invalid flag in the last*/
		write_atable(fep, index_end - 1, 0, 0);
		/* number of valid entries left */
		return i - blockidx;
	}
}

void esw_atable_dynamicms_del_entries_for_port(
	struct switch_enet_private *fep, int port_index)
{
	unsigned long read_lo, read_hi;
	unsigned int port_idx;
	int i;

	for (i = 0; i < ESW_ATABLE_MEM_NUM_ENTRIES; i++) {
		read_atable(fep, i, &read_lo, &read_hi);
		if (read_hi & (1 << 16)) {
			port_idx = AT_EXTRACT_PORT(read_hi);

			if (port_idx == port_index)
				write_atable(fep, i, 0, 0);
		}
	}
}

void esw_atable_dynamicms_del_entries_for_other_port(
	struct switch_enet_private *fep,
	int port_index)
{
	unsigned long read_lo, read_hi;
	unsigned int port_idx;
	int i;

	for (i = 0; i < ESW_ATABLE_MEM_NUM_ENTRIES; i++) {
		read_atable(fep, i, &read_lo, &read_hi);
		if (read_hi & (1 << 16)) {
			port_idx = AT_EXTRACT_PORT(read_hi);

			if (port_idx != port_index)
				write_atable(fep, i, 0, 0);
		}
	}
}

/*
 *  Scan one complete block (Slot) for outdated entries and delete them.
 *  blockidx index of block of entries that should be analyzed.
 *  return number of deleted entries, 0 if nothing was modified.
 */
int esw_atable_dynamicms_check_block_age(
	struct switch_enet_private *fep, int blockidx) {

	int i, tm, tdelta;
	int deleted = 0, entries = 0;
	unsigned long read_lo, read_hi;
	/* Scan all entries from last down to
	 * have faster deletion speed if necessary*/
	for (i = (blockidx + ATABLE_ENTRY_PER_SLOT - 1);
		i >= blockidx; i--) {
		read_atable(fep, i, &read_lo, &read_hi);

		if (read_hi & (1 << 16)) {
			/* the entry is valide*/
			tm = AT_EXTRACT_TIMESTAMP(read_hi);
			tdelta = TIMEDELTA(fep->currTime, tm);
			if (tdelta > fep->ageMax) {
				esw_del_atable_dynamic(fep,
					blockidx, i-blockidx);
				deleted++;
			} else {
				/* statistics */
				entries++;
			}
		}
	}

	/*update statistics*/
	if (fep->atMaxEntriesPerBlock < entries)
		fep->atMaxEntriesPerBlock = entries;

	return deleted;
}

/* scan the complete address table and find the most current entry.
 * The time of the most current entry then is used as current time
 * for the context structure.
 * In addition the atCurrEntries value is updated as well.
 * return time that has been set in the context.
 */
int esw_atable_dynamicms_find_set_latesttime(
	struct switch_enet_private *fep) {

	int tm_min, tm_max, tm;
	int delta, cur, i;
	unsigned long read_lo, read_hi;

	tm_min = (1 << AT_DENTRY_TIMESTAMP_WIDTH) - 1;
	tm_max = 0;
	cur = 0;

	for (i = 0; i < ESW_ATABLE_MEM_NUM_ENTRIES; i++) {
		read_atable(fep, i, &read_lo, &read_hi);
		if (read_hi & (1 << 16)) {
			/*the entry is valid*/
			tm = AT_EXTRACT_TIMESTAMP(read_hi);
			if (tm > tm_max)
				tm_max = tm;
			if (tm < tm_min)
				tm_min = tm;
			cur++;
		}
	}

	delta = TIMEDELTA(tm_max, tm_min);
	if (delta < fep->ageMax) {
		/*Difference must be in range*/
		fep->currTime = tm_max;
	} else {
		fep->currTime = tm_min;
	}

	fep->atCurrEntries = cur;
	return fep->currTime;
}

int esw_atable_dynamicms_get_port(
	struct switch_enet_private *fep,
	unsigned long write_lo,
	unsigned long write_hi,
	int block_index)
{
	int i, index_end;
	unsigned long read_lo, read_hi, port;

	index_end = block_index + ATABLE_ENTRY_PER_SLOT;
	/* Now search all the entries in the selected block */
	for (i = block_index; i < index_end; i++) {
		read_atable(fep, i, &read_lo, &read_hi);

		if ((read_lo == write_lo) &&
			((read_hi & 0x0000ffff) ==
			(write_hi & 0x0000ffff))) {
			/* found correct address,*/
			if (read_hi & (1 << 16)) {
				/*extract the port index  from the valid entry*/
				port = AT_EXTRACT_PORT(read_hi);
				return port;
			}
		}
	}

	return -1;
}

/* Get the port index from the source MAC address
 * of the received frame
 * @return port index
 */
int esw_atable_dynamicms_get_portindex_from_mac(
	struct switch_enet_private *fep,
	unsigned char *mac_addr,
	unsigned long write_lo,
	unsigned long write_hi)
{
	int blockIdx;
	int rc;
	/*compute the block index*/
	blockIdx = GET_BLOCK_PTR(crc8_calc(mac_addr));
	/* Get the ingress port index of the received BPDU */
	rc = esw_atable_dynamicms_get_port(fep,
		write_lo, write_hi, blockIdx);

	return rc;
}

/* dynamicms MAC address table learn and migration*/
int esw_atable_dynamicms_learn_migration(
	struct switch_enet_private *fep,
	int currTime)
{
	eswPortInfo *pESWPortInfo;
	int index;
	int inserted = 0;

	pESWPortInfo = esw_portinfofifo_read(fep);
	/* Anything to learn */
	if (pESWPortInfo != 0) {
		/*get block index from lookup table*/
		index = GET_BLOCK_PTR(pESWPortInfo->hash);
		inserted = esw_update_atable_dynamic1(
			pESWPortInfo->maclo,
			pESWPortInfo->machi, index,
			pESWPortInfo->port, currTime, fep);
	}

	return 0;
}
/* -----------------------------------------------------------------*/
/*
 * esw_forced_forward
 * The frame is forwared to the forced destination ports.
 * It only replace the MAC lookup function,
 * all other filtering(eg.VLAN verification) act as normal
 */
int esw_forced_forward(struct switch_enet_private *fep,
	int port1, int port2, int enable)
{
	unsigned long tmp = 0;

	/* Enable Forced forwarding for port num */
	if ((port1 == 1) && (port2 == 1))
		tmp |= FSL_ESW_P0FFEN_FD(3);
	else if (port1 == 1)
		/*Enable Forced forwarding for port 1 only*/
		tmp |= FSL_ESW_P0FFEN_FD(1);
	else if (port2 == 1)
		/*Enable Forced forwarding for port 2 only*/
		tmp |= FSL_ESW_P0FFEN_FD(2);
	else {
		printk(KERN_ERR "%s:do not support "
			"the forced forward mode"
			"port1 %x port2 %x\n",
			__func__, port1, port2);
		return -1;
	}

	if (enable == 1)
		tmp |= FSL_ESW_P0FFEN_FEN;
	else if (enable == 0)
		tmp &= ~FSL_ESW_P0FFEN_FEN;
	else {
		printk(KERN_ERR "%s: the enable %x is error\n",
			__func__, enable);
		return -2;
	}

	writel(tmp, fep->membase + FEC_ESW_P0FFEN);
	return 0;
}

void esw_get_forced_forward(
	struct switch_enet_private *fep,
	unsigned long *ulForceForward)
{
	*ulForceForward = readl(fep->membase + FEC_ESW_P0FFEN);
}

void esw_get_port_enable(
	struct switch_enet_private *fep,
	unsigned long *ulPortEnable)
{
	*ulPortEnable = readl(fep->membase + FEC_ESW_PER);
}

/*
 * enable or disable port n tx or rx
 * tx_en 0 disable port n tx
 * tx_en 1 enable  port n tx
 * rx_en 0 disbale port n rx
 * rx_en 1 enable  port n rx
 */
int esw_port_enable_config(struct switch_enet_private *fep,
	int port, int tx_en, int rx_en)
{
	unsigned long tmp = 0;

	tmp = readl(fep->membase + FEC_ESW_PER);
	if (tx_en == 1) {
		if (port == 0)
			tmp |= FSL_ESW_PER_TE0;
		else if (port == 1)
			tmp |= FSL_ESW_PER_TE1;
		else if (port == 2)
			tmp |= FSL_ESW_PER_TE2;
		else {
			printk(KERN_ERR "%s:do not support the"
				" port %x tx enable\n",
				__func__, port);
			return -1;
		}
	} else if (tx_en == 0) {
		if (port == 0)
			tmp &= (~FSL_ESW_PER_TE0);
		else if (port == 1)
			tmp &= (~FSL_ESW_PER_TE1);
		else if (port == 2)
			tmp &= (~FSL_ESW_PER_TE2);
		else {
			printk(KERN_ERR "%s:do not support "
				"the port %x tx disable\n",
				__func__, port);
			return -2;
		}
	} else {
		printk(KERN_ERR "%s:do not support the port %x"
			" tx op value %x\n",
			__func__, port, tx_en);
		return -3;
	}

	if (rx_en == 1) {
		if (port == 0)
			tmp |= FSL_ESW_PER_RE0;
		else if (port == 1)
			tmp |= FSL_ESW_PER_RE1;
		else if (port == 2)
			tmp |= FSL_ESW_PER_RE2;
		else {
			printk(KERN_ERR "%s:do not support the "
				"port %x rx enable\n",
				__func__, port);
			return -4;
		}
	} else if (rx_en == 0) {
		if (port == 0)
			tmp &= (~FSL_ESW_PER_RE0);
		else if (port == 1)
			tmp &= (~FSL_ESW_PER_RE1);
		else if (port == 2)
			tmp &= (~FSL_ESW_PER_RE2);
		else {
			printk(KERN_ERR "%s:do not support the "
				"port %x rx disable\n",
				__func__, port);
			return -5;
		}
	} else {
		printk(KERN_ERR "%s:do not support the port %x"
			" rx op value %x\n",
			__func__, port, tx_en);
		return -6;
	}

	writel(tmp, fep->membase + FEC_ESW_PER);
	return 0;
}


void esw_get_port_broadcast(struct switch_enet_private *fep,
			unsigned long *ulPortBroadcast)
{
	*ulPortBroadcast = readl(fep->membase + FEC_ESW_DBCR);
}

int esw_port_broadcast_config(struct switch_enet_private *fep,
			int port, int enable)
{
	unsigned long tmp = 0;

	if ((port > 2) || (port < 0)) {
		printk(KERN_ERR "%s:do not support the port %x"
			" default broadcast\n",
			__func__, port);
		return -1;
	}

	tmp = readl(fep->membase + FEC_ESW_DBCR);
	if (enable == 1) {
		if (port == 0)
			tmp |= FSL_ESW_DBCR_P0;
		else if (port == 1)
			tmp |= FSL_ESW_DBCR_P1;
		else if (port == 2)
			tmp |= FSL_ESW_DBCR_P2;
	} else if (enable == 0) {
		if (port == 0)
			tmp &= ~FSL_ESW_DBCR_P0;
		else if (port == 1)
			tmp &= ~FSL_ESW_DBCR_P1;
		else if (port == 2)
			tmp &= ~FSL_ESW_DBCR_P2;
	}

	writel(tmp, fep->membase + FEC_ESW_DBCR);
	return 0;
}


void esw_get_port_multicast(struct switch_enet_private *fep,
	unsigned long *ulPortMulticast)
{
	*ulPortMulticast = readl(fep->membase + FEC_ESW_DMCR);
}

int esw_port_multicast_config(struct switch_enet_private *fep,
	int port, int enable)
{
	unsigned long tmp = 0;

	if ((port > 2) || (port < 0)) {
		printk(KERN_ERR "%s:do not support the port %x"
			" default broadcast\n",
			__func__, port);
		return -1;
	}

	tmp = readl(fep->membase + FEC_ESW_DMCR);
	if (enable == 1) {
		if (port == 0)
			tmp |= FSL_ESW_DMCR_P0;
		else if (port == 1)
			tmp |= FSL_ESW_DMCR_P1;
		else if (port == 2)
			tmp |= FSL_ESW_DMCR_P2;
	} else if (enable == 0) {
		if (port == 0)
			tmp &= ~FSL_ESW_DMCR_P0;
		else if (port == 1)
			tmp &= ~FSL_ESW_DMCR_P1;
		else if (port == 2)
			tmp &= ~FSL_ESW_DMCR_P2;
	}

	writel(tmp, fep->membase + FEC_ESW_DMCR);
	return 0;
}


void esw_get_port_blocking(struct switch_enet_private *fep,
	unsigned long *ulPortBlocking)
{
	*ulPortBlocking = readl(fep->membase + FEC_ESW_BKLR) & 0x0000000f;
}

int esw_port_blocking_config(struct switch_enet_private *fep,
	int port, int enable)
{
	unsigned long tmp = 0;

	if ((port > 2) || (port < 0)) {
		printk(KERN_ERR "%s:do not support the port %x"
			" default broadcast\n",
			__func__, port);
		return -1;
	}

	tmp = readl(fep->membase + FEC_ESW_BKLR);
	if (enable == 1) {
		if (port == 0)
			tmp |= FSL_ESW_BKLR_BE0;
		else if (port == 1)
			tmp |= FSL_ESW_BKLR_BE1;
		else if (port == 2)
			tmp |= FSL_ESW_BKLR_BE2;
	} else if (enable == 0) {
		if (port == 0)
			tmp &= ~FSL_ESW_BKLR_BE0;
		else if (port == 1)
			tmp &= ~FSL_ESW_BKLR_BE1;
		else if (port == 2)
			tmp &= ~FSL_ESW_BKLR_BE2;
	}

	writel(tmp, fep->membase + FEC_ESW_BKLR);
	return 0;
}


void esw_get_port_learning(struct switch_enet_private *fep,
	unsigned long *ulPortLearning)
{
	*ulPortLearning =
		(readl(fep->membase + FEC_ESW_BKLR) & 0x000f0000) >> 16;
}

int esw_port_learning_config(struct switch_enet_private *fep,
	int port, int disable)
{
	unsigned long tmp = 0;

	if ((port > 2) || (port < 0)) {
		printk(KERN_ERR "%s:do not support the port %x"
			" default broadcast\n",
			__func__, port);
		return -1;
	}

	tmp = readl(fep->membase + FEC_ESW_BKLR);
	if (disable == 0) {
		fep->learning_irqhandle_enable = 0;
		if (port == 0)
			tmp |= FSL_ESW_BKLR_LD0;
		else if (port == 1)
			tmp |= FSL_ESW_BKLR_LD1;
		else if (port == 2)
			tmp |= FSL_ESW_BKLR_LD2;
	} else if (disable == 1) {
		if (port == 0)
			tmp &= ~FSL_ESW_BKLR_LD0;
		else if (port == 1)
			tmp &= ~FSL_ESW_BKLR_LD1;
		else if (port == 2)
			tmp &= ~FSL_ESW_BKLR_LD2;
	}

	writel(tmp, fep->membase + FEC_ESW_BKLR);
	return 0;
}

/*********************************************************************/
void esw_mac_lookup_table_range(struct switch_enet_private *fep)
{
	int index;
	unsigned long read_lo, read_hi;
	/* Pointer to switch address look up memory*/
	for (index = 0; index < 2048; index++)
		write_atable(fep, index, index, (~index));

	/* Pointer to switch address look up memory*/
	for (index = 0; index < 2048; index++) {
		read_atable(fep, index, &read_lo, &read_hi);
		if (read_lo != index) {
			printk(KERN_ERR "%s:Mismatch at low %d\n",
				__func__, index);
			return;
		}

		if (read_hi != (~index)) {
			printk(KERN_ERR "%s:Mismatch at high %d\n",
				__func__, index);
			return;
		}
	}
}

/*
 * Checks IP Snoop options of handling the snooped frame.
 * mode 0 : The snooped frame is forward only to management port
 * mode 1 : The snooped frame is copy to management port and
 *              normal forwarding is checked.
 * mode 2 : The snooped frame is discarded.
 * mode 3 : Disable the ip snoop function
 * ip_header_protocol : the IP header protocol field
 */
int esw_ip_snoop_config(struct switch_enet_private *fep,
		int mode, unsigned long ip_header_protocol)
{
	unsigned long tmp = 0, protocol_type = 0;
	int num = 0;

	/* Config IP Snooping */
	if (mode == 0) {
		/* Enable IP Snooping */
		tmp = FSL_ESW_IPSNP_EN;
		tmp |= FSL_ESW_IPSNP_MODE(0);/*For Forward*/
	} else if (mode == 1) {
		/* Enable IP Snooping */
		tmp = FSL_ESW_IPSNP_EN;
		/*For Forward and copy_to_mangmnt_port*/
		tmp |= FSL_ESW_IPSNP_MODE(1);
	} else if (mode == 2) {
		/* Enable IP Snooping */
		tmp = FSL_ESW_IPSNP_EN;
		tmp |= FSL_ESW_IPSNP_MODE(2);/*discard*/
	} else if (mode == 3) {
		/* disable IP Snooping */
		tmp = FSL_ESW_IPSNP_EN;
		tmp &= ~FSL_ESW_IPSNP_EN;
	} else {
		printk(KERN_ERR "%s: the mode %x "
			"we do not support\n", __func__, mode);
		return -1;
	}

	protocol_type = ip_header_protocol;
	for (num = 0; num < 8; num++) {
		unsigned long reg = readl(fep->membase + FEC_ESW_IPSNP(num));
		if (protocol_type ==
			AT_EXTRACT_IP_PROTOCOL(reg)) {
			writel(tmp | FSL_ESW_IPSNP_PROTOCOL(protocol_type),
				fep->membase + FEC_ESW_IPSNP(num));
			break;
		} else if (!reg) {
			writel(tmp | FSL_ESW_IPSNP_PROTOCOL(protocol_type),
				fep->membase + FEC_ESW_IPSNP(num));
			break;
		}
	}
	if (num == 8) {
		printk(KERN_INFO "IP snooping table is full\n");
		return 0;
	}

	return 0;
}

void esw_get_ip_snoop_config(struct switch_enet_private *fep,
	unsigned long *ulpESW_IPSNP)
{
	int i;

	for (i = 0; i < 8; i++)
		*(ulpESW_IPSNP + i) = readl(fep->membase + FEC_ESW_IPSNP(i));
}
/*
 * Checks TCP/UDP Port Snoop options of handling the snooped frame.
 * mode 0 : The snooped frame is forward only to management port
 * mode 1 : The snooped frame is copy to management port and
 *              normal forwarding is checked.
 * mode 2 : The snooped frame is discarded.
 * mode 3 : Disable the TCP/UDP port snoop function
 * compare_port : port number in the TCP/UDP header
 * compare_num 1: TCP/UDP source port number is compared
 * compare_num 2: TCP/UDP destination port number is compared
 * compare_num 3: TCP/UDP source and destination port number is compared
 */
int esw_tcpudp_port_snoop_config(struct switch_enet_private *fep,
		int mode, int compare_port, int compare_num)
{
	unsigned long tmp;
	int num;

	/* Enable TCP/UDP port Snooping */
	tmp = FSL_ESW_PSNP_EN;
	if (mode == 0)
		tmp |= FSL_ESW_PSNP_MODE(0);/*For Forward*/
	else if (mode == 1)/*For Forward and copy_to_mangmnt_port*/
		tmp |= FSL_ESW_PSNP_MODE(1);
	else if (mode == 2)
		tmp |= FSL_ESW_PSNP_MODE(2);/*discard*/
	else if (mode == 3) /*disable the port function*/
		tmp &= (~FSL_ESW_PSNP_EN);
	else {
		printk(KERN_ERR "%s: the mode %x we do not support\n",
			__func__, mode);
		return -1;
	}

	if (compare_num == 1)
		tmp |= FSL_ESW_PSNP_CS;
	else if (compare_num == 2)
		tmp |= FSL_ESW_PSNP_CD;
	else if (compare_num == 3)
		tmp |= FSL_ESW_PSNP_CD | FSL_ESW_PSNP_CS;
	else {
		printk(KERN_ERR "%s: the compare port address %x"
			" we do not support\n",
			__func__, compare_num);
		return -1;
	}

	for (num = 0; num < 8; num++) {
		u32 reg = readl(fep->membase + FEC_ESW_PSNP(num));
		if (compare_port ==
			AT_EXTRACT_TCP_UDP_PORT(reg)) {
			writel(tmp | FSL_ESW_PSNP_PORT_COMPARE(compare_port),
				fep->membase + FEC_ESW_PSNP(num));
			break;
		} else if (!reg) {
			writel(tmp | FSL_ESW_PSNP_PORT_COMPARE(compare_port),
				fep->membase + FEC_ESW_PSNP(num));
			break;
		}
	}
	if (num == 8) {
		printk(KERN_INFO "TCP/UDP port snooping table is full\n");
		return 0;
	}

	return 0;
}

void esw_get_tcpudp_port_snoop_config(
	struct switch_enet_private *fep,
	unsigned long *ulpESW_PSNP)
{
	int i;

	for (i = 0; i < 8; i++)
		*(ulpESW_PSNP + i) = readl(fep->membase + FEC_ESW_PSNP(i));
}

/*mirror*/
void esw_get_port_mirroring(struct switch_enet_private *fep)
{
	unsigned long tmp, tmp1, tmp2;

	tmp = readl(fep->membase + FEC_ESW_MCR);

	printk(KERN_INFO "Mirror Port: %ld   Egress Port Match:%s    "
		"Ingress Port Match:%s\n", tmp & 0xf,
		tmp & 1 ? "Y" : "N",
		tmp & 1 ? "Y" : "N");

	if ((tmp >> 6) & 1)
		printk(KERN_INFO "Egress Port to be mirrored: Port %d\n",
			readl(fep->membase + FEC_ESW_EGMAP) >> 1);
	if ((tmp >> 5) & 1)
		printk(KERN_INFO "Ingress Port to be mirrored: Port %d\n",
			readl(fep->membase + FEC_ESW_INGMAP) >> 1);

	printk(KERN_INFO "Egress Des Address Match:%s    "
		"Egress Src Address Match:%s\n",
		(tmp >> 10) & 1 ? "Y" : "N",
		(tmp >> 9) & 1 ? "Y" : "N");
	printk(KERN_INFO "Ingress Des Address Match:%s   "
		"Ingress Src Address Match:%s\n",
		(tmp >> 8) & 1 ? "Y" : "N",
		(tmp >> 7) & 1 ? "Y" : "N");

	if ((tmp >> 10) & 1) {
		tmp1 = readl(fep->membase + FEC_ESW_ENGDAL);
		tmp2 = readl(fep->membase + FEC_ESW_ENGDAH);
		printk(KERN_INFO "Egress Des Address to be mirrored: "
			"%02lx-%02lx-%02lx-%02lx-%02lx-%02lx\n",
			tmp1 & 0xff,
			(tmp1 >> 8) & 0xff,
			(tmp1  >> 16) & 0xff,
			(tmp1 >> 24) & 0xff,
			tmp2 & 0xff,
			(tmp2 >> 8) & 0xff);
	}
	if ((tmp >> 9) & 1) {
		tmp1 = readl(fep->membase + FEC_ESW_ENGSAL);
		tmp2 = readl(fep->membase + FEC_ESW_ENGSAH);
		printk("Egress Src Address to be mirrored: "
			"%02lx-%02lx-%02lx-%02lx-%02lx-%02lx\n",
			tmp1 & 0xff,
			(tmp1 >> 8) & 0xff,
			(tmp1 >> 16) & 0xff,
			(tmp1 >> 24) & 0xff,
			tmp2 & 0xff,
			(tmp2 >> 8) & 0xff);
	}
	if ((tmp >> 8) & 1) {
		tmp1 = readl(fep->membase + FEC_ESW_INGDAL);
		tmp2 = readl(fep->membase + FEC_ESW_INGDAH);
		printk("Ingress Des Address to be mirrored: "
			"%02lx-%02lx-%02lx-%02lx-%02lx-%02lx\n",
			tmp1 & 0xff,
			(tmp1 >> 8) & 0xff,
			(tmp1 >> 16) & 0xff,
			(tmp1 >> 24) & 0xff,
			tmp2 & 0xff,
			(tmp2 >> 8) & 0xff);
	}
	if ((tmp >> 7) & 1) {
		tmp1 = readl(fep->membase + FEC_ESW_INGSAL);
		tmp2 = readl(fep->membase + FEC_ESW_INGSAH);
		printk("Ingress Src Address to be mirrored: "
			"%02lx-%02lx-%02lx-%02lx-%02lx-%02lx\n",
			tmp1 & 0xff,
			(tmp1 >> 8) & 0xff,
			(tmp1 >> 16) & 0xff,
			(tmp1 >> 24) & 0xff,
			tmp2 & 0xff,
			(tmp2 >> 8) & 0xff);
	}
}

int esw_port_mirroring_config_port_match(struct switch_enet_private *fep,
	int mirror_port, int port_match_en, int port)
{
	unsigned long tmp = 0;

	tmp = readl(fep->membase + FEC_ESW_MCR);
	if (mirror_port != (tmp & 0xf))
		tmp = 0;

	switch (port_match_en) {
	case MIRROR_EGRESS_PORT_MATCH:
		tmp |= FSL_ESW_MCR_EGMAP;
		if (port == 0)
			writel(FSL_ESW_EGMAP_EG0, fep->membase + FEC_ESW_EGMAP);
		else if (port == 1)
			writel(FSL_ESW_EGMAP_EG1, fep->membase + FEC_ESW_EGMAP);
		else if (port == 2)
			writel(FSL_ESW_EGMAP_EG2, fep->membase + FEC_ESW_EGMAP);
		break;
	case MIRROR_INGRESS_PORT_MATCH:
		tmp |= FSL_ESW_MCR_INGMAP;
		if (port == 0)
			writel(FSL_ESW_INGMAP_ING0,
				fep->membase + FEC_ESW_INGMAP);
		else if (port == 1)
			writel(FSL_ESW_INGMAP_ING1,
				fep->membase + FEC_ESW_INGMAP);
		else if (port == 2)
			writel(FSL_ESW_INGMAP_ING2,
				fep->membase + FEC_ESW_INGMAP);
		break;
	default:
		tmp = 0;
		break;
	}

	tmp = tmp & 0x07e0;
	if (port_match_en)
		tmp |= FSL_ESW_MCR_MEN | FSL_ESW_MCR_PORT(mirror_port);

	writel(tmp, fep->membase + FEC_ESW_MCR);
	return 0;
}

int esw_port_mirroring_config(struct switch_enet_private *fep,
	int mirror_port, int port, int mirror_enable,
	unsigned char *src_mac, unsigned char *des_mac,
	int egress_en, int ingress_en,
	int egress_mac_src_en, int egress_mac_des_en,
	int ingress_mac_src_en, int ingress_mac_des_en)
{
	unsigned long tmp;

	/*mirroring config*/
	tmp = 0;
	if (egress_en == 1) {
		tmp |= FSL_ESW_MCR_EGMAP;
		if (port == 0)
			writel(FSL_ESW_EGMAP_EG0, fep->membase + FEC_ESW_EGMAP);
		else if (port == 1)
			writel(FSL_ESW_EGMAP_EG1, fep->membase + FEC_ESW_EGMAP);
		else if (port == 2)
			writel(FSL_ESW_EGMAP_EG2, fep->membase + FEC_ESW_EGMAP);
		else {
			printk(KERN_ERR "%s: the port %x we do not support\n",
					__func__, port);
			return -1;
		}
	} else if (egress_en == 0) {
		tmp &= (~FSL_ESW_MCR_EGMAP);
	} else {
		printk(KERN_ERR "%s: egress_en %x we do not support\n",
			__func__, egress_en);
		return -1;
	}

	if (ingress_en == 1) {
		tmp |= FSL_ESW_MCR_INGMAP;
		if (port == 0)
			writel(FSL_ESW_INGMAP_ING0,
				fep->membase + FEC_ESW_INGMAP);
		else if (port == 1)
			writel(FSL_ESW_INGMAP_ING1,
				fep->membase + FEC_ESW_INGMAP);
		else if (port == 2)
			writel(FSL_ESW_INGMAP_ING2,
				fep->membase + FEC_ESW_INGMAP);
		else {
			printk(KERN_ERR "%s: the port %x we do not support\n",
				__func__, port);
			return -1;
		}
	} else if (ingress_en == 0) {
		tmp &= ~FSL_ESW_MCR_INGMAP;
	} else{
		printk(KERN_ERR "%s: ingress_en %x we do not support\n",
				__func__, ingress_en);
		return -1;
	}

	if (egress_mac_src_en == 1) {
		tmp |= FSL_ESW_MCR_EGSA;
		writel((src_mac[5] << 8) | (src_mac[4]),
			fep->membase + FEC_ESW_ENGSAH);
		writel((unsigned long)((src_mac[3] << 24) | (src_mac[2] << 16) |
			(src_mac[1] << 8) | src_mac[0]),
			fep->membase + FEC_ESW_ENGSAL);
	} else if (egress_mac_src_en == 0) {
		tmp &= ~FSL_ESW_MCR_EGSA;
	} else {
		printk(KERN_ERR "%s: egress_mac_src_en  %x we do not support\n",
			__func__, egress_mac_src_en);
		return -1;
	}

	if (egress_mac_des_en == 1) {
		tmp |= FSL_ESW_MCR_EGDA;
		writel((des_mac[5] << 8) | (des_mac[4]),
			fep->membase + FEC_ESW_ENGDAH);
		writel((unsigned long)((des_mac[3] << 24) | (des_mac[2] << 16) |
			(des_mac[1] << 8) | des_mac[0]),
			fep->membase + FEC_ESW_ENGDAL);
	} else if (egress_mac_des_en == 0) {
		tmp &= ~FSL_ESW_MCR_EGDA;
	} else {
		printk(KERN_ERR "%s: egress_mac_des_en  %x we do not support\n",
			__func__, egress_mac_des_en);
		return -1;
	}

	if (ingress_mac_src_en == 1) {
		tmp |= FSL_ESW_MCR_INGSA;
		writel((src_mac[5] << 8) | (src_mac[4]),
			fep->membase + FEC_ESW_INGSAH);
		writel((unsigned long)((src_mac[3] << 24) | (src_mac[2] << 16) |
			(src_mac[1] << 8) | src_mac[0]),
			fep->membase + FEC_ESW_INGSAL);
	} else if (ingress_mac_src_en == 0) {
		tmp &= ~FSL_ESW_MCR_INGSA;
	} else {
		printk(KERN_ERR "%s: ingress_mac_src_en  %x "
			"we do not support\n",
			__func__, ingress_mac_src_en);
		return -1;
	}

	if (ingress_mac_des_en == 1) {
		tmp |= FSL_ESW_MCR_INGDA;
		writel((des_mac[5] << 8) | (des_mac[4]),
			fep->membase + FEC_ESW_INGDAH);
		writel((unsigned long)((des_mac[3] << 24) | (des_mac[2] << 16) |
			(des_mac[1] << 8) | des_mac[0]),
			fep->membase + FEC_ESW_INGDAL);
	} else if (ingress_mac_des_en == 0) {
		tmp &= ~FSL_ESW_MCR_INGDA;
	} else {
		printk(KERN_ERR "%s: ingress_mac_des_en  %x we do not support\n",
			__func__, ingress_mac_des_en);
		return -1;
	}

	if (mirror_enable == 1)
		tmp |= FSL_ESW_MCR_MEN | FSL_ESW_MCR_PORT(mirror_port);
	else if (mirror_enable == 0)
		tmp &= ~FSL_ESW_MCR_MEN;
	else
		printk(KERN_ERR "%s: the mirror enable %x is error\n",
			__func__, mirror_enable);


	writel(tmp, fep->membase + FEC_ESW_MCR);
	return 0;
}

int esw_port_mirroring_config_addr_match(struct switch_enet_private *fep,
	int mirror_port, int addr_match_enable, unsigned char *mac_addr)
{
	unsigned long tmp = 0;

	tmp = readl(fep->membase + FEC_ESW_MCR);
	if (mirror_port != (tmp & 0xf))
		tmp = 0;

	switch (addr_match_enable) {
	case MIRROR_EGRESS_SOURCE_MATCH:
		tmp |= FSL_ESW_MCR_EGSA;
		writel((mac_addr[5] << 8) | (mac_addr[4]),
			fep->membase + FEC_ESW_ENGSAH);
		writel((unsigned long)((mac_addr[3] << 24) |
			(mac_addr[2] << 16) | (mac_addr[1] << 8) | mac_addr[0]),
				fep->membase + FEC_ESW_ENGSAL);
		break;
	case MIRROR_INGRESS_SOURCE_MATCH:
		tmp |= FSL_ESW_MCR_INGSA;
		writel((mac_addr[5] << 8) | (mac_addr[4]),
			fep->membase + FEC_ESW_INGSAH);
		writel((unsigned long)((mac_addr[3] << 24) |
			(mac_addr[2] << 16) | (mac_addr[1] << 8) | mac_addr[0]),
				fep->membase + FEC_ESW_INGSAL);
		break;
	case MIRROR_EGRESS_DESTINATION_MATCH:
		tmp |= FSL_ESW_MCR_EGDA;
		writel((mac_addr[5] << 8) | (mac_addr[4]),
			fep->membase + FEC_ESW_ENGDAH);
		writel((unsigned long)((mac_addr[3] << 24) |
			(mac_addr[2] << 16) | (mac_addr[1] << 8) | mac_addr[0]),
			fep->membase + FEC_ESW_ENGDAL);
		break;
	case MIRROR_INGRESS_DESTINATION_MATCH:
		tmp |= FSL_ESW_MCR_INGDA;
		writel((mac_addr[5] << 8) | (mac_addr[4]),
			fep->membase + FEC_ESW_INGDAH);
		writel((unsigned long)((mac_addr[3] << 24) |
			(mac_addr[2] << 16) | (mac_addr[1] << 8) | mac_addr[0]),
				fep->membase + FEC_ESW_INGDAL);
		break;
	default:
		tmp = 0;
		break;
	}

	tmp = tmp & 0x07e0;
	if (addr_match_enable)
		tmp |= FSL_ESW_MCR_MEN | FSL_ESW_MCR_PORT(mirror_port);

	writel(tmp, fep->membase + FEC_ESW_MCR);
	return 0;
}

void esw_get_vlan_verification(struct switch_enet_private *fep,
	unsigned long *ulValue)
{
	*ulValue = readl(fep->membase + FEC_ESW_VLANV);
}

int esw_set_vlan_verification(struct switch_enet_private *fep, int port,
	int vlan_domain_verify_en, int vlan_discard_unknown_en)
{
	u32 tmp;

	if ((port < 0) || (port > 2)) {
		printk(KERN_ERR "%s: do not support the port %d\n",
			__func__, port);
		return -1;
	}
	tmp = readl(fep->membase + FEC_ESW_VLANV);
	if (vlan_domain_verify_en == 1) {
		if (port == 0)
			tmp |= FSL_ESW_VLANV_VV0;
		else if (port == 1)
			tmp |= FSL_ESW_VLANV_VV1;
		else if (port == 2)
			tmp |= FSL_ESW_VLANV_VV2;
	} else if (vlan_domain_verify_en == 0) {
		if (port == 0)
			tmp &= ~FSL_ESW_VLANV_VV0;
		else if (port == 1)
			tmp &= ~FSL_ESW_VLANV_VV1;
		else if (port == 2)
			tmp &= ~FSL_ESW_VLANV_VV2;
	} else {
		printk(KERN_INFO "%s: donot support "
			"vlan_domain_verify %x\n",
			__func__, vlan_domain_verify_en);
		return -2;
	}

	if (vlan_discard_unknown_en == 1) {
		if (port == 0)
			tmp |= FSL_ESW_VLANV_DU0;
		else if (port == 1)
			tmp |= FSL_ESW_VLANV_DU1;
		else if (port == 2)
			tmp |= FSL_ESW_VLANV_DU2;
	} else if (vlan_discard_unknown_en == 0) {
		if (port == 0)
			tmp &= ~FSL_ESW_VLANV_DU0;
		else if (port == 1)
			tmp &= ~FSL_ESW_VLANV_DU1;
		else if (port == 2)
			tmp &= ~FSL_ESW_VLANV_DU2;
	} else {
		printk(KERN_INFO "%s: donot support "
			"vlan_discard_unknown %x\n",
			__func__, vlan_discard_unknown_en);
		return -3;
	}

	writel(tmp, fep->membase + FEC_ESW_VLANV);
	return 0;
}

void esw_get_vlan_resolution_table(struct switch_enet_private *fep,
	struct eswVlanTableItem *tableaddr)
{
	int vnum = 0;
	int i;
	u32 tmp;

	for (i = 0; i < 32; i++) {
		tmp = readl(fep->membase + FEC_ESW_VRES(i));
		if (tmp) {
			tableaddr->table[i].port_vlanid =
				tmp >> 3;
			tableaddr->table[i].vlan_domain_port =
				tmp & 7;
			vnum++;
		}
	}
	tableaddr->valid_num = vnum;
}

int esw_set_vlan_id(struct switch_enet_private *fep, unsigned long configData)
{
	int i;
	u32 tmp;

	for (i = 0; i < 32; i++) {
		tmp = readl(fep->membase + FEC_ESW_VRES(i));
		if (tmp == 0) {
			writel(FSL_ESW_VRES_VLANID(configData),
					fep->membase + FEC_ESW_VRES(i));
			return 0;
		} else if (((tmp >> 3) & 0xfff) == configData) {
			printk(KERN_INFO "The VLAN already exists\n");
			return 0;
		}
	}

	printk(KERN_INFO "The VLAN can't create, because VLAN table is full\n");
	return 0;
}

int esw_set_vlan_id_cleared(struct switch_enet_private *fep,
		unsigned long configData)
{
	int i;
	u32 tmp;

	for (i = 0; i < 32; i++) {
		tmp = readl(fep->membase + FEC_ESW_VRES(i));
		if (((tmp >> 3) & 0xfff) == configData) {
			writel(0, fep->membase + FEC_ESW_VRES(i));
			break;
		}
	}
	return 0;
}

int esw_set_port_in_vlan_id(struct switch_enet_private *fep,
	       eswIoctlVlanResoultionTable configData)
{
	int i;
	int lastnum = 0;
	u32 tmp;

	for (i = 0; i < 32; i++) {
		tmp = readl(fep->membase + FEC_ESW_VRES(i));
		if (tmp == 0) {
			lastnum = i;
			break;
		} else if (((tmp >> 3) & 0xfff) ==
				configData.port_vlanid) {
			/* update the port members of this vlan */
			tmp |= 1 << configData.vlan_domain_port;
			writel(tmp, fep->membase + FEC_ESW_VRES(i));
			return 0;
		}
	}
	/* creat a new vlan in vlan table */
	writel(FSL_ESW_VRES_VLANID(configData.port_vlanid) |
		(1 << configData.vlan_domain_port),
		fep->membase + FEC_ESW_VRES(lastnum));
	return 0;
}

int esw_set_vlan_resolution_table(struct switch_enet_private *fep,
	unsigned short port_vlanid, int vlan_domain_num,
	int vlan_domain_port)
{
	if ((vlan_domain_num < 0)
		|| (vlan_domain_num > 31)) {
		printk(KERN_ERR "%s: do not support the "
			"vlan_domain_num %d\n",
		__func__, vlan_domain_num);
		return -1;
	}

	if ((vlan_domain_port < 0)
		|| (vlan_domain_port > 7)) {
		printk(KERN_ERR "%s: do not support the "
			"vlan_domain_port %d\n",
			__func__, vlan_domain_port);
		return -2;
	}

	writel(
		FSL_ESW_VRES_VLANID(port_vlanid)
		| vlan_domain_port,
		fep->membase + FEC_ESW_VRES(vlan_domain_num));

	return 0;
}

void esw_get_vlan_input_config(struct switch_enet_private *fep,
	eswIoctlVlanInputStatus *pVlanInputConfig)
{
	int i;

	for (i = 0; i < 3; i++)
		pVlanInputConfig->ESW_PID[i] =
			readl(fep->membase + FEC_ESW_PID(i));

	pVlanInputConfig->ESW_VLANV  = readl(fep->membase + FEC_ESW_VLANV);
	pVlanInputConfig->ESW_VIMSEL = readl(fep->membase + FEC_ESW_VIMSEL);
	pVlanInputConfig->ESW_VIMEN  = readl(fep->membase + FEC_ESW_VIMEN);

	for (i = 0; i < 32; i++)
		pVlanInputConfig->ESW_VRES[i] =
			readl(fep->membase + FEC_ESW_VRES(i));
}


int esw_vlan_input_process(struct switch_enet_private *fep,
	int port, int mode, unsigned short port_vlanid)
{
	u32 tmp;

	if ((mode < 0) || (mode > 5)) {
		printk(KERN_ERR "%s: do not support the"
			" VLAN input processing mode %d\n",
			__func__, mode);
		return -1;
	}

	if ((port < 0) || (port > 3)) {
		printk(KERN_ERR "%s: do not support the port %d\n",
			__func__, mode);
		return -2;
	}

	writel(FSL_ESW_PID_VLANID(port_vlanid),
		fep->membase + FEC_ESW_PID(port));

	if (port == 0) {
		tmp = readl(fep->membase + FEC_ESW_VIMEN);
		if (mode == 4)
			tmp &= ~FSL_ESW_VIMEN_EN0;
		else
			tmp |= FSL_ESW_VIMEN_EN0;
		writel(tmp, fep->membase + FEC_ESW_VIMEN);

		tmp = readl(fep->membase + FEC_ESW_VIMSEL);
		tmp &= ~FSL_ESW_VIMSEL_IM0(3);
		tmp |= FSL_ESW_VIMSEL_IM0(mode);
		writel(tmp, fep->membase + FEC_ESW_VIMSEL);
	} else if (port == 1) {
		tmp = readl(fep->membase + FEC_ESW_VIMEN);
		if (mode == 4)
			tmp &= ~FSL_ESW_VIMEN_EN1;
		else
			tmp |= FSL_ESW_VIMEN_EN1;
		writel(tmp, fep->membase + FEC_ESW_VIMEN);

		tmp = readl(fep->membase + FEC_ESW_VIMSEL);
		tmp &= ~FSL_ESW_VIMSEL_IM1(3);
		tmp |= FSL_ESW_VIMSEL_IM1(mode);
		writel(tmp, fep->membase + FEC_ESW_VIMSEL);
	} else if (port == 2) {
		tmp = readl(fep->membase + FEC_ESW_VIMEN);
		if (mode == 4)
			tmp &= ~FSL_ESW_VIMEN_EN2;
		else
			tmp |= FSL_ESW_VIMEN_EN2;
		writel(tmp, fep->membase + FEC_ESW_VIMEN);

		tmp = readl(fep->membase + FEC_ESW_VIMSEL);
		tmp &= ~FSL_ESW_VIMSEL_IM2(3);
		tmp |= FSL_ESW_VIMSEL_IM2(mode);
		writel(tmp, fep->membase + FEC_ESW_VIMSEL);
	} else {
		printk(KERN_ERR "%s: do not support the port %d\n",
			__func__, port);
		return -2;
	}

	return 0;
}

void esw_get_vlan_output_config(struct switch_enet_private *fep,
	unsigned long *ulVlanOutputConfig)
{
	*ulVlanOutputConfig = readl(fep->membase + FEC_ESW_VOMSEL);
}

int esw_vlan_output_process(struct switch_enet_private *fep,
	int port, int mode)
{
	u32 tmp;

	if ((port < 0) || (port > 2)) {
		printk(KERN_ERR "%s: do not support the port %d\n",
			__func__, mode);
		return -1;
	}
	tmp = readl(fep->membase + FEC_ESW_VOMSEL);
	if (port == 0) {
		tmp &= ~FSL_ESW_VOMSEL_OM0(3);
		tmp |= FSL_ESW_VOMSEL_OM0(mode);
	} else if (port == 1) {
		tmp &= ~FSL_ESW_VOMSEL_OM1(3);
		tmp |= FSL_ESW_VOMSEL_OM1(mode);
	} else if (port == 2) {
		tmp &= ~FSL_ESW_VOMSEL_OM2(3);
		tmp |= FSL_ESW_VOMSEL_OM2(mode);
	} else {
		printk(KERN_ERR "%s: do not support the port %d\n",
			__func__, port);
		return -1;
	}
	writel(tmp, fep->membase + FEC_ESW_VOMSEL);
	return 0;
}

/*
 * frame calssify and priority resolution
 * vlan priority lookup
 */
int esw_framecalssify_vlan_priority_lookup(struct switch_enet_private *fep,
	int port, int func_enable, int vlan_pri_table_num,
	int vlan_pri_table_value)
{
	u32 tmp;

	if ((port < 0) || (port > 3)) {
		printk(KERN_ERR "%s: do not support the port %d\n",
			__func__, port);
		return -1;
	}

	if (func_enable == 0) {
		tmp = readl(fep->membase + FEC_ESW_PRES(port));
		tmp &= ~FSL_ESW_PRES_VLAN;
		writel(tmp, fep->membase + FEC_ESW_PRES(port));
		printk(KERN_ERR "%s: disable port %d VLAN priority "
			"lookup function\n", __func__, port);
		return 0;
	}

	if ((vlan_pri_table_num < 0) || (vlan_pri_table_num > 7)) {
		printk(KERN_ERR "%s: do not support the priority %d\n",
			__func__, vlan_pri_table_num);
		return -1;
	}

	tmp = readl(fep->membase + FEC_ESW_PVRES(port));
	tmp |= ((vlan_pri_table_value & 0x3)
		<< (vlan_pri_table_num*3));
	writel(tmp, fep->membase + FEC_ESW_PVRES(port));

	/* enable port  VLAN priority lookup function*/
	tmp = readl(fep->membase + FEC_ESW_PRES(port));
	tmp |= FSL_ESW_PRES_VLAN;
	writel(tmp, fep->membase + FEC_ESW_PRES(port));
	return 0;
}

int esw_framecalssify_ip_priority_lookup(struct switch_enet_private *fep,
	int port, int func_enable, int ipv4_en, int ip_priority_num,
	int ip_priority_value)
{
	unsigned long tmp = 0, tmp_prio = 0;

	if ((port < 0) || (port > 3)) {
		printk(KERN_ERR "%s: do not support the port %d\n",
			__func__, port);
		return -1;
	}

	if (func_enable == 0) {
		tmp = readl(fep->membase + FEC_ESW_PRES(port));
		tmp &= ~FSL_ESW_PRES_IP;
		writel(tmp, fep->membase + FEC_ESW_PRES(port));
		printk(KERN_ERR "%s: disable port %d ip priority "
			"lookup function\n", __func__, port);
		return 0;
	}

	/* IPV4 priority 64 entry table lookup*/
	/* IPv4 head 6 bit TOS field*/
	if (ipv4_en == 1) {
		if ((ip_priority_num < 0) || (ip_priority_num > 63)) {
			printk(KERN_ERR "%s: do not support the table entry %d\n",
				__func__, ip_priority_num);
			return -2;
		}
	} else { /* IPV6 priority 256 entry table lookup*/
		/* IPv6 head 8 bit COS field*/
		if ((ip_priority_num < 0) || (ip_priority_num > 255)) {
			printk(KERN_ERR "%s: do not support the table entry %d\n",
				__func__, ip_priority_num);
			return -3;
		}
	}

	/* IP priority  table lookup : address*/
	tmp = FSL_ESW_IPRES_ADDRESS(ip_priority_num);
	/* IP priority  table lookup : ipv4sel*/
	if (ipv4_en == 1)
		tmp = tmp | FSL_ESW_IPRES_IPV4SEL;
	/* IP priority  table lookup : priority*/
	if (port == 0)
		tmp |= FSL_ESW_IPRES_PRI0(ip_priority_value);
	else if (port == 1)
		tmp |= FSL_ESW_IPRES_PRI1(ip_priority_value);
	else if (port == 2)
		tmp |= FSL_ESW_IPRES_PRI2(ip_priority_value);

	/* configure*/
	writel(FSL_ESW_IPRES_READ | FSL_ESW_IPRES_ADDRESS(ip_priority_num),
		fep->membase + FEC_ESW_IPRES);
	tmp_prio = readl(fep->membase + FEC_ESW_IPRES);

	writel(tmp | tmp_prio, fep->membase + FEC_ESW_IPRES);

	writel(FSL_ESW_IPRES_READ | FSL_ESW_IPRES_ADDRESS(ip_priority_num),
		fep->membase + FEC_ESW_IPRES);
	tmp_prio = readl(fep->membase + FEC_ESW_IPRES);

	/* enable port  IP priority lookup function*/
	tmp = readl(fep->membase + FEC_ESW_PRES(port));
	tmp |= FSL_ESW_PRES_IP;
	writel(tmp, fep->membase + FEC_ESW_PRES(port));
	return 0;
}

int esw_framecalssify_mac_priority_lookup(
		struct switch_enet_private *fep, int port)
{
	u32 tmp;

	if ((port < 0) || (port > 3)) {
		printk(KERN_ERR "%s: do not support the port %d\n",
			__func__, port);
		return -1;
	}

	tmp = readl(fep->membase + FEC_ESW_PRES(port));
	tmp |= FSL_ESW_PRES_MAC;
	writel(tmp, fep->membase + FEC_ESW_PRES(port));

	return 0;
}

int esw_frame_calssify_priority_init(struct switch_enet_private *fep,
	int port, unsigned char priority_value)
{
	if ((port < 0) || (port > 3)) {
		printk(KERN_ERR "%s: do not support the port %d\n",
			__func__, port);
		return -1;
	}
	/*disable all priority lookup function*/
	writel(0, fep->membase + FEC_ESW_PRES(port));
	writel(FSL_ESW_PRES_DFLT_PRI(priority_value & 0x7),
			fep->membase + FEC_ESW_PRES(port));

	return 0;
}

int esw_get_statistics_status(struct switch_enet_private *fep,
	esw_statistics_status *pStatistics)
{
	pStatistics->ESW_DISCN   = readl(fep->membase + FEC_ESW_DISCN);
	pStatistics->ESW_DISCB   = readl(fep->membase + FEC_ESW_DISCB);
	pStatistics->ESW_NDISCN  = readl(fep->membase + FEC_ESW_NDISCN);
	pStatistics->ESW_NDISCB  = readl(fep->membase + FEC_ESW_NDISCB);
	return 0;
}

int esw_get_port_statistics_status(struct switch_enet_private *fep,
	int port, esw_port_statistics_status *pPortStatistics)
{
	if ((port < 0) || (port > 3)) {
		printk(KERN_ERR "%s: do not support the port %d\n",
			__func__, port);
		return -1;
	}

	pPortStatistics->FSL_ESW_POQC   =
		readl(fep->membase + FEC_ESW_POQC(port));
	pPortStatistics->FSL_ESW_PMVID  =
		readl(fep->membase + FEC_ESW_PMVID(port));
	pPortStatistics->FSL_ESW_PMVTAG =
		readl(fep->membase + FEC_ESW_PMVTAG(port));
	pPortStatistics->FSL_ESW_PBL    =
		readl(fep->membase + FEC_ESW_PBL(port));
	return 0;
}

int esw_get_output_queue_status(struct switch_enet_private *fep,
	esw_output_queue_status *pOutputQueue)
{
	pOutputQueue->ESW_MMSR  = readl(fep->membase + FEC_ESW_MMSR);
	pOutputQueue->ESW_LMT   = readl(fep->membase + FEC_ESW_LMT);
	pOutputQueue->ESW_LFC   = readl(fep->membase + FEC_ESW_LFC);
	pOutputQueue->ESW_IOSR  = readl(fep->membase + FEC_ESW_IOSR);
	pOutputQueue->ESW_PCSR  = readl(fep->membase + FEC_ESW_PCSR);
	pOutputQueue->ESW_QWT   = readl(fep->membase + FEC_ESW_QWT);
	pOutputQueue->ESW_P0BCT = readl(fep->membase + FEC_ESW_P0BCT);
	return 0;
}

/* set output queue memory status and configure*/
int esw_set_output_queue_memory(struct switch_enet_private *fep,
	int fun_num, esw_output_queue_status *pOutputQueue)
{
	if (fun_num == 1) {
		/* memory manager status*/
		writel(pOutputQueue->ESW_MMSR, fep->membase + FEC_ESW_MMSR);
	} else if (fun_num == 2) {
		/*low memory threshold*/
		writel(pOutputQueue->ESW_LMT, fep->membase + FEC_ESW_LMT);
	} else if (fun_num == 3) {
		/*lowest number of free cells*/
		writel(pOutputQueue->ESW_LFC, fep->membase + FEC_ESW_LFC);
	} else if (fun_num == 4) {
		/*queue weights*/
		writel(pOutputQueue->ESW_QWT, fep->membase + FEC_ESW_QWT);
	} else if (fun_num == 5) {
		/*port 0 backpressure congenstion thresled*/
		writel(pOutputQueue->ESW_P0BCT, fep->membase + FEC_ESW_P0BCT);
	} else {
		printk(KERN_ERR "%s: do not support the cmd %x\n",
			__func__, fun_num);
		return -1;
	}
	return 0;
}

int esw_get_irq_status(struct switch_enet_private *fep,
	eswIoctlIrqStatus *pIrqStatus)
{
	pIrqStatus->isr             = readl(fep->membase + FEC_ESW_ISR);
	pIrqStatus->imr             = readl(fep->membase + FEC_ESW_IMR);
	pIrqStatus->rx_buf_pointer  = readl(fep->membase + FEC_ESW_RDSR);
	pIrqStatus->tx_buf_pointer  = readl(fep->membase + FEC_ESW_TDSR);
	pIrqStatus->rx_max_size     = readl(fep->membase + FEC_ESW_MRBR);
	pIrqStatus->rx_buf_active   = readl(fep->membase + FEC_ESW_RDAR);
	pIrqStatus->tx_buf_active   = readl(fep->membase + FEC_ESW_TDAR);
	return 0;
}

int esw_set_irq_mask(struct switch_enet_private *fep,
	unsigned long mask, int enable)
{
	u32 tmp = readl(fep->membase + FEC_ESW_IMR);

	if (enable == 1)
		tmp |= mask;
	else if (enable == 1)
		tmp &= (~mask);
	else {
		printk(KERN_INFO "%s: enable %lx is error value\n",
			__func__, mask);
		return -1;
	}
	writel(tmp, fep->membase + FEC_ESW_IMR);
	return 0;
}

void esw_clear_irq_event(struct switch_enet_private *fep,
	unsigned long mask)
{
	u32 tmp = readl(fep->membase + FEC_ESW_ISR);
	tmp |= mask;
	writel(tmp, fep->membase + FEC_ESW_ISR);
}

void esw_get_switch_mode(struct switch_enet_private *fep,
	unsigned long *ulModeConfig)
{
	*ulModeConfig = readl(fep->membase + FEC_ESW_MODE);
}

void esw_switch_mode_configure(struct switch_enet_private *fep,
	unsigned long configure)
{
	u32 tmp = readl(fep->membase + FEC_ESW_MODE);
	tmp |= configure;
	writel(tmp, fep->membase + FEC_ESW_MODE);
}

void esw_get_bridge_port(struct switch_enet_private *fep,
	unsigned long *ulBMPConfig)
{
	*ulBMPConfig = readl(fep->membase + FEC_ESW_BMPC);
}

void  esw_bridge_port_configure(struct switch_enet_private *fep,
	unsigned long configure)
{
	writel(configure, fep->membase + FEC_ESW_BMPC);
}

int esw_get_port_all_status(struct switch_enet_private *fep,
		unsigned char portnum, struct port_all_status *port_alstatus)
{
	unsigned long PortBlocking;
	unsigned long PortLearning;
	unsigned long VlanVerify;
	unsigned long DiscardUnknown;
	unsigned long MultiReso;
	unsigned long BroadReso;
	unsigned long FTransmit;
	unsigned long FReceive;

	PortBlocking = readl(fep->membase + FEC_ESW_BKLR) & 0x0000000f;
	PortLearning = (readl(fep->membase + FEC_ESW_BKLR) & 0x000f0000) >> 16;
	VlanVerify = readl(fep->membase + FEC_ESW_VLANV) & 0x0000000f;
	DiscardUnknown = (readl(fep->membase + FEC_ESW_VLANV) & 0x000f0000)
			>> 16;
	MultiReso = readl(fep->membase + FEC_ESW_DMCR) & 0x0000000f;
	BroadReso = readl(fep->membase + FEC_ESW_DBCR) & 0x0000000f;
	FTransmit = readl(fep->membase + FEC_ESW_PER) & 0x0000000f;
	FReceive = (readl(fep->membase + FEC_ESW_PER) & 0x000f0000) >> 16;

	switch (portnum) {
	case 0:
		port_alstatus->link_status = 1;
		port_alstatus->block_status = PortBlocking & 1;
		port_alstatus->learn_status = PortLearning & 1;
		port_alstatus->vlan_verify = VlanVerify & 1;
		port_alstatus->discard_unknown = DiscardUnknown & 1;
		port_alstatus->multi_reso = MultiReso & 1;
		port_alstatus->broad_reso = BroadReso & 1;
		port_alstatus->ftransmit = FTransmit & 1;
		port_alstatus->freceive = FReceive & 1;
		break;
	case 1:
		port_alstatus->link_status =
			ports_link_status.port1_link_status;
		port_alstatus->block_status = (PortBlocking >> 1) & 1;
		port_alstatus->learn_status = (PortLearning >> 1) & 1;
		port_alstatus->vlan_verify = (VlanVerify >> 1) & 1;
		port_alstatus->discard_unknown = (DiscardUnknown >> 1) & 1;
		port_alstatus->multi_reso = (MultiReso >> 1) & 1;
		port_alstatus->broad_reso = (BroadReso >> 1) & 1;
		port_alstatus->ftransmit = (FTransmit >> 1) & 1;
		port_alstatus->freceive = (FReceive >> 1) & 1;
		break;
	case 2:
		port_alstatus->link_status =
			ports_link_status.port2_link_status;
		port_alstatus->block_status = (PortBlocking >> 2) & 1;
		port_alstatus->learn_status = (PortLearning >> 2) & 1;
		port_alstatus->vlan_verify = (VlanVerify >> 2) & 1;
		port_alstatus->discard_unknown = (DiscardUnknown >> 2) & 1;
		port_alstatus->multi_reso = (MultiReso >> 2) & 1;
		port_alstatus->broad_reso = (BroadReso >> 2) & 1;
		port_alstatus->ftransmit = (FTransmit >> 2) & 1;
		port_alstatus->freceive = (FReceive >> 2) & 1;
		break;
	default:
		printk(KERN_ERR "%s:do not support the port %d",
					__func__, portnum);
		break;
	}
	return 0;
}

int esw_atable_get_entry_port_number(struct switch_enet_private *fep,
		unsigned char *mac_addr, unsigned char *port)
{
	int block_index, block_index_end, entry;
	unsigned long read_lo, read_hi;
	unsigned long mac_addr_lo, mac_addr_hi;

	mac_addr_lo = (unsigned long)((mac_addr[3]<<24) | (mac_addr[2]<<16) |
		(mac_addr[1]<<8) | mac_addr[0]);
	mac_addr_hi = (unsigned long)((mac_addr[5]<<8) | (mac_addr[4]));

	block_index = GET_BLOCK_PTR(crc8_calc(mac_addr));
	block_index_end = block_index + ATABLE_ENTRY_PER_SLOT;

	/* now search all the entries in the selected block */
	for (entry = block_index; entry < block_index_end; entry++) {
		read_atable(fep, entry, &read_lo, &read_hi);
		if ((read_lo == mac_addr_lo) &&
			((read_hi & 0x0000ffff) ==
			 (mac_addr_hi & 0x0000ffff))) {
			/* found the correct address */
			if ((read_hi & (1 << 16)) && (!(read_hi & (1 << 17))))
				*port = AT_EXTRACT_PORT(read_hi);
			break;
		} else
			*port = -1;
	}

	return 0;
}

int esw_get_mac_address_lookup_table(struct switch_enet_private *fep,
	unsigned long *tableaddr, unsigned long *dnum, unsigned long *snum)
{
	unsigned long read_lo, read_hi;
	unsigned long entry;
	unsigned long dennum = 0;
	unsigned long sennum = 0;

	for (entry = 0; entry < ESW_ATABLE_MEM_NUM_ENTRIES; entry++) {
		read_atable(fep, entry, &read_lo, &read_hi);
		if ((read_hi & (1 << 17)) && (read_hi & (1 << 16))) {
			/* static entry */
			*(tableaddr + (2047 - sennum) * 11) = entry;
			*(tableaddr + (2047 - sennum) * 11 + 2) =
				read_lo & 0x000000ff;
			*(tableaddr + (2047 - sennum) * 11 + 3) =
				(read_lo & 0x0000ff00) >> 8;
			*(tableaddr + (2047 - sennum) * 11 + 4) =
				(read_lo & 0x00ff0000) >> 16;
			*(tableaddr + (2047 - sennum) * 11 + 5) =
				(read_lo & 0xff000000) >> 24;
			*(tableaddr + (2047 - sennum) * 11 + 6) =
				read_hi & 0x000000ff;
			*(tableaddr + (2047 - sennum) * 11 + 7) =
				(read_hi & 0x0000ff00) >> 8;
			*(tableaddr + (2047 - sennum) * 11 + 8) =
				AT_EXTRACT_PORTMASK(read_hi);
			*(tableaddr + (2047 - sennum) * 11 + 9) =
				AT_EXTRACT_PRIO(read_hi);
			sennum++;
		} else if ((read_hi & (1 << 16)) && (!(read_hi & (1 << 17)))) {
			/* dynamic entry */
			*(tableaddr + dennum * 11) = entry;
			*(tableaddr + dennum * 11 + 2) = read_lo & 0xff;
			*(tableaddr + dennum * 11 + 3) =
				(read_lo & 0x0000ff00) >> 8;
			*(tableaddr + dennum * 11 + 4) =
				(read_lo & 0x00ff0000) >> 16;
			*(tableaddr + dennum * 11 + 5) =
				(read_lo & 0xff000000) >> 24;
			*(tableaddr + dennum * 11 + 6) = read_hi & 0xff;
			*(tableaddr + dennum * 11 + 7) =
				(read_hi & 0x0000ff00) >> 8;
			*(tableaddr + dennum * 11 + 8) =
				AT_EXTRACT_PORT(read_hi);
			*(tableaddr + dennum * 11 + 9) =
				AT_EXTRACT_TIMESTAMP(read_hi);
			dennum++;
		}
	}

	*dnum = dennum;
	*snum = sennum;
	return 0;
}

/* The timer should create an interrupt every 4 seconds*/
static void l2switch_aging_timer(unsigned long data)
{
	struct switch_enet_private *fep;

	fep = (struct switch_enet_private *)data;

	if (fep) {
		TIMEINCREMENT(fep->currTime);
		fep->timeChanged++;
	}

	mod_timer(&fep->timer_aging, jiffies + LEARNING_AGING_TIMER);
}

static int switch_enet_learning(void *arg)
{
	struct switch_enet_private *fep = arg;

	while (!kthread_should_stop()) {

		set_current_state(TASK_INTERRUPTIBLE);

		/* check learning record valid */
		if (readl(fep->membase + FEC_ESW_LSR))
			esw_atable_dynamicms_learn_migration(fep,
					fep->currTime);
		else
			schedule_timeout(HZ/100);
	}

	return 0;
}

static int switch_enet_ioctl(struct net_device *dev,
		struct ifreq *ifr, int cmd)
{
	struct switch_enet_private *fep = netdev_priv(dev);
	int ret = 0;

	switch (cmd) {
	case ESW_SET_PORTENABLE_CONF:
	{
		eswIoctlPortEnableConfig configData;
		ret = copy_from_user(&configData,
			ifr->ifr_data,
			sizeof(eswIoctlPortEnableConfig));
		if (ret)
			return -EFAULT;

		ret = esw_port_enable_config(fep,
			configData.port,
			configData.tx_enable,
			configData.rx_enable);
	}
		break;
	case ESW_SET_BROADCAST_CONF:
	{
		eswIoctlPortConfig configData;
		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(eswIoctlPortConfig));
		if (ret)
			return -EFAULT;

		ret = esw_port_broadcast_config(fep,
			configData.port, configData.enable);
	}
		break;

	case ESW_SET_MULTICAST_CONF:
	{
		eswIoctlPortConfig configData;
		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(eswIoctlPortConfig));
		if (ret)
			return -EFAULT;

		ret = esw_port_multicast_config(fep,
			configData.port, configData.enable);
	}
		break;

	case ESW_SET_BLOCKING_CONF:
	{
		eswIoctlPortConfig configData;
		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(eswIoctlPortConfig));

		if (ret)
			return -EFAULT;

		ret = esw_port_blocking_config(fep,
			configData.port, configData.enable);
	}
		break;

	case ESW_SET_LEARNING_CONF:
	{
		eswIoctlPortConfig configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(eswIoctlPortConfig));
		if (ret)
			return -EFAULT;

		ret = esw_port_learning_config(fep,
			configData.port, configData.enable);
	}
		break;

	case ESW_SET_PORT_ENTRY_EMPTY:
	{
		unsigned long portnum;

		ret = copy_from_user(&portnum,
			ifr->ifr_data, sizeof(portnum));
		if (ret)
			return -EFAULT;
		esw_atable_dynamicms_del_entries_for_port(fep, portnum);
	}
		break;

	case ESW_SET_OTHER_PORT_ENTRY_EMPTY:
	{
		unsigned long portnum;

		ret = copy_from_user(&portnum,
			ifr->ifr_data, sizeof(portnum));
		if (ret)
			return -EFAULT;

		esw_atable_dynamicms_del_entries_for_other_port(fep, portnum);
	}
		break;

	case ESW_SET_IP_SNOOP_CONF:
	{
		eswIoctlIpsnoopConfig configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(eswIoctlIpsnoopConfig));
		if (ret)
			return -EFAULT;

		ret = esw_ip_snoop_config(fep, configData.mode,
				configData.ip_header_protocol);
	}
		break;

	case ESW_SET_PORT_SNOOP_CONF:
	{
		eswIoctlPortsnoopConfig configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(eswIoctlPortsnoopConfig));
		if (ret)
			return -EFAULT;

		ret = esw_tcpudp_port_snoop_config(fep, configData.mode,
				configData.compare_port,
				configData.compare_num);
	}
		break;

	case ESW_SET_PORT_MIRROR_CONF_PORT_MATCH:
	{
		struct eswIoctlMirrorCfgPortMatch configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(configData));
		if (ret)
			return -EFAULT;
		ret = esw_port_mirroring_config_port_match(fep,
			configData.mirror_port, configData.port_match_en,
			configData.port);
	}
		break;

	case ESW_SET_PORT_MIRROR_CONF:
	{
		eswIoctlPortMirrorConfig configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(eswIoctlPortMirrorConfig));
		if (ret)
			return -EFAULT;

		ret = esw_port_mirroring_config(fep,
			configData.mirror_port, configData.port,
			configData.mirror_enable,
			configData.src_mac, configData.des_mac,
			configData.egress_en, configData.ingress_en,
			configData.egress_mac_src_en,
			configData.egress_mac_des_en,
			configData.ingress_mac_src_en,
			configData.ingress_mac_des_en);
	}
		break;

	case ESW_SET_PORT_MIRROR_CONF_ADDR_MATCH:
	{
		struct eswIoctlMirrorCfgAddrMatch configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(configData));
		if (ret)
			return -EFAULT;

		ret = esw_port_mirroring_config_addr_match(fep,
			configData.mirror_port, configData.addr_match_en,
			configData.mac_addr);
	}
		break;

	case ESW_SET_PIRORITY_VLAN:
	{
		eswIoctlPriorityVlanConfig configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(eswIoctlPriorityVlanConfig));
		if (ret)
			return -EFAULT;

		ret = esw_framecalssify_vlan_priority_lookup(fep,
			configData.port, configData.func_enable,
			configData.vlan_pri_table_num,
			configData.vlan_pri_table_value);
	}
		break;

	case ESW_SET_PIRORITY_IP:
	{
		eswIoctlPriorityIPConfig configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(eswIoctlPriorityIPConfig));
		if (ret)
			return -EFAULT;

		ret = esw_framecalssify_ip_priority_lookup(fep,
			configData.port, configData.func_enable,
			configData.ipv4_en, configData.ip_priority_num,
			configData.ip_priority_value);
	}
		break;

	case ESW_SET_PIRORITY_MAC:
	{
		eswIoctlPriorityMacConfig configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(eswIoctlPriorityMacConfig));
		if (ret)
			return -EFAULT;

		ret = esw_framecalssify_mac_priority_lookup(fep,
			configData.port);
	}
		break;

	case ESW_SET_PIRORITY_DEFAULT:
	{
		eswIoctlPriorityDefaultConfig configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(eswIoctlPriorityDefaultConfig));
		if (ret)
			return -EFAULT;

		ret = esw_frame_calssify_priority_init(fep,
			configData.port, configData.priority_value);
	}
		break;

	case ESW_SET_P0_FORCED_FORWARD:
	{
		eswIoctlP0ForcedForwardConfig configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(eswIoctlP0ForcedForwardConfig));
		if (ret)
			return -EFAULT;

		ret = esw_forced_forward(fep, configData.port1,
			configData.port2, configData.enable);
	}
		break;

	case ESW_SET_BRIDGE_CONFIG:
	{
		unsigned long configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(unsigned long));
		if (ret)
			return -EFAULT;

		esw_bridge_port_configure(fep, configData);
	}
		break;

	case ESW_SET_SWITCH_MODE:
	{
		unsigned long configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(unsigned long));
		if (ret)
			return -EFAULT;

		esw_switch_mode_configure(fep, configData);
	}
		break;

	case ESW_SET_OUTPUT_QUEUE_MEMORY:
	{
		eswIoctlOutputQueue configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(eswIoctlOutputQueue));
		if (ret)
			return -EFAULT;

		ret = esw_set_output_queue_memory(fep,
			configData.fun_num, &configData.sOutputQueue);
	}
		break;

	case ESW_SET_VLAN_OUTPUT_PROCESS:
	{
		eswIoctlVlanOutputConfig configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(eswIoctlVlanOutputConfig));
		if (ret)
			return -EFAULT;

		ret = esw_vlan_output_process(fep,
			configData.port, configData.mode);
	}
		break;

	case ESW_SET_VLAN_INPUT_PROCESS:
	{
		eswIoctlVlanInputConfig configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data,
			sizeof(eswIoctlVlanInputConfig));
		if (ret)
			return -EFAULT;

		ret = esw_vlan_input_process(fep, configData.port,
				configData.mode, configData.port_vlanid);
	}
		break;

	case ESW_SET_VLAN_DOMAIN_VERIFICATION:
	{
		eswIoctlVlanVerificationConfig configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data,
			sizeof(eswIoctlVlanVerificationConfig));
		if (ret)
			return -EFAULT;

		ret = esw_set_vlan_verification(
			fep, configData.port,
			configData.vlan_domain_verify_en,
			configData.vlan_discard_unknown_en);
	}
		break;

	case ESW_SET_VLAN_RESOLUTION_TABLE:
	{
		eswIoctlVlanResoultionTable configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data,
			sizeof(eswIoctlVlanResoultionTable));
		if (ret)
			return -EFAULT;

		ret = esw_set_vlan_resolution_table(
			fep, configData.port_vlanid,
			configData.vlan_domain_num,
			configData.vlan_domain_port);

	}
		break;

	case ESW_SET_VLAN_ID:
	{
		unsigned long configData;
		ret = copy_from_user(&configData, ifr->ifr_data,
				sizeof(configData));
		if (ret)
			return -EFAULT;

		ret = esw_set_vlan_id(fep, configData);
	}
		break;

	case ESW_SET_VLAN_ID_CLEARED:
	{
		unsigned long configData;
		ret = copy_from_user(&configData, ifr->ifr_data,
				sizeof(configData));
		if (ret)
			return -EFAULT;

		ret = esw_set_vlan_id_cleared(fep, configData);
	}
		break;

	case ESW_SET_PORT_IN_VLAN_ID:
	{
		eswIoctlVlanResoultionTable configData;

		ret = copy_from_user(&configData, ifr->ifr_data,
				sizeof(configData));
		if (ret)
			return -EFAULT;

		ret = esw_set_port_in_vlan_id(fep, configData);
	}
		break;

	case ESW_UPDATE_STATIC_MACTABLE:
	{
		eswIoctlUpdateStaticMACtable configData;

		ret = copy_from_user(&configData,
			ifr->ifr_data, sizeof(eswIoctlUpdateStaticMACtable));
		if (ret)
			return -EFAULT;

		ret = esw_update_atable_static(configData.mac_addr,
				configData.port, configData.priority, fep);
	}
		break;

	case ESW_CLEAR_ALL_MACTABLE:
	{
		esw_clear_atable(fep);
	}
		break;

	/*get*/
	case ESW_GET_STATISTICS_STATUS:
	{
		esw_statistics_status Statistics;
		esw_port_statistics_status PortSta;
		int i;

		ret = esw_get_statistics_status(fep, &Statistics);
		if (ret != 0) {
			printk(KERN_ERR "%s: cmd %x fail\n", __func__, cmd);
			return -1;
		}
		printk(KERN_INFO "DISCN : %10ld      DISCB : %10ld\n",
				Statistics.ESW_DISCN, Statistics.ESW_DISCB);
		printk(KERN_INFO "NDISCN: %10ld      NDISCB: %10ld\n",
				Statistics.ESW_NDISCN, Statistics.ESW_NDISCB);

		for (i = 0; i < 3; i++) {
			ret = esw_get_port_statistics_status(fep, i,
					&PortSta);
			if (ret != 0) {
				printk(KERN_ERR "%s: cmd %x fail\n",
					__func__, cmd);
				return -1;
			}
			printk(KERN_INFO "port %d:  POQC  : %ld\n",
					i, PortSta.FSL_ESW_POQC);
			printk(KERN_INFO "         PMVID : %ld\n",
					PortSta.FSL_ESW_PMVID);
			printk(KERN_INFO "	 PMVTAG: %ld\n",
					PortSta.FSL_ESW_PMVTAG);
			printk(KERN_INFO "	 PBL   : %ld\n",
					PortSta.FSL_ESW_PBL);
		}
	}
		break;

	case ESW_GET_LEARNING_CONF:
	{
		unsigned long PortLearning;

		esw_get_port_learning(fep, &PortLearning);
		ret = copy_to_user(ifr->ifr_data, &PortLearning,
			sizeof(unsigned long));
		if (ret)
			return -EFAULT;
	}
		break;

	case ESW_GET_BLOCKING_CONF:
	{
		unsigned long PortBlocking;

		esw_get_port_blocking(fep, &PortBlocking);
		ret = copy_to_user(ifr->ifr_data, &PortBlocking,
			sizeof(unsigned long));
		if (ret)
			return -EFAULT;
	}
		break;

	case ESW_GET_MULTICAST_CONF:
	{
		unsigned long PortMulticast;

		esw_get_port_multicast(fep, &PortMulticast);
		ret = copy_to_user(ifr->ifr_data, &PortMulticast,
			sizeof(unsigned long));
		if (ret)
			return -EFAULT;
	}
		break;

	case ESW_GET_BROADCAST_CONF:
	{
		unsigned long PortBroadcast;

		esw_get_port_broadcast(fep, &PortBroadcast);
		ret = copy_to_user(ifr->ifr_data, &PortBroadcast,
		sizeof(unsigned long));
		if (ret)
			return -EFAULT;
	}
		break;

	case ESW_GET_PORTENABLE_CONF:
	{
		unsigned long PortEnable;

		esw_get_port_enable(fep, &PortEnable);
		ret = copy_to_user(ifr->ifr_data, &PortEnable,
			sizeof(unsigned long));
		if (ret)
			return -EFAULT;
	}
		break;

	case ESW_GET_IP_SNOOP_CONF:
	{
		unsigned long ESW_IPSNP[8];
		int i;

		esw_get_ip_snoop_config(fep, (unsigned long *)ESW_IPSNP);
		printk(KERN_INFO "IP Protocol     Mode     Type\n");
		for (i = 0; i < 8; i++) {
			if (ESW_IPSNP[i] != 0)
				printk(KERN_INFO "%3ld             "
					"%1ld        %s\n",
					(ESW_IPSNP[i] >> 8) & 0xff,
					(ESW_IPSNP[i] >> 1) & 3,
					ESW_IPSNP[i] & 1 ? "Active" :
					"Inactive");
		}
	}
		break;

	case ESW_GET_PORT_SNOOP_CONF:
	{
		unsigned long ESW_PSNP[8];
		int i;

		esw_get_tcpudp_port_snoop_config(fep,
				(unsigned long *)ESW_PSNP);
		printk(KERN_INFO "TCP/UDP Port  SrcCompare  DesCompare  "
				"Mode  Type\n");
		for (i = 0; i < 8; i++) {
			if (ESW_PSNP[i] != 0)
				printk(KERN_INFO "%5ld         %s           "
					"%s           %1ld     %s\n",
					(ESW_PSNP[i] >> 16) & 0xffff,
					(ESW_PSNP[i] >> 4) & 1 ? "Y" : "N",
					(ESW_PSNP[i] >> 3) & 1 ? "Y" : "N",
					(ESW_PSNP[i] >> 1) & 3,
					ESW_PSNP[i] & 1 ? "Active" :
					"Inactive");
		}
	}
		break;

	case ESW_GET_PORT_MIRROR_CONF:
		esw_get_port_mirroring(fep);
		break;

	case ESW_GET_P0_FORCED_FORWARD:
	{
		unsigned long ForceForward;

		esw_get_forced_forward(fep, &ForceForward);
		ret = copy_to_user(ifr->ifr_data, &ForceForward,
			sizeof(unsigned long));
		if (ret)
			return -EFAULT;
	}
		break;

	case ESW_GET_SWITCH_MODE:
	{
		unsigned long Config;

		esw_get_switch_mode(fep, &Config);
		ret = copy_to_user(ifr->ifr_data, &Config,
			sizeof(unsigned long));
		if (ret)
			return -EFAULT;
	}
		break;

	case ESW_GET_BRIDGE_CONFIG:
	{
		unsigned long Config;

		esw_get_bridge_port(fep, &Config);
		ret = copy_to_user(ifr->ifr_data, &Config,
			sizeof(unsigned long));
		if (ret)
			return -EFAULT;
	}
		break;
	case ESW_GET_OUTPUT_QUEUE_STATUS:
	{
		esw_output_queue_status Config;
		esw_get_output_queue_status(fep,
			&Config);
		ret = copy_to_user(ifr->ifr_data, &Config,
			sizeof(esw_output_queue_status));
		if (ret)
			return -EFAULT;
	}
		break;

	case ESW_GET_VLAN_OUTPUT_PROCESS:
	{
		unsigned long Config;
		int tmp;
		int i;

		esw_get_vlan_output_config(fep, &Config);

		for (i = 0; i < 3; i++) {
			tmp = (Config >> (i << 1)) & 3;

			if (tmp != 0)
				printk(KERN_INFO "port %d: vlan output "
					"manipulation enable (mode %d)\n",
					i, tmp);
			else
				printk(KERN_INFO "port %d: vlan output "
					"manipulation disable\n", i);
		}
	}
		break;

	case ESW_GET_VLAN_INPUT_PROCESS:
	{
		eswIoctlVlanInputStatus Config;
		int i;

		esw_get_vlan_input_config(fep, &Config);

		for (i = 0; i < 3; i++) {
			if (((Config.ESW_VIMEN >> i) & 1) == 0)
				printk(KERN_INFO "port %d: vlan input "
						"manipulation disable\n", i);
			else
				printk("port %d: vlan input manipulation enable"
					" (mode %ld, vlan id %ld)\n", i,
					(((Config.ESW_VIMSEL >> (i << 1)) & 3)
					 + 1), Config.ESW_PID[i]);
		}
	}
		break;

	case ESW_GET_VLAN_RESOLUTION_TABLE:
	{
		struct eswVlanTableItem vtableitem;
		unsigned char tmp0, tmp1, tmp2;
		int i;

		esw_get_vlan_resolution_table(fep, &vtableitem);

		printk(KERN_INFO "VLAN Name      VLAN Id      Ports\n");
		for (i = 0; i < vtableitem.valid_num; i++) {
			tmp0 = vtableitem.table[i].vlan_domain_port & 1;
			tmp1 = (vtableitem.table[i].vlan_domain_port >> 1) & 1;
			tmp2 = (vtableitem.table[i].vlan_domain_port >> 2) & 1;
			printk(KERN_INFO "%2d             %4d         %s%s%s\n",
				i, vtableitem.table[i].port_vlanid,
				tmp0 ? "0 " : "", tmp1 ? "1 " : "",
				tmp2 ? "2" : "");
		}
	}
		break;

	case ESW_GET_VLAN_DOMAIN_VERIFICATION:
	{
		unsigned long Config;

		esw_get_vlan_verification(fep, &Config);
		ret = copy_to_user(ifr->ifr_data, &Config,
			sizeof(unsigned long));
		if (ret)
			return -EFAULT;
	}
		break;

	case ESW_GET_ENTRY_PORT_NUMBER:
	{
		unsigned char mac_addr[6];
		unsigned char portnum;

		ret = copy_from_user(mac_addr,
			ifr->ifr_data, sizeof(mac_addr));
		if (ret)
			return -EFAULT;

		ret = esw_atable_get_entry_port_number(fep, mac_addr,
				&portnum);

		ret = copy_to_user(ifr->ifr_data, &portnum,
				sizeof(unsigned char));
		if (ret)
			return -EFAULT;
	}
		break;

	case ESW_GET_LOOKUP_TABLE:
	{
		unsigned long *ConfigData;
		unsigned long dennum, sennum;
		int i;
		int tmp;

		ConfigData = kmalloc(sizeof(struct eswAddrTableEntryExample) *
				ESW_ATABLE_MEM_NUM_ENTRIES, GFP_KERNEL);
		ret = esw_get_mac_address_lookup_table(fep, ConfigData,
				&dennum, &sennum);
		printk(KERN_INFO "Dynamic entries number: %ld\n", dennum);
		printk(KERN_INFO "Static entries number: %ld\n", sennum);
		printk(KERN_INFO "Type      MAC address         Port   Timestamp\n");
		for (i = 0; i < dennum; i++) {
			printk(KERN_INFO "dynamic   "
				"%02lx-%02lx-%02lx-%02lx-%02lx-%02lx   "
				"%01lx      %4ld\n", *(ConfigData + i * 11 + 2),
				*(ConfigData + i * 11 + 3),
				*(ConfigData + i * 11 + 4),
				*(ConfigData + i * 11 + 5),
				*(ConfigData + i * 11 + 6),
				*(ConfigData + i * 11 + 7),
				*(ConfigData + i * 11 + 8),
				*(ConfigData + i * 11 + 9));
		}

		if (sennum != 0)
			printk(KERN_INFO "Type      MAC address"
					"         Port   Priority\n");

		for (i = 0; i < sennum; i++) {
			printk(KERN_INFO "static    %02lx-%02lx-%02lx-%02lx"
					"-%02lx-%02lx   ",
					*(ConfigData + (2047 - i) * 11 + 2),
					*(ConfigData + (2047 - i) * 11 + 3),
					*(ConfigData + (2047 - i) * 11 + 4),
					*(ConfigData + (2047 - i) * 11 + 5),
					*(ConfigData + (2047 - i) * 11 + 6),
					*(ConfigData + (2047 - i) * 11 + 7));

			tmp = *(ConfigData + (2047 - i) * 11 + 8);
			if ((tmp == 0) || (tmp == 2) || (tmp == 4))
				printk("%01x      ", tmp >> 1);
			else if (tmp == 3)
				printk("0,1    ");
			else if (tmp == 5)
				printk("0,2    ");
			else if (tmp == 6)
				printk("1,2    ");

			printk("%4ld\n", *(ConfigData + (2047 - i) * 11 + 9));
		}
		kfree(ConfigData);
	}
		break;

	case ESW_GET_PORT_STATUS:
	{
		unsigned long PortBlocking;

		esw_get_port_blocking(fep, &PortBlocking);

		ports_link_status.port0_block_status = PortBlocking & 1;
		ports_link_status.port1_block_status = (PortBlocking >> 1) & 1;
		ports_link_status.port2_block_status = PortBlocking >> 2;

		ret = copy_to_user(ifr->ifr_data, &ports_link_status,
				sizeof(ports_link_status));
		if (ret)
			return -EFAULT;
	}
		break;

	case ESW_GET_PORT_ALL_STATUS:
	{
		unsigned char portnum;
		struct port_all_status port_astatus;

		ret = copy_from_user(&portnum,
			ifr->ifr_data, sizeof(portnum));
		if (ret)
			return -EFAULT;

		esw_get_port_all_status(fep, portnum, &port_astatus);
		printk(KERN_INFO "Port %d status:\n", portnum);
		printk(KERN_INFO "Link:%-4s          Blocking:%1s          "
			"Learning:%1s\n",
			port_astatus.link_status ? "Up" : "Down",
			port_astatus.block_status ? "Y" : "N",
			port_astatus.learn_status ? "N" : "Y");
		printk(KERN_INFO "VLAN Verify:%1s      Discard Unknown:%1s   "
			"Multicast Res:%1s\n",
			port_astatus.vlan_verify ? "Y" : "N",
			port_astatus.discard_unknown ? "Y" : "N",
			port_astatus.multi_reso ? "Y" : "N");
		printk(KERN_INFO "Broadcast Res:%1s    Transmit:%-7s    "
			"Receive:%7s\n",
			port_astatus.broad_reso ? "Y" : "N",
			port_astatus.ftransmit ? "Enable" : "Disable",
			port_astatus.freceive ? "Enable" : "Disable");

	}
		break;

	case ESW_GET_USER_PID:
	{
		long get_pid = 0;
		ret = copy_from_user(&get_pid,
			ifr->ifr_data, sizeof(get_pid));

		if (ret)
			return -EFAULT;
		user_pid = get_pid;
	}
		break;

	default:
		return -EOPNOTSUPP;
	}

	return ret;
}

static netdev_tx_t switch_enet_start_xmit(struct sk_buff *skb,
				struct net_device *dev)
{
	struct switch_enet_private *fep;
	cbd_t	*bdp;
	unsigned short	status;
	unsigned long flags;
	void *bufaddr;

	fep = netdev_priv(dev);

	spin_lock_irqsave(&fep->hw_lock, flags);
	/* Fill in a Tx ring entry */
	bdp = fep->cur_tx;

	status = bdp->cbd_sc;

	/* Clear all of the status flags */
	status &= ~BD_ENET_TX_STATS;

	/* Set buffer length and buffer pointer.
	*/
	bufaddr = skb->data;
	bdp->cbd_datlen = skb->len;

	/*
	 *	On some FEC implementations data must be aligned on
	 *	4-byte boundaries. Use bounce buffers to copy data
	 *	and get it aligned. Ugh.
	 */
	if (((unsigned long)bufaddr) & 0xf) {
		unsigned int index;
		index = bdp - fep->tx_bd_base;
		memcpy(fep->tx_bounce[index],
		       (void *)skb->data, bdp->cbd_datlen);
		bufaddr = fep->tx_bounce[index];
	}

	/* Save skb pointer. */
	fep->tx_skbuff[fep->skb_cur] = skb;

	dev->stats.tx_bytes += skb->len;
	fep->skb_cur = (fep->skb_cur+1) & TX_RING_MOD_MASK;

	/* Push the data cache so the CPM does not get stale memory
	 * data.
	 */
	bdp->cbd_bufaddr = dma_map_single(&fep->pdev->dev, bufaddr,
			SWITCH_ENET_TX_FRSIZE, DMA_TO_DEVICE);

	/* Send it on its way.  Tell FEC it's ready, interrupt when done,
	 * it's the last BD of the frame, and to put the CRC on the end.
	 */
	status |= (BD_ENET_TX_READY | BD_ENET_TX_INTR
			| BD_ENET_TX_LAST | BD_ENET_TX_TC);
	bdp->cbd_sc = status;

	dev->trans_start = jiffies;

	/* Trigger transmission start */
	writel(FSL_ESW_TDAR_X_DES_ACTIVE, fep->membase + FEC_ESW_TDAR);

	/* If this was the last BD in the ring,
	 * start at the beginning again.*/
	if (status & BD_ENET_TX_WRAP)
		bdp = fep->tx_bd_base;
	else
		bdp++;

	if (bdp == fep->dirty_tx) {
		fep->tx_full = 1;
		netif_stop_queue(dev);
		printk(KERN_ERR "%s:  net stop\n", __func__);
	}

	fep->cur_tx = (cbd_t *)bdp;

	spin_unlock_irqrestore(&fep->hw_lock, flags);

	return NETDEV_TX_OK;
}

static void switch_timeout(struct net_device *dev)
{
	struct switch_enet_private *fep = netdev_priv(dev);

	printk(KERN_ERR "%s: transmit timed out.\n", dev->name);
	dev->stats.tx_errors++;
	switch_restart(dev, fep->full_duplex);
	netif_wake_queue(dev);
}

/* The interrupt handler.
 * This is called from the MPC core interrupt.
 */
static irqreturn_t switch_enet_interrupt(int irq, void *dev_id)
{
	struct	net_device *dev = dev_id;
	struct switch_enet_private *fep = netdev_priv(dev);
	uint	int_events;
	irqreturn_t ret = IRQ_NONE;

	/* Get the interrupt events that caused us to be here.
	*/
	do {
		int_events = readl(fep->membase + FEC_ESW_ISR);
		writel(int_events, fep->membase + FEC_ESW_ISR);

		/* Handle receive event in its own function. */

		if (int_events & FSL_ESW_ISR_RXF) {
			ret = IRQ_HANDLED;
			switch_enet_rx(dev);
		}

		if (int_events & FSL_ESW_ISR_TXF) {
			ret = IRQ_HANDLED;
			switch_enet_tx(dev);
		}

	} while (int_events);

	return ret;
}

static void switch_enet_tx(struct net_device *dev)
{
	struct	switch_enet_private *fep;
	struct bufdesc *bdp;
	unsigned short status;
	struct	sk_buff	*skb;
	unsigned long flags;

	fep = netdev_priv(dev);
	spin_lock_irqsave(&fep->hw_lock, flags);
	bdp = fep->dirty_tx;

	while (((status = bdp->cbd_sc) & BD_ENET_TX_READY) == 0) {
		if (bdp == fep->cur_tx && fep->tx_full == 0)
			break;

		if (bdp->cbd_bufaddr)
			dma_unmap_single(&fep->pdev->dev, bdp->cbd_bufaddr,
				SWITCH_ENET_TX_FRSIZE, DMA_TO_DEVICE);
		bdp->cbd_bufaddr = 0;

		skb = fep->tx_skbuff[fep->skb_dirty];
		/* Check for errors. */
		if (status & (BD_ENET_TX_HB | BD_ENET_TX_LC |
				   BD_ENET_TX_RL | BD_ENET_TX_UN |
				   BD_ENET_TX_CSL)) {
			dev->stats.tx_errors++;
			if (status & BD_ENET_TX_HB)  /* No heartbeat */
				dev->stats.tx_heartbeat_errors++;
			if (status & BD_ENET_TX_LC)  /* Late collision */
				dev->stats.tx_window_errors++;
			if (status & BD_ENET_TX_RL)  /* Retrans limit */
				dev->stats.tx_aborted_errors++;
			if (status & BD_ENET_TX_UN)  /* Underrun */
				dev->stats.tx_fifo_errors++;
			if (status & BD_ENET_TX_CSL) /* Carrier lost */
				dev->stats.tx_carrier_errors++;
		} else {
			dev->stats.tx_packets++;
		}

		/* Deferred means some collisions occurred during transmit,
		 * but we eventually sent the packet OK.
		 */
		if (status & BD_ENET_TX_DEF)
			dev->stats.collisions++;

		/* Free the sk buffer associated with this last transmit.
		 */
		dev_kfree_skb_any(skb);
		fep->tx_skbuff[fep->skb_dirty] = NULL;
		fep->skb_dirty = (fep->skb_dirty + 1) & TX_RING_MOD_MASK;

		/* Update pointer to next buffer descriptor to be transmitted.
		 */
		if (status & BD_ENET_TX_WRAP)
			bdp = fep->tx_bd_base;
		else
			bdp++;

		/* Since we have freed up a buffer, the ring is no longer
		 * full.
		 */
		if (fep->tx_full) {
			fep->tx_full = 0;
			printk(KERN_ERR "%s: tx full is zero\n", __func__);
			if (netif_queue_stopped(dev))
				netif_wake_queue(dev);
		}
	}
	fep->dirty_tx = (cbd_t *)bdp;
	spin_unlock_irqrestore(&fep->hw_lock, flags);
}


/* During a receive, the cur_rx points to the current incoming buffer.
 * When we update through the ring, if the next incoming buffer has
 * not been given to the system, we just set the empty indicator,
 * effectively tossing the packet.
 */
static void switch_enet_rx(struct net_device *dev)
{
	struct	switch_enet_private *fep;
	cbd_t *bdp;
	unsigned short status;
	struct	sk_buff	*skb;
	ushort	pkt_len;
	__u8 *data;
	unsigned long flags;

	fep = netdev_priv(dev);

	spin_lock_irqsave(&fep->hw_lock, flags);
	/* First, grab all of the stats for the incoming packet.
	 * These get messed up if we get called due to a busy condition.
	 */
	bdp = fep->cur_rx;

	while (!((status = bdp->cbd_sc) & BD_ENET_RX_EMPTY)) {

		/* Since we have allocated space to hold a complete frame,
		 * the last indicator should be set.
		 * */
		if ((status & BD_ENET_RX_LAST) == 0)
			printk(KERN_ERR "SWITCH ENET: rcv is not +last\n");

		if (!fep->opened)
			goto rx_processing_done;

		/* Check for errors. */
		if (status & (BD_ENET_RX_LG | BD_ENET_RX_SH | BD_ENET_RX_NO |
			   BD_ENET_RX_CR | BD_ENET_RX_OV)) {
			dev->stats.rx_errors++;
			if (status & (BD_ENET_RX_LG | BD_ENET_RX_SH)) {
				/* Frame too long or too short. */
				dev->stats.rx_length_errors++;
			}
			if (status & BD_ENET_RX_NO)	/* Frame alignment */
				dev->stats.rx_frame_errors++;
			if (status & BD_ENET_RX_CR)	/* CRC Error */
				dev->stats.rx_crc_errors++;
			if (status & BD_ENET_RX_OV)	/* FIFO overrun */
				dev->stats.rx_fifo_errors++;
		}
		/* Report late collisions as a frame error.
		 * On this error, the BD is closed, but we don't know what we
		 * have in the buffer.  So, just drop this frame on the floor.
		 * */
		if (status & BD_ENET_RX_CL) {
			dev->stats.rx_errors++;
			dev->stats.rx_frame_errors++;
			goto rx_processing_done;
		}
		/* Process the incoming frame */
		dev->stats.rx_packets++;
		pkt_len = bdp->cbd_datlen;
		dev->stats.rx_bytes += pkt_len;
		data = (__u8 *)__va(bdp->cbd_bufaddr);

		if (bdp->cbd_bufaddr)
			dma_unmap_single(&fep->pdev->dev, bdp->cbd_bufaddr,
				SWITCH_ENET_TX_FRSIZE, DMA_FROM_DEVICE);

		/* This does 16 byte alignment, exactly what we need.
		 * The packet length includes FCS, but we don't want to
		 * include that when passing upstream as it messes up
		 * bridging applications.
		 * */
		skb = dev_alloc_skb(pkt_len - 4 + NET_IP_ALIGN);

		if (skb == NULL)
			dev->stats.rx_dropped++;

		if (unlikely(!skb)) {
			printk(KERN_ERR "%s: Memory squeeze, dropping packet.\n",
					dev->name);
			dev->stats.rx_dropped++;
		} else {
			skb_reserve(skb, NET_IP_ALIGN);
			skb_put(skb, pkt_len - 4);	/* Make room */
			skb_copy_to_linear_data(skb, data, pkt_len - 4);
			skb->protocol = eth_type_trans(skb, dev);
			netif_rx(skb);
		}

		bdp->cbd_bufaddr = dma_map_single(&fep->pdev->dev, data,
			SWITCH_ENET_TX_FRSIZE, DMA_FROM_DEVICE);
rx_processing_done:

		/* Clear the status flags for this buffer */
		status &= ~BD_ENET_RX_STATS;

		/* Mark the buffer empty */
		status |= BD_ENET_RX_EMPTY;
		bdp->cbd_sc = status;

		/* Update BD pointer to next entry */
		if (status & BD_ENET_RX_WRAP)
			bdp = fep->rx_bd_base;
		else
			bdp++;

		/* Doing this here will keep the FEC running while we process
		 * incoming frames.  On a heavily loaded network, we should be
		 * able to keep up at the expense of system resources.
		 * */
		writel(FSL_ESW_RDAR_R_DES_ACTIVE, fep->membase + FEC_ESW_RDAR);
	}
	fep->cur_rx = (cbd_t *)bdp;

	spin_unlock_irqrestore(&fep->hw_lock, flags);
}

static int fec_mdio_transfer(struct mii_bus *bus, int phy_id,
	int reg, int regval)
{
	unsigned long   flags;
	struct switch_enet_private *fep = bus->priv;
	int retval = 0;
	int tries = 100;

	spin_lock_irqsave(&fep->mii_lock, flags);

	fep->mii_timeout = 0;
	init_completion(&fep->mdio_done);

	regval |= phy_id << 23;
	writel(regval, fep->enetbase + FSL_FEC_MMFR0);

	/* wait for it to finish, this takes about 23 us on lite5200b */
	while (!(readl(fep->enetbase + FSL_FEC_EIR0) & FEC_ENET_MII) && --tries)
		udelay(5);
	if (!tries) {
		printk(KERN_ERR "%s timeout\n", __func__);
		return -ETIMEDOUT;
	}

	writel(FEC_ENET_MII, fep->enetbase + FSL_FEC_EIR0);
	retval = (readl(fep->enetbase + FSL_FEC_MMFR0) & 0xffff);
	spin_unlock_irqrestore(&fep->mii_lock, flags);

	return retval;
}

static int fec_enet_mdio_read(struct mii_bus *bus,
	int phy_id, int reg)
{
	int ret;
	ret = fec_mdio_transfer(bus, phy_id, reg,
		mk_mii_read(reg));
	return ret;
}

static int fec_enet_mdio_write(struct mii_bus *bus,
	int phy_id, int reg, u16 data)
{
	return fec_mdio_transfer(bus, phy_id, reg,
			mk_mii_write(reg, data));
}

static void switch_adjust_link1(struct net_device *dev)
{
	struct switch_enet_private *priv = netdev_priv(dev);
	struct phy_device *phydev1 = priv->phydev[0];
	int new_state = 0;

	if (phydev1->link != PHY_DOWN) {
		if (phydev1->duplex != priv->phy1_duplex) {
			new_state = 1;
			priv->phy1_duplex = phydev1->duplex;
		}

		if (phydev1->speed != priv->phy1_speed) {
			new_state = 1;
			priv->phy1_speed = phydev1->speed;
		}

		if (priv->phy1_old_link == PHY_DOWN) {
			new_state = 1;
			priv->phy1_old_link = phydev1->link;
		}
	} else if (priv->phy1_old_link) {
		new_state = 1;
		priv->phy1_old_link = PHY_DOWN;
		priv->phy1_speed = 0;
		priv->phy1_duplex = -1;
	}

	if (new_state) {
		ports_link_status.port1_link_status = phydev1->link;
		if (phydev1->link == PHY_DOWN)
			esw_atable_dynamicms_del_entries_for_port(priv, 1);

		/*Send the new status to user space*/
		if (user_pid != 1)
			sys_tkill(user_pid, SIGUSR1);
		phy_print_status(phydev1);
	}
}

static void switch_adjust_link2(struct net_device *dev)
{
	struct switch_enet_private *priv = netdev_priv(dev);
	struct phy_device *phydev2 = priv->phydev[1];
	int new_state = 0;

	if (phydev2->link != PHY_DOWN) {
		if (phydev2->duplex != priv->phy2_duplex) {
			new_state = 1;
			priv->phy2_duplex = phydev2->duplex;
		}

		if (phydev2->speed != priv->phy2_speed) {
			new_state = 1;
			priv->phy2_speed = phydev2->speed;
		}

		if (priv->phy2_old_link == PHY_DOWN) {
			new_state = 1;
			priv->phy2_old_link = phydev2->link;
		}
	} else if (priv->phy2_old_link) {
		new_state = 1;
		priv->phy2_old_link = PHY_DOWN;
		priv->phy2_speed = 0;
		priv->phy2_duplex = -1;
	}

	if (new_state) {
		ports_link_status.port2_link_status = phydev2->link;
		if (phydev2->link == PHY_DOWN)
			esw_atable_dynamicms_del_entries_for_port(priv, 2);

		/*Send the new status to user space*/
		if (user_pid != 1)
			sys_tkill(user_pid, SIGUSR1);
		phy_print_status(phydev2);
	}
}

static int switch_init_phy(struct net_device *dev)
{
	struct switch_enet_private *priv = netdev_priv(dev);
	struct phy_device *phydev[SWITCH_EPORT_NUMBER] = {NULL, NULL};
	int i, j = 0;

	/* search for connect PHY device */
	for (i = 0; i < PHY_MAX_ADDR; i++) {
		struct phy_device *const tmp_phydev =
			priv->mdio_bus->phy_map[i];

		if (!tmp_phydev)
			continue;

		phydev[j++] = tmp_phydev;
		if (j >= SWITCH_EPORT_NUMBER)
			break;
	}

	/* now we are supposed to have a proper phydev, to attach to... */
	if ((!phydev[0]) && (!phydev[1])) {
		printk(KERN_INFO "%s: Don't found any phy device at all\n",
			dev->name);
		return -ENODEV;
	}

	priv->phy1_link = PHY_DOWN;
	priv->phy1_old_link = PHY_DOWN;
	priv->phy1_speed = 0;
	priv->phy1_duplex = -1;

	priv->phy2_link = PHY_DOWN;
	priv->phy2_old_link = PHY_DOWN;
	priv->phy2_speed = 0;
	priv->phy2_duplex = -1;

	phydev[0] = phy_connect(dev, dev_name(&phydev[0]->dev),
		&switch_adjust_link1, PHY_INTERFACE_MODE_RMII);
	if (IS_ERR(phydev[0])) {
		printk(KERN_ERR " %s phy_connect failed\n", __func__);
		return PTR_ERR(phydev[0]);
	}

	phydev[1] = phy_connect(dev, dev_name(&phydev[1]->dev),
		&switch_adjust_link2, PHY_INTERFACE_MODE_RMII);
	if (IS_ERR(phydev[1])) {
		printk(KERN_ERR " %s phy_connect failed\n", __func__);
		return PTR_ERR(phydev[1]);
	}

	for (i = 0; i < SWITCH_EPORT_NUMBER; i++) {
		phydev[i]->supported &= PHY_BASIC_FEATURES;
		phydev[i]->advertising = phydev[i]->supported;
		priv->phydev[i] = phydev[i];
		printk(KERN_INFO "attached phy %i to driver %s "
			"(mii_bus:phy_addr=%s, irq=%d)\n",
			phydev[i]->addr, phydev[i]->drv->name,
			dev_name(&priv->phydev[i]->dev),
			priv->phydev[i]->irq);
	}

	return 0;
}

static void switch_enet_free_buffers(struct net_device *ndev)
{
	struct switch_enet_private *fep = netdev_priv(ndev);
	int i;
	struct sk_buff *skb;
	cbd_t	*bdp;

	bdp = fep->rx_bd_base;
	for (i = 0; i < RX_RING_SIZE; i++) {
		skb = fep->rx_skbuff[i];

		if (bdp->cbd_bufaddr)
			dma_unmap_single(&fep->pdev->dev, bdp->cbd_bufaddr,
					SWITCH_ENET_RX_FRSIZE, DMA_FROM_DEVICE);
		if (skb)
			dev_kfree_skb(skb);
		bdp++;
	}

	bdp = fep->tx_bd_base;
	for (i = 0; i < TX_RING_SIZE; i++)
		kfree(fep->tx_bounce[i]);
}

static int switch_alloc_buffers(struct net_device *ndev)
{
	struct switch_enet_private *fep = netdev_priv(ndev);
	int i;
	struct sk_buff *skb;
	cbd_t	*bdp;

	bdp = fep->rx_bd_base;
	for (i = 0; i < RX_RING_SIZE; i++) {
		skb = dev_alloc_skb(SWITCH_ENET_RX_FRSIZE);
		if (!skb) {
			switch_enet_free_buffers(ndev);
			return -ENOMEM;
		}
		fep->rx_skbuff[i] = skb;

		bdp->cbd_bufaddr = dma_map_single(&fep->pdev->dev, skb->data,
				SWITCH_ENET_RX_FRSIZE, DMA_FROM_DEVICE);
		bdp->cbd_sc = BD_ENET_RX_EMPTY;

		bdp++;
	}

	/* Set the last buffer to wrap. */
	bdp--;
	bdp->cbd_sc |= BD_SC_WRAP;

	bdp = fep->tx_bd_base;
	for (i = 0; i < TX_RING_SIZE; i++) {
		fep->tx_bounce[i] = kmalloc(SWITCH_ENET_TX_FRSIZE, GFP_KERNEL);

		bdp->cbd_sc = 0;
		bdp->cbd_bufaddr = 0;
		bdp++;
	}

	/* Set the last buffer to wrap. */
	bdp--;
	bdp->cbd_sc |= BD_SC_WRAP;

	return 0;
}

static int switch_enet_open(struct net_device *dev)
{
	struct switch_enet_private *fep = netdev_priv(dev);
	int i;

	fep->phy1_link = 0;
	fep->phy2_link = 0;

	switch_init_phy(dev);
	for (i = 0; i < SWITCH_EPORT_NUMBER; i++) {
		phy_write(fep->phydev[i], MII_BMCR, BMCR_RESET);
		udelay(10);
		phy_start(fep->phydev[i]);
	}

	fep->phy1_old_link = 0;
	fep->phy2_old_link = 0;
	fep->phy1_link = 1;
	fep->phy2_link = 1;

	/* no phy,  go full duplex,  it's most likely a hub chip */
	switch_restart(dev, 1);

	/* if the fec open firstly, we need to do nothing*/
	/* otherwise, we need to restart the FEC*/
	if (fep->sequence_done == 0)
		switch_restart(dev, 1);
	else
		fep->sequence_done = 0;

	fep->currTime = 0;
	fep->learning_irqhandle_enable = 0;

	writel(0x70007, fep->membase + FEC_ESW_PER);
	writel(FSL_ESW_DBCR_P0 | FSL_ESW_DBCR_P1 | FSL_ESW_DBCR_P2,
			fep->membase + FEC_ESW_DBCR);
	writel(FSL_ESW_DMCR_P0 | FSL_ESW_DMCR_P1 | FSL_ESW_DMCR_P2,
			fep->membase + FEC_ESW_DMCR);

	writel(0,fep->membase + FEC_ESW_BKLR);

	netif_start_queue(dev);

	/* And last, enable the receive processing.*/
	writel(FSL_ESW_RDAR_R_DES_ACTIVE, fep->membase + FEC_ESW_RDAR);

	fep->opened = 1;

	return 0;
}

static int switch_enet_close(struct net_device *dev)
{
	struct switch_enet_private *fep = netdev_priv(dev);
	int i;

	/* Don't know what to do yet.*/
	fep->opened = 0;
	netif_stop_queue(dev);
	switch_stop(dev);

	for (i = 0; i < SWITCH_EPORT_NUMBER; i++) {
		phy_disconnect(fep->phydev[i]);
		phy_stop(fep->phydev[i]);
		phy_write(fep->phydev[i], MII_BMCR, BMCR_PDOWN);
	}

	return 0;
}

#define HASH_BITS	6		/* #bits in hash */
#define CRC32_POLY	0xEDB88320
#ifdef UNUSED
static void set_multicast_list(struct net_device *dev)
{
	struct switch_enet_private *fep;
	unsigned int i, bit, data, crc;
	struct netdev_hw_addr *ha;

	fep = netdev_priv(dev);

	if (dev->flags & IFF_PROMISC) {
		printk(KERN_INFO "%s IFF_PROMISC\n", __func__);
	} else {
		if (dev->flags & IFF_ALLMULTI)
			/* Catch all multicast addresses, so set the
			 * filter to all 1's.
			 */
			printk(KERN_INFO "%s IFF_ALLMULTI\n", __func__);
		else {
			netdev_for_each_mc_addr(ha, dev) {
				if (!(ha->addr[0] & 1))
					continue;

				/* calculate crc32 value of mac address
				*/
				crc = 0xffffffff;

				for (i = 0; i < dev->addr_len; i++) {
					data = ha->addr[i];
					for (bit = 0; bit < 8; bit++,
						data >>= 1) {
						crc = (crc >> 1) ^
						(((crc ^ data) & 1) ?
						CRC32_POLY : 0);
					}
				}

			}
		}
	}
}
#endif

/* Set a MAC change in hardware.*/
static void switch_get_mac_address(struct net_device *dev)
{
	struct switch_enet_private *fep = netdev_priv(dev);
	struct switch_platform_data *pdata = fep->pdev->dev.platform_data;
	unsigned char *iap, tmpaddr[ETH_ALEN];

	iap = macaddr;

	if (!is_valid_ether_addr(iap))
		if (pdata)
			memcpy(iap, pdata->mac, ETH_ALEN);

	if (!is_valid_ether_addr(iap)) {
		*((unsigned long *) &tmpaddr[0]) =
			be32_to_cpu(readl(fep->enetbase + FSL_FEC_PALR0));
		*((unsigned short *) &tmpaddr[4]) =
			be16_to_cpu(readl(fep->enetbase + FSL_FEC_PAUR0) >> 16);
		iap = &tmpaddr[0];
	}

	memcpy(dev->dev_addr, iap, ETH_ALEN);

	/* Adjust MAC if using macaddr */
	if (iap == macaddr)
		dev->dev_addr[ETH_ALEN-1] =
			macaddr[ETH_ALEN-1] + fep->pdev->id;
}

static void switch_hw_init(struct net_device *dev)
{
	struct switch_enet_private *fep = netdev_priv(dev);

	/* Initialize MAC 0/1 */
	writel(FSL_FEC_RCR_MAX_FL(1522) | FSL_FEC_RCR_RMII_MODE | FSL_FEC_RCR_PROM
			| FSL_FEC_RCR_MII_MODE | FSL_FEC_RCR_MII_MODE,
			fep->enetbase + FSL_FEC_RCR0);
	writel(FSL_FEC_RCR_MAX_FL(1522) | FSL_FEC_RCR_RMII_MODE | FSL_FEC_RCR_PROM
			| FSL_FEC_RCR_MII_MODE | FSL_FEC_RCR_MII_MODE,
			fep->enetbase + FSL_FEC_RCR1);

	writel(FSL_FEC_TCR_FDEN, fep->enetbase + FSL_FEC_TCR0);
	writel(FSL_FEC_TCR_FDEN, fep->enetbase + FSL_FEC_TCR1);

	writel(0x1a, fep->enetbase + FSL_FEC_MSCR0);

	/* Set the station address for the ENET Adapter */
	writel(dev->dev_addr[3] |
		dev->dev_addr[2] << 8 |
		dev->dev_addr[1] << 16 |
		dev->dev_addr[0] << 24, fep->enetbase + FSL_FEC_PALR0);
	writel((dev->dev_addr[5] << 16) |
		(dev->dev_addr[4] << 24),
		fep->enetbase + FSL_FEC_PAUR0);
	writel(dev->dev_addr[3] |
		dev->dev_addr[2] << 8 |
		dev->dev_addr[1] << 16 |
		dev->dev_addr[0] << 24, fep->enetbase + FSL_FEC_PALR1);
	writel((dev->dev_addr[5] << 16) |
		(dev->dev_addr[4] << 24),
		fep->enetbase + FSL_FEC_PAUR1);

	writel(FEC_ENET_TXF | FEC_ENET_RXF, fep->enetbase + FSL_FEC_EIMR0);
	writel(FEC_ENET_TXF | FEC_ENET_RXF, fep->enetbase + FSL_FEC_EIMR1);

	writel(FSL_FEC_ECR_ETHER_EN | (0x1 << 8), fep->enetbase + FSL_FEC_ECR0);
	writel(FSL_FEC_ECR_ETHER_EN | (0x1 << 8), fep->enetbase + FSL_FEC_ECR1);
	udelay(20);
}

static const struct net_device_ops switch_netdev_ops = {
	.ndo_open		= switch_enet_open,
	.ndo_stop		= switch_enet_close,
	.ndo_start_xmit		= switch_enet_start_xmit,
	.ndo_do_ioctl		= switch_enet_ioctl,
	.ndo_tx_timeout		= switch_timeout,
};

/* Initialize the FEC Ethernet.
 */
 /*
  * XXX:  We need to clean up on failure exits here.
  */
static int switch_enet_init(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct switch_enet_private *fep = netdev_priv(dev);
	cbd_t *cbd_base;
	int ret = 0;

	/* Allocate memory for buffer descriptors. */
	cbd_base = dma_alloc_coherent(NULL, PAGE_SIZE, &fep->bd_dma,
			GFP_KERNEL);
	if (!cbd_base) {
		printk(KERN_ERR "FEC: allocate descriptor memory failed?\n");
		return -ENOMEM;
	}

	spin_lock_init(&fep->hw_lock);
	spin_lock_init(&fep->mii_lock);

	writel(FSL_ESW_MODE_SW_RST, fep->membase + FEC_ESW_MODE);
	udelay(10);
	writel(FSL_ESW_MODE_STATRST, fep->membase + FEC_ESW_MODE);
	writel(FSL_ESW_MODE_SW_EN, fep->membase + FEC_ESW_MODE);

	/* Enable transmit/receive on all ports */
	writel(0xffffffff, fep->membase + FEC_ESW_PER);

	/* Management port configuration,
	 * make port 0 as management port */
	writel(0, fep->membase + FEC_ESW_BMPC);

	/* Clear any outstanding interrupt.*/
	writel(0xffffffff, fep->membase + FEC_ESW_ISR);
	writel(0, fep->membase + FEC_ESW_IMR);
	udelay(100);

	switch_get_mac_address(dev);

	/* Set receive and transmit descriptor base.
	*/
	fep->rx_bd_base = cbd_base;
	fep->tx_bd_base = cbd_base + RX_RING_SIZE;

	dev->base_addr = (unsigned long)fep->membase;

	/* The FEC Ethernet specific entries in the device structure. */
	dev->watchdog_timeo = TX_TIMEOUT;
	dev->netdev_ops	= &switch_netdev_ops;

	fep->skb_cur = fep->skb_dirty = 0;

	ret = switch_alloc_buffers(dev);
	if (ret)
		return ret;

	/* Set receive and transmit descriptor base.*/
	writel(fep->bd_dma, fep->membase + FEC_ESW_RDSR);
	writel((unsigned long)fep->bd_dma +
			sizeof(struct bufdesc) * RX_RING_SIZE,
			fep->membase + FEC_ESW_TDSR);

	/*set mii*/
	switch_hw_init(dev);

	/* Clear any outstanding interrupt.*/
	writel(0xffffffff, fep->membase + FEC_ESW_ISR);
	writel(FSL_ESW_IMR_RXF | FSL_ESW_IMR_TXF, fep->membase + FEC_ESW_IMR);
	esw_clear_atable(fep);

	/* Queue up command to detect the PHY and initialize the
	 * remainder of the interface.
	 */
#ifndef CONFIG_FEC_SHARED_PHY
	fep->phy_addr = 0;
#else
	fep->phy_addr = fep->index;
#endif

	fep->sequence_done = 1;

	return ret;
}

/* This function is called to start or restart the FEC during a link
 * change.  This only happens when switching between half and full
 * duplex.
 */
static void switch_restart(struct net_device *dev, int duplex)
{
	struct switch_enet_private *fep;
	int i;

	fep = netdev_priv(dev);

	/* Whack a reset.  We should wait for this.*/
	writel(1, fep->enetbase + FSL_FEC_ECR0);
	writel(1, fep->enetbase + FSL_FEC_ECR1);
	udelay(10);

	writel(FSL_ESW_MODE_SW_RST, fep->membase + FEC_ESW_MODE);
	udelay(10);
	writel(FSL_ESW_MODE_STATRST, fep->membase + FEC_ESW_MODE);
	writel(FSL_ESW_MODE_SW_EN, fep->membase + FEC_ESW_MODE);

	/* Enable transmit/receive on all ports */
	writel(0xffffffff, fep->membase + FEC_ESW_PER);

	/* Management port configuration,
	 * make port 0 as management port */
	writel(0, fep->membase + FEC_ESW_BMPC);

	/* Clear any outstanding interrupt.*/
	writel(0xffffffff, fep->membase + FEC_ESW_ISR);

	switch_hw_init(dev);

	/* Set station address.*/
	switch_get_mac_address(dev);

	writel(0, fep->membase + FEC_ESW_IMR);
	udelay(10);

	/* Set maximum receive buffer size.
	*/
	writel(PKT_MAXBLR_SIZE, fep->membase + FEC_ESW_MRBR);

	/* Set receive and transmit descriptor base.
	*/
	writel(fep->bd_dma, fep->membase + FEC_ESW_RDSR);
	writel((unsigned long)fep->bd_dma +
			sizeof(struct bufdesc) * RX_RING_SIZE,
			fep->membase + FEC_ESW_TDSR);

	fep->dirty_tx = fep->cur_tx = fep->tx_bd_base;
	fep->cur_rx = fep->rx_bd_base;

	/* Reset SKB transmit buffers.
	*/
	fep->skb_cur = fep->skb_dirty = 0;
	for (i = 0; i <= TX_RING_MOD_MASK; i++) {
		if (fep->tx_skbuff[i] != NULL) {
			dev_kfree_skb_any(fep->tx_skbuff[i]);
			fep->tx_skbuff[i] = NULL;
		}
	}

	/*hardware has set in hw_init*/
	fep->full_duplex = duplex;

	/* Clear any outstanding interrupt.*/
	writel(0xffffffff, fep->membase + FEC_ESW_ISR);
	writel(FSL_ESW_IMR_RXF | FSL_ESW_IMR_TXF, fep->membase + FEC_ESW_IMR);
}

static void switch_stop(struct net_device *dev)
{
	struct switch_enet_private *fep = netdev_priv(dev);

	/* We cannot expect a graceful transmit
	 * stop without link */
	if (fep->phy1_link)
		udelay(10);
	if (fep->phy2_link)
		udelay(10);

	/* Whack a reset.  We should wait for this */
	udelay(10);
}

static int fec_mdio_register(struct net_device *dev)
{
	int i, err = 0;
	struct switch_enet_private *fep = netdev_priv(dev);

	fep->mdio_bus = mdiobus_alloc();
	if (!fep->mdio_bus) {
		printk(KERN_ERR "ethernet switch mdiobus_alloc fail\n");
		return -ENOMEM;
	}

	fep->mdio_bus->name = "fsl l2 switch MII Bus";

	snprintf(fep->mdio_bus->id, MII_BUS_ID_SIZE, "%x", fep->pdev->id);

	fep->mdio_bus->read = &fec_enet_mdio_read;
	fep->mdio_bus->write = &fec_enet_mdio_write;
	fep->mdio_bus->priv = fep;

	fep->mdio_bus->irq = kmalloc(sizeof(int) * PHY_MAX_ADDR, GFP_KERNEL);
	if (!fep->mdio_bus->irq) {
		err = -ENOMEM;
		return err;
	}

	for (i = 0; i < PHY_MAX_ADDR; i++)
		fep->mdio_bus->irq[i] = PHY_POLL;

	err = mdiobus_register(fep->mdio_bus);
	if (err) {
		mdiobus_free(fep->mdio_bus);
		printk(KERN_ERR "%s: ethernet mdiobus_register fail\n",
			dev->name);
		return -EIO;
	}

	printk(KERN_INFO "%s mdiobus(%s) register ok.\n",
		fep->mdio_bus->name, fep->mdio_bus->id);
	return err;
}

static const struct of_device_id of_eth_switch_match[] = {
	{ .compatible = "fsl,eth-switch", },
	{},
};

MODULE_DEVICE_TABLE(of, of_eth_switch_match);

static int eth_switch_probe(struct platform_device *pdev)
{
	struct net_device *ndev;
	int err;
	struct switch_enet_private *fep;
	struct task_struct *task;
	int i, irq, ret = 0;
	struct resource *res=NULL;
	const struct of_device_id *match;

	printk(KERN_INFO "Ethernet Switch Version 1.0\n");
	match = of_match_device(of_eth_switch_match, &pdev->dev);
	if (!match){
		return -EINVAL;
	}
	else{
		pdev->id_entry = match->data;
	}

	ndev = alloc_etherdev(sizeof(struct switch_enet_private));
	if (!ndev) {
		printk(KERN_ERR "%s: ethernet switch alloc_etherdev fail\n",
				ndev->name);
		return -ENOMEM;
	}
	SET_NETDEV_DEV(ndev, &pdev->dev);

	fep = netdev_priv(ndev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		printk(KERN_ERR "get platform resource error\n");
		return -ENXIO;
	}

	ret = of_address_to_resource(pdev->dev.of_node, 0, res);
	if (ret) {
		/* Fail */
		printk(KERN_ERR "get address resource error!\n");
		return -ENXIO;
	}
	res = request_mem_region(res->start, resource_size(res), pdev->name);
	if (!res)
		return -EBUSY;
	memset(fep, 0, sizeof(*fep));

	fep->membase = ioremap(res->start, resource_size(res));
	if (!fep->membase)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res)
		return -ENXIO;

	res = request_mem_region(res->start, resource_size(res), pdev->name);
	if (!res)
		return -EBUSY;
	fep->macbase = ioremap(res->start, resource_size(res));

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res)
		return -ENXIO;

	res = request_mem_region(res->start, resource_size(res), pdev->name);
	if (!res)
		return -EBUSY;
	fep->enetbase = ioremap(res->start, resource_size(res));
	if (!fep->enetbase)
		return -ENOMEM;

	fep->pdev = pdev;
	platform_set_drvdata(pdev, ndev);

	printk(KERN_INFO "%s: ethernet switch init\n", __func__);

	/* This device has up to three irqs on some platforms */
	for (i = 0; i < 3; i++) {
		irq = platform_get_irq(pdev, i);
		if (i && irq < 0)
			break;
		ret = request_irq(irq, switch_enet_interrupt, IRQF_DISABLED,
			pdev->name, ndev);
		if (ret) {
			while (--i >= 0) {
				irq = platform_get_irq(pdev, i);
				free_irq(irq, ndev);
			}
			return -ENOMEM;
		}
	}

	fep->clk = clk_get(&pdev->dev, "switch_clk");

	if (IS_ERR(fep->clk)) {
		ret = PTR_ERR(fep->clk);
		goto failed_clk;
	}
	clk_enable(fep->clk);

	err = switch_enet_init(pdev);
	if (err) {
		free_netdev(ndev);
		platform_set_drvdata(pdev, NULL);
		return -EIO;
	}

	err = fec_mdio_register(ndev);
	if (err) {
		printk(KERN_ERR "%s: L2 switch fec_mdio_register error!\n",
				ndev->name);
		free_netdev(ndev);
		platform_set_drvdata(pdev, NULL);
		return -ENOMEM;
	}

	/* setup timer for Learning Aging function */
	init_timer(&fep->timer_aging);
	fep->timer_aging.function = l2switch_aging_timer;
	fep->timer_aging.data = (unsigned long) fep;
	fep->timer_aging.expires = jiffies + LEARNING_AGING_TIMER;
	add_timer(&fep->timer_aging);

	/* register network device*/
	if (register_netdev(ndev) != 0) {
		/* XXX: missing cleanup here */
		free_netdev(ndev);
		platform_set_drvdata(pdev, NULL);
		printk(KERN_ERR "%s: L2 switch register_netdev fail\n",
				ndev->name);
		return -EIO;
	}

	task = kthread_run(switch_enet_learning, fep,
			"fsl_l2switch_learning");
	if (IS_ERR(task)) {
		err = PTR_ERR(task);
		return err;
	}

	printk(KERN_INFO "%s: ethernet switch %pM\n",
			ndev->name, ndev->dev_addr);
	return 0;
failed_clk:
	iounmap(fep->membase);
	return ret;
}

static int eth_switch_remove(struct platform_device *pdev)
{
	int i;
	struct net_device *dev;
	struct switch_enet_private *fep;
	struct switch_platform_private *chip;

	chip = platform_get_drvdata(pdev);
	if (chip) {
		for (i = 0; i < chip->num_slots; i++) {
			fep = chip->fep_host[i];
			dev = fep->netdev;
			fep->sequence_done = 1;
			unregister_netdev(dev);
			free_netdev(dev);

			del_timer_sync(&fep->timer_aging);
		}

		platform_set_drvdata(pdev, NULL);
		kfree(chip);

	} else
		printk(KERN_ERR "%s: can not get the "
			"switch_platform_private %x\n", __func__,
			(unsigned int)chip);

	return 0;
}

static struct platform_driver eth_switch_driver = {
	.probe          = eth_switch_probe,
	.remove         = (eth_switch_remove),
	.driver         = {
		.name   = "eth-switch",
		.owner  = THIS_MODULE,
		.of_match_table = of_eth_switch_match,
	},
};

static int fec_mac_addr_setup(char *mac_addr)
{
	char *ptr, *p = mac_addr;
	unsigned long tmp;
	int i = 0, ret = 0;

	while (p && (*p) && i < 6) {
		ptr = strchr(p, ':');
		if (ptr)
			*ptr++ = '\0';

		if (strlen(p)) {
			ret = strict_strtoul(p, 16, &tmp);
			if (ret < 0 || tmp > 0xff)
				break;
			macaddr[i++] = tmp;
		}
		p = ptr;
	}

	return 0;
}

__setup("fec_mac=", fec_mac_addr_setup);

static int __init fsl_l2_switch_init(void)
{
	return platform_driver_register(&eth_switch_driver);
}

static void __exit fsl_l2_switch_exit(void)
{
	platform_driver_unregister(&eth_switch_driver);
}


module_init(fsl_l2_switch_init);
module_exit(fsl_l2_switch_exit);
MODULE_LICENSE("GPL");
