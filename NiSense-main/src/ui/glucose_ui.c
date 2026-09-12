/* glucose_ui.c – Metabolic UI screen (BL-GLU / INS-RES / BL-HEM / HOMA-IR)
 *
 * Repurposed glucose result screen with product Metabolic labels.
 * Long-press BL-GLU still runs the glucose measurement path.
 */

#include "glucose.h"
#include "glucose_ui.h"
#include "glucose_logger.h"
#include "ui_common.h"
#include "ui_theme.h"
#include "ui_shell.h"
#include "resource.h"
#include "resource_icons.h"
#include "buzzer.h"
#include "home_last_results.h"
#if defined(CONFIG_APP_HEALTH_SCHED)
#include "health_sched.h"
#endif
#if defined(CONFIG_APP_FEATURE_PPG)
#include "ppg.h"
#include <zephyr/subsys/ppg_algo/ppg_algo.h>
#endif
#include "ui_events.h"
#include <lvgl.h>
#include "resource_fonts.h"
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor/glucose_algorithm.h>
#include <zephyr/drivers/sensor/glucose_algorithm_config.h>

LOG_MODULE_REGISTER(glucose_ui, CONFIG_GLUCOSE_UI_LOG_LEVEL);

/* UI screen elements */
static lv_obj_t *glucose_status_label;
static lv_obj_t *glucose_cards_cont;
static struct ns_card glucose_card;   /* BL-GLU (mg/dL) */
static struct ns_card insulin_card;   /* INS-RES (uIU/mL) */
static struct ns_card hb_card;        /* BL-HEM (g/dL) from last PPG */
static struct ns_card homa_card;      /* HOMA-IR index */
static lv_obj_t *glucose_result_label; /* alias: glucose_card.value_label */
static bool glucose_measuring = false;

/* Work queue for background glucose measurement (prevents UI freeze).
 * Stack size is tunable via CONFIG_GLUCOSE_UI_THREAD_STACK_SIZE. */
static K_THREAD_STACK_DEFINE(glucose_thread_stack,
			     CONFIG_GLUCOSE_UI_THREAD_STACK_SIZE);
static struct k_thread glucose_thread;
static k_tid_t glucose_thread_id;
static struct k_sem glucose_start_sem;
static void glucose_measurement_thread_fn(void *p1, void *p2, void *p3);

/* Deferred update mechanism for thread-safe LVGL access */
static struct glucose_sensor_result pending_result;
static struct glucose_algorithm_details pending_algo_details;
static struct glucose_sample_data pending_sample;  /* New: Per-sample data */
static bool pending_result_valid = false;
static volatile bool pending_result_update = false;
static volatile bool pending_sample_update = false;  /* New: Per-sample update flag */

/* Forward declarations */
static void update_glucose_screen(void);
static void glucose_ui_sample_callback(const struct glucose_sample_data *sample, void *user_data);
static void glucose_ui_result_callback(const struct glucose_sensor_result *result, void *user_data);

#if defined(CONFIG_GLUCOSE_SENSOR)
static enum ns_status glucose_card_status(float mg_dl)
{
	if (mg_dl < (float)CONFIG_APP_HEALTH_GLUCOSE_CRIT_LOW ||
	    mg_dl > (float)CONFIG_APP_HEALTH_GLUCOSE_CRIT_HIGH) {
		return NS_STATUS_CRITICAL;
	}
	if (mg_dl < (float)CONFIG_APP_HEALTH_GLUCOSE_ABN_LOW ||
	    mg_dl > (float)CONFIG_APP_HEALTH_GLUCOSE_ABN_HIGH) {
		return NS_STATUS_RISK;
	}
	return NS_STATUS_NORMAL;
}

static void glucose_ui_format_card_mg_dl(float mg_dl, char *buf, size_t len)
{
	int32_t mg_x10 = (int32_t)(mg_dl * 10.0f + 0.5f);

	snprintf(buf, len, "%ld.%ld",
		 (long)(mg_x10 / 10), (long)(mg_x10 % 10));
}

