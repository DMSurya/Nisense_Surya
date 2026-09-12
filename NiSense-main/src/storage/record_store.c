/**
 * @file record_store.c
 * @brief Append-only NOR record store (see include/record_store.h).
 */

#include "record_store.h"
#include "nor_qspi_busy.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/crc.h>
#include <string.h>
#include <errno.h>

#ifdef CONFIG_SETTINGS
#include <zephyr/settings/settings.h>
#define REC_META_KEY "recstore/meta"
#endif

LOG_MODULE_REGISTER(record_store, CONFIG_LOG_DEFAULT_LEVEL);

BUILD_ASSERT(sizeof(struct rec_glucose) <= RECORD_PAYLOAD_MAX,
	     "rec_glucose exceeds slot payload");
BUILD_ASSERT(sizeof(struct rec_vitals) <= RECORD_PAYLOAD_MAX,
	     "rec_vitals exceeds slot payload");
BUILD_ASSERT(sizeof(struct rec_ppg_sample) == 48U, "rec_ppg_sample size drift");
BUILD_ASSERT(sizeof(struct rec_glucose_raw_sample) == 8U,
	     "rec_glucose_raw_sample size drift");
BUILD_ASSERT(REC_PPG_SAMPLES_PER_CHUNK >= 1U, "PPG chunk packing broken");
BUILD_ASSERT(REC_GLUCOSE_SAMPLES_PER_CHUNK >= 1U, "glucose chunk packing broken");

#define SECTOR_SIZE      4096U
#define SLOTS_PER_SECTOR (SECTOR_SIZE / RECORD_SLOT_SIZE)   /* 16 */

#ifndef CONFIG_RECORD_STORE_SUMMARY_RESERVE
#define CONFIG_RECORD_STORE_SUMMARY_RESERVE 128
#endif

/* Persisted metadata (kept in NVS via settings). Head/tail are absolute,
 * monotonically increasing slot counters; physical slot = counter % slot_count. */
struct rec_meta {
	uint64_t head;            /* next slot to write */
	uint64_t tail;            /* oldest un-reclaimed slot */
	uint32_t next_record_id;
	uint32_t dropped;
	uint32_t next_measurement_id; /* monotonic cycle id allocator */
	uint32_t summary_ack_upto;    /* SUMMARY ACK watermark (no tail move) */
	uint32_t crc_fail_count;
};

/* Legacy sizes for settings migration. */
#define REC_META_SIZE_V1 24U /* head/tail/next_record_id/dropped */
#define REC_META_SIZE_V2 28U /* + next_measurement_id */

static const struct flash_area *g_fa;
static uint32_t g_slot_count;
static struct rec_meta g_meta;
static uint32_t g_active_measurement_id;
static bool g_ready;
static bool g_meta_dirty;
static K_MUTEX_DEFINE(g_lock);

/* Amortized read_next scan cursor (invalidated on head/tail change). */
static bool g_scan_valid;
static uint64_t g_scan_counter;
static uint32_t g_scan_after_id;
static uint8_t g_scan_type;

/* ---- metadata persistence ------------------------------------------------ */

static void scan_invalidate(void)
{
	g_scan_valid = false;
}

#ifdef CONFIG_SETTINGS
static int meta_direct_load(const char *key, size_t len, settings_read_cb read_cb,
			    void *cb_arg, void *param)
{
	ARG_UNUSED(key);
	struct rec_meta *m = param;

	if (len == sizeof(*m)) {
		ssize_t rc = read_cb(cb_arg, m, sizeof(*m));

		return (rc < 0) ? (int)rc : 0;
	}

	memset(m, 0, sizeof(*m));

	if (len == REC_META_SIZE_V1 || len == REC_META_SIZE_V2) {
		ssize_t rc = read_cb(cb_arg, m, len);

		if (rc < 0) {
			return (int)rc;
		}
		if (m->next_measurement_id == 0U) {
			m->next_measurement_id = 1U;
		}
		return 0;
	}
	return -EINVAL;
}

static bool meta_load(struct rec_meta *m)
{
	struct rec_meta tmp = {0};

	if (settings_load_subtree_direct(REC_META_KEY, meta_direct_load, &tmp) == 0 &&
	    (tmp.head != 0U || tmp.tail != 0U || tmp.next_record_id != 0U)) {
		*m = tmp;
		return true;
	}
	return false;
}

static void meta_save(void)
{
	(void)settings_save_one(REC_META_KEY, &g_meta, sizeof(g_meta));
	g_meta_dirty = false;
}
#else
static bool meta_load(struct rec_meta *m) { ARG_UNUSED(m); return false; }
static void meta_save(void) { g_meta_dirty = false; }
#endif /* CONFIG_SETTINGS */

