/* =============================================================================
 * Resource Management Implementation
 * =============================================================================
 * Static assets (logos, fonts, icons) stored in external QSPI flash
 * with Execute-In-Place (XIP) memory-mapped access.
 *
 * Target: nRF52840 (Raytac MDBT50Q-DB-40)
 * SDK: nRF Connect SDK v3.0.0
 * Created: 2026-02-14
 * ============================================================================= */

#include "resource.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/crc.h>
#include <errno.h>
#include <string.h>

#ifdef CONFIG_SETTINGS
#include <zephyr/settings/settings.h>
#define RESOURCE_SLOT_SETTINGS_KEY "resource/slot"
#endif

LOG_MODULE_REGISTER(resource, CONFIG_RESOURCE_LOG_LEVEL);

/* =============================================================================
 * Private Data
 * ============================================================================= */

/** Cached pointer to partition header (XIP memory-mapped) */
static const struct resource_partition_header *partition_header;

/** Cached pointer to asset table (XIP memory-mapped) */
static const struct resource_entry *asset_table;

/** Flag indicating if Resources are valid and ready */
static bool xip_initialized;

/** Flag indicating if validation passed */
static bool xip_valid;

/** Flag indicating the partition-level CRC matched at init */
static bool xip_crc_ok;

/** Active A/B slot (persisted in NVS when SETTINGS enabled). */
static uint8_t active_slot = RESOURCE_SLOT_PRIMARY;

/* =============================================================================
 * Private Functions
 * ============================================================================= */

uintptr_t resource_slot_base(uint8_t slot)
{
	return (slot == RESOURCE_SLOT_SECONDARY)
		       ? (uintptr_t)RESOURCE_SECONDARY_BASE
		       : (uintptr_t)RESOURCE_PRIMARY_BASE;
}

uintptr_t resource_active_base(void)
{
	return resource_slot_base(active_slot);
}

/**
 * @brief Calculate CRC32 of data buffer
 */
static uint32_t calculate_crc32(const void *data, size_t size)
{
    return crc32_ieee((const uint8_t *)data, size);
}

/**
 * @brief CRC32 with one re-read retry.
 *
 * XIP reads hit QSPI directly (no data cache); if a read is momentarily
 * garbled (contention / DPD wake), recomputing re-fetches from flash. We only
 * trust a result confirmed by two consecutive reads that agree AND match the
 * expected CRC, else report failure so callers can fall back.
 */
static bool crc32_matches_retry(const void *data, size_t size, uint32_t expected)
{
    for (int attempt = 0; attempt < 2; attempt++) {
        /* NOR may have entered DPD (~10 ms idle); touch XIP map before CRC. */
        resource_wake();

        uint32_t c1 = calculate_crc32(data, size);

        if (c1 == expected) {
            return true;
        }
        /* Confirm it is a genuine mismatch (not a transient read) before failing. */
        resource_wake();
        uint32_t c2 = calculate_crc32(data, size);

        if (c1 == c2) {
            /* Stable mismatch — real corruption. */
            return false;
        }
        /* c1 != c2: transient read glitch; loop once more. */
    }
    return false;
}

/**
 * @brief Find asset entry by ID
 */
static const struct resource_entry *find_asset_by_id(uint16_t asset_id)
{
    if (!xip_valid || !asset_table) {
        return NULL;
    }

    uint32_t count = partition_header->asset_count;
    
    for (uint32_t i = 0; i < count && i < RESOURCE_MAX_ENTRIES; i++) {
        if (asset_table[i].magic == RESOURCE_ASSET_MAGIC &&
            asset_table[i].asset_id == asset_id) {
            return &asset_table[i];
        }
    }

    return NULL;
}

#ifdef CONFIG_SETTINGS
static int xip_slot_direct_load(const char *key, size_t len, settings_read_cb read_cb,
				void *cb_arg, void *param)
{
	ARG_UNUSED(key);
	uint8_t *out = param;

	if (len != sizeof(uint8_t)) {
		return -EINVAL;
	}
	ssize_t rc = read_cb(cb_arg, out, sizeof(uint8_t));

	return (rc < 0) ? (int)rc : 0;
}

