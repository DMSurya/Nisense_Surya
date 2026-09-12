/* =============================================================================
 * Calibration Data Structures
 * =============================================================================
 * Production calibration data stored in external flash (calibration_model partition)
 * Location: 0x2F6000 in external QSPI flash (64 KB)
 * 
 * This data is written during production calibration and is READ-ONLY at runtime.
 * Never updated via OTA - only via factory programming.
 *
 * Target: nRF52840 (Raytac MDBT50Q-DB-40)
 * Created: 2026-02-14
 * ============================================================================= */

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants and Magic Numbers
 * ============================================================================= */

#define CALIB_MAGIC                 0x43414C42  /* "CALB" */
#define CALIB_VERSION               1
#define CALIB_PARTITION_SIZE        0x10000     /* 64 KB */

/* Partition offsets within calibration (base: 0x2F6000) */
#define CALIB_HEADER_OFFSET         0x0000      /* 256 bytes */
#define CALIB_PPG_OFFSET            0x0100      /* 4 KB */
#define CALIB_TEMP_OFFSET           0x1100      /* 2 KB */
#define CALIB_GLUCOSE_OFFSET        0x1900      /* 8 KB */
#define CALIB_ACCEL_OFFSET          0x3900      /* 2 KB */
#define CALIB_MAX32664_OFFSET       0x4100      /* 4 KB */
#define CALIB_DISPLAY_OFFSET        0x5100      /* 2 KB */
#define CALIB_RESERVED_OFFSET       0x5900      /* 38 KB reserved */

/* Status flags */
#define CALIB_FLAG_VALID            (1 << 0)
#define CALIB_FLAG_PRODUCTION       (1 << 1)
#define CALIB_FLAG_DEVELOPMENT      (1 << 2)

/* =============================================================================
 * Calibration Header (256 bytes)
 * ============================================================================= */

struct calibration_header {
    uint32_t magic;                 /* Must be CALIB_MAGIC */
    uint32_t version;               /* Structure version */
    uint32_t total_size;            /* Total calibration data size */
    uint32_t crc32;                 /* CRC32 of all data after header */
    uint32_t timestamp;             /* Unix timestamp of calibration */
    uint32_t flags;                 /* CALIB_FLAG_* */
    char     device_serial[24];     /* Production serial number */
    char     calibrated_by[16];     /* Operator ID / station */
    char     software_version[16];  /* Calibration software version */
    char     hardware_revision[8];  /* PCB revision */
    uint8_t  reserved[168];         /* Reserved for future use */
} __packed;

_Static_assert(sizeof(struct calibration_header) == 256, 
               "calibration_header must be 256 bytes");

/* =============================================================================
 * PPG / SpO2 / Heart Rate Calibration (4 KB)
 * ============================================================================= */

struct ppg_calibration {
    uint32_t magic;                     /* Section magic: 'PPG\0' */
    uint32_t version;
    
    /* SpO2 calibration */
    float    spo2_offset;               /* SpO2 percentage offset */
    float    spo2_scale;                /* SpO2 scaling factor */
    float    r_ratio_coefficients[4];   /* R-ratio polynomial: a0 + a1*R + a2*R^2 + a3*R^3 */
    
    /* Heart rate calibration */
    float    hr_offset_bpm;             /* HR BPM offset */
    float    hr_scale;                  /* HR scaling factor */
    
    /* LED current calibration (microamps) */
    uint16_t led_current_red_ua;        /* Red LED calibrated current */
    uint16_t led_current_ir_ua;         /* IR LED calibrated current */
    uint16_t led_current_green_ua;      /* Green LED calibrated current */
    uint16_t led_current_ambient_ua;    /* Ambient reference */
    
    /* Signal quality thresholds */
    uint16_t min_signal_amplitude;      /* Minimum valid signal */
    uint16_t max_signal_amplitude;      /* Maximum/saturation threshold */
    uint16_t motion_artifact_threshold; /* Accelerometer correlation */
    uint16_t perfusion_index_min;       /* Minimum PI for valid reading */
    
    /* Skin tone compensation */
    float    melanin_correction[3];     /* Correction factors for skin types */
    
    uint8_t  reserved[4028];            /* Pad to 4 KB */
} __packed;

_Static_assert(sizeof(struct ppg_calibration) == 4096, 
               "ppg_calibration must be 4096 bytes");

/* =============================================================================
 * Temperature Sensor Calibration (2 KB)
 * ============================================================================= */

struct temp_calibration {
    uint32_t magic;                     /* Section magic: 'TEMP' */
    uint32_t version;
    
    /* Linear calibration */
    float    offset_celsius;            /* Temperature offset */
    float    scale_factor;              /* Linear scale factor */
    