static void meta_save_if_dirty(void)
{
	if (g_meta_dirty) {
		meta_save();
	}
}

/* ---- helpers ------------------------------------------------------------- */

static inline uint32_t phys_slot(uint64_t counter)
{
	return (uint32_t)(counter % g_slot_count);
}

static inline off_t slot_offset(uint32_t slot)
{
	return (off_t)slot * RECORD_SLOT_SIZE;
}

static int read_frame(uint32_t slot, struct rec_frame_header *hdr, uint8_t *payload)
{
	uint8_t buf[RECORD_SLOT_SIZE];
	int rc = flash_area_read(g_fa, slot_offset(slot), buf, RECORD_SLOT_SIZE);

	if (rc != 0) {
		return rc;
	}
	memcpy(hdr, buf, sizeof(*hdr));
	if (hdr->magic != RECORD_STORE_MAGIC || hdr->len > RECORD_PAYLOAD_MAX) {
		return -ENOENT;
	}
	if (crc32_ieee(&buf[RECORD_FRAME_HDR_SIZE], hdr->len) != hdr->crc32) {
		g_meta.crc_fail_count++;
		g_meta_dirty = true;
		return -EIO;
	}
	if (payload != NULL) {
		memcpy(payload, &buf[RECORD_FRAME_HDR_SIZE], hdr->len);
	}
	return 0;
}

static int erase_sector_for_slot(uint32_t slot)
{
	off_t sector_off = (off_t)(slot / SLOTS_PER_SECTOR) * SECTOR_SIZE;
	int rc;

	nor_qspi_busy_acquire();
	rc = flash_area_erase(g_fa, sector_off, SECTOR_SIZE);
	nor_qspi_busy_release();
	return rc;
}

static uint32_t count_pending_summary_unlocked(void)
{
	uint32_t n = 0U;

	for (uint64_t c = g_meta.tail; c < g_meta.head; c++) {
		struct rec_frame_header hdr;

		if (read_frame(phys_slot(c), &hdr, NULL) != 0) {
			continue;
		}
		if (RECORD_TYPE_IS_SUMMARY(hdr.type)) {
			n++;
		}
	}
	return n;
}

/**
 * Make one free slot when the ring is full.
 * @return 0 if space available (or made), -ENOSPC if raw write blocked by reserve.
 */
static int make_room_for_append(uint8_t new_type)
{
	while ((g_meta.head - g_meta.tail) >= g_slot_count) {
		struct rec_frame_header hdr;
		int rf = read_frame(phys_slot(g_meta.tail), &hdr, NULL);
		bool can_drop = false;

		if (rf != 0) {
			can_drop = true;
		} else if (RECORD_TYPE_IS_RAW(hdr.type)) {
			can_drop = true;
		} else if (RECORD_TYPE_IS_SUMMARY(hdr.type)) {
			uint32_t pending_summary = count_pending_summary_unlocked();

			if (pending_summary > (uint32_t)CONFIG_RECORD_STORE_SUMMARY_RESERVE) {
				can_drop = true;
			}
		} else {
			can_drop = true;
		}

		if (can_drop) {
			g_meta.tail++;
			g_meta.dropped++;
			scan_invalidate();
			LOG_WRN("record_store: ring full, dropped oldest unsynced record");
			return 0;
		}

		/* Protected clinical summary at tail. */
		if (RECORD_TYPE_IS_RAW(new_type)) {
			LOG_ERR("record_store: -ENOSPC (summary reserve protects oldest)");
			return -ENOSPC;
		}

		/* New summary must land — force-drop protected oldest. */
		g_meta.tail++;
		g_meta.dropped++;
		scan_invalidate();
		LOG_WRN("record_store: ring full, force-dropped protected summary for new summary");
		return 0;
	}
	return 0;
}

static void reconcile_orphans(void)
{
	uint32_t adopted = 0U;

	/* Adopt consecutive valid frames at head that match next_record_id
	 * (NOR write completed, meta_save did not). */
	while (adopted < SLOTS_PER_SECTOR) {
		struct rec_frame_header hdr;
		int rc = read_frame(phys_slot(g_meta.head), &hdr, NULL);

		if (rc != 0) {
			break;
		}
		if (hdr.record_id != g_meta.next_record_id) {
			break;
		}
		LOG_WRN("record_store: reconcile orphan record_id=%u", hdr.record_id);
		g_meta.next_record_id++;
		g_meta.head++;
		adopted++;
	}

	if (adopted > 0U) {
		scan_invalidate();
		meta_save();
		LOG_INF("record_store: reconciled %u orphan slot(s)", adopted);
	}
}

