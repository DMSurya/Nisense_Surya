/**
 * @file max32664.h
 * @brief MAX32664 sensor hub driver internal definitions
 *
 * Internal header for MAX32664 hub driver implementation.
 * This file should not be included by application code.
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_MAX32664_INTERNAL_H_
#define ZEPHYR_DRIVERS_SENSOR_MAX32664_INTERNAL_H_

#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

/* GPIO helper macro for checking if GPIO port is ready */
#define GPIO_PORT_READY(gpio_spec) \
	((gpio_spec).port != NULL && device_is_ready((gpio_spec).port))

#ifdef __cplusplus
extern "C" {
#endif

/* MAX32664 I2C Slave Addresses */
/* Note: Standard default address is 0x55 (7-bit) per MAX32664 datasheet.
 * MAX32664B and MAX32664C support selectable address via I2C command.
 * Actual address comes from device tree (reg property).
 */
#define MAX32664_SLAVE_ADDR_DEFAULT   			0x55  /* Standard default address */
#define MAX32664_SLAVE_ADDR_GWEC      			0x68  /* Example: U7 Version C (if address changed) */
#define MAX32664_SLAVE_ADDR_GWED      			0x66  /* Example: U9 Version D (if address changed) */

/* Hub status byte */
#define MAX32664_STATUS_SUCCESS       			0x00  /* No error */
#define MAX32664_STATUS_ERR_UNAVAIL_CMD			0x01  /* Illegal family byte and/or index byte */
#define MAX32664_STATUS_ERR_UNAVAIL_FUNC		0x02  /* Function not implemented */
#define MAX32664_STATUS_ERR_DATA_FORMAT			0x03  /* Data format error */
#define MAX32664_STATUS_ERR_INPUT_VALUE			0x04  /* Input value out of range */
#define MAX32664_STATUS_ERR_INVALID_MODE		0x05  /* Invalid mode for command */
#define MAX32664_STATUS_ERR_BTLDR_TRY_AGAIN		0x05  /* Bootloader busy, try again */
#define MAX32664_STATUS_ERR_BTLDR_GENERAL		0x80  /* Bootloader general error */
#define MAX32664_STATUS_ERR_BTLDR_CHECKSUM		0x81  /* Bootloader checksum error */
#define MAX32664_STATUS_ERR_BTLDR_AUTH			0x82  /* Bootloader authentication failure */
#define MAX32664_STATUS_ERR_BTLDR_INVALID_APP	0x83  /* Bootloader invalid application */
#define MAX32664_STATUS_ERR_TRY_AGAIN			0xFE  /* Device busy, try again later */
#define MAX32664_STATUS_ERR_UNKNOWN				0xFF  /* Unknown error */


/* Hub I2C Registers (from MAX32664 datasheet) */
#define MAX32664_REG_DEVICE_STATUS    			0x00  /* Boot status */
#define MAX32664_REG_FIFO_CONFIG      			0x09  /* FIFO mode */
#define MAX32664_REG_DATA_RDY_INTR    			0x0A  /* Interrupt config */
#define MAX32664_REG_SYSTEM_CONTROL   			0x0D  /* Power, reset */
#define MAX32664_REG_OUTPUT_FIFO_SIZE 			0x10  /* Samples ready */
#define MAX32664_REG_READ_DATA_OUTPUT 			0x12  /* Read hub-processed data */
#define MAX32664_REG_WRITE_REGISTER   			0x50  /* Write hub commands */
#define MAX32664_REG_READ_REGISTER    			0x51  /* Read hub registers */

/* MAX32664 MCU Type Register Values (Family 0xFF, Index 0x00)
 * NOTE: Can only distinguish MAX32625 vs MAX32660/MAX32664
 * CANNOT identify A/B/C/D variants - use AFE WHOAMI for variant detection
 */
#define MAX32664_MCU_TYPE_MAX32625    			0x00  /* MAX32625 */
#define MAX32664_MCU_TYPE_MAX32660_64 			0x01  /* MAX32660/MAX32664 (A/B/C/D variants) */

/* AFE Part ID / WHOAMI (hub Family 0x41 → AFE reg 0xFF)
 * Used to classify optical AFE family (GWEC vs GWED), not LED population.
 *
 * Datasheet Part ID (reg 0xFF):
 *   MAX86140 = 0x24, MAX86141 = 0x25, MAXM86161 = 0x36, MAXM86146 = 0x2E
 *   MAX30101 / MAX30102 / MAX30105 = 0x15 (identical — cannot distinguish by Part ID)
 *   MAX30100 = 0x11 (legacy; not a hub Variant A/D AFE)
 * Refs: MAX30101 DS, MAX30102 DS (PART_ID=0x15); MAX30100 DS (PART_ID=0x11).
 */
#define MAX32664_AFE_WHOAMI_MAX86140  			0x24  /* Variants B/C */
#define MAX32664_AFE_WHOAMI_MAX86141  			0x25  /* Variants B/C */
#define MAX32664_AFE_WHOAMI_MAX86161  			0x36  /* Variants B/C (MAXM module) */
#define MAX32664_AFE_WHOAMI_MAXM86146 			0x2E  /* Variants B/C (MAXM module) */
#define MAX32664_AFE_WHOAMI_MAX3010X  			0x15  /* MAX30101/30102/30105 family */
#define MAX32664_AFE_WHOAMI_MAX30101  			MAX32664_AFE_WHOAMI_MAX3010X
#define MAX32664_AFE_WHOAMI_MAX30102  			MAX32664_AFE_WHOAMI_MAX3010X /* same Part ID as 30101 */
#define MAX32664_AFE_WHOAMI_MAX30100  			0x11  /* Legacy MAX30100 only */

/* Accelerometer WHOAMI Register Values (Family 0x41, Index 0x04, Reg 0x0F)
 * KX-122: Register 0x0F returns 0x1D
 * LIS2DS12: Register 0x0F returns 0x43
 */
#define MAX32664_ACCEL_WHOAMI_KX122    			0x1D  /* Kionix accelerometer */
#define MAX32664_ACCEL_WHOAMI_LIS2DS12 			0x43  /* STMicroelectronics accelerometer */

/* Hub Variant Types (A/B/C/D per MAX32664 User Guide)
 * Detection via AFE WHOAMI + algorithm capabilities
 */
enum max32664_variant {
	MAX32664_VARIANT_UNKNOWN = 0,
	MAX32664_VARIANT_A = 1,      /* MaximFast: HR+SpO2, 100Hz, MAX30101, no accel required */
	MAX32664_VARIANT_B = 2,      /* WHRM: Wrist HR, 25Hz, MAX86140/141, KX-122 REQUIRED */
	MAX32664_VARIANT_C = 3,      /* WHRM+WSpO2 v3x: HR+SpO2, 25Hz, MAX86141, KX-122 REQUIRED */
	MAX32664_VARIANT_D = 4,      /* BPT: Blood Pressure+HR+SpO2, 100Hz, MAX30101, calibration REQUIRED */
};

/* Hub status byte (Family 0x00, Index 0x00) — per MAX32664 user guide
 * Err1/Err2 (bits 1–2): not used.
 * SCDMonSkin (bit 7): not available on some FW builds (e.g. 32.x, 35.x).
 */
#define MAX32664_STATUS_ERR0           			BIT(0)  /* 1 = sensor comm problem */
#define MAX32664_STATUS_DATA_RDY       			BIT(3)  /* DataRdyInt: FIFO ≥ threshold */
#define MAX32664_STATUS_FIFO_OUT_OVR   			BIT(4)  /* Sensor hub output FIFO overflow */
#define MAX32664_STATUS_FIFO_IN_OVR    			BIT(5)  /* Sensor hub input FIFO overflow */
#define MAX32664_STATUS_HOST_ACCEL_UF  			BIT(6)  /* Host accel feed underflow */
#define MAX32664_STATUS_SCD_MON_SKIN			BIT(7)  /* 1 = skin detected (if supported) */

/* AGC Algorithm Configuration Codes (Family 0x50, Index 0x00)
 * Used for Auto Gain Control tuning (variants A/D)
 */
#define MAX32664_AGC_PARAM_TARGET      			0x00  /* AGC target % of full-scale ADC (0-100) */
#define MAX32664_AGC_PARAM_STEP_SIZE   			0x01  /* AGC step size % (0-100) */
#define MAX32664_AGC_PARAM_SENSITIVITY 			0x02  /* AGC sensitivity % (0-100) */
#define MAX32664_AGC_PARAM_SAMPLES_AVG 			0x03  /* Samples to average (0-255) */

/* WHRM Algorithm Mode Configuration (Family 0x52, Index 0x03)
 * Used for Wrist Heart Rate Monitor mode selection (variants B/C)
 */
#define MAX32664_WHRM_MODE_DISABLED    			0x00  /* Algorithm disabled */
#define MAX32664_WHRM_MODE_BASIC       			0x01  /* Basic mode (no motion rejection) */
#define MAX32664_WHRM_MODE_STANDARD    			0x02  /* Standard motion rejection (needs KX-122) */
#define MAX32664_WHRM_MODE_AGGRESSIVE  			0x03  /* Aggressive motion rejection (needs KX-122) */

/* Hub Command Codes (from MAX32664 firmware interface) */
#define MAX32664_CMD_ENABLE_PPG       			0x44
#define MAX32664_CMD_ENABLE_ACCEL     			0x44
#define MAX32664_CMD_SET_SAMPLE_RATE  			0x44

struct max32664_raw_runtime {
	uint32_t read_count;
	uint32_t rejected_count;
	bool payload_logged;
	bool use_fifo_read_count_arg;
	uint8_t fifo_stagnant_reads;
	bool use_afe_fifo_fallback;
	uint8_t sensor_report_bytes;
	uint32_t prev_ir;
	uint32_t prev_red;
	bool prev_valid;
	bool leds_enabled; /**< Green+Red+IR PA on — mirrors max86141_ppg_leds_set() */
	/** Remaining FIFO frames to drop after LED wake (see Kconfig settle). */
	uint16_t settle_discard_left;
	uint16_t settle_discarded; /**< Frames dropped this session (for logs) */
};

/* Driver-specific data structure */
struct max32664_data {
	/** I2C bus specification */
	struct i2c_dt_spec bus;
	
	/** Interrupt GPIO specification (optional) */
	struct gpio_dt_spec int_gpio;

	/** Serializes every hub command (write+delay+read is a single
	 * critical section). Soft-recursive via bus_lock_depth so a caller
	 * can coarsen the lock across a multi-command sequence
	 * (see max32664_bus_lock) while max32664_i2c_transmit() still
	 * takes it per-transaction (Zephyr k_mutex is not recursive).
	 */
	struct k_mutex bus_mutex;
	k_tid_t bus_lock_owner;
	uint8_t bus_lock_depth;
	
	/** Hub variant */
	enum max32664_variant variant;
	
	/** Current sensor data from hub */
	struct {
		/* PPG data (both variants) */
		uint32_t ppg_ir;
		uint32_t ppg_red;
		uint32_t ppg_ambient;  /* GWEC only */
		
		/* Accelerometer (GWEC only) */
		int16_t accel_x;
		int16_t accel_y;
		int16_t accel_z;

		/* FIFO-embedded accel (RAW mode, Table 6 bytes 18-23: int16 MSB-first, 1 mg/LSB).
		 * Set by max32664_raw_read_ppg_ex() when payload_bytes >= 24.
		 * Consumed by lis2ds12_accel_sample_fetch() when valid. */
		bool fifo_accel_valid;
		int16_t fifo_accel_x_mg;
		int16_t fifo_accel_y_mg;
		int16_t fifo_accel_z_mg;
		
		/* Proximity/ALS (GWED only) */
		uint16_t prox_distance;
		uint16_t als_lux;
		
		/* Blood Pressure (Variant D BPT only) */
		uint16_t systolic_bp;   /* Systolic blood pressure (mmHg) */
		uint16_t diastolic_bp;  /* Diastolic blood pressure (mmHg) */
		uint8_t bp_progress;    /* BPT measurement progress (0-100%) */
		uint8_t bp_confidence;  /* BPT confidence (0-100) */
		
		/* Algorithm outputs */
		float spo2;             /* SpO2 percentage (0-100) */
		uint8_t spo2_confidence;/* SpO2 confidence (0-100) */
		int32_t heart_rate;     /* Heart rate in 0.1 BPM (divide by 10) */
		uint8_t hr_confidence;  /* HR confidence (0-100) */
		uint8_t signal_quality; /* Overall signal quality (0-100) */
		uint8_t scd_state;      /* Skin Contact Detection state (0=no contact) */
	} sample;
	
	/** Polling work queue */
	struct k_work_delayable poll_work;
	
	/** Interrupt work queue */
	struct k_work interrupt_work;
	
	/** GPIO callback */
	struct gpio_callback gpio_cb;
	
	/** Device pointer (for GPIO callback) */
	const struct device *dev;
	
	/** Sample ready semaphore */
	struct k_sem sample_sem;
	
	/** Configuration */
	struct {
		uint16_t poll_interval_ms;
		bool interrupt_mode;
		bool initialized;
	} config;
	
	/** Device state */
	bool sampling;
	uint32_t firmware_version;
	/**
	 * Runtime FIFO-entry size override. When non-zero, the common FIFO
	 * read path uses this value instead of the variant-ops default. Set
	 * by the output-mode helper when the hub is successfully placed in
	 * mode 0x03 (algorithm + sensor) so the larger frame is pulled in a
	 * single transaction. Reset to 0 whenever the hub falls back to
	 * mode 0x02 (algorithm-only) or PAUSE.
	 */
	size_t fifo_entry_size_runtime;
	/** When true, mode-0x03 parsing already forced a fallback to 0x02 this session */
	bool hub_mode03_fallback_done;
	uint8_t mcu_type;  /* MCU type register (A/B/C/D variant) */
	uint8_t afe_whoami;  /* AFE WHOAMI register (actual hardware detected) */
	uint8_t accel_whoami;  /* Accelerometer WHOAMI register (actual hardware) */
	bool afe_pre_enabled;  /* AFE already enabled by firmware (v40.x) */
	bool accel_required;   /* Accelerometer mandatory for this variant (B/C) */
	bool accel_enabled;    /* Accelerometer enable command succeeded */
	bool accel_verified;   /* Accelerometer WHOAMI confirmed (hardware present) */
	uint8_t pending_fifo_count; /* Count captured by poll worker for next sample_fetch */
	bool calibration_required;  /* BPT calibration mandatory (variant D) */
	bool calibration_done;      /* BPT calibration completed */

	/** RAW-mode session state (reset at each measurement boundary) */
	struct max32664_raw_runtime raw_runtime;
	
	/** AGC Configuration (variants A/D) */
	struct {
		uint8_t target_percent;    /* AGC target % (70-75 typical) */
		uint8_t step_size;         /* Step size % (10 typical) */
		uint8_t sensitivity;       /* Sensitivity % (50 typical) */
		uint8_t samples_avg;       /* Samples to average (10 typical) */
		bool configured;           /* AGC config applied */
	} agc;
};

/**
 * Optional DT variant-hint (maxim,max32664.yaml).
 * Maps board SKU family; compared against FW-major classification at init.
 */
enum max32664_variant_hint {
	MAX32664_HINT_NONE = 0, /**< No DT hint — FW major + probe only */
	MAX32664_HINT_GWEC,     /**< Wrist: expect Variant B or C */
	MAX32664_HINT_GWED,     /**< Finger: expect Variant A or D */
};

/** Driver-specific configuration structure */
struct max32664_config {
	struct i2c_dt_spec bus;
	struct gpio_dt_spec int_gpio;
	struct gpio_dt_spec rstn_gpio;  /* RSTN reset pin (active-low) */
	const struct device *vin_supply; /* Power supply regulator (for power cycle reset) */
	uint16_t poll_interval_ms;
	bool interrupt_mode;
	bool accel_external;  /* true = app feeds accel data, false = hub reads via SPI */
	enum max32664_variant_hint variant_hint;
};

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_SENSOR_MAX32664_INTERNAL_H_ */

