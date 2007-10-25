/*
 * Copyright 2004-2007 Freescale Semiconductor, Inc. All Rights Reserved.
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
 * @file plat-mxc/sdma/sdma_malloc.c
 * @brief This file contains functions for SDMA non-cacheable buffers allocation
 *
 * SDMA (Smart DMA) is used for transferring data between MCU and peripherals
 *
 * @ingroup SDMA
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <asm/dma.h>
#include <asm/mach/dma.h>
#include <asm/arch/hardware.h>

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>

#define DEBUG 0

#if DEBUG
#define DPRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

/*!
 * Defines SDMA non-cacheable buffers pool
 */
static struct dma_pool *pool;

/*!
 * SDMA memory conversion hashing structure
 */
typedef struct {
	struct list_head node;
	int use_count;
	/*! Virtual address */
	void *virt;
	/*! Physical address */
	unsigned long phys;
} virt_phys_struct;

static struct list_head buf_map;

/*!
 * Defines the size of each buffer in SDMA pool.
 * The size must be at least 512 bytes, because
 * sdma channel control blocks array size is 512 bytes
 */
#define SDMA_POOL_SIZE 1024

/*!
 * Adds new buffer structure into conversion hash tables
 *
 * @param   vf   SDMA memory conversion hashing structure
 *
 * @return       1 on success, 0 on fail
 */
static int add_entry(virt_phys_struct * vf)
{
	virt_phys_struct *p;

	vf->phys &= PAGE_MASK;
	vf->virt = (void *)((u32) vf->virt & PAGE_MASK);

	list_for_each_entry(p, &buf_map, node) {
		if (p->virt == vf->virt) {
			p->use_count++;
			return 0;
		}
	}

	p = kmalloc(sizeof(virt_phys_struct), GFP_KERNEL);
	if (p == 0) {
		return -ENOMEM;
	}

	*p = *vf;
	p->use_count = 1;
	list_add_tail(&p->node, &buf_map);

	DPRINTK("added vaddr 0x%p, paddr 0x%08X to list\n", p->virt, p->phys);

	return 0;
}

/*!
 * Deletes buffer stracture from conversion hash tables
 *
 * @param   buf   SDMA memory buffer virtual addr
 *
 * @return       0 on success, -1 on fail
 */
static int delete_entry(void *buf)
{
	virt_phys_struct *p;

	buf = (void *)((u32) buf & PAGE_MASK);

	list_for_each_entry(p, &buf_map, node) {
		if (p->virt == buf) {
			p->use_count--;
			break;
		}
	}

	if (p->use_count == 0) {
		list_del(&p->node);
		kfree(p);
	}

	return 0;
}

/*!
 * Virtual to physical address conversion functio
 *
 * @param   buf  pointer to virtual address
 *
 * @return       physical address
 */
unsigned long sdma_virt_to_phys(void *buf)
{
	u32 offset = (u32) buf & (~PAGE_MASK);
	virt_phys_struct *p;

	DPRINTK("searching for vaddr 0x%p\n", buf);

	list_for_each_entry(p, &buf_map, node) {
		if ((u32) p->virt == ((u32) buf & PAGE_MASK)) {
			return p->phys | offset;
		}
	}

	if (virt_addr_valid(buf)) {
		return virt_to_phys(buf);
	}

	printk(KERN_WARNING
	       "SDMA malloc: could not translate virt address 0x%p\n", buf);
	return 0;
}

/*!
 * Physical to virtual address conversion functio
 *
 * @param   buf  pointer to physical address
 *
 * @return       virtual address
 */
void *sdma_phys_to_virt(unsigned long buf)
{
	u32 offset = buf & (~PAGE_MASK);
	virt_phys_struct *p;

	list_for_each_entry(p, &buf_map, node) {
		if (p->phys == (buf & PAGE_MASK)) {
			return (void *)((u32) p->virt | offset);
		}
	}

	printk(KERN_WARNING
	       "SDMA malloc: could not translate phys address 0x%lx\n", buf);
	return 0;
}

/*!
 * Allocates uncacheable buffer
 *
 * @param   size    size of allocated buffer
 * @return  pointer to buffer
 */
void *sdma_malloc(size_t size)
{
	void *buf;
	dma_addr_t dma_addr;
	virt_phys_struct vf;

	if (size > SDMA_POOL_SIZE) {
		printk(KERN_WARNING
		       "size in sdma_malloc is more than %d bytes\n",
		       SDMA_POOL_SIZE);
		buf = 0;
	} else {
		buf = dma_pool_alloc(pool, GFP_KERNEL, &dma_addr);
		if (buf > 0) {
			vf.virt = buf;
			vf.phys = dma_addr;

			if (add_entry(&vf) < 0) {
				dma_pool_free(pool, buf, dma_addr);
				buf = 0;
			}
		}
	}

	DPRINTK("allocated vaddr 0x%p\n", buf);
	return buf;
}

/*!
 * Frees uncacheable buffer
 *
 * @param  buf    buffer pointer for deletion
 */
void sdma_free(void *buf)
{
	dma_pool_free(pool, buf, sdma_virt_to_phys(buf));
	delete_entry(buf);
}

/*!
 * SDMA buffers pool initialization function
 */
void __init init_sdma_pool(void)
{
	pool = dma_pool_create("SDMA", NULL, SDMA_POOL_SIZE, 0, 0);

	INIT_LIST_HEAD(&buf_map);
}

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MXC Linux SDMA API");
MODULE_LICENSE("GPL");