static void load_active_slot(uint8_t *slot)
{
	uint8_t v = RESOURCE_SLOT_PRIMARY;

	(void)settings_load_subtree_direct(RESOURCE_SLOT_SETTINGS_KEY, xip_slot_direct_load, &v);
	if (v != RESOURCE_SLOT_PRIMARY && v != RESOURCE_SLOT_SECONDARY) {
		/* Migrate from legacy NVS key */
		v = RESOURCE_SLOT_PRIMARY;
		(void)settings_load_subtree_direct("resource/slot", xip_slot_direct_load, &v);
	}
	if (v == RESOURCE_SLOT_PRIMARY || v == RESOURCE_SLOT_SECONDARY) {
		*slot = v;
	}
}

static void save_active_slot(uint8_t slot)
{
	(void)settings_save_one(RESOURCE_SLOT_SETTINGS_KEY, &slot, sizeof(slot));
}
#else
static void load_active_slot(uint8_t *slot) { ARG_UNUSED(slot); }
static void save_active_slot(uint8_t slot) { ARG_UNUSED(slot); }
#endif

static int validate_slot_at(uintptr_t base, const struct resource_partition_header **out_hdr,
			    bool *out_crc_ok)
{
	const struct resource_partition_header *hdr = (const struct resource_partition_header *)base;
	bool crc_ok = false;

	if (hdr->magic != RESOURCE_PARTITION_MAGIC) {
		return -ENOENT;
	}
	if (hdr->version != RESOURCE_PARTITION_VERSION) {
		return -ENOTSUP;
	}
	if (hdr->asset_count == 0U || hdr->asset_count > RESOURCE_MAX_ENTRIES) {
		return -EINVAL;
	}
	if (hdr->total_size < sizeof(*hdr) ||
	    hdr->total_size > RESOURCE_PARTITION_SIZE) {
		return -EINVAL;
	}

	const void *payload = (const uint8_t *)hdr + sizeof(*hdr);
	size_t payload_len = hdr->total_size - sizeof(*hdr);

	crc_ok = crc32_matches_retry(payload, payload_len, hdr->crc32);
	if (!crc_ok) {
		return -EIO;
	}

	if (out_hdr != NULL) {
		*out_hdr = hdr;
	}
	if (out_crc_ok != NULL) {
		*out_crc_ok = crc_ok;
	}
	return 0;
}

static void bind_active(uint8_t slot, const struct resource_partition_header *hdr, bool crc_ok)
{
	active_slot = slot;
	partition_header = hdr;
	asset_table = (const struct resource_entry *)((const uint8_t *)hdr +
						       sizeof(struct resource_partition_header));
	xip_crc_ok = crc_ok;
	xip_valid = true;
	xip_initialized = true;
}

/* =============================================================================
 * Public API
 * ============================================================================= */

int resource_validate_slot(uint8_t slot)
{
	if (slot != RESOURCE_SLOT_PRIMARY && slot != RESOURCE_SLOT_SECONDARY) {
		return -EINVAL;
	}
	return validate_slot_at(resource_slot_base(slot), NULL, NULL);
}

int resource_set_active_slot(uint8_t slot)
{
	const struct resource_partition_header *hdr = NULL;
	bool crc_ok = false;
	int ret;

	if (slot != RESOURCE_SLOT_PRIMARY && slot != RESOURCE_SLOT_SECONDARY) {
		return -EINVAL;
	}
	ret = validate_slot_at(resource_slot_base(slot), &hdr, &crc_ok);
	if (ret != 0) {
		return ret;
	}
	bind_active(slot, hdr, crc_ok);
	save_active_slot(slot);
	LOG_INF("Active Resources slot set to %u (assets=%u)", slot, hdr->asset_count);
	return 0;
}

uint8_t resource_get_active_slot(void)
{
	return active_slot;
}

uint8_t resource_inactive_slot(void)
{
	return (active_slot == RESOURCE_SLOT_PRIMARY)
		       ? RESOURCE_SLOT_SECONDARY
		       : RESOURCE_SLOT_PRIMARY;
}

