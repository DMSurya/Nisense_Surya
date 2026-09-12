/*
 * Copyright (c) 2025 MAX20360 Contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * Maxim MAX20360 battery charger — datasheet Rev 11 register map.
 */

#define DT_DRV_COMPAT maxim_max20360_charger

#include <zephyr/drivers/charger.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <stdlib.h>

#include "../regulator/max20360_regs.h"
#include "../regulator/max20360_ivmon.h"

LOG_MODULE_REGISTER(charger_max20360, CONFIG_CHARGER_LOG_LEVEL);

/* ChgCntl0 (0x10): FrcPChg[7], ChgBatReChg[6:5], ChgBatReg[4:1], ChgEn[0] */
#define MAX20360_CHGCNTL0_CHG_EN       BIT(0)
#define MAX20360_CHGCNTL0_BATREG_MASK  GENMASK(4, 1)
#define MAX20360_CHGCNTL0_BATREG_SHIFT 1
#define MAX20360_CHGCNTL0_FRCPCHG      BIT(7)

/* ChgCntl1 (0x11): BatPD / VPChg / IPChg / IChgDone — no IFCHG field */
#define MAX20360_CHGCNTL1_BATPD        BIT(7)

/* ILimCntl (0x0F): ILim[2:0]; preserve SysMinVlt / ILimBlank on RMW */
#define MAX20360_ILIMCNTL_ILIM_MASK    GENMASK(2, 0)

/* Status bits */
#define MAX20360_STATUS0_CHGSTAT_MASK  GENMASK(2, 0)
#define MAX20360_STATUS0_THMSTAT_MASK  GENMASK(5, 3)
#define MAX20360_STATUS0_THMSTAT_SHIFT 3
#define MAX20360_STATUS1_CHGTMO        BIT(0)
#define MAX20360_STATUS1_USBOK         BIT(3)
#define MAX20360_STATUS2_CHGTHMSD      BIT(7)
#define MAX20360_STATUS4_BATGOOD       BIT(7)
#define MAX20360_STATUS4_BATREGDONE    BIT(6)

#define MAX20360_LOCK_CHG              BIT(0)
#define MAX20360_UNLOCK_PASSWORD       0x55
#define MAX20360_LOCK_PASSWORD         0xAA

/* ChgBatReg: 4.05 V–4.60 V / 50 mV, code in [4:1] */
#define MAX20360_CCV_MIN_UV            4050000
#define MAX20360_CCV_MAX_UV            4600000
#define MAX20360_CCV_STEP_UV           50000

/* BatPD discharge wait: datasheet silent; RBAT_PD≈15 kΩ — board policy floor */
#define MAX20360_BATPD_WAIT_MS         50
#define MAX20360_PRESENCE_PROBE_MIN_MS 1000

/* StepChgCfg0: ChgStepRise from 3.80 V / 50 mV; ChgIStep × IFCHG */
#define MAX20360_STEP_RISE_MIN_UV      3800000
#define MAX20360_STEP_RISE_STEP_UV     50000

static const int max20360_ilim_ua[] = {
	50000, 90000, 150000, 200000, 300000, 400000, 450000, 1000000,
};

struct charger_max20360_config {
	struct i2c_dt_spec bus;
	const struct device *pmic;
	uint32_t charge_voltage_uv;
	uint32_t input_current_limit_ua;
	uint32_t riset_ohms;
};

struct charger_max20360_data {
	bool charging_enabled;
	uint32_t charge_voltage_uv;
	uint32_t ifchg_ua;
	uint32_t input_current_limit_ua;
	bool sysbatlim_active;
	bool chgsyslim_active;
	bool last_usb_ok;
	bool present_cached;
	int64_t last_probe_ms;
};

static int charger_bus_read(const struct device *dev, uint8_t reg, uint8_t *val)
{
	const struct charger_max20360_config *cfg = dev->config;

	return i2c_reg_read_byte_dt(&cfg->bus, reg, val);
}

static int charger_bus_write(const struct device *dev, uint8_t reg, uint8_t val)
{
	const struct charger_max20360_config *cfg = dev->config;

	return i2c_reg_write_byte_dt(&cfg->bus, reg, val);
}

