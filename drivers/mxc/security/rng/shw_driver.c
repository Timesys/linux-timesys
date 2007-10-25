/*
 * Copyright 2005-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*! @file shw_driver.c
 *
 * This is the user-mode driver code for the FSL Security Hardware (SHW) API.
 * as well as the 'common' FSL SHW API code for kernel API users.
 *
 * Its interaction with the Linux kernel is from calls to shw_init() when the
 * driver is loaded, and shw_shutdown() should the driver be unloaded.
 *
 * The User API (driver interface) is handled by the following functions:
 * @li shw_open()    - handles open() system call on FSL SHW device
 * @li shw_release() - handles close() system call on FSL SHW device
 * @li shw_ioctl()    - handles ioctl() system call on FSL SHW device
 *
 * The driver also provides the following functions for kernel users of the FSL
 * SHW API:
 * @li fsl_shw_register_user()
 * @li fsl_shw_deregister_user()
 * @li fsl_shw_get_capabilities()
 * @li fsl_shw_get_results()
 *
 * All other functions are internal to the driver.
 *
 * The life of the driver starts at boot (or module load) time, with a call by
 * the kernel to shw_init().
 *
 * The life of the driver ends when the kernel is shutting down (or the driver
 * is being unloaded).  At this time, shw_shutdown() is called.  No function
 * will ever be called after that point.
 *
 * In the case that the driver is reloaded, a new copy of the driver, with
 * fresh global values, etc., is loaded, and there will be a new call to
 * shw_init().
 *
 * In user mode, the user's fsl_shw_register_user() call causes an open() event
 * on the driver followed by a ioctl() with the registration information.  Any
 * subsequent API calls by the user are handled through the ioctl() function
 * and shuffled off to the appropriate routine (or driver) for service.  The
 * fsl_shw_deregister_user() call by the user results in a close() function
 * call on the driver.
 *
 * In kernel mode, the driver provides the functions fsl_shw_register_user(),
 * fsl_shw_deregister_user(), fsl_shw_get_capabilities(), and
 * fsl_shw_get_results().  Other parts of the API are provided by other
 * drivers, if available, to support the cryptographic functions.
 *  @ingroup RNG
 */

#include "portable_os.h"
#include "fsl_shw.h"

#include "shw_internals.h"

/*!****************************************************************************
 *
 *  Function Declarations
 *
 *****************************************************************************/

/* kernel interface functions */
OS_DEV_INIT_DCL(shw_init);
OS_DEV_SHUTDOWN_DCL(shw_shutdown);
OS_DEV_IOCTL_DCL(shw_ioctl);

/*!****************************************************************************
 *
 *  Global / Static Variables
 *
 *****************************************************************************/

/*!
 *  Major node (user/device interaction value) of this driver.
 */
static int shw_major_node = SHW_MAJOR_NODE;

/*!
 *  Flag to know whether the driver has been associated with its user device
 *  node (e.g. /dev/shw).
 */
static int shw_device_registered = 0;

/*!
 * OS-dependent handle used for registering user interface of a driver.
 */
static os_driver_reg_t reg_handle;

/*!
 * Linked List of registered users of the API
 */

/*!
 * This is the lock for all user request pools.  H/W component drivers may also
 * use it for their own work queues.
 */
os_lock_t shw_queue_lock = NULL;

#ifndef FSL_HAVE_SAHARA
/*! Empty list of supported symmetric algorithms. */
static fsl_shw_key_alg_t pf_syms[] = {
};

/*! Empty list of supported symmetric modes. */
static fsl_shw_sym_mode_t pf_modes[] = {
};

/*! Empty list of supported hash algorithms. */
static fsl_shw_hash_alg_t pf_hashes[] = {
};
#endif				/* no Sahara */

/*! This matches SHW capabilities... */
static fsl_shw_pco_t cap = {
	1, 1,			/* api version number - major & minor */
	1, 0,			/* driver version number - major & minor */
	sizeof(pf_syms) / sizeof(fsl_shw_key_alg_t),	/* key alg count */
	pf_syms,		/* key alg list ptr */
	sizeof(pf_modes) / sizeof(fsl_shw_sym_mode_t),	/* sym mode count */
	pf_modes,		/* modes list ptr */
	sizeof(pf_hashes) / sizeof(fsl_shw_hash_alg_t),	/* hash alg count */
	pf_hashes,		/* hash list ptr */
	/*
	 * The following table must be set to handle all values of key algorithm
	 * and sym mode, and be in the correct order..
	 */
	{			/* Stream, ECB, CBC, CTR */
	 {0, 0, 0, 0}
	 ,			/* HMAC */
	 {0, 0, 0, 0}
	 ,			/* AES  */
	 {0, 0, 0, 0}
	 ,			/* DES */
	 {0, 0, 0, 0}
	 ,			/* 3DES */
	 {0, 0, 0, 0}		/* ARC4 */
	 }
	,
};

