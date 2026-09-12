/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Maxim MAX20360 internal SAR ADC driver (Zephyr ADC API)
 *
 * The MAX20360 exposes three I2C slaves:
 *   - PMIC (regulators/charger/IVMON mux)  7-bit 0x28 (SlaveID 0x50/0x51)
 *   - Fuel gauge                            7-bit 0x36
 *   - Haptic driver + SAR ADC               7-bit 0x50 (SlaveID 0xA0/0xA1)
 *
 * This driver talks to the haptic/ADC slave and implements the standard
 * Zephyr ADC API.  Rail measurements (BAT, SYS, bucks, LDOs, ISET) are
 * routed through the IVMON multiplexer, which lives in the PMIC register
 * map and is owned by the regulator driver — channel selection is done
 * through max20360_ivmon_select() (plain I2C write, no PMIC lock needed).
 * VHDIN is converted directly without the mux.
 *
 * Conversion (datasheet Table 6): V = raw x 5.5 V / 255 for VHDIN/VIVMON,
 * so ref_internal = 5500 mV at 8-bit resolution.  ADC_GAIN_1_2/1_3/1_4
 * select the corresponding IVMON divider ratio.
 */

#define DT_DRV_COMPAT maxim_max20360_adc

#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "max20360_adc.h"
#include "../regulator/max20360_regs.h"
#include "../regulator/max20360_ivmon.h"

LOG_MODULE_REGISTER(adc_max20360, CONFIG_ADC_LOG_LEVEL);

#define MAX20360_ADC_RESOLUTION   8
#define MAX20360_ADC_REF_MV       MAX20360_ADC_VIVMON_FS_MV /* 5500 */
#define MAX20360_ADC_POLL_TRIES   10
#define MAX20360_ADC_POLL_MS      2

struct adc_max20360_config {
	struct i2c_dt_spec bus;      /* Haptic/ADC slave (7-bit 0x50) */
	const struct device *pmic;   /* Regulator parent: owns IVMON mux */
};

struct adc_max20360_data {
	struct k_mutex lock;
	/* IVMON divider ratio per channel, from adc_channel_setup() gain */
	uint8_t ratio[MAX20360_ADC_CHAN_COUNT];
	uint16_t configured;         /* Bitmask of set-up channels */
};

/* Map logical channel to IVMON mux selector (0 = direct/no mux) */
static uint8_t chan_to_ivmon(uint8_t channel_id)
{
	switch (channel_id) {
	case MAX20360_ADC_CHAN_BAT:
		return MAX20360_IVMON_CNTL_BAT;
	case MAX20360_ADC_CHAN_SYS:
		return MAX20360_IVMON_CNTL_SYS;
	case MAX20360_ADC_CHAN_BK1OUT:
		return MAX20360_IVMON_CNTL_BK1OUT;
	case MAX20360_ADC_CHAN_BK2OUT:
		return MAX20360_IVMON_CNTL_BK2OUT;
	case MAX20360_ADC_CHAN_BK3OUT:
		return MAX20360_IVMON_CNTL_BK3OUT;
	case MAX20360_ADC_CHAN_L1OUT:
		return MAX20360_IVMON_CNTL_L1OUT;
	case MAX20360_ADC_CHAN_L2OUT:
		return MAX20360_IVMON_CNTL_L2OUT;
	case MAX20360_ADC_CHAN_SFOUT:
		return MAX20360_IVMON_CNTL_SFOUT;
	case MAX20360_ADC_CHAN_BBOUT:
		return MAX20360_IVMON_CNTL_BBOUT;
	case MAX20360_ADC_CHAN_ISET:
		return MAX20360_IVMON_CNTL_ISET;
	default:
		return MAX20360_IVMON_CNTL_DISABLED;
	}
}

static int gain_to_ratio(enum adc_gain gain, uint8_t *ratio)
{
	switch (gain) {
	case ADC_GAIN_1:
		*ratio = MAX20360_IVMON_RATIO_1_1;
		return 0;
	case ADC_GAIN_1_2:
		*ratio = MAX20360_IVMON_RATIO_1_2;
		return 0;
	case ADC_GAIN_1_3:
		*ratio = MAX20360_IVMON_RATIO_1_3;
		return 0;
	case ADC_GAIN_1_4:
		*ratio = MAX20360_IVMON_RATIO_1_4;
		return 0;
	default:
		return -EINVAL;
	}
}

