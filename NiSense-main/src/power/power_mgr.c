/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "power_mgr.h"

#include "config_manager.h"
#include "power_batt.h"

#if defined(CONFIG_APP_FEATURE_DISPLAY)
#include "device_prefs.h"
#endif

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#if defined(CONFIG_REGULATOR_MAX20360)
#include <max20360_ivmon.h>
#include <max20360_regs.h>
#endif

#if defined(CONFIG_APP_HEALTH_SCHED)
#include "health_sched.h"
#endif
#if defined(CONFIG_APP_FEATURE_BLE)
#include "ble_gatt.h"
#endif
#if defined(CONFIG_BT)
#include "led_status.h"
#endif
#if defined(CONFIG_APP_FEATURE_DISPLAY)
#include "ui_hold_overlay.h"
#endif

LOG_MODULE_REGISTER(power_mgr, LOG_LEVEL_INF);

struct power_mgr_cfg {
	uint8_t poll_interval_s;
	uint8_t sleep_soc_pct;
	uint8_t low_soc_pct;
	uint8_t resume_soc_pct;
	uint16_t sleep_timeout_s;
	bool auto_sleep;
	uint8_t brightness;
};

static struct power_mgr_cfg s_cfg;
static enum power_mgr_state s_state = POWER_MGR_STATE_NORMAL;
static struct power_batt_snapshot s_snap;
static int s_soc = -1;
static bool s_display_asleep;
static bool s_critical_latched;
static int64_t s_last_activity_ms;

static struct k_work_delayable s_poll_work;
static struct k_work s_pmic_event_work;
static K_MUTEX_DEFINE(s_lock);

#if DT_HAS_CHOSEN(zephyr_display)
static const struct device *const s_disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
#define HAS_DISPLAY 1
#else
#define HAS_DISPLAY 0
#endif

static void load_cfg_from_manager(void)
{
	const struct app_config *app = config_manager_get();

	memset(&s_cfg, 0, sizeof(s_cfg));
	s_cfg.poll_interval_s = POWER_MGR_DEFAULT_POLL_INTERVAL_S;
	s_cfg.sleep_soc_pct = POWER_MGR_DEFAULT_SLEEP_SOC_PCT;
	s_cfg.low_soc_pct = POWER_MGR_DEFAULT_LOW_SOC_PCT;
	s_cfg.resume_soc_pct = POWER_MGR_DEFAULT_RESUME_SOC_PCT;
	s_cfg.sleep_timeout_s = POWER_MGR_DEFAULT_SLEEP_TIMEOUT_S;
	s_cfg.auto_sleep = true;
	s_cfg.brightness = 80;

	if (app == NULL) {
		return;
	}

	if (app->device.batt_poll_interval_s >= 5U) {
		s_cfg.poll_interval_s = app->device.batt_poll_interval_s;
	}
	if (app->device.batt_sleep_soc_pct > 0U) {
		s_cfg.sleep_soc_pct = app->device.batt_sleep_soc_pct;
	}
	if (app->device.batt_low_soc_pct > 0U) {
		s_cfg.low_soc_pct = app->device.batt_low_soc_pct;
	}
	if (app->device.batt_resume_soc_pct > 0U) {
		s_cfg.resume_soc_pct = app->device.batt_resume_soc_pct;
	}
	if (app->device.sleep_timeout_s > 0U) {
		s_cfg.sleep_timeout_s = app->device.sleep_timeout_s;
	}
	s_cfg.auto_sleep = app->device.auto_sleep;
	s_cfg.brightness = app->device.brightness;
}

static void display_sleep(void)
{
	if (s_display_asleep) {
		return;
	}

#if HAS_DISPLAY
	if (device_is_ready(s_disp)) {
		(void)display_blanking_on(s_disp);
	}
#endif
#if defined(CONFIG_APP_FEATURE_DISPLAY)
	ui_hold_overlay_cancel();

	int ret = device_prefs_apply_brightness(0);

	if (ret != 0) {
		LOG_WRN("Backlight off failed: %d", ret);
	} else {
		LOG_INF("Backlight off");
	}
#endif
	s_display_asleep = true;
	LOG_INF("Display asleep");
}

static void display_wake(void)
{
#if HAS_DISPLAY
	if (device_is_ready(s_disp)) {
		(void)display_blanking_off(s_disp);
	}
#endif
#if defined(CONFIG_APP_FEATURE_DISPLAY)
	int ret = device_prefs_apply_brightness(s_cfg.brightness);

	if (ret != 0) {
		LOG_WRN("Backlight on failed: %d", ret);
	}
#endif
	s_display_asleep = false;
}

