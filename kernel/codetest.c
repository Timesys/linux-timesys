/*
-------------------------------------------------------------------------------|
|
| codetest.c - Linux implementation of CodeTEST RTOS event hooks
|
| Copyright (c) 2003 - 2007 Freescale Semiconductor, Inc.
|
| This software is licensed under the GPL.  It may be redistributed
| and/or modified under the terms of the GNU General Public License as
| published by the Free Software Foundation; either version 2 of the
| License, or (at your option) any later version.
|
| Module version: Revision: 1.11
|
-------------------------------------------------------------------------------|
*/

#include <linux/kernel.h>
#include <asm/io.h>
#include <asm/page.h>
#include <linux/sched.h>
#include <asm/current.h>
#include <linux/module.h>
#include <linux/string.h>
#include <asm/system.h>
#include <linux/mm.h>
#include <asm/pgalloc.h>
#include <linux/autoconf.h>
#ifdef CONFIG_ARM
#include <linux/time.h>
#endif

/*
** HWIC definitions
*/
#define CT_TASK_CREATE    (0x2a100000)
#define CT_TASK_ENTER     (0x2a200000)
#define CT_TASK_DELETE    (0x2a300000)
#define CT_TASK_EXIT      (0x2a400000)
#define CT_ISR_ENTER      (0x2a500000)
#define CT_ISR_EXIT       (0x2a600000)

#define CT_TASK_NAMES     	(0x00020000)
#define CT_TASK_IDS       	(0x00010000)
#define CT_TPP_OVERRUN_TAG	(0x2c400000)

#ifdef CONFIG_CT_USEPCI
#include <linux/pci.h>

#ifdef CodeTEST
#pragma no_tagging ctTag
#endif

#define CT_VENDOR_ID           (0x11ab)
#define CT_DEVICE_ID           (0xf007)

enum {
	CT_BAR_0,
	CT_BAR_1,
	CT_BAR_2,
	CT_BAR_3,
	CT_BAR_4,
	CT_BAR_5,
	CT_NUM_RESOURCES
};

#endif				/* CONFIG_CT_USEPCI */

volatile unsigned long *ct_ctrl_port_ptr = NULL;
volatile unsigned long *ct_data_port_ptr = NULL;
void ct_enable_pid_task_name(void);
void ct_disable_pid_task_name(void);
void ct_enable_hooks(void);
void ct_disable_hooks(void);
extern unsigned long ct_get_phys_addr(void);
extern void ct_set_phys_addr(unsigned long addr);
int ctTag(unsigned long);
int ctDataTag(unsigned long tagID);

#ifndef CONFIG_CT_HOOKS_ENABLED
#define CONFIG_CT_HOOKS_ENABLED  (0)
#endif

#ifdef CONFIG_CT_USEMICTOR
#define CT_PHYS_ADDR CONFIG_CT_ADDR
#else
#define CT_PHYS_ADDR 0
#endif

#ifndef CONFIG_CT_BIG_ENDIAN
#define _LITTLE_ENDIAN
#endif

static unsigned long ct_phys_addr = CT_PHYS_ADDR;
static unsigned long ct_hooks_enabled = CONFIG_CT_HOOKS_ENABLED;

unsigned ct_pid_task_name_enabled = 0;
/*
**    SWIC definitions
*/
#include "ctswic.h"

/* the kernel event tags are placed in SWIC buffer*/
unsigned ct_swic_enabled = 0;
/* the ISR event tags are ignored */
unsigned ct_filter_events = 1;
/* DEBUG -  force tags output to HWIC ports when SWIC enabled */
unsigned ct_force_hwic_enabled = 0;
/* reference to ring buffer defined in ctdriver */
struct Hook_Buffer *ct_hook_buffer = NULL;

static spinlock_t hook_buffer_lock = SPIN_LOCK_UNLOCKED;
static unsigned long lock_flags;

#define HI_BIT_ONLY 0x80000000
#define HOOK_INC(x)  ((x) = (((x)+1) & (HOOK_BUFFER_SIZE-1)))

unsigned task_tag_count = 0;
static unsigned dropped = 0;
static pid_t deleted_thread_id = -1;

typedef unsigned long long U64;

void ctWriteSWICTaskTags(unsigned n_dtags, const u32 dtags[], u32 tag);
int ct_encode_taskname(const char *name, u32 buffer[8], u32 pid);
u32 ctTagClock(void);

EXPORT_SYMBOL(ct_enable_pid_task_name);
EXPORT_SYMBOL(ct_disable_pid_task_name);

