/* =============================================================================
 * Resource Management
 * =============================================================================
 * Static assets (logos, fonts, icons) stored in external QSPI flash
 * with Execute-In-Place (XIP) memory-mapped access.
 *
 * Dual A/B partitions in external QSPI (see 51_qspi.overlayinc):
 *   resource-primary   @ 0x0F6000 (512 KB)
 *   resource-secondary @ 0x176000 (512 KB)
 *
 * Active slot is selected via NVS ("resource/slot"). BLE OTA always writes the
 * inactive slot, validates (magic + CRC + ota_compat), then flips NVS.
 *
 * XIP Base: 0x12000000 (nRF52840 QSPI memory-mapped region)
 *
 * Target: nRF52840 (Raytac MDBT50Q-DB-40)
 * ============================================================================= */

#ifndef RESOURCE_H
#define RESOURCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * XIP Memory Map Constants
 * ============================================================================= */

/** QSPI XIP base address on nRF52840 */
#define RESOURCE_MMAP_BASE            0x12000000

/** Primary partition offset (manufacturing / flash scripts) */
#define RESOURCE_PARTITION_OFFSET 0x0F6000
#define RESOURCE_PRIMARY_OFFSET   0x0F6000
#define RESOURCE_SECONDARY_OFFSET 0x176000

/** Resources partition size (each A/B slot) */
#define RESOURCE_PARTITION_SIZE   0x80000     /* 512 KB */

/** Primary XIP map address (factory default / manufacturing) */
#define RESOURCE_PRIMARY_BASE \
	(RESOURCE_MMAP_BASE + RESOURCE_PRIMARY_OFFSET)
#define RESOURCE_SECONDARY_BASE \
	(RESOURCE_MMAP_BASE + RESOURCE_SECONDARY_OFFSET)

/** Factory/primary mmap base; prefer resource_active_base() at runtime. */
#define RESOURCE_BASE             RESOURCE_PRIMARY_BASE

#define RESOURCE_SLOT_PRIMARY     0U
#define RESOURCE_SLOT_SECONDARY   1U

/** Maximum number of assets in the table */
#define RESOURCE_MAX_ENTRIES              32

/* =============================================================================
 * Asset Types
 * ============================================================================= */

enum resource_type {
    RESOURCE_TYPE_INVALID      = 0x00,
    RESOURCE_TYPE_LOGO         = 0x01,     /* Boot/splash logo (RGB565) */
    RESOURCE_TYPE_ICON         = 0x02,     /* UI icon (RGB565 or indexed) */
    RESOURCE_TYPE_FONT         = 0x03,     /* LVGL font binary */
    RESOURCE_TYPE_ANIMATION    = 0x04,     /* Animation frames */
    RESOURCE_TYPE_SOUND        = 0x05,     /* Audio sample */
    RESOURCE_TYPE_DATA         = 0x06,     /* Generic read-only data */
    RESOURCE_TYPE_MAX
};

/** Image format for logo/icon assets */
enum resource_image_format {
    RESOURCE_IMAGE_FORMAT_RGB565     = 0x00,     /* 16-bit RGB565 (2 bytes/pixel) */
    RESOURCE_IMAGE_FORMAT_RGB888     = 0x01,     /* 24-bit RGB888 (3 bytes/pixel) */
    RESOURCE_IMAGE_FORMAT_ARGB8888   = 0x02,     /* 32-bit ARGB (4 bytes/pixel) */
    RESOURCE_IMAGE_FORMAT_INDEXED8   = 0x03,     /* 8-bit indexed with palette */
    RESOURCE_IMAGE_FORMAT_INDEXED4   = 0x04,     /* 4-bit indexed with palette */
    RESOURCE_IMAGE_FORMAT_INDEXED1   = 0x05,     /* 1-bit monochrome */
    RESOURCE_IMAGE_FORMAT_RLE        = 0x06,     /* RLE compressed */
};

/* =============================================================================
 * Asset Header Structures
 * ============================================================================= */

/** Magic number for Resource partition header: "XIPA" (on-disk id) */
#define RESOURCE_PARTITION_MAGIC         0x41504958

/** Magic number for individual asset: "XAST" */
#define RESOURCE_ASSET_MAGIC             0x54534158

/** Current partition format version */
#define RESOURCE_PARTITION_VERSION       1

/** Current asset format version */
#define RESOURCE_ASSET_VERSION           1

/**
 * @brief XIP Partition Header (256 bytes)
 * Located at the start of resource_primary partition
 */
struct resource_partition_header {
    uint32_t magic;                 /**< Must be RESOURCE_PARTITION_MAGIC */
    uint32_t version;               /**< Partition format version */
    uint32_t total_size;            /**< Total used size in partition */
    uint32_t crc32;                 /**< CRC32 of all data after header */
    uint32_t asset_count;           /**< Number of assets in partition */
    uint32_t timestamp;             /**< Unix timestamp of creation */
    char     build_id[32];          /**< Build identifier string */
    uint8_t  reserved[200];         /**< Reserved for future use */
} __packed;

