#error "please port this file to linux 2.6.18"
/*
-------------------------------------------------------------------------------|
|
| ctswic.h - Header for CodeTEST SWIC data structures used
|            in the Linux kernel
|
| Copyright (c) 2003 - 2007 Freescale Semiconductor, Inc.
| 
| This software is licensed under the GPL.  It may be redistributed 
| and/or modified under the terms of the GNU General Public License as 
| published by the Free Software Foundation; either version 2 of the 
| License, or (at your option) any later version.
|
| Module version: Revision: 1.8
|
-------------------------------------------------------------------------------|
*/

#ifndef CT_SWIC_H_
#define CT_SWIC_H_

/* define this if you disabled in ctdriver.c */
#ifdef CONFIG_ARM
#define CT_SWIC_BUFFER_NOCACHE
#endif

typedef struct Q_Tag {
	u32 tag;
	u32 a_time;
} Q_Tag;

#define HOOK_BUFFER_SIZE  2048	/* must be a power of two */

struct Hook_Buffer {
	int head;
	volatile int tail;
	Q_Tag buffer[HOOK_BUFFER_SIZE];
};

#endif
