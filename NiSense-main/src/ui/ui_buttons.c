/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Wires PMIC PFN1 (power) and SW1 (nav) GPIO buttons to UI / power_mgr /
 * health scheduler, with full-screen hold countdown overlays.
 */

#include "nav_button.h"
#include "power_button.h"
#include "ui_hold_overlay.h"

#include "buzzer.h"
#include "config_manager.h"
#include "health_sched.h"
#include "power_mgr.h"
#include "ui_common.h"

#include <zephyr/autoconf.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ui_buttons, LOG_LEVEL_INF);

static uint32_t s_measure_hold_ms;
static uint32_t s_power_hold_ms;

/** PMIC KIN shutdown threshold (MAX20360 Figure 18 / PwrRstCfg 0110). */
#define PMIC_KIN_SHUTDOWN_MS 12000U

/** Overlay starts at HOLD_ARMED; duration is the remaining hold time. */
static uint32_t hold_overlay_duration_ms(uint32_t hold_ms)
{
	if (hold_ms > HW_BUTTON_HOLD_ARM_MS) {
		return hold_ms - HW_BUTTON_HOLD_ARM_MS;
	}
	return hold_ms;
}

static uint32_t measure_hold_ms(void)
{
#if defined(CONFIG_APP_FEATURE_CONFIG_PERSIST)
	const struct app_config *app = config_manager_get();

	if (app != NULL && app->device.measure_hold_ms >= 800U) {
		return app->device.measure_hold_ms;
	}
#endif
	return (uint32_t)CONFIG_UI_MEASURE_LONG_PRESS_MS;
}

static uint32_t power_hold_ms(void)
{
	uint32_t ms = (uint32_t)CONFIG_UI_POWER_LONG_PRESS_MS;

#if defined(CONFIG_APP_FEATURE_CONFIG_PERSIST)
	const struct app_config *app = config_manager_get();

	if (app != NULL && app->device.power_hold_ms >= 1000U) {
		ms = app->device.power_hold_ms;
	}
#endif
	/* KIN mode PMIC always needs >12 s; clamp stale persisted values. */
	if (ms < PMIC_KIN_SHUTDOWN_MS) {
		ms = PMIC_KIN_SHUTDOWN_MS;
	}
	return ms;
}

static void apply_hold_config(void)
{
	s_measure_hold_ms = measure_hold_ms();
	s_power_hold_ms = power_hold_ms();
	nav_button_set_hold_ms(s_measure_hold_ms);
	power_button_set_hold_ms(s_power_hold_ms);
}

static void start_measure_cycle(bool skip_proximity)
{
#if defined(CONFIG_APP_HEALTH_SCHED)
	if (health_sched_is_busy()) {
		return;
	}

	int ret = health_sched_start_cycle(HEALTH_TRIGGER_MANUAL, true, skip_proximity);

	if (ret == 0) {
		LOG_INF("Manual cycle started from nav button%s",
			skip_proximity ? " (prox bypass)" : "");
		buzzer_play_tone(BUZZER_TONE_BEEP, 30, 40);
	} else {
		LOG_WRN("Nav hold measure failed: %d", ret);
	}
#else
	ARG_UNUSED(skip_proximity);
#endif
}

static void nav_cancel_hold_complete(void *user_data)
{
	ARG_UNUSED(user_data);

#if defined(CONFIG_APP_HEALTH_SCHED)
	int ret = health_sched_cancel_cycle();

	if (ret == 0) {
		LOG_INF("Measurement cancelled from nav hold");
		buzzer_play_tone(BUZZER_TONE_BEEP, 30, 40);
	} else {
		LOG_WRN("Nav cancel failed: %d", ret);
	}
#endif
}

static void nav_measure_hold_complete(void *user_data)
{
	ARG_UNUSED(user_data);

#if defined(CONFIG_APP_HEALTH_SCHED)
	if (health_sched_is_busy()) {
		return;
	}
#endif
	start_measure_cycle(true);
}

static void power_hold_complete(void *user_data)
{
	ARG_UNUSED(user_data);

	LOG_INF("Power off — PMIC shutdown (release button)");
	buzzer_play_tone(BUZZER_TONE_BEEP, 30, 80);
}

