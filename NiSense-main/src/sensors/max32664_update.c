/**
 * @file max32664_update.c
 * @brief MAX32664 firmware update implementation
 * 
 * Bootloader protocol (per MAX32664 User Guide Table 9):
 * - Enter bootloader: Family 0x01, Index 0x00, Data 0x08
 * - Set IV:           Family 0x80, Index 0x00, Data [11 bytes from .msbl @ 0x28-0x32]
 * - Set Auth:         Family 0x80, Index 0x01, Data [16 bytes from .msbl @ 0x34-0x43]
 * - Set num pages:    Family 0x80, Index 0x02, Data [0x00, num_pages]  (byte 0x44)
 * - Erase app:        Family 0x80, Index 0x03  (CMD_DELAY = 1400ms)
 * - Write page:       Family 0x80, Index 0x04, Data [8192 bytes + 16-byte CRC from .msbl]  (CMD_DELAY = 680ms)
 * - Flash app:        Family 0x80, Index 0x06  (optional finalize; see Kconfig)
 * - Exit bootloader:  Family 0x01, Index 0x00, Data 0x00
 *
 * Bootloader latch (ADI UG p.17 / Table 9):
 *   After GPIO RSTN/MFIO entry, send 0x01 0x00 0x08 within ~780 ms of BL
 *   readiness or a valid app image will auto-jump out of bootloader.
 *
 * Page Write Protocol:
 * Each page write transaction:
 *   [0x80, 0x04, <8192 bytes page data>, <16 bytes CRC from .msbl file>]
 * Total: 8210 bytes per page.
 *
 * The 16-byte CRC is read directly from the .msbl file (it follows each
 * 8192-byte page block in the file).  Do NOT compute it independently.
 * Wait 680ms after sending before reading status (per MAX32664 datasheet).
 *
 * Stability guardrails (activated before flashing):
 * - health_sched / PPG / accel feeder quiesced
 * - PM idle disabled
 *
 * Memory requirements:
 * - TX buffer: 8198 bytes (heap-allocated)
 * Total: ~8KB heap
 */

#include "max32664_update.h"
#include <app_data.h>
#include <zephyr/drivers/sensor/max32664.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/policy.h>
#include <string.h>
#include <stdlib.h>

#if defined(CONFIG_APP_HEALTH_SCHED)
#include "health_sched.h"
#endif
#if defined(CONFIG_PPG_ALGO)
#include <zephyr/subsys/ppg_algo/ppg_algo.h>
#endif


LOG_MODULE_REGISTER(max32664_update, CONFIG_SENSOR_LOG_LEVEL);

/* Bootloader commands (Family 0x80 in bootloader mode)
 * Index table per MAX32664 User Guide Table 9:
 *   0x00  Set Initialization Vector     (11 bytes from .msbl @ 0x28-0x32)
 *   0x01  Set Authentication bytes      (16 bytes from .msbl @ 0x34-0x43)
 *   0x02  Set Number of Pages           (2-byte big-endian: [0x00, num_pages])
 *   0x03  Erase application flash       (CMD_DELAY = 1400ms)
 *   0x04  Write page                    (8192 data + 16 CRC bytes, CMD_DELAY = 680ms)
 *   0x06  Flash application             (finalize flashed image)
 *   0x08  Get/set CRC
 */
#define BOOTLOADER_SET_IV       		0x00  	/* Set Initialization Vector (11 bytes from .msbl @ 0x28) */
#define BOOTLOADER_SET_AUTH     		0x01  	/* Set Authentication bytes (16 bytes from .msbl @ 0x34) */
#define BOOTLOADER_SET_NUM_PAGES		0x02  	/* Set number of pages */
#define BOOTLOADER_ERASE_APP    		0x03  	/* Erase application flash (CMD_DELAY = 1400ms) */
#define BOOTLOADER_WRITE_PAGE   		0x04  	/* Write page data (CMD_DELAY = 680ms) */
#define BOOTLOADER_FLASH_APP    		0x06  	/* Finalize flashed image */
#define BOOTLOADER_CRC          		0x08  	/* Get/set CRC */

#define FAMILY_CONFIG           		0x01  	/* Configuration family (mode changes) */
#define FAMILY_HUB_STATUS       		0x00  	/* Hub status read */
#define FAMILY_OUTPUT_MODE      		0x02  	/* Output/operating mode read */
#define FAMILY_BOOTLOADER       		0x80  	/* Bootloader family (flash operations) */
#define FAMILY_BOOTLOADER_INFO  		0x81  	/* Bootloader version / page size */
#define FAMILY_IDENTITY         		0xFF  	/* Device identity / firmware version */

/* Index bytes - FAMILY_CONFIG (0x01) */	
#define CONFIG_IDX_OP_MODE      		0x00  	/* Operating mode */

/* Index bytes - FAMILY_OUTPUT_MODE (0x02) */	
#define OUTPUT_MODE_IDX         		0x00  	/* Read current operating mode */

/* Index bytes - FAMILY_HUB_STATUS (0x00) */	
#define HUB_STATUS_IDX          		0x00  	/* Read hub status byte */

/* Index bytes - FAMILY_BOOTLOADER_INFO (0x81) */
#define BL_INFO_IDX_VERSION     		0x00  	/* Bootloader version */
#define BL_INFO_IDX_PAGE_SIZE   		0x01  	/* Bootloader page size */

/* Index bytes - FAMILY_IDENTITY (0xFF) */	
#define IDENTITY_IDX_MCU_TYPE   		0x00  	/* MCU type */
#define IDENTITY_IDX_FW_VERSION 		0x03  	/* Application firmware version */

/* Operating mode data values (used with CONFIG_IDX_OP_MODE) */
#define OP_MODE_APPLICATION     		0x00  	/* Enter application mode */
#define OP_MODE_BOOTLOADER      		0x08  	/* Enter bootloader mode */

