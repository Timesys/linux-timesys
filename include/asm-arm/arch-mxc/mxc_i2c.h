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
#ifndef __ASM_ARCH_MXC_I2C_H__
#define __ASM_ARCH_MXC_I2C_H__

/*!
 * @defgroup MXCI2C Inter-IC (I2C) Driver
 */

/*!
 * @file arch-mxc/mxc_i2c.h
 *
 * @brief This file contains the I2C chip level configuration details.
 *
 * It also contains the API function that other drivers can use to read/write
 * to the I2C device registers.
 *
 * @ingroup MXCI2C
 */

/*!
 * This defines the string used to identify MXC I2C Bus drivers
 */
#define MXC_ADAPTER_NAME        "MXC I2C Adapter"

#define MXC_I2C_FLAG_READ	0x01	/* if set, is read; else is write */
#define MXC_I2C_FLAG_POLLING	0x02	/* if set, is polling mode; else is interrupt mode */

/*!
 * Function used to read the register of the i2c slave device. This function
 * is a kernel level API that can be called by other device drivers.
 *
 * @param   bus_id       the MXC I2C bus that the slave device is connected to
 *                       (0 based)
 * @param   addr         slave address of the device we wish to read data from
 * @param   *reg         register in the device we wish to access
 * @param   reg_len      number of bytes in the register address
 * @param   *buf         data buffer
 * @param   num          number of data bytes to transfer
 *
 * @return  Function returns the number of messages transferred to/from the device
 *          or a negative number on failure
 */
int mxc_i2c_read(int bus_id, unsigned int addr, char *reg, int reg_len,
		 char *buf, int num);

/*!
 * Function used to write to the register of the i2c slave device. This function
 * is a kernel level API that can be called by other device drivers.
 *
 * @param   bus_id       the MXC I2C bus that the slave device is connected to
 *                       (0 based)
 * @param   addr         slave address of the device we wish to write data to
 * @param   *reg         register in the device we wish to access
 * @param   reg_len      number of bytes in the register address
 * @param   *buf         data buffer
 * @param   num          number of data bytes to transfer
 *
 * @return  Function returns the number of messages transferred to/from the device
 *          or a negative number on failure
 */
int mxc_i2c_write(int bus_id, unsigned int addr, char *reg, int reg_len,
		  char *buf, int num);

/*!
 * Function used to read the register of the i2c slave device. This function
 * is a kernel level API that can be called by other device drivers.
 *
 * @param   bus_id       the MXC I2C bus that the slave device is connected to
 *                       (0 based)
 * @param   addr         slave address of the device we wish to read data from
 * @param   *reg         register in the device we wish to access
 * @param   reg_len      number of bytes in the register address
 * @param   *buf         data buffer
 * @param   num          number of data bytes to transfer
 *
 * @return  Function returns the number of messages transferred to/from the
 *          device or a negative number on failure *
 */
int mxc_i2c_polling_read(int bus_id, unsigned int addr, char *reg, int reg_len,
			 char *buf, int num);

/*!
 * Function used to write to the register of the i2c slave device. This function
 * is a kernel level API that can be called by other device drivers.
 *
 * @param   bus_id       the MXC I2C bus that the slave device is connected to
 *                       (0 based)
 * @param   addr         slave address of the device we wish to write data to
 * @param   *reg         register in the device we wish to access
 * @param   reg_len      number of bytes in the register address
 * @param   *buf         data buffer
 * @param   num          number of data bytes to transfer
 *
 * @return  Function returns the number of messages transferred to/from the
 *          device or a negative number on failure
 */
int mxc_i2c_polling_write(int bus_id, unsigned int addr, char *reg, int reg_len,
			  char *buf, int num);

#endif				/* __ASM_ARCH_MXC_I2C_H__ */
