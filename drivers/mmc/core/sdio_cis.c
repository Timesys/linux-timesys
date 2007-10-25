/*
 * linux/drivers/mmc/core/sdio_cis.c
 *
 * Author:	Nicolas Pitre
 * Created:	June 11, 2007
 * Copyright:	MontaVista Software Inc.
 *
 * Copyright 2007 Pierre Ossman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#include <linux/kernel.h>

#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>

#include "sdio_cis.h"
#include "sdio_ops.h"

#define sdio_card_func_id(card, func) \
	(func ? sdio_func_id(func) : mmc_card_id(card))

static int cistpl_vers_1(struct mmc_card *card, struct sdio_func *func,
			 const unsigned char *buf, unsigned size)
{
	unsigned i, nr_strings = 0;

	printk(KERN_DEBUG "%s: CISTPL_VERS_1 = %u.%u\n",
	       sdio_card_func_id(card, func), buf[0], buf[1]);
	buf += 2;

	for (i = 0; i < size - 2; i++) {
		if (buf[i] == 0xff)
			break;
		if (buf[i] == 0)
			nr_strings++;
	}
	printk(KERN_INFO "%s:", sdio_card_func_id(card, func));
	for (i = 0; i < nr_strings; i++) {
		printk(" \"%s\"", buf);
		while (*buf++);
	}
	printk("\n");

	return 0;
}

static int cistpl_manfid(struct mmc_card *card, struct sdio_func *func,
			 const unsigned char *buf, unsigned size)
{
	unsigned int vendor, device;

	/* TPLMID_MANF */
	vendor = buf[0] | (buf[1] << 8);
	printk(KERN_DEBUG "%s: TPLMID_MANF = 0x%04x\n",
	       sdio_card_func_id(card, func), vendor);

	/* TPLMID_CARD */
	device = buf[2] | (buf[3] << 8);
	printk(KERN_DEBUG "%s: TPLMID_CARD = 0x%04x\n",
	       sdio_card_func_id(card, func), device);

	if (func) {
		func->vendor = vendor;
		func->device = device;
	} else {
		card->cis.vendor = vendor;
		card->cis.device = device;
	}

	return 0;
}

static int cistpl_funcid(struct mmc_card *card, struct sdio_func *func,
			 const unsigned char *buf, unsigned size)
{
	/* TPLFID_FUNCTION */
	printk(KERN_DEBUG "%s: TPLFID_FUNCTION = 0x%02x\n",
	       sdio_card_func_id(card, func), buf[0]);

	return 0;
}

static const unsigned char speed_val[16] =
	{ 0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80 };
static const unsigned int speed_unit[8] =
	{ 10000, 100000, 1000000, 10000000, 0, 0, 0, 0 };

static int cistpl_funce_common(struct mmc_card *card,
			       const unsigned char *buf, unsigned size)
{
	if (size < 0x04 || buf[0] != 0)
		return -EINVAL;

	/* TPLFE_FN0_BLK_SIZE */
	card->cis.blksize = buf[1] | (buf[2] << 8);
	printk(KERN_DEBUG "%s: TPLFE_FN0_BLK_SIZE = %u\n",
	       mmc_card_id(card), (unsigned)card->cis.blksize);
	/* TPLFE_MAX_TRAN_SPEED */
	card->cis.max_dtr = speed_val[(buf[3] >> 3) & 15] *
			    speed_unit[buf[3] & 7];
	printk(KERN_DEBUG "%s: max speed = %u kbps\n",
	       mmc_card_id(card), (unsigned)card->cis.max_dtr/1000);

	return 0;
}