/* Status byte values returned in every MAX32664 response (SS_STATUS, OEM SHComm.h) */
#define SS_SUCCESS              		0x00  	/* No error */
#define SS_ERR_COMMAND          		0x01  	/* Unrecognised command */
#define SS_ERR_UNAVAILABLE      		0x02  	/* Command not available in current mode */
#define SS_ERR_DATA_FORMAT      		0x03  	/* Bad data format */
#define SS_ERR_INPUT_VALUE      		0x04  	/* Invalid input value */
#define SS_ERR_BTLDR_GENERAL    		0x80  	/* Bootloader general error */
#define SS_ERR_BTLDR_CHECKSUM   		0x81  	/* Bootloader CRC mismatch — bad .msbl file */
#define SS_ERR_TRY_AGAIN        		0xFE  	/* Bootloader busy — retry after delay */
#define SS_ERR_UNKNOWN          		0xFF  	/* Unknown error */

/* MAX32664 page size (from datasheet) */	
#define MAX32664_PAGE_SIZE     			8192  	/* 8KB per page */
#define MAX32664_MAX_PAGES     			128   	/* Max pages in application area */

/* MSBL file format constants
 * File structure:
 *   [0x00-0x4B]  76-byte header
 *   [0x4C-0x205B]  Page 1: 8192 data + 16 checksum = 8208 bytes
 *   [0x205C-0x406B]  Page 2: 8192 data + 16 checksum = 8208 bytes
 *   ... etc
 */
#define MSBL_HEADER_SIZE        		0x4C  	/* Header size: 76 bytes (0x00-0x4B) */
#define MSBL_PAGE_DATA_SIZE     		8192  	/* Pure data size per page */
#define MSBL_PAGE_CHECKSUM_SIZE 		16    	/* Checksum bytes per page */
#define MSBL_PAGE_SIZE          		8208  	/* Page size in .msbl file (8192 + 16 checksum) */
#define MSBL_OFFSET_IV          		0x28  	/* Initialization Vector offset (11 bytes) */
#define MSBL_OFFSET_AUTH        		0x34  	/* Authentication bytes offset (16 bytes) */
#define MSBL_OFFSET_NUM_PAGES   		0x44  	/* Number of pages offset (1 byte) */

/* Page write timing settings */
#define INTER_PAGE_DELAY_MS     		15    	/* Delay after successful page write */
#define PAGE_FLASH_DELAY_MS     		CONFIG_MAX32664_UPDATE_PAGE_FLASH_DELAY_MS
#define BOOTLOADER_BUSY_RETRIES         CONFIG_MAX32664_UPDATE_PAGE_WRITE_RETRIES
#define BOOTLOADER_BUSY_RETRY_DELAY_MS  120
#define BOOTLOADER_BUSY_POLL_COUNT      20
#define BOOTLOADER_BUSY_POLL_DELAY_MS   50

/* Page write buffer layout:
 *   [0..1]          2-byte command header   (set by write_page_to_bootloader)
 *   [2..8193]    8192-byte page data        (pre-filled by caller from .msbl)
 *   [8194..8209]   16-byte CRC              (read from .msbl file by caller)
 * Total: 8210 bytes
 */
#define PAGE_TX_TOTAL  (2 + MAX32664_PAGE_SIZE + 16)  /* 8210 bytes */

#if defined(CONFIG_MAX32664_UPDATE_CHUNKED_WRITE)
#define MAX32664_CHUNK_SIZE  CONFIG_MAX32664_UPDATE_CHUNK_SIZE
#endif

/* Return human-readable name for a status byte (for LOG_ERR calls) */
static const char *ss_status_str(uint8_t status)
{
	switch (status) {
	case SS_SUCCESS:            return "SUCCESS";
	case SS_ERR_COMMAND:        return "ERR_COMMAND";
	case SS_ERR_UNAVAILABLE:    return "ERR_UNAVAILABLE";
	case SS_ERR_DATA_FORMAT:    return "ERR_DATA_FORMAT";
	case SS_ERR_INPUT_VALUE:    return "ERR_INPUT_VALUE";
	case SS_ERR_BTLDR_GENERAL:  return "ERR_BTLDR_GENERAL";
	case SS_ERR_BTLDR_CHECKSUM: return "ERR_BTLDR_CHECKSUM (bad CRC in .msbl)";
	case SS_ERR_TRY_AGAIN:      return "ERR_TRY_AGAIN (bootloader busy)";
	case SS_ERR_UNKNOWN:        return "ERR_UNKNOWN";
	default:                    return "UNRECOGNISED";
	}
}

/* ----- Stability Guardrails State ----- */
struct update_guardrails_state {
	bool pm_was_enabled;
	bool auto_was_enabled;
	bool feeder_was_running;
};

/**
 * @brief Pause schedulers / feeder that may contend for the hub I2C bus.
 */
static void activate_update_guardrails(struct update_guardrails_state *state,
				      const struct device *dev)
{
	LOG_INF("=== Activating stability guardrails ===");

	state->auto_was_enabled = false;
	state->feeder_was_running = max32664_accel_feeder_is_running();

#if defined(CONFIG_APP_HEALTH_SCHED)
	state->auto_was_enabled = health_sched_auto_enabled();
	if (state->auto_was_enabled) {
		health_sched_set_auto_enabled(false);
		LOG_INF("health_sched auto disabled for update");
	}
	if (health_sched_is_busy()) {
		int cret = health_sched_cancel_cycle();

		LOG_INF("health_sched cycle cancel: %d", cret);
	}
#endif

#if defined(CONFIG_PPG_ALGO)
	if (ppg_algo_is_measuring() || ppg_algo_is_finalizing()) {
		int pret = ppg_algo_stop_measurement();

		LOG_INF("ppg_algo stop for update: %d", pret);
	}
#endif

	(void)max32664_accel_feeder_stop(dev);
	LOG_INF("accel feeder stop (was_running=%d)", state->feeder_was_running);
	/* One feeder period so an in-flight tick finishes before BL entry. */
	k_msleep(50);

#if defined(CONFIG_PM)
	state->pm_was_enabled = true;
	LOG_INF("Disabling PM idle...");
	pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
#else
	state->pm_was_enabled = false;
#endif

	LOG_INF("Guardrails active - ready for firmware surgery");
}

