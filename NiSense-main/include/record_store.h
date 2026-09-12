/* =============================================================================
 * NOR Record Store
 * =============================================================================
 * Append-only, wear-aware ring of fixed-size framed records on the external
 * QSPI `record_store` partition.
 *
 * Summaries (glucose / vitals / temp) hold clinical + analytics scalars.
 * Waveforms (PPG optics/DC/AC, glucose ADC) are stored as chunk records with
 * parent_id = summary record_id for AI export / BLE sync / CSV.
 *
 * measurement_id groups every record written during one health Measure cycle
 * (TEMP_PRE/POST + VITALS + PPG_RAW + GLUCOSE + GLUCOSE_RAW). Idle temp and
 * standalone single-sensor runs use measurement_id = 0. record_id remains the
 * NOR/BLE watermark for ACK reclaim.
 * ============================================================================= */

#ifndef RECORD_STORE_H
#define RECORD_STORE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <zephyr/toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RECORD_STORE_MAGIC     0x53434552U /* "RECS" */
#define RECORD_SLOT_SIZE       256U
#define RECORD_FRAME_HDR_SIZE  16U
#define RECORD_PAYLOAD_MAX     (RECORD_SLOT_SIZE - RECORD_FRAME_HDR_SIZE)
#define RECORD_SCHEMA_VERSION  3U

/** Typical PPG measurement footprint: 1 VITALS + ~125 PPG_RAW chunks (20 s @ 25 Hz). */
#define RECORD_STORE_PPG_FOOTPRINT_SLOTS 126U

/** ACK reclaim mode (wire-compatible with RECORD_SYNC_MODE_* / BLE_REC_MODE_*). */
#define RECORD_ACK_MODE_SUMMARY 0x01U
#define RECORD_ACK_MODE_FULL    0x02U

/** Record types. */
enum record_type {
	RECORD_TYPE_INVALID     = 0,
	RECORD_TYPE_GLUCOSE     = 1,
	RECORD_TYPE_VITALS      = 2,
	RECORD_TYPE_TEMP        = 3,
	RECORD_TYPE_HOME        = 4,
	RECORD_TYPE_PPG_RAW     = 5,
	RECORD_TYPE_GLUCOSE_RAW = 6,
};

#define RECORD_TYPE_IS_SUMMARY(t) \
	((t) == RECORD_TYPE_GLUCOSE || (t) == RECORD_TYPE_VITALS || \
	 (t) == RECORD_TYPE_TEMP || (t) == RECORD_TYPE_HOME)

#define RECORD_TYPE_IS_RAW(t) \
	((t) == RECORD_TYPE_PPG_RAW || (t) == RECORD_TYPE_GLUCOSE_RAW)

/** On-flash frame header (16 bytes) preceding each record payload. */
struct rec_frame_header {
	uint32_t magic;      /* RECORD_STORE_MAGIC */
	uint16_t type;       /* enum record_type */
	uint8_t  version;    /* RECORD_SCHEMA_VERSION */
	uint8_t  len;        /* payload length (<= RECORD_PAYLOAD_MAX) */
	uint32_t record_id;  /* monotonic, assigned on append */
	uint32_t crc32;      /* CRC32 (IEEE) over payload[0..len) */
} __packed;

/* ---- summary payloads --------------------------------------------------- */

/** Full glucose analytics summary (RECORD_TYPE_GLUCOSE). */
struct rec_glucose {
	uint32_t timestamp;
	uint32_t device_id;
	uint16_t glucose_mg_dl;
	uint8_t  quality;
	uint8_t  variant;
	uint32_t model_version;
	float    intercept;
	float    outlier_k;
	float    tot_coeff;
	float    y1_value;
	float    avg_val;
	float    std_dev;
	float    up_lim;
	float    ll_lim;
	int32_t  p_count;
	int32_t  n_count;
	float    p_val;
	float    n_val;
	float    p_plus_n;
	float    y2_val;
	float    y2_percent;
	int32_t  group_cd;
	float    y2_factor;
	float    y2_factor_val;
	float    const_val;
	float    y3_value;
	int32_t  y3_row_no;
	float    elim_per;
	float    elim_val;
	int32_t  y_value;
	float    calibration_factor;
	float    ag_adjusted;
	float    normalized_glucose;
	float    actual_insulin;
	float    insulin_correction;
	float    insulin_ratio;
	float    inverse_ratio;
	float    homa_ir_index;
	uint32_t measurement_id;    /* shared health-cycle id; 0 = none */
} __packed;

