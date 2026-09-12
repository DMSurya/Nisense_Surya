/**
 * @file max32664_accel_feeder.c
 * @brief Host-fed accelerometer feeder for MAX32664C WHRM
 *
 * Background thread that periodically reads the LIS2DS12 accelerometer via
 * MAX32664 hub passthrough (Family 0x41 / Index 0x04 register reads) and
 * feeds the samples back to the hub input FIFO (Family 0x14 / Index 0x04).
 *
 * Why this exists:
 *   MAX32664C GWEC firmware v30.2.2 only auto-drives a KX122 on its internal
 *   sensor bus. Boards that ship with a LIS2DS12 instead (wired to the same
 *   hub sensor bus) can still run WHRM+WSpO2 by having the host read the
 *   accel and replay samples into the hub's input FIFO at 25 Hz.
 *
 * Lifecycle:
 *   - Thread is created at POST_KERNEL (like other kernel threads).
 *   - Thread blocks on `start_sem` immediately; wakes when hub enters the
 *     sampling state via max32664_accel_feeder_start(dev).
 *   - On stop, the thread loops back to waiting on `start_sem`. No thread
 *     is destroyed or recreated.
 *   - Multiple hub instances are not supported (first instance with
 *     accel_external=true wins). If that changes, replace the singleton
 *     with a per-device work queue.
 *
 * Error handling:
 *   - Transient passthrough failures (hub busy) are retried next tick.
 *   - FEEDER_CONSECUTIVE_ERR_WARN failures log a single warning.
 *   - FEEDER_CONSECUTIVE_ERR_STOP failures hard-stop the feeder (self-
 *     disable + log one error). This prevents runaway bus traffic when
 *     the hub has silently gone offline or the application forgot to
 *     call max32664_accel_feeder_stop(). The feeder can be resumed by
 *     a fresh max32664_accel_feeder_start() call.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include "max32664.h"
#include "max32664_priv.h"
#include "lis2ds12_regs.h"
#include <zephyr/drivers/sensor/max32664.h>

LOG_MODULE_REGISTER(max32664_accel_feeder, CONFIG_SENSOR_LOG_LEVEL);

#if !defined(CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER)
#error "This file should only be compiled when CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER=y"
#endif

/* Period derived from configured feed rate (Hz). 25 Hz -> 40 ms. */
#define FEEDER_PERIOD_MS \
	(1000 / CONFIG_MAX32664_EXTERNAL_ACCEL_FEED_RATE_HZ)

/* Cap on consecutive read-or-feed failures before a single warning is
 * logged (early signal that WHRM motion-rejection is degrading). */
#define FEEDER_CONSECUTIVE_ERR_WARN 3

/* Hard ceiling: after this many consecutive failures the feeder stops
 * itself, logs one error, and returns to waiting on the start semaphore.
 * At 25 Hz / 40 ms period the default (25) corresponds to ~1 s of
 * continuous I2C failures, which is a reliable signal that the hub has
 * gone offline (e.g. the application already shut it down but didn't
 * notify the feeder). Hammering further just spams the bus and the log. */
#define FEEDER_CONSECUTIVE_ERR_STOP 25
#define FEEDER_ERR_BACKOFF_MS 500

static K_SEM_DEFINE(feeder_start_sem, 0, 1);
static const struct device *feeder_hub;
static atomic_t feeder_should_run = ATOMIC_INIT(0);
static atomic_t feeder_tick_count = ATOMIC_INIT(0);

/**
 * @brief Read one LIS2DS12 sample via passthrough into raw int16 X/Y/Z.
 *
 * Reads OUT_X_L through OUT_Z_H (6 bytes). Does not check data-ready; the
 * BDU=1 setting ensures atomic L/H pair updates. Re-using the STATUS check
 * across the passthrough is expensive (2 extra I2C round trips) and WHRM
 * tolerates repeated samples better than gaps — so we always read.
 */
static int feeder_read_lis2ds12(const struct device *hub,
				int16_t *x, int16_t *y, int16_t *z)
{
	uint8_t raw[6];
	int ret;

	for (int i = 0; i < 6; i++) {
		ret = lis2ds12_read_reg(hub, LIS2DS12_REG_OUT_X_L + i, &raw[i]);
		if (ret) {
			return ret;
		}
	}

	/* 14-bit signed, left-justified in 16-bit, little-endian byte order. */
	*x = (int16_t)((raw[1] << 8) | raw[0]);
	*y = (int16_t)((raw[3] << 8) | raw[2]);
	*z = (int16_t)((raw[5] << 8) | raw[4]);
	return 0;
}

