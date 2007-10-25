/* REVISIT Doxygen fixups */
/*
 * DPM support for Freescale i.MX27
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Copyright (C) 2002, 2004 MontaVista Software <source@mvista.com>.
 * Copyright 2007 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * Based on code by Matthew Locke, Dmitry Chigirev, and Bishop Brock.
 */

/*!
 * @file mach-mx27/dpm.c
 *
 * @brief This file provides DPM support hooks for the Freescale i.MX27
 *
 * @ingroup DPM_MX27
 */

#include <linux/dpm.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/pm.h>
#include <linux/delay.h>

#include <asm/hardirq.h>
#include <asm/page.h>
#include <asm/processor.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <asm/arch/mxc_pm.h>
#include <asm/arch/clock.h>

static unsigned saved_cpu_freq;
static unsigned long saved_loops_per_jiffy;
static unsigned int curr_mode = DPM_MODE_RUN;

static struct clk *cpu_clk;
static struct clk *ahb_clk;
static struct clk *ipg_clk;

extern void (*pm_idle) (void);

static int mxc_dpm_set_opt(struct dpm_opt *cur, struct dpm_opt *new)
{
	struct dpm_md_opt *md_cur, *md_new;
	unsigned long flags;

	md_cur = &cur->md_opt;
	md_new = &new->md_opt;

	if (md_new->cpu == -1)
		md_new->cpu = md_cur->cpu;
	if (md_new->ahb == -1)
		md_new->ahb = md_cur->ahb;
	if (md_new->ip == -1)
		md_new->ip = md_cur->ip;
	if (md_new->mode == -1)
		md_new->mode = md_cur->mode;

	if (md_new->cpu == 0 || md_new->mode == DPM_MODE_SLEEP) {

#ifdef CONFIG_PM
		pm_suspend(PM_SUSPEND_MEM);

		/* Here when we wake up.  Recursive call to switch back to
		 * to task state.
		 */
		dpm_set_os(DPM_TASK_STATE);
#endif
		return 0;
	}

	if (md_new->mode == DPM_MODE_WAIT) {
#ifdef CONFIG_PM
		pm_suspend(PM_SUSPEND_STANDBY);

		/* Here when we wake up.  Recursive call to switch back to
		 * to task state.
		 */
		dpm_set_os(DPM_TASK_STATE);
#endif
		return 0;
	}

	if (md_new->mode == DPM_MODE_STOP) {
#ifdef CONFIG_PM
		pm_suspend(PM_SUSPEND_STOP);

		/* Here when we wake up.  Recursive call to switch back to
		 * to task state.
		 */
		dpm_set_os(DPM_TASK_STATE);
#endif
		return 0;
	}

	local_irq_save(flags);

	if (md_new->cpu) {
		loops_per_jiffy =
		    dpm_compute_lpj(saved_loops_per_jiffy, saved_cpu_freq,
				    md_new->cpu / 1000);
	}
#ifdef CONFIG_PM
#ifdef CONFIG_MXC_DVFS
	mxc_pm_dvfs(md_new->cpu, md_new->ahb, md_new->ip);
#endif
	curr_mode = md_new->mode;
#endif

	local_irq_restore(flags);
	return 0;
}

