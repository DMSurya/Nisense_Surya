/* temp_ui.c - Temperature sensor UI screen
 *
 * Displays all available temperature sensors:
 * - nRF52840 die temperature (internal)
 * - MAX30208 wearable sensor (wrist, continuous)
 * - MAX30205 pulse sensor (finger, spot check)
 */

#include "temp_ui.h"
#include "ui_common.h"
#include "ui_theme.h"
#include "ui_shell.h"
#include "temp.h"
#include "resource.h"
#include "resource_icons.h"
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/max302xx.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

LOG_MODULE_REGISTER(temp_ui, CONFIG_TEMP_UI_LOG_LEVEL);

/* Temperature sensor nodes from device tree */
#define TEMP_WEARABLE_NODE DT_ALIAS(temp_wearable)
#define TEMP_PULSE_NODE DT_ALIAS(temp_pulse)
#define NRF_TEMP_NODE DT_NODELABEL(temp)

/* Temperature sensor devices */
#if DT_NODE_EXISTS(TEMP_WEARABLE_NODE)
static const struct device *temp_wearable = DEVICE_DT_GET(TEMP_WEARABLE_NODE);
#else
static const struct device *temp_wearable = NULL;
#endif

#if DT_NODE_EXISTS(TEMP_PULSE_NODE)
static const struct device *temp_pulse = DEVICE_DT_GET(TEMP_PULSE_NODE);
#else
static const struct device *temp_pulse = NULL;
#endif

#if DT_NODE_EXISTS(NRF_TEMP_NODE)
static const struct device *nrf_temp = DEVICE_DT_GET(NRF_TEMP_NODE);
#else
static const struct device *nrf_temp = NULL;
#endif

/* UI elements */
static lv_obj_t *cards_cont;
static struct ns_card die_card;       /* nRF die temp  */
static struct ns_card wearable_card;  /* Wrist temp    */
static struct ns_card pulse_card;     /* Finger temp   */
static lv_obj_t *die_label;      /* alias: die_card.value_label      */
static lv_obj_t *wearable_label; /* alias: wearable_card.value_label */
static lv_obj_t *pulse_label;    /* alias: pulse_card.value_label    */
static lv_obj_t *status_label;
static lv_obj_t *wearable_title; /* alias: wearable_card.name_label  */
static lv_obj_t *pulse_title;    /* alias: pulse_card.name_label     */

/*
 * BUGFIX: hold last known-good, non-zero reading per sensor. A read that
 * succeeds but returns exactly 0.0 C (I2C glitch / sensor not yet warmed
 * up) previously fell straight through to the label unfiltered, flashing
 * "0.0 C" on screen. None of these three sensors legitimately read 0.0 C
 * in normal wear, so 0.0 C is treated the same as a failed read: keep
 * showing the last good value until a new non-zero sample arrives.
 */
static float die_last_c;
static bool  die_last_valid;
static float wearable_last_c;
static bool  wearable_last_valid;
static float pulse_last_c;
static bool  pulse_last_valid;

/* Temperature range for gauge (in Celsius) */
#define TEMP_MIN -10
#define TEMP_MAX 50
#define TEMP_WARN_HIGH 40
#define TEMP_WARN_LOW 10

/**
 * @brief Detect MAX302xx sensor type by checking unique ID support
 * @param dev Sensor device
 * @return Sensor name string ("MAX30205", "MAX30208", or "MAX30210")
 */
static const char *detect_sensor_type(const struct device *dev)
{
	if (dev == NULL || !device_is_ready(dev)) {
		return "N/A";
	}
	
	struct sensor_value id[6];
	int ret = sensor_attr_get(dev, SENSOR_CHAN_ALL, 
	                          SENSOR_ATTR_MAX302XX_UNIQUE_ID, id);
	
	if (ret == -ENOTSUP) {
		/* No unique ID support = MAX30205 */
		return "MAX30205";
	} else if (ret == 0) {
		/* Has unique ID - could be MAX30208 or MAX30210 */
		/* For now, we assume MAX30208 since we can't distinguish without Part ID read */
		/* Future: Could add SENSOR_ATTR_MAX302XX_PART_ID to driver */
		return "MAX30208";
	}
	
	return "Unknown";
}

/**
 * @brief Create temperature sensor screen
 */