static void feeder_thread_fn(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	LOG_INF("Accel feeder thread ready (waiting for hub start)");

	while (true) {
		/* Wait until the hub signals it is in sampling state. */
		(void)k_sem_take(&feeder_start_sem, K_FOREVER);

		if (feeder_hub == NULL || !device_is_ready(feeder_hub)) {
			LOG_ERR("Feeder started without a valid hub device");
			continue;
		}

		LOG_INF("Accel feeder active @ %d Hz (period %d ms) on %s",
			CONFIG_MAX32664_EXTERNAL_ACCEL_FEED_RATE_HZ,
			FEEDER_PERIOD_MS, feeder_hub->name);

		unsigned int consecutive_err = 0;
		atomic_set(&feeder_tick_count, 0);

		while (atomic_get(&feeder_should_run)) {
			int16_t x = 0, y = 0, z = 0;
			int ret;

			/* Keep the 6 passthrough reads + 1 feed command atomic
			 * vs. the hub poll work (sysworkq). Without this lock
			 * the two threads' write/delay/read sequences interleave
			 * on the bus and both sides see spurious 0xff hub
			 * command errors.
			 */
			max32664_bus_lock(feeder_hub);
			ret = feeder_read_lis2ds12(feeder_hub, &x, &y, &z);
			if (ret == 0) {
				ret = max32664_feed_accel(feeder_hub, x, y, z);
			}
			max32664_bus_unlock(feeder_hub);

			if (ret) {
				consecutive_err++;
				if (consecutive_err == FEEDER_CONSECUTIVE_ERR_WARN) {
					LOG_WRN("Feeder: %u consecutive failures (last=%d) — "
						"continuing, WHRM motion-rejection degraded",
						consecutive_err, ret);
				}
				if (consecutive_err >= FEEDER_CONSECUTIVE_ERR_STOP) {
					/* BUGFIX: this used to permanently stop the
					 * feeder for the rest of the session, which is
					 * exactly why the accelerometer reads as a
					 * frozen constant value in field data — once
					 * this fires, the hub just keeps echoing back
					 * whatever was last successfully fed, no matter
					 * how much the wrist/finger actually moves.
					 * Back off and retry instead of stopping for
					 * good: a burst of I2C contention (seen for
					 * real as "MFIO poll timeout" during LIS2DS12
					 * setup on this same bus) is often transient,
					 * not a permanent fault. */
					LOG_WRN("Feeder: %u consecutive failures (last=%d); "
						"backing off %d ms and retrying (not stopping)",
						consecutive_err, ret, FEEDER_ERR_BACKOFF_MS);
					k_msleep(FEEDER_ERR_BACKOFF_MS);
					consecutive_err = 0;
					continue;
				}
			} else {
				consecutive_err = 0;
			}

			/* Optional diagnostic: log every Nth successful sample. */
			if (IS_ENABLED(CONFIG_LOG) &&
			    CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER_LOG_EVERY_N > 0) {
				atomic_val_t ticks = atomic_inc(&feeder_tick_count);
				if ((ticks + 1) %
				    CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER_LOG_EVERY_N == 0) {
					LOG_DBG("Feed: X=%6d Y=%6d Z=%6d (tick %ld)",
						x, y, z, (long)(ticks + 1));
				}
			}

			k_msleep(FEEDER_PERIOD_MS);
		}

		LOG_INF("Accel feeder stopped after %ld ticks",
			(long)atomic_get(&feeder_tick_count));
	}
}

K_THREAD_DEFINE(max32664_accel_feeder_tid,
		CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER_STACK_SIZE,
		feeder_thread_fn, NULL, NULL, NULL,
		CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER_PRIORITY,
		0, 0);

/* ============================================================================
 * Public control API (declared in max32664_priv.h)
 * ============================================================================ */

int max32664_accel_feeder_start(const struct device *hub)
{
	const struct max32664_config *cfg;

	if (hub == NULL || !device_is_ready(hub)) {
		return -ENODEV;
	}
	cfg = hub->config;
	if (!cfg->accel_external) {
		/* Not configured for host-fed accel — nothing to do, no error. */
		return 0;
	}

	if (atomic_get(&feeder_should_run)) {
		LOG_DBG("Accel feeder already running");
		return 0;
	}

	feeder_hub = hub;
	atomic_set(&feeder_should_run, 1);
	k_sem_give(&feeder_start_sem);
	return 0;
}

int max32664_accel_feeder_stop(const struct device *hub)
{
	ARG_UNUSED(hub); /* Singleton feeder; hub identity checked at start */

	if (!atomic_get(&feeder_should_run)) {
		return 0;
	}
	atomic_set(&feeder_should_run, 0);
	/* Thread will exit its inner loop on the next tick and re-block on
	 * feeder_start_sem. No join / no resources to free. */
	return 0;
}

bool max32664_accel_feeder_is_running(void)
{
	return atomic_get(&feeder_should_run) != 0;
}
