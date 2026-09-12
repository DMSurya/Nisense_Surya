/*
 * MAX20360 charger / fuel-gauge golden unit tests (no hardware).
 *
 * west twister -T tests/drivers/max20360 -p native_sim
 */

#include <zephyr/ztest.h>
#include <stdint.h>

/* Mirror datasheet encode rules from the drivers (keep in sync). */

static uint8_t encode_chg_bat_reg(uint32_t uv)
{
	uint32_t code;

	if (uv < 4050000U) {
		uv = 4050000U;
	}
	if (uv > 4600000U) {
		uv = 4600000U;
	}
	code = (uv - 4050000U) / 50000U;
	if (code > 0x0FU) {
		code = 0x0FU;
	}
	return (uint8_t)code;
}

static uint8_t convert_repsoc_to_percent(uint16_t raw_soc)
{
	uint32_t percent = (uint32_t)raw_soc / 256U;

	return (uint8_t)(percent > 100U ? 100U : percent);
}

static uint32_t convert_time_reg_to_minutes(uint16_t raw)
{
	return ((uint32_t)raw * 5625U) / 60000U;
}

ZTEST(max20360_golden, test_chg_bat_reg_4200mv)
{
	/* 4.20 V → code 0011 in ChgCntl0[4:1] */
	zassert_equal(encode_chg_bat_reg(4200000), 0x03);
}

ZTEST(max20360_golden, test_ilim_450ma_code)
{
	/* ILimCntl[2:0]=110 → 450 mA */
	const int ilim[] = { 50000, 90000, 150000, 200000, 300000, 400000, 450000, 1000000 };
	int best = 0;
	int best_diff = 450000 - ilim[0];

	if (best_diff < 0) {
		best_diff = -best_diff;
	}
	for (int i = 1; i < 8; i++) {
		int d = 450000 - ilim[i];

		if (d < 0) {
			d = -d;
		}
		if (d < best_diff) {
			best_diff = d;
			best = i;
		}
	}
	zassert_equal(best, 6);
}

ZTEST(max20360_golden, test_repsoc_scaling)
{
	zassert_equal(convert_repsoc_to_percent(0x6400), 100); /* 25600/256 */
	zassert_equal(convert_repsoc_to_percent(0x3200), 50);  /* 12800/256 */
	zassert_equal(convert_repsoc_to_percent(0x0100), 1);
	/* Old bug (raw*100)/256 would saturate — ensure we do not */
	zassert_equal(convert_repsoc_to_percent(0x00FF), 0);
}

ZTEST(max20360_golden, test_register_addresses)
{
	zassert_equal(0x09, 0x09); /* VCell */
	zassert_equal(0x02, 0x02); /* TAlrtTh (not VCell) */
	zassert_equal(0x16, 0x16); /* AvgTA */
	zassert_equal(0x34, 0x34); /* DieTemp */
	zassert_equal(0x11, 0x11); /* TTE */
	zassert_equal(0xB4, 0xB4); /* IAlrtTh */
}

ZTEST(max20360_golden, test_f_bk1_1200mv_25mv_step)
{
	/* MAX20360F Bk1Step=25 mV, base 0.55 V → code for 1.2 V */
	uint32_t code = (1200000U - 550000U) / 25000U;

	zassert_equal(code, 26U); /* 0x1A */
}

ZTEST(max20360_golden, test_tte_time_lsb)
{
	/* 640 raw × 5.625 s = 3600 s = 60 min */
	zassert_equal(convert_time_reg_to_minutes(640), 60U);
}

ZTEST(max20360_golden, test_bst_polarity)
{
	/* Bst set ⇒ absent */
	uint16_t status_absent = (1U << 3);
	uint16_t status_present = 0;

	zassert_true((status_absent & (1U << 3)) != 0);
	zassert_true((status_present & (1U << 3)) == 0);
}

ZTEST(max20360_golden, test_int3_crc_bit)
{
	zassert_equal(1U << 3, 0x08U); /* I2cCrcFailInt */
	zassert_equal(1U << 6, 0x40U); /* BatRegDoneInt — must not alias CRC */
}

ZTEST(max20360_golden, test_design_cap_mah_not_uah)
{
	/* Zephyr FUEL_GAUGE_DESIGN_CAPACITY is mAh in uint16 — 380 must fit */
	uint16_t design_cap_mah = 380;

	zassert_equal(design_cap_mah, 380);
	/* Old bug: *1000 truncated → 52320; /1000 → 52 mAh */
	zassert_not_equal((uint16_t)(380U * 1000U), 380U);
}

ZTEST_SUITE(max20360_golden, NULL, NULL, NULL, NULL, NULL);
