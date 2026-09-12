/* vcnl3040.c - Driver for Vishay VCNL3040 proximity sensor
 *
 * Copyright (c) 2025 AARMS Value Chain
 * SPDX-License-Identifier: Apache-2.0
 *
 * ==============================================================================
 * WEARABLE APPLICATION DESIGN NOTES
 * ==============================================================================
 *
 * This driver is optimized for wrist-worn wearable devices with the following
 * features tailored for robust wear detection:
 *
 * 1. ACTIVE FORCE (ONE-SHOT) MODE
 *    - Triggers single measurement per sample_fetch() instead of continuous
 *    - Reduces power consumption significantly
 *    - 3ms settling time ensures accurate readings
 *
 * 2. DARK OFFSET CALIBRATION
 *    - Performed at initialization (16-sample average)
 *    - Compensates for ambient IR and sensor baseline
 *    - Must be done with sensor NOT in skin contact
 *    - Non-fatal if calibration fails (reduced accuracy)
 *
 * 3. IIR LOW-PASS FILTER
 *    - Alpha = 0.25 (implemented as divide-by-4 for integer math)
 *    - Provides motion-stable proximity readings
 *    - Filters out transient noise from wrist movement
 *    - No floating point operations
 *
 * 4. MULTI-PULSE AVERAGING
 *    - Default 4 pulses (configurable via device tree)
 *    - Improves SNR on skin contact vs. longer integration time
 *    - Better power efficiency than increasing LED current
 *
 * 5. WEAR STATE CLASSIFICATION
 *    - Three states: NOT_WORN, WORN_LOOSE, WORN_GOOD
 *    - Based on filtered proximity thresholds
 *    - Hysteresis prevents rapid transitions during motion
 *    - Application policy separated from driver logic
 *
 * 6. POWER OPTIMIZATIONS
 *    - Smart persistence enabled (noise immunity)
 *    - Sunlight cancellation (outdoor robustness)
 *    - 1/40 duty cycle default (low power)
 *    - Moderate LED current (adequate for wrist detection)
 *
 * API USAGE:
 *   sensor_sample_fetch(dev, SENSOR_CHAN_PROX);
 *   sensor_channel_get(dev, SENSOR_CHAN_PROX, &val);
 *     // val.val1 = raw proximity (offset-corrected)
 *     // val.val2 = filtered proximity (motion-stable)
 *   sensor_channel_get(dev, SENSOR_CHAN_DISTANCE, &val);
 *     // val.val1 = wear state (0/1/2)
 *     // Note: DISTANCE repurposed for wear detection - NOT a distance sensor
 *
 * LIMITATIONS:
 *   - This is NOT a distance sensor - do not use for ranging
 *   - Calibration must be done without skin contact
 *   - Thresholds may need tuning per skin type/device placement
 *
 * ==============================================================================
 */

#define DT_DRV_COMPAT vishay_vcnl3040

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(vcnl3040, LOG_LEVEL_INF);

/* VCNL3040 Register Addresses */
#define VCNL3040_REG_PS_CONF1_CONF2        0x03  /* PS_CONF1/PS_CONF2 */
#define VCNL3040_REG_PS_CONF3_MS           0x04  /* PS_CONF3/PS_MS */
#define VCNL3040_REG_PS_CANC               0x05  /* PS_CANC */
#define VCNL3040_REG_PS_THDL               0x06  /* PS low threshold */
#define VCNL3040_REG_PS_THDH               0x07  /* PS high threshold */
#define VCNL3040_REG_PS_DATA               0x08  /* PS output data */
#define VCNL3040_REG_INT_FLAG              0x0B  /* Interrupt flag */
#define VCNL3040_REG_ID                    0x0C  /* Device ID */

