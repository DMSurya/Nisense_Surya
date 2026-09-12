/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Cloud telemetry — MQTT publish of measurement results when Wi-Fi is up.
 * Broker settings come from config.json (cloud section).
 */

#ifndef CLOUD_TELEMETRY_H_
#define CLOUD_TELEMETRY_H_

#include <stdint.h>

struct ble_vitals_data;
struct ble_glucose_data;
struct ble_wifi_status;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize cloud telemetry (MQTT client + work items).
 * @return 0 on success, negative errno on failure
 */
int cloud_telemetry_init(void);

/**
 * @brief React to Wi-Fi status changes (connect / disconnect).
 * Schedules MQTT connect after a short delay when connected.
 */
void cloud_telemetry_on_wifi_status(const struct ble_wifi_status *status);

/**
 * @brief Queue vitals JSON publish to MQTT (non-blocking).
 */
void cloud_telemetry_publish_vitals(const struct ble_vitals_data *data);

/**
 * @brief Queue glucose JSON publish to MQTT (non-blocking).
 */
void cloud_telemetry_publish_glucose(const struct ble_glucose_data *data);

#ifdef __cplusplus
}
#endif

#endif /* CLOUD_TELEMETRY_H_ */