/* These are often handy */
#ifndef FALSE
/*! Not true.  Guaranteed to be zero. */
#define FALSE 0
#endif
#ifndef TRUE
/*! True.  Guaranteed to be non-zero. */
#define TRUE 1
#endif

/*!****************************************************************************
 *
 *  Function Implementations - Externally Accessible
 *
 *****************************************************************************/

/*!***************************************************************************/
/* fn shw_init()                                                            */
/*!***************************************************************************/
/*!
 * Initialize the driver.
 *
 * This routine is called during kernel init or module load (insmod).
 *
 * @return OS_ERROR_OK_S on success, errno on failure
 */
OS_DEV_INIT(shw_init)
{
	os_error_code error_code = OS_ERROR_NO_MEMORY_S;	/* assume failure */

	pr_debug("SHW Driver: Loading\n");

	user_list = NULL;
	shw_queue_lock = os_lock_alloc_init();

	if (shw_queue_lock != NULL) {
		error_code = shw_setup_user_driver_interaction();
		if (error_code != OS_ERROR_OK_S) {
			pr_debug("SHW Driver: Failed to setup user"
				 " i/f: %d\n", error_code);
		}
	}
	/* queue_lock not NULL */
	if (error_code != OS_ERROR_OK_S) {
		pr_debug("SHW: Driver initialization failed. %d\n", error_code);
		shw_cleanup();
	} else {
		pr_debug("SHW: Driver initialization complete.\n");
	}

	os_dev_init_return(error_code);
}				/* shw_init */

/*!***************************************************************************/
/* fn shw_shutdown()                                                         */
/*!***************************************************************************/
/*!
 * Prepare driver for exit.
 *
 * This is called during @c rmmod when the driver is unloading or when the
 * kernel is shutting down.
 *
 * Calls shw_cleanup() to do all work to undo anything that happened during
 * initialization or while driver was running.
 */
OS_DEV_SHUTDOWN(shw_shutdown)
{

	pr_debug("SHW: shutdown called\n");
	shw_cleanup();

	os_dev_shutdown_return(OS_ERROR_OK_S);
}				/* shw_shutdown */

/*!***************************************************************************/
/* fn shw_cleanup()                                                          */
/*!***************************************************************************/
/*!
 * Prepare driver for shutdown.
 *
 * Remove the driver registration.
 *
 */
static void shw_cleanup(void)
{
	if (shw_device_registered) {

		/* Turn off the all association with OS */
		os_driver_remove_registration(reg_handle);
		shw_device_registered = 0;
	}

	if (shw_queue_lock != NULL) {
		os_lock_deallocate(shw_queue_lock);
	}
	pr_debug("SHW Driver: Cleaned up\n");
}				/* shw_cleanup */

/*!***************************************************************************/
/* fn shw_open()                                                             */
/*!***************************************************************************/
/*!
 * Handle @c open() call from user.
 *
 * @return OS_ERROR_OK_S on success (always!)
 */
OS_DEV_OPEN(shw_open)
{
	os_error_code status = OS_ERROR_OK_S;

	os_dev_set_user_private(NULL);	/* Make sure */

	os_dev_open_return(status);
}				/* shw_open */

/*!***************************************************************************/
/* fn shw_ioctl()                                                            */
/*!***************************************************************************/
/*!
 * Process an ioctl() request from user-mode API.
 *
 * This code determines which of the API requests the user has made and then
 * sends the request off to the appropriate function.
 *
 * @return ioctl_return()
 */
