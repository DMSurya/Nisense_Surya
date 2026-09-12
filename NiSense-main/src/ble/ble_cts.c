/*
 * Bluetooth SIG Current Time Service (0x1805) — map CTS calendar to MCP7940.
 */

#include "ble_cts.h"

#include "rtc.h"

#if defined(CONFIG_UI_SCREEN_DEVICE_INFO)
#include "device_info_ui.h"
#endif

#include <errno.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/services/cts.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(ble_cts, LOG_LEVEL_INF);

/* CTS day-of-week: 1=Monday … 7=Sunday. struct tm: 0=Sunday … 6=Saturday. */
static uint8_t tm_wday_to_cts(int tm_wday)
{
	if (tm_wday < 0 || tm_wday > 6) {
		return 0; /* unknown */
	}
	return (tm_wday == 0) ? 7U : (uint8_t)tm_wday;
}

static int cts_wday_to_tm(uint8_t cts_wday)
{
	if (cts_wday == 0U) {
		return -1; /* unknown — leave unset */
	}
	if (cts_wday == 7U) {
		return 0; /* Sunday */
	}
	if (cts_wday >= 1U && cts_wday <= 6U) {
		return (int)cts_wday;
	}
	return -1;
}

static int fill_current_cts_time(struct bt_cts_time_format *cts_time)
{
	struct tm tm;

	if (cts_time == NULL) {
		return -EINVAL;
	}

	if (rtc_read_time(&tm) != 0) {
		LOG_WRN("CTS read: rtc_read_time failed");
		return -EIO;
	}

	cts_time->year = sys_cpu_to_le16((uint16_t)(tm.tm_year + 1900));
	cts_time->mon = (uint8_t)(tm.tm_mon + 1);
	cts_time->mday = (uint8_t)tm.tm_mday;
	cts_time->hours = (uint8_t)tm.tm_hour;
	cts_time->min = (uint8_t)tm.tm_min;
	cts_time->sec = (uint8_t)tm.tm_sec;
	cts_time->wday = tm_wday_to_cts(tm.tm_wday);
	cts_time->fractions256 = 0U;
	cts_time->reason = BT_CTS_UPDATE_REASON_UNKNOWN;

	LOG_INF("CTS read: %04d-%02d-%02d %02d:%02d:%02d wday=%u",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, cts_time->wday);

	return 0;
}

static int cts_time_write(struct bt_cts_time_format *cts_time)
{
	struct tm tm = { 0 };
	struct tm verify = { 0 };
	uint16_t year;
	int wday;
	int ret;

	if (cts_time == NULL) {
		return -EINVAL;
	}

	year = sys_le16_to_cpu(cts_time->year);
	LOG_INF("CTS write raw: Y=%u M=%u D=%u %02u:%02u:%02u wday=%u frac=%u reason=0x%02x",
		year, cts_time->mon, cts_time->mday,
		cts_time->hours, cts_time->min, cts_time->sec,
		cts_time->wday, cts_time->fractions256, cts_time->reason);

	if (year < 2000U || year > 2099U ||
	    cts_time->mon < 1U || cts_time->mon > 12U ||
	    cts_time->mday < 1U || cts_time->mday > 31U ||
	    cts_time->hours > 23U || cts_time->min > 59U || cts_time->sec > 59U) {
		LOG_WRN("CTS write rejected: invalid calendar fields");
		return -EINVAL;
	}

	wday = cts_wday_to_tm(cts_time->wday);
	if (wday < 0) {
		LOG_WRN("CTS write rejected: invalid day of week %u", cts_time->wday);
		return -EINVAL;
	}

	tm.tm_year = (int)year - 1900;
	tm.tm_mon = (int)cts_time->mon - 1;
	tm.tm_mday = (int)cts_time->mday;
	tm.tm_hour = (int)cts_time->hours;
	tm.tm_min = (int)cts_time->min;
	tm.tm_sec = (int)cts_time->sec;
	tm.tm_wday = wday;
	tm.tm_yday = -1;
	tm.tm_isdst = -1;

	/* Full calendar validity (rejects Feb 30, Apr 31, non-leap Feb 29, …)
	 * is enforced inside rtc_write_datetime() before any I2C write.
	 */
	LOG_INF("CTS sync apply: %04d-%02d-%02d %02d:%02d:%02d wday=%d",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);

	ret = rtc_write_datetime(&tm);
	if (ret != 0) {
		LOG_ERR("CTS sync: rtc_write_datetime failed (%d)", ret);
		return -EIO;
	}

	(void)rtc_update_cache();

	if (rtc_read_time(&verify) == 0) {
		LOG_INF("CTS sync verified: %04d-%02d-%02d %02d:%02d:%02d wday=%d",
			verify.tm_year + 1900, verify.tm_mon + 1, verify.tm_mday,
			verify.tm_hour, verify.tm_min, verify.tm_sec, verify.tm_wday);
	} else {
		LOG_WRN("CTS sync: write ok but verify read failed");
	}

#if defined(CONFIG_UI_SCREEN_DEVICE_INFO)
	device_info_ui_invalidate_clock();
	LOG_INF("CTS sync: home clock invalidated");
#endif
	return 0;
}

static const struct bt_cts_cb s_cts_cb = {
	.cts_time_write = cts_time_write,
	.fill_current_cts_time = fill_current_cts_time,
};

int ble_cts_init(void)
{
	int ret = bt_cts_init(&s_cts_cb);

	if (ret != 0) {
		LOG_ERR("bt_cts_init failed: %d", ret);
		return ret;
	}

	LOG_INF("Current Time Service ready (0x1805 / 0x2A2B)");
	return 0;
}
