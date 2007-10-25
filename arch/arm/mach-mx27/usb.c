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
 * @defgroup USB_MX27 ARC OTG USB Driver for i.MX27
 * @ingroup USB
 */

/*!
 * @file mach-mx27/usb.c
 *
 * @brief platform related part of usb driver.
 * @ingroup USB_MX27
 */

/*!
 *Include files
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/usb/fsl_xcvr.h>
#include <linux/usb/otg.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/arch/arc_otg.h>

extern struct platform_device *host_pdev_register(struct resource *res,
						  int n_res,
						  struct fsl_usb2_platform_data
						  *config);
extern int fsl_usb_host_init(struct platform_device *pdev);
extern void fsl_usb_host_uninit(struct fsl_usb2_platform_data *pdata);
extern int usbotg_init(struct platform_device *pdev);
extern void usbotg_uninit(struct fsl_usb2_platform_data *pdata);
extern int gpio_usbh1_active(void);
extern void gpio_usbh1_inactive(void);
extern int gpio_usbh2_active(void);
extern void gpio_usbh2_inactive(void);
extern int gpio_usbotg_hs_active(void);
extern void gpio_usbotg_hs_inactive(void);
extern int gpio_usbotg_fs_active(void);
extern void gpio_usbotg_fs_inactive(void);

#ifdef CONFIG_USB_EHCI_ARC_H1
/*!
 * HOST1 config
 */
/* *INDENT-OFF* */
static struct fsl_usb2_platform_data usbh1_config = {
	.name              = "Host 1",
	.platform_init     = fsl_usb_host_init,
	.platform_uninit   = fsl_usb_host_uninit,
	.usbmode           = (u32) &UH1_USBMODE,
	.power_budget      = 500,		/* 500 mA max power */
	.gpio_usb_active   = gpio_usbh1_active,
	.gpio_usb_inactive = gpio_usbh1_inactive,
	.transceiver       = "serial",
};

static struct resource usbh1_resources[] = {
	{
		.start = (u32) (USB_H1REGS_BASE),
		.end   = (u32) (USB_H1REGS_BASE + 0x1ff),
		.flags = IORESOURCE_MEM,
	},
	{
		.start = INT_USB1,
		.flags = IORESOURCE_IRQ,
	},
};
/* *INDENT-ON* */
#endif

#ifdef CONFIG_USB_EHCI_ARC_H2
/*!
 * HOST2 config
 */
/* *INDENT-OFF* */
static struct fsl_usb2_platform_data usbh2_config = {
	.name              = "Host 2",
	.platform_init     = fsl_usb_host_init,
	.platform_uninit   = fsl_usb_host_uninit,
	.usbmode           = (u32) &UH2_USBMODE,
	.viewport          = (u32) &UH2_ULPIVIEW,
	.power_budget      = 500,  /* 500 mA max power */
	.gpio_usb_active   = gpio_usbh2_active,
	.gpio_usb_inactive = gpio_usbh2_inactive,
	.transceiver       = "isp1504",
};

static struct resource usbh2_resources[] = {
	{
		.start = (u32) (USB_H2REGS_BASE),
		.end   = (u32) (USB_H2REGS_BASE + 0x1ff),
		.flags = IORESOURCE_MEM,
	},
	{
		.start = INT_USB2,
		.flags = IORESOURCE_IRQ,
	},
};
/* *INDENT-ON* */
#endif

/*!
 * OTG config
 */
