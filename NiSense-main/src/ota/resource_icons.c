/* resource_icons.c - Runtime-resolved XIP parameter icons
 *
 * Fills LVGL image descriptors (ARGB8888) for the per-parameter icons stored in
 * the external XIP flash asset table. Resolved at runtime via
 * resource_get_image() so descriptors track the actual asset layout instead
 * of hardcoded offsets.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include <stdint.h>
#include "resource.h"
#include "resource_icons.h"

LOG_MODULE_REGISTER(resource_icons, LOG_LEVEL_INF);

#if defined(CONFIG_RESOURCE)

/* Icons are stored as ARGB8888 (4 bytes/pixel) by convert_logos_to_resource.py. */
#define ICON_BPP 4

/* Known icon asset IDs (must match resource.h and the converter map). */
static const uint16_t s_icon_ids[] = {
    RESOURCE_ID_ICON_HEART,
    RESOURCE_ID_ICON_SPO2,
    RESOURCE_ID_ICON_GLUCOSE,
    RESOURCE_ID_ICON_TEMP,
    RESOURCE_ID_ICON_BATTERY,
    RESOURCE_ID_ICON_BLUETOOTH,
    RESOURCE_ID_ICON_WIFI,
    RESOURCE_ID_ICON_SETTINGS,
    RESOURCE_ID_ICON_WARNING,
    RESOURCE_ID_ICON_CHECK,
    RESOURCE_ID_ICON_HEMOGLOBIN,
    RESOURCE_ID_ICON_RESP,
    RESOURCE_ID_ICON_INSULIN,
    RESOURCE_ID_ICON_HOMA,
    RESOURCE_ID_ICON_BATTERY_LOW,
    RESOURCE_ID_ICON_BATTERY_CRITICAL,
    RESOURCE_ID_ICON_BLUETOOTH_OFF,
    RESOURCE_ID_ICON_BLUETOOTH_ON,
    RESOURCE_ID_ICON_WIFI_OFF,
    RESOURCE_ID_ICON_WIFI_ON,
};

#define ICON_COUNT ((int)(sizeof(s_icon_ids) / sizeof(s_icon_ids[0])))

static lv_image_dsc_t s_icons[ICON_COUNT];
static bool s_ready;

void resource_icons_init(void)
{
    int resolved = 0;

    for (int i = 0; i < ICON_COUNT; i++) {
        uint16_t w = 0, h = 0;
        const void *px = resource_get_image(s_icon_ids[i], &w, &h);

        if (px == NULL || w == 0 || h == 0) {
            s_icons[i].data = NULL;
            continue;
        }

        s_icons[i].header.magic = LV_IMAGE_HEADER_MAGIC;
        s_icons[i].header.cf = LV_COLOR_FORMAT_ARGB8888;
        s_icons[i].header.flags = 0;
        s_icons[i].header.w = w;
        s_icons[i].header.h = h;
        s_icons[i].header.stride = (uint32_t)w * ICON_BPP;
        s_icons[i].data = (const uint8_t *)px;
        s_icons[i].data_size = (uint32_t)w * h * ICON_BPP;
        resolved++;
    }

    s_ready = true;
    LOG_INF("XIP icons resolved: %d/%d", resolved, ICON_COUNT);
}

const lv_image_dsc_t *xip_icon(uint16_t asset_id)
{
    if (!s_ready) {
        return NULL;
    }
    for (int i = 0; i < ICON_COUNT; i++) {
        if (s_icon_ids[i] == asset_id) {
            return s_icons[i].data ? &s_icons[i] : NULL;
        }
    }
    return NULL;
}

#define XIP_ICON_SRC_PX 40

void resource_icon_apply_display_size(lv_obj_t *img, uint16_t display_px)
{
	if (img == NULL || display_px == 0U) {
		return;
	}

	lv_obj_set_size(img, display_px, display_px);
	lv_image_set_pivot(img, 0, 0);
	lv_image_set_scale(img, (int32_t)((display_px * 256U) / XIP_ICON_SRC_PX));
}

#else /* !CONFIG_RESOURCE */

void resource_icons_init(void) { }

const lv_image_dsc_t *xip_icon(uint16_t asset_id)
{
    ARG_UNUSED(asset_id);
    return NULL;
}

void resource_icon_apply_display_size(lv_obj_t *img, uint16_t display_px)
{
    ARG_UNUSED(img);
    ARG_UNUSED(display_px);
}

#endif /* CONFIG_RESOURCE */
