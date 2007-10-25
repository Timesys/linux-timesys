/*
 * Copyright 2004-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 *	otg_{get,set}_transceiver() are from arm/plat-omap/usb.c.
 *	which is Copyright (C) 2004 Texas Instruments, Inc.
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
 *@defgroup USB ARC OTG USB Driver
 */

/*!
 * @file usb_common.c
 *
 * @brief platform related part of usb driver.
 * @ingroup USB
 */

/*!
 *Include files
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/usb/otg.h>
#include <linux/usb/fsl_xcvr.h>
#include <asm/arch/arc_otg.h>

#define MXC_NUMBER_USB_TRANSCEIVER 6
struct fsl_xcvr_ops *g_xc_ops[MXC_NUMBER_USB_TRANSCEIVER] = { NULL };

static struct clk *usb_clk;
static struct clk *usb_ahb_clk;

/*
 * make sure USB_CLK is running at 60 MHz +/- 1000 Hz
 */
static int fsl_check_usbclk(void)
{
	unsigned long freq;

	usb_ahb_clk = clk_get(NULL, "usb_ahb_clk");
	if (clk_enable(usb_ahb_clk)) {
		printk(KERN_ERR "clk_enable(usb_ahb_clk) failed\n");
		return -EINVAL;
	}
	clk_put(usb_ahb_clk);

	usb_clk = clk_get(NULL, "usb_clk");
	freq = clk_get_rate(usb_clk);
	clk_put(usb_clk);
	if ((freq < 59999000) || (freq > 60001000)) {
		printk(KERN_ERR "USB_CLK=%lu, should be 60MHz\n", freq);
		return -1;
	}

	return 0;
}

void fsl_usb_xcvr_register(struct fsl_xcvr_ops *xcvr_ops)
{
	int i;

	pr_debug("%s\n", __FUNCTION__);
	for (i = 0; i < MXC_NUMBER_USB_TRANSCEIVER; i++) {
		if (g_xc_ops[i] == NULL) {
			g_xc_ops[i] = xcvr_ops;
			return;
		}
	}

	pr_debug("Failed %s\n", __FUNCTION__);
}

EXPORT_SYMBOL(fsl_usb_xcvr_register);

void fsl_usb_xcvr_unregister(struct fsl_xcvr_ops *xcvr_ops)
{
	int i;

	pr_debug("%s\n", __FUNCTION__);
	for (i = 0; i < MXC_NUMBER_USB_TRANSCEIVER; i++) {
		if (g_xc_ops[i] == xcvr_ops) {
			g_xc_ops[i] = NULL;
			return;
		}
	}

	pr_debug("Failed %s\n", __FUNCTION__);
}

EXPORT_SYMBOL(fsl_usb_xcvr_unregister);

static struct fsl_xcvr_ops *fsl_usb_get_xcvr(char *name)
{
	int i;

	pr_debug("%s\n", __FUNCTION__);
	if (name == NULL) {
		printk(KERN_ERR "None tranceiver name be passed\n");
		return NULL;
	}

	for (i = 0; i < MXC_NUMBER_USB_TRANSCEIVER; i++) {
		if (strcmp(g_xc_ops[i]->name, name) == 0) {
			return g_xc_ops[i];
		}
	}
	pr_debug("Failed %s\n", __FUNCTION__);
	return NULL;
}

/* The dmamask must be set for EHCI to work */
static u64 ehci_dmamask = ~(u32) 0;

/*!
 * Register an instance of a USB host platform device.
 *
 * @param	res:	resource pointer
 * @param       n_res:	number of resources
 * @param       config: config pointer
 *
 * @return      newly-registered platform_device
 *
 * The USB controller supports 3 host interfaces, and the
 * kernel can be configured to support some number of them.
 * Each supported host interface is registered as an instance
 * of the "fsl-ehci" device.  Call this function multiple times
 * to register each host interface.
 */
