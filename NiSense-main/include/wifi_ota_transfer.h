#ifndef WIFI_OTA_TRANSFER_H
#define WIFI_OTA_TRANSFER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Apply OTA assets requested by bulk-session START flags while Wi-Fi is up.
 * Streams GET /ota/<asset> into the inactive flash slots (model/resource).
 * Firmware images are not applied here (use BLE SMP); returns 0 for that flag.
 */
int wifi_ota_apply_requested(uint16_t flags);

int wifi_ota_fetch_into(const char *path, uint8_t *dst, size_t dst_cap, size_t *out_len);
int wifi_ota_fetch_model(uint8_t *dst, size_t dst_cap, size_t *out_len);
int wifi_ota_fetch_resource(uint8_t *dst, size_t dst_cap, size_t *out_len);
int wifi_ota_fetch_firmware(uint8_t *dst, size_t dst_cap, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_OTA_TRANSFER_H */
