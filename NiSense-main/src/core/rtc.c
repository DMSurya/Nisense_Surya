/**
 * @file rtc.c
 * @brief RTC time management via MCP79xx Zephyr RTC API
 */

#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#if defined(CONFIG_POSIX_CLOCK)
#include <zephyr/posix/time.h>
#endif

#if defined(CONFIG_DATE_TIME)
#include <date_time.h>
#endif

LOG_MODULE_REGISTER(rtc, LOG_LEVEL_INF);

/* Hardware OSCTRIM magnitude max (DS20005010J); 1 LSB treated as 1 ppm */
#define RTC_TRIM_PPM_MAX 127

#define RTC_NODE DT_ALIAS(rtc0)

static const struct device *rtc_dev = DEVICE_DT_GET_OR_NULL(RTC_NODE);

static struct tm build_tm;

#if defined(CONFIG_APP_RTC_PERIODIC_ALARM)
static volatile uint32_t alarm_count;
static volatile bool alarm_active;
#endif

static volatile bool rtc_ready;
static volatile uint32_t rtc_timestamp_cache;
static volatile uint32_t rtc_cache_time;
#define RTC_CACHE_INVALID 0xFFFFFFFF

#if defined(CONFIG_APP_RTC_PERIODIC_ALARM)
static void alarm_periodic_callback(const struct device *dev, uint16_t id,
				    void *user_data);
static int rtc_set_alarm_1min(void);
#endif

static bool rtc_calendar_valid(const struct tm *tm)
{
	static const uint8_t days_in_month[] = {
		31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};
	int year;
	int max_day;
	bool leap;

	if (tm == NULL) {
		return false;
	}
	if (tm->tm_sec < 0 || tm->tm_sec > 59 ||
	    tm->tm_min < 0 || tm->tm_min > 59 ||
	    tm->tm_hour < 0 || tm->tm_hour > 23 ||
	    tm->tm_mon < 0 || tm->tm_mon > 11 ||
	    tm->tm_mday < 1 ||
	    tm->tm_year < 100 || tm->tm_year > 199) {
		return false;
	}

	year = tm->tm_year + 1900;
	leap = ((year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0));
	max_day = days_in_month[tm->tm_mon];
	if (tm->tm_mon == 1 && leap) {
		max_day = 29;
	}

	return tm->tm_mday <= max_day;
}

/**
 * Push MCP7940 calendar into Zephyr CLOCK_REALTIME (and Nordic date_time).
 */
static void rtc_sync_system_clock(const struct tm *tm)
{
	struct tm t;
	time_t secs;
	int ret;

	if (tm == NULL) {
		return;
	}

	t = *tm;
	secs = mktime(&t);
	if (secs == (time_t)-1) {
		LOG_WRN("System clock sync skipped: mktime failed");
		return;
	}

#if defined(CONFIG_DATE_TIME)
	ret = date_time_set(&t);
	if (ret) {
		LOG_DBG("date_time_set: %d (non-fatal)", ret);
	}
#endif

#if defined(CONFIG_POSIX_CLOCK)
	{
		struct timespec ts = {
			.tv_sec = secs,
			.tv_nsec = 0,
		};

		ret = clock_settime(CLOCK_REALTIME, &ts);
		if (ret) {
			LOG_WRN("CLOCK_REALTIME sync failed: %d", ret);
		} else {
			LOG_INF("CLOCK_REALTIME synced from RTC (Unix %lld)",
				(long long)secs);
		}
	}
#else
	ARG_UNUSED(ret);
	LOG_DBG("POSIX clock not enabled; shell date stays on boot epoch");
#endif
}

static int rtc_read_unix_secs(uint32_t *unix_secs)
{
	struct rtc_time rt;
	struct tm t;
	time_t secs;
	int ret;

	if (!rtc_dev || !unix_secs) {
		return -ENODEV;
	}

	ret = rtc_get_time(rtc_dev, &rt);
	if (ret) {
		return ret;
	}

	t = (struct tm){
		.tm_sec = rt.tm_sec,
		.tm_min = rt.tm_min,
		.tm_hour = rt.tm_hour,
		.tm_mday = rt.tm_mday,
		.tm_mon = rt.tm_mon,
		.tm_year = rt.tm_year,
		.tm_isdst = -1,
	};

	secs = mktime(&t);
	if (secs == (time_t)-1) {
		return -EINVAL;
	}

	*unix_secs = (uint32_t)secs;
	return 0;
}