static enum power_mgr_state evaluate_state(const struct power_batt_snapshot *snap, int soc)
{
	if (snap->usb_online) {
		/*
		 * Externally powered: allow heavy work. An absent pack reports
		 * SoC 0 — that must not keep CRITICAL latched forever.
		 */
		if (snap->cell_state == POWER_BATT_CELL_ABSENT) {
			return POWER_MGR_STATE_NORMAL;
		}
		if (s_critical_latched && soc >= 0 && soc >= (int)s_cfg.resume_soc_pct) {
			return POWER_MGR_STATE_CHARGING;
		}
		if (s_critical_latched) {
			return POWER_MGR_STATE_CRITICAL;
		}
		return POWER_MGR_STATE_NORMAL;
	}

	if (soc >= 0 && soc <= (int)s_cfg.sleep_soc_pct) {
		return POWER_MGR_STATE_CRITICAL;
	}

	if (soc >= 0 && soc <= (int)s_cfg.low_soc_pct) {
		return POWER_MGR_STATE_LOW;
	}

	return POWER_MGR_STATE_NORMAL;
}

static void apply_state_side_effects(enum power_mgr_state new_state)
{
	if (new_state == s_state) {
		return;
	}

	LOG_INF("Power state %d -> %d (soc=%d)", s_state, new_state, s_soc);
	s_state = new_state;

	switch (new_state) {
	case POWER_MGR_STATE_CRITICAL:
		s_critical_latched = true;
		display_sleep();
#if defined(CONFIG_APP_HEALTH_SCHED)
		health_sched_set_auto_enabled(false);
#endif
#if defined(CONFIG_BT)
		led_status_set(LED_STATUS_BATTERY_LOW);
#endif
		break;
	case POWER_MGR_STATE_CHARGING:
		s_critical_latched = false;
#if defined(CONFIG_APP_HEALTH_SCHED)
		health_sched_set_auto_enabled(true);
#endif
#if defined(CONFIG_BT)
		led_status_set(LED_STATUS_CHARGING);
#endif
		break;
	case POWER_MGR_STATE_LOW:
#if defined(CONFIG_BT)
		led_status_set(LED_STATUS_BATTERY_LOW);
#endif
		break;
	case POWER_MGR_STATE_NORMAL:
	default:
		s_critical_latched = false;
#if defined(CONFIG_BT)
		led_status_set(LED_STATUS_IDLE);
#endif
		break;
	}
}

static void poll_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	int soc = -1;
	enum power_mgr_state new_state;

	k_mutex_lock(&s_lock, K_FOREVER);
	(void)power_batt_snapshot(&s_snap);
	(void)power_batt_soc_from_snapshot(&s_snap, &soc);
	s_soc = soc;
	new_state = evaluate_state(&s_snap, soc);
	apply_state_side_effects(new_state);
#if defined(CONFIG_BT)
	/* USB present: charging LED even when policy state stays NORMAL. */
	if (s_snap.usb_online && new_state != POWER_MGR_STATE_CRITICAL) {
		led_status_set(LED_STATUS_CHARGING);
	}
#endif
	k_mutex_unlock(&s_lock);

#if defined(CONFIG_APP_FEATURE_BLE)
	if (soc >= 0) {
		(void)ble_gatt_set_battery_level((uint8_t)CLAMP(soc, 0, 100));
	}
#endif

	(void)k_work_reschedule(&s_poll_work, K_SECONDS(s_cfg.poll_interval_s));
}

static void pmic_event_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	(void)k_work_reschedule(&s_poll_work, K_NO_WAIT);
}

#if defined(CONFIG_REGULATOR_MAX20360) && DT_NODE_EXISTS(DT_NODELABEL(max20360_regulators))
static void pmic_int_handler(const struct device *dev, const uint8_t int_status[4])
{
	if (int_status[0] & MAX20360_INT0_USBOKINT) {
		LOG_INF("PMIC INT: USB/charger present");
	}

	if (int_status[2] & MAX20360_INT2_SYSBATLIMINT) {
		LOG_WRN("PMIC INT: SysBatLim — input power insufficient for load");
	}

	if (int_status[3] & MAX20360_INT3_BATGOODINT) {
		uint8_t st4;

		if (max20360_read_status4(dev, &st4) == 0 &&
		    (st4 & MAX20360_STATUS4_BATGOOD) == 0) {
			LOG_WRN("PMIC INT: BatGood low — VBAT below UVLO");
		} else {
			LOG_INF("PMIC INT: BatGood status change");
		}
	}

	if (int_status[3] & MAX20360_INT3_BSTFAULTINT) {
		LOG_ERR("PMIC INT: boost regulator fault");
	}

	(void)k_work_submit(&s_pmic_event_work);
}
#endif