static void deactivate_update_guardrails(const struct update_guardrails_state *state)
{
	LOG_INF("=== Deactivating stability guardrails ===");

#if defined(CONFIG_PM)
	if (state->pm_was_enabled) {
		LOG_DBG("Re-enabling PM idle...");
		pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
	}
#endif

#if defined(CONFIG_APP_HEALTH_SCHED)
	if (state->auto_was_enabled) {
		health_sched_set_auto_enabled(true);
		LOG_INF("health_sched auto restored");
	}
#endif
	/* Do not auto-restart feeder — next PPG/RAW start owns that. */

	LOG_INF("Guardrails deactivated - normal operation restored");
}

/* Firmware info extracted from .msbl file */
struct max32664_firmware_info {
	uint8_t num_pages;          /* Number of pages in firmware */
	uint8_t iv_bytes[11];       /* Initialization Vector */
	uint8_t auth_bytes[16];     /* Authentication bytes */
	uint8_t ver_major;          /* New firmware version (parsed from filename) */
	uint8_t ver_minor;
	uint8_t ver_patch;
};

/* I2C helper to send bootloader commands */
static int bootloader_i2c_command(const struct device *dev,
				  uint8_t family, uint8_t index,
				  const uint8_t *data, size_t data_len,
				  uint8_t *response, size_t response_len,	/* response_len always status + data*/
				  uint32_t delay_ms)
{
	struct max32664_bootloader_io io;
	const struct i2c_dt_spec *bus;
	uint8_t tx_buf[256];
	uint8_t rx_buf[256];
	int ret;

	ret = max32664_get_bootloader_io(dev, &io);
	if (ret) {
		LOG_ERR("Failed to get MAX32664 transport interface: %d", ret);
		return ret;
	}
	bus = &io.bus;
	
	LOG_INF("Command: Family=0x%02X, Index=0x%02X", family, index);

	if (data_len + 2 > sizeof(tx_buf)) {
		return -EINVAL;
	}
	
	/* Build command: [family, index, ...data] */
	tx_buf[0] = family;
	tx_buf[1] = index;
	if (data && data_len > 0) {
		memcpy(&tx_buf[2], data, data_len);
	}
	
	/* Log TX data */
	LOG_INF("  TX [%zu bytes]: %02X %02X ...", 2 + data_len, tx_buf[0], tx_buf[1]);
	if (data_len > 0 && data_len <= 32) {
		LOG_HEXDUMP_INF(tx_buf, 2 + data_len, "  TX full:");
	}

	/* Hold hub bus lock across write + delay + read (+ busy polls).
	 * Soft-recursive if caller already session-locked the update. */
	max32664_bus_lock(dev);
	
	/* Send command */
	ret = i2c_write_dt(bus, tx_buf, 2 + data_len);
	if (ret) {
		LOG_ERR("  I2C write failed: %d", ret);
		goto out_unlock;
	}
	
	/* Wait for processing */
	k_msleep(delay_ms);
	
	/* Read response if requested */
	if (response && response_len > 0) {
		ret = i2c_read_dt(bus, rx_buf, response_len);
		if (ret) {
			LOG_ERR("  I2C read failed: %d", ret);
			goto out_unlock;
		}
		
		/* Log RX data */
		LOG_INF("  RX [%zu bytes]: status=0x%02X", response_len, rx_buf[0]);
		if (response_len <= 16) {
			LOG_HEXDUMP_INF(rx_buf, response_len, "  RX full:");
		}
		
		/* Check status byte.
		 * If busy (0xFE), poll additional read-only responses before returning.
		 * Some MAX32664 bootloaders complete the command asynchronously and
		 * expect status polling rather than immediate command re-send. */
		if (rx_buf[0] == SS_ERR_TRY_AGAIN) {
			bool ready = false;

			for (int poll = 0; poll < BOOTLOADER_BUSY_POLL_COUNT; poll++) {
				k_msleep(BOOTLOADER_BUSY_POLL_DELAY_MS);

				ret = i2c_read_dt(bus, rx_buf, response_len);
				if (ret) {
					LOG_ERR("  I2C read poll failed: %d", ret);
					goto out_unlock;
				}

				if (rx_buf[0] == SS_SUCCESS) {
					LOG_DBG("  Bootloader ready after %d poll(s)", poll + 1);
					ready = true;
					break;
				}

				if (rx_buf[0] != SS_ERR_TRY_AGAIN) {
					LOG_ERR("  Command failed after busy poll: 0x%02x (%s)",
						rx_buf[0], ss_status_str(rx_buf[0]));
					ret = -EIO;
					goto out_unlock;
				}
			}

			if (!ready) {
				LOG_DBG("  Bootloader busy (0xFE) — caller should retry");
				ret = -EAGAIN;
				goto out_unlock;
			}
		}
		if (rx_buf[0] != SS_SUCCESS) {
			LOG_ERR("  Command failed: 0x%02x (%s)", rx_buf[0], ss_status_str(rx_buf[0]));
			ret = -EIO;
			goto out_unlock;
		}
		
		/* Copy response data (skip status byte) */
		memcpy(response, &rx_buf[1], response_len - 1);
	}

	ret = 0;

out_unlock:
	max32664_bus_unlock(dev);
	return ret;
}

/* Retry wrapper for commands that may see SS_ERR_TRY_AGAIN (0xFE)
 * immediately after bootloader entry or during flash operations. */
