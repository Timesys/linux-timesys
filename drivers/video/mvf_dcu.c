/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * Faraday DCU Frame Buffer device driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/console.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <asm/mach-types.h>
#include <mach/mvf.h>
#include <mach/mvf-dcu-fb.h>


#define DRIVER_NAME	"mvf-dcu"

static struct fb_videomode __devinitdata mvf_dcu_default_mode = {
	.pixclock	= 31250,
	.xres		= 800,
	.yres		= 480,
	.left_margin	= 42,
	.right_margin	= 86,
	.upper_margin	= 10,
	.lower_margin	= 33,
	.hsync_len	= 128,
	.vsync_len	= 2,
	.sync		= FB_SYNC_COMP_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT |
			  FB_SYNC_HOR_HIGH_ACT,
	.vmode		= FB_VMODE_NONINTERLACED,
};

static struct fb_videomode __devinitdata mvf_dcu_mode_db[] = {
	{
		.name		= "480x272",
		.xres		= 480,
		.yres		= 272,
		.left_margin	= 2,
		.right_margin	= 2,
		.upper_margin	= 1,
		.lower_margin	= 1,
		.hsync_len	= 41,
		.vsync_len	= 2,
		.sync		= FB_SYNC_COMP_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
        {
                .name           = "pm070wl4",
		.pixclock	= 31250,
                .xres           = 800,
                .yres           = 480,
                .left_margin    = 42,
                .right_margin   = 86,
                .upper_margin   = 10,
                .lower_margin   = 33,
                .hsync_len      = 128,
                .vsync_len      = 2,
                .sync           = FB_SYNC_COMP_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT |
				  FB_SYNC_HOR_HIGH_ACT,
                .vmode          = FB_VMODE_NONINTERLACED,
        },

};

static DEFINE_SPINLOCK(dcu_lock);

/* Structure containing the Faraday DCU specific framebuffer information */
struct mvf_dcu_fb_data {
	struct fb_info *mvf_dcu_info[DCU_LAYER_NUM - 1];
	struct dcu_layer_desc *dummy_layer_desc;
	void __iomem *base;
	unsigned int irq;
	struct clk *clk;
	int fb_enabled;
};

struct mfb_info {
	int index;
	int type;
	char *id;
	int registered;
	int blank;
	char *mode_str;
	int default_bpp;
	unsigned long pseudo_palette[16];
	struct dcu_layer_desc *layer_desc;
	int cursor_reset;
	unsigned char g_alpha;
	unsigned int count;
	int x_layer_d;	/* layer display x offset to physical screen */
	int y_layer_d;	/* layer display y offset to physical screen */
	struct mvf_dcu_fb_data *parent;
};

static struct mfb_info mfb_template[] = {
	{
	.index = 0,
	.type = DCU_TYPE_OUTPUT,
	.id = "Layer0",
	.registered = 0,
	.g_alpha = 0xff,
	.count = 0,
	.x_layer_d = 0,
	.y_layer_d = 0,
	},
	{
	.index = 1,
	.type = DCU_TYPE_OUTPUT,
	.id = "Layer1",
	.registered = 0,
	.g_alpha = 0xff,
	.count = 0,
	.x_layer_d = 50,
	.y_layer_d = 50,
	},
	{
	.index = 2,
	.type = DCU_TYPE_OUTPUT,
	.id = "Layer2",
	.registered = 0,
	.g_alpha = 0xff,
	.count = 0,
	.x_layer_d = 100,
	.y_layer_d = 100,
	},
	{
	.index = 3,
	.type = DCU_TYPE_OUTPUT,
	.id = "Layer3",
	.registered = 0,
	.g_alpha = 0xff,
	.count = 0,
	.x_layer_d = 150,
	.y_layer_d = 150,
	},
};

static int total_open_layers = 0;


static int mvf_dcu_enable_panel(struct fb_info *info)
{
	struct mfb_info *mfbi = info->par;
	struct mvf_dcu_fb_data *dcu = mfbi->parent;
	struct dcu_layer_desc *layer_desc = mfbi->layer_desc;
	int i;
	int ret = 0;

	if (mfbi->type != DCU_TYPE_OFF) {
		i = mfbi->index;
		writel(DCU_CTRLDESCLN_0_HEIGHT(layer_desc->height) |
			DCU_CTRLDESCLN_0_WIDTH(layer_desc->width),
			dcu->base + DCU_CTRLDESCLN_0(i));
		writel(DCU_CTRLDESCLN_1_POSY(layer_desc->posy) |
			DCU_CTRLDESCLN_1_POSX(layer_desc->posx),
			dcu->base + DCU_CTRLDESCLN_1(i));
		writel(layer_desc->addr, dcu->base + DCU_CTRLDESCLN_2(i));
		writel(DCU_CTRLDESCLN_3_EN(layer_desc->en) |
			DCU_CTRLDESCLN_3_TILE_EN(layer_desc->tile_en) |
			DCU_CTRLDESCLN_3_DATA_SEL(layer_desc->data_sel_clut) |
			DCU_CTRLDESCLN_3_SAFETY_EN(layer_desc->safety_en) |
			DCU_CTRLDESCLN_3_TRANS(layer_desc->trans) |
			DCU_CTRLDESCLN_3_BPP(layer_desc->bpp) |
			DCU_CTRLDESCLN_3_RLE_EN(layer_desc->rle_en) |
			DCU_CTRLDESCLN_3_LUOFFS(layer_desc->lut_offset) |
			DCU_CTRLDESCLN_3_BB(layer_desc->chroma_key_en) |
			DCU_CTRLDESCLN_3_AB(layer_desc->blend),
			dcu->base + DCU_CTRLDESCLN_3(i));
		writel(DCU_CTRLDESCLN_4_CKMAX_R(layer_desc->ck_r_max) |
			DCU_CTRLDESCLN_4_CKMAX_G(layer_desc->ck_g_max) |
			DCU_CTRLDESCLN_4_CKMAX_B(layer_desc->ck_b_max),
			dcu->base + DCU_CTRLDESCLN_4(i));
		writel(DCU_CTRLDESCLN_5_CKMIN_R(layer_desc->ck_r_min) |
			DCU_CTRLDESCLN_5_CKMIN_G(layer_desc->ck_g_min) |
			DCU_CTRLDESCLN_5_CKMIN_B(layer_desc->ck_b_min),
			dcu->base + DCU_CTRLDESCLN_5(i));
		writel(DCU_CTRLDESCLN_6_TILE_VER(layer_desc->tile_height) |
			DCU_CTRLDESCLN_6_TILE_HOR(layer_desc->tile_width),
			dcu->base + DCU_CTRLDESCLN_6(i));
		writel(DCU_CTRLDESCLN_7_FG_FCOLOR(layer_desc->trans_fgcolor),
			dcu->base + DCU_CTRLDESCLN_7(i));
		writel(DCU_CTRLDESCLN_8_BG_BCOLOR(layer_desc->trans_bgcolor),
			dcu->base + DCU_CTRLDESCLN_8(i));
	} else
		ret = -EINVAL;

	writel(DCU_UPDATE_MODE_READREG(1), dcu->base + DCU_UPDATE_MODE);
	return ret;
}

static int mvf_dcu_disable_panel(struct fb_info *info)
{
	struct mfb_info *mfbi = info->par;
	struct mvf_dcu_fb_data *dcu = mfbi->parent;
	struct dcu_layer_desc *layer_desc = mfbi->layer_desc;
	int ret = 0;
	int i;

	i = mfbi->index;
	writel(DCU_CTRLDESCLN_0_HEIGHT(layer_desc->height) |
		DCU_CTRLDESCLN_0_WIDTH(layer_desc->width),
		dcu->base + DCU_CTRLDESCLN_0(i));
	writel(DCU_CTRLDESCLN_1_POSY(0) | DCU_CTRLDESCLN_1_POSX(0),
		dcu->base + DCU_CTRLDESCLN_1(i));
	writel(layer_desc->addr, dcu->base + DCU_CTRLDESCLN_2(i));
	writel(DCU_CTRLDESCLN_3_EN(0) | DCU_CTRLDESCLN_3_TRANS(0),
		dcu->base + DCU_CTRLDESCLN_3(i));
	writel(DCU_CTRLDESCLN_4_CKMAX_R(0xff) |
		DCU_CTRLDESCLN_4_CKMAX_G(0xff) |
		DCU_CTRLDESCLN_4_CKMAX_B(0xff),
		dcu->base + DCU_CTRLDESCLN_4(i));
	writel(DCU_CTRLDESCLN_5_CKMIN_R(0) |
		DCU_CTRLDESCLN_5_CKMIN_G(0) |
		DCU_CTRLDESCLN_5_CKMIN_B(0),
		dcu->base + DCU_CTRLDESCLN_5(i));
	writel(DCU_CTRLDESCLN_6_TILE_VER(0) | DCU_CTRLDESCLN_6_TILE_HOR(0),
		dcu->base + DCU_CTRLDESCLN_6(i));
	writel(DCU_CTRLDESCLN_7_FG_FCOLOR(0), dcu->base + DCU_CTRLDESCLN_7(i));
	writel(DCU_CTRLDESCLN_8_BG_BCOLOR(0), dcu->base + DCU_CTRLDESCLN_8(i));

	writel(DCU_UPDATE_MODE_READREG(1), dcu->base + DCU_UPDATE_MODE);
	return ret;
}

static void enable_lcdc(struct fb_info *info)
{
	struct mfb_info *mfbi = info->par;
	struct mvf_dcu_fb_data *dcu = mfbi->parent;
	unsigned int dcu_mode;

	if (!dcu->fb_enabled) {
		dcu_mode = readl(dcu->base + DCU_DCU_MODE);
		writel(dcu_mode | DCU_MODE_DCU_MODE(1),
			dcu->base + DCU_DCU_MODE);
		dcu->fb_enabled++;
	}
}

static void disable_lcdc(struct fb_info *info)
{
	struct mfb_info *mfbi = info->par;
	struct mvf_dcu_fb_data *dcu = mfbi->parent;

	if (dcu->fb_enabled) {
		writel(DCU_MODE_DCU_MODE(0), dcu->base + DCU_DCU_MODE);
		dcu->fb_enabled = 0;
	}
}

static void adjust_layer_size_position(struct fb_var_screeninfo *var,
		struct fb_info *info)
{
	struct mfb_info *mfbi = info->par;
	struct mvf_dcu_fb_data *dcu = mfbi->parent;
	__u32 base_layer_width, base_layer_height;

