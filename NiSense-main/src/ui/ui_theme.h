/* ui_theme.h – NiSense brand color system & shared UI helpers
 *
 * Single source of truth for the NiSense visual identity. Derived from the
 * brand logo (deep medical blue + health green) so the boot screen, dashboard
 * and every device screen feel like one ecosystem.
 *
 * Design rules:
 *   - Blue + green dominate ~80-90% of the UI (trust, health, AI).
 *   - Red / orange / yellow are reserved STRICTLY for alerts and medical
 *     severity, never for decoration.
 *   - Use the NS_COLOR_* tokens everywhere instead of raw lv_color_hex().
 */

#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Primary brand palette
 * ============================================================================= */
#define NS_RGB_DEEP_BLUE     0x0B1E9A  /* Deep brand blue   */
#define NS_RGB_PRIMARY       0x1E4DFF  /* Primary blue      */
#define NS_RGB_SECONDARY     0x00C8FF  /* Cyan accent       */
#define NS_RGB_HEALTH        0x7ED321  /* Health green      */
#define NS_RGB_VITAL         0x58C41A  /* Vital green       */
#define NS_RGB_WHITE         0xFFFFFF

/* =============================================================================
 * Dark theme surfaces (match the boot logo background)
 * ============================================================================= */
#define NS_RGB_BG            0x040C24  /* Screen background  */
#define NS_RGB_CARD          0x122158  /* Card surface       */
#define NS_RGB_BORDER        0x1E4DFF  /* Card border = brand primary */
#define NS_RGB_TEXT          0xFFFFFF  /* Primary text       */
#define NS_RGB_TEXT_DIM      0xA8B0D3  /* Secondary text     */

/* =============================================================================
 * Status / severity colors (use the SAME system everywhere)
 * ============================================================================= */
#define NS_RGB_NORMAL        0x00C853  /* Normal / OK        */
#define NS_RGB_WARNING       0xFFD600  /* Attention          */
#define NS_RGB_RISK          0xFF6D00  /* High risk          */
#define NS_RGB_CRITICAL      0xD50000  /* Critical           */
#define NS_RGB_INFO          0x00C8FF  /* Processing / info  */

/* =============================================================================
 * Per-parameter accent colors (consistent across every screen)
 * ============================================================================= */
#define NS_RGB_HR            0xFF4A4A  /* Heart rate         */
#define NS_RGB_SPO2          0x00A6FF  /* SpO2               */
#define NS_RGB_GLUCOSE       0xFF9F1A  /* Glucose            */
#define NS_RGB_HB            0x8A2EFF  /* Hemoglobin         */
#define NS_RGB_TEMP          0x00D46A  /* Temperature        */
#define NS_RGB_RESP          0x00C8C8  /* Respiratory        */
#define NS_RGB_HRV           0x4DD2FF  /* HRV                */
#define NS_RGB_BP_SYS        0xFF4D4D  /* BP systolic        */
#define NS_RGB_BP_DIA        0x2D7FFF  /* BP diastolic       */

/* =============================================================================
 * lv_color_t convenience wrappers
 * ============================================================================= */
#define NS_COLOR_PRIMARY     lv_color_hex(NS_RGB_PRIMARY)
#define NS_COLOR_SECONDARY   lv_color_hex(NS_RGB_SECONDARY)
#define NS_COLOR_HEALTH      lv_color_hex(NS_RGB_HEALTH)
#define NS_COLOR_BG          lv_color_hex(NS_RGB_BG)
#define NS_COLOR_CARD        lv_color_hex(NS_RGB_CARD)
#define NS_COLOR_BORDER      lv_color_hex(NS_RGB_BORDER)
#define NS_COLOR_TEXT        lv_color_hex(NS_RGB_TEXT)
#define NS_COLOR_TEXT_DIM    lv_color_hex(NS_RGB_TEXT_DIM)

#define NS_COLOR_NORMAL      lv_color_hex(NS_RGB_NORMAL)
#define NS_COLOR_WARNING     lv_color_hex(NS_RGB_WARNING)
#define NS_COLOR_RISK        lv_color_hex(NS_RGB_RISK)
#define NS_COLOR_CRITICAL    lv_color_hex(NS_RGB_CRITICAL)
#define NS_COLOR_INFO        lv_color_hex(NS_RGB_INFO)