static int adc_max20360_channel_setup(const struct device *dev,
				      const struct adc_channel_cfg *channel_cfg)
{
	struct adc_max20360_data *data = dev->data;
	uint8_t ratio;

	if (channel_cfg->channel_id >= MAX20360_ADC_CHAN_COUNT) {
		LOG_ERR("Invalid channel %u", channel_cfg->channel_id);
		return -EINVAL;
	}
	if (channel_cfg->differential) {
		LOG_ERR("Differential mode not supported");
		return -ENOTSUP;
	}
	if (channel_cfg->reference != ADC_REF_INTERNAL) {
		LOG_ERR("Only ADC_REF_INTERNAL supported");
		return -ENOTSUP;
	}
	if (channel_cfg->acquisition_time != ADC_ACQ_TIME_DEFAULT) {
		LOG_ERR("Acquisition time is fixed");
		return -ENOTSUP;
	}
	if (gain_to_ratio(channel_cfg->gain, &ratio) != 0) {
		LOG_ERR("Unsupported gain %d", channel_cfg->gain);
		return -ENOTSUP;
	}
	if (channel_cfg->channel_id == MAX20360_ADC_CHAN_VHDIN &&
	    ratio != MAX20360_IVMON_RATIO_1_1) {
		LOG_ERR("VHDIN is direct: only ADC_GAIN_1 supported");
		return -ENOTSUP;
	}

	data->ratio[channel_cfg->channel_id] = ratio;
	data->configured |= BIT(channel_cfg->channel_id);
	return 0;
}

/* Run one conversion on the haptic/ADC slave: configure, launch, poll, read */
static int adc_max20360_convert(const struct device *dev, uint8_t adcsel, uint8_t *raw)
{
	const struct adc_max20360_config *cfg = dev->config;
	uint8_t adccfg;
	uint8_t adcen = 0;
	int ret;

	adccfg = (MAX20360_ADC_AVGSIZ_4 << MAX20360_ADCCFG_AVGSIZ_SHIFT) |
		 (adcsel & MAX20360_ADCCFG_SEL_MASK);
	ret = i2c_reg_write_byte_dt(&cfg->bus, MAX20360_REG_ADCCFG, adccfg);
	if (ret) {
		LOG_ERR("ADCCfg write failed: %d", ret);
		return ret;
	}

	ret = i2c_reg_write_byte_dt(&cfg->bus, MAX20360_REG_ADCEN,
				    MAX20360_ADCEN_CONVLAUNCH);
	if (ret) {
		LOG_ERR("Conversion launch failed: %d", ret);
		return ret;
	}

	for (int i = 0; i < MAX20360_ADC_POLL_TRIES; i++) {
		k_msleep(MAX20360_ADC_POLL_MS);
		ret = i2c_reg_read_byte_dt(&cfg->bus, MAX20360_REG_ADCEN, &adcen);
		if (ret) {
			LOG_ERR("Status poll failed: %d", ret);
			return ret;
		}
		if ((adcen & MAX20360_ADCEN_CONVLAUNCH) == 0) {
			return i2c_reg_read_byte_dt(&cfg->bus,
						    MAX20360_REG_ADCDATAVG, raw);
		}
	}

	LOG_ERR("Conversion timed out");
	return -ETIMEDOUT;
}

static int adc_max20360_read_channel(const struct device *dev, uint8_t channel_id,
				     uint8_t *raw)
{
	const struct adc_max20360_config *cfg = dev->config;
	struct adc_max20360_data *data = dev->data;
	uint8_t ivmon = chan_to_ivmon(channel_id);
	int ret;

	if (ivmon == MAX20360_IVMON_CNTL_DISABLED) {
		/* VHDIN: direct ADC channel, no mux involved */
		return adc_max20360_convert(dev, MAX20360_ADC_SEL_VHDIN, raw);
	}

	if (!device_is_ready(cfg->pmic)) {
		LOG_ERR("PMIC device not ready for IVMON select");
		return -ENODEV;
	}

	ret = max20360_ivmon_select(cfg->pmic, ivmon, data->ratio[channel_id]);
	if (ret) {
		return ret;
	}

	/* IVMON settling (typically <1 ms) */
	k_msleep(2);

	ret = adc_max20360_convert(dev, MAX20360_ADC_SEL_VIVMON, raw);

	/* Always park the mux to avoid divider leakage */
	(void)max20360_ivmon_select(cfg->pmic, MAX20360_IVMON_CNTL_DISABLED,
				    MAX20360_IVMON_RATIO_1_1);
	return ret;
}