/* ---- public API ---------------------------------------------------------- */

int record_store_init(void)
{
	int rc;

	if (g_ready) {
		return 0;
	}

	rc = flash_area_open(FIXED_PARTITION_ID(record_store_partition), &g_fa);
	if (rc != 0) {
		LOG_ERR("record_store: flash_area_open failed %d", rc);
		return rc;
	}

	g_slot_count = (uint32_t)(g_fa->fa_size / RECORD_SLOT_SIZE);
	if (g_slot_count == 0U) {
		LOG_ERR("record_store: partition too small");
		return -EINVAL;
	}

	memset(&g_meta, 0, sizeof(g_meta));
	g_meta.next_record_id = 1U;
	g_meta.next_measurement_id = 1U;
	g_active_measurement_id = 0U;
	g_meta_dirty = false;
	scan_invalidate();

	if (meta_load(&g_meta)) {
		if (g_meta.next_measurement_id == 0U) {
			g_meta.next_measurement_id = 1U;
		}
		LOG_INF("record_store: meta loaded (head=%llu tail=%llu next_id=%u "
			"next_meas=%u sum_ack=%u crc_fail=%u)",
			(unsigned long long)g_meta.head, (unsigned long long)g_meta.tail,
			g_meta.next_record_id, g_meta.next_measurement_id,
			g_meta.summary_ack_upto, g_meta.crc_fail_count);
		reconcile_orphans();
	} else {
		LOG_INF("record_store: fresh store (%u slots, %u KB)",
			g_slot_count, (unsigned)(g_fa->fa_size / 1024));
	}

	g_ready = true;
	return 0;
}

int record_store_begin_measurement(uint32_t *out_id)
{
	uint32_t id;

	if (!g_ready) {
		return -ENODEV;
	}

	k_mutex_lock(&g_lock, K_FOREVER);
	id = g_meta.next_measurement_id;
	if (id == 0U) {
		id = 1U;
	}
	g_meta.next_measurement_id = id + 1U;
	g_active_measurement_id = id;
	meta_save();
	k_mutex_unlock(&g_lock);

	if (out_id != NULL) {
		*out_id = id;
	}
	LOG_INF("record_store: begin measurement_id=%u", id);
	return 0;
}

void record_store_end_measurement(void)
{
	k_mutex_lock(&g_lock, K_FOREVER);
	g_active_measurement_id = 0U;
	k_mutex_unlock(&g_lock);
}

uint32_t record_store_active_measurement_id(void)
{
	return g_active_measurement_id;
}

int record_store_append(uint8_t type, const void *payload, size_t len, uint32_t *out_id)
{
	uint8_t slot_buf[RECORD_SLOT_SIZE];
	struct rec_frame_header *hdr = (struct rec_frame_header *)slot_buf;
	int rc = 0;

	if (!g_ready) {
		return -ENODEV;
	}
	if (payload == NULL || len == 0U || len > RECORD_PAYLOAD_MAX) {
		return -EINVAL;
	}

	k_mutex_lock(&g_lock, K_FOREVER);

	rc = make_room_for_append(type);
	if (rc != 0) {
		goto out;
	}

	uint32_t slot = phys_slot(g_meta.head);

	/* Erase the sector when the head enters its first slot.
	 * Always erase before programming — never rewrite a dirty page. */
	if ((slot % SLOTS_PER_SECTOR) == 0U) {
		rc = erase_sector_for_slot(slot);
		if (rc != 0) {
			LOG_ERR("record_store: erase failed %d", rc);
			goto out;
		}
	}

	memset(slot_buf, 0xFF, sizeof(slot_buf));
	hdr->magic = RECORD_STORE_MAGIC;
	hdr->type = type;
	hdr->version = RECORD_SCHEMA_VERSION;
	hdr->len = (uint8_t)len;
	hdr->record_id = g_meta.next_record_id;
	hdr->crc32 = crc32_ieee(payload, len);
	memcpy(&slot_buf[RECORD_FRAME_HDR_SIZE], payload, len);

	nor_qspi_busy_acquire();
	rc = flash_area_write(g_fa, slot_offset(slot), slot_buf, RECORD_SLOT_SIZE);
	nor_qspi_busy_release();
	if (rc != 0) {
		LOG_ERR("record_store: write failed %d", rc);
		goto out;
	}

	if (out_id != NULL) {
		*out_id = g_meta.next_record_id;
	}
	g_meta.next_record_id++;
	g_meta.head++;
	scan_invalidate();
	meta_save();

out:
	k_mutex_unlock(&g_lock);
	return rc;
}