	base_layer_width = dcu->mvf_dcu_info[0]->var.xres;
	base_layer_height = dcu->mvf_dcu_info[0]->var.yres;

	if (mfbi->x_layer_d < 0)
		mfbi->x_layer_d = 0;
	if (mfbi->y_layer_d < 0)
		mfbi->y_layer_d = 0;
}

static int mvf_dcu_check_var(struct fb_var_screeninfo *var,
				struct fb_info *info)
{
	struct mfb_info *mfbi = info->par;

	if (var->xres_virtual < var->xres)
		var->xres_virtual = var->xres;
	if (var->yres_virtual < var->yres)
		var->yres_virtual = var->yres;

	if (var->xoffset < 0)
		var->xoffset = 0;

	if (var->yoffset < 0)
		var->yoffset = 0;

	if (var->xoffset + info->var.xres > info->var.xres_virtual)
		var->xoffset = info->var.xres_virtual - info->var.xres;

	if (var->yoffset + info->var.yres > info->var.yres_virtual)
		var->yoffset = info->var.yres_virtual - info->var.yres;

	if ((var->bits_per_pixel != 32) && (var->bits_per_pixel != 24) &&
	    (var->bits_per_pixel != 16))
		var->bits_per_pixel = mfbi->default_bpp;

	switch (var->bits_per_pixel) {
	case 16:
		var->red.length = 5;
		var->red.offset = 11;
		var->red.msb_right = 0;

		var->green.length = 6;
		var->green.offset = 5;
		var->green.msb_right = 0;

		var->blue.length = 5;
		var->blue.offset = 0;
		var->blue.msb_right = 0;

		var->transp.length = 0;
		var->transp.offset = 0;
		var->transp.msb_right = 0;
		break;
	case 24:
		var->red.length = 8;
		var->red.offset = 16;
		var->red.msb_right = 0;

		var->green.length = 8;
		var->green.offset = 8;
		var->green.msb_right = 0;

		var->blue.length = 8;
		var->blue.offset = 0;
		var->blue.msb_right = 0;

		var->transp.length = 0;
		var->transp.offset = 0;
		var->transp.msb_right = 0;
		break;
	case 32:
		var->red.length = 8;
		var->red.offset = 16;
		var->red.msb_right = 0;

		var->green.length = 8;
		var->green.offset = 8;
		var->green.msb_right = 0;

		var->blue.length = 8;
		var->blue.offset = 0;
		var->blue.msb_right = 0;

		var->transp.length = 8;
		var->transp.offset = 24;
		var->transp.msb_right = 0;

		break;
	}

