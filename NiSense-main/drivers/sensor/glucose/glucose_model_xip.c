/**
 * @file glucose_model_xip.c
 * @brief Clinical model pack (A/B) with glucose GMDL entries selected by SKU.
 */

#include "glucose_model_xip.h"
#include "glucose_algorithm_matrices.h"
#include "glucose_algorithm_profiles.h"
#include "glucose_hw_variant.h"
#include "product_hw_variant.h"

#ifdef CONFIG_GLUCOSE_MATRICES_FROM_XIP

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/crc.h>
#include <errno.h>
#include <string.h>

#ifdef CONFIG_SETTINGS
#include <zephyr/settings/settings.h>
#define SLOT_SETTINGS_KEY_NEW "model/slot"
#define SLOT_SETTINGS_KEY_OLD "glucose/slot"
#endif

LOG_MODULE_REGISTER(glucose_model_xip, CONFIG_GLUCOSE_ALGORITHM_LOG_LEVEL);

static uint8_t active_slot = GLUCOSE_MODEL_SLOT_PRIMARY;
static const struct glucose_model_header *active_hdr;
static const struct glucose_model_group_entry *active_entries;
static uintptr_t active_gmdl_base;
static bool model_ready;

static void model_mmap_wake(uintptr_t base)
{
	volatile uint32_t word = *(volatile uint32_t *)base;

	(void)word;
}

uintptr_t glucose_model_xip_slot_base(uint8_t slot)
{
	return (slot == GLUCOSE_MODEL_SLOT_SECONDARY)
		       ? (uintptr_t)GLUCOSE_MODEL_SECONDARY_XIP_ADDR
		       : (uintptr_t)GLUCOSE_MODEL_PRIMARY_XIP_ADDR;
}

static uintptr_t slot_base(uint8_t slot)
{
	return glucose_model_xip_slot_base(slot);
}

static int validate_gmdl(uintptr_t base, const struct glucose_model_header **out_hdr)
{
	const struct glucose_model_header *hdr = (const struct glucose_model_header *)base;

	if (hdr->magic != GLUCOSE_MODEL_MAGIC) {
		return -ENODATA;
	}
	if (hdr->format_version != GLUCOSE_MODEL_FORMAT_VERSION) {
		return -ENOTSUP;
	}
	if (hdr->group_col != GLUCOSE_MODEL_GROUP_COL) {
		return -EINVAL;
	}
	if (hdr->num_groups == 0U || hdr->num_groups > GLUCOSE_MODEL_MAX_GROUPS) {
		return -EINVAL;
	}
	if (hdr->total_size < sizeof(*hdr) ||
	    hdr->total_size > GLUCOSE_MODEL_PARTITION_SIZE) {
		return -EINVAL;
	}

	const uint8_t *payload = (const uint8_t *)(base + sizeof(*hdr));
	uint32_t payload_len = hdr->total_size - (uint32_t)sizeof(*hdr);
	uint32_t crc = crc32_ieee(payload, payload_len);

	if (crc != hdr->payload_crc32) {
		LOG_ERR("GMDL @0x%08lx CRC mismatch: calc=0x%08x hdr=0x%08x",
			(unsigned long)base, crc, hdr->payload_crc32);
		return -EIO;
	}

	if (out_hdr != NULL) {
		*out_hdr = hdr;
	}
	return 0;
}

static int validate_pack(uintptr_t base, const struct model_pack_header **out_pack)
{
	model_mmap_wake(base);

	const struct model_pack_header *pack = (const struct model_pack_header *)base;

	/* Legacy: bare GMDL at slot start (pre-pack manufacturing images) */
	if (pack->magic == GLUCOSE_MODEL_MAGIC) {
		if (out_pack != NULL) {
			*out_pack = NULL;
		}
		return 0; /* caller treats as single-GMDL slot */
	}

	if (pack->magic != MODEL_PACK_MAGIC) {
		return -ENODATA;
	}
	if (pack->format_version != MODEL_PACK_FORMAT_VERSION) {
		return -ENOTSUP;
	}
	if (pack->num_entries == 0U || pack->num_entries > MODEL_PACK_MAX_ENTRIES) {
		return -EINVAL;
	}
	if (pack->total_size < sizeof(*pack) ||
	    pack->total_size > GLUCOSE_MODEL_PARTITION_SIZE) {
		return -EINVAL;
	}

	const uint8_t *payload = (const uint8_t *)(base + sizeof(*pack));
	uint32_t payload_len = pack->total_size - (uint32_t)sizeof(*pack);

	model_mmap_wake(base);
	uint32_t crc = crc32_ieee(payload, payload_len);

	if (crc != pack->payload_crc32) {
		/* One retry after DPD / transient mmap glitch */
		model_mmap_wake(base);
		crc = crc32_ieee(payload, payload_len);
	}
	if (crc != pack->payload_crc32) {
		LOG_ERR("model pack @0x%08lx CRC mismatch: calc=0x%08x hdr=0x%08x",
			(unsigned long)base, crc, pack->payload_crc32);
		return -EIO;
	}

	if (out_pack != NULL) {
		*out_pack = pack;
	}
	return 0;
}