static int charger_lock_pmic(const struct device *dev)
{
	const struct charger_max20360_config *cfg = dev->config;

	if (cfg->pmic != NULL && device_is_ready(cfg->pmic)) {
		return max20360_pmic_lock(cfg->pmic);
	}
	return 0;
}

static void charger_unlock_pmic(const struct device *dev)
{
	const struct charger_max20360_config *cfg = dev->config;

	if (cfg->pmic != NULL && device_is_ready(cfg->pmic)) {
		max20360_pmic_unlock(cfg->pmic);
	}
}

/** LockMsk: 0 = not masked; clear ChgLck then password. */
static int charger_hw_unlock(const struct device *dev)
{
	uint8_t lock_mask;
	int ret;

	ret = charger_bus_read(dev, MAX20360_REG_LOCKMASK, &lock_mask);
	if (ret) {
		return ret;
	}
	lock_mask &= (uint8_t)~MAX20360_LOCK_CHG;
	ret = charger_bus_write(dev, MAX20360_REG_LOCKMASK, lock_mask);
	if (ret) {
		return ret;
	}
	return charger_bus_write(dev, MAX20360_REG_LOCKUNLOCK, MAX20360_UNLOCK_PASSWORD);
}

static int charger_hw_lock(const struct device *dev)
{
	uint8_t lock_mask;
	int ret;

	ret = charger_bus_read(dev, MAX20360_REG_LOCKMASK, &lock_mask);
	if (ret) {
		return ret;
	}
	lock_mask &= (uint8_t)~MAX20360_LOCK_CHG;
	ret = charger_bus_write(dev, MAX20360_REG_LOCKMASK, lock_mask);
	if (ret) {
		return ret;
	}
	return charger_bus_write(dev, MAX20360_REG_LOCKUNLOCK, MAX20360_LOCK_PASSWORD);
}

static uint8_t encode_chg_bat_reg(uint32_t uv)
{
	uint32_t code;

	if (uv < MAX20360_CCV_MIN_UV) {
		uv = MAX20360_CCV_MIN_UV;
	}
	if (uv > MAX20360_CCV_MAX_UV) {
		uv = MAX20360_CCV_MAX_UV;
	}
	code = (uv - MAX20360_CCV_MIN_UV) / MAX20360_CCV_STEP_UV;
	if (code > 0x0FU) {
		code = 0x0FU;
	}
	return (uint8_t)code;
}

static int find_nearest_ilim(uint32_t ua)
{
	int best = 0;
	int best_diff = abs((int)ua - max20360_ilim_ua[0]);

	for (int i = 1; i < ARRAY_SIZE(max20360_ilim_ua); i++) {
		int diff = abs((int)ua - max20360_ilim_ua[i]);

		if (diff < best_diff) {
			best_diff = diff;
			best = i;
		}
	}
	return best;
}

static enum charger_status decode_chgstat(uint8_t chgstat)
{
	switch (chgstat & 0x07U) {
	case 0x0: /* off */
	case 0x1: /* temp suspend */
		return CHARGER_STATUS_NOT_CHARGING;
	case 0x2: /* precharge */
	case 0x3: /* CC */
	case 0x4: /* CV */
	case 0x5: /* maintain */
		return CHARGER_STATUS_CHARGING;
	case 0x6: /* maintain timer done */
		return CHARGER_STATUS_FULL;
	case 0x7: /* fault */
	default:
		return CHARGER_STATUS_UNKNOWN;
	}
}

/**
 * BatPD presence probe (datasheet ~p.3060). Holds PMIC mutex for entire sequence.
 * Gated to ≤1 Hz. Clears BatPD/FrcPChg on all exits.
 */