/* *INDENT-OFF* */
#if defined(CONFIG_USB_EHCI_ARC_OTG) || defined(CONFIG_USB_GADGET_ARC) || defined(CONFIG_OTG_BTC_ARC)
#if defined(CONFIG_MC13783_MXC)
static struct fsl_usb2_platform_data mxc_serial_host_config = {
	.name              = "OTG",
	.platform_init     = usbotg_init,
	.platform_uninit   = usbotg_uninit,
	.usbmode           = (u32) &UOG_USBMODE,
	.viewport          = (u32) &UOG_ULPIVIEW,
	.does_otg          = 1,
	.operating_mode    = FSL_USB2_DR_HOST,
	.power_budget      = 500,  /* 500 mA max power */
	.gpio_usb_active   = gpio_usbotg_fs_active,
	.gpio_usb_inactive = gpio_usbotg_fs_inactive,
	.transceiver       = "mc13783",
};
#elif defined(CONFIG_ISP1301_MXC)
static struct fsl_usb2_platform_data mxc_serial_host_config = {
	.name              = "OTG",
	.platform_init     = usbotg_init,
	.platform_uninit   = usbotg_uninit,
	.usbmode           = (u32) &UOG_USBMODE,
	.viewport          = (u32) &UOG_ULPIVIEW,
	.does_otg          = 0,
	.operating_mode    = FSL_USB2_DR_HOST,
	.power_budget      = 500,  /* 500 mA max power */
	.gpio_usb_active   = gpio_usbotg_fs_active,
	.gpio_usb_inactive = gpio_usbotg_fs_inactive,
	.transceiver       = "isp1301",
};
#endif
#if defined(CONFIG_ISP1504_MXC)
static struct fsl_usb2_platform_data mxc_isp1504_config = {
	.name              = "OTG",
	.platform_init     = usbotg_init,
	.platform_uninit   = usbotg_uninit,
	.usbmode           = (u32) &UOG_USBMODE,
	.viewport          = (u32) &UOG_ULPIVIEW,
	.does_otg          = 1,
	.power_budget      = 500,  /* 500 mA max power */
	.gpio_usb_active   = gpio_usbotg_hs_active,
	.gpio_usb_inactive = gpio_usbotg_hs_inactive,
	.transceiver       = "isp1504",
};
#endif

static struct resource otg_resources[] = {
	{
		.start = (u32) (USB_OTGREGS_BASE),
		.end   = (u32) (USB_OTGREGS_BASE + 0x1ff),
		.flags = IORESOURCE_MEM,
	},
	{
		.start = INT_USB3,
		.flags = IORESOURCE_IRQ,
	},
};
#endif
/* *INDENT-ON* */

#if defined(CONFIG_USB_GADGET_ARC) || defined(CONFIG_OTG_BTC_ARC)
/*!
 * OTG Gadget device
 */
static void udc_release(struct device *dev)
{
	/* normally not freed */
}

static u64 udc_dmamask = ~(u32) 0;

#if defined(CONFIG_MC13783_MXC)
static struct fsl_usb2_platform_data mxc_serial_peripheral_config = {
	.name = "OTG",
	.platform_init = usbotg_init,
	.platform_uninit = usbotg_uninit,
	.usbmode = (u32) & UOG_USBMODE,
	.does_otg = 1,
	.operating_mode = FSL_USB2_DR_DEVICE,
	.power_budget = 150,	/* 150 mA max power */
	.gpio_usb_active = gpio_usbotg_fs_active,
	.gpio_usb_inactive = gpio_usbotg_fs_inactive,
	.transceiver = "mc13783",
};
#elif defined(CONFIG_ISP1301_MXC)
static struct fsl_usb2_platform_data mxc_serial_peripheral_config = {
	.name = "OTG",
	.platform_init = usbotg_init,
	.platform_uninit = usbotg_uninit,
	.usbmode = (u32) & UOG_USBMODE,
	.does_otg = 0,
	.operating_mode = FSL_USB2_DR_DEVICE,
	.power_budget = 150,	/* 150 mA max power */
	.gpio_usb_active = gpio_usbotg_fs_active,
	.gpio_usb_inactive = gpio_usbotg_fs_inactive,
	.transceiver = "isp1301",
};
#endif

