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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <mach/arc_otg.h>
#include <mach/hardware.h>
#include <mach/clock.h>
#include "devices-mvf.h"
#include "crm_regs.h"
#include "regs-anadig.h"
#include "usb.h"

static int usbotg_init_ext(struct platform_device *pdev);
static void usbotg_uninit_ext(struct platform_device *pdev);
static void usbotg_clock_gate(bool on);
static void _dr_discharge_line(bool enable);
static void usbotg_wakeup_event_clear(void);

/* The usb_phy0_clk do not have enable/disable function at clock.c
 * and PLL output for usb0's phy should be always enabled.
 * usb_phy0_clk only stands for usb uses pll3 as its parent.
 */
static struct clk *usb_phy0_clk;
static u8 otg_used;

/* Beginning of Common operation for DR port */

/*
 * platform data structs
 * - Which one to use is determined by CONFIG options in usb.h
 * - operating_mode plugged at run time
 */
static struct fsl_usb2_platform_data dr_utmi_config = {
	.name              = "DR",
	.init              = usbotg_init_ext,
	.exit              = usbotg_uninit_ext,
	.phy_mode          = FSL_USB2_PHY_UTMI_WIDE,
	.power_budget      = 500,		/* 500 mA max power */
	.usb_clock_for_pm  = usbotg_clock_gate,
	.transceiver       = "utmi",
	.phy_regs = MVF_USBPHY0_BASE_ADDR,
	.dr_discharge_line = _dr_discharge_line,
};

/* Platform data for wakeup operation */
static struct fsl_usb2_wakeup_platform_data dr_wakeup_config = {
	.name = "Gadget wakeup",
	.usb_clock_for_pm  = usbotg_clock_gate,
	.usb_wakeup_exhandle = usbotg_wakeup_event_clear,
};

static void usbotg_internal_phy_clock_gate(bool on)
{
	u32 reg;

	void __iomem *phy_reg = MVF_IO_ADDRESS(MVF_USBPHY0_BASE_ADDR);
	reg = __raw_readl(phy_reg + HW_USBPHY_CTRL);

	if (on)
		reg &= ~BM_USBPHY_CTRL_CLKGATE;
	else
		reg |= BM_USBPHY_CTRL_CLKGATE;

	__raw_writel(reg, phy_reg + HW_USBPHY_CTRL);
}

static int usb_phy_enable(struct fsl_usb2_platform_data *pdata)
{
	u32 tmp;
	void __iomem *phy_reg = MVF_IO_ADDRESS(MVF_USBPHY0_BASE_ADDR);
	void __iomem *phy_ctrl;

	/* Stop then Reset */
	UOG_USBCMD &= ~UCMD_RUN_STOP;
	while (UOG_USBCMD & UCMD_RUN_STOP)
		;

	UOG_USBCMD |= UCMD_RESET;
	while ((UOG_USBCMD) & (UCMD_RESET))
		;
	/* Reset USBPHY module */
	phy_ctrl = phy_reg + HW_USBPHY_CTRL;
	tmp = __raw_readl(phy_ctrl);
	tmp |= BM_USBPHY_CTRL_SFTRST;
	__raw_writel(tmp, phy_ctrl);
	udelay(10);

	/* Remove CLKGATE and SFTRST */
	tmp = __raw_readl(phy_ctrl);
	tmp &= ~(BM_USBPHY_CTRL_CLKGATE | BM_USBPHY_CTRL_SFTRST);
	__raw_writel(tmp, phy_ctrl);
	udelay(10);

	/* Power up the PHY */
	__raw_writel(0, phy_reg + HW_USBPHY_PWD);

	return 0;
}
/* Notes: configure USB clock*/
static int usbotg_init_ext(struct platform_device *pdev)
{
	struct clk *usb_clk;
	u32 ret;

	usb_clk = clk_get(NULL, "mvf-usb.0");
	clk_enable(usb_clk);
	usb_phy0_clk = usb_clk;

	ret = usbotg_init(pdev);
	if (ret) {
		printk(KERN_ERR "otg init fails......\n");
		return ret;
	}
	if (!otg_used) {
		usbotg_internal_phy_clock_gate(true);
		usb_phy_enable(pdev->dev.platform_data);
		/*
		 * after the phy reset,can not read the value for id/vbus at
		 * the register of otgsc ,cannot  read at once ,need delay 3 ms
		 */
		mdelay(3);
	}
	otg_used++;

	return ret;
}

