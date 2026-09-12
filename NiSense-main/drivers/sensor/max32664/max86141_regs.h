/**
 * @file max86141_regs.h
 * @brief MAX86141 PPG AFE - Register definitions and hub passthrough helpers
 *
 * Provides:
 * - Complete MAX86141 register map
 * - Transparent read/write wrappers that hide the hub passthrough layer
 *
 * Usage:
 *   uint8_t val;
 *   max86141_read_reg(hub_dev, MAX86141_REG_PART_ID, &val);
 *   max86141_write_reg(hub_dev, MAX86141_REG_SYSTEM_CTRL, 0x00);
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MAX86141_REGS_H_
#define MAX86141_REGS_H_

#include <zephyr/device.h>
#include "max32664_priv.h"

/* Hub sensor index for MAX86141 (always 0x00 on MAX32664 GWEC) */
#define HUB_SENSOR_IDX_MAX86141     0x00

/* ============================================================================
 * MAX86141 Register Map
 * ============================================================================ */

/* Interrupt Registers */
#define MAX86141_REG_INT_STATUS1        0x00
#define MAX86141_REG_INT_STATUS2        0x01
#define MAX86141_REG_INT_ENABLE1        0x02
#define MAX86141_REG_INT_ENABLE2        0x03

/* FIFO Registers */
#define MAX86141_REG_FIFO_WR_PTR        0x04
#define MAX86141_REG_FIFO_RD_PTR        0x05
#define MAX86141_REG_OVF_COUNTER        0x06
#define MAX86141_REG_FIFO_DATA_CNT      0x07
#define MAX86141_REG_FIFO_DATA          0x08
#define MAX86141_REG_FIFO_CONFIG1       0x09
#define MAX86141_REG_FIFO_CONFIG2       0x0A

/* System Registers */
#define MAX86141_REG_SYSTEM_CTRL        0x0D

/* PPG Configuration */
#define MAX86141_REG_PPG_SYNC_CTRL      0x10
#define MAX86141_REG_PPG_CONFIG1        0x11
#define MAX86141_REG_PPG_CONFIG2        0x12
#define MAX86141_REG_PPG_CONFIG3        0x13
#define MAX86141_REG_PROX_INT_THRESH    0x14
#define MAX86141_REG_PD_CONFIG          0x15
#define MAX86141_REG_PICKET_FENCE       0x16

/*
 * SFH7074 dual-PD board: PD1 → PD1IN, PD3 → PD2IN (SFH PD2 unused).
 * PD_CONFIG: PDBIAS2[6:4]=101, PDBIAS1[2:0]=101 → both 65–130 pF.
 */
#define MAX86141_PD_CONFIG_SFH7074_DUAL 0x55U

/* DIRECT_AMBIENT exposure tag (MAX86141 LED_SEQ LEDCn encoding). */
#define MAX86141_LED_TAG_DIRECT_AMBIENT 0x9U

/*
 * Product LED sequence (SFH7074: LED1=Green, LED2=Red, LED3=IR):
 *   LEDC1=LED3(IR), LEDC2=LED2(Red), LEDC3=LED1(Green), LEDC4=DIRECT_AMBIENT.
 * LED_SEQ1 = (LEDC2<<4)|LEDC1 = 0x23
 * LED_SEQ2 = (LEDC4<<4)|LEDC3 = 0x91  (Green + ambient)
 * LED_SEQ3 = 0x00
 *
 * RAW mode ignores DT child LED currents / sample-rate for this sequence;
 * host AGC adjusts PA at runtime (see max86141_ppg_leds_set / raw_runtime).
 */
#define MAX86141_LED_SEQ1_PRODUCT  		0x23U
#define MAX86141_LED_SEQ2_PRODUCT  		0x91U
#define MAX86141_LED_SEQ3_PRODUCT  		0x00U
/* Each LED RGE=01 → 62 mA full-scale. */
#define MAX86141_LED_RANGE1_62MA   		0x15U