/* HWIC kernel exports */
EXPORT_SYMBOL(ct_enable_hooks);
EXPORT_SYMBOL(ct_disable_hooks);
EXPORT_SYMBOL(ct_get_phys_addr);
EXPORT_SYMBOL(ct_set_phys_addr);
EXPORT_SYMBOL(ctTag);
EXPORT_SYMBOL(ctDataTag);

/* SWIC kernel exports */
EXPORT_SYMBOL(ct_swic_enabled);
EXPORT_SYMBOL(ct_filter_events);
EXPORT_SYMBOL(ct_force_hwic_enabled);
EXPORT_SYMBOL(ct_hook_buffer);

/*
-------------------------------------------------------------------------------|
|
| ct_init() - perform physical -> kernel virtual mapping for the tag ports
|
-------------------------------------------------------------------------------|
*/
int ct_init(void)
{
#ifdef CONFIG_CT_USEPCI
	struct pci_dev *ct;
	static int printed = 0;
#endif

	/*
	 ** Check to see if the tag pointers have been initialized
	 */
	if (ct_ctrl_port_ptr == NULL) {
#ifdef CONFIG_CT_USEPCI
		/*
		 ** Check to see if the CodeTEST PCI adapter has been found
		 */
		if ((ct = pci_find_device(CT_VENDOR_ID, CT_DEVICE_ID, 0)) == 0) {
			/*
			 ** Be polite and only print once every 4 billion tries
			 */
			if (!printed++) {
				printk("CODETEST: PCI device not located\n");
			}
			return -1;
		}
		if (pci_enable_device(ct) < 0) {
			printk("CODETEST: fail to enable PCI device\n");
			return -1;
		}
		/*printk( "CODETEST: found device \"%s\"\n", ct->name ); */
		printk
		    ("CODETEST: resource \"%s\", start 0x%08x, end 0x%08x, flags 0x%08x\n",
		     ct->resource[CT_BAR_2].name,
		     (unsigned int)ct->resource[CT_BAR_2].start,
		     (unsigned int)ct->resource[CT_BAR_2].end,
		     (unsigned int)ct->resource[CT_BAR_2].flags);
		/*
		 ** Store the address of the PCI adapter tag port window
		 */
		ct_phys_addr = ct->resource[CT_BAR_2].start;
#endif

		/*
		 ** Map the physical addresses of the ports to kernel virtual addresses
		 */
		printk("CODETEST: attempting to map physical address 0x%08lx\n",
		       (unsigned long)ct_phys_addr);
		ct_ctrl_port_ptr =
		    (unsigned long *)ioremap_nocache(ct_phys_addr, PAGE_SIZE);
		if (ct_ctrl_port_ptr != NULL) {
			ct_data_port_ptr = ct_ctrl_port_ptr + 1;
			printk
			    ("CODETEST: physical address 0x%08lx mapped to kernel virtual address 0x%08lx\n",
			     ct_phys_addr, (unsigned long)ct_ctrl_port_ptr);
		} else {
			printk
			    ("CODETEST: ioremap cannot map physical address 0x%08lx to a kernel virtual address\n",
			     ct_phys_addr);
			ct_hooks_enabled = 0;
		}

	}

	return 0;
}

/***************************************************************************/
int ctTag(unsigned long tagID)
{
	if (ct_ctrl_port_ptr == NULL)
		return 0;

	*ct_ctrl_port_ptr = tagID;
	return (tagID);

}				/* ctTag */

/***************************************************************************/
int ctDataTag(unsigned long tagID)
{
	if (ct_data_port_ptr == NULL)
		return 0;

	*ct_data_port_ptr = tagID;
	return (tagID);

}				/* ctDataTag */

void ctWriteHWICTaskTags(unsigned n_dtags, const u32 dtags[], u32 tag)
{
	int i = 0;

	/*
	 ** If enabled, check to see if the tag pointers have been initialized
	 */

	if (((ct_hooks_enabled == 0) && (ct_force_hwic_enabled == 0)) ||
	    ((ct_ctrl_port_ptr == NULL) && (ct_init() == -1))) {
		return;
	}

	while (i < n_dtags) {
		*ct_data_port_ptr = dtags[i];
		i++;
	}
	*ct_ctrl_port_ptr = tag;

	return;
}

/*
-------------------------------------------------------------------------------|
|
| Accessor functions
|
-------------------------------------------------------------------------------|
*/

