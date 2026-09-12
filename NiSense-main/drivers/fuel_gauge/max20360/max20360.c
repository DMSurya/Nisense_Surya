/*
 * Copyright (c) 2025 MAX20360 Contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * Maxim MAX20360 fuel gauge (ModelGauge m5 / MAX17260 register map).
 * MAX20360 datasheet “MAX17620” references are a typo — use MAX17260 Table 17.
 */

#define DT_DRV_COMPAT maxim_max20360_fuel_gauge

#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>
#include <string.h>

#include "max20360_fg.h"

LOG_MODULE_REGISTER(fuel_gauge_max20360, CONFIG_FUEL_GAUGE_MAX20360_LOG_LEVEL);

#define MAX20360_FG_I2C_READ_RETRIES 2

/* MAX17260 ModelGauge m5 EZ registers (Table 17) */
#define MAX17260_REG_STATUS      0x00
#define MAX17260_REG_VALRTTH     0x01
#define MAX17260_REG_TALRTTH     0x02
#define MAX17260_REG_SALRTTH     0x03
#define MAX17260_REG_REPCAP      0x05
#define MAX17260_REG_REPSOC      0x06
#define MAX17260_REG_TEMP        0x08
#define MAX17260_REG_VCELL       0x09
#define MAX17260_REG_CURRENT     0x0A
#define MAX17260_REG_AVGCURRENT  0x0B
#define MAX17260_REG_FULLCAPREP  0x10
#define MAX17260_REG_TTE         0x11
#define MAX17260_REG_AVGTA       0x16
#define MAX17260_REG_CYCLES      0x17
#define MAX17260_REG_DESIGNCAP   0x18
#define MAX17260_REG_AVGVCELL    0x19
#define MAX17260_REG_MAXMINTEMP  0x1A
#define MAX17260_REG_MAXMINVOLT  0x1B
#define MAX17260_REG_MAXMINCURR  0x1C
#define MAX17260_REG_CONFIG      0x1D
#define MAX17260_REG_ICHGTERM    0x1E
#define MAX17260_REG_AVCAP       0x1F
#define MAX17260_REG_TTF         0x20
#define MAX17260_REG_DEVNAME     0x21
#define MAX17260_REG_DIETEMP     0x34
#define MAX17260_REG_QH          0x4D
#define MAX17260_REG_IALRTTH     0xB4
#define MAX17260_REG_CONFIG2     0xBB

/* Compatibility aliases used below */
#define MAX17620_REG_STATUS      MAX17260_REG_STATUS
#define MAX17620_REG_VCELL       MAX17260_REG_VCELL
#define MAX17620_REG_REPSOC      MAX17260_REG_REPSOC
#define MAX17620_REG_REPCAP      MAX17260_REG_REPCAP
#define MAX17620_REG_FULLCAPREP  MAX17260_REG_FULLCAPREP
#define MAX17620_REG_TEMP        MAX17260_REG_TEMP
#define MAX17620_REG_CURRENT     MAX17260_REG_CURRENT
#define MAX17620_REG_AVGCURRENT  MAX17260_REG_AVGCURRENT
#define MAX17620_REG_CYCLES      MAX17260_REG_CYCLES
#define MAX17620_REG_DESIGNCAP   MAX17260_REG_DESIGNCAP
#define MAX17620_REG_TTF         MAX17260_REG_TTF
#define MAX17620_REG_DEVNAME     MAX17260_REG_DEVNAME

#define MAX17620_STATUS_BST      BIT(3) /* 0=present, 1=absent (MAX17260 Table 7) */
#define MAX17620_STATUS_POR      BIT(1)

/* Time registers: 5.625 s/LSB → minutes = raw * 5625 / (1000 * 60) */
#define MAX17260_TIME_LSB_MS     5625

struct fuel_gauge_max20360_config {
	struct i2c_dt_spec bus;
	uint16_t design_capacity_mah;
	uint16_t design_voltage_mv;
	uint16_t rsense_mohm;
};