/* PS_CONF1 bits (low byte of 0x03) */
#define VCNL3040_PS_SD                     BIT(0)   /* Shutdown bit (1=shutdown) */
#define VCNL3040_PS_IT_SHIFT               1        /* Integration time shift */
#define VCNL3040_PS_IT_MASK                0x0E     /* Integration time mask */
#define VCNL3040_PS_PERS_SHIFT             4        /* Persistence shift */
#define VCNL3040_PS_PERS_MASK              0x30     /* Persistence mask */
#define VCNL3040_PS_DUTY_SHIFT             6        /* Duty cycle shift */
#define VCNL3040_PS_DUTY_MASK              0xC0     /* Duty cycle mask */

/* PS_CONF2 bits (high byte of 0x03) */
#define VCNL3040_PS_HD                     BIT(11)  /* 12-bit or 16-bit mode */
#define VCNL3040_PS_INT_SHIFT              8        /* Interrupt mode shift */
#define VCNL3040_PS_INT_MASK               0x0300   /* Interrupt mode mask */

/* PS_CONF3 bits (low byte of 0x04) */
#define VCNL3040_PS_MPS_SHIFT              5        /* Multi-pulse shift */
#define VCNL3040_PS_MPS_MASK               0x60     /* Multi-pulse mask */
#define VCNL3040_PS_SMART_PERS             BIT(4)   /* Smart persistence */
#define VCNL3040_PS_AF                     BIT(3)   /* Active force mode */
#define VCNL3040_PS_TRIG                   BIT(2)   /* Trigger one-time */
#define VCNL3040_PS_SC_EN                  BIT(0)   /* Sunlight cancellation */

/* PS_MS bits (high byte of 0x04) */
#define VCNL3040_LED_I_SHIFT               8        /* LED current shift */
#define VCNL3040_LED_I_MASK                0x0700   /* LED current mask */
#define VCNL3040_PS_SPO                    BIT(14)  /* Sunlight protect output */
#define VCNL3040_PS_SP_EN                  BIT(15)  /* Sunlight protect enable */

/* Device ID value */
#define VCNL3040_DEVICE_ID                 0x0186

/* Wearable wear detection states */
enum vcnl3040_wear_state {
	VCNL3040_NOT_WORN = 0,      /* No skin contact detected */
	VCNL3040_WORN_LOOSE = 1,    /* Skin contact but loose fit */
	VCNL3040_WORN_GOOD = 2,     /* Good skin contact for measurements */
};

/* IIR filter parameters (alpha = 0.25 = 1/4 for efficient integer math) */
#define VCNL3040_IIR_ALPHA_SHIFT    2    /* Divide by 4 for alpha = 0.25 */
#define VCNL3040_CALIBRATION_SAMPLES 16  /* Number of samples for dark offset calibration */

/* Wear detection thresholds (tuned for wrist-worn application).
 *
 * Optical PS counts rise with closer / more reflective surfaces. Firm skin
 * contact on this hardware is typically ~30k–40k. A previous WORN_GOOD floor
 * of 20k also accepted bench table-top reflections, so measurements started
 * off-wrist. Require ≥30k for WORN_GOOD; table/air stay below.
 */
#define VCNL3040_THRESHOLD_NOT_WORN   500    /* Below this = not worn */
#define VCNL3040_THRESHOLD_WORN_LOOSE 30000  /* Above this = good contact (WORN_GOOD) */
#define VCNL3040_HYSTERESIS           300    /* Deadband to prevent state chattering */

/* Active Force mode settling time (datasheet: typ 2ms for IT=8T) */
#define VCNL3040_AF_SETTLE_MS         3

/* Configuration structure */
struct vcnl3040_config {
	struct i2c_dt_spec i2c;
	uint8_t led_current;
	uint8_t duty_cycle;
	uint8_t integration_time;
	uint8_t multi_pulse;         /* Multi-pulse count for better SNR */
	uint16_t threshold_low;
	uint16_t threshold_high;
};