int record_store_read_next(uint32_t after_id, struct rec_frame_header *hdr_out,
			   void *payload, size_t *out_len)
{
	return record_store_read_next_type(after_id, RECORD_TYPE_INVALID,
					   hdr_out, payload, out_len);
}

int record_store_read_next_type(uint32_t after_id, uint8_t type,
				struct rec_frame_header *hdr_out,
				void *payload, size_t *out_len)
{
	int result = -ENOENT;
	uint64_t start;

	if (!g_ready) {
		return -ENODEV;
	}

	k_mutex_lock(&g_lock, K_FOREVER);

	start = g_meta.tail;
	if (g_scan_valid && g_scan_after_id == after_id && g_scan_type == type &&
	    g_scan_counter >= g_meta.tail && g_scan_counter < g_meta.head) {
		start = g_scan_counter;
	}

	for (uint64_t c = start; c < g_meta.head; c++) {
		struct rec_frame_header hdr;
		uint8_t buf[RECORD_PAYLOAD_MAX];

		if (read_frame(phys_slot(c), &hdr, buf) != 0) {
			continue;
		}
		if (hdr.record_id <= after_id) {
			continue;
		}
		if (type != RECORD_TYPE_INVALID && (uint8_t)hdr.type != type) {
			continue;
		}
		if (hdr_out != NULL) {
			*hdr_out = hdr;
		}
		if (payload != NULL) {
			memcpy(payload, buf, hdr.len);
		}
		if (out_len != NULL) {
			*out_len = hdr.len;
		}
		g_scan_valid = true;
		g_scan_counter = c + 1U;
		g_scan_after_id = hdr.record_id;
		g_scan_type = type;
		result = 0;
		break;
	}

	meta_save_if_dirty();
	k_mutex_unlock(&g_lock);
	return result;
}

int record_store_read_latest_type(uint8_t type, struct rec_frame_header *hdr_out,
				  void *payload, size_t *out_len)
{
	int result = -ENOENT;

	if (!g_ready) {
		return -ENODEV;
	}
	if (type == RECORD_TYPE_INVALID) {
		return -EINVAL;
	}

	k_mutex_lock(&g_lock, K_FOREVER);

	if (g_meta.head == g_meta.tail) {
		goto out;
	}

	for (uint64_t c = g_meta.head; c > g_meta.tail; ) {
		struct rec_frame_header hdr;
		uint8_t buf[RECORD_PAYLOAD_MAX];

		c--;
		if (read_frame(phys_slot(c), &hdr, buf) != 0) {
			continue;
		}
		if ((uint8_t)hdr.type != type) {
			continue;
		}
		if (hdr_out != NULL) {
			*hdr_out = hdr;
		}
		if (payload != NULL) {
			memcpy(payload, buf, hdr.len);
		}
		if (out_len != NULL) {
			*out_len = hdr.len;
		}
		result = 0;
		break;
	}

out:
	meta_save_if_dirty();
	k_mutex_unlock(&g_lock);
	return result;
}

int record_store_read_id(uint32_t record_id, struct rec_frame_header *hdr_out,
			 void *payload, size_t *out_len)
{
	int result = -ENOENT;

	if (!g_ready) {
		return -ENODEV;
	}

	k_mutex_lock(&g_lock, K_FOREVER);

	for (uint64_t c = g_meta.tail; c < g_meta.head; c++) {
		struct rec_frame_header hdr;
		uint8_t buf[RECORD_PAYLOAD_MAX];

		if (read_frame(phys_slot(c), &hdr, buf) != 0) {
			continue;
		}
		if (hdr.record_id != record_id) {
			continue;
		}
		if (hdr_out != NULL) {
			*hdr_out = hdr;
		}
		if (payload != NULL) {
			memcpy(payload, buf, hdr.len);
		}
		if (out_len != NULL) {
			*out_len = hdr.len;
		}
		result = 0;
		break;
	}

	meta_save_if_dirty();
	k_mutex_unlock(&g_lock);
	return result;
}

