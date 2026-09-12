/*
 * Copyright (c) 2019-2020 Peter Bigot Consulting, LLC
 * Copyright (c) 2021 Laird Connectivity
 * Copyright (c) 2026 AARMS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file mcp79xx.c
 * @brief Out-of-tree RTC driver for Microchip MCP79xx family (MCP7940N primary)
 *
 * Registers the Zephyr RTC API only (not Counter). Year encoding uses
 * datasheet offset (years since 2000 → tm_year since 1900 = +100).
 *
 * Calibration follows Zephyr rtc_set_calibration: positive ppb increases
 * RTC frequency (MCP7940N OSCTRIM SIGN=1, add clocks); negative decreases
 * (SIGN=0, subtract). Datasheet DS20005010J Register 5-15.
 *
 * Alarm IRQ currently clears ALMxIF only. Dispatch to rtc_alarm*_callback
 * plus errata #2 verify-on-IRQ is deferred until a product feature needs
 * hardware alarms (see drivers/counter/README.md Track B).
 */

#ifdef CONFIG_SOC_POSIX
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#define DT_DRV_COMPAT microchip_mcp79xx

#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/rtc/mcp7940n.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/timeutil.h>
#include <zephyr/sys/util.h>
#include <string.h>
#include <time.h>

LOG_MODULE_REGISTER(MCP79XX, CONFIG_COUNTER_LOG_LEVEL);

#define ALARM0_ID                       0
#define ALARM1_ID                       1

#define RTC_TIME_REGISTERS_SIZE         sizeof(struct mcp7940n_time_registers)

/* tm_year offset: MCP79xx stores years since 2000; POSIX tm_year since 1900 */
#define MCP79XX_YEAR_OFFSET             100

/*
 * OSCTRIM TRIMVAL steps ≈ 1.017 ppm (DS20005010J §5.6). This driver maps
 * 1 LSB = 1 ppm for simplicity; residual ~1.7% under-trim is accepted.
 * Register magnitude max is 127 (~±129 ppm physical).
 */
#define MCP79XX_TRIM_PPM_MAX            127

#define RTC_BCD_DECODE(reg_prefix) (reg_prefix##_one + reg_prefix##_ten * 10)

struct mcp79xx_config {
	struct i2c_dt_spec i2c;
	const struct gpio_dt_spec int_gpios;
};

struct mcp79xx_data {
	const struct device *mcp79xx;
	struct k_sem lock;
	struct mcp7940n_time_registers registers;
	struct mcp7940n_alarm_registers alm0_registers;
	struct mcp7940n_alarm_registers alm1_registers;

	struct k_work alarm_work;
	struct gpio_callback int_callback;

#ifdef CONFIG_RTC_ALARM
	rtc_alarm_callback rtc_alarm0_callback;
	void *rtc_alarm0_user_data;
	rtc_alarm_callback rtc_alarm1_callback;
	void *rtc_alarm1_user_data;
#endif

	bool int_active_high;
};

static time_t decode_rtc(const struct device *dev)
{
	struct mcp79xx_data *data = dev->data;
	struct tm time = { 0 };

	time.tm_sec = RTC_BCD_DECODE(data->registers.rtc_sec.sec);
	time.tm_min = RTC_BCD_DECODE(data->registers.rtc_min.min);
	time.tm_hour = RTC_BCD_DECODE(data->registers.rtc_hours.hr);
	time.tm_mday = RTC_BCD_DECODE(data->registers.rtc_date.date);
	time.tm_wday = data->registers.rtc_weekday.weekday;
	time.tm_mon = RTC_BCD_DECODE(data->registers.rtc_month.month) - 1;
	time.tm_year = RTC_BCD_DECODE(data->registers.rtc_year.year) +
			MCP79XX_YEAR_OFFSET;

	return timeutil_timegm(&time);
}

static int read_register(const struct device *dev, uint8_t addr, uint8_t *val)
{
	const struct mcp79xx_config *cfg = dev->config;

	return i2c_write_read_dt(&cfg->i2c, &addr, sizeof(addr), val, 1);
}

