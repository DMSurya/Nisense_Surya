/**
 * @file max86141_ppg.c
 * @brief MAX86141 PPG Sensor - Zephyr Sensor API via MAX32664 Hub Passthrough
 *
 * This driver exposes the MAX86141 PPG sensor as a standard Zephyr sensor device.
 * It accesses the MAX86141 through the MAX32664 hub's passthrough mode.
 *
 * Supported Channels:
 *   - SENSOR_CHAN_IR: Infrared LED PPG signal (LED3)
 *   - SENSOR_CHAN_RED: Red LED PPG signal (LED2)
 *   - SENSOR_CHAN_GREEN: Green LED PPG signal (LED1, always driven)
 *
 * Usage:
 *   const struct device *ppg_dev = DEVICE_DT_GET(DT_NODELABEL(max86141_ppg));
 *   sensor_sample_fetch(ppg_dev);
 *   sensor_channel_get(ppg_dev, SENSOR_CHAN_IR, &ir_val);
 *   sensor_channel_get(ppg_dev, SENSOR_CHAN_RED, &red_val);
 *
 * Note: Built when CONFIG_MAX32664_RAW_PPG_SENSOR=y (RAW mode; see Kconfig).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT maxim_max86141_ppg

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include "max32664.h"
#include "max32664_priv.h"
#include "max86141_regs.h"

LOG_MODULE_REGISTER(max86141_ppg, CONFIG_SENSOR_LOG_LEVEL);

/* Interrupt enable bits (INT_ENABLE1) */
#define MAX86141_INT_EN_A_FULL      0xC0	/* FIFO Full and Data rdy */
#define MAX86141_INT_EN_DATA_RDY    0x40

/* ============================================================================
 * Driver Configuration
 * ============================================================================ */

struct max86141_ppg_config {
	const struct device *hub_dev;  /* Parent MAX32664 device */
	uint8_t led1_current;  /* LED1 current (0-255, 0.2mA/LSB) */
	uint8_t led2_current;  /* LED2 current */
	uint8_t led3_current;  /* LED3 current */
	uint8_t sample_rate;   /* Sample rate code (see datasheet) */
	uint8_t adc1_range;     /* PPG Channel1 ADC range code */
	uint8_t adc2_range;     /* PPG Channel2 ADC range code */
	uint8_t sample_avg;    /* Sample averaging code */
	uint8_t fifo_a_full;   /* FIFO almost-full threshold (0-127) */
	bool fifo_rollover;    /* FIFO roll-over enable */
	bool timestamp_enable; /* PPG FIFO timestamp enable */
	bool a_full_interrupt; /* Use A_FULL interrupt instead of PPG_RDY */
};

/** Hub burst buffer: ~1.3 s @ 25 Hz; sized to fit nRF52840 RAM budget. */
#define MAX86141_PPG_QUEUE_LEN 32
/** Per-drain stack batch (keep small — runs on ppg_algo workqueue stack). */
#define MAX86141_PPG_DRAIN_BATCH 16

struct max86141_ppg_queued {
	uint32_t ir;
	uint32_t red;
	uint32_t green;
	uint32_t ambient;
};

struct max86141_ppg_data {
	uint32_t ir_raw;       /* Last IR sample (19-bit) */
	uint32_t red_raw;      /* Last Red sample (19-bit) */
	uint32_t green_raw;    /* Last Green sample (19-bit) */
	uint32_t ambient_raw;  /* Last ambient slot sample (19-bit) */
	uint8_t sample_count;  /* Samples read in last fetch */
	uint32_t empty_fifo_count; /* Consecutive no-data fetches */
	bool configured;       /* Configuration complete flag */
	struct max86141_ppg_queued fifo_buf[MAX86141_PPG_QUEUE_LEN];
	uint8_t fifo_count;
	uint8_t fifo_read_idx;
};

/* ============================================================================
 * Sensor API Implementation
 * ============================================================================ */

static void max86141_map_hub_sample(const struct max86141_ppg_config *cfg,
				    const struct max32664_ppg_sample *in,
				    struct max86141_ppg_queued *out)
{
	ARG_UNUSED(cfg);

