/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "power_batt.h"

#include <string.h>
#include <zephyr/kernel.h>
#include <errno.h>
#include <stdlib.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/charger.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/logging/log.h>
#include <zephyr/devicetree.h>

#if defined(CONFIG_ADC_MAX20360)
#include <max20360_adc.h>
#endif
#if defined(CONFIG_REGULATOR_MAX20360)
#include <max20360_ivmon.h>
#include <max20360_regs.h>
#endif
#if defined(CONFIG_FUEL_GAUGE_MAX20360)
#include <max20360_fg.h>
#endif

LOG_MODULE_REGISTER(power_batt, LOG_LEVEL_INF);

#define POWER_BATT_CHG_CURRENT_PRESENT_UA  30000L  /* 30 mA — charging into a cell */
#define POWER_BATT_CHG_CURRENT_IDLE_UA     30000L  /* below → idle / no cell on USB */
#define POWER_BATT_V_AGREE_MV              400     /* PMIC vs FG VCELL agreement */

static K_MUTEX_DEFINE(s_snap_lock);
static struct power_batt_snapshot s_snap_cache;
static int64_t s_snap_cache_time;
static bool s_snap_cache_valid;
static int64_t s_fg_ext_time;

/* MAX20360 internal SAR ADC (haptic/ADC slave) via standard Zephyr ADC API */
#if defined(CONFIG_ADC_MAX20360) && DT_HAS_ALIAS(pmicadc)

static int pmic_adc_read_mv(uint8_t channel_id, int32_t *mv)
{
	const struct device *adc = DEVICE_DT_GET(DT_ALIAS(pmicadc));
	const struct adc_channel_cfg ccfg = {
		.gain = ADC_GAIN_1,
		.reference = ADC_REF_INTERNAL,
		.acquisition_time = ADC_ACQ_TIME_DEFAULT,
		.channel_id = channel_id,
	};
	uint16_t raw = 0;
	struct adc_sequence seq = {
		.channels = BIT(channel_id),
		.buffer = &raw,
		.buffer_size = sizeof(raw),
		.resolution = 8,
	};
	int32_t val;
	int ret;

	if (!device_is_ready(adc)) {
		return -ENODEV;
	}

	ret = adc_channel_setup(adc, &ccfg);
	if (ret) {
		return ret;
	}

	ret = adc_read(adc, &seq);
	if (ret) {
		return ret;
	}

	val = raw;
	ret = adc_raw_to_millivolts(adc_ref_internal(adc), ADC_GAIN_1, 8, &val);
	if (ret) {
		return ret;
	}

	*mv = val;
	return 0;
}

#else

static int pmic_adc_read_mv(uint8_t channel_id, int32_t *mv)
{
	ARG_UNUSED(channel_id);
	ARG_UNUSED(mv);
	return -ENOTSUP;
}

#define MAX20360_ADC_CHAN_BAT 0

#endif /* CONFIG_ADC_MAX20360 && DT_HAS_ALIAS(pmicadc) */

const struct device *power_batt_fuel_gauge(void)
{
#if defined(CONFIG_FUEL_GAUGE) && DT_HAS_ALIAS(fuelgauge)
	return DEVICE_DT_GET_OR_NULL(DT_ALIAS(fuelgauge));
#else
	return NULL;
#endif
}

const struct device *power_batt_charger(void)
{
#if DT_HAS_ALIAS(charger0)
	return DEVICE_DT_GET_OR_NULL(DT_ALIAS(charger0));
#else
	return NULL;
#endif
}

const struct device *power_batt_pmic(void)
{
#if defined(CONFIG_REGULATOR_MAX20360) && DT_NODE_EXISTS(DT_NODELABEL(max20360_regulators))
	return DEVICE_DT_GET_OR_NULL(DT_NODELABEL(max20360_regulators));
#else
	return NULL;
#endif
}

bool power_batt_fuel_gauge_ready(void)
{
	const struct device *fg = power_batt_fuel_gauge();

	return (fg != NULL) && device_is_ready(fg);
}

bool power_batt_charger_ready(void)
{
	const struct device *chg = power_batt_charger();

	return (chg != NULL) && device_is_ready(chg);
}