static int read_time(const struct device *dev, time_t *unix_time)
{
	struct mcp79xx_data *data = dev->data;
	const struct mcp79xx_config *cfg = dev->config;
	uint8_t addr = REG_RTC_SEC;
	int rc;

	rc = i2c_write_read_dt(&cfg->i2c, &addr, sizeof(addr), &data->registers,
			       RTC_TIME_REGISTERS_SIZE);
	if (rc >= 0) {
		*unix_time = decode_rtc(dev);
	}

	return rc;
}

static int write_register(const struct device *dev, enum mcp7940n_register addr,
			  uint8_t value)
{
	const struct mcp79xx_config *cfg = dev->config;
	uint8_t time_data[2] = { addr, value };

	return i2c_write_dt(&cfg->i2c, time_data, sizeof(time_data));
}

static int set_day_of_week(const struct device *dev, time_t *unix_time)
{
	struct mcp79xx_data *data = dev->data;
	struct tm time_buffer = { 0 };
	int rc;

	if (gmtime_r(unix_time, &time_buffer) == NULL) {
		return -EINVAL;
	}

	data->registers.rtc_weekday.weekday = time_buffer.tm_wday;
	rc = write_register(dev, REG_RTC_WDAY,
			    *((uint8_t *)(&data->registers.rtc_weekday)));
	return rc;
}

/**
 * Clear ALMxIF with I2C error checking. Callback dispatch is deferred
 * (Track B: wire rtc_alarm*_callback + errata #2 verify-on-IRQ together).
 */
static void mcp79xx_handle_interrupt(const struct device *dev, uint8_t alarm_id)
{
	struct mcp79xx_data *data = dev->data;
	uint8_t alarm_reg_address;
	struct mcp7940n_alarm_registers *alm_regs;
	int rc;

	if (alarm_id == ALARM0_ID) {
		alarm_reg_address = REG_ALM0_WDAY;
		alm_regs = &data->alm0_registers;
	} else if (alarm_id == ALARM1_ID) {
		alarm_reg_address = REG_ALM1_WDAY;
		alm_regs = &data->alm1_registers;
	} else {
		return;
	}

	k_sem_take(&data->lock, K_FOREVER);

	rc = read_register(dev, alarm_reg_address,
			   (uint8_t *)&alm_regs->alm_weekday);
	if (rc < 0) {
		LOG_ERR("ALM%u IF read failed: %d", alarm_id, rc);
		goto out;
	}

	if (!alm_regs->alm_weekday.alm_if) {
		goto out;
	}

	alm_regs->alm_weekday.alm_if = 0;
	rc = write_register(dev, alarm_reg_address,
			    *((uint8_t *)(&alm_regs->alm_weekday)));
	if (rc < 0) {
		LOG_ERR("ALM%u IF clear failed: %d (flag may still be set)",
			alarm_id, rc);
	}

out:
	k_sem_give(&data->lock);
}

static void mcp79xx_work_handler(struct k_work *work)
{
	struct mcp79xx_data *data =
		CONTAINER_OF(work, struct mcp79xx_data, alarm_work);

	mcp79xx_handle_interrupt(data->mcp79xx, ALARM0_ID);
	mcp79xx_handle_interrupt(data->mcp79xx, ALARM1_ID);
}

static void mcp79xx_gpio_cb(const struct device *dev,
			    struct gpio_callback *gpio_cb, uint32_t pins)
{
	struct mcp79xx_data *data =
		CONTAINER_OF(gpio_cb, struct mcp79xx_data, int_callback);

	ARG_UNUSED(dev);
	ARG_UNUSED(pins);

	k_work_submit(&data->alarm_work);
}

