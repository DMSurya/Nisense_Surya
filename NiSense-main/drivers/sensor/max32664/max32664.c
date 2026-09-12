/**
 * @file max32664.c
 * @brief MAX32664 biometric sensor hub driver
 *
 * Unified driver for MAX32664 sensor hub (all A/B/C/D variants).
 * Hub manages downstream sensors internally (MAX86141, MAX30101, LIS2DS12/KX-122, VCNL3040).
 * nRF52840 communicates ONLY with hubs via I2C slave interface.
 *
 * Variant Support:
 * - MAX32664A: MaximFast (HR + SpO2, 100Hz, MAX30101, optional accel)
 * - MAX32664B: WHRM (Heart Rate, 25Hz, MAX86141, KX-122 REQUIRED)
 * - MAX32664C: WHRM+WSpO2 v3x (HR + SpO2, 25Hz, MAX86141, KX-122 REQUIRED)
 * - MAX32664D: BPT (Blood Pressure + HR + SpO2, 100Hz, MAX30101, calibration REQUIRED)
 *
 * Features:
 * - Automatic variant detection via AFE WHOAMI + firmware version
 * - AGC (Auto Gain Control) configuration for variants A/D
 * - WHRM mode selection for variants B/C (motion artifact rejection)
 * - Accelerometer validation (mandatory for B/C)
 * - BPT calibration tracking (mandatory for D)
 * - Enhanced status flag parsing
 * - Exponential backoff retry logic for 0xFE (ERR_TRY_AGAIN) errors
 *
 * Hardware Topology:
 * - U7: MAX32664 Variant C (WHRM+WSpO2, manages MAX86141 + KX-122 via SPI)
 * - U9: MAX32664 Variant D (BPT, manages MAX30101 + VCNL3040 via I2C)
 * Legacy hardware names: GWEC (Variant C), GWED (Variant D)
 * 
 * I2C Address: Standard default is 0x55 (7-bit). Address can be changed
 * via I2C command on MAX32664B/C variants. Actual address configured in device tree.
 *
 * GPIO Configuration (Optional):
 * ================================
 * This driver supports three operating modes based on devicetree configuration:
 *
 * 1. I2C-ONLY MODE (no GPIOs):
 *    - RSTN: Not connected (assumes external pull-up or power-on reset)
 *    - MFIO: Not connected (driver uses polling mode)
 *    - Use case: Level translator issues, simplified wiring, testing
 *    - Example devicetree:
 *      max32664@55 {
 *          compatible = "maxim,max32664-gwed";
 *          reg = <0x55>;
 *          poll-interval-ms = <200>;  // Polling every 200ms
 *      };
 *
 * 2. INTERRUPT + POLL FALLBACK (current NiSense HW — RSTN + MFIO wired):
 *    - RSTN: Connected (P0.20) for reliable reset / bootloader entry
 *    - MFIO: Connected (P1.06) data-ready IRQ; hub cmd 0xB8 enables output
 *    - poll-interval-ms still runs as fallback (GWEC may skip DataRdy on RAW)
 *    - Example devicetree:
 *      max32664@55 {
 *          compatible = "maxim,max32664-gwed";
 *          reg = <0x55>;
 *          interrupt-gpios = <&gpio1 6 GPIO_ACTIVE_LOW>;
 *          reset-gpios = <&gpio0 20 GPIO_ACTIVE_LOW>;
 *          poll-interval-ms = <200>;
 *      };
 *
 * 3. POLLING MODE (with RSTN):
 *    - RSTN: Connected (controlled hardware reset)
 *    - MFIO: Not connected (driver polls periodically)
 *    - Use case: Reliable reset with periodic sampling
 *    - Example devicetree:
 *      max32664@55 {
 *          compatible = "maxim,max32664-gwed";
 *          reg = <0x55>;
 *          reset-gpios = <&gpio0 20 GPIO_ACTIVE_LOW>;
 *          poll-interval-ms = <200>;
 *      };
 *
 * Datasheet: MAX32664 Biometric Sensor Hub
 */

/* Unified driver - auto-detects variant (A/B/C/D) via AFE WHOAMI register
 * Also supports variant-specific compatible strings for explicit configuration
 */
#define DT_DRV_COMPAT maxim_max32664

/* Include variant-specific compatibles as aliases */
#define DT_DRV_COMPAT_GWEC maxim_max32664_gwec /* Legacy: Variant C */
#define DT_DRV_COMPAT_GWED maxim_max32664_gwed /* Legacy: Variant D */

#include "max32664.h"
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/max32664.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <string.h>

/* Include private header for variant ops and internal APIs */
#include "max32664_priv.h"

#if IS_ENABLED(CONFIG_MAX32664_LOG_AFE_LED_ON_SAMPLING_START) || \
    defined(CONFIG_MAX32664_MODE_RAW)
#include "max86141_regs.h"
#include "max86141_green.h"
#endif

#include <zephyr/pm/device.h>
#include <zephyr/drivers/sensor/max32664.h>

LOG_MODULE_REGISTER(max32664, CONFIG_SENSOR_LOG_LEVEL);

/* ============================================================================
 * Variant Operations Dispatch
 * ============================================================================ */

/**
 * @brief Get variant-specific operations structure
 *
 * @param variant Detected variant type
 * @return Pointer to ops structure, or NULL if unknown
 */
const struct max32664_variant_ops *max32664_get_variant_ops(enum max32664_variant variant)
{
#if defined(CONFIG_MAX32664_MODE_HUB)
	/* HUB mode: variant ops are needed for algorithm management */
	switch (variant) {
	case MAX32664_VARIANT_A:
		return &max32664a_ops;
	case MAX32664_VARIANT_B:
#if IS_ENABLED(CONFIG_MAX32664_HUB_VARIANT_B)
		return &max32664b_ops;
#else
		/* Stub gated out — see MAX32664_HUB_VARIANT_B */
		return NULL;
#endif
	case MAX32664_VARIANT_C:
		return &max32664c_ops;
	case MAX32664_VARIANT_D:
		return &max32664d_ops;
	default:
		return NULL;
	}
#else
	/* RAW mode: variant ops not used - sensors accessed directly */
	ARG_UNUSED(variant);
	return NULL;
#endif
}

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Sensor API functions */
static int max32664_sample_fetch(const struct device *dev,
				  enum sensor_channel chan);
static int max32664_channel_get(const struct device *dev,
				 enum sensor_channel chan,
				 struct sensor_value *val);
static int max32664_attr_set(const struct device *dev,
			      enum sensor_channel chan,
			      enum sensor_attribute attr,
			      const struct sensor_value *val);
static int max32664_attr_get(const struct device *dev,
			      enum sensor_channel chan,
			      enum sensor_attribute attr,
			      struct sensor_value *val);

/* Init and helper functions */
static int max32664_init(const struct device *dev);
static int max32664_configure_gpios(const struct max32664_config *cfg);
static int max32664_reset_sequence(const struct max32664_config *cfg);
static int max32664_check_device_mode(const struct device *dev, uint8_t *mode_out);
static int max32664_read_and_log_mcu_type(const struct device *dev, struct max32664_data *data);
static int max32664_read_and_log_fw_version(const struct device *dev, struct max32664_data *data, uint8_t *major, uint8_t *minor, uint8_t *patch);
static int max32664_detect_and_log_variant(const struct device *dev, struct max32664_data *data);
static int max32664_variant_init(const struct device *dev, struct max32664_data *data);
static int max32664_post_init_setup(const struct device *dev, struct max32664_data *data);



/* ============================================================================
 * I2C Slave Interface Functions
 * ============================================================================
 * All hub communication occurs via I2C slave interface. Hub firmware manages
 * downstream sensors internally (MAX86141, MAX30101, LIS2DS12, VCNL3040).
 * ============================================================================ */





/* Transport helpers moved to max32664_transport.c to keep this file focused
 * on init/state/polling and variant orchestration logic. */

/**
 * @brief Power cycle the hub via regulator (if vin-supply specified)
 *
 * This function uses the regulator subsystem to power cycle the MAX32664 hub.
 * It's the most reliable reset method when RSTN GPIO is not available.
 *
 * The power cycle sequence:
 * 1. Disable regulator (power off)
 * 2. Wait 200ms for capacitors to discharge
 * 3. Enable regulator (power on)
 * 4. Wait 1600ms for hub to boot into application mode
 *
 * @param dev Device pointer
 * @return 0 on success, -ENOTSUP if no regulator, negative error on failure
 */
static int max32664_regulator_power_cycle(const struct device *dev)
{
	return max32664_init_regulator_power_cycle(dev);
}

/**
 * @brief Perform software reset via bootloader mode toggle
 *
 * This attempts to reset the hub by entering and exiting bootloader mode.
 * Works when hub is responsive but stuck in algorithm processing.
 *
 * @param dev Device pointer
 * @return 0 on success, negative error on failure
 */
static int max32664_software_reset(const struct device *dev)
{
	return max32664_init_software_reset(dev);
}