bool power_batt_pmic_ready(void)
{
	const struct device *pmic = power_batt_pmic();

	return (pmic != NULL) && device_is_ready(pmic);
}

static int soc_estimate_from_voltage_uv(int32_t voltage_uv)
{
	int mv = (int)(voltage_uv / 1000);

	if (mv <= POWER_BATT_EMPTY_VOLTAGE_MV) {
		return 0;
	}
	if (mv >= POWER_BATT_FULL_VOLTAGE_MV) {
		return 100;
	}
	return (mv - POWER_BATT_EMPTY_VOLTAGE_MV) * 100 /
	       (POWER_BATT_FULL_VOLTAGE_MV - POWER_BATT_EMPTY_VOLTAGE_MV);
}

int power_batt_voltage_uv(int32_t *voltage_uv)
{
	int32_t mv;

	if (voltage_uv == NULL) {
		return -EINVAL;
	}

	if (pmic_adc_read_mv(MAX20360_ADC_CHAN_BAT, &mv) == 0) {
		*voltage_uv = mv * 1000;
		return 0;
	}

#if defined(CONFIG_FUEL_GAUGE)
	return power_batt_fg_vcell_uv(voltage_uv);
#else
	return -ENODEV;
#endif
}

int power_batt_fg_vcell_uv(int32_t *voltage_uv)
{
#if defined(CONFIG_FUEL_GAUGE)
	const struct device *fg = power_batt_fuel_gauge();
	union fuel_gauge_prop_val val;

	if (voltage_uv == NULL) {
		return -EINVAL;
	}
	if (fg == NULL || !device_is_ready(fg)) {
		return -ENODEV;
	}
	if (fuel_gauge_get_prop(fg, FUEL_GAUGE_VOLTAGE, &val) != 0) {
		return -EIO;
	}
	*voltage_uv = val.voltage;
	return 0;
#else
	ARG_UNUSED(voltage_uv);
	return -ENOTSUP;
#endif
}

static bool read_usb_online(bool *online)
{
#if DT_HAS_ALIAS(charger0)
	const struct device *chg = power_batt_charger();
	union charger_propval val;

	if (online == NULL) {
		return false;
	}
	*online = false;
	if (chg == NULL || !device_is_ready(chg)) {
		return false;
	}
	if (charger_get_prop(chg, CHARGER_PROP_ONLINE, &val) != 0) {
		return false;
	}
	*online = (val.online != CHARGER_ONLINE_OFFLINE);
	return true;
#else
	ARG_UNUSED(online);
	return false;
#endif
}

static enum power_batt_cell_state evaluate_cell_state(const struct power_batt_snapshot *snap)
{
	int32_t pmic_mv = snap->pmic_vbat_uv / 1000;
	int32_t fg_mv = snap->fg_vcell_uv / 1000;
	int32_t i_abs_ua = snap->current_ua >= 0 ? snap->current_ua : -snap->current_ua;

	if (snap->usb_online) {
		/*
		 * USB present: charger can hold BAT at VBAT_REG with no pack.
		 * BatRegDone + negligible charge current ⇒ absent (datasheet §presence).
		 */
		if (snap->bat_reg_done && i_abs_ua < POWER_BATT_CHG_CURRENT_IDLE_UA) {
			return POWER_BATT_CELL_ABSENT;
		}
		if (i_abs_ua >= POWER_BATT_CHG_CURRENT_PRESENT_UA) {
			return POWER_BATT_CELL_PRESENT;
		}
		/* MAX17260: Bst clear = present */
		if (!snap->fg_pack_absent && fg_mv >= 3000) {
			return POWER_BATT_CELL_PRESENT;
		}
		if (!snap->bat_good && pmic_mv > 0 && pmic_mv < 2500) {
			return POWER_BATT_CELL_ABSENT;
		}
	} else {
		/* On battery power: sub-UVLO ⇒ no usable pack. */
		if (pmic_mv > 0 && pmic_mv < 3000 && fg_mv > 0 && fg_mv < 3000) {
			return POWER_BATT_CELL_ABSENT;
		}
		if (pmic_mv >= 3000 || fg_mv >= 3000) {
			return POWER_BATT_CELL_PRESENT;
		}
		if (i_abs_ua > POWER_BATT_CHG_CURRENT_IDLE_UA) {
			return POWER_BATT_CELL_PRESENT;
		}
	}