struct fuel_gauge_max20360_data {
	/* Cache for recent readings */
	uint16_t last_voltage_mv;
	int16_t last_current_ma;
	uint8_t last_soc_pct;
	int16_t last_temp_c;
	uint16_t last_cycles;
	bool initialized;
};

/* ===== I2C Helper Functions ===== */

/**
 * @brief Read a 16-bit register from MAX17620-compatible fuel gauge
 *
 * @param dev Fuel gauge device
 * @param reg Register address
 * @param val Pointer to store 16-bit value (little-endian)
 * @return 0 on success, negative error code on failure
 */
static int fuel_gauge_max20360_read_reg16(const struct device *dev, uint8_t reg, uint16_t *val)
{
	const struct fuel_gauge_max20360_config *cfg = dev->config;
	uint8_t buf[2];
	int ret = -EIO;
	int attempt;

	if (!device_is_ready(cfg->bus.bus)) {
		LOG_ERR("I2C bus not ready for fuel gauge");
		return -ENODEV;
	}

	for (attempt = 0; attempt < MAX20360_FG_I2C_READ_RETRIES; attempt++) {
		ret = i2c_burst_read_dt(&cfg->bus, reg, buf, 2);
		if (ret == 0) {
			*val = buf[0] | (buf[1] << 8);
			return 0;
		}
	}

	LOG_ERR("I2C reg16 read failed: reg=0x%02x ret=%d", reg, ret);
	return ret;
}

/**
 * @brief Write a 16-bit register to MAX17620-compatible fuel gauge
 *
 * @param dev Fuel gauge device
 * @param reg Register address
 * @param val 16-bit value to write (little-endian)
 * @return 0 on success, negative error code on failure
 */
static int fuel_gauge_max20360_write_reg16(const struct device *dev, uint8_t reg, uint16_t val)
{
	const struct fuel_gauge_max20360_config *cfg = dev->config;
	uint8_t buf[2];
	int ret;

	if (!device_is_ready(cfg->bus.bus)) {
		LOG_ERR("I2C bus not ready for fuel gauge");
		return -ENODEV;
	}

	/* MAX1762x uses little-endian format: LSB first */
	buf[0] = val & 0xFF;
	buf[1] = (val >> 8) & 0xFF;

	ret = i2c_burst_write_dt(&cfg->bus, reg, buf, 2);
	if (ret) {
		LOG_ERR("I2C reg16 write failed: reg=0x%02x val=0x%04x ret=%d", reg, val, ret);
	}

	return ret;
}

/* ===== Battery Property Conversions ===== */

/**
 * @brief Convert raw VCELL register to microvolts
 * MAX17620: VCELL register, 78.125 uV/LSB
 *
 * @param raw_vcell Raw 16-bit VCELL value
 * @return Voltage in microvolts
 */
static uint32_t convert_vcell_to_uv(uint16_t raw_vcell)
{
	/* VCELL LSB = 78.125 uV = 78125 nV
	 * For uV: raw * 78.125 = raw * 78125 / 1000
	 */
	return ((uint32_t)raw_vcell * 78125) / 1000;
}

/**
 * @brief Convert raw CURRENT register to microamps  
 * MAX17620: CURRENT register, 156.25 uA/LSB, signed 16-bit
 *
 * @param raw_current Raw 16-bit current value (2's complement)
 * @return Current in microamps (positive = charging, negative = discharging)
 */
static int32_t convert_current_to_ua(uint16_t raw_current, uint16_t rsense_mohm)
{
	/* Current LSB = 1.5625 µV / RSENSE → µA = raw * 1562.5 / rsense_mohm */
	int16_t signed_current = (int16_t)raw_current;
	uint16_t rs = rsense_mohm ? rsense_mohm : 10;

	return ((int32_t)signed_current * 15625) / (10 * (int32_t)rs);
}

/** RepSOC: percent = raw / 256 (LSB = 1/256 %), clamp 0–100. */
static uint8_t convert_repsoc_to_percent(uint16_t raw_soc)
{
	uint32_t percent = (uint32_t)raw_soc / 256U;

	return (uint8_t)MIN(percent, 100U);
}

