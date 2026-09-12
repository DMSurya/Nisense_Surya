/**
 * @file max32664c.c
 * @brief MAX32664 Variant C (WHRM+WSpO2) — HUB mode only
 *
 * Built only when CONFIG_MAX32664_MODE_HUB=y. Product builds that use
 * CONFIG_MAX32664_MODE_RAW=y do NOT link this file; LED_SEQ + Family 0x12
 * sensor FIFO are owned by max32664_raw.c / max86141_green.c instead.
 *
 * Variant C (HUB mode) details:
 * - Algorithm: WHRM+WSpO2 v3x (wrist HR + SpO2)
 * - AFE: MAX86141 on SFH7074 (LED1=Green, LED2=Red, LED3=IR)
 * - Accelerometer: KX-122 hub-driven or LIS2DS12 host-fed (required)
 * - Sample rate: 25 Hz
 * - Default FIFO (output mode 0x02): 16-byte WHRM algo record (HR/SpO2/…),
 *   not raw IR/Red/Green words. Mode 0x03 prepends a sensor blob whose
 *   exposure order is whatever WHRM programmed — do not assume product SEQ.
 *
 * LED sequence: after WHRM enable the hub owns MAX86141 LED_SEQ/PA. Idle/AFE
 * recovery in max32664_runtime.c re-applies product SEQ IR→Red→Green
 * (MAX86141_LED_SEQ*_PRODUCT) + SFH7074 dual-PD bias when the path is dark.
 *
 * Critical: accelerometer must be enabled for motion rejection.
 * Firmware: v30.x GWEC often pre-enables AFE/algo (status 0x02 on enable).
 */

#include "max32664_priv.h"
#include "lis2ds12_regs.h"
#include "max86141_regs.h"

LOG_MODULE_DECLARE(max32664, CONFIG_SENSOR_LOG_LEVEL);

/* Forward declarations */
static void max32664c_dump_registers(const struct device *dev);
#if IS_ENABLED(CONFIG_MAX32664_LED_SELF_TEST)
static void max32664c_led_self_test(const struct device *dev);
#endif

/**
 * @brief Configure LIS2DS12 for host-fed WHRM (accel-source = "external").
 *
 * Used when the hub firmware (v30.2.2 GWEC) only knows how to drive a KX122
 * on its internal bus but the board ships with a LIS2DS12. In this mode the
 * hub never touches the accel itself — nRF configures and reads it via
 * Family 0x40 / 0x41 passthrough, then feeds samples back via 0x14 / 0x04.
 *
 * Settings chosen to match WHRM's expectation of 25 Hz samples at ±2 g:
 *   CTRL1 = ODR(25 Hz) | FS(±2 g) | BDU=1
 *
 * Does not pulse reset — assumes the chip came up from power-on defaults.
 */
static int max32664c_lis2ds12_configure_for_host_feed(const struct device *dev)
{
	uint8_t whoami = 0;
	uint8_t ctrl1;
	int ret;

	ret = lis2ds12_read_reg(dev, LIS2DS12_REG_WHO_AM_I, &whoami);
	if (ret) {
		LOG_ERR("External accel: LIS2DS12 WHOAMI read failed: %d", ret);
		return ret;
	}
	if (whoami != LIS2DS12_WHOAMI_VALUE && whoami != LIS2DS12_WHOAMI_ALT) {
		LOG_WRN("External accel: unexpected WHOAMI 0x%02x (expected 0x43/0x1D)",
			whoami);
	} else {
		LOG_INF("External accel: LIS2DS12 WHOAMI=0x%02x via passthrough", whoami);
	}

	ctrl1 = (LIS2DS12_ODR_25HZ << LIS2DS12_ODR_POS) |
		(LIS2DS12_FS_2G    << LIS2DS12_FS_POS)  |
		(1                 << LIS2DS12_BDU_POS);
	ret = lis2ds12_write_reg(dev, LIS2DS12_REG_CTRL1, ctrl1);
	if (ret) {
		LOG_ERR("External accel: CTRL1 write failed: %d", ret);
		return ret;
	}
	ret = lis2ds12_write_reg(dev, LIS2DS12_REG_CTRL2, 0x00);
	if (ret) {
		LOG_WRN("External accel: CTRL2 write failed: %d (continuing)", ret);
	}

	LOG_INF("External accel: LIS2DS12 configured @ 25 Hz, ±2 g, BDU=1");
	return 0;
}

#if IS_ENABLED(CONFIG_MAX32664_LED_SELF_TEST)
/**
 * @brief One-shot MAX86141 LED self-test (bring-up diagnostic)
 *
 * Runs AFTER the AFE has been enabled (Step 1 of variant-C init) and BEFORE
 * the WHRM+WSpO2 algorithm is enabled (Step 3). In that window the MAX86141
 * sequencer is already running, but the hub firmware is not yet stomping
 * LED_PA / LED_SEQ on every tick — so direct passthrough writes take effect
 * long enough to be visible.
 *
 * Sequence (each LED held for CONFIG_MAX32664_LED_SELF_TEST_MS):
 *   1. LED_SEQ1 = 0x01, LED1_PA = drive, others = 0 → Green should glow
 *   2. LED_SEQ1 = 0x02, LED2_PA = drive, others = 0 → Red should glow
 *   3. LED_SEQ1 = 0x03, LED3_PA = drive, others = 0 → IR (phone camera only)
 *
 * Original LED_SEQ1 / LED1_PA / LED2_PA / LED3_PA are captured before the
 * test and written back afterwards so the WHRM hand-off sees the firmware's
 * preset configuration untouched.
 *
 * SFH7074 wiring on this board (verified):
 *   LED1 = Green (SFH pin 2), LED2 = Red (SFH pin 3), LED3 = IR (SFH pin 8)
 */
