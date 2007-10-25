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
 * @file fsl_platform.h
 *
 * Header file to isolate code which might be platform-dependent
 */

#ifndef FSL_PLATFORM_H
#define FSL_PLATFORM_H

#ifdef __KERNEL__
#include "portable_os.h"
#endif

#if defined(FSL_PLATFORM_OTHER)

/* Have Makefile or other method of setting FSL_HAVE_* flags */

#elif defined(CONFIG_ARCH_MX3)	/* i.MX31 */

#define FSL_HAVE_RTIC
#define FSL_HAVE_RNGA

#elif defined(CONFIG_ARCH_MX27)

#define FSL_HAVE_SAHARA2
#define SUBMIT_MULTIPLE_DARS
#define FSL_HAVE_RTIC
#define USE_OLD_PTRS

#elif defined(CONFIG_8548)

#define FSL_HAVE_SEC2x

#elif defined(CONFIG_MPC8374)

#define FSL_HAVE_SEC3x

#else

#error UNKNOWN_PLATFORM

#endif				/* platform checks */

#endif				/* FSL_PLATFORM_H */