static int mxc_dpm_init_opt(struct dpm_opt *opt)
{
	int core = opt->pp[DPM_CORE_FREQ];
	int ahb = opt->pp[DPM_AHB_FREQ];
	int ip = opt->pp[DPM_IP_FREQ];
	int mode = opt->pp[DPM_MODE];
	struct dpm_md_opt *md_opt = &opt->md_opt;

	if (mode != DPM_MODE_SLEEP &&
	    mode != DPM_MODE_RUN &&
	    mode != DPM_MODE_WAIT && mode != DPM_MODE_STOP && mode != -1) {
		printk(KERN_ERR "MXC DPM Error: "
		       "MCU mode is out of range. "
		       "Possible settings: 0 - sleep,  1 - run, 2 - wait, 3 - stop\n");
		return -EINVAL;
	}

	if (mode == DPM_MODE_SLEEP || core == 0) {
		md_opt->cpu = 0;
		md_opt->ahb = 0;
		md_opt->ip = 0;
		md_opt->mode = mode;
		return 0;
	}

	if (ahb != -1 && (ahb > AHB_MAX || ahb < AHB_MIN)) {
		printk(KERN_ERR "MXC DPM Error: "
		       "AHB frequency setting %u is out of range for opt "
		       "named %s. Possible settings: %d .. %d.\n",
		       ahb, opt->name, AHB_MIN, AHB_MAX);
		return -EINVAL;
	}

	if (ahb != -1 && ahb % AHB_MIN != 0) {
		printk(KERN_ERR "MXC DPM Error: "
		       "AHB frequency setting %u is out of range for opt "
		       "named %s. AHB frequency should be multiple of %d.\n",
		       ahb, opt->name, AHB_MIN);
		return -EINVAL;
	}

	if (ip != -1 && (ip > IPG_MAX || ip < IPG_MIN)) {
		printk(KERN_ERR "MXC DPM Error: "
		       "IPG frequency setting %u is out of range for opt "
		       "named %s. Possible settings: %d .. %d.\n",
		       ip, opt->name, IPG_MIN, IPG_MAX);
		return -EINVAL;
	}

	if (ip != -1 && ip % IPG_MIN != 0) {
		printk(KERN_ERR "MXC DPM Error: "
		       "IP frequency setting %u is out of range for opt "
		       "named %s. IP frequency should be multiple of %d.\n",
		       ip, opt->name, IPG_MIN);
		return -EINVAL;
	}

	if (core != -1 && (core > ARM_MAX || core < ARM_MIN) && core != 0) {
		printk(KERN_ERR "MXC DPM Error: "
		       "ARM frequency setting %u is out of range for opt "
		       "named %s. Possible settings: %d .. %d.\n",
		       core, opt->name, ARM_MIN, ARM_MAX);
		return -EINVAL;
	}
#if 0
	if (core != -1 && core % ARM_MIN != 0) {
		printk(KERN_ERR "MXC DPM Error: "
		       "ARM frequency setting %u is out of range for opt "
		       "named %s. ARM frequency should be multiple of %d.\n",
		       core, opt->name, ARM_MIN);
		return -EINVAL;
	}
#endif
	md_opt->cpu = core;
	md_opt->ahb = ahb;
	md_opt->ip = ip;
	md_opt->mode = mode;
	return 0;
}

/*!
 * Fully determine the current machine-dependent operating point, and fill in a
 * structure presented by the caller. This should only be called when the
 * dpm_sem is held. This call can return an error if the system is currently at
 * an operating point that could not be constructed by dpm_md_init_opt().
 */
static int mxc_dpm_get_opt(struct dpm_opt *opt)
{
	struct dpm_md_opt *md_opt;

	md_opt = &opt->md_opt;

	md_opt->cpu = clk_get_rate(cpu_clk);
	md_opt->ahb = clk_get_rate(ahb_clk);
	md_opt->ip = clk_get_rate(ipg_clk);
	md_opt->mode = curr_mode;

	return 0;
}

/****************************************************************************
 * Machine-dependent /proc/driver/dpm/md entries
 ****************************************************************************/

static inline int p5d(char *buf, unsigned mhz)
{
	return sprintf(buf, "%5d", mhz);	/* Round */
}

int dpm_proc_print_opt(char *buf, struct dpm_opt *opt)
{
	int len = 0;
	struct dpm_md_opt *md_opt;

	md_opt = &opt->md_opt;

	len += sprintf(buf + len, "%12s %9lu", opt->name, opt->stats.count);
	len += sprintf(buf + len, "\t%d\t%d\t%d\n",
		       md_opt->cpu, md_opt->ahb, md_opt->ip);
	/* Add MODE string later */
	return len;
}