static void max32664c_led_self_test(const struct device *dev)
{
	const uint32_t dwell_ms = CONFIG_MAX32664_LED_SELF_TEST_MS;
	const uint8_t  drive    = (uint8_t)CONFIG_MAX32664_LED_SELF_TEST_DRIVE;
	uint8_t saved_seq1 = 0, saved_pa1 = 0, saved_pa2 = 0, saved_pa3 = 0;
	uint8_t saved_range1 = 0;
	uint8_t readback = 0;
	int ret;

	LOG_INF("==== MAX86141 LED SELF-TEST BEGIN (dwell=%u ms, drive=0x%02x) ====",
		dwell_ms, drive);
	LOG_INF("Wiring (SFH7074): LED1=Green, LED2=Red, LED3=IR");

	/* Snapshot current firmware-programmed values so we can restore them. */
	(void)max32664_read_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
				       MAX86141_REG_LED_SEQ1,     &saved_seq1);
	(void)max32664_read_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
				       MAX86141_REG_LED1_DRV_CUR, &saved_pa1);
	(void)max32664_read_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
				       MAX86141_REG_LED2_DRV_CUR, &saved_pa2);
	(void)max32664_read_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
				       MAX86141_REG_LED3_DRV_CUR, &saved_pa3);
	(void)max32664_read_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
				       MAX86141_REG_LED_RANGE1,   &saved_range1);
	LOG_INF("Saved: LED_SEQ1=0x%02x LED1_PA=0x%02x LED2_PA=0x%02x LED3_PA=0x%02x "
		"LED_RANGE1=0x%02x",
		saved_seq1, saved_pa1, saved_pa2, saved_pa3, saved_range1);

	/* Force LED_RANGE1 to maximum scale (0b1111 1111 → all channels 124 mA FS)
	 * for the duration of the self-test so that a 0xFF LEDn_PA setting maps to
	 * the highest achievable optical output. WHRM firmware's preset value is
	 * restored at the end — this write only influences the visual dwell. */
	ret = max32664_write_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
					MAX86141_REG_LED_RANGE1, 0xFF);
	if (ret) {
		LOG_WRN("Self-test: LED_RANGE1<=0xFF write failed: %d (continuing "
			"with firmware preset 0x%02x)", ret, saved_range1);
	} else {
		(void)max32664_read_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
					       MAX86141_REG_LED_RANGE1, &readback);
		LOG_INF("Self-test: LED_RANGE1<=0xFF (readback=0x%02x)", readback);
	}

	/* Per-LED drive table: {label, seq1_value, pa_reg} */
	static const struct {
		const char *label;
		uint8_t     seq1;
		uint8_t     pa_reg;
	} steps[] = {
		{ "GREEN (LED1)", 0x01, MAX86141_REG_LED1_DRV_CUR },
		{ "RED   (LED2)", 0x02, MAX86141_REG_LED2_DRV_CUR },
		{ "IR    (LED3)", 0x03, MAX86141_REG_LED3_DRV_CUR },
	};

	for (size_t i = 0; i < ARRAY_SIZE(steps); i++) {
		/* Park all LED drives at 0 before switching slot assignment so
		 * we never get a brief cross-illumination if the sequencer
		 * latches between writes. */
		(void)max32664_write_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
						MAX86141_REG_LED1_DRV_CUR, 0x00);
		(void)max32664_write_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
						MAX86141_REG_LED2_DRV_CUR, 0x00);
		(void)max32664_write_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
						MAX86141_REG_LED3_DRV_CUR, 0x00);

		ret = max32664_write_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
						MAX86141_REG_LED_SEQ1,
						steps[i].seq1);
		if (ret) {
			LOG_ERR("Self-test: LED_SEQ1<=0x%02x failed: %d",
				steps[i].seq1, ret);
			continue;
		}

		ret = max32664_write_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
						steps[i].pa_reg, drive);
		if (ret) {
			LOG_ERR("Self-test: %s PA write failed: %d",
				steps[i].label, ret);
			continue;
		}

		/* Read back the PA + LED_SEQ1 we just wrote so the log PROVES the
		 * MAX86141 accepted the config. A mismatch here means the chip is
		 * refusing the write (e.g. SHDN bit, bus corruption) — totally
		 * different failure mode from "write OK but no glow" (wire/LED
		 * open). This is the key forensic datum when a stage appears dark. */
		uint8_t rb_pa = 0xEE, rb_seq = 0xEE;
		(void)max32664_read_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
					       steps[i].pa_reg, &rb_pa);
		(void)max32664_read_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
					       MAX86141_REG_LED_SEQ1, &rb_seq);

		LOG_INF(">>> %s ON for %u ms (wrote PA=0x%02x/SEQ1=0x%02x, "
			"readback PA=0x%02x SEQ1=0x%02x) — expect glow on module",
			steps[i].label, dwell_ms, drive, steps[i].seq1,
			rb_pa, rb_seq);

		if (rb_pa != drive) {
			LOG_ERR(">>> %s readback MISMATCH: wrote 0x%02x, read 0x%02x "
				"— MAX86141 refused the write (chip-level issue, "
				"NOT wiring)", steps[i].label, drive, rb_pa);
		} else if (rb_seq != steps[i].seq1) {
			LOG_ERR(">>> %s SEQ1 readback MISMATCH: wrote 0x%02x, read 0x%02x",
				steps[i].label, steps[i].seq1, rb_seq);
		}

		k_msleep(dwell_ms);
	}

	/* Extinguish, then restore firmware presets so Step 3 (WHRM enable)
	 * takes over from a known state. */
	(void)max32664_write_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
					MAX86141_REG_LED1_DRV_CUR, 0x00);
	(void)max32664_write_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
					MAX86141_REG_LED2_DRV_CUR, 0x00);
	(void)max32664_write_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
					MAX86141_REG_LED3_DRV_CUR, 0x00);

	(void)max32664_write_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
					MAX86141_REG_LED_RANGE1,   saved_range1);
	(void)max32664_write_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
					MAX86141_REG_LED_SEQ1,     saved_seq1);
	(void)max32664_write_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
					MAX86141_REG_LED1_DRV_CUR, saved_pa1);
	(void)max32664_write_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
					MAX86141_REG_LED2_DRV_CUR, saved_pa2);
	(void)max32664_write_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
					MAX86141_REG_LED3_DRV_CUR, saved_pa3);

	LOG_INF("==== MAX86141 LED SELF-TEST END — presets restored ====");
}
#endif /* CONFIG_MAX32664_LED_SELF_TEST */

