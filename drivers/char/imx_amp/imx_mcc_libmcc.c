/*
 * imx_mcc_tty.c - tty demo driver used to test imx mcc
 * posix tty interface.
 *
 * Copyright (C) 2014-2015 Freescale Semiconductor, Inc.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/mcc_config_linux.h>
#include <linux/mcc_common.h>
#include <linux/mcc_api.h>
#include <linux/mcc_linux.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/cdev.h>

typedef unsigned int MCC_READ_MODE;

/**
 * struct mcctty_port - Wrapper struct for imx mcc tty port.
 * @port:		TTY port data
 */
struct mcctty_port {
	struct delayed_work	read;
	struct tty_port		port;
	spinlock_t		rx_lock;
};

static struct mcctty_port mcc_tty_port;

enum {
	MCC_NODE_A9 = 0,
	MCC_NODE_M4 = 0,

	MCC_A9_PORT = 1,
	MCC_M4_PORT = 2,
};

typedef enum write_mode_enum {MODE_IMAGE_LOAD, MODE_MCC} WRITE_MODE;
char *virt_load_addr;
int phys_load_addr;
int phys_start_addr;
struct file *myfile;
WRITE_MODE write_mode;
int offset = 0;

static dev_t first;
static struct cdev c_dev;
static struct class *cl;

/* mcc tty/pingpong demo */
static MCC_ENDPOINT mcc_endpoint_a9_pingpong = {0, MCC_NODE_A9, MCC_A9_PORT};
static MCC_ENDPOINT mcc_endpoint_m4_pingpong = {1, MCC_NODE_M4, MCC_M4_PORT};
struct mcc_tty_msg {
	int data;
};

static int mcctty_put_char(struct tty_struct *tty, unsigned char ch)
{
	if(write_mode == MODE_IMAGE_LOAD)
        {
                writeb(ch, virt_load_addr + offset);
                offset += 1;
                return 1;
        }
	printk("mcctty_put_char (for MQX image load): should not be here!\n");
	return -1;
}
static int mcctty_write(struct tty_struct *tty, const unsigned char __user *buf,
			 int total)
{
	int i, count, ret = 0;
	unsigned char *tmp;
	MCC_MEM_SIZE num_of_received_bytes;
	struct mcc_tty_msg tty_msg;

	if(write_mode == MODE_IMAGE_LOAD)
        {
		return mcctty_put_char(tty, buf[0]);
	}

	if (NULL == buf) {
		pr_err("buf shouldn't be null.\n");
		return -ENOMEM;
	}
	count = total;
	tmp = (unsigned char *)buf;
	for (i = 0; i <= strlen(buf); i++) {
		tty_msg.data = i;
		/*
		 * wait until the remote endpoint is created by
		 * the other core
		 */
		ret = mcc_send(&mcc_endpoint_a9_pingpong,
				&mcc_endpoint_m4_pingpong, &tty_msg,
				sizeof(struct mcc_tty_msg),
				0xffffffff);
		while (MCC_ERR_ENDPOINT == ret) {
			pr_err("\n send err ret %d, re-send\n", ret);
			ret = mcc_send(&mcc_endpoint_a9_pingpong,
					&mcc_endpoint_m4_pingpong, &tty_msg,
					sizeof(struct mcc_tty_msg),
					0xffffffff);
			msleep(5000);
		}

		ret = mcc_recv(&mcc_endpoint_m4_pingpong,
				&mcc_endpoint_a9_pingpong, &tty_msg,
				sizeof(struct mcc_tty_msg),
				&num_of_received_bytes, 0xffffffff);
		if (MCC_SUCCESS != ret) {
			pr_err("A9 Main task receive error: %d\n", ret);
		} else {
			printk("mcc_tty: successfully received! data = %d\n", tty_msg.data);
		}
	}
	return total;
}

