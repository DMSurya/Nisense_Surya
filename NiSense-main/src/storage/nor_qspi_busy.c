/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nor_qspi_busy.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(nor_qspi_busy, LOG_LEVEL_WRN);

static atomic_t qspi_busy_depth;

bool nor_qspi_busy(void)
{
	return atomic_get(&qspi_busy_depth) > 0;
}

void nor_qspi_busy_acquire(void)
{
	(void)atomic_inc(&qspi_busy_depth);
}

void nor_qspi_busy_release(void)
{
	atomic_val_t prev = atomic_dec(&qspi_busy_depth);

	if (prev <= 0) {
		/* Under-release: keep depth at 0 so UI is not stuck paused. */
		atomic_set(&qspi_busy_depth, 0);
		LOG_WRN("release with depth already 0");
	}
}
