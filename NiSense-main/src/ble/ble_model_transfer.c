/**
 * @file ble_model_transfer.c
 * @brief BLE GATT service for A/B glucose-model update (see ble_model_transfer.h).
 *
 * Streams a packed glucose model image into the *inactive* glucose-model slot,
 * validates it (CRC always; Ed25519 when CONFIG_GLUCOSE_MODEL_SIGNED; ota_compat),
 * then flips the active slot and re-initialises the algorithm.
 */

#include "ble_model_transfer.h"
#include "glucose_model_xip.h"
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

#if defined(CONFIG_GLUCOSE_MATRICES_FROM_XIP)

extern int glucose_model_revalidate(void);

#if defined(CONFIG_GLUCOSE_MODEL_SIGNED)
#include <psa/crypto.h>
#include "glucose_model_pubkey.h"
#endif

LOG_MODULE_REGISTER(ble_model_xfer, CONFIG_LOG_DEFAULT_LEVEL);

#define MODEL_SVC_UUID_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef3)
#define MODEL_CTRL_UUID_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf301)
#define MODEL_DATA_UUID_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf302)
#define MODEL_STATUS_UUID_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf303)

static struct bt_uuid_128 model_svc_uuid = BT_UUID_INIT_128(MODEL_SVC_UUID_VAL);
static struct bt_uuid_128 model_ctrl_uuid = BT_UUID_INIT_128(MODEL_CTRL_UUID_VAL);
static struct bt_uuid_128 model_data_uuid = BT_UUID_INIT_128(MODEL_DATA_UUID_VAL);
static struct bt_uuid_128 model_status_uuid = BT_UUID_INIT_128(MODEL_STATUS_UUID_VAL);

#define STAGE_SIZE 256U

static const struct flash_area *g_fa;
static struct ble_model_status g_status;
static uint8_t g_stage[STAGE_SIZE];
static size_t g_stage_len;
static uint32_t g_write_off;
static bool g_status_notify_enabled;
static uint8_t g_stage_slot = GLUCOSE_MODEL_SLOT_SECONDARY;
static uint8_t g_last_ui_pct = 0xFF;

static uint16_t flash_area_id_for_slot(uint8_t slot)
{
	return (slot == GLUCOSE_MODEL_SLOT_SECONDARY)
		       ? FIXED_PARTITION_ID(model_secondary_partition)
		       : FIXED_PARTITION_ID(model_primary_partition);
}

static void refresh_active_status(void)
{
	g_status.active_version = glucose_model_xip_active_version();
	g_status.active_slot = glucose_model_xip_get_active_slot();
}

static void notify_status(void)
{
	refresh_active_status();
	if (g_status.total > 0U &&
	    (g_status.state == BLE_MODEL_STATE_RECEIVING ||
	     g_status.state == BLE_MODEL_STATE_ERASING ||
	     g_status.state == BLE_MODEL_STATE_READY)) {
		uint8_t pct = (uint8_t)((g_status.received * 100U) / g_status.total);

		if (pct != g_last_ui_pct && (pct == 0U || pct >= g_last_ui_pct + 2U ||
					     pct == 100U)) {
			g_last_ui_pct = pct;
			ota_progress_ui_set(pct, "Glucose model");
			if (g_status_notify_enabled) {
				extern const struct bt_gatt_service_static model_transfer_svc;
				(void)bt_gatt_notify(NULL, &model_transfer_svc.attrs[6],
						     &g_status, sizeof(g_status));
			}
			return;
		}
	}
	if (!g_status_notify_enabled) {
		return;
	}
	extern const struct bt_gatt_service_static model_transfer_svc;
	(void)bt_gatt_notify(NULL, &model_transfer_svc.attrs[6], &g_status, sizeof(g_status));
}

static void set_error(int err)
{
	g_status.state = BLE_MODEL_STATE_ERROR;
	g_status.error = (int8_t)((err < -128) ? -128 : err);
	if (g_fa != NULL) {
		flash_area_close(g_fa);
		g_fa = NULL;
	}
	g_stage_len = 0;
	ota_progress_ui_end(false, "Model update failed");
	notify_status();
}

