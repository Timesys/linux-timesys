/*
 * MXC GPIO support. (c) 2008 Daniel Mack <daniel@caiaq.de>
 * Copyright 2008 Juergen Beisert, kernel@pengutronix.de
 *
 * Based on code from Freescale,
 * Copyright (C) 2004-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/basic_mmio_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/device.h>
#include <linux/module.h>
#include <asm-generic/bug.h>
#include <linux/gpio/driver.h>
#include <linux/of_gpio.h>

enum vf6xx_gpio_hwtype {
	VF610_GPIO,		/* VF610 */
};

/*    Interrupt Assignments for the GPIO
Vector  Cortex NVIC      Cortex  GIC      GPIO
offset   M4    Interrupt   A5   Interrupt Port/Bank
address Vector ID        Vector  ID       number
------------------------------------------------
000001ec  123   107      155    139       GPIO0
000001ec  124   108      156    140       GPIO1
000001ec  125   109      157    141       GPIO2
000001ec  126   110      158    142       GPIO3
000001ec  127   111      159    143       GPIO4
------------------------------------------------
*/
/* device type dependent stuff */
struct vf6xx_gpio_hwdata {

	unsigned pdor_reg;
	unsigned psor_reg;
	unsigned pcor_reg;
	unsigned ptor_reg;
	unsigned pdir_reg;
	unsigned dfwr_reg;
	unsigned dfcr_reg;
	unsigned dfer_reg;
	unsigned isr_reg;
	unsigned low_level;
	unsigned high_level;
	unsigned rise_edge;
	unsigned fall_edge;
	unsigned either_edge;
};
/**
 * @base: the offset to the controller in virtual memory.
 * @bgc: descriptor interface to a gpio chip.
 */
struct vf6xx_gpio_port {
	struct list_head node;
	void __iomem *base;
	void __iomem *base2;
	int id;
	int irq;
	int irq_high;
	struct irq_domain *domain;
	struct bgpio_chip bgc;
	enum vf6xx_gpio_hwtype devid;
};

static struct vf6xx_gpio_hwdata vf610_gpio_hwdata = {
	.pdor_reg = 0x00,
	.psor_reg = 0x04,
	.pcor_reg = 0x08,
	.ptor_reg = 0x0C,
	.pdir_reg = 0x10,
	.dfer_reg = 0xC0,
	.dfcr_reg = 0xC4,
	.dfwr_reg = 0xC8,
	.isr_reg = 0xA0,	/* Interrupt status register */
	.low_level = 0x08,	//Note this needs to be shifted; by 16
	.high_level = 0x0C,
	.rise_edge = 0x09,
	.fall_edge = 0x0A,
	.either_edge = 0x0B,
};

static enum vf6xx_gpio_hwtype vf6xx_gpio_hwtype;
static struct vf6xx_gpio_hwdata *vf6xx_gpio_hwdata;

#define GPIO_PDOR	(vf6xx_gpio_hwdata->pdor_reg)
#define GPIO_PSOR	(vf6xx_gpio_hwdata->psor_reg)
#define GPIO_PCOR	(vf6xx_gpio_hwdata->pcor_reg)
#define GPIO_PTOR	(vf6xx_gpio_hwdata->ptor_reg)
#define GPIO_PDIR	(vf6xx_gpio_hwdata->pdir_reg)
#define GPIO_DFER	(vf6xx_gpio_hwdata->dfer_reg)
#define GPIO_DFCR	(vf6xx_gpio_hwdata->dfcr_reg)
#define GPIO_DFWR	(vf6xx_gpio_hwdata->dfwr_reg)
#define GPIO_ISR 	(vf6xx_gpio_hwdata->isr_reg)

#define GPIO_DMAREQ_RISE_EDGE	0x01
#define GPIO_DMAREQ_FALL_EDGE	0x02
#define GPIO_DMAREQ_EITHER_EDGE	0x03

