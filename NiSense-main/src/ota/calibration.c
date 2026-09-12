/* =============================================================================
 * Calibration Data Implementation
 * =============================================================================
 * Production calibration data stored in external flash (calibration_model partition)
 * Location: 0x2F6000 in external QSPI flash (64 KB)
 * 
 * Target: nRF52840 (Raytac MDBT50Q-DB-40)
 * SDK: nRF Connect SDK v3.0.0
 * Created: 2026-02-14
 * ============================================================================= */

#include "calibration.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/crc.h>
#include <math.h>

LOG_MODULE_REGISTER(calibration, CONFIG_CALIBRATION_LOG_LEVEL);

/* =============================================================================
 * Memory Map Constants
 * ============================================================================= */

/** QSPI XIP base address on nRF52840 */
#define RESOURCE_MMAP_BASE            0x12000000

/** Calibration partition offset in external flash */
#define CALIB_PARTITION_OFFSET      0x2F6000

/** Calibration data XIP base address */
#define CALIB_BASE_ADDRESS          (RESOURCE_MMAP_BASE + CALIB_PARTITION_OFFSET)

/* =============================================================================
 * Private Data
 * ============================================================================= */

/** Cached pointer to calibration data (XIP memory-mapped) */
static const struct calibration_data *calib_data;

/** Flag indicating if calibration is valid */
static bool calib_valid;

/** Flag indicating if module is initialized */
static bool calib_initialized;

/* Section magic numbers for validation */
#define PPG_MAGIC       0x00475050  /* "PPG\0" */
#define TEMP_MAGIC      0x504D4554  /* "TEMP" */
#define GLUC_MAGIC      0x43554C47  /* "GLUC" */
#define ACCL_MAGIC      0x4C434341  /* "ACCL" */
#define M264_MAGIC      0x3436324D  /* "M264" */
#define DISP_MAGIC      0x50534944  /* "DISP" */

/* =============================================================================
 * Public API
 * ============================================================================= */

int calibration_init(void)
{
    LOG_INF("Initializing calibration from 0x%08x", CALIB_BASE_ADDRESS);

    /* Point to XIP memory-mapped calibration data */
    calib_data = (const struct calibration_data *)CALIB_BASE_ADDRESS;
    calib_initialized = true;

    /* Validate main header magic */
    if (calib_data->header.magic != CALIB_MAGIC) {
        LOG_WRN("Calibration magic invalid: 0x%08x (expected 0x%08x)",
                calib_data->header.magic, CALIB_MAGIC);
        
        if (calib_data->header.magic == 0xFFFFFFFF) {
            LOG_WRN("Calibration partition appears erased");
        }
        
        calib_valid = false;
        return -ENOENT;
    }

    /* Validate version */
    if (calib_data->header.version != CALIB_VERSION) {
        LOG_WRN("Calibration version mismatch: %u (expected %u)",
                calib_data->header.version, CALIB_VERSION);
        /* Continue anyway - may be backwards compatible */
    }

    /* Validate section magics */
    bool sections_ok = true;
    
    if (calib_data->ppg.magic != PPG_MAGIC) {
        LOG_WRN("PPG section magic invalid: 0x%08x", calib_data->ppg.magic);
        sections_ok = false;
    }
    if (calib_data->temp.magic != TEMP_MAGIC) {
        LOG_WRN("TEMP section magic invalid: 0x%08x", calib_data->temp.magic);
        sections_ok = false;
    }
    if (calib_data->glucose.magic != GLUC_MAGIC) {
        LOG_WRN("GLUC section magic invalid: 0x%08x", calib_data->glucose.magic);
        sections_ok = false;
    }
    if (calib_data->accel.magic != ACCL_MAGIC) {
        LOG_WRN("ACCL section magic invalid: 0x%08x", calib_data->accel.magic);
        sections_ok = false;
    }
    if (calib_data->max32664.magic != M264_MAGIC) {
        LOG_WRN("M264 section magic invalid: 0x%08x", calib_data->max32664.magic);
        sections_ok = false;
    }
    if (calib_data->display.magic != DISP_MAGIC) {
        LOG_WRN("DISP section magic invalid: 0x%08x", calib_data->display.magic);
        sections_ok = false;
    }

    if (!sections_ok) {
        LOG_WRN("Some calibration sections invalid - using partial data");
    }

    /* Mark as valid */
    calib_valid = true;

    LOG_INF("Calibration initialized: serial=%s, flags=0x%x",
            calib_data->header.device_serial[0] ? calib_data->header.device_serial : "N/A",
            calib_data->header.flags);

    return 0;
}

bool calibration_is_valid(void)
{
    return calib_initialized && calib_valid;
}

const struct calibration_header *calibration_get_header(void)
{
    if (!calib_valid || !calib_data) {
        return NULL;
    }
    return &calib_data->header;
}

