/**
 * @file ble_record_sync.c
 * @brief BLE GATT service for NOR record_store bulk pull (see ble_record_sync.h).
 */

#include "ble_record_sync.h"
#include "record_sync_engine.h"
#include "record_store.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <string.h>
#include <errno.h>

LOG_MODULE_REGISTER(ble_rec_sync, CONFIG_LOG_DEFAULT_LEVEL);

#define REC_SVC_UUID_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef4)
#define REC_CTRL_UUID_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf401)
#define REC_DATA_UUID_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf402)
#define REC_STATUS_UUID_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf403)

static struct bt_uuid_128 rec_svc_uuid = BT_UUID_INIT_128(REC_SVC_UUID_VAL);
static struct bt_uuid_128 rec_ctrl_uuid = BT_UUID_INIT_128(REC_CTRL_UUID_VAL);
static struct bt_uuid_128 rec_data_uuid = BT_UUID_INIT_128(REC_DATA_UUID_VAL);
static struct bt_uuid_128 rec_status_uuid = BT_UUID_INIT_128(REC_STATUS_UUID_VAL);

static struct ble_record_sync_status g_status;
static bool g_data_notify_enabled;
static bool g_status_notify_enabled;
static uint8_t g_mode;
static uint32_t g_after_id;
static atomic_t g_abort;
static struct bt_conn *g_sync_conn;

static uint8_t g_frame[RECORD_SYNC_FRAME_MAX_SIZE];

/* Bulk TX pipeline: several ATT notifications stay queued in the controller so
 * the radio can pack multiple packets per connection event. Serialising one
 * notification per round trip caps throughput at ~1 packet/interval. */
#define REC_TX_SLOTS     4
#define REC_TX_SLOT_SIZE 244U

struct rec_tx_slot {
	struct bt_gatt_notify_params params;
	uint8_t buf[REC_TX_SLOT_SIZE];
};

static struct rec_tx_slot g_tx[REC_TX_SLOTS];
static K_SEM_DEFINE(g_tx_free, REC_TX_SLOTS, REC_TX_SLOTS);
static atomic_t g_tx_busy;

static K_THREAD_STACK_DEFINE(sync_stack, 2048);
static struct k_thread sync_thread;
static k_tid_t sync_tid;

extern const struct bt_gatt_service_static record_sync_svc;

BUILD_ASSERT(BLE_REC_FRAME_HDR_SIZE == RECORD_SYNC_FRAME_HDR_SIZE,
	     "BLE record frame header must match shared sync engine");

static void refresh_pending(void)
{
	g_status.pending = record_store_pending();
}

static void notify_status(void)
{
	refresh_pending();
	if (!g_status_notify_enabled) {
		return;
	}
	/* attrs[7] = Status value (see service layout below). */
	(void)bt_gatt_notify(NULL, &record_sync_svc.attrs[7], &g_status, sizeof(g_status));
}

static void set_error(int err)
{
	g_status.state = BLE_REC_STATE_ERROR;
	g_status.error = (int8_t)((err < -128) ? -128 : err);
	notify_status();
}

void ble_record_sync_reset(void)
{
	atomic_set(&g_abort, 1);
	g_status.state = BLE_REC_STATE_IDLE;
	g_status.error = 0;
	g_status.cursor_id = 0;
	g_status.sent_count = 0;
	g_mode = 0;
	g_after_id = 0;
	/* While the sync thread runs it owns the connection reference and drops it
	 * on the way out, so the pointer stays valid for in-flight notifies. */
	if (sync_tid == NULL && g_sync_conn != NULL) {
		bt_conn_unref(g_sync_conn);
		g_sync_conn = NULL;
	}
	refresh_pending();
}

static void tx_slot_done(struct bt_conn *conn, void *user_data)
{
	ARG_UNUSED(conn);
	atomic_clear_bit(&g_tx_busy, (int)(uintptr_t)user_data);
	k_sem_give(&g_tx_free);
}

static void tx_pool_reset(void)
{
	k_sem_reset(&g_tx_free);
	atomic_clear(&g_tx_busy);
	for (int i = 0; i < REC_TX_SLOTS; i++) {
		k_sem_give(&g_tx_free);
	}
}