OS_DEV_IOCTL(shw_ioctl)
{
	os_error_code code = OS_ERROR_FAIL_S;

	fsl_shw_uco_t *user_ctx = os_dev_get_user_private();

	pr_debug("SHW: IOCTL %d received\n", os_dev_get_ioctl_op());
	switch (os_dev_get_ioctl_op()) {

	case SHW_IOCTL_REQUEST + SHW_USER_REQ_REGISTER_USER:
		{
			fsl_shw_uco_t *user_ctx =
			    os_alloc_memory(sizeof(*user_ctx), 0);

			if (user_ctx == NULL) {
				code = OS_ERROR_NO_MEMORY_S;
			} else {
				code = init_uco(user_ctx, (fsl_shw_uco_t *)
						os_dev_get_ioctl_arg());
				if (code == OS_ERROR_OK_S) {
					os_dev_set_user_private(user_ctx);
				} else {
					os_free_memory(user_ctx);
				}
			}
		}
		break;

	case SHW_IOCTL_REQUEST + SHW_USER_REQ_DEREGISTER_USER:
		break;

	case SHW_IOCTL_REQUEST + SHW_USER_REQ_GET_RESULTS:
		code = get_results(user_ctx, (struct results_req *)
				   os_dev_get_ioctl_arg());
		break;

	case SHW_IOCTL_REQUEST + SHW_USER_REQ_GET_CAPABILITIES:
		code = get_capabilities(user_ctx, (fsl_shw_pco_t *)
					os_dev_get_ioctl_arg());
		break;

	case SHW_IOCTL_REQUEST + SHW_USER_REQ_GET_RANDOM:
		pr_debug("SHW: get_random ioctl received\n");
		code = get_random(user_ctx, (struct get_random_req *)
				  os_dev_get_ioctl_arg());
		break;

	case SHW_IOCTL_REQUEST + SHW_USER_REQ_ADD_ENTROPY:
		pr_debug("SHW: add_entropy ioctl received\n");
		code = add_entropy(user_ctx, (struct add_entropy_req *)
				   os_dev_get_ioctl_arg());
		break;

	default:
		pr_debug("SHW: Unexpected ioctl %d\n", os_dev_get_ioctl_op());
		break;
	}

	os_dev_ioctl_return(code);
}

/*!***************************************************************************/
/* fn shw_release()                                                         */
/*!***************************************************************************/
/*!
 * Handle @c close() call from user.
 * This is a Linux device driver interface routine.
 *
 * @return OS_ERROR_OK_S on success (always!)
 */
OS_DEV_CLOSE(shw_release)
{
	fsl_shw_uco_t *user_ctx = os_dev_get_user_private();
	os_error_code code = OS_ERROR_OK_S;

	if (user_ctx != NULL) {

		fsl_shw_deregister_user(user_ctx);
		os_free_memory(user_ctx);
		os_dev_set_user_private(NULL);
	}

	os_dev_close_return(code);
}				/* shw_release */

/*!***************************************************************************/
/* fn shw_user_callback()                                                    */
/*!***************************************************************************/
/*!
 * FSL SHW User callback function.
 *
 * This function is set in the kernel version of the user context as the
 * callback function when the user mode user wants a callback.  Its job is to
 * inform the user process that results (may) be available.  It does this by
 * sending a SIGUSR2 signal which is then caught by the user-mode FSL SHW
 * library.
 *
 * @param uco          Kernel version of uco associated with the request.
 *
 * @return void
 */
static void shw_user_callback(fsl_shw_uco_t * uco)
{
	pr_debug("SHW: Signalling callback user process for context %p\n", uco);
	os_send_signal(uco->process, SIGUSR2);
}

/*!***************************************************************************/
/* fn setup_user_driver_interaction()                                        */
/*!***************************************************************************/
/*!
 * Register the driver with the kernel as the driver for shw_major_node.  Note
 * that this value may be zero, in which case the major number will be assigned
 * by the OS.  shw_major_node is never modified.
 *
 * The open(), ioctl(), and close() handles for the driver ned to be registered
 * with the kernel.  Upon success, shw_device_registered will be true;
 *
 * @return OS_ERROR_OK_S on success, or an os err code
 */
static os_error_code shw_setup_user_driver_interaction(void)
{
	os_error_code error_code;

	os_driver_init_registration(reg_handle);
	os_driver_add_registration(reg_handle, OS_FN_OPEN,
				   OS_DEV_OPEN_REF(shw_open));
	os_driver_add_registration(reg_handle, OS_FN_IOCTL,
				   OS_DEV_IOCTL_REF(shw_ioctl));
	os_driver_add_registration(reg_handle, OS_FN_CLOSE,
				   OS_DEV_CLOSE_REF(shw_release));
	error_code = os_driver_complete_registration(reg_handle, shw_major_node,
						     SHW_DRIVER_NAME);

	if (error_code != OS_ERROR_OK_S) {
		/* failure ! */
		pr_debug("SHW Driver: register device driver failed: %d\n",
			 error_code);
	} else {		/* success */
		shw_device_registered = TRUE;
		pr_debug("SHW Driver:  Major node is %d\n",
			 os_driver_get_major(reg_handle));
	}

	return error_code;
}				/* shw_setup_user_driver_interaction */

/*!****************************************************************/
/* User Mode Support                                              */
/*!****************************************************************/

/*!
 * Initialze kernel User Context Object from User-space version.
 *
 * Copy user UCO into kernel UCO, set flags and fields for operation
 * within kernel space.  Add user to driver's list of users.
 *
 * @param user_ctx        Pointer to kernel space UCO
 * @param user_mode_uco   User pointer to user space version
 *
 * @return os_error_code
 */