static int rtc_time_to_bcd_regs(const struct rtc_time *rtc_tm, uint8_t regs[7])
{
	int year_since_2000;

	if (rtc_tm->tm_sec > 59 || rtc_tm->tm_min > 59 || rtc_tm->tm_hour > 23 ||
	    rtc_tm->tm_mday < 1 || rtc_tm->tm_mday > 31 ||
	    rtc_tm->tm_mon < 0 || rtc_tm->tm_mon > 11 ||
	    rtc_tm->tm_wday < 0 || rtc_tm->tm_wday > 6 ||
	    rtc_tm->tm_year < 100 || rtc_tm->tm_year > 199) {
		return -EINVAL;
	}

	regs[0] = bin2bcd(rtc_tm->tm_sec) & 0x7F;
	regs[1] = bin2bcd(rtc_tm->tm_min) & 0x7F;
	regs[2] = bin2bcd(rtc_tm->tm_hour) & 0x3F; /* 24-hour: bit6 clear */
	regs[3] = (bin2bcd(rtc_tm->tm_wday + 1) & 0x07) | 0x08; /* VBATEN=1 */
	regs[4] = bin2bcd(rtc_tm->tm_mday) & 0x3F;
	regs[5] = bin2bcd(rtc_tm->tm_mon + 1) & 0x1F;

	year_since_2000 = rtc_tm->tm_year - MCP79XX_YEAR_OFFSET;
	if (year_since_2000 < 0 || year_since_2000 > 99) {
		return -EINVAL;
	}
	regs[6] = bin2bcd(year_since_2000);

	return 0;
}

static int bcd_regs_to_rtc_time(const uint8_t regs[7], struct rtc_time *rtc_tm)
{
	rtc_tm->tm_sec = bcd2bin(regs[0] & 0x7F);
	rtc_tm->tm_min = bcd2bin(regs[1] & 0x7F);
	rtc_tm->tm_hour = bcd2bin(regs[2] & 0x3F);
	rtc_tm->tm_wday = bcd2bin(regs[3] & 0x07) - 1;
	rtc_tm->tm_mday = bcd2bin(regs[4] & 0x3F);
	rtc_tm->tm_mon = bcd2bin(regs[5] & 0x1F) - 1;
	rtc_tm->tm_year = bcd2bin(regs[6]) + MCP79XX_YEAR_OFFSET;
	rtc_tm->tm_yday = -1;
	rtc_tm->tm_isdst = -1;
	rtc_tm->tm_nsec = 0;

	return 0;
}

static int mcp79xx_rtc_api_set_time(const struct device *dev,
				    const struct rtc_time *rtc_tm)
{
	struct mcp79xx_data *data = dev->data;
	const struct mcp79xx_config *cfg = dev->config;
	uint8_t regs[7];
	uint8_t time_data[8];
	uint8_t wday_rd;
	int rc;

	if (!rtc_tm) {
		return -EINVAL;
	}

	rc = rtc_time_to_bcd_regs(rtc_tm, regs);
	if (rc < 0) {
		LOG_ERR("Invalid RTC time");
		return rc;
	}

	k_sem_take(&data->lock, K_FOREVER);

	/* Stop oscillator before loading (DS §5.3). */
	rc = write_register(dev, REG_RTC_SEC, regs[0] & ~BIT(7));
	if (rc < 0) {
		goto out;
	}

	time_data[0] = REG_RTC_SEC;
	memcpy(&time_data[1], regs, 7);
	rc = i2c_write_dt(&cfg->i2c, time_data, 8);
	if (rc < 0) {
		goto out;
	}

	/*
	 * Errata DS80000611 #3: after Date/Month/Year write, write Date again
	 * (order Date→Month→Year already satisfied by the burst above).
	 */
	rc = write_register(dev, REG_RTC_DATE, regs[4]);
	if (rc < 0) {
		goto out;
	}

	/* Restart oscillator */
	regs[0] |= BIT(7);
	rc = write_register(dev, REG_RTC_SEC, regs[0]);
	if (rc < 0) {
		goto out;
	}

	/*
	 * Errata DS80000611 #4: weekday may change if written while osc
	 * stopped — read back after ST restart and rewrite if needed.
	 * Note: any RTCWKDAY write clears PWRFAIL (intentional; unused).
	 */
	rc = read_register(dev, REG_RTC_WDAY, &wday_rd);
	if (rc < 0) {
		goto out;
	}

	if ((wday_rd & 0x0F) != (regs[3] & 0x0F)) {
		LOG_DBG("RTCWKDAY mismatch after ST restart (got 0x%02x want 0x%02x)",
			wday_rd, regs[3]);
		rc = write_register(dev, REG_RTC_WDAY, regs[3]);
		if (rc < 0) {
			goto out;
		}
	}

	LOG_DBG("RTC time set: %04d-%02d-%02d %02d:%02d:%02d",
		rtc_tm->tm_year + 1900, rtc_tm->tm_mon + 1, rtc_tm->tm_mday,
		rtc_tm->tm_hour, rtc_tm->tm_min, rtc_tm->tm_sec);

out:
	k_sem_give(&data->lock);
	return rc;
}

