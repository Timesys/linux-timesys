/*
* header goes here
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/sched.h>

#include <mach/mvf.h>

#define NUM_GATES 16
#define MASK_FROM_GATE(gate_num) ((u32)(1 << (NUM_GATES*2 - 1 - idx[gate_num])))

#define THIS_CORE (0)
#define LOCK_VALUE (THIS_CORE + 1)

#define SEMA4_CP0INE (MVF_SEMA4_BASE_ADDR + 0x40)
#define SEMA4_CP0NTF (MVF_SEMA4_BASE_ADDR + 0x80)

//#if 0
#include <linux/mm.h>
#include <linux/version.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <mach/hardware.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/time.h>
//#endif

#include <linux/debugfs.h>

#include <linux/mvf_sema4.h>

// ************************************ Local Data *************************************************

static MVF_SEMA4* gates[NUM_GATES];

static bool initialized = false;

// account for the way the bits are set / returned in CP0INE and CP0NTF
static const int idx[16] = {3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12};

// debugfs
#define DEBUGFS_DIR "mvf_sema4"
static struct dentry *debugfs_dir;

// ************************************ Interrupt handler *************************************************

static irqreturn_t sema4_irq_handler(int irq, void *dev_id)
{
	int gate_num;

	u32 cp0ntf = readl(MVF_IO_ADDRESS(SEMA4_CP0NTF));

	for(gate_num=0; gate_num<NUM_GATES; gate_num++)
	{
		// interrupt from this gate?
		if(cp0ntf & MASK_FROM_GATE(gate_num))
		{
			// grab the gate to stop the interrupts
			writeb(LOCK_VALUE, MVF_IO_ADDRESS(MVF_SEMA4_BASE_ADDR) + gate_num);

			// make sure there's a gate assigned
			if(gates[gate_num])
			{
				if(gates[gate_num]->use_interrupts) {
					// wake up whoever was aiting
					wake_up_interruptible(&(gates[gate_num]->wait_queue));
					// bump stats
					gates[gate_num]->interrupts++;
				}
			}
		}
	}

	return IRQ_HANDLED;
}

// ************************************ Utility functions *************************************************

static int initialize(void)
{
	int i;

	// clear the gates table
	for(i=0; i<NUM_GATES; i++)
		gates[i] = NULL;

	// clear out all notification requests
	writel(0, MVF_IO_ADDRESS(SEMA4_CP0INE));

	//Register the interrupt handler
	if (request_irq(MVF_INT_SEMA4, sema4_irq_handler, 0, "mvf_sema4_handler", NULL) != 0)
	{
		printk(KERN_ERR "Failed to register MVF_INT_SEMA4 interrupt.\n");
		return -EIO;
	}

	// debugfs
	debugfs_dir = debugfs_create_dir(DEBUGFS_DIR, NULL);

	initialized = true;
	return 0;
}

int mvf_sema4_assign(int gate_num, bool use_interrupts, MVF_SEMA4** sema4_p)
{
	int retval;
	u32 cp0ine;
	unsigned long irq_flags;
	char debugfs_gatedir[4];

	// take the opportunity to initialize the whole sub-system
	if(!initialized)
	{
		retval = initialize();
		if(retval)
			return retval;
	}

	if((gate_num < 0) || (gate_num >= NUM_GATES))
		return -EINVAL;

	if(gates[gate_num])
		return -EBUSY;

	*sema4_p = (MVF_SEMA4 *)kmalloc(sizeof(MVF_SEMA4), GFP_KERNEL);
	if(*sema4_p == NULL)
		return -ENOMEM;
	memset(*sema4_p, 0, sizeof(MVF_SEMA4));

	gates[gate_num] = *sema4_p;
	(*sema4_p)->gate_num = gate_num;
	(*sema4_p)->use_interrupts = use_interrupts;

	if(use_interrupts)
	{
		init_waitqueue_head(&((*sema4_p)->wait_queue));
		local_irq_save(irq_flags);
		cp0ine = readl(MVF_IO_ADDRESS(SEMA4_CP0INE));
		cp0ine |= MASK_FROM_GATE(gate_num);
		writel(cp0ine, MVF_IO_ADDRESS(SEMA4_CP0INE));
		local_irq_restore(irq_flags);
	}

	// debugfs
	sprintf(debugfs_gatedir, "%d", gate_num);
	debugfs_dir = debugfs_create_dir(debugfs_gatedir, debugfs_dir);
	debugfs_create_u32("attempts", 0444, debugfs_dir, &(*sema4_p)->attempts);
	debugfs_create_u32("interrupts", 0444, debugfs_dir, &(*sema4_p)->interrupts);

	return 0;
}

int mvf_sema4_deassign(MVF_SEMA4 *sema4)
{
	u32 cp0ine;
	unsigned long irq_flags;

	int gate_num;
	if(!sema4)
		return -EINVAL;
	gate_num = sema4->gate_num;

	if(sema4->use_interrupts)
	{
		local_irq_save(irq_flags);
		cp0ine = readl(MVF_IO_ADDRESS(SEMA4_CP0INE));
		cp0ine &= ~MASK_FROM_GATE(gate_num);
		writel(cp0ine, MVF_IO_ADDRESS(SEMA4_CP0INE));
		local_irq_restore(irq_flags);
	}

	kfree(sema4);
	gates[gate_num] = NULL;

	return 0;
}

int mvf_sema4_lock(MVF_SEMA4 *sema4, unsigned int timeout_us)
{
	int retval;
	int gate_num;
	if(!sema4)
		return -EINVAL;
	gate_num = sema4->gate_num;

	// cant use timeouts if not using interruppts
	// TODO use spin lock if not using interrupts
	if((!sema4->use_interrupts) && timeout_us)
		return -EINVAL;

	// bump stats
	gates[gate_num]->attempts++;

	// try to grab it
	writeb(LOCK_VALUE, MVF_IO_ADDRESS(MVF_SEMA4_BASE_ADDR) + gate_num);
	if(readb(MVF_IO_ADDRESS(MVF_SEMA4_BASE_ADDR) + gate_num) == LOCK_VALUE)
		return 0;

	// no timeout, fail
	if(!timeout_us)
		return -EBUSY;

	// wait forever?
	if(timeout_us == 0xffffffff)
	{
		if(wait_event_interruptible(sema4->wait_queue, (readb(MVF_IO_ADDRESS(MVF_SEMA4_BASE_ADDR) + gate_num) == LOCK_VALUE)))
			return -ERESTARTSYS;
	}
	else
	{
		// return: 0 = timeout, >0 = woke up with that many jiffies left, <0 = error
		retval = wait_event_interruptible_timeout(sema4->wait_queue,
							(readb(MVF_IO_ADDRESS(MVF_SEMA4_BASE_ADDR) + gate_num) == LOCK_VALUE),
							usecs_to_jiffies(timeout_us));
		if(retval == 0)
			return -ETIME;
		else if(retval < 0)
			return retval;
	}

	return 0;
}

int mvf_sema4_unlock(MVF_SEMA4 *sema4)
{
	if(!sema4)
		return -EINVAL;

	// unlock it
	writeb(0, MVF_IO_ADDRESS(MVF_SEMA4_BASE_ADDR) + sema4->gate_num);

	return 0;
}

