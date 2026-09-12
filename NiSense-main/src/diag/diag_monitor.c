/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Diagnostics Monitor — periodic stack + heap dump.
 *
 * See diag_monitor.h for intent.
 */

#include "diag_monitor.h"
#include "ram_budget.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_heap.h>
#if defined(CONFIG_APP_FEATURE_DIAG_MONITOR)
#include <zephyr/debug/thread_analyzer.h>
#endif

#if defined(CONFIG_LVGL)
#include <lvgl.h>
#endif

LOG_MODULE_REGISTER(diag_mon, LOG_LEVEL_INF);

#ifdef CONFIG_APP_FEATURE_DIAG_MONITOR

/* Semaphore used to wake the monitor early for a forced dump. */
static K_SEM_DEFINE(diag_force_sem, 0, 1);

/* Monotonic dump counter (printed in each report header). */
static uint32_t diag_dump_seq;

/* ------------------------------------------------------------------------- */
/* Heap stats                                                                */
/* ------------------------------------------------------------------------- */

#if (CONFIG_HEAP_MEM_POOL_SIZE > 0) && defined(CONFIG_SYS_HEAP_RUNTIME_STATS)
/*
 * The Zephyr kernel defines the system malloc/calloc backing heap as
 *     K_HEAP_DEFINE(_system_heap, CONFIG_HEAP_MEM_POOL_SIZE);
 * in kernel/mempool.c. There is no public accessor, so we reach in via
 * extern. This is the same pattern used by Zephyr samples/subsys/mem.
 */
extern struct k_heap _system_heap;

static void diag_print_system_heap(void)
{
	struct sys_memory_stats stats;
	int ret = sys_heap_runtime_stats_get(&_system_heap.heap, &stats);

	if (ret != 0) {
		printk("  system heap: stats unavailable (ret=%d)\n", ret);
		return;
	}

	/* Derive total from live stats so subsystem-mandated minimums are
	 * reflected correctly (K_HEAP_MEM_POOL_SIZE may exceed
	 * CONFIG_HEAP_MEM_POOL_SIZE when other modules add requirements). */
	size_t used = stats.allocated_bytes;
	size_t peak = stats.max_allocated_bytes;
	size_t free_b = stats.free_bytes;
	size_t total = used + free_b;
	unsigned int pct_used = (total > 0U) ? (unsigned int)((used * 100U) / total) : 0U;
	unsigned int pct_peak = (total > 0U) ? (unsigned int)((peak * 100U) / total) : 0U;

	printk("  system heap (total %u B, CONFIG_HEAP_MEM_POOL_SIZE=%u):\n",
	       (unsigned int)total, (unsigned int)CONFIG_HEAP_MEM_POOL_SIZE);
	printk("    used=%u  peak=%u  free=%u  (used %u%%, peak %u%%)\n",
	       (unsigned int)used, (unsigned int)peak, (unsigned int)free_b,
	       pct_used, pct_peak);
}
#else
static inline void diag_print_system_heap(void)
{
	printk("  system heap stats disabled "
	       "(need CONFIG_SYS_HEAP_RUNTIME_STATS + HEAP_MEM_POOL_SIZE>0)\n");
}
#endif

/* ------------------------------------------------------------------------- */
/* LVGL dedicated pool (CONFIG_LV_Z_MEM_POOL_SYS_HEAP)                       */
/* ------------------------------------------------------------------------- */

#if defined(CONFIG_LVGL) && defined(CONFIG_SYS_HEAP_RUNTIME_STATS)
static void diag_print_lvgl_heap(void)
{
	lv_mem_monitor_t mon;

	lv_mem_monitor(&mon);

	printk("  LVGL pool (CONFIG_LV_Z_MEM_POOL_SIZE=%u):\n",
	       (unsigned int)CONFIG_LV_Z_MEM_POOL_SIZE);
	printk("    used=%u%%  max_used=%u  free=%u  total=%u\n",
	       (unsigned int)mon.used_pct,
	       (unsigned int)mon.max_used,
	       (unsigned int)mon.free_size,
	       (unsigned int)mon.total_size);
}
#else
static inline void diag_print_lvgl_heap(void)
{
#if defined(CONFIG_LVGL)
	printk("  LVGL pool: stats need CONFIG_SYS_HEAP_RUNTIME_STATS\n");
#else
	printk("  LVGL pool: display disabled\n");
#endif
}
#endif

