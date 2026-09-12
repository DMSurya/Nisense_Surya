/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "max86141_green.h"

#include "max86141_regs.h"

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(max32664, CONFIG_SENSOR_LOG_LEVEL);

/* Product RAW: see MAX86141_LED_SEQ*_PRODUCT in max86141_regs.h. */
#define LED_SEQ1_PRODUCT     MAX86141_LED_SEQ1_PRODUCT
#define LED_SEQ2_PRODUCT     MAX86141_LED_SEQ2_PRODUCT
#define LED_SEQ3_PRODUCT     MAX86141_LED_SEQ3_PRODUCT
#define LED_RANGE1_ALL_62MA  MAX86141_LED_RANGE1_62MA
#define PPG_LED_CUR_OFF      0x00U

/* Host AGC PA — now independently closed-loop per channel (see
 * max32664_raw_host_agc() in max32664_raw.c). Red/Green used to be
 * derived as a fixed fraction of this IR value (red_pa_from() /
 * green_pa_from() below); those functions are kept only to seed a
 * reasonable cold-start value before the first real AGC update. */
static uint8_t g_led_pa = MAX86141_LED_PA_DEFAULT;
static uint8_t g_red_pa = MAX86141_LED_PA_DEFAULT;
static uint8_t g_green_pa = MAX86141_LED_PA_DEFAULT;

#define LED_TEST_DWELL_MS    1500U
#define LED_TEST_GAP_MS      700U
#define LED_TEST_DRIVE       0xFFU
#define LED_TEST_RANGE1      0xFFU
#define LED_TEST_TICK_MS     100U

static const uint8_t ppg_led_pa_reg[] = {
	[MAX86141_PPG_LED_GREEN] = MAX86141_REG_LED1_DRV_CUR,
	[MAX86141_PPG_LED_RED]   = MAX86141_REG_LED2_DRV_CUR,
	[MAX86141_PPG_LED_IR]    = MAX86141_REG_LED3_DRV_CUR,
};

static const uint8_t ppg_led_seq1_excl[] = {
	[MAX86141_PPG_LED_GREEN] = 0x01U,
	[MAX86141_PPG_LED_RED]   = 0x02U,
	[MAX86141_PPG_LED_IR]    = 0x03U,
};

static const char *ppg_led_name[] = {
	[MAX86141_PPG_LED_GREEN] = "GREEN",
	[MAX86141_PPG_LED_RED]   = "RED",
	[MAX86141_PPG_LED_IR]    = "IR",
};

void max86141_ppg_set_led_pa(uint8_t pa)
{
	if (pa < MAX86141_LED_PA_MIN) {
		pa = MAX86141_LED_PA_MIN;
	} else if (pa > MAX86141_LED_PA_MAX) {
		pa = MAX86141_LED_PA_MAX;
	}
	g_led_pa = pa;
}

uint8_t max86141_ppg_get_led_pa(void)
{
	return g_led_pa;
}

void max86141_ppg_set_led_pa_red(uint8_t pa)
{
	if (pa < MAX86141_LED_PA_MIN) {
		pa = MAX86141_LED_PA_MIN;
	} else if (pa > MAX86141_LED_PA_MAX) {
		pa = MAX86141_LED_PA_MAX;
	}
	g_red_pa = pa;
}

uint8_t max86141_ppg_get_led_pa_red(void)
{
	return g_red_pa;
}

void max86141_ppg_set_led_pa_green(uint8_t pa)
{
	if (pa < MAX86141_LED_PA_MIN) {
		pa = MAX86141_LED_PA_MIN;
	} else if (pa > MAX86141_LED_PA_MAX) {
		pa = MAX86141_LED_PA_MAX;
	}
	g_green_pa = pa;
}

uint8_t max86141_ppg_get_led_pa_green(void)
{
	return g_green_pa;
}

