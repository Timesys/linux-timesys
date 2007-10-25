ifeq ($(CONFIG_ARCH_MX27),y)
	obj-$(CONFIG_FB_MXC_TVOUT)              += fs453.o
	obj-$(CONFIG_FB_MXC_SYNC_PANEL)         += mx2fb.o mxcfb_modedb.o
	obj-$(CONFIG_FB_MXC_EPSON_PANEL)        += mx2fb_epson.o
else
	obj-$(CONFIG_FB_MXC_TVOUT)              += fs453.o
	obj-$(CONFIG_FB_MXC_SYNC_PANEL)         += mxcfb.o mxcfb_modedb.o
	obj-$(CONFIG_FB_MXC_EPSON_PANEL)        += mxcfb_epson.o
	obj-$(CONFIG_FB_MXC_EPSON_QVGA_PANEL)   += mxcfb_epson_qvga.o
	obj-$(CONFIG_FB_MXC_TOSHIBA_QVGA_PANEL) += mxcfb_toshiba_qvga.o
	obj-$(CONFIG_FB_MXC_SHARP_128_PANEL)    += mxcfb_sharp_128x128.o
endif
