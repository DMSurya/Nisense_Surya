/* =============================================================================
 * XIP Font Management
 * =============================================================================
 * 14 / 20 pt from one subset TTF in NOR flash via TinyTTF.
 * UI_FONT_LARGE aliases 20 pt (a third larger face OOMs the LVGL pool).
 * Onboard flash keeps only the LVGL default 14pt Montserrat as a boot fallback.
 *
 * The TTF file is stored in the Resources partition and accessed via
 * memory-mapped XIP addressing (0x120F4000+).
 *
 * Target: nRF52840 (Raytac MDBT50Q-DB-40)
 * Created: 2026-02-24
 * ============================================================================= */

#ifndef RESOURCE_FONTS_H
#define RESOURCE_FONTS_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/autoconf.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Font Configuration
 * ============================================================================= */

/** Font sizes that will be created from the TTF file */
#define RESOURCE_FONT_SIZE_SMALL     14
#define RESOURCE_FONT_SIZE_MEDIUM    20
/*
 * Large size is an alias of medium for this build: a third TinyTTF face at
 * 28/36 pt exhausts CONFIG_LV_Z_MEM_POOL_SIZE (~31 KB) and leaves the UI
 * blank / unresponsive after BLE connect glyph churn.
 */
#define RESOURCE_FONT_SIZE_LARGE     RESOURCE_FONT_SIZE_MEDIUM

/* =============================================================================
 * Font Selection Macros
 * 
 * These macros provide compatibility between XIP fonts and compiled fonts.
 * UI code should use these instead of directly referencing lv_font_montserrat_*.
 * ============================================================================= */

#if defined(CONFIG_APP_FONT_PROFILE_RESOURCE_RUNTIME) && \
	defined(CONFIG_RESOURCE_FONTS) && defined(CONFIG_LV_USE_TINY_TTF)
/* XIP fonts (TinyTTF) - fonts loaded from external flash at runtime */
#define UI_FONT_SMALL   ((const lv_font_t *)resource_font_small())
#define UI_FONT_MEDIUM  ((const lv_font_t *)resource_font_medium())
#define UI_FONT_LARGE   ((const lv_font_t *)resource_font_large())
#define UI_FONT_14      UI_FONT_SMALL
#define UI_FONT_20      UI_FONT_MEDIUM
#define UI_FONT_28      UI_FONT_LARGE
#else
/* Compiled fonts (Montserrat from LVGL) */
#include <lvgl.h>
#define UI_FONT_SMALL   (&lv_font_montserrat_14)
#define UI_FONT_MEDIUM  (&lv_font_montserrat_20)
#define UI_FONT_LARGE   (&lv_font_montserrat_28)
#define UI_FONT_CLOCK   (&lv_font_montserrat_48)
#define UI_FONT_14      UI_FONT_SMALL
#define UI_FONT_20      UI_FONT_MEDIUM
#define UI_FONT_28      UI_FONT_LARGE
#endif

#if !defined(UI_FONT_CLOCK)
/* XIP TinyTTF: large aliases medium — clock uses the biggest available face. */
#define UI_FONT_CLOCK   UI_FONT_LARGE
#endif

/* =============================================================================
 * API Functions
 * ============================================================================= */

/**
 * @brief Initialize XIP fonts from external flash
 * 
 * Loads Montserrat TTF from Resources partition and creates
 * LVGL font objects for sizes 14, 20, 28.
 * 
 * @return 0 on success, negative errno on failure
 */
int resource_fonts_init(void);

/**
 * @brief Check if XIP fonts are initialized
 * @return true if fonts are ready for use
 */
bool resource_fonts_is_ready(void);

/**
 * @brief Get small font (14pt)
 * @return Pointer to lv_font_t or NULL if not loaded
 */
const void *resource_font_small(void);

/**
 * @brief Get medium font (20pt)
 * @return Pointer to lv_font_t or NULL if not loaded
 */
const void *resource_font_medium(void);

/**
 * @brief Get large font (aliases medium — no separate TinyTTF face)
 * @return Pointer to lv_font_t or NULL if not loaded
 */
const void *resource_font_large(void);

/**
 * @brief Get font by size
 * @param size Font size in points (14 or 20+)
 * @return Pointer to lv_font_t or NULL if not loaded
 */
const void *resource_font_get(uint32_t size);

/**
 * @brief Pre-cache common glyphs in the TinyTTF 14pt font (reduces stb churn).
 *
 * Call once after resource_fonts_init() on the LVGL thread.
 */
void resource_fonts_prewarm(void);

/**
 * @brief Deinitialize XIP fonts and free resources
 */
void resource_fonts_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* RESOURCE_FONTS_H */
