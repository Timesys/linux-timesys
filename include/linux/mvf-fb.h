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

#define MFB_SET_ALPHA           0x80014d00
#define MFB_GET_ALPHA           0x40014d00
#define MFB_SET_LAYER		0x80084d04
#define MFB_GET_LAYER		0x40084d04

#define FBIOGET_GWINFO		0x46E0
#define FBIOPUT_GWINFO		0x46E1

#ifndef u32
#define u32 unsigned int
#endif

struct mfb_alpha {
	int enable;
	int alpha;
};

struct layer_display_offset {
	int x_layer_d;
	int y_layer_d;
};

/*
 * These are the fields of control descriptor for every layer
 */
struct dcu_layer_desc {
	u32 layer_num;
	u32 width;
	u32 height;
	u32 posx;
	u32 posy;
	u32 addr;
	u32 blend;
	u32 chroma_key_en;
	u32 lut_offset;
	u32 rle_en;
	u32 bpp;
	u32 trans;
	u32 safety_en;
	u32 data_sel_clut;
	u32 tile_en;
	u32 en;
	u32 ck_r_min;
	u32 ck_r_max;
	u32 ck_g_min;
	u32 ck_g_max;
	u32 ck_b_min;
	u32 ck_b_max;
	u32 tile_width;
	u32 tile_height;
	u32 trans_fgcolor;
	u32 trans_bgcolor;
} __packed;

#endif
