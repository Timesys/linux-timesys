/*
 * File: dvf-tda1998x.c
 *
 * Description: This file contains condensed support for the TDA1998X HDMI codec required for display
 * from the Freescale Vybrid DCU4 output. It assumes the i2c adapter access routines are available.
 */
// CNC TODO Header?

//TODO Pare this down CNC
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/console.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <asm/mach-types.h>
#include <mach/mvf.h>
#include <mach/mvf-dcu-fb.h>

//Required
#include <linux/i2c.h>
#include "mvf-tda1998x.h"


/* Global Data */
static struct i2c_adapter *g_i2c_adapter = NULL;
static u8 g_u8_curr_page = 0;

/* Static Functions */
static void
set_page(uint16_t reg)
{
	if (REG2PAGE(reg) != g_u8_curr_page) {
		uint8_t buf[2] = { REG_CURPAGE, REG2PAGE(reg) };
        struct i2c_msg msg = {TDA1998X_I2C_HDMI_SA, 0, sizeof(buf), buf};
        int ret;
            
        ret = i2c_transfer(g_i2c_adapter, &msg, 1);
		if (ret < 0)
        {
			printk(KERN_ERR "Error %d writing to REG_CURPAGE\n", ret);
        }
        else
        {
            g_u8_curr_page = REG2PAGE(reg);
        }
	}
}

static void
reg_write(uint16_t reg, uint8_t val)
{
	uint8_t buf[2] = {REG2ADDR(reg), val};
	int ret;
    struct i2c_msg msg = {TDA1998X_I2C_HDMI_SA, 0, sizeof(buf), buf};

	set_page(reg);

    ret = i2c_transfer(g_i2c_adapter, &msg, 1);
	if (ret < 0)
		printk(KERN_ERR "Error %d writing to 0x%x\n", ret, reg);
}

static uint8_t
reg_read(uint16_t reg)
{
	uint8_t u8addr = REG2ADDR(reg);
	uint8_t u8ret = 0;
	int ret;
    struct i2c_msg rgMsg[] = {/* slaveaddr, flags, len, buf */
                              {TDA1998X_I2C_HDMI_SA, 0,        sizeof(u8addr), &u8addr},
                              {TDA1998X_I2C_HDMI_SA, I2C_M_RD, sizeof(u8ret),  &u8ret}};

    /* Change the register page to the one we want */
	set_page(reg);

    ret = i2c_transfer(g_i2c_adapter, rgMsg, 2);
	if (ret < 0)
    {
        printk(KERN_ERR "<%s> Error %d reading from 0x%x\n", __func__, ret, reg);
        return 0;
    }
    else
    {
        return u8ret;
    }
}

static void
reg_set(uint16_t reg, uint8_t val)
{
	reg_write(reg, reg_read(reg) | val);
}

static void
reg_clear(uint16_t reg, uint8_t val)
{
	reg_write(reg, reg_read(reg) & ~val);
}

static bool
tda1998x_init_encoder(void)
{
	int ret;
    uint8_t rgBuf[][2] = { /* Register Address, Register Value */
                          {REG_CEC_ENAMODS,         (CEC_ENAMODS_EN_RXSENS 
                                                     | CEC_ENAMODS_EN_HDMI)},
                          {REG_CEC_FRO_IM_CLK_CTRL, (CEC_FRO_IM_CLK_CTRL_GHOST_DIS 
                                                     | CEC_FRO_IM_CLK_CTRL_IMCLK_SEL)},
                          {REG_CEC_ENAMODS,         (CEC_ENAMODS_EN_CEC_CLK 
                                                     | CEC_ENAMODS_EN_RXSENS 
                                                     | CEC_ENAMODS_EN_HDMI  
                                                     | CEC_ENAMODS_EN_CEC)}
                         };
    struct i2c_msg rgMsg[] = 
    {
        /* slaveaddr,flags,len,buf */
        {TDA1998X_I2C_CEC_SA, 0, sizeof(rgBuf[0]), rgBuf[0]},
        {TDA1998X_I2C_CEC_SA, 0, sizeof(rgBuf[1]), rgBuf[1]},
        {TDA1998X_I2C_CEC_SA, 0, sizeof(rgBuf[2]), rgBuf[2]},
    };

    /* Initialize the HDMI interface, otherwise the HDMI I2C I/F is off */
    ret = i2c_transfer(g_i2c_adapter, &rgMsg[0], 1);
    if (ret < 0)
        goto ret_error;

    /*
     * Begin Reset Sequence 
     */
	/* reset audio and i2c master: */
	reg_set(REG_SOFTRESET, SOFTRESET_AUDIO | SOFTRESET_I2C_MASTER);
	msleep(50);
	reg_clear(REG_SOFTRESET, SOFTRESET_AUDIO | SOFTRESET_I2C_MASTER);
	msleep(50);

	/* reset transmitter */
	reg_set(REG_MAIN_CNTRL0, MAIN_CNTRL0_SR);
	reg_clear(REG_MAIN_CNTRL0, MAIN_CNTRL0_SR);

	/* PLL registers common configuration */
	reg_write(REG_PLL_SERIAL_1, 0x00);
	reg_write(REG_PLL_SERIAL_2, PLL_SERIAL_2_SRL_NOSC(1));
	reg_write(REG_PLL_SERIAL_3, 0x00);
	reg_write(REG_SERIALIZER,   0x00);
	reg_write(REG_BUFFER_OUT,   0x00);
	reg_write(REG_PLL_SCG1,     0x00);
	reg_write(REG_AUDIO_DIV,    0x03);
	reg_write(REG_SEL_CLK,      SEL_CLK_SEL_CLK1 | SEL_CLK_ENA_SC_CLK);
	reg_write(REG_PLL_SCGN1,    0xfa);
	reg_write(REG_PLL_SCGN2,    0x00);
	reg_write(REG_PLL_SCGR1,    0x5b);
	reg_write(REG_PLL_SCGR2,    0x00);
	reg_write(REG_PLL_SCG2,     0x10);
    /* 
     * End Reset Sequence 
     */

    /* Enable DDC */
	reg_write(REG_DDC_DISABLE, 0x00);

	/* Set clock on DDC channel */
	reg_write(REG_TX3, 39);

    /* Initialize CEC Interface */
    ret = i2c_transfer(g_i2c_adapter, &rgMsg[1], 1);
    if (ret < 0)
        goto ret_error;

    /* Enable DPMS */
    /* enable video ports */
    reg_write(REG_ENA_VP_0, 0xff);
    reg_write(REG_ENA_VP_1, 0xff);
    reg_write(REG_ENA_VP_2, 0xff);
    /* set muxing after enabling ports: */
    /* Set to RGB 3x 8-bit per 7.2.3 mappings */
    reg_write(REG_VIP_CNTRL_0, 0x23);
    reg_write(REG_VIP_CNTRL_1, 0x01);
    reg_write(REG_VIP_CNTRL_2, 0x45);

    /* Enable HDMI Encoding */
    ret = i2c_transfer(g_i2c_adapter, &rgMsg[2], 1);
    if (ret < 0)
        goto ret_error;

    reg_write(REG_TX33, TX33_HDMI);
    reg_write(REG_TBG_CNTRL_1, 0x7c);

    return true;

ret_error:
    printk(KERN_ERR "<%s> HDMI I2C I/F failed (%d)\n", __func__, ret);
    return false;
} /* tda1998x_init_encoder */

/* Public Functions */
bool init_tda19988(void)
{
    /* Get the handle to the i2c channel */
    g_i2c_adapter = i2c_get_adapter(TDA1998X_I2C_BUS); 
    if (g_i2c_adapter == NULL)
    {
        printk(KERN_ERR "<%s> CEC I2C Adapter not found\n", __func__);
        return false;
    }
    return tda1998x_init_encoder();
}