/* Runtime data structure */
struct vcnl3040_data {
	uint16_t proximity_raw;       /* Raw proximity counts from sensor */
	uint16_t proximity_filtered;  /* IIR filtered proximity (motion-stable) */
	uint16_t dark_offset;         /* Dark offset calibration value */
	enum vcnl3040_wear_state wear_state;  /* Current wear detection state */
	bool calibrated;              /* True after successful dark offset calibration */
	bool hw_ready;                /* True once vcnl3040_hw_bringup() has actually
					  succeeded — see the 2026-09-02 redesign notes
					  above vcnl3040_init() for why this exists
					  separately from Zephyr's own device_is_ready(). */
};

/* Read 16-bit register (little-endian) */
static int vcnl3040_read_reg16(const struct device *dev, uint8_t reg, uint16_t *val)
{
	const struct vcnl3040_config *cfg = dev->config;
	uint8_t buf[2];
	int ret;

	ret = i2c_write_read_dt(&cfg->i2c, &reg, 1, buf, 2);
	if (ret < 0) {
		LOG_ERR("Failed to read reg 0x%02X: %d", reg, ret);
		return ret;
	}

	*val = sys_get_le16(buf);
	return 0;
}

/* Write 16-bit register (little-endian) */
static int vcnl3040_write_reg16(const struct device *dev, uint8_t reg, uint16_t val)
{
	const struct vcnl3040_config *cfg = dev->config;
	uint8_t buf[3];
	
	buf[0] = reg;
	sys_put_le16(val, &buf[1]);

	int ret = i2c_write_dt(&cfg->i2c, buf, 3);
	if (ret < 0) {
		LOG_ERR("Failed to write reg 0x%02X: %d", reg, ret);
		return ret;
	}

	return 0;
}

/* Asymmetric IIR filter:
 *   Fall (raw < filtered): instant update — contact-loss must be detected
 *     immediately to allow the proximity guard to fire without IIR-induced lag.
 *   Rise (raw > filtered): alpha=0.25 — slow integration stabilises WORN
 *     detection against transient motion noise.
 */
static uint16_t vcnl3040_filter_proximity(uint16_t current_filtered, uint16_t new_raw)
{
	if (new_raw < current_filtered) {
		/* Fast path: proximity dropped — update instantly so guard triggers
		 * within one sample rather than after ~9 filtered samples. */
		return new_raw;
	}

	/* Slow path: proximity rising — smooth with alpha=0.25 */
	int32_t diff = (int32_t)new_raw - (int32_t)current_filtered;
	int32_t filtered = (int32_t)current_filtered + (diff >> VCNL3040_IIR_ALPHA_SHIFT);

	if (filtered > 65535) {
		filtered = 65535;
	}

	return (uint16_t)filtered;
}

/* Classify wear state with true state-based hysteresis.
 * Each state has different entry/exit thresholds to prevent chattering.
 *   NOT_WORN  → WORN_LOOSE : proximity >= NOT_WORN + HYSTERESIS
 *   WORN_LOOSE→ NOT_WORN   : proximity <  NOT_WORN
 *   WORN_LOOSE→ WORN_GOOD  : proximity >= WORN_LOOSE
 *   WORN_GOOD → WORN_LOOSE : proximity <  WORN_LOOSE - HYSTERESIS
 */
static enum vcnl3040_wear_state vcnl3040_classify_wear_state(
	uint16_t proximity_filtered, enum vcnl3040_wear_state current_state)
{
	switch (current_state) {
	case VCNL3040_NOT_WORN:
		if (proximity_filtered >= (VCNL3040_THRESHOLD_NOT_WORN + VCNL3040_HYSTERESIS)) {
			return VCNL3040_WORN_LOOSE;
		}
		return VCNL3040_NOT_WORN;

	case VCNL3040_WORN_LOOSE:
		if (proximity_filtered < VCNL3040_THRESHOLD_NOT_WORN) {
			return VCNL3040_NOT_WORN;
		} else if (proximity_filtered >= VCNL3040_THRESHOLD_WORN_LOOSE) {
			return VCNL3040_WORN_GOOD;
		}
		return VCNL3040_WORN_LOOSE;

	case VCNL3040_WORN_GOOD:
		if (proximity_filtered < (VCNL3040_THRESHOLD_WORN_LOOSE - VCNL3040_HYSTERESIS)) {
			return VCNL3040_WORN_LOOSE;
		}
		return VCNL3040_WORN_GOOD;

	default:
		return VCNL3040_NOT_WORN;
	}
}

