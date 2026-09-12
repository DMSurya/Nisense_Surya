/* =============================================================================
 * XIP Font Management Implementation
 * =============================================================================
 * 14 / 20 pt from one subset TTF in NOR (TinyTTF, cache_size=0).
 * UI_FONT_LARGE aliases medium — a separate larger face OOMs the LVGL pool
 * (~31 KB) and blanks the UI after BLE-connect glyph churn.
 * ============================================================================= */

#include "resource_fonts.h"
#include "resource.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

#if defined(CONFIG_APP_FONT_PROFILE_RESOURCE_RUNTIME) && \
    defined(CONFIG_RESOURCE_FONTS) && defined(CONFIG_LV_USE_TINY_TTF)

#include <lvgl.h>
#include <libs/tiny_ttf/lv_tiny_ttf.h>

LOG_MODULE_REGISTER(resource_fonts, LOG_LEVEL_INF);

#define RESOURCE_ID_FONT_TTF   RESOURCE_ID_FONT_SMALL

/* 0 = direct glyph alloc/free per draw (no LRU cache in LVGL pool). */
#define XIP_TINY_TTF_CACHE_PER_FONT 0

static lv_font_t *font_small;
static lv_font_t *font_medium;
static bool fonts_initialized;

static const void *ttf_src;
static size_t ttf_src_size;

#if defined(CONFIG_RESOURCE_FONTS_RAM_CACHE)
/* Static RAM copy of the subset TTF; keeps glyph reads off QSPI. */
static uint8_t ttf_ram_cache[CONFIG_RESOURCE_FONTS_RAM_CACHE_SIZE];
#endif

static lv_font_t *create_ttf_font(uint32_t pt)
{
	return lv_tiny_ttf_create_data_ex(ttf_src, ttf_src_size, pt,
					  LV_FONT_KERNING_NONE,
					  XIP_TINY_TTF_CACHE_PER_FONT);
}

int resource_fonts_init(void)
{
	if (fonts_initialized) {
		return 0;
	}

	if (!resource_is_valid()) {
		LOG_ERR("Resources not valid - cannot load fonts");
		return -ENOENT;
	}

	/* Verified read: CRC-checked with retry. A corrupt/garbled TTF would
	 * otherwise render broken glyphs with no recovery until reboot. */
	ttf_src = resource_get_verified(RESOURCE_ID_FONT_TTF, &ttf_src_size);
	if (ttf_src == NULL || ttf_src_size == 0U) {
		LOG_ERR("TTF font asset not found or failed CRC (ID 0x%04x)",
			RESOURCE_ID_FONT_TTF);
		return -ENOENT;
	}

#if defined(CONFIG_RESOURCE_FONTS_RAM_CACHE)
	/* Copy the subset TTF into RAM so glyph rasterisation never reads QSPI
	 * mid-draw (immune to record-store/model-update contention and DPD wake). */
	if (ttf_src_size <= sizeof(ttf_ram_cache)) {
		memcpy(ttf_ram_cache, ttf_src, ttf_src_size);
		ttf_src = ttf_ram_cache;
		LOG_INF("TTF cached in RAM (%zu bytes), rasterising from RAM", ttf_src_size);
	} else {
		LOG_WRN("TTF %zu bytes > RAM cache %zu; reading from XIP directly",
			ttf_src_size, sizeof(ttf_ram_cache));
	}
#endif

	LOG_INF("TTF font source %p (%zu bytes), cacheless TinyTTF", ttf_src, ttf_src_size);

	font_small = create_ttf_font(RESOURCE_FONT_SIZE_SMALL);
	if (font_small == NULL) {
		LOG_ERR("Failed to create %d pt font", RESOURCE_FONT_SIZE_SMALL);
		resource_fonts_deinit();
		return -ENOMEM;
	}

	font_medium = create_ttf_font(RESOURCE_FONT_SIZE_MEDIUM);
	if (font_medium == NULL) {
		LOG_ERR("Failed to create %d pt font", RESOURCE_FONT_SIZE_MEDIUM);
		resource_fonts_deinit();
		return -ENOMEM;
	}

	fonts_initialized = true;
	LOG_INF("XIP TinyTTF ready: %d / %d pt (large aliases medium)",
		RESOURCE_FONT_SIZE_SMALL, RESOURCE_FONT_SIZE_MEDIUM);
	return 0;
}

void resource_fonts_prewarm(void)
{
	/* Intentionally empty: prewarm with cache_size=0 only spikes LVGL heap at boot. */
}

bool resource_fonts_is_ready(void)
{
	return fonts_initialized;
}

const void *resource_font_small(void)
{
	return font_small ? font_small : &lv_font_montserrat_14;
}

const void *resource_font_medium(void)
{
	return font_medium ? font_medium : resource_font_small();
}

const void *resource_font_large(void)
{
	return resource_font_medium();
}

const void *resource_font_get(uint32_t size)
{
	if (size <= 16U) {
		return resource_font_small();
	}
	return resource_font_medium();
}

void resource_fonts_deinit(void)
{
	if (font_medium) {
		lv_tiny_ttf_destroy(font_medium);
		font_medium = NULL;
	}
	if (font_small) {
		lv_tiny_ttf_destroy(font_small);
		font_small = NULL;
	}

	ttf_src = NULL;
	ttf_src_size = 0U;
	fonts_initialized = false;
}

#else /* stub when disabled */

int resource_fonts_init(void) { return 0; }
void resource_fonts_prewarm(void) { }
bool resource_fonts_is_ready(void) { return false; }
const void *resource_font_small(void) { return NULL; }
const void *resource_font_medium(void) { return NULL; }
const void *resource_font_large(void) { return NULL; }
const void *resource_font_get(uint32_t size) { (void)size; return NULL; }
void resource_fonts_deinit(void) { }

#endif