void ct_enable_pid_task_name(void)
{
	ct_pid_task_name_enabled = 1;
}

void ct_disable_pid_task_name(void)
{
	ct_pid_task_name_enabled = 0;
}

void ct_enable_hooks(void)
{
	ct_hooks_enabled = 1;
}

void ct_disable_hooks(void)
{
	ct_hooks_enabled = 0;
}

unsigned long ct_get_phys_addr(void)
{
	return ct_phys_addr;
}

void ct_set_phys_addr(unsigned long addr)
{
	ct_phys_addr = addr;
}

/*
 This function is called to insert a Kernel event tags in the ring buffer
 Arguments: 	n_dtags - number of data tags for this event
		dtags 	- the data tags vector
		tag	- the control tag for this event
*/
void ctWriteSWICTaskTags(unsigned n_dtags, const u32 dtags[], u32 tag)
{
	struct Hook_Buffer *hb = ct_hook_buffer;
	u32 *pdtags = (u32 *) dtags;
	Q_Tag *qp;
	/*unsigned long x; */

	task_tag_count++;	/* statistic gathering */
	/*
	   hb should not be null, but this may be a kernal task
	   so we are being extra careful
	 */
	if (hb) {
		int n_tags = n_dtags + 1;
		/* compute number of empty slots in the ring buffer */
		int slots;

		struct page *ppage;
		size_t hb_size =
		    (sizeof(struct Hook_Buffer) +
		     PAGE_SIZE) / PAGE_SIZE * PAGE_SIZE;

		spin_lock_irqsave(&hook_buffer_lock, lock_flags);

#ifdef CT_SWIC_BUFFER_NOCACHE
		/* update cache from physical memory */
		for (ppage = virt_to_page((char *)hb);
		     ppage < virt_to_page((char *)hb + hb_size); ppage++) {
			flush_dcache_page(ppage);
		}
#endif

		slots = ((hb->head - hb->tail) & (HOOK_BUFFER_SIZE - 1));

		if (slots == 0) {
			slots = HOOK_BUFFER_SIZE - 1;
		} else
			slots--;

		/*
		   Disable hardware interrupts while writing tags to the hook buffer. Otherwise, with the
		   ISR hooks enabled, task and ISR tags could be interleaved.
		 */
		/*local_irq_save(x); */

		if (slots < n_tags) {
			/* not enough room, flush all but first tag as its
			   not safe to mess with tag at head */
			int x = hb->head;
			while (hb->buffer[x].a_time == HI_BIT_ONLY) {
				/* skip over data tags */
				HOOK_INC(x);
			}
			HOOK_INC(x);	/* keep this control tag */
			hb->tail = x;

			/*
			   Insert a TPP OVERRUN tag to mark the drop of the full buffer
			 */
			qp = &hb->buffer[hb->tail];
			qp->tag = CT_TPP_OVERRUN_TAG;	/* the control tag */
			qp->a_time = ctTagClock() & ~HI_BIT_ONLY;

			HOOK_INC(hb->tail);

			dropped++;	/* stats */
		}

		{
			while (n_dtags > 0) {
				qp = &hb->buffer[hb->tail];
				qp->tag = *pdtags;
				qp->a_time = HI_BIT_ONLY;

				HOOK_INC(hb->tail);
				pdtags++;
				n_dtags--;
			}
			qp = &hb->buffer[hb->tail];
			qp->tag = tag;	/* the control tag */
			qp->a_time = ctTagClock() & ~HI_BIT_ONLY;

			HOOK_INC(hb->tail);
		}
		/*
		   Reenable hardware interrupts
		 */
		/*local_irq_restore(x); */

#ifdef CT_SWIC_BUFFER_NOCACHE
		/* write cache into physical memory */
		for (ppage = virt_to_page((char *)hb);
		     ppage < virt_to_page((char *)hb + hb_size); ppage++) {
			flush_dcache_page(ppage);
		}
#endif

		spin_unlock_irqrestore(&hook_buffer_lock, lock_flags);

	}
}

/* -------------------------------------------
int ct_encode_taskname(const char* name, u32 buffer[8])

 packs name into buffer as required for output as codetest tags.
 (TBD, find and reference appropriate doc)

  name is truncated to 32 chars.

   return value is number of words written into buffer[]


------------------------------------------------------- */