static uint32_t convert_time_reg_to_minutes(uint16_t raw)
{
	/* minutes = raw * 5.625 / 60 = raw * 5625 / 60000 */
	return ((uint32_t)raw * MAX17260_TIME_LSB_MS) / 60000U;
}

/**
 * @brief Convert raw TEMP register to decikelvin
 * MAX17620: TEMP register, 1/256 degC/LSB
 *
 * @param raw_temp Raw 16-bit TEMP value
 * @return Temperature in decikelvin (1/10 K)
 */
static int16_t convert_temp_to_decikelvin(uint16_t raw_temp)
{
	/* TEMP LSB = 1/256 C/LSB
	 * Temperature in C = raw / 256
	 * Convert to K: C + 273.15
	 * Convert to decikelvin: (C + 273.15) * 10
	 */
	int32_t temp_dc = ((int32_t)(int16_t)raw_temp * 10) / 256; /* decicelsius */
	return (int16_t)(temp_dc + 2732); /* Add 273.2 decikelvin */
}

/**
 * @brief Convert raw REPCAP register to microamp-hours
 * MAX17620: REPCAP register, 0.5 mAh/LSB
 *
 * @param raw_cap Raw 16-bit REPCAP value
 * @return Capacity in microamp-hours
 */
static uint32_t convert_repcap_to_uah(uint16_t raw_cap, uint16_t rsense_mohm)
{
	/* Capacity LSB = 5.0 µVh / RSENSE → 0.5 mAh @ 10 mΩ → 500 µAh/LSB */
	uint16_t rs = rsense_mohm ? rsense_mohm : 10;

	return ((uint32_t)raw_cap * 5000U) / rs;
}

static uint16_t fuel_gauge_max20360_mah_to_raw(uint32_t mah, uint16_t rsense_mohm)
{
	uint16_t rs = rsense_mohm ? rsense_mohm : 10;
	/* raw = mah / (5/rs) = mah * rs / 5 ; @10 mΩ → mah*2 */
	uint32_t raw = (mah * rs) / 5U;

	return (uint16_t)MIN(raw, 0xFFFFU);
}

static uint32_t fuel_gauge_max20360_raw_to_mah(uint16_t raw, uint16_t rsense_mohm)
{
	return convert_repcap_to_uah(raw, rsense_mohm) / 1000U;
}

/** Program DesignCap/FullCapRep and rescale REPCAP to preserve SOC %. */
static int fuel_gauge_max20360_program_design_cap(const struct device *dev,
						  uint16_t old_design_raw,
						  uint32_t design_mah)
{
	const struct fuel_gauge_max20360_config *cfg = dev->config;
	uint16_t expected_raw = fuel_gauge_max20360_mah_to_raw(design_mah, cfg->rsense_mohm);
	uint16_t repsoc_raw = 0;
	uint16_t repcap_raw = 0;
	uint32_t old_design_mah;
	uint32_t new_repcap_raw;
	int ret;

	if (expected_raw == 0U) {
		return -EINVAL;
	}

	ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_REPSOC, &repsoc_raw);
	if (ret != 0) {
		repsoc_raw = 0;
	}

	ret = fuel_gauge_max20360_write_reg16(dev, MAX17620_REG_DESIGNCAP, expected_raw);
	if (ret != 0) {
		return ret;
	}

	ret = fuel_gauge_max20360_write_reg16(dev, MAX17620_REG_FULLCAPREP, expected_raw);
	if (ret != 0) {
		LOG_WRN("FullCapRep write failed: %d", ret);
	}

	ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_REPCAP, &repcap_raw);
	if (ret != 0) {
		repcap_raw = 0;
	}

	old_design_mah = fuel_gauge_max20360_raw_to_mah(old_design_raw, cfg->rsense_mohm);
	if (old_design_raw > 0U && repcap_raw > 0U &&
	    old_design_mah >= (design_mah / 2U)) {
		new_repcap_raw = ((uint32_t)repcap_raw * expected_raw) / old_design_raw;
	} else {
		new_repcap_raw = ((uint32_t)expected_raw * repsoc_raw) / 256U;
	}

	if (new_repcap_raw > 0xFFFFU) {
		new_repcap_raw = expected_raw;
	}

	ret = fuel_gauge_max20360_write_reg16(dev, MAX17620_REG_REPCAP,
					      (uint16_t)new_repcap_raw);
	if (ret == 0) {
		LOG_INF("DesignCap set to %u mAh (raw=0x%04x); REPCAP -> %u mAh (SOC raw=0x%04x)",
			design_mah, expected_raw,
			fuel_gauge_max20360_raw_to_mah((uint16_t)new_repcap_raw,
						       cfg->rsense_mohm),
			repsoc_raw);
	}

	return ret;
}