#if DT_HAS_CHOSEN(zephyr_touch)
static void touch_input_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	if (evt->type == INPUT_EV_ABS || evt->type == INPUT_EV_KEY) {
		power_mgr_activity_notify();
	}
}
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_CHOSEN(zephyr_touch)), touch_input_cb, NULL);
#endif

int power_mgr_init(void)
{
	load_cfg_from_manager();
	s_last_activity_ms = k_uptime_get();

	k_work_init_delayable(&s_poll_work, poll_work_handler);
	k_work_init(&s_pmic_event_work, pmic_event_work_handler);

#if defined(CONFIG_REGULATOR_MAX20360) && DT_NODE_EXISTS(DT_NODELABEL(max20360_regulators))
	{
		const struct device *pmic = DEVICE_DT_GET(DT_NODELABEL(max20360_regulators));
		int pmic_ret = max20360_pmic_int_init(pmic, pmic_int_handler);

		if (pmic_ret != 0) {
			LOG_WRN("PMIC INT init failed: %d (SOC-only battery policy)", pmic_ret);
		} else {
			LOG_INF("PMIC INT (P0.31) enabled — BatGood, SysBatLim, UsbOk");
		}
	}
#endif

	(void)k_work_schedule(&s_poll_work, K_SECONDS(2));

	LOG_INF("Power mgr: poll=%u s, idle=%u s, sleep<=%u%%, low<=%u%%",
		s_cfg.poll_interval_s, s_cfg.sleep_timeout_s,
		s_cfg.sleep_soc_pct, s_cfg.low_soc_pct);
	return 0;
}

void power_mgr_apply_config(void)
{
	k_mutex_lock(&s_lock, K_FOREVER);
	load_cfg_from_manager();
	k_mutex_unlock(&s_lock);
	(void)k_work_reschedule(&s_poll_work, K_NO_WAIT);
}

void power_mgr_tick(void)
{
	if (!s_cfg.auto_sleep || s_state == POWER_MGR_STATE_CRITICAL) {
		return;
	}

	int64_t now = k_uptime_get();
	uint32_t timeout_ms = (uint32_t)s_cfg.sleep_timeout_s * 1000U;

	if (!s_display_asleep && (now - s_last_activity_ms) >= (int64_t)timeout_ms) {
		display_sleep();
	}
}

void power_mgr_activity_notify(void)
{
	s_last_activity_ms = k_uptime_get();
	if (s_state != POWER_MGR_STATE_CRITICAL) {
		display_wake();
	}
}

enum power_mgr_state power_mgr_get_state(void)
{
	return s_state;
}

bool power_mgr_display_asleep(void)
{
	return s_display_asleep;
}

int power_mgr_get_soc(int *soc_percent)
{
	if (soc_percent == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&s_lock, K_FOREVER);
	if (s_soc < 0) {
		k_mutex_unlock(&s_lock);
		return -ENODEV;
	}
	*soc_percent = s_soc;
	k_mutex_unlock(&s_lock);
	return 0;
}

int power_mgr_get_snapshot(struct power_batt_snapshot *snap)
{
	if (snap == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&s_lock, K_FOREVER);
	*snap = s_snap;
	k_mutex_unlock(&s_lock);
	return 0;
}

uint32_t power_mgr_poll_interval_ms(void)
{
	return (uint32_t)s_cfg.poll_interval_s * 1000U;
}

bool power_mgr_allow_heavy_work(void)
{
	/*
	 * USB can power LEDs/sensors even with pack absent / SoC 0.
	 * CRITICAL while USB+pack still charging from empty remains blocked
	 * until resume SoC (evaluate_state); USB+absent is NORMAL.
	 */
	if (s_snap.usb_online && s_state != POWER_MGR_STATE_CRITICAL) {
		return true;
	}
	return s_state != POWER_MGR_STATE_CRITICAL;
}
