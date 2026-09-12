/*
 * Copyright (c) 2025 NiSense
 * SPDX-License-Identifier: Apache-2.0
 *
 * MAX3010x Pulse Oximeter / Heart-Rate Sensor - Custom Driver Header
 *
 * Unified driver supporting:
 *   - MAX30101: Red + IR + Green LEDs (3 channels)
 *   - MAX30102: Red + IR LEDs only (2 channels)
 *   - MAX30105: Red + IR + Green LEDs (3 channels, particle sensor variant)
 *
 * Register definitions, data structures, and constants shared across variants.
 * All modern MAX3010x variants (30101/30102/30105) share Part ID 0x15 and
 * identical register maps - only LED population differs.
 */

#ifndef DRIVERS_SENSOR_MAX3010X_PPG_H_
#define DRIVERS_SENSOR_MAX3010X_PPG_H_

#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <max3010x_ppg.h>  /* Public API: custom sensor attributes */

/* ========== Chip Variants ========== */
enum max3010x_variant {
	MAX3010X_VARIANT_30101,  /* 3 LEDs: Red + IR + Green */
	MAX3010X_VARIANT_30102,  /* 2 LEDs: Red + IR only */
	MAX3010X_VARIANT_30105,  /* 3 LEDs: Red + IR + Green (particle sensor) */
};

/* LED count per variant */
#define MAX3010X_LEDS_30101    3
#define MAX3010X_LEDS_30102    2
#define MAX3010X_LEDS_30105    3

/* ========== Part Identification ========== */
/* All modern MAX3010x variants share the same Part ID */
#define MAX3010X_PART_ID               0x15

/* ========== Register Map (identical across 30101/30102/30105) ========== */
#define MAX3010X_REG_INT_STS1          0x00
#define MAX3010X_REG_INT_STS2          0x01
#define MAX3010X_REG_INT_EN1           0x02
#define MAX3010X_REG_INT_EN2           0x03
#define MAX3010X_REG_FIFO_WR_PTR       0x04
#define MAX3010X_REG_OVF_CTR           0x05
#define MAX3010X_REG_FIFO_RD_PTR       0x06
#define MAX3010X_REG_FIFO_DATA         0x07
#define MAX3010X_REG_FIFO_CFG          0x08
#define MAX3010X_REG_MODE_CFG          0x09
#define MAX3010X_REG_SPO2_CFG          0x0A
#define MAX3010X_REG_LED1_PA           0x0C  /* Red   */
#define MAX3010X_REG_LED2_PA           0x0D  /* IR    */
#define MAX3010X_REG_LED3_PA           0x0E  /* Green (no effect on MAX30102) */
#define MAX3010X_REG_PILOT_PA          0x10
#define MAX3010X_REG_MULTI_LED1        0x11
#define MAX3010X_REG_MULTI_LED2        0x12
#define MAX3010X_REG_TEMP_INT          0x1F
#define MAX3010X_REG_TEMP_FRAC         0x20
#define MAX3010X_REG_TEMP_CFG          0x21
#define MAX3010X_REG_PROX_INT_THRESH   0x30
#define MAX3010X_REG_REV_ID            0xFE
#define MAX3010X_REG_PART_ID           0xFF

/* ========== MODE_CFG Register (0x09) ========== */
#define MAX3010X_MODE_SHDN             BIT(7)
#define MAX3010X_MODE_RESET            BIT(6)
#define MAX3010X_MODE_MASK             0x07
#define MAX3010X_MODE_HR               0x02  /* Heart Rate: Red LED only  */
#define MAX3010X_MODE_SPO2             0x03  /* SpO2: Red + IR LEDs       */
#define MAX3010X_MODE_MULTI            0x07  /* Multi-LED: configurable   */

/* ========== FIFO_CFG Register (0x08) ========== */
#define MAX3010X_FIFO_SMP_AVE_SHIFT    5
#define MAX3010X_FIFO_ROLLOVER_EN      BIT(4)
#define MAX3010X_FIFO_A_FULL_MASK      0x0F

