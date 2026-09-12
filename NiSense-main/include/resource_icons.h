/* resource_icons.h - Runtime-resolved XIP parameter icons
 *
 * Builds LVGL image descriptors for the per-parameter UI icons stored in the
 * external XIP flash asset table. Descriptors are resolved at runtime (no
 * hardcoded offsets) so they stay valid regardless of asset table layout.
 *
 * Call resource_icons_init() once after resource_init(). Then pass the result of
 * xip_icon(asset_id) as the icon_src to ns_make_param_card(). If an icon is not
 * present in the flashed asset binary, xip_icon() returns NULL and the card
 * simply renders without an icon.
 */

#pragma once

#include <lvgl.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Resolve all known icon descriptors from the Resource table.
 *        Safe to call multiple times. No-op if display feature is disabled.
 */
void resource_icons_init(void);

/**
 * @brief Native pixel width/height of icons in the Resource table.
 */
#define XIP_ICON_SRC_PX 40

/**
 * @brief Scale an lv_image widget to display_px from the 40x40 XIP source.
 *
 * Setting only lv_obj_set_size() without lv_image_set_scale() clips the bitmap
 * to the widget box and shows a corrupted top-left fragment.
 */
void resource_icon_apply_display_size(lv_obj_t *img, uint16_t display_px);

/**
 * @param asset_id One of RESOURCE_ID_ICON_* (see resource.h).
 * @return Pointer to a populated lv_image_dsc_t, or NULL if the icon is not
 *         available in the flashed asset binary.
 */
const lv_image_dsc_t *xip_icon(uint16_t asset_id);

#ifdef __cplusplus
}
#endif