/* ============================================================================
 * Variant C Initialization
 * ============================================================================ */

/**
 * @brief Initialize Variant C (WHRM+WSpO2)
 *
 * Initialization sequence:
 * 1. Enable MAX86141 AFE (PPG sensor)
 * 2. Enable KX-122/LIS2DS12 accelerometer (MANDATORY)
 * 3. Enable WHRM algorithm
 * 4. Configure WHRM mode (motion rejection level)
 * 5. Set output mode to PAUSE (wait for explicit start)
 *
 * @param dev Pointer to device structure
 * @return 0 on success, negative errno on failure
 */
static int max32664c_init(const struct device *dev)
{
	struct max32664_data *data = dev->data;
	const struct max32664_config *cfg = dev->config;
	uint8_t tx[8], rx[8];
	int ret;

	LOG_INF("Initializing Variant C (MAX86141 + %s + WHRM)...",
		cfg->accel_external ? "LIS2DS12 host-fed" : "KX-122 hub-driven");

	/* ──────────────────────────────────────────────────────────────────
	 * Step 1: Enable MAX86141 AFE (PPG sensor)
	 * ────────────────────────────────────────────────────────────────── */
	LOG_INF("Step 1: Enabling MAX86141 AFE...");
	/* Per Maxim UG "Measuring SpO2 and HR using MAX32664C" Table 14 step 1.3:
	 *   AA 44 00 01 00  → enable AFE with sensor-hub-side samples.
	 * The 4th byte (0x00) is the data-source selector: 0x00 = hub-side,
	 * 0x01 = host-side. MAX86141 is always driven by the hub, so 0x00.
	 */
	tx[0] = 0x44; /* Family: Enable Sensor */
	tx[1] = 0x00; /* Index:  MAX86141 (AFE) */
	tx[2] = 0x01; /* Enable */
	tx[3] = 0x00; /* Data source: hub-side */

	/* Use retry - AFE enable may take time, hub may be busy (especially v40.x) */
	ret = max32664_i2c_transmit_retry(dev, tx, 4, rx, 1, 250, 3);
	if (ret) {
		LOG_ERR("Failed to enable MAX86141 AFE: %d (status=0x%02x)", ret, rx[0]);
		return ret;
	}
	LOG_INF("MAX86141 AFE enabled, status=0x%02x", rx[0]);
	k_msleep(50); /* AFE stabilization */

	/* ──────────────────────────────────────────────────────────────────
	 * Step 2: Accelerometer
	 *
	 * Per Maxim UG "Measuring SpO2 and HR using MAX32664C" Table 14,
	 * step 1.4, the accel-enable command is 4 bytes and the LAST byte
	 * selects where samples come from:
	 *   AA 44 04 01 00  → hub-side accel  (hub auto-polls KX122 over SPI)
	 *   AA 44 04 01 01  → host-side accel (hub expects WRITE_INPUT feeds)
	 *
	 * We pick the mode based on DTS accel-source:
	 *  - "internal": hub drives its own KX122/LIS2DS12 over SPI (stock path)
	 *  - "external": nRF reads LIS2DS12 via passthrough and feeds samples
	 *                back via Family 0x14 / Index 0x00 (WRITE_EXTERNAL_TO_FIFO)
	 *                / SubIdx 0x04 (accel) — see max32664_feed_accel().
	 *                REQUIRES the 0x44/0x04/0x01/0x01 command below,
	 *                otherwise the hub refuses WRITE_INPUT with status 0x01.
	 * ────────────────────────────────────────────────────────────────── */
	if (cfg->accel_external) {
		LOG_INF("Step 2: External accel mode — configuring LIS2DS12 via passthrough "
			"(hub self-polling DISABLED)");

		ret = max32664c_lis2ds12_configure_for_host_feed(dev);
		if (ret) {
			LOG_WRN("External accel setup failed: %d — WHRM will run without "
				"motion rejection until feeder recovers", ret);
			data->accel_enabled = false;
			data->accel_verified = false;
		} else {
			data->accel_enabled = true;
			data->accel_verified = true;
		}
		data->accel_required = false; /* non-fatal: algo still produces HR */

		/* Tell the hub that accel samples will be host-fed. This is what
		 * opens the WRITE_INPUT (0x14 0x00 0x04) pathway for the feeder.
		 * Without this the hub returns status 0x01 (ERR_UNAVAIL_CMD) to
		 * every feed tick even though the byte format is correct.
		 */
		LOG_INF("Step 2b: Enabling host-side accel feed (0x44 0x04 0x01 0x01)...");
		tx[0] = 0x44; /* Family: Enable Sensor */
		tx[1] = 0x04; /* Index:  Accelerometer */
		tx[2] = 0x01; /* Enable */
		tx[3] = 0x01; /* Data source: host-side (WRITE_INPUT feed) */

		ret = max32664_i2c_transmit_retry(dev, tx, 4, rx, 1, 250, 3);
		if (ret || rx[0] != MAX32664_STATUS_SUCCESS) {
			LOG_WRN("Host-side accel enable failed: ret=%d status=0x%02x — "
				"WHRM will reject WRITE_INPUT; motion rejection disabled",
				ret, rx[0]);
			/* Not fatal; PPG path may still produce HR without accel,
			 * and we want to keep the rest of init going so diagnostics
			 * still come out.
			 */
		} else {
			LOG_INF("Host-side accel feed enabled, hub status=0x%02x", rx[0]);
		}
		k_msleep(50);
	} else {
		LOG_INF("Step 2: Enabling KX-122/LIS2DS12 accelerometer (MANDATORY for variant C)...");
		/* Hub-side accel: fourth byte 0x00 = hub drives sensor itself. */
		tx[0] = 0x44; /* Family: Enable Sensor */
		tx[1] = 0x04; /* Index:  Accelerometer */
		tx[2] = 0x01; /* Enable */
		tx[3] = 0x00; /* Data source: hub-side (auto-poll) */

		ret = max32664_i2c_transmit_retry(dev, tx, 4, rx, 1, 100, 3);
		if (ret) {
			LOG_ERR("FATAL: Failed to enable accelerometer: %d (status=0x%02x)",
				ret, rx[0]);
			LOG_ERR("Variant C REQUIRES accelerometer for WHRM algorithm");
			LOG_ERR("Check KX-122 wiring, power, and hub firmware configuration");
			return -ENODEV;
		}

		LOG_INF("Accelerometer enabled, status=0x%02x", rx[0]);
		data->accel_enabled = true;
		k_msleep(50);

		/* Verify accelerometer is actually responding by reading WHOAMI */
		tx[0] = 0x41; /* Family: Read Sensor Register */
		tx[1] = 0x04; /* Index: Accelerometer */
		tx[2] = 0x0F; /* Register: WHOAMI */
		ret = max32664_i2c_transmit_retry(dev, tx, 3, rx, 2, 10, 3);
		if (ret == 0 && rx[0] == 0x00) {
			if (rx[1] == 0x43) {
				LOG_INF("Accelerometer verified: LIS2DS12 WHOAMI=0x%02x", rx[1]);
				data->accel_verified = true;
			} else if (rx[1] == 0x1D) {
				LOG_INF("Accelerometer verified: KX-122 WHOAMI=0x%02x", rx[1]);
				data->accel_verified = true;
			} else if (rx[1] != 0x00) {
				LOG_INF("Accelerometer WHOAMI=0x%02x (unknown type, may work)",
					rx[1]);
				data->accel_verified = true;
			} else {
				LOG_WRN("Accelerometer WHOAMI=0x00 - sensor may not be connected!");
				LOG_WRN("WHRM algorithm requires accelerometer - trying sensor-only mode");
				data->accel_verified = false;
			}
		} else {
			LOG_WRN("Failed to read accelerometer WHOAMI: ret=%d, status=0x%02x",
				ret, rx[0]);
			LOG_WRN("WHRM algorithm requires accelerometer - trying sensor-only mode");
			data->accel_verified = false;
		}
	}

	/* Optional bring-up diagnostic: visually verify each LED wiring by
	 * driving LED1/LED2/LED3 in turn BEFORE the WHRM algorithm takes over
	 * the LED drive registers. Gated behind CONFIG_MAX32664_LED_SELF_TEST
	 * so it never runs in production builds. */
#if IS_ENABLED(CONFIG_MAX32664_LED_SELF_TEST)
	max32664c_led_self_test(dev);
#endif

	/* Dump sensor registers for diagnostics */
	max32664c_dump_registers(dev);

	/* ──────────────────────────────────────────────────────────────────
	 * Step 3: Enable WHRM+WSpO2 Algorithm
	 * Index 0x02 = WHRM+WSpO2 (combined HR + SpO2 for wrist)
	 * ────────────────────────────────────────────────────────────────── */
	LOG_INF("Step 3: Enabling WHRM+WSpO2 algorithm (Index 0x02)...");
	tx[0] = 0x52; /* Family: Algorithm Enable */
	tx[1] = 0x02; /* Index: WHRM+WSpO2 (Variant C algorithm) */
	tx[2] = 0x01; /* Enable */

	/* Use retry - algorithm enable may take time, hub may be busy */
	ret = max32664_i2c_transmit_retry(dev, tx, 3, rx, 1, 500, 3);
	if (ret == 0 && rx[0] == 0x00) {
		LOG_INF("WHRM+WSpO2 algorithm enabled successfully");
	} else if (rx[0] == 0x02) {
		/* Status 0x02 = already enabled or invalid parameter (v30.x firmware) */
		LOG_INF("WHRM+WSpO2 algorithm pre-enabled by firmware (status=0x%02x)", rx[0]);
	} else {
		LOG_WRN("WHRM+WSpO2 algorithm enable status: 0x%02x (ret=%d) - continuing", rx[0], ret);
	}

	/* ──────────────────────────────────────────────────────────────────
	 * Step 4: Configure WHRM Mode (Motion Rejection)
	 * ────────────────────────────────────────────────────────────────── */
	LOG_INF("Step 4: Configuring WHRM mode with motion rejection...");
	ret = max32664_configure_whrm_mode(dev, MAX32664_WHRM_MODE_STANDARD);
	if (ret) {
		LOG_WRN("Failed to set WHRM mode: %d - using default", ret);
		/* Non-fatal - algorithm will work with default mode */
	}

	/* ──────────────────────────────────────────────────────────────────
	 * Step 5: Set Output Mode to PAUSE
	 * ────────────────────────────────────────────────────────────────── */
	LOG_INF("Step 5: Setting output mode to PAUSE (0x00) - ready for start command...");
	tx[0] = 0x10; /* Family: Output Mode */
	tx[1] = 0x00; /* Index: Set mode */
	tx[2] = 0x00; /* Value: 0x00 = Pause (no output) */

	ret = max32664_i2c_transmit_retry(dev, tx, 3, rx, 1, 10, 3);
	if (ret || rx[0] != 0x00) {
		LOG_ERR("Failed to set pause mode: %d (status=0x%02x)", ret, rx[0]);
		return ret ? ret : -EIO;
	}

	k_msleep(50);
	LOG_INF("Variant C initialized - hub in PAUSE mode");
	LOG_INF("Call sensor_attr_set() to start sampling (25Hz, 16-byte FIFO entries)");

	return 0;
}

