/* drivers/misc/lowmemorykiller.c
 *
 * The lowmemorykiller driver lets user-space specify a set of memory thresholds
 * where processes with a range of oom_score_adj values will get killed. Specify
 * the minimum oom_score_adj values in
 * /sys/module/lowmemorykiller/parameters/adj and the number of free pages in
 * /sys/module/lowmemorykiller/parameters/minfree. Both files take a comma
 * separated list of numbers in ascending order.
 *
 * For example, write "0,8" to /sys/module/lowmemorykiller/parameters/adj and
 * "1024,4096" to /sys/module/lowmemorykiller/parameters/minfree to kill
 * processes with a oom_score_adj value of 8 or higher when the free memory
 * drops below 4096 pages and kill processes with a oom_score_adj value of 0 or
 * higher when the free memory drops below 1024 pages.
 *
 * The driver considers memory used for caches to be free, but if a large
 * percentage of the cached memory is locked this can be very inaccurate
 * and processes may not get killed until the normal oom killer is triggered.
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/swap.h>
#include <linux/rcupdate.h>
#include <linux/notifier.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/swap.h>
#include <linux/fs.h>
#include <linux/cpuset.h>
#include <linux/vmpressure.h>
#include <linux/zcache.h>
#include <linux/sched/rt.h>

#define CREATE_TRACE_POINTS
#include <trace/events/almk.h>

#ifdef CONFIG_HIGHMEM
#define _ZONE ZONE_HIGHMEM
#else
#define _ZONE ZONE_NORMAL
#endif

#ifdef CONFIG_HSWAP
#include <linux/delay.h>
#include <linux/kthread.h>
#include "../../block/zram/zram_drv.h"
#endif

#define CREATE_TRACE_POINTS
#include "trace/lowmemorykiller.h"

/*
 * It's reasonable to grant the dying task an even higher priority to
 * be sure it will be scheduled sooner and free the desired pmem.
 * It was suggested using SCHED_RR:1 (the lowest RT priority),
 * so that this task won't interfere with any running RT task.
 */
static void boost_dying_task_prio(struct task_struct *p)
{
	if (!rt_task(p)) {
		struct sched_param param;
		param.sched_priority = 1;
		sched_setscheduler_nocheck(p, SCHED_RR, &param);
	}
}

static uint32_t lowmem_debug_level = 1;
static short lowmem_adj[6] = {
	0,
	1,
	6,
	12,
};
static int lowmem_adj_size = 4;
static int lowmem_minfree[6] = {
	3 * 512,	/* 6MB */
	2 * 1024,	/* 8MB */
	4 * 1024,	/* 16MB */
	16 * 1024,	/* 64MB */
};
static int lowmem_minfree_size = 4;
static int lmk_fast_run = 0;

static int lmk_kill_cnt = 0;
#ifdef CONFIG_HSWAP
static int lmk_reclaim_cnt = 0;
#endif

static unsigned long lowmem_deathpending_timeout;

#define lowmem_print(level, x...)			\
	do {						\
		if (lowmem_debug_level >= (level))	\
			pr_info(x);			\
	} while (0)

static unsigned long lowmem_count(struct shrinker *s,
				  struct shrink_control *sc)
{
	return global_page_state(NR_ACTIVE_ANON) +
		global_page_state(NR_ACTIVE_FILE) +
		global_page_state(NR_INACTIVE_ANON) +
		global_page_state(NR_INACTIVE_FILE);
}

static atomic_t shift_adj = ATOMIC_INIT(0);
static short adj_max_shift = 353;
module_param_named(adj_max_shift, adj_max_shift, short,
	S_IRUGO | S_IWUSR);

/* User knob to enable/disable adaptive lmk feature */
static int enable_adaptive_lmk;
module_param_named(enable_adaptive_lmk, enable_adaptive_lmk, int,
	S_IRUGO | S_IWUSR);

/*
 * This parameter controls the behaviour of LMK when vmpressure is in
 * the range of 90-94. Adaptive lmk triggers based on number of file
 * pages wrt vmpressure_file_min, when vmpressure is in the range of
 * 90-94. Usually this is a pseudo minfree value, higher than the
 * highest configured value in minfree array.
 */