	var->height = -1;
	var->width = -1;
	var->grayscale = 0;

	/* Copy nonstd field to/from sync for fbset usage */
	var->sync |= var->nonstd;
	var->nonstd |= var->sync;

	adjust_layer_size_position(var, info);
	return 0;
}

static void set_fix(struct fb_info *info)
{
	struct fb_fix_screeninfo *fix = &info->fix;
	struct fb_var_screeninfo *var = &info->var;
	struct mfb_info *mfbi = info->par;

	strncpy(fix->id, mfbi->id, strlen(mfbi->id));
	fix->line_length = var->xres_virtual * var->bits_per_pixel / 8;
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->accel = FB_ACCEL_NONE;
	fix->visual = FB_VISUAL_TRUECOLOR;
	fix->xpanstep = 1;
	fix->ypanstep = 1;
}

static void update_lcdc(struct fb_info *info)
{
	struct fb_var_screeninfo *var = &info->var;
	struct mfb_info *mfbi = info->par;
	struct mvf_dcu_fb_data *dcu = mfbi->parent;

	if (mfbi->type == DCU_TYPE_OFF) {
		mvf_dcu_disable_panel(info);
		return;
	}

	disable_lcdc(info);

	/* Height and width of the cursor in pixels */
	writel(DCU_CTRLDESCCURSOR1_HEIGHT(16) | DCU_CTRLDESCCURSOR1_WIDTH(16),
		dcu->base + DCU_CTRLDESCCURSOR1);
	/* Y and X positions of the cursor in pixels */
	writel(DCU_CTRLDESCCURSOR2_POSY(0) | DCU_CTRLDESCCURSOR2_POSX(0),
		dcu->base + DCU_CTRLDESCCURSOR2);
	writel(DCU_CTRLDESCCURSOR3_CUR_EN(0), dcu->base + DCU_CTRLDESCCURSOR3);

	writel(DCU_BGND_R(0) | DCU_BGND_G(0) | DCU_BGND_B(0),
		dcu->base + DCU_BGND);

	writel(DCU_DISP_SIZE_DELTA_Y(var->yres) |
		DCU_DISP_SIZE_DELTA_X(var->xres / 16),
		dcu->base + DCU_DISP_SIZE);

	/* Horizontal and vertical sync parameter */
	writel(DCU_HSYN_PARA_BP(var->left_margin) |
		DCU_HSYN_PARA_PW(var->hsync_len) |
		DCU_HSYN_PARA_FP(var->right_margin),
		dcu->base + DCU_HSYN_PARA);

	writel(DCU_VSYN_PARA_BP(var->upper_margin) |
		DCU_VSYN_PARA_PW(var->vsync_len) |
		DCU_VSYN_PARA_FP(var->lower_margin),
		dcu->base + DCU_VSYN_PARA);

	writel(DCU_MODE_BLEND_ITER(3) | DCU_MODE_RASTER_EN(1),
			dcu->base + DCU_DCU_MODE);

	writel(9, dcu->base + DCU_DIV_RATIO);

	writel(DCU_SYN_POL_INV_PXCK(0) | DCU_SYN_POL_NEG(0) |
		DCU_SYN_POL_INV_VS(1) | DCU_SYN_POL_INV_HS(1),
		dcu->base + DCU_SYN_POL);
	writel(DCU_THRESHOLD_LS_BF_VS(0x3) | DCU_THRESHOLD_OUT_BUF_HIGH(0x78) |
		DCU_THRESHOLD_OUT_BUF_LOW(0), dcu->base + DCU_THRESHOLD);

	/* Enable the DCU */
	enable_lcdc(info);
}

static int map_video_memory(struct fb_info *info)
{
	u32 smem_len = info->fix.line_length * info->var.yres_virtual;

	info->screen_base = kzalloc(smem_len, GFP_KERNEL);
	if (info->screen_base == NULL) {
		printk(KERN_ERR "Unable to allocate fb memory\n");
		return -ENOMEM;
	}
	mutex_lock(&info->mm_lock);
	info->fix.smem_start = virt_to_phys(info->screen_base);
	info->fix.smem_len = smem_len;
	mutex_unlock(&info->mm_lock);
	info->screen_size = info->fix.smem_len;

	return 0;
}

static void unmap_video_memory(struct fb_info *info)
{
	mutex_lock(&info->mm_lock);
	info->screen_base = NULL;
	info->fix.smem_start = 0;
	info->fix.smem_len = 0;
	mutex_unlock(&info->mm_lock);
}

/*
 * Using the fb_var_screeninfo in fb_info we set the layer of this
 * particular framebuffer. It is a light version of mvf_dcu_set_par.
 */
static int mvf_dcu_set_layer(struct fb_info *info)
{
	struct mfb_info *mfbi = info->par;
	struct dcu_layer_desc *layer_desc = mfbi->layer_desc;
	struct fb_var_screeninfo *var = &info->var;
	struct mvf_dcu_fb_data *dcu = mfbi->parent;
	int pixel_offset;
	unsigned long addr;

	pixel_offset = (var->yoffset * var->xres_virtual) + var->xoffset;
	addr = layer_desc->addr + (pixel_offset * (var->bits_per_pixel >> 3));

	writel(addr, dcu->base + DCU_CTRLDESCLN_2(mfbi->index));
	writel(DCU_UPDATE_MODE_READREG(1), dcu->base + DCU_UPDATE_MODE);

	return 0;
}

/*
 * Using the fb_var_screeninfo in fb_info we set the resolution of this
 * particular framebuffer. This function alters the fb_fix_screeninfo stored
 * in fb_info. It does not alter var in fb_info since we are using that
 * data. This means we depend on the data in var inside fb_info to be
 * supported by the hardware. mvf_dcu_check_var is always called before
 * mvf_dcu_set_par to ensure this.
 */
static int mvf_dcu_set_par(struct fb_info *info)
{
	unsigned long len;
	struct fb_var_screeninfo *var = &info->var;
	struct mfb_info *mfbi = info->par;
	struct dcu_layer_desc *layer_desc = mfbi->layer_desc;

	set_fix(info);
	mfbi->cursor_reset = 1;

	len = info->var.yres_virtual * info->fix.line_length;
	if (len != info->fix.smem_len) {
		if (info->fix.smem_start)
			unmap_video_memory(info);

		/* Memory allocation for framebuffer */
		if (map_video_memory(info)) {
			printk(KERN_ERR "Unable to allocate fb memory 1\n");
			return -ENOMEM;
		}
	}

	layer_desc->addr = info->fix.smem_start;

	/* Layer should not be greater than display size */
	layer_desc->width = var->xres;
	layer_desc->height = var->yres;
	layer_desc->posx = mfbi->x_layer_d;
	layer_desc->posy = mfbi->y_layer_d;

	layer_desc->blend = 0x01;
	layer_desc->chroma_key_en = 0;
	layer_desc->lut_offset = 0;
	layer_desc->rle_en = 0;
	layer_desc->bpp = BPP_24;
	layer_desc->trans = mfbi->g_alpha;
	layer_desc->safety_en = 0;
	layer_desc->data_sel_clut = 0;
	layer_desc->tile_en = 0;
	layer_desc->en = 1;
	layer_desc->ck_r_min = 0;
	layer_desc->ck_r_max = 0xff;
	layer_desc->ck_g_min = 0;
	layer_desc->ck_g_max = 0xff;
	layer_desc->ck_b_min = 0;
	layer_desc->ck_b_max = 0xff;
	layer_desc->tile_width = 0;
	layer_desc->tile_height = 0;
	layer_desc->trans_fgcolor = 0;
	layer_desc->trans_bgcolor = 0;

	/* Only layer 0 could update LCDC */
	if (mfbi->index == 0)
		update_lcdc(info);

	mvf_dcu_enable_panel(info);
	return 0;
}

static inline __u32 CNVT_TOHW(__u32 val, __u32 width)
{
	return ((val<<width) + 0x7FFF - val) >> 16;
}

static int mvf_dcu_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp, struct fb_info *info)
{
	int ret = 1;

	/*
	 * If greyscale is true, then we convert the RGB value
	 * to greyscale no matter what visual we are using.
	 */
	if (info->var.grayscale)
		red = green = blue = (19595 * red + 38470 * green +
				      7471 * blue) >> 16;
	switch (info->fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		/*
		 * 16-bit True Colour.  We encode the RGB value
		 * according to the RGB bitfield information.
		 */
		if (regno < 16) {
			u32 *pal = info->pseudo_palette;
			u32 v;

			red = CNVT_TOHW(red, info->var.red.length);
			green = CNVT_TOHW(green, info->var.green.length);
			blue = CNVT_TOHW(blue, info->var.blue.length);
			transp = CNVT_TOHW(transp, info->var.transp.length);

			v = (red << info->var.red.offset) |
			    (green << info->var.green.offset) |
			    (blue << info->var.blue.offset) |
			    (transp << info->var.transp.offset);

			pal[regno] = v;
			ret = 0;
		}
		break;
	case FB_VISUAL_STATIC_PSEUDOCOLOR:
	case FB_VISUAL_PSEUDOCOLOR:
		break;
	}

	return ret;
}