/* ============================================================================
 * Variant C Register Dump
 * ============================================================================ */

/**
 * @brief Read a block of sensor registers via passthrough into a buffer.
 *
 * Used by the extended dump below. Unlike the hub's built-in 0x43 dump (which
 * returns whatever address/value pairs firmware chooses — typically 0x00–0x16
 * on MAX86141 and 0x00–0x0E on LIS2DS12), this reads a *specific* contiguous
 * range via Family 0x41. Failures are recorded as 0xFF sentinels so the hex
 * line still renders and the caller can tell which addresses didn't respond.
 *
 * Cost: one passthrough transaction per byte (~10 ms on v30.x, ~50 ms on v40.x
 * per max32664_read_sensor_reg's per-firmware tuning). Intended for one-shot
 * use at init, never in the hot path.
 */
static void max32664c_read_reg_block(const struct device *dev,
				     uint8_t sensor_idx,
				     uint8_t start_reg,
				     uint8_t *buf, uint8_t count)
{
	for (uint8_t i = 0; i < count; i++) {
		if (max32664_read_sensor_reg(dev, sensor_idx,
					     start_reg + i, &buf[i]) != 0) {
			buf[i] = 0xFF; /* sentinel — register didn't respond */
		}
	}
}

/**
 * @brief Dump Variant C sensor registers for diagnostics
 *
 * Two-pass dump:
 *   1. Hub-driven Family 0x43 dump (coarse — addresses chosen by firmware,
 *      usually the lower ~16 registers of each sensor).
 *   2. Explicit passthrough reads of the register ranges the firmware dump
 *      skips and that matter for HR/SpO2 operation:
 *        MAX86141  0x20..0x2B  — LED sequencing + LED1..LED6 drive + range.
 *                               An all-zero drive region means the AFE isn't
 *                               actually driving any LED, which is one of the
 *                               reasons WHRM produces no algorithm output.
 *        LIS2DS12  0x20..0x27  — CTRL1..CTRL5 + FIFO_CTRL + STATUS.
 *                               CTRL1 must read back with ODR=0x3 (25 Hz),
 *                               FS=0x0 (±2 g) and BDU=1 — otherwise the
 *                               passthrough write at init silently failed.
 *        LIS2DS12  0x28..0x2D  — OUT_X/Y/Z — a quick sanity check that the
 *                               accel is actually producing samples.
 *
 * Runs only at init and tolerates individual register-read failures (0xFF
 * sentinel) so one bad register never suppresses the rest of the dump.
 */