void ble_model_transfer_reset(void)
{
	if (g_fa != NULL) {
		flash_area_close(g_fa);
		g_fa = NULL;
	}
	g_stage_len = 0;
	g_write_off = 0;
	g_last_ui_pct = 0xFF;
	g_status.state = BLE_MODEL_STATE_IDLE;
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

static int begin_transfer(uint32_t total, uint32_t version, uint8_t variant)
{
	int rc;

	if (total < sizeof(struct glucose_model_header) ||
	    total > GLUCOSE_MODEL_PARTITION_SIZE) {
		LOG_ERR("model BEGIN: bad total_size %u", total);
		return -EINVAL;
	}

	g_stage_slot = glucose_model_xip_inactive_slot();

	if (g_fa != NULL) {
		flash_area_close(g_fa);
		g_fa = NULL;
	}
	rc = flash_area_open(flash_area_id_for_slot(g_stage_slot), &g_fa);
	if (rc != 0) {
		LOG_ERR("model BEGIN: flash_area_open slot %u failed %d", g_stage_slot, rc);
		return rc;
	}

	g_status.state = BLE_MODEL_STATE_ERASING;
	g_status.error = 0;
	g_status.received = 0;
	g_status.total = total;
	g_stage_len = 0;
	g_write_off = 0;
	g_last_ui_pct = 0xFF;
	ota_progress_ui_begin("Glucose model");
	notify_status();

	nor_qspi_busy_acquire();
	rc = flash_area_erase(g_fa, 0, GLUCOSE_MODEL_PARTITION_SIZE);
	nor_qspi_busy_release();
	if (rc != 0) {
		LOG_ERR("model BEGIN: erase failed %d", rc);
		return rc;
	}

	g_status.state = BLE_MODEL_STATE_RECEIVING;
	LOG_INF("model BEGIN: total=%u version=%u variant=%u stage_slot=%u",
		total, version, variant, g_stage_slot);
	notify_status();
	return 0;
}

#if defined(CONFIG_GLUCOSE_MODEL_SIGNED)
static int verify_signature(const uint8_t *sig, size_t sig_len)
{
	const uint8_t *img = (const uint8_t *)glucose_model_xip_slot_base(g_stage_slot);
	const struct glucose_model_header *hdr = (const struct glucose_model_header *)img;
	psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t key_id = 0;
	psa_status_t st;
	int ret = -EPERM;

	if (sig_len != 64U) {
		return -EINVAL;
	}
	if (psa_crypto_init() != PSA_SUCCESS) {
		return -EIO;
	}

	psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_VERIFY_MESSAGE);
	psa_set_key_algorithm(&attr, PSA_ALG_PURE_EDDSA);
	psa_set_key_type(&attr, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_TWISTED_EDWARDS));
	psa_set_key_bits(&attr, 255);

	st = psa_import_key(&attr, GLUCOSE_MODEL_PUBKEY, sizeof(GLUCOSE_MODEL_PUBKEY), &key_id);
	if (st != PSA_SUCCESS) {
		LOG_ERR("model sig: import key failed %d", (int)st);
		return -EIO;
	}

	st = psa_verify_message(key_id, PSA_ALG_PURE_EDDSA, img, hdr->total_size, sig, sig_len);
	ret = (st == PSA_SUCCESS) ? 0 : -EPERM;
	if (ret != 0) {
		LOG_ERR("model sig: verify failed %d", (int)st);
	}
	psa_destroy_key(key_id);
	return ret;
}
#else
static int verify_signature(const uint8_t *sig, size_t sig_len)
{
	ARG_UNUSED(sig);
	ARG_UNUSED(sig_len);
	return 0;
}
#endif