static int tx_slot_acquire(k_timeout_t timeout)
{
	for (int retry = 0; retry < 3; retry++) {
		if (k_sem_take(&g_tx_free, timeout) != 0) {
			return -EAGAIN;
		}
		for (int i = 0; i < REC_TX_SLOTS; i++) {
			if (!atomic_test_and_set_bit(&g_tx_busy, i)) {
				return i;
			}
		}
		/* Count outran the bitmask (late completion from a previous
		 * session): swallow the surplus permit and look again. */
		k_sleep(K_MSEC(2));
	}
	return -EAGAIN;
}

static void tx_slot_release(int slot)
{
	atomic_clear_bit(&g_tx_busy, slot);
	k_sem_give(&g_tx_free);
}

/** Block until every queued notification has left the controller. */
static int tx_drain(k_timeout_t timeout)
{
	int taken = 0;
	int rc = 0;

	for (int i = 0; i < REC_TX_SLOTS; i++) {
		if (k_sem_take(&g_tx_free, timeout) != 0) {
			rc = -ETIMEDOUT;
			break;
		}
		taken++;
	}
	while (taken-- > 0) {
		k_sem_give(&g_tx_free);
	}
	return rc;
}

/** Fragment payload per notification: ATT payload (MTU − 3) minus frag header. */
static uint16_t frag_chunk_size(void)
{
	uint16_t mtu = 23U;
	uint16_t chunk;

	if (g_sync_conn != NULL) {
		mtu = bt_gatt_get_mtu(g_sync_conn);
	}
	if (mtu < 23U) {
		mtu = 23U;
	}
	chunk = (uint16_t)(mtu - 3U);
	chunk = (chunk > BLE_REC_FRAG_HDR_SIZE) ?
		(uint16_t)(chunk - BLE_REC_FRAG_HDR_SIZE) : 20U;
	if (chunk > REC_TX_SLOT_SIZE - BLE_REC_FRAG_HDR_SIZE) {
		chunk = REC_TX_SLOT_SIZE - BLE_REC_FRAG_HDR_SIZE;
	}
	return chunk;
}

static int notify_frame(const uint8_t *frame, uint16_t frame_len)
{
	const uint16_t chunk = frag_chunk_size();
	uint16_t offset = 0;

	if (!g_data_notify_enabled) {
		return -EAGAIN;
	}
	/* Slot accounting relies on exactly one completion per notify, so never
	 * fan out to "all subscribers" (bt_gatt_notify_cb with a NULL conn). */
	if (g_sync_conn == NULL) {
		return -ENOTCONN;
	}

	while (offset < frame_len) {
		uint16_t take = (uint16_t)MIN((uint16_t)(frame_len - offset), chunk);
		uint8_t flags = (offset + take < frame_len) ? BLE_REC_FRAG_MORE : 0U;
		struct rec_tx_slot *s;
		int slot = tx_slot_acquire(K_SECONDS(5));
		int rc;

		if (slot < 0) {
			return slot;
		}

		s = &g_tx[slot];
		s->buf[0] = flags;
		sys_put_le16(offset, &s->buf[1]);
		sys_put_le16(frame_len, &s->buf[3]);
		memcpy(&s->buf[BLE_REC_FRAG_HDR_SIZE], frame + offset, take);

		memset(&s->params, 0, sizeof(s->params));
		/* attrs[4] = Data value */
		s->params.attr = &record_sync_svc.attrs[4];
		s->params.data = s->buf;
		s->params.len = (uint16_t)(BLE_REC_FRAG_HDR_SIZE + take);
		s->params.func = tx_slot_done;
		s->params.user_data = (void *)(uintptr_t)slot;

		rc = bt_gatt_notify_cb(g_sync_conn, &s->params);
		for (int attempt = 0; rc == -ENOMEM || rc == -EAGAIN; attempt++) {
			/* Host buffers exhausted: let the link drain, then retry. */
			if (attempt >= 500 || atomic_get(&g_abort) != 0) {
				break;
			}
			k_sleep(K_MSEC(2));
			rc = bt_gatt_notify_cb(g_sync_conn, &s->params);
		}
		if (rc != 0) {
			tx_slot_release(slot);
			return rc;
		}
		offset = (uint16_t)(offset + take);
	}
	return 0;
}

