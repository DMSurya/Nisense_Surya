/**
 * @file ota_progress_ui.c
 * @brief Minimal LVGL overlay for BLE OTA transfer progress.
 */

#include "ota_progress_ui.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>

LOG_MODULE_REGISTER(ota_progress_ui, CONFIG_LOG_DEFAULT_LEVEL);

#if defined(CONFIG_APP_FEATURE_DISPLAY) && defined(CONFIG_LVGL)

#include <lvgl.h>
#include "ui_theme.h"
#include "resource_fonts.h"
#include "power_mgr.h"

static lv_obj_t *s_overlay;
static lv_obj_t *s_title;
static lv_obj_t *s_bar;
static lv_obj_t *s_detail;
static bool s_active;
static uint8_t s_pct;
static char s_phase[32];
static char s_detail_buf[48];

struct ota_ui_msg {
	uint8_t kind; /* 0=begin, 1=set, 2=end */
	uint8_t pct;
	bool success;
	char text[48];
};

static void apply_begin(void)
{
	if (s_overlay == NULL) {
		s_overlay = lv_obj_create(lv_layer_top());
		lv_obj_set_size(s_overlay, LV_PCT(100), LV_PCT(100));
		lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x101418), 0);
		lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
		lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);

		s_title = lv_label_create(s_overlay);
		lv_obj_set_style_text_font(s_title, UI_FONT_20, 0);
		lv_obj_set_style_text_color(s_title, lv_color_hex(0xE8EEF2), 0);
		lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 36);

		s_bar = lv_bar_create(s_overlay);
		lv_obj_set_size(s_bar, 200, 14);
		lv_obj_align(s_bar, LV_ALIGN_CENTER, 0, 0);
		lv_bar_set_range(s_bar, 0, 100);
		lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);

		s_detail = lv_label_create(s_overlay);
		lv_obj_set_style_text_font(s_detail, UI_FONT_14, 0);
		lv_obj_set_style_text_color(s_detail, lv_color_hex(0xA8B4BC), 0);
		lv_obj_align(s_detail, LV_ALIGN_BOTTOM_MID, 0, -40);
	}

	lv_label_set_text(s_title, s_phase[0] ? s_phase : "Updating…");
	lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
	lv_label_set_text(s_detail, "Receiving…");
	lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
	s_active = true;
	power_mgr_activity_notify();
}

static void apply_set(void)
{
	char line[64];

	if (!s_active || s_overlay == NULL) {
		return;
	}
	lv_bar_set_value(s_bar, s_pct, LV_ANIM_OFF);
	if (s_detail_buf[0] != '\0') {
		snprintf(line, sizeof(line), "%s  %u%%", s_detail_buf, s_pct);
	} else {
		snprintf(line, sizeof(line), "%u%%", s_pct);
	}
	lv_label_set_text(s_detail, line);
	power_mgr_activity_notify();
}

static void apply_end(bool success, const char *msg)
{
	if (s_overlay == NULL) {
		s_active = false;
		return;
	}
	if (msg && msg[0]) {
		lv_label_set_text(s_detail, msg);
	}
	lv_bar_set_value(s_bar, success ? 100 : s_pct, LV_ANIM_OFF);
	/* Keep visible briefly then hide — caller may immediately begin next phase. */
	lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
	s_active = false;
}

static void ui_async_cb(void *param)
{
	struct ota_ui_msg *m = param;

	if (m == NULL) {
		return;
	}
	switch (m->kind) {
	case 0:
		strncpy(s_phase, m->text, sizeof(s_phase) - 1);
		s_phase[sizeof(s_phase) - 1] = '\0';
		apply_begin();
		break;
	case 1:
		s_pct = m->pct;
		strncpy(s_detail_buf, m->text, sizeof(s_detail_buf) - 1);
		s_detail_buf[sizeof(s_detail_buf) - 1] = '\0';
		apply_set();
		break;
	case 2:
		apply_end(m->success, m->text);
		break;
	default:
		break;
	}
	k_free(m);
}

