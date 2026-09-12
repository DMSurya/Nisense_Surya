/* max32664_update_ui.c – MAX32664 Firmware Update UI Screen
 *
 * Loads .msbl from the QSPI `app` partition (APP_DATA_RESOURCE_MMAP_ADDR).
 * FatFS `/NAND:` is not used.
 */

#include "max32664_update.h"
#include "max32664_update_ui.h"
#include "ui_common.h"
#include "ui_theme.h"
#include "buzzer.h"
#include <app_data.h>
#include <lvgl.h>
#include "resource_fonts.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor/max32664.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/devicetree.h>

LOG_MODULE_REGISTER(max32664_update_ui, CONFIG_MAX32664_UPDATE_UI_LOG_LEVEL);

static void buzzer_beep(uint32_t duration_ms)
{
	buzzer_play_tone(BUZZER_TONE_BEEP, 50, duration_ms);  /* Auto-stops after duration */
	k_msleep(duration_ms);  /* Wait for beep to complete */
}

/**
 * @brief Helper: Buzzer beep pattern (non-blocking per-beep)
 * 
 * @param count Number of beeps
 * @param beep_ms Beep duration per beep
 * @param pause_ms Pause between beeps
 */
static void buzzer_beep_pattern(int count, uint32_t beep_ms, uint32_t pause_ms)
{
	for (int i = 0; i < count; i++) {
		uint32_t freq = (i == count - 1) ? BUZZER_TONE_BEEP : BUZZER_TONE_ALARM;
		buzzer_play_tone(freq, 50, beep_ms);  /* Auto-stops after beep_ms */
		k_msleep(beep_ms);  /* Wait for current beep */
		if (i < count - 1) {
			k_msleep(pause_ms);  /* Pause before next beep */
		}
	}
}

#define MAX32664_UPDATE_SCREEN_NAME "MAX32664 Update"
#define MAX_FILENAME_LEN 128

/* UI elements */
static lv_obj_t *fw_file_label;
static lv_obj_t *fw_version_label;  /* Display current firmware version */
static lv_obj_t *fw_progress_bar;
static lv_obj_t *fw_update_btn;
static lv_obj_t *fw_update_btn_label;
static lv_obj_t *fw_status_label;

/* State tracking */
static char msbl_filename[MAX_FILENAME_LEN] = {0};
static bool fw_file_found = false;
static bool update_in_progress = false;

static void max32664_update_btn_cb(lv_event_t *e);
static void update_max32664_update_screen(void);
static void start_firmware_update(void);
static void refresh_version_label_cb(void *param);

static void cleanup_usb_state(void)
{
	/* No FatFS session — image comes from QSPI app partition XIP. */
}

/**
 * @brief Read and display MAX32664 firmware version
 */
static void update_firmware_version_display(void)
{
	if (fw_version_label == NULL ||
	    ui_get_screen(UI_SCREEN_MAX32664_UPDATE) == NULL) {
		return;
	}

	/* Always use the hub device for firmware version (works in both RAW and HUB mode) */
	const struct device *max32664_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(max32664_hub));
	
	if (max32664_dev == NULL || !device_is_ready(max32664_dev)) {
		lv_label_set_text(fw_version_label, "Device not ready");
		lv_obj_set_style_text_color(fw_version_label, NS_COLOR_CRITICAL,
					    LV_PART_MAIN);
		return;
	}
	
	uint8_t major = 0, minor = 0, patch = 0;
	int ret = max32664_read_firmware_version(max32664_dev, &major, &minor, &patch);
	
	if (ret == 0) {
		char version_buf[32];
		snprintf(version_buf, sizeof(version_buf), "FW: v%u.%u.%u", major, minor, patch);
		lv_label_set_text(fw_version_label, version_buf);
		lv_obj_set_style_text_color(fw_version_label, NS_COLOR_NORMAL,
					    LV_PART_MAIN);
	} else {
		lv_label_set_text(fw_version_label, "FW: Unknown");
		lv_obj_set_style_text_color(fw_version_label, NS_COLOR_WARNING,
					    LV_PART_MAIN);
	}
}

/**
 * @brief Probe QSPI app partition for a plausible .msbl header.
 */