	/* Both rails in Li-ion range and agree ⇒ present. */
	if (pmic_mv >= 3000 && fg_mv >= 3000 &&
	    abs((int)(pmic_mv - fg_mv)) < POWER_BATT_V_AGREE_MV) {
		return POWER_BATT_CELL_PRESENT;
	}

	if (pmic_mv >= 3000 && fg_mv >= 3000) {
		/* Large disagreement — trust charge activity over voltage alone. */
		if (i_abs_ua >= POWER_BATT_CHG_CURRENT_PRESENT_UA) {
			return POWER_BATT_CELL_PRESENT;
		}
	}

	return POWER_BATT_CELL_UNKNOWN;
}

#if defined(CONFIG_FUEL_GAUGE)
static void power_batt_fill_fg_ext(const struct device *fg, struct power_batt_snapshot *snap)
{
	union fuel_gauge_prop_val val;
	int64_t now = k_uptime_get();

	if (fg == NULL || snap == NULL) {
		return;
	}

	/* Slow gauge fields: one batched read pass every 10 s (same cadence as power_mgr). */
	if (s_fg_ext_time != 0 && (now - s_fg_ext_time) < 10000) {
		if (s_snap_cache_valid) {
			snap->fg_ext = s_snap_cache.fg_ext;
		}
		return;
	}

	memset(&snap->fg_ext, 0, sizeof(snap->fg_ext));

	if (fuel_gauge_get_prop(fg, FUEL_GAUGE_TEMPERATURE, &val) == 0) {
		snap->fg_ext.temp_c = (int16_t)((val.temperature / 10) - 273);
	}
	if (fuel_gauge_get_prop(fg, FUEL_GAUGE_CYCLE_COUNT, &val) == 0) {
		snap->fg_ext.cycle_count = (uint16_t)val.cycle_count;
	}
	if (fuel_gauge_get_prop(fg, FUEL_GAUGE_REMAINING_CAPACITY, &val) == 0) {
		/* Zephyr remaining_capacity is µAh */
		snap->fg_ext.remaining_mah = (uint16_t)(val.remaining_capacity / 1000U);
	}
	if (fuel_gauge_get_prop(fg, FUEL_GAUGE_FULL_CHARGE_CAPACITY, &val) == 0) {
		/* Zephyr full_charge_capacity is µAh */
		snap->fg_ext.full_mah = (uint16_t)(val.full_charge_capacity / 1000U);
	}
	if (fuel_gauge_get_prop(fg, FUEL_GAUGE_DESIGN_CAPACITY, &val) == 0) {
		/* Zephyr design_cap is already mAh (uint16) */
		snap->fg_ext.design_mah = val.design_cap;
	}
	if (snap->fg_ext.design_mah == 0U) {
		snap->fg_ext.design_mah = POWER_BATT_DESIGN_CAPACITY_MAH;
	}
	/* Clamp learned full/remaining to KM20-class design (±100%) */
	if (snap->fg_ext.full_mah == 0U ||
	    snap->fg_ext.full_mah > snap->fg_ext.design_mah * 2U ||
	    snap->fg_ext.full_mah < snap->fg_ext.design_mah / 2U) {
		snap->fg_ext.full_mah = snap->fg_ext.design_mah;
	}
	if (snap->fg_ext.remaining_mah > snap->fg_ext.full_mah) {
		snap->fg_ext.remaining_mah = snap->fg_ext.full_mah;
	}
	if (fuel_gauge_get_prop(fg, FUEL_GAUGE_RUNTIME_TO_EMPTY, &val) == 0 &&
	    val.runtime_to_empty > 0) {
		snap->fg_ext.time_to_empty_min = (uint16_t)val.runtime_to_empty;
	}
	if (fuel_gauge_get_prop(fg, FUEL_GAUGE_RUNTIME_TO_FULL, &val) == 0 &&
	    val.runtime_to_full > 0) {
		snap->fg_ext.time_to_full_min = (uint16_t)val.runtime_to_full;
	}
	if (fuel_gauge_get_prop(fg, FUEL_GAUGE_AVG_CURRENT, &val) == 0) {
		snap->fg_ext.avg_current_ma = (int16_t)(val.avg_current / 1000);
	}

	snap->fg_ext.valid = true;
	s_fg_ext_time = now;
}
#endif /* CONFIG_FUEL_GAUGE */