int
read_proc_dpm_md_opts(char *page, char **start, off_t offset,
		      int count, int *eof, void *data)
{
	int len = 0;
	int limit = offset + count;
	struct dpm_opt *opt;
	struct list_head *opt_list;

	/* FIXME: For now we assume that the complete table,
	 * formatted, fits within one page */
	if (offset >= PAGE_SIZE)
		return 0;

	if (dpm_lock_interruptible())
		return -ERESTARTSYS;

	if (!dpm_initialized)
		len += sprintf(page + len, "DPM is not initialized\n");
	else if (!dpm_enabled)
		len += sprintf(page + len, "DPM is disabled\n");
	else {
		len += sprintf(page + len,
			       "The active DPM policy is \"%s\"\n",
			       dpm_active_policy->name);
		len += sprintf(page + len,
			       "The current operating point is \"%s\"\n",
			       dpm_active_opt->name);
	}

	if (dpm_initialized) {
		len += sprintf(page + len,
			       "Table of all defined operating points, "
			       "frequencies in MHz:\n");

		len += sprintf(page + len,
			       " Name           Count  DPLL  CPU  TC  PER  DSP  DSPMMU   LCD\n");

		list_for_each(opt_list, &dpm_opts) {
			opt = list_entry(opt_list, struct dpm_opt, list);
			if (len >= PAGE_SIZE)
				BUG();
			if (len >= limit)
				break;
			len += dpm_proc_print_opt(page + len, opt);
		}
	}
	dpm_unlock();
	*eof = 1;
	if (offset >= len)
		return 0;
	*start = page + offset;
	return min(count, len - (int)offset);
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * /proc/driver/dpm/md/cmd (Write-only)
 *
 *  This is a catch-all, simple command processor for the MX27 DPM
 *  implementation. These commands are for experimentation and development
 *  _only_, and may leave the system in an unstable state.
 *
 *  No commands defined now.
 *
 ****************************************************************************/

int
write_proc_dpm_md_cmd(struct file *file, const char *buffer,
		      unsigned long count, void *data)
{
	char *buf, *tok, *s;
	static const char *whitespace = " \t\r\n";
	int ret = 0;

	if (current->uid != 0)
		return -EACCES;
	if (count == 0)
		return 0;
	if (!(buf = kmalloc(count + 1, GFP_KERNEL)))
		return -ENOMEM;
	if (copy_from_user(buf, buffer, count)) {
		kfree(buf);
		return -EFAULT;
	}
	buf[count] = '\0';
	s = buf + strspn(buf, whitespace);
	tok = strsep(&s, whitespace);

	if (strcmp(tok, "define-me") == 0) {
		;
	} else {
		ret = -EINVAL;
	}
	kfree(buf);
	if (ret == 0)
		return count;
	else
		return ret;
}

/****************************************************************************
 *  DPM Idle Handler
 ****************************************************************************/

static void (*orig_idle) (void);

static void mxc_dpm_idle(void)
{
	extern void default_idle(void);

	if (orig_idle)
		orig_idle();
	else
		default_idle();
}

/****************************************************************************
 * Initialization/Exit
 ****************************************************************************/

static void mxc_dpm_startup(void)
{
	if (!saved_loops_per_jiffy) {
		saved_loops_per_jiffy = loops_per_jiffy;
		saved_cpu_freq = clk_get_rate(cpu_clk) / 1000;
	}
	orig_idle = pm_idle;
	pm_idle = dpm_idle;
}

static void mxc_dpm_cleanup(void)
{
	pm_idle = orig_idle;
}

static int __init mxc_dpm_init(void)
{
	printk(KERN_INFO "Freescale i.MX27 Dynamic Power Management.\n");

	cpu_clk = clk_get(NULL, "cpu_clk");
	ahb_clk = clk_get(NULL, "ahb_clk");
	ipg_clk = clk_get(NULL, "ipg_clk");

	dpm_md.init_opt = mxc_dpm_init_opt;
	dpm_md.set_opt = mxc_dpm_set_opt;
	dpm_md.get_opt = mxc_dpm_get_opt;
	dpm_md.check_constraint = dpm_default_check_constraint;
	dpm_md.idle = mxc_dpm_idle;
	dpm_md.startup = mxc_dpm_startup;
	dpm_md.cleanup = mxc_dpm_cleanup;

	return 0;
}

__initcall(mxc_dpm_init);