static int commit_transfer(const uint8_t *sig, size_t sig_len)
{
	int rc;
	const struct glucose_model_header *hdr;

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
		LOG_ERR("model COMMIT: size mismatch received=%u total=%u",
			g_status.received, g_status.total);
		return -EIO;
	}

	rc = glucose_model_xip_validate_slot(g_stage_slot);
	if (rc != 0) {
		LOG_ERR("model COMMIT: stage slot %u invalid %d", g_stage_slot, rc);
		return rc;
	}

	hdr = glucose_model_xip_peek_header(g_stage_slot);
	rc = ota_compat_check_model(hdr);
	if (rc != 0) {
		LOG_ERR("model COMMIT: ota_compat rejected %d", rc);
		return rc;
	}

	rc = verify_signature(sig, sig_len);
	if (rc != 0) {
		LOG_ERR("model COMMIT: signature rejected %d", rc);
		return rc;
	}

	rc = glucose_model_xip_set_active_slot(g_stage_slot);
	if (rc != 0) {
		LOG_ERR("model COMMIT: set active slot failed %d", rc);
		return rc;
	}

	(void)glucose_model_revalidate();

	g_status.state = BLE_MODEL_STATE_COMMITTED;
	g_status.error = 0;
	refresh_active_status();
	LOG_INF("model COMMIT: activated slot %u (version=%u)",
		g_stage_slot, g_status.active_version);
	ota_progress_ui_end(true, "Model updated");
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
	case BLE_MODEL_CMD_BEGIN:
		if (len < 10U) {
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		}
		rc = begin_transfer(sys_get_le32(&p[1]), sys_get_le32(&p[5]), p[9]);
		if (rc != 0) {
			set_error(rc);
			return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
		}
		break;

	case BLE_MODEL_CMD_COMMIT: {
		const uint8_t *sig = NULL;
		size_t sig_len = 0;

		if (len >= 2U) {
			sig_len = p[1];
			if (sig_len > 0U) {
				if (len < 2U + sig_len) {
					return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
				}
				sig = &p[2];
			}
		}
		rc = commit_transfer(sig, sig_len);
		if (rc != 0) {
			set_error(rc);
			return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
		}
		break;
	}

	case BLE_MODEL_CMD_ABORT:
		LOG_INF("model ABORT by peer");
		ble_model_transfer_reset();
		ota_progress_ui_end(false, "Model aborted");
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

	if (g_status.state != BLE_MODEL_STATE_RECEIVING || g_fa == NULL) {
		return BT_GATT_ERR(BT_ATT_ERR_WRITE_REQ_REJECTED);
	}
	if (len == 0U) {
		return 0;
	}
	if ((g_status.received + len) > g_status.total) {
		LOG_ERR("model DATA: overflow (%u + %u > %u)",
			g_status.received, len, g_status.total);
		set_error(-EFBIG);
		return BT_GATT_ERR(BT_ATT_ERR_WRITE_REQ_REJECTED);
	}

	if (append_data(buf, len) != 0) {
		set_error(-EIO);
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	if (g_status.received == g_status.total) {
		g_status.state = BLE_MODEL_STATE_READY;
		notify_status();
	} else if ((g_status.received & 0x3FFU) == 0U) {
		/* Throttle mid-stream status (every ~1 KiB) without blocking ATT. */
		notify_status();
	}
	return len;
}

BT_GATT_SERVICE_DEFINE(model_transfer_svc,
	BT_GATT_PRIMARY_SERVICE(&model_svc_uuid),

	BT_GATT_CHARACTERISTIC(&model_ctrl_uuid.uuid,
		BT_GATT_CHRC_WRITE,
		BT_GATT_PERM_WRITE_ENCRYPT,
		NULL, write_ctrl, NULL),

	BT_GATT_CHARACTERISTIC(&model_data_uuid.uuid,
		BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		BT_GATT_PERM_WRITE_ENCRYPT,
		NULL, write_data, NULL),

	BT_GATT_CHARACTERISTIC(&model_status_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ_ENCRYPT,
		read_status, NULL, NULL),
	BT_GATT_CCC(status_ccc_changed,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE_ENCRYPT),
);

#endif /* CONFIG_GLUCOSE_MATRICES_FROM_XIP */