/* Register/FIFO protocol helpers moved to max32664_protocol.c to keep this
 * file focused on probing/initialization and runtime orchestration. */











/* ============================================================================
 * Hub Initialization
 * ============================================================================
 * Initialization sequence:
 * 1. Verify I2C communication
 * 2. Detect variant (GWEC vs GWED) from I2C address
 * 3. Read firmware version
 * 4. Configure hub for data output
 * 5. Send commands to enable sensors (hub-managed internally)
 * 6. Configure interrupts (optional) or polling
 * 7. Initialize data structures
 * ============================================================================ */

static int max32664_try_output_mode(const struct device *dev,
			     uint8_t preferred_mode,
			     uint8_t fallback_mode)
{
	return max32664_runtime_try_output_mode(dev, preferred_mode, fallback_mode);
}

/**
 * @brief Request the preferred HUB output mode (algo-only vs algo+sensor)
 *
 * Centralises the decision between output mode 0x02 ("algorithm data only",
 * 16-byte FIFO entries) and 0x03 ("algorithm + raw sensor data", larger
 * entries) so both the post-init step and the start-sampling step stay in
 * sync, and so the driver's `fifo_entry_size_runtime` is set correctly
 * whichever mode the hub actually accepts.
 *
 * When CONFIG_MAX32664_HUB_OUTPUT_MODE_ALGO_PLUS_SENSOR is enabled the
 * driver prefers mode 0x03 and falls back to 0x02 (not PAUSE) if the
 * firmware rejects it  keeping the hub in a sampling state either way.
 * With the Kconfig disabled, behaviour is identical to the old hard-coded
 * path (mode 0x02, fallback to PAUSE).
 *
 * On success this function updates `data->fifo_entry_size_runtime` so the
 * common FIFO read path requests the correct number of bytes per entry.
 */
#if IS_ENABLED(CONFIG_MAX32664_MODE_HUB)
static int max32664_request_hub_output_mode(const struct device *dev)
{
	return max32664_runtime_request_hub_output_mode(dev);
}

int max32664_hub_force_output_mode_algo_only(const struct device *dev)
{
	return max32664_runtime_force_output_mode_algo_only(dev);
}
#endif /* CONFIG_MAX32664_MODE_HUB */

/* ============================================================================
 * Initialization Functions
 * ============================================================================ */

/**
 * @brief Initialize MAX32664 sensor hub
 *
 * Performs full initialization sequence including variant detection,
 * sensor enable commands, and polling/interrupt setup.
 *
 * Initialization Steps:
 *   1. Configure GPIOs (RSTN, MFIO)
 *   2. Reset sequence (hardware or software reset)
 *   3. Check device mode (application/bootloader/reset)
 *   4. Read MCU type
 *   5. Read firmware version
 *   6. Detect variant (includes hardware WHOAMI validation)
 *   7. Variant-specific initialization
 *   8. Post-initialization setup (output mode, data structs, status, IRQ/poll)
 *
 * @param dev Device pointer
 * @return 0 on success, negative error code on failure
 */
