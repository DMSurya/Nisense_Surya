/**
 * @file max32664_sampling.c
 * @brief MAX32664 sampling/data-path helpers (poll/IRQ/FIFO parse/channel get)
 */

#include "max32664_priv.h"

LOG_MODULE_DECLARE(max32664, CONFIG_SENSOR_LOG_LEVEL);

#if !defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
void max32664_sampling_poll_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct max32664_data *data = CONTAINER_OF(dwork, struct max32664_data, poll_work);
	const struct device *dev = data->dev;

	uint8_t fifo_count;
	int ret;

	/* Check if sampling is still active */
	if (!data->sampling) {
		LOG_DBG("Polling stopped - sampling disabled");
		return; /* Don't reschedule */
	}

#if defined(CONFIG_MAX32664_MODE_RAW)
	static int last_fifo_count = -1;
	static uint32_t unchanged_count = 0U;
	/* OUTPUT_MODE_SENSOR_DATA (0x01): GWEC 30.x often fills Family 0x12
	 * without asserting DataRdyInt (bit 3). Use FIFO count alone.
	 */
	ret = max32664_get_fifo_count(dev, &fifo_count);
	if (ret) {
		LOG_WRN("Failed to get FIFO count: %d", ret);
		goto reschedule;
	}

	if (last_fifo_count != (int)fifo_count) {
		if ((last_fifo_count == 0) && (fifo_count > 0)) {
			LOG_DBG("RAW poll: FIFO resumed after %u unchanged zero polls (samples=%u)",
				unchanged_count, fifo_count);
		} else {
			LOG_DBG("RAW poll: hub output FIFO samples=%u", fifo_count);
		}
		last_fifo_count = (int)fifo_count;
		unchanged_count = 0U;
	} else {
		unchanged_count++;
		if ((fifo_count == 0U && (unchanged_count % 50U) == 0U) ||
		    (fifo_count > 0U && (unchanged_count % 100U) == 0U)) {
			LOG_DBG("RAW poll: hub output FIFO samples=%u (unchanged x%u)",
				fifo_count, unchanged_count);
		}
	}

	if (fifo_count > 0) {
		k_sem_give(&data->sample_sem);
	}
#else
	/* HUB algorithm mode: DataRdyInt (bit 3) is **threshold**, not "any byte
	 * in FIFO"  datasheet: 0 = FIFO below threshold, 1 = at/above. Family
	 * 0x12 can still report samples>0 while bit3=0. Wake on FIFO count like RAW. */
	ret = max32664_get_fifo_count(dev, &fifo_count);
	if (ret) {
		LOG_WRN("Failed to get FIFO count: %d", ret);
		goto reschedule;
	}

	uint8_t status = 0;
	int st_ret = max32664_get_hub_status(dev, &status);

	if (st_ret != 0) {
		LOG_WRN("Failed to get hub status: %d", st_ret);
	}

	const bool data_rdy_th =
		(st_ret == 0) && ((status & MAX32664_STATUS_DATA_RDY) != 0U);

	LOG_DBG("Poll: FIFO samples=%u status=0x%02x (DataRdyInt=%u = at/above thr)",
		fifo_count, status, data_rdy_th ? 1U : 0U);

	if (st_ret == 0 && (status & MAX32664_STATUS_ERR0) != 0U) {
		LOG_WRN("Hub Err0 (sensor comm): status=0x%02x", status);
	}
	if (st_ret == 0 &&
	    (status & (MAX32664_STATUS_FIFO_OUT_OVR | MAX32664_STATUS_FIFO_IN_OVR)) != 0U) {
		LOG_WRN("Hub FIFO overflow: status=0x%02x", status);
	}

	if (fifo_count == 0) {
		LOG_DBG("Hub output FIFO empty");
		goto reschedule;
	}

	LOG_DBG("FIFO has %u samples  signaling (DataRdyInt may be 0 below thr)",
		fifo_count);
	k_sem_give(&data->sample_sem);
#endif /* CONFIG_MAX32664_MODE_RAW */