static int mvf_dcu_pan_display(struct fb_var_screeninfo *var,
			     struct fb_info *info)
{
	if ((info->var.xoffset == var->xoffset) &&
	    (info->var.yoffset == var->yoffset))
		return 0;	/* No change, do nothing */

	if (var->xoffset < 0 || var->yoffset < 0
	    || var->xoffset + info->var.xres > info->var.xres_virtual
	    || var->yoffset + info->var.yres > info->var.yres_virtual)
		return -EINVAL;

	info->var.xoffset = var->xoffset;
	info->var.yoffset = var->yoffset;

	if (var->vmode & FB_VMODE_YWRAP)
		info->var.vmode |= FB_VMODE_YWRAP;
	else
		info->var.vmode &= ~FB_VMODE_YWRAP;

	mvf_dcu_set_layer(info);

	return 0;
}

static int mvf_dcu_blank(int blank_mode, struct fb_info *info)
{
	struct mfb_info *mfbi = info->par;

#ifdef CONFIG_MVF_DCU_BLANKING_TEST
	mfbi->blank = blank_mode;

	switch (blank_mode) {
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_NORMAL:
		mvf_dcu_disable_panel(info);
		break;
	case FB_BLANK_POWERDOWN:
		disable_lcdc(info);
		break;
	case FB_BLANK_UNBLANK:
		mvf_dcu_enable_panel(info);
		break;
	}
#endif

	return 0;
}