static void glucose_ui_apply_hb_card(void)
{
	char buf[40];
	uint16_t hb_x10 = 0U;
	bool hb_ok = false;

#if defined(CONFIG_APP_FEATURE_PPG)
	struct ppg_algo_result pr = {0};

	if (ppg_get_result(&pr) == 0 && pr.hb_g_dl_x10 > 0U) {
		hb_x10 = pr.hb_g_dl_x10;
		hb_ok = pr.hb_valid;
	}
#endif
	if (hb_x10 == 0U) {
		const struct home_last_results *lr = home_last_results_get();

		if (lr->loaded && lr->hb_g_dl_x10 > 0U) {
			hb_x10 = lr->hb_g_dl_x10;
			hb_ok = lr->hb_valid;
		}
	}

	if (!ui_is_obj_valid(hb_card.value_label, UI_SCREEN_GLUCOSE)) {
		return;
	}
	if (hb_x10 > 0U) {
		snprintf(buf, sizeof(buf), "%u.%u",
			 hb_x10 / 10U, hb_x10 % 10U);
		lv_label_set_text(hb_card.value_label, buf);
		ns_card_set_status(&hb_card,
				   hb_ok ? NS_STATUS_NORMAL : NS_STATUS_ATTENTION);
	} else {
		lv_label_set_text(hb_card.value_label, "--");
		ns_card_set_status(&hb_card, NS_STATUS_NEUTRAL);
	}
}

static void glucose_ui_apply_algo_to_cards(const struct glucose_algorithm_details *algo)
{
	if (algo == NULL) {
		if (ui_is_obj_valid(insulin_card.value_label, UI_SCREEN_GLUCOSE)) {
			lv_label_set_text(insulin_card.value_label, "--");
		}
		if (ui_is_obj_valid(homa_card.value_label, UI_SCREEN_GLUCOSE)) {
			lv_label_set_text(homa_card.value_label, "--");
		}
		return;
	}

	double homa_ir = algo->homa_ir_index;
	double fasting_insulin = algo->actual_insulin;
	char card_buf[40];

	double threshold_normal = HOMA_IR_INTERPRETATION_THRESHOLD_NORMAL;
	double threshold_moderate = HOMA_IR_INTERPRETATION_THRESHOLD_MODERATE;

	if (ui_is_obj_valid(insulin_card.value_label, UI_SCREEN_GLUCOSE)) {
		snprintf(card_buf, sizeof(card_buf), "%.1f",
			 (double)fasting_insulin);
		lv_label_set_text(insulin_card.value_label, card_buf);
		ns_card_set_status(&insulin_card,
			fasting_insulin < 10.0 ? NS_STATUS_NORMAL :
			fasting_insulin < 15.0 ? NS_STATUS_ATTENTION :
			fasting_insulin < 25.0 ? NS_STATUS_RISK : NS_STATUS_CRITICAL);
	}
	if (ui_is_obj_valid(homa_card.value_label, UI_SCREEN_GLUCOSE)) {
		snprintf(card_buf, sizeof(card_buf), "%.2f",
			 (double)homa_ir);
		lv_label_set_text(homa_card.value_label, card_buf);
		ns_card_set_status(&homa_card,
			homa_ir < threshold_normal ? NS_STATUS_NORMAL :
			homa_ir < threshold_moderate ? NS_STATUS_ATTENTION : NS_STATUS_CRITICAL);
	}
}

static void glucose_ui_apply_last_result(void);
static void glucose_ui_on_sched_event(const struct ui_event *ev, void *user)
{
	ARG_UNUSED(user);

	if (ev == NULL || ev->type != UI_EVENT_MEAS_RESULT_READY) {
		return;
	}
	if (ev->data.meas_type == UI_MEAS_GLUCOSE) {
		glucose_ui_apply_last_result();
	}
}

static void glucose_ui_ensure_events(void)
{
	static bool subscribed;

	if (!subscribed && ui_events_subscribe(glucose_ui_on_sched_event, NULL) == 0) {
		subscribed = true;
	}
}

