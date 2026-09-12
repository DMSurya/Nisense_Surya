#ifndef RTC_H
#define RTC_H

#include <time.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @file rtc.h
 * @brief RTC time management (MCP79xx / Zephyr RTC API)
 */

/** Initialize RTC; syncs to build time if needed. Does not arm HW alarms unless
 *  CONFIG_APP_RTC_PERIODIC_ALARM=y (experimental; not for product).
 */
int rtc_init(void);

/** Update cached Unix timestamp from hardware. Call periodically. */
int rtc_update_cache(void);

/** Get calendar time. */
int rtc_read_time(struct tm *tm);

/** Set calendar time (rejects invalid calendar dates, e.g. Feb 30). */
int rtc_write_datetime(const struct tm *tm);

/** Set RTC from Unix timestamp (seconds since epoch). */
int rtc_set_from_ntp(uint32_t unix_secs);

/** Get Unix timestamp (seconds since epoch). */
int rtc_get_unix_time(uint32_t *unix_secs);

/**
 * @brief Apply oscillator trim (Zephyr RTC calibration polarity).
 * @param ppm Signed ppm, range -127..+127 (MCP7940N OSCTRIM).
 *            Positive = increase RTC frequency (add clocks).
 *            Negative = decrease RTC frequency (subtract clocks).
 * @return 0 on success, negative errno on failure.
 */
int rtc_set_trim_ppm(int32_t ppm);

/** True if experimental periodic alarm is armed (always false if Kconfig off). */
bool rtc_is_alarm_active(void);

/** Alarm fire count since init (0 if periodic alarm disabled). */
uint32_t rtc_get_alarm_count(void);

/* --- RTC UI screen (rtc_ui.c) --- */
#include <lvgl.h>

void rtc_ui_create_screen(lv_obj_t *screen);
void rtc_ui_update_screen(void);
void rtc_ui_cleanup_screen(void);

#endif /* RTC_H */