static void max32664c_dump_registers(const struct device *dev)
{
	uint8_t tx[2];
	uint8_t dump_buf[33]; /* 1 status + 32 bytes (16 addr-value pairs) */
	uint8_t block[14];
	int ret;

	/* ─── Pass 1: hub-driven coarse dump (unchanged) ───────────────── */

	tx[0] = 0x43; /* Family: Dump Registers */
	tx[1] = 0x00; /* Index: MAX86141 */
	ret = max32664_i2c_transmit(dev, tx, 2, dump_buf, 33, 50);
	if (ret == 0 && dump_buf[0] == 0x00) {
		LOG_HEXDUMP_INF(&dump_buf[1], 32, "MAX86141 Register Dump:");
	} else {
		LOG_WRN("Failed to dump MAX86141 registers: %d (status=0x%02x)", ret, dump_buf[0]);
	}

	tx[0] = 0x43; /* Family: Dump Registers */
	tx[1] = 0x04; /* Index: Accelerometer */
	ret = max32664_i2c_transmit(dev, tx, 2, dump_buf, 33, 50);
	if (ret == 0 && dump_buf[0] == 0x00) {
		LOG_HEXDUMP_INF(&dump_buf[1], 32, "LIS2DS12/KX-122 Accelerometer Dump:");
	} else {
		LOG_WRN("Failed to dump accelerometer registers: %d (status=0x%02x)", ret, dump_buf[0]);
	}

	/* ─── Pass 2: explicit passthrough reads for the registers that
	 * matter to HR/SpO2 but are outside the hub's 0x43 dump window. ── */

	/* MAX86141 0x20..0x2B: LED sequencing + drive currents + range.
	 * Layout: [LED_SEQ1 LED_SEQ2 LED_SEQ3 LED1_DRV LED2_DRV LED3_DRV
	 *          LED4_DRV LED5_DRV LED6_DRV LEDPILOT_PA LED_RANGE1 LED_RANGE2]. */
	max32664c_read_reg_block(dev, HUB_SENSOR_IDX_MAX86141,
				 MAX86141_REG_LED_SEQ1, block, 12);
	LOG_HEXDUMP_INF(block, 12, "MAX86141 0x20..0x2B LED_SEQ/LED_PA/LED_RANGE:");
	LOG_INF("  LED drive currents: LED1=0x%02x LED2=0x%02x LED3=0x%02x "
		"LED4=0x%02x LED5=0x%02x LED6=0x%02x",
		block[3], block[4], block[5], block[6], block[7], block[8]);
	if ((block[3] | block[4] | block[5]) == 0x00) {
		LOG_WRN("  MAX86141: LED1..LED3 drive current is ZERO — PPG has no light source");
	}

	/* LIS2DS12 0x20..0x27: CTRL1..CTRL5, FIFO_CTRL, 0x26 reserved, STATUS. */
	max32664c_read_reg_block(dev, HUB_SENSOR_IDX_LIS2DS12,
				 LIS2DS12_REG_CTRL1, block, 8);
	LOG_HEXDUMP_INF(block, 8, "LIS2DS12 0x20..0x27 CTRL1..CTRL5/FIFO_CTRL/rsvd/STATUS:");
	{
		uint8_t ctrl1 = block[0];
		uint8_t odr_code = (ctrl1 >> LIS2DS12_ODR_POS) & 0x0F;
		uint8_t fs_code  = (ctrl1 >> LIS2DS12_FS_POS)  & 0x03;
		uint8_t bdu      = (ctrl1 >> LIS2DS12_BDU_POS) & 0x01;
		LOG_INF("  CTRL1=0x%02x decoded: ODR=0x%x FS=0x%x BDU=%u "
			"(expected ODR=0x3/25Hz FS=0x0/±2g BDU=1)",
			ctrl1, odr_code, fs_code, bdu);
		if (ctrl1 == 0x00 || ctrl1 == 0xFF) {
			LOG_WRN("  LIS2DS12: CTRL1 reads as 0x%02x — passthrough "
				"write at init did not stick, accel is in power-down",
				ctrl1);
		}
	}

	/* LIS2DS12 0x28..0x2D: OUT_X/Y/Z. One-shot peek — if these are all
	 * zero with CTRL1 programmed, the accel chip is configured but not
	 * producing output (data-ready never sets). */
	max32664c_read_reg_block(dev, HUB_SENSOR_IDX_LIS2DS12,
				 LIS2DS12_REG_OUT_X_L, block, 6);
	LOG_HEXDUMP_INF(block, 6, "LIS2DS12 0x28..0x2D OUT_X/Y/Z:");
	{
		int16_t x = (int16_t)((block[1] << 8) | block[0]);
		int16_t y = (int16_t)((block[3] << 8) | block[2]);
		int16_t z = (int16_t)((block[5] << 8) | block[4]);
		LOG_INF("  OUT raw: X=%6d Y=%6d Z=%6d", x, y, z);
	}
}

