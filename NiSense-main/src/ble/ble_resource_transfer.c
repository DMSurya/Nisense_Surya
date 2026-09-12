/**
 * @file ble_resource_transfer.c
 * @brief BLE GATT A/B update for Resources (fonts/icons/logos).
 */

#include "ble_resource_transfer.h"
#include "resource.h"
#include "nor_qspi_busy.h"
#include "ota_compat.h"
#include "ota_progress_ui.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/byteorder.h>
#include <string.h>
#include <errno.h>

#if defined(CONFIG_RESOURCE) && defined(CONFIG_BT)

LOG_MODULE_REGISTER(ble_resource_xfer, CONFIG_LOG_DEFAULT_LEVEL);

#define XIP_SVC_UUID_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef5)
#define XIP_CTRL_UUID_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf501)
#define XIP_DATA_UUID_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf502)
#define XIP_STATUS_UUID_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf503)

static struct bt_uuid_128 xip_svc_uuid = BT_UUID_INIT_128(XIP_SVC_UUID_VAL);
static struct bt_uuid_128 xip_ctrl_uuid = BT_UUID_INIT_128(XIP_CTRL_UUID_VAL);
static struct bt_uuid_128 xip_data_uuid = BT_UUID_INIT_128(XIP_DATA_UUID_VAL);
static struct bt_uuid_128 xip_status_uuid = BT_UUID_INIT_128(XIP_STATUS_UUID_VAL);

#define STAGE_SIZE 256U

static const struct flash_area *g_fa;
static struct ble_resource_status g_status;
static uint8_t g_stage[STAGE_SIZE];
static size_t g_stage_len;
static uint32_t g_write_off;
static bool g_status_notify_enabled;
static uint8_t g_stage_slot = RESOURCE_SLOT_SECONDARY;
static uint8_t g_last_ui_pct = 0xFF;

static uint16_t flash_area_id_for_slot(uint8_t slot)
{
	return (slot == RESOURCE_SLOT_SECONDARY)
		       ? FIXED_PARTITION_ID(resource_secondary_partition)
		       : FIXED_PARTITION_ID(resource_primary_partition);
}

static void refresh_active_status(void)
{
	const struct resource_partition_header *hdr = resource_get_header();

	g_status.active_slot = resource_get_active_slot();
	g_status.active_version = hdr ? hdr->version : 0U;
}

static void notify_status(void)
{
	refresh_active_status();
	if (g_status.total > 0U &&
	    (g_status.state == BLE_RESOURCE_STATE_RECEIVING ||
	     g_status.state == BLE_RESOURCE_STATE_ERASING ||
	     g_status.state == BLE_RESOURCE_STATE_READY)) {
		uint8_t pct = (uint8_t)((g_status.received * 100U) / g_status.total);

		if (pct != g_last_ui_pct &&
		    (pct == 0U || pct >= g_last_ui_pct + 2U || pct == 100U)) {
			g_last_ui_pct = pct;
			ota_progress_ui_set(pct, "Resources");
		}
	}
	if (!g_status_notify_enabled) {
		return;
	}
	extern const struct bt_gatt_service_static resource_transfer_svc;
	(void)bt_gatt_notify(NULL, &resource_transfer_svc.attrs[6], &g_status, sizeof(g_status));
}

static void set_error(int err)
{
	g_status.state = BLE_RESOURCE_STATE_ERROR;
	g_status.error = (int8_t)((err < -128) ? -128 : err);
	if (g_fa != NULL) {
		flash_area_close(g_fa);
		g_fa = NULL;
	}
	g_stage_len = 0;
	ota_progress_ui_end(false, "Assets update failed");
	notify_status();
}

void ble_resource_transfer_reset(void)
{
	if (g_fa != NULL) {
		flash_area_close(g_fa);
		g_fa = NULL;
	}
	g_stage_len = 0;
	g_write_off = 0;
	g_last_ui_pct = 0xFF;
	g_status.state = BLE_RESOURCE_STATE_IDLE;
	g_status.error = 0;
	g_status.received = 0;
	g_status.total = 0;
	refresh_active_status();
}

static int flush_block(const uint8_t *buf, size_t len)
{
	int rc;

	nor_qspi_busy_acquire();
	rc = flash_area_write(g_fa, g_write_off, buf, len);
	nor_qspi_busy_release();
	if (rc == 0) {
		g_write_off += (uint32_t)len;
	}
	return rc;
}

static int append_data(const uint8_t *data, size_t n)
{
	while (n > 0U) {
		size_t take = MIN(n, STAGE_SIZE - g_stage_len);

		memcpy(&g_stage[g_stage_len], data, take);
		g_stage_len += take;
		data += take;
		n -= take;
		g_status.received += (uint32_t)take;

		if (g_stage_len == STAGE_SIZE) {
			int rc = flush_block(g_stage, STAGE_SIZE);

			if (rc != 0) {
				return rc;
			}
			g_stage_len = 0;
		}
	}
	return 0;
}