static int max32664_init(const struct device *dev)
{
	const struct max32664_config *cfg = dev->config;
	struct max32664_data *data = dev->data;
	int ret;
	uint8_t major = 0, minor = 0, patch = 0;
	uint8_t device_mode = 0;

	data->dev = dev;
	k_mutex_init(&data->bus_mutex);
	LOG_INF("Initializing MAX32664 hub @ 0x%02x...", cfg->bus.addr);

	if (!device_is_ready(cfg->bus.bus)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	// Step 1: Configure GPIOs
	ret = max32664_configure_gpios(cfg);
	if (ret) return ret;

	// Step 2: Reset sequence
	ret = max32664_reset_sequence(cfg);
	if (ret) return ret;

	// Step 3: Check device mode
	ret = max32664_check_device_mode(dev, &device_mode);
	if (ret) return ret;
	if (device_mode != 0x00) {
		LOG_ERR("Device not in application mode! Current mode=0x%02x", device_mode);
		return -EINVAL;
	}
	LOG_INF("Device in application mode (ready)");

	// Step 4: Read MCU type
	ret = max32664_read_and_log_mcu_type(dev, data);
	if (ret) LOG_WRN("Failed to read MCU type: %d", ret);

	// Step 5: Read firmware version
	ret = max32664_read_and_log_fw_version(dev, data, &major, &minor, &patch);
	if (ret) LOG_WRN("Failed to read firmware version: %d", ret);

	// Step 6: Detect variant (includes hardware WHOAMI validation)
	ret = max32664_detect_and_log_variant(dev, data);
	if (ret) return ret;

	// Step 7: Variant-specific initialization
	ret = max32664_variant_init(dev, data);
	if (ret) return ret;

	// Step 8: Post-initialization setup (output mode, data structs, status, IRQ/poll)
	ret = max32664_post_init_setup(dev, data);
	return ret;
}

/* ============================================================================
 * Init Helper Function Implementations
 * ============================================================================ */

static int max32664_configure_gpios(const struct max32664_config *cfg) {
	return max32664_init_configure_gpios(cfg);
}

static int max32664_reset_sequence(const struct max32664_config *cfg) {
	return max32664_init_reset_sequence(cfg);
}

static int max32664_check_device_mode(const struct device *dev, uint8_t *mode_out) {
	return max32664_init_check_device_mode(dev, mode_out);
}

static int max32664_read_and_log_mcu_type(const struct device *dev, struct max32664_data *data) {
	return max32664_init_read_and_log_mcu_type(dev, data);
}

static int max32664_read_and_log_fw_version(const struct device *dev, struct max32664_data *data, uint8_t *major, uint8_t *minor, uint8_t *patch) {
	return max32664_init_read_and_log_fw_version(dev, data, major, minor, patch);
}

int max32664_stop_algorithms_and_sensors(const struct device *dev) {
	struct max32664_data *data = dev->data;
	const struct max32664_config *cfg = dev->config;
	uint8_t tx[4], rx[2];
	int ret, any_success = 0;

	/*
	 * Family 0x44 Sensor Mode Enable/Disable — forms are NOT symmetric:
	 *
	 * Optical AFE (idx 0x00 MAX8614x / 0x03 MAX3010x):
	 *   Classic UG table (~p.29): write bytes = one mode byte
	 *     0x44 <idx> 0x00  → Disable   (3-byte TX)
	 *     0x44 <idx> 0x01  → Enable    (3-byte TX)
	 *   MAX32664C SampleHostCode 30.13.31 / Measuring-SpO2 UG Table 14:
	 *     Enable AFE uses a 4th "sample source" byte:
	 *     0x44 0x00 0x01 0x00  → Enable, hub-side samples
	 *     Disable remains 0x44 0x00 0x00 (3-byte) in C Quick Start tables.
	 *
	 * Accelerometer (idx 0x04) — always two write bytes (4-byte TX):
	 *   0x44 0x04 0x00 0x00  Disable hub-side accel
	 *   0x44 0x04 0x00 0x01  Disable host-fed accel path
	 *   0x44 0x04 0x01 0x00  Enable hub-side accel
	 *   0x44 0x04 0x01 0x01  Enable host-fed accel
	 *   (ADI MAX32664 User Guide, Sensor Mode Enable, accelerometer row)
	 */
	LOG_DBG("Stopping any running algorithms/sensors...");
	LOG_DBG("Setting output mode to PAUSE...");
	tx[0] = 0x10; tx[1] = 0x00; tx[2] = 0x00;
	ret = max32664_i2c_transmit(dev, tx, 3, rx, 1, 100);
	if (ret == 0) {
		any_success = 1;
	}
	k_msleep(200);

	LOG_DBG("Stopping all algorithms...");
	tx[0] = 0x52; tx[1] = 0x00; tx[2] = 0x00;
	ret = max32664_i2c_transmit(dev, tx, 3, rx, 1, 500);
	if (ret == 0) {
		any_success = 1;
	}
	k_msleep(200);

	if (data->variant == MAX32664_VARIANT_A || data->variant == MAX32664_VARIANT_D) {
		/* Classic UG: MAX3010x disable is 3-byte (no sample-source byte). */
		LOG_DBG("Disabling MAX3010x AFE (0x44 0x03 0x00)...");
		tx[0] = 0x44;
		tx[1] = 0x03;
		tx[2] = 0x00;
		ret = max32664_i2c_transmit(dev, tx, 3, rx, 1, 250);
		if (ret == 0) {
			any_success = 1;
		}
	} else if (data->variant == MAX32664_VARIANT_B ||
		   data->variant == MAX32664_VARIANT_C) {
		/* C Quick Start / Measuring-SpO2: AFE disable is 3-byte. */
		LOG_DBG("Disabling MAX86141 AFE (0x44 0x00 0x00)...");
		tx[0] = 0x44;
		tx[1] = 0x00;
		tx[2] = 0x00;
		ret = max32664_i2c_transmit(dev, tx, 3, rx, 1, 250);
		if (ret == 0) {
			any_success = 1;
		}

		/* Accel disable must be 4-byte (UG accel row) — was previously omitted. */
		LOG_DBG("Disabling hub-side accel (0x44 0x04 0x00 0x00)...");
		tx[0] = 0x44;
		tx[1] = 0x04;
		tx[2] = 0x00;
		tx[3] = 0x00;
		ret = max32664_i2c_transmit(dev, tx, 4, rx, 1, 250);
		if (ret == 0) {
			any_success = 1;
		}

		if (cfg->accel_external) {
			LOG_DBG("Disabling host-fed accel path (0x44 0x04 0x00 0x01)...");
			tx[3] = 0x01;
			ret = max32664_i2c_transmit(dev, tx, 4, rx, 1, 250);
			if (ret == 0) {
				any_success = 1;
			}
		}
	}
	k_msleep(100);

	LOG_DBG("Flushing FIFO...");
	tx[0] = 0x12;
	tx[1] = 0x00;
	ret = max32664_i2c_transmit(dev, tx, 2, rx, 1, 100);
	if (ret == 0) {
		any_success = 1;
	}
	k_msleep(200);
	if (!any_success) {
		LOG_DBG("No stop commands succeeded - attempting software reset...");
		ret = max32664_software_reset(dev);
		if (ret == 0) {
			return 0;
		}
		LOG_DBG("Software reset failed - attempting regulator power cycle...");
		ret = max32664_regulator_power_cycle(dev);
		if (ret == 0 || ret == -ENOTSUP) {
			return 0;
		}
		return ret;
	}
	return 0;
}

static int max32664_detect_and_log_variant(const struct device *dev, struct max32664_data *data) {
	return max32664_init_detect_and_log_variant(dev, data);
}

static int max32664_variant_init(const struct device *dev, struct max32664_data *data) {
	return max32664_init_variant_init(dev, data);
}

static int max32664_post_init_setup(const struct device *dev, struct max32664_data *data)
{
	const struct max32664_config *cfg = dev->config;
	int ret;

	/*
	 * Operating mode is a Kconfig choice (exactly one of PASSTHROUGH / RAW /
	 * HUB). Branches below are mutually exclusive — do not nest
	 * !PASSTHROUGH / !RAW guards inside a mode arm.
	 */
#if defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
	/* Hub as I2C bridge only: PAUSE output, no Family 0x12 sampling path. */
	LOG_INF("PASSTHROUGH mode post-init: setting output mode to PAUSE (0x00)...");

	ret = max32664_try_output_mode(dev, 0x00, 0x00); /* PAUSE */
	if (ret) {
		LOG_ERR("Failed to set PAUSE mode in PASSTHROUGH: %d", ret);
		return ret;
	}

	k_sem_init(&data->sample_sem, 0, 1);
	data->sampling = false;
	data->config.initialized = true;
	data->config.interrupt_mode = false; /* PASSTHROUGH uses host sensor polling */
	data->config.poll_interval_ms = cfg->poll_interval_ms;

	LOG_INF(" MAX32664 PASSTHROUGH Mode initialization complete ");
	LOG_INF("Hub in PAUSE mode - access sensors via ncs,ppg-primary and accel-sensor");
	return 0;

#elif defined(CONFIG_MAX32664_MODE_RAW)
	/* Sensor-data-only output; host DSP drains Family 0x12 / AFE FIFO. */
	LOG_INF("RAW mode post-init: setting output mode to SENSOR DATA ONLY (0x01)...");

	ret = max32664_try_output_mode(dev, 0x01, 0x00); /* sensor data, fallback PAUSE */
	if (ret) {
		LOG_ERR("Failed to set sensor data mode in RAW: %d", ret);
		return ret;
	}

	k_sem_init(&data->sample_sem, 0, 1);
	data->sampling = false;
	data->config.initialized = true;
	data->config.interrupt_mode = cfg->interrupt_mode && (cfg->int_gpio.port != NULL);
	data->config.poll_interval_ms = cfg->poll_interval_ms;

	/* Wake: MFIO IRQ when wired + poll fallback. Drain is on-demand via
	 * max86141_ppg / max32664_raw_drain_ppg(); start/stop via hub
	 * SAMPLING_FREQUENCY. Handler is compiled for RAW|HUB only. */
	k_work_init_delayable(&data->poll_work, max32664_sampling_poll_work_handler);

	/* AN6924 ~p.12: MFIO IRQ mode unsupported on FW 32.x.y — Normal+poll. */
	if (data->config.interrupt_mode &&
	    (((data->firmware_version >> 16) & 0xFF) == 32U)) {
		LOG_WRN("RAW: FW 32.x.y — MFIO IRQ unsupported; using Normal MFIO + poll");
		data->config.interrupt_mode = false;
	}

	if (data->config.interrupt_mode) {
		uint8_t tx_irq[2];
		uint8_t rx_irq[1];

		ret = gpio_pin_configure_dt(&cfg->int_gpio, GPIO_INPUT);
		if (ret) {
			LOG_ERR("RAW: failed to configure MFIO as input: %d", ret);
			return ret;
		}

		ret = gpio_pin_interrupt_configure_dt(&cfg->int_gpio, GPIO_INT_EDGE_FALLING);
		if (ret) {
			LOG_ERR("RAW: failed to configure MFIO interrupt: %d", ret);
			return ret;
		}

		gpio_init_callback(&data->gpio_cb,
				   max32664_sampling_gpio_interrupt_handler,
				   BIT(cfg->int_gpio.pin));
		ret = gpio_add_callback_dt(&cfg->int_gpio, &data->gpio_cb);
		if (ret) {
			LOG_ERR("RAW: failed to add MFIO callback: %d", ret);
			return ret;
		}

		/* AN6924: enable hub MFIO interrupt output (Family 0xB8, data 0x01). */
		tx_irq[0] = 0xB8;
		tx_irq[1] = 0x01;
		ret = max32664_i2c_transmit(dev, tx_irq, 2, rx_irq, 1, 10);
		if (ret) {
			LOG_WRN("RAW: hub interrupt enable (0xB8) failed: %d — poll fallback only",
				ret);
			data->config.interrupt_mode = false;
		} else {
			k_work_init(&data->interrupt_work,
				    max32664_sampling_interrupt_work_handler);
			LOG_INF("RAW: MFIO interrupt enabled (poll @ %ums as fallback)",
				cfg->poll_interval_ms);
		}
	}

	if (!data->config.interrupt_mode) {
		LOG_INF("RAW: polling mode configured (interval: %ums)", cfg->poll_interval_ms);
	}

	LOG_INF(" MAX32664 RAW Mode initialization complete ");
	LOG_INF("Hub in SENSOR DATA mode - raw values readable via sensor channels");
	LOG_INF("Config: wake=%s, poll_interval=%ums",
		data->config.interrupt_mode ? "MFIO+poll" : "polling",
		cfg->poll_interval_ms);
	return 0;

#else /* CONFIG_MAX32664_MODE_HUB */
	uint8_t tx[3], rx[2];

	/* On-chip algo output. max32664_request_hub_output_mode() picks 0x03
	 * (algo+sensor) when HUB_OUTPUT_MODE_ALGO_PLUS_SENSOR is y, else 0x02. */
	LOG_INF("HUB mode post-init: selecting output mode...");
	ret = max32664_request_hub_output_mode(dev);
	if (ret) {
		LOG_ERR("Failed to set HUB output mode: %d", ret);
		return ret;
	}
	LOG_INF("Hub in ALGORITHM DATA mode - call sensor_attr_set() to start sampling");

	k_sem_init(&data->sample_sem, 0, 1);
	data->sampling = false;
	data->config.initialized = true;
	data->config.interrupt_mode = cfg->interrupt_mode;
	data->config.poll_interval_ms = cfg->poll_interval_ms;

	uint8_t hub_status;
	ret = max32664_get_hub_status(dev, &hub_status);
	if (ret == 0) {
		bool err0, data_rdy, fifo_out_ovr, fifo_in_ovr, host_accel_uf;

		max32664_parse_status(hub_status, &err0, &data_rdy, &fifo_out_ovr,
				      &fifo_in_ovr, &host_accel_uf);
		LOG_INF("Hub status: 0x%02x (ERR0=%d, DataRdy=%d, FIFO_Out_Ovr=%d, FIFO_In_Ovr=%d, Accel_UF=%d)",
			hub_status, err0, data_rdy, fifo_out_ovr, fifo_in_ovr, host_accel_uf);
		if (err0) {
			LOG_ERR("Hub reports critical error (ERR0 set) - check sensor configuration");
			return -EIO;
		}
		if (data->accel_required && !data->accel_enabled) {
			LOG_ERR("FATAL: Variant %s requires accelerometer but it failed to initialize",
				data->variant == MAX32664_VARIANT_B ? "B" : "C");
			return -ENODEV;
		}
		if (data->calibration_required && !data->calibration_done) {
			LOG_WRN("Variant D (BPT) requires calibration - BP data currently invalid");
		}
	} else {
		LOG_WRN("Failed to read hub status: %d - continuing anyway", ret);
	}

	LOG_INF(" MAX32664 Variant %s initialization complete ",
		data->variant == MAX32664_VARIANT_A ? "A" :
		data->variant == MAX32664_VARIANT_B ? "B" :
		data->variant == MAX32664_VARIANT_C ? "C" :
		data->variant == MAX32664_VARIANT_D ? "D" : "?");
	LOG_INF("Ready for sampling - use sensor_attr_set() to start");
	if (data->agc.configured) {
		LOG_INF("AGC configured: %u%% target, %u%% step, %u%% sensitivity, %u samples avg",
			data->agc.target_percent, data->agc.step_size, data->agc.sensitivity,
			data->agc.samples_avg);
	}

	/* AN6924 ~p.12: MFIO IRQ mode unsupported on FW 32.x.y — Normal+poll. */
	if (data->config.interrupt_mode && cfg->int_gpio.port != NULL &&
	    (((data->firmware_version >> 16) & 0xFF) == 32U)) {
		LOG_WRN("HUB: FW 32.x.y — MFIO IRQ unsupported; using Normal MFIO + poll");
		data->config.interrupt_mode = false;
	}

	if (data->config.interrupt_mode && cfg->int_gpio.port != NULL) {
		ret = gpio_pin_configure_dt(&cfg->int_gpio, GPIO_INPUT);
		if (ret) {
			LOG_ERR("Failed to reconfigure MFIO as input: %d", ret);
			return ret;
		}
		ret = gpio_pin_interrupt_configure_dt(&cfg->int_gpio, GPIO_INT_EDGE_FALLING);
		if (ret) {
			LOG_ERR("Failed to configure MFIO interrupt: %d", ret);
			return ret;
		}
		gpio_init_callback(&data->gpio_cb, max32664_sampling_gpio_interrupt_handler,
				   BIT(cfg->int_gpio.pin));
		ret = gpio_add_callback_dt(&cfg->int_gpio, &data->gpio_cb);
		if (ret) {
			LOG_ERR("Failed to add MFIO callback: %d", ret);
			return ret;
		}
		/* AN6924: enable hub MFIO interrupt output (Family 0xB8, data 0x01). */
		tx[0] = 0xB8;
		tx[1] = 0x01;
		ret = max32664_i2c_transmit(dev, tx, 2, rx, 1, 10);
		if (ret) {
			LOG_WRN("Failed to enable hub interrupt mode: %d", ret);
			data->config.interrupt_mode = false;
		} else {
			k_work_init(&data->interrupt_work, max32664_sampling_interrupt_work_handler);
			LOG_INF("Interrupt mode enabled (MFIO pin)");
		}
	}

	k_work_init_delayable(&data->poll_work, max32664_sampling_poll_work_handler);

	if (!data->config.interrupt_mode || !cfg->int_gpio.port) {
		LOG_INF("Polling mode configured (interval: %ums) - not started",
			cfg->poll_interval_ms);
	} else {
		LOG_INF("Interrupt mode primary, polling available as fallback");
	}

	LOG_INF("MAX32664 initialization complete (use attr_set to start sampling)");
	return 0;
#endif /* CONFIG_MAX32664_MODE_PASSTHROUGH / RAW / HUB */
}

/* ============================================================================
 * Polling & Interrupt Handling (HUB mode only)
 * ============================================================================
 * Driver supports two modes:
 * - Polling mode: Periodically checks hub for new data (default)
 * - Interrupt mode: Hub signals data-ready via MFIO pin (optional)
 * ============================================================================ */

/**
 * @brief Polling work handler (reads hub periodically)
 *
 * Called periodically (every poll_interval_ms) to check if hub has data
 * ready. Uses Family 0x00 (hub status) and Family 0x12 (FIFO count) commands.
 *
 * @param work Work queue item
 */


/* ============================================================================
 * Data Parsing & Sensor API (HUB mode only)
 * ============================================================================
 * Hub provides pre-processed sensor data in variant-specific formats:
 * - GWEC: PPG (IR+Red+Ambient) + Accel (X+Y+Z) from MAX86141 + LIS2DS12
 * - GWED: PPG (IR+Red) + Prox + ALS from MAX30101 + VCNL3040
 * 
 * Note: RAW mode data parsing is in sample_fetch, not here.
 * ============================================================================ */





/**
 * @brief Attribute set - configure hub parameters
 *
 * Zephyr sensor API function for setting sensor attributes.
 * Currently not implemented - would send hub commands to configure
 * LED current, sample rate, etc.
 *
 * @param dev Device pointer
 * @param chan Sensor channel
 * @param attr Attribute to set
 * @param val Attribute value
 * @return -ENOTSUP (not implemented)
 */
static int max32664_attr_set(const struct device *dev,
			      enum sensor_channel chan,
			      enum sensor_attribute attr,
			      const struct sensor_value *val)
{
	struct max32664_data *data = dev->data;
	const struct max32664_config *cfg = dev->config;

	ARG_UNUSED(chan);
#if !defined(CONFIG_MAX32664_MODE_RAW)
	/* cfg->accel_external is only accessed in the RAW mode START branch. */
	ARG_UNUSED(cfg);
#endif
#if !defined(CONFIG_MAX32664_MODE_RAW) && !defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
	/* HUB mode delegates sampling control to the hub helpers and never
	 * touches `data` directly here. Silence -Wunused-variable. */
	ARG_UNUSED(data);
#endif
	
	LOG_DBG("attr_set called: attr=%d, val=%d", attr, val->val1);
	
	/* SENSOR_ATTR_SAMPLING_FREQUENCY: 0 = stop, non-zero = start */
	if (attr == SENSOR_ATTR_SAMPLING_FREQUENCY) {
		LOG_INF("SAMPLING_FREQUENCY request: %s (val=%d, chan=%d)", 
			val->val1 == 0 ? "STOP" : "START", val->val1, chan);
		
#if defined(CONFIG_MAX32664_MODE_RAW)
		/*
		 * RAW MODE — true sensor-only (Zephyr max32664c_set_mode_raw /
		 * ADI Measuring-SpO2 UG Table 5):
		 *   stop algo → output 0x01 → enable AFE/accel → host LED/AFE
		 * Never arm WHRM (0x52 … 0x01): host owns MAX86141 via Family
		 * 0x40 passthrough; hub only multiplexes sensor FIFO (0x12).
		 */
		if (val->val1 == 0) {
			LOG_INF("RAW mode: Stopping sensor-only sampling...");
			max32664_raw_reset_session_state(dev);
			data->sampling = false;
			k_work_cancel_delayable(&data->poll_work);

			if (data->variant == MAX32664_VARIANT_C ||
			    data->variant == MAX32664_VARIANT_B) {
				uint8_t tx_dis[3];
				uint8_t rx_dis[1];

				/* Defensive: keep algo off (should already be). */
				tx_dis[0] = 0x52;
				tx_dis[1] = (data->variant == MAX32664_VARIANT_C) ?
					    0x02 : 0x01;
				tx_dis[2] = 0x00;
				(void)max32664_i2c_transmit_retry(dev, tx_dis,
					sizeof(tx_dis), rx_dis, 1, 200, 3);
				LOG_DBG("RAW: algo confirm-off (status=0x%02x)",
					rx_dis[0]);
			}

			/* Host-owned AFE: LEDs off + SHDN until next measure. */
			(void)max86141_ppg_leds_set(dev, false,
						    &data->raw_runtime.leds_enabled);
			(void)max86141_write_reg(dev, MAX86141_REG_SYSTEM_CTRL, 0x02);

			(void)max32664_accel_feeder_stop(dev);
			return 0;
		} else {
			LOG_INF("RAW mode: Starting sensor-only sampling (%s, poll=%ums)...",
				data->config.interrupt_mode ? "MFIO+poll" : "polling",
				data->config.poll_interval_ms);
			max32664_raw_reset_session_state(dev);

			if (data->variant == MAX32664_VARIANT_C ||
			    data->variant == MAX32664_VARIANT_B) {
				uint8_t tx4[4];
				uint8_t tx3[3];
				uint8_t rx1[1];
				uint8_t fw_major = (data->firmware_version >> 16) & 0xFF;
				uint8_t fw_minor = (data->firmware_version >> 8) & 0xFF;
				uint8_t fw_patch = data->firmware_version & 0xFF;
				bool report_period_cmd_supported =
					!(fw_major == 30U && fw_minor == 2U && fw_patch == 2U);

				/* 1) Stop algo first (Zephyr max32664c_stop_algo). */
				tx3[0] = 0x52;
				tx3[1] = (data->variant == MAX32664_VARIANT_C) ?
					 0x02 : 0x01;
				tx3[2] = 0x00;
				(void)max32664_i2c_transmit_retry(dev, tx3,
					sizeof(tx3), rx1, 1, 200, 3);
				LOG_DBG("RAW: algo stopped (status=0x%02x)", rx1[0]);
				k_msleep(20);

				/* 2) Sensor-only output (Family 0x10 Index 0x00 = 0x01). */
				tx3[0] = 0x10;
				tx3[1] = 0x00;
				tx3[2] = 0x01;
				(void)max32664_i2c_transmit_retry(dev, tx3,
					sizeof(tx3), rx1, 1, 100, 3);
				LOG_DBG("RAW: output SENSOR_ONLY (status=0x%02x)", rx1[0]);

				/* 3) Enable MAX86141 (hub-side, source=0x00) */
				tx4[0] = 0x44;
				tx4[1] = HUB_SENSOR_IDX_MAX86141;
				tx4[2] = 0x01;
				tx4[3] = 0x00;
				(void)max32664_i2c_transmit_retry(dev, tx4,
					sizeof(tx4), rx1, 1, 100, 3);
				LOG_DBG("RAW: MAX86141 enabled (status=0x%02x)", rx1[0]);
				k_msleep(20);

				/* 4) Enable accel (hub-side or host-fed) */
				tx4[1] = 0x04; /* HUB_SENSOR_IDX_LIS2DS12 */
				tx4[3] = cfg->accel_external ? 0x01 : 0x00;
				(void)max32664_i2c_transmit_retry(dev, tx4,
					sizeof(tx4), rx1, 1, 100, 3);
				LOG_DBG("RAW: LIS2DS12 enabled (ext=%d, status=0x%02x)",
					cfg->accel_external, rx1[0]);
				k_msleep(20);

				/* Do NOT enable WHRM (0x52 … 0x01) — host owns AFE. */

				if (report_period_cmd_supported) {
					/* AN6924: 0x10/0x02 report period (40ms * N). */
					tx3[0] = 0x10;
					tx3[1] = 0x02;
					tx3[2] = 0x01;
					(void)max32664_i2c_transmit_retry(dev, tx3,
						sizeof(tx3), rx1, 1, 10, 3);
					LOG_DBG("RAW: report period=40ms (status=0x%02x)",
						rx1[0]);
				} else {
					LOG_DBG("RAW: report period skipped on FW 30.2.2");
				}

				/* 5) Host AFE ownership via Family 0x40 — restore map + LEDs. */
				{
					uint8_t sys_ctrl = 0U;
					uint8_t led_seq1 = 0U;
					int afe_reg_err = 0;

					afe_reg_err |= max86141_read_reg(dev,
						MAX86141_REG_SYSTEM_CTRL, &sys_ctrl);
					afe_reg_err |= max86141_read_reg(dev,
						MAX86141_REG_LED_SEQ1, &led_seq1);

					if (afe_reg_err) {
						LOG_WRN("RAW: AFE sanity read failed; reapplying MAX86141 RAW config");
						(void)max32664_raw_configure_ppg(dev);
					} else if (((sys_ctrl & BIT(1)) != 0U) ||
						   (led_seq1 != MAX86141_LED_SEQ1_PRODUCT)) {
						LOG_WRN("RAW: AFE state mismatched (SYS=0x%02x SEQ1=0x%02x); reapplying config",
							sys_ctrl, led_seq1);
						(void)max32664_raw_configure_ppg(dev);
					} else {
						LOG_DBG("RAW: AFE SEQ ok (SYS=0x%02x SEQ1=0x%02x)",
							sys_ctrl, led_seq1);
					}

					(void)max86141_ppg_leds_set(dev, true,
								    &data->raw_runtime.leds_enabled);
				}
			}

			data->sampling = true;
			k_work_schedule(&data->poll_work, K_NO_WAIT);
			(void)max32664_accel_feeder_start(dev);
			return 0;
		}
		
#elif defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
		/*  PASSTHROUGH MODE: Channel-specific independent control 
		 * Allow application to control PPG and accelerometer independently.
		 * Channel determines which child sensor is controlled:
		 * - SENSOR_CHAN_IR/RED/GREEN  PPG sensor only
		 * - SENSOR_CHAN_ACCEL_*  Accelerometer only
		 * - SENSOR_CHAN_ALL  Both sensors (application controls both)
		 */
		const struct device *ppg_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(max86141_afe));
		const struct device *accel_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(accel_sensor));
		int ret = 0;
		
		/* Determine target sensor based on channel */
		bool control_ppg = false;
		bool control_accel = false;
		
		switch (chan) {
		case SENSOR_CHAN_IR:
		case SENSOR_CHAN_RED:
		case SENSOR_CHAN_GREEN:
			control_ppg = true;
			break;
			
		case SENSOR_CHAN_ACCEL_XYZ:
		case SENSOR_CHAN_ACCEL_X:
		case SENSOR_CHAN_ACCEL_Y:
		case SENSOR_CHAN_ACCEL_Z:
			control_accel = true;
			break;
			
		case SENSOR_CHAN_ALL:
			/* Control both sensors */
			control_ppg = true;
			control_accel = true;
			break;
			
		default:
			LOG_WRN("Unknown channel %d - no sensor controlled", chan);
			return -EINVAL;
		}
		
		/* Control PPG sensor if requested */
		if (control_ppg && ppg_dev && device_is_ready(ppg_dev)) {
			if (val->val1 == 0) {
				LOG_INF("Stopping PPG sensor...");
			} else {
				LOG_INF("Starting PPG sensor...");
			}
			ret = sensor_attr_set(ppg_dev, chan, attr, val);
			if (ret && ret != -ENOTSUP) {
				LOG_ERR("PPG sensor control failed: %d", ret);
				return ret;
			}
		}
		
		/* Control accelerometer if requested */
		if (control_accel && accel_dev && device_is_ready(accel_dev)) {
			if (val->val1 == 0) {
				LOG_INF("Stopping accelerometer...");
			} else {
				LOG_INF("Starting accelerometer...");
			}
			ret = sensor_attr_set(accel_dev, chan, attr, val);
			if (ret && ret != -ENOTSUP) {
				LOG_ERR("Accelerometer control failed: %d", ret);
				return ret;
			}
		}
		
		/* Update MAX32664 sampling flag for consistency */
		if (val->val1 == 0) {
			data->sampling = false;
			(void)max32664_accel_feeder_stop(dev);
		} else {
			data->sampling = true;
			(void)max32664_accel_feeder_start(dev);
		}
		
		LOG_INF("PASSTHROUGH mode: Sensor control complete (PPG=%d, Accel=%d)",
			control_ppg, control_accel);
		return 0;
		
#else
		/*  HUB MODE: Unified hub control  */
		if (val->val1 == 0) {
			(void)max32664_accel_feeder_stop(dev);
			return max32664_runtime_hub_stop_sampling(dev);
		} else {
			int ret = max32664_runtime_hub_start_sampling(dev);
			if (ret == 0) {
				(void)max32664_accel_feeder_start(dev);
			}
			return ret;
		}
#endif /* PASSTHROUGH/RAW vs HUB mode */
	}
	
	/* SENSOR_ATTR_MAX32664_ACCEL_DATA: thin wrapper over max32664_feed_accel().
	 * The sensor_value array carries raw int16 samples in val[*].val1; no
	 * rescaling is performed so the application can match the hub's expected
	 * encoding directly. */
	if ((enum sensor_attribute)attr ==
	    (enum sensor_attribute)SENSOR_ATTR_MAX32664_ACCEL_DATA) {
		if (val == NULL) {
			LOG_ERR("NULL accelerometer data");
			return -EINVAL;
		}
		return max32664_feed_accel(dev,
					   (int16_t)val[0].val1,
					   (int16_t)val[1].val1,
					   (int16_t)val[2].val1);
	}
	
	return -ENOTSUP;
}

