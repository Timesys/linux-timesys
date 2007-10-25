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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <asm/arch/mxc_i2c.h>

#include <asm/arch/pmic_power.h>

#include "mxc_v4l2_capture.h"

#define OV2640_DEBUG
#ifdef OV2640_DEBUG
#define PRINTK(fmt,arg...) printk(KERN_ERR fmt,##arg)
#else
#define PRINTK(fmt,arg...)
#endif

#define OV2640_I2C_ADDRESS 0x30

static sensor_interface *interface_param = NULL;
static int reset_frame_rate = 30;
static int ov2640_attach(struct i2c_adapter *adapter);
static int ov2640_detach(struct i2c_client *client);

static struct i2c_driver ov2640_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "OV2640 Client",
		   },
	.attach_adapter = ov2640_attach,
	.detach_client = ov2640_detach,
};

static struct i2c_client ov2640_i2c_client = {
	.name = "ov2640 I2C dev",
	.addr = OV2640_I2C_ADDRESS,
	.driver = &ov2640_i2c_driver,
};

/*!
 * ov2640 I2C attach function
 *
 * @param adapter            struct i2c_adapter *
 * @return  Error code indicating success or failure
 */
static int ov2640_attach(struct i2c_adapter *adapter)
{
	if (strcmp(adapter->name, MXC_ADAPTER_NAME) != 0) {
		PRINTK("mt9v111_attach: %s\n", adapter->name);
		return -1;
	}
	ov2640_i2c_client.adapter = adapter;
	if (i2c_attach_client(&ov2640_i2c_client)) {
		ov2640_i2c_client.adapter = NULL;
		PRINTK("ov2640_attach: i2c_attach_client failed\n");
		return -1;
	}
	interface_param = (sensor_interface *)
	    kmalloc(sizeof(sensor_interface), GFP_KERNEL);

	if (!interface_param) {
		PRINTK("ov2640_attach: kmalloc failed \n");
		return -1;
	}
	return 0;
}

/*!
 * ov2640 I2C detach function
 *
 * @param client            struct i2c_client *
 * @return  Error code indicating success or failure
 */
static int ov2640_detach(struct i2c_client *client)
{
	int err;
	PMIC_STATUS ret;

	if (!ov2640_i2c_client.adapter)
		return -1;

	err = i2c_detach_client(&ov2640_i2c_client);
	ov2640_i2c_client.adapter = NULL;

	if (interface_param)
		kfree(interface_param);

	interface_param = NULL;

	/* DOVDD */
	if ((ret = pmic_power_regulator_off(REGU_GPO3)) < 0) {
		PRINTK("%s:REGU_GPO3 power on error:%d\n", __func__, ret);
		return 0;
	} else {
		PRINTK("%s:REGU_GPO3 power on ok\n", __func__);
	}
	PRINTK("%s:OV2640 power off ok\n", __func__);

	return err;
}

static int
ov2640_i2c_client_xfer(int addr, u8 reg, char *buf, int num, int tran_flag)
{
	struct i2c_msg msg[2];
	int ret;

	msg[0].addr = addr;
	msg[0].len = 1;
	msg[0].buf = &reg;
	msg[0].flags = tran_flag;
	msg[0].flags &= ~I2C_M_RD;

	msg[1].addr = addr;
	msg[1].len = num;
	msg[1].buf = buf;
	msg[1].flags = tran_flag;

	if (tran_flag & MXC_I2C_FLAG_READ) {
		msg[1].flags |= I2C_M_RD;
	} else {
		msg[1].flags &= ~I2C_M_RD;
	}

	if (ov2640_i2c_client.adapter == NULL) {
		PRINTK("%s:adapter error\n", __func__);
		return -1;
	}

	ret = i2c_transfer(ov2640_i2c_client.adapter, msg, 2);

	if (ret >= 0) {
		/* PRINTK("%s:i2c transfer num:%d\n", __func__, ret); */
		return 0;
	}
	PRINTK("%s:i2c transfer error:%d\n", __func__, ret);
	return ret;
}