static int instance_id = 0;
struct platform_device *host_pdev_register(struct resource *res, int n_res, struct fsl_usb2_platform_data
					   *config)
{
	struct platform_device *pdev;
	int rc;

	pr_debug("register host res=0x%p, size=%d\n", res, n_res);

	pdev = platform_device_register_simple("fsl-ehci",
					       instance_id, res, n_res);
	if (IS_ERR(pdev)) {
		pr_debug("can't register %s Host, %ld\n",
			 config->name, PTR_ERR(pdev));
		return NULL;
	}

	pdev->dev.coherent_dma_mask = 0xffffffff;
	pdev->dev.dma_mask = &ehci_dmamask;

	/*
	 * platform_device_add_data() makes a copy of
	 * the platform_data passed in.  That makes it
	 * impossible to share the same config struct for
	 * all OTG devices (host,gadget,otg).  So, just
	 * set the platorm_data pointer ourselves.
	 */
	rc = platform_device_add_data(pdev, config,
				      sizeof(struct fsl_usb2_platform_data));
	if (rc) {
		platform_device_unregister(pdev);
		return NULL;
	}

	printk(KERN_INFO "usb: %s host (%s) registered\n", config->name,
	       config->transceiver);
	pr_debug("pdev=0x%p  dev=0x%p  resources=0x%p  pdata=0x%p\n",
		 pdev, &pdev->dev, pdev->resource, pdev->dev.platform_data);

	instance_id++;

	return pdev;
}

static int fsl_usb_mem_init(struct platform_device *pdev)
{
	struct resource *res;
	struct fsl_usb2_platform_data *pdata;

	pdata = (struct fsl_usb2_platform_data *)pdev->dev.platform_data;

	pr_debug("%s: pdev=0x%p  pdata=0x%p\n", __FUNCTION__, pdev, pdata);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no MEM resource.\n");
		return -ENODEV;
	}

	pdata->r_start = res->start;
	pdata->r_len = res->end - res->start + 1;
	pr_debug("%s: MEM resource start=0x%x  len=0x%x\n", pdata->name,
		 res->start, pdata->r_len);

	if (!request_mem_region(pdata->r_start, pdata->r_len, "OTG")) {
		dev_err(&pdev->dev, "request_mem_region failed\n");
		return -EBUSY;
	}
	pdata->regs = ioremap(pdata->r_start, pdata->r_len);
	pr_debug("ioremapped to 0x%p\n", pdata->regs);

	if (pdata->regs == NULL) {
		dev_err(&pdev->dev, "ioremap failed\n");
		release_mem_region(pdata->r_start, pdata->r_len);
		return -EFAULT;
	}

	pr_debug("%s: success\n", __FUNCTION__);
	return 0;
}

static int fsl_usb_mem_map(struct platform_device *pdev)
{
	struct resource *res;
	struct fsl_usb2_platform_data *pdata;

	pdata = (struct fsl_usb2_platform_data *)pdev->dev.platform_data;

	pr_debug("%s: pdev=0x%p  pdata=0x%p\n", __FUNCTION__, pdev, pdata);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no MEM resource.\n");
		return -ENODEV;
	}

	pdata->r_start = res->start;
	pdata->r_len = res->end - res->start + 1;
	pr_debug("%s: MEM resource start=0x%x  len=0x%x\n", pdata->name,
		 res->start, pdata->r_len);

	pdata->regs = ioremap(pdata->r_start, pdata->r_len);
	pr_debug("ioremapped to 0x%p\n", pdata->regs);

	if (pdata->regs == NULL) {
		dev_err(&pdev->dev, "ioremap failed\n");
		return -EFAULT;
	}

	pr_debug("%s: success\n", __FUNCTION__);
	return 0;
}

void fsl_platform_set_vbus_power(struct fsl_usb2_platform_data *pdata, int on)
{
	if (pdata->xcvr_ops && pdata->xcvr_ops->set_vbus_power)
		pdata->xcvr_ops->set_vbus_power((u32 *) pdata->viewport, on);
}

EXPORT_SYMBOL(fsl_platform_set_vbus_power);