static int mcp79xx_rtc_api_get_time(const struct device *dev,
				    struct rtc_time *rtc_tm)
{
	struct mcp79xx_data *data = dev->data;
	const struct mcp79xx_config *cfg = dev->config;
	uint8_t regs[7];
	uint8_t addr = REG_RTC_SEC;
	int rc;

	if (!rtc_tm) {
		return -EINVAL;
	}

	k_sem_take(&data->lock, K_FOREVER);

	rc = i2c_write_read_dt(&cfg->i2c, &addr, sizeof(addr), regs, 7);
	if (rc < 0) {
		goto out;
	}

	rc = bcd_regs_to_rtc_time(regs, rtc_tm);

out:
	k_sem_give(&data->lock);
	return rc;
}

#ifdef CONFIG_RTC_ALARM
static int mcp79xx_rtc_alarm_set_time(const struct device *dev, uint16_t id,
				      uint16_t mask, const struct rtc_time *rtc_tm)
{
	struct mcp79xx_data *data = dev->data;
	const struct mcp79xx_config *cfg = dev->config;
	uint8_t regs[6];
	uint8_t alm_cfg = 0;
	uint8_t alm_data[7];
	uint8_t ctrl;
	int rc;

	if (id > 1 || !rtc_tm) {
		return -EINVAL;
	}

	regs[0] = bin2bcd(rtc_tm->tm_sec) & 0x7F;
	regs[1] = bin2bcd(rtc_tm->tm_min) & 0x7F;
	regs[2] = bin2bcd(rtc_tm->tm_hour) & 0x3F;
	regs[3] = bin2bcd(rtc_tm->tm_wday + 1) & 0x07;
	regs[4] = bin2bcd(rtc_tm->tm_mday) & 0x3F;
	regs[5] = bin2bcd(rtc_tm->tm_mon + 1) & 0x1F;

	if (mask & RTC_ALARM_TIME_MASK_SECOND) {
		alm_cfg = 0;
	}
	if (mask & RTC_ALARM_TIME_MASK_MINUTE) {
		alm_cfg = 1;
	}
	if (mask & RTC_ALARM_TIME_MASK_HOUR) {
		alm_cfg = 2;
	}
	if (mask & RTC_ALARM_TIME_MASK_MONTHDAY) {
		alm_cfg = 4;
	}
	if (mask & RTC_ALARM_TIME_MASK_WEEKDAY) {
		alm_cfg = 3;
	}

	regs[3] |= (alm_cfg << 4);
	regs[3] |= (data->int_active_high ? BIT(7) : 0);

	k_sem_take(&data->lock, K_FOREVER);

	alm_data[0] = (id == 0) ? REG_ALM0_SEC : REG_ALM1_SEC;
	memcpy(&alm_data[1], regs, 6);
	rc = i2c_write_dt(&cfg->i2c, alm_data, 7);
	if (rc < 0) {
		goto out;
	}

	rc = read_register(dev, REG_RTC_CONTROL, &ctrl);
	if (rc < 0) {
		goto out;
	}

	ctrl |= (id == 0) ? BIT(4) : BIT(5);
	rc = write_register(dev, REG_RTC_CONTROL, ctrl);

out:
	k_sem_give(&data->lock);
	return rc;
}