/* Bulk-transfer connection interval, in 1.25 ms units. PPCP advertises a
 * power-friendly 30–50 ms which caps the pull at a few packets per event; ask
 * for a tighter interval while the transfer runs. Rejection is harmless. */
#define REC_SYNC_INT_MIN 12U /* 15 ms */
#define REC_SYNC_INT_MAX 24U /* 30 ms */

/* CONFIG_BT_USER_PHY_UPDATE leaves the PHY to the app, so ask explicitly. Peers
 * without 2M support simply stay on 1M. */
static void request_2m_phy(void)
{
	if (g_sync_conn == NULL) {
		return;
	}
	(void)bt_conn_le_phy_update(g_sync_conn, BT_CONN_LE_PHY_PARAM_2M);
}

static void request_conn_interval(uint16_t int_min, uint16_t int_max)
{
	struct bt_le_conn_param param = {
		.interval_min = int_min,
		.interval_max = int_max,
		.latency = 0,
		.timeout = CONFIG_BT_PERIPHERAL_PREF_TIMEOUT,
	};
	struct bt_conn_info info;

	if (g_sync_conn == NULL) {
		return;
	}
	if (bt_conn_get_info(g_sync_conn, &info) == 0 &&
	    info.type == BT_CONN_TYPE_LE &&
	    info.le.interval_us == ((uint32_t)int_min * 1250U)) {
		return;
	}
	(void)bt_conn_le_param_update(g_sync_conn, &param);
}

/** Drain the TX pipeline, relax the link and drop this session's conn ref. */
static void session_end(void)
{
	(void)tx_drain(K_SECONDS(10));
	request_conn_interval(CONFIG_BT_PERIPHERAL_PREF_MIN_INT,
			      CONFIG_BT_PERIPHERAL_PREF_MAX_INT);
	if (g_sync_conn != NULL) {
		bt_conn_unref(g_sync_conn);
		g_sync_conn = NULL;
	}
	sync_tid = NULL;
}

static void sync_thread_fn(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);
	uint32_t after = g_after_id;
	int64_t last_status_ms = k_uptime_get();

	atomic_set(&g_abort, 0);
	tx_pool_reset();
	request_2m_phy();
	g_status.state = BLE_REC_STATE_SENDING;
	g_status.error = 0;
	g_status.sent_count = 0;
	g_status.cursor_id = after;
	notify_status();

	/* Only one LL control procedure may be outstanding — let the PHY update
	 * settle before asking for a tighter interval. */
	k_sleep(K_MSEC(100));
	request_conn_interval(REC_SYNC_INT_MIN, REC_SYNC_INT_MAX);

	while (atomic_get(&g_abort) == 0) {
		struct rec_frame_header hdr;
		uint8_t payload[RECORD_PAYLOAD_MAX];
		size_t plen = 0;
		uint16_t frame_len;
		int rc;

		rc = record_sync_read_next(&after, g_mode, &hdr, payload, &plen);
		if (rc != 0) {
			break;
		}

		rc = record_sync_pack_frame(&hdr, payload, plen, g_frame, sizeof(g_frame),
					    &frame_len);
		if (rc != 0) {
			set_error(rc);
			session_end();
			return;
		}

		if (notify_frame(g_frame, frame_len) != 0) {
			set_error(-EIO);
			session_end();
			return;
		}

		g_status.cursor_id = after;
		g_status.sent_count++;
		/* Time-throttled: at full pipeline speed a per-record status notify
		 * would steal a meaningful share of the link (and re-scan pending). */
		if (k_uptime_get() - last_status_ms >= 250) {
			last_status_ms = k_uptime_get();
			notify_status();
		}
	}

	if (atomic_get(&g_abort) != 0) {
		g_status.state = BLE_REC_STATE_IDLE;
		g_status.error = 0;
		notify_status();
		session_end();
		return;
	}

	/* Drain before publishing DONE so sent_count is fully on air by the time
	 * the phone reconciles it against the frames it assembled. */
	(void)tx_drain(K_SECONDS(10));
	g_status.state = BLE_REC_STATE_DONE;
	g_status.error = 0;
	notify_status();
	LOG_INF("record sync done: sent=%u cursor=%u", g_status.sent_count,
		g_status.cursor_id);
	session_end();
}