const struct ppg_calibration *calibration_get_ppg(void)
{
    if (!calib_valid || !calib_data) {
        return NULL;
    }
    
    /* Validate section magic */
    if (calib_data->ppg.magic != PPG_MAGIC) {
        return NULL;
    }
    
    return &calib_data->ppg;
}

const struct temp_calibration *calibration_get_temp(void)
{
    if (!calib_valid || !calib_data) {
        return NULL;
    }
    
    if (calib_data->temp.magic != TEMP_MAGIC) {
        return NULL;
    }
    
    return &calib_data->temp;
}

const struct glucose_calibration *calibration_get_glucose(void)
{
    if (!calib_valid || !calib_data) {
        return NULL;
    }
    
    if (calib_data->glucose.magic != GLUC_MAGIC) {
        return NULL;
    }
    
    return &calib_data->glucose;
}

const struct accel_calibration *calibration_get_accel(void)
{
    if (!calib_valid || !calib_data) {
        return NULL;
    }
    
    if (calib_data->accel.magic != ACCL_MAGIC) {
        return NULL;
    }
    
    return &calib_data->accel;
}

const struct max32664_calibration *calibration_get_max32664(void)
{
    if (!calib_valid || !calib_data) {
        return NULL;
    }
    
    if (calib_data->max32664.magic != M264_MAGIC) {
        return NULL;
    }
    
    return &calib_data->max32664;
}

const struct display_calibration *calibration_get_display(void)
{
    if (!calib_valid || !calib_data) {
        return NULL;
    }
    
    if (calib_data->display.magic != DISP_MAGIC) {
        return NULL;
    }
    
    return &calib_data->display;
}

void calibration_print_info(void)
{
    LOG_INF("========== Calibration Info ==========");
    LOG_INF("Base Address: 0x%08x", CALIB_BASE_ADDRESS);
    LOG_INF("Initialized:  %s", calib_initialized ? "yes" : "no");
    LOG_INF("Valid:        %s", calib_valid ? "yes" : "no");

    if (!calib_valid || !calib_data) {
        LOG_INF("No valid calibration data");
        LOG_INF("======================================");
        return;
    }

    LOG_INF("--- Header ---");
    LOG_INF("Magic:     0x%08x", calib_data->header.magic);
    LOG_INF("Version:   %u", calib_data->header.version);
    LOG_INF("Size:      %u bytes", calib_data->header.total_size);
    LOG_INF("CRC32:     0x%08x", calib_data->header.crc32);
    LOG_INF("Timestamp: %u", calib_data->header.timestamp);
    LOG_INF("Flags:     0x%x", calib_data->header.flags);
    
    if (calib_data->header.device_serial[0]) {
        LOG_INF("Serial:    %.24s", calib_data->header.device_serial);
    }
    if (calib_data->header.calibrated_by[0]) {
        LOG_INF("Calib By:  %.16s", calib_data->header.calibrated_by);
    }
    if (calib_data->header.hardware_revision[0]) {
        LOG_INF("HW Rev:    %.8s", calib_data->header.hardware_revision);
    }

    LOG_INF("--- Sections ---");
    LOG_INF("PPG:     magic=0x%08x v%u %s", 
            calib_data->ppg.magic, calib_data->ppg.version,
            calib_data->ppg.magic == PPG_MAGIC ? "OK" : "INVALID");
    LOG_INF("TEMP:    magic=0x%08x v%u %s",
            calib_data->temp.magic, calib_data->temp.version,
            calib_data->temp.magic == TEMP_MAGIC ? "OK" : "INVALID");
    LOG_INF("GLUCOSE: magic=0x%08x v%u %s",
            calib_data->glucose.magic, calib_data->glucose.version,
            calib_data->glucose.magic == GLUC_MAGIC ? "OK" : "INVALID");
    LOG_INF("ACCEL:   magic=0x%08x v%u %s",
            calib_data->accel.magic, calib_data->accel.version,
            calib_data->accel.magic == ACCL_MAGIC ? "OK" : "INVALID");
    LOG_INF("M32664:  magic=0x%08x v%u %s",
            calib_data->max32664.magic, calib_data->max32664.version,
            calib_data->max32664.magic == M264_MAGIC ? "OK" : "INVALID");
    LOG_INF("DISPLAY: magic=0x%08x v%u %s",
            calib_data->display.magic, calib_data->display.version,
            calib_data->display.magic == DISP_MAGIC ? "OK" : "INVALID");

    /* Print some key calibration values if valid */
    if (calib_data->ppg.magic == PPG_MAGIC) {
        LOG_INF("--- PPG Values ---");
        LOG_INF("SpO2 offset: %.2f, scale: %.3f",
                (double)calib_data->ppg.spo2_offset,
                (double)calib_data->ppg.spo2_scale);
        LOG_INF("HR offset: %.1f BPM", (double)calib_data->ppg.hr_offset_bpm);
        LOG_INF("LED currents: R=%u, IR=%u, G=%u uA",
                calib_data->ppg.led_current_red_ua,
                calib_data->ppg.led_current_ir_ua,
                calib_data->ppg.led_current_green_ua);
    }

    if (calib_data->temp.magic == TEMP_MAGIC) {
        LOG_INF("--- Temp Values ---");
        LOG_INF("Offset: %.2f C, Scale: %.4f",
                (double)calib_data->temp.offset_celsius,
                (double)calib_data->temp.scale_factor);
        LOG_INF("Valid range: %.1f to %.1f C",
                (double)calib_data->temp.min_valid_celsius,
                (double)calib_data->temp.max_valid_celsius);
    }

    LOG_INF("======================================");
}