/* ========== SPO2_CFG Register (0x0A) ========== */
#define MAX3010X_SPO2_ADC_RGE_SHIFT    5
#define MAX3010X_SPO2_SR_SHIFT         2
#define MAX3010X_SPO2_PW_SHIFT         0

/* ========== FIFO Constants ========== */
#define MAX3010X_FIFO_DEPTH            32
#define MAX3010X_FIFO_PTR_MASK         0x1F
#define MAX3010X_BYTES_PER_CHANNEL     3
#define MAX3010X_MAX_CHANNELS          3
#define MAX3010X_ADC_MASK              0x3FFFF  /* 18-bit */

/* Maximum raw bytes from one FIFO drain: 32 samples × 3 ch × 3 bytes = 288 */
#define MAX3010X_MAX_FIFO_BYTES \
	(MAX3010X_FIFO_DEPTH * MAX3010X_MAX_CHANNELS * MAX3010X_BYTES_PER_CHANNEL)

/* ========== Multi-LED Slot Values ========== */
#define MAX3010X_SLOT_DISABLED         0
#define MAX3010X_SLOT_RED              1
#define MAX3010X_SLOT_IR               2
#define MAX3010X_SLOT_GREEN            3  /* No effect on MAX30102 (produces zeros) */

/* ========== Interrupt Bits ========== */
#define MAX3010X_INT_A_FULL            BIT(7)
#define MAX3010X_INT_DATA_RDY          BIT(6)
#define MAX3010X_INT_ALC_OVF           BIT(5)
#define MAX3010X_INT_PROX_INT          BIT(4)
#define MAX3010X_INT_DIE_TEMP_RDY      BIT(1)

/* ========== Temperature Config ========== */
#define MAX3010X_TEMP_EN               BIT(0)  /* TEMP_CFG[0]: trigger conversion */

/* ========== Device Tree Configuration ========== */
struct max3010x_ppg_config {
	struct i2c_dt_spec i2c;
	enum max3010x_variant variant;  /* Chip variant (from DT compatible) */
	uint8_t num_leds;               /* Number of physical LEDs (2 or 3) */
	uint8_t mode;                   /* MAX3010X_MODE_HR / _SPO2 / _MULTI */
	uint8_t spo2_cfg;               /* SPO2_CFG register value            */
	uint8_t fifo_cfg;               /* FIFO_CFG register value            */
	uint8_t led1_pa;                /* Red LED current   (0x00..0xFF)     */
	uint8_t led2_pa;                /* IR LED current    (0x00..0xFF)     */
	uint8_t led3_pa;                /* Green LED current (0x00..0xFF)     */
	uint8_t slot[4];                /* Multi-LED slot assignments         */
};

/* ========== Run-time Driver Data ========== */
struct max3010x_ppg_data {
	/* Current sample (returned by channel_get) */
	uint32_t raw[MAX3010X_MAX_CHANNELS];

	/* Die temperature (°C × 1000 for milli-degrees) */
	int32_t die_temp_mdeg;

	/* Internal FIFO buffer for batch reads from device */
	uint32_t fifo_buf[MAX3010X_FIFO_DEPTH][MAX3010X_MAX_CHANNELS];
	uint8_t fifo_count;       /* Samples in buffer after last drain  */
	uint8_t fifo_read_idx;    /* Next sample to serve via channel_get */

	/* Channel mapping: map[LED_channel] = FIFO slot index.
	 * LED_channel: 0=Red, 1=IR, 2=Green.
	 * Value >= num_channels means channel is inactive. */
	uint8_t num_channels;
	uint8_t map[MAX3010X_MAX_CHANNELS];

	/* Power state */
	bool in_shutdown;
};

#endif /* DRIVERS_SENSOR_MAX3010X_PPG_H_ */
