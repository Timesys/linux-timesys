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
#ifndef __PMIC_CONFIG_H__
#define __PMIC_CONFIG_H__

 /*!
  * @file pmic_config.h
  * @brief This file contains configuration define used by PMIC Client drivers.
  *
  * @ingroup PMIC_CORE
  */

/*
 * Includes
 */
#include <linux/kernel.h>	/* printk() */
#include <linux/module.h>	/* modules */
#include <linux/init.h>		/* module_{init,exit}() */
#include <linux/slab.h>		/* kmalloc()/kfree() */
#include <linux/stddef.h>

#include <asm/uaccess.h>	/* copy_{from,to}_user() */
#include <asm/arch/pmic_status.h>
#include <asm/arch/pmic_external.h>

/*
 * Bitfield macros that use rely on bitfield width/shift information.
 */
#define BITFMASK(field) (((1U << (field ## _WID)) - 1) << (field ## _LSH))
#define BITFVAL(field, val) ((val) << (field ## _LSH))
#define BITFEXT(var, bit) ((var & BITFMASK(bit)) >> (bit ## _LSH))

/*
 * Macros implementing error handling
 */
#define CHECK_ERROR(a)			\
do { 					\
        int ret = (a); 			\
        if(ret != PMIC_SUCCESS) 	\
	return ret; 			\
} while (0)

#define CHECK_ERROR_KFREE(func, freeptrs) \
do { \
  int ret = (func); \
  if (ret != PMIC_SUCCESS) \
  { \
     freeptrs; \
     return ret; \
  }\
} while(0);

#endif				/* __PMIC_CONFIG_H__ */