	/* RAW/PASSTHROUGH: hub reports IR/Red/Green in fixed slot order.
	 * Do not remap channels here. */
	out->ir = in->ir;
	out->red = in->red;
	out->green = in->green;
	out->ambient = in->ambient;
}

static int max86141_hub_fifo_refill(const struct device *dev)
{
	const struct max86141_ppg_config *cfg = dev->config;
	struct max86141_ppg_data *data = dev->data;
	const struct device *hub = cfg->hub_dev;
	struct max32664_ppg_sample batch[MAX86141_PPG_DRAIN_BATCH];

	data->fifo_count = 0U;
	data->fifo_read_idx = 0U;

	while (data->fifo_count < MAX86141_PPG_QUEUE_LEN) {
		size_t batch_cap = MIN((size_t)(MAX86141_PPG_QUEUE_LEN - data->fifo_count),
				       ARRAY_SIZE(batch));
		int n;

#ifdef CONFIG_MAX32664_MODE_PASSTHROUGH
		uint32_t ir_val = 0U;
		uint32_t red_val = 0U;
		uint32_t green_val = 0U;
		int pt = max32664_passthrough_read_ppg_ex(hub, &ir_val, &red_val,
							  &green_val);

		if (pt < 0) {
			return pt;
		}
		if (pt == 0) {
			n = 0;
		} else {
			batch[0].ir = ir_val;
			batch[0].red = red_val;
			batch[0].green = green_val;
			batch[0].ambient = 0U;
			n = 1;
		}
#else
		n = max32664_raw_drain_ppg(hub, batch, batch_cap);
#endif

		if (n < 0) {
			return n;
		}
		if (n == 0) {
			break;
		}

		for (int i = 0; i < n; i++) {
			struct max86141_ppg_queued mapped;

			max86141_map_hub_sample(cfg, &batch[i], &mapped);

			if ((mapped.ir == 0U && mapped.red == 0U) ||
			    mapped.ir > 0x7FFFFU || mapped.red > 0x7FFFFU) {
				continue;
			}

			data->fifo_buf[data->fifo_count++] = mapped;
			if (data->fifo_count >= MAX86141_PPG_QUEUE_LEN) {
				break;
			}
		}

		if (data->fifo_count >= MAX86141_PPG_QUEUE_LEN) {
			break;
		}
	}

	return data->fifo_count > 0U ? 0 : -EAGAIN;
}

/**
 * @brief Fetch one PPG sample set from the hub output FIFO (RAW: Family 0x12)
 *
 * Returns 0 when a new sample was stored for channel_get. Returns -EAGAIN
 * when the hub has no sample to pop (matches max3010x_ppg and ppg_algo's
 * drain loop). Passthrough mode uses its own reader.
 */
static int max86141_ppg_sample_fetch(const struct device *dev,
				     enum sensor_channel chan)
{
	const struct max86141_ppg_config *cfg = dev->config;
	struct max86141_ppg_data *data = dev->data;
	int ret;

	ARG_UNUSED(chan);

	if (!device_is_ready(cfg->hub_dev)) {
		LOG_ERR("Hub device not ready");
		return -ENODEV;
	}

	if (data->fifo_read_idx < data->fifo_count) {
		const struct max86141_ppg_queued *q = &data->fifo_buf[data->fifo_read_idx++];

		data->ir_raw = q->ir;
		data->red_raw = q->red;
		data->green_raw = q->green;
		data->ambient_raw = q->ambient;
		data->sample_count = 1;
		if (data->empty_fifo_count > 0U) {
			data->empty_fifo_count = 0U;
		}
		return 0;
	}

	ret = max86141_hub_fifo_refill(dev);
	if (ret == -EAGAIN) {
		data->empty_fifo_count++;
		if (data->empty_fifo_count == 10U ||
		    (data->empty_fifo_count % 50U) == 0U) {
			LOG_WRN("No PPG samples popped (consecutive empty reads=%u)",
				data->empty_fifo_count);
		}
		data->sample_count = 0;
		return -EAGAIN;
	}
	if (ret) {
		return ret;
	}