static os_error_code init_uco(fsl_shw_uco_t * user_ctx, void *user_mode_uco)
{
	os_error_code code;

	code = os_copy_from_user(user_ctx, user_mode_uco, sizeof(*user_ctx));
	if (code == OS_ERROR_OK_S) {
		user_ctx->flags |= FSL_UCO_USERMODE_USER;
		user_ctx->result_pool.head = NULL;
		user_ctx->result_pool.tail = NULL;
		user_ctx->process = os_get_process_handle();
		user_ctx->callback = shw_user_callback;
		SHW_ADD_USER(user_ctx);
	}
	pr_debug("SHW: init uco returning %d (flags %x)\n", code,
		 user_ctx->flags);

	return code;
}

/*!
 * Copy array from kernel to user space.
 *
 * This routine will check bounds before trying to copy, and return failure
 * on bounds violation or error during the copy.
 *
 * @param userloc   Location in userloc to place data.  If NULL, the function
 *                  will do nothing (except return NULL).
 * @param userend   Address beyond allowed copy region at @c userloc.
 * @param data_start Location of data to be copied
 * @param element_size  sizeof() an element
 * @param element_count Number of elements of size element_size to copy.
 * @return New value of userloc, or NULL if there was an error.
 */
inline static void *copy_array(void *userloc, void *userend, void *data_start,
			       unsigned element_size, unsigned element_count)
{
	unsigned byte_count = element_size * element_count;

	if ((userloc == NULL) || (userend == NULL)
	    || ((userloc + byte_count) >= userend) ||
	    (copy_to_user(userloc, data_start, byte_count) != OS_ERROR_OK_S)) {
		userloc = NULL;
	} else {
		userloc += byte_count;
	}

	return userloc;
}

/*!
 * Send an FSL SHW API return code up into the user-space request structure.
 *
 * @param user_header   User address of request block / request header
 * @param result_code   The FSL SHW API code to be placed at header.code
 *
 * @return an os_error_code
 *
 * NOTE: This function assumes that the shw_req_header is at the beginning of
 * each request structure.
 */
inline static os_error_code copy_fsl_code(void *user_header,
					  fsl_shw_return_t result_code)
{
	return os_copy_to_user(user_header +
			       offsetof(struct shw_req_header, code),
			       &result_code, sizeof(result_code));
}

/*!
 * Handle user-mode Get Capabilities request
 *
 * Right now, this function can only have a failure if the user has failed to
 * provide a pointer to a location in user space with enough room to hold the
 * fsl_shw_pco_t structure and any associated data.  It will treat this failure
 * as an ioctl failure and return an ioctl error code, instead of treating it
 * as an API failure.
 *
 * @param user_ctx    The kernel version of user's context
 * @param user_mode_pco_request  Pointer to user-space request
 *
 * @return an os_error_code
 */
static os_error_code get_capabilities(fsl_shw_uco_t * user_ctx,
				      void *user_mode_pco_request)
{
	os_error_code code;
	struct capabilities_req req;
	fsl_shw_pco_t local_cap;

	memcpy(&local_cap, &cap, sizeof(cap));
	/* Initialize pointers to out-of-struct arrays */
	local_cap.sym_algorithms = NULL;
	local_cap.sym_modes = NULL;
	local_cap.sym_modes = NULL;

	code = os_copy_from_user(&req, user_mode_pco_request, sizeof(req));
	if (code == OS_ERROR_OK_S) {
		void *endcap;
		void *user_bounds;
		pr_debug("SHE: Received get_cap request: 0x%p/%u/0x%x\n",
			 req.capabilities, req.size, sizeof(fsl_shw_pco_t));
		endcap = req.capabilities + 1;	/* point to end of structure */
		user_bounds = (void *)req.capabilities + req.size;	/* end of area */

		printk(KERN_INFO "next: %p, end: %p\n", endcap, user_bounds);	//
		/* First verify that request is big enough for the main structure */
		if (endcap >= user_bounds) {
			endcap = NULL;	/* No! */
		}

		/* Copy any Symmetric Algorithm suppport */
		if (cap.sym_algorithm_count != 0) {
			local_cap.sym_algorithms = endcap;
			endcap =
			    copy_array(endcap, user_bounds, cap.sym_algorithms,
				       sizeof(fsl_shw_key_alg_t),
				       cap.sym_algorithm_count);
		}

		/* Copy any Symmetric Modes suppport */
		if (cap.sym_mode_count != 0) {
			local_cap.sym_modes = endcap;
			endcap = copy_array(endcap, user_bounds, cap.sym_modes,
					    sizeof(fsl_shw_sym_mode_t),
					    cap.sym_mode_count);
		}

		/* Copy any Hash Algorithm suppport */
		if (cap.hash_algorithm_count != 0) {
			local_cap.hash_algorithms = endcap;
			endcap =
			    copy_array(endcap, user_bounds, cap.hash_algorithms,
				       sizeof(fsl_shw_hash_alg_t),
				       cap.hash_algorithm_count);
		}

		/* Now copy up the (possibly modified) main structure */
		if (endcap != NULL) {
			code =
			    os_copy_to_user(req.capabilities, &local_cap,
					    sizeof(cap));
		}

		if (endcap == NULL) {
			code = OS_ERROR_BAD_ADDRESS;
		}

		/* And return the FSL SHW code in the request structure. */
		if (code == OS_ERROR_OK_S) {
			code =
			    copy_fsl_code(user_mode_pco_request,
					  FSL_RETURN_OK_S);
		}
	}

	/* code may already be set to an error.  This is another error case.  */

	pr_debug("SHW: get capabilities returning %d\n", code);

	return code;
}