/** Vitals + vascular summary (RECORD_TYPE_VITALS). */
struct rec_vitals {
	uint32_t timestamp;
	uint32_t device_id;
	uint16_t hr_bpm;
	uint8_t  hr_conf;
	uint8_t  spo2_percent;
	uint8_t  spo2_conf;
	uint8_t  hb_conf;
	uint16_t hb_g_dl_x10;
	uint8_t  resp_rate_bpm;
	uint8_t  resp_conf;
	uint16_t sdnn_ms;
	uint16_t rmssd_ms;
	uint16_t systolic_mmhg;
	uint16_t diastolic_mmhg;
	uint8_t  quality;
	uint8_t  flags;             /* BIT0 hr,1 spo2,2 hb,3 resp,4 hrv,5 bp */
	uint16_t snr_db_x10;
	uint8_t  perfusion_index_x10;
	uint8_t  quality_reason;    /* PPG_QUALITY_REASON_* (was _pad0) */
	uint16_t sample_rate_hz;
	uint16_t sample_count;
	uint32_t measurement_id;    /* shared health-cycle id; 0 = none */
	/* Study hooks (ISO 80601-2-61 Arms) — appended; older readers ignore trailing bytes if size-checked */
	uint8_t  led_pa;
	uint8_t  motion_bin;
	uint8_t  skin_tone_bin;
	uint8_t  _pad1;
	uint16_t r_value_x1000;
	uint16_t ac_ir_amp;
} __packed;

/** Temperature snapshot (RECORD_TYPE_TEMP). */
struct rec_temp {
	uint32_t timestamp;
	uint32_t device_id;
	int16_t  soc_temp_c_x100;
	int16_t  skin_temp_c_x100;
	uint8_t  skin_band;
	uint8_t  source;
	uint32_t measurement_id;    /* shared health-cycle id; 0 = idle/none */
} __packed;

/* ---- raw chunk headers + packing ---------------------------------------- */

#define REC_PPG_RAW_FLAG_ACCEL      0x0001U
#define REC_PPG_RAW_FLAG_DC_AC      0x0002U
#define REC_PPG_RAW_FLAG_TIMESTAMP  0x0004U /* per-sample capture time present */

/**
 * Packed PPG sample (schema v3: 48 bytes).
 * Legacy schema v2 samples were 42 bytes (no timestamp_* fields).
 * Summary VITALS.timestamp = calculation-complete wall time;
 * raw timestamp_* = when that sample was captured.
 */
struct rec_ppg_sample {
	uint32_t ir;
	uint32_t red;
	uint32_t green;
	int32_t  ir_dc;
	int32_t  red_dc;
	int32_t  green_dc;
	int32_t  ir_ac;
	int32_t  red_ac;
	int32_t  green_ac;
	int16_t  ax;
	int16_t  ay;
	int16_t  az;
	uint32_t timestamp_unix; /**< Capture wall-clock seconds (RTC) */
	uint16_t timestamp_ms;   /**< 0–999 ms within timestamp_unix */
} __packed;

struct rec_ppg_raw_hdr {
	uint32_t parent_id;
	uint16_t sample_rate_hz;
	uint16_t total_samples;
	uint16_t chunk_index;
	uint16_t chunk_count;
	uint16_t n_in_chunk;
	uint16_t flags;
	uint32_t measurement_id;    /* same cycle id as parent summary */
} __packed;

struct rec_glucose_raw_hdr {
	uint32_t parent_id;
	uint16_t total_samples;
	uint16_t chunk_index;
	uint16_t chunk_count;
	uint16_t n_in_chunk;
	uint16_t _pad;
	uint32_t measurement_id;    /* same cycle id as parent summary */
} __packed;