static int find_msbl_file(char *out_path, size_t out_sz)
{
	const uint8_t *msbl = (const uint8_t *)APP_DATA_RESOURCE_MMAP_ADDR;
	uint8_t pages = 0;
	int ret;

	if (out_path == NULL || out_sz < 16) {
		return -EINVAL;
	}

	ret = max32664_msbl_sanity_check(msbl, APP_DATA_PARTITION_SIZE, &pages);
	if (ret) {
		out_path[0] = '\0';
		return -ENOENT;
	}

	snprintf(out_path, out_sz, "QSPI:app (%u pages)", pages);
	LOG_INF("Found MSBL in app partition: %u pages", pages);
	return 0;
}

/**
 * @brief Update button callback
 * 
 * Validates state and triggers firmware update in background thread.
 */
static void max32664_update_btn_cb(lv_event_t *e)
{
	/* Validate event object */
	if (e == NULL) {
		LOG_ERR("max32664_update_btn_cb: NULL event");
		return;
	}
	
	/* Ignore clicks during gestures */
	uint32_t now = k_uptime_get_32();
	if (gesture_in_progress || (now - last_gesture_time < GESTURE_DEBOUNCE_MS)) {
		gesture_in_progress = false;
		return;
	}
	
	/* Check if screen still exists */
	if (ui_get_screen(UI_SCREEN_MAX32664_UPDATE) == NULL) {
		return;
	}
	
	/* Validate firmware file found and not already updating */
	if (!fw_file_found) {
		LOG_WRN("No firmware file found");
		return;
	}
	
	if (update_in_progress) {
		LOG_WRN("Update already in progress");
		return;
	}
	
	/* Disable button and start update */
	if (ui_is_obj_valid(fw_update_btn, UI_SCREEN_MAX32664_UPDATE)) {
		lv_obj_add_state(fw_update_btn, LV_STATE_DISABLED);
	}
	
	if (ui_is_obj_valid(fw_status_label, UI_SCREEN_MAX32664_UPDATE)) {
		lv_label_set_text(fw_status_label, "Preparing...");
	}
	
	/* Buzzer feedback */
	buzzer_beep(100);
	
	start_firmware_update();
}

/**
 * @brief Firmware update context (heap-allocated for thread safety)
 */
struct fw_update_ctx {
	int progress;
	char status[128];
	bool done;
	bool error;
};

static void max32664_ui_detach_widgets(void)
{
	(void)lv_async_call_cancel(refresh_version_label_cb, NULL);

	fw_file_label = NULL;
	fw_version_label = NULL;
	fw_progress_bar = NULL;
	fw_update_btn = NULL;
	fw_update_btn_label = NULL;
	fw_status_label = NULL;
}

/**
 * @brief Update progress bar and status label (LVGL thread-safe)
 * 
 * Called via lv_async_call from update thread.
 */
static void update_ui_progress_cb(void *param)
{
	struct fw_update_ctx *ctx = (struct fw_update_ctx *)param;
	
	if (ctx == NULL) {
		return;
	}

	/* Screen gone or widgets detached — free context, update thread keeps running. */
	if (ui_get_screen(UI_SCREEN_MAX32664_UPDATE) == NULL ||
	    fw_progress_bar == NULL) {
		k_free(ctx);
		return;
	}
	
	if (fw_progress_bar != NULL) {
		lv_bar_set_value(fw_progress_bar, ctx->progress, LV_ANIM_ON);
	}
	
	if (fw_status_label != NULL) {
		lv_obj_set_style_text_color(fw_status_label,
					    ctx->error ? NS_COLOR_CRITICAL : NS_COLOR_TEXT,
					    LV_PART_MAIN);
		lv_label_set_text(fw_status_label, ctx->status);
	}
	
	if (ctx->done) {
		update_in_progress = false;
		if (fw_update_btn != NULL) {
			lv_obj_clear_state(fw_update_btn, LV_STATE_DISABLED);
		}
		
		/* Buzzer feedback: 2 beeps for success, 3 for error */
		if (ctx->error) {
			buzzer_beep_pattern(3, 100, 100);
		} else {
			buzzer_beep_pattern(2, 200, 100);
		}
	}
	
	/* Free context */
	k_free(ctx);
}

