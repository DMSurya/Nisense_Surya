/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * HTTP Client Wrapper
 *
 * Lightweight application-level wrapper for HTTP/HTTPS operations
 * using the WExx UART-based Wi-Fi module driver APIs.
 */

#ifndef HTTP_CLIENT_H_
#define HTTP_CLIENT_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the HTTP client module
 *
 * Resolves the underlying WExx device and verifies it is ready.
 *
 * @return 0 on success, negative errno on failure
 */
int http_client_init(void);

/**
 * @brief Perform an HTTP GET request
 *
 * @param host Target hostname (e.g. "testbin.com")
 * @param path URL path (e.g. "/get")
 * @param port Target port (typically 80 for HTTP, 443 for HTTPS)
 * @param ssl Use HTTPS if true, HTTP if false
 * @param response Buffer to store the response payload
 * @param resp_len Size of the response buffer
 * @return HTTP status code (e.g. 200) on success, negative errno on failure
 */
int http_client_get(const char *host, const char *path, uint16_t port, bool ssl,
                    char *response, size_t resp_len);

/**
 * @brief Perform an HTTP POST request
 *
 * @param host Target hostname (e.g. "testbin.com")
 * @param path URL path (e.g. "/post")
 * @param port Target port (typically 80 for HTTP, 443 for HTTPS)
 * @param ssl Use HTTPS if true, HTTP if false
 * @param data POST data payload (e.g. JSON string)
 * @param response Buffer to store the response payload
 * @param resp_len Size of the response buffer
 * @return HTTP status code (e.g. 200) on success, negative errno on failure
 */
int http_client_post(const char *host, const char *path, uint16_t port, bool ssl,
                     const char *data, char *response, size_t resp_len);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_CLIENT_H_ */