int record_store_ack_synced(uint32_t up_to_id, uint8_t mode)
{
	if (!g_ready) {
		return -ENODEV;
	}
	if (mode != RECORD_ACK_MODE_SUMMARY && mode != RECORD_ACK_MODE_FULL) {
		return -EINVAL;
	}

	k_mutex_lock(&g_lock, K_FOREVER);

	if (mode == RECORD_ACK_MODE_SUMMARY) {
		if (up_to_id > g_meta.summary_ack_upto) {
			g_meta.summary_ack_upto = up_to_id;
		}
		LOG_INF("record_store: SUMMARY ack up_to=%u (tail unchanged, pending=%u)",
			up_to_id, (uint32_t)(g_meta.head - g_meta.tail));
		meta_save();
		k_mutex_unlock(&g_lock);
		return 0;
	}

	/* FULL: advance tail past every kept record whose id <= up_to_id. */
	while (g_meta.tail < g_meta.head) {
		struct rec_frame_header hdr;

		if (read_frame(phys_slot(g_meta.tail), &hdr, NULL) == 0 &&
		    hdr.record_id > up_to_id) {
			break;
		}
		g_meta.tail++;
	}
	if (up_to_id > g_meta.summary_ack_upto) {
		g_meta.summary_ack_upto = up_to_id;
	}
	scan_invalidate();
	meta_save();

	k_mutex_unlock(&g_lock);
	return 0;
}

void record_store_get_stats(struct record_store_stats *stats)
{
	uint32_t pending;
	uint32_t pending_summary = 0U;
	uint32_t pending_raw = 0U;
	uint32_t pending_glucose = 0U;
	uint32_t pending_vitals = 0U;
	uint32_t pending_temp = 0U;
	uint32_t pending_ppg_raw = 0U;
	uint32_t pending_glucose_raw = 0U;

	if (stats == NULL) {
		return;
	}
	k_mutex_lock(&g_lock, K_FOREVER);
	pending = (uint32_t)(g_meta.head - g_meta.tail);
	for (uint64_t c = g_meta.tail; c < g_meta.head; c++) {
		struct rec_frame_header hdr;

		if (read_frame(phys_slot(c), &hdr, NULL) != 0) {
			continue;
		}
		if (RECORD_TYPE_IS_RAW(hdr.type)) {
			pending_raw++;
		} else if (RECORD_TYPE_IS_SUMMARY(hdr.type)) {
			pending_summary++;
		}
		switch (hdr.type) {
		case RECORD_TYPE_GLUCOSE:
			pending_glucose++;
			break;
		case RECORD_TYPE_VITALS:
			pending_vitals++;
			break;
		case RECORD_TYPE_TEMP:
			pending_temp++;
			break;
		case RECORD_TYPE_PPG_RAW:
			pending_ppg_raw++;
			break;
		case RECORD_TYPE_GLUCOSE_RAW:
			pending_glucose_raw++;
			break;
		default:
			break;
		}
	}
	stats->next_record_id = g_meta.next_record_id;
	stats->head_slot = phys_slot(g_meta.head);
	stats->tail_slot = phys_slot(g_meta.tail);
	stats->slot_count = g_slot_count;
	stats->pending = pending;
	stats->dropped = g_meta.dropped;
	stats->free_slots = (pending < g_slot_count) ? (g_slot_count - pending) : 0U;
	stats->partition_bytes = g_ready ? (uint32_t)g_fa->fa_size : 0U;
	stats->used_bytes = pending * RECORD_SLOT_SIZE;
	stats->free_bytes = stats->free_slots * RECORD_SLOT_SIZE;
	stats->pending_summary = pending_summary;
	stats->pending_raw = pending_raw;
	stats->pending_glucose = pending_glucose;
	stats->pending_vitals = pending_vitals;
	stats->pending_temp = pending_temp;
	stats->pending_ppg_raw = pending_ppg_raw;
	stats->pending_glucose_raw = pending_glucose_raw;
	stats->summary_ack_upto = g_meta.summary_ack_upto;
	stats->crc_fail_count = g_meta.crc_fail_count;
	meta_save_if_dirty();
	k_mutex_unlock(&g_lock);
}

uint32_t record_store_pending(void)
{
	if (!g_ready) {
		return 0;
	}
	return (uint32_t)(g_meta.head - g_meta.tail);
}

int record_store_factory_reset(void)
{
	int rc;

	if (!g_ready) {
		return -ENODEV;
	}

	k_mutex_lock(&g_lock, K_FOREVER);
	nor_qspi_busy_acquire();
	rc = flash_area_erase(g_fa, 0, g_fa->fa_size);
	nor_qspi_busy_release();

	memset(&g_meta, 0, sizeof(g_meta));
	g_meta.next_record_id = 1U;
	g_meta.next_measurement_id = 1U;
	g_active_measurement_id = 0U;
	scan_invalidate();
	meta_save();
	k_mutex_unlock(&g_lock);

	LOG_WRN("record_store: factory reset (erased %u KB)",
		(unsigned)(g_fa->fa_size / 1024));
	return rc;
}

void record_store_lock(void)
{
	k_mutex_lock(&g_lock, K_FOREVER);
}

void record_store_unlock(void)
{
	k_mutex_unlock(&g_lock);
}
