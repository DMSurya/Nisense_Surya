/**
 * @file accel_ui.c
 * @brief Accelerometer visualization UI screen
 * 
 * Displays real-time accelerometer data with animated visualization:
 * - X/Y/Z axis values (in milli-g)
 * - Animated bubble that moves based on tilt
 * - Color-coded bars for each axis
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include <math.h>
#include "ui_common.h"
#include "ui_theme.h"

LOG_MODULE_REGISTER(accel_ui, CONFIG_ACCEL_UI_LOG_LEVEL);

/* UI objects */
static lv_obj_t *accel_title_label = NULL;
static lv_obj_t *accel_x_label = NULL;
static lv_obj_t *accel_y_label = NULL;
static lv_obj_t *accel_z_label = NULL;
static lv_obj_t *accel_x_bar = NULL;
static lv_obj_t *accel_y_bar = NULL;
static lv_obj_t *accel_z_bar = NULL;
static lv_obj_t *accel_bubble = NULL;
static lv_obj_t *accel_bubble_container = NULL;

/* Accelerometer data */
static int16_t accel_x_mg = 0;
static int16_t accel_y_mg = 0;
static int16_t accel_z_mg = 0;

/* Animation smoothing */
#define ACCEL_SMOOTH_FACTOR 0.3f  /* 0.0 = no smoothing, 1.0 = heavy smoothing - reduced for responsiveness */
static float smooth_x = 0.0f;
static float smooth_y = 0.0f;

/**
 * @brief Create accelerometer UI screen
 * 
 * @return Pointer to created screen object, or NULL on failure
 */
lv_obj_t *ui_create_screen_accel(void)
{
	lv_obj_t *screen = ui_get_screen(UI_SCREEN_ACCEL);
	
	/* Create screen if it doesn't exist */
	if (screen == NULL) {
		screen = lv_obj_create(NULL);
		if (screen == NULL) {
			LOG_ERR("Failed to create accel screen");
			return NULL;
		}
		
		/* Set background color */
		ns_apply_screen_bg(screen);
		
		/* Title label (placed below the global status strip by the helper) */
		accel_title_label = ns_make_screen_title(screen, "Accelerometer");
		if (accel_title_label == NULL) {
			LOG_ERR("Failed to create title label");
			ui_delete_screen(UI_SCREEN_ACCEL);
			return NULL;
		}
		
		/* Bubble container (center area for tilt visualization) */
		accel_bubble_container = lv_obj_create(screen);
		if (accel_bubble_container == NULL) {
			LOG_ERR("Failed to create bubble container");
			ui_delete_screen(UI_SCREEN_ACCEL);
			return NULL;
		}
		lv_obj_set_size(accel_bubble_container, 100, 100);
		lv_obj_align(accel_bubble_container, LV_ALIGN_CENTER, 0, -12);
		lv_obj_set_style_bg_color(accel_bubble_container, NS_COLOR_CARD, 0);
		lv_obj_set_style_border_width(accel_bubble_container, 2, 0);
		lv_obj_set_style_border_color(accel_bubble_container, NS_COLOR_BORDER, 0);
		lv_obj_set_style_radius(accel_bubble_container, 10, 0);
		lv_obj_clear_flag(accel_bubble_container, LV_OBJ_FLAG_SCROLLABLE);
		
		/* Animated bubble (moves based on tilt) */
		accel_bubble = lv_obj_create(accel_bubble_container);
		if (accel_bubble == NULL) {
			LOG_ERR("Failed to create bubble");
			ui_delete_screen(UI_SCREEN_ACCEL);
			return NULL;
		}
		lv_obj_set_size(accel_bubble, 20, 20);
		lv_obj_set_style_radius(accel_bubble, LV_RADIUS_CIRCLE, 0);
		lv_obj_set_style_bg_color(accel_bubble, NS_COLOR_NORMAL, 0);
		lv_obj_set_style_border_width(accel_bubble, 0, 0);
		lv_obj_align(accel_bubble, LV_ALIGN_CENTER, 0, 0);
		
		/* X-axis label and bar */
		accel_x_label = lv_label_create(screen);
		if (accel_x_label == NULL) {
			LOG_ERR("Failed to create X label");
			ui_delete_screen(UI_SCREEN_ACCEL);
			return NULL;
		}
		lv_label_set_text(accel_x_label, "X: 0 mg");
		lv_obj_set_style_text_color(accel_x_label, NS_COLOR_HR, 0);
		lv_obj_align(accel_x_label, LV_ALIGN_BOTTOM_LEFT, 10, -60);
		
		accel_x_bar = lv_bar_create(screen);
		if (accel_x_bar == NULL) {
			LOG_ERR("Failed to create X bar");
			ui_delete_screen(UI_SCREEN_ACCEL);
			return NULL;
		}
		lv_obj_set_size(accel_x_bar, 200, 8);
		lv_obj_align(accel_x_bar, LV_ALIGN_BOTTOM_LEFT, 20, -45);
		lv_bar_set_range(accel_x_bar, -2000, 2000);  /* ±2g range */
		lv_bar_set_value(accel_x_bar, 0, LV_ANIM_OFF);
		lv_obj_set_style_bg_color(accel_x_bar, NS_COLOR_HR, LV_PART_INDICATOR);
		
		/* Y-axis label and bar */
		accel_y_label = lv_label_create(screen);
		if (accel_y_label == NULL) {
			LOG_ERR("Failed to create Y label");
			ui_delete_screen(UI_SCREEN_ACCEL);
			return NULL;
		}
		lv_label_set_text(accel_y_label, "Y: 0 mg");
		lv_obj_set_style_text_color(accel_y_label, NS_COLOR_SECONDARY, 0);
		lv_obj_align(accel_y_label, LV_ALIGN_BOTTOM_LEFT, 10, -35);
		
		accel_y_bar = lv_bar_create(screen);
		if (accel_y_bar == NULL) {
			LOG_ERR("Failed to create Y bar");
			ui_delete_screen(UI_SCREEN_ACCEL);
			return NULL;
		}
		lv_obj_set_size(accel_y_bar, 200, 8);
		lv_obj_align(accel_y_bar, LV_ALIGN_BOTTOM_LEFT, 20, -20);
		lv_bar_set_range(accel_y_bar, -2000, 2000);
		lv_bar_set_value(accel_y_bar, 0, LV_ANIM_OFF);
		lv_obj_set_style_bg_color(accel_y_bar, NS_COLOR_SECONDARY, LV_PART_INDICATOR);
		
		/* Z-axis label and bar */
		accel_z_label = lv_label_create(screen);
		if (accel_z_label == NULL) {
			LOG_ERR("Failed to create Z label");
			ui_delete_screen(UI_SCREEN_ACCEL);
			return NULL;
		}
		lv_label_set_text(accel_z_label, "Z: 1000 mg");
		lv_obj_set_style_text_color(accel_z_label, NS_COLOR_WARNING, 0);
		lv_obj_align(accel_z_label, LV_ALIGN_BOTTOM_LEFT, 10, -10);
		
		accel_z_bar = lv_bar_create(screen);
		if (accel_z_bar == NULL) {
			LOG_ERR("Failed to create Z bar");
			ui_delete_screen(UI_SCREEN_ACCEL);
			return NULL;
		}
		lv_obj_set_size(accel_z_bar, 200, 8);
		lv_obj_align(accel_z_bar, LV_ALIGN_BOTTOM_RIGHT, -20, -5);
		lv_bar_set_range(accel_z_bar, -2000, 2000);
		lv_bar_set_value(accel_z_bar, 1000, LV_ANIM_OFF);  /* Default: gravity on Z */
		lv_obj_set_style_bg_color(accel_z_bar, NS_COLOR_WARNING, LV_PART_INDICATOR);
		
		LOG_INF("Accelerometer screen created");
	}
	
	return screen;
}