reschedule:
	/* Schedule next poll only if sampling is active */
	if (data->sampling) {
		k_work_reschedule(&data->poll_work, K_MSEC(data->config.poll_interval_ms));
	}
}
#endif /* !CONFIG_MAX32664_MODE_PASSTHROUGH */

#if !defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
void max32664_sampling_gpio_interrupt_handler(const struct device *gpio_dev,
					      struct gpio_callback *cb,
					      uint32_t pins)
{
	ARG_UNUSED(gpio_dev);
	ARG_UNUSED(pins);

	/* Get data structure from callback */
	struct max32664_data *data = CONTAINER_OF(cb, struct max32664_data, gpio_cb);
	const struct device *dev = data->dev;

	if (dev == NULL) {
		return;
	}

	k_work_submit(&data->interrupt_work);
}

void max32664_sampling_interrupt_work_handler(struct k_work *work)
{
	struct max32664_data *data = CONTAINER_OF(work, struct max32664_data,
						   interrupt_work);

	/* MFIO falling edge: wake waiters. RAW consumers also drain on the
	 * next algo tick via max86141_ppg → max32664_raw_drain_ppg(); the
	 * 200 ms poll_work remains as fallback if DataRdy is quiet. */
	k_sem_give(&data->sample_sem);
}

#endif /* !CONFIG_MAX32664_MODE_PASSTHROUGH */

#if defined(CONFIG_MAX32664_MODE_HUB)