/*!
 * Handle user-mode Get Results request
 *
 * Get arguments from user space into kernel space, then call
 * fsl_shw_get_results, and then copy its return code and any results from
 * kernel space back to user space.
 *
 * @param user_ctx    The kernel version of user's context
 * @param user_mode_results_req  Pointer to user-space request
 *
 * @return an os_error_code
 */
static os_error_code get_results(fsl_shw_uco_t * user_ctx,
				 void *user_mode_results_req)
{
	os_error_code code;
	struct results_req req;
	fsl_shw_result_t *results = NULL;
	int loop;

	code = os_copy_from_user(&req, user_mode_results_req, sizeof(req));
	loop = 0;

	if (code == OS_ERROR_OK_S) {
		results = os_alloc_memory(req.requested * sizeof(*results), 0);
		if (results == NULL) {
			code = OS_ERROR_NO_MEMORY_S;
		}
	}

	if (code == OS_ERROR_OK_S) {
		fsl_shw_return_t err =
		    fsl_shw_get_results(user_ctx, req.requested,
					results, &req.actual);

		/* Send API return code up to user. */
		code = copy_fsl_code(user_mode_results_req, err);

		if ((code == OS_ERROR_OK_S) && (err == FSL_RETURN_OK_S)) {
			/* Now copy up the result count */
			code = os_copy_to_user(user_mode_results_req
					       + offsetof(struct results_req,
							  actual), &req.actual,
					       sizeof(req.actual));
			if ((code == OS_ERROR_OK_S) && (req.actual != 0)) {
				/* now copy up the results... */
				code = os_copy_to_user(req.results, results,
						       req.actual *
						       sizeof(*results));
			}
		}
	}

	if (results != NULL) {
		os_free_memory(results);
	}

	return code;
}

/*!
 * Process header of user-mode request.
 *
 * Mark header as User Mode request.  Update UCO's flags and reference fields
 * with current versions from the header.
 *
 * @param user_ctx  Pointer to kernel version of UCO.
 * @param hdr       Pointer to common part of user request.
 *
 * @return void
 */
inline static void process_hdr(fsl_shw_uco_t * user_ctx,
			       struct shw_req_header *hdr)
{
	hdr->flags |= FSL_UCO_USERMODE_USER;
	user_ctx->flags = hdr->flags;
	user_ctx->user_ref = hdr->user_ref;

	return;
}

/*!
 * Handle user-mode Get Random request
 *
 * @param user_ctx    The kernel version of user's context
 * @param user_mode_get_random_req  Pointer to user-space request
 *
 * @return an os_error_code
 */
static os_error_code get_random(fsl_shw_uco_t * user_ctx,
				void *user_mode_get_random_req)
{
	os_error_code code;
	struct get_random_req req;

	code = os_copy_from_user(&req, user_mode_get_random_req, sizeof(req));
	if (code == OS_ERROR_OK_S) {
		process_hdr(user_ctx, &req.hdr);
		pr_debug("SHW: get_random() for %d bytes in %sblocking mode\n",
			 req.size, (req.hdr.flags & FSL_UCO_BLOCKING_MODE) ?
			 "" : "non-");
		req.hdr.code =
		    fsl_shw_get_random(user_ctx, req.size, req.random);

		pr_debug("SHW: get_random() returning %d\n", req.hdr.code);

		/* Copy FSL function status back to user */
		code = copy_fsl_code(user_mode_get_random_req, req.hdr.code);
	}

	return code;
}

/*!
 * Handle user-mode Add Entropy request
 *
 * @param user_ctx    Pointer to the kernel version of user's context
 * @param user_mode_add_entropy_req  Address of user-space request
 *
 * @return an os_error_code
 */