/**
 * @brief Update accelerometer visualization
 * 
 * Reads accelerometer data via MAX32664 hub or LIS2DS12 directly and updates UI elements
 */
void accel_ui_cleanup_screen(void)
{
	accel_title_label = NULL;
	accel_x_label = NULL;
	accel_y_label = NULL;
	accel_z_label = NULL;
	accel_x_bar = NULL;
	accel_y_bar = NULL;
	accel_z_bar = NULL;
	accel_bubble = NULL;
	accel_bubble_container = NULL;
}

void ui_update_screen_accel(void)
{
	/* Check if screen exists */
	if (ui_get_screen(UI_SCREEN_ACCEL) == NULL) {
		return;
	}
	
	/* Get accelerometer device based on compile-time mode selection */
	const struct device *accel_dev = NULL;
	
#if defined(CONFIG_MAX32664_MODE_RAW) || defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
	/* RAW/PASSTHROUGH mode: Use the LIS2DS12 accelerometer child sensor directly */
	accel_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(accel_sensor));
#else
	/* HUB mode: Use the MAX32664 hub device (reads accel via hub SPI) */
	accel_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(max32664_hub));
#endif
	
	if (accel_dev == NULL || !device_is_ready(accel_dev)) {
		/* No device available, use dummy data for demo */
		accel_x_mg = (int16_t)(sin(k_uptime_get() / 1000.0) * 500);
		accel_y_mg = (int16_t)(cos(k_uptime_get() / 1000.0) * 500);
		accel_z_mg = 1000;  /* Gravity */
	} else {
		/* Read accelerometer via standard Zephyr sensor API */
		struct sensor_value accel[3];
		int ret = sensor_sample_fetch_chan(accel_dev, SENSOR_CHAN_ACCEL_XYZ);
		if (ret == 0) {
			sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_X, &accel[0]);
			sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_Y, &accel[1]);
			sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_Z, &accel[2]);
			
			/* Convert from m/s² (sensor_value) to milli-g
			 * Formula: mg = m/s² * 1000 / 9.80665 ≈ m/s² * 101.97
			 * sensor_value: val1 = integer m/s², val2 = fractional in micro (10^-6)
			 * Full value in micro-m/s² = val1 * 1_000_000 + val2
			 * Convert to milli-g: (micro_ms2 * 1000) / (9806650) ≈ micro_ms2 / 9807
			 */
			int64_t micro_ms2_x = (int64_t)accel[0].val1 * 1000000LL + accel[0].val2;
			int64_t micro_ms2_y = (int64_t)accel[1].val1 * 1000000LL + accel[1].val2;
			int64_t micro_ms2_z = (int64_t)accel[2].val1 * 1000000LL + accel[2].val2;
			
			/* Convert: mg = micro_ms2 / 9807 (where 9807 ≈ 9.807 m/s² per g) */
			accel_x_mg = (int16_t)(micro_ms2_x / 9807LL);
			accel_y_mg = (int16_t)(micro_ms2_y / 9807LL);
			accel_z_mg = (int16_t)(micro_ms2_z / 9807LL);
		}
	}
	
	/* Update text labels */
	if (ui_is_obj_valid(accel_x_label, UI_SCREEN_ACCEL)) {
		lv_label_set_text_fmt(accel_x_label, "X: %d mg", accel_x_mg);
	}
	if (ui_is_obj_valid(accel_y_label, UI_SCREEN_ACCEL)) {
		lv_label_set_text_fmt(accel_y_label, "Y: %d mg", accel_y_mg);
	}
	if (ui_is_obj_valid(accel_z_label, UI_SCREEN_ACCEL)) {
		lv_label_set_text_fmt(accel_z_label, "Z: %d mg", accel_z_mg);
	}
	
	/* Update bar graphs */
	if (ui_is_obj_valid(accel_x_bar, UI_SCREEN_ACCEL)) {
		lv_bar_set_value(accel_x_bar, accel_x_mg, LV_ANIM_ON);
	}
	if (ui_is_obj_valid(accel_y_bar, UI_SCREEN_ACCEL)) {
		lv_bar_set_value(accel_y_bar, accel_y_mg, LV_ANIM_ON);
	}
	if (ui_is_obj_valid(accel_z_bar, UI_SCREEN_ACCEL)) {
		lv_bar_set_value(accel_z_bar, accel_z_mg, LV_ANIM_ON);
	}
	
	/* Update animated bubble position (smoothed for fluid motion) */
	if (ui_is_obj_valid(accel_bubble, UI_SCREEN_ACCEL) && 
	    ui_is_obj_valid(accel_bubble_container, UI_SCREEN_ACCEL)) {
		/* Apply exponential smoothing */
		smooth_x = smooth_x * ACCEL_SMOOTH_FACTOR + (float)accel_x_mg * (1.0f - ACCEL_SMOOTH_FACTOR);
		smooth_y = smooth_y * ACCEL_SMOOTH_FACTOR + (float)accel_y_mg * (1.0f - ACCEL_SMOOTH_FACTOR);
		
		/* Map accelerometer values to bubble position
		 * ±1000mg (±1g) -> ±50px movement (increased sensitivity) */
		int32_t bubble_x = (int32_t)(smooth_x / 20.0f);  /* 1000mg / 20 = 50px */
		int32_t bubble_y = (int32_t)(smooth_y / 20.0f);
		
		/* Clamp to container bounds (container is 120x120, bubble is 20x20, so ±50px max) */
		if (bubble_x > 45) bubble_x = 45;
		if (bubble_x < -45) bubble_x = -45;
		if (bubble_y > 45) bubble_y = 45;
		if (bubble_y < -45) bubble_y = -45;
		
		/* Update bubble position relative to container center (inverted Y for intuitive tilt) */
		lv_obj_set_pos(accel_bubble, 50 + bubble_x - 10, 50 - bubble_y - 10);  /* 50 = center, -10 = half bubble size */
		
		/* Change bubble color based on Z-axis (tilt intensity) */
		float z_ratio = fabsf((float)accel_z_mg / 2000.0f);
		if (z_ratio > 1.0f) z_ratio = 1.0f;
		
		/* Green (flat) -> Red (tilted) */
		uint8_t red = (uint8_t)(255 * (1.0f - z_ratio));
		uint8_t green = (uint8_t)(255 * z_ratio);
		lv_obj_set_style_bg_color(accel_bubble, lv_color_make(red, green, 0), 0);
	}
}