static int power_batt_snapshot_fill(struct power_batt_snapshot *snap)
{
	const struct device *fg;
	const struct device *pmic;
	uint8_t st4 = 0;
	uint8_t st3 = 0;
	uint16_t fg_st = 0;

	memset(snap, 0, sizeof(*snap));
	snap->cell_state = POWER_BATT_CELL_UNKNOWN;
	snap->fg_soc_percent = -1;

	(void)power_batt_voltage_uv(&snap->pmic_vbat_uv);

	fg = power_batt_fuel_gauge();
	if (power_batt_fuel_gauge_ready()) {
		union fuel_gauge_prop_val val;

		if (fuel_gauge_get_prop(fg, FUEL_GAUGE_VOLTAGE, &val) == 0) {
			snap->fg_vcell_uv = val.voltage;
		}
		if (fuel_gauge_get_prop(fg, FUEL_GAUGE_CURRENT, &val) == 0) {
			snap->current_ua = val.current;
		}
		if (fuel_gauge_get_prop(fg, FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE, &val) == 0) {
			snap->fg_soc_percent = (int)val.relative_state_of_charge;
		}
#if defined(CONFIG_FUEL_GAUGE_MAX20360)
		if (max20360_fg_read_status(fg, &fg_st) == 0) {
			/* Bst=1 ⇒ absent (MAX17260 Table 7) */
			snap->fg_pack_absent = (fg_st & MAX20360_FG_STATUS_BST) != 0;
		}
#endif
#if defined(CONFIG_FUEL_GAUGE)
		power_batt_fill_fg_ext(fg, snap);
#endif
	} else if (s_snap_cache_valid) {
		snap->fg_ext = s_snap_cache.fg_ext;
	}

	(void)read_usb_online(&snap->usb_online);

	pmic = power_batt_pmic();
	if (power_batt_pmic_ready()) {
#if defined(CONFIG_REGULATOR_MAX20360)
		if (max20360_read_status4(pmic, &st4) == 0) {
			snap->bat_good = (st4 & MAX20360_STATUS4_BATGOOD) != 0;
			snap->bat_reg_done = (st4 & MAX20360_STATUS4_BATREGDONE) != 0;
		}
		if (max20360_read_status3(pmic, &st3) == 0) {
			snap->sys_bat_lim = (st3 & MAX20360_STATUS3_SYSBATLIM) != 0;
		}
#endif
	}

	snap->cell_state = evaluate_cell_state(snap);

	/* No pack: do not advertise gauge capacity (ModelGauge floats without a cell). */
	if (snap->cell_state == POWER_BATT_CELL_ABSENT) {
		snap->fg_ext.remaining_mah = 0;
		snap->fg_ext.full_mah = 0;
		snap->fg_ext.time_to_empty_min = 0;
		snap->fg_ext.time_to_full_min = 0;
		/* Keep design_mah as the known KM20 rating for UI/config. */
		if (snap->fg_ext.design_mah == 0U) {
			snap->fg_ext.design_mah = POWER_BATT_DESIGN_CAPACITY_MAH;
		}
	}

	return 0;
}

int power_batt_snapshot(struct power_batt_snapshot *snap)
{
	int ret;

	if (snap == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&s_snap_lock, K_FOREVER);
	ret = power_batt_snapshot_fill(snap);
	if (ret == 0) {
		s_snap_cache = *snap;
		s_snap_cache_time = k_uptime_get();
		s_snap_cache_valid = true;
	}
	k_mutex_unlock(&s_snap_lock);
	return ret;
}

int power_batt_snapshot_cached(struct power_batt_snapshot *snap, uint32_t max_age_ms)
{
	int64_t now;

	if (snap == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&s_snap_lock, K_FOREVER);
	now = k_uptime_get();
	if (max_age_ms > 0U && s_snap_cache_valid &&
	    (now - s_snap_cache_time) <= (int64_t)max_age_ms) {
		*snap = s_snap_cache;
		k_mutex_unlock(&s_snap_lock);
		return 0;
	}
	k_mutex_unlock(&s_snap_lock);

	return power_batt_snapshot(snap);
}

