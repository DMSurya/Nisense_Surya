/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * MAX86141 PPG LED helpers for RAW mode (SFH7074: LED1=Green, LED2=Red,
 * LED3=IR). Product SEQ is IR→Red→Green→DIRECT_AMBIENT.
 *
 * max86141_ppg_leds_set() gates all three PAs + product SEQ for measurement
 * start/stop. Drain reasserts only when PA was zeroed. Host AGC owns PA via
 * max86141_ppg_set_led_pa().
 *
 * FIFO fillers (0x7ECxx / 0x7FFFF) sanitized via max86141_green_sanitize_adc().
 */

#ifndef MAX86141_GREEN_H_
#define MAX86141_GREEN_H_

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor/max32664.h>

/** Hub slot3/4 ADC sentinel (exact) and near-fillers (FW 30.2.2 emits 0x7ECxx). */
#define MAX86141_GREEN_ADC_INVALID     0x7FFFFU
/** Same floor as MAX32664_HUB_ADC_INVALID_LO (not 0x70000 — kills real green DC). */
#define MAX86141_HUB_ADC_INVALID_LO    MAX32664_HUB_ADC_INVALID_LO

/** Which physical LED driver to gate (exclusive self-test / debug). */
enum max86141_ppg_led {
	MAX86141_PPG_LED_GREEN = 0, /**< LED1 */
	MAX86141_PPG_LED_RED   = 1, /**< LED2 */
	MAX86141_PPG_LED_IR    = 2, /**< LED3 — not visible to eye; use phone camera */
};

static inline uint32_t max86141_green_sanitize_adc(uint32_t adc)
{
	return max32664_hub_adc_sanitize(adc);
}

static inline bool max86141_hub_adc_plausible(uint32_t adc)
{
	return max32664_hub_adc_plausible(adc);
}

/**
 * @brief Drive exactly one PPG LED (exclusive LED_SEQ + that PA).
 *
 * Zeros the other two PAs. When @p on is false, all PAs are zeroed (SEQ left).
 *
 * @return 0 on success, negative errno on I2C failure / bad @p led.
 */
int max86141_ppg_led_set(const struct device *hub, enum max86141_ppg_led led,
			 bool on);

/**
 * @brief Gate Green+Red+IR together with the product LED_SEQ (incl. ambient).
 *
 * Restores LED_SEQ1=0x23, LED_SEQ2=0x91 (Green+DIRECT_AMBIENT), LED_SEQ3=0x00,
 * RANGE1=0x15 (62 mA FS per LED), and SFH7074 dual-PD bias. When on, also
 * clears AFE SHDN. Used at measurement start/stop; drain only reasserts when
 * PA was zeroed by the hub.
 *
 * @param leds_on_out Optional runtime flag (max32664_raw_runtime.leds_enabled).
 */
int max86141_ppg_leds_set(const struct device *hub, bool on, bool *leds_on_out);

/**
 * @brief Set host AGC / factory-seeded LED PA (applied on next leds_set(true)).
 *
 * This is the IR (LED3) channel. Red and Green now have their own
 * independent AGC state — see max86141_ppg_set_led_pa_red()/_green() —
 * rather than being derived as a fixed fraction of this value. IR
 * remains the "primary" channel other code reads back via
 * max86141_ppg_get_led_pa() for logging/telemetry.
 */
void max86141_ppg_set_led_pa(uint8_t pa);

/** @brief Current host LED PA register value (IR channel). */
uint8_t max86141_ppg_get_led_pa(void);

/**
 * @brief Set host AGC PA for the Red (LED2) channel independently of IR.
 *
 * Previously Red current was always a fixed fraction of IR's current
 * (a feed-forward ratio hand-tuned from a single field session). That
 * scheme can hit its own DC target only when Red's actual perfusion
 * index happens to match whatever session the ratio was derived from —
 * it has no way to correct itself from real Red DC/AC readings. This
 * lets the AGC close the loop on Red directly.
 */
void max86141_ppg_set_led_pa_red(uint8_t pa);

/** @brief Current host LED PA register value (Red channel). */
uint8_t max86141_ppg_get_led_pa_red(void);

/**
 * @brief Set host AGC PA for the Green (LED1) channel independently of IR.
 * See max86141_ppg_set_led_pa_red() — same rationale, Green channel.
 */
void max86141_ppg_set_led_pa_green(uint8_t pa);

/** @brief Current host LED PA register value (Green channel). */
uint8_t max86141_ppg_get_led_pa_green(void);

#endif /* MAX86141_GREEN_H_ */