/* ------------------------------------------------------------------------- */
/* Compile-time pool sizes (for cross-check with peaks above)                */
/* ------------------------------------------------------------------------- */

static void diag_print_configured_pools(void)
{
	printk("  configured sizes (Kconfig):\n");
	printk("    CONFIG_HEAP_MEM_POOL_SIZE=%u\n",
	       (unsigned int)CONFIG_HEAP_MEM_POOL_SIZE);
	printk("    CONFIG_MAIN_STACK_SIZE=%u\n",
	       (unsigned int)CONFIG_MAIN_STACK_SIZE);
#if defined(CONFIG_LV_Z_MEM_POOL_SIZE)
	printk("    CONFIG_LV_Z_MEM_POOL_SIZE=%u\n",
	       (unsigned int)CONFIG_LV_Z_MEM_POOL_SIZE);
#endif
}

/* ------------------------------------------------------------------------- */
/* One-shot dump                                                             */
/* ------------------------------------------------------------------------- */

static void diag_do_dump(void)
{
	uint32_t seq = ++diag_dump_seq;
	int64_t up_ms = k_uptime_get();

	printk("\n========== DIAG DUMP #%u  @ uptime %lld ms ==========\n",
	       (unsigned int)seq, (long long)up_ms);

	printk("-- threads (stack high-watermark + CPU %%):\n");
	thread_analyzer_print(0);

	printk("-- heap:\n");
	diag_print_system_heap();

	printk("-- LVGL:\n");
	diag_print_lvgl_heap();

	printk("-- linker RAM (static BSS/data):\n");
	ram_budget_print_linker_tail();

	printk("-- Kconfig pools:\n");
	diag_print_configured_pools();

	printk("========== END DIAG DUMP #%u ==========\n\n",
	       (unsigned int)seq);
}

void diag_monitor_dump_now(void)
{
	k_sem_give(&diag_force_sem);
}

/* ------------------------------------------------------------------------- */
/* Monitor thread                                                            */
/* ------------------------------------------------------------------------- */

static void diag_monitor_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	/* Initial delay: let boot complete and threads exercise their paths
	 * at least once so the high-watermark is meaningful. */
	if (CONFIG_DIAG_MONITOR_INITIAL_DELAY_SEC > 0) {
		k_sleep(K_SECONDS(CONFIG_DIAG_MONITOR_INITIAL_DELAY_SEC));
	}

	LOG_INF("diag monitor active: interval=%ds, initial_delay=%ds",
		CONFIG_DIAG_MONITOR_INTERVAL_SEC,
		CONFIG_DIAG_MONITOR_INITIAL_DELAY_SEC);

	while (1) {
		diag_do_dump();

		/* Sleep for the interval OR wake early on a forced dump
		 * request via diag_monitor_dump_now(). */
		(void)k_sem_take(&diag_force_sem,
				 K_SECONDS(CONFIG_DIAG_MONITOR_INTERVAL_SEC));
	}
}

K_THREAD_DEFINE(diag_monitor_tid,
		CONFIG_DIAG_MONITOR_STACK_SIZE,
		diag_monitor_thread, NULL, NULL, NULL,
		CONFIG_DIAG_MONITOR_PRIORITY,
		0, 0);

#else /* !CONFIG_APP_FEATURE_DIAG_MONITOR */

void diag_monitor_dump_now(void) { /* no-op */ }

#endif /* CONFIG_APP_FEATURE_DIAG_MONITOR */