/* Green runs at a reduced fraction of the Red/IR current.
 *
 * Per sir's own bench testing: driving Green at the same current as Red/IR
 * was hurting Perfusion Index — "green seems has impact on this PI...give
 * it try by reducing green power." Green's optical path (SFH7074 LED1,
 * 520nm) has different skin penetration/reflectance characteristics than
 * Red/IR; running it at full AGC-controlled current can push its DC toward
 * a range where added drive doesn't buy more usable pulsatile signal.
 *
 * CORRECTED 2026-08-24: at the previous 55% fraction, fresh field data
 * shows Green_DC measuring ~430,687 against the reference target of
 * ~270,000 (see reference operating point in max86141_green.c Red
 * comment above) — Green_DC running ~1.6x too high. Scaling the fraction
 * by that same ratio (270000/430687 = 0.627) gives ~35%, i.e. a further
 * reduction from 55%. GREEN_PA_MIN_FLOOR keeps it from dropping so low
 * Green stops sensing anything at all. Re-verify against a fresh export
 * once flashed, alongside the IR/Red corrections made in the same
 * update. */
#define GREEN_PA_NUMERATOR   35U
#define GREEN_PA_DENOM       100U
#define GREEN_PA_MIN_FLOOR   MAX86141_LED_PA_MIN

/* Red runs at a REDUCED fraction relative to IR — corrected 2026-08-24
 * from an earlier over-boost.
 *
 * R = (AC_red/DC_red)/(AC_ir/DC_ir) is, in ideal linear operation, mostly
 * INDEPENDENT of overall LED intensity: driving both LEDs harder by the
 * same factor scales AC and DC together and cancels out of the ratio.
 * The previous 130% boost was a hypothesis to correct a low observed R —
 * but fresh field data (2026-08-24 export) shows it overshot: with the
 * 130% boost active, Red_DC measured ~404,000 against a target of
 * ~260,000 (per the reference operating point: IR_DC=250000, Red_DC=
 * 260000, Green_DC=270000 -> SpO2~95.6%, Hb~14g/dL). That's Red_DC
 * running ~1.55x too high. Scaling the boost fraction by that same ratio
 * (260000/404428 = 0.643) gives ~84%, i.e. Red should run at a REDUCED
 * fraction of IR's current, not a boosted one. This is now a measured
 * correction, not a hypothesis — but should still be re-verified against
 * a fresh export once flashed, since IR's own drive level is also being
 * corrected in the same update (see RAW_AGC_DC_LO/HI in max32664_raw.c). */
#define RED_PA_NUMERATOR     84U
#define RED_PA_DENOM         100U
#define RED_PA_MAX_FLOOR     0xFFU

static inline uint8_t red_pa_from(uint8_t cur)
{
	uint32_t scaled = ((uint32_t)cur * RED_PA_NUMERATOR) / RED_PA_DENOM;

	if (scaled > RED_PA_MAX_FLOOR) {
		scaled = RED_PA_MAX_FLOOR;
	}
	return (uint8_t)scaled;
}

static inline uint8_t green_pa_from(uint8_t cur)
{
	uint32_t scaled = ((uint32_t)cur * GREEN_PA_NUMERATOR) / GREEN_PA_DENOM;

	if (scaled < GREEN_PA_MIN_FLOOR) {
		scaled = GREEN_PA_MIN_FLOOR;
	}
	return (uint8_t)scaled;
}

static bool g_agc_seeded;

static int ppg_leds_pa_all(const struct device *hub, bool on)
{
	int ret;
	uint8_t green;
	uint8_t red;
	uint8_t ir;

	if (on && !g_agc_seeded) {
		/* Cold-start only: seed Red/Green from the historical
		 * field-tuned ratio to IR so the very first measurement
		 * isn't shooting from a generic default. From here on,
		 * max32664_raw_host_agc() drives all three independently
		 * from their own measured DC/AC. */
		g_red_pa = red_pa_from(g_led_pa);
		g_green_pa = green_pa_from(g_led_pa);
		g_agc_seeded = true;
	}

	green = on ? g_green_pa : PPG_LED_CUR_OFF;
	red = on ? g_red_pa : PPG_LED_CUR_OFF;
	ir = on ? g_led_pa : PPG_LED_CUR_OFF;

	ret = max86141_write_reg(hub, MAX86141_REG_LED1_DRV_CUR, green);
	if (ret != 0) {
		return ret;
	}
	ret = max86141_write_reg(hub, MAX86141_REG_LED2_DRV_CUR, red);
	if (ret != 0) {
		return ret;
	}
	return max86141_write_reg(hub, MAX86141_REG_LED3_DRV_CUR, ir);
}