    /* Polynomial correction (for non-linearity) */
    float    poly_coefficients[5];      /* a0 + a1*T + a2*T^2 + a3*T^3 + a4*T^4 */
    
    /* Operating range */
    float    min_valid_celsius;         /* Minimum valid reading */
    float    max_valid_celsius;         /* Maximum valid reading */
    
    /* Ambient compensation */
    float    ambient_correction_slope;  /* Correction based on ambient temp */
    
    uint8_t  reserved[2000];            /* Pad to 2 KB */
} __packed;

_Static_assert(sizeof(struct temp_calibration) == 2048, 
               "temp_calibration must be 2048 bytes");

/* =============================================================================
 * Glucose Sensor Calibration (8 KB)
 * ============================================================================= */

struct glucose_calibration {
    uint32_t magic;                     /* Section magic: 'GLUC' */
    uint32_t version;
    
    /* Model identification */
    uint32_t model_id;                  /* Glucose model identifier */
    float    model_version;             /* Model version number */
    
    /* Wavelength calibration */
    float    wavelength_offset_nm[4];   /* Offset for each LED wavelength */
    float    wavelength_scale[4];       /* Scale for each LED wavelength */
    
    /* Calibration matrix (compact representation) */
    uint16_t matrix_rows;               /* Number of rows in calibration matrix */
    uint16_t matrix_cols;               /* Number of columns */
    float    calibration_factors[256];  /* Calibration matrix (or reference) */
    
    /* Operating parameters */
    float    min_glucose_mg_dl;         /* Minimum valid reading */
    float    max_glucose_mg_dl;         /* Maximum valid reading */
    float    measurement_interval_s;    /* Recommended interval */
    
    /* Temperature compensation */
    float    temp_compensation_slope;   /* Glucose vs temperature */
    float    temp_reference_celsius;    /* Reference temperature */
    
    uint8_t  reserved[7096];            /* Pad to 8 KB */
} __packed;

_Static_assert(sizeof(struct glucose_calibration) == 8192, 
               "glucose_calibration must be 8192 bytes");

/* =============================================================================
 * Accelerometer Calibration (2 KB)
 * ============================================================================= */

struct accel_calibration {
    uint32_t magic;                     /* Section magic: 'ACCL' */
    uint32_t version;
    
    /* Zero-g offset (millig) */
    int16_t  offset_x_mg;
    int16_t  offset_y_mg;
    int16_t  offset_z_mg;
    int16_t  reserved_align;
    
    /* Scale factors */
    float    scale_x;
    float    scale_y;
    float    scale_z;
    
    /* Cross-axis sensitivity matrix (3x3) */
    float    cross_axis[9];
    
    /* Operating parameters */
    float    noise_floor_mg;            /* Noise floor in millig */
    uint16_t sample_rate_hz;            /* Calibrated sample rate */
    uint16_t full_scale_g;              /* Full scale range */
    
    uint8_t  reserved[1976];            /* Pad to 2 KB */
} __packed;

_Static_assert(sizeof(struct accel_calibration) == 2048, 
               "accel_calibration must be 2048 bytes");

/* =============================================================================
 * MAX32664 AFE Configuration (4 KB)
 * ============================================================================= */

struct max32664_calibration {
    uint32_t magic;                     /* Section magic: 'M264' */
    uint32_t version;
    
    /* AFE register configuration */
    uint8_t  afe_config_mode;           /* 0=passthrough, 1=algorithm, 2=raw */
    uint8_t  sample_rate_hz;            /* 25, 50, 100, etc. */
    uint8_t  pulse_width_us;            /* LED pulse width */
    uint8_t  adc_range;                /* PPG Channel1 ADC input range setting */
    
    /* LED configuration */
    uint8_t  led1_current;              /* LED1 (Red) current register value */
    uint8_t  led2_current;              /* LED2 (IR) current register value */
    uint8_t  led3_current;              /* LED3 (Green) current register value */
    uint8_t  led_pilot;                 /* LED pilot current */
    
    /* Timing */
    uint16_t sample_average;            /* Number of samples to average */
    uint16_t fifo_almost_full;          /* FIFO almost full threshold */
    
    /* Algorithm parameters (if using MAX32664 internal algo) */
    float    spo2_a_coefficient;
    float    spo2_b_coefficient;
    float    spo2_c_coefficient;
    
    /* Firmware info */
    uint32_t fw_version;                /* Expected MAX32664 firmware version */
    
    uint8_t  reserved[4060];            /* Pad to 4 KB */
} __packed;

_Static_assert(sizeof(struct max32664_calibration) == 4096, 
               "max32664_calibration must be 4096 bytes");

/* =============================================================================
 * Display Calibration (2 KB)
 * ============================================================================= */

