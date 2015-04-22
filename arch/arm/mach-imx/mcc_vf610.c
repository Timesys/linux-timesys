/*
 * Copyright (C) 2014-2015 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0+ and/or BSD-3-Clause
 * The GPL-2.0+ license for this file can be found in the COPYING.GPL file
 * included with this distribution or at
 * http://www.gnu.org/licenses/gpl-2.0.html
 * The BSD-3-Clause License for this file can be found in the COPYING.BSD file
 * included with this distribution or at
 * http://opensource.org/licenses/BSD-3-Clause
 */

#include <linux/io.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mcc_config_linux.h>
#include <linux/mcc_common.h>
#include <linux/mcc_linux.h>
#include <linux/mcc_vf610.h>

unsigned int imx_mcc_buffer_freed = 0, imx_mcc_buffer_queued = 0;
/* Used for blocking send */
static DECLARE_WAIT_QUEUE_HEAD(buffer_freed_wait_queue);
/* Used for blocking recv */
static DECLARE_WAIT_QUEUE_HEAD(buffer_queued_wait_queue);

/*!
 * \brief This function returns the core number
 *
 * \return int
 */
unsigned int _psp_core_num(void)
{
    return 0;
}

/*!
 * \brief This function returns the node number
 *
 * \return unsigned int
 */
unsigned int _psp_node_num(void)
{
    return MCC_LINUX_NODE_NUMBER;
}


int mcc_wait_for_buffer_freed(MCC_RECEIVE_BUFFER **buffer, unsigned int timeout)
{
    int return_value;
    unsigned long timeout_j; /* jiffies */
    MCC_RECEIVE_BUFFER *buf = null;

	/*
	 * Blocking calls: CPU-to-CPU ISR sets the event and thus
	 * resumes tasks waiting for a free MCC buffer.
	 * As the interrupt request is send to all cores when a buffer
	 * is freed it could happen that several tasks from different
	 * cores/nodes are waiting for a free buffer and all of them
	 * are notified that the buffer has been freed. This function
	 * has to check (after the wake up) that a buffer is really
	 * available and has not been already grabbed by another
	 * "competitor task" that has been faster. If so, it has to
	 * wait again for the next notification.
	 */
	while (buf == null) {
		if (timeout == 0xffffffff) {
			/*
			 * In order to level up the robust, do not always
			 * wait event here. Wake up itself after every 1~s.
			 */
			timeout_j = usecs_to_jiffies(1000);
			wait_event_timeout(buffer_freed_wait_queue,
					imx_mcc_buffer_freed == 1, timeout_j);
		} else {
			timeout_j = msecs_to_jiffies(timeout);
			wait_event_timeout(buffer_freed_wait_queue,
					imx_mcc_buffer_freed == 1, timeout_j);
		}

		return_value = mcc_get_semaphore();
		if (return_value != MCC_SUCCESS)
			return return_value;

		MCC_DCACHE_INVALIDATE_MLINES((void *)
				&bookeeping_data->free_list,
				sizeof(MCC_RECEIVE_LIST *));

		buf = mcc_dequeue_buffer(&bookeeping_data->free_list);
		mcc_release_semaphore();
		if (imx_mcc_buffer_freed)
			imx_mcc_buffer_freed = 0;
	}

	*buffer = buf;
	return MCC_SUCCESS;
}

