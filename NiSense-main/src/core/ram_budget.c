/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ram_budget.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(ram_budget, LOG_LEVEL_INF);

extern char _end;

void ram_budget_log_linker_tail(void)
{
	uintptr_t ram_base = (uintptr_t)CONFIG_SRAM_BASE_ADDRESS;
	uintptr_t ram_end = ram_base + (uintptr_t)KB(CONFIG_SRAM_SIZE);
	uintptr_t heap_base = ROUND_UP((uintptr_t)&_end, 8U);
	size_t tail = (ram_end > heap_base) ? (size_t)(ram_end - heap_base) : 0U;
	size_t static_used = (size_t)(heap_base - ram_base);

	LOG_INF("RAM static: _end=%p used=%u B / %u KB SRAM, libc tail=%u B",
		&_end, (unsigned int)static_used, CONFIG_SRAM_SIZE, (unsigned int)tail);

#if defined(CONFIG_COMMON_LIBC_MALLOC) && (CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE == 0)
	if (tail < 512U) {
		LOG_WRN("Dynamic libc malloc arena <512 B — avoid FS_FATFS_LFN_MODE_HEAP "
			"(first malloc will BusFault)");
	}
#elif defined(CONFIG_COMMON_LIBC_MALLOC) && (CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE > 0)
	LOG_INF("libc malloc: static arena %u B (CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE)",
		(unsigned int)CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE);
#else
	ARG_UNUSED(tail);
#endif
}

void ram_budget_print_linker_tail(void)
{
	uintptr_t ram_base = (uintptr_t)CONFIG_SRAM_BASE_ADDRESS;
	uintptr_t ram_end = ram_base + (uintptr_t)KB(CONFIG_SRAM_SIZE);
	uintptr_t heap_base = ROUND_UP((uintptr_t)&_end, 8U);
	size_t tail = (ram_end > heap_base) ? (size_t)(ram_end - heap_base) : 0U;
	size_t static_used = (size_t)(heap_base - ram_base);

	printk("  _end=%p  static_used=%u B  libc_malloc_tail=%u B (of %u KB SRAM)\n",
	       &_end, (unsigned int)static_used, (unsigned int)tail, CONFIG_SRAM_SIZE);
#if defined(CONFIG_COMMON_LIBC_MALLOC) && (CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE == 0)
	if (tail < 512U) {
		printk("  WARNING: libc malloc arena <512 B\n");
	}
#elif defined(CONFIG_COMMON_LIBC_MALLOC) && (CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE > 0)
	printk("  libc static arena: %u B\n",
	       (unsigned int)CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE);
#endif
}