static os_error_code add_entropy(fsl_shw_uco_t * user_ctx,
				 void *user_mode_add_entropy_req)
{
	os_error_code code;
	struct add_entropy_req req;
	uint8_t *local_buffer = NULL;

	code = os_copy_from_user(&req, user_mode_add_entropy_req, sizeof(req));
	if (code == OS_ERROR_OK_S) {
		local_buffer = os_alloc_memory(req.size, 0);	/* for random */
		if (local_buffer != NULL) {
			code =
			    os_copy_from_user(local_buffer, req.entropy,
					      req.size);
		}
		if (code == OS_ERROR_OK_S) {
			req.hdr.code = fsl_shw_add_entropy(user_ctx, req.size,
							   local_buffer);

			code =
			    copy_fsl_code(user_mode_add_entropy_req,
					  req.hdr.code);
		}
	}

	if (local_buffer != NULL) {
		os_free_memory(local_buffer);
	}

	return code;
}

/*!****************************************************************/
/* End User Mode Support                                          */
/*!****************************************************************/

#ifdef LINUX_VERSION_CODE
EXPORT_SYMBOL(fsl_shw_register_user);
#endif
/* REQ-S2LRD-PINTFC-API-GEN-004 */
/*
 * Handle user registration.
 *
 * @param  user_ctx   The user context for the registration.
 */
fsl_shw_return_t fsl_shw_register_user(fsl_shw_uco_t * user_ctx)
{
	fsl_shw_return_t code = FSL_RETURN_INTERNAL_ERROR_S;

	if ((user_ctx->flags & FSL_UCO_BLOCKING_MODE) &&
	    (user_ctx->flags & FSL_UCO_CALLBACK_MODE)) {
		code = FSL_RETURN_BAD_FLAG_S;
		goto error_exit;
	} else if (user_ctx->pool_size == 0) {
		code = FSL_RETURN_NO_RESOURCE_S;
		goto error_exit;
	} else {
		user_ctx->result_pool.head = NULL;
		user_ctx->result_pool.tail = NULL;
		SHW_ADD_USER(user_ctx);
		code = FSL_RETURN_OK_S;
	}

      error_exit:
	return code;
}

#ifdef LINUX_VERSION_CODE
EXPORT_SYMBOL(fsl_shw_deregister_user);
#endif
/* REQ-S2LRD-PINTFC-API-GEN-005 */
/*!
 * Destroy the association between the the user and the provider of the API.
 *
 * @param  user_ctx   The user context which is no longer needed.
 *
 * @return    A return code of type #fsl_shw_return_t.
 */
fsl_shw_return_t fsl_shw_deregister_user(fsl_shw_uco_t * user_ctx)
{
	shw_queue_entry_t *finished_request;

	/* Clean up what we find in result pool. */
	do {
		os_lock_context_t lock_context;
		os_lock_save_context(shw_queue_lock, lock_context);
		finished_request = user_ctx->result_pool.head;

		if (finished_request != NULL) {
			SHW_QUEUE_REMOVE_ENTRY(&user_ctx->result_pool,
					       finished_request);
			os_unlock_restore_context(shw_queue_lock, lock_context);
			os_free_memory(finished_request);
		} else {
			os_unlock_restore_context(shw_queue_lock, lock_context);
		}
	} while (finished_request != NULL);

	SHW_REMOVE_USER(user_ctx);

	return FSL_RETURN_OK_S;
}

#ifdef LINUX_VERSION_CODE
EXPORT_SYMBOL(fsl_shw_get_results);
#endif
/* REQ-S2LRD-PINTFC-API-GEN-006 */
fsl_shw_return_t fsl_shw_get_results(fsl_shw_uco_t * user_ctx,
				     unsigned result_size,
				     fsl_shw_result_t results[],
				     unsigned *result_count)
{
	shw_queue_entry_t *finished_request;
	unsigned loop = 0;

	do {
		os_lock_context_t lock_context;

		/* Protect state of user's result pool until we have retrieved and
		 * remove the first entry, or determined that the pool is empty. */
		os_lock_save_context(shw_queue_lock, lock_context);
		finished_request = user_ctx->result_pool.head;

		if (finished_request != NULL) {
			uint32_t code = 0;

			SHW_QUEUE_REMOVE_ENTRY(&user_ctx->result_pool,
					       finished_request);
			os_unlock_restore_context(shw_queue_lock, lock_context);

			results[loop].user_ref = finished_request->user_ref;
			results[loop].code = finished_request->code;
			results[loop].detail1 = 0;
			results[loop].detail2 = 0;
			results[loop].user_req =
			    finished_request->user_mode_req;
			if (finished_request->postprocess != NULL) {
				code =
				    finished_request->
				    postprocess(finished_request);
			}

			results[loop].code = finished_request->code;
			os_free_memory(finished_request);
			if (code == 0) {
				loop++;
			}
		} else {	/* finished_request is NULL */
			/* pool is empty */
			os_unlock_restore_context(shw_queue_lock, lock_context);
		}

	} while ((loop < result_size) && (finished_request != NULL));

	*result_count = loop;

	return FSL_RETURN_OK_S;
}