/* ===== Fuel Gauge API Implementation ===== */

/**
 * @brief Get battery property from MAX20360 fuel gauge (MAX17620-compatible)
 */
static int fuel_gauge_max20360_get_property(const struct device *dev,
					    fuel_gauge_prop_t prop,
					    union fuel_gauge_prop_val *val)
{
	const struct fuel_gauge_max20360_config *cfg = dev->config;
	struct fuel_gauge_max20360_data *data = dev->data;
	uint16_t reg_val;
	int ret;

	switch (prop) {
	case FUEL_GAUGE_VOLTAGE:
		ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_VCELL, &reg_val);
		if (ret) {
			return ret;
		}
		val->voltage = convert_vcell_to_uv(reg_val);
		data->last_voltage_mv = val->voltage / 1000;
		LOG_DBG("Battery voltage: %d uV (raw=0x%04x)", val->voltage, reg_val);
		break;

	case FUEL_GAUGE_CURRENT:
		ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_CURRENT, &reg_val);
		if (ret) {
			return ret;
		}
		val->current = convert_current_to_ua(reg_val, cfg->rsense_mohm);
		data->last_current_ma = val->current / 1000;
		LOG_DBG("Battery current: %d uA (raw=0x%04x)", val->current, reg_val);
		break;
	
	case FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE:
		ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_REPSOC, &reg_val);
		if (ret) {
			return ret;
		}
		val->relative_state_of_charge = convert_repsoc_to_percent(reg_val);
		data->last_soc_pct = val->relative_state_of_charge;
		LOG_DBG("State of charge: %d%% (raw=0x%04x)", val->relative_state_of_charge, reg_val);
		break;
	
	case FUEL_GAUGE_REMAINING_CAPACITY:
		ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_REPCAP, &reg_val);
		if (ret) {
			return ret;
		}
		val->remaining_capacity = convert_repcap_to_uah(reg_val, cfg->rsense_mohm);
		{
			uint32_t design_uah = (uint32_t)cfg->design_capacity_mah * 1000U;

			if (design_uah > 0U &&
			    val->remaining_capacity > design_uah + (design_uah / 4U)) {
				val->remaining_capacity = design_uah;
			}
		}
		LOG_DBG("Remaining capacity: %d uAh (raw=0x%04x)", val->remaining_capacity, reg_val);
		break;
	
	case FUEL_GAUGE_CYCLE_COUNT:
		ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_CYCLES, &reg_val);
		if (ret) {
			return ret;
		}
		val->cycle_count = reg_val;
		data->last_cycles = reg_val;
		LOG_DBG("Cycle count: %d (raw=0x%04x)", val->cycle_count, reg_val);
		break;
	
	case FUEL_GAUGE_TEMPERATURE:
		ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_TEMP, &reg_val);
		if (ret) {
			return ret;
		}
		val->temperature = convert_temp_to_decikelvin(reg_val);
		data->last_temp_c = (val->temperature - 2732) / 10; /* Store in C */
		LOG_DBG("Temperature: %d.%d K (%d.%dC, raw=0x%04x)", 
			val->temperature / 10, val->temperature % 10,
			data->last_temp_c / 10, abs(data->last_temp_c % 10), reg_val);
		break;

	case FUEL_GAUGE_AVG_CURRENT:
		ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_AVGCURRENT, &reg_val);
		if (ret) {
			return ret;
		}
		val->avg_current = convert_current_to_ua(reg_val, cfg->rsense_mohm);
		LOG_DBG("Average current: %d uA (raw=0x%04x)", val->avg_current, reg_val);
		break;

	case FUEL_GAUGE_CHARGE_CURRENT:
		ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_CURRENT, &reg_val);
		if (ret) {
			return ret;
		}
		val->chg_current = convert_current_to_ua(reg_val, cfg->rsense_mohm);
		if (val->chg_current < 0) {
			val->chg_current = 0;
		}
		break;

	case FUEL_GAUGE_DESIGN_CAPACITY:
		/* Zephyr: DESIGN_CAPACITY is mAh (uint16), not µAh */
		if (cfg->design_capacity_mah > 0U) {
			val->design_cap = cfg->design_capacity_mah;
			break;
		}

		ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_DESIGNCAP, &reg_val);
		if (ret) {
			return ret;
		}
		val->design_cap = (uint16_t)fuel_gauge_max20360_raw_to_mah(reg_val,
									   cfg->rsense_mohm);
		break;

	case FUEL_GAUGE_FULL_CHARGE_CAPACITY:
	{
		uint32_t full_mah;

		ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_FULLCAPREP, &reg_val);
		if (ret) {
			return ret;
		}
		full_mah = fuel_gauge_max20360_raw_to_mah(reg_val, cfg->rsense_mohm);
		if (cfg->design_capacity_mah > 0U &&
		    (full_mah == 0U ||
		     full_mah < (cfg->design_capacity_mah * 3U / 4U) ||
		     full_mah > (cfg->design_capacity_mah * 2U))) {
			val->full_charge_capacity =
				(uint32_t)cfg->design_capacity_mah * 1000U;
		} else {
			val->full_charge_capacity =
				convert_repcap_to_uah(reg_val, cfg->rsense_mohm);
		}
		break;
	}

	case FUEL_GAUGE_ABSOLUTE_STATE_OF_CHARGE:
		/* For MAX17620, absolute SOC is same as relative SOC */
		ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_REPSOC, &reg_val);
		if (ret) {
			return ret;
		}
		val->absolute_state_of_charge = convert_repsoc_to_percent(reg_val);
		LOG_DBG("Absolute state of charge: %d%% (raw=0x%04x)", 
			val->absolute_state_of_charge, reg_val);
		break;

	case FUEL_GAUGE_RUNTIME_TO_FULL:
		ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_TTF, &reg_val);
		if (ret) {
			return ret;
		}
		val->runtime_to_full = convert_time_reg_to_minutes(reg_val);
		break;

	case FUEL_GAUGE_RUNTIME_TO_EMPTY:
		ret = fuel_gauge_max20360_read_reg16(dev, MAX17260_REG_TTE, &reg_val);
		if (ret) {
			return ret;
		}
		val->runtime_to_empty = convert_time_reg_to_minutes(reg_val);
		break;

	case FUEL_GAUGE_FLAGS:
		ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_STATUS, &reg_val);
		if (ret) {
			return ret;
		}
		/* Map MAX17620 status bits to fuel gauge flags */
		/* Note: Flag definitions may vary by Zephyr version - using raw status for now */
		val->flags = reg_val; /* Return raw status register as flags */
		LOG_DBG("Flags: 0x%04x (status=0x%04x)", val->flags, reg_val);
		break;

	case FUEL_GAUGE_STATUS:
		ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_STATUS, &reg_val);
		if (ret) {
			return ret;
		}
		/* Status register value */
		val->fg_status = reg_val;
		LOG_DBG("Status: 0x%04x", val->fg_status);
		break;

	case FUEL_GAUGE_PRESENT_STATE:
		/* Check if battery is present by reading voltage */
		/* Note: Zephyr API may not fully support this property */
		ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_VCELL, &reg_val);
		if (ret) {
			return ret;
		}
		/* Battery is present if voltage > 2.0V (typical minimum) */
		/* Log the result but return ENOTSUP as union member may not exist */
		{
			uint32_t voltage_uv = convert_vcell_to_uv(reg_val);
			bool present = (voltage_uv > 2000000);
			LOG_DBG("Battery present: %s (voltage=%u uV)", 
				present ? "yes" : "no", voltage_uv);
		}
		return -ENOTSUP;

	case FUEL_GAUGE_CONNECT_STATE:
		/* MAX17260: Bst=0 present, Bst=1 absent */
		ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_STATUS, &reg_val);
		if (ret) {
			return ret;
		}
		LOG_DBG("Pack present (Bst clear): %s (status=0x%04x)",
			(reg_val & MAX17620_STATUS_BST) ? "no" : "yes", reg_val);
		return -ENOTSUP;

	case FUEL_GAUGE_DESIGN_VOLTAGE:
		val->design_volt = cfg->design_voltage_mv;
		break;
	
	default:
		LOG_ERR("Unsupported property: %d", prop);
		return -ENOTSUP;
	}

	return 0;
}