/* Trigger Active Force mode one-shot measurement
 * This conserves power compared to continuous mode.
 */
static int vcnl3040_trigger_measurement(const struct device *dev)
{
	uint16_t conf_val;
	int ret;

	/* Read current PS_CONF3/PS_MS register */
	ret = vcnl3040_read_reg16(dev, VCNL3040_REG_PS_CONF3_MS, &conf_val);
	if (ret < 0) {
		return ret;
	}

	/* Set AF (Active Force) and TRIG bits to trigger one-shot measurement
	 * Per datasheet: Both PS_AF and PS_TRIG must be set for Active Force mode
	 */
	conf_val |= (VCNL3040_PS_AF | VCNL3040_PS_TRIG);

	ret = vcnl3040_write_reg16(dev, VCNL3040_REG_PS_CONF3_MS, conf_val);
	if (ret < 0) {
		LOG_ERR("Failed to trigger AF measurement: %d", ret);
		return ret;
	}

	/* Wait for conversion to complete (datasheet: typ 2ms for IT=8T) */
	k_msleep(VCNL3040_AF_SETTLE_MS);

	return 0;
}

/* Perform dark offset calibration at initialization
 * Averages 16 samples with no skin contact to establish baseline.
 * This compensates for ambient IR and internal sensor offsets.
 */
static int vcnl3040_calibrate_dark_offset(const struct device *dev)
{
	struct vcnl3040_data *data = dev->data;
	uint32_t sum = 0;
	uint16_t sample;
	int ret;

	/* Guard: if the device is already worn at init, calibrating now would
	 * inflate dark_offset to a worn-contact level, suppressing all future
	 * proximity readings and making the sensor appear NOT_WORN when worn.
	 * Take one sample first; skip calibration if contact is detected. */
	ret = vcnl3040_trigger_measurement(dev);
	if (ret == 0 &&
	    vcnl3040_read_reg16(dev, VCNL3040_REG_PS_DATA, &sample) == 0 &&
	    sample >= VCNL3040_THRESHOLD_NOT_WORN) {
		LOG_WRN("Dark offset calibration skipped: device appears worn at init (raw=%u)",
			(unsigned)sample);
		data->dark_offset = 0;
		data->calibrated = false;
		return 0;
	}

	LOG_INF("Starting dark offset calibration (ensure no skin contact)...");

	/* Collect multiple samples for averaging */
	for (int i = 0; i < VCNL3040_CALIBRATION_SAMPLES; i++) {
		/* Trigger one-shot measurement */
		ret = vcnl3040_trigger_measurement(dev);
		if (ret < 0) {
			LOG_ERR("Calibration trigger failed: %d", ret);
			return ret;
		}

		/* Read proximity data */
		ret = vcnl3040_read_reg16(dev, VCNL3040_REG_PS_DATA, &sample);
		if (ret < 0) {
			LOG_ERR("Calibration read failed: %d", ret);
			return ret;
		}

		sum += sample;
		k_msleep(10);  /* Small delay between samples */
	}

	/* Calculate average dark offset */
	data->dark_offset = (uint16_t)(sum / VCNL3040_CALIBRATION_SAMPLES);
	data->calibrated = true;

	LOG_INF("Dark offset calibration complete: %u counts", data->dark_offset);

	return 0;
}

/* Sample fetch function - triggers one-shot measurement and updates all data */
/* Forward declaration: defined further down (needs vcnl3040_hw_bringup(),
 * which lives near vcnl3040_init() for readability), but must be callable
 * from sample_fetch() here. See the 2026-09-02 redesign note above
 * vcnl3040_hw_bringup() for what this actually does. */