int ct_encode_taskname(const char *name_recv, u32 buffer[8], u32 pid)
{
	size_t len;
	int ret;		/* return value, number of words written into buffer */
	int index;		/* index for buffer [] */
	const char *p;		/* walks name backwards in multiples of 4 */
	char pid_string[32];
	int x, i, j;
	char name[100];

	strcpy(name, name_recv);
	len = strlen(name);
	if (ct_pid_task_name_enabled) {
		/*sprintf(pid_string,"%d",pid);
		   printk("CODETEST: %s\n",pid_string);
		   strcat(name,pid_string); */
		pid_string[0] = '0';
		for (x = pid, i = 0; x > 0; i++) {
			pid_string[i] = (x % 10) + '0';
			x = x / 10;
		}
		pid_string[i] = '\0';
		//printk("CODETEST: p=%s\n",pid_string);
		if (i == 0)
			i++;
		name[len] = '_';
		len++;
		name[len] = '\0';
		for (x = i - 1, j = 0; x >= 0; x--, j++)
			name[len + j] = pid_string[x];
		name[len + j] = '\0';
		len = strlen(name);
		//printk("CODETEST: n=%s\n",name);
	}
	if (len > 31)
		len = 31;	/* truncate if name is too long */
	ret = (len + 3) / 4;

	p = name + (ret - 1) * 4;

	/* The first word is special as it is null padded if
	   len is not a multiple of 4 */

	buffer[0] = 0;
	switch (len % 4) {
	case 0:
		p += 4;
		ret += 1;
		break;
	case 3:
		buffer[0] += p[2] << 8;
	case 2:
		buffer[0] += p[1] << 16;
	case 1:
		buffer[0] += p[0] << 24;
	}
	p -= 4;

	for (index = 1; index < ret; index++, p -= 4) {
		buffer[index] = (p[0] << 24) + (p[1] << 16)
		    + (p[2] << 8) + p[3];
	}

	return ret;
}

#if defined(CONFIG_X86)

static U64 rawClock(void)
{				/* Linux version */
	register U64 result asm("eax");	/* this works, placing upper bits in edx! */
	asm("rdtsc");		/* long long value in edx:eax */
	return result;
}

/*
Only 31 bits of the clock are used in a CodeTEST tag.
The absolute time to delta time computation in swtpp.c{.w},
assumes two adjacent tags have absolute times with difference less than
2^31, so it is desirable that the clock not roll over quickly.
Shifting the 64 bit clock by 11, will give a clock rate of
about 2^20 ticks per second on a 2GHz cpu. So, the 31 bit clock will
roll over in 34 minutes.
*/

#define CLOCKSHIFT   11

u32 ctTagClock(void)
{
	return (rawClock()) >> CLOCKSHIFT;
}

#elif defined(CONFIG_PPC)

static u32 rawClock(void)
{
	register unsigned long low asm("%r3");
	register unsigned long high asm("%r4");

	asm("mftb %r3 ");	/* Return value for PPC */
	asm("mftbu %r4");	/* Return upper value for PPC */

	return low & 0x7fffffff;
}

u32 ctTagClock(void)
{
	return rawClock();
}

#else

#define MICROS_PER_SECOND  1000000

static u32 rawClock(void)
{
	struct timeval tv;
	do_gettimeofday(&tv);
	return (tv.tv_sec * MICROS_PER_SECOND + tv.tv_usec);
}

u32 ctTagClock(void)
{
	return rawClock();
}

#endif

/*
-------------------------------------------------------------------------------|
|
| Thread event hooks
|
-------------------------------------------------------------------------------|
*/

/*
    These functions are called from kernel for task context changes and ISR events
*/
void ct_thread_create(struct task_struct *p)
{
	u32 count;
	u32 tagBuf[9];

	count = ct_encode_taskname(p->comm, tagBuf, p->pid);
	tagBuf[count] = p->pid;

	if (ct_hooks_enabled || ct_force_hwic_enabled)
		ctWriteHWICTaskTags(count + 1, tagBuf,
				    (CT_TASK_CREATE) |
				    (CT_TASK_NAMES) |
				    ((count - 0x01) & 0xffff));

	if ((ct_hook_buffer != NULL) && (ct_swic_enabled != 0))
		ctWriteSWICTaskTags(count + 1, tagBuf,
				    (CT_TASK_CREATE) |
				    (CT_TASK_NAMES) |
				    ((count - 0x01) & 0xffff));
}