static void post_msg(uint8_t kind, uint8_t pct, bool success, const char *text)
{
	struct ota_ui_msg *m = k_malloc(sizeof(*m));

	if (m == NULL) {
		return;
	}
	memset(m, 0, sizeof(*m));
	m->kind = kind;
	m->pct = pct;
	m->success = success;
	if (text) {
		strncpy(m->text, text, sizeof(m->text) - 1);
	}
	lv_async_call(ui_async_cb, m);
}

void ota_progress_ui_begin(const char *phase)
{
	post_msg(0, 0, false, phase ? phase : "Updating…");
}

void ota_progress_ui_set(uint8_t percent, const char *detail)
{
	if (percent > 100U) {
		percent = 100U;
	}
	post_msg(1, percent, false, detail ? detail : "");
}

void ota_progress_ui_end(bool success, const char *msg)
{
	post_msg(2, success ? 100U : 0U, success, msg ? msg : "");
}

bool ota_progress_ui_is_active(void)
{
	return s_active;
}

#else /* !DISPLAY */

void ota_progress_ui_begin(const char *phase)
{
	LOG_INF("OTA begin: %s", phase ? phase : "?");
}

void ota_progress_ui_set(uint8_t percent, const char *detail)
{
	LOG_DBG("OTA %u%% %s", percent, detail ? detail : "");
}

void ota_progress_ui_end(bool success, const char *msg)
{
	LOG_INF("OTA end success=%d %s", (int)success, msg ? msg : "");
}

bool ota_progress_ui_is_active(void)
{
	return false;
}

#endif /* CONFIG_APP_FEATURE_DISPLAY && CONFIG_LVGL */

#if defined(CONFIG_MCUMGR_GRP_IMG) && defined(CONFIG_MCUMGR_MGMT_NOTIFICATION_HOOKS)

#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>

static enum mgmt_cb_return ota_img_mgmt_cb(uint32_t event,
					   enum mgmt_cb_return prev_status,
					   int32_t *rc, uint16_t *group,
					   bool *abort_more, void *data,
					   size_t data_size)
{
	ARG_UNUSED(prev_status);
	ARG_UNUSED(rc);
	ARG_UNUSED(group);
	ARG_UNUSED(abort_more);
	ARG_UNUSED(data);
	ARG_UNUSED(data_size);

	switch (event) {
	case MGMT_EVT_OP_IMG_MGMT_DFU_STARTED:
		ota_progress_ui_begin("Firmware");
		ota_progress_ui_set(0, "SMP upload");
		break;
	case MGMT_EVT_OP_IMG_MGMT_DFU_STOPPED:
		ota_progress_ui_end(false, "Firmware stopped");
		break;
	case MGMT_EVT_OP_IMG_MGMT_DFU_PENDING:
		ota_progress_ui_set(100, "Pending reboot");
		break;
	case MGMT_EVT_OP_IMG_MGMT_DFU_CONFIRMED:
		ota_progress_ui_end(true, "Firmware confirmed");
		break;
	default:
		break;
	}
	return MGMT_CB_OK;
}

static struct mgmt_callback s_img_cb = {
	.callback = ota_img_mgmt_cb,
	.event_id = (MGMT_EVT_OP_IMG_MGMT_DFU_STARTED |
		     MGMT_EVT_OP_IMG_MGMT_DFU_STOPPED |
		     MGMT_EVT_OP_IMG_MGMT_DFU_PENDING |
		     MGMT_EVT_OP_IMG_MGMT_DFU_CONFIRMED),
};

void ota_progress_smp_hooks_init(void)
{
	mgmt_callback_register(&s_img_cb);
	LOG_INF("OTA SMP img_mgmt progress hooks registered");
}

#elif defined(CONFIG_MCUMGR_GRP_IMG)

void ota_progress_smp_hooks_init(void)
{
	LOG_INF("OTA SMP: enable CONFIG_MCUMGR_MGMT_NOTIFICATION_HOOKS for device progress");
}

#else

void ota_progress_smp_hooks_init(void)
{
}

#endif
