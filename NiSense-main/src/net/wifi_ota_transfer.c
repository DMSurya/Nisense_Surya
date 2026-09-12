/**
 * @file wifi_ota_transfer.c
 * @brief Fetch OTA objects from the phone HTTP server over Wi-Fi (chunked hex).
 *
 * When bulk-session START flags request OTA, stream model/resource images into
 * the inactive QSPI slots. Firmware remains BLE SMP (MCUboot).
 */

#include "wifi_bulk_session.h"
#include "wifi_ota_transfer.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <errno.h>
#include <string.h>

#if defined(CONFIG_GLUCOSE_MATRICES_FROM_XIP) || defined(CONFIG_RESOURCE)
#include "nor_qspi_busy.h"
#include "ota_compat.h"
#endif

#if defined(CONFIG_GLUCOSE_MATRICES_FROM_XIP)
#include "glucose_model_xip.h"
#endif

#if defined(CONFIG_RESOURCE)
#include "resource.h"
#endif

LOG_MODULE_REGISTER(wifi_ota_xfer, CONFIG_LOG_DEFAULT_LEVEL);

#define WIFI_OTA_STAGE_SIZE 256U

struct wifi_ota_sink {
	uint8_t *buf;
	size_t cap;
	size_t used;
};

#if defined(CONFIG_GLUCOSE_MATRICES_FROM_XIP) || defined(CONFIG_RESOURCE)
struct wifi_ota_flash_sink {
	const struct flash_area *fa;
	uint8_t stage[WIFI_OTA_STAGE_SIZE];
	size_t stage_len;
	uint32_t write_off;
	uint32_t received;
	uint32_t max_size;
};
#endif

static int wifi_ota_append(const uint8_t *data, size_t len, void *user_data)
{
	struct wifi_ota_sink *sink = user_data;

	if (sink == NULL || data == NULL) {
		return -EINVAL;
	}
	if (sink->used + len > sink->cap) {
		return -ENOMEM;
	}
	memcpy(sink->buf + sink->used, data, len);
	sink->used += len;
	return 0;
}

#if defined(CONFIG_GLUCOSE_MATRICES_FROM_XIP) || defined(CONFIG_RESOURCE)
static int flash_flush(struct wifi_ota_flash_sink *sink, const uint8_t *data, size_t len)
{
	int rc;

	if (sink == NULL || sink->fa == NULL || data == NULL || len == 0U) {
		return -EINVAL;
	}
	if (sink->write_off + len > sink->max_size) {
		return -ENOMEM;
	}

	nor_qspi_busy_acquire();
	rc = flash_area_write(sink->fa, sink->write_off, data, len);
	nor_qspi_busy_release();
	if (rc != 0) {
		return rc;
	}
	sink->write_off += (uint32_t)len;
	return 0;
}

static int flash_sink_write(const uint8_t *data, size_t len, void *user_data)
{
	struct wifi_ota_flash_sink *sink = user_data;

	if (sink == NULL || data == NULL) {
		return -EINVAL;
	}

	while (len > 0U) {
		size_t take = WIFI_OTA_STAGE_SIZE - sink->stage_len;

		if (take > len) {
			take = len;
		}
		memcpy(&sink->stage[sink->stage_len], data, take);
		sink->stage_len += take;
		data += take;
		len -= take;
		sink->received += (uint32_t)take;

		if (sink->stage_len == WIFI_OTA_STAGE_SIZE) {
			int rc = flash_flush(sink, sink->stage, WIFI_OTA_STAGE_SIZE);

			if (rc != 0) {
				return rc;
			}
			sink->stage_len = 0U;
		}
	}
	return 0;
}

static int flash_sink_finish(struct wifi_ota_flash_sink *sink)
{
	int rc = 0;

	if (sink->stage_len > 0U) {
		size_t wlen = sink->stage_len;

		while ((wlen % 4U) != 0U) {
			sink->stage[wlen++] = 0xFF;
		}
		rc = flash_flush(sink, sink->stage, wlen);
		sink->stage_len = 0U;
	}
	return rc;
}
#endif /* CONFIG_GLUCOSE_MATRICES_FROM_XIP || CONFIG_RESOURCE */