/* Fuel gauge driver API */
static const struct fuel_gauge_driver_api fuel_gauge_max20360_api = {
	.get_property = fuel_gauge_max20360_get_property,
	/* set_property not yet implemented (battery properties are read-only) */
};

/* ===== Driver Initialization ===== */

/**
 * @brief Initialize MAX20360 fuel gauge driver (MAX17620-compatible)
 *
 * @param dev Fuel gauge device
 * @return 0 on success, negative error code on failure
 */
static int fuel_gauge_max20360_init(const struct device *dev)
{
	const struct fuel_gauge_max20360_config *cfg = dev->config;
	struct fuel_gauge_max20360_data *data = dev->data;
	uint16_t status_reg, device_name;
	int ret;

	LOG_DBG("Initializing MAX20360 fuel gauge (MAX17620-compatible)");

	/* Verify I2C bus is ready */
	if (!device_is_ready(cfg->bus.bus)) {
		LOG_ERR("I2C bus not ready for MAX20360 fuel gauge");
		return -ENODEV;
	}

	/* Initialize data structure */
	memset(data, 0, sizeof(*data));

	/* Read status register to verify communication */
	ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_STATUS, &status_reg);
	if (ret) {
		LOG_ERR("Failed to read status register: %d", ret);
		return ret;
	}

	/* Read device name register for identification */
	ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_DEVNAME, &device_name);
	if (ret) {
		LOG_WRN("Failed to read device name register: %d", ret);
		device_name = 0x0000; /* Continue anyway */
	}

	if (status_reg & MAX17620_STATUS_POR) {
		LOG_INF("Fuel gauge POR — programming DesignCap and clearing POR");
		if (cfg->design_capacity_mah > 0U) {
			uint16_t design_raw = 0;

			(void)fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_DESIGNCAP,
							     &design_raw);
			(void)fuel_gauge_max20360_program_design_cap(dev, design_raw,
								    cfg->design_capacity_mah);
		}
		status_reg &= (uint16_t)~MAX17620_STATUS_POR;
		(void)fuel_gauge_max20360_write_reg16(dev, MAX17620_REG_STATUS, status_reg);
	}

	if (cfg->design_capacity_mah > 0U && !(status_reg & MAX17620_STATUS_POR)) {
		uint16_t design_raw;
		uint32_t read_mah;

		ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_DESIGNCAP, &design_raw);
		if (ret == 0) {
			read_mah = fuel_gauge_max20360_raw_to_mah(design_raw, cfg->rsense_mohm);
			if (read_mah == 0U ||
			    read_mah < (cfg->design_capacity_mah / 2U) ||
			    (read_mah > cfg->design_capacity_mah ?
			     read_mah - cfg->design_capacity_mah :
			     cfg->design_capacity_mah - read_mah) > 38U) {
				ret = fuel_gauge_max20360_program_design_cap(dev, design_raw,
									     cfg->design_capacity_mah);
				if (ret != 0) {
					LOG_WRN("DesignCap program failed: %d", ret);
				}
			} else {
				uint16_t repcap_raw;
				uint32_t rep_mah;

				LOG_INF("DesignCap OK: %u mAh (expected %u mAh)",
					read_mah, cfg->design_capacity_mah);
				ret = fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_REPCAP,
								    &repcap_raw);
				if (ret == 0) {
					rep_mah = fuel_gauge_max20360_raw_to_mah(repcap_raw,
										 cfg->rsense_mohm);
					if (rep_mah > cfg->design_capacity_mah +
						    (cfg->design_capacity_mah / 4U)) {
						uint32_t scaled =
							((uint32_t)repcap_raw *
							 fuel_gauge_max20360_mah_to_raw(
								 cfg->design_capacity_mah,
								 cfg->rsense_mohm)) /
							design_raw;

						ret = fuel_gauge_max20360_write_reg16(
							dev, MAX17620_REG_REPCAP,
							(uint16_t)scaled);
						if (ret == 0) {
							LOG_INF("REPCAP scaled to %u mAh",
								fuel_gauge_max20360_raw_to_mah(
									(uint16_t)scaled,
									cfg->rsense_mohm));
						}
					}
				}
			}
		}
	}

	/* Mark as initialized */
	data->initialized = true;

	LOG_INF("MAX20360 fuel gauge initialized (status=0x%04x, device=0x%04x)", 
		status_reg, device_name);
	return 0;
}