void ct_thread_delete(struct task_struct *p)
{
	u32 count;
	u32 tagBuf[9];

	deleted_thread_id = p->pid;
	count = ct_encode_taskname(p->comm, tagBuf, p->pid);
	tagBuf[count] = p->pid;

	if (ct_hooks_enabled || ct_force_hwic_enabled)
		ctWriteHWICTaskTags(count + 1, tagBuf,
				    (CT_TASK_DELETE) |
				    (CT_TASK_NAMES) |
				    ((count - 0x01) & 0xffff));

	if ((ct_hook_buffer != NULL) && (ct_swic_enabled != 0))
		ctWriteSWICTaskTags(count + 1, tagBuf,
				    (CT_TASK_DELETE) |
				    (CT_TASK_NAMES) |
				    ((count - 0x01) & 0xffff));
}

void ct_thread_enter(struct task_struct *next)
{
	u32 count;
	u32 tagBuf[9];

	count = ct_encode_taskname(next->comm, tagBuf, next->pid);
	tagBuf[count] = next->pid;

	if (ct_hooks_enabled || ct_force_hwic_enabled)
		ctWriteHWICTaskTags(count + 1, tagBuf,
				    (CT_TASK_ENTER) |
				    (CT_TASK_NAMES) |
				    ((count - 0x01) & 0xffff));

	if ((ct_hook_buffer != NULL) && (ct_swic_enabled != 0))
		ctWriteSWICTaskTags(count + 1, tagBuf,
				    (CT_TASK_ENTER) |
				    (CT_TASK_NAMES) |
				    ((count - 0x01) & 0xffff));
}

void ct_thread_exit(struct task_struct *prev)
{
	u32 count;
	u32 tagBuf[9];

	/*
	 ** If this exit comes after the thread have been deleted ignore this tag
	 ** This algorithm assumes that after a delete event always comes comes an
	 ** exit event for the deleted thread
	 */
	if (prev->pid != deleted_thread_id) {
		count = ct_encode_taskname(prev->comm, tagBuf, prev->pid);
		tagBuf[count] = prev->pid;

		if (ct_hooks_enabled || ct_force_hwic_enabled)
			ctWriteHWICTaskTags(count + 1, tagBuf,
					    (CT_TASK_EXIT) |
					    (CT_TASK_NAMES) |
					    ((count - 0x01) & 0xffff));

		if ((ct_hook_buffer != NULL) && (ct_swic_enabled != 0))
			ctWriteSWICTaskTags(count + 1, tagBuf,
					    (CT_TASK_EXIT) |
					    (CT_TASK_NAMES) |
					    ((count - 0x01) & 0xffff));
	} else {
		/* ignore the exit event */
		deleted_thread_id = -1;
	}
}

/*
-------------------------------------------------------------------------------|
|
| ISR hooks
|
-------------------------------------------------------------------------------|
*/
void ct_isr_enter(int irq)
{
	if (irq == 0)
		return;
	if (ct_hooks_enabled || ct_force_hwic_enabled) {
		/*
		 ** Check to see if the tag pointers have been initialized
		 */
		/*
		 ** Initialization call removed because, on some xscale platforms,
		 ** interrupts start before virtual memory has been initialized.
		 ** JCG 12/2/02
		 **
		 if( ct_ctrl_port_ptr == NULL && ct_init() == -1 )
		 */
		if (ct_ctrl_port_ptr == NULL) {
			return;
		}

		*ct_ctrl_port_ptr = CT_ISR_ENTER | (irq & 0xffff);
	}
	if ((ct_hook_buffer != NULL) && (ct_swic_enabled != 0)
	    && (ct_filter_events == 0)) {
		ctWriteSWICTaskTags(0, NULL, (CT_ISR_ENTER) | (irq & 0xffff));
	}

}

void ct_isr_exit(int irq)
{
	if (irq == 0)
		return;

	if (ct_hooks_enabled || ct_force_hwic_enabled) {
		/*
		 ** Check to see if the tag pointers have been initialized
		 */
		/*
		 ** Initialization call removed because interrupts start before
		 ** virtual memory has been initialized on some platforms
		 ** JCG 12/2/02
		 **
		 if( ct_ctrl_port_ptr == NULL && ct_init() == -1 )
		 */
		if (ct_ctrl_port_ptr == NULL) {
			return;
		}

		*ct_ctrl_port_ptr = CT_ISR_EXIT | (irq & 0xffff);
	}
	if ((ct_hook_buffer != NULL) && (ct_swic_enabled != 0)
	    && (ct_filter_events == 0)) {
		ctWriteSWICTaskTags(0, NULL, (CT_ISR_EXIT) | (irq & 0xffff));
	}
}
