/* buzzer_ui.c - Buzzer Control UI Screen
 *
 * Provides UI for playing different buzzer tones with play/stop buttons.
 */

#include "buzzer.h"
#include "ui_common.h"
#include "ui_theme.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include "resource_fonts.h"
#include <stdio.h>

LOG_MODULE_REGISTER(buzzer_ui, CONFIG_BUZZER_UI_LOG_LEVEL);

/* UI Objects */
static lv_obj_t *tone_buttons[8];
static lv_obj_t *stop_button;
static lv_obj_t *status_label;
static uint32_t current_frequency = 0;

/* Tone definitions */
struct tone_config {
	const char *name;
	uint32_t frequency;
	uint8_t duty_cycle;
};

static const struct tone_config tones[] = {
	{"C4 (262Hz)", BUZZER_TONE_C4, 50},
	{"D4 (294Hz)", BUZZER_TONE_D4, 50},
	{"E4 (330Hz)", BUZZER_TONE_E4, 50},
	{"F4 (349Hz)", BUZZER_TONE_F4, 50},
	{"G4 (392Hz)", BUZZER_TONE_G4, 50},
	{"A4 (440Hz)", BUZZER_TONE_A4, 50},
	{"Beep 1kHz", BUZZER_TONE_BEEP, 50},
	{"Alarm 2kHz", BUZZER_TONE_ALARM, 50},
};

#define NUM_TONES (sizeof(tones) / sizeof(tones[0]))

/* Button event handlers */
static void tone_button_event_cb(lv_event_t *e)
{
	if (e == NULL) return;
	
	lv_event_code_t code = lv_event_get_code(e);
	if (code != LV_EVENT_CLICKED) return;
	
	lv_obj_t *btn = lv_event_get_target(e);
	if (btn == NULL) return;
	
	/* Find which button was clicked */
	int tone_idx = -1;
	for (int i = 0; i < NUM_TONES; i++) {
		if (tone_buttons[i] == btn) {
			tone_idx = i;
			break;
		}
	}
	
	if (tone_idx < 0 || tone_idx >= NUM_TONES) {
		LOG_ERR("Invalid tone button index");
		return;
	}
	
	const struct tone_config *tone = &tones[tone_idx];
	/* Play tone indefinitely (duration=0) until user stops manually */
	int ret = buzzer_play_tone(tone->frequency, tone->duty_cycle, 0);
	if (ret == 0) {
		current_frequency = tone->frequency;
		if (ui_is_obj_valid(status_label, UI_SCREEN_BUZZER)) {
			char buf[64];
			snprintf(buf, sizeof(buf), "Playing: %s", tone->name);
			lv_label_set_text(status_label, buf);
		}
		LOG_INF("Playing tone: %s (%u Hz)", tone->name, tone->frequency);
	} else {
		LOG_ERR("Failed to play tone: %d", ret);
		if (ui_is_obj_valid(status_label, UI_SCREEN_BUZZER)) {
			lv_label_set_text(status_label, "Error playing tone");
		}
	}
}

static void stop_button_event_cb(lv_event_t *e)
{
	if (e == NULL) return;
	
	lv_event_code_t code = lv_event_get_code(e);
	if (code != LV_EVENT_CLICKED) return;
	
	int ret = buzzer_stop();
	if (ret == 0) {
		current_frequency = 0;
		if (ui_is_obj_valid(status_label, UI_SCREEN_BUZZER)) {
			lv_label_set_text(status_label, "Stopped");
		}
		LOG_INF("Buzzer stopped");
	} else {
		LOG_ERR("Failed to stop buzzer: %d", ret);
	}
}

void buzzer_ui_create_screen(lv_obj_t *screen)
{
	if (screen == NULL) {
		LOG_ERR("Screen object is NULL");
		return;
	}
	
	ns_apply_screen_bg(screen);
	
	/* Title (placed below the global status strip by the helper) */
	lv_obj_t *title = ns_make_screen_title(screen, "Buzzer Control");
	if (title == NULL) {
		LOG_ERR("Failed to create title label");
		return;
	}
	
	/* Status label */
	status_label = lv_label_create(screen);
	if (status_label == NULL) {
		LOG_ERR("Failed to create status label");
		return;
	}
	lv_label_set_text(status_label, "Stopped");
	lv_obj_set_style_text_color(status_label, NS_COLOR_WARNING, LV_PART_MAIN);
	lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 60);
	
	/* Create tone buttons in grid layout */
	int btn_width = 100;
	int btn_height = 26;
	int x_spacing = 10;
	int y_spacing = 2;
	int start_y = 80;
	
	for (int i = 0; i < NUM_TONES; i++) {
		int row = i / 2;
		int col = i % 2;
		int x = (col == 0) ? 10 : (120 + x_spacing);
		int y = start_y + (row * (btn_height + y_spacing));
		
		tone_buttons[i] = lv_btn_create(screen);
		if (tone_buttons[i] == NULL) {
			LOG_ERR("Failed to create tone button %d", i);
			return;
		}
		lv_obj_set_size(tone_buttons[i], btn_width, btn_height);
		lv_obj_set_pos(tone_buttons[i], x, y);
		lv_obj_set_style_bg_color(tone_buttons[i], NS_COLOR_PRIMARY, LV_PART_MAIN);
		lv_obj_add_event_cb(tone_buttons[i], tone_button_event_cb, LV_EVENT_CLICKED, NULL);
		
		lv_obj_t *btn_label = lv_label_create(tone_buttons[i]);
		if (btn_label == NULL) {
			LOG_ERR("Failed to create button label %d", i);
			return;
		}
		lv_label_set_text(btn_label, tones[i].name);
		lv_obj_set_style_text_font(btn_label, UI_FONT_14, LV_PART_MAIN);
		lv_obj_center(btn_label);
	}
	
	/* Stop button (full width at bottom) */
	stop_button = lv_btn_create(screen);
	if (stop_button == NULL) {
		LOG_ERR("Failed to create stop button");
		return;
	}
	lv_obj_set_size(stop_button, 220, 35);
	lv_obj_align(stop_button, LV_ALIGN_BOTTOM_MID, 0, -5);
	lv_obj_set_style_bg_color(stop_button, NS_COLOR_CRITICAL, LV_PART_MAIN);
	lv_obj_add_event_cb(stop_button, stop_button_event_cb, LV_EVENT_CLICKED, NULL);
	
	lv_obj_t *btn_label = lv_label_create(stop_button);
	if (btn_label == NULL) {
		LOG_ERR("Failed to create stop button label");
		return;
	}
	lv_label_set_text(btn_label, "STOP");
	lv_obj_set_style_text_font(btn_label, UI_FONT_14, LV_PART_MAIN);
	lv_obj_center(btn_label);
	
	LOG_INF("Buzzer UI screen created");
}

void buzzer_ui_update_screen(void)
{
	/* Update status based on current state */
	if (!ui_is_obj_valid(status_label, UI_SCREEN_BUZZER)) {
		return;
	}
	
	if (buzzer_is_playing() && current_frequency > 0) {
		/* Already showing correct status from button press */
	} else if (current_frequency == 0) {
		lv_label_set_text(status_label, "Stopped");
	}
}

void buzzer_ui_cleanup_screen(void)
{
	/* Stop buzzer if playing */
	buzzer_stop();
	current_frequency = 0;
	
	/* Clear object pointers */
	for (int i = 0; i < NUM_TONES; i++) {
		tone_buttons[i] = NULL;
	}
	stop_button = NULL;
	status_label = NULL;
	
	LOG_INF("Buzzer UI screen cleaned up");
}