static int bootloader_i2c_command_with_retry(const struct device *dev,
					     uint8_t family, uint8_t index,
					     const uint8_t *data, size_t data_len,
					     uint8_t *response, size_t response_len,
					     uint32_t delay_ms,
					     uint8_t retries,
					     uint32_t retry_delay_ms,
					     const char *op_name)
{
	int ret;

	for (uint8_t attempt = 0; attempt <= retries; attempt++) {
		ret = bootloader_i2c_command(dev, family, index, data, data_len,
						    response, response_len, delay_ms);
		if (ret != -EAGAIN) {
			return ret;
		}

		if (attempt == retries) {
			LOG_ERR("%s: bootloader busy timeout after %u retries",
				op_name ? op_name : "Command", retries);
			return -ETIMEDOUT;
		}

		LOG_WRN("%s: bootloader busy (0xFE), retry %u/%u in %u ms",
			op_name ? op_name : "Command",
			attempt + 1, retries, retry_delay_ms);
		k_msleep(retry_delay_ms);
	}

	return -ETIMEDOUT;
}

/**
 * @brief Write a single page to MAX32664 bootloader
 * 
 * Protocol: [0x80, 0x04, <8192 bytes page data>, <16 bytes CRC from .msbl>]
 * followed by a status read after PAGE_FLASH_DELAY_MS (680ms).
 * 
 * The 16-byte CRC MUST be read from the .msbl file — it is NOT computed here.
 * tx_buf[2..8193] and tx_buf[8194..8209] must already be filled by the caller.
 * 
 * @param dev MAX32664 device handle
 * @param tx_buf Pre-allocated buffer (PAGE_TX_TOTAL = 8210 bytes):
 *               [0-1]:       Command header  (0x80, 0x04 - set by this function)
 *               [2..8193]:   8192-byte page data (pre-filled by caller from .msbl)
 *               [8194..8209]: 16-byte CRC    (pre-filled by caller from .msbl)
 * @return 0 on success, negative errno on failure
 *
 * Write path selected at compile time:
 *   CONFIG_MAX32664_UPDATE_CHUNKED_WRITE=y (default)
 *     -> Multiple i2c_write() calls of CHUNK_SIZE bytes each.
 *        Each call issues START + data + STOP.  The bootloader buffers
 *        incoming data across transactions.
 *   CONFIG_MAX32664_UPDATE_CHUNKED_WRITE=n
 *     -> Single i2c_write() for the full 8210-byte page.
 */
static int write_page_to_bootloader(const struct device *dev, uint8_t *tx_buf)
{
	struct max32664_bootloader_io io;
	const struct i2c_dt_spec *bus;
	int ret;
	uint32_t start_time, elapsed_ms;

	ret = max32664_get_bootloader_io(dev, &io);
	if (ret) {
		LOG_ERR("Failed to get MAX32664 transport interface: %d", ret);
		return ret;
	}
	bus = &io.bus;

	start_time = k_uptime_get_32();

	/* Set command header */
	tx_buf[0] = FAMILY_BOOTLOADER;       /* 0x80 */
	tx_buf[1] = BOOTLOADER_WRITE_PAGE;   /* 0x04 */

	/* tx_buf[2..8193] = page data    (pre-filled by caller from .msbl)
	 * tx_buf[8194..8209] = 16B CRC   (pre-filled by caller from .msbl)
	 * No checksum computation needed - CRC comes directly from the file. */

	/* Keep chunked/single-shot page traffic exclusive of feeder/sampling. */
	max32664_bus_lock(dev);

#if defined(CONFIG_MAX32664_UPDATE_CHUNKED_WRITE)
	/* Chunked path: stream full PAGE_TX_TOTAL (header + data + 16B CRC = 8210B)
	 * in CHUNK_SIZE chunks.  Each chunk is a separate i2c_write() call. */
	{
		uint8_t *ptr = tx_buf;
		size_t remaining = PAGE_TX_TOTAL;
		int chunk_num = 0;
#if 1
		int total_chunks = (int)DIV_ROUND_UP(PAGE_TX_TOTAL, MAX32664_CHUNK_SIZE);
		LOG_INF("  Chunked write: %d B/chunk, %d chunks, %d B total",
			MAX32664_CHUNK_SIZE, total_chunks, PAGE_TX_TOTAL);
#endif
		ret = 0;
		while (remaining > 0) {
			size_t chunk = MIN(remaining, (size_t)MAX32664_CHUNK_SIZE);
			size_t offset = (size_t)(ptr - tx_buf);
#if 1
			LOG_INF("  Chunk %d/%d: offset=%zu len=%zu hdr=[%02X %02X %02X %02X]",
				chunk_num + 1, total_chunks,
				offset, chunk,
				ptr[0],
				chunk > 1 ? ptr[1] : 0,
				chunk > 2 ? ptr[2] : 0,
				chunk > 3 ? ptr[3] : 0);
#endif
			ret = i2c_write_dt(bus, ptr, (uint32_t)chunk);
			if (ret) {
				LOG_ERR("  Chunk %d FAILED: %d (offset=%zu)", chunk_num + 1, ret, offset);
				break;
			}
#if 1
			LOG_INF("  Chunk %d OK", chunk_num + 1);
#endif
			ptr       += chunk;
			remaining -= chunk;
			chunk_num++;
		}
	}
#else
	/* Single-shot: one i2c_write() for the full 8210-byte page.
	 * At correct 100 kHz bus speed this takes ~738 ms.
	 * Prerequisite: I2C1 must have pull-ups on the nRF side of the PCA9306
	 * level shifter (either external 4.7 kΩ or internal via bias-pull-up in
	 * pinctrl), otherwise large transfers are throttled to ~0.78 kHz. */
	LOG_DBG("  Single-shot write: %d B", PAGE_TX_TOTAL);
	ret = i2c_write_dt(bus, tx_buf, PAGE_TX_TOTAL);
#endif

	if (ret) {
		LOG_ERR("  Page write failed: %d", ret);
		max32664_bus_unlock(dev);
		return ret;
	}
	
	/* Wait for bootloader to begin flash programming */
	k_msleep(PAGE_FLASH_DELAY_MS);
	max32664_bus_unlock(dev);

	/* Poll page write status via the standard command helper.
	 * bootloader_i2c_command returns -EAGAIN when the bootloader responds 0xFE
	 * (still programming flash) — retry with 200ms back-off.
	 * Per OEM reference: SS_DEFAULT_RETRIES = 4. */
	{
		uint8_t hub_status;
		int retries = CONFIG_MAX32664_UPDATE_PAGE_WRITE_RETRIES;

		do {
			ret = bootloader_i2c_command(dev, FAMILY_HUB_STATUS, HUB_STATUS_IDX,
						     NULL, 0, &hub_status, 2, 10);
			if (ret == 0) {
				LOG_DBG("  Hub status after page write: 0x%02X", hub_status);
				break;  /* Page programmed OK */
			} else if (ret == -EAGAIN && retries-- > 0) {
				LOG_DBG("  Bootloader busy (0xFE), retrying (%d left)...", retries);
				k_msleep(200);
			} else if (ret == -EAGAIN) {
				LOG_ERR("  Page write timed out after %d retries",
					CONFIG_MAX32664_UPDATE_PAGE_WRITE_RETRIES);
				return -ETIMEDOUT;
			} else {
				return ret;
			}
		} while (true);
	}

	elapsed_ms = k_uptime_get_32() - start_time;
	LOG_DBG("  Page write complete in %u ms", elapsed_ms);

	/* Inter-page delay */
	k_msleep(INTER_PAGE_DELAY_MS);

	return 0;
}