static int vmpressure_file_min;
module_param_named(vmpressure_file_min, vmpressure_file_min, int,
	S_IRUGO | S_IWUSR);

enum {
	VMPRESSURE_NO_ADJUST = 0,
	VMPRESSURE_ADJUST_ENCROACH,
	VMPRESSURE_ADJUST_NORMAL,
};

int adjust_minadj(short *min_score_adj)
{
	int ret = VMPRESSURE_NO_ADJUST;

	if (!enable_adaptive_lmk)
		return 0;

	if (atomic_read(&shift_adj) &&
		(*min_score_adj > adj_max_shift)) {
		if (*min_score_adj == OOM_SCORE_ADJ_MAX + 1)
			ret = VMPRESSURE_ADJUST_ENCROACH;
		else
			ret = VMPRESSURE_ADJUST_NORMAL;
		*min_score_adj = adj_max_shift;
	}
	atomic_set(&shift_adj, 0);

	return ret;
}

static int lmk_vmpressure_notifier(struct notifier_block *nb,
			unsigned long action, void *data)
{
	int other_free, other_file;
	unsigned long pressure = action;
	int array_size = ARRAY_SIZE(lowmem_adj);

	if (!enable_adaptive_lmk)
		return 0;

	if (pressure >= 95) {
		other_file = global_page_state(NR_FILE_PAGES) + zcache_pages() -
			global_page_state(NR_SHMEM) -
			total_swapcache_pages();
		other_free = global_page_state(NR_FREE_PAGES);

		atomic_set(&shift_adj, 1);
		trace_almk_vmpressure(pressure, other_free, other_file);
	} else if (pressure >= 90) {
		if (lowmem_adj_size < array_size)
			array_size = lowmem_adj_size;
		if (lowmem_minfree_size < array_size)
			array_size = lowmem_minfree_size;

		other_file = global_page_state(NR_FILE_PAGES) + zcache_pages() -
			global_page_state(NR_SHMEM) -
			total_swapcache_pages();

		other_free = global_page_state(NR_FREE_PAGES);

		if ((other_free < lowmem_minfree[array_size - 1]) &&
			(other_file < vmpressure_file_min)) {
				atomic_set(&shift_adj, 1);
				trace_almk_vmpressure(pressure, other_free,
					other_file);
		}
	} else if (atomic_read(&shift_adj)) {
		/*
		 * shift_adj would have been set by a previous invocation
		 * of notifier, which is not followed by a lowmem_shrink yet.
		 * Since vmpressure has improved, reset shift_adj to avoid
		 * false adaptive LMK trigger.
		 */
		trace_almk_vmpressure(pressure, other_free, other_file);
		atomic_set(&shift_adj, 0);
	}

	return 0;
}

static struct notifier_block lmk_vmpr_nb = {
	.notifier_call = lmk_vmpressure_notifier,
};

static int test_task_flag(struct task_struct *p, int flag)
{
	struct task_struct *t;

	for_each_thread(p, t) {
		task_lock(t);
		if (test_tsk_thread_flag(t, flag)) {
			task_unlock(t);
			return 1;
		}
		task_unlock(t);
	}

	return 0;
}

static DEFINE_MUTEX(scan_mutex);

int can_use_cma_pages(gfp_t gfp_mask)
{
	int can_use = 0;
	int mtype = gfpflags_to_migratetype(gfp_mask);
	int i = 0;
	int *mtype_fallbacks = get_migratetype_fallbacks(mtype);

	if (is_migrate_cma(mtype)) {
		can_use = 1;
	} else {
		for (i = 0;; i++) {
			int fallbacktype = mtype_fallbacks[i];

			if (is_migrate_cma(fallbacktype)) {
				can_use = 1;
				break;
			}

			if (fallbacktype == MIGRATE_RESERVE)
				break;
		}
	}
	return can_use;
}