/* ============================================================================
 * Variant C FIFO Parsing
 * ============================================================================ */

/**
 * @brief Heuristic: does a 16-byte slice look like GWEC WHRM output (BE)?
 *
 * Used when output mode 0x03 is enabled: firmware builds differ on whether
 * the 16-byte algorithm record is last, first, or embedded in a larger blob.
 * Reject obvious garbage (e.g. HR 1800 bpm, confidence 250%).
 */
static bool max32664c_whrm16_plausible(const uint8_t *p)
{
	uint16_t hr_raw = ((uint16_t)p[0] << 8) | p[1];
	uint8_t hr_conf = p[2];
	uint16_t spo2_raw = ((uint16_t)p[6] << 8) | p[7];
	uint8_t spo2_conf = p[8];

	if (hr_conf > 100U || spo2_conf > 100U) {
		return false;
	}

	const bool hr_ok =
		(hr_raw == 0U && hr_conf == 0U) ||
		(hr_raw >= 300U && hr_raw <= 2500U) ||
		(hr_raw >= 550U && hr_raw <= 650U);
	const bool spo2_ok =
		(spo2_raw == 0U && spo2_conf == 0U) ||
		(spo2_raw >= 700U && spo2_raw <= 1000U) ||
		(spo2_raw >= 580U && spo2_raw <= 620U);

	return hr_ok && spo2_ok;
}

/** @return byte offset of first plausible WHRM block, or (size_t)-1 */
static size_t max32664c_find_whrm16_offset(const uint8_t *buf, size_t len)
{
	if (len < 16U) {
		return (size_t)-1;
	}
	for (size_t off = 0; off <= len - 16U; off++) {
		if (max32664c_whrm16_plausible(&buf[off])) {
			return off;
		}
	}
	return (size_t)-1;
}

/**
 * @brief Parse Variant C FIFO data (16 B algo-only, or extended mode 0x03)
 *
 * Output mode 0x02: each FIFO entry is exactly 16 bytes of WHRM fields
 * (HR/SpO2/RR/confidence/SCD). This is NOT the RAW IR/Red/Green layout.
 *
 * Output mode 0x03: hub prepends sensor bytes; layout varies across GWEC
 * 30.x builds. PPG words in the prefix follow the hub's exposure order
 * (WHRM may differ from product LED_SEQ IR→Red→Green). We scan for a
 * plausible 16-byte WHRM record. If none is found, publish Maxim idle
 * placeholder once and force mode 0x02.
 */