static int cistpl_funce_func(struct sdio_func *func,
			     const unsigned char *buf, unsigned size)
{
	unsigned val;
	unsigned vsn;
	unsigned min_size;

	vsn = func->card->cccr.sdio_vsn;
	min_size = (vsn == SDIO_SDIO_REV_1_00) ? 28 : 42;

	if (size < min_size || buf[0] != 1)
		return -EINVAL;

	/* TPLFE_FUNCTION_INFO */
	val = buf[1];
	printk(KERN_DEBUG "%s: TPLFE_FUNCTION_INFO = 0x%02x\n",
	       sdio_func_id(func), val);
	/* TPLFE_STD_IO_REV */
	val = buf[2];
	printk(KERN_DEBUG "%s: TPLFE_STD_IO_REV = 0x%02x\n",
	       sdio_func_id(func), val);
	/* TPLFE_CARD_PSN */
	val = buf[3] | (buf[4] << 8) | (buf[5] << 16) | (buf[6] << 24);
	printk(KERN_DEBUG "%s: TPLFE_CARD_PSN = 0x%08x\n",
	       sdio_func_id(func), val);
	/* TPLFE_CSA_SIZE */
	val = buf[7] | (buf[8] << 8) | (buf[9] << 16) | (buf[10] << 24);
	printk(KERN_DEBUG "%s: TPLFE_CSA_SIZE = 0x%08x\n",
	       sdio_func_id(func), val);
	/* TPLFE_CSA_PROPERTY */
	val = buf[11];
	printk(KERN_DEBUG "%s: TPLFE_CSA_PROPERTY = 0x%02x\n",
	       sdio_func_id(func), val);
	/* TPLFE_MAX_BLK_SIZE */
	func->max_blksize = buf[12] | (buf[13] << 8);
	printk(KERN_DEBUG "%s: TPLFE_MAX_BLK_SIZE = %u\n",
	       sdio_func_id(func), (unsigned)func->max_blksize);
	/* TPLFE_OCR */
	val = buf[14] | (buf[15] << 8) | (buf[16] << 16) | (buf[17] << 24);
	printk(KERN_DEBUG "%s: TPLFE_OCR = 0x%08x\n",
	       sdio_func_id(func), val);
	/* TPLFE_OP_MIN_PWR, TPLFE_OP_AVG_PWR, TPLFE_OP_MAX_PWR */
	printk(KERN_DEBUG "%s: operating current (min/avg/max) = %u/%u/%u mA\n",
	       sdio_func_id(func), buf[18], buf[19], buf[20]);
	/* TPLFE_SB_MIN_PWR, TPLFE_SB_AVG_PWR, TPLFE_SB_MAX_PWR */
	printk(KERN_DEBUG "%s: standby current (min/avg/max) = %u/%u/%u mA\n",
	       sdio_func_id(func), buf[21], buf[22], buf[23]);
	/* TPLFE_MIN_BW */
	val = buf[24] | (buf[25] << 8);
	printk(KERN_DEBUG "%s: minimum data bandwidth = %u KB/sec\n",
	       sdio_func_id(func), val);
	/* TPLFE_OPT_BW */
	val = buf[26] | (buf[27] << 8);
	printk(KERN_DEBUG "%s: optimum data bandwidth = %u KB/sec\n",
	       sdio_func_id(func), val);
	if (vsn > SDIO_SDIO_REV_1_00) {
		/* TPLFE_ENABLE_TIMEOUT_VAL */
		val = buf[28] | (buf[29] << 8);
		printk(KERN_DEBUG "%s: TPLFE_ENABLE_TIMEOUT_VAL = %u\n",
		       sdio_func_id(func), val);
		/* TPLFE_SP_AVG_PWR_3.3V */
		val = buf[30] | (buf[31] << 8);
		/* TPLFE_SP_MAX_PWR_3.3V */
		val = buf[32] | (buf[33] << 8);
		/* TPLFE_HP_AVG_PWR_3.3V */
		val = buf[34] | (buf[35] << 8);
		/* TPLFE_HP_MAX_PWR_3.3V */
		val = buf[36] | (buf[37] << 8);
		/* TPLFE_LP_AVG_PWR_3.3V */
		val = buf[38] | (buf[39] << 8);
		/* TPLFE_LP_MAX_PWR_3.3V */
		val = buf[40] | (buf[41] << 8);
	}

	return 0;
}

static int cistpl_funce(struct mmc_card *card, struct sdio_func *func,
			const unsigned char *buf, unsigned size)
{
	int ret;

	/*
	 * There should be two versions of the CISTPL_FUNCE tuple,
	 * one for the common CIS (function 0) and a version used by
	 * the individual function's CIS (1-7). Yet, the later has a
	 * different length depending on the SDIO spec version.
	 */
	if (func)
		ret = cistpl_funce_func(func, buf, size);
	else
		ret = cistpl_funce_common(card, buf, size);

	if (ret) {
		printk(KERN_ERR "%s: bad CISTPL_FUNCE size %u "
		       "type %u\n", sdio_card_func_id(card, func),
		       size, buf[0]);
		return ret;
	}

	return 0;
}

typedef int (tpl_parse_t)(struct mmc_card *, struct sdio_func *,
			   const unsigned char *, unsigned);

struct cis_tpl {
	unsigned char code;
	unsigned char min_size;
	tpl_parse_t *parse;
};

static const struct cis_tpl cis_tpl_list[] = {
	{	0x15,	3,	cistpl_vers_1	},
	{	0x20,	4,	cistpl_manfid	},
	{	0x21,	2,	cistpl_funcid	},
	{	0x22,	0,	cistpl_funce	},
};

