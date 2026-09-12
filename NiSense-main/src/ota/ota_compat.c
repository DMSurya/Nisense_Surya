/**
 * @file ota_compat.c
 * @brief Firmware ↔ Resource / glucose-model compatibility gates for OTA COMMIT.
 */

#include "ota_compat.h"

#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ota_compat, CONFIG_LOG_DEFAULT_LEVEL);

static const struct ota_compat g_compat = {
	.resource_format_min = RESOURCE_PARTITION_VERSION,
	.model_format_min = GLUCOSE_MODEL_FORMAT_VERSION,
	.model_version_min = 0U, /* any CRC-valid model with matching format */
	.model_variant_mask = OTA_COMPAT_MODEL_VARIANT_ANY,
};

const struct ota_compat *ota_compat_get(void)
{
	return &g_compat;
}

int ota_compat_check_model(const struct glucose_model_header *hdr)
{
	const struct ota_compat *c = ota_compat_get();

	if (hdr == NULL) {
		return -EINVAL;
	}
	if (hdr->magic != GLUCOSE_MODEL_MAGIC) {
		return -ENODATA;
	}
	if (hdr->format_version < c->model_format_min) {
		LOG_ERR("model format %u < required %u",
			hdr->format_version, c->model_format_min);
		return -ENOTSUP;
	}
	if (hdr->format_version != GLUCOSE_MODEL_FORMAT_VERSION) {
		LOG_ERR("model format %u != firmware format %u",
			hdr->format_version, GLUCOSE_MODEL_FORMAT_VERSION);
		return -ENOTSUP;
	}
	if (((1U << hdr->variant) & c->model_variant_mask) == 0U) {
		LOG_ERR("model variant %u not in mask 0x%x",
			hdr->variant, c->model_variant_mask);
		return -ENOTSUP;
	}
	if (hdr->model_version < c->model_version_min) {
		LOG_ERR("model version %u < required %u",
			hdr->model_version, c->model_version_min);
		return -ENOTSUP;
	}
	return 0;
}

int ota_compat_check_resource(const struct resource_partition_header *hdr)
{
	const struct ota_compat *c = ota_compat_get();

	if (hdr == NULL) {
		return -EINVAL;
	}
	if (hdr->magic != RESOURCE_PARTITION_MAGIC) {
		return -ENODATA;
	}
	if (hdr->version < c->resource_format_min) {
		LOG_ERR("resource format %u < required %u",
			hdr->version, c->resource_format_min);
		return -ENOTSUP;
	}
	if (hdr->version != RESOURCE_PARTITION_VERSION) {
		LOG_ERR("resource format %u != firmware format %u",
			hdr->version, RESOURCE_PARTITION_VERSION);
		return -ENOTSUP;
	}
	if (hdr->total_size < sizeof(*hdr) ||
	    hdr->total_size > RESOURCE_PARTITION_SIZE) {
		return -EINVAL;
	}
	if (hdr->asset_count == 0U || hdr->asset_count > RESOURCE_MAX_ENTRIES) {
		return -EINVAL;
	}
	return 0;
}
