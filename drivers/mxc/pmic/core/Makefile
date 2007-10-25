#
# Makefile for the PMIC core drivers.
#
obj-$(CONFIG_MXC_SPI_PMIC_CORE) += pmic_core_spi_mod.o
obj-$(CONFIG_MXC_PMIC_CHARDEV)	+= pmic-dev.o

pmic_core_spi_mod-objs := pmic_external.o pmic_event.o pmic_core_spi.o 

ifeq ($(CONFIG_MXC_PMIC_MC13783),y)
pmic_core_spi_mod-objs += mc13783.o
endif