lv_obj_t *create_temp_screen(void)
{
	lv_obj_t *screen = lv_obj_create(NULL);
	if (screen == NULL) {
		LOG_ERR("Failed to create temperature screen");
		return NULL;
	}
	
	ns_apply_screen_bg(screen);
	
	/* Title */
	lv_obj_t *title = lv_label_create(screen);
	if (title == NULL) {
		LOG_ERR("Failed to create title label");
		lv_obj_del(screen);
		return NULL;
	}
	lv_label_set_text(title, "Temperature");
	lv_obj_set_style_text_color(title, NS_COLOR_TEXT, LV_PART_MAIN);
	lv_obj_set_style_text_font(title, UI_FONT_20, LV_PART_MAIN);
	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, UI_SHELL_TOP_INSET + 5);
	
	/* Scrollable card container */
	cards_cont = lv_obj_create(screen);
	if (cards_cont == NULL) {
		LOG_ERR("Failed to create temp cards container");
		lv_obj_del(screen);
		return NULL;
	}
	lv_obj_remove_style_all(cards_cont);
	lv_obj_set_size(cards_cont, 236, 150);
	lv_obj_align(cards_cont, LV_ALIGN_TOP_MID, 0, UI_SHELL_TOP_INSET + 36);
	lv_obj_set_flex_flow(cards_cont, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_row(cards_cont, 6, LV_PART_MAIN);
	lv_obj_set_style_pad_all(cards_cont, 2, LV_PART_MAIN);
	lv_obj_set_scroll_dir(cards_cont, LV_DIR_VER);
	lv_obj_set_scrollbar_mode(cards_cont, LV_SCROLLBAR_MODE_AUTO);

	ns_make_param_card(cards_cont, "MCU Die", NS_COLOR_TEXT_DIM, xip_icon(RESOURCE_ID_ICON_SETTINGS), &die_card);
	ns_make_param_card(cards_cont, "Wrist: Detecting...", NS_COLOR_TEMP, xip_icon(RESOURCE_ID_ICON_TEMP), &wearable_card);
	ns_make_param_card(cards_cont, "Finger: Detecting...", NS_COLOR_SPO2, xip_icon(RESOURCE_ID_ICON_TEMP), &pulse_card);

	die_label      = die_card.value_label;
	wearable_label = wearable_card.value_label;
	pulse_label    = pulse_card.value_label;
	wearable_title = wearable_card.name_label;
	pulse_title    = pulse_card.name_label;

	lv_label_set_text(die_label, "--.- C");
	lv_label_set_text(wearable_label, "--.- C");
	lv_label_set_text(pulse_label, "--.- C");
	
	/* Status label */
	status_label = lv_label_create(screen);
	if (status_label == NULL) {
		LOG_ERR("Failed to create status label");
		lv_obj_del(screen);
		return NULL;
	}
	lv_label_set_text(status_label, "Initializing...");
	lv_obj_set_style_text_color(status_label, NS_COLOR_WARNING, LV_PART_MAIN);
	lv_obj_set_style_text_font(status_label, UI_FONT_14, LV_PART_MAIN);
	lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -6);
	
	LOG_INF("Temperature screen created (3 sensors)");
	return screen;
}

/**
 * @brief Read temperature from a sensor on its explicit channel.
 *
 * Each sensor reports on exactly one channel — the nRF die sensor on
 * SENSOR_CHAN_DIE_TEMP, the MAX302xx skin sensors on SENSOR_CHAN_AMBIENT_TEMP.
 * Passing the correct channel avoids the previous DIE-first guess that limped
 * along on the AMBIENT fallback.
 */
static bool read_temperature(const struct device *dev, enum sensor_channel chan,
			     float *temp_c)
{
	struct sensor_value temp_val;

	if (dev == NULL || !device_is_ready(dev)) {
		return false;
	}

	if (sensor_sample_fetch(dev) < 0) {
		return false;
	}

	if (sensor_channel_get(dev, chan, &temp_val) < 0) {
		return false;
	}

	*temp_c = sensor_value_to_double(&temp_val);
	return true;
}

/**
 * @brief Format temperature for display
 */
static void format_temp(float temp_c, char *buf, size_t buf_size, bool show_fahrenheit)
{
	if (show_fahrenheit) {
		/* Calculate Fahrenheit using integer math */
		int temp_c_x10 = (int)(temp_c * 10);
		int temp_f_x10 = (temp_c_x10 * 9 / 5) + 320;
		int temp_f_int = temp_f_x10 / 10;
		int temp_f_frac = temp_f_x10 % 10;
		int temp_c_int = (int)temp_c;
		int temp_c_frac = (int)((temp_c - temp_c_int) * 10);
		
		snprintf(buf, buf_size, "%d.%d C  %d.%d F", 
		         temp_c_int, temp_c_frac, temp_f_int, temp_f_frac);
	} else {
		/* Celsius only */
		int temp_c_int = (int)temp_c;
		int temp_c_frac = (int)((temp_c - temp_c_int) * 10);
		snprintf(buf, buf_size, "%d.%d C", temp_c_int, temp_c_frac);
	}
}

/**
 * @brief Update temperature screen with current sensor data
 */