int max32664_sampling_parse_hub_data(const struct device *dev)
{
	/* One sample_fetch must not leave tens/hundreds of hub output FIFO entries
	 * unread: the hub keeps producing while the app polls slower, so
	 * FifoOutOvrInt (bit 4) can set (e.g. status 0x18) and FIFO count plateaus.
	 * Pop stale frames; parse only the newest. */
	static const unsigned int fifo_discard_cap = 192U;

	struct max32664_data *data = dev->data;
	uint8_t tx[2];
	/* rx[0] is the hub status byte, rx[1..] is the variant payload.
	 * 65 bytes covers the largest expected entry: output mode 0x03 with
	 * CONFIG_MAX32664_HUB_MODE03_FRAME_BYTES at its upper Kconfig range
	 * (64) plus the leading status byte. */
	uint8_t rx[65];
	int ret;
	uint8_t fifo_count;

	/* Get FIFO count first */
	ret = max32664_get_fifo_count(dev, &fifo_count);
	if (ret) {
		return ret;
	}

	if (fifo_count == 0) {
		return -ENODATA;
	}

	/* Read FIFO data (Family 0x12, Index 0x01) */
	tx[0] = 0x12; /* Family: Output FIFO */
	tx[1] = 0x01; /* Index: Read NUM_SAMPLES */

	/* Get expected FIFO entry size.
	 *
	 * Precedence:
	 *   1. data->fifo_entry_size_runtime   set by the HUB output-mode
	 *      helper when mode 0x03 (algo + sensor) is active, so the larger
	 *      34-byte frame is pulled in one shot.
	 *   2. ops->fifo_entry_size            variant-default (16 B on C).
	 *   3. Hard-coded variant fallback if ops is missing.
	 */
	const struct max32664_variant_ops *ops = max32664_get_variant_ops(data->variant);
	size_t rx_len = 1; /* status byte */

	if (data->fifo_entry_size_runtime > 0) {
		rx_len += data->fifo_entry_size_runtime;
	} else if (ops && ops->fifo_entry_size > 0) {
		rx_len += ops->fifo_entry_size;
	} else {
		/* Fallback sizes if no ops */
		if (data->variant == MAX32664_VARIANT_B || data->variant == MAX32664_VARIANT_C) {
			rx_len = 17;  /* 1 status + 16 bytes data */
		} else if (data->variant == MAX32664_VARIANT_D) {
			rx_len = 17;  /* 1 status + 16 bytes BPT algorithm data */
		} else {
			rx_len = 12;  /* 1 status + 11 bytes MaximFast data */
		}
	}

	/* Drain all but the last entry so channel_get sees fresh algo output. */
	unsigned int to_discard =
		(fifo_count > 1U) ? ((unsigned int)fifo_count - 1U) : 0U;

	if (to_discard > fifo_discard_cap) {
		LOG_WRN("HUB output FIFO backlog %u  discarding %u stale frame(s) "
			"this fetch (cap=%u); check FifoOutOvr / consume rate",
			(unsigned int)fifo_count, fifo_discard_cap, fifo_discard_cap);
		to_discard = fifo_discard_cap;
	} else if (to_discard > 0U) {
		LOG_DBG("Draining %u stale hub output FIFO frame(s)", to_discard);
	}

	while (to_discard > 0U) {
		ret = max32664_i2c_transmit_retry(dev, tx, sizeof(tx), rx, rx_len, 10, 3);
		if (ret) {
			LOG_ERR("FIFO discard read failed: %d", ret);
			return ret;
		}
		to_discard--;
	}

	/* Newest frame (or only frame) */
	ret = max32664_i2c_transmit_retry(dev, tx, sizeof(tx), rx, rx_len, 10, 3);
	if (ret) {
		LOG_ERR("Failed to read FIFO data: %d", ret);
		return ret;
	}

	/* rx[0] is the hub status byte; the variant payload starts at rx[1].
	 * Log with the same offset so what we print matches what parsers see. */
	const uint8_t *payload = &rx[1];
	const size_t payload_len = rx_len - 1;

	/* In algo-only mode the frame is always 16 B and a pair of DBG lines is
	 * enough; mode 0x03 frames grow past 16 B (raw PPG + accel prefix), so
	 * use a HEXDUMP for full visibility on the larger payload. */
	if (payload_len <= 16) {
		LOG_DBG("FIFO payload (%zu bytes): %02x %02x %02x %02x %02x %02x %02x %02x",
			payload_len,
			payload_len > 0 ? payload[0] : 0, payload_len > 1 ? payload[1] : 0,
			payload_len > 2 ? payload[2] : 0, payload_len > 3 ? payload[3] : 0,
			payload_len > 4 ? payload[4] : 0, payload_len > 5 ? payload[5] : 0,
			payload_len > 6 ? payload[6] : 0, payload_len > 7 ? payload[7] : 0);
		if (payload_len > 8) {
			LOG_DBG("FIFO payload cont: %02x %02x %02x %02x %02x %02x %02x %02x",
				payload[8],
				payload_len > 9  ? payload[9]  : 0,
				payload_len > 10 ? payload[10] : 0,
				payload_len > 11 ? payload[11] : 0,
				payload_len > 12 ? payload[12] : 0,
				payload_len > 13 ? payload[13] : 0,
				payload_len > 14 ? payload[14] : 0,
				payload_len > 15 ? payload[15] : 0);
		}
	} else {
		LOG_HEXDUMP_DBG(payload, payload_len, "FIFO payload (algo+sensor)");
	}

	/* Variant parsers expect just the payload (no status byte). A previous
	 * version passed the whole rx buffer which caused the parser to reject
	 * a 17-byte frame as "expected 16" and silently fall through to a
	 * generic misparse that aliased HR/SpO2 bytes onto PPG fields. The
	 * variant parser is authoritative; no generic fallback.
	 */
	if (!ops || !ops->parse_fifo) {
		LOG_ERR("No FIFO parser registered for variant %d", data->variant);
		return -ENOTSUP;
	}

	ret = ops->parse_fifo(dev, payload, payload_len);
	if (ret) {
		LOG_WRN("Variant parse_fifo failed: %d (payload %zu bytes)",
			ret, payload_len);
		return ret;
	}
	return 0;
}

int max32664_sampling_channel_get_hub(const struct device *dev,
					     enum sensor_channel chan,
					     struct sensor_value *val)
{
	struct max32664_data *data = dev->data;