/* ===== Device Registration ===== */

#define FUEL_GAUGE_MAX20360_DEFINE(inst)                                    \
	static const struct fuel_gauge_max20360_config                          \
	fuel_gauge_max20360_config_##inst = {                                   \
		.bus = I2C_DT_SPEC_INST_GET(inst),                              \
		.design_capacity_mah =                                           \
			DT_INST_PROP_OR(inst, design_capacity_mah, 0),           \
		.design_voltage_mv =                                             \
			DT_INST_PROP_OR(inst, design_voltage_millivolt, 3700),   \
		.rsense_mohm = DT_INST_PROP_OR(inst, rsense_milliohms, 10),     \
	};                                                                       \
	                                                                         \
	static struct fuel_gauge_max20360_data fuel_gauge_max20360_data_##inst; \
	                                                                         \
	DEVICE_DT_INST_DEFINE(inst,                                             \
			      fuel_gauge_max20360_init,                          \
			      NULL,                                              \
			      &fuel_gauge_max20360_data_##inst,                  \
			      &fuel_gauge_max20360_config_##inst,                \
			      POST_KERNEL,                                       \
			      CONFIG_FUEL_GAUGE_INIT_PRIORITY,                  \
			      &fuel_gauge_max20360_api);

DT_INST_FOREACH_STATUS_OKAY(FUEL_GAUGE_MAX20360_DEFINE)