_Static_assert(sizeof(struct resource_partition_header) == 256, 
               "resource_partition_header must be 256 bytes");

/**
 * @brief Asset Table Entry (32 bytes)
 * Array of these follows the partition header
 */
struct resource_entry {
    uint32_t magic;                 /**< Must be RESOURCE_ASSET_MAGIC */
    uint32_t offset;                /**< Offset from partition start */
    uint32_t size;                  /**< Asset data size in bytes */
    uint32_t crc32;                 /**< CRC32 of asset data */
    uint16_t asset_id;              /**< Unique asset identifier */
    uint8_t  type;                  /**< xip_asset_type */
    uint8_t  format;                /**< xip_image_format or 0 */
    uint16_t width;                 /**< Image width (for images) */
    uint16_t height;                /**< Image height (for images) */
    uint8_t  reserved[8];           /**< Reserved */
} __packed;

_Static_assert(sizeof(struct resource_entry) == 32, 
               "resource_entry must be 32 bytes");

/**
 * @brief Image Asset Header (16 bytes)
 * Embedded at the start of each image asset's data
 */
struct resource_image_header {
    uint16_t width;                 /**< Image width in pixels */
    uint16_t height;                /**< Image height in pixels */
    uint8_t  format;                /**< xip_image_format */
    uint8_t  bits_per_pixel;        /**< 1, 4, 8, 16, 24, or 32 */
    uint16_t stride;                /**< Bytes per row (may include padding) */
    uint32_t data_size;             /**< Raw pixel data size */
    uint32_t palette_entries;       /**< 0 for non-indexed formats */
    /* Palette data follows (if indexed) */
    /* Pixel data follows palette */
} __packed;

_Static_assert(sizeof(struct resource_image_header) == 16, 
               "resource_image_header must be 16 bytes");

/* =============================================================================
 * Predefined Asset IDs
 * ============================================================================= */

/** Logo assets */
#define RESOURCE_ID_LOGO_BOOT      0x0001  /**< Boot splash logo */
#define RESOURCE_ID_LOGO_CHARGE    0x0002  /**< Charging indicator */
#define RESOURCE_ID_LOGO_SHUTDOWN  0x0003  /**< Shutdown logo */

/** Icon assets (0x0100 - ) */
#define RESOURCE_ID_ICON_HEART     0x0100  /**< Heart rate icon */
#define RESOURCE_ID_ICON_SPO2      0x0101  /**< SpO2 icon */
#define RESOURCE_ID_ICON_GLUCOSE   0x0102  /**< Glucose icon */
#define RESOURCE_ID_ICON_TEMP      0x0103  /**< Temperature icon */
#define RESOURCE_ID_ICON_BATTERY   0x0104  /**< Battery icon */
#define RESOURCE_ID_ICON_BLUETOOTH 0x0105  /**< Bluetooth icon */
#define RESOURCE_ID_ICON_WIFI      0x0106  /**< WiFi icon */
#define RESOURCE_ID_ICON_SETTINGS  0x0107  /**< Settings icon */
#define RESOURCE_ID_ICON_WARNING   0x0108  /**< Warning icon */
#define RESOURCE_ID_ICON_CHECK     0x0109  /**< Checkmark icon */
#define RESOURCE_ID_ICON_HEMOGLOBIN 0x010A /**< Hemoglobin icon */
#define RESOURCE_ID_ICON_RESP      0x010B  /**< Respiration icon */
#define RESOURCE_ID_ICON_INSULIN   0x010C  /**< Insulin icon */
#define RESOURCE_ID_ICON_HOMA      0x010D  /**< HOMA-IR icon */

/** Icon state variants (0x0110 - ) */
#define RESOURCE_ID_ICON_BATTERY_LOW      0x0110  /**< Battery low state icon */
#define RESOURCE_ID_ICON_BATTERY_CRITICAL 0x0111  /**< Battery critical state icon */
#define RESOURCE_ID_ICON_BLUETOOTH_OFF    0x0112  /**< Bluetooth disconnected state icon */
#define RESOURCE_ID_ICON_BLUETOOTH_ON     0x0113  /**< Bluetooth connected state icon */
#define RESOURCE_ID_ICON_WIFI_OFF         0x0114  /**< WiFi disconnected state icon */
#define RESOURCE_ID_ICON_WIFI_ON          0x0115  /**< WiFi connected state icon */

/** Font assets (0x0200 - )*/
#define RESOURCE_ID_FONT_SMALL     0x0200  /**< Small font (14pt) */
#define RESOURCE_ID_FONT_MEDIUM    0x0201  /**< Medium font (20pt) */
#define RESOURCE_ID_FONT_LARGE     0x0202  /**< Large font (28pt) */
#define RESOURCE_ID_FONT_NUMBERS   0x0203  /**< Large numbers only */