void tune_lmk_zone_param(struct zonelist *zonelist, int classzone_idx,
					int *other_free, int *other_file,
					int use_cma_pages)
{
	struct zone *zone;
	struct zoneref *zoneref;
	int zone_idx;

	for_each_zone_zonelist(zone, zoneref, zonelist, MAX_NR_ZONES) {
		zone_idx = zonelist_zone_idx(zoneref);
		if (zone_idx == ZONE_MOVABLE) {
			if (!use_cma_pages && other_free)
				*other_free -=
				    zone_page_state(zone, NR_FREE_CMA_PAGES);
			continue;
		}

		if (zone_idx > classzone_idx) {
			if (other_free != NULL)
				*other_free -= zone_page_state(zone,
							       NR_FREE_PAGES);
			if (other_file != NULL)
				*other_file -= zone_page_state(zone,
							       NR_FILE_PAGES)
					- zone_page_state(zone, NR_SHMEM)
					- zone_page_state(zone, NR_SWAPCACHE);
		} else if (zone_idx < classzone_idx) {
			if (zone_watermark_ok(zone, 0, 0, classzone_idx, 0) &&
			    other_free) {
				if (!use_cma_pages) {
					*other_free -= min(
					  zone->lowmem_reserve[classzone_idx] +
					  zone_page_state(
					    zone, NR_FREE_CMA_PAGES),
					  zone_page_state(
					    zone, NR_FREE_PAGES));
				} else {
					*other_free -=
					  zone->lowmem_reserve[classzone_idx];
				}
			} else {
				if (other_free)
					*other_free -=
					  zone_page_state(zone, NR_FREE_PAGES);
			}
		}
	}
}

#ifdef CONFIG_HIGHMEM
void adjust_gfp_mask(gfp_t *gfp_mask)
{
	struct zone *preferred_zone;
	struct zonelist *zonelist;
	enum zone_type high_zoneidx;

	if (current_is_kswapd()) {
		zonelist = node_zonelist(0, *gfp_mask);
		high_zoneidx = gfp_zone(*gfp_mask);
		first_zones_zonelist(zonelist, high_zoneidx, NULL,
				&preferred_zone);

		if (high_zoneidx == ZONE_NORMAL) {
			if (zone_watermark_ok_safe(preferred_zone, 0,
					high_wmark_pages(preferred_zone), 0,
					0))
				*gfp_mask |= __GFP_HIGHMEM;
		} else if (high_zoneidx == ZONE_HIGHMEM) {
			*gfp_mask |= __GFP_HIGHMEM;
		}
	}
}
#else
void adjust_gfp_mask(gfp_t *unused)
{
}
#endif

void tune_lmk_param(int *other_free, int *other_file, struct shrink_control *sc)
{
	gfp_t gfp_mask;
	struct zone *preferred_zone;
	struct zonelist *zonelist;
	enum zone_type high_zoneidx, classzone_idx;
	unsigned long balance_gap;
	int use_cma_pages;

	gfp_mask = sc->gfp_mask;
	adjust_gfp_mask(&gfp_mask);

	zonelist = node_zonelist(0, gfp_mask);
	high_zoneidx = gfp_zone(gfp_mask);
	first_zones_zonelist(zonelist, high_zoneidx, NULL, &preferred_zone);
	classzone_idx = zone_idx(preferred_zone);
	use_cma_pages = can_use_cma_pages(gfp_mask);

	balance_gap = min(low_wmark_pages(preferred_zone),
			  (preferred_zone->present_pages +
			   KSWAPD_ZONE_BALANCE_GAP_RATIO-1) /
			   KSWAPD_ZONE_BALANCE_GAP_RATIO);

	if (likely(current_is_kswapd() && zone_watermark_ok(preferred_zone, 0,
			  high_wmark_pages(preferred_zone) + SWAP_CLUSTER_MAX +
			  balance_gap, 0, 0))) {
		if (lmk_fast_run)
			tune_lmk_zone_param(zonelist, classzone_idx, other_free,
				       other_file, use_cma_pages);
		else
			tune_lmk_zone_param(zonelist, classzone_idx, other_free,
				       NULL, use_cma_pages);

		if (zone_watermark_ok(preferred_zone, 0, 0, _ZONE, 0)) {
			if (!use_cma_pages) {
				*other_free -= min(
				  preferred_zone->lowmem_reserve[_ZONE]
				  + zone_page_state(
				    preferred_zone, NR_FREE_CMA_PAGES),
				  zone_page_state(
				    preferred_zone, NR_FREE_PAGES));
			} else {
				*other_free -=
				  preferred_zone->lowmem_reserve[_ZONE];
			}
		} else {
			*other_free -= zone_page_state(preferred_zone,
						      NR_FREE_PAGES);
		}

		lowmem_print(4, "lowmem_shrink of kswapd tunning for highmem "
			     "ofree %d, %d\n", *other_free, *other_file);
	} else {
		tune_lmk_zone_param(zonelist, classzone_idx, other_free,
			       other_file, use_cma_pages);

		if (!use_cma_pages) {
			*other_free -=
			  zone_page_state(preferred_zone, NR_FREE_CMA_PAGES);
		}

		lowmem_print(4, "lowmem_shrink tunning for others ofree %d, "
			     "%d\n", *other_free, *other_file);
	}
}

