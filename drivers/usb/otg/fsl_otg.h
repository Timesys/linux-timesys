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

#include "otg_fsm.h"
#include <linux/usb/otg.h>

 /* USB Command  Register Bit Masks */
#define USB_CMD_RUN_STOP		(0x1<<0  )
#define USB_CMD_CTRL_RESET		(0x1<<1  )
#define USB_CMD_PERIODIC_SCHEDULE_EN	(0x1<<4  )
#define USB_CMD_ASYNC_SCHEDULE_EN	(0x1<<5  )
#define USB_CMD_INT_AA_DOORBELL		(0x1<<6  )
#define USB_CMD_ASP			(0x3<<8  )
#define USB_CMD_ASYNC_SCH_PARK_EN	(0x1<<11 )
#define USB_CMD_SUTW			(0x1<<13 )
#define USB_CMD_ATDTW			(0x1<<14 )
#define USB_CMD_ITC			(0xFF<<16)

/* bit 15,3,2 are frame list size */
#define USB_CMD_FRAME_SIZE_1024		(0x0<<15 | 0x0<<2)
#define USB_CMD_FRAME_SIZE_512		(0x0<<15 | 0x1<<2)
#define USB_CMD_FRAME_SIZE_256		(0x0<<15 | 0x2<<2)
#define USB_CMD_FRAME_SIZE_128		(0x0<<15 | 0x3<<2)
#define USB_CMD_FRAME_SIZE_64		(0x1<<15 | 0x0<<2)
#define USB_CMD_FRAME_SIZE_32		(0x1<<15 | 0x1<<2)
#define USB_CMD_FRAME_SIZE_16		(0x1<<15 | 0x2<<2)
#define USB_CMD_FRAME_SIZE_8		(0x1<<15 | 0x3<<2)

/* bit 9-8 are async schedule park mode count */
#define USB_CMD_ASP_00			(0x0<<8)
#define USB_CMD_ASP_01			(0x1<<8)
#define USB_CMD_ASP_10			(0x2<<8)
#define USB_CMD_ASP_11			(0x3<<8)
#define USB_CMD_ASP_BIT_POS		(8)

/* bit 23-16 are interrupt threshold control */
#define USB_CMD_ITC_NO_THRESHOLD	(0x00<<16)
#define USB_CMD_ITC_1_MICRO_FRM		(0x01<<16)
#define USB_CMD_ITC_2_MICRO_FRM		(0x02<<16)
#define USB_CMD_ITC_4_MICRO_FRM		(0x04<<16)
#define USB_CMD_ITC_8_MICRO_FRM		(0x08<<16)
#define USB_CMD_ITC_16_MICRO_FRM	(0x10<<16)
#define USB_CMD_ITC_32_MICRO_FRM	(0x20<<16)
#define USB_CMD_ITC_64_MICRO_FRM	(0x40<<16)
#define USB_CMD_ITC_BIT_POS		(16)

/* USB Status Register Bit Masks */
#define USB_STS_INT			(0x1<<0 )
#define USB_STS_ERR			(0x1<<1 )
#define USB_STS_PORT_CHANGE		(0x1<<2 )
#define USB_STS_FRM_LST_ROLL		(0x1<<3 )
#define USB_STS_SYS_ERR			(0x1<<4 )
#define USB_STS_IAA			(0x1<<5 )
#define USB_STS_RESET_RECEIVED		(0x1<<6 )
#define USB_STS_SOF			(0x1<<7 )
#define USB_STS_DCSUSPEND		(0x1<<8 )
#define USB_STS_HC_HALTED		(0x1<<12)
#define USB_STS_RCL			(0x1<<13)
#define USB_STS_PERIODIC_SCHEDULE	(0x1<<14)
#define USB_STS_ASYNC_SCHEDULE		(0x1<<15)

/* USB Interrupt Enable Register Bit Masks */
#define USB_INTR_INT_EN			(0x1<<0 )
#define USB_INTR_ERR_INT_EN		(0x1<<1 )
#define USB_INTR_PC_DETECT_EN		(0x1<<2 )
#define USB_INTR_FRM_LST_ROLL_EN	(0x1<<3 )
#define USB_INTR_SYS_ERR_EN		(0x1<<4 )
#define USB_INTR_ASYN_ADV_EN		(0x1<<5 )
#define USB_INTR_RESET_EN		(0x1<<6 )
#define USB_INTR_SOF_EN			(0x1<<7 )
#define USB_INTR_DEVICE_SUSPEND		(0x1<<8 )

/* Device Address bit masks */
#define USB_DEVICE_ADDRESS_MASK		(0x7F<<25)
#define USB_DEVICE_ADDRESS_BIT_POS	(25)

/* USB MODE Register Bit Masks */
#define  USB_MODE_CTRL_MODE_IDLE	(0x0<<0)
#define  USB_MODE_CTRL_MODE_DEVICE	(0x2<<0)
#define  USB_MODE_CTRL_MODE_HOST	(0x3<<0)
#define  USB_MODE_CTRL_MODE_RSV		(0x1<<0)
#define  USB_MODE_SETUP_LOCK_OFF	(0x1<<3)
#define  USB_MODE_STREAM_DISABLE	(0x1<<4)

/*
 *  A-DEVICE timing  constants
 */

/* Wait for VBUS Rise  */
#define TA_WAIT_VRISE	(100)	/* a_wait_vrise 100 ms, section: 6.6.5.1 */