#if defined(CONFIG_GLUCOSE_MATRICES_FROM_XIP)
static uint16_t model_flash_id(uint8_t slot)
{
	return (slot == GLUCOSE_MODEL_SLOT_SECONDARY)
		       ? FIXED_PARTITION_ID(model_secondary_partition)
		       : FIXED_PARTITION_ID(model_primary_partition);
}

static int wifi_ota_apply_model(void)
{
	struct wifi_ota_flash_sink sink;
	uint8_t slot = glucose_model_xip_inactive_slot();
	const struct flash_area *fa = NULL;
	const struct glucose_model_header *hdr;
	int rc;

	memset(&sink, 0, sizeof(sink));
	sink.max_size = GLUCOSE_MODEL_PARTITION_SIZE;

	rc = flash_area_open(model_flash_id(slot), &fa);
	if (rc != 0) {
		return rc;
	}
	sink.fa = fa;

	nor_qspi_busy_acquire();
	rc = flash_area_erase(fa, 0, GLUCOSE_MODEL_PARTITION_SIZE);
	nor_qspi_busy_release();
	if (rc != 0) {
		flash_area_close(fa);
		return rc;
	}

	LOG_INF("Wi-Fi OTA: streaming model into slot %u", slot);
	rc = wifi_bulk_get_asset("/ota/model", flash_sink_write, &sink);
	if (rc == 0) {
		rc = flash_sink_finish(&sink);
	}
	flash_area_close(fa);
	sink.fa = NULL;
	if (rc != 0) {
		LOG_ERR("Wi-Fi OTA model fetch failed: %d", rc);
		return rc;
	}

	rc = glucose_model_xip_validate_slot(slot);
	if (rc != 0) {
		LOG_ERR("Wi-Fi OTA model validate failed: %d", rc);
		return rc;
	}

	hdr = glucose_model_xip_peek_header(slot);
	rc = ota_compat_check_model(hdr);
	if (rc != 0) {
		LOG_ERR("Wi-Fi OTA model compat rejected: %d", rc);
		return rc;
	}

	rc = glucose_model_xip_set_active_slot(slot);
	if (rc != 0) {
		return rc;
	}

	extern int glucose_model_revalidate(void);

	(void)glucose_model_revalidate();
	LOG_INF("Wi-Fi OTA model activated slot %u (%u bytes)", slot, sink.received);
	return 0;
}
#endif /* CONFIG_GLUCOSE_MATRICES_FROM_XIP */

#if defined(CONFIG_RESOURCE)
static uint16_t resource_flash_id(uint8_t slot)
{
	return (slot == RESOURCE_SLOT_SECONDARY)
		       ? FIXED_PARTITION_ID(resource_secondary_partition)
		       : FIXED_PARTITION_ID(resource_primary_partition);
}

static int wifi_ota_apply_resource(void)
{
	struct wifi_ota_flash_sink sink;
	uint8_t slot = resource_inactive_slot();
	const struct flash_area *fa = NULL;
	int rc;

	memset(&sink, 0, sizeof(sink));
	sink.max_size = RESOURCE_PARTITION_SIZE;

	rc = flash_area_open(resource_flash_id(slot), &fa);
	if (rc != 0) {
		return rc;
	}
	sink.fa = fa;

	nor_qspi_busy_acquire();
	rc = flash_area_erase(fa, 0, RESOURCE_PARTITION_SIZE);
	nor_qspi_busy_release();
	if (rc != 0) {
		flash_area_close(fa);
		return rc;
	}

	LOG_INF("Wi-Fi OTA: streaming resource into slot %u", slot);
	rc = wifi_bulk_get_asset("/ota/resource", flash_sink_write, &sink);
	if (rc == 0) {
		rc = flash_sink_finish(&sink);
	}
	flash_area_close(fa);
	sink.fa = NULL;
	if (rc != 0) {
		LOG_ERR("Wi-Fi OTA resource fetch failed: %d", rc);
		return rc;
	}

	rc = resource_validate_slot(slot);
	if (rc != 0) {
		LOG_ERR("Wi-Fi OTA resource validate failed: %d", rc);
		return rc;
	}

	{
		const struct resource_partition_header *hdr =
			(const struct resource_partition_header *)resource_slot_base(slot);

		rc = ota_compat_check_resource(hdr);
		if (rc != 0) {
			LOG_ERR("Wi-Fi OTA resource compat rejected: %d", rc);
			return rc;
		}
	}

	rc = resource_set_active_slot(slot);
	if (rc != 0) {
		return rc;
	}

	LOG_INF("Wi-Fi OTA resource activated slot %u (%u bytes)", slot, sink.received);
	return 0;
}
#endif /* CONFIG_RESOURCE */

