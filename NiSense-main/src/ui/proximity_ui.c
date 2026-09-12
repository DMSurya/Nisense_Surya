/* proximity_ui.c - Proximity sensor UI screen
 *
 * Displays VCNL3040 proximity sensor data including:
 * - Raw proximity (offset-corrected)
 * - Filtered proximity (motion-stable)
 * - Wear detection state (NOT_WORN / WORN_LOOSE / WORN_GOOD)
 */

#include "ui_common.h"
#include "ui_theme.h"
#include "proximity_ui.h"
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include "resource_fonts.h"
#include <stdio.h>

LOG_MODULE_REGISTER(proximity_ui, CONFIG_PROXIMITY_UI_LOG_LEVEL);

/* VCNL3040 device from device tree alias */
#define VCNL3040_NODE DT_ALIAS(vcnl3040)

#if DT_NODE_HAS_STATUS(VCNL3040_NODE, okay)
static const struct device *vcnl3040_dev = DEVICE_DT_GET(VCNL3040_NODE);
#define VCNL3040_PRESENT 1
#else
static const struct device *vcnl3040_dev = NULL;
#define VCNL3040_PRESENT 0
#warning "VCNL3040 proximity sensor (vcnl3040 alias) not found or disabled in device tree"
#endif

/* UI elements */
static lv_obj_t *prox_status_label;
static lv_obj_t *prox_value_label;        /* Raw proximity */
static lv_obj_t *prox_filtered_label;     /* Filtered proximity */
static lv_obj_t *prox_wear_label;         /* Wear detection state */

/* Wear state strings */
static const char *wear_state_strings[] = {
	"NOT_WORN",
	"WORN_LOOSE",
	"WORN_GOOD"
};

/* Wear state colors (brand status tokens) */
static const uint32_t wear_state_colors[] = {
	NS_RGB_CRITICAL,  /* NOT_WORN   */
	NS_RGB_WARNING,   /* WORN_LOOSE */
	NS_RGB_NORMAL     /* WORN_GOOD  */
};