/* Wait for B-Connect */
#define TA_WAIT_BCON	(10000)	/* a_wait_bcon > 1 sec, section: 6.6.5.2 
				 * This is only used to get out of 
				 * OTG_STATE_A_WAIT_BCON state if there was 
				 * no connection for these many milliseconds 
				 */

/* A-Idle to B-Disconnect */
/* It is necessary for this timer to be more than 750 ms because of a bug in OPT
 * test 5.4 in which B OPT disconnects after 750 ms instead of 75ms as stated 
 * in the test description 
 */
#define TA_AIDL_BDIS	(5000)	/* a_suspend minimum 200 ms, section: 6.6.5.3 */

/* B-Idle to A-Disconnect */
#define TA_BIDL_ADIS	(12)	/* 3 to 200 ms */

/* B-device timing constants */

/* Data-Line Pulse Time*/
#define TB_DATA_PLS	(10)	/* b_srp_init,continue 5~10ms, section:5.3.3 */
#define TB_DATA_PLS_MIN	(5)	/* minimum 5 ms */
#define TB_DATA_PLS_MAX	(10)	/* maximum 10 ms */

/* SRP Initiate Time  */
#define TB_SRP_INIT	(100)	/* b_srp_init,maximum 100 ms, section:5.3.8 */

/* SRP Fail Time  */
#define TB_SRP_FAIL	(7000)	/* b_srp_init,Fail time 5~30s, section:6.8.2.2 */

/* SRP result wait time */
#define TB_SRP_WAIT	(60)

/* VBus time */
#define TB_VBUS_PLS	(30)	/* time to keep vbus pulsing asserted */

/* Discharge time */
/* This time should be less than 10ms. It varies from system to system. */
#define TB_VBUS_DSCHRG	(8)

/* A-SE0 to B-Reset  */
#define TB_ASE0_BRST	(20)	/* b_wait_acon, mini 3.125 ms,section:6.8.2.4 */

/* A bus suspend timer before we can switch to b_wait_aconn */
#define TB_A_SUSPEND	(7)
#define TB_BUS_RESUME	(12)

/* SE0 Time Before SRP */
#define TB_SE0_SRP	(2)	/* b_idle,minimum 2 ms, section:5.3.2 */

#define SET_OTG_STATE(otg_ptr, newstate)	((otg_ptr)->state=newstate)

struct usb_dr_mmap {
	/* Capability register */
	u8 res1[256];
	u16 caplength;		/* Capability Register Length */
	u16 hciversion;		/* Host Controller Interface Version */
	u32 hcsparams;		/* Host Controller Structual Parameters */
	u32 hccparams;		/* Host Controller Capability Parameters */
	u8 res2[20];
	u32 dciversion;		/* Device Controller Interface Version */
	u32 dccparams;		/* Device Controller Capability Parameters */
	u8 res3[24];
	/* Operation register */
	u32 usbcmd;		/* USB Command Register */
	u32 usbsts;		/* USB Status Register */
	u32 usbintr;		/* USB Interrupt Enable Register */
	u32 frindex;		/* Frame Index Register */
	u8 res4[4];
	u32 deviceaddr;		/* Device Address */
	u32 endpointlistaddr;	/* Endpoint List Address Register */
	u8 res5[4];
	u32 burstsize;		/* Master Interface Data Burst Size Register */
	u32 txttfilltuning;	/* Transmit FIFO Tuning Controls Register */
	u8 res6[8];
	u32 ulpiview;		/* ULPI register access */
	u8 res7[12];
	u32 configflag;		/* Configure Flag Register */
	u32 portsc;		/* Port 1 Status and Control Register */
	u8 res8[28];
	u32 otgsc;		/* On-The-Go Status and Control */
	u32 usbmode;		/* USB Mode Register */
	u32 endptsetupstat;	/* Endpoint Setup Status Register */
	u32 endpointprime;	/* Endpoint Initialization Register */
	u32 endptflush;		/* Endpoint Flush Register */
	u32 endptstatus;	/* Endpoint Status Register */
	u32 endptcomplete;	/* Endpoint Complete Register */
	u32 endptctrl[6];	/* Endpoint Control Registers */
	u8 res9[552];
	u32 snoop1;
	u32 snoop2;
	u32 age_cnt_thresh;	/* Age Count Threshold Register */
	u32 pri_ctrl;		/* Priority Control Register */
	u32 si_ctrl;		/* System Interface Control Register */
	u8 res10[236];
	u32 control;		/* General Purpose Control Register */
};

struct fsl_otg_timer {
	unsigned long expires;	/* Number of count increase to timeout */
	unsigned long count;	/* Tick counter */
	void (*function) (unsigned long);	/* Timeout function */
	unsigned long data;	/* Data passed to function */
	struct list_head list;
};

struct fsl_otg_timer inline *otg_timer_initializer
    (void (*function) (unsigned long), unsigned long expires,
     unsigned long data) {
	struct fsl_otg_timer *timer;
	timer = kmalloc(sizeof(struct fsl_otg_timer), GFP_KERNEL);
	if (timer == NULL)
		return NULL;
	timer->function = function;
	timer->expires = expires;
	timer->data = data;
	return timer;
}

struct fsl_otg {
	struct otg_transceiver otg;
	struct otg_fsm fsm;
	struct usb_dr_mmap *dr_mem_map;
	struct work_struct otg_event;

	/*used for usb host */
	u8 host_working;
	u8 on_off;

	int irq;
};

struct fsl_otg_config {
	u8 otg_port;
};

extern const char *state_string(enum usb_otg_state state);
extern int otg_set_resources(struct resource *resources, int num);