#define GPIO_INT_LOW_LEV	(vf6xx_gpio_hwdata->low_level)
#define GPIO_INT_HIGH_LEV	(vf6xx_gpio_hwdata->high_level)
#define GPIO_INT_RISE_EDGE	(vf6xx_gpio_hwdata->rise_edge)
#define GPIO_INT_FALL_EDGE	(vf6xx_gpio_hwdata->fall_edge)
#define GPIO_INT_EITHER_EDGE (vf6xx_gpio_hwdata->either_edge)
#define GPIO_INT_NONE		 (vf6xx_gpio_hwdata->int_none)

static struct platform_device_id vf6xx_gpio_ids[] = {
	{
		.name = "fsl,vf610-gpio",
		.driver_data = VF610_GPIO,
	},
	{
	/* sentinel */
	}
};

MODULE_DEVICE_TABLE(platform, vf6xx_gpio_ids);

static const struct of_device_id vf6xx_gpio_dt_ids[] = {
	{.compatible = "fsl,vf610-gpio",.data = &vf6xx_gpio_ids[VF610_GPIO],},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, vf6xx_gpio_dt_ids);

/*
 * The list is used
 * to save the references to all ports,
 */
static LIST_HEAD(vf6xx_gpio_ports);

/*
 * Note: This driver assumes 32 GPIOs are handled in one register
*/

/*
 * For MVF, the status can also be clear in the pin configuration register.
 */
static void _clear_gpio_irqstatus(struct vf6xx_gpio_port *port, u32 index)
{
	__raw_writel(1 << index, port->base + GPIO_ISR);
}

static void gpio_ack_irq(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct vf6xx_gpio_port *port = gc->private;
	u32 gpio_idx = d->hwirq;
	_clear_gpio_irqstatus(port, gpio_idx);
}

static void gpio_mask_irq(struct irq_data *d)
{
}

static void gpio_unmask_irq(struct irq_data *d)
{
}

static int gpio_set_irq_type(struct irq_data *d, u32 type)
{

	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct vf6xx_gpio_port *port = gc->private;
	u32 gpio_idx = d->hwirq;
	u32 gpio = port->bgc.gc.base + gpio_idx;
	int edge = 0;
	void __iomem *reg = port->base;

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		edge = GPIO_INT_RISE_EDGE;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		edge = GPIO_INT_FALL_EDGE;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		edge = GPIO_INT_EITHER_EDGE;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		edge = GPIO_INT_LOW_LEV;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		edge = GPIO_INT_HIGH_LEV;
		break;
	default:
		return -EINVAL;
	}
	//printk("%s %d gpio = %d gpio_idx = %d edge = %d\n", __FUNCTION__, __LINE__, gpio, gpio_idx, edge);
	writel((edge << 16), port->base + gpio_idx * 4);
	_clear_gpio_irqstatus(port, gpio_idx);

	return 0;
}

/* handle 32 interrupts in one status register */
static void vf6xx_gpio_irq_handler(struct vf6xx_gpio_port *port, u32 irq_stat)
{
	//printk("%s %d irq_stat = %d \n", __FUNCTION__, __LINE__, irq_stat);
	while (irq_stat != 0) {
		int irqoffset = fls(irq_stat) - 1;


		generic_handle_irq(irq_find_mapping(port->domain, irqoffset));

		irq_stat &= ~(1 << irqoffset);
	}
}

/* Vybrid and MX3 has one interrupt *per* gpio port */
static void mx3_gpio_irq_handler(u32 irq, struct irq_desc *desc)
{
	u32 irq_stat;
	struct vf6xx_gpio_port *port = irq_get_handler_data(irq);
	struct irq_chip *chip = irq_get_chip(irq);

	chained_irq_enter(chip, desc);

	irq_stat = readl(port->base + GPIO_ISR);

	vf6xx_gpio_irq_handler(port, irq_stat);

	chained_irq_exit(chip, desc);
}

/*
 * Set interrupt number "irq" in the GPIO as a wake-up source.
 * While system is running, all registered GPIO interrupts need to have
 * wake-up enabled. When system is suspended, only selected GPIO interrupts
 * need to have wake-up enabled.
 * @param  irq          interrupt source number
 * @param  enable       enable as wake-up if equal to non-zero
 * @return       This function returns 0 on success.
 */