#ifdef CONFIG_HSWAP
static bool reclaim_task_is_ok(int selected_task_anon_size)
{
	int free_size = zram0_free_size() - get_lowest_prio_swapper_space_nrpages();

	if (selected_task_anon_size < free_size)
		return true;

	return false;
}

#define OOM_SCORE_SERVICE_B_ADJ 800
#define OOM_SCORE_CACHED_APP_MIN_ADJ 900

static DEFINE_MUTEX(reclaim_mutex);

static struct completion reclaim_completion;
static struct task_struct *selected_task;

#define RESET_TIME 3600000 /* activity top time reset time(msec) */
static int reset_task_time_thread(void *p)
{
	struct task_struct *tsk;

	while (1) {
		struct task_struct *p;

		rcu_read_lock();
		for_each_process(tsk) {
			if (tsk->flags & PF_KTHREAD)
				continue;

			/* if task no longer has any memory ignore it */
			if (test_task_flag(tsk, TIF_MEMDIE))
				continue;

			if (tsk->exit_state || !tsk->mm)
				continue;

			p = find_lock_task_mm(tsk);
			if (!p)
				continue;

			if (p->signal->top_time)
				p->signal->top_time =
					(p->signal->top_time * 3) / 4;

			task_unlock(p);
		}
		rcu_read_unlock();
		msleep(RESET_TIME);
	}
	return 0;
}

static int reclaim_task_thread(void *p)
{
	int selected_tasksize;
	struct reclaim_param rp;

	init_completion(&reclaim_completion);

	while (1) {
		wait_for_completion(&reclaim_completion);

		mutex_lock(&reclaim_mutex);
		if (!selected_task)
			goto reclaim_end;

		lowmem_print(3, "hswap: scheduled reclaim task '%s'(%d), adj%hd\n",
				selected_task->comm, selected_task->pid,
				selected_task->signal->oom_score_adj);

		task_lock(selected_task);
		if (selected_task->exit_state || !selected_task->mm) {
			task_unlock(selected_task);
			put_task_struct(selected_task);
			goto reclaim_end;
		}

		selected_tasksize = get_mm_rss(selected_task->mm);
		if (!selected_tasksize) {
			task_unlock(selected_task);
			put_task_struct(selected_task);
			goto reclaim_end;
		}

		task_unlock(selected_task);

		rp = reclaim_task_file_anon(selected_task, selected_tasksize);
		lowmem_print(3, "Reclaimed '%s' (%d), adj %hd,\n" \
				"   nr_reclaimed %d\n",
			     selected_task->comm, selected_task->pid,
			     selected_task->signal->oom_score_adj,
			     rp.nr_reclaimed);
#ifdef CONFIG_HSWAP
		++lmk_reclaim_cnt;
#endif

		put_task_struct(selected_task);

reclaim_end:
		init_completion(&reclaim_completion);
		mutex_unlock(&reclaim_mutex);
	}

	return 0;
}

#define RECLAIM_TASK_CNT 100
struct task_struct* reclaim_task[RECLAIM_TASK_CNT];