/* ============================================================================
 * Public Sensor Ecosystem API Implementation
 * ============================================================================
 * These functions implement the unified sensor ecosystem API defined in
 * include/zephyr/drivers/sensor/max32664.h
 * 
 * Note: AFE and accelerometer functions are in max32664_afe.c and max32664_accel.c
 * ============================================================================ */

/**
 * @brief Get detected hub variant (A/B/C/D)
 */
enum max32664_variant_type max32664_get_variant(const struct device *dev)
{
	struct max32664_data *data = dev->data;
	
	/* Map internal variant enum to public enum */
	switch (data->variant) {
	case MAX32664_VARIANT_A:
		return MAX32664_VARIANT_TYPE_A;
	case MAX32664_VARIANT_B:
		return MAX32664_VARIANT_TYPE_B;
	case MAX32664_VARIANT_C:
		return MAX32664_VARIANT_TYPE_C;
	case MAX32664_VARIANT_D:
		return MAX32664_VARIANT_TYPE_D;
	default:
		return MAX32664_VARIANT_TYPE_UNKNOWN;
	}
}

/**
 * @brief Get detected AFE type
 */
enum max32664_afe_type max32664_get_afe_type(const struct device *dev)
{
	struct max32664_data *data = dev->data;
	
	switch (data->afe_whoami) {
	case MAX32664_AFE_WHOAMI_MAX86141:
		return MAX32664_AFE_MAX86141;
	case MAX32664_AFE_WHOAMI_MAX86161:
		return MAX32664_AFE_MAX86161;
	case MAX32664_AFE_WHOAMI_MAX3010X: /* MAX30101/30102/30105 share Part ID 0x15 */
		return MAX32664_AFE_MAX30101;
	default:
		return MAX32664_AFE_UNKNOWN;
	}
}