static void parse_build_time(void)
{
	static const char *months[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	char month[4];
	int day, year, hour, min, sec;
	int mon = 0;

	sscanf(__DATE__, "%3s %d %d", month, &day, &year);
	sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);

	for (int i = 0; i < 12; i++) {
		if (strncmp(month, months[i], 3) == 0) {
			mon = i;
			break;
		}
	}

	build_tm = (struct tm){
		.tm_year = year - 1900,
		.tm_mon = mon,
		.tm_mday = day,
		.tm_hour = hour,
		.tm_min = min,
		.tm_sec = sec,
		.tm_isdst = -1,
	};
}

int rtc_write_datetime(const struct tm *tm)
{
	struct rtc_time rt;
	int ret;

	if (!rtc_dev || !tm) {
		return -EINVAL;
	}

	if (!rtc_calendar_valid(tm)) {
		LOG_WRN("Rejecting invalid calendar date");
		return -EINVAL;
	}

	rt = (struct rtc_time){
		.tm_sec = tm->tm_sec,
		.tm_min = tm->tm_min,
		.tm_hour = tm->tm_hour,
		.tm_mday = tm->tm_mday,
		.tm_mon = tm->tm_mon,
		.tm_year = tm->tm_year,
		.tm_wday = tm->tm_wday,
		.tm_yday = tm->tm_yday,
		.tm_isdst = tm->tm_isdst,
		.tm_nsec = 0,
	};

	LOG_INF("Setting RTC: %04d-%02d-%02d %02d:%02d:%02d wday=%d",
		tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_wday);

	ret = rtc_set_time(rtc_dev, &rt);
	if (ret) {
		LOG_ERR("rtc_set_time failed: %d", ret);
	} else {
		LOG_INF("RTC hardware updated OK");
		rtc_sync_system_clock(tm);
	}
	return ret;
}

int rtc_read_time(struct tm *tm)
{
	struct rtc_time rt;
	int ret;

	if (!rtc_dev || !tm) {
		return -ENODEV;
	}

	ret = rtc_get_time(rtc_dev, &rt);
	if (ret) {
		LOG_ERR("rtc_get_time failed: %d", ret);
		return ret;
	}

	tm->tm_sec = rt.tm_sec;
	tm->tm_min = rt.tm_min;
	tm->tm_hour = rt.tm_hour;
	tm->tm_mday = rt.tm_mday;
	tm->tm_mon = rt.tm_mon;
	tm->tm_year = rt.tm_year;
	tm->tm_wday = rt.tm_wday;
	tm->tm_yday = rt.tm_yday;
	tm->tm_isdst = rt.tm_isdst;

	return 0;
}

int rtc_get_unix_time(uint32_t *unix_secs)
{
	return rtc_read_unix_secs(unix_secs);
}

int rtc_set_from_ntp(uint32_t unix_secs)
{
	time_t t = (time_t)unix_secs;
	struct tm *tm_ptr = gmtime(&t);

	if (!tm_ptr) {
		return -EINVAL;
	}

	LOG_INF("Setting RTC from Unix time: %u", unix_secs);
	return rtc_write_datetime(tm_ptr);
}

#if defined(CONFIG_APP_RTC_PERIODIC_ALARM)
/*
 * Experimental bring-up only. IRQ→rtc_alarm callback dispatch and errata #2
 * verify-on-IRQ are not complete — do not enable for product builds.
 */
static void alarm_periodic_callback(const struct device *dev, uint16_t id,
				    void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(id);
	ARG_UNUSED(user_data);

	alarm_count++;
	LOG_INF("RTC alarm fired (count: %u)", alarm_count);

	if (rtc_set_alarm_1min() != 0) {
		LOG_ERR("Failed to re-arm alarm");
		alarm_active = false;
	}
}

static int rtc_set_alarm_1min(void)
{
	struct rtc_time now;
	struct rtc_time alarm;
	uint16_t mask;
	int ret;

	if (!rtc_dev || !rtc_ready) {
		return -ENODEV;
	}

	ret = rtc_get_time(rtc_dev, &now);
	if (ret) {
		LOG_ERR("Failed to read time for alarm: %d", ret);
		return ret;
	}

	alarm = now;
	alarm.tm_min++;
	if (alarm.tm_min >= 60) {
		alarm.tm_min = 0;
		alarm.tm_hour = (alarm.tm_hour + 1) % 24;
	}

	mask = RTC_ALARM_TIME_MASK_MINUTE | RTC_ALARM_TIME_MASK_SECOND;
	ret = rtc_alarm_set_time(rtc_dev, 0, mask, &alarm);
	if (ret) {
		LOG_ERR("rtc_alarm_set_time failed: %d", ret);
		return ret;
	}

	ret = rtc_alarm_set_callback(rtc_dev, 0, alarm_periodic_callback, NULL);
	if (ret) {
		LOG_ERR("rtc_alarm_set_callback failed: %d", ret);
		return ret;
	}

	alarm_active = true;
	return 0;
}
#endif /* CONFIG_APP_RTC_PERIODIC_ALARM */