void fsl_platform_perform_remote_wakeup(struct fsl_usb2_platform_data *pdata)
{
	if (pdata->xcvr_ops && pdata->xcvr_ops->set_remote_wakeup)
		pdata->xcvr_ops->set_remote_wakeup((u32 *) pdata->viewport);
}

EXPORT_SYMBOL(fsl_platform_perform_remote_wakeup);

#if defined(CONFIG_USB_OTG)
static struct otg_transceiver *xceiv;

/**
 * otg_get_transceiver - find the (single) OTG transceiver driver
 *
 * Returns the transceiver driver, after getting a refcount to it; or
 * null if there is no such transceiver.  The caller is responsible for
 * releasing that count.
 */
struct otg_transceiver *otg_get_transceiver(void)
{
	pr_debug("%s xceiv=0x%p\n", __FUNCTION__, xceiv);
	if (xceiv)
		get_device(xceiv->dev);
	return xceiv;
}

EXPORT_SYMBOL(otg_get_transceiver);

int otg_set_transceiver(struct otg_transceiver *x)
{
	pr_debug("%s xceiv=0x%p  x=0x%p\n", __FUNCTION__, xceiv, x);
	if (xceiv && x)
		return -EBUSY;
	xceiv = x;
	return 0;
}

EXPORT_SYMBOL(otg_set_transceiver);
#endif

static void usbh1_set_serial_xcvr(void)
{
	pr_debug("%s: \n", __FUNCTION__);
	USBCTRL &= ~(UCTRL_H1SIC_MASK | UCTRL_BPE);	/* disable bypass mode */
	USBCTRL |= UCTRL_H1SIC_SU6 |	/* single-ended / unidir. */
	    UCTRL_H1WIE | UCTRL_H1DT |	/* disable H1 TLL */
	    UCTRL_H1PM;		/* power mask */
	UH1_PORTSC1 &= ~PORTSC_PTS_MASK;
	UH1_PORTSC1 |= PORTSC_PTS_SERIAL;
}

static void usbh2_set_ulpi_xcvr(void)
{
	pr_debug("%s\n", __FUNCTION__);
	USBCTRL &= ~(UCTRL_H2SIC_MASK | UCTRL_BPE);	/* disable bypass mode */
	USBCTRL |= UCTRL_H2WIE |	/* wakeup intr enable */
	    UCTRL_H2UIE |	/* ULPI intr enable */
	    UCTRL_H2DT |	/* disable H2 TLL */
	    UCTRL_H2PM;		/* power mask */

	UH2_PORTSC1 &= ~PORTSC_PTS_MASK;	/* set ULPI xcvr */
	UH2_PORTSC1 |= PORTSC_PTS_ULPI;

	/* Turn off the usbpll for ulpi tranceivers */
	clk_disable(usb_clk);
}

int fsl_usb_host_init(struct platform_device *pdev)
{
	struct fsl_usb2_platform_data *pdata;
	struct fsl_xcvr_ops *xops;
	int rc;

	pdata = (struct fsl_usb2_platform_data *)pdev->dev.platform_data;

	pr_debug("%s: pdev=0x%p  pdata=0x%p\n", __FUNCTION__, pdev,
		 pdata->name);

	xops = fsl_usb_get_xcvr(pdata->transceiver);
	if (!xops) {
		printk(KERN_ERR "%s transceiver ops missing\n", pdata->name);
		return -EINVAL;
	}
	pdata->xcvr_ops = xops;
	pdata->xcvr_type = xops->xcvr_type;

	if (fsl_check_usbclk() != 0)
		return -EINVAL;

	/* request_mem_region and ioremap registers */
	if ((rc = fsl_usb_mem_init(pdev))) {
		pdata->gpio_usb_inactive();	/* release our pins */
		return rc;
	}

	pr_debug("%s: grab pins\n", __FUNCTION__);
	if (pdata->gpio_usb_active())
		return -EINVAL;

	if (clk_enable(usb_clk)) {
		printk(KERN_ERR "clk_enable(usb_clk) failed\n");
		return -EINVAL;
	}

	if (xops->init)
		xops->init(xops);

	if (xops->xcvr_type == PORTSC_PTS_SERIAL) {
		usbh1_set_serial_xcvr();
	} else if (xops->xcvr_type == PORTSC_PTS_ULPI) {
		usbh2_set_ulpi_xcvr();
	}

	pr_debug("%s: %s success\n", __FUNCTION__, pdata->name);
	return 0;
}