static int begin_transfer(uint32_t total)
{
	int rc;

	if (total < sizeof(struct resource_partition_header) ||
	    total > RESOURCE_PARTITION_SIZE) {
		LOG_ERR("xip BEGIN: bad total_size %u", total);
		return -EINVAL;
	}

	g_stage_slot = resource_inactive_slot();

	if (g_fa != NULL) {
		flash_area_close(g_fa);
		g_fa = NULL;
	}
	rc = flash_area_open(flash_area_id_for_slot(g_stage_slot), &g_fa);
	if (rc != 0) {
		LOG_ERR("xip BEGIN: flash_area_open slot %u failed %d", g_stage_slot, rc);
		return rc;
	}

	g_status.state = BLE_RESOURCE_STATE_ERASING;
	g_status.error = 0;
	g_status.received = 0;
	g_status.total = total;
	g_stage_len = 0;
	g_write_off = 0;
	g_last_ui_pct = 0xFF;
	ota_progress_ui_begin("Resources");
	notify_status();

	nor_qspi_busy_acquire();
	rc = flash_area_erase(g_fa, 0, RESOURCE_PARTITION_SIZE);
	nor_qspi_busy_release();
	if (rc != 0) {
		LOG_ERR("xip BEGIN: erase failed %d", rc);
		return rc;
	}

	g_status.state = BLE_RESOURCE_STATE_RECEIVING;
	LOG_INF("xip BEGIN: total=%u stage_slot=%u", total, g_stage_slot);
	notify_status();
	return 0;
}

static int commit_transfer(void)
{
	int rc;
	const struct resource_partition_header *hdr;

	if (g_fa == NULL) {
		return -EPERM;
	}

	if (g_stage_len > 0U) {
		size_t wlen = g_stage_len;

		while ((wlen % 4U) != 0U) {
			g_stage[wlen++] = 0xFF;
		}
		rc = flush_block(g_stage, wlen);
		if (rc != 0) {
			return rc;
		}
		g_stage_len = 0;
	}

	flash_area_close(g_fa);
	g_fa = NULL;

	if (g_status.total != 0U && g_status.received != g_status.total) {
		LOG_ERR("xip COMMIT: size mismatch received=%u total=%u",
			g_status.received, g_status.total);
		return -EIO;
	}

	rc = resource_validate_slot(g_stage_slot);
	if (rc != 0) {
		LOG_ERR("xip COMMIT: stage slot %u invalid %d", g_stage_slot, rc);
		return rc;
	}

	hdr = (const struct resource_partition_header *)resource_slot_base(g_stage_slot);
	rc = ota_compat_check_resource(hdr);
	if (rc != 0) {
		LOG_ERR("resource COMMIT: ota_compat rejected %d", rc);
		return rc;
	}

	rc = resource_set_active_slot(g_stage_slot);
	if (rc != 0) {
		LOG_ERR("xip COMMIT: set active failed %d", rc);
		return rc;
	}

	g_status.state = BLE_RESOURCE_STATE_COMMITTED;
	g_status.error = 0;
	refresh_active_status();
	LOG_INF("xip COMMIT: activated slot %u", g_stage_slot);
	ota_progress_ui_end(true, "Assets updated");
	notify_status();
	return 0;
}

static ssize_t read_status(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			   void *buf, uint16_t len, uint16_t offset)
{
	ARG_UNUSED(conn);
	refresh_active_status();
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &g_status, sizeof(g_status));
}

static void status_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	g_status_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
}

static ssize_t write_ctrl(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(offset);
	ARG_UNUSED(flags);

	const uint8_t *p = buf;
	int rc;

	if (len < 1U) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	switch (p[0]) {
	case BLE_RESOURCE_CMD_BEGIN:
		/* [cmd][total_size:4] */
		if (len < 5U) {
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		}
		rc = begin_transfer(sys_get_le32(&p[1]));
		if (rc != 0) {
			set_error(rc);
			return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
		}
		break;

	case BLE_RESOURCE_CMD_COMMIT:
		rc = commit_transfer();
		if (rc != 0) {
			set_error(rc);
			return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
		}
		break;

	case BLE_RESOURCE_CMD_ABORT:
		LOG_INF("xip ABORT by peer");
		ble_resource_transfer_reset();
		ota_progress_ui_end(false, "Assets aborted");
		notify_status();
		break;

	default:
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	return len;
}

static ssize_t write_data(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(offset);
	ARG_UNUSED(flags);

	if (g_status.state != BLE_RESOURCE_STATE_RECEIVING || g_fa == NULL) {
		return BT_GATT_ERR(BT_ATT_ERR_WRITE_REQ_REJECTED);
	}
	if (len == 0U) {
		return 0;
	}
	if ((g_status.received + len) > g_status.total) {
		set_error(-EFBIG);
		return BT_GATT_ERR(BT_ATT_ERR_WRITE_REQ_REJECTED);
	}

	if (append_data(buf, len) != 0) {
		set_error(-EIO);
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	if (g_status.received == g_status.total) {
		g_status.state = BLE_RESOURCE_STATE_READY;
		notify_status();
	} else if ((g_status.received & 0x3FFU) == 0U) {
		notify_status();
	}
	return len;
}

BT_GATT_SERVICE_DEFINE(resource_transfer_svc,
	BT_GATT_PRIMARY_SERVICE(&xip_svc_uuid),

	BT_GATT_CHARACTERISTIC(&xip_ctrl_uuid.uuid,
		BT_GATT_CHRC_WRITE,
		BT_GATT_PERM_WRITE_ENCRYPT,
		NULL, write_ctrl, NULL),

	BT_GATT_CHARACTERISTIC(&xip_data_uuid.uuid,
		BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		BT_GATT_PERM_WRITE_ENCRYPT,
		NULL, write_data, NULL),

	BT_GATT_CHARACTERISTIC(&xip_status_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ_ENCRYPT,
		read_status, NULL, NULL),
	BT_GATT_CCC(status_ccc_changed,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE_ENCRYPT),
);

#endif /* CONFIG_RESOURCE && CONFIG_BT */