void update_temp_screen(void)
{
	char buf[32];
	float temp_c;
	int ready_count = 0;
	
	/* Check if screen elements exist */
	if (die_label == NULL || wearable_label == NULL || 
	    pulse_label == NULL || status_label == NULL) {
		return;
	}
	
	/* Update nRF SoC die temperature (junction temp, not body temp) */
	if (read_temperature(nrf_temp, SENSOR_CHAN_DIE_TEMP, &temp_c) && temp_c != 0.0f) {
		die_last_c = temp_c;
		die_last_valid = true;
		format_temp(die_last_c, buf, sizeof(buf), false);
		lv_label_set_text(die_label, buf);
		ready_count++;
	} else if (die_last_valid) {
		format_temp(die_last_c, buf, sizeof(buf), false);
		lv_label_set_text(die_label, buf);
		ready_count++;
	} else {
		lv_label_set_text(die_label, "--.- C");
	}
	
	/* Update wrist body temperature (MAX30208) */
	if (read_temperature(temp_wearable, SENSOR_CHAN_AMBIENT_TEMP, &temp_c) && temp_c != 0.0f) {
		wearable_last_c = temp_c;
		wearable_last_valid = true;

		/* Detect and update sensor type in title */
		const char *sensor_type = detect_sensor_type(temp_wearable);
		snprintf(buf, sizeof(buf), "Wrist Skin (%s):", sensor_type);
		if (wearable_title) {
			lv_label_set_text(wearable_title, buf);
		}
		
		format_temp(wearable_last_c, buf, sizeof(buf), true);
		lv_label_set_text(wearable_label, buf);
		ready_count++;
		
		/* Skin wellness band (NOT core temperature / fever). */
		switch (temp_classify_skin(wearable_last_c)) {
		case TEMP_SKIN_LOW:      ns_card_set_status(&wearable_card, NS_STATUS_INFO); break;
		case TEMP_SKIN_ELEVATED: ns_card_set_status(&wearable_card, NS_STATUS_ATTENTION); break;
		case TEMP_SKIN_HIGH:     ns_card_set_status(&wearable_card, NS_STATUS_RISK); break;
		case TEMP_SKIN_NORMAL:
		default:                 ns_card_set_status(&wearable_card, NS_STATUS_NORMAL); break;
		}
	} else if (wearable_last_valid) {
		format_temp(wearable_last_c, buf, sizeof(buf), true);
		lv_label_set_text(wearable_label, buf);
		ready_count++;
	} else {
		lv_label_set_text(wearable_label, "--.- C");
	}
	
	/* Update finger body temperature (MAX30205) */
	if (read_temperature(temp_pulse, SENSOR_CHAN_AMBIENT_TEMP, &temp_c) && temp_c != 0.0f) {
		pulse_last_c = temp_c;
		pulse_last_valid = true;

		/* Detect and update sensor type in title */
		const char *sensor_type = detect_sensor_type(temp_pulse);
		snprintf(buf, sizeof(buf), "Finger Skin (%s):", sensor_type);
		if (pulse_title) {
			lv_label_set_text(pulse_title, buf);
		}
		
		format_temp(pulse_last_c, buf, sizeof(buf), true);
		lv_label_set_text(pulse_label, buf);
		ready_count++;
		
		/* Skin wellness band (NOT core temperature / fever). */
		switch (temp_classify_skin(pulse_last_c)) {
		case TEMP_SKIN_LOW:      ns_card_set_status(&pulse_card, NS_STATUS_INFO); break;
		case TEMP_SKIN_ELEVATED: ns_card_set_status(&pulse_card, NS_STATUS_ATTENTION); break;
		case TEMP_SKIN_HIGH:     ns_card_set_status(&pulse_card, NS_STATUS_RISK); break;
		case TEMP_SKIN_NORMAL:
		default:                 ns_card_set_status(&pulse_card, NS_STATUS_NORMAL); break;
		}
	} else if (pulse_last_valid) {
		format_temp(pulse_last_c, buf, sizeof(buf), true);
		lv_label_set_text(pulse_label, buf);
		ready_count++;
	} else {
		lv_label_set_text(pulse_label, "--.- C");
	}
	
	/* Update status */
	if (ready_count == 0) {
		lv_label_set_text(status_label, "All Sensors Offline");
		lv_obj_set_style_text_color(status_label, NS_COLOR_CRITICAL, LV_PART_MAIN);
	} else if (ready_count == 3) {
		lv_label_set_text(status_label, "All Sensors Ready");
		lv_obj_set_style_text_color(status_label, NS_COLOR_NORMAL, LV_PART_MAIN);
	} else {
		snprintf(buf, sizeof(buf), "%d/%d Sensors Ready", ready_count, 3);
		lv_label_set_text(status_label, buf);
		lv_obj_set_style_text_color(status_label, NS_COLOR_WARNING, LV_PART_MAIN);
	}
}

void temp_ui_cleanup_screen(void)
{
	cards_cont = NULL;
	die_card = (struct ns_card){0};
	wearable_card = (struct ns_card){0};
	pulse_card = (struct ns_card){0};
	die_label = NULL;
	wearable_label = NULL;
	pulse_label = NULL;
	status_label = NULL;
	wearable_title = NULL;
	pulse_title = NULL;
	LOG_DBG("Temperature screen cleanup complete");
}