static void update_proximity_screen(void)
{
	if (ui_get_screen(UI_SCREEN_PROXIMITY) == NULL) {
		return;
	}
	
#if !VCNL3040_PRESENT
	if (ui_is_obj_valid(prox_status_label, UI_SCREEN_PROXIMITY)) {
		lv_label_set_text(prox_status_label, "Status: Driver not instantiated");
		lv_obj_set_style_text_color(prox_status_label, NS_COLOR_CRITICAL, LV_PART_MAIN);
	}
	if (ui_is_obj_valid(prox_value_label, UI_SCREEN_PROXIMITY)) {
		lv_label_set_text(prox_value_label, "(Device tree binding issue)");
		lv_obj_set_style_text_color(prox_value_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
	}
	return;
#endif
	
	if (!device_is_ready(vcnl3040_dev)) {
		if (ui_is_obj_valid(prox_status_label, UI_SCREEN_PROXIMITY)) {
			lv_label_set_text(prox_status_label, "Status: Sensor not ready");
			lv_obj_set_style_text_color(prox_status_label, NS_COLOR_CRITICAL, LV_PART_MAIN);
		}
		return;
	}
	
	/* Fetch sensor data */
	int ret = sensor_sample_fetch(vcnl3040_dev);
	if (ret != 0) {
		if (ui_is_obj_valid(prox_status_label, UI_SCREEN_PROXIMITY)) {
			lv_label_set_text(prox_status_label, "Status: Read error");
			lv_obj_set_style_text_color(prox_status_label, NS_COLOR_RISK, LV_PART_MAIN);
		}
		return;
	}
	
	/* Get proximity values (val1=raw, val2=filtered) */
	struct sensor_value prox_val;
	ret = sensor_channel_get(vcnl3040_dev, SENSOR_CHAN_PROX, &prox_val);
	if (ret != 0) {
		LOG_ERR("Failed to get proximity: %d", ret);
		return;
	}
	
	/* Get wear state (val1=0/1/2 for NOT_WORN/WORN_LOOSE/WORN_GOOD) */
	struct sensor_value wear_val;
	ret = sensor_channel_get(vcnl3040_dev, SENSOR_CHAN_DISTANCE, &wear_val);
	if (ret != 0) {
		LOG_ERR("Failed to get wear state: %d", ret);
		return;
	}
	
	/* Update status */
	if (ui_is_obj_valid(prox_status_label, UI_SCREEN_PROXIMITY)) {
		lv_label_set_text(prox_status_label, "Status: Active");
		lv_obj_set_style_text_color(prox_status_label, NS_COLOR_NORMAL, LV_PART_MAIN);
	}
	
	/* Update raw proximity value */
	if (ui_is_obj_valid(prox_value_label, UI_SCREEN_PROXIMITY)) {
		char buf[32];
		snprintf(buf, sizeof(buf), "Raw: %d", prox_val.val1);
		lv_label_set_text(prox_value_label, buf);
	}
	
	/* Update filtered proximity value */
	if (ui_is_obj_valid(prox_filtered_label, UI_SCREEN_PROXIMITY)) {
		char buf[32];
		snprintf(buf, sizeof(buf), "Filtered: %d", prox_val.val2);
		lv_label_set_text(prox_filtered_label, buf);
	}
	
	/* Update wear detection state */
	if (ui_is_obj_valid(prox_wear_label, UI_SCREEN_PROXIMITY)) {
		int wear_state = wear_val.val1;
		if (wear_state < 0 || wear_state > 2) {
			wear_state = 0;  /* Clamp to valid range */
		}
		
		char buf[48];
		snprintf(buf, sizeof(buf), "Wear: %s", wear_state_strings[wear_state]);
		lv_label_set_text(prox_wear_label, buf);
		lv_obj_set_style_text_color(prox_wear_label, 
			lv_color_hex(wear_state_colors[wear_state]), LV_PART_MAIN);
	}
}

void proximity_ui_create_screen(lv_obj_t *screen)
{
	if (screen == NULL) {
		LOG_ERR("Failed to create proximity screen - out of memory");
		return;
	}
	
	ns_apply_screen_bg(screen);
	
	/* Title (placed below the global status strip by the helper) */
	lv_obj_t *title = ns_make_screen_title(screen, "Proximity Sensor");
	if (title == NULL) {
		LOG_ERR("Failed to create proximity title label - out of memory");
		ui_delete_screen(UI_SCREEN_PROXIMITY);
		return;
	}
	
	/* Status label */
	prox_status_label = lv_label_create(screen);
	if (prox_status_label == NULL) {
		LOG_ERR("Failed to create proximity status label - out of memory");
		ui_delete_screen(UI_SCREEN_PROXIMITY);
		return;
	}
	lv_label_set_text(prox_status_label, "Status: Initializing...");
	lv_obj_set_style_text_font(prox_status_label, UI_FONT_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(prox_status_label, NS_COLOR_INFO, LV_PART_MAIN);
	lv_obj_align(prox_status_label, LV_ALIGN_CENTER, 0, -60);
	
	/* Proximity value label (raw) */
	prox_value_label = lv_label_create(screen);
	if (prox_value_label == NULL) {
		LOG_ERR("Failed to create proximity value label - out of memory");
		ui_delete_screen(UI_SCREEN_PROXIMITY);
		return;
	}
	lv_label_set_text(prox_value_label, "Raw: ---");
	lv_obj_set_style_text_font(prox_value_label, UI_FONT_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(prox_value_label, NS_COLOR_TEXT, LV_PART_MAIN);
	lv_obj_align(prox_value_label, LV_ALIGN_CENTER, 0, -30);
	
	/* Filtered proximity label */
	prox_filtered_label = lv_label_create(screen);
	if (prox_filtered_label == NULL) {
		LOG_ERR("Failed to create filtered proximity label - out of memory");
		ui_delete_screen(UI_SCREEN_PROXIMITY);
		return;
	}
	lv_label_set_text(prox_filtered_label, "Filtered: ---");
	lv_obj_set_style_text_font(prox_filtered_label, UI_FONT_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(prox_filtered_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
	lv_obj_align(prox_filtered_label, LV_ALIGN_CENTER, 0, 0);
	
	/* Wear detection state label */
	prox_wear_label = lv_label_create(screen);
	if (prox_wear_label == NULL) {
		LOG_ERR("Failed to create wear detection label - out of memory");
		ui_delete_screen(UI_SCREEN_PROXIMITY);
		return;
	}
	lv_label_set_text(prox_wear_label, "Wear: ---");
	lv_obj_set_style_text_font(prox_wear_label, UI_FONT_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(prox_wear_label, NS_COLOR_TEXT, LV_PART_MAIN);
	lv_obj_align(prox_wear_label, LV_ALIGN_CENTER, 0, 40);
	
	/* Info text */
	lv_obj_t *info = lv_label_create(screen);
	if (info != NULL) {
		lv_label_set_text(info, "VCNL3040 Wear Detection");
		lv_obj_set_style_text_font(info, UI_FONT_14, LV_PART_MAIN);
		lv_obj_set_style_text_color(info, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
		lv_obj_align(info, LV_ALIGN_BOTTOM_MID, 0, -10);
	}
	
	/* Check device readiness */
#if !VCNL3040_PRESENT
	LOG_WRN("Proximity screen: VCNL3040 not in device tree");
	if (ui_is_obj_valid(prox_status_label, UI_SCREEN_PROXIMITY)) {
		lv_label_set_text(prox_status_label, "Status: Not configured");
		lv_obj_set_style_text_color(prox_status_label, NS_COLOR_RISK, LV_PART_MAIN);
	}
#else
	/* VCNL3040 device obtained from device tree - check if ready */
	if (vcnl3040_dev == NULL || !device_is_ready(vcnl3040_dev)) {
		LOG_WRN("Proximity screen: VCNL3040 device not ready (check I2C address 0x60)");
		if (ui_is_obj_valid(prox_status_label, UI_SCREEN_PROXIMITY)) {
			lv_label_set_text(prox_status_label, "Status: Sensor not ready");
			lv_obj_set_style_text_color(prox_status_label, NS_COLOR_CRITICAL, LV_PART_MAIN);
		}
		if (ui_is_obj_valid(prox_value_label, UI_SCREEN_PROXIMITY)) {
			lv_label_set_text(prox_value_label, "(Check hardware connection)");
			lv_obj_set_style_text_color(prox_value_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
		}
	} else {
		LOG_INF("Proximity screen created");
		update_proximity_screen();
	}
#endif
}

void proximity_ui_update_screen(void)
{
	update_proximity_screen();
}

void proximity_ui_cleanup_screen(void)
{
	prox_status_label = NULL;
	prox_value_label = NULL;
	prox_filtered_label = NULL;
	prox_wear_label = NULL;
}