int max32664_read_firmware_version(const struct device *dev, uint8_t *major, uint8_t *minor, uint8_t *patch)
{
	uint8_t response[4];
	int ret;
	
	if (major == NULL || minor == NULL || patch == NULL) {
		return -EINVAL;
	}
	
	LOG_DBG("Reading MAX32664 firmware version...");
	
	/* Read firmware version: Family 0xFF, Index 0x03 */
	ret = bootloader_i2c_command(dev, FAMILY_IDENTITY, IDENTITY_IDX_FW_VERSION, NULL, 0, response, 4, 10);
	if (ret) {
		LOG_ERR("Failed to read firmware version: %d", ret);
		return ret;
	}
	
	/* Response format: [status, major, minor, patch] */
	*major = response[0];
	*minor = response[1];
	*patch = response[2];
	
	LOG_INF("MAX32664 Firmware Version: %u.%u.%u", *major, *minor, *patch);
	
	return 0;
}

int max32664_enter_bootloader(const struct device *dev)
{
	struct max32664_bootloader_io io;
	uint8_t data = OP_MODE_BOOTLOADER;  /* Bootloader mode */
	uint8_t status;
	uint8_t response[8];
	int ret;
	uint32_t t_gpio_begin_ms = 0;
	uint32_t t_rstn_rise_ms = 0;
	uint32_t t_bl_ready_ms = 0;
	uint32_t t_latch_done_ms = 0;
	bool used_gpio = false;

	ret = max32664_get_bootloader_io(dev, &io);
	if (ret) {
		LOG_ERR("Failed to get MAX32664 transport interface: %d", ret);
		return ret;
	}

	LOG_INF("=== Step 1: Enter bootloader mode ===");

	max32664_bus_lock(dev);

	/* GPIO entry (ADI UG p.17) then mandatory software latch 0x01 0x00 0x08
	 * within ~780 ms of BL readiness, or a valid app exits BL automatically. */
	if (io.rstn_gpio.port != NULL && io.int_gpio.port != NULL) {
		used_gpio = true;
		t_gpio_begin_ms = k_uptime_get_32();
		LOG_INF("BL: GPIO entry begin t=%u ms", t_gpio_begin_ms);

		/* RSTN low ≥10 ms; MFIO low ≥1 ms before RSTN rise.
		 * DT: GPIO_ACTIVE_LOW → logical 1 = phys LOW. */
		gpio_pin_set_dt(&io.rstn_gpio, 1);
		k_msleep(10);

		gpio_pin_set_dt(&io.int_gpio, 1);
		k_msleep(1);

		gpio_pin_set_dt(&io.rstn_gpio, 0);
		t_rstn_rise_ms = k_uptime_get_32();
		LOG_INF("BL: RSTN released t=%u ms", t_rstn_rise_ms);

		/* UG: +50 ms after RSTN rise → device is in bootloader mode. */
		k_msleep(50);
		t_bl_ready_ms = k_uptime_get_32();
		LOG_INF("BL: ready (RSTN+50ms) t=%u ms", t_bl_ready_ms);

		/* Latch NOW — do not burn the 780 ms window on MFIO release /
		 * version queries. Table 9: 0x01 0x00 0x08 immediately. */
		ret = bootloader_i2c_command(dev, FAMILY_CONFIG, CONFIG_IDX_OP_MODE,
					     &data, 1, &status, 1, 10);
		t_latch_done_ms = k_uptime_get_32();
		{
			uint32_t from_ready = t_latch_done_ms - t_bl_ready_ms;
			uint32_t from_rstn = t_latch_done_ms - t_rstn_rise_ms;

			LOG_INF("BL: latch 0x01 0x00 0x08 done t=%u ms "
				"(+%u ms from BL-ready, +%u ms from RSTN rise) "
				"ret=%d status=0x%02x",
				t_latch_done_ms, from_ready, from_rstn, ret,
				(ret == 0) ? status : 0xFF);
			if (from_ready > 780U) {
				LOG_ERR("BL: LATCH OUTSIDE ~780 ms WINDOW "
					"(from BL-ready=%u ms) — app may have jumped",
					from_ready);
			} else {
				LOG_INF("BL: latch within 780 ms window (OK)");
			}
		}

		/* Release MFIO after latch (host may later use it as IRQ). */
		gpio_pin_set_dt(&io.int_gpio, 0);

		if (ret) {
			LOG_ERR("BL: software latch failed: %d", ret);
			goto out_unlock;
		}
	} else {
		LOG_WRN("No GPIO control - I2C-only bootloader entry (0x01 0x00 0x08)");
		t_bl_ready_ms = k_uptime_get_32();
		ret = bootloader_i2c_command(dev, FAMILY_CONFIG, CONFIG_IDX_OP_MODE,
					     &data, 1, &status, 1, 50);
		t_latch_done_ms = k_uptime_get_32();
		LOG_INF("BL: I2C latch t=%u ms elapsed=%u ms ret=%d",
			t_latch_done_ms, t_latch_done_ms - t_bl_ready_ms, ret);
		if (ret) {
			LOG_ERR("Failed to enter bootloader: %d", ret);
			goto out_unlock;
		}
		k_msleep(350);
	}

	/* Step 2: Verify bootloader mode */
	LOG_INF("=== Step 2: Read/Verify mode ===");

	ret = bootloader_i2c_command_with_retry(dev,
						 FAMILY_OUTPUT_MODE, OUTPUT_MODE_IDX,
						 NULL, 0, &status, 2, 10,
						 BOOTLOADER_BUSY_RETRIES,
						 BOOTLOADER_BUSY_RETRY_DELAY_MS,
						 "Read/Verify mode");
	if (ret) {
		LOG_ERR("FAILED to read mode: %d (gpio_entry=%d)", ret, used_gpio);
		goto out_unlock;
	}
	if (status != 0x08) {
		LOG_ERR("FAILED: Expected mode 0x08, got 0x%02x (gpio_entry=%d)",
			status, used_gpio);
		ret = -EIO;
		goto out_unlock;
	}
	LOG_INF("SUCCESS: Device is in bootloader mode (0x%02X) t=%u ms",
		status, k_uptime_get_32());
	
	/* Step 3: Get ID and MCU type */
	LOG_INF("=== Step 3: Get ID and MCU type ===");
	ret = bootloader_i2c_command_with_retry(dev,
						 FAMILY_IDENTITY, IDENTITY_IDX_MCU_TYPE,
						 NULL, 0, response, 2, 10,
						 BOOTLOADER_BUSY_RETRIES,
						 BOOTLOADER_BUSY_RETRY_DELAY_MS,
						 "Get MCU ID");
	if (ret) {
		LOG_WRN("Get ID failed: %d (may not be critical)", ret);
	} else {
		LOG_INF("SUCCESS: MCU ID = 0x%02X", response[0]);
	}
	
	/* Step 4: Read bootloader firmware version */
	LOG_INF("=== Step 4: Read bootloader version ===");
	ret = bootloader_i2c_command_with_retry(dev,
						 FAMILY_BOOTLOADER_INFO, BL_INFO_IDX_VERSION,
						 NULL, 0, response, 4, 10,
						 BOOTLOADER_BUSY_RETRIES,
						 BOOTLOADER_BUSY_RETRY_DELAY_MS,
						 "Get bootloader version");
	if (ret) {
		LOG_WRN("Get bootloader version failed: %d (may not be critical)", ret);
	} else {
		LOG_INF("SUCCESS: Bootloader Version = %u.%u.%u", response[0], response[1], response[2]);
	}
	
	/* Step 5: Read bootloader page size */
	LOG_INF("=== Step 5: Read bootloader page size ===");
	ret = bootloader_i2c_command_with_retry(dev,
						 FAMILY_BOOTLOADER_INFO, BL_INFO_IDX_PAGE_SIZE,
						 NULL, 0, response, 3, 10,
						 BOOTLOADER_BUSY_RETRIES,
						 BOOTLOADER_BUSY_RETRY_DELAY_MS,
						 "Get bootloader page size");
	if (ret) {
		LOG_WRN("Get bootloader info failed: %d (may not be critical)", ret);
	} else {
		uint16_t bl_page_size = ((uint16_t)response[0] << 8) | response[1];
		LOG_INF("SUCCESS: Bootloader page size = %u bytes (0x%02X 0x%02X)",
			bl_page_size, response[0], response[1]);
		if (bl_page_size != MSBL_PAGE_DATA_SIZE) {
			LOG_WRN("Page size mismatch: bootloader reports %u, expected %u",
				bl_page_size, MSBL_PAGE_DATA_SIZE);
		}
	}

	ret = 0;

out_unlock:
	max32664_bus_unlock(dev);
	return ret;
}