	if (data->empty_fifo_count >= 10U) {
		LOG_INF("PPG data available after %u empty reads", data->empty_fifo_count);
	}
	data->empty_fifo_count = 0U;

	data->ir_raw = data->fifo_buf[0].ir;
	data->red_raw = data->fifo_buf[0].red;
	data->green_raw = data->fifo_buf[0].green;
	data->ambient_raw = data->fifo_buf[0].ambient;
	data->fifo_read_idx = 1U;
	data->sample_count = 1;
	return 0;
}

static int max86141_ppg_channel_get(const struct device *dev,
				    enum sensor_channel chan,
				    struct sensor_value *val)
{
	struct max86141_ppg_data *data = dev->data;

	switch (chan) {
	case SENSOR_CHAN_IR:
		val->val1 = data->ir_raw;
		val->val2 = 0;
		break;

	case SENSOR_CHAN_RED:
		val->val1 = data->red_raw;
		val->val2 = 0;
		break;

	case SENSOR_CHAN_GREEN:
		val->val1 = data->green_raw;
		val->val2 = 0;
		break;

	case SENSOR_CHAN_LIGHT:
		val->val1 = data->ambient_raw;
		val->val2 = 0;
		break;

	default:
		LOG_ERR("Unsupported channel: %d", chan);
		return -ENOTSUP;
	}

	return 0;
}

static int max86141_ppg_attr_set(const struct device *dev,
				 enum sensor_channel chan,
				 enum sensor_attribute attr,
				 const struct sensor_value *val)
{
	const struct max86141_ppg_config *cfg = dev->config;
	const struct device *hub = cfg->hub_dev;
	int ret;

	switch (attr) {
	case SENSOR_ATTR_SAMPLING_FREQUENCY:
		/* Note: the host-fed accelerometer feeder is NOT started/stopped
		 * here. Lifecycle is owned by ppg_algo_start/stop_measurement()
		 * (subsys/ppg_algo/ppg_algo.c) so that the feeder runs regardless.
		 *
		 * RAW mode: hub attr_set(SAMPLING_FREQUENCY) owns the sensor-only
		 * start/stop sequence (algo off, output 0x01, AFE/accel enable,
		 * host LED/SHDN via Family 0x40). Child SYSTEM_CTRL writes here
		 * would race that path — keep this a no-op in RAW. */
#if defined(CONFIG_MAX32664_MODE_RAW)
		if (val->val1 == 0) {
			LOG_INF("RAW: child AFE shutdown skipped; hub attr_set() owns start/stop");
		} else {
			LOG_INF("RAW: child AFE wakeup skipped; hub attr_set() owns start/stop");
		}
		return 0;
#else
		/* Non-RAW: directly control MAX86141 SYSTEM_CTRL via passthrough. */
		if (val->val1 == 0) {
			LOG_INF("Stopping PPG measurement");
			/* Enter shutdown (LEDs off, sensor idle) */
			ret = max86141_write_reg(hub, MAX86141_REG_SYSTEM_CTRL, 0x02);
		} else {
			LOG_INF("Starting PPG measurement");
			/* Clear any pending interrupts */
			uint8_t dummy;
			max86141_read_reg(hub, MAX86141_REG_INT_STATUS1, &dummy);
			max86141_read_reg(hub, MAX86141_REG_INT_STATUS2, &dummy);

			/* Exit shutdown mode: clear SHDN bit to start sampling */
			ret = max86141_write_reg(hub, MAX86141_REG_SYSTEM_CTRL, 0x00);
			if (ret) {
				LOG_ERR("Failed to start PPG sensor: %d", ret);
				return ret;
			}
			LOG_INF("PPG sensor started (LEDs on)");
		}
#endif /* CONFIG_MAX32664_MODE_RAW */
		break;

	case SENSOR_ATTR_OVERSAMPLING:
		/* Set sample averaging */
		ret = max86141_write_reg(hub, MAX86141_REG_PPG_CONFIG2,
					 val->val1 & 0x07);
		break;

	default:
		return -ENOTSUP;
	}

	return ret;
}