static long mcc_ioctl(struct file *f, unsigned cmd, unsigned long arg)
{
	void __user *buf = (void __user *)arg;
        void *src_gpr2, *ccm_ccowr;
	void *data_buffer;
	MCC_NODE this_node;
	MCC_ENDPOINT *endpoint;
	MCC_INFO_STRUCT *info;
	struct mcc_send_info_struct sis;
	struct mcc_send_nocopy_info_struct snis;
	struct mcc_recv_info_struct ris;
	struct mcc_recv_nocopy_info_struct rnis;
	struct mcc_get_buffer_struct gbs;
	struct mcc_queue_info_struct q_info;
	struct mqx_boot_info_struct mqx_boot_info;
	int retval;
	unsigned int recv_offset, buffer_offset;

	switch(cmd)
	{
		case MCC_SET_NODE:
			if (copy_from_user(&this_node, buf, sizeof(this_node)))
				return -EFAULT;

			printk(KERN_DEBUG "tty_libmcc: mcc_initialize: going to init node %d\n", this_node);
	                return mcc_initialize(this_node);

		case MCC_CREATE_ENDPOINT:
		case MCC_DESTROY_ENDPOINT:
			if (copy_from_user(&endpoint, buf, sizeof(endpoint)))
				return -EFAULT;
			printk(KERN_DEBUG "tty_libmcc: creating/destroying endpoint %d,%d,%d\n", endpoint->core, endpoint->node, endpoint->port);
			if(cmd == MCC_CREATE_ENDPOINT)
				return mcc_create_endpoint(endpoint, endpoint->port);
			else
				return mcc_destroy_endpoint(endpoint);
		case MCC_DESTROY:
			if (copy_from_user(&this_node, buf, sizeof(this_node)))
				return -EFAULT;

			return mcc_destroy(this_node);
		case MCC_SET_MODE_LOAD_MQX_IMAGE:
			if (copy_from_user(&mqx_boot_info, buf, sizeof(mqx_boot_info)))
				return -EFAULT;

			phys_start_addr = mqx_boot_info.phys_start_addr;
			phys_load_addr = mqx_boot_info.phys_load_addr;
			printk(KERN_DEBUG "phys_start_addr=%08x, phys_load_addr=%08x\n", phys_start_addr, phys_load_addr);

			if (request_mem_region(phys_load_addr, MAX_LOAD_SIZE, "mqx_boot"))
			{
				virt_load_addr = ioremap_nocache(phys_load_addr, MAX_LOAD_SIZE);
				if (!virt_load_addr)
				{
					printk(KERN_ERR "unable to map region %08x\n", phys_load_addr);
					release_mem_region(phys_load_addr, MAX_LOAD_SIZE);
					return -ENOMEM;
				}
			}
			else
			{
				printk(KERN_ERR "unable to reserve image load memory region\n");
				return -ENOMEM;
			}

			write_mode = MODE_IMAGE_LOAD;
			return MCC_SUCCESS;

		case  MCC_BOOT_MQX_IMAGE:
			write_mode = MODE_MCC;
			src_gpr2 = ioremap(0x4006E028, 4);
			ccm_ccowr = ioremap(0x4006B08C, 4);

			printk(KERN_DEBUG "Booting mqx image, size of %d bytes\n", offset);

			if( !src_gpr2 || !ccm_ccowr )
			{
				printk(KERN_ERR "unable to map src_gpr2 or ccm_ccowr to start m4 clock, aborting.\n");
				return -ENOMEM;
			}

			//0x4006E028 - GPR2 Register write Image Entry Point as per the Memory Map File of the Binary
			writel(phys_start_addr, src_gpr2);

			// 0x4006B08C - CCM_CCOWR Register - Set bit 16 - AUX_CORE_WKUP to enable M4 clock.
			writel(0x15a5a, ccm_ccowr);

			iounmap(src_gpr2);
			iounmap(ccm_ccowr);

			return MCC_SUCCESS;

		case MCC_GET_INFO:
			if(copy_from_user(&info, buf, sizeof(MCC_INFO_STRUCT *)))
				return -EINVAL;
			retval = mcc_get_info(0, info);

			printk(KERN_DEBUG "version info: %s\n", info->version_string);
			return retval;

		case MCC_SEND:
			if (copy_from_user(&sis, buf, sizeof(struct mcc_send_info_struct)))
				return -EINVAL;

			return mcc_send(sis.src_endpoint, sis.dest_endpoint, sis.msg, sis.msg_size, sis.timeout_ms);

		case MCC_SEND_NOCOPY:
			if (copy_from_user(&snis, buf, sizeof(struct mcc_send_nocopy_info_struct)))
				return -EINVAL;

			return mcc_send_nocopy(snis.src_endpoint, snis.dest_endpoint, (void *)(snis.offset + (unsigned int)bookeeping_data), snis.msg_size);

		case MCC_RECV:
			if(copy_from_user(&ris, buf, sizeof(struct mcc_recv_info_struct)))
				return -EINVAL;

			return mcc_recv(ris.src_endpoint, ris.dest_endpoint, ris.buffer, ris.buffer_size, ris.recv_size, ris.timeout_ms);

		case MCC_RECV_NOCOPY:
			if(copy_from_user(&rnis, buf, sizeof(struct mcc_recv_nocopy_info_struct)))
				return -EINVAL;

			printk(KERN_DEBUG "tty_libmcc: before mcc_recv_nocopy: buffer addr %p, the data is n/a", data_buffer);
			retval = mcc_recv_nocopy(rnis.src_endpoint, rnis.dest_endpoint, &data_buffer, rnis.recv_size, rnis.timeout_ms);
			printk(KERN_DEBUG "tty_libmcc: bookeeping_data=%p, buffer addr %p, offset=%08x, the data is %08x", bookeeping_data, data_buffer, data_buffer-(void *)bookeeping_data, *(int *) data_buffer);
			if(retval != MCC_SUCCESS)
				return retval;

			recv_offset = data_buffer-(void *)bookeeping_data;
			printk(KERN_DEBUG "tty_libmcc: recv_offset = %08x\n", recv_offset);
			rnis.offset = recv_offset;
			printk(KERN_DEBUG "tty_libmcc: rnis.offset = %08x\n", rnis.offset);

			if (copy_to_user(buf, &rnis, sizeof(rnis)))
				return -EINVAL;

			return MCC_SUCCESS;

		case MCC_GET_RECEIVE_BUFFER:
			if(copy_from_user(&gbs, buf, sizeof(gbs)))
				return -EINVAL;
			retval = mcc_get_buffer(&data_buffer, gbs.buf_size, gbs.timeout_ms);

			if(retval != MCC_SUCCESS)
				return retval;
			offset = data_buffer-(void *)bookeeping_data;
			gbs.offset = offset;

			if (copy_to_user(buf, &gbs, sizeof(gbs)))
				return -EINVAL;

			return MCC_SUCCESS;

		case MCC_FREE_RECEIVE_BUFFER:
			if(copy_from_user(&buffer_offset, buf, sizeof(unsigned int)))
				return -EINVAL;

			printk(KERN_DEBUG "tty_libmcc: offset=%08x, buffer to free is %p\n\n", buffer_offset, (void *)((unsigned int) bookeeping_data + buffer_offset));
			return mcc_free_buffer((void *)((unsigned int) bookeeping_data + buffer_offset));

		case MCC_GET_QUEUE_INFO:
			if (copy_from_user(&q_info, buf, sizeof(q_info)))
				return -EFAULT;
			retval = mcc_msgs_available(&q_info.endpoint, &q_info.current_queue_length);
			if(retval != MCC_SUCCESS)
				return retval;
			if (copy_to_user(buf, &q_info, sizeof(q_info)))
				return -EINVAL;

			return MCC_SUCCESS;
	}
	return -1;
}