int max32664_exit_bootloader(const struct device *dev)
{
	uint8_t data = OP_MODE_APPLICATION;  /* Application mode */
	uint8_t status = 0xFF;
	uint8_t mode = 0xFF;
	int ret;

	LOG_INF("=== Exit BL: enter application mode t=%u ms ===", k_uptime_get_32());

	/* Family 0x01, Index 0x00, Data 0x00 */
	ret = bootloader_i2c_command(dev, FAMILY_CONFIG, CONFIG_IDX_OP_MODE, &data, 1, &status, 1, 50);
	if (ret) {
		LOG_WRN("Exit bootloader command failed: %d (may be normal)", ret);
	} else {
		LOG_INF("Exit BL command status=0x%02x", status);
	}

	/* Wait for reboot (AN6924 Variant C ~1.8 s) */
	k_msleep(2000);

	ret = bootloader_i2c_command(dev, FAMILY_OUTPUT_MODE, OUTPUT_MODE_IDX,
				     NULL, 0, &mode, 2, 10);
	if (ret) {
		LOG_WRN("Post-exit mode read failed: %d (app may still be starting)", ret);
	} else {
		LOG_INF("Post-exit mode=0x%02x (expect 0x00 application) t=%u ms",
			mode, k_uptime_get_32());
	}

	LOG_INF("MAX32664 should now be in application mode");
	return 0;
}

int max32664_msbl_sanity_check(const uint8_t *msbl, size_t msbl_len, uint8_t *num_pages_out)
{
	uint8_t pages;
	size_t need;

	if (msbl == NULL || msbl_len < MSBL_HEADER_SIZE) {
		return -EINVAL;
	}

	pages = msbl[MSBL_OFFSET_NUM_PAGES];
	if (pages == 0 || pages > MAX32664_MAX_PAGES) {
		return -EINVAL;
	}

	need = MSBL_HEADER_SIZE + ((size_t)pages * MSBL_PAGE_SIZE);
	if (msbl_len < need) {
		LOG_ERR("MSBL truncated: have %zu need %zu (%u pages)", msbl_len, need, pages);
		return -EINVAL;
	}

	if (num_pages_out) {
		*num_pages_out = pages;
	}
	return 0;
}