static int ov2640_write_reg(u8 reg, u8 val)
{
	if (ov2640_i2c_client_xfer(OV2640_I2C_ADDRESS, reg, &val, 1, 0) < 0) {
		PRINTK("%s:write reg error: reg=%x, val=%x\n",
		       __func__, reg, val);
		return -1;
	}
	return 0;
}

/* should be replaced by width and height version. */
static int ov2640_init_1600_1120(void)
{
	ov2640_write_reg(0xff, 1);
	ov2640_write_reg(0x12, 0x80);
	udelay(1000);
	ov2640_write_reg(0xff, 0x00);
	ov2640_write_reg(0x2c, 0xff);
	ov2640_write_reg(0x2e, 0xdf);
	ov2640_write_reg(0xff, 0x01);
	ov2640_write_reg(0x3c, 0x32);
	ov2640_write_reg(0x11, 0x01);
	ov2640_write_reg(0x09, 0x00);
	ov2640_write_reg(0x04, 0x28);
	ov2640_write_reg(0x13, 0xe5);
	ov2640_write_reg(0x14, 0x48);
	ov2640_write_reg(0x2c, 0x0c);
	ov2640_write_reg(0x33, 0x78);
	ov2640_write_reg(0x3a, 0x33);
	ov2640_write_reg(0x3b, 0xfb);
	ov2640_write_reg(0x3e, 0x00);
	ov2640_write_reg(0x43, 0x11);
	ov2640_write_reg(0x16, 0x10);
	ov2640_write_reg(0x39, 0x82);
	ov2640_write_reg(0x35, 0x88);
	ov2640_write_reg(0x22, 0x0a);
	ov2640_write_reg(0x37, 0x40);
	ov2640_write_reg(0x23, 0x00);
	ov2640_write_reg(0x34, 0xa0);
	ov2640_write_reg(0x36, 0x1a);
	ov2640_write_reg(0x06, 0x02);
	ov2640_write_reg(0x07, 0xc0);
	ov2640_write_reg(0x0d, 0xb7);
	ov2640_write_reg(0x0e, 0x01);
	ov2640_write_reg(0x4c, 0x00);
	ov2640_write_reg(0x4a, 0x81);
	ov2640_write_reg(0x21, 0x99);
	ov2640_write_reg(0x24, 0x40);
	ov2640_write_reg(0x25, 0x38);
	ov2640_write_reg(0x26, 0x82);
	ov2640_write_reg(0x5c, 0x00);
	ov2640_write_reg(0x63, 0x00);
	ov2640_write_reg(0x46, 0x3f);
	ov2640_write_reg(0x0c, 0x3c);
	ov2640_write_reg(0x5d, 0x55);
	ov2640_write_reg(0x5e, 0x7d);
	ov2640_write_reg(0x5f, 0x7d);
	ov2640_write_reg(0x60, 0x55);
	ov2640_write_reg(0x61, 0x70);
	ov2640_write_reg(0x62, 0x80);
	ov2640_write_reg(0x7c, 0x05);
	ov2640_write_reg(0x20, 0x80);
	ov2640_write_reg(0x28, 0x30);
	ov2640_write_reg(0x6c, 0x00);
	ov2640_write_reg(0x6d, 0x80);
	ov2640_write_reg(0x6e, 0x00);
	ov2640_write_reg(0x70, 0x02);
	ov2640_write_reg(0x71, 0x94);
	ov2640_write_reg(0x73, 0xc1);
	ov2640_write_reg(0x3d, 0x34);
	ov2640_write_reg(0x5a, 0x57);
	ov2640_write_reg(0x4f, 0xbb);
	ov2640_write_reg(0x50, 0x9c);
	ov2640_write_reg(0xff, 0x00);
	ov2640_write_reg(0xe5, 0x7f);
	ov2640_write_reg(0xf9, 0xc0);
	ov2640_write_reg(0x41, 0x24);
	ov2640_write_reg(0x44, 0x06);
	ov2640_write_reg(0xe0, 0x14);
	ov2640_write_reg(0x76, 0xff);
	ov2640_write_reg(0x33, 0xa0);
	ov2640_write_reg(0x42, 0x20);
	ov2640_write_reg(0x43, 0x18);
	ov2640_write_reg(0x4c, 0x00);
	ov2640_write_reg(0x87, 0xd0);
	ov2640_write_reg(0xd7, 0x03);
	ov2640_write_reg(0xd9, 0x10);
	ov2640_write_reg(0xd3, 0x82);
	ov2640_write_reg(0xc8, 0x08);
	ov2640_write_reg(0xc9, 0x80);
	ov2640_write_reg(0x7c, 0x00);
	ov2640_write_reg(0x7d, 0x00);
	ov2640_write_reg(0x7c, 0x03);
	ov2640_write_reg(0x7d, 0x48);
	ov2640_write_reg(0x7d, 0x48);
	ov2640_write_reg(0x7c, 0x08);
	ov2640_write_reg(0x7d, 0x20);
	ov2640_write_reg(0x7d, 0x10);
	ov2640_write_reg(0x7d, 0x0e);
	ov2640_write_reg(0x90, 0x00);
	ov2640_write_reg(0x91, 0x0e);
	ov2640_write_reg(0x91, 0x1a);
	ov2640_write_reg(0x91, 0x31);
	ov2640_write_reg(0x91, 0x5a);
	ov2640_write_reg(0x91, 0x69);
	ov2640_write_reg(0x91, 0x75);
	ov2640_write_reg(0x91, 0x7e);
	ov2640_write_reg(0x91, 0x88);
	ov2640_write_reg(0x91, 0x8f);
	ov2640_write_reg(0x91, 0x96);
	ov2640_write_reg(0x91, 0xa3);
	ov2640_write_reg(0x91, 0xaf);
	ov2640_write_reg(0x91, 0xc4);
	ov2640_write_reg(0x91, 0xd7);
	ov2640_write_reg(0x91, 0xe8);
	ov2640_write_reg(0x91, 0x20);

	ov2640_write_reg(0x92, 0x00);
	ov2640_write_reg(0x93, 0x06);
	ov2640_write_reg(0x93, 0xe3);
	ov2640_write_reg(0x93, 0x03);
	ov2640_write_reg(0x93, 0x03);
	ov2640_write_reg(0x93, 0x00);
	ov2640_write_reg(0x93, 0x02);
	ov2640_write_reg(0x93, 0x00);
	ov2640_write_reg(0x93, 0x00);
	ov2640_write_reg(0x93, 0x00);
	ov2640_write_reg(0x93, 0x00);
	ov2640_write_reg(0x93, 0x00);
	ov2640_write_reg(0x93, 0x00);
	ov2640_write_reg(0x93, 0x00);

	ov2640_write_reg(0x96, 0x00);
	ov2640_write_reg(0x97, 0x08);
	ov2640_write_reg(0x97, 0x19);
	ov2640_write_reg(0x97, 0x02);
	ov2640_write_reg(0x97, 0x0c);
	ov2640_write_reg(0x97, 0x24);
	ov2640_write_reg(0x97, 0x30);
	ov2640_write_reg(0x97, 0x28);
	ov2640_write_reg(0x97, 0x26);
	ov2640_write_reg(0x97, 0x02);
	ov2640_write_reg(0x97, 0x98);
	ov2640_write_reg(0x97, 0x80);
	ov2640_write_reg(0x97, 0x00);
	ov2640_write_reg(0x97, 0x00);

	ov2640_write_reg(0xa4, 0x00);
	ov2640_write_reg(0xa8, 0x00);
	ov2640_write_reg(0xc5, 0x11);
	ov2640_write_reg(0xc6, 0x51);
	ov2640_write_reg(0xbf, 0x80);
	ov2640_write_reg(0xc7, 0x10);
	ov2640_write_reg(0xb6, 0x66);
	ov2640_write_reg(0xb8, 0xa5);
	ov2640_write_reg(0xb7, 0x64);
	ov2640_write_reg(0xb9, 0x7c);
	ov2640_write_reg(0xb3, 0xaf);
	ov2640_write_reg(0xb4, 0x97);
	ov2640_write_reg(0xb5, 0xff);
	ov2640_write_reg(0xb0, 0xc5);
	ov2640_write_reg(0xb1, 0x94);
	ov2640_write_reg(0xb2, 0x0f);
	ov2640_write_reg(0xc4, 0x5c);

	ov2640_write_reg(0xa6, 0x00);
	ov2640_write_reg(0xa7, 0x20);
	ov2640_write_reg(0xa7, 0xd8);
	ov2640_write_reg(0xa7, 0x1b);
	ov2640_write_reg(0xa7, 0x31);
	ov2640_write_reg(0xa7, 0x00);
	ov2640_write_reg(0xa7, 0x18);
	ov2640_write_reg(0xa7, 0x20);
	ov2640_write_reg(0xa7, 0xd8);
	ov2640_write_reg(0xa7, 0x19);
	ov2640_write_reg(0xa7, 0x31);
	ov2640_write_reg(0xa7, 0x00);
	ov2640_write_reg(0xa7, 0x18);
	ov2640_write_reg(0xa7, 0x20);
	ov2640_write_reg(0xa7, 0xd8);
	ov2640_write_reg(0xa7, 0x19);
	ov2640_write_reg(0xa7, 0x31);
	ov2640_write_reg(0xa7, 0x00);
	ov2640_write_reg(0xa7, 0x18);

	ov2640_write_reg(0xc0, 0xc8);
	ov2640_write_reg(0xc1, 0x96);
	ov2640_write_reg(0x86, 0x3d);
	ov2640_write_reg(0x50, 0x00);
	ov2640_write_reg(0x51, 0x90);
	ov2640_write_reg(0x52, 0x18);
	ov2640_write_reg(0x53, 0x00);
	ov2640_write_reg(0x54, 0x00);
	ov2640_write_reg(0x55, 0x88);
	ov2640_write_reg(0x57, 0x00);
	ov2640_write_reg(0x5a, 0x90);
	ov2640_write_reg(0x5b, 0x18);
	ov2640_write_reg(0x5c, 0x05);
	ov2640_write_reg(0xc3, 0xef);
	ov2640_write_reg(0x7f, 0x00);
	ov2640_write_reg(0xda, 0x01);
	ov2640_write_reg(0xe5, 0x1f);
	ov2640_write_reg(0xe1, 0x67);
	ov2640_write_reg(0xe0, 0x00);
	ov2640_write_reg(0xdd, 0x7f);
	ov2640_write_reg(0x05, 0x00);

	return 0;
}