static void on_nav_button(enum hw_button_event ev)
{
	switch (ev) {
	case HW_BUTTON_PRESSED:
		LOG_INF("Nav SW1 pressed (P0.29)");
		power_mgr_activity_notify();
		break;
	case HW_BUTTON_HOLD_ARMED:
#if defined(CONFIG_APP_HEALTH_SCHED)
		if (health_sched_is_busy()) {
			ui_hold_overlay_begin("Cancel", "Hold to stop scan",
					      hold_overlay_duration_ms(s_measure_hold_ms),
					      nav_cancel_hold_complete, NULL);
			break;
		}
#endif
		ui_hold_overlay_begin("Measure", "Hold to start scan",
				      hold_overlay_duration_ms(s_measure_hold_ms),
				      nav_measure_hold_complete, NULL);
		break;
	case HW_BUTTON_SHORT:
		ui_hold_overlay_cancel();
		if (ui_is_navigation_blocked()) {
			LOG_WRN("Nav short ignored — measurement active");
			break;
		}
		LOG_INF("Nav SW1 short");
		ui_request_next_screen();
		buzzer_play_tone(BUZZER_TONE_BEEP, 30, 30);
		break;
	case HW_BUTTON_HOLD_COMPLETE:
		/* Single trigger: the overlay countdown callback
		 * (nav_measure_hold_complete / nav_cancel_hold_complete)
		 * runs on the LVGL thread. */
		break;
	case HW_BUTTON_HOLD_CANCEL:
		ui_hold_overlay_cancel();
		break;
	case HW_BUTTON_RELEASE_AFTER_HOLD:
		ui_hold_overlay_cancel();
		break;
	default:
		break;
	}
}

static void on_power_button(enum hw_button_event ev)
{
	switch (ev) {
	case HW_BUTTON_PRESSED:
		LOG_INF("Power SW2/PFN1 pressed (P1.04)");
		break;
	case HW_BUTTON_HOLD_ARMED:
		ui_hold_overlay_begin("Power off", "Release to cancel",
				      hold_overlay_duration_ms(s_power_hold_ms),
				      power_hold_complete, NULL);
		break;
	case HW_BUTTON_SHORT:
		ui_hold_overlay_cancel();
		if (power_mgr_display_asleep()) {
			power_mgr_activity_notify();
			LOG_INF("Power short: wake display");
		} else {
			ui_request_screen(UI_SCREEN_DEVICE_INFO);
			power_mgr_activity_notify();
			LOG_INF("Power short: home screen");
		}
		buzzer_play_tone(BUZZER_TONE_BEEP, 30, 30);
		break;
	case HW_BUTTON_HOLD_COMPLETE:
		break;
	case HW_BUTTON_HOLD_CANCEL:
		ui_hold_overlay_cancel();
		break;
	case HW_BUTTON_RELEASE_AFTER_HOLD:
		ui_hold_overlay_cancel();
		break;
	default:
		break;
	}
}

int ui_buttons_gpio_early_init(void)
{
	apply_hold_config();

	nav_button_set_handler(on_nav_button);
	power_button_set_handler(on_power_button);

	int nav_ret = nav_button_init();
	bool nav_ok = (nav_ret == 0);

	if (nav_ret != 0 && nav_ret != -ENOTSUP) {
		LOG_WRN("Nav button (SW1 P0.29) init failed: %d", nav_ret);
	}

	int power_ret = power_button_init();
	bool power_ok = (power_ret == 0);

	if (power_ret != 0 && power_ret != -ENOTSUP) {
		LOG_WRN("Power button (SW2/PFN1 P1.04) init failed: %d", power_ret);
	}

	LOG_INF("HW buttons GPIO: SW1=%s SW2/PFN1=%s (hold measure %u ms, power %u ms)",
		nav_ok ? "ok" : (nav_ret == -ENOTSUP ? "n/a" : "fail"),
		power_ok ? "ok" : (power_ret == -ENOTSUP ? "n/a" : "fail"),
		s_measure_hold_ms, s_power_hold_ms);
	return (nav_ok || nav_ret == -ENOTSUP) && (power_ok || power_ret == -ENOTSUP) ? 0 : -EIO;
}

int ui_buttons_init(void)
{
	apply_hold_config();

	nav_button_set_handler(on_nav_button);
	power_button_set_handler(on_power_button);

	LOG_INF("HW button handlers registered");
	return 0;
}

void ui_buttons_apply_config(void)
{
	apply_hold_config();
}