/**
 * @brief Firmware update worker thread
 * 
 * Performs actual MAX32664 firmware update via I2C bootloader protocol.
 * Updates UI via lv_async_call for thread safety.
 */

/* Progress callback for firmware update (called from update function) */
static void firmware_update_progress(uint32_t current_page, uint32_t total_pages, void *user_data)
{
	struct fw_update_ctx *ctx = k_malloc(sizeof(*ctx));
	if (ctx == NULL) {
		return;  /* Skip progress update if out of memory */
	}
	
	/* Calculate percentage: (current_page / total_pages) * 100 */
	ctx->progress = (current_page * 100) / total_pages;
	ctx->done = false;
	ctx->error = false;
	snprintf(ctx->status, sizeof(ctx->status), "Writing page %u/%u...", current_page, total_pages);
	
	lv_async_call(update_ui_progress_cb, ctx);
}

static void firmware_update_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	
	const char *fw_src = "QSPI app partition";
	struct fw_update_ctx *ctx;
	int ret;
	
	LOG_INF("Firmware update thread started for: %s", fw_src);
	ARG_UNUSED(msbl_filename);
	
	/* Get MAX32664 device (use alias from device tree) */
	const struct device *max32664_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(max32664_hub));
	
	if (max32664_dev == NULL) {
		LOG_ERR("MAX32664 device not found in device tree");
		ctx = k_malloc(sizeof(*ctx));
		if (ctx) {
			ctx->progress = 0;
			ctx->done = true;
			ctx->error = true;
			snprintf(ctx->status, sizeof(ctx->status), "Device not found");
			lv_async_call(update_ui_progress_cb, ctx);
		}
		return;
	}
	
	/* Force power cycle BK2 regulator to reset MAX32664 (may be stuck from failed update) */
	LOG_INF("Power cycling MAX32664 via BK2 regulator...");
	const struct device *bk2 = DEVICE_DT_GET(DT_ALIAS(bk2));
	if (device_is_ready(bk2)) {
		regulator_disable(bk2);
		k_msleep(100);  /* Wait for power-down */
		regulator_enable(bk2);
		k_msleep(200);  /* Wait for power-up + hub boot (150ms min from device tree) */
		LOG_INF("Power cycle complete, hub should be in clean state");
	} else {
		LOG_WRN("BK2 regulator not available, skipping power cycle");
	}
	
	/* Note: device_is_ready() checks DRIVER init status from boot, not runtime I2C response.
	 * If driver failed to init (e.g., hub was stuck during boot), it won't become "ready" now.
	 * The update code will attempt I2C communication regardless - if hub is responsive after
	 * power cycle, it will work even if driver shows "not ready". */
	
	if (!device_is_ready(max32664_dev)) {
		LOG_WRN("MAX32664 driver shows not ready (may have failed boot init)");
		LOG_INF("Proceeding with update anyway - power cycle may have recovered device");
		/* Don't abort - continue to update attempt */
	} else {
		LOG_INF("MAX32664 device ready for firmware update");
	}
	
	/* Report start */
	ctx = k_malloc(sizeof(*ctx));
	if (ctx) {
		ctx->progress = 0;
		ctx->done = false;
		ctx->error = false;
		snprintf(ctx->status, sizeof(ctx->status), "Starting update...");
		lv_async_call(update_ui_progress_cb, ctx);
	}
	
	/* Perform firmware update from QSPI app partition */
	ret = max32664_update_firmware_from_app_partition(max32664_dev,
							  firmware_update_progress, NULL);
	
	/* Report result */
	ctx = k_malloc(sizeof(*ctx));
	if (ctx == NULL) {
		LOG_ERR("Failed to allocate update context");
		update_in_progress = false;
		return;
	}
	
	if (ret == 0) {
		/* Power cycle BK2 to ensure clean boot of new firmware.
		 * The I2C exit command alone may not trigger a hard reset — the same
		 * power cycle used before the update is needed after it too. */
		if (device_is_ready(bk2)) {
			LOG_INF("Power cycling MAX32664 to boot new firmware...");
			regulator_disable(bk2);
			k_msleep(200);
			regulator_enable(bk2);
			k_msleep(3000);  /* Allow new firmware to fully initialize */
			LOG_INF("Post-update power cycle complete");
		} else {
			LOG_WRN("BK2 not available - new firmware boot may be unreliable");
		}

		LOG_INF("Firmware update completed successfully");
		ctx->progress = 100;
		ctx->done = true;
		ctx->error = false;
		snprintf(ctx->status, sizeof(ctx->status), "Update complete!");

		/* Refresh version label with new firmware version */
		lv_async_call(refresh_version_label_cb, NULL);
	} else {
		LOG_ERR("Firmware update failed: %d", ret);
		ctx->progress = 0;
		ctx->done = true;
		ctx->error = true;
		snprintf(ctx->status, sizeof(ctx->status), "Update failed: %d", ret);
	}

	lv_async_call(update_ui_progress_cb, ctx);

	/* Re-enable USB now that update is complete */
	cleanup_usb_state();
}

