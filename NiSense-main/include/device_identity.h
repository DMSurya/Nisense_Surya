/**
 * @file device_identity.h
 * @brief nRF hardware device ID as uppercase hex (first 8 bytes)
 */

#ifndef DEVICE_IDENTITY_H_
#define DEVICE_IDENTITY_H_

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Format hwinfo device ID into buf (uppercase hex, no separators).
 *
 * @return Number of hex characters written (excluding NUL), or negative errno.
 */
int device_identity_get_hex(char *buf, size_t len);

/**
 * @brief Low 32 bits of the hwinfo device ID (little-endian), 0 if unavailable.
 */
uint32_t device_identity_get_u32(void);

#endif /* DEVICE_IDENTITY_H_ */
