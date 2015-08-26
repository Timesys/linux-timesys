/*
 * This is the Linux MCC configuration file
 *
 * Copyright (C) 2014 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 *
 * SPDX-License-Identifier: GPL-2.0+ and/or BSD-3-Clause
 * The GPL-2.0+ license for this file can be found in the COPYING.GPL file
 * included with this distribution or at
 * http://www.gnu.org/licenses/gpl-2.0.html
 * The BSD-3-Clause License for this file can be found in the COPYING.BSD file
 * included with this distribution or at
 * http://opensource.org/licenses/BSD-3-Clause
 */

#ifndef __MCC_LINUX_CONFIG_
#define __MCC_CLINUX_CONFIG_

/* size (in bytes) and number of receive buffers */
#define MCC_ATTR_NUM_RECEIVE_BUFFERS   (10)
#define MCC_ATTR_BUFFER_SIZE_IN_BYTES  (1024)

/* maximum number of receive endpoints (application specific setting),
 * do not assign it to a value greater than 255 ! */
#define MCC_ATTR_MAX_RECEIVE_ENDPOINTS (5)

/* size of the signal queue */
#define MCC_MAX_OUTSTANDING_SIGNALS    (10)

/* number of cores */
#define MCC_NUM_CORES                  (2)

/* core number */
#define MCC_CORE_NUMBER                (_psp_core_num())

/* node number */
#define MCC_NODE_NUMBER                (_psp_node_num())

/* MQX code size maximum size (in bytes) */
#define MAX_LOAD_SIZE (256*1024)

/* Linux MCC IOCTL definitions */ 
#define MCC_CREATE_ENDPOINT                                             _IOW('M', 1, MCC_ENDPOINT)
#define MCC_DESTROY_ENDPOINT                                            _IOW('M', 2, MCC_ENDPOINT)
#define MCC_SET_RECEIVE_ENDPOINT                                        _IOW('M', 3, MCC_ENDPOINT)
#define MCC_SET_SEND_ENDPOINT                                           _IOW('M', 4, MCC_ENDPOINT)
#define MCC_SET_TIMEOUT                                                 _IOW('M', 5, unsigned int)
#define MCC_GET_INFO                                                    _IOR('M', 6, MCC_INFO_STRUCT)
#define MCC_SET_READ_MODE                                               _IOW('M', 7, MCC_READ_MODE)
#define MCC_SET_MODE_LOAD_MQX_IMAGE                                     _IOW('M', 8, struct mqx_boot_info_struct)
#define MCC_BOOT_MQX_IMAGE                                              _IO('M', 9)
#define MCC_FREE_RECEIVE_BUFFER                                         _IOW('M', 10, unsigned int)
#define MCC_GET_QUEUE_INFO                                              _IOR('M', 11, struct mcc_queue_info_struct)
#define MCC_GET_NODE                                                    _IOR('M', 12, MCC_NODE)
#define MCC_SET_NODE                                                    _IOW('M', 13, MCC_NODE)
#define MCC_CHECK_ENDPOINT_EXISTS                                       _IOR('M', 14, MCC_ENDPOINT)
#define MCC_SEND							_IOW('M', 15, struct mcc_send_info_struct)
#define MCC_SEND_NOCOPY							_IOW('M', 16, struct mcc_send_nocopy_info_struct)
#define MCC_RECV							_IOW('M', 17, struct mcc_recv_info_struct)
#define MCC_RECV_NOCOPY							_IOW('M', 18, struct mcc_recv_nocopy_info_struct)
#define MCC_GET_RECEIVE_BUFFER						_IOW('M', 19, struct mcc_get_buffer_struct)
#define MCC_DESTROY							_IOW('M', 20, MCC_NODE)
/* semaphore number */
#define MCC_SHMEM_SEMAPHORE_NUMBER      (1)
#define MCC_PRINTF_SEMAPHORE_NUMBER     (2)
#define MCC_I2C_SEMAPHORE_NUMBER        (3)
#define MCC_RESERVED1_SEMAPHORE_NUMBER  (4)
#define MCC_RESERVED2_SEMAPHORE_NUMBER  (5)
#define MCC_POWER_SHMEM_NUMBER          (6)

/* data non-copy mechanisms enabled  */
#define MCC_SEND_RECV_NOCOPY_API_ENABLED (1)

#endif /* __MCC_LINUX_CONFIG_*/