static int charger_presence_probe(const struct device *dev, bool *present)
{
	struct charger_max20360_data *data = dev->data;
	uint8_t st1, st4, cntl0, cntl1;
	int64_t now = k_uptime_get();
	bool usb_ok;
	int ret;

	*present = data->present_cached;

	ret = charger_bus_read(dev, MAX20360_REG_STATUS1, &st1);
	if (ret) {
		return ret;
	}
	usb_ok = (st1 & MAX20360_STATUS1_USBOK) != 0;
	if (!usb_ok) {
		data->last_usb_ok = false;
		data->present_cached = false;
		*present = false;
		return 0;
	}

	if (data->last_usb_ok && (now - data->last_probe_ms) < MAX20360_PRESENCE_PROBE_MIN_MS) {
		*present = data->present_cached;
		return 0;
	}
	data->last_usb_ok = true;
	data->last_probe_ms = now;

	ret = charger_hw_unlock(dev);
	if (ret) {
		return ret;
	}

	ret = charger_bus_read(dev, MAX20360_REG_CHGCNTL1, &cntl1);
	if (ret) {
		goto lock_out;
	}
	cntl1 |= MAX20360_CHGCNTL1_BATPD;
	ret = charger_bus_write(dev, MAX20360_REG_CHGCNTL1, cntl1);
	if (ret) {
		goto clear_batpd;
	}

	k_msleep(MAX20360_BATPD_WAIT_MS);

	/* USB may have dropped mid-wait */
	ret = charger_bus_read(dev, MAX20360_REG_STATUS1, &st1);
	if (ret || !(st1 & MAX20360_STATUS1_USBOK)) {
		data->present_cached = false;
		*present = false;
		goto clear_batpd;
	}

	ret = charger_bus_read(dev, MAX20360_REG_STATUS4, &st4);
	if (ret) {
		goto clear_batpd;
	}

	cntl1 &= (uint8_t)~MAX20360_CHGCNTL1_BATPD;
	(void)charger_bus_write(dev, MAX20360_REG_CHGCNTL1, cntl1);

	if (st4 & MAX20360_STATUS4_BATGOOD) {
		data->present_cached = true;
		*present = true;
		goto lock_out;
	}

	/* Fallback: FrcPChg + ChgEn → BatRegDone=1 ⇒ no pack */
	ret = charger_bus_read(dev, MAX20360_REG_CHGCNTL0, &cntl0);
	if (ret) {
		goto lock_out;
	}
	cntl0 |= MAX20360_CHGCNTL0_FRCPCHG | MAX20360_CHGCNTL0_CHG_EN;
	ret = charger_bus_write(dev, MAX20360_REG_CHGCNTL0, cntl0);
	if (ret) {
		goto clear_frc;
	}

	k_msleep(20);

	ret = charger_bus_read(dev, MAX20360_REG_STATUS4, &st4);
	if (ret == 0) {
		/* BatRegDone=1 with FrcPChg ⇒ floating BAT / no pack */
		data->present_cached = !(st4 & MAX20360_STATUS4_BATREGDONE);
		*present = data->present_cached;
	}

clear_frc:
	if (charger_bus_read(dev, MAX20360_REG_CHGCNTL0, &cntl0) == 0) {
		cntl0 &= (uint8_t)~MAX20360_CHGCNTL0_FRCPCHG;
		(void)charger_bus_write(dev, MAX20360_REG_CHGCNTL0, cntl0);
	}
	goto lock_out;

clear_batpd:
	if (charger_bus_read(dev, MAX20360_REG_CHGCNTL1, &cntl1) == 0) {
		cntl1 &= (uint8_t)~MAX20360_CHGCNTL1_BATPD;
		(void)charger_bus_write(dev, MAX20360_REG_CHGCNTL1, cntl1);
	}

lock_out:
	(void)charger_hw_lock(dev);
	return ret;
}

static int charger_max20360_set_enable(const struct device *dev, bool enable)
{
	struct charger_max20360_data *data = dev->data;
	uint8_t reg_val;
	int ret;

	ret = charger_lock_pmic(dev);
	if (ret) {
		return ret;
	}
	ret = charger_hw_unlock(dev);
	if (ret) {
		charger_unlock_pmic(dev);
		return ret;
	}

	ret = charger_bus_read(dev, MAX20360_REG_CHGCNTL0, &reg_val);
	if (ret == 0) {
		if (enable) {
			reg_val |= MAX20360_CHGCNTL0_CHG_EN;
		} else {
			reg_val &= (uint8_t)~MAX20360_CHGCNTL0_CHG_EN;
		}
		ret = charger_bus_write(dev, MAX20360_REG_CHGCNTL0, reg_val);
		if (ret == 0) {
			data->charging_enabled = enable;
		}
	}

	(void)charger_hw_lock(dev);
	charger_unlock_pmic(dev);
	return ret;
}