static int vcnl3040_ensure_ready(const struct device *dev);

static int vcnl3040_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	struct vcnl3040_data *data = dev->data;
	uint16_t ps_data;
	int ret;

	ret = vcnl3040_ensure_ready(dev);
	if (ret != 0) {
		return ret;
	}

	if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_PROX) {
		return -ENOTSUP;
	}

	/* Trigger one-shot Active Force measurement */
	ret = vcnl3040_trigger_measurement(dev);
	if (ret < 0) {
		return ret;
	}

	/* Read proximity data */
	ret = vcnl3040_read_reg16(dev, VCNL3040_REG_PS_DATA, &ps_data);
	if (ret < 0) {
		return ret;
	}

	/* Apply dark offset calibration (prevent underflow) */
	if (data->calibrated) {
		data->proximity_raw = (ps_data > data->dark_offset) ? (uint16_t)(ps_data - data->dark_offset) : 0U;
	} else {
		/* No calibration performed yet - use raw value */
		data->proximity_raw = ps_data;
	}

	/* Apply IIR filter for motion stability */
	data->proximity_filtered = vcnl3040_filter_proximity(
		data->proximity_filtered, data->proximity_raw);

	/* Update wear detection state (state-based hysteresis) */
	data->wear_state = vcnl3040_classify_wear_state(data->proximity_filtered, data->wear_state);
#if 0
	LOG_INF("Prox: ps_data=%u offset=%u raw=%u filt=%u wear=%d",
		ps_data, data->dark_offset,
		data->proximity_raw, data->proximity_filtered, data->wear_state);
#endif
	return 0;
}

/* Channel get function - exposes raw, filtered proximity and wear state */
static int vcnl3040_channel_get(const struct device *dev,
				enum sensor_channel chan,
				struct sensor_value *val)
{
	struct vcnl3040_data *data = dev->data;

	switch (chan) {
	case SENSOR_CHAN_PROX:
		/* val1 = raw proximity counts (offset-corrected)
		 * val2 = filtered proximity (motion-stable for wearable detection)
		 */
		val->val1 = data->proximity_raw;
		val->val2 = data->proximity_filtered;
		break;

	case SENSOR_CHAN_DISTANCE:
		/* val1 = wear state (0=NOT_WORN, 1=WORN_LOOSE, 2=WORN_GOOD)
		 * val2 = reserved (future use)
		 * Note: This is NOT a distance sensor - repurposed for wear detection
		 */
		val->val1 = (int32_t)data->wear_state;
		val->val2 = 0;
		break;

	default:
		return -ENOTSUP;
	}

	return 0;
}

/* Attribute set function */
static int vcnl3040_attr_set(const struct device *dev,
			     enum sensor_channel chan,
			     enum sensor_attribute attr,
			     const struct sensor_value *val)
{
	uint16_t threshold;

	if (chan != SENSOR_CHAN_PROX) {
		return -ENOTSUP;
	}

	switch (attr) {
	case SENSOR_ATTR_LOWER_THRESH:
		threshold = (uint16_t)val->val1;
		return vcnl3040_write_reg16(dev, VCNL3040_REG_PS_THDL, threshold);

	case SENSOR_ATTR_UPPER_THRESH:
		threshold = (uint16_t)val->val1;
		return vcnl3040_write_reg16(dev, VCNL3040_REG_PS_THDH, threshold);

	default:
		return -ENOTSUP;
	}
}

/* Attribute get function */
static int vcnl3040_attr_get(const struct device *dev,
			     enum sensor_channel chan,
			     enum sensor_attribute attr,
			     struct sensor_value *val)
{
	uint16_t threshold;
	int ret;

	if (chan != SENSOR_CHAN_PROX) {
		return -ENOTSUP;
	}