static void usbotg_uninit_ext(struct platform_device *pdev)
{
	struct fsl_usb2_platform_data *pdata = pdev->dev.platform_data;

	clk_disable(usb_phy0_clk);
	clk_put(usb_phy0_clk);

	usbotg_uninit(pdata);
	otg_used--;
}

static void usbotg_clock_gate(bool on)
{
	pr_debug("%s: on is %d\n", __func__, on);
	if (on)
		clk_enable(usb_phy0_clk);
	else
		clk_disable(usb_phy0_clk);
}
/*
void mvf_set_otghost_vbus_func(driver_vbus_func driver_vbus)
{
	dr_utmi_config.platform_driver_vbus = driver_vbus;
}
*/
static void _dr_discharge_line(bool enable)
{
	void __iomem *phy_reg = MVF_IO_ADDRESS(MVF_USBPHY0_BASE_ADDR);
	if (enable) {
		__raw_writel(BF_USBPHY_DEBUG_ENHSTPULLDOWN(0x3),
				phy_reg + HW_USBPHY_DEBUG_SET);
		__raw_writel(BF_USBPHY_DEBUG_HSTPULLDOWN(0x3),
				phy_reg + HW_USBPHY_DEBUG_SET);
	} else {
		__raw_writel(BF_USBPHY_DEBUG_ENHSTPULLDOWN(0x3),
				phy_reg + HW_USBPHY_DEBUG_CLR);
		__raw_writel(BF_USBPHY_DEBUG_HSTPULLDOWN(0x3),
				phy_reg + HW_USBPHY_DEBUG_CLR);
	}

}

/* Below two macros are used at otg mode to indicate usb mode*/
#define ENABLED_BY_HOST   (0x1 << 0)
#define ENABLED_BY_DEVICE (0x1 << 1)
static u32 low_power_enable_src; /* only useful at otg mode */
static void enter_phy_lowpower_suspend(struct fsl_usb2_platform_data *pdata,
					bool enable)
{
	void __iomem *phy_reg = MVF_IO_ADDRESS(MVF_USBPHY0_BASE_ADDR);
	u32 tmp;
	pr_debug("DR: %s begins, enable is %d\n", __func__, enable);

	if (enable) {
		UOG_PORTSC1 |= PORTSC_PHCD;
		tmp = (BM_USBPHY_PWD_TXPWDFS
			| BM_USBPHY_PWD_TXPWDIBIAS
			| BM_USBPHY_PWD_TXPWDV2I
			| BM_USBPHY_PWD_RXPWDENV
			| BM_USBPHY_PWD_RXPWD1PT1
			| BM_USBPHY_PWD_RXPWDDIFF
			| BM_USBPHY_PWD_RXPWDRX);
		__raw_writel(tmp, phy_reg + HW_USBPHY_PWD_SET);
		usbotg_internal_phy_clock_gate(false);

	} else {
		if (UOG_PORTSC1 & PORTSC_PHCD) {
			UOG_PORTSC1 &= ~PORTSC_PHCD;
			mdelay(1);
		}
		usbotg_internal_phy_clock_gate(true);
		tmp = (BM_USBPHY_PWD_TXPWDFS
			| BM_USBPHY_PWD_TXPWDIBIAS
			| BM_USBPHY_PWD_TXPWDV2I
			| BM_USBPHY_PWD_RXPWDENV
			| BM_USBPHY_PWD_RXPWD1PT1
			| BM_USBPHY_PWD_RXPWDDIFF
			| BM_USBPHY_PWD_RXPWDRX);
		__raw_writel(tmp, phy_reg + HW_USBPHY_PWD_CLR);

	}
	pr_debug("DR: %s ends, enable is %d\n", __func__, enable);
}