static void parse_version_from_name(const char *name_hint,
				    struct max32664_firmware_info *fw_info)
{
	fw_info->ver_major = 0;
	fw_info->ver_minor = 0;
	fw_info->ver_patch = 0;
	if (!name_hint) {
		return;
	}

	const char *base = strrchr(name_hint, '/');
	const char *ver = strrchr(base ? base : name_hint, '_');

	if (!ver) {
		return;
	}

	char *end;
	unsigned long v;

	ver++;
	v = strtoul(ver, &end, 10);
	if (end != ver && *end == '.') {
		fw_info->ver_major = (uint8_t)v;
		ver = end + 1;
		v = strtoul(ver, &end, 10);
		if (end != ver && *end == '.') {
			fw_info->ver_minor = (uint8_t)v;
			ver = end + 1;
			v = strtoul(ver, &end, 10);
			if (end != ver) {
				fw_info->ver_patch = (uint8_t)v;
			}
		}
	}
}

static int parse_msbl_header_buf(const uint8_t *msbl, size_t msbl_len,
				 const char *name_hint,
				 struct max32664_firmware_info *fw_info)
{
	int ret = max32664_msbl_sanity_check(msbl, msbl_len, &fw_info->num_pages);

	if (ret) {
		return ret;
	}

	memcpy(fw_info->iv_bytes, &msbl[MSBL_OFFSET_IV], 11);
	memcpy(fw_info->auth_bytes, &msbl[MSBL_OFFSET_AUTH], 16);
	parse_version_from_name(name_hint, fw_info);

	LOG_INF("=== MSBL Firmware Info ===");
	LOG_INF("Number of pages: %u", fw_info->num_pages);
	LOG_INF("Image size: %zu bytes", msbl_len);
	if (fw_info->ver_major || fw_info->ver_minor || fw_info->ver_patch) {
		LOG_INF("New firmware version: v%u.%u.%u (from name)",
			fw_info->ver_major, fw_info->ver_minor, fw_info->ver_patch);
	}
	LOG_HEXDUMP_INF(fw_info->iv_bytes, 11, "Initialization Vector:");
	LOG_HEXDUMP_INF(fw_info->auth_bytes, 16, "Authentication Bytes:");
	LOG_INF("=========================");
	return 0;
}