static uint16_t sku_entry_type(void)
{
	return (product_hw_detect() == PRODUCT_HW_WATCH)
		       ? (uint16_t)MODEL_PACK_ENTRY_GLUCOSE_WEARABLE
		       : (uint16_t)MODEL_PACK_ENTRY_GLUCOSE_PULSE;
}

static int find_gmdl_in_pack(uintptr_t base, const struct model_pack_header *pack,
			     uintptr_t *out_gmdl)
{
	const struct model_pack_entry *entries =
		(const struct model_pack_entry *)(base + sizeof(*pack));
	uint16_t want = sku_entry_type();

	for (uint16_t i = 0; i < pack->num_entries; i++) {
		if (entries[i].type != want) {
			continue;
		}
		if (entries[i].offset + entries[i].size > GLUCOSE_MODEL_PARTITION_SIZE) {
			return -EINVAL;
		}
		uintptr_t gmdl = base + entries[i].offset;
		uint32_t crc = crc32_ieee((const uint8_t *)gmdl, entries[i].size);

		if (crc != entries[i].entry_crc32) {
			LOG_ERR("pack entry %u CRC fail", i);
			return -EIO;
		}
		*out_gmdl = gmdl;
		return 0;
	}
	LOG_ERR("model pack missing entry type %u for SKU %s", want,
		product_hw_variant_name(product_hw_detect()));
	return -ENOENT;
}

static int resolve_gmdl(uintptr_t base, uintptr_t *out_gmdl,
			const struct glucose_model_header **out_hdr)
{
	const struct model_pack_header *pack = NULL;
	int ret = validate_pack(base, &pack);

	if (ret != 0) {
		return ret;
	}

	if (pack == NULL) {
		/* Legacy bare GMDL */
		ret = validate_gmdl(base, out_hdr);
		if (ret == 0) {
			*out_gmdl = base;
		}
		return ret;
	}

	ret = find_gmdl_in_pack(base, pack, out_gmdl);
	if (ret != 0) {
		return ret;
	}
	return validate_gmdl(*out_gmdl, out_hdr);
}

static void set_active(uint8_t slot, uintptr_t gmdl_base,
		       const struct glucose_model_header *hdr)
{
	active_slot = slot;
	active_gmdl_base = gmdl_base;
	active_hdr = hdr;
	active_entries = (const struct glucose_model_group_entry *)
		(gmdl_base + sizeof(*hdr));
	model_ready = true;
}

static const struct glucose_model_group_entry *find_group(uint8_t group_num)
{
	if (!model_ready) {
		return NULL;
	}
	for (uint16_t i = 0; i < active_hdr->num_groups; i++) {
		if (active_entries[i].group_num == group_num) {
			return &active_entries[i];
		}
	}
	return NULL;
}

#ifdef CONFIG_SETTINGS
static int slot_direct_load(const char *key, size_t len, settings_read_cb read_cb,
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
	uint8_t v = GLUCOSE_MODEL_SLOT_PRIMARY;

	(void)settings_load_subtree_direct(SLOT_SETTINGS_KEY_NEW, slot_direct_load, &v);
	if (v != GLUCOSE_MODEL_SLOT_PRIMARY && v != GLUCOSE_MODEL_SLOT_SECONDARY) {
		v = GLUCOSE_MODEL_SLOT_PRIMARY;
		(void)settings_load_subtree_direct(SLOT_SETTINGS_KEY_OLD, slot_direct_load, &v);
	}
	if (v == GLUCOSE_MODEL_SLOT_PRIMARY || v == GLUCOSE_MODEL_SLOT_SECONDARY) {
		*slot = v;
	}
}

static void save_active_slot(uint8_t slot)
{
	(void)settings_save_one(SLOT_SETTINGS_KEY_NEW, &slot, sizeof(slot));
}
#else
static void load_active_slot(uint8_t *slot) { ARG_UNUSED(slot); }
static void save_active_slot(uint8_t slot) { ARG_UNUSED(slot); }
#endif