EXPORT_SYMBOL(fsl_usb_host_init);

void fsl_usb_host_uninit(struct fsl_usb2_platform_data *pdata)
{
	pr_debug("%s\n", __FUNCTION__);

	if (pdata->xcvr_ops && pdata->xcvr_ops->uninit)
		pdata->xcvr_ops->uninit(pdata->xcvr_ops);

	iounmap(pdata->regs);
	release_mem_region(pdata->r_start, pdata->r_len);

	pdata->regs = NULL;
	pdata->r_start = pdata->r_len = 0;

	pdata->gpio_usb_inactive();
	if (pdata->xcvr_type == PORTSC_PTS_SERIAL)
		clk_disable(usb_clk);
}

EXPORT_SYMBOL(fsl_usb_host_uninit);

static void otg_set_serial_xcvr(void)
{
	u32 tmp;

	pr_debug("%s\n", __FUNCTION__);
	tmp = UOG_PORTSC1 & ~PORTSC_PTS_MASK;
	tmp |= PORTSC_PTS_SERIAL;
	UOG_PORTSC1 = tmp;
}

void otg_set_serial_host(void)
{
	pr_debug("%s\n", __FUNCTION__);
	/* set USBCTRL for host operation
	 * disable: bypass mode,
	 * set: single-ended/unidir/6 wire, OTG wakeup intr enable,
	 *      power mask
	 */
	USBCTRL &= ~UCTRL_OSIC_MASK;
#if defined(CONFIG_ARCH_MX27) || defined(CONFIG_ARCH_MX3)
	USBCTRL &= ~UCTRL_BPE;
#endif

#if defined(CONFIG_MXC_USB_SB3)
	USBCTRL |= UCTRL_OSIC_SB3 | UCTRL_OWIE | UCTRL_OPM;
#elif defined(CONFIG_MXC_USB_SU6)
	USBCTRL |= UCTRL_OSIC_SU6 | UCTRL_OWIE | UCTRL_OPM;
#elif defined(CONFIG_MXC_USB_DB4)
	USBCTRL |= UCTRL_OSIC_DB4 | UCTRL_OWIE | UCTRL_OPM;
#else
	USBCTRL |= UCTRL_OSIC_DU6 | UCTRL_OWIE | UCTRL_OPM;
#endif

	USB_OTG_MIRROR = 0xa;
}

EXPORT_SYMBOL(otg_set_serial_host);

void otg_set_serial_peripheral(void)
{
	/* set USBCTRL for device operation
	 * disable: bypass mode
	 * set: differential/unidir/6 wire, OTG wakeup intr enable,
	 *      power mask
	 */
	USBCTRL &= ~UCTRL_OSIC_MASK;
#if defined(CONFIG_ARCH_MX27) || defined(CONFIG_ARCH_MX3)
	USBCTRL &= ~UCTRL_BPE;
#endif

#if defined(CONFIG_MXC_USB_SB3)
	USBCTRL |= UCTRL_OSIC_SB3 | UCTRL_OWIE | UCTRL_OPM;
#elif defined(CONFIG_MXC_USB_SU6)
	USBCTRL |= UCTRL_OSIC_SU6 | UCTRL_OWIE | UCTRL_OPM;
#elif defined(CONFIG_MXC_USB_DB4)
	USBCTRL |= UCTRL_OSIC_DB4 | UCTRL_OWIE | UCTRL_OPM;
#else
	USBCTRL |= UCTRL_OSIC_DU6 | UCTRL_OWIE | UCTRL_OPM;
#endif

	USB_OTG_MIRROR = 0xd;
}