/**
 * @brief Get AFE WHOAMI value
 */
uint8_t max32664_get_afe_whoami(const struct device *dev)
{
	struct max32664_data *data = dev->data;
	return data->afe_whoami;
}

/**
 * @brief Get hub firmware version
 */
int max32664_get_firmware_version(const struct device *dev,
				  uint8_t *major, uint8_t *minor, uint8_t *patch)
{
	struct max32664_data *data = dev->data;
	
	if (major) {
		*major = (data->firmware_version >> 16) & 0xFF;
	}
	if (minor) {
		*minor = (data->firmware_version >> 8) & 0xFF;
	}
	if (patch) {
		*patch = data->firmware_version & 0xFF;
	}
	
	return 0;
}

int max32664_get_bootloader_io(const struct device *dev,
			      struct max32664_bootloader_io *io)
{
	const struct max32664_config *cfg;

	if (dev == NULL || io == NULL) {
		return -EINVAL;
	}

	cfg = dev->config;
	io->bus = cfg->bus;
	io->int_gpio = cfg->int_gpio;
	io->rstn_gpio = cfg->rstn_gpio;

	return 0;
}

/* Note: AFE functions (max32664_afe_read_reg, max32664_afe_write_reg, etc.)
 * are now in max32664_afe.c
 * 
 * Note: Accelerometer functions (max32664_accel_read, max32664_accel_read_reg, etc.)
 * are now in max32664_accel.c
 */

