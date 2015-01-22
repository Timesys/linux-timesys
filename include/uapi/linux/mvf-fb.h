/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 *  Freescale DCU Frame Buffer device driver ioctls
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#ifndef __MVF_FB_H__
#define __MVF_FB_H__

#include <linux/fb.h>

/* ioctls */

#define MFB_SET_ALPHA		_IOW('M', 0, __u8)
#define MFB_GET_ALPHA		_IOR('M', 0, __u8)
#define MFB_SET_LAYER		_IOW('M', 4, struct layer_display_offset)
#define MFB_GET_LAYER		_IOR('M', 4, struct layer_display_offset)

#ifndef u32
#define u32 unsigned int
#endif

struct layer_display_offset {
	int x_layer_d;
	int y_layer_d;
};

#endif