int wifi_ota_apply_requested(uint16_t flags)
{
	int rc = 0;
	int first_err = 0;

	if ((flags & (WIFI_BULK_FLAG_OTA_MODEL | WIFI_BULK_FLAG_OTA_RESOURCE |
		      WIFI_BULK_FLAG_OTA_FIRMWARE)) == 0U) {
		return 0;
	}

	if ((flags & WIFI_BULK_FLAG_OTA_MODEL) != 0U) {
#if defined(CONFIG_GLUCOSE_MATRICES_FROM_XIP)
		rc = wifi_ota_apply_model();
#else
		rc = -ENOTSUP;
		LOG_WRN("Wi-Fi OTA model requested but XIP model store disabled");
#endif
		if (rc != 0 && first_err == 0) {
			first_err = rc;
		}
	}

	if ((flags & WIFI_BULK_FLAG_OTA_RESOURCE) != 0U) {
#if defined(CONFIG_RESOURCE)
		rc = wifi_ota_apply_resource();
#else
		rc = -ENOTSUP;
		LOG_WRN("Wi-Fi OTA resource requested but RESOURCE disabled");
#endif
		if (rc != 0 && first_err == 0) {
			first_err = rc;
		}
	}

	if ((flags & WIFI_BULK_FLAG_OTA_FIRMWARE) != 0U) {
		/* MCUboot image upload stays on BLE SMP (phone pushes after session). */
		LOG_INF("Wi-Fi OTA firmware flag set; apply via BLE SMP after session");
	}

	return first_err;
}

int wifi_ota_fetch_into(const char *path, uint8_t *dst, size_t dst_cap, size_t *out_len)
{
	struct wifi_ota_sink sink = {
		.buf = dst,
		.cap = dst_cap,
		.used = 0U,
	};
	int rc;

	if (path == NULL || dst == NULL || dst_cap == 0U) {
		return -EINVAL;
	}

	rc = wifi_bulk_get_asset_chunked(path, (uint32_t)dst_cap, wifi_ota_append, &sink);
	if (rc != 0) {
		LOG_ERR("Wi-Fi OTA fetch %s failed: %d", path, rc);
		return rc;
	}
	if (out_len != NULL) {
		*out_len = sink.used;
	}
	LOG_INF("Wi-Fi OTA fetched %s (%u bytes)", path, (unsigned int)sink.used);
	return 0;
}

int wifi_ota_fetch_model(uint8_t *dst, size_t dst_cap, size_t *out_len)
{
	return wifi_ota_fetch_into("/ota/model", dst, dst_cap, out_len);
}

int wifi_ota_fetch_resource(uint8_t *dst, size_t dst_cap, size_t *out_len)
{
	return wifi_ota_fetch_into("/ota/resource", dst, dst_cap, out_len);
}

int wifi_ota_fetch_firmware(uint8_t *dst, size_t dst_cap, size_t *out_len)
{
	return wifi_ota_fetch_into("/ota/firmware", dst, dst_cap, out_len);
}