/* =============================================================================
 * API Functions
 * ============================================================================= */

/**
 * @brief Initialize Resources module
 * Loads NVS active slot, validates preferred then fallback, enables XIP access.
 * @return 0 on success, negative errno on failure
 */
int resource_init(void);

/**
 * @brief Validate a specific A/B slot without changing the active slot.
 * @param slot RESOURCE_SLOT_PRIMARY or RESOURCE_SLOT_SECONDARY
 * @return 0 if magic/version/CRC OK
 */
int resource_validate_slot(uint8_t slot);

/**
 * @brief Make @p slot the active read slot (validated + persisted to NVS).
 */
int resource_set_active_slot(uint8_t slot);

uint8_t resource_get_active_slot(void);
uint8_t resource_inactive_slot(void);
uintptr_t resource_slot_base(uint8_t slot);
uintptr_t resource_active_base(void);

/**
 * @brief Check if Resources are valid and accessible
 * @return true if XIP partition is valid
 */
bool resource_is_valid(void);

/**
 * @brief Get pointer to asset data by ID
 * @param asset_id Asset identifier (RESOURCE_ID_*)
 * @param size Output: size of asset data (can be NULL)
 * @return Pointer to asset data (XIP memory-mapped) or NULL if not found
 */
const void *resource_get(uint16_t asset_id, size_t *size);

/**
 * @brief Get pointer to asset data only if its CRC verifies (with retry).
 * @param asset_id Asset identifier (RESOURCE_ID_*)
 * @param size Output: size of asset data (can be NULL)
 * @return Pointer to asset data, or NULL if not found or CRC failed.
 */
const void *resource_get_verified(uint16_t asset_id, size_t *size);

/**
 * @brief Validate the CRC of every asset in the partition.
 * @return Number of assets that failed CRC (0 = all good), or -ENOENT if invalid.
 */
int resource_validate_all(void);

/**
 * @brief Re-prime the XIP mapping after a sleep / deep-power-down resume.
 *
 * Issues a real QSPI read of the partition header so the controller exits DPD
 * and re-fetches before the UI draws fonts/icons from XIP. Call from the
 * display wake / power-resume path.
 */
void resource_wake(void);

/**
 * @brief Get image asset with dimensions
 * @param asset_id Asset identifier
 * @param width Output: image width (can be NULL)
 * @param height Output: image height (can be NULL)
 * @return Pointer to raw pixel data or NULL if not found
 */
const void *resource_get_image(uint16_t asset_id, uint16_t *width, uint16_t *height);

/**
 * @brief Get LVGL image descriptor for Resource
 * Fills an lv_img_dsc_t structure pointing to XIP memory
 * @param asset_id Asset identifier
 * @param dsc Output: LVGL image descriptor to fill
 * @return 0 on success, negative errno on failure
 */
int resource_get_lvgl_image(uint16_t asset_id, void *dsc);

/**
 * @brief Get font asset
 * @param asset_id Font asset identifier
 * @return Pointer to font data or NULL if not found
 */
const void *resource_get_font(uint16_t asset_id);

/**
 * @brief Get number of assets in partition
 * @return Number of assets, or 0 if partition invalid
 */
uint32_t resource_get_count(void);

/**
 * @brief Get asset entry by index
 * @param index Asset index (0 to count-1)
 * @return Pointer to asset entry or NULL if invalid
 */
const struct resource_entry *resource_get_entry(uint32_t index);

/**
 * @brief Print Resources info to log
 */
void resource_print_info(void);

/**
 * @brief Validate asset CRC
 * @param asset_id Asset to validate
 * @return true if CRC matches
 */
bool resource_validate_crc(uint16_t asset_id);

/**
 * @brief Get XIP partition header
 * @return Pointer to partition header
 */
const struct resource_partition_header *resource_get_header(void);

/* =============================================================================
 * Utility Macros
 * ============================================================================= */

/**
 * @brief Get XIP memory-mapped address for an offset within the *active* partition
 */
#define RESOURCE_MMAP_ADDR(offset) ((const void *)(resource_active_base() + (uintptr_t)(offset)))

/**
 * @brief Check if address is within either Resources A/B region
 */
#define RESOURCE_IS_VALID_ADDR(addr) \
    ((((uintptr_t)(addr) >= RESOURCE_PRIMARY_BASE) && \
      ((uintptr_t)(addr) < (RESOURCE_PRIMARY_BASE + RESOURCE_PARTITION_SIZE))) || \
     (((uintptr_t)(addr) >= RESOURCE_SECONDARY_BASE) && \
      ((uintptr_t)(addr) < (RESOURCE_SECONDARY_BASE + RESOURCE_PARTITION_SIZE))))

#ifdef __cplusplus
}
#endif

#endif /* RESOURCE_H */