static void start_sync_thread(void)
{
	if (sync_tid != NULL) {
		return;
	}
	sync_tid = k_thread_create(&sync_thread, sync_stack,
				   K_THREAD_STACK_SIZEOF(sync_stack),
				   sync_thread_fn, NULL, NULL, NULL,
				   7, 0, K_NO_WAIT);
	k_thread_name_set(sync_tid, "rec_sync");
}

static ssize_t read_status(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			   void *buf, uint16_t len, uint16_t offset)
{
	ARG_UNUSED(conn);
	refresh_pending();
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &g_status, sizeof(g_status));
}

static void status_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	g_status_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
}

static void data_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	g_data_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
}

static ssize_t write_ctrl(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(attr);
	ARG_UNUSED(offset);
	ARG_UNUSED(flags);

	const uint8_t *p = buf;

	if (len < 1U) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	switch (p[0]) {
	case BLE_REC_CMD_START:
		/* [cmd][after_id:4][mode:1] */
		if (len < 6U) {
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		}
		if (g_status.state == BLE_REC_STATE_SENDING) {
			return BT_GATT_ERR(BT_ATT_ERR_WRITE_REQ_REJECTED);
		}
		g_after_id = sys_get_le32(&p[1]);
		g_mode = p[5];
		if (g_mode != BLE_REC_MODE_SUMMARY_ONLY &&
		    g_mode != BLE_REC_MODE_FULL) {
			return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
		}
		if (!g_data_notify_enabled) {
			set_error(-EAGAIN);
			return BT_GATT_ERR(BT_ATT_ERR_WRITE_REQ_REJECTED);
		}
		if (g_sync_conn != NULL) {
			bt_conn_unref(g_sync_conn);
			g_sync_conn = NULL;
		}
		if (conn != NULL) {
			g_sync_conn = bt_conn_ref(conn);
		}
		LOG_INF("record sync START after=%u mode=%u mtu=%u",
			g_after_id, g_mode,
			g_sync_conn != NULL ? bt_gatt_get_mtu(g_sync_conn) : 0U);
		start_sync_thread();
		break;

	case BLE_REC_CMD_ACK:
		/* [cmd][up_to_id:4][mode:1?] — 5-byte legacy = FULL */
		if (len < 5U) {
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		}
		{
			uint32_t up_to = sys_get_le32(&p[1]);
			uint8_t ack_mode = (len >= 6U) ? p[5] : BLE_REC_MODE_FULL;
			int rc;

			if (ack_mode != BLE_REC_MODE_SUMMARY_ONLY &&
			    ack_mode != BLE_REC_MODE_FULL) {
				return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
			}
			rc = record_store_ack_synced(up_to, ack_mode);
			if (rc != 0) {
				set_error(rc);
				return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
			}
			LOG_INF("record sync ACK up_to=%u mode=%u pending=%u", up_to,
				ack_mode, record_store_pending());
			refresh_pending();
			notify_status();
		}
		break;

	case BLE_REC_CMD_ABORT:
		LOG_INF("record sync ABORT");
		ble_record_sync_reset();
		notify_status();
		break;

	default:
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	return len;
}

/* Attribute layout:
 *   [0] Primary Service
 *   [1] Control declaration        [2] Control value (f401)
 *   [3] Data declaration           [4] Data value (f402)
 *   [5] Data CCC
 *   [6] Status declaration         [7] Status value (f403)
 *   [8] Status CCC
 */
BT_GATT_SERVICE_DEFINE(record_sync_svc,
	BT_GATT_PRIMARY_SERVICE(&rec_svc_uuid),

	BT_GATT_CHARACTERISTIC(&rec_ctrl_uuid.uuid,
		BT_GATT_CHRC_WRITE,
		BT_GATT_PERM_WRITE_ENCRYPT,
		NULL, write_ctrl, NULL),

	BT_GATT_CHARACTERISTIC(&rec_data_uuid.uuid,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, NULL),
	BT_GATT_CCC(data_ccc_changed,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE_ENCRYPT),

	BT_GATT_CHARACTERISTIC(&rec_status_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ_ENCRYPT,
		read_status, NULL, NULL),
	BT_GATT_CCC(status_ccc_changed,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE_ENCRYPT),
);