/**
 * @brief Set hub operating mode (not implemented - mode is compile-time)
 * 
 * Note: Operating mode (HUB/RAW/PASSTHROUGH) is set at compile time via
 * Kconfig (CONFIG_MAX32664_MODE_*) and cannot be changed at runtime.
 * The driver architecture requires different code paths for each mode.
 * 
 * @param dev MAX32664 device pointer
 * @param mode Target operating mode (ignored)
 * @return -ENOTSUP (not supported)
 */
int max32664_set_mode(const struct device *dev, enum max32664_mode mode)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(mode);
	
	LOG_WRN("Runtime mode switching not supported (mode is compile-time via Kconfig)");
	return -ENOTSUP;
}

/**
 * @brief Get compile-time operating mode (Kconfig choice).
 */
enum max32664_mode max32664_get_mode(const struct device *dev)
{
	ARG_UNUSED(dev);
#if defined(CONFIG_MAX32664_MODE_RAW)
	return MAX32664_MODE_RAW;
#elif defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
	return MAX32664_MODE_PASSTHROUGH;
#elif defined(CONFIG_MAX32664_MODE_HUB)
	return MAX32664_MODE_HUB;
#else
#error "Exactly one of CONFIG_MAX32664_MODE_{RAW,PASSTHROUGH,HUB} must be selected"
#endif
}

/**
 * @brief Start hub algorithms (HUB mode only; mirrors algo_stop).
 */
int max32664_algo_start(const struct device *dev)
{
#if defined(CONFIG_MAX32664_MODE_HUB)
	struct max32664_data *data = dev->data;
	uint8_t tx[3], rx[1];
	int ret;

	if (data->sampling) {
		return 0;
	}

	/* Enable algorithm (Family 0x52, Index 0x07, Data 0x01) — inverse of stop. */
	tx[0] = 0x52;
	tx[1] = 0x07;
	tx[2] = 0x01;
	ret = max32664_i2c_transmit(dev, tx, 3, rx, 1, 120);
	if (ret || rx[0] != 0x00) {
		LOG_ERR("Failed to start algorithm");
		return ret ? ret : -EIO;
	}

	data->sampling = true;
	return 0;
#else
	ARG_UNUSED(dev);
	LOG_WRN("algo_start only applicable in HUB mode");
	return -ENOTSUP;
#endif /* CONFIG_MAX32664_MODE_HUB */
}

/**
 * @brief Stop hub algorithms (HUB mode only)
 * 
 * Disables algorithm processing in HUB mode. Not applicable to RAW/PASSTHROUGH modes.
 * 
 * @param dev MAX32664 device pointer
 * @return 0 on success, negative errno on failure
 */
int max32664_algo_stop(const struct device *dev)
{
#ifdef CONFIG_MAX32664_MODE_HUB
	struct max32664_data *data = dev->data;
	uint8_t tx[3], rx[1];
	int ret;

	if (!data->sampling) {
		return 0;  /* Already stopped */
	}

	/* Disable algorithm (Family 0x52, Index 0x07, Data 0x00) */
	tx[0] = 0x52;
	tx[1] = 0x07;
	tx[2] = 0x00;
	ret = max32664_i2c_transmit(dev, tx, 3, rx, 1, 120);
	if (ret || rx[0] != 0x00) {
		LOG_ERR("Failed to stop algorithm");
		return ret ? ret : -EIO;
	}

	data->sampling = false;
	return 0;
#else
	ARG_UNUSED(dev);
	LOG_WRN("algo_stop only applicable in HUB mode");
	return -ENOTSUP;
#endif /* CONFIG_MAX32664_MODE_HUB */
}

/**
 * @brief Check if hub algorithms are running
 */
bool max32664_algo_is_running(const struct device *dev)
{
	struct max32664_data *data = dev->data;
	return data->sampling;
}

/* ============================================================================
 * Mode-Aware Sensor API Dispatch
 * ============================================================================ */

/**
 * @brief Sample fetch - mode-aware dispatch
 *
 * Dispatches to appropriate implementation based on operating mode:
 * - HUB mode: Read algorithm results from hub FIFO
 * - PASSTHROUGH/RAW mode: Proxy to child sensor devices
 *
 * @param dev Device pointer
 * @param chan Sensor channel
 * @return 0 on success, negative error code on failure
 */
static int max32664_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	struct max32664_data *data = dev->data;
	
	if (!data->config.initialized) {
		return -ENODEV;
	}
	
#if defined(CONFIG_MAX32664_MODE_RAW)
	/*
	 * Parent hub must not parse a hardcoded Family 0x12 frame — payload
	 * size is dynamic (see max32664_raw / max86141_ppg). Proxy to the
	 * product PPG child (ncs,ppg-primary) when present.
	 */
	{
		const struct device *ppg_dev = NULL;
		const struct device *accel_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(accel_sensor));
		int ret = -ENOTSUP;

#if DT_HAS_CHOSEN(ncs_ppg_primary)
		ppg_dev = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(ncs_ppg_primary));
#endif
		if (ppg_dev == NULL) {
			ppg_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(max86141_afe));
		}

		switch (chan) {
		case SENSOR_CHAN_IR:
		case SENSOR_CHAN_RED:
		case SENSOR_CHAN_GREEN:
			if (ppg_dev && device_is_ready(ppg_dev)) {
				return sensor_sample_fetch_chan(ppg_dev, chan);
			}
			return -ENODEV;
		case SENSOR_CHAN_ACCEL_XYZ:
		case SENSOR_CHAN_ACCEL_X:
		case SENSOR_CHAN_ACCEL_Y:
		case SENSOR_CHAN_ACCEL_Z:
			if (accel_dev && device_is_ready(accel_dev)) {
				return sensor_sample_fetch_chan(accel_dev, chan);
			}
			return -ENODEV;
		case SENSOR_CHAN_ALL:
			if (ppg_dev && device_is_ready(ppg_dev)) {
				ret = sensor_sample_fetch(ppg_dev);
			}
			if (accel_dev && device_is_ready(accel_dev)) {
				int aret = sensor_sample_fetch(accel_dev);

				ret = (ret && ret != -ENOTSUP) ? ret : aret;
			}
			return (ret == -ENOTSUP) ? -ENODEV : ret;
		default:
			ARG_UNUSED(data);
			return -ENOTSUP;
		}
	}

#elif defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
	/* PASSTHROUGH mode: Proxy to child sensors (direct register access) */
	const struct device *ppg_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(max86141_afe));
	const struct device *accel_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(accel_sensor));
	int ret = 0;
	
	switch (chan) {
	case SENSOR_CHAN_IR:
	case SENSOR_CHAN_RED:
	case SENSOR_CHAN_GREEN:
		/* Fetch PPG data from child sensor */
		if (ppg_dev && device_is_ready(ppg_dev)) {
			ret = sensor_sample_fetch_chan(ppg_dev, chan);
		} else {
			LOG_WRN("PPG sensor not ready");
			ret = -ENODEV;
		}
		break;
		
	case SENSOR_CHAN_ACCEL_XYZ:
	case SENSOR_CHAN_ACCEL_X:
	case SENSOR_CHAN_ACCEL_Y:
	case SENSOR_CHAN_ACCEL_Z:
		/* Fetch accel data from child sensor */
		if (accel_dev && device_is_ready(accel_dev)) {
			ret = sensor_sample_fetch_chan(accel_dev, chan);
		} else {
			LOG_WRN("Accel sensor not ready");
			ret = -ENODEV;
		}
		break;
		
	case SENSOR_CHAN_ALL:
		/* Fetch all sensors */
		if (ppg_dev && device_is_ready(ppg_dev)) {
			int ppg_ret = sensor_sample_fetch(ppg_dev);
			if (ppg_ret) {
				LOG_WRN("PPG fetch failed: %d", ppg_ret);
				ret = ppg_ret;
			}
		}
		if (accel_dev && device_is_ready(accel_dev)) {
			int accel_ret = sensor_sample_fetch(accel_dev);
			if (accel_ret) {
				LOG_WRN("Accel fetch failed: %d", accel_ret);
				ret = ret ? ret : accel_ret;  /* Return first error */
			}
		}
		break;
		
	default:
		LOG_WRN("Unsupported channel %d in PASSTHROUGH mode", chan);
		ret = -ENOTSUP;
	}
	
	return ret;
