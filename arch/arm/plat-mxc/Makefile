#
# Makefile for the linux kernel.
#

# Common support
obj-y := gpio.o clock.o wdog.o snoop.o irq.o io.o

ifneq ($(CONFIG_ARCH_MX27),y)
obj-y += time.o cpu_common.o  spba.o sdma/
endif

obj-$(CONFIG_ARCH_MX27) += cpu_common.o dma_mx2.o usb_common.o
obj-$(CONFIG_ARCH_MX3) += dptc.o usb_common.o

obj-m :=
obj-n :=
obj-  :=

# LEDs support
obj-$(CONFIG_LEDS) += leds.o

# USB support
obj-$(CONFIG_ISP1504_MXC) += isp1504xc.o

obj-$(CONFIG_ISP1301_MXC) += isp1301xc.o

obj-$(CONFIG_MC13783_MXC) += mc13783_xc.o

ifneq ($(CONFIG_USB_EHCI_ARC_H1),)
obj-y += serialxc.o
endif