static int max32664c_parse_fifo(const struct device *dev, const uint8_t *fifo_data, size_t len)
{
	struct max32664_data *data = dev->data;
	const uint8_t *algo = fifo_data;
	int32_t accel_x = 0, accel_y = 0, accel_z = 0;
	uint32_t ppg_s1_pd1 = 0, ppg_s1_pd2 = 0, ppg_s2_pd1 = 0, ppg_s2_pd2 = 0;
	bool have_sensor_prefix = false;
	size_t whrm_off = 0;

	if (len == 16U) {
		/* Algorithm-data-only (output mode 0x02). */
	} else if (len >= 17U) {
		have_sensor_prefix = true;
		whrm_off = max32664c_find_whrm16_offset(fifo_data, len);

		if (whrm_off != (size_t)-1) {
			algo = fifo_data + whrm_off;
			/* Legacy decode path when the 16 B WHRM block sits after 18 B prefix
			 * (12 B PPG + 6 B accel) — still useful on builds that match it. */
			if (whrm_off >= 18U) {
				const uint8_t *sensor = fifo_data;
				size_t sensor_len = whrm_off;

				if (sensor_len >= 3U) {
					ppg_s1_pd1 = (((uint32_t)sensor[0] << 16) |
						      ((uint32_t)sensor[1] << 8) |
						       (uint32_t)sensor[2]) &
						     0x000FFFFFU;
				}
				if (sensor_len >= 6U) {
					ppg_s1_pd2 = (((uint32_t)sensor[3] << 16) |
						      ((uint32_t)sensor[4] << 8) |
						       (uint32_t)sensor[5]) &
						     0x000FFFFFU;
				}
				if (sensor_len >= 9U) {
					ppg_s2_pd1 = (((uint32_t)sensor[6] << 16) |
						      ((uint32_t)sensor[7] << 8) |
						       (uint32_t)sensor[8]) &
						     0x000FFFFFU;
				}
				if (sensor_len >= 12U) {
					ppg_s2_pd2 = (((uint32_t)sensor[9]  << 16) |
						      ((uint32_t)sensor[10] << 8) |
						       (uint32_t)sensor[11]) &
						     0x000FFFFFU;
				}
				if (sensor_len >= 18U) {
					accel_x = (int16_t)(((uint16_t)sensor[12] << 8) |
							     sensor[13]);
					accel_y = (int16_t)(((uint16_t)sensor[14] << 8) |
							     sensor[15]);
					accel_z = (int16_t)(((uint16_t)sensor[16] << 8) |
							     sensor[17]);
				}
			} else {
				/* WHRM block is not in the "18 B prefix + tail" position — still
				 * log the first four 20-bit PPG-ish words from the frame head
				 * for optical bring-up (green / red / IR energy). */
				if (len >= 3U) {
					ppg_s1_pd1 = (((uint32_t)fifo_data[0] << 16) |
						      ((uint32_t)fifo_data[1] << 8) |
						       (uint32_t)fifo_data[2]) &
						     0x000FFFFFU;
				}
				if (len >= 6U) {
					ppg_s1_pd2 = (((uint32_t)fifo_data[3] << 16) |
						      ((uint32_t)fifo_data[4] << 8) |
						       (uint32_t)fifo_data[5]) &
						     0x000FFFFFU;
				}
				if (len >= 9U) {
					ppg_s2_pd1 = (((uint32_t)fifo_data[6] << 16) |
						      ((uint32_t)fifo_data[7] << 8) |
						       (uint32_t)fifo_data[8]) &
						     0x000FFFFFU;
				}
				if (len >= 12U) {
					ppg_s2_pd2 = (((uint32_t)fifo_data[9]  << 16) |
						      ((uint32_t)fifo_data[10] << 8) |
						       (uint32_t)fifo_data[11]) &
						     0x000FFFFFU;
				}
				LOG_INF("Variant C mode03: WHRM at offset %zu in %zu B frame "
					"(expected-prefix decode skipped)",
					whrm_off, len);
			}
		} else {
#if IS_ENABLED(CONFIG_MAX32664_HUB_OUTPUT_MODE_ALGO_PLUS_SENSOR)
			/* No plausible WHRM window — observed on GWEC 30.2.2 with host-fed
			 * accel when CONFIG_MAX32664_HUB_MODE03_FRAME_BYTES does not match
			 * the hub's real packing. Fall back to 0x02 once per sampling session. */
			if (!data->hub_mode03_fallback_done) {
				data->hub_mode03_fallback_done = true;
				(void)max32664_hub_force_output_mode_algo_only(dev);
			}
#endif
			/* Idle placeholder so ppg_algo/UI never see multi-thousand BPM. */
			data->sample.heart_rate = 600;
			data->sample.hr_confidence = 0;
			data->sample.spo2 = 60.0f;
			data->sample.spo2_confidence = 0;
			data->sample.signal_quality = 0;
			data->sample.accel_x = 0;
			data->sample.accel_y = 0;
			data->sample.accel_z = 0;
			if (len >= 3U) {
				ppg_s1_pd1 = (((uint32_t)fifo_data[0] << 16) |
					      ((uint32_t)fifo_data[1] << 8) |
					       (uint32_t)fifo_data[2]) &
					     0x000FFFFFU;
			}
			data->sample.ppg_ir = ppg_s1_pd1;
			LOG_WRN("Variant C mode03: no plausible WHRM 16B in %zu B frame — "
				"publishing idle HR/SpO2; check CONFIG_MAX32664_HUB_MODE03_FRAME_BYTES "
				"or disable CONFIG_MAX32664_HUB_OUTPUT_MODE_ALGO_PLUS_SENSOR",
				len);
			if (len >= 6U) {
				ppg_s1_pd2 = (((uint32_t)fifo_data[3] << 16) |
					      ((uint32_t)fifo_data[4] << 8) |
					       (uint32_t)fifo_data[5]) &
					     0x000FFFFFU;
			}
			if (len >= 9U) {
				ppg_s2_pd1 = (((uint32_t)fifo_data[6] << 16) |
					      ((uint32_t)fifo_data[7] << 8) |
					       (uint32_t)fifo_data[8]) &
					     0x000FFFFFU;
			}
			if (len >= 12U) {
				ppg_s2_pd2 = (((uint32_t)fifo_data[9]  << 16) |
					      ((uint32_t)fifo_data[10] << 8) |
					       (uint32_t)fifo_data[11]) &
					     0x000FFFFFU;
			}
			LOG_INF("Variant C raw (mode03 fallback): PPG w0=%u w1=%u w2=%u w3=%u",
				ppg_s1_pd1, ppg_s1_pd2, ppg_s2_pd1, ppg_s2_pd2);
			k_sem_give(&data->sample_sem);
			return 0;
		}
	} else {
		LOG_ERR("Invalid FIFO entry size: %zu (expected 16 or ≥17)", len);
		return -EINVAL;
	}

	/* Heart rate: 16-bit big-endian, 0.1 bpm resolution. */
	uint16_t hr_raw  = ((uint16_t)algo[0] << 8) | algo[1];
	uint8_t  hr_conf = algo[2];
	uint16_t rr_raw  = ((uint16_t)algo[3] << 8) | algo[4];
	uint8_t  activity = algo[5];

	/* SpO2: 16-bit big-endian, 0.1% resolution. */
	uint16_t spo2_raw   = ((uint16_t)algo[6] << 8) | algo[7];
	uint8_t  spo2_conf  = algo[8];
	uint8_t  spo2_done  = algo[9];
	uint16_t aux        = ((uint16_t)algo[10] << 8) | algo[11];
	uint16_t hr2_raw    = ((uint16_t)algo[12] << 8) | algo[13];
	uint8_t  spo2_state = algo[14];
	uint8_t  scd_state  = algo[15];

	float hr_bpm    = hr_raw  / 10.0f;
	float hr2_bpm   = hr2_raw / 10.0f;
	float spo2_pct  = spo2_raw / 10.0f;
	float rr_ms     = rr_raw  / 10.0f;

	/* Publish to the shared sample struct. The contract documented in
	 * max32664.h is:
	 *   heart_rate  — 0.1 BPM units (channel_get divides by 10)
	 *   spo2        — percent as float (channel_get keeps val1=int, val2=µ-frac)
	 * so we store the raw big-endian value for HR and the already-scaled
	 * float percent for SpO2. Confidence goes to the dedicated fields so
	 * upstream (ppg_algo) can gate output on a real algorithm lock.
	 */
	data->sample.heart_rate       = (int32_t)hr_raw;   /* 0.1 BPM units */
	data->sample.hr_confidence    = hr_conf;
	data->sample.spo2             = spo2_pct;          /* float percent */
	data->sample.spo2_confidence  = spo2_conf;
	data->sample.signal_quality   = (hr_conf + spo2_conf) / 2;
	data->sample.scd_state        = scd_state;
	data->sample.accel_x          = (int16_t)accel_x;
	data->sample.accel_y          = (int16_t)accel_y;
	data->sample.accel_z          = (int16_t)accel_z;
	/* Parked the first slot / first PD raw count on ppg_ir so it is visible
	 * upstream via the existing SENSOR_CHAN_IR pathway when mode 0x03 is
	 * active. In algo-only (mode 0x02) frames ppg_s1_pd1 is 0. */
	data->sample.ppg_ir           = ppg_s1_pd1;

	LOG_DBG("Variant C Sample: HR=%.1f bpm (conf=%u%%), HR2=%.1f bpm, "
		"RR=%.1f ms (activity=0x%02x), SpO2=%.1f%% (conf=%u%%, done=%u), "
		"aux=0x%04x, spo2_state=%u scd_state=%u",
		(double)hr_bpm, hr_conf, (double)hr2_bpm,
		(double)rr_ms, activity,
		(double)spo2_pct, spo2_conf, spo2_done,
		aux, spo2_state, scd_state);

	if (have_sensor_prefix) {
		LOG_INF("Variant C raw: PPG s1/PD1=%u s1/PD2=%u s2/PD1=%u s2/PD2=%u "
			"| accel X=%d Y=%d Z=%d",
			ppg_s1_pd1, ppg_s1_pd2, ppg_s2_pd1, ppg_s2_pd2,
			(int)accel_x, (int)accel_y, (int)accel_z);
	}

	k_sem_give(&data->sample_sem);

	return 0;
}

/* ============================================================================
 * Variant C Operations Structure
 * ============================================================================ */

const struct max32664_variant_ops max32664c_ops = {
	.init = max32664c_init,
	.parse_fifo = max32664c_parse_fifo,
	.fifo_entry_size = 16,
	.dump_registers = max32664c_dump_registers,
	.name = "Variant C (WHRM+WSpO2)",
};