static struct task_struct *find_suitable_reclaim(int reclaim_cnt,
		int *rss_size)
{
	struct task_struct *selected = NULL;
	int selected_tasksize = 0;
	int tasksize, anonsize;
	long selected_top_time = -1;
	int i = 0;

	for (i = 0; i < reclaim_cnt; i++) {
		struct task_struct *p;

		p = reclaim_task[i];

		task_lock(p);
		if (p->exit_state || !p->mm) {
			task_unlock(p);
			continue;
		}

		tasksize = get_mm_rss(p->mm);
		anonsize = get_mm_counter(p->mm, MM_ANONPAGES);
		task_unlock(p);

		if (!tasksize)
			continue;

		if (!reclaim_task_is_ok(anonsize))
			continue;

		if (selected_tasksize > tasksize)
			continue;

		selected_top_time = p->signal->top_time;
		selected_tasksize = tasksize;
		selected = p;
	}

	*rss_size = selected_tasksize;

	return selected;
}

void reclaim_arr_free(int reclaim_cnt)
{
	int i;

	for (i = 0; i < reclaim_cnt; i++)
		reclaim_task[i] = NULL;
}
#endif

static unsigned long lowmem_scan(struct shrinker *s, struct shrink_control *sc)
{
	struct task_struct *tsk;
	struct task_struct *selected = NULL;
	unsigned long rem = 0;
	int tasksize;
	int i;
	int ret = 0;
	short min_score_adj = OOM_SCORE_ADJ_MAX + 1;
	int minfree = 0;
	int selected_tasksize = 0;
	short selected_oom_score_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);
	int other_free;
	int other_file;
#ifdef CONFIG_HSWAP
	long selected_top_time = -1, cur_top_time;
	int reclaimed_cnt = 0, reclaim_cnt = 0;
	int hswap_tasksize = 0;
	int swapsize = 0, selected_swapsize = 0;
#endif

	if (mutex_lock_interruptible(&scan_mutex) < 0)
		return 0;

#ifdef CONFIG_HSWAP
	if (!mutex_trylock(&reclaim_mutex)) {
		mutex_unlock(&scan_mutex);
		return 0;
	}
	mutex_unlock(&reclaim_mutex);
#endif

	other_free = global_page_state(NR_FREE_PAGES);

#ifdef CONFIG_MIGRATE_HIGHORDER
	other_free -= global_page_state(NR_FREE_HIGHORDER_PAGES);
