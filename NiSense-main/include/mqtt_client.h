/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * MQTT Client Wrapper
 *
 * Lightweight application-level wrapper for MQTT/MQTTS operations
 * using the WExx UART-based Wi-Fi module driver APIs.
 */

#ifndef MQTT_CLIENT_H_
#define MQTT_CLIENT_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <zephyr/drivers/wifi/wexx.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the MQTT client module
 *
 * Resolves the underlying WExx device and verifies it is ready.
 *
 * @return 0 on success, negative errno on failure
 */
int mqtt_client_init(void);

/**
 * @brief Connect to an MQTT broker
 *
 * @param client_id Unique client identifier. If NULL or empty, a unique ID is auto-generated.
 * @param broker Broker address (host name or IP)
 * @param port Broker port (typically 1883 for TCP, 8883 for SSL)
 * @param ssl Use SSL/TLS if true, TCP if false
 * @param username Credentials username. Pass NULL if anonymous.
 * @param password Credentials password. Pass NULL if anonymous.
 * @return 0 on success, negative errno on failure
 */
int mqtt_client_connect(const char *client_id, const char *broker, uint16_t port, bool ssl,
                        const char *username, const char *password);

/**
 * @brief Disconnect from the MQTT broker
 *
 * @return 0 on success, negative errno on failure
 */
int mqtt_client_disconnect(void);

/**
 * @brief Publish a message payload to a topic
 *
 * @param topic Target publication topic
 * @param payload Payload data buffer
 * @param payload_len Size of payload buffer in bytes
 * @param qos Quality of Service level (0, 1, or 2)
 * @param retain Retain flag
 * @return 0 on success, negative errno on failure
 */
int mqtt_client_publish(const char *topic, const uint8_t *payload, size_t payload_len,
                        uint8_t qos, bool retain);

/**
 * @brief Subscribe to a topic
 *
 * @param topic Subscription topic path
 * @param qos Quality of Service level
 * @return 0 on success, negative errno on failure
 */
int mqtt_client_subscribe(const char *topic, uint8_t qos);

/**
 * @brief Set callback handler for incoming MQTT publications
 *
 * @param cb Callback function matching wexx_mqtt_msg_cb_t
 * @param user_data Callback user context payload
 */
void mqtt_client_set_callback(wexx_mqtt_msg_cb_t cb, void *user_data);

/**
 * @brief Check if the MQTT client is currently connected
 *
 * @return true if connected, false otherwise
 */
bool mqtt_client_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_CLIENT_H_ */