static int sdio_read_cis(struct mmc_card *card, struct sdio_func *func)
{
	int ret;
	struct sdio_func_tuple *this, **prev;
	unsigned i, ptr = 0;

	/*
	 * Note that this works for the common CIS (function number 0) as
	 * well as a function's CIS * since SDIO_CCCR_CIS and SDIO_FBR_CIS
	 * have the same offset.
	 */
	for (i = 0; i < 3; i++) {
		unsigned char x, fn;

		if (func)
			fn = func->num;
		else
			fn = 0;

		ret = mmc_io_rw_direct(card, 0, 0,
			SDIO_FBR_BASE(fn) + SDIO_FBR_CIS + i, 0, &x);
		if (ret)
			return ret;
		ptr |= x << (i * 8);
	}

	if (func)
		prev = &func->tuples;
	else
		prev = &card->tuples;

	BUG_ON(*prev);

	do {
		unsigned char tpl_code, tpl_link;

		ret = mmc_io_rw_direct(card, 0, 0, ptr++, 0, &tpl_code);
		if (ret)
			break;

		/* 0xff means we're done */
		if (tpl_code == 0xff)
			break;

		ret = mmc_io_rw_direct(card, 0, 0, ptr++, 0, &tpl_link);
		if (ret)
			break;

		this = kmalloc(sizeof(*this) + tpl_link, GFP_KERNEL);
		if (!this)
			return -ENOMEM;

		for (i = 0; i < tpl_link; i++) {
			ret = mmc_io_rw_direct(card, 0, 0,
					       ptr + i, 0, &this->data[i]);
			if (ret)
				break;
		}
		if (ret) {
			kfree(this);
			break;
		}

		for (i = 0; i < ARRAY_SIZE(cis_tpl_list); i++)
			if (cis_tpl_list[i].code == tpl_code)
				break;
		if (i >= ARRAY_SIZE(cis_tpl_list)) {
			/* this tuple is unknown to the core */
			this->next = NULL;
			this->code = tpl_code;
			this->size = tpl_link;
			*prev = this;
			prev = &this->next;
			printk(KERN_DEBUG
			       "%s: queuing CIS tuple 0x%02x length %u\n",
			       sdio_card_func_id(card, func),
			       tpl_code, tpl_link);
		} else {
			const struct cis_tpl *tpl = cis_tpl_list + i;
			if (tpl_link < tpl->min_size) {
				printk(KERN_ERR
				       "%s: bad CIS tuple 0x%02x (length = %u, expected >= %u\n",
				       sdio_card_func_id(card, func),
				       tpl_code, tpl_link, tpl->min_size);
				ret = -EINVAL;
			} else {
				ret = tpl->parse(card, func,
						 this->data, tpl_link);
			}
			kfree(this);
		}

		ptr += tpl_link;
	} while (!ret);

	/*
	 * Link in all unknown tuples found in the common CIS so that
	 * drivers don't have to go digging in two places.
	 */
	if (func)
		*prev = card->tuples;

	return ret;
}

int sdio_read_common_cis(struct mmc_card *card)
{
	return sdio_read_cis(card, NULL);
}

void sdio_free_common_cis(struct mmc_card *card)
{
	struct sdio_func_tuple *tuple, *victim;

	tuple = card->tuples;

	while (tuple) {
		victim = tuple;
		tuple = tuple->next;
		kfree(victim);
	}

	card->tuples = NULL;
}

int sdio_read_func_cis(struct sdio_func *func)
{
	int ret;

	ret = sdio_read_cis(func->card, func);
	if (ret)
		return ret;

	/*
	 * Since we've linked to tuples in the card structure,
	 * we must make sure we have a reference to it.
	 */
	get_device(&func->card->dev);

	/*
	 * Vendor/device id is optional for function CIS, so
	 * copy it from the card structure as needed.
	 */
	if (func->vendor == 0) {
		func->vendor = func->card->cis.vendor;
		func->device = func->card->cis.device;
	}

	return 0;
}

void sdio_free_func_cis(struct sdio_func *func)
{
	struct sdio_func_tuple *tuple, *victim;

	tuple = func->tuples;

	while (tuple && tuple != func->card->tuples) {
		victim = tuple;
		tuple = tuple->next;
		kfree(victim);
	}

	func->tuples = NULL;

	/*
	 * We have now removed the link to the tuples in the
	 * card structure, so remove the reference.
	 */
	put_device(&func->card->dev);
}