static int ov2640_init_800_600(void)
{
	ov2640_write_reg(0xff, 0x00);
	ov2640_write_reg(0xff, 0x01);
	ov2640_write_reg(0x12, 0x80);
	udelay(1000);
	ov2640_write_reg(0xff, 0x00);
	ov2640_write_reg(0x2c, 0xff);
	ov2640_write_reg(0x2e, 0xdf);
	ov2640_write_reg(0xff, 0x01);
	ov2640_write_reg(0x3c, 0x32);
	ov2640_write_reg(0x11, 0x01);
	ov2640_write_reg(0x09, 0x00);
	ov2640_write_reg(0x04, 0x28);
	ov2640_write_reg(0x13, 0xe5);
	ov2640_write_reg(0x14, 0x48);
	ov2640_write_reg(0x2c, 0x0c);
	ov2640_write_reg(0x33, 0x78);
	ov2640_write_reg(0x3a, 0x33);
	ov2640_write_reg(0x3b, 0xfb);
	ov2640_write_reg(0x3e, 0x00);
	ov2640_write_reg(0x43, 0x11);
	ov2640_write_reg(0x16, 0x10);
	ov2640_write_reg(0x39, 0x92);
	ov2640_write_reg(0x35, 0xda);
	ov2640_write_reg(0x22, 0x1a);
	ov2640_write_reg(0x37, 0xc3);
	ov2640_write_reg(0x23, 0x00);
	ov2640_write_reg(0x34, 0xc0);
	ov2640_write_reg(0x36, 0x1a);
	ov2640_write_reg(0x06, 0x88);
	ov2640_write_reg(0x07, 0xc0);
	ov2640_write_reg(0x0d, 0x87);
	ov2640_write_reg(0x0e, 0x41);
	ov2640_write_reg(0x4c, 0x00);
	ov2640_write_reg(0x4a, 0x81);
	ov2640_write_reg(0x21, 0x99);
	ov2640_write_reg(0x24, 0x40);
	ov2640_write_reg(0x25, 0x38);
	ov2640_write_reg(0x26, 0x82);
	ov2640_write_reg(0x5c, 0x00);
	ov2640_write_reg(0x63, 0x00);
	ov2640_write_reg(0x46, 0x22);
	ov2640_write_reg(0x0c, 0x3c);
	ov2640_write_reg(0x5d, 0x55);
	ov2640_write_reg(0x5e, 0x7d);
	ov2640_write_reg(0x5f, 0x7d);
	ov2640_write_reg(0x60, 0x55);
	ov2640_write_reg(0x61, 0x70);
	ov2640_write_reg(0x62, 0x80);
	ov2640_write_reg(0x7c, 0x05);
	ov2640_write_reg(0x20, 0x80);
	ov2640_write_reg(0x28, 0x30);
	ov2640_write_reg(0x6c, 0x00);
	ov2640_write_reg(0x6d, 0x80);
	ov2640_write_reg(0x6e, 0x00);
	ov2640_write_reg(0x70, 0x02);
	ov2640_write_reg(0x71, 0x94);
	ov2640_write_reg(0x73, 0xc1);
	ov2640_write_reg(0x12, 0x40);
	ov2640_write_reg(0x17, 0x11);
	ov2640_write_reg(0x18, 0x43);
	ov2640_write_reg(0x19, 0x00);
	ov2640_write_reg(0x1a, 0x4b);
	ov2640_write_reg(0x32, 0x09);
	ov2640_write_reg(0x37, 0xc0);
	ov2640_write_reg(0x4f, 0xca);
	ov2640_write_reg(0x50, 0xa8);
	ov2640_write_reg(0x6d, 0x00);
	ov2640_write_reg(0x3d, 0x38);
	ov2640_write_reg(0xff, 0x00);
	ov2640_write_reg(0xe5, 0x7f);
	ov2640_write_reg(0xf9, 0xc0);
	ov2640_write_reg(0x41, 0x24);
	ov2640_write_reg(0x44, 0x06);
	ov2640_write_reg(0xe0, 0x14);
	ov2640_write_reg(0x76, 0xff);
	ov2640_write_reg(0x33, 0xa0);
	ov2640_write_reg(0x42, 0x20);
	ov2640_write_reg(0x43, 0x18);
	ov2640_write_reg(0x4c, 0x00);
	ov2640_write_reg(0x87, 0xd0);
	ov2640_write_reg(0x88, 0x3f);
	ov2640_write_reg(0xd7, 0x03);
	ov2640_write_reg(0xd9, 0x10);
	ov2640_write_reg(0xd3, 0x82);
	ov2640_write_reg(0xc8, 0x08);
	ov2640_write_reg(0xc9, 0x80);
	ov2640_write_reg(0x7c, 0x00);
	ov2640_write_reg(0x7d, 0x00);
	ov2640_write_reg(0x7c, 0x03);
	ov2640_write_reg(0x7d, 0x48);
	ov2640_write_reg(0x7d, 0x48);
	ov2640_write_reg(0x7c, 0x08);
	ov2640_write_reg(0x7d, 0x20);
	ov2640_write_reg(0x7d, 0x10);
	ov2640_write_reg(0x7d, 0x0e);
	ov2640_write_reg(0x90, 0x00);
	ov2640_write_reg(0x91, 0x0e);
	ov2640_write_reg(0x91, 0x1a);
	ov2640_write_reg(0x91, 0x31);
	ov2640_write_reg(0x91, 0x5a);
	ov2640_write_reg(0x91, 0x69);
	ov2640_write_reg(0x91, 0x75);
	ov2640_write_reg(0x91, 0x7e);
	ov2640_write_reg(0x91, 0x88);
	ov2640_write_reg(0x91, 0x8f);
	ov2640_write_reg(0x91, 0x96);
	ov2640_write_reg(0x91, 0xa3);
	ov2640_write_reg(0x91, 0xaf);
	ov2640_write_reg(0x91, 0xc4);
	ov2640_write_reg(0x91, 0xd7);
	ov2640_write_reg(0x91, 0xe8);
	ov2640_write_reg(0x91, 0x20);

	ov2640_write_reg(0x92, 0x00);
	ov2640_write_reg(0x93, 0x06);
	ov2640_write_reg(0x93, 0xe3);
	ov2640_write_reg(0x93, 0x03);
	ov2640_write_reg(0x93, 0x03);
	ov2640_write_reg(0x93, 0x00);
	ov2640_write_reg(0x93, 0x02);
	ov2640_write_reg(0x93, 0x00);
	ov2640_write_reg(0x93, 0x00);
	ov2640_write_reg(0x93, 0x00);
	ov2640_write_reg(0x93, 0x00);
	ov2640_write_reg(0x93, 0x00);
	ov2640_write_reg(0x93, 0x00);
	ov2640_write_reg(0x93, 0x00);

	ov2640_write_reg(0x96, 0x00);
	ov2640_write_reg(0x97, 0x08);
	ov2640_write_reg(0x97, 0x19);
	ov2640_write_reg(0x97, 0x02);
	ov2640_write_reg(0x97, 0x0c);
	ov2640_write_reg(0x97, 0x24);
	ov2640_write_reg(0x97, 0x30);
	ov2640_write_reg(0x97, 0x28);
	ov2640_write_reg(0x97, 0x26);
	ov2640_write_reg(0x97, 0x02);
	ov2640_write_reg(0x97, 0x98);
	ov2640_write_reg(0x97, 0x80);
	ov2640_write_reg(0x97, 0x00);
	ov2640_write_reg(0x97, 0x00);

	ov2640_write_reg(0xa4, 0x00);
	ov2640_write_reg(0xa8, 0x00);
	ov2640_write_reg(0xc5, 0x11);
	ov2640_write_reg(0xc6, 0x51);
	ov2640_write_reg(0xbf, 0x80);
	ov2640_write_reg(0xc7, 0x10);
	ov2640_write_reg(0xb6, 0x66);
	ov2640_write_reg(0xb8, 0xa5);
	ov2640_write_reg(0xb7, 0x64);
	ov2640_write_reg(0xb9, 0x7c);
	ov2640_write_reg(0xb3, 0xaf);
	ov2640_write_reg(0xb4, 0x97);
	ov2640_write_reg(0xb5, 0xff);
	ov2640_write_reg(0xb0, 0xc5);
	ov2640_write_reg(0xb1, 0x94);
	ov2640_write_reg(0xb2, 0x0f);
	ov2640_write_reg(0xc4, 0x5c);

	ov2640_write_reg(0xa6, 0x00);
	ov2640_write_reg(0xa7, 0x20);
	ov2640_write_reg(0xa7, 0xd8);
	ov2640_write_reg(0xa7, 0x1b);
	ov2640_write_reg(0xa7, 0x31);
	ov2640_write_reg(0xa7, 0x00);
	ov2640_write_reg(0xa7, 0x18);
	ov2640_write_reg(0xa7, 0x20);
	ov2640_write_reg(0xa7, 0xd8);
	ov2640_write_reg(0xa7, 0x19);
	ov2640_write_reg(0xa7, 0x31);
	ov2640_write_reg(0xa7, 0x00);
	ov2640_write_reg(0xa7, 0x18);
	ov2640_write_reg(0xa7, 0x20);
	ov2640_write_reg(0xa7, 0xd8);
	ov2640_write_reg(0xa7, 0x19);
	ov2640_write_reg(0xa7, 0x31);
	ov2640_write_reg(0xa7, 0x00);
	ov2640_write_reg(0xa7, 0x18);

	ov2640_write_reg(0xc0, 0x64);
	ov2640_write_reg(0xc1, 0x4b);
	ov2640_write_reg(0x86, 0x1d);
	ov2640_write_reg(0x50, 0x00);
	ov2640_write_reg(0x51, 0xc8);
	ov2640_write_reg(0x52, 0x96);
	ov2640_write_reg(0x53, 0x00);
	ov2640_write_reg(0x54, 0x00);
	ov2640_write_reg(0x55, 0x00);
	ov2640_write_reg(0x57, 0x00);
	ov2640_write_reg(0x5a, 0xc8);
	ov2640_write_reg(0x5b, 0x96);
	ov2640_write_reg(0x5c, 0x00);
	ov2640_write_reg(0xc3, 0xef);
	ov2640_write_reg(0x7f, 0x00);
	ov2640_write_reg(0xda, 0x01);
	ov2640_write_reg(0xe5, 0x1f);
	ov2640_write_reg(0xe1, 0x67);
	ov2640_write_reg(0xe0, 0x00);
	ov2640_write_reg(0xdd, 0x7f);
	ov2640_write_reg(0x05, 0x00);

	return 0;
}