EXPORT_SYMBOL(otg_set_serial_peripheral);

static void otg_set_ulpi_xcvr(void)
{
	u32 tmp;

	pr_debug("%s\n", __FUNCTION__);
	USBCTRL &= ~UCTRL_OSIC_MASK;
#if defined(CONFIG_ARCH_MX27) || defined(CONFIG_ARCH_MX3)
	USBCTRL &= ~UCTRL_BPE;
#endif
	USBCTRL |= UCTRL_OUIE |	/* ULPI intr enable */
	    UCTRL_OWIE |	/* OTG wakeup intr enable */
	    UCTRL_OPM;		/* power mask */

	/* set ULPI xcvr */
	tmp = UOG_PORTSC1 & ~PORTSC_PTS_MASK;
	tmp |= PORTSC_PTS_ULPI;
	UOG_PORTSC1 = tmp;

	/* need to reset the controller here so that the ID pin
	 * is correctly detected.
	 */
	UOG_USBCMD |= UCMD_RESET;

	/* allow controller to reset, and leave time for
	 * the ULPI transceiver to reset too.
	 */
	msleep(100);

	/* Turn off the usbpll for ulpi tranceivers */
	clk_disable(usb_clk);
}

static int otg_used = 0;

int usbotg_init(struct platform_device *pdev)
{
	struct fsl_usb2_platform_data *pdata;
	struct fsl_xcvr_ops *xops;
	int rc;

	pdata = (struct fsl_usb2_platform_data *)pdev->dev.platform_data;

	pr_debug("%s: pdev=0x%p  pdata=0x%p\n", __FUNCTION__, pdev, pdata);

	xops = fsl_usb_get_xcvr(pdata->transceiver);
	if (!xops) {
		printk(KERN_ERR "OTG transceiver ops missing\n");
		return -EINVAL;
	}
	pdata->xcvr_ops = xops;
	pdata->xcvr_type = xops->xcvr_type;

	if (!otg_used) {
		if (fsl_check_usbclk() != 0)
			return -EINVAL;

		pr_debug("%s: grab pins\n", __FUNCTION__);
		if (pdata->gpio_usb_active())
			return -EINVAL;

		if (clk_enable(usb_clk)) {
			printk(KERN_ERR "clk_enable(usb_clk) failed\n");
			return -EINVAL;
		}

		/* request_mem_region and ioremap registers */
		if ((rc = fsl_usb_mem_init(pdev))) {
			return rc;
		}

		if (xops->init)
			xops->init(xops);

		if (xops->xcvr_type == PORTSC_PTS_SERIAL) {
			if (pdata->operating_mode == FSL_USB2_DR_HOST) {
				otg_set_serial_host();
				/* need reset */
				UOG_USBCMD |= UCMD_RESET;
				msleep(100);
			} else if (pdata->operating_mode == FSL_USB2_DR_DEVICE)
				otg_set_serial_peripheral();
			otg_set_serial_xcvr();
		} else if (xops->xcvr_type == PORTSC_PTS_ULPI) {
			otg_set_ulpi_xcvr();
		}
	} else {
		fsl_usb_mem_map(pdev);
	}

	otg_used++;
	pr_debug("%s: success\n", __FUNCTION__);
	return 0;
}

EXPORT_SYMBOL(usbotg_init);

void usbotg_uninit(struct fsl_usb2_platform_data *pdata)
{
	pr_debug("%s\n", __FUNCTION__);

	otg_used--;
	if (!otg_used) {
		if (pdata->xcvr_ops && pdata->xcvr_ops->uninit)
			pdata->xcvr_ops->uninit(pdata->xcvr_ops);

		iounmap(pdata->regs);
		release_mem_region(pdata->r_start, pdata->r_len);

		pdata->regs = NULL;
		pdata->r_start = pdata->r_len = 0;

		pdata->gpio_usb_inactive();
		if (pdata->xcvr_type == PORTSC_PTS_SERIAL)
			clk_disable(usb_clk);
	}
}

EXPORT_SYMBOL(usbotg_uninit);