struct display_calibration {
    uint32_t magic;                     /* Section magic: 'DISP' */
    uint32_t version;
    
    /* Gamma correction (LUT per channel) */
    uint8_t  gamma_red[16];             /* Red channel gamma LUT (simplified) */
    uint8_t  gamma_green[16];           /* Green channel gamma LUT */
    uint8_t  gamma_blue[16];            /* Blue channel gamma LUT */
    
    /* White balance */
    uint8_t  white_balance_r;           /* Red multiplier (0-255) */
    uint8_t  white_balance_g;           /* Green multiplier */
    uint8_t  white_balance_b;           /* Blue multiplier */
    uint8_t  brightness_default;        /* Default brightness (0-255) */
    
    /* Display orientation */
    uint8_t  rotation_degrees;          /* 0, 90, 180, 270 */
    uint8_t  mirror_x;                  /* Horizontal mirror */
    uint8_t  mirror_y;                  /* Vertical mirror */
    uint8_t  reserved_align;
    
    uint8_t  reserved[1984];            /* Pad to 2 KB */
} __packed;

_Static_assert(sizeof(struct display_calibration) == 2048, 
               "display_calibration must be 2048 bytes");

/* =============================================================================
 * Complete Calibration Data Structure
 * ============================================================================= */

struct calibration_data {
    struct calibration_header     header;       /* 256 bytes */
    struct ppg_calibration        ppg;          /* 4 KB */
    struct temp_calibration       temp;         /* 2 KB */
    struct glucose_calibration    glucose;      /* 8 KB */
    struct accel_calibration      accel;        /* 2 KB */
    struct max32664_calibration   max32664;     /* 4 KB */
    struct display_calibration    display;      /* 2 KB */
    /* Total: 22.25 KB, leaving ~42 KB reserved in 64 KB partition */
};

/* =============================================================================
 * API Functions
 * ============================================================================= */

/**
 * @brief Initialize calibration module and verify data integrity
 * @return 0 on success, negative errno on failure
 */
int calibration_init(void);

/**
 * @brief Check if calibration data is valid
 * @return true if calibration data is present and valid
 */
bool calibration_is_valid(void);

/**
 * @brief Get pointer to calibration header
 * @return Pointer to header (memory-mapped) or NULL if invalid
 */
const struct calibration_header *calibration_get_header(void);

/**
 * @brief Get pointer to PPG calibration data
 * @return Pointer to PPG calibration or NULL if invalid
 */
const struct ppg_calibration *calibration_get_ppg(void);

/**
 * @brief Get pointer to temperature calibration data
 * @return Pointer to temp calibration or NULL if invalid
 */
const struct temp_calibration *calibration_get_temp(void);

/**
 * @brief Get pointer to glucose calibration data
 * @return Pointer to glucose calibration or NULL if invalid
 */
const struct glucose_calibration *calibration_get_glucose(void);

/**
 * @brief Get pointer to accelerometer calibration data
 * @return Pointer to accel calibration or NULL if invalid
 */
const struct accel_calibration *calibration_get_accel(void);

/**
 * @brief Get pointer to MAX32664 calibration data
 * @return Pointer to MAX32664 calibration or NULL if invalid
 */
const struct max32664_calibration *calibration_get_max32664(void);

/**
 * @brief Get pointer to display calibration data
 * @return Pointer to display calibration or NULL if invalid
 */
const struct display_calibration *calibration_get_display(void);

/**
 * @brief Print calibration summary to log
 */
void calibration_print_info(void);

/**
 * @brief Log once which factory sections will be applied (ppg/glucose/temp y/n)
 */
void calibration_log_apply_summary(void);

/**
 * @brief Apply PPG SpO2/HR linear trim when PPG section is valid
 *
 * No-op if calibration missing/invalid or scale is non-finite/zero.
 * Clamps SpO2 to 0–100 and HR to 0–300.
 */
void calibration_apply_ppg_hr_spo2(uint16_t *hr_bpm, uint16_t *spo2_pct);

/**
 * @brief Apply temperature linear trim when TEMP section is valid
 *
 * No-op if missing/invalid. Optionally clamps to min/max when both > 0 and ordered.
 */
void calibration_apply_temp_c(float *temp_c);

/**
 * @brief Apply glucose clamp and optional temp compensation
 *
 * @param glucose_mg_dl In/out glucose
 * @param skin_temp_c   Skin/wrist temp °C, or NaN to skip temp compensation
 */
void calibration_apply_glucose_mg_dl(float *glucose_mg_dl, float skin_temp_c);

/**
 * @brief Calculate CRC32 of calibration data
 * @param data Pointer to data
 * @param len Length of data
 * @return CRC32 value
 */
uint32_t calibration_crc32(const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CALIBRATION_H */
