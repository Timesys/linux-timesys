#
# Makefile for the linux kernel.
#

# Object file lists.
obj-y   := cpu.o mm.o system.o devices.o dummy_gpio.o irq.o bus_freq.o mvf_fec.o usb_dr.o usb_dr2.o pm.o mvf_sema4.o
obj-y += l2switch.o

obj-$(CONFIG_ARCH_MVFA5) += clock.o mvf_suspend.o
obj-$(CONFIG_MACH_MVFA5_TWR_VF700) += board-twr-vf700.o
obj-$(CONFIG_MACH_PCM052) += board-pcm052.o
obj-$(CONFIG_MACH_PCL052) += board-pcl052.o