	switch (attr) {
	case SENSOR_ATTR_LOWER_THRESH:
		ret = vcnl3040_read_reg16(dev, VCNL3040_REG_PS_THDL, &threshold);
		if (ret == 0) {
			val->val1 = threshold;
			val->val2 = 0;
		}
		return ret;

	case SENSOR_ATTR_UPPER_THRESH:
		ret = vcnl3040_read_reg16(dev, VCNL3040_REG_PS_THDH, &threshold);
		if (ret == 0) {
			val->val1 = threshold;
			val->val2 = 0;
		}
		return ret;

	default:
		return -ENOTSUP;
	}
}

/*
 * 2026-09-02 REDESIGN: hardware bring-up is no longer tied to Zephyr's
 * one-shot device init() call at all.
 *
 * Everything above this comment (three rounds of retry-budget tuning:
 * 200ms -> 3s -> back to 3s after someone else's regression) was aimed at
 * finding a boot-time window long enough to catch the sensor rail coming
 * up. That approach has a hard ceiling: vcnl3040_init() runs once, during
 * Zephyr's POST_KERNEL init, before main() — and per the 2026-09-01
 * investigation, the gap between kernel boot and main()'s power_init()
 * call has been observed anywhere from under a second to over 18 seconds
 * across different sessions, for reasons not fully understood (ruled out
 * RTT blocking; see CHANGES doc). No fixed retry budget inside init() can
 * reliably bridge a gap of unknown, widely-varying length without either
 * failing on the slow sessions or wasting seconds of boot time on every
 * normal one.
 *
 * Fix: split hardware bring-up (vcnl3040_hw_bringup(), below) out from
 * Zephyr's device init() (vcnl3040_init(), further below). init() still
 * makes a reasonable first attempt (3s budget, same as before) — this
 * covers the common case where the rail is already up shortly after
 * boot, with no behavior change from before. If that attempt fails,
 * instead of returning an error (which permanently latches
 * device_is_ready() == false for the rest of the session — the actual
 * mechanism behind "proximity works fine in some sessions and never in
 * others"), init() now returns 0 anyway, and vcnl3040_ensure_ready() —
 * called at the top of every vcnl3040_sample_fetch() — quietly retries
 * hardware bring-up on demand. Since sample_fetch() is invoked by
 * temp.c's periodic wear-check poll every few seconds for as long as the
 * device is running, the sensor now keeps trying to come online for the
 * entire session instead of getting exactly one bounded window at boot —
 * whether the rail takes 1 second or 20 seconds to actually appear, the
 * very next poll after that succeeds, and hw_ready latches true from
 * then on with zero further overhead. This works entirely through this
 * driver's own logic; it does not depend on any Zephyr device-model
 * internals beyond the public, always-safe device_is_ready() field this
 * driver has always used.
 *
 * @param dev VCNL3040 device
 * @param max_attempts Extra retries after the first try, 50ms apart
 *        (0 = try once, no retry — used by the lazy on-demand path so a
 *        single sample_fetch() call can never block for seconds; 60 = the
 *        original 3s boot-time budget, used once from vcnl3040_init()).
 * @return 0 on success (device fully configured and calibrated),
 *         negative errno if the chip still isn't responding.
 */