void calibration_log_apply_summary(void)
{
	bool ppg = (calibration_get_ppg() != NULL);
	bool glucose = (calibration_get_glucose() != NULL);
	bool temp = (calibration_get_temp() != NULL);

	LOG_INF("cal applied: ppg=%s glucose=%s temp=%s",
		ppg ? "y" : "n", glucose ? "y" : "n", temp ? "y" : "n");
}

void calibration_apply_ppg_hr_spo2(uint16_t *hr_bpm, uint16_t *spo2_pct)
{
	const struct ppg_calibration *ppg = calibration_get_ppg();
	float hr;
	float spo2;

	if (ppg == NULL) {
		return;
	}

	if (hr_bpm != NULL) {
		hr = (float)*hr_bpm;
		if (isfinite(ppg->hr_scale) && ppg->hr_scale != 0.0f) {
			hr = hr * ppg->hr_scale;
		}
		if (isfinite(ppg->hr_offset_bpm)) {
			hr += ppg->hr_offset_bpm;
		}
		if (hr < 0.0f) {
			hr = 0.0f;
		} else if (hr > 300.0f) {
			hr = 300.0f;
		}
		*hr_bpm = (uint16_t)(hr + 0.5f);
	}

	if (spo2_pct != NULL) {
		spo2 = (float)*spo2_pct;
		if (isfinite(ppg->spo2_scale) && ppg->spo2_scale != 0.0f) {
			spo2 = spo2 * ppg->spo2_scale;
		}
		if (isfinite(ppg->spo2_offset)) {
			spo2 += ppg->spo2_offset;
		}
		if (spo2 < 0.0f) {
			spo2 = 0.0f;
		} else if (spo2 > 100.0f) {
			spo2 = 100.0f;
		}
		*spo2_pct = (uint16_t)(spo2 + 0.5f);
	}
}

void calibration_apply_temp_c(float *temp_c)
{
	const struct temp_calibration *temp = calibration_get_temp();
	float t;

	if (temp_c == NULL || temp == NULL || !isfinite(*temp_c)) {
		return;
	}

	t = *temp_c;
	if (isfinite(temp->scale_factor) && temp->scale_factor != 0.0f) {
		t *= temp->scale_factor;
	}
	if (isfinite(temp->offset_celsius)) {
		t += temp->offset_celsius;
	}

	if (isfinite(temp->min_valid_celsius) && isfinite(temp->max_valid_celsius) &&
	    temp->min_valid_celsius < temp->max_valid_celsius &&
	    temp->min_valid_celsius > -50.0f && temp->max_valid_celsius < 100.0f) {
		if (t < temp->min_valid_celsius) {
			t = temp->min_valid_celsius;
		} else if (t > temp->max_valid_celsius) {
			t = temp->max_valid_celsius;
		}
	}

	*temp_c = t;
}

void calibration_apply_glucose_mg_dl(float *glucose_mg_dl, float skin_temp_c)
{
	const struct glucose_calibration *g = calibration_get_glucose();
	float v;

	if (glucose_mg_dl == NULL || g == NULL || !isfinite(*glucose_mg_dl)) {
		return;
	}

	v = *glucose_mg_dl;

	if (isfinite(skin_temp_c) &&
	    isfinite(g->temp_compensation_slope) &&
	    isfinite(g->temp_reference_celsius) &&
	    g->temp_compensation_slope != 0.0f) {
		v += g->temp_compensation_slope *
		     (skin_temp_c - g->temp_reference_celsius);
	}

	if (isfinite(g->min_glucose_mg_dl) && isfinite(g->max_glucose_mg_dl) &&
	    g->min_glucose_mg_dl > 0.0f &&
	    g->max_glucose_mg_dl > g->min_glucose_mg_dl &&
	    g->max_glucose_mg_dl <= 600.0f) {
		if (v < g->min_glucose_mg_dl) {
			v = g->min_glucose_mg_dl;
		} else if (v > g->max_glucose_mg_dl) {
			v = g->max_glucose_mg_dl;
		}
	}

	if (v < 0.0f) {
		v = 0.0f;
	} else if (v > 600.0f) {
		v = 600.0f;
	}

	*glucose_mg_dl = v;
}

uint32_t calibration_crc32(const void *data, size_t len)
{
    return crc32_ieee((const uint8_t *)data, len);
}