#else
	/* HUB mode: Poll hub or wait for interrupt */
	int ret;
	
	/* If polling mode and not started, start polling */
	if (!data->config.interrupt_mode && !data->sampling) {
		k_work_schedule(&data->poll_work, K_NO_WAIT);
		data->sampling = true;
	}
	
	/* Wait for data ready (timeout based on poll interval) */
	k_timeout_t timeout = data->config.interrupt_mode ?
		K_SECONDS(1) : K_MSEC(data->config.poll_interval_ms * 2);
	
	ret = k_sem_take(&data->sample_sem, timeout);
	if (ret) {
		LOG_WRN("Sample fetch timeout");
		return -ETIMEDOUT;
	}
	
	/* Parse hub data */
	ret = max32664_sampling_parse_hub_data(dev);
	if (ret) {
		return ret;
	}
	
	return 0;
#endif
}

/**
 * @brief Channel get - mode-aware dispatch
 *
 * Dispatches to appropriate implementation based on operating mode:
 * - HUB mode: Return algorithm results (HR, SpO2, etc.)
 * - PASSTHROUGH/RAW mode: Proxy to child sensor devices for raw data
 *
 * @param dev Device pointer
 * @param chan Sensor channel to read
 * @param val Pointer to sensor_value structure to fill
 * @return 0 on success, -ENOTSUP for unsupported channel
 */
static int max32664_channel_get(const struct device *dev,
				 enum sensor_channel chan,
				 struct sensor_value *val)
{
	struct max32664_data *data = dev->data;
	
	if (!data->config.initialized) {
		return -ENODEV;
	}
	
#if defined(CONFIG_MAX32664_MODE_RAW)
	/*
	 * RAW: SCD/SpO2-cal are hub-algo features; host DSP uses proximity/
	 * quality instead (AN6924 SCD/cal reports apply to HUB mode only).
	 * Proxy optical/accel channels to child devices.
	 */
	{
		const struct device *ppg_dev = NULL;
		const struct device *accel_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(accel_sensor));

#if DT_HAS_CHOSEN(ncs_ppg_primary)
		ppg_dev = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(ncs_ppg_primary));
#endif
		if (ppg_dev == NULL) {
			ppg_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(max86141_afe));
		}

		if (chan == SENSOR_CHAN_IR || chan == SENSOR_CHAN_RED ||
		    chan == SENSOR_CHAN_GREEN) {
			if (ppg_dev && device_is_ready(ppg_dev)) {
				return sensor_channel_get(ppg_dev, chan, val);
			}
			return -ENODEV;
		}
		if (chan >= SENSOR_CHAN_ACCEL_X && chan <= SENSOR_CHAN_ACCEL_XYZ) {
			if (accel_dev && device_is_ready(accel_dev)) {
				return sensor_channel_get(accel_dev, chan, val);
			}
			return -ENODEV;
		}
		ARG_UNUSED(data);
		return -ENOTSUP;
	}

#elif defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
	/* PASSTHROUGH mode: Proxy to child sensors */
	const struct device *ppg_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(max86141_afe));
	const struct device *accel_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(accel_sensor));
	
	/* Check if this is a PPG channel */
	if (chan == SENSOR_CHAN_IR || chan == SENSOR_CHAN_RED || chan == SENSOR_CHAN_GREEN) {
		if (ppg_dev && device_is_ready(ppg_dev)) {
			return sensor_channel_get(ppg_dev, chan, val);
		}
		LOG_WRN("PPG sensor not ready");
		return -ENODEV;
	}
	
	/* Check if this is an accelerometer channel */
	if (chan >= SENSOR_CHAN_ACCEL_X && chan <= SENSOR_CHAN_ACCEL_XYZ) {
		if (accel_dev && device_is_ready(accel_dev)) {
			return sensor_channel_get(accel_dev, chan, val);
		}
		LOG_WRN("Accel sensor not ready");
		return -ENODEV;
	}
	
	/* Unsupported channel for PASSTHROUGH mode */
	LOG_WRN("Channel %d not supported in PASSTHROUGH mode", chan);
		return -ENOTSUP;
#else
	/* HUB mode: Return algorithm results */
	return max32664_sampling_channel_get_hub(dev, chan, val);
#endif
}

/**
 * @brief Get sensor attribute
 *
 * Zephyr sensor API function. Query current sensor configuration.
 *
 * @param dev Device pointer
 * @param chan Sensor channel
 * @param attr Attribute to query
 * @param val Pointer to value structure to fill
 * @return 0 on success, -ENOTSUP for unsupported attribute
 */
static int max32664_attr_get(const struct device *dev,
			      enum sensor_channel chan,
			      enum sensor_attribute attr,
			      struct sensor_value *val)
{
	struct max32664_data *data = dev->data;
	
	ARG_UNUSED(chan);
	
	if (!data->config.initialized) {
		return -ENODEV;
	}
	
	/* Cast the switch expression to `int` so SENSOR_ATTR_MAX32664_* (which
	 * live in a private range above the public `enum sensor_attribute`)
	 * can appear as case labels without tripping -Wswitch. */
	switch ((int)attr) {
	case SENSOR_ATTR_SAMPLING_FREQUENCY:
		/* Return sampling state: 1 = running, 0 = stopped */
		val->val1 = data->sampling ? 1 : 0;
		val->val2 = 0;
		return 0;
		
#if !defined(CONFIG_MAX32664_MODE_RAW) && !defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
	/* HUB mode specific attributes (private range above the public enum). */
	case SENSOR_ATTR_MAX32664_VARIANT:
		/* Return detected variant (A/B/C/D) */
		val->val1 = (int32_t)data->variant;
		val->val2 = 0;
		return 0;
		
	case SENSOR_ATTR_MAX32664_AFE_TYPE:
		/* Return detected AFE type */
		val->val1 = (int32_t)data->afe_whoami;
		val->val2 = 0;
		return 0;
#endif
		
	default:
		/* Mode-specific or unsupported attribute */
#if defined(CONFIG_MAX32664_MODE_RAW) || defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
		/* In RAW/PASSTHROUGH mode, proxy to Watch PPG child (ncs,ppg-primary) */
		const struct device *ppg_dev = NULL;
		const struct device *accel_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(accel_sensor));

#if DT_HAS_CHOSEN(ncs_ppg_primary)
		ppg_dev = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(ncs_ppg_primary));
#endif

		/* Try PPG sensor first */
		if (ppg_dev && device_is_ready(ppg_dev)) {
			int ret = sensor_attr_get(ppg_dev, chan, attr, val);
			if (ret != -ENOTSUP) {
				return ret;
			}
		}

		/* Try accelerometer */
		if (accel_dev && device_is_ready(accel_dev)) {
			int ret = sensor_attr_get(accel_dev, chan, attr, val);
			if (ret != -ENOTSUP) {
				return ret;
			}
		}
#endif
		return -ENOTSUP;
	}
}

/* ============================================================================
 * Sleep / Shutdown / Soft-reset (AN6924 Table 25 ~p.85) + PM
 * ============================================================================ */

int max32664_sleep(const struct device *dev)
{
	struct max32664_data *data = dev->data;
	const struct max32664_config *cfg = dev->config;
	int ret = 0;

	/*
	 * Idle deep-sleep path: stop host-side sampling and leave MFIO high.
	 * Hub enters deep sleep when idle + MFIO high (AN6924); no ShutdownLP.
	 */
#if defined(CONFIG_MAX32664_MODE_HUB)
	ret = max32664_runtime_hub_stop_sampling(dev);
#elif defined(CONFIG_MAX32664_MODE_RAW)
	data->sampling = false;
	(void)k_work_cancel_delayable(&data->poll_work);
	if (data->config.interrupt_mode) {
		(void)k_work_cancel(&data->interrupt_work);
	}
#if IS_ENABLED(CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER)
	(void)max32664_accel_feeder_stop(dev);
#endif
	ret = max32664_try_output_mode(dev, 0x00, 0x00); /* PAUSE */
#else /* PASSTHROUGH */
	data->sampling = false;
	ret = max32664_try_output_mode(dev, 0x00, 0x00);
#endif

	if (cfg->int_gpio.port != NULL && device_is_ready(cfg->int_gpio.port)) {
		/* Ensure MFIO inactive-high so hub may deep-sleep. */
		(void)gpio_pin_configure_dt(&cfg->int_gpio, GPIO_OUTPUT_INACTIVE);
		(void)gpio_pin_set_dt(&cfg->int_gpio, 0);
		data->config.interrupt_mode = false;
	}

	LOG_INF("Hub sleep/idle requested (ret=%d)", ret);
	return ret;
}

