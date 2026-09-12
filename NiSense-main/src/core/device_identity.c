/**
 * @file device_identity.c
 * @brief Shared hwinfo-based device identity string
 */

#include "device_identity.h"

#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/byteorder.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

uint32_t device_identity_get_u32(void)
{
	uint8_t dev_id[16];
	ssize_t id_len = hwinfo_get_device_id(dev_id, sizeof(dev_id));

	if (id_len < 4) {
		return 0U;
	}
	return sys_get_le32(dev_id);
}

int device_identity_get_hex(char *buf, size_t len)
{
	uint8_t dev_id[16];
	ssize_t id_len;
	int written = 0;

	if (buf == NULL || len == 0) {
		return -EINVAL;
	}

	id_len = hwinfo_get_device_id(dev_id, sizeof(dev_id));
	if (id_len <= 0) {
		written = snprintf(buf, len, "UNKNOWN");
		return (written > 0 && (size_t)written < len) ? written : -EIO;
	}

	for (int i = 0; i < id_len && i < 8; i++) {
		int n = snprintf(buf + written, len - (size_t)written, "%02X", dev_id[i]);
		if (n <= 0 || (size_t)(written + n) >= len) {
			return -ENOMEM;
		}
		written += n;
	}

	buf[written] = '\0';
	return written;
}