static int gpio_set_wake_irq(struct irq_data *d, u32 enable)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct vf6xx_gpio_port *port = gc->private;

	if (enable) {
			enable_irq_wake(port->irq);
	} else {
			disable_irq_wake(port->irq);
	}

	return 0;
}

static void __init vf6xx_gpio_init_gc(struct vf6xx_gpio_port *port,
				      int irq_base)
{
	struct irq_chip_generic *gc;
	struct irq_chip_type *ct;

	//printk("%s %d irq_base = %d\n", __FUNCTION__, __LINE__, irq_base);
	gc = irq_alloc_generic_chip("gpio-vf610", 1, irq_base,
				    port->base, handle_level_irq);
	gc->private = port;

	ct = gc->chip_types;

	ct->chip.irq_ack = gpio_ack_irq;
	ct->chip.irq_mask = gpio_mask_irq;
	ct->chip.irq_unmask = gpio_unmask_irq;
	ct->chip.irq_set_type = gpio_set_irq_type;
	ct->chip.irq_set_wake = gpio_set_wake_irq;
	//ct->regs.ack = GPIO_ISR;

	irq_setup_generic_chip(gc, IRQ_MSK(32), IRQ_GC_INIT_NESTED_LOCK,
			       IRQ_NOREQUEST, 0);

}

static void vf6xx_gpio_get_hw(struct platform_device *pdev)
{
	const struct of_device_id *of_id =
	    of_match_device(vf6xx_gpio_dt_ids, &pdev->dev);
	enum vf6xx_gpio_hwtype hwtype;

	if (of_id)
		pdev->id_entry = of_id->data;
	hwtype = pdev->id_entry->driver_data;

	if (vf6xx_gpio_hwtype) {
		/*
		 * The driver works with a reasonable presupposition,
		 * that is all gpio ports must be the same type when
		 * running on one soc.
		 */
		BUG_ON(vf6xx_gpio_hwtype != hwtype);
		return;
	}

	if (hwtype == VF610_GPIO)
		vf6xx_gpio_hwdata = &vf610_gpio_hwdata;

	vf6xx_gpio_hwtype = hwtype;
}

static int vf6xx_gpio_to_irq(struct gpio_chip *gc, unsigned offset)
{
	struct bgpio_chip *bgc = to_bgpio_chip(gc);
	struct vf6xx_gpio_port *port =
	    container_of(bgc, struct vf6xx_gpio_port, bgc);

	int irqmapping = irq_find_mapping(port->domain, offset);

	//printk("%s %d irqmapping = %d offset = %d\n", __FUNCTION__, __LINE__, irqmapping, offset);
	return irq_find_mapping(port->domain, offset);
}

static int vf6xx_gpio_get_direction(struct gpio_chip *chip, unsigned offset)
{
	/* Set the IOMUX to the proper mode for GPIO
	   Then read the GPIO reg
	   eg for gpio chip 0, 400F_F000 Port Data Output Register (GPIO0_PDOR)
	   return 0=out 1=in
	   test with echo 0 > /sys/class/gpio/export
	 */
	struct bgpio_chip *bgc = to_bgpio_chip(chip);
	volatile u32 val;
	/* PDOR */
	val = readl((void *)(bgc->reg_set + GPIO_PDOR));
	val = (val >> offset) & 1;
	writel(val, (void *)(bgc->reg_set + GPIO_PCOR));

	if (val & 1)
		return 0;
	if (val & 0)
		return 1;
	else
		return -EINVAL;
}

static int vf6xx_direction_input_set(struct gpio_chip *chip, unsigned offset)
{
	/* The input/output direction is controlled by the pinctrl interface.  */
	pinctrl_gpio_direction_input(offset);

	return 0;
}

static int vf6xx_direction_output_set(struct gpio_chip *chip,
				      unsigned offset, int value)
{
	struct bgpio_chip *bgc = to_bgpio_chip(chip);
	volatile u32 val = 1 << offset;

	/* set GPIOx_PCOR bit to one since register is write only no need
	   to read-modify-write.
	 */
	writel(val, (void *)(bgc->reg_set + GPIO_PCOR));

	/* PDOR is read/write, set bit to one for output mode */
	val = readl((void *)(bgc->reg_set + GPIO_PDOR));
	val = val | (1 << offset);
	writel(val, (void *)(bgc->reg_set + GPIO_PDOR));

	return 0;
}