static int charger_configure_step(const struct device *dev)
{
	/* Default: ChgStepRise = 4.20 V (code 8), ChgIStep = 0.5×IFCHG (code 3) */
	uint8_t rise = (4200000U - MAX20360_STEP_RISE_MIN_UV) / MAX20360_STEP_RISE_STEP_UV;
	uint8_t cfg0, cfg1;
	int ret;

	ret = charger_bus_read(dev, MAX20360_REG_STEPCHGCFG0, &cfg0);
	if (ret) {
		return ret;
	}
	/* Preserve upper nibble (hysteresis etc.); pack ChgStepRise in low nibble */
	cfg0 = (cfg0 & 0xF0U) | (rise & 0x0FU);
	ret = charger_bus_write(dev, MAX20360_REG_STEPCHGCFG0, cfg0);
	if (ret) {
		return ret;
	}

	ret = charger_bus_read(dev, MAX20360_REG_STEPCHGCFG1, &cfg1);
	if (ret) {
		return ret;
	}
	cfg1 = (cfg1 & 0xF8U) | 0x03U; /* ChgIStep = 011 → 0.5×IFCHG */
	return charger_bus_write(dev, MAX20360_REG_STEPCHGCFG1, cfg1);
}

static int charger_max20360_get_prop(const struct device *dev, charger_prop_t prop,
				     union charger_propval *val)
{
	struct charger_max20360_data *data = dev->data;
	uint8_t st0, st1, st2, st3;
	bool present;
	int ret;

	ret = charger_lock_pmic(dev);
	if (ret) {
		return ret;
	}

	switch (prop) {
	case CHARGER_PROP_ONLINE:
		ret = charger_bus_read(dev, MAX20360_REG_STATUS1, &st1);
		if (ret) {
			break;
		}
		val->online = (st1 & MAX20360_STATUS1_USBOK) ? CHARGER_ONLINE_FIXED :
							       CHARGER_ONLINE_OFFLINE;
		break;

	case CHARGER_PROP_PRESENT:
		ret = charger_presence_probe(dev, &present);
		if (ret == 0) {
			val->present = present;
		}
		break;

	case CHARGER_PROP_CONSTANT_CHARGE_VOLTAGE_UV:
		val->const_charge_voltage_uv = data->charge_voltage_uv;
		break;

	case CHARGER_PROP_CONSTANT_CHARGE_CURRENT_UA:
		/* IFCHG is RISET-only — report derived µA */
		val->const_charge_current_ua = data->ifchg_ua;
		break;

	case CHARGER_PROP_INPUT_REGULATION_CURRENT_UA:
		val->input_current_regulation_current_ua = data->input_current_limit_ua;
		break;

	case CHARGER_PROP_STATUS:
		ret = charger_bus_read(dev, MAX20360_REG_STATUS0, &st0);
		if (ret) {
			break;
		}
		ret = charger_bus_read(dev, MAX20360_REG_STATUS1, &st1);
		if (ret) {
			break;
		}
		ret = charger_bus_read(dev, MAX20360_REG_STATUS2, &st2);
		if (ret) {
			break;
		}
		ret = charger_bus_read(dev, MAX20360_REG_STATUS3, &st3);
		if (ret == 0) {
			data->sysbatlim_active = !!(st3 & MAX20360_STATUS3_SYSBATLIM);
			data->chgsyslim_active = !!(st3 & MAX20360_STATUS3_CHGSYSLIM);
		}

		/*
		 * Without a pack, ChgStat can still show CC/CV/precharge while the
		 * charger regulates a floating BAT pin — that is not "charging".
		 * Prefer cached presence (probe); refresh at most once via PRESENT path.
		 */
		if ((st1 & MAX20360_STATUS1_USBOK) && !data->present_cached) {
			bool pack = false;

			(void)charger_presence_probe(dev, &pack);
			if (!pack) {
				val->status = CHARGER_STATUS_NOT_CHARGING;
				break;
			}
		}

		if (st1 & MAX20360_STATUS1_CHGTMO) {
			LOG_WRN("Charger timeout (Status1.ChgTmo)");
			val->status = CHARGER_STATUS_UNKNOWN;
		} else if (st2 & MAX20360_STATUS2_CHGTHMSD) {
			LOG_WRN("Charger thermal shutdown (Status2.ChgThmSD)");
			val->status = CHARGER_STATUS_NOT_CHARGING;
		} else {
			val->status = decode_chgstat(st0 & MAX20360_STATUS0_CHGSTAT_MASK);
		}

		LOG_DBG("ChgStat=0x%x UsbOk=%d present=%d ChgTmo=%d ThmSD=%d",
			st0 & 0x7, !!(st1 & MAX20360_STATUS1_USBOK),
			data->present_cached,
			!!(st1 & MAX20360_STATUS1_CHGTMO),
			!!(st2 & MAX20360_STATUS2_CHGTHMSD));
		break;

	default:
		ret = -ENOTSUP;
		break;
	}