#endif

	if (global_page_state(NR_SHMEM) + total_swapcache_pages() <
		global_page_state(NR_FILE_PAGES) + zcache_pages())
		other_file = global_page_state(NR_FILE_PAGES) + zcache_pages() -
						global_page_state(NR_SHMEM) -
						total_swapcache_pages();
	else
		other_file = 0;

	tune_lmk_param(&other_free, &other_file, sc);

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;
	if (lowmem_minfree_size < array_size)
		array_size = lowmem_minfree_size;
	for (i = 0; i < array_size; i++) {
		minfree = lowmem_minfree[i];
		if (other_free < minfree && other_file < minfree) {
			min_score_adj = lowmem_adj[i];
			break;
		}
	}

	ret = adjust_minadj(&min_score_adj);

	lowmem_print(3, "lowmem_scan %lu, %x, ofree %d %d, ma %hd\n",
			sc->nr_to_scan, sc->gfp_mask, other_free,
			other_file, min_score_adj);

	if (min_score_adj == OOM_SCORE_ADJ_MAX + 1) {
		trace_almk_shrink(0, ret, other_free, other_file, 0);
		lowmem_print(5, "lowmem_scan %lu, %x, return 0\n",
			     sc->nr_to_scan, sc->gfp_mask);
		mutex_unlock(&scan_mutex);
		return 0;
	}

	selected_oom_score_adj = min_score_adj;

	rcu_read_lock();
	for_each_process(tsk) {
		struct task_struct *p;
		short oom_score_adj;

		if (tsk->flags & PF_KTHREAD)
			continue;

		/* if task no longer has any memory ignore it */
		if (test_task_flag(tsk, TIF_MM_RELEASED))
			continue;

		if (time_before_eq(jiffies, lowmem_deathpending_timeout)) {
			if (test_task_flag(tsk, TIF_MEMDIE)) {
				rcu_read_unlock();
				/* give the system time to free up the memory */
				msleep_interruptible(20);
#ifdef CONFIG_HSWAP
				goto end_lmk;
#endif
				mutex_unlock(&scan_mutex);
				return 0;
			}
		}

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		oom_score_adj = p->signal->oom_score_adj;
#ifdef CONFIG_HSWAP
		cur_top_time = p->signal->top_time;

		if (p->signal->reclaimed)
			reclaimed_cnt++;

		if (oom_score_adj >= OOM_SCORE_SERVICE_B_ADJ &&
				!p->signal->reclaimed) {
			if (reclaim_cnt < RECLAIM_TASK_CNT)
				reclaim_task[reclaim_cnt++] = p;
		}

		if (min_score_adj > OOM_SCORE_SERVICE_B_ADJ) {
			if (oom_score_adj <= OOM_SCORE_SERVICE_B_ADJ) {
				task_unlock(p);
				continue;
			}
		} else {
#endif
			if (oom_score_adj < min_score_adj) {
				task_unlock(p);
				continue;
			}
#ifdef CONFIG_HSWAP
		}
#endif
		tasksize = get_mm_rss(p->mm);
#ifdef CONFIG_HSWAP
		swapsize = get_mm_counter(p->mm, MM_SWAPENTS);
#endif
		task_unlock(p);
		if (tasksize <= 0)
			continue;
		if (selected) {
#ifdef CONFIG_HSWAP
			if (min_score_adj <= OOM_SCORE_SERVICE_B_ADJ) {
				if (oom_score_adj < selected_oom_score_adj)
					continue;
				if (oom_score_adj == selected_oom_score_adj &&
						tasksize <= selected_tasksize)
					continue;
			} else {
				if (selected_top_time >= 0  &&
						selected_top_time < cur_top_time)
					continue;
				if (selected_top_time == cur_top_time) {
					if (tasksize <= selected_tasksize)
						continue;
				}
			}
#else
			if (oom_score_adj < selected_oom_score_adj)
				continue;
			if (oom_score_adj == selected_oom_score_adj &&
			    tasksize <= selected_tasksize)
				continue;
#endif
		}
		selected = p;
		selected_tasksize = tasksize;
#ifdef CONFIG_HSWAP
		selected_swapsize = swapsize;
		selected_top_time = cur_top_time;
#endif
		selected_oom_score_adj = oom_score_adj;
		lowmem_print(3, "select '%s' (%d), adj %hd, size %d, to kill\n",
			     p->comm, p->pid, oom_score_adj, tasksize);
	}
	if (selected) {
		long cache_size = other_file * (long)(PAGE_SIZE / 1024);
		long cache_limit = minfree * (long)(PAGE_SIZE / 1024);
		long free = other_free * (long)(PAGE_SIZE / 1024);
		trace_lowmemory_kill(selected, cache_size, cache_limit, free);
#ifdef CONFIG_HSWAP
		if (min_score_adj < OOM_SCORE_SERVICE_B_ADJ)
			goto kill;
		else if (!reclaim_cnt && (min_score_adj > OOM_SCORE_CACHED_APP_MIN_ADJ)) {
			rcu_read_unlock();
			goto end_lmk;
		}

		if (reclaim_cnt && mutex_trylock(&reclaim_mutex)) {
			selected_task = find_suitable_reclaim(reclaim_cnt, &hswap_tasksize);
			if (selected_task) {
				unsigned long flags;

				if (lock_task_sighand(selected_task, &flags)) {
					selected_task->signal->reclaimed = 1;
					unlock_task_sighand(selected_task, &flags);
				}
				get_task_struct(selected_task);
				complete(&reclaim_completion);
				mutex_unlock(&reclaim_mutex);
				rem += hswap_tasksize;
				lowmem_print(1, "Reclaiming '%s' (%d), adj %hd, top time = %ld\n" \
						"   to free %ldkB on behalf of '%s' (%d) because\n" \
						"   cache %ldkB is below limit %ldkB for oom_score_adj %hd\n" \
						"   Free memory is %ldkB above reserved.\n",
						selected_task->comm, selected_task->pid,
						selected_task->signal->oom_score_adj, selected_task->signal->top_time,
						hswap_tasksize * (long)(PAGE_SIZE / 1024),
						current->comm, current->pid,
						other_file * (long)(PAGE_SIZE / 1024),
						minfree * (long)(PAGE_SIZE / 1024),
						min_score_adj,
						other_free * (long)(PAGE_SIZE / 1024));
				lowmem_print(3, "reclaimed cnt = %d, reclaim cont = %d, min oom score= %hd\n",
						reclaimed_cnt, reclaim_cnt, min_score_adj);
				lowmem_deathpending_timeout = jiffies + HZ;
				rcu_read_unlock();
				goto end_lmk;
			} else {
				mutex_unlock(&reclaim_mutex);
			}
		}
		selected_tasksize += selected_swapsize;
kill:
#endif
#ifndef CONFIG_HSWAP
		lowmem_print(1, "Killing '%s' (%d), adj %hd,\n"
#else
		lowmem_print(1, "Killing '%s' (%d), adj %hd, reclaimable cnt %d\n"
#endif
				"   to free %ldkB on behalf of '%s' (%d) because\n" \
				"   cache %ldkB is below limit %ldkB for oom_score_adj %hd\n" \
				"   Free memory is %ldkB above reserved.\n" \
				"   Free CMA is %ldkB\n" \
				"   Total reserve is %ldkB\n" \
				"   Total free pages is %ldkB\n" \
				"   Total file cache is %ldkB\n" \
				"   Total zcache is %ldkB\n" \
				"   GFP mask is 0x%x\n",
			     selected->comm, selected->pid,
			     selected_oom_score_adj,
#ifdef CONFIG_HSWAP
				 reclaim_cnt,
#endif
			     selected_tasksize * (long)(PAGE_SIZE / 1024),
			     current->comm, current->pid,
			     cache_size, cache_limit,
			     min_score_adj,
			     other_free * (long)(PAGE_SIZE / 1024),
			     global_page_state(NR_FREE_CMA_PAGES) *
				(long)(PAGE_SIZE / 1024),
			     totalreserve_pages * (long)(PAGE_SIZE / 1024),
			     global_page_state(NR_FREE_PAGES) *
				(long)(PAGE_SIZE / 1024),
			     global_page_state(NR_FILE_PAGES) *
				(long)(PAGE_SIZE / 1024),
			     (long)zcache_pages() * (long)(PAGE_SIZE / 1024),
			     sc->gfp_mask);

		if (lowmem_debug_level >= 2 && selected_oom_score_adj == 0) {
			show_mem(SHOW_MEM_FILTER_NODES);
			dump_tasks(NULL, NULL);
		}

		lowmem_deathpending_timeout = jiffies + HZ;
		set_tsk_thread_flag(selected, TIF_MEMDIE);
		//Improve the priority of killed process can accelerate the process to die,
		//and the process memory would be released quickly
		boost_dying_task_prio(selected);
		send_sig(SIGKILL, selected, 0);
		rem += selected_tasksize;
#ifdef CONFIG_HSWAP
		lowmem_print(3, "reclaimed cnt = %d, reclaim cont = %d, min oom score= %hd\n",
				reclaimed_cnt, reclaim_cnt, min_score_adj);
#endif
		rcu_read_unlock();
		/* give the system time to free up the memory */
		msleep_interruptible(20);
		trace_almk_shrink(selected_tasksize, ret,
			other_free, other_file, selected_oom_score_adj);
		++lmk_kill_cnt;
	} else {
		trace_almk_shrink(1, ret, other_free, other_file, 0);
		rcu_read_unlock();
	}

	lowmem_print(4, "lowmem_scan %lu, %x, return %lu\n",
		     sc->nr_to_scan, sc->gfp_mask, rem);
#ifdef CONFIG_HSWAP
end_lmk:
	reclaim_arr_free(reclaim_cnt);
#endif
	mutex_unlock(&scan_mutex);
	return rem;
}