static int mvf_dcu_ioctl(struct fb_info *info, unsigned int cmd,
		unsigned long arg)
{
	struct mfb_info *mfbi = info->par;
	struct dcu_layer_desc *layer_desc = mfbi->layer_desc;
	struct layer_display_offset layer_d;
	void __user *buf = (void __user *)arg;
	unsigned char global_alpha;

	if (!arg)
		return -EINVAL;
	switch (cmd) {
	case MFB_SET_LAYER:
		if (copy_from_user(&layer_d, buf, sizeof(layer_d)))
			return -EFAULT;
		mfbi->x_layer_d = layer_d.x_layer_d;
		mfbi->y_layer_d = layer_d.y_layer_d;
		pr_debug("set LAYER display offset of index %d to (%d,%d)\n",
			mfbi->index, layer_d.x_layer_d, layer_d.y_layer_d);
		mvf_dcu_check_var(&info->var, info);
		mvf_dcu_set_par(info);
		break;
	case MFB_GET_LAYER:
		layer_d.x_layer_d = mfbi->x_layer_d;
		layer_d.y_layer_d = mfbi->y_layer_d;
		if (copy_to_user(buf, &layer_d, sizeof(layer_d)))
			return -EFAULT;
		pr_debug("get LAYER display offset of index %d (%d,%d)\n",
			mfbi->index, layer_d.x_layer_d, layer_d.y_layer_d);
		break;
	case MFB_GET_ALPHA:
		global_alpha = mfbi->g_alpha;
		if (copy_to_user(buf, &global_alpha, sizeof(global_alpha)))
			return -EFAULT;
		pr_debug("get global alpha of index %d\n", mfbi->index);
		break;
	case MFB_SET_ALPHA:
		if (copy_from_user(&global_alpha, buf, sizeof(global_alpha)))
			return -EFAULT;
		mfbi->g_alpha = global_alpha;
		mvf_dcu_check_var(&info->var, info);
		mvf_dcu_set_par(info);
		pr_debug("set global alpha for index %d\n", mfbi->index);
		break;
	case FBIOGET_GWINFO:
		if (mfbi->type == DCU_TYPE_OFF)
			return -ENODEV;
		/* get graphic window information */
		if (copy_to_user(buf, layer_desc, sizeof(*layer_desc)))
			return -EFAULT;
		break;
	case FBIOGET_HWCINFO:
		pr_debug("FBIOGET_HWCINFO:0x%08x\n", FBIOGET_HWCINFO);
		break;
	case FBIOPUT_MODEINFO:
		pr_debug("FBIOPUT_MODEINFO:0x%08x\n", FBIOPUT_MODEINFO);
		break;
	case FBIOGET_DISPINFO:
		pr_debug("FBIOGET_DISPINFO:0x%08x\n", FBIOGET_DISPINFO);
		break;

	default:
		printk(KERN_ERR "Unknown ioctl command (0x%08X)\n", cmd);
		return -ENOIOCTLCMD;
	}

	return 0;
}