	charger_unlock_pmic(dev);
	return ret;
}

static int charger_max20360_set_prop(const struct device *dev, charger_prop_t prop,
				     const union charger_propval *val)
{
	struct charger_max20360_data *data = dev->data;
	uint8_t reg_val;
	int ret;
	int idx;

	ret = charger_lock_pmic(dev);
	if (ret) {
		return ret;
	}
	ret = charger_hw_unlock(dev);
	if (ret) {
		charger_unlock_pmic(dev);
		return ret;
	}

	switch (prop) {
	case CHARGER_PROP_CONSTANT_CHARGE_VOLTAGE_UV:
		if (val->const_charge_voltage_uv < MAX20360_CCV_MIN_UV ||
		    val->const_charge_voltage_uv > MAX20360_CCV_MAX_UV) {
			ret = -EINVAL;
			break;
		}
		ret = charger_bus_read(dev, MAX20360_REG_CHGCNTL0, &reg_val);
		if (ret) {
			break;
		}
		reg_val &= (uint8_t)~MAX20360_CHGCNTL0_BATREG_MASK;
		reg_val |= (uint8_t)((encode_chg_bat_reg(val->const_charge_voltage_uv)
				      << MAX20360_CHGCNTL0_BATREG_SHIFT) &
				     MAX20360_CHGCNTL0_BATREG_MASK);
		ret = charger_bus_write(dev, MAX20360_REG_CHGCNTL0, reg_val);
		if (ret == 0) {
			data->charge_voltage_uv = val->const_charge_voltage_uv;
		}
		break;

	case CHARGER_PROP_CONSTANT_CHARGE_CURRENT_UA:
		/* IFCHG is set by RISET only — not I²C-programmable */
		LOG_WRN("Charge current is RISET-derived (%u uA); set ignored", data->ifchg_ua);
		ret = -ENOTSUP;
		break;

	case CHARGER_PROP_INPUT_REGULATION_CURRENT_UA:
		idx = find_nearest_ilim(val->input_current_regulation_current_ua);
		ret = charger_bus_read(dev, MAX20360_REG_ILIMCNTL, &reg_val);
		if (ret) {
			break;
		}
		reg_val = (reg_val & (uint8_t)~MAX20360_ILIMCNTL_ILIM_MASK) |
			  ((uint8_t)idx & MAX20360_ILIMCNTL_ILIM_MASK);
		ret = charger_bus_write(dev, MAX20360_REG_ILIMCNTL, reg_val);
		if (ret == 0) {
			data->input_current_limit_ua = max20360_ilim_ua[idx];
		}
		break;

	default:
		ret = -ENOTSUP;
		break;
	}

	(void)charger_hw_lock(dev);
	charger_unlock_pmic(dev);
	return ret;
}