static struct shrinker lowmem_shrinker = {
	.scan_objects = lowmem_scan,
	.count_objects = lowmem_count,
	.seeks = DEFAULT_SEEKS * 16
};

static int __init lowmem_init(void)
{
#ifdef CONFIG_HSWAP
	struct task_struct *reclaim_tsk;
	struct task_struct *reset_top_time_tsk;

	reclaim_tsk = kthread_run(reclaim_task_thread, NULL, "reclaim_task");
	reset_top_time_tsk = kthread_run(reset_task_time_thread, NULL, "reset_task");
#endif
	register_shrinker(&lowmem_shrinker);
	vmpressure_notifier_register(&lmk_vmpr_nb);
	return 0;
}

static void __exit lowmem_exit(void)
{
	unregister_shrinker(&lowmem_shrinker);
}

#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
static short lowmem_oom_adj_to_oom_score_adj(short oom_adj)
{
	if (oom_adj == OOM_ADJUST_MAX)
		return OOM_SCORE_ADJ_MAX;
	else
		return (oom_adj * OOM_SCORE_ADJ_MAX) / -OOM_DISABLE;
}

static void lowmem_autodetect_oom_adj_values(void)
{
	int i;
	short oom_adj;
	short oom_score_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;

	if (array_size <= 0)
		return;

	oom_adj = lowmem_adj[array_size - 1];
	if (oom_adj > OOM_ADJUST_MAX)
		return;

	oom_score_adj = lowmem_oom_adj_to_oom_score_adj(oom_adj);
	if (oom_score_adj <= OOM_ADJUST_MAX)
		return;

	lowmem_print(1, "lowmem_shrink: convert oom_adj to oom_score_adj:\n");
	for (i = 0; i < array_size; i++) {
		oom_adj = lowmem_adj[i];
		oom_score_adj = lowmem_oom_adj_to_oom_score_adj(oom_adj);
		lowmem_adj[i] = oom_score_adj;
		lowmem_print(1, "oom_adj %d => oom_score_adj %d\n",
			     oom_adj, oom_score_adj);
	}
}