static int mcp79xx_rtc_alarm_get_time(const struct device *dev, uint16_t id,
				      uint16_t *mask, struct rtc_time *rtc_tm)
{
	struct mcp79xx_data *data = dev->data;
	const struct mcp79xx_config *cfg = dev->config;
	uint8_t regs[6];
	uint8_t base_reg;
	uint8_t alm_cfg;
	int rc;

	if (id > 1 || !mask || !rtc_tm) {
		return -EINVAL;
	}

	k_sem_take(&data->lock, K_FOREVER);

	base_reg = (id == 0) ? REG_ALM0_SEC : REG_ALM1_SEC;
	rc = i2c_write_read_dt(&cfg->i2c, &base_reg, sizeof(base_reg), regs, 6);
	if (rc < 0) {
		goto out;
	}

	alm_cfg = (regs[3] >> 4) & 0x07;
	*mask = 0;
	switch (alm_cfg) {
	case 0:
		*mask = RTC_ALARM_TIME_MASK_SECOND;
		break;
	case 1:
		*mask = RTC_ALARM_TIME_MASK_MINUTE | RTC_ALARM_TIME_MASK_SECOND;
		break;
	case 2:
		*mask = RTC_ALARM_TIME_MASK_HOUR | RTC_ALARM_TIME_MASK_MINUTE |
			RTC_ALARM_TIME_MASK_SECOND;
		break;
	case 3:
		*mask = RTC_ALARM_TIME_MASK_WEEKDAY | RTC_ALARM_TIME_MASK_HOUR |
			RTC_ALARM_TIME_MASK_MINUTE | RTC_ALARM_TIME_MASK_SECOND;
		break;
	case 4:
	case 7:
		*mask = RTC_ALARM_TIME_MASK_MONTHDAY | RTC_ALARM_TIME_MASK_HOUR |
			RTC_ALARM_TIME_MASK_MINUTE | RTC_ALARM_TIME_MASK_SECOND;
		break;
	default:
		break;
	}

	rtc_tm->tm_sec = bcd2bin(regs[0] & 0x7F);
	rtc_tm->tm_min = bcd2bin(regs[1] & 0x7F);
	rtc_tm->tm_hour = bcd2bin(regs[2] & 0x3F);
	rtc_tm->tm_wday = bcd2bin(regs[3] & 0x07) - 1;
	rtc_tm->tm_mday = bcd2bin(regs[4] & 0x3F);
	rtc_tm->tm_mon = bcd2bin(regs[5] & 0x1F) - 1;
	rtc_tm->tm_year = 126;
	rtc_tm->tm_yday = -1;
	rtc_tm->tm_isdst = -1;
	rtc_tm->tm_nsec = 0;

out:
	k_sem_give(&data->lock);
	return rc;
}

static int mcp79xx_rtc_alarm_is_pending(const struct device *dev, uint16_t id)
{
	struct mcp79xx_data *data = dev->data;
	uint8_t wday_reg;
	uint8_t reg;
	int rc;

	if (id > 1) {
		return -EINVAL;
	}

	k_sem_take(&data->lock, K_FOREVER);

	reg = (id == 0) ? REG_ALM0_WDAY : REG_ALM1_WDAY;
	rc = read_register(dev, reg, &wday_reg);
	if (rc < 0) {
		goto out;
	}

	rc = (wday_reg & BIT(3)) ? 1 : 0;
	if (rc == 1) {
		wday_reg &= ~BIT(3);
		if (write_register(dev, reg, wday_reg) < 0) {
			LOG_ERR("ALM%u pending clear failed", id);
		}
	}

out:
	k_sem_give(&data->lock);
	return rc;
}