/* *INDENT-OFF* */
static struct platform_device otg_udc_device = {
	.name = "arc_udc",
	.id   = -1,
	.dev  = {
		.release           = udc_release,
		.dma_mask          = &udc_dmamask,
		.coherent_dma_mask = 0xffffffff,
#if defined(CONFIG_MC13783_MXC) || defined(CONFIG_ISP1301_MXC)
		.platform_data     = &mxc_serial_peripheral_config,
#elif defined(CONFIG_ISP1504_MXC)
		.platform_data     = &mxc_isp1504_config,
#endif
		},
	.resource = otg_resources,
	.num_resources = ARRAY_SIZE(otg_resources),
};
/* *INDENT-ON* */
#endif

#if defined(CONFIG_USB_OTG)
static void pindetect_release(struct device *dev)
{
}

/* *INDENT-OFF* */
static struct resource pindetect_resources[] = {
	{
		.start = (u32) (USB_OTGREGS_BASE),
		.end   = (u32) (USB_OTGREGS_BASE + 0x1ff),
		.flags = IORESOURCE_MEM,
	},
	{
		.start = INT_USB3,
		.flags = IORESOURCE_IRQ,
	},
};

static u64 pindetect_dmamask = ~(u32) 0;

static struct fsl_usb2_platform_data fsl_otg_config = {
	.name            = "fsl_arc",
	.platform_init   = usbotg_init,
	.platform_uninit = usbotg_uninit,
	.usbmode         = (u32) &UOG_USBMODE,
	.power_budget    = 500,		/* 500 mA max power */
#if defined(CONFIG_MC13783_MXC)
	.operating_mode    = FSL_USB2_DR_DEVICE,
	.gpio_usb_active   = gpio_usbotg_fs_active,
	.gpio_usb_inactive = gpio_usbotg_fs_inactive,
	.transceiver       = "mc13783",
#elif defined(CONFIG_ISP1504_MXC)
	.gpio_usb_active   = gpio_usbotg_hs_active,
	.gpio_usb_inactive = gpio_usbotg_hs_inactive,
	.transceiver       = "isp1504",
#endif
};

static struct platform_device fsl_device = {
	.name = "fsl_arc",
	.id   = -1,
	.dev  = {
		.release           = pindetect_release,
		.dma_mask          = &pindetect_dmamask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data     = &fsl_otg_config,
		},
	.resource      = pindetect_resources,
	.num_resources = ARRAY_SIZE(pindetect_resources),
};
#endif				/* CONFIG_USB_OTG */

static int __init mx27_usb_init(void)
{
	struct fsl_usb2_platform_data __attribute__((unused)) *pdata;

	pr_debug("%s: \n", __FUNCTION__);

#ifdef CONFIG_USB_EHCI_ARC_H1
	host_pdev_register(usbh1_resources, ARRAY_SIZE(usbh1_resources),
			   &usbh1_config);
#endif

#ifdef CONFIG_USB_EHCI_ARC_H2
	host_pdev_register(usbh2_resources, ARRAY_SIZE(usbh2_resources),
			   &usbh2_config);
#endif

#ifdef CONFIG_USB_EHCI_ARC_OTG
#if defined(CONFIG_MC13783_MXC)	|| defined(CONFIG_ISP1301_MXC)
	host_pdev_register(otg_resources, ARRAY_SIZE(otg_resources),
			   &mxc_serial_host_config);
#elif defined(CONFIG_ISP1504_MXC)
	host_pdev_register(otg_resources, ARRAY_SIZE(otg_resources),
			   &mxc_isp1504_config);
#endif
#endif

#if defined(CONFIG_USB_GADGET_ARC) || defined(CONFIG_OTG_BTC_ARC)
	if (platform_device_register(&otg_udc_device)) {
		printk(KERN_ERR "usb: can't register OTG Gadget\n");
	} else {
		pdata = otg_udc_device.dev.platform_data;
		printk(KERN_INFO "usb: OTG gadget (%s) registered\n",
		       pdata->transceiver);
	}
#endif

#ifdef CONFIG_USB_OTG
	if (platform_device_register(&fsl_device))
		printk(KERN_ERR "usb: can't register otg device\n");
	else
		printk(KERN_INFO "usb: OTG OTG registered\n");
#endif

	return 0;
}

module_init(mx27_usb_init);