#define NS_COLOR_HR          lv_color_hex(NS_RGB_HR)
#define NS_COLOR_SPO2        lv_color_hex(NS_RGB_SPO2)
#define NS_COLOR_GLUCOSE     lv_color_hex(NS_RGB_GLUCOSE)
#define NS_COLOR_HB          lv_color_hex(NS_RGB_HB)
#define NS_COLOR_TEMP        lv_color_hex(NS_RGB_TEMP)
#define NS_COLOR_RESP        lv_color_hex(NS_RGB_RESP)
#define NS_COLOR_HRV         lv_color_hex(NS_RGB_HRV)
#define NS_COLOR_BP_SYS      lv_color_hex(NS_RGB_BP_SYS)
#define NS_COLOR_BP_DIA      lv_color_hex(NS_RGB_BP_DIA)

/* =============================================================================
 * Severity levels for ns_status_color()
 * ============================================================================= */
enum ns_status {
	NS_STATUS_NORMAL = 0,
	NS_STATUS_ATTENTION,
	NS_STATUS_RISK,
	NS_STATUS_CRITICAL,
	NS_STATUS_INFO,
	NS_STATUS_NEUTRAL,
};

/**
 * @brief Map a severity level to its brand status color.
 */
lv_color_t ns_status_color(enum ns_status level);

/**
 * @brief Apply the standard NiSense dark background to a screen object.
 */
void ns_apply_screen_bg(lv_obj_t *screen);

/**
 * @brief Create a branded parameter card.
 *
 * Builds a rounded card (NiSense card surface + border) laid out as:
 *   [icon]  NAME            VALUE
 *                           (status dot)
 *
 * All sub-widgets are created on @p parent. The returned struct exposes the
 * label handles so callers can update value/status at runtime. Pass NULL for
 * @p icon_src to leave the icon slot empty (filled later by the XIP icons).
 *
 * @param parent      Parent object (screen or scrollable container).
 * @param name        Static parameter name (e.g. "Heart Rate").
 * @param accent      Accent color for the value text and icon tint.
 * @param icon_src    Optional lv_image_dsc_t* for the parameter icon (or NULL).
 * @param out         Filled with the created widget handles (must be non-NULL).
 */
struct ns_card {
	lv_obj_t *card;        /* root card container         */
	lv_obj_t *icon;        /* lv_image (may be NULL)       */
	lv_obj_t *name_label;  /* parameter name              */
	lv_obj_t *value_label; /* big value text              */
	lv_obj_t *status_dot;  /* small colored status circle */
};

/**
 * @brief Create a result tile matching the old home 2×2 health tiles (110×62).
 *
 * Layout (icon left, text right — same as legacy home):
 *   [icon]  NAME              (dot)
 *           VALUE
 */
void ns_make_result_tile(lv_obj_t *parent, const char *name, lv_color_t accent,
			 const void *icon_src, struct ns_card *out);

/**
 * @brief Create a 2×2 flex wrap container for result tiles.
 *
 * @return Container sized for four 110×62 tiles, or NULL on failure.
 */
lv_obj_t *ns_make_result_grid(lv_obj_t *parent);

/**
 * @brief Update a card's status dot color.
 */
void ns_card_set_status(struct ns_card *card, enum ns_status level);

/**
 * @brief Create a standard brand screen title.
 *
 * Brand-cyan (NS_COLOR_SECONDARY), UI_FONT_20, centered at the top just below
 * the global wearable status strip (UI_SHELL_TOP_INSET) so it never overlaps
 * the time/battery/connectivity icons. Returns the label handle so callers can
 * re-align or keep a reference; returns NULL if @p parent is NULL or allocation
 * fails.
 *
 * @param parent  Screen (or container) to create the title on.
 * @param text    Static title string.
 * @return Created lv_label, or NULL on failure.
 */
lv_obj_t *ns_make_screen_title(lv_obj_t *parent, const char *text);

#ifdef __cplusplus
}
#endif