/* Thread for firmware update.
 * Stack size tunable via CONFIG_MAX32664_UPDATE_THREAD_STACK_SIZE. */
static K_THREAD_STACK_DEFINE(update_thread_stack,
			     CONFIG_MAX32664_UPDATE_THREAD_STACK_SIZE);
static struct k_thread update_thread;
static k_tid_t update_thread_id;

/**
 * @brief Start firmware update in background thread
 */
static void start_firmware_update(void)
{
	if (update_in_progress) {
		LOG_WRN("Update already in progress");
		return;
	}
	
	update_in_progress = true;
	
	/* Reset progress bar */
	if (ui_is_obj_valid(fw_progress_bar, UI_SCREEN_MAX32664_UPDATE)) {
		lv_bar_set_value(fw_progress_bar, 0, LV_ANIM_OFF);
	}
	
	/* Create update thread */
	update_thread_id = k_thread_create(&update_thread,
					    update_thread_stack,
					    K_THREAD_STACK_SIZEOF(update_thread_stack),
					    firmware_update_thread,
					    NULL, NULL, NULL,
					    K_PRIO_PREEMPT(8), 0, K_NO_WAIT);
	
	if (update_thread_id == NULL) {
		LOG_ERR("Failed to create update thread");
		update_in_progress = false;
		if (ui_is_obj_valid(fw_update_btn, UI_SCREEN_MAX32664_UPDATE)) {
			lv_obj_clear_state(fw_update_btn, LV_STATE_DISABLED);
		}
	}
}

/**
 * @brief Update screen content (file scan, status)
 * 
 * Called when screen is created or refreshed.
 * NOTE: File scan is done ONCE when screen is created, not on every update.
 */