/** Per-sample glucose raw (schema v3). Legacy was adc+mv only (4 B). */
struct rec_glucose_raw_sample {
	uint16_t adc;
	int16_t  mv_x10;
	uint32_t timestamp_unix; /**< Capture wall-clock seconds (RTC) */
} __packed;

#define REC_PPG_SAMPLE_SIZE       ((uint16_t)sizeof(struct rec_ppg_sample))
#define REC_PPG_RAW_HDR_SIZE      ((uint16_t)sizeof(struct rec_ppg_raw_hdr))
#define REC_PPG_SAMPLES_PER_CHUNK \
	((RECORD_PAYLOAD_MAX - REC_PPG_RAW_HDR_SIZE) / REC_PPG_SAMPLE_SIZE)

#define REC_GLUCOSE_RAW_SAMPLE_SIZE ((uint16_t)sizeof(struct rec_glucose_raw_sample))
#define REC_GLUCOSE_RAW_HDR_SIZE    ((uint16_t)sizeof(struct rec_glucose_raw_hdr))
#define REC_GLUCOSE_SAMPLES_PER_CHUNK \
	((RECORD_PAYLOAD_MAX - REC_GLUCOSE_RAW_HDR_SIZE) / REC_GLUCOSE_RAW_SAMPLE_SIZE)

/** Store statistics. */
struct record_store_stats {
	uint32_t next_record_id;
	uint32_t head_slot;
	uint32_t tail_slot;
	uint32_t slot_count;
	uint32_t pending;
	uint32_t dropped;
	uint32_t free_slots;
	uint32_t partition_bytes;
	uint32_t used_bytes;
	uint32_t free_bytes;
	uint32_t pending_summary;
	uint32_t pending_raw;
	uint32_t pending_glucose;
	uint32_t pending_vitals;
	uint32_t pending_temp;
	uint32_t pending_ppg_raw;
	uint32_t pending_glucose_raw;
	uint32_t summary_ack_upto; /**< SUMMARY ACK watermark (no space reclaim) */
	uint32_t crc_fail_count;   /**< CRC mismatches seen on read */
};

int record_store_init(void);

/**
 * @brief Allocate a new measurement_id for a health Measure cycle.
 *
 * Persists next_measurement_id in NVS meta. Call once at cycle start; stamp
 * the returned id into every record until @ref record_store_end_measurement.
 */
int record_store_begin_measurement(uint32_t *out_id);

/** Clear the active measurement_id (cycle DONE / cancel). */
void record_store_end_measurement(void);

/** Active cycle id, or 0 if none. */
uint32_t record_store_active_measurement_id(void);

int record_store_append(uint8_t type, const void *payload, size_t len, uint32_t *out_id);

int record_store_read_next(uint32_t after_id, struct rec_frame_header *hdr,
			   void *payload, size_t *out_len);

/**
 * @brief Read the record with exact @p record_id (scan pending ring).
 * @return 0 on success, -ENOENT if not found.
 */
int record_store_read_id(uint32_t record_id, struct rec_frame_header *hdr,
			 void *payload, size_t *out_len);

/**
 * @brief Next pending record with id > after_id and matching @p type
 *        (or any type if type == RECORD_TYPE_INVALID).
 */
int record_store_read_next_type(uint32_t after_id, uint8_t type,
				struct rec_frame_header *hdr,
				void *payload, size_t *out_len);

/**
 * @brief Most recent record of @p type (scan newest→oldest in the pending ring).
 * @return 0 on success, -ENOENT if none.
 */
int record_store_read_latest_type(uint8_t type, struct rec_frame_header *hdr,
				  void *payload, size_t *out_len);

/**
 * @brief Mark records synced through @p up_to_id.
 *
 * @param up_to_id Highest record_id covered by the host.
 * @param mode     RECORD_ACK_MODE_SUMMARY — update summary watermark only
 *                 (never advances ring tail / never reclaims raw).
 *                 RECORD_ACK_MODE_FULL — advance tail (space reclaim).
 */
int record_store_ack_synced(uint32_t up_to_id, uint8_t mode);

void record_store_get_stats(struct record_store_stats *stats);

uint32_t record_store_pending(void);

int record_store_factory_reset(void);

void record_store_lock(void);
void record_store_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* RECORD_STORE_H */
