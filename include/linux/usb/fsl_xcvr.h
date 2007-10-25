#ifndef __LINUX_USB_FSL_XCVR_H
#define __LINUX_USB_FSL_XCVR_H
/**
 * @name: transceiver name
 * @xcvr_type: one of PORTSC_PTS_{UTMI,SERIAL,ULPI} 
 * @init: transceiver- and board-specific initialization function
 * @uninit: transceiver- and board-specific uninitialization function
 * @set_host:
 * @set_device:
 *
 */
struct fsl_xcvr_ops {
	char *name;
	u32 xcvr_type;

	void (*init)(struct fsl_xcvr_ops *ops);
	void (*uninit)(struct fsl_xcvr_ops *ops);
	void (*set_host)(void);
	void (*set_device)(void);
	void (*set_vbus_power) (u32 *view, int on);
	void (*set_remote_wakeup)(u32 *view);
};

#endif