bool rtc_is_alarm_active(void)
{
#if defined(CONFIG_APP_RTC_PERIODIC_ALARM)
	return alarm_active;
#else
	return false;
#endif
}

uint32_t rtc_get_alarm_count(void)
{
#if defined(CONFIG_APP_RTC_PERIODIC_ALARM)
	return alarm_count;
#else
	return 0;
#endif
}

int rtc_init(void)
{
	uint32_t rtc_unix = 0;
	time_t build_secs;
	time_t rtc_secs;
	time_t diff;
	struct tm rtc_tm;
	struct tm sys_tm;
	int ret;

	rtc_ready = false;

	if (!rtc_dev) {
		LOG_ERR("RTC device not found");
		return -ENODEV;
	}
	if (!device_is_ready(rtc_dev)) {
		LOG_ERR("RTC device not ready");
		return -ENODEV;
	}

	parse_build_time();
	build_secs = mktime(&build_tm);
	LOG_INF("Build time: %04d-%02d-%02d %02d:%02d:%02d (Unix: %lld)",
		build_tm.tm_year + 1900, build_tm.tm_mon + 1, build_tm.tm_mday,
		build_tm.tm_hour, build_tm.tm_min, build_tm.tm_sec,
		(long long)build_secs);

	ret = rtc_read_unix_secs(&rtc_unix);
	if (ret) {
		LOG_ERR("Failed to read RTC: %d", ret);
		return ret;
	}

	rtc_secs = (time_t)rtc_unix;
	diff = build_secs - rtc_secs;
	gmtime_r(&rtc_secs, &rtc_tm);
	LOG_INF("RTC current: %04d-%02d-%02d %02d:%02d:%02d (Unix: %lld, diff: %llds)",
		rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday,
		rtc_tm.tm_hour, rtc_tm.tm_min, rtc_tm.tm_sec,
		(long long)rtc_secs, (long long)diff);

	if (rtc_secs == 0 || rtc_tm.tm_year + 1900 < 2020) {
		LOG_WRN("RTC %s - syncing to build time",
			rtc_secs == 0 ? "uninitialized" : "invalid year");

		ret = rtc_write_datetime(&build_tm);
		if (ret) {
			LOG_ERR("Failed to sync RTC: %d", ret);
		} else {
			LOG_INF("RTC synced to build time");
			rtc_secs = build_secs;
		}
	} else {
		LOG_INF("RTC time valid (preserving stored clock)");
	}

	if (rtc_read_time(&sys_tm) == 0) {
		rtc_sync_system_clock(&sys_tm);
	}

	rtc_ready = true;
	rtc_timestamp_cache = (uint32_t)rtc_secs;
	rtc_cache_time = k_uptime_get_32();

#if defined(CONFIG_APP_RTC_PERIODIC_ALARM)
	ret = rtc_set_alarm_1min();
	if (ret) {
		LOG_WRN("Alarm setup failed: %d (continuing)", ret);
	} else {
		LOG_INF("RTC alarm enabled (1-minute periodic, experimental)");
	}
#else
	LOG_INF("RTC periodic alarm disabled (CONFIG_APP_RTC_PERIODIC_ALARM=n)");
#endif

	LOG_INF("RTC initialized");
	return 0;
}

int rtc_update_cache(void)
{
	uint32_t secs = 0;
	int ret;

	if (!rtc_ready || !rtc_dev) {
		return -ENODEV;
	}

	ret = rtc_read_unix_secs(&secs);
	if (ret) {
		rtc_timestamp_cache = RTC_CACHE_INVALID;
		return ret;
	}

	rtc_timestamp_cache = secs;
	rtc_cache_time = k_uptime_get_32();
	return 0;
}

int rtc_set_trim_ppm(int32_t ppm)
{
	int32_t ppb;

	if (!rtc_dev) {
		return -ENODEV;
	}

	if (ppm < -RTC_TRIM_PPM_MAX || ppm > RTC_TRIM_PPM_MAX) {
		LOG_WRN("RTC trim %d ppm out of HW range ±%d", ppm,
			RTC_TRIM_PPM_MAX);
		return -EINVAL;
	}

#if defined(CONFIG_RTC_CALIBRATION)
	/*
	 * Zephyr rtc_set_calibration: positive ppb increases RTC frequency.
	 * ppb = ppm * 1000. OSCTRIM uses 1 LSB ≈ 1 ppm (accepted vs 1.017).
	 */
	ppb = ppm * 1000;
	return rtc_set_calibration(rtc_dev, ppb);
#else
	ARG_UNUSED(ppb);
	return -ENOTSUP;
#endif
}