int mcc_wait_for_buffer_queued(MCC_ENDPOINT *endpoint, unsigned int timeout)
{
	unsigned long timeout_j; /* jiffies */
	MCC_RECEIVE_LIST *tmp_list;

	/* Get list of buffers kept by the particular endpoint */
	tmp_list = mcc_get_endpoint_list(*endpoint);

	if (timeout == 0xffffffff) {
		wait_event(buffer_queued_wait_queue,
				imx_mcc_buffer_queued == 1);
		mcc_get_semaphore();
		/*
		* double check if the tmp_list head is still null
		* or not, if yes, wait again.
		*/
		while (tmp_list->head == null) {
			imx_mcc_buffer_queued = 0;
			mcc_release_semaphore();
			wait_event(buffer_queued_wait_queue,
					imx_mcc_buffer_queued == 1);
			mcc_get_semaphore();
		}
	} else {
		timeout_j = msecs_to_jiffies(timeout);
		wait_event_timeout(buffer_queued_wait_queue,
				imx_mcc_buffer_queued == 1, timeout_j);
		mcc_get_semaphore();
	}

	if (imx_mcc_buffer_queued)
		imx_mcc_buffer_queued = 0;

	if (tmp_list->head == null) {
		pr_err("%s can't get queued buffer.\n", __func__);
		mcc_release_semaphore();
		return MCC_ERR_TIMEOUT;
	}

	tmp_list->head = (MCC_RECEIVE_BUFFER *)
		MCC_MEM_PHYS_TO_VIRT(tmp_list->head);
	mcc_release_semaphore();

	return MCC_SUCCESS;
}

/*!
 * \brief This function triggers the CPU-to-CPU interrupt.
 *
 * Platform-specific software triggering the inter-CPU interrupts.
 */
int mcc_triger_cpu_to_cpu_interrupt(void)
{
	writel_relaxed(VF610_M4_CORE_NUMBER << VF610_MSCM_IRCPGIR_CPUTL_SHIFT,
		mscm_base + VF610_MSCM_IRCPGIR);

	return MCC_SUCCESS;
}

/*!
 * \brief This function enable the CPU-to-CPU interrupt.
 *
 * Platform-specific software enable the inter-CPU interrupts.
 */
int imx_mcc_bsp_int_enable(void)
{
	int i;

	for(i = 0; i< VF610_MAX_CPU_TO_CPU_INTERRUPTS; i++)
		enable_irq(VF610_INT_CPU_INT0 + i);

	return MCC_SUCCESS;
}

/*!
 * \brief This function disable the CPU-to-CPU interrupt.
 *
 * Platform-specific software disable the inter-CPU interrupts.
 */
int imx_mcc_bsp_int_disable(void)
{
	int i;

	for(i = 0; i< VF610_MAX_CPU_TO_CPU_INTERRUPTS; i++)
		disable_irq(VF610_INT_CPU_INT0 + i);

	return MCC_SUCCESS;
}

int nprint = -3;
int bad = 0;
irqreturn_t cpu_to_cpu_irq_handler(int irq, void *dev_id)
{
	MCC_SIGNAL signal;
	int ret;
	int i = 0;

	int interrupt_id = irq - VF610_INT_CPU_INT0;
	if(bad) {
		//Clear the interrupt status
		VF610_MSCM_WRITE((VF610_MSCM_IRCPnIR_INT0_MASK<<interrupt_id), VF610_MSCM_IRCPnIR);

		return IRQ_HANDLED;
	}


	if(nprint > 0) {
		printk("==============\n");
	}

	mcc_get_semaphore();
	ret = mcc_dequeue_signal(MCC_CORE_NUMBER, &signal);

	while(ret)
	{
		if(signal.type == BUFFER_FREED)
		{
			imx_mcc_buffer_freed = 1;
                        wake_up(&buffer_freed_wait_queue);

		}
		else if(signal.destination.port == MCC_RESERVED_PORT_NUMBER)
		{
			printk("**********bad destination port! it is set to MCC_RESERVED_PORT_NUMBER!!\n");
			bad = 1;
			nprint = -1;
		}
		else
		{
				if(signal.type == BUFFER_QUEUED && signal.destination.core ==  MCC_CORE_NUMBER)
				{
					imx_mcc_buffer_queued = 1;
		                        wake_up(&buffer_queued_wait_queue);
					break;
				}
		}

		if(nprint > 0)
		{
			printk("==============\n");
		}

		ret = mcc_dequeue_signal(MCC_CORE_NUMBER, &signal);
	}

	//Clear the interrupt status
	VF610_MSCM_WRITE((VF610_MSCM_IRCPnIR_INT0_MASK<<interrupt_id), VF610_MSCM_IRCPnIR);

	mcc_release_semaphore();

	return IRQ_HANDLED;
}