static int mvf_dcu_open(struct fb_info *info, int user)
{
	struct mfb_info *mfbi = info->par;
	struct mvf_dcu_fb_data *dcu = mfbi->parent;
	int i;
	int ret = 0;

	mfbi->index = info->node;
	spin_lock(&dcu_lock);

	/* if first time any layer open (e.g., at boot time) reset all */
	if (total_open_layers == 0) {

		writel(0, dcu->base + DCU_INT_STATUS);
		writel(0, dcu->base + DCU_PARR_ERR_STA_1);
		writel(0, dcu->base + DCU_PARR_ERR_STA_2);
		writel(0, dcu->base + DCU_PARR_ERR_STA_3);

		for (i = 0; i < 64; i++) {
			writel(0, dcu->base + DCU_CTRLDESCLN_0(i));
			writel(0, dcu->base + DCU_CTRLDESCLN_1(i));
			writel(0, dcu->base + DCU_CTRLDESCLN_2(i));
			writel(0, dcu->base + DCU_CTRLDESCLN_3(i));
			writel(0, dcu->base + DCU_CTRLDESCLN_4(i));
			writel(0, dcu->base + DCU_CTRLDESCLN_5(i));
			writel(0, dcu->base + DCU_CTRLDESCLN_6(i));
			writel(0, dcu->base + DCU_CTRLDESCLN_7(i));
			writel(0, dcu->base + DCU_CTRLDESCLN_8(i));
		}
	}

	mfbi->count++;
	if (mfbi->count == 1) {
		pr_debug("open layer index %d\n", mfbi->index);
		mvf_dcu_check_var(&info->var, info);
		ret = mvf_dcu_set_par(info);
		if (ret < 0)
			mfbi->count--;
		else
			total_open_layers++;
	}

	spin_unlock(&dcu_lock);
	return ret;
}

static int mvf_dcu_release(struct fb_info *info, int user)
{
	struct mfb_info *mfbi = info->par;
	int ret = 0;

	spin_lock(&dcu_lock);
	mfbi->count--;
	if (mfbi->count == 0) {
		pr_debug("release layer index %d\n", mfbi->index);
		ret = mvf_dcu_disable_panel(info);
		if (ret < 0)
			mfbi->count++;
		else
			total_open_layers--;
	}

	spin_unlock(&dcu_lock);
	return ret;
}

static struct fb_ops mvf_dcu_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = mvf_dcu_check_var,
	.fb_set_par = mvf_dcu_set_par,
	.fb_setcolreg = mvf_dcu_setcolreg,
	.fb_blank = mvf_dcu_blank,
	.fb_pan_display = mvf_dcu_pan_display,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	.fb_ioctl = mvf_dcu_ioctl,
	.fb_open = mvf_dcu_open,
	.fb_release = mvf_dcu_release,
};

static int init_fbinfo(struct fb_info *info)
{
	struct mfb_info *mfbi = info->par;

	info->device = NULL;
	info->var.activate = FB_ACTIVATE_NOW;
	info->fbops = &mvf_dcu_ops;
	info->flags = FBINFO_FLAG_DEFAULT;
	info->pseudo_palette = &mfbi->pseudo_palette;

	/* Allocate colormap */
	fb_alloc_cmap(&info->cmap, 16, 0);
	return 0;
}