int max20360_fg_read_status(const struct device *dev, uint16_t *status)
{
	if (dev == NULL || status == NULL) {
		return -EINVAL;
	}

	return fuel_gauge_max20360_read_reg16(dev, MAX17620_REG_STATUS, status);
}

int max20360_fg_read_die_temp_c(const struct device *dev, int16_t *temp_c)
{
	uint16_t raw;
	int ret;

	if (dev == NULL || temp_c == NULL) {
		return -EINVAL;
	}
	ret = fuel_gauge_max20360_read_reg16(dev, MAX17260_REG_DIETEMP, &raw);
	if (ret) {
		return ret;
	}
	/* Same 1/256 °C/LSB format as Temp */
	*temp_c = (int16_t)(((int32_t)(int16_t)raw) / 256);
	return 0;
}

int max20360_fg_read_avg_ta_c(const struct device *dev, int16_t *temp_c)
{
	uint16_t raw;
	int ret;

	if (dev == NULL || temp_c == NULL) {
		return -EINVAL;
	}
	ret = fuel_gauge_max20360_read_reg16(dev, MAX17260_REG_AVGTA, &raw);
	if (ret) {
		return ret;
	}
	*temp_c = (int16_t)(((int32_t)(int16_t)raw) / 256);
	return 0;
}