int glucose_model_xip_init(void)
{
	uint8_t pref = GLUCOSE_MODEL_SLOT_PRIMARY;
	uintptr_t gmdl = 0;
	const struct glucose_model_header *hdr = NULL;

	load_active_slot(&pref);

	if (resolve_gmdl(slot_base(pref), &gmdl, &hdr) == 0) {
		set_active(pref, gmdl, hdr);
	} else {
		uint8_t other = (pref == GLUCOSE_MODEL_SLOT_PRIMARY)
					? GLUCOSE_MODEL_SLOT_SECONDARY
					: GLUCOSE_MODEL_SLOT_PRIMARY;

		if (resolve_gmdl(slot_base(other), &gmdl, &hdr) == 0) {
			LOG_WRN("preferred model slot %u invalid; using slot %u", pref, other);
			set_active(other, gmdl, hdr);
		} else {
			LOG_ERR("no valid model pack/slot found (primary or secondary)");
			model_ready = false;
			return -ENODATA;
		}
	}

	LOG_INF("Model store slot %u: glucose variant=%u groups=%u ver=%u SKU=%s",
		active_slot, active_hdr->variant, active_hdr->num_groups,
		active_hdr->model_version, product_hw_variant_name(product_hw_detect()));
	return 0;
}

int glucose_model_xip_validate_slot(uint8_t slot)
{
	uintptr_t gmdl = 0;
	const struct glucose_model_header *hdr = NULL;

	return resolve_gmdl(slot_base(slot), &gmdl, &hdr);
}

int glucose_model_xip_set_active_slot(uint8_t slot)
{
	uintptr_t gmdl = 0;
	const struct glucose_model_header *hdr = NULL;
	int ret = resolve_gmdl(slot_base(slot), &gmdl, &hdr);

	if (ret != 0) {
		return ret;
	}
	set_active(slot, gmdl, hdr);
	save_active_slot(slot);
	LOG_INF("Active model slot set to %u (ver=%u)", slot, hdr->model_version);
	return 0;
}

uint8_t glucose_model_xip_get_active_slot(void)
{
	return active_slot;
}

uint8_t glucose_model_xip_inactive_slot(void)
{
	return (active_slot == GLUCOSE_MODEL_SLOT_PRIMARY)
		       ? GLUCOSE_MODEL_SLOT_SECONDARY
		       : GLUCOSE_MODEL_SLOT_PRIMARY;
}

const struct glucose_model_header *glucose_model_xip_peek_header(uint8_t slot)
{
	uintptr_t gmdl = 0;
	const struct glucose_model_header *hdr = NULL;

	if (slot != GLUCOSE_MODEL_SLOT_PRIMARY &&
	    slot != GLUCOSE_MODEL_SLOT_SECONDARY) {
		return NULL;
	}
	if (resolve_gmdl(slot_base(slot), &gmdl, &hdr) != 0) {
		return NULL;
	}
	return hdr;
}

uint32_t glucose_model_xip_active_version(void)
{
	return model_ready ? active_hdr->model_version : 0U;
}

int glucose_matrix_files_validate(void)
{
	const struct glucose_algorithm_profile *profile = glucose_algorithm_get_active_profile();

	if (!model_ready) {
		int ret = glucose_model_xip_init();

		if (ret != 0) {
			return ret;
		}
	}

	if ((enum glucose_hw_variant)active_hdr->variant != profile->variant) {
		LOG_ERR("model variant %u != active profile variant %u",
			active_hdr->variant, profile->variant);
		return -ENOTSUP;
	}
	if (active_hdr->num_groups < profile->num_groups) {
		LOG_ERR("model has %u groups but profile needs %u",
			active_hdr->num_groups, profile->num_groups);
		return -EINVAL;
	}
	for (uint8_t g = 1; g <= profile->num_groups; g++) {
		if (find_group(g) == NULL) {
			LOG_ERR("model missing group %u", g);
			return -ENOENT;
		}
	}

	LOG_INF("Validated %s glucose model (slot %u): %u groups",
		profile->name, active_slot, profile->num_groups);
	return 0;
}

int glucose_matrix_lookup_correction(uint8_t group_num, float glucose_estimate,
				     float *correction_percent, uint16_t *matched_row)
{
	if (correction_percent == NULL) {
		return -EINVAL;
	}
	if (!model_ready) {
		return -ENODATA;
	}

	const struct glucose_model_group_entry *e = find_group(group_num);

	if (e == NULL) {
		return -EINVAL;
	}

	/* data_offset is relative to the GMDL blob base (not the pack slot). */
	const float *rows = (const float *)(active_gmdl_base + e->data_offset);

	for (uint16_t r = 0; r < e->row_count; r++) {
		float lower = rows[(size_t)r * GLUCOSE_MODEL_GROUP_COL + 0];
		float upper = rows[(size_t)r * GLUCOSE_MODEL_GROUP_COL + 1];
		float corr = rows[(size_t)r * GLUCOSE_MODEL_GROUP_COL + 2];

		if (glucose_estimate > lower && glucose_estimate < upper) {
			*correction_percent = corr;
			if (matched_row != NULL) {
				*matched_row = r;
			}
			return 0;
		}
	}

	LOG_WRN("Model group %u: Y3=%.4f not found in any row range", group_num,
		(double)glucose_estimate);
	return -ERANGE;
}

#endif /* CONFIG_GLUCOSE_MATRICES_FROM_XIP */