static int __devinit install_fb(struct fb_info *info)
{
	struct mfb_info *mfbi = info->par;
	struct fb_videomode *db = mvf_dcu_mode_db;
	unsigned int dbsize = ARRAY_SIZE(mvf_dcu_mode_db);
	int rc;

	if (init_fbinfo(info))
		return -EINVAL;

	rc = fb_find_mode(&info->var, info, mfbi->mode_str, db, dbsize,
			&mvf_dcu_default_mode, mfbi->default_bpp);

	if (mvf_dcu_check_var(&info->var, info)) {
		printk(KERN_ERR "fb_check_var failed");
		fb_dealloc_cmap(&info->cmap);
		return -EINVAL;
	}

	if (register_framebuffer(info) < 0) {
		printk(KERN_ERR "register_framebuffer failed");
		unmap_video_memory(info);
		fb_dealloc_cmap(&info->cmap);
		return -EINVAL;
	}

	mfbi->registered = 1;
	printk(KERN_INFO "fb%d: %s fb device registered successfully.\n",
		info->node, info->fix.id);

	return 0;
}

static void uninstall_fb(struct fb_info *info)
{
	struct mfb_info *mfbi = info->par;

	if (!mfbi->registered)
		return;

	unregister_framebuffer(info);
	unmap_video_memory(info);
	if (&info->cmap)
		fb_dealloc_cmap(&info->cmap);

	mfbi->registered = 0;
}

static irqreturn_t mvf_dcu_irq(int irq, void *dev_id)
{
	struct mvf_dcu_fb_data *dcu = dev_id;
	unsigned int status = readl(dcu->base + DCU_INT_STATUS);
	unsigned int dcu_mode;

	if (status) {
		/* This is the workaround for underrun */
		if (status & DCU_INT_STATUS_UNDRUN) {
			dcu_mode = readl(dcu->base + DCU_DCU_MODE);
			dcu_mode &= ~DCU_MODE_DCU_MODE_MASK;
			writel(dcu_mode | DCU_MODE_DCU_MODE(0),
				dcu->base + DCU_DCU_MODE);
			pr_debug("Err: DCU occurs underrun!\n");
			udelay(1);
			writel(dcu_mode | DCU_MODE_DCU_MODE(1),
				dcu->base + DCU_DCU_MODE);
		}

		if (status & DCU_INT_STATUS_LYR_TRANS_FINISH)
			writel(DCU_INT_STATUS_LYR_TRANS_FINISH,
				dcu->base + DCU_INT_STATUS);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

static int request_irq_local(int irq, struct mvf_dcu_fb_data *dcu)
{
	unsigned long status;
	int ret;

	ret = request_irq(irq, mvf_dcu_irq, IRQF_DISABLED, DRIVER_NAME, dcu);
	if (ret)
		pr_info("Request dcu IRQ failed.\n");
	else {
		/* Read to clear the status */
		status = readl(dcu->base + DCU_INT_STATUS);
		status = readl(dcu->base + DCU_PARR_ERR_STA_1);
		status = readl(dcu->base + DCU_PARR_ERR_STA_2);
		status = readl(dcu->base + DCU_PARR_ERR_STA_3);
		writel(DCU_INT_MASK_ALL, dcu->base + DCU_INT_MASK);
		writel(DCU_INT_MASK_ALL, dcu->base + DCU_MSK_PARR_ERR_STA_1);
		writel(DCU_INT_MASK_ALL, dcu->base + DCU_MSK_PARR_ERR_STA_2);
		writel(DCU_INT_MASK_ALL, dcu->base + DCU_MSK_PARR_ERR_STA_3);
	}
	return ret;
}

static void free_irq_local(int irq, struct mvf_dcu_fb_data *dcu)
{
	/* Disable all LCDC interrupt */
	writel(DCU_INT_MASK_ALL, dcu->base + DCU_INT_MASK);
	writel(DCU_MSK_PARR_ERR_ALL, dcu->base + DCU_MSK_PARR_ERR_STA_1);
	writel(DCU_MSK_PARR_ERR_ALL, dcu->base + DCU_MSK_PARR_ERR_STA_2);
	writel(DCU_MSK_PARR_ERR_ALL, dcu->base + DCU_MSK_PARR_ERR_STA_3);

	free_irq(irq, dcu);
}

#ifdef CONFIG_PM
static int mvf_dcu_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	struct mvf_dcu_fb_data *dcu = dev_get_drvdata(&pdev->dev);

	clk_disable(dcu->clk);
	return 0;
}

static int mvf_dcu_resume(struct platform_device *pdev)
{
	struct mvf_dcu_fb_data *dcu = dev_get_drvdata(&pdev->dev);

	clk_enable(dcu->clk);
	return 0;
}
#else
#define mvf_dcu_suspend	NULL
#define mvf_dcu_resume	NULL
#endif

static int __devinit mvf_dcu_probe(struct platform_device *pdev)
{
	struct mvf_dcu_platform_data *plat_data = pdev->dev.platform_data;
	struct mvf_dcu_fb_data *dcu;
	struct mfb_info *mfbi;
	struct resource *res;
	int ret = 0;
	int i;

	dcu = kmalloc(sizeof(struct mvf_dcu_fb_data), GFP_KERNEL);
	if (!dcu)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(dcu->mvf_dcu_info); i++) {
		dcu->mvf_dcu_info[i] =
			framebuffer_alloc(sizeof(struct mfb_info),
			&pdev->dev);
		if (!dcu->mvf_dcu_info[i]) {
			dev_err(&pdev->dev, "cannot allocate memory\n");
			ret = ENOMEM;
			goto failed_alloc_framebuffer;
		}
		mfbi = dcu->mvf_dcu_info[i]->par;
		memcpy(mfbi, &mfb_template[i], sizeof(struct mfb_info));
		mfbi->parent = dcu;
		mfbi->mode_str = plat_data->mode_str;
		mfbi->default_bpp = plat_data->default_bpp;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
		goto failed_alloc_framebuffer;
	}
	if (!request_mem_region(res->start, resource_size(res), DRIVER_NAME)) {
		dev_err(&pdev->dev, "request_mem_region failed\n");
		ret = -EBUSY;
		goto failed_alloc_framebuffer;
	}

	dcu->base = ioremap(res->start, resource_size(res));
	if (!dcu->base) {
		dev_err(&pdev->dev, "cannot map DCU registers!\n");
		ret = -EFAULT;
		goto failed_ioremap;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "cannot get IRQ resource\n");
		ret = -ENODEV;
		goto failed_get_resource;
	}

	dcu->irq = res->start;
	ret = request_irq_local(dcu->irq, dcu);
	if (ret) {
		dev_err(&pdev->dev, "could not request irq for dcu\n");
		goto failed_get_resource;
	}

	gpio_request_one(DCU_LCD_ENABLE_PIN, GPIOF_OUT_INIT_LOW, "DCU");
	msleep(2);
	gpio_set_value(DCU_LCD_ENABLE_PIN, 0);

	writel(0x20000000, MVF_IO_ADDRESS(MVF_TCON0_BASE_ADDR));

	dcu->clk = clk_get(&pdev->dev, "dcu_clk");
	if (IS_ERR(dcu->clk)) {
		dev_err(&pdev->dev, "unable to get clock\n");
		goto failed_getclock;
	}

	clk_enable(dcu->clk);
	dcu->fb_enabled = 0;

	for (i = 0; i < ARRAY_SIZE(dcu->mvf_dcu_info); i++) {
		dcu->mvf_dcu_info[i]->fix.smem_start = 0;
		mfbi = dcu->mvf_dcu_info[i]->par;
		mfbi->layer_desc =
			kzalloc(sizeof(struct dcu_layer_desc), GFP_KERNEL);
		ret = install_fb(dcu->mvf_dcu_info[i]);
		if (ret) {
			dev_err(&pdev->dev,
				"Failed to register framebuffer %d\n",
				i);
			goto failed_install_fb;
		}
	}

	dev_set_drvdata(&pdev->dev, dcu);
	return 0;