static int adc_max20360_read(const struct device *dev,
			     const struct adc_sequence *sequence)
{
	struct adc_max20360_data *data = dev->data;
	uint16_t *buf = sequence->buffer;
	uint32_t channels = sequence->channels;
	size_t needed;
	int ret = 0;

	if (sequence->resolution != MAX20360_ADC_RESOLUTION) {
		LOG_ERR("Only %u-bit resolution supported", MAX20360_ADC_RESOLUTION);
		return -ENOTSUP;
	}
	if (sequence->options != NULL) {
		LOG_ERR("Sequence options not supported");
		return -ENOTSUP;
	}
	if (sequence->oversampling != 0) {
		LOG_ERR("Oversampling not supported");
		return -ENOTSUP;
	}
	if (channels == 0 || (channels & ~(uint32_t)data->configured) != 0 ||
	    (channels & ~(uint32_t)(BIT(MAX20360_ADC_CHAN_COUNT) - 1)) != 0) {
		LOG_ERR("Invalid/unconfigured channels 0x%08x", channels);
		return -EINVAL;
	}

	needed = (size_t)POPCOUNT(channels) * sizeof(uint16_t);
	if (sequence->buffer_size < needed) {
		LOG_ERR("Buffer too small: %zu < %zu", sequence->buffer_size, needed);
		return -ENOMEM;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	for (uint8_t ch = 0; ch < MAX20360_ADC_CHAN_COUNT; ch++) {
		uint8_t raw;

		if ((channels & BIT(ch)) == 0) {
			continue;
		}
		ret = adc_max20360_read_channel(dev, ch, &raw);
		if (ret) {
			break;
		}
		*buf++ = raw;
	}

	k_mutex_unlock(&data->lock);
	return ret;
}

static int adc_max20360_init(const struct device *dev)
{
	const struct adc_max20360_config *cfg = dev->config;
	struct adc_max20360_data *data = dev->data;
	uint8_t val;
	int ret;

	if (!i2c_is_ready_dt(&cfg->bus)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	k_mutex_init(&data->lock);

	/* Probe the haptic/ADC slave so a wiring/address issue is visible at boot */
	ret = i2c_reg_read_byte_dt(&cfg->bus, MAX20360_REG_ADCCFG, &val);
	if (ret) {
		LOG_WRN("Haptic/ADC slave (0x%02x) probe failed: %d", cfg->bus.addr, ret);
	} else {
		LOG_INF("MAX20360 ADC ready (haptic/ADC slave 0x%02x)", cfg->bus.addr);
	}

	return 0;
}

static DEVICE_API(adc, adc_max20360_api) = {
	.channel_setup = adc_max20360_channel_setup,
	.read = adc_max20360_read,
	.ref_internal = MAX20360_ADC_REF_MV,
};

#define ADC_MAX20360_DEFINE(inst)                                              \
	static const struct adc_max20360_config adc_max20360_config_##inst = { \
		.bus = I2C_DT_SPEC_INST_GET(inst),                             \
		.pmic = DEVICE_DT_GET(DT_INST_PHANDLE(inst, maxim_pmic)),      \
	};                                                                     \
	static struct adc_max20360_data adc_max20360_data_##inst;              \
	DEVICE_DT_INST_DEFINE(inst, adc_max20360_init, NULL,                   \
			      &adc_max20360_data_##inst,                       \
			      &adc_max20360_config_##inst, POST_KERNEL,        \
			      CONFIG_ADC_MAX20360_INIT_PRIORITY,               \
			      &adc_max20360_api);

DT_INST_FOREACH_STATUS_OKAY(ADC_MAX20360_DEFINE)
