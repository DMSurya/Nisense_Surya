/* =============================================================================
 * OTA compatibility contract (app ↔ Resource store ↔ glucose model)
 * =============================================================================
 * The running firmware declares minimum formats/versions it accepts. Bundle
 * COMMIT paths call these checks after CRC(/signature) validation and before
 * flipping the active A/B slot. Manifest `provides`/`requires` on the phone
 * are advisory; device COMMIT is authoritative.
 * ============================================================================= */

#ifndef OTA_COMPAT_H
#define OTA_COMPAT_H

#include <stdint.h>
#include <stdbool.h>

#include "glucose_model_xip.h"
#include "resource.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Bitmask for accepted glucose model variants. */
#define OTA_COMPAT_MODEL_VARIANT_WEARABLE  (1U << GLUCOSE_MODEL_VARIANT_WEARABLE)
#define OTA_COMPAT_MODEL_VARIANT_PULSE     (1U << GLUCOSE_MODEL_VARIANT_PULSE)
#define OTA_COMPAT_MODEL_VARIANT_ANY \
	(OTA_COMPAT_MODEL_VARIANT_WEARABLE | OTA_COMPAT_MODEL_VARIANT_PULSE)

/**
 * @brief Firmware acceptance table (compile-time defaults; override in one place).
 *
 * model_version_min 0 = any CRC-valid model with matching format/variant.
 */
struct ota_compat {
	uint16_t resource_format_min;
	uint16_t model_format_min;
	uint32_t model_version_min;
	uint16_t model_variant_mask;
};

/** Default compatibility accepted by this firmware image. */
const struct ota_compat *ota_compat_get(void);

/**
 * @brief Validate a packed glucose-model header against @ref ota_compat_get.
 * @return 0 if acceptable, negative errno otherwise (-ENOTSUP/-EINVAL).
 */
int ota_compat_check_model(const struct glucose_model_header *hdr);

/**
 * @brief Validate an Resource store partition header against @ref ota_compat_get.
 * @return 0 if acceptable, negative errno otherwise.
 */
int ota_compat_check_resource(const struct resource_partition_header *hdr);

#ifdef __cplusplus
}
#endif

#endif /* OTA_COMPAT_H */
