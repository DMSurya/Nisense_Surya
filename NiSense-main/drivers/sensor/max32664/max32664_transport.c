/**
 * @file max32664_transport.c
 * @brief MAX32664 shared I2C transport and bus locking helpers
 *
 * AN6924 Normal MFIO Mode (Quick Start ~pp.12–13 / User Guide):
 *   assert MFIO low ≥300 µs before I2C START, hold low for the entire
 *   write + CMD_DELAY + read, then release high after STOP.
 * Assumption: DT interrupt-gpios uses GPIO_ACTIVE_LOW so logical assert
 *   (gpio_pin_set_dt(..., 1)) drives the pin physically low.
 *
 * AN6924 MFIO Interrupt Mode (~p.12): after an IRQ, wake with five dummy
 *   writes of byte 0x00 to I2C slave address 0x00, then talk to the hub.
 */

#include "max32664_priv.h"

LOG_MODULE_DECLARE(max32664, CONFIG_SENSOR_LOG_LEVEL);

/* AN6924 / User Guide: ≥250–300 µs; use 300 µs to satisfy both. */
#define MAX32664_MFIO_WAKE_US		300U
/* Extra margin before START after assert (empirical on nRF I2C + hub). */
#define MAX32664_MFIO_WAKE_HOLD_US	500U

/**
 * Soft-recursive bus lock: Zephyr k_mutex is not recursive, but the feeder
 * wraps multi-command sequences with bus_lock while i2c_transmit also locks.
 */
static void bus_lock_nested(struct max32664_data *data)
{
	k_tid_t self = k_current_get();

	if (data->bus_lock_depth > 0U && data->bus_lock_owner == self) {
		data->bus_lock_depth++;
		return;
	}

	k_mutex_lock(&data->bus_mutex, K_FOREVER);
	data->bus_lock_owner = self;
	data->bus_lock_depth = 1U;
}

static void bus_unlock_nested(struct max32664_data *data)
{
	if (data->bus_lock_depth > 1U) {
		data->bus_lock_depth--;
		return;
	}

	data->bus_lock_depth = 0U;
	data->bus_lock_owner = NULL;
	k_mutex_unlock(&data->bus_mutex);
}

/**
 * AN6924 IRQ wake: five dummy writes to 7-bit address 0x00 (not hub 0x55).
 * Caller must hold the bus lock. MFIO stays an input owned by the IRQ path.
 */
int max32664_mfio_irq_wake(const struct device *dev)
{
	const struct max32664_config *cfg = dev->config;
	uint8_t dummy = 0x00;
	int ret = 0;

	/*
	 * Addr 0x00 is intentional (AN6924 p.12). Some controllers NACK; we
	 * still issue all five writes — the side-effect is the wake, not ACK.
	 */
	for (int i = 0; i < 5; i++) {
		ret = i2c_write(cfg->bus.bus, &dummy, 1, 0x00);
		if (ret && ret != -EIO) {
			LOG_DBG("IRQ wake write %d/5: %d (NACK expected on some SoCs)",
				i + 1, ret);
		}
	}

	return 0;
}

int max32664_mfio_txn_begin(const struct device *dev, bool *held_as_output)
{
	const struct max32664_config *cfg = dev->config;
	struct max32664_data *data = dev->data;

	*held_as_output = false;

	if (cfg->int_gpio.port == NULL || !device_is_ready(cfg->int_gpio.port)) {
		return 0;
	}

	if (data->config.interrupt_mode) {
		/* AN6924 p.12 — do not reconfigure MFIO as output in IRQ mode. */
		return max32664_mfio_irq_wake(dev);
	}

	/*
	 * Normal MFIO: drive active (phys LOW with ACTIVE_LOW DT) and keep it
	 * low until mfio_txn_end. Do not release before I2C (prior bug).
	 */
	(void)gpio_pin_configure_dt(&cfg->int_gpio, GPIO_OUTPUT_INACTIVE);
	(void)gpio_pin_set_dt(&cfg->int_gpio, 1); /* assert → phys LOW */
	k_usleep(MAX32664_MFIO_WAKE_HOLD_US);
	*held_as_output = true;
	return 0;
}