static int vcnl3040_hw_bringup(const struct device *dev, int max_attempts)
{
	const struct vcnl3040_config *cfg = dev->config;
	uint16_t device_id;
	uint16_t conf_val;
	int ret;

	ret = vcnl3040_read_reg16(dev, VCNL3040_REG_ID, &device_id);
	for (int attempt = 0; ret < 0 && attempt < max_attempts; attempt++) {
		k_msleep(50);
		ret = vcnl3040_read_reg16(dev, VCNL3040_REG_ID, &device_id);
	}

	if (ret < 0) {
		return ret;
	}

	if (device_id != VCNL3040_DEVICE_ID) {
		LOG_ERR("Invalid device ID: 0x%04X (expected 0x%04X)", 
			device_id, VCNL3040_DEVICE_ID);
		return -EINVAL;
	}

	LOG_INF("VCNL3040 detected (ID: 0x%04X)", device_id);

	/* Configure PS_CONF1/PS_CONF2 for wearable application
	 * - 16-bit mode for full dynamic range
	 * - Power on (PS_SD cleared)
	 * - Integration time and duty cycle from DT
	 */
	conf_val = 0;
	/* Integration time (bits 3:1) - prefer IT=8T for wearable (good SNR, ~2ms) */
	conf_val |= (cfg->integration_time << VCNL3040_PS_IT_SHIFT) & VCNL3040_PS_IT_MASK;
	/* Duty cycle (bits 7:6) - prefer 1/40 for low power wearable */
	conf_val |= (cfg->duty_cycle << VCNL3040_PS_DUTY_SHIFT) & VCNL3040_PS_DUTY_MASK;
	/* Persistence (bits 5:4) - set to 1 for stability */
	conf_val |= (1 << VCNL3040_PS_PERS_SHIFT) & VCNL3040_PS_PERS_MASK;
	/* 16-bit mode (bit 11) */
	conf_val |= VCNL3040_PS_HD;
	/* Power on: clear shutdown bit (bit 0) */
	conf_val &= ~VCNL3040_PS_SD;

	ret = vcnl3040_write_reg16(dev, VCNL3040_REG_PS_CONF1_CONF2, conf_val);
	if (ret < 0) {
		LOG_ERR("Failed to configure PS_CONF1/2: %d", ret);
		return ret;
	}

	/* Configure PS_CONF3/PS_MS for wearable optimizations
	 * - LED current for adequate SNR on wrist
	 * - Multi-pulse averaging for motion noise rejection
	 * - Smart persistence for stable wear detection
	 * - Sunlight cancellation for robustness
	 * - Active Force mode will be triggered per sample_fetch()
	 */
	conf_val = 0;
	/* LED current (bits 10:8) - moderate current for wrist detection */
	conf_val |= (cfg->led_current << VCNL3040_LED_I_SHIFT) & VCNL3040_LED_I_MASK;
	/* Multi-pulse (bits 6:5) - 4 pulses for better SNR on skin */
	conf_val |= (cfg->multi_pulse << VCNL3040_PS_MPS_SHIFT) & VCNL3040_PS_MPS_MASK;
	/* Smart persistence (bit 4) - improves noise immunity */
	conf_val |= VCNL3040_PS_SMART_PERS;
	/* Sunlight cancellation (bit 0) - outdoor robustness */
	conf_val |= VCNL3040_PS_SC_EN;
	/* Note: PS_AF (Active Force) bit will be set during sample_fetch() */

	ret = vcnl3040_write_reg16(dev, VCNL3040_REG_PS_CONF3_MS, conf_val);
	if (ret < 0) {
		LOG_ERR("Failed to configure PS_CONF3/MS: %d", ret);
		return ret;
	}

	/* Set thresholds from DT (used for interrupt-driven applications) */
	ret = vcnl3040_write_reg16(dev, VCNL3040_REG_PS_THDL, cfg->threshold_low);
	if (ret < 0) {
		LOG_ERR("Failed to set low threshold: %d", ret);
		return ret;
	}

	ret = vcnl3040_write_reg16(dev, VCNL3040_REG_PS_THDH, cfg->threshold_high);
	if (ret < 0) {
		LOG_ERR("Failed to set high threshold: %d", ret);
		return ret;
	}

	/* Perform dark offset calibration for wearable accuracy
	 * This should be done with sensor NOT in contact with skin
	 */
	ret = vcnl3040_calibrate_dark_offset(dev);
	if (ret < 0) {
		LOG_WRN("Dark offset calibration failed: %d (continuing anyway)", ret);
		/* Non-fatal - sensor will work but with reduced accuracy */
	}

	LOG_INF("VCNL3040 init complete: LED=%d, duty=%d, IT=%d, MPS=%d, thresh=%d/%d",
		cfg->led_current, cfg->duty_cycle, cfg->integration_time,
		cfg->multi_pulse, cfg->threshold_low, cfg->threshold_high);

	return 0;
}