static int mcp79xx_rtc_alarm_set_callback(const struct device *dev, uint16_t id,
					  rtc_alarm_callback cb, void *user_data)
{
	struct mcp79xx_data *data = dev->data;

	if (id > 1) {
		return -EINVAL;
	}

	k_sem_take(&data->lock, K_FOREVER);

	if (id == 0) {
		data->rtc_alarm0_callback = cb;
		data->rtc_alarm0_user_data = user_data;
	} else {
		data->rtc_alarm1_callback = cb;
		data->rtc_alarm1_user_data = user_data;
	}

	k_sem_give(&data->lock);
	return 0;
}
#endif /* CONFIG_RTC_ALARM */

#ifdef CONFIG_RTC_CALIBRATION
/**
 * Map Zephyr calibration (ppb) to OSCTRIM.
 *
 * Zephyr: positive ppb increases RTC frequency; negative decreases it.
 * DS20005010J Reg 5-15: SIGN=1 add clocks (speed up); SIGN=0 subtract (slow).
 */
static int mcp79xx_rtc_set_calibration(const struct device *dev,
				       int32_t calibration)
{
	struct mcp79xx_data *data = dev->data;
	int32_t ppm = calibration / 1000;
	uint8_t trim_reg;
	int rc;

	if (ppm < -MCP79XX_TRIM_PPM_MAX || ppm > MCP79XX_TRIM_PPM_MAX) {
		return -EINVAL;
	}

	k_sem_take(&data->lock, K_FOREVER);

	if (ppm > 0) {
		/* Speed up: SIGN=1, add clocks */
		trim_reg = 0x80 | ((uint8_t)ppm & 0x7F);
	} else if (ppm < 0) {
		/* Slow down: SIGN=0, subtract clocks */
		trim_reg = (uint8_t)(-ppm) & 0x7F;
	} else {
		trim_reg = 0;
	}

	rc = write_register(dev, REG_RTC_OSCTRIM, trim_reg);
	LOG_DBG("RTC calibration %d ppb (%d ppm) → OSCTRIM=0x%02x",
		calibration, ppm, trim_reg);

	k_sem_give(&data->lock);
	return rc;
}

static int mcp79xx_rtc_get_calibration(const struct device *dev,
				       int32_t *calibration)
{
	struct mcp79xx_data *data = dev->data;
	uint8_t trim_reg;
	int32_t ppm;
	int rc;

	if (!calibration) {
		return -EINVAL;
	}

	k_sem_take(&data->lock, K_FOREVER);

	rc = read_register(dev, REG_RTC_OSCTRIM, &trim_reg);
	if (rc < 0) {
		goto out;
	}

	ppm = trim_reg & 0x7F;
	if (trim_reg & 0x80) {
		/* SIGN=1 → add → positive Zephyr calibration */
		*calibration = ppm * 1000;
	} else if (ppm != 0) {
		/* SIGN=0 → subtract → negative Zephyr calibration */
		*calibration = -ppm * 1000;
	} else {
		*calibration = 0;
	}

out:
	k_sem_give(&data->lock);
	return rc;
}
#endif /* CONFIG_RTC_CALIBRATION */