/* LED PA ceiling.
 *
 * REVERTED last session's cap (was 0x52/~20mA) after checking the actual
 * pulse timing: PPG_CONFIG1 sets PPG_TINT=58.7us, and this samples at 25 Hz
 * (40ms period) — the LED is only actually on for ~0.15% of each sample
 * period, not continuously. Treating the DAC current as if it were a
 * continuous exposure level (my earlier reasoning) overstated the real
 * time-averaged optical output by ~650x; "58mA peak, 0.15% duty" works out
 * to roughly 0.09mA time-averaged, which is why Maxim's own datasheet rates
 * this chip's LED range up to 124mA for exactly this kind of pulsed
 * reflectance-PPG use.
 *
 * Confirmed empirically too: at the capped current, avg DC dropped from
 * ~180-200k to ~82-85k (of 524,287 max) and R fell back to 0.14-0.26 —
 * below the plausible 0.3-2.0 SpO2 band, breaking confidence again. Back
 * to 0xF0 (~58 mA peak / ~0.09 mA time-averaged) restores the healthier
 * signal (R 0.4-1.4) seen before the cap.
 *
 * CAVEAT: still could not extract max86140-max86141.rar in this
 * environment to read Maxim's own photobiological exposure table directly
 * — this duty-cycle math is sound but if an exact certified limit is
 * needed for a regulatory submission, verify against that table. */
#define MAX86141_LED_PA_DEFAULT    		0x7FU
#define MAX86141_LED_PA_MIN        		0x20U
#define MAX86141_LED_PA_MAX        		0xF0U

/* LED Sequence Registers */
#define MAX86141_REG_LED_SEQ1           0x20
#define MAX86141_REG_LED_SEQ2           0x21
#define MAX86141_REG_LED_SEQ3           0x22

/* LED Drive Current Registers */
#define MAX86141_REG_LED1_DRV_CUR       0x23
#define MAX86141_REG_LED2_DRV_CUR       0x24
#define MAX86141_REG_LED3_DRV_CUR       0x25
#define MAX86141_REG_LED4_DRV_CUR       0x26
#define MAX86141_REG_LED5_DRV_CUR       0x27
#define MAX86141_REG_LED6_DRV_CUR       0x28
#define MAX86141_REG_LEDPILOT_PA        0x29
#define MAX86141_REG_LED_RANGE1         0x2A
#define MAX86141_REG_LED_RANGE2         0x2B

/* Identification */
#define MAX86141_REG_PART_ID            0xFF

/* Interrupt enable bit masks */
#define MAX86141_INT_EN_A_FULL          0xC0
#define MAX86141_INT_EN_DATA_RDY        0x40

/* FIFO tag values */
#define MAX86141_TAG_LED1_PD1           0x01
#define MAX86141_TAG_LED2_PD1           0x02
#define MAX86141_TAG_LED3_PD1           0x03
#define MAX86141_TAG_TIMESTAMP          0x1F

/* Part ID values */
#define MAX86141_PART_ID_MAX86141       0x25
#define MAX86141_PART_ID_MAX86142       0x36

/* ============================================================================
 * Transparent Register Access Wrappers
 *
 * These inline functions hide the hub passthrough layer so callers
 * simply pass (hub_dev, register, data) without repeating the sensor index.
 * ============================================================================ */

/**
 * @brief Read a MAX86141 register via hub passthrough
 *
 * @param hub  MAX32664 hub device
 * @param reg  MAX86141 register address
 * @param val  Pointer to store the read value
 * @return 0 on success, negative errno on failure
 */
static inline int max86141_read_reg(const struct device *hub,
				    uint8_t reg, uint8_t *val)
{
	return max32664_read_sensor_reg(hub, HUB_SENSOR_IDX_MAX86141, reg, val);
}

/**
 * @brief Write a MAX86141 register via hub passthrough
 *
 * @param hub  MAX32664 hub device
 * @param reg  MAX86141 register address
 * @param val  Value to write
 * @return 0 on success, negative errno on failure
 */
static inline int max86141_write_reg(const struct device *hub,
				     uint8_t reg, uint8_t val)
{
	return max32664_write_sensor_reg(hub, HUB_SENSOR_IDX_MAX86141, reg, val);
}

/**
 * @brief Burst-read multiple bytes from MAX86141 FIFO via hub passthrough
 *
 * Optimized for high-throughput FIFO draining. ~5x faster than calling
 * max86141_read_reg() in a loop (single MFIO wake, 1.5ms inter-byte delay).
 *
 * @param hub        MAX32664 hub device
 * @param out_buf    Output buffer for FIFO bytes
 * @param num_bytes  Number of bytes to read from FIFO
 * @return 0 on success, negative errno on failure
 */
static inline int max86141_read_fifo_burst(const struct device *hub,
					   uint8_t *out_buf,
					   uint16_t num_bytes)
{
	return max32664_read_fifo_burst(hub, HUB_SENSOR_IDX_MAX86141,
					MAX86141_REG_FIFO_DATA, out_buf,
					num_bytes);
}

#endif /* MAX86141_REGS_H_ */