#ifdef LINUX_VERSION_CODE
EXPORT_SYMBOL(fsl_shw_get_capabilities);
#endif
fsl_shw_pco_t *fsl_shw_get_capabilities(fsl_shw_uco_t * user_ctx)
{

	/* Unused */
	(void)user_ctx;

	return &cap;
}

#if !(defined(FSL_HAVE_SAHARA) || defined(FSL_HAVE_RNGA)                    \
      || defined(FSL_HAVE_RNGC))

fsl_shw_return_t fsl_shw_get_random(fsl_shw_uco_t * user_ctx,
				    uint32_t length, uint8_t * data)
{

	/* Unused */
	(void)user_ctx;
	(void)length;
	(void)data;

	return FSL_RETURN_ERROR_S;
}

fsl_shw_return_t fsl_shw_add_entropy(fsl_shw_uco_t * user_ctx,
				     uint32_t length, uint8_t * data)
{

	/* Unused */
	(void)user_ctx;
	(void)length;
	(void)data;

	return FSL_RETURN_ERROR_S;
}

EXPORT_SYMBOL(fsl_shw_add_entropy);
EXPORT_SYMBOL(fsl_shw_get_random);

#endif

#ifndef FSL_HAVE_SAHARA
#ifdef LINUX_VERSION_CODE
EXPORT_SYMBOL(fsl_shw_symmetric_decrypt);
#endif
fsl_shw_return_t fsl_shw_symmetric_decrypt(fsl_shw_uco_t * user_ctx,
					   fsl_shw_sko_t * key_info,
					   fsl_shw_scco_t * sym_ctx,
					   uint32_t length,
					   const uint8_t * ct, uint8_t * pt)
{

	/* Unused */
	(void)user_ctx;
	(void)key_info;
	(void)sym_ctx;
	(void)length;
	(void)ct;
	(void)pt;

	return FSL_RETURN_ERROR_S;
}

#ifdef LINUX_VERSION_CODE
EXPORT_SYMBOL(fsl_shw_symmetric_encrypt);
#endif
fsl_shw_return_t fsl_shw_symmetric_encrypt(fsl_shw_uco_t * user_ctx,
					   fsl_shw_sko_t * key_info,
					   fsl_shw_scco_t * sym_ctx,
					   uint32_t length,
					   const uint8_t * pt, uint8_t * ct)
{

	/* Unused */
	(void)user_ctx;
	(void)key_info;
	(void)sym_ctx;
	(void)length;
	(void)pt;
	(void)ct;

	return FSL_RETURN_ERROR_S;
}

#ifdef LINUX_VERSION_CODE
EXPORT_SYMBOL(fsl_shw_establish_key);
#endif
fsl_shw_return_t fsl_shw_establish_key(fsl_shw_uco_t * user_ctx,
				       fsl_shw_sko_t * key_info,
				       fsl_shw_key_wrap_t establish_type,
				       const uint8_t * key)
{

	/* Unused */
	(void)user_ctx;
	(void)key_info;
	(void)establish_type;
	(void)key;

	return FSL_RETURN_ERROR_S;
}

#ifdef LINUX_VERSION_CODE
EXPORT_SYMBOL(fsl_shw_extract_key);
#endif
fsl_shw_return_t fsl_shw_extract_key(fsl_shw_uco_t * user_ctx,
				     fsl_shw_sko_t * key_info,
				     uint8_t * covered_key)
{

	/* Unused */
	(void)user_ctx;
	(void)key_info;
	(void)covered_key;

	return FSL_RETURN_ERROR_S;
}

#ifdef LINUX_VERSION_CODE
EXPORT_SYMBOL(fsl_shw_release_key);
#endif
fsl_shw_return_t fsl_shw_release_key(fsl_shw_uco_t * user_ctx,
				     fsl_shw_sko_t * key_info)
{

	/* Unused */
	(void)user_ctx;
	(void)key_info;

	return FSL_RETURN_ERROR_S;
}
#endif