static int mcc_mmap(struct file *filp, struct vm_area_struct *vma)
{
        int ret;

        vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

        ret = remap_pfn_range(vma,
                vma->vm_start,
                (unsigned int) MCC_MEM_VIRT_TO_PHYS(bookeeping_data) >> PAGE_SHIFT,
                sizeof(MCC_BOOKEEPING_STRUCT),
                vma->vm_page_prot);
        if (ret != 0)
                return -EAGAIN;

        return 0;
}

static const struct file_operations imxmcctty_fops = {
	.owner			= THIS_MODULE,
	.write			= mcctty_write,
	.unlocked_ioctl		= mcc_ioctl,
	.mmap			= mcc_mmap,
};

static struct tty_driver *mcctty_driver;

static int imx_mcc_tty_probe(struct platform_device *pdev)
{
        if (alloc_chrdev_region(&first, 0, 1, "mcc") < 0)
        {
                return -EIO;
        }

        if ((cl = class_create(THIS_MODULE, "mcc")) == NULL)
        {
                unregister_chrdev_region(first, 1);
                return -EIO;
        }

        if (device_create(cl, NULL, first, NULL, "mcc") == NULL)
        {
                class_destroy(cl);
                unregister_chrdev_region(first, 1);
                return -EIO;
        }

        cdev_init(&c_dev, &imxmcctty_fops);
        if (cdev_add(&c_dev, first, 1) == -1)
        {
                device_destroy(cl, first);
                class_destroy(cl);
                unregister_chrdev_region(first, 1);
                return -EIO;
        }

	return 0;
}

static int imx_mcc_tty_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct mcctty_port *cport = &mcc_tty_port;

	/* destroy the mcc tty endpoint here */
	ret = mcc_destroy_endpoint(&mcc_endpoint_a9_pingpong);
	if (ret)
		pr_err("failed to destroy a9 mcc ep.\n");
	else
		pr_info("destroy a9 mcc ep.\n");

	tty_unregister_driver(mcctty_driver);
	tty_port_destroy(&cport->port);
	put_tty_driver(mcctty_driver);

	return ret;
}

static const struct of_device_id vf610_mcc_libmcc_ids[] = {
	{ .compatible = "fsl,vf610-mcc-libmcc", },
	{ /* sentinel */ }
};

static struct platform_driver imxmcctty_driver = {
	.driver = {
		.name = "vf610-mcc-libmcc",
		.owner  = THIS_MODULE,
		.of_match_table = vf610_mcc_libmcc_ids,
		},
	.probe = imx_mcc_tty_probe,
	.remove = imx_mcc_tty_remove,
};

/*!
 * Initialise the imxmcctty_driver.
 *
 * @return  The function always returns 0.
 */

static int __init imxmcctty_init(void)
{
	if (platform_driver_register(&imxmcctty_driver) != 0)
		return -ENODEV;

	printk(KERN_INFO "IMX MCC libmcc TTY driver module loaded\n");
	return 0;
}

static void __exit imxmcctty_exit(void)
{
	/* Unregister the device structure */
	platform_driver_unregister(&imxmcctty_driver);
}

module_init(imxmcctty_init);
module_exit(imxmcctty_exit);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MCC TTY driver");
MODULE_LICENSE("GPL");