static void update_max32664_update_screen(void)
{
	/* Check if screen exists */
	if (ui_get_screen(UI_SCREEN_MAX32664_UPDATE) == NULL) {
		return;
	}
	
	/* Don't re-scan if we already found a file (avoids mount looping) */
	if (fw_file_found && msbl_filename[0] != '\0') {
		/* File already scanned, just update UI state */
		const char *filename = strrchr(msbl_filename, '/');
		filename = filename ? filename + 1 : msbl_filename;
		
		if (ui_is_obj_valid(fw_file_label, UI_SCREEN_MAX32664_UPDATE)) {
			lv_label_set_text(fw_file_label, filename);
			lv_obj_set_style_text_color(fw_file_label, NS_COLOR_NORMAL,
						    LV_PART_MAIN);
		}
		
		if (ui_is_obj_valid(fw_update_btn, UI_SCREEN_MAX32664_UPDATE) &&
		    !update_in_progress) {
			lv_obj_clear_state(fw_update_btn, LV_STATE_DISABLED);
		}
		
		if (ui_is_obj_valid(fw_status_label, UI_SCREEN_MAX32664_UPDATE) &&
		    !update_in_progress) {
			lv_label_set_text(fw_status_label, "Ready");
			lv_obj_set_style_text_color(fw_status_label, NS_COLOR_TEXT,
						    LV_PART_MAIN);
		}
		return;
	}
	
	/* Probe QSPI app partition for .msbl (no FatFS) */
	int ret;

	ret = find_msbl_file(msbl_filename, sizeof(msbl_filename));
	fw_file_found = (ret == 0);
	
	if (fw_file_found) {
		if (ui_is_obj_valid(fw_file_label, UI_SCREEN_MAX32664_UPDATE)) {
			lv_label_set_text(fw_file_label, msbl_filename);
			lv_obj_set_style_text_color(fw_file_label, NS_COLOR_NORMAL,
						    LV_PART_MAIN);
		}
		
		if (ui_is_obj_valid(fw_update_btn, UI_SCREEN_MAX32664_UPDATE) &&
		    !update_in_progress) {
			lv_obj_clear_state(fw_update_btn, LV_STATE_DISABLED);
		}
		
		if (ui_is_obj_valid(fw_status_label, UI_SCREEN_MAX32664_UPDATE) &&
		    !update_in_progress) {
			lv_label_set_text(fw_status_label, "Ready");
			lv_obj_set_style_text_color(fw_status_label, NS_COLOR_TEXT,
						    LV_PART_MAIN);
		}
	} else {
		if (ui_is_obj_valid(fw_file_label, UI_SCREEN_MAX32664_UPDATE)) {
			lv_label_set_text(fw_file_label, "No .msbl in QSPI app");
			lv_obj_set_style_text_color(fw_file_label, NS_COLOR_CRITICAL,
						    LV_PART_MAIN);
		}
		
		if (ui_is_obj_valid(fw_update_btn, UI_SCREEN_MAX32664_UPDATE)) {
			lv_obj_add_state(fw_update_btn, LV_STATE_DISABLED);
		}
		
		if (ui_is_obj_valid(fw_status_label, UI_SCREEN_MAX32664_UPDATE)) {
			lv_label_set_text(fw_status_label, "Program .msbl to QSPI app slot");
			lv_obj_set_style_text_color(fw_status_label, NS_COLOR_WARNING,
						    LV_PART_MAIN);
		}
	}
	
	/* Reset progress bar */
	if (ui_is_obj_valid(fw_progress_bar, UI_SCREEN_MAX32664_UPDATE)) {
		lv_bar_set_value(fw_progress_bar, 0, LV_ANIM_OFF);
	}
	
	/* NOTE: Firmware version is read ONCE in screen_create(), not on every refresh */
}

/**
 * @brief Create MAX32664 firmware update screen
 * 
 * @param parent Parent screen object
 */