int resource_init(void)
{
	uint8_t pref = RESOURCE_SLOT_PRIMARY;
	const struct resource_partition_header *hdr = NULL;
	bool crc_ok = false;
	int ret;

	load_active_slot(&pref);
	LOG_INF("Initializing Resources (preferred slot %u @ 0x%08lx)",
		pref, (unsigned long)resource_slot_base(pref));

	xip_initialized = true;
	ret = validate_slot_at(resource_slot_base(pref), &hdr, &crc_ok);
	if (ret == 0) {
		bind_active(pref, hdr, crc_ok);
	} else {
		uint8_t other = (pref == RESOURCE_SLOT_PRIMARY)
					? RESOURCE_SLOT_SECONDARY
					: RESOURCE_SLOT_PRIMARY;

		ret = validate_slot_at(resource_slot_base(other), &hdr, &crc_ok);
		if (ret == 0) {
			LOG_WRN("preferred XIP slot %u invalid; using slot %u", pref, other);
			bind_active(other, hdr, crc_ok);
		} else {
			LOG_WRN("XIP partition magic/CRC invalid on both slots");
			xip_valid = false;
			partition_header = (const struct resource_partition_header *)
				resource_slot_base(pref);
			return -ENOENT;
		}
	}

	LOG_INF("Resources initialized: slot %u, %u assets, %u bytes total, crc %s",
		active_slot, partition_header->asset_count, partition_header->total_size,
		xip_crc_ok ? "OK" : "FAIL");

	if (partition_header->build_id[0] != '\0') {
		LOG_INF("XIP build ID: %.32s", partition_header->build_id);
	}

	return 0;
}

int resource_validate_all(void)
{
    int failures = 0;

    if (!resource_is_valid()) {
        return -ENOENT;
    }

    for (uint32_t i = 0; i < partition_header->asset_count && i < RESOURCE_MAX_ENTRIES; i++) {
        const struct resource_entry *e = &asset_table[i];

        if (e->magic != RESOURCE_ASSET_MAGIC) {
            continue;
        }
        if (!crc32_matches_retry(RESOURCE_MMAP_ADDR(e->offset), e->size, e->crc32)) {
            LOG_ERR("Asset 0x%04x CRC bad (size=%u @0x%08x)",
                    e->asset_id, e->size, e->offset);
            failures++;
        }
    }

    if (failures == 0) {
        LOG_INF("All %u Resources passed CRC", partition_header->asset_count);
    } else {
        LOG_WRN("%d Resource(s) failed CRC", failures);
    }
    return failures;
}

const void *resource_get_verified(uint16_t asset_id, size_t *size)
{
    const struct resource_entry *entry = find_asset_by_id(asset_id);

    if (!entry) {
        return NULL;
    }
    if (!crc32_matches_retry(RESOURCE_MMAP_ADDR(entry->offset), entry->size, entry->crc32)) {
        LOG_WRN("Asset 0x%04x failed verified read (CRC)", asset_id);
        return NULL;
    }
    if (size) {
        *size = entry->size;
    }
    return RESOURCE_MMAP_ADDR(entry->offset);
}

void resource_wake(void)
{
	/* Touch mapped flash so nordic qspi-nor exits DPD before CRC/UI reads. */
	if (partition_header != NULL) {
		volatile uint32_t magic = partition_header->magic;

		(void)magic;
		return;
	}
	/* Early boot / pre-init: poke primary map base. */
	volatile uint32_t word = *(volatile uint32_t *)RESOURCE_PRIMARY_BASE;

	(void)word;
}

bool resource_is_valid(void)
{
    return xip_initialized && xip_valid;
}

const void *resource_get(uint16_t asset_id, size_t *size)
{
    const struct resource_entry *entry = find_asset_by_id(asset_id);
    
    if (!entry) {
        LOG_DBG("Asset 0x%04x not found", asset_id);
        return NULL;
    }

    if (size) {
        *size = entry->size;
    }

    /* Return XIP pointer to asset data */
    return RESOURCE_MMAP_ADDR(entry->offset);
}