static int mcp79xx_init(const struct device *dev)
{
	struct mcp79xx_data *data = dev->data;
	const struct mcp79xx_config *cfg = dev->config;
	time_t unix_time = 0;
	int rc;

	k_sem_init(&data->lock, 0, 1);

	if (!device_is_ready(cfg->i2c.bus)) {
		LOG_ERR("I2C device %s is not ready", cfg->i2c.bus->name);
		rc = -ENODEV;
		goto out;
	}

	rc = read_time(dev, &unix_time);
	if (rc < 0) {
		goto out;
	}

	LOG_INF("MCP79xx initialized, current time: %u", (uint32_t)unix_time);

	rc = set_day_of_week(dev, &unix_time);
	if (rc < 0) {
		goto out;
	}

	/* Force 24-hour mode (errata #1 applies only in 12h mode). */
	data->registers.rtc_hours.twelve_hr = false;
	rc = write_register(dev, REG_RTC_HOUR,
			    *((uint8_t *)(&data->registers.rtc_hours)));
	if (rc < 0) {
		goto out;
	}

	/* Disarm leftover alarms from prior firmware (avoid MFP IRQ noise). */
	{
		uint8_t ctrl;

		rc = read_register(dev, REG_RTC_CONTROL, &ctrl);
		if (rc < 0) {
			goto out;
		}
		ctrl &= ~(BIT(4) | BIT(5)); /* ALM0EN | ALM1EN */
		rc = write_register(dev, REG_RTC_CONTROL, ctrl);
		if (rc < 0) {
			goto out;
		}
	}

	if (cfg->int_gpios.port != NULL) {
		if (!gpio_is_ready_dt(&cfg->int_gpios)) {
			LOG_ERR("Port device %s is not ready",
				cfg->int_gpios.port->name);
			rc = -ENODEV;
			goto out;
		}

		data->mcp79xx = dev;
		k_work_init(&data->alarm_work, mcp79xx_work_handler);

		gpio_pin_configure_dt(&cfg->int_gpios, GPIO_INPUT);
		gpio_pin_interrupt_configure_dt(&cfg->int_gpios,
						GPIO_INT_EDGE_TO_ACTIVE);
		gpio_init_callback(&data->int_callback, mcp79xx_gpio_cb,
				   BIT(cfg->int_gpios.pin));
		(void)gpio_add_callback(cfg->int_gpios.port, &data->int_callback);

		data->int_active_high =
			((cfg->int_gpios.dt_flags & GPIO_ACTIVE_LOW) !=
			 GPIO_ACTIVE_LOW);
		data->alm0_registers.alm_weekday.alm_pol = data->int_active_high;
		data->alm1_registers.alm_weekday.alm_pol = data->int_active_high;
		rc = write_register(dev, REG_ALM0_WDAY,
			*((uint8_t *)(&data->alm0_registers.alm_weekday)));
		if (rc < 0) {
			goto out;
		}
		rc = write_register(dev, REG_ALM1_WDAY,
			*((uint8_t *)(&data->alm1_registers.alm_weekday)));
	}

out:
	k_sem_give(&data->lock);
	return rc;
}

static DEVICE_API(rtc, mcp79xx_rtc_api) = {
	.set_time = mcp79xx_rtc_api_set_time,
	.get_time = mcp79xx_rtc_api_get_time,
#ifdef CONFIG_RTC_ALARM
	.alarm_set_time = mcp79xx_rtc_alarm_set_time,
	.alarm_get_time = mcp79xx_rtc_alarm_get_time,
	.alarm_is_pending = mcp79xx_rtc_alarm_is_pending,
	.alarm_set_callback = mcp79xx_rtc_alarm_set_callback,
#endif
#ifdef CONFIG_RTC_CALIBRATION
	.set_calibration = mcp79xx_rtc_set_calibration,
	.get_calibration = mcp79xx_rtc_get_calibration,
#endif
};

#define INST_DT_MCP79XX(index)						\
									\
	static struct mcp79xx_data mcp79xx_data_##index;		\
									\
	static const struct mcp79xx_config mcp79xx_config_##index = {	\
		.i2c = I2C_DT_SPEC_INST_GET(index),			\
		.int_gpios = GPIO_DT_SPEC_INST_GET_OR(index, int_gpios, {0}), \
	};								\
									\
	DEVICE_DT_INST_DEFINE(index, mcp79xx_init, NULL,		\
			      &mcp79xx_data_##index,			\
			      &mcp79xx_config_##index,			\
			      POST_KERNEL,				\
			      CONFIG_COUNTER_INIT_PRIORITY,		\
			      &mcp79xx_rtc_api);

DT_INST_FOREACH_STATUS_OKAY(INST_DT_MCP79XX);