static void glucose_ui_apply_last_result(void)
{
	float mg_dl = 0.0f;
	bool have = false;
	char buf[64];

	if (glucose_measuring || pending_result_update) {
		return;
	}
	if (!ui_is_obj_valid(glucose_result_label, UI_SCREEN_GLUCOSE)) {
		return;
	}

	struct glucose_sensor_result result;

	if (glucose_get_result(&result) == 0 &&
	    ((result.flags & GLUCOSE_SENSOR_FLAG_VALID) || result.glucose_mg_dl > 0.0f)) {
		mg_dl = result.glucose_mg_dl;
		have = (mg_dl > 0.0f);
	}

	if (!have) {
		const struct home_last_results *lr = home_last_results_get();

		if (lr->loaded && lr->glucose_mg_dl > 0U) {
			mg_dl = (float)lr->glucose_mg_dl;
			have = true;
		}
	}

	if (!have) {
		glucose_ui_apply_hb_card();
		return;
	}

	glucose_ui_format_card_mg_dl(mg_dl, buf, sizeof(buf));
	lv_label_set_text(glucose_result_label, buf);
	lv_obj_set_style_text_color(glucose_result_label, NS_COLOR_GLUCOSE, LV_PART_MAIN);
	ns_card_set_status(&glucose_card, glucose_card_status(mg_dl));

	struct glucose_sensor_result stored;

	if (glucose_get_result(&stored) == 0 &&
	    stored.algorithm_details != NULL) {
		glucose_ui_apply_algo_to_cards(stored.algorithm_details);
	} else {
		const struct home_last_results *lr = home_last_results_get();

		if (lr->loaded && (lr->insulin_valid || lr->homa_valid)) {
			struct glucose_algorithm_details pseudo = {0};

			if (lr->insulin_valid) {
				pseudo.actual_insulin = (double)lr->insulin_uiu_ml;
			}
			if (lr->homa_valid) {
				pseudo.homa_ir_index = (double)lr->homa_ir_index;
			}
			glucose_ui_apply_algo_to_cards(&pseudo);
		}
	}

	glucose_ui_apply_hb_card();
}
#endif /* CONFIG_GLUCOSE_SENSOR */

/**
 * @brief Dedicated thread for glucose measurement
 * Runs independently to prevent blocking UI thread
 */
static void glucose_measurement_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	
	while (1) {
		/* Wait for measurement trigger */
		k_sem_take(&glucose_start_sem, K_FOREVER);
		
		LOG_INF("Starting glucose measurement from UI...");
		int ret = glucose_start_measurement_from_ui();
		if (ret < 0) {
			LOG_ERR("Glucose measurement failed: %d", ret);
			glucose_measuring = false;
		}
	}
}

