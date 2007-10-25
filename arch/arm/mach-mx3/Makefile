#
# Makefile for the linux kernel.
#

# Object file lists.

obj-y			:= system.o iomux.o cpu.o mm.o clock.o usb.o dptc.o devices.o serial.o dma.o mxc_pm.o dvfs_v2.o
obj-$(CONFIG_MACH_MX31ADS)	+= mx31ads.o mx31ads_gpio.o

# power management
obj-$(CONFIG_MXC_DVFS)  += dvfs.o
obj-$(CONFIG_PM) 		+= pm.o
obj-$(CONFIG_DPM)		+= dpm.o