static int max86141_ppg_attr_get(const struct device *dev,
				 enum sensor_channel chan,
				 enum sensor_attribute attr,
				 struct sensor_value *val)
{
	const struct max86141_ppg_config *cfg = dev->config;
	const struct device *hub = cfg->hub_dev;
	uint8_t reg_val;
	int ret;

	switch (attr) {
	case SENSOR_ATTR_SAMPLING_FREQUENCY:
		ret = max86141_read_reg(hub, MAX86141_REG_PPG_CONFIG1, &reg_val);
		if (ret == 0) {
			val->val1 = (reg_val >> 2) & 0x0F;
			val->val2 = 0;
		}
		break;

	default:
		return -ENOTSUP;
	}

	return ret;
}

static uint16_t max86141_led_current_ma(uint8_t led_drv,
                                        uint8_t led_rge)
{
    uint16_t lsb_uA;

    switch (led_rge) {
    case 0x00: lsb_uA = 120; break;  /* 0.12 mA */
    case 0x01: lsb_uA = 240; break;  /* 0.24 mA */
    case 0x02: lsb_uA = 360; break;  /* 0.36 mA */
    case 0x03: lsb_uA = 480; break;  /* 0.48 mA */
    default:   lsb_uA = 0;   break;
    }

    return (led_drv * lsb_uA) / 1000;
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

static int max86141_ppg_init(const struct device *dev)
{
	const struct max86141_ppg_config *cfg = dev->config;
	struct max86141_ppg_data *data = dev->data;
	const struct device *hub = cfg->hub_dev;
	uint8_t part_id;
	int ret;

	LOG_INF("Initializing MAX86141 PPG sensor via hub passthrough...");

	/* Wait for hub device to be ready */
	if (!device_is_ready(hub)) {
		LOG_ERR("Hub device %s not ready", hub->name);
		return -ENODEV;
	}

	/* Verify MAX86141 is present by reading Part ID */
	ret = max86141_read_reg(hub, MAX86141_REG_PART_ID, &part_id);
	if (ret) {
		LOG_ERR("Failed to read MAX86141 Part ID: %d", ret);
		return ret;
	}

	if (part_id != 0x25 && part_id != 0x36) {
		LOG_ERR("Invalid MAX86141 Part ID: 0x%02X (expected 0x25)", part_id);
		return -ENODEV;
	}

	LOG_INF("MAX86141 detected: Part ID 0x%02X", part_id);

#if defined(CONFIG_MAX32664_MODE_RAW)
	/* max32664_raw_mode_init() + max32664_raw_configure_ppg() already program the
	 * AFE and leave it sampling; hub output mode 0x01 streams Family 0x12.
	 * Re-writing all registers here and forcing SYSTEM_CTRL shutdown breaks
	 * that path (FIFO count stays 0 until the app starts the child again).
	 * Start/stop for measurements remains sensor_attr_set(SAMPLING_FREQUENCY).
	 */
	LOG_INF("RAW: skip duplicate MAX86141 register init (hub raw owns AFE config)");
	data->configured = true;
	return 0;
#endif

	/* Configure MAX86141 registers for PPG sampling.
	 * In RAW mode, these settings determine what the hub reads
	 * from the MAX86141 and outputs via its output FIFO. */

	/* Configure PPG_SYNC_CTRL: optional FIFO timestamp, no GPIO sync.
	 * PASSTHROUGH: max32664_passthrough_read_ppg_ex() drains fixed 6-byte
	 * LED pairs; timestamp tags (0x1F) change FIFO layout and cause bogus
	 * ADC values (e.g. spikes past plausible PPG). Force timestamp off here. */
#if defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
	const bool fifo_timestamp = false;

	if (cfg->timestamp_enable) {
		LOG_INF("PASSTHROUGH: ignoring DT ppg-timestamp-enable "
			"(AFE FIFO burst expects LED samples only)");
	}
#else
	const bool fifo_timestamp = cfg->timestamp_enable;
#endif
	uint8_t ppg_sync_ctrl = fifo_timestamp ? 0x80 : 0x00;
	ret = max86141_write_reg(hub, MAX86141_REG_PPG_SYNC_CTRL, ppg_sync_ctrl);
	if (ret) {
		LOG_ERR("Failed to configure PPG_SYNC_CTRL: %d", ret);
		return ret;
	}
	LOG_INF("PPG_SYNC_CTRL configured: 0x%02X (timestamp %s)",
		ppg_sync_ctrl, fifo_timestamp ? "enabled" : "disabled");

	k_msleep(10);

	ret = max86141_write_reg(hub, MAX86141_REG_INT_ENABLE1,
				 cfg->a_full_interrupt ? MAX86141_INT_EN_A_FULL : MAX86141_INT_EN_DATA_RDY);
	if (ret) {
		LOG_ERR("Failed to enable interrupts: %d", ret);
		return ret;
	}

	ret = max86141_write_reg(hub, MAX86141_REG_FIFO_CONFIG1, cfg->fifo_a_full & 0x7F);
	if (ret) {
		LOG_ERR("Failed to configure FIFO threshold: %d", ret);
		return ret;
	}

	ret = max86141_write_reg(hub, MAX86141_REG_FIFO_CONFIG2,
				 (cfg->fifo_rollover ? 0x0E : 0x00));
	if (ret) {
		LOG_ERR("Failed to enable FIFO roll-over: %d", ret);
		return ret;
	}

	LOG_INF("Configuring PPG_CONFIG1: ADC1=0x%02X, ADC2=0x%02X, TINT=0x02",
		cfg->adc1_range, cfg->adc2_range);
	uint8_t ppg_cfg1 = (cfg->adc1_range << 2) |
			   (cfg->adc2_range << 4) |
			   0x02;
	ret = max86141_write_reg(hub, MAX86141_REG_PPG_CONFIG1, ppg_cfg1);
	if (ret) {
		LOG_ERR("Failed to configure PPG_CONFIG1: %d", ret);
		return ret;
	}

	LOG_INF("Configuring PPG_CONFIG2: SR=0x%02X, AVG=0x%02X",
		cfg->sample_rate, cfg->sample_avg);
	uint8_t ppg_cfg2 = (cfg->sample_rate << 3) | (cfg->sample_avg & 0x07);
	ret = max86141_write_reg(hub, MAX86141_REG_PPG_CONFIG2, ppg_cfg2);
	if (ret) {
		LOG_ERR("Failed to configure PPG_CONFIG2: %d", ret);
		return ret;
	}

	uint8_t ppg_cfg3 = 0x40;
	ret = max86141_write_reg(hub, MAX86141_REG_PPG_CONFIG3, ppg_cfg3);
	if (ret) {
		LOG_ERR("Failed to configure PPG_CONFIG3: %d", ret);
		return ret;
	}

	/* SFH7074: LED1=Green, LED2=Red, LED3=IR — three exposures, no ambient. */
	uint8_t led_seq1 = MAX86141_LED_SEQ1_PRODUCT;
	uint8_t led_seq2 = MAX86141_LED_SEQ2_PRODUCT;
	uint8_t led_seq3 = MAX86141_LED_SEQ3_PRODUCT;
	ret = max86141_write_reg(hub, MAX86141_REG_LED_SEQ1, led_seq1);
	if (ret) return ret;
	ret = max86141_write_reg(hub, MAX86141_REG_LED_SEQ2, led_seq2);
	if (ret) return ret;
	ret = max86141_write_reg(hub, MAX86141_REG_LED_SEQ3, led_seq3);
	if (ret) return ret;
	LOG_INF("LED_SEQ1=0x%02X LED_SEQ2=0x%02X LED_SEQ3=0x%02X (IR/Red/Green/Ambient)",
		led_seq1, led_seq2, led_seq3);

	ret = max86141_write_reg(hub, MAX86141_REG_LED1_DRV_CUR, cfg->led1_current);
	if (ret) return ret;
	ret = max86141_write_reg(hub, MAX86141_REG_LED2_DRV_CUR, cfg->led2_current);
	if (ret) return ret;
	ret = max86141_write_reg(hub, MAX86141_REG_LED3_DRV_CUR, cfg->led3_current);
	if (ret) return ret;

	uint8_t led_range = 0x00;
	ret = max86141_write_reg(hub, MAX86141_REG_LED_RANGE1, led_range);
	if (ret) return ret;

	LOG_INF("LED1=%umA, LED2=%umA, LED3=%umA",
		max86141_led_current_ma(cfg->led1_current, led_range),
		max86141_led_current_ma(cfg->led2_current, led_range),
		max86141_led_current_ma(cfg->led3_current, led_range));

	ret = max86141_write_reg(hub, MAX86141_REG_PD_CONFIG,
				 MAX86141_PD_CONFIG_SFH7074_DUAL);
	if (ret) return ret;
	ret = max86141_write_reg(hub, MAX86141_REG_PICKET_FENCE, 0x00);
	if (ret) return ret;

	/* Keep device in shutdown mode initially (LEDs off) */
	uint8_t sys_ctrl = (1 << 2) | (1 << 1);  /* low power & shutdown mode */
	ret = max86141_write_reg(hub, MAX86141_REG_SYSTEM_CTRL, sys_ctrl);
	if (ret) return ret;
	LOG_DBG("MAX86141 initialized in shutdown mode (SYSTEM_CTRL=0x%02X)", sys_ctrl);

	data->configured = true;
	LOG_INF("MAX86141 in shutdown mode - use sensor_attr_set() to start sampling");

	LOG_INF("MAX86141 initialization complete");

	return 0;
}

/* ============================================================================
 * Driver API
 * ============================================================================ */

static const struct sensor_driver_api max86141_ppg_driver_api = {
	.sample_fetch = max86141_ppg_sample_fetch,
	.channel_get = max86141_ppg_channel_get,
	.attr_set = max86141_ppg_attr_set,
	.attr_get = max86141_ppg_attr_get,
};

/* ============================================================================
 * Device Instantiation
 * ============================================================================ */

/* Check if we have a max86141_ppg node defined */
#if DT_HAS_COMPAT_STATUS_OKAY(maxim_max86141_ppg)

#define MAX86141_PPG_DEFINE(inst)                                              \
	static struct max86141_ppg_data max86141_ppg_data_##inst;               \
                                                                               \
	static const struct max86141_ppg_config max86141_ppg_config_##inst = {  \
		.hub_dev = DEVICE_DT_GET(DT_INST_PARENT(inst)),                 \
		.led1_current = DT_INST_PROP_OR(inst, led1_current, 0x40),      \
		.led2_current = DT_INST_PROP_OR(inst, led2_current, 0x40),      \
		.led3_current = DT_INST_PROP_OR(inst, led3_current, 0x20),      \
		.sample_rate = DT_INST_PROP_OR(inst, sample_rate, 6),           \
		.adc1_range = DT_INST_PROP_OR(inst, adc1_range, 0),               \
		.adc2_range = DT_INST_PROP_OR(inst, adc2_range, 0),               \
		.sample_avg = DT_INST_PROP_OR(inst, sample_avg, 3),             \
		.fifo_a_full = DT_INST_PROP_OR(inst, fifo_a_full_threshold, 0x7F), \
		.fifo_rollover = DT_INST_PROP_OR(inst, fifo_rollover_enable, true), \
		.timestamp_enable = DT_INST_PROP_OR(inst, ppg_timestamp_enable, true), \
		.a_full_interrupt = DT_INST_PROP_OR(inst, ppg_a_full_interrupt, true), \
	};                                                                      \
                                                                               \
	DEVICE_DT_INST_DEFINE(inst, max86141_ppg_init, NULL,                    \
			      &max86141_ppg_data_##inst,                         \
			      &max86141_ppg_config_##inst,                       \
			      POST_KERNEL,                                      \
			      CONFIG_MAX32664_CHILD_SENSOR_INIT_PRIORITY,         \
			      &max86141_ppg_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MAX86141_PPG_DEFINE)

#else /* No device tree node - use hub device directly */

/* When no separate DT node exists, the PPG sensor is accessed via hub device */
/* Application should use max32664_raw_read_ppg() directly */

#endif /* DT_HAS_COMPAT_STATUS_OKAY(maxim_max86141_ppg) */