static int lowmem_adj_array_set(const char *val, const struct kernel_param *kp)
{
	int ret;

	ret = param_array_ops.set(val, kp);

	/* HACK: Autodetect oom_adj values in lowmem_adj array */
	lowmem_autodetect_oom_adj_values();

	return ret;
}

static int lowmem_adj_array_get(char *buffer, const struct kernel_param *kp)
{
	return param_array_ops.get(buffer, kp);
}

static void lowmem_adj_array_free(void *arg)
{
	param_array_ops.free(arg);
}

static struct kernel_param_ops lowmem_adj_array_ops = {
	.set = lowmem_adj_array_set,
	.get = lowmem_adj_array_get,
	.free = lowmem_adj_array_free,
};

static const struct kparam_array __param_arr_adj = {
	.max = ARRAY_SIZE(lowmem_adj),
	.num = &lowmem_adj_size,
	.ops = &param_ops_short,
	.elemsize = sizeof(lowmem_adj[0]),
	.elem = lowmem_adj,
};
#endif

module_param_named(cost, lowmem_shrinker.seeks, int, S_IRUGO | S_IWUSR);
#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
module_param_cb(adj, &lowmem_adj_array_ops,
		.arr = &__param_arr_adj, S_IRUGO | S_IWUSR);
__MODULE_PARM_TYPE(adj, "array of short");
#else
module_param_array_named(adj, lowmem_adj, short, &lowmem_adj_size,
			 S_IRUGO | S_IWUSR);
#endif
module_param_array_named(minfree, lowmem_minfree, uint, &lowmem_minfree_size,
			 S_IRUGO | S_IWUSR);
module_param_named(debug_level, lowmem_debug_level, uint, S_IRUGO | S_IWUSR);
module_param_named(lmk_fast_run, lmk_fast_run, int, S_IRUGO | S_IWUSR);

module_param_named(lmk_kill_cnt, lmk_kill_cnt, int, S_IRUGO);
#ifdef CONFIG_HSWAP
module_param_named(lmk_reclaim_cnt, lmk_reclaim_cnt, int, S_IRUGO);
#endif

module_init(lowmem_init);
module_exit(lowmem_exit);

MODULE_LICENSE("GPL");