failed_install_fb:
	kfree(mfbi->layer_desc);
failed_getclock:
	free_irq_local(dcu->irq, dcu);
failed_get_resource:
	iounmap(dcu->base);
failed_ioremap:
	release_mem_region(res->start, resource_size(res));
failed_alloc_framebuffer:
	for (i = 0; i < ARRAY_SIZE(dcu->mvf_dcu_info); i++) {
		if (dcu->mvf_dcu_info[i])
			framebuffer_release(dcu->mvf_dcu_info[i]);
	}
	kfree(dcu);
	return ret;
}

static int mvf_dcu_remove(struct platform_device *pdev)
{
	struct mvf_dcu_fb_data *dcu = dev_get_drvdata(&pdev->dev);
	struct resource *res;
	int i;

	disable_lcdc(dcu->mvf_dcu_info[0]);
	free_irq_local(dcu->irq, dcu);
	clk_disable(dcu->clk);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;
	release_mem_region(res->start, resource_size(res));

	for (i = ARRAY_SIZE(dcu->mvf_dcu_info); i > 0; i--)
		uninstall_fb(dcu->mvf_dcu_info[i - 1]);

	iounmap(dcu->base);
	for (i = 0; i < ARRAY_SIZE(dcu->mvf_dcu_info); i++)
		if (dcu->mvf_dcu_info[i])
			framebuffer_release(dcu->mvf_dcu_info[i]);
	kfree(dcu);

	return 0;
}

static struct platform_driver mvf_dcu_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.probe = mvf_dcu_probe,
	.remove = mvf_dcu_remove,
	.suspend = mvf_dcu_suspend,
	.resume = mvf_dcu_resume,
};

static int __init mvf_dcu_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&mvf_dcu_driver);
	if (ret)
		printk(KERN_ERR
			"mvf-dcu: failed to register platform driver\n");

	return ret;
}

static void __exit mvf_dcu_exit(void)
{
	platform_driver_unregister(&mvf_dcu_driver);
}

module_init(mvf_dcu_init);
module_exit(mvf_dcu_exit);

MODULE_AUTHOR("Alison Wang");
MODULE_DESCRIPTION("Faraday DCU framebuffer driver");
MODULE_LICENSE("GPL");
