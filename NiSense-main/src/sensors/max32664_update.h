/**
 * @file max32664_update.h
 * @brief MAX32664 firmware update over I2C
 *
 * Primary path: host-held .msbl buffer or QSPI `app` partition
 * (see include/app_data.h). FatFS `/NAND:` is not supported.
 */

#ifndef MAX32664_UPDATE_H_
#define MAX32664_UPDATE_H_

#include <stddef.h>
#include <stdint.h>
#include <zephyr/device.h>

/**
 * @brief Progress callback for firmware update
 * @param current_page Current page being written (0-based completed count)
 * @param total_pages Total number of pages
 * @param user_data User-provided context pointer
 */
typedef void (*max32664_update_progress_cb)(uint32_t current_page, uint32_t total_pages,
					     void *user_data);

/**
 * @brief Update MAX32664 firmware from an in-memory .msbl image (primary API).
 *
 * @param dev MAX32664 device
 * @param msbl Pointer to full .msbl (header + pages)
 * @param msbl_len Byte length of image
 * @param name_hint Optional filename for version parsing (may be NULL)
 * @param progress_cb Optional progress callback
 * @param user_data Passed to progress_cb
 * @return 0 on success, negative errno on failure
 */
int max32664_update_firmware_buf(const struct device *dev,
				 const uint8_t *msbl, size_t msbl_len,
				 const char *name_hint,
				 max32664_update_progress_cb progress_cb,
				 void *user_data);

/**
 * @brief Update from QSPI `app` partition XIP mapping (APP_DATA_RESOURCE_MMAP_ADDR).
 *
 * Expects a complete .msbl already programmed into the app partition.
 */
int max32664_update_firmware_from_app_partition(const struct device *dev,
						max32664_update_progress_cb progress_cb,
						void *user_data);

/**
 * @brief Path-based update — only when a real FS is configured.
 *
 * Without FatFS this returns -ENOTSUP. Prefer buffer / app-partition APIs.
 */
int max32664_update_firmware(const struct device *dev, const char *firmware_path,
			     max32664_update_progress_cb progress_cb, void *user_data);

/**
 * @brief Read MAX32664 firmware version (application mode).
 */
int max32664_read_firmware_version(const struct device *dev, uint8_t *major,
				   uint8_t *minor, uint8_t *patch);

int max32664_enter_bootloader(const struct device *dev);
int max32664_exit_bootloader(const struct device *dev);

/**
 * @brief Host-side MSBL header sanity (size / page count). Hub still authenticates.
 *
 * @return 0 if plausible, negative errno otherwise; optionally fills num_pages.
 */
int max32664_msbl_sanity_check(const uint8_t *msbl, size_t msbl_len, uint8_t *num_pages_out);

/**
 * @brief Host provenance gate before erase (Level 1 allow-list + optional SHA-256).
 *
 * @return 0 if allowed, -EPERM if rejected, -EINVAL on bad args
 */
int max32664_msbl_policy_check(const uint8_t *msbl, size_t msbl_len,
			       const char *name_hint, uint8_t ver_major,
			       uint8_t ver_minor, uint8_t ver_patch);

#endif /* MAX32664_UPDATE_H_ */