/*
	For intput mode get the value
*/
static int vf6xx_gpio_val_get(struct gpio_chip *chip, unsigned offset)
{
	struct bgpio_chip *bgc = to_bgpio_chip(chip);
	volatile u32 val;

	/* If pin is input read PDIR */
	val = readl((void *)(bgc->reg_set + GPIO_PDIR));
	return ((val >> offset) & 1);
}

/*
	Only works if direction is set to output.
*/
static void vf6xx_gpio_val_set(struct gpio_chip *chip, unsigned offset,
			       int value)
{
	struct bgpio_chip *bgc = to_bgpio_chip(chip);
	volatile u32 val;

	/* If val == zero then set PCOR to clear else set PSOR to set */
	if (value == 0) {
		writel((1 << offset),
		       (void *)(bgc->reg_set + GPIO_PCOR));
	} else {
		writel((1 << offset | val),
		       (void *)(bgc->reg_set + GPIO_PSOR));
	}
	return;
}

static int vf6xx_of_xlate(struct gpio_chip *gc,
			  const struct of_phandle_args *gpiospec, u32 * flags)
{
	//printk("%s %d\n", __FUNCTION__, __LINE__);
	if (WARN_ON(gc->of_gpio_n_cells < 1))
		return -EINVAL;
	if (WARN_ON(gpiospec->args_count < gc->of_gpio_n_cells))
		return -EINVAL;
	if (gpiospec->args[0] > gc->ngpio)
		return -EINVAL;
	//printk("%s arg0=%d arg1=%d arg2=%d \n", __FUNCTION__, gpiospec->args[0], gpiospec->args[1], gpiospec->args[2]);
	return gpiospec->args[0];
}

static int vf6xx_gpio_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id =
	    of_match_device(vf6xx_gpio_dt_ids, &pdev->dev);
	struct device_node *np = pdev->dev.of_node;
	struct vf6xx_gpio_port *port;
	struct resource *iores;
	int irq_base;
	int err;

	//printk("%s %d\n", __FUNCTION__, __LINE__);
	vf6xx_gpio_get_hw(pdev);
	/* Create holders for this driver */
	port = (struct vf6xx_gpio_port *)devm_kzalloc(&pdev->dev, sizeof(*port),
						   GFP_KERNEL);
	if (!port)
		return -ENOMEM;

	/* The word gpio must be the preamble in file *.dts or *.dtsi
	   and of_alias_get_id will find gpio0, gpio1... */
	port->id = of_alias_get_id(np, "gpio");
	if (port->id < 0)
		return port->id;
	port->devid = (enum vf6xx_gpio_hwtype)of_id->data;

	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	port->base = devm_ioremap_resource(&pdev->dev, iores);
	if (IS_ERR(port->base)) {
		return PTR_ERR(port->base);
	}
	/* Store the base for memory mapped registers */
	port->bgc.reg_dat = port->base;

	iores = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	port->base2 = devm_ioremap_resource(&pdev->dev, iores);
	if (IS_ERR(port->base2)) {
		return PTR_ERR(port->base2);
	}
	/* Store the base for memory mapped registers */
	port->bgc.reg_set = port->base2;

	port->irq = platform_get_irq(pdev, 0);
	if (port->irq < 0)
		return -EINVAL;

//printk("port->irq = %d base1 = 0x%08x base2 = 0x%08x\n",port->irq, port->base, port->base2 );
	/* disable the interrupt and clear the status */
	writel(~0, port->base + GPIO_ISR);

	/* setup one handler for each entry */
	irq_set_chained_handler(port->irq, mx3_gpio_irq_handler);
	irq_set_handler_data(port->irq, port);

	/*int bgpio_init(struct bgpio_chip *bgc, struct device *dev,
		       unsigned long sz, void __iomem *dat, void __iomem *set,
		       void __iomem *clr, void __iomem *dirout, void __iomem *dirin,
		       unsigned long flags);*/
	err = bgpio_init(&port->bgc, &pdev->dev, 4,
			 port->base2 + GPIO_PDOR,
			 port->base2 + GPIO_PSOR,
			 port->base2 + GPIO_PCOR,
			 NULL, NULL, 0);
	if (err)
		goto out_bgio;

	port->bgc.gc.to_irq = vf6xx_gpio_to_irq;
	port->bgc.gc.base = (pdev->id < 0) ? of_alias_get_id(np, "gpio") * 32 :
					     pdev->id * 32;