/*!
 * ov2640 sensor interface Initialization
 * @param param            sensor_interface *
 * @param width            u32
 * @param height           u32
 * @return  None
 */
static void ov2640_interface(sensor_interface * param, u32 width, u32 height)
{
	param->Vsync_pol = 0x0;
	param->clk_mode = 0x0;	/* gated */
	param->pixclk_pol = 0x0;
	param->data_width = 0x1;
	param->data_pol = 0x0;
	param->ext_vsync = 0x0;
	param->Vsync_pol = 0x0;
	param->Hsync_pol = 0x0;
	param->width = width - 1;
	param->height = height - 1;
	param->pixel_fmt = IPU_PIX_FMT_UYVY;
	param->mclk = 27000000;
}

static void
ov2640_set_color(int bright, int saturation, int red, int green, int blue)
{
	return;
}

static void
ov2640_get_color(int *bright, int *saturation, int *red, int *green, int *blue)
{
	return;
}

static void ov2640_set_ae_mode(int ae_mode)
{
	return;
}

static void ov2640_get_ae_mode(int *ae_mode)
{
	return;
}

extern void gpio_sensor_active(void);
extern cam_data *g_cam;

static sensor_interface *ov2640_config(int *frame_rate, int high_quality)
{
	u32 out_width, out_height;
	PMIC_STATUS ret;
	t_regulator_voltage voltage;

	/* AVDD--2.8v */
	voltage.vmmc1 = VMMC1_2_8V;
	if ((ret = pmic_power_regulator_set_voltage(REGU_VMMC1, voltage)) < 0) {
		PRINTK("%s:vmmc1 set voltage error:%d\n", __func__, ret);
		return NULL;
	} else {
		PRINTK("%s:vmmc1 set voltage ok\n", __func__);
	}

	if ((ret = pmic_power_regulator_on(REGU_VMMC1)) < 0) {
		PRINTK("%s:vmmc1 power on error:%d\n", __func__, ret);
		return NULL;
	} else {
		PRINTK("%s:vmmc1 power on ok\n", __func__);
	}

	/* DVDD--1.3v */
	voltage.vvib = VVIB_1_3V;
	if ((ret = pmic_power_regulator_set_voltage(REGU_VVIB, voltage)) < 0) {
		PRINTK("%s:VVIB set voltage error:%d\n", __func__, ret);
		return NULL;
	} else {
		PRINTK("%s:VVIB set voltage ok\n", __func__);
	}
	if ((ret = pmic_power_regulator_on(REGU_VVIB)) < 0) {
		PRINTK("%s:VVIB power regulator on error:%d\n", __func__, ret);
		return NULL;
	} else {
		PRINTK("%s:VVIB power on ok\n", __func__);
	}

	/* DOVDD--2v(1.8-3.0) */
	voltage.sw2b = SW2B_2V;
	if ((ret = pmic_power_regulator_set_voltage(SW_SW2B, voltage)) < 0) {
		PRINTK("%s:SW2B set voltage error:%d\n", __func__, ret);
		return NULL;
	} else {
		PRINTK("%s:SW2B set voltage ok\n", __func__);
	}
	if (pmic_power_set_regen_assig(REGU_GPO3, 1) < 0) {
		PRINTK("%s:set_regen_assig error\n", __func__);
		return NULL;
	} else {
		PRINTK("%s:set_regen_assig ok\n", __func__);
	}
	if ((ret = pmic_power_regulator_on(REGU_GPO3)) < 0) {
		PRINTK("%s:REGU_GPO3 power on error:%d\n", __func__, ret);
		return NULL;
	} else {
		PRINTK("%s:REGU_GPO3 power on ok\n", __func__);
	}
	PRINTK("%s:OV2640 power on ok\n", __func__);

	if (high_quality) {
		out_width = 1600;
		out_height = 1120;
		g_cam->crop_bounds.left = 0;
		g_cam->crop_bounds.width = 1600;
		g_cam->crop_bounds.top = 0;
		g_cam->crop_bounds.height = 1120;
		g_cam->crop_current = g_cam->crop_defrect = g_cam->crop_bounds;
#ifdef CONFIG_ARCH_MX3
		ipu_csi_set_window_size(g_cam->crop_current.width,
					g_cam->crop_current.height);
		ipu_csi_set_window_pos(g_cam->crop_current.left,
				       g_cam->crop_current.top);
#endif
		g_cam->streamparm.parm.capture.capturemode = 1;
	} else {
		out_width = 640;
		out_height = 480;
		g_cam->crop_bounds.left = 0;
		g_cam->crop_bounds.width = 640;
		g_cam->crop_bounds.top = 0;
		g_cam->crop_bounds.height = 480;
		g_cam->crop_current = g_cam->crop_defrect = g_cam->crop_bounds;
#ifdef CONFIG_ARCH_MX3
		ipu_csi_set_window_size(g_cam->crop_current.width,
					g_cam->crop_current.height);
		ipu_csi_set_window_pos(g_cam->crop_current.left,
				       g_cam->crop_current.top);
#endif
		g_cam->streamparm.parm.capture.capturemode = 0;
	}
	ov2640_interface(interface_param, out_width, out_height);
	set_mclk_rate(&interface_param->mclk);

	if (high_quality) {
		ov2640_init_1600_1120();
	} else {
		ov2640_init_800_600();
	}
	return interface_param;
}

