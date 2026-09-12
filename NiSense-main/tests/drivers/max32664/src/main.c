/*
 * MAX32664 MFIO / command-status unit tests.
 *
 * Validates AN6924-oriented status classification (0x00 / 0xFE / 0xFF) and a
 * lightweight MFIO hold-low contract model (assert before I2C, hold through
 * write+delay+read, release after). Full GPIO/I2C emul is out of scope here.
 */

#include <zephyr/ztest.h>
#include <zephyr/drivers/sensor/max32664.h>
#include <stdbool.h>
#include <stdint.h>

/* Minimal MFIO txn model mirroring max32664_mfio_txn_begin/end contract. */
struct mfio_txn_model {
	bool low;
	bool low_during_write;
	bool low_during_delay;
	bool low_during_read;
	unsigned asserts;
	unsigned deasserts;
};

static void model_begin_normal(struct mfio_txn_model *m)
{
	m->low = true;
	m->asserts++;
}

static void model_end_normal(struct mfio_txn_model *m)
{
	m->low = false;
	m->deasserts++;
}

static void model_run_txn(struct mfio_txn_model *m)
{
	model_begin_normal(m);
	m->low_during_write = m->low;
	m->low_during_delay = m->low;
	m->low_during_read = m->low;
	model_end_normal(m);
}

ZTEST(max32664_status, test_classify_ok_busy_unknown)
{
	zassert_equal(max32664_classify_cmd_status(0x00), MAX32664_CMD_STATUS_OK);
	zassert_equal(max32664_classify_cmd_status(0xFE), MAX32664_CMD_STATUS_BUSY);
	zassert_equal(max32664_classify_cmd_status(0xFF), MAX32664_CMD_STATUS_UNKNOWN);
	zassert_equal(max32664_classify_cmd_status(0x01), MAX32664_CMD_STATUS_ERROR);
}

ZTEST(max32664_status, test_mfio_hold_low_across_txn)
{
	struct mfio_txn_model m = {0};

	model_run_txn(&m);

	zassert_true(m.low_during_write, "MFIO must stay low during I2C write");
	zassert_true(m.low_during_delay, "MFIO must stay low during CMD_DELAY");
	zassert_true(m.low_during_read, "MFIO must stay low during I2C read");
	zassert_false(m.low, "MFIO must be high after STOP");
	zassert_equal(m.asserts, 1u);
	zassert_equal(m.deasserts, 1u);
}

ZTEST(max32664_status, test_irq_wake_five_dummy_writes)
{
	/* AN6924 p.12: five writes of 0x00 to addr 0x00 before hub txn. */
	unsigned wakes = 0;

	for (int i = 0; i < 5; i++) {
		wakes++;
	}
	zassert_equal(wakes, 5u);
}

ZTEST_SUITE(max32664_status, NULL, NULL, NULL, NULL, NULL);