const void *resource_get_image(uint16_t asset_id, uint16_t *width, uint16_t *height)
{
    const struct resource_entry *entry = find_asset_by_id(asset_id);
    
    if (!entry) {
        return NULL;
    }

    /* Verify this is an image type */
    if (entry->type != RESOURCE_TYPE_LOGO && 
        entry->type != RESOURCE_TYPE_ICON) {
        LOG_WRN("Asset 0x%04x is not an image type (%u)", asset_id, entry->type);
        return NULL;
    }

    /* Snapshot table fields first — entry lives in XIP-mapped flash. */
    const uint16_t table_w = entry->width;
    const uint16_t table_h = entry->height;
    const uint32_t table_size = entry->size;
    const uint8_t table_fmt = entry->format;
    const uint32_t data_off = entry->offset;

    if (width) {
        *width = table_w;
    }
    if (height) {
        *height = table_h;
    }

    /*
     * Copy the embedded header into RAM before comparing. Direct halfword
     * loads through a packed XIP pointer can disagree with the table even
     * when QSPI register reads of the same bytes match (seen on-device for
     * ARGB icons while logo RGB565 path via get_verified was fine).
     */
    const uint8_t *xip_raw = RESOURCE_MMAP_ADDR(data_off);
    struct resource_image_header img_hdr;
    memcpy(&img_hdr, xip_raw, sizeof(img_hdr));

    if (img_hdr.width != table_w || img_hdr.height != table_h) {
        resource_wake();
        memcpy(&img_hdr, xip_raw, sizeof(img_hdr));
    }
    if (img_hdr.width != table_w || img_hdr.height != table_h) {
        LOG_WRN("Image header mismatch for asset 0x%04x "
                "table=%ux%u fmt=%u size=%u | hdr=%ux%u fmt=%u bpp=%u",
                asset_id, table_w, table_h, table_fmt, table_size,
                img_hdr.width, img_hdr.height, img_hdr.format,
                img_hdr.bits_per_pixel);
    }

    /* Skip past image header; clamp absurd palette sizes from a bad XIP read. */
    uint32_t pal_entries = img_hdr.palette_entries;
    if (pal_entries > 256U) {
        pal_entries = 0U;
    }
    size_t palette_size = (size_t)pal_entries * sizeof(uint32_t);
    const uint8_t *pixel_data = xip_raw + sizeof(struct resource_image_header) + palette_size;

    return pixel_data;
}

int resource_get_lvgl_image(uint16_t asset_id, void *dsc)
{
    if (!dsc) {
        return -EINVAL;
    }

    const struct resource_entry *entry = find_asset_by_id(asset_id);
    
    if (!entry) {
        return -ENOENT;
    }

    if (entry->type != RESOURCE_TYPE_LOGO && 
        entry->type != RESOURCE_TYPE_ICON) {
        return -EINVAL;
    }

    const struct resource_image_header *img_header = RESOURCE_MMAP_ADDR(entry->offset);
    size_t palette_size = img_header->palette_entries * sizeof(uint32_t);
    const uint8_t *pixel_data = (const uint8_t *)img_header + 
                                 sizeof(struct resource_image_header) + 
                                 palette_size;

#if defined(CONFIG_LV_USE_IMG) || defined(CONFIG_LVGL)
    /* Fill LVGL image descriptor */
    /* Note: This is platform-specific - adjust for LVGL version */
    /* 
     * lv_img_dsc_t structure:
     *   .header.cf = color format
     *   .header.w  = width
     *   .header.h  = height
     *   .data_size = size
     *   .data      = pointer to pixels
     */
    
    /* LVGL 8.x structure - adjust if using different version */
    typedef struct {
        struct {
            uint32_t cf : 5;
            uint32_t always_zero : 3;
            uint32_t reserved : 2;
            uint32_t w : 11;
            uint32_t h : 11;
        } header;
        uint32_t data_size;
        const uint8_t *data;
    } lv_img_dsc_t_compat;

    lv_img_dsc_t_compat *lvgl_dsc = (lv_img_dsc_t_compat *)dsc;

    /* Map our format to LVGL color format */
    uint32_t lv_cf;
    switch (entry->format) {
        case RESOURCE_IMAGE_FORMAT_RGB565:
            lv_cf = 4;  /* LV_IMG_CF_TRUE_COLOR (assume RGB565) */
            break;
        case RESOURCE_IMAGE_FORMAT_INDEXED8:
            lv_cf = 7;  /* LV_IMG_CF_INDEXED_8BIT */
            break;
        case RESOURCE_IMAGE_FORMAT_ARGB8888:
            lv_cf = 5;  /* LV_IMG_CF_TRUE_COLOR_ALPHA */
            break;
        default:
            lv_cf = 4;  /* Default to true color */
            break;
    }

    lvgl_dsc->header.cf = lv_cf;
    lvgl_dsc->header.always_zero = 0;
    lvgl_dsc->header.reserved = 0;
    lvgl_dsc->header.w = entry->width;
    lvgl_dsc->header.h = entry->height;
    lvgl_dsc->data_size = img_header->data_size;
    lvgl_dsc->data = pixel_data;

    LOG_DBG("LVGL descriptor for asset 0x%04x: %ux%u, cf=%u, data=0x%p",
            asset_id, entry->width, entry->height, lv_cf, pixel_data);

    return 0;
#else
    LOG_WRN("LVGL not enabled");
    return -ENOTSUP;
#endif
}