void max32664_update_screen_create(lv_obj_t *parent)
{
	if (parent == NULL) {
		LOG_ERR("Failed to create MAX32664 update screen - parent is NULL");
		return;
	}
	
	/* Reset state */
	update_in_progress = false;
	memset(msbl_filename, 0, sizeof(msbl_filename));
	fw_file_found = false;
	
	/* Brand background */
	ns_apply_screen_bg(parent);
	
	/* Title (placed below the global status strip by the helper) */
	lv_obj_t *title = ns_make_screen_title(parent, MAX32664_UPDATE_SCREEN_NAME);
	if (title == NULL) {
		LOG_ERR("Failed to create title label - out of memory");
		ui_delete_screen(UI_SCREEN_MAX32664_UPDATE);
		return;
	}
	
	/* Firmware version label */
	fw_version_label = lv_label_create(parent);
	if (fw_version_label == NULL) {
		LOG_ERR("Failed to create version label - out of memory");
		ui_delete_screen(UI_SCREEN_MAX32664_UPDATE);
		return;
	}
	lv_label_set_text(fw_version_label, "Reading version...");
	lv_obj_set_style_text_color(fw_version_label, NS_COLOR_TEXT, LV_PART_MAIN);
	lv_obj_align(fw_version_label, LV_ALIGN_TOP_MID, 0, 58);
	
	/* Firmware file label */
	fw_file_label = lv_label_create(parent);
	if (fw_file_label == NULL) {
		LOG_ERR("Failed to create file label - out of memory");
		ui_delete_screen(UI_SCREEN_MAX32664_UPDATE);
		return;
	}
	lv_label_set_text(fw_file_label, "Scanning...");
	lv_obj_set_style_text_color(fw_file_label, NS_COLOR_TEXT, LV_PART_MAIN);
	lv_obj_set_width(fw_file_label, 220);  /* Allow text wrapping for long names */
	lv_label_set_long_mode(fw_file_label, LV_LABEL_LONG_WRAP);
	lv_obj_align(fw_file_label, LV_ALIGN_TOP_MID, 0, 80);
	
	/* Progress bar */
	fw_progress_bar = lv_bar_create(parent);
	if (fw_progress_bar == NULL) {
		LOG_ERR("Failed to create progress bar - out of memory");
		ui_delete_screen(UI_SCREEN_MAX32664_UPDATE);
		return;
	}
	lv_obj_set_size(fw_progress_bar, 200, 20);
	lv_obj_align(fw_progress_bar, LV_ALIGN_TOP_MID, 0, 128);
	lv_bar_set_range(fw_progress_bar, 0, 100);
	lv_bar_set_value(fw_progress_bar, 0, LV_ANIM_OFF);
	lv_obj_set_style_bg_color(fw_progress_bar, NS_COLOR_CARD, LV_PART_MAIN);
	lv_obj_set_style_bg_color(fw_progress_bar, NS_COLOR_NORMAL, LV_PART_INDICATOR);
	
	/* Update button */
	fw_update_btn = lv_btn_create(parent);
	if (fw_update_btn == NULL) {
		LOG_ERR("Failed to create update button - out of memory");
		ui_delete_screen(UI_SCREEN_MAX32664_UPDATE);
		return;
	}
	lv_obj_set_size(fw_update_btn, 140, 45);
	lv_obj_align(fw_update_btn, LV_ALIGN_TOP_MID, 0, 162);
	lv_obj_add_event_cb(fw_update_btn, max32664_update_btn_cb, LV_EVENT_CLICKED, NULL);
	
	fw_update_btn_label = lv_label_create(fw_update_btn);
	if (fw_update_btn_label == NULL) {
		LOG_ERR("Failed to create button label - out of memory");
		ui_delete_screen(UI_SCREEN_MAX32664_UPDATE);
		return;
	}
	lv_label_set_text(fw_update_btn_label, "Update Firmware");
	lv_obj_center(fw_update_btn_label);
	
	/* Status label */
	fw_status_label = lv_label_create(parent);
	if (fw_status_label == NULL) {
		LOG_ERR("Failed to create status label - out of memory");
		ui_delete_screen(UI_SCREEN_MAX32664_UPDATE);
		return;
	}
	lv_label_set_text(fw_status_label, "");
	lv_obj_set_style_text_color(fw_status_label, NS_COLOR_TEXT, LV_PART_MAIN);
	lv_obj_set_width(fw_status_label, 220);
	lv_label_set_long_mode(fw_status_label, LV_LABEL_LONG_WRAP);
	lv_obj_align(fw_status_label, LV_ALIGN_TOP_MID, 0, 215);
	
	/* Read firmware version ONCE at screen creation (not in refresh loop) */
	update_firmware_version_display();
	
	/* Initial screen update (scans for .msbl file) */
	update_max32664_update_screen();
	
	LOG_INF("MAX32664 update screen created");
}

/**
 * @brief Async callback to refresh the firmware version label
 *
 * Called via lv_async_call from the update thread after a post-update power cycle.
 */
static void refresh_version_label_cb(void *param)
{
	ARG_UNUSED(param);
	update_firmware_version_display();
}

/**
 * @brief Refresh MAX32664 update screen (public API)
 *
 * Call after file system changes to rescan for firmware files.
 */
void max32664_update_screen_refresh(void)
{
	/* Only update if screen exists */
	if (ui_get_screen(UI_SCREEN_MAX32664_UPDATE) != NULL) {
		update_max32664_update_screen();
	}
}

/**
 * @brief Destroy MAX32664 update screen (public API)
 * 
 * Called when user navigates away from the screen.
 * Re-enables USB if it was disabled by this screen.
 */
void max32664_update_screen_destroy(void)
{
	/* Drop pending LVGL refresh callbacks before the widget tree is destroyed. */
	max32664_ui_detach_widgets();

	/* Re-enable USB if user navigates away before update completes */
	cleanup_usb_state();
	
	msbl_filename[0] = '\0';
	fw_file_found = false;
	/* Leave update_in_progress unchanged if a background flash is still running. */
	
	LOG_INF("MAX32664 update screen destroyed");
}
