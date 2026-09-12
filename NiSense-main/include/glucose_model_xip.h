/* =============================================================================
 * Clinical Model Store (XIP-mapped A/B on external QSPI)
 * =============================================================================
 * Each A/B slot is a *model pack* containing typed entries. Today: glucose
 * wearable + pulse (GMDL blobs). Future: PPG/HR/SpO2 model entries.
 *
 * Partitions (see 51_qspi.overlayinc):
 *   model-primary   @ 0x1F6000 (512 KB)
 *   model-secondary @ 0x276000 (512 KB)
 *
 * NVS active slot: "model/slot" (legacy "glucose/slot" migrated on read).
 * BLE …def3 writes the inactive slot, CRC-validates, then flips NVS.
 * ============================================================================= */

#ifndef GLUCOSE_MODEL_XIP_H
#define GLUCOSE_MODEL_XIP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GLUCOSE_MODEL_XIP_BASE            0x12000000U

/** Model A/B slot offsets (must match 51_qspi.overlayinc) */
#define GLUCOSE_MODEL_PRIMARY_OFFSET      0x1F6000U
#define GLUCOSE_MODEL_SECONDARY_OFFSET    0x276000U
#define GLUCOSE_MODEL_PARTITION_SIZE      0x80000U  /* 512 KB */

#define GLUCOSE_MODEL_PRIMARY_XIP_ADDR \
	(GLUCOSE_MODEL_XIP_BASE + GLUCOSE_MODEL_PRIMARY_OFFSET)
#define GLUCOSE_MODEL_SECONDARY_XIP_ADDR \
	(GLUCOSE_MODEL_XIP_BASE + GLUCOSE_MODEL_SECONDARY_OFFSET)

/* ---- Outer model pack (catalog of GMDL / future entries) ---------------- */

/** Pack magic "MDLP" little-endian */
#define MODEL_PACK_MAGIC                  0x504C444DU
#define MODEL_PACK_FORMAT_VERSION         1U
#define MODEL_PACK_MAX_ENTRIES            8U

enum model_pack_entry_type {
	MODEL_PACK_ENTRY_GLUCOSE_WEARABLE = 0,
	MODEL_PACK_ENTRY_GLUCOSE_PULSE    = 1,
	/* Future: MODEL_PACK_ENTRY_PPG_*, HR, SpO2, … */
};

struct model_pack_header {
	uint32_t magic;            /**< MODEL_PACK_MAGIC */
	uint16_t format_version;   /**< MODEL_PACK_FORMAT_VERSION */
	uint16_t num_entries;
	uint32_t total_size;       /**< bytes used in slot including this header */
	uint32_t payload_crc32;    /**< CRC32 IEEE over bytes after this header */
	uint32_t timestamp;
	uint32_t pack_version;
	char     build_id[32];
	uint8_t  reserved[72];
} __packed;

_Static_assert(sizeof(struct model_pack_header) == 128,
	       "model_pack_header must be 128 bytes");

struct model_pack_entry {
	uint16_t type;             /**< enum model_pack_entry_type */
	uint16_t reserved;
	uint32_t offset;           /**< from slot/pack base */
	uint32_t size;             /**< entry blob size */
	uint32_t entry_crc32;      /**< CRC32 of entry blob */
} __packed;

_Static_assert(sizeof(struct model_pack_entry) == 16,
	       "model_pack_entry must be 16 bytes");

/* ---- Inner glucose GMDL blob (unchanged clinical format) ---------------- */

#define GLUCOSE_MODEL_MAX_GROUPS          16U
#define GLUCOSE_MODEL_GROUP_COL           3U
#define GLUCOSE_MODEL_MAGIC               0x4C444D47U /* "GMDL" */
#define GLUCOSE_MODEL_FORMAT_VERSION      1U

enum glucose_model_variant {
	GLUCOSE_MODEL_VARIANT_WEARABLE = 0,
	GLUCOSE_MODEL_VARIANT_PULSE    = 1,
};

struct glucose_model_header {
	uint32_t magic;
	uint16_t format_version;
	uint16_t variant;
	uint16_t num_groups;
	uint16_t group_col;
	uint32_t total_size;
	uint32_t payload_crc32;
	uint32_t timestamp;
	uint32_t model_version;
	char     build_id[32];
	uint8_t  reserved[68];
} __packed;

struct glucose_model_group_entry {
	uint16_t group_num;
	uint16_t row_count;
	uint32_t data_offset;
	uint32_t data_crc32;
	uint32_t reserved;
} __packed;

#define GLUCOSE_MODEL_SLOT_PRIMARY    0U
#define GLUCOSE_MODEL_SLOT_SECONDARY  1U

/**
 * @brief Resolve active model pack, CRC-validate, bind glucose entry for SKU.
 *
 * Call early at boot after product_hw_detect(). Safe to call multiple times.
 */
int glucose_model_xip_init(void);

/** Early boot alias — same as init (pack CRC + SKU entry). */
static inline int model_store_boot_verify(void)
{
	return glucose_model_xip_init();
}

int glucose_model_xip_validate_slot(uint8_t slot);
int glucose_model_xip_set_active_slot(uint8_t slot);
uint8_t glucose_model_xip_get_active_slot(void);
uint8_t glucose_model_xip_inactive_slot(void);
uintptr_t glucose_model_xip_slot_base(uint8_t slot);
const struct glucose_model_header *glucose_model_xip_peek_header(uint8_t slot);
uint32_t glucose_model_xip_active_version(void);

#ifdef __cplusplus
}
#endif

#endif /* GLUCOSE_MODEL_XIP_H */