	switch (chan) {
	case SENSOR_CHAN_IR:
		val->val1 = (int32_t)data->sample.ppg_ir;
		val->val2 = 0;
		break;
	case SENSOR_CHAN_RED:
		val->val1 = (int32_t)data->sample.ppg_red;
		val->val2 = 0;
		break;
	case SENSOR_CHAN_PROX:
		if (data->variant == MAX32664_VARIANT_D) {
			val->val1 = (int32_t)data->sample.prox_distance;
			val->val2 = 0;
		} else {
			return -ENOTSUP;
		}
		break;
	case SENSOR_CHAN_LIGHT:
		if (data->variant == MAX32664_VARIANT_D) {
			val->val1 = (int32_t)data->sample.als_lux;
			val->val2 = 0;
		} else {
			return -ENOTSUP;
		}
		break;
	case SENSOR_CHAN_ACCEL_X:
		if (data->variant == MAX32664_VARIANT_C) {
			val->val1 = (int32_t)data->sample.accel_x;
			val->val2 = 0;
		} else {
			return -ENOTSUP;
		}
		break;
	case SENSOR_CHAN_ACCEL_Y:
		if (data->variant == MAX32664_VARIANT_C) {
			val->val1 = (int32_t)data->sample.accel_y;
			val->val2 = 0;
		} else {
			return -ENOTSUP;
		}
		break;
	case SENSOR_CHAN_ACCEL_Z:
		if (data->variant == MAX32664_VARIANT_C) {
			val->val1 = (int32_t)data->sample.accel_z;
			val->val2 = 0;
		} else {
			return -ENOTSUP;
		}
		break;
	default:
		/* Check custom channels */
		if (chan == SENSOR_CHAN_MAX32664_SPO2) {
			val->val1 = (int32_t)data->sample.spo2;
			val->val2 = (int32_t)((data->sample.spo2 - val->val1) * 1000000);
		} else if (chan == SENSOR_CHAN_MAX32664_HR) {
			/* Heart rate stored in 0.1 BPM units (e.g., 720 = 72.0 BPM) */
			val->val1 = data->sample.heart_rate / 10;
			val->val2 = (data->sample.heart_rate % 10) * 100000;
		} else if (chan == SENSOR_CHAN_MAX32664_QUALITY) {
			val->val1 = data->sample.signal_quality;
			val->val2 = 0;
		} else if (chan == SENSOR_CHAN_MAX32664_AMBIENT) {
			if (data->variant == MAX32664_VARIANT_C) {
				val->val1 = (int32_t)data->sample.ppg_ambient;
				val->val2 = 0;
			} else {
				return -ENOTSUP;
			}
		} else if (chan == SENSOR_CHAN_MAX32664_SYSTOLIC_BP) {
			if (data->variant == MAX32664_VARIANT_D) {
				val->val1 = data->sample.systolic_bp;
				val->val2 = 0;
			} else {
				return -ENOTSUP;
			}
		} else if (chan == SENSOR_CHAN_MAX32664_DIASTOLIC_BP) {
			if (data->variant == MAX32664_VARIANT_D) {
				val->val1 = data->sample.diastolic_bp;
				val->val2 = 0;
			} else {
				return -ENOTSUP;
			}
		} else if (chan == SENSOR_CHAN_MAX32664_BP_CONFIDENCE) {
			if (data->variant == MAX32664_VARIANT_D) {
				val->val1 = data->sample.bp_confidence;
				val->val2 = 0;
			} else {
				return -ENOTSUP;
			}
		} else if (chan == SENSOR_CHAN_MAX32664_HR_CONFIDENCE) {
			val->val1 = data->sample.hr_confidence;
			val->val2 = 0;
		} else if (chan == SENSOR_CHAN_MAX32664_SPO2_CONFIDENCE) {
			val->val1 = data->sample.spo2_confidence;
			val->val2 = 0;
		} else if (chan == SENSOR_CHAN_MAX32664_SCD_STATE) {
			/*
			 * AN6924 SCD: 0 = no contact / off-skin; non-zero =
			 * contact class reported by WHRM suite (see AlgoHub).
			 * STATUS bit SCD_MON_SKIN (BIT7) is separate IRQ/status.
			 */
			val->val1 = data->sample.scd_state;
			val->val2 = 0;
		} else {
			return -ENOTSUP;
		}
		break;
	}

	return 0;
}
#endif /* CONFIG_MAX32664_MODE_HUB */