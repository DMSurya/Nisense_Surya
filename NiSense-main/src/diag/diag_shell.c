/**
 * @file diag_shell.c
 * @brief Diagnostics shell commands over USB CDC-ACM (VCOM).
 *
 * Transport: APP_FEATURE_SHELL (selects USB) → shell.conf + usb.conf.
 * Board DT provides CDC UART and zephyr,shell-uart. Logging stays on RTT.
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "record_store.h"
#include "config_manager.h"
#include "power_mgr.h"
#include "power_batt.h"
#include "git_version.h"

#include <zephyr/drivers/charger.h>
#include <zephyr/drivers/hwinfo.h>

#if defined(CONFIG_PPG_ALGO)
#include <zephyr/subsys/ppg_algo/ppg_algo.h>
#endif

static const char *rec_type_name(uint8_t type)
{
	switch (type) {
	case RECORD_TYPE_GLUCOSE:     return "glucose";
	case RECORD_TYPE_VITALS:      return "vitals";
	case RECORD_TYPE_TEMP:        return "temp";
	case RECORD_TYPE_HOME:        return "home";
	case RECORD_TYPE_PPG_RAW:     return "ppg_raw";
	case RECORD_TYPE_GLUCOSE_RAW: return "glucose_raw";
	default:                      return "?";
	}
}

static uint32_t rec_timestamp(uint8_t type, const uint8_t *payload, size_t len)
{
	if (payload == NULL || len < sizeof(uint32_t)) {
		return 0U;
	}

	switch (type) {
	case RECORD_TYPE_GLUCOSE:
		if (len >= sizeof(struct rec_glucose)) {
			return ((const struct rec_glucose *)payload)->timestamp;
		}
		break;
	case RECORD_TYPE_VITALS:
		if (len >= sizeof(struct rec_vitals)) {
			return ((const struct rec_vitals *)payload)->timestamp;
		}
		break;
	case RECORD_TYPE_TEMP:
		if (len >= sizeof(struct rec_temp)) {
			return ((const struct rec_temp *)payload)->timestamp;
		}
		break;
	default:
		break;
	}
	return 0U;
}

static uint32_t rec_parent_id(uint8_t type, const uint8_t *payload, size_t len)
{
	if (payload == NULL) {
		return 0U;
	}
	if (type == RECORD_TYPE_PPG_RAW && len >= sizeof(struct rec_ppg_raw_hdr)) {
		return ((const struct rec_ppg_raw_hdr *)payload)->parent_id;
	}
	if (type == RECORD_TYPE_GLUCOSE_RAW &&
	    len >= sizeof(struct rec_glucose_raw_hdr)) {
		return ((const struct rec_glucose_raw_hdr *)payload)->parent_id;
	}
	return 0U;
}

static void print_rec_detail(const struct shell *sh, const struct rec_frame_header *hdr,
			     const uint8_t *payload, size_t len)
{
	uint32_t parent = rec_parent_id((uint8_t)hdr->type, payload, len);

	shell_print(sh, "id=%u type=%s ver=%u len=%u ts=%u parent=%u",
		    hdr->record_id, rec_type_name((uint8_t)hdr->type), hdr->version,
		    hdr->len, rec_timestamp((uint8_t)hdr->type, payload, len), parent);

	switch (hdr->type) {
	case RECORD_TYPE_GLUCOSE:
		if (len >= sizeof(struct rec_glucose)) {
			const struct rec_glucose *g = (const struct rec_glucose *)payload;
			int mmol_x100 = (int)(g->normalized_glucose * 100.0f);
			int insulin_x100 = (int)(g->actual_insulin * 100.0f);
			int homa_x100 = (int)(g->homa_ir_index * 100.0f);

			shell_print(sh,
				    "  glucose=%u mg/dL q=%u var=%u mv=%u group=%d mmol=%d.%02d insulin=%d.%02d homa=%d.%02d",
				    g->glucose_mg_dl, g->quality, g->variant,
				    g->model_version, (int)g->group_cd,
				    mmol_x100 / 100, abs(mmol_x100 % 100),
				    insulin_x100 / 100, abs(insulin_x100 % 100),
				    homa_x100 / 100, abs(homa_x100 % 100));
		}
		break;
	case RECORD_TYPE_VITALS:
		if (len >= sizeof(struct rec_vitals)) {
			const struct rec_vitals *v = (const struct rec_vitals *)payload;

			shell_print(sh,
				    "  hr=%u spo2=%u%% hb=%u.%u g/dL resp=%u sdnn=%u rmssd=%u bp=%u/%u q=%u n=%u",
				    v->hr_bpm, v->spo2_percent,
				    v->hb_g_dl_x10 / 10U, v->hb_g_dl_x10 % 10U,
				    v->resp_rate_bpm, v->sdnn_ms, v->rmssd_ms,
				    v->systolic_mmhg, v->diastolic_mmhg, v->quality,
				    v->sample_count);
		}
		break;
	case RECORD_TYPE_TEMP:
		if (len >= sizeof(struct rec_temp)) {
			const struct rec_temp *t = (const struct rec_temp *)payload;

			shell_print(sh, "  soc=%d.%02d C skin=%d.%02d C band=%u src=%u",
				    t->soc_temp_c_x100 / 100, abs(t->soc_temp_c_x100 % 100),
				    t->skin_temp_c_x100 / 100, abs(t->skin_temp_c_x100 % 100),
				    t->skin_band, t->source);
		}
		break;
	case RECORD_TYPE_PPG_RAW:
		if (len >= sizeof(struct rec_ppg_raw_hdr)) {
			const struct rec_ppg_raw_hdr *h =
				(const struct rec_ppg_raw_hdr *)payload;

			shell_print(sh,
				    "  chunk %u/%u n=%u total=%u rate=%u flags=0x%04x",
				    h->chunk_index, h->chunk_count, h->n_in_chunk,
				    h->total_samples, h->sample_rate_hz, h->flags);
		}
		break;
	case RECORD_TYPE_GLUCOSE_RAW:
		if (len >= sizeof(struct rec_glucose_raw_hdr)) {
			const struct rec_glucose_raw_hdr *h =
				(const struct rec_glucose_raw_hdr *)payload;

			shell_print(sh, "  chunk %u/%u n=%u total=%u",
				    h->chunk_index, h->chunk_count, h->n_in_chunk,
				    h->total_samples);
		}
		break;
	default:
		break;
	}
}

static int cmd_rec_stats(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	struct record_store_stats st;

	record_store_get_stats(&st);
	shell_print(sh, "record store:");
	shell_print(sh, "  slots       : %u (%u free)", st.slot_count, st.free_slots);
	shell_print(sh, "  pending     : %u (summary=%u raw=%u)",
		    st.pending, st.pending_summary, st.pending_raw);
	shell_print(sh, "  next_id     : %u", st.next_record_id);
	shell_print(sh, "  head/tail   : %u / %u", st.head_slot, st.tail_slot);
	shell_print(sh, "  dropped     : %u", st.dropped);
	shell_print(sh, "  crc_fail    : %u", st.crc_fail_count);
	shell_print(sh, "  summary_ack : %u", st.summary_ack_upto);
	shell_print(sh, "  partition   : %u bytes (%u KB)",
		    st.partition_bytes, st.partition_bytes / 1024U);
	shell_print(sh, "  used/free   : %u / %u bytes (slot=%u)",
		    st.used_bytes, st.free_bytes, RECORD_SLOT_SIZE);
	return 0;
}

static int cmd_rec_count(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	struct record_store_stats st;

	record_store_get_stats(&st);
	shell_print(sh,
		    "count: pending=%u summary=%u raw=%u free_slots=%u capacity=%u next_id=%u dropped=%u",
		    st.pending, st.pending_summary, st.pending_raw, st.free_slots,
		    st.slot_count, st.next_record_id, st.dropped);
	return 0;
}

static int cmd_rec_size(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	struct record_store_stats st;

	record_store_get_stats(&st);
	shell_print(sh, "size: partition=%u B used=%u B free=%u B slot=%u B slots=%u",
		    st.partition_bytes, st.used_bytes, st.free_bytes,
		    RECORD_SLOT_SIZE, st.slot_count);
	return 0;
}

static int cmd_rec_dump(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t max = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : 20U;
	uint32_t after = 0;
	uint32_t shown = 0;

	if (max == 0U) {
		max = 20U;
	}

	while (shown < max) {
		struct rec_frame_header hdr;
		uint8_t payload[RECORD_PAYLOAD_MAX];
		size_t len = 0;

		if (record_store_read_next(after, &hdr, payload, &len) != 0) {
			break;
		}
		print_rec_detail(sh, &hdr, payload, len);
		after = hdr.record_id;
		shown++;
	}
	shell_print(sh, "(%u records)", shown);
	return 0;
}

static int cmd_rec_summary(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t max = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : 20U;
	uint32_t after = 0;
	uint32_t shown = 0;

	if (max == 0U) {
		max = 20U;
	}

	while (shown < max) {
		struct rec_frame_header hdr;
		uint8_t payload[RECORD_PAYLOAD_MAX];
		size_t len = 0;

		if (record_store_read_next(after, &hdr, payload, &len) != 0) {
			break;
		}
		after = hdr.record_id;
		if (!RECORD_TYPE_IS_SUMMARY(hdr.type)) {
			continue;
		}
		print_rec_detail(sh, &hdr, payload, len);
		shown++;
	}
	shell_print(sh, "(%u summary records)", shown);
	return 0;
}

static int cmd_rec_get(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t want_id;
	struct rec_frame_header hdr;
	uint8_t payload[RECORD_PAYLOAD_MAX];
	size_t len = 0;
	uint32_t after = 0;
	uint32_t raw_n = 0;

	if (argc < 2) {
		shell_error(sh, "usage: nisense rec get <record_id>");
		return -EINVAL;
	}

	want_id = (uint32_t)strtoul(argv[1], NULL, 10);
	if (want_id == 0U) {
		shell_error(sh, "record_id must be > 0");
		return -EINVAL;
	}

	if (record_store_read_id(want_id, &hdr, payload, &len) != 0) {
		shell_error(sh, "record id=%u not found", want_id);
		return -ENOENT;
	}

	print_rec_detail(sh, &hdr, payload, len);

	while (record_store_read_next(after, &hdr, payload, &len) == 0) {
		after = hdr.record_id;
		if (!RECORD_TYPE_IS_RAW(hdr.type)) {
			continue;
		}
		if (rec_parent_id((uint8_t)hdr.type, payload, len) != want_id) {
			continue;
		}
		print_rec_detail(sh, &hdr, payload, len);
		raw_n++;
	}
	shell_print(sh, "(%u raw chunks for parent %u)", raw_n, want_id);
	return 0;
}

static int cmd_rec_raw(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t want_id;
	uint32_t after = 0;
	uint32_t raw_n = 0;

	if (argc < 2) {
		shell_error(sh, "usage: nisense rec raw <parent_id>");
		return -EINVAL;
	}

	want_id = (uint32_t)strtoul(argv[1], NULL, 10);
	if (want_id == 0U) {
		shell_error(sh, "parent_id must be > 0");
		return -EINVAL;
	}

	while (true) {
		struct rec_frame_header hdr;
		uint8_t payload[RECORD_PAYLOAD_MAX];
		size_t len = 0;

		if (record_store_read_next(after, &hdr, payload, &len) != 0) {
			break;
		}
		after = hdr.record_id;
		if (!RECORD_TYPE_IS_RAW(hdr.type)) {
			continue;
		}
		if (rec_parent_id((uint8_t)hdr.type, payload, len) != want_id) {
			continue;
		}
		print_rec_detail(sh, &hdr, payload, len);
		raw_n++;
	}
	shell_print(sh, "raw chunks for parent %u: %u", want_id, raw_n);
	return 0;
}

static void export_glucose_csv(const struct shell *sh, const struct rec_glucose *g)
{
	char line[768];

	shell_print(sh,
		    "Timestamp_unix,Patient,Device_ID,Glucose_mg_dL,Quality,Variant,Model_Version,"
		    "Intercept,Outlier_K,Tot_Coeff,Y1,Avg,StdDev,UpLim,LlLim,P_Count,N_Count,"
		    "P_Val,N_Val,P_Plus_N,Y2_Val,Y2_Percent,Group_CD,Y2_Factor,Y2_Factor_Val,"
		    "Const_Val,Y3_Value,Y3_Row,Elim_Per,Elim_Val,Y_Value,Cal_Factor,AG_Adj,"
		    "Norm_Glucose,Insulin,Insulin_Corr,Insulin_Ratio,Inv_Ratio,HOMA_IR");
	snprintk(line, sizeof(line),
		 "%u,,%u,%u,%u,%u,%u,"
		 "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%d,"
		 "%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.6f,%.6f,"
		 "%.6f,%.6f,%d,%.6f,%.6f,%d,%.6f,%.6f,"
		 "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f",
		 g->timestamp, g->device_id, g->glucose_mg_dl, g->quality,
		 g->variant, g->model_version,
		 (double)g->intercept, (double)g->outlier_k, (double)g->tot_coeff,
		 (double)g->y1_value, (double)g->avg_val, (double)g->std_dev,
		 (double)g->up_lim, (double)g->ll_lim, (int)g->p_count, (int)g->n_count,
		 (double)g->p_val, (double)g->n_val, (double)g->p_plus_n,
		 (double)g->y2_val, (double)g->y2_percent, (int)g->group_cd,
		 (double)g->y2_factor, (double)g->y2_factor_val,
		 (double)g->const_val, (double)g->y3_value, (int)g->y3_row_no,
		 (double)g->elim_per, (double)g->elim_val, (int)g->y_value,
		 (double)g->calibration_factor, (double)g->ag_adjusted,
		 (double)g->normalized_glucose, (double)g->actual_insulin,
		 (double)g->insulin_correction, (double)g->insulin_ratio,
		 (double)g->inverse_ratio, (double)g->homa_ir_index);
	shell_print(sh, "%s", line);
}

static void export_vitals_csv(const struct shell *sh, const struct rec_vitals *v)
{
	float hb = (float)v->hb_g_dl_x10 / 10.0f;

	shell_print(sh,
		    "Timestamp_unix,Patient,Device_ID,HR_BPM,HR_Conf,SpO2_Pct,SpO2_Conf,"
		    "Hb_g_dL,Hb_Conf,Resp_BPM,Resp_Conf,SDNN_ms,RMSSD_ms,Sys_mmHg,Dia_mmHg,"
		    "Quality,SNR_dB_x10,Perf_x10,Sample_Rate_Hz,Sample_Count");
	shell_print(sh,
		    "%u,,%u,%u,%u,%u,%u,%.1f,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
		    v->timestamp, v->device_id, v->hr_bpm, v->hr_conf, v->spo2_percent,
		    v->spo2_conf, (double)hb, v->hb_conf, v->resp_rate_bpm, v->resp_conf,
		    v->sdnn_ms, v->rmssd_ms, v->systolic_mmhg, v->diastolic_mmhg,
		    v->quality, v->snr_db_x10, v->perfusion_index_x10,
		    v->sample_rate_hz, v->sample_count);
}

static void export_ppg_raw_csv(const struct shell *sh, uint32_t parent_id,
			       uint32_t ts_sec, uint16_t rate_hz)
{
	uint32_t after = 0;
	uint32_t sample_index = 0;
	uint16_t chunks_found = 0;
	uint16_t expect_chunks = 0;

	shell_print(sh,
		    "Timestamp_unix_ms,Sample_Index,IR,Red,Green,IR_DC,Red_DC,Green_DC,"
		    "IR_AC,Red_AC,Green_AC,Accel_X,Accel_Y,Accel_Z,parent_id");

	while (true) {
		struct rec_frame_header hdr;
		uint8_t payload[RECORD_PAYLOAD_MAX];
		size_t len = 0;
		const struct rec_ppg_raw_hdr *rh;
		const struct rec_ppg_sample *samples;

		if (record_store_read_next(after, &hdr, payload, &len) != 0) {
			break;
		}
		after = hdr.record_id;
		if (hdr.type != RECORD_TYPE_PPG_RAW ||
		    len < sizeof(struct rec_ppg_raw_hdr)) {
			continue;
		}
		rh = (const struct rec_ppg_raw_hdr *)payload;
		if (rh->parent_id != parent_id) {
			continue;
		}
		if (expect_chunks == 0U) {
			expect_chunks = rh->chunk_count;
		}
		chunks_found++;
		samples = (const struct rec_ppg_sample *)
			(payload + sizeof(struct rec_ppg_raw_hdr));
		for (uint16_t i = 0; i < rh->n_in_chunk; i++) {
			uint32_t ts_ms;

			if ((rh->flags & REC_PPG_RAW_FLAG_TIMESTAMP) != 0U) {
				ts_ms = samples[i].timestamp_unix * 1000U +
					samples[i].timestamp_ms;
			} else {
				ts_ms = ts_sec * 1000U;
				if (rate_hz > 0U) {
					ts_ms += (sample_index * 1000U) / rate_hz;
				}
			}
			shell_print(sh,
				    "%u,%u,%u,%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%u",
				    ts_ms, sample_index,
				    samples[i].ir, samples[i].red, samples[i].green,
				    samples[i].ir_dc, samples[i].red_dc, samples[i].green_dc,
				    samples[i].ir_ac, samples[i].red_ac, samples[i].green_ac,
				    samples[i].ax, samples[i].ay, samples[i].az,
				    parent_id);
			sample_index++;
		}
	}
	if (expect_chunks != 0U && chunks_found != expect_chunks) {
		shell_warn(sh,
			   "PPG_RAW incomplete for parent %u: found %u chunks, expected %u",
			   parent_id, chunks_found, expect_chunks);
	}
}

static void export_glucose_raw_csv(const struct shell *sh, uint32_t parent_id)
{
	uint32_t after = 0;
	uint32_t sample_index = 0;
	uint16_t chunks_found = 0;
	uint16_t expect_chunks = 0;

	shell_print(sh, "Timestamp_unix,Sample_Index,ADC,Voltage_mV,parent_id");

	while (true) {
		struct rec_frame_header hdr;
		uint8_t payload[RECORD_PAYLOAD_MAX];
		size_t len = 0;
		const struct rec_glucose_raw_hdr *rh;

		if (record_store_read_next(after, &hdr, payload, &len) != 0) {
			break;
		}
		after = hdr.record_id;
		if (hdr.type != RECORD_TYPE_GLUCOSE_RAW ||
		    len < sizeof(struct rec_glucose_raw_hdr)) {
			continue;
		}
		rh = (const struct rec_glucose_raw_hdr *)payload;
		if (rh->parent_id != parent_id) {
			continue;
		}
		if (expect_chunks == 0U) {
			expect_chunks = rh->chunk_count;
		}
		chunks_found++;
		for (uint16_t i = 0; i < rh->n_in_chunk; i++) {
			const uint8_t *slot = payload + sizeof(struct rec_glucose_raw_hdr) +
					     i * REC_GLUCOSE_RAW_SAMPLE_SIZE;
			uint16_t adc;
			int16_t mv_x10;
			uint32_t ts = 0;

			memcpy(&adc, slot, sizeof(adc));
			memcpy(&mv_x10, slot + sizeof(adc), sizeof(mv_x10));
			if (REC_GLUCOSE_RAW_SAMPLE_SIZE >= 8U) {
				memcpy(&ts, slot + 4, sizeof(ts));
			}
			shell_print(sh, "%u,%u,%u,%.1f,%u",
				    ts, sample_index, adc, (double)mv_x10 / 10.0, parent_id);
			sample_index++;
		}
	}
	if (expect_chunks != 0U && chunks_found != expect_chunks) {
		shell_warn(sh,
			   "GLUCOSE_RAW incomplete for parent %u: found %u chunks, expected %u",
			   parent_id, chunks_found, expect_chunks);
	}
}

static int cmd_rec_export(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t want_id;
	struct rec_frame_header hdr;
	uint8_t payload[RECORD_PAYLOAD_MAX];
	size_t len = 0;

	if (argc < 2) {
		shell_error(sh, "usage: nisense rec export <record_id>");
		return -EINVAL;
	}

	want_id = (uint32_t)strtoul(argv[1], NULL, 10);
	if (record_store_read_id(want_id, &hdr, payload, &len) != 0) {
		shell_error(sh, "record id=%u not found", want_id);
		return -ENOENT;
	}

	if (hdr.type == RECORD_TYPE_GLUCOSE && len >= sizeof(struct rec_glucose)) {
		shell_print(sh, "--- Glucose.csv ---");
		export_glucose_csv(sh, (const struct rec_glucose *)payload);
		shell_print(sh, "--- Glucose_Raw.csv ---");
		export_glucose_raw_csv(sh, want_id);
		return 0;
	}
	if (hdr.type == RECORD_TYPE_VITALS && len >= sizeof(struct rec_vitals)) {
		const struct rec_vitals *v = (const struct rec_vitals *)payload;

		shell_print(sh, "--- Vitals.csv ---");
		export_vitals_csv(sh, v);
		shell_print(sh, "--- PPG_Raw.csv ---");
		export_ppg_raw_csv(sh, want_id, v->timestamp, v->sample_rate_hz);
		return 0;
	}

	shell_error(sh, "export supports glucose/vitals summary ids only (got type=%s)",
		    rec_type_name((uint8_t)hdr.type));
	return -EINVAL;
}

static int cmd_rec_read(const struct shell *sh, size_t argc, char **argv)
{
	return cmd_rec_get(sh, argc, argv);
}

static int cmd_rec_ack(const struct shell *sh, size_t argc, char **argv)
{
	uint8_t mode = RECORD_ACK_MODE_FULL;
	uint32_t id;
	int rc;

	if (argc < 2) {
		shell_error(sh, "usage: nisense rec ack <record_id> [summary|full]");
		return -EINVAL;
	}
	id = (uint32_t)strtoul(argv[1], NULL, 10);
	if (argc >= 3) {
		if (strcmp(argv[2], "summary") == 0) {
			mode = RECORD_ACK_MODE_SUMMARY;
		} else if (strcmp(argv[2], "full") == 0) {
			mode = RECORD_ACK_MODE_FULL;
		} else {
			shell_error(sh, "mode must be 'summary' or 'full'");
			return -EINVAL;
		}
	}
	rc = record_store_ack_synced(id, mode);

	shell_print(sh, "acked up to id=%u mode=%s (rc=%d, pending=%u)", id,
		    mode == RECORD_ACK_MODE_SUMMARY ? "summary" : "full", rc,
		    record_store_pending());
	return rc;
}

static int cmd_rec_reset(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	int rc = record_store_factory_reset();

	shell_print(sh, "record store factory reset (rc=%d)", rc);
	return rc;
}

static int cmd_cfg_show(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	const struct app_config *cfg = config_manager_get();

	if (cfg == NULL) {
		shell_error(sh, "config manager unavailable");
		return -ENODEV;
	}

	shell_print(sh, "config (%s, persisted=%s):",
		    config_manager_get_path(),
		    config_manager_file_exists() ? "yes" : "no");
	shell_print(sh, "  device.name       : %s", cfg->device.name);
	shell_print(sh, "  device.brightness : %u", cfg->device.brightness);
	shell_print(sh, "  device.volume     : %u", cfg->device.volume);
	shell_print(sh, "  device.auto_sleep : %s",
		    cfg->device.auto_sleep ? "yes" : "no");
	shell_print(sh, "  device.sleep_s    : %u", cfg->device.sleep_timeout_s);
	shell_print(sh, "  logging.enabled   : %s",
		    cfg->logging.enabled ? "yes" : "no");
	shell_print(sh, "  wifi.enabled      : %s",
		    cfg->wifi.enabled ? "yes" : "no");
	shell_print(sh, "  wifi.ssid         : %s", cfg->wifi.ssid);
	shell_print(sh, "  ppg.sample_count  : %u", cfg->ppg.sample_count);
	shell_print(sh, "  ppg.gluc_samples  : %u", cfg->ppg.glucose_num_samples);
	shell_print(sh, "  ppg.gluc_delay_ms : %u", cfg->ppg.glucose_delay_ms);
	shell_print(sh, "  ppg.flags         : 0x%02x", cfg->ppg.flags);
	shell_print(sh, "  ppg.stream_decim  : %u", cfg->ppg.stream_decimate);
	shell_print(sh, "  ppg.health_auto   : %u", cfg->ppg.health_auto_enabled);
	shell_print(sh, "  ppg.health_int_s  : %u", cfg->ppg.health_interval_sec);
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(rec_cmds,
	SHELL_CMD(stats, NULL, "Stats: summary/raw pending + NOR sizes", cmd_rec_stats),
	SHELL_CMD(count, NULL, "Pending / free / capacity counts", cmd_rec_count),
	SHELL_CMD(size, NULL, "Partition used/free byte sizes", cmd_rec_size),
	SHELL_CMD_ARG(summary, NULL, "Dump [N] summary records (skip raw)", cmd_rec_summary, 1, 1),
	SHELL_CMD_ARG(get, NULL, "Summary <id> plus raw chunks", cmd_rec_get, 2, 0),
	SHELL_CMD_ARG(raw, NULL, "List raw chunks for parent <id>", cmd_rec_raw, 2, 0),
	SHELL_CMD_ARG(export, NULL, "CSV export for summary <id>", cmd_rec_export, 2, 0),
	SHELL_CMD_ARG(dump, NULL, "Dump [N] pending records (default 20)", cmd_rec_dump, 1, 1),
	SHELL_CMD_ARG(read, NULL, "Read one pending record by id", cmd_rec_read, 2, 0),
	SHELL_CMD_ARG(ack, NULL, "Ack synced up to <record_id> [summary|full]",
		      cmd_rec_ack, 2, 1),
	SHELL_CMD(reset, NULL, "Factory-reset the record store", cmd_rec_reset),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(cfg_cmds,
	SHELL_CMD(show, NULL, "Show persisted app config", cmd_cfg_show),
	SHELL_SUBCMD_SET_END
);

static int cmd_power(const struct shell *sh, size_t argc, char **argv)
{
	struct power_batt_snapshot snap;
	union charger_propval chg_val;
	const struct device *chg;
	int soc = 0;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (power_mgr_get_snapshot(&snap) != 0) {
		shell_error(sh, "power: snapshot failed");
		return -EIO;
	}

	(void)power_batt_soc_from_snapshot(&snap, &soc);

	shell_print(sh, "pack=%s usb=%s bat_good=%s bat_reg_done=%s sys_bat_lim=%s",
		    power_batt_cell_state_str(snap.cell_state),
		    snap.usb_online ? "yes" : "no",
		    snap.bat_good ? "yes" : "no",
		    snap.bat_reg_done ? "yes" : "no",
		    snap.sys_bat_lim ? "yes" : "no");
	shell_print(sh, "vbat=%ld mV vcell=%ld mV current=%ld mA soc=%d%%",
		    (long)(snap.pmic_vbat_uv / 1000),
		    (long)(snap.fg_vcell_uv / 1000),
		    (long)(snap.current_ua / 1000), soc);

	if (snap.fg_ext.valid) {
		shell_print(sh,
			    "fg: rem=%u full=%u design=%u mAh temp=%d C cycles=%u "
			    "tte=%u min ttf=%u min avg_i=%d mA",
			    snap.fg_ext.remaining_mah, snap.fg_ext.full_mah,
			    snap.fg_ext.design_mah, (int)snap.fg_ext.temp_c,
			    snap.fg_ext.cycle_count, snap.fg_ext.time_to_empty_min,
			    snap.fg_ext.time_to_full_min, (int)snap.fg_ext.avg_current_ma);
	} else {
		shell_print(sh, "fg: (no extended data)");
	}

	chg = power_batt_charger();
	if (chg != NULL && device_is_ready(chg)) {
		const char *status_str = "Unknown";

		if (charger_get_prop(chg, CHARGER_PROP_STATUS, &chg_val) == 0) {
			switch (chg_val.status) {
			case CHARGER_STATUS_CHARGING:
				status_str = "Charging";
				break;
			case CHARGER_STATUS_FULL:
				status_str = "Full";
				break;
			case CHARGER_STATUS_NOT_CHARGING:
				status_str = "Not charging";
				break;
			default:
				break;
			}
		}
		shell_print(sh, "charger: status=%s", status_str);

		if (charger_get_prop(chg, CHARGER_PROP_CONSTANT_CHARGE_VOLTAGE_UV,
				     &chg_val) == 0) {
			shell_print(sh, "charger: vset=%ld mV",
				    (long)(chg_val.const_charge_voltage_uv / 1000));
		}
		if (charger_get_prop(chg, CHARGER_PROP_CONSTANT_CHARGE_CURRENT_UA,
				     &chg_val) == 0) {
			shell_print(sh, "charger: iset=%ld mA",
				    (long)(chg_val.const_charge_current_ua / 1000));
		}
	} else {
		shell_print(sh, "charger: N/A");
	}

	return 0;
}

static int cmd_info(const struct shell *sh, size_t argc, char **argv)
{
	uint8_t dev_id[16];
	char hex[17] = {0};
	struct record_store_stats st;
	uint32_t up_sec;
	int ret;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "fw=%s git=%s", FW_VERSION_STRING, GIT_HASH);

	ret = hwinfo_get_device_id(dev_id, sizeof(dev_id));
	if (ret > 0) {
		int n = (ret < 8) ? ret : 8;

		for (int i = 0; i < n; i++) {
			snprintf(&hex[i * 2], sizeof(hex) - (size_t)(i * 2), "%02x",
				 dev_id[i]);
		}
		shell_print(sh, "id=%s", hex);
	} else {
		shell_print(sh, "id=N/A");
	}

	up_sec = (uint32_t)(k_uptime_get() / 1000);
	shell_print(sh, "uptime=%u s", up_sec);

#if defined(CONFIG_PPG_ALGO)
	shell_print(sh, "ppg=%s", ppg_algo_version());
#else
	shell_print(sh, "build=%.11s", __DATE__);
#endif

	record_store_get_stats(&st);
	shell_print(sh, "rec_pending=%u", st.pending);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(nisense_cmds,
	SHELL_CMD(rec, &rec_cmds, "NOR record store", NULL),
	SHELL_CMD(cfg, &cfg_cmds, "App configuration (NVS)", NULL),
	SHELL_CMD(power, NULL, "Battery / PMIC / charger dump", cmd_power),
	SHELL_CMD(info, NULL, "FW / device ID / uptime / records", cmd_info),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(nisense, &nisense_cmds, "NiSense diagnostics", NULL);