static void glucose_ui_set_idle_hint(void)
{
	if (glucose_measuring) {
		return;
	}
	if (ui_is_obj_valid(glucose_status_label, UI_SCREEN_GLUCOSE)) {
		lv_label_set_text(glucose_status_label, "Swipe to Measure to scan");
		lv_obj_set_style_text_color(glucose_status_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
	}
}

/**
 * @brief Start/stop glucose measurement (long-press on Glucose card).
 */

static void update_glucose_screen(void)
{
	if (ui_get_screen(UI_SCREEN_GLUCOSE) == NULL) {
		return;
	}
	
	int state = glucose_get_state();
	if (state >= 0 && glucose_status_label != NULL) {
		const char *status_text;
		lv_color_t status_color;
		static int last_logged_state = -1;  /* Track last logged state to avoid spam */
		
		/* Get dynamic num_samples from configuration */
		uint16_t num_samples = 80;  /* Default fallback */
		glucose_get_config(&num_samples, NULL);
		
		static char sampling_text[32];
		snprintf(sampling_text, sizeof(sampling_text), "ADC VALUES %u NOS", num_samples);
		
		switch (state) {
		case 0:  /* GLUCOSE_SENSOR_STATE_IDLE */
			status_text = "Status: Idle";
			status_color = NS_COLOR_TEXT;
			break;
		case 1:  /* GLUCOSE_SENSOR_STATE_LED_ON */
			status_text = "Reading Glucose...";
			status_color = NS_COLOR_INFO;
			if (last_logged_state != 1) {
				LOG_INF("Progress: Reading Glucose...");
				last_logged_state = 1;
			}
			break;
		case 2:  /* GLUCOSE_SENSOR_STATE_SAMPLING */
			status_text = sampling_text;
			status_color = NS_COLOR_INFO;
			if (last_logged_state != 2) {
				LOG_INF("Progress: ADC VALUES %u NOS", num_samples);
				last_logged_state = 2;
			}
			break;
		case 3:  /* GLUCOSE_SENSOR_STATE_ACQUISITION_COMPLETE */
			status_text = "DATA ACQUISITION COMPLETE";
			status_color = NS_COLOR_NORMAL;
			if (last_logged_state != 3) {
				LOG_INF("Progress: DATA ACQUISITION COMPLETE");
				last_logged_state = 3;
			}
			break;
		case 4:  /* GLUCOSE_SENSOR_STATE_PROCESSING */
			status_text = "GLUCOSE ALGORITHM IN PROCESS";
			status_color = NS_COLOR_INFO;
			if (last_logged_state != 4) {
				LOG_INF("Progress: GLUCOSE ALGORITHM IN PROCESS");
				last_logged_state = 4;
			}
			break;
		default:
			status_text = "Status: Unknown";
			status_color = NS_COLOR_RISK;
			last_logged_state = -1;
			break;
		}
		
		if (ui_is_obj_valid(glucose_status_label, UI_SCREEN_GLUCOSE)) {
			lv_label_set_text(glucose_status_label, status_text);
			lv_obj_set_style_text_color(glucose_status_label, status_color, LV_PART_MAIN);
		}
				
		/* Button text handled by long-press — status only */
		if (glucose_measuring) {
			/* keep status from state machine */
		} else {
			glucose_ui_set_idle_hint();
		}
	}
	
	/* Check for pending sample update (real-time per-sample display) */
	if (pending_sample_update) {
		pending_sample_update = false;
		
		/* Update status: "x/Y" */
		char status_buf[32];
		snprintf(status_buf, sizeof(status_buf), "Status: %u/%u",
			(unsigned)(pending_sample.sample_number + 1),
			(unsigned)pending_sample.total_samples);
		
		if (ui_is_obj_valid(glucose_status_label, UI_SCREEN_GLUCOSE)) {
			lv_label_set_text(glucose_status_label, status_buf);
			lv_obj_set_style_text_color(glucose_status_label, NS_COLOR_INFO, LV_PART_MAIN);
		}
		
		/* Display sample: "ADC raw(mV)" format in result label */
		char sample_buf[32];
		/* Convert voltage to integer for display: xxx.x mV */
		int32_t mv_x10 = (int32_t)(pending_sample.voltage_mv * 10.0f + 0.5f);
		snprintf(sample_buf, sizeof(sample_buf), "%04u(%ld.%ldmV)",
			(unsigned)pending_sample.raw_adc_value,
			(long)(mv_x10 / 10), (long)(mv_x10 % 10));
		
		if (ui_is_obj_valid(glucose_result_label, UI_SCREEN_GLUCOSE)) {
			lv_label_set_text(glucose_result_label, sample_buf);
			lv_obj_set_style_text_color(glucose_result_label, NS_COLOR_WARNING, LV_PART_MAIN);  /* sampling */
		}
	}
	
	/* Check for pending result update from callback (thread-safe flag check) */
	if (pending_result_update && pending_result_valid) {
		/* Clear flag before processing (atomic operation) */
		pending_result_update = false;
		
		char buf[48];
		if (pending_result.flags & GLUCOSE_SENSOR_FLAG_VALID) {
			int32_t glucose_mg_dl_x10 = (int32_t)(pending_result.glucose_mg_dl * 10.0f + 0.5f);
			int32_t glucose_mg_dl_int = glucose_mg_dl_x10 / 10;
			int32_t glucose_mg_dl_frac = glucose_mg_dl_x10 % 10;

			/* Get HOMA-IR and fasting insulin when algorithm details are present */
			if (pending_result.algorithm_details != NULL) {
				glucose_ui_apply_algo_to_cards(pending_result.algorithm_details);

				LOG_INF("Fasting Insulin: %.2f µIU/mL",
					(double)pending_result.algorithm_details->actual_insulin);
				LOG_INF("HOMA-IR Index: %.6f",
					(double)pending_result.algorithm_details->homa_ir_index);
			} else {
				glucose_ui_apply_algo_to_cards(NULL);
			}

			/* Glucose card value (compact — matches vitals HR/SpO2 density) */
			glucose_ui_format_card_mg_dl(pending_result.glucose_mg_dl, buf, sizeof(buf));
			
			/* Log final result to terminal */
			LOG_INF("Progress: RESULT = %ld.%ld mg/dL",
				(long)glucose_mg_dl_int, (long)glucose_mg_dl_frac);
			LOG_INF("=== GLUCOSE MEASUREMENT COMPLETE ===");
			
			/* Update LVGL objects (safe - running from main thread) */
			if (ui_is_obj_valid(glucose_result_label, UI_SCREEN_GLUCOSE)) {
				lv_label_set_text(glucose_result_label, buf);
				lv_obj_set_style_text_color(glucose_result_label, NS_COLOR_GLUCOSE, LV_PART_MAIN);
				ns_card_set_status(&glucose_card, NS_STATUS_NORMAL);
			}
			if (ui_is_obj_valid(glucose_status_label, UI_SCREEN_GLUCOSE)) {
				lv_label_set_text(glucose_status_label, "Complete");
				lv_obj_set_style_text_color(glucose_status_label, NS_COLOR_NORMAL, LV_PART_MAIN);
			}
		} else {
			/* Measurement failed */
			if (ui_is_obj_valid(glucose_result_label, UI_SCREEN_GLUCOSE)) {
				lv_label_set_text(glucose_result_label, "Error");
				lv_obj_set_style_text_color(glucose_result_label, NS_COLOR_CRITICAL, LV_PART_MAIN);
				ns_card_set_status(&glucose_card, NS_STATUS_CRITICAL);
			}
			if (ui_is_obj_valid(glucose_status_label, UI_SCREEN_GLUCOSE)) {
				lv_label_set_text(glucose_status_label, "Failed");
				lv_obj_set_style_text_color(glucose_status_label, NS_COLOR_CRITICAL, LV_PART_MAIN);
			}
		}
	}
	
#if defined(CONFIG_GLUCOSE_SENSOR)
	glucose_ui_apply_last_result();
#endif
}

/* Public API for UI screen management */
void glucose_ui_create_screen(lv_obj_t *screen)
{
	if (screen == NULL) {
		LOG_ERR("Failed to create glucose screen - screen is NULL");
		return;
	}
	
	/* Initialize semaphore and thread for background measurements (once) */
	static bool thread_initialized = false;
	if (!thread_initialized) {
		k_sem_init(&glucose_start_sem, 0, 1);
		glucose_thread_id = k_thread_create(&glucose_thread,
					    glucose_thread_stack,
					    K_THREAD_STACK_SIZEOF(glucose_thread_stack),
					    glucose_measurement_thread_fn,
					    NULL, NULL, NULL,
					    K_PRIO_PREEMPT(7), 0, K_NO_WAIT);
		k_thread_name_set(glucose_thread_id, "glucose_meas");
		thread_initialized = true;
	}

	glucose_ui_ensure_events();
	
	ns_apply_screen_bg(screen);
	
	/* Title */
	lv_obj_t *title = lv_label_create(screen);
	if (title == NULL) {
		LOG_ERR("Failed to create glucose title label - out of memory");
		ui_delete_screen(UI_SCREEN_GLUCOSE);
		return;
	}
	if (title != NULL) {
		lv_label_set_text(title, "Metabolic");
		lv_obj_set_style_text_font(title, UI_FONT_20, LV_PART_MAIN);
		lv_obj_set_style_text_color(title, NS_COLOR_SECONDARY, LV_PART_MAIN);
		lv_obj_align(title, LV_ALIGN_TOP_MID, 0, UI_SHELL_TOP_INSET + 6);
	}
	
	/* Status label */
	glucose_status_label = lv_label_create(screen);
	if (glucose_status_label == NULL) {
		LOG_ERR("Failed to create glucose status label - out of memory");
		ui_delete_screen(UI_SCREEN_GLUCOSE);
		return;
	}
	if (glucose_status_label != NULL) {
		lv_label_set_text(glucose_status_label, "Swipe to Measure");
		lv_obj_set_style_text_font(glucose_status_label, UI_FONT_14, LV_PART_MAIN);
		lv_obj_set_style_text_color(glucose_status_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
		lv_obj_align(glucose_status_label, LV_ALIGN_BOTTOM_MID, 0, -8);
	}

	/* 2×2 square tiles (like previous home) */
	glucose_cards_cont = ns_make_result_grid(screen);
	if (glucose_cards_cont == NULL) {
		LOG_ERR("Failed to create metabolic tile grid - out of memory");
		ui_delete_screen(UI_SCREEN_GLUCOSE);
		return;
	}

	ns_make_result_tile(glucose_cards_cont, "BL-GLU", NS_COLOR_GLUCOSE,
			    xip_icon(RESOURCE_ID_ICON_GLUCOSE), &glucose_card);
	ns_make_result_tile(glucose_cards_cont, "INS-RES", NS_COLOR_SECONDARY,
			    xip_icon(RESOURCE_ID_ICON_INSULIN), &insulin_card);
	ns_make_result_tile(glucose_cards_cont, "BL-HEM", NS_COLOR_HB,
			    xip_icon(RESOURCE_ID_ICON_HEMOGLOBIN), &hb_card);
	ns_make_result_tile(glucose_cards_cont, "HOMA-IR", NS_COLOR_PRIMARY,
			    xip_icon(RESOURCE_ID_ICON_HOMA), &homa_card);
	glucose_result_label = glucose_card.value_label;
	lv_label_set_text(glucose_card.value_label, "--");
	lv_label_set_text(insulin_card.value_label, "--");
	lv_label_set_text(hb_card.value_label, "--");
	lv_label_set_text(homa_card.value_label, "--");

	glucose_register_sample_callback(glucose_ui_sample_callback, NULL);
	glucose_register_result_callback(glucose_ui_result_callback, NULL);

	update_glucose_screen();
}

void glucose_ui_update_screen(void)
{
	update_glucose_screen();
}

void glucose_ui_cleanup_screen(void)
{
	/* Detach widget pointers only — glucose measurement may continue in background. */
	glucose_status_label = NULL;
	glucose_cards_cont = NULL;
	glucose_card = (struct ns_card){0};
	insulin_card = (struct ns_card){0};
	hb_card = (struct ns_card){0};
	homa_card = (struct ns_card){0};
	glucose_result_label = NULL;
	glucose_measuring = false;
	pending_result_update = false;
	pending_result_valid = false;
	pending_sample_update = false;

	glucose_register_sample_callback(NULL, NULL);
	(void)glucose_unregister_result_callback(glucose_ui_result_callback);
}

/**
 * @brief Update UI with per-sample data (called from glucose.c trigger handler)
 * Thread-safe: sets flag for update_glucose_screen() to process
 */
static void glucose_ui_sample_callback(const struct glucose_sample_data *sample, void *user_data)
{
	ARG_UNUSED(user_data);

	if (sample == NULL) {
		return;
	}
	
	/* Copy sample data to pending buffer (thread-safe) */
	memcpy(&pending_sample, sample, sizeof(struct glucose_sample_data));
	pending_sample_update = true;
}

/**
 * @brief Update UI with final measurement result (called from glucose.c trigger handler)
 * Thread-safe: sets flag for update_glucose_screen() to process
 */
static void glucose_ui_result_callback(const struct glucose_sensor_result *result, void *user_data)
{
	ARG_UNUSED(user_data);

	if (result == NULL) {
		return;
	}
	
	/* Copy result to pending buffer (thread-safe).
	 * algorithm_details is a pointer — deep-copy into pending_algo_details so the
	 * UI thread does not dereference driver-owned memory later.
	 */
	memcpy(&pending_result, result, sizeof(struct glucose_sensor_result));
	if (result->algorithm_details != NULL) {
		memcpy(&pending_algo_details, result->algorithm_details,
		       sizeof(pending_algo_details));
		pending_result.algorithm_details = &pending_algo_details;
	} else {
		pending_result.algorithm_details = NULL;
	}
	pending_result_valid = true;
	pending_result_update = true;
	glucose_measuring = false;
}