void max32664_mfio_txn_end(const struct device *dev, bool held_as_output)
{
	const struct max32664_config *cfg = dev->config;

	if (!held_as_output || cfg->int_gpio.port == NULL) {
		return;
	}

	/* Release high so the hub may re-enter deep sleep / AFE service. */
	(void)gpio_pin_set_dt(&cfg->int_gpio, 0); /* deassert → phys HIGH */
}

int max32664_i2c_transmit(const struct device *dev,
			  uint8_t *tx_buf, uint8_t tx_len,
			  uint8_t *rx_buf, uint8_t rx_len,
			  uint16_t delay_ms)
{
	const struct max32664_config *cfg = dev->config;
	struct max32664_data *data = dev->data;
	bool mfio_held = false;
	int ret;

	bus_lock_nested(data);

	(void)max32664_mfio_txn_begin(dev, &mfio_held);

#if defined(CONFIG_MAX32664_I2C_DEBUG_DUMP)
	LOG_HEXDUMP_INF(tx_buf, tx_len, "TX");
#endif

	ret = i2c_write_dt(&cfg->bus, tx_buf, tx_len);
	if (ret) {
		LOG_ERR("I2C write error: %d", ret);
		ret = -EBUSY;
		goto out;
	}

	k_msleep(delay_ms);

	if (rx_buf != NULL && rx_len > 0) {
		ret = i2c_read_dt(&cfg->bus, rx_buf, rx_len);
		if (ret) {
			LOG_ERR("I2C read error: %d", ret);
			ret = -EBUSY;
			goto out;
		}

#if defined(CONFIG_MAX32664_I2C_DEBUG_DUMP)
		LOG_HEXDUMP_INF(rx_buf, rx_len, "RX");
#endif

		if (rx_buf[0] != 0) {
			enum max32664_cmd_status_class cls =
				max32664_classify_cmd_status(rx_buf[0]);

			if (cls == MAX32664_CMD_STATUS_BUSY) {
				LOG_DBG("Hub busy (0xFE), caller may retry");
			} else if (cls == MAX32664_CMD_STATUS_UNKNOWN) {
				LOG_ERR("Hub status 0xFF (unknown/asleep) — "
					"MFIO wake required before next cmd");
			} else {
				LOG_ERR("Hub command error, status: 0x%02x", rx_buf[0]);
			}
			ret = -EINVAL;
			goto out;
		}
	}

	ret = 0;
out:
	max32664_mfio_txn_end(dev, mfio_held);
	bus_unlock_nested(data);
	return ret;
}

void max32664_bus_lock(const struct device *dev)
{
	struct max32664_data *data = dev->data;

	bus_lock_nested(data);
}

void max32664_bus_unlock(const struct device *dev)
{
	struct max32664_data *data = dev->data;

	bus_unlock_nested(data);
}

int max32664_i2c_transmit_retry(const struct device *dev,
				uint8_t *tx_buf, uint8_t tx_len,
				uint8_t *rx_buf, uint8_t rx_len,
				uint16_t initial_delay_ms,
				uint8_t max_retries)
{
	int ret = -EIO;
	uint16_t delay_ms = initial_delay_ms;

	for (uint8_t attempt = 0; attempt <= max_retries; attempt++) {
		ret = max32664_i2c_transmit(dev, tx_buf, tx_len, rx_buf, rx_len, delay_ms);
		if (ret == MAX32664_STATUS_SUCCESS) {
			return ret;
		}

		if (ret == -EINVAL && rx_buf && rx_len > 0 &&
		    rx_buf[0] == MAX32664_STATUS_ERR_TRY_AGAIN) {
			if (attempt < max_retries) {
				delay_ms = initial_delay_ms * (1 << (attempt + 1));
				LOG_DBG("Hub busy (0xFE), retry %d/%d with %ums delay",
					attempt + 1, max_retries, delay_ms);
				k_msleep(10);
				continue;
			}
			LOG_WRN("Hub still busy after %d retries", max_retries);
		}

		return ret;
	}

	return ret;
}