static void __phy_lowpower_suspend(struct fsl_usb2_platform_data *pdata,
					bool enable, int source)
{
	if (enable) {
		low_power_enable_src |= source;
		enter_phy_lowpower_suspend(pdata, enable);
	} else {
		enter_phy_lowpower_suspend(pdata, enable);
		low_power_enable_src &= ~source;
	}
}

static void otg_wake_up_enable(struct fsl_usb2_platform_data *pdata,
				bool enable)
{
	void __iomem *phy_reg = MVF_IO_ADDRESS(MVF_USBPHY0_BASE_ADDR);

	pr_debug("%s, enable is %d\n", __func__, enable);
	if (enable) {
		__raw_writel(BM_USBPHY_CTRL_ENIDCHG_WKUP
				| BM_USBPHY_CTRL_ENVBUSCHG_WKUP
				| BM_USBPHY_CTRL_ENDPDMCHG_WKUP
				| BM_USBPHY_CTRL_ENAUTOSET_USBCLKS
				| BM_USBPHY_CTRL_ENAUTOCLR_PHY_PWD
				| BM_USBPHY_CTRL_ENAUTOCLR_CLKGATE
				| BM_USBPHY_CTRL_ENAUTOCLR_USBCLKGATE
				| BM_USBPHY_CTRL_ENAUTO_PWRON_PLL,
				phy_reg + HW_USBPHY_CTRL_SET);
		USBC0_CTRL |= UCTRL_OWIE;
	} else {
		USBC0_CTRL &= ~UCTRL_OWIE;
		/* The interrupt must be disabled for at least 3 clock
		 * cycles of the standby clock(32k Hz) , that is 0.094 ms*/
		udelay(100);
	}
}

static void __wakeup_irq_enable(struct fsl_usb2_platform_data *pdata,
				bool on, int source)
{
	/* otg host and device share the OWIE bit, only when host and device
	 * all enable the wakeup irq, we can enable the OWIE bit
	 */
	 if (on) {
		otg_wake_up_enable(pdata, on);
	} else {
		otg_wake_up_enable(pdata, on);
		/* The interrupt must be disabled for at least 3 clock
		 * cycles of the standby clock(32k Hz) , that is 0.094 ms*/
		udelay(100);
	}
}

/* The wakeup operation for DR port, it will clear the wakeup irq status
 * and re-enable the wakeup
 */
static void usbotg_wakeup_event_clear(void)
{
	int wakeup_req = USBC0_CTRL & UCTRL_OWIR;

	if (wakeup_req != 0) {
		pr_debug("Unknown wakeup.(OTGSC 0x%x)\n", UOG_OTGSC);
		/* Disable OWIE to clear OWIR, wait 3 clock
		 * cycles of standly clock(32KHz)
		 */
		USBC0_CTRL &= ~UCTRL_OWIE;
		udelay(100);
		USBC0_CTRL |= UCTRL_OWIE;
	}
}

/* End of Common operation for DR port */
#ifdef CONFIG_USB_EHCI_ARC_OTG
#endif /* CONFIG_USB_EHCI_ARC_OTG */


#ifdef CONFIG_USB_GADGET_ARC
/* Beginning of device related operation for DR port */
static void _device_phy_lowpower_suspend(struct fsl_usb2_platform_data *pdata,
					bool enable)
{
	__phy_lowpower_suspend(pdata, enable, ENABLED_BY_DEVICE);
}

static void _device_wakeup_enable(struct fsl_usb2_platform_data *pdata,
				bool enable)
{
	__wakeup_irq_enable(pdata, enable, ENABLED_BY_DEVICE);
	if (enable) {
		pr_debug("device wakeup enable\n");
		device_wakeup_enable(&pdata->pdev->dev);
	} else {
		pr_debug("device wakeup disable\n");
		device_wakeup_disable(&pdata->pdev->dev);
	}
}

static enum usb_wakeup_event
_is_device_wakeup(struct fsl_usb2_platform_data *pdata)
{
	int wakeup_req = USBC0_CTRL & UCTRL_OWIR;

	/* if ID=1, it is a device wakeup event */
	if (wakeup_req && (UOG_OTGSC & OTGSC_STS_USB_ID) &&
			(UOG_USBSTS & USBSTS_URI)) {
		printk(KERN_INFO "otg udc wakeup, host sends reset signal\n");
		return WAKEUP_EVENT_DPDM;
	}
	if (wakeup_req && (UOG_OTGSC & OTGSC_STS_USB_ID) &&  \
		((UOG_USBSTS & USBSTS_PCI) ||
		 (UOG_PORTSC1 & PORTSC_PORT_FORCE_RESUME))) {
		/*
		 * When the line state from J to K, the Port Change Detect bit
		 * in the USBSTS register is also set to '1'.
		 */
		printk(KERN_INFO "otg udc wakeup, host sends resume signal\n");
		return WAKEUP_EVENT_DPDM;
	}
	if (wakeup_req && (UOG_OTGSC & OTGSC_STS_USB_ID) &&
			(UOG_OTGSC & OTGSC_STS_A_VBUS_VALID) &&
			(UOG_OTGSC & OTGSC_IS_B_SESSION_VALID)) {
		printk(KERN_INFO "otg udc vbus rising wakeup\n");
		return WAKEUP_EVENT_VBUS;
	}
	if (wakeup_req && (UOG_OTGSC & OTGSC_STS_USB_ID) &&
			!(UOG_OTGSC & OTGSC_STS_A_VBUS_VALID)) {
		printk(KERN_INFO "otg udc vbus falling wakeup\n");
		return WAKEUP_EVENT_VBUS;
	}

	return WAKEUP_EVENT_DPDM;
}

static void device_wakeup_handler(struct fsl_usb2_platform_data *pdata)
{
	_device_phy_lowpower_suspend(pdata, false);
	_device_wakeup_enable(pdata, false);
}
/* end of device related operation for DR port */
#endif /* CONFIG_USB_GADGET_ARC */

void __init mvf_usb_dr_init(void)
{
	struct platform_device *pdev, *pdev_wakeup;
	u32 reg;
#ifdef CONFIG_USB_GADGET_ARC
	dr_utmi_config.operating_mode = DR_UDC_MODE;
	dr_utmi_config.wake_up_enable = _device_wakeup_enable;
	dr_utmi_config.platform_suspend = NULL;
	dr_utmi_config.platform_resume  = NULL;
	dr_utmi_config.phy_lowpower_suspend = _device_phy_lowpower_suspend;
	dr_utmi_config.is_wakeup_event = _is_device_wakeup;
	dr_utmi_config.wakeup_pdata = &dr_wakeup_config;
	dr_utmi_config.wakeup_handler = device_wakeup_handler;

	pdev = mvf_add_fsl_usb2_udc(0, &dr_utmi_config);

	dr_wakeup_config.usb_pdata[2] = pdev->dev.platform_data;

	/* register wakeup device */
	pdev_wakeup = mvf_add_fsl_usb2_udc_wakeup(0, &dr_wakeup_config);
	if (pdev != NULL)
		((struct fsl_usb2_platform_data *)
			(pdev->dev.platform_data))->wakeup_pdata =
				(struct fsl_usb2_wakeup_platform_data *)
					(pdev_wakeup->dev.platform_data);

	device_set_wakeup_capable(&pdev->dev, true);
	__raw_writel(((0x01 << 30) | 0x2), ANADIG_USB1_MISC);

	__raw_writel(0x00000894, USBPHY1_CTRL);
	udelay(10);

	__raw_writel(0x0, USBPHY1_PWD);
	__raw_writel(0x1, USBPHY1_IP);
	udelay(20);
	__raw_writel(0x7, USBPHY1_IP);

	reg = __raw_readl(USBPHY1_DEBUG);
	reg &= ~0x40000000; /* clear gate clock */
	__raw_writel(reg, USBPHY1_DEBUG);

	__raw_writel(0x10000007, USBPHY1_TX);
	reg = __raw_readl(USBPHY1_CTRL);
	reg |= ((0xD1 << 11) | 0x6);
	__raw_writel(reg, USBPHY1_CTRL);

	reg = __raw_readl(ANADIG_USB1_VBUS_DETECT);
	reg |= 0xE8;
	__raw_writel(reg, ANADIG_USB1_VBUS_DETECT);
	__raw_writel(0x001c0000, ANADIG_USB1_CHRG_DETECT);
#endif

}