#ifdef LINUX_VERSION_CODE
EXPORT_SYMBOL(fsl_shw_hash);
#endif
#if !defined(FSL_HAVE_SAHARA)
fsl_shw_return_t fsl_shw_hash(fsl_shw_uco_t * user_ctx,
			      fsl_shw_hco_t * hash_ctx,
			      const uint8_t * msg,
			      uint32_t length,
			      uint8_t * result, uint32_t result_len)
{
	fsl_shw_return_t ret = FSL_RETURN_ERROR_S;

	/* Unused */
	(void)user_ctx;
	(void)hash_ctx;
	(void)msg;
	(void)length;
	(void)result;
	(void)result_len;

	return ret;
}
#endif

#ifndef FSL_HAVE_SAHARA
#ifdef LINUX_VERSION_CODE
EXPORT_SYMBOL(fsl_shw_hmac_precompute);
#endif

fsl_shw_return_t fsl_shw_hmac_precompute(fsl_shw_uco_t * user_ctx,
					 fsl_shw_sko_t * key_info,
					 fsl_shw_hmco_t * hmac_ctx)
{
	fsl_shw_return_t status = FSL_RETURN_ERROR_S;

	/* Unused */
	(void)user_ctx;
	(void)key_info;
	(void)hmac_ctx;

	return status;
}

#ifdef LINUX_VERSION_CODE
EXPORT_SYMBOL(fsl_shw_hmac);
#endif

fsl_shw_return_t fsl_shw_hmac(fsl_shw_uco_t * user_ctx,
			      fsl_shw_sko_t * key_info,
			      fsl_shw_hmco_t * hmac_ctx,
			      const uint8_t * msg,
			      uint32_t length,
			      uint8_t * result, uint32_t result_len)
{
	fsl_shw_return_t status = FSL_RETURN_ERROR_S;

	/* Unused */
	(void)user_ctx;
	(void)key_info;
	(void)hmac_ctx;
	(void)msg;
	(void)length;
	(void)result;
	(void)result_len;

	return status;
}
#endif

#ifndef FSL_HAVE_SAHARA
#ifdef LINUX_VERSION_CODE
EXPORT_SYMBOL(fsl_shw_gen_encrypt);
#endif

fsl_shw_return_t fsl_shw_gen_encrypt(fsl_shw_uco_t * user_ctx,
				     fsl_shw_acco_t * auth_ctx,
				     fsl_shw_sko_t * cipher_key_info,
				     fsl_shw_sko_t * auth_key_info,
				     uint32_t auth_data_length,
				     const uint8_t * auth_data,
				     uint32_t payload_length,
				     const uint8_t * payload,
				     uint8_t * ct, uint8_t * auth_value)
{
	volatile fsl_shw_return_t status = FSL_RETURN_ERROR_S;

	/* Unused */
	(void)user_ctx;
	(void)auth_ctx;
	(void)cipher_key_info;
	(void)auth_key_info;	/* save compilation warning */
	(void)auth_data_length;
	(void)auth_data;
	(void)payload_length;
	(void)payload;
	(void)ct;
	(void)auth_value;

	return status;
}

#ifdef LINUX_VERSION_CODE
EXPORT_SYMBOL(fsl_shw_auth_decrypt);
#endif
/*!
 * @brief Authenticate and decrypt a (CCM) stream.
 *
 * @param user_ctx         The user's context
 * @param auth_ctx         Info on this Auth operation
 * @param cipher_key_info  Key to encrypt payload
 * @param auth_key_info    (unused - same key in CCM)
 * @param auth_data_length Length in bytes of @a auth_data
 * @param auth_data        Any auth-only data
 * @param payload_length   Length in bytes of @a payload
 * @param ct               The encrypted data
 * @param auth_value       The authentication code to validate
 * @param payload     The location to store decrypted data
 *
 * @return    A return code of type #fsl_shw_return_t.
 */
fsl_shw_return_t fsl_shw_auth_decrypt(fsl_shw_uco_t * user_ctx,
				      fsl_shw_acco_t * auth_ctx,
				      fsl_shw_sko_t * cipher_key_info,
				      fsl_shw_sko_t * auth_key_info,
				      uint32_t auth_data_length,
				      const uint8_t * auth_data,
				      uint32_t payload_length,
				      const uint8_t * ct,
				      const uint8_t * auth_value,
				      uint8_t * payload)
{
	volatile fsl_shw_return_t status = FSL_RETURN_ERROR_S;

	/* Unused */
	(void)user_ctx;
	(void)auth_ctx;
	(void)cipher_key_info;
	(void)auth_key_info;	/* save compilation warning */
	(void)auth_data_length;
	(void)auth_data;
	(void)payload_length;
	(void)ct;
	(void)auth_value;
	(void)payload;

	return status;
}
#endif