static int ppg_leds_apply_product_seq(const struct device *hub)
{
	int ret;

	ret = max86141_write_reg(hub, MAX86141_REG_LED_SEQ1, LED_SEQ1_PRODUCT);
	if (ret != 0) {
		return ret;
	}
	ret = max86141_write_reg(hub, MAX86141_REG_LED_SEQ2, LED_SEQ2_PRODUCT);
	if (ret != 0) {
		return ret;
	}
	return max86141_write_reg(hub, MAX86141_REG_LED_SEQ3, LED_SEQ3_PRODUCT);
}

static int ppg_led_drive_exclusive(const struct device *hub,
				   enum max86141_ppg_led led, bool on)
{
	int ret;

	ret = ppg_leds_pa_all(hub, false);
	if (ret != 0) {
		return ret;
	}
	if (!on) {
		return 0;
	}

	ret = max86141_write_reg(hub, MAX86141_REG_SYSTEM_CTRL, 0x00);
	if (ret != 0) {
		return ret;
	}
	ret = max86141_write_reg(hub, MAX86141_REG_LED_RANGE1, LED_TEST_RANGE1);
	if (ret != 0) {
		return ret;
	}
	ret = max86141_write_reg(hub, MAX86141_REG_LED_SEQ1, ppg_led_seq1_excl[led]);
	if (ret != 0) {
		return ret;
	}
	ret = max86141_write_reg(hub, MAX86141_REG_LED_SEQ2, 0x00);
	if (ret != 0) {
		return ret;
	}
	ret = max86141_write_reg(hub, MAX86141_REG_LED_SEQ3, 0x00);
	if (ret != 0) {
		return ret;
	}
	return max86141_write_reg(hub, ppg_led_pa_reg[led], LED_TEST_DRIVE);
}

int max86141_ppg_led_set(const struct device *hub, enum max86141_ppg_led led,
			 bool on)
{
	uint8_t rb_pa = 0U;
	uint8_t rb_seq = 0U;
	int ret;

	if (led > MAX86141_PPG_LED_IR) {
		return -EINVAL;
	}

	ret = ppg_led_drive_exclusive(hub, led, on);
	if (ret != 0) {
		return ret;
	}

	if (!on) {
		LOG_INF("LED %s OFF", ppg_led_name[led]);
		return 0;
	}

	(void)max86141_read_reg(hub, ppg_led_pa_reg[led], &rb_pa);
	(void)max86141_read_reg(hub, MAX86141_REG_LED_SEQ1, &rb_seq);
	LOG_INF("LED %s ON  (PA rb=0x%02X SEQ1 rb=0x%02X expect PA=0x%02X SEQ=0x%02X)",
		ppg_led_name[led], rb_pa, rb_seq, LED_TEST_DRIVE, ppg_led_seq1_excl[led]);
	return 0;
}

int max86141_ppg_leds_set(const struct device *hub, bool on, bool *leds_on_out)
{
	bool was_on = (leds_on_out != NULL) ? *leds_on_out : !on;
	int ret;

	ret = ppg_leds_apply_product_seq(hub);
	if (ret != 0) {
		return ret;
	}

	if (on) {
		ret = max86141_write_reg(hub, MAX86141_REG_LED_RANGE1, LED_RANGE1_ALL_62MA);
		if (ret != 0) {
			return ret;
		}
		/* Re-bias both SFH7074 PDs (hub WHRM may have overwritten). */
		ret = max86141_write_reg(hub, MAX86141_REG_PD_CONFIG,
					 MAX86141_PD_CONFIG_SFH7074_DUAL);
		if (ret != 0) {
			return ret;
		}
		/* Wake AFE out of SHDN for measurement. */
		ret = max86141_write_reg(hub, MAX86141_REG_SYSTEM_CTRL, 0x00);
		if (ret != 0) {
			return ret;
		}
	}

	ret = ppg_leds_pa_all(hub, on);
	if (ret != 0) {
		return ret;
	}

	if (leds_on_out != NULL) {
		*leds_on_out = on;
	}

	if (was_on != on) {
		LOG_INF("PPG LEDs %s (product SEQ IR/Red/Green/Ambient, "
			"PA green=0x%02X red=0x%02X ir=0x%02X)",
			on ? "on" : "off", g_green_pa, g_red_pa, g_led_pa);
	}

	return 0;
}