static void ov2640_get_control_params(int *ae, int *awb, int *flicker)
{
	*ae = 0;
	*awb = 0;
	*flicker = 0;
}

static sensor_interface *ov2640_reset(void)
{
	return ov2640_config(&reset_frame_rate, 0);
}

static int ov2640_get_status(void)
{
	return 0;
}

struct camera_sensor camera_sensor_if = {
	.set_color = ov2640_set_color,
	.get_color = ov2640_get_color,
	.set_ae_mode = ov2640_set_ae_mode,
	.get_ae_mode = ov2640_get_ae_mode,
	.get_control_params = ov2640_get_control_params,
	.config = ov2640_config,
	.reset = ov2640_reset,
	.get_status = ov2640_get_status,
};

EXPORT_SYMBOL(camera_sensor_if);

/*!
 * ov2640 init function
 *
 * @return  Error code indicating success or failure
 */
static __init int ov2640_init(void)
{
	u8 err;

	gpio_sensor_active();

	err = i2c_add_driver(&ov2640_i2c_driver);

	return err;
}

extern void gpio_sensor_inactive(void);
/*!
 * OV2640 cleanup function
 *
 * @return  Error code indicating success or failure
 */
static void __exit ov2640_clean(void)
{
	i2c_del_driver(&ov2640_i2c_driver);

	gpio_sensor_inactive();
}

module_init(ov2640_init);
module_exit(ov2640_clean);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("OV2640 Camera Driver");
MODULE_LICENSE("GPL");