int max32664_update_firmware_buf(const struct device *dev,
				 const uint8_t *msbl, size_t msbl_len,
				 const char *name_hint,
				 max32664_update_progress_cb progress_cb,
				 void *user_data)
{
	struct max32664_firmware_info fw_info;
	struct update_guardrails_state guardrails = {0};
	uint8_t cmd_buf[32];
	uint8_t response[4];
	uint8_t old_major = 0, old_minor = 0, old_patch = 0;
	uint8_t new_major = 0, new_minor = 0, new_patch = 0;
	uint32_t page_num = 0;
	uint32_t total_pages;
	uint8_t *tx_buf = NULL;
	int ret;

	LOG_INF("Starting MAX32664 firmware update from buffer (%zu bytes)", msbl_len);

	ret = parse_msbl_header_buf(msbl, msbl_len, name_hint, &fw_info);
	if (ret) {
		return ret;
	}

	total_pages = fw_info.num_pages;

	/* Provenance before any BL entry / erase (Maxim auth still applies later). */
	{
		size_t policy_len = MSBL_HEADER_SIZE +
				    ((size_t)fw_info.num_pages * MSBL_PAGE_SIZE);

		if (policy_len > msbl_len) {
			policy_len = msbl_len;
		}
		ret = max32664_msbl_policy_check(msbl, policy_len, name_hint,
						 fw_info.ver_major, fw_info.ver_minor,
						 fw_info.ver_patch);
		if (ret) {
			LOG_ERR("MSBL rejected by host provenance policy: %d", ret);
			return ret;
		}
	}

	if (max32664_read_firmware_version(dev, &old_major, &old_minor, &old_patch) != 0) {
		LOG_WRN("Could not read old firmware version (will show 0.0.0)");
	}

	activate_update_guardrails(&guardrails, dev);

	/* Session lock across enter → program → exit so feeder/sampling
	 * cannot slip between page commands. Nested locks in helpers are OK. */
	max32664_bus_lock(dev);

	ret = max32664_enter_bootloader(dev);
	if (ret) {
		goto exit_with_guardrails;
	}

	LOG_INF("=== Step 6: Set number of pages ===");
	cmd_buf[0] = 0x00;
	cmd_buf[1] = fw_info.num_pages;
	ret = bootloader_i2c_command_with_retry(dev,
						 FAMILY_BOOTLOADER, BOOTLOADER_SET_NUM_PAGES,
						 cmd_buf, 2, response, 1, 50,
						 BOOTLOADER_BUSY_RETRIES,
						 BOOTLOADER_BUSY_RETRY_DELAY_MS,
						 "Set number of pages");
	LOG_INF("BL: set_num_pages ret=%d t=%u", ret, k_uptime_get_32());
	if (ret) {
		goto exit_with_guardrails;
	}

	LOG_INF("=== Step 7: Set Initialization Vector ===");
	memcpy(cmd_buf, fw_info.iv_bytes, 11);
	ret = bootloader_i2c_command_with_retry(dev,
						 FAMILY_BOOTLOADER, BOOTLOADER_SET_IV,
						 cmd_buf, 11, response, 1, 500,
						 BOOTLOADER_BUSY_RETRIES,
						 BOOTLOADER_BUSY_RETRY_DELAY_MS,
						 "Set Initialization Vector");
	LOG_INF("BL: set_IV ret=%d t=%u", ret, k_uptime_get_32());
	if (ret) {
		goto exit_with_guardrails;
	}

	LOG_INF("=== Step 8: Set Authentication bytes ===");
	memcpy(cmd_buf, fw_info.auth_bytes, 16);
	ret = bootloader_i2c_command_with_retry(dev,
						 FAMILY_BOOTLOADER, BOOTLOADER_SET_AUTH,
						 cmd_buf, 16, response, 1, 500,
						 BOOTLOADER_BUSY_RETRIES,
						 BOOTLOADER_BUSY_RETRY_DELAY_MS,
						 "Set Authentication bytes");
	LOG_INF("BL: set_AUTH ret=%d t=%u", ret, k_uptime_get_32());
	if (ret) {
		goto exit_with_guardrails;
	}

	LOG_INF("=== Step 9: Erase application flash (CMD_DELAY=%d ms) ===",
		CONFIG_MAX32664_UPDATE_ERASE_CMD_DELAY_MS);
	ret = bootloader_i2c_command_with_retry(dev,
						 FAMILY_BOOTLOADER, BOOTLOADER_ERASE_APP,
						 NULL, 0, response, 1,
						 CONFIG_MAX32664_UPDATE_ERASE_CMD_DELAY_MS,
						 BOOTLOADER_BUSY_RETRIES,
						 BOOTLOADER_BUSY_RETRY_DELAY_MS,
						 "Erase application flash");
	LOG_INF("BL: erase ret=%d t=%u", ret, k_uptime_get_32());
	if (ret) {
		goto exit_with_guardrails;
	}

	if (CONFIG_MAX32664_UPDATE_ERASE_POST_SETTLE_MS > 0) {
		k_msleep(CONFIG_MAX32664_UPDATE_ERASE_POST_SETTLE_MS);
	}

	tx_buf = k_malloc(PAGE_TX_TOTAL);
	if (!tx_buf) {
		ret = -ENOMEM;
		goto exit_with_guardrails;
	}

	while (page_num < total_pages) {
		size_t off = MSBL_HEADER_SIZE + ((size_t)page_num * MSBL_PAGE_SIZE);

		memcpy(&tx_buf[2], &msbl[off], MAX32664_PAGE_SIZE);
		memcpy(&tx_buf[2 + MAX32664_PAGE_SIZE],
		       &msbl[off + MAX32664_PAGE_SIZE], MSBL_PAGE_CHECKSUM_SIZE);

		LOG_INF("--- Page %u/%u t=%u ---", page_num + 1, total_pages,
			k_uptime_get_32());
		ret = write_page_to_bootloader(dev, tx_buf);
		if (ret) {
			LOG_ERR("BL: page %u write failed: %d", page_num + 1, ret);
			goto exit_with_guardrails;
		}
		if (page_num == 0U) {
			LOG_INF("BL: first page write OK t=%u", k_uptime_get_32());
		}

		page_num++;
		if (progress_cb) {
			progress_cb(page_num, total_pages, user_data);
		}
	}

#if IS_ENABLED(CONFIG_MAX32664_UPDATE_SEND_FLASH_APP)
	LOG_INF("=== Step 11: Finalize flashed application (0x80 0x06) ===");
	ret = bootloader_i2c_command_with_retry(dev,
						 FAMILY_BOOTLOADER, BOOTLOADER_FLASH_APP,
						 NULL, 0, response, 1, 1400,
						 BOOTLOADER_BUSY_RETRIES,
						 BOOTLOADER_BUSY_RETRY_DELAY_MS,
						 "Finalize flashed application");
	LOG_INF("BL: finalize ret=%d t=%u", ret, k_uptime_get_32());
	if (ret) {
		goto exit_with_guardrails;
	}
#else
	LOG_INF("=== Step 11: Finalize skipped (CONFIG_MAX32664_UPDATE_SEND_FLASH_APP=n; UG Table 9) ===");
#endif

	LOG_INF("Firmware update complete (%u pages written) t=%u",
		page_num, k_uptime_get_32());
	ret = 0;

exit_with_guardrails:
	if (tx_buf) {
		k_free(tx_buf);
	}

	{
		int exit_ret = max32664_exit_bootloader(dev);

		if (ret == 0 && exit_ret != 0) {
			ret = exit_ret;
		}
	}

	max32664_bus_unlock(dev);
	deactivate_update_guardrails(&guardrails);

	if (ret == 0) {
		k_msleep(1000);
		if (max32664_read_firmware_version(dev, &new_major, &new_minor,
						   &new_patch) == 0) {
			LOG_INF("*** Firmware Update Summary ***");
			LOG_INF("Old Version: v%u.%u.%u", old_major, old_minor, old_patch);
			LOG_INF("New Version: v%u.%u.%u", new_major, new_minor, new_patch);
		}
	}

	return ret;
}

int max32664_update_firmware_from_app_partition(const struct device *dev,
						max32664_update_progress_cb progress_cb,
						void *user_data)
{
	const uint8_t *msbl = (const uint8_t *)APP_DATA_RESOURCE_MMAP_ADDR;
	uint8_t pages = 0;
	size_t len;
	int ret;

	ret = max32664_msbl_sanity_check(msbl, APP_DATA_PARTITION_SIZE, &pages);
	if (ret) {
		LOG_ERR("No valid .msbl in QSPI app partition: %d", ret);
		return ret;
	}

	len = MSBL_HEADER_SIZE + ((size_t)pages * MSBL_PAGE_SIZE);
	LOG_INF("Flashing hub from QSPI app partition (%u pages, %zu bytes)", pages, len);
	return max32664_update_firmware_buf(dev, msbl, len, "app_partition.msbl",
					    progress_cb, user_data);
}

int max32664_update_firmware(const struct device *dev, const char *firmware_path,
			     max32664_update_progress_cb progress_cb, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(firmware_path);
	ARG_UNUSED(progress_cb);
	ARG_UNUSED(user_data);
	LOG_ERR("Path-based MSBL update removed (/NAND: FatFS gone). "
		"Use max32664_update_firmware_buf() or _from_app_partition()");
	return -ENOTSUP;
}

