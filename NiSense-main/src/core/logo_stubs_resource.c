/* logo_stubs_resource.c - Resource logo descriptors (runtime-resolved)
 *
 * Provides `logo` (boot splash) and `logo_charge` (charging screen). Pixel
 * data lives in the Resource store (QSPI mmap). Descriptors are filled at
 * runtime from the asset table via resource_get_verified() / resource_get().
 *
 * resource_logo_init() must be called after resource_init() and before UI
 * creates the splash image or charger screen.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include <stdint.h>
#include "resource.h"
#include "resource_logo.h"

LOG_MODULE_REGISTER(resource_logo, LOG_LEVEL_INF);

#define LOGO_W   240
#define LOGO_H   240
#define LOGO_BPP 2
#define LOGO_STRIDE (LOGO_W * LOGO_BPP)

static bool resource_logo_resolve(uint16_t asset_id, lv_image_dsc_t *out, const char *tag)
{
	size_t asset_size = 0;
	/* Prefer CRC-verified read; after validate_all(), a DPD wake miss can still
	 * fail the re-check — fall back to table lookup (already CRC'd at boot). */
	const uint8_t *asset =
		(const uint8_t *)resource_get_verified(asset_id, &asset_size);

	if (asset == NULL) {
		resource_wake();
		asset = (const uint8_t *)resource_get(asset_id, &asset_size);
		if (asset != NULL) {
			LOG_WRN("%s verified CRC missed; using table pointer after wake",
				tag);
		}
	}

	const struct resource_image_header *hdr;
	const uint8_t *px;
	lv_color_format_t cf;
	uint32_t bpp;
	size_t palette_size;
	size_t payload_off;

	if (asset == NULL || asset_size < sizeof(struct resource_image_header)) {
		return false;
	}

	hdr = (const struct resource_image_header *)asset;
	switch (hdr->format) {
	case RESOURCE_IMAGE_FORMAT_RGB565:
		cf = LV_COLOR_FORMAT_RGB565;
		bpp = 2;
		break;
	case RESOURCE_IMAGE_FORMAT_ARGB8888:
		cf = LV_COLOR_FORMAT_ARGB8888;
		bpp = 4;
		break;
	default:
		LOG_WRN("Unsupported %s format %u in Resource header", tag, hdr->format);
		return false;
	}

	palette_size = (size_t)hdr->palette_entries * sizeof(uint32_t);
	payload_off = sizeof(struct resource_image_header) + palette_size;
	if (payload_off + hdr->data_size > asset_size) {
		LOG_WRN("%s payload out of bounds (off=%u size=%u asset=%u)",
			tag,
			(unsigned int)payload_off,
			(unsigned int)hdr->data_size,
			(unsigned int)asset_size);
		return false;
	}

	px = asset + payload_off;

	out->header.magic = LV_IMAGE_HEADER_MAGIC;
	out->header.cf = cf;
	out->header.flags = 0;
	out->header.w = hdr->width;
	out->header.h = hdr->height;
	out->header.stride = hdr->stride ? hdr->stride : ((uint32_t)hdr->width * bpp);
	out->data = px;
	out->data_size = hdr->data_size;

	LOG_INF("%s resolved @ %p (%ux%u fmt=%u stride=%u size=%u)",
		tag,
		px,
		hdr->width,
		hdr->height,
		hdr->format,
		out->header.stride,
		out->data_size);

	return true;
}

#if defined(CONFIG_UI_STARTUP_LOGO)
lv_image_dsc_t logo = {
	.header.magic = LV_IMAGE_HEADER_MAGIC,
	.header.cf = LV_COLOR_FORMAT_RGB565,
	.header.flags = 0,
	.header.w = LOGO_W,
	.header.h = LOGO_H,
	.header.stride = LOGO_STRIDE,
	.data = NULL,
	.data_size = 0};
#endif

#if defined(CONFIG_UI_SCREEN_CHARGER)
lv_image_dsc_t logo_charge = {
	.header.magic = LV_IMAGE_HEADER_MAGIC,
	.header.cf = LV_COLOR_FORMAT_RGB565,
	.header.flags = 0,
	.header.w = LOGO_W,
	.header.h = LOGO_H,
	.header.stride = LOGO_STRIDE,
	.data = NULL,
	.data_size = 0};

bool resource_logo_charge_ready(void)
{
	return logo_charge.data != NULL && logo_charge.data_size > 0U;
}
#endif

void resource_logo_init(void)
{
	resource_wake();

#if defined(CONFIG_UI_STARTUP_LOGO)
	if (!resource_logo_resolve(RESOURCE_ID_LOGO_BOOT, &logo, "Boot logo")) {
		LOG_ERR("Boot logo resolve failed — splash will be blank");
	}
#endif

#if defined(CONFIG_UI_SCREEN_CHARGER)
	if (!resource_logo_resolve(RESOURCE_ID_LOGO_CHARGE, &logo_charge, "Charge logo")) {
		LOG_WRN("Charge logo resolve failed — charger UI falls back to text");
	}
#endif
}