/*
 * Lazy retry, called from the top of vcnl3040_sample_fetch() on every
 * single call. Cheap when already ready (one bool check). When not yet
 * ready, tries exactly once more (max_attempts=0, no internal sleep) so a
 * single caller — e.g. temp.c's periodic wear-check poll — is never
 * blocked for more than one quick I2C transaction. The poll's own
 * natural cadence (every few seconds, for the whole session) provides
 * the "keep trying over time" behavior instead of a bounded boot-time
 * window. See the design note above vcnl3040_hw_bringup() for the full
 * reasoning.
 */
static int vcnl3040_ensure_ready(const struct device *dev)
{
	struct vcnl3040_data *data = dev->data;
	int ret;

	if (data->hw_ready) {
		return 0;
	}

	ret = vcnl3040_hw_bringup(dev, 0);
	if (ret == 0) {
		data->hw_ready = true;
		LOG_INF("VCNL3040 came online after boot (late bring-up succeeded)");
	}
	return ret;
}

/* Initialization function */
static int vcnl3040_init(const struct device *dev)
{
	const struct vcnl3040_config *cfg = dev->config;
	struct vcnl3040_data *data = dev->data;
	int ret;

	/* Check I2C bus readiness */
	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	/* First attempt, generous budget (3s) — covers the common case where
	 * the sensor rail is already up shortly after boot. See the redesign
	 * note above vcnl3040_hw_bringup() for what happens if this fails. */
	ret = vcnl3040_hw_bringup(dev, 60);
	if (ret == 0) {
		data->hw_ready = true;
	} else {
		LOG_WRN("VCNL3040 not responding yet at boot (%d) — will keep "
			"retrying on every proximity read instead of giving up "
			"for the rest of the session (see 2026-09-02 redesign "
			"note above)", ret);
		data->hw_ready = false;
	}

	/* Always report success to Zephyr's device model. Returning an error
	 * here would permanently latch device_is_ready() == false for this
	 * session — exactly the trap this redesign exists to avoid. Whether
	 * the sensor is *actually* usable yet is tracked separately in
	 * data->hw_ready and re-checked by vcnl3040_ensure_ready() on every
	 * real use. */
	return 0;
}

/* Sensor driver API */
static const struct sensor_driver_api vcnl3040_api __attribute__((unused)) = {
	.sample_fetch = vcnl3040_sample_fetch,
	.channel_get = vcnl3040_channel_get,
	.attr_set = vcnl3040_attr_set,
	.attr_get = vcnl3040_attr_get,
};

/* Device instantiation macro */
#define VCNL3040_INIT(n)							\
	static struct vcnl3040_data vcnl3040_data_##n;				\
										\
	static const struct vcnl3040_config vcnl3040_config_##n = {		\
		.i2c = I2C_DT_SPEC_INST_GET(n),					\
		.led_current = DT_INST_PROP(n, led_current),			\
		.duty_cycle = DT_INST_PROP(n, duty_cycle),			\
		.integration_time = DT_INST_PROP(n, integration_time),		\
		.multi_pulse = DT_INST_PROP_OR(n, multi_pulse, 2),		\
		.threshold_low = DT_INST_PROP(n, proximity_threshold_low),	\
		.threshold_high = DT_INST_PROP(n, proximity_threshold_high),	\
	};									\
										\
	SENSOR_DEVICE_DT_INST_DEFINE(n, vcnl3040_init, NULL,			\
				     &vcnl3040_data_##n, &vcnl3040_config_##n,	\
				     POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,	\
				     &vcnl3040_api);

DT_INST_FOREACH_STATUS_OKAY(VCNL3040_INIT)