bool power_batt_cell_present(void)
{
	struct power_batt_snapshot snap;

	if (power_batt_snapshot(&snap) != 0) {
		return false;
	}

	return snap.cell_state == POWER_BATT_CELL_PRESENT;
}

const char *power_batt_cell_state_str(enum power_batt_cell_state state)
{
	switch (state) {
	case POWER_BATT_CELL_PRESENT:
		return "Present";
	case POWER_BATT_CELL_ABSENT:
		return "Absent";
	default:
		return "Unknown";
	}
}

static int soc_percent_from_snap(const struct power_batt_snapshot *snap, int *soc_percent)
{
	int estimated;

	if (snap->cell_state == POWER_BATT_CELL_ABSENT) {
		*soc_percent = 0;
		return 0;
	}

	/* USB bench: idle + gauge reports pack absent ⇒ external power only. */
	if (snap->usb_online && snap->cell_state == POWER_BATT_CELL_UNKNOWN) {
		int32_t i_abs = snap->current_ua >= 0 ? snap->current_ua : -snap->current_ua;

		if (i_abs < POWER_BATT_CHG_CURRENT_IDLE_UA && snap->fg_pack_absent) {
			*soc_percent = 0;
			return 0;
		}
	}

#if defined(CONFIG_FUEL_GAUGE)
	if (snap->fg_soc_percent >= 0 && snap->fg_soc_percent <= 100) {
		uint8_t soc = (uint8_t)snap->fg_soc_percent;
		uint16_t batt_mv = (uint16_t)(snap->pmic_vbat_uv / 1000U);
		int estimated_mv = soc_estimate_from_voltage_uv((int32_t)batt_mv * 1000L);

		if (estimated_mv >= 0 && soc > (uint8_t)estimated_mv + 25U && estimated_mv < 80) {
			soc = (uint8_t)estimated_mv;
		}
		*soc_percent = soc;
		return 0;
	}
#endif

	if (snap->pmic_vbat_uv > 0) {
		estimated = soc_estimate_from_voltage_uv(snap->pmic_vbat_uv);
		*soc_percent = estimated;
		return 0;
	}
	if (snap->fg_vcell_uv > 0) {
		estimated = soc_estimate_from_voltage_uv(snap->fg_vcell_uv);
		*soc_percent = estimated;
		return 0;
	}

	return -ENODEV;
}

int power_batt_soc_percent(int *soc_percent)
{
	struct power_batt_snapshot snap;

	if (soc_percent == NULL) {
		return -EINVAL;
	}

	if (power_batt_snapshot(&snap) != 0) {
		return -ENODEV;
	}

	return soc_percent_from_snap(&snap, soc_percent);
}

int power_batt_soc_percent_cached(int *soc_percent, uint32_t max_age_ms)
{
	struct power_batt_snapshot snap;

	if (soc_percent == NULL) {
		return -EINVAL;
	}

	if (power_batt_snapshot_cached(&snap, max_age_ms) != 0) {
		return -ENODEV;
	}

	return soc_percent_from_snap(&snap, soc_percent);
}

int power_batt_soc_from_snapshot(const struct power_batt_snapshot *snap, int *soc_percent)
{
	if (snap == NULL || soc_percent == NULL) {
		return -EINVAL;
	}

	return soc_percent_from_snap(snap, soc_percent);
}

bool power_batt_clamp_soc_to_voltage(uint16_t battery_mv, uint8_t *soc_percent)
{
	int estimated;

	if (soc_percent == NULL) {
		return false;
	}

	if (!power_batt_cell_present()) {
		if (*soc_percent != 0) {
			*soc_percent = 0;
			return true;
		}
		return false;
	}

	estimated = soc_estimate_from_voltage_uv((int32_t)battery_mv * 1000L);
	if (estimated < 0) {
		estimated = 0;
	}
	if (estimated > 100) {
		estimated = 100;
	}

	if (*soc_percent > (uint8_t)estimated + 25U && estimated < 80) {
		*soc_percent = (uint8_t)estimated;
		return true;
	}

	return false;
}