//printk("%s %d port->bgc.gc.base = %d err= %d\n", __FUNCTION__, __LINE__, port->bgc.gc.base, err);

	/* Create the gpio_chip object to manage gpios using the descriptor
	   interface */
	port->bgc.gc.label = "gpioctrl";
	port->bgc.gc.dev = NULL;
	port->bgc.gc.owner = THIS_MODULE;
	port->bgc.gc.list = port->node;
	port->bgc.gc.request = NULL;
	port->bgc.gc.free = NULL;
	port->bgc.gc.get_direction = vf6xx_gpio_get_direction;
	port->bgc.gc.direction_input = vf6xx_direction_input_set;
	port->bgc.gc.direction_output = vf6xx_direction_output_set;
	port->bgc.gc.get = vf6xx_gpio_val_get;
	port->bgc.gc.set = vf6xx_gpio_val_set;
	port->bgc.gc.dbg_show = NULL;
	port->bgc.gc.ngpio = 32;
	port->bgc.gc.desc = NULL;
	port->bgc.gc.can_sleep = 0;

#ifdef CONFIG_OF_GPIO
	port->bgc.gc.of_node = np;
	port->bgc.gc.of_gpio_n_cells = 1;
	port->bgc.gc.of_xlate = vf6xx_of_xlate;

#endif
#ifdef CONFIG_PINCTRL
	/* The pinctrl is also called the iomux.  */
	port->bgc.gc.pin_ranges = port->node;
#endif

	err = gpiochip_add(&port->bgc.gc);
	if (err < 0)
		goto out_bgpio_remove;

	irq_base = irq_alloc_descs(-1, 0, 32, numa_node_id());
printk("%s %d irq_base = %d\n", __FUNCTION__, __LINE__, irq_base);
	if (irq_base < 0) {
		err = irq_base;
		goto out_gpiochip_remove;
	}

	port->domain = irq_domain_add_legacy(np, 32, irq_base, 0,
					     &irq_domain_simple_ops, NULL);
	if (!port->domain) {
		err = -ENODEV;
		goto out_irqdesc_free;
	}

	/* gpio-vf6xx can be a generic irq chip */
	vf6xx_gpio_init_gc(port, irq_base);
	list_add_tail(&port->node, &vf6xx_gpio_ports);

	return 0;

out_irqdesc_free:
	printk( "%s failed with out_irqdesc_free errno %d\n", __func__, err);
	irq_free_descs(irq_base, 32);
out_gpiochip_remove:
	printk( "%s failed with out_gpiochip_remove errno %d\n", __func__, err);
	WARN_ON(gpiochip_remove(&port->bgc.gc) < 0);
out_bgpio_remove:
	printk( "%s failed with out_bgpio_remove errno %d\n", __func__, err);
	bgpio_remove(&port->bgc);
out_bgio:
	printk( "%s failed with out_bgio errno %d\n", __func__, err);
	return err;
}

static struct platform_driver vf6xx_gpio_driver = {
	.driver = {
		   .name = "fsl,vf610-gpio",
		   .owner = THIS_MODULE,
		   .of_match_table = vf6xx_gpio_dt_ids,
		   },
	.probe = vf6xx_gpio_probe,
	.id_table = vf6xx_gpio_ids,
};

static int __init gpio_vf6xx_init(void)
{
	return platform_driver_register(&vf6xx_gpio_driver);
}

postcore_initcall(gpio_vf6xx_init);

MODULE_AUTHOR("Freescale Semiconductor, "
	      "Daniel Mack <danielncaiaq.de>, "
	      "Juergen Beisert <kernel@pengutronix.de>");
MODULE_DESCRIPTION("Freescale MXC GPIO");
MODULE_LICENSE("GPL");