int max32664_shutdown(const struct device *dev, bool retain_ram)
{
	uint8_t tx[3];
	uint8_t rx[1];
	int ret;

	/* Stop sampling first so FIFO/feeder threads release the bus. */
	(void)max32664_sleep(dev);

	/* AN6924 Table 25: ShutdownLP0=0x01, ShutdownLP1=0x10. */
	tx[0] = 0x01;
	tx[1] = 0x00;
	tx[2] = retain_ram ? 0x10 : 0x01;
	ret = max32664_i2c_transmit(dev, tx, 3, rx, 1, 50);
	if (ret) {
		LOG_ERR("ShutdownLP%c failed: %d", retain_ram ? '1' : '0', ret);
		return ret;
	}

	LOG_INF("Hub ShutdownLP%c accepted (restart via %s)",
		retain_ram ? '1' : '0',
		retain_ram ? "RSTN or power cycle" : "power cycle");
	return 0;
}

int max32664_soft_reset(const struct device *dev)
{
	uint8_t tx[3] = {0x01, 0x00, 0x02};
	uint8_t rx[1];
	int ret;

	(void)max32664_sleep(dev);

	ret = max32664_i2c_transmit(dev, tx, 3, rx, 1, 2000);
	if (ret) {
		return ret;
	}

	k_msleep(MAX32664_APP_INIT_MS);
	return 0;
}

#if defined(CONFIG_MAX32664_HUB_SPO2_CAL_REPORT)
int max32664_read_spo2_calibration(const struct device *dev, int32_t a_b_c_x1e5[3])
{
	uint8_t tx[3] = {0x51, 0x07, 0x00};
	uint8_t rx[1 + 12];
	int ret;

	if (a_b_c_x1e5 == NULL) {
		return -EINVAL;
	}

	ret = max32664_i2c_transmit(dev, tx, sizeof(tx), rx, sizeof(rx), 50);
	if (ret) {
		return ret;
	}

	/* UG-6924: three big-endian int32 coefficients × 1e5. */
	for (int i = 0; i < 3; i++) {
		const uint8_t *p = &rx[1 + (i * 4)];

		a_b_c_x1e5[i] = (int32_t)(((uint32_t)p[0] << 24) |
					  ((uint32_t)p[1] << 16) |
					  ((uint32_t)p[2] << 8) |
					  (uint32_t)p[3]);
	}
	return 0;
}

int max32664_write_spo2_calibration(const struct device *dev,
				    const int32_t a_b_c_x1e5[3])
{
	uint8_t tx[15];
	uint8_t rx[1];
	int ret;

	if (a_b_c_x1e5 == NULL) {
		return -EINVAL;
	}

	tx[0] = 0x50;
	tx[1] = 0x07;
	tx[2] = 0x00;
	for (int i = 0; i < 3; i++) {
		uint32_t v = (uint32_t)a_b_c_x1e5[i];

		tx[3 + i * 4] = (uint8_t)(v >> 24);
		tx[4 + i * 4] = (uint8_t)(v >> 16);
		tx[5 + i * 4] = (uint8_t)(v >> 8);
		tx[6 + i * 4] = (uint8_t)v;
	}

	ret = max32664_i2c_transmit(dev, tx, sizeof(tx), rx, 1, 50);
	return ret;
}
#else /* !CONFIG_MAX32664_HUB_SPO2_CAL_REPORT */
int max32664_read_spo2_calibration(const struct device *dev, int32_t a_b_c_x1e5[3])
{
	ARG_UNUSED(dev);
	ARG_UNUSED(a_b_c_x1e5);
	return -ENOTSUP;
}

int max32664_write_spo2_calibration(const struct device *dev,
				    const int32_t a_b_c_x1e5[3])
{
	ARG_UNUSED(dev);
	ARG_UNUSED(a_b_c_x1e5);
	return -ENOTSUP;
}
#endif /* CONFIG_MAX32664_HUB_SPO2_CAL_REPORT */

#if defined(CONFIG_PM_DEVICE)
static int max32664_pm_action(const struct device *dev, enum pm_device_action action)
{
	int ret = 0;

	switch (action) {
	case PM_DEVICE_ACTION_SUSPEND:
		/* Prefer idle sleep over ShutdownLP so RESUME need not power-cycle. */
		ret = max32664_sleep(dev);
		break;
	case PM_DEVICE_ACTION_RESUME:
		/*
		 * After idle sleep: re-run GPIO/output setup for the compile-time
		 * mode. Full ShutdownLP recovery requires RSTN/power — not handled
		 * here (call max32664_soft_reset / HW reset from the app).
		 */
		ret = max32664_post_init_setup(dev, (struct max32664_data *)dev->data);
		break;
	default:
		ret = -ENOTSUP;
		break;
	}

	return ret;
}
#endif /* CONFIG_PM_DEVICE */

/* ============================================================================
 * Sensor Driver API Structure
 * ============================================================================ */

static const struct sensor_driver_api max32664_api = {
	.sample_fetch = max32664_sample_fetch,
	.channel_get = max32664_channel_get,
	.attr_set = max32664_attr_set,
	.attr_get = max32664_attr_get,
};

/* ============================================================================
 * Device Tree Instantiation
 * ============================================================================
 * Unified instantiation for all MAX32664 variants.
 * Runtime variant = FW major, cross-checked against DT variant-hint.
 * ============================================================================ */

/* Helper macro to get vin-supply regulator device if specified in DT */
#define MAX32664_VIN_SUPPLY(n) \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, vin_supply), \
		(DEVICE_DT_GET(DT_PHANDLE(DT_DRV_INST(n), vin_supply))), \
		(NULL))

/* accel-source enum: 0 = "internal" (hub drives its own SPI accel),
 *                    1 = "external" (host reads LIS2DS12 via passthrough
 *                                    and feeds samples back via 0x14/0x04).
 * Defaults to 0 when the property is absent. */
#define MAX32664_ACCEL_EXTERNAL(n) \
	(DT_INST_ENUM_IDX_OR(n, accel_source, 0) == 1)

/* variant-hint yaml enum: 0 = "gwec", 1 = "gwed"; property absent → NONE */
#define MAX32664_VARIANT_HINT(n) \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, variant_hint), \
		((DT_INST_ENUM_IDX(n, variant_hint) == 0) ? \
			MAX32664_HINT_GWEC : MAX32664_HINT_GWED), \
		(MAX32664_HINT_NONE))

#define MAX32664_INIT(n) \
	static struct max32664_data max32664_data_##n; \
	\
	static const struct max32664_config max32664_config_##n = { \
		.bus = I2C_DT_SPEC_INST_GET(n), \
		.int_gpio = GPIO_DT_SPEC_INST_GET_OR(n, interrupt_gpios, {0}), \
		.rstn_gpio = GPIO_DT_SPEC_INST_GET_OR(n, reset_gpios, {0}), \
		.vin_supply = MAX32664_VIN_SUPPLY(n), \
		.poll_interval_ms = DT_INST_PROP_OR(n, poll_interval_ms, \
				     CONFIG_MAX32664_POLLING_INTERVAL_MS), \
		/* Interrupt mode is automatically enabled when MFIO is described in DT. */ \
		.interrupt_mode = DT_INST_NODE_HAS_PROP(n, interrupt_gpios), \
		.accel_external = MAX32664_ACCEL_EXTERNAL(n), \
		.variant_hint = MAX32664_VARIANT_HINT(n), \
	}; \
	\
	IF_ENABLED(CONFIG_PM_DEVICE, (PM_DEVICE_DT_INST_DEFINE(n, max32664_pm_action);)) \
	\
	DEVICE_DT_INST_DEFINE(n, \
			      max32664_init, \
			      COND_CODE_1(CONFIG_PM_DEVICE, (PM_DEVICE_DT_INST_GET(n)), (NULL)), \
			      &max32664_data_##n, \
			      &max32664_config_##n, \
			      POST_KERNEL, \
			      CONFIG_SENSOR_INIT_PRIORITY, \
			      &max32664_api);

/* Instantiate for base compatible */
DT_INST_FOREACH_STATUS_OKAY(MAX32664_INIT)

/* Also instantiate for variant-specific compatibles */
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT maxim_max32664_gwec
DT_INST_FOREACH_STATUS_OKAY(MAX32664_INIT)

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT maxim_max32664_gwed
DT_INST_FOREACH_STATUS_OKAY(MAX32664_INIT)