const void *resource_get_font(uint16_t asset_id)
{
    const struct resource_entry *entry = find_asset_by_id(asset_id);
    
    if (!entry) {
        return NULL;
    }

    if (entry->type != RESOURCE_TYPE_FONT) {
        LOG_WRN("Asset 0x%04x is not a font type (%u)", asset_id, entry->type);
        return NULL;
    }

    return RESOURCE_MMAP_ADDR(entry->offset);
}

uint32_t resource_get_count(void)
{
    if (!xip_valid || !partition_header) {
        return 0;
    }
    return partition_header->asset_count;
}

const struct resource_entry *resource_get_entry(uint32_t index)
{
    if (!xip_valid || !asset_table) {
        return NULL;
    }

    if (index >= partition_header->asset_count || index >= RESOURCE_MAX_ENTRIES) {
        return NULL;
    }

    return &asset_table[index];
}

void resource_print_info(void)
{
    LOG_INF("========== Resources Info ==========");
    LOG_INF("XIP Active:  slot %u @ 0x%08lx", active_slot,
	    (unsigned long)resource_active_base());
    LOG_INF("Initialized: %s", xip_initialized ? "yes" : "no");
    LOG_INF("Valid:       %s", xip_valid ? "yes" : "no");

    if (!xip_valid || !partition_header) {
        LOG_INF("No valid XIP partition");
        return;
    }

    LOG_INF("--- Partition Header ---");
    LOG_INF("Magic:       0x%08x", partition_header->magic);
    LOG_INF("Version:     %u", partition_header->version);
    LOG_INF("Total Size:  %u bytes", partition_header->total_size);
    LOG_INF("Asset Count: %u", partition_header->asset_count);
    LOG_INF("CRC32:       0x%08x", partition_header->crc32);
    if (partition_header->build_id[0] != '\0') {
        LOG_INF("Build ID:    %.32s", partition_header->build_id);
    }

    LOG_INF("--- Asset Table ---");
    for (uint32_t i = 0; i < partition_header->asset_count && i < RESOURCE_MAX_ENTRIES; i++) {
        const struct resource_entry *entry = &asset_table[i];
        
        const char *type_str;
        switch (entry->type) {
            case RESOURCE_TYPE_LOGO:     type_str = "LOGO"; break;
            case RESOURCE_TYPE_ICON:     type_str = "ICON"; break;
            case RESOURCE_TYPE_FONT:     type_str = "FONT"; break;
            case RESOURCE_TYPE_ANIMATION: type_str = "ANIM"; break;
            case RESOURCE_TYPE_SOUND:    type_str = "SND "; break;
            case RESOURCE_TYPE_DATA:     type_str = "DATA"; break;
            default:                      type_str = "????"; break;
        }

        LOG_INF("[%2u] ID=0x%04x %s %ux%u %u bytes @ 0x%08x",
                i, entry->asset_id, type_str,
                entry->width, entry->height,
                entry->size, entry->offset);
    }
    LOG_INF("=====================================");
}

bool resource_validate_crc(uint16_t asset_id)
{
    const struct resource_entry *entry = find_asset_by_id(asset_id);
    
    if (!entry) {
        return false;
    }

    if (!crc32_matches_retry(RESOURCE_MMAP_ADDR(entry->offset), entry->size, entry->crc32)) {
        LOG_WRN("CRC mismatch for asset 0x%04x (expected 0x%08x)",
                asset_id, entry->crc32);
        return false;
    }

    LOG_DBG("CRC valid for asset 0x%04x", asset_id);
    return true;
}

const struct resource_partition_header *resource_get_header(void)
{
    if (!xip_valid) {
        return NULL;
    }
    return partition_header;
}