static const struct charger_driver_api charger_max20360_api = {
	.get_property = charger_max20360_get_prop,
	.set_property = charger_max20360_set_prop,
};

static int charger_max20360_init(const struct device *dev)
{
	const struct charger_max20360_config *cfg = dev->config;
	struct charger_max20360_data *data = dev->data;
	uint8_t chip_id;
	int ret;

	if (!i2c_is_ready_dt(&cfg->bus)) {
		return -ENODEV;
	}

	/* IFCHG = KISET×VISET/RISET ≈ 2000×1/RISET → µA */
	if (cfg->riset_ohms == 0U) {
		LOG_ERR("riset-ohms must be non-zero");
		return -EINVAL;
	}
	data->ifchg_ua = 2000000000U / cfg->riset_ohms;
	data->charge_voltage_uv = cfg->charge_voltage_uv;
	data->input_current_limit_ua = cfg->input_current_limit_ua;
	data->charging_enabled = false;
	data->last_probe_ms = -MAX20360_PRESENCE_PROBE_MIN_MS;

	ret = charger_bus_read(dev, MAX20360_REG_CHIPID, &chip_id);
	if (ret) {
		return ret;
	}

	ret = charger_max20360_set_prop(dev, CHARGER_PROP_CONSTANT_CHARGE_VOLTAGE_UV,
		&(union charger_propval){ .const_charge_voltage_uv = data->charge_voltage_uv });
	if (ret) {
		LOG_WRN("Failed to set charge voltage: %d", ret);
	}

	ret = charger_max20360_set_prop(dev, CHARGER_PROP_INPUT_REGULATION_CURRENT_UA,
		&(union charger_propval){
			.input_current_regulation_current_ua = data->input_current_limit_ua });
	if (ret) {
		LOG_WRN("Failed to set input limit: %d", ret);
	}

	ret = charger_lock_pmic(dev);
	if (ret == 0) {
		ret = charger_hw_unlock(dev);
		if (ret == 0) {
			(void)charger_configure_step(dev);
			(void)charger_hw_lock(dev);
		}
		charger_unlock_pmic(dev);
	}

	ret = charger_max20360_set_enable(dev, true);
	if (ret) {
		LOG_ERR("Failed to enable charger: %d", ret);
	}

	LOG_INF("MAX20360 charger init ChipID=0x%02x V=%u mV IFCHG=%u mA (RISET=%u) ILIM=%u mA",
		chip_id, data->charge_voltage_uv / 1000U, data->ifchg_ua / 1000U,
		cfg->riset_ohms, data->input_current_limit_ua / 1000U);
	return 0;
}

#define CHARGER_MAX20360_PMIC(inst) \
	DEVICE_DT_GET(DT_CHILD(DT_INST_PARENT(inst), regulators))

#define CHARGER_MAX20360_DEFINE(inst)                                                      \
	static const struct charger_max20360_config charger_max20360_config_##inst = {     \
		.bus = I2C_DT_SPEC_GET(DT_INST_PARENT(inst)),                              \
		.pmic = CHARGER_MAX20360_PMIC(inst),                                       \
		.charge_voltage_uv =                                                       \
			DT_INST_PROP_OR(inst, charge_voltage_microvolt, 4200000),          \
		.input_current_limit_ua =                                                  \
			DT_INST_PROP_OR(inst, input_current_limit_microamp, 450000),       \
		.riset_ohms = DT_INST_PROP_OR(inst, riset_ohms, 4000),                     \
	};                                                                                 \
	static struct charger_max20360_data charger_max20360_data_##inst;                  \
	DEVICE_DT_INST_DEFINE(inst, charger_max20360_init, NULL,                           \
			      &charger_max20360_data_##inst,                               \
			      &charger_max20360_config_##inst, POST_KERNEL,                \
			      CONFIG_CHARGER_MAX20360_INIT_PRIORITY,                       \
			      &charger_max20360_api);

DT_INST_FOREACH_STATUS_OKAY(CHARGER_MAX20360_DEFINE)
