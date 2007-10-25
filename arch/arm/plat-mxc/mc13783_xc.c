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

#include <linux/kernel.h>

#include <asm/arch/hardware.h>
#include <asm/arch/pmic_external.h>
#include <asm/arch/pmic_convity.h>
#include <asm/arch/arc_otg.h>
#include <linux/platform_device.h>
#include <linux/usb/fsl_xcvr.h>
#include <linux/delay.h>

#define ID_GND                  (1UL << 0)	/*!< ID is grounded */
#define A_SESS_VLD              (1UL << 1)	/*!< Vbus greater than Session valid threshold */
#define B_SESS_VLD              (1UL << 2)	/*!< Vbus greater than B-Session Valid threshold */
#define VBUS_VLD                (1UL << 3)	/*!< Vbus greater than A-Device Vbus Valid threshold */
#define B_SESS_END              (1UL << 4)	/*!< Vbus less than B-Session end thresshold */

#define TRUE  1
#define FALSE 0

static inline void mc13783_set_host(void);
static inline void mc13783_set_peripheral(void);

PMIC_CONVITY_HANDLE pmic_handle = (PMIC_CONVITY_HANDLE) NULL;

extern void otg_set_serial_peripheral(void);
extern void otg_set_serial_host(void);

static void pmic_event_handler(const PMIC_CONVITY_EVENTS event)
{
	if (event == USB_DETECT_MINI_B) {
		otg_set_serial_peripheral();
	}
	if (event == USB_DETECT_MINI_A) {
		otg_set_serial_host();
	}
}

static int usb_pmic_mod_init(void)
{
	PMIC_STATUS rs = PMIC_ERROR;

	rs = pmic_convity_open(&pmic_handle, USB);
	if (rs != PMIC_SUCCESS) {
		printk(KERN_ERR "pmic_convity_open returned error %d\n", rs);
		return rs;
	}

	rs = pmic_convity_set_callback(pmic_handle, pmic_event_handler,
				       USB_DETECT_MINI_A | USB_DETECT_MINI_B);

	if (rs != PMIC_SUCCESS) {
		printk(KERN_ERR
		       "pmic_convity_set_callback returned error %d\n", rs);
		return rs;
	}

	return rs;
}

static void usb_pmic_mod_exit(void)
{
	PMIC_STATUS rs;

	pmic_convity_set_mode(pmic_handle, RS232_1);
	pmic_convity_clear_callback(pmic_handle);

	if (pmic_handle != (PMIC_CONVITY_HANDLE) NULL) {
		rs = pmic_convity_close(pmic_handle);
		if (rs != PMIC_SUCCESS) {
			printk(KERN_ERR
			       "pmic_convity_close() returned error %d", rs);
		} else {
			pmic_handle = (PMIC_CONVITY_HANDLE) NULL;
		}
	}
}

static inline void mc13783_set_host(void)
{
	PMIC_STATUS rs = PMIC_ERROR;

	rs = pmic_convity_usb_otg_set_config(pmic_handle, USB_UDM_PD);
	rs |= pmic_convity_usb_otg_set_config(pmic_handle, USB_UDP_PD);
	rs |= pmic_convity_set_output(pmic_handle, TRUE, TRUE);
	rs |=
	    pmic_convity_usb_otg_set_config(pmic_handle,
					    USB_VBUS_CURRENT_LIMIT_LOW_30MS);
#ifndef CONFIG_USB_OTG
	rs |= pmic_convity_usb_otg_clear_config(pmic_handle, USB_OTG_SE0CONN);
	rs |= pmic_convity_usb_otg_clear_config(pmic_handle, USB_PU);
#endif

	if (rs != PMIC_SUCCESS) {
		printk(KERN_ERR "mc13783_set_host failed\n");
	}
}

static inline void mc13783_set_peripheral(void)
{
	PMIC_STATUS rs = PMIC_ERROR;

	rs = pmic_convity_usb_set_speed(pmic_handle, USB_FULL_SPEED);
#ifndef CONFIG_USB_OTG
	rs |= pmic_convity_usb_otg_set_config(pmic_handle, USB_OTG_SE0CONN);
	rs |= pmic_convity_usb_otg_set_config(pmic_handle, USB_PU);
#endif

	if (rs != PMIC_SUCCESS) {
		printk(KERN_ERR "mc13783_set_peripheral failed\n");
	}
}

static struct fsl_xcvr_ops mc13783_ops_otg = {
	.name = "mc13783",
	.xcvr_type = PORTSC_PTS_SERIAL,
	.set_host = mc13783_set_host,
	.set_device = mc13783_set_peripheral,
};

extern void fsl_usb_xcvr_register(struct fsl_xcvr_ops *xcvr_ops);

int mc13783xc_init(void)
{
	PMIC_STATUS rs = PMIC_ERROR;

#if defined(CONFIG_MXC_USB_SB3)
	int xc_mode = USB_SINGLE_ENDED_BIDIR;
#elif defined(CONFIG_MXC_USB_SU6)
	int xc_mode = USB_SINGLE_ENDED_UNIDIR;
#elif defined(CONFIG_MXC_USB_DB4)
	int xc_mode = USB_DIFFERENTIAL_BIDIR;
#else
	int xc_mode = USB_DIFFERENTIAL_UNIDIR;
#endif

	rs = usb_pmic_mod_init();
	if (rs != PMIC_SUCCESS) {
		usb_pmic_mod_exit();
		printk(KERN_ERR "usb_pmic_mod_init failed\n");
		return rs;
	}

	rs = pmic_convity_usb_otg_set_config(pmic_handle, USB_OTG_SE0CONN);
	rs |= pmic_convity_usb_set_xcvr(pmic_handle, xc_mode);
	rs |= pmic_convity_usb_otg_set_config(pmic_handle, USB_PULL_OVERRIDE);
	rs |= pmic_convity_usb_otg_clear_config(pmic_handle, USB_USBCNTRL);
	rs |= pmic_convity_usb_otg_set_config(pmic_handle, USBXCVREN);
	rs |= pmic_convity_usb_otg_clear_config(pmic_handle, USB_DP150K_PU);
	rs |= pmic_convity_set_output(pmic_handle, FALSE, TRUE);

	if (rs != PMIC_SUCCESS) {
		printk(KERN_ERR "pmic configuration failed\n");
	}

	fsl_usb_xcvr_register(&mc13783_ops_otg);

#ifdef CONFIG_USB_OTG
	mc13783_set_peripheral();
	mc13783_set_host();
#endif

	return rs;
}

extern void fsl_usb_xcvr_unregister(struct fsl_xcvr_ops *xcvr_ops);

void mc13783xc_uninit(void)
{
	pmic_convity_usb_otg_set_config(pmic_handle, USB_PULL_OVERRIDE);
	pmic_convity_usb_otg_clear_config(pmic_handle, USB_OTG_SE0CONN);
	pmic_convity_usb_otg_set_config(pmic_handle, USB_USBCNTRL);

	pmic_convity_usb_otg_clear_config(pmic_handle, USB_UDP_PD);
	pmic_convity_usb_otg_clear_config(pmic_handle, USB_UDM_PD);

	usb_pmic_mod_exit();

	fsl_usb_xcvr_unregister(&mc13783_ops_otg);
}

module_init(mc13783xc_init);
module_exit(mc13783xc_uninit);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("mc13783xc");
MODULE_LICENSE("GPL");
