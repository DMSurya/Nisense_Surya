/*
 * Copyright (c) 2025 MAX20360 Contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * Maxim MAX20360 PMIC Regulator Driver
 * Complete implementation with all regulator types, interrupt handling, and monitoring
 * Parent PMIC device + child regulators (Buck1/2, Buck-Boost, LDOs, Load Switches)
 */

#define DT_DRV_COMPAT maxim_max20360_regulator

#include <string.h>
#include <strings.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/linear_range.h>
#include <zephyr/devicetree.h>

#include "max20360_regs.h"
#include "max20360_ivmon.h"

LOG_MODULE_REGISTER(regulator_max20360, CONFIG_REGULATOR_LOG_LEVEL);

/* ===== Regulator Config ===== */

/* Enable/Control/Status bit definitions from MAX20360 registers */
#define MAX20360_EN_BIT         BIT(0)      /* Enable bit in control registers */
#define MAX20360_PG_BIT         BIT(0)      /* Power Good bit in status */
#define MAX20360_STAT_UVLO      BIT(2)      /* Under-voltage lockout */
#define MAX20360_MODE_MASK      GENMASK(7, 1)  /* Operating mode bits */
#define MAX20360_FAULT_MASK     BIT(3)      /* Fault flag bit */

/* Regulator source identifiers (from device tree property) */
enum max20360_regulator_source {
	MAX20360_SOURCE_BK1 = 0,
	MAX20360_SOURCE_BK2,
	MAX20360_SOURCE_BK3,
	MAX20360_SOURCE_BBOUT,
	MAX20360_SOURCE_L1OUT,
	MAX20360_SOURCE_L2OUT,
	MAX20360_SOURCE_LSW1,
	MAX20360_SOURCE_LSW2,
	MAX20360_SOURCE_SFOUT,
	MAX20360_SOURCE_BOOST,
};

/* Descriptor for each regulator output */
struct max20360_regulator_desc {
	uint8_t ena_reg;        /* Enable/mode register */
	uint8_t vset_reg;       /* Voltage setting register */
	uint8_t status_reg;     /* Status/feedback register */
	const struct linear_range *voltage_range;
	const char *name;
	enum max20360_regulator_source source;
};

struct regulator_max20360_config {
	struct regulator_common_config common;
	struct i2c_dt_spec bus;  /* I2C bus spec from device tree */
	const struct max20360_regulator_desc *desc;
	uint8_t source;
	const struct device *parent_dev;  /* Pointer to parent PMIC device (const, set at compile time) */
	bool power_cycle_on_init;  /* Perform power cycle during init (clean reset) */
	uint16_t power_cycle_delay_ms;  /* Delay between disable and enable (ms) */
};

struct regulator_max20360_data {
	struct regulator_common_data common;
	bool enabled;
	int32_t current_voltage_uv;
};

/* ===== Enable/Control Register Specifications ===== */

/* All enable registers follow the same structure:
 * BITS 7:5: XXXSeq - Enable sequencing (READ ONLY)
 *   000 = Disabled
 *   001 = Reserved
 *   010 = Enabled at 0% of Boot/POR delay
 *   011 = Enabled at 25% of Boot/POR delay
 *   100 = Enabled at 50% of Boot/POR delay
 *   101 = Reserved
 *   110 = Reserved
 *   111 = Controlled by XXXEn[1:0] after 100% of Boot/POR delay (MANUAL MODE)
 *
 * BITS 1:0: XXXEn - Enable control (WRITE/READ, effective only when Seq=111)
 *   00 = Disabled (output not actively discharged unless reset/shutdown/off)
 *   01 = Enabled
 *   10 = Controlled by MPC_ bits (see MPC_ control registers)
 *   11 = Reserved
 */

/* Buck1 Enable - Register 0x1B (Buck1Ena)
 * To manually control Buck1:
 * 1. Set Buck1Seq to 111 (controlled by Buck1En)
 * 2. Set Buck1En to 01 (enable) or 00 (disable)
 * Default boot behavior controlled by Buck1Seq field.
 */

/* Buck2 Enable - Register 0x27 (Buck2Ena)
 * To manually control Buck2:
 * 1. Set Buck2Seq to 111 (controlled by Buck2En)
 * 2. Set Buck2En to 01 (enable) or 00 (disable)
 * Default boot behavior controlled by Buck2Seq field.
 */

/* Buck3 Enable - Register 0x34 (Buck3Ena)
 * To manually control Buck3:
 * 1. Set Buck3Seq to 111 (controlled by Buck3En)
 * 2. Set Buck3En to 01 (enable) or 00 (disable)
 * Default boot behavior controlled by Buck3Seq field.
 */

/* Buck-Boost Enable - Register 0x40 (BBstEna)
 * To manually control Buck-Boost:
 * 1. Set BBstSeq to 111 (controlled by BBstEn)
 * 2. Set BBstEn to 01 (enable) or 00 (disable)
 * Default boot behavior controlled by BBstSeq field.
 */

/* LDO1 Enable - Register 0x51 (LDO1Ena)
 * To manually control LDO1:
 * 1. Set LDO1Seq to 111 (controlled by LDO1En)
 * 2. Set LDO1En to 01 (enable) or 00 (disable)
 * Default boot behavior controlled by LDO1Seq field.
 */

/* LDO2 Enable - Register 0x55 (LDO2Ena)
 * To manually control LDO2:
 * 1. Set LDO2Seq to 111 (controlled by LDO2En)
 * 2. Set LDO2En to 01 (enable) or 00 (disable)
 * NOTE: LDO2Seq 001 = Enabled always when BAT/SYS present (special case)
 * Default boot behavior controlled by LDO2Seq field.
 */

/* Load Switch 1 Enable - Register 0x59 (LSW1Ena)
 * To manually control LSW1:
 * 1. Set LSW1Seq to 111 (controlled by LSW1En)
 * 2. Set LSW1En to 01 (enable) or 00 (disable)
 * Default boot behavior controlled by LSW1Seq field.
 */

/* Load Switch 2 Enable - Register 0x5C (LSW2Ena)
 * To manually control LSW2:
 * 1. Set LSW2Seq to 111 (controlled by LSW2En)
 * 2. Set LSW2En to 01 (enable) or 00 (disable)
 * Default boot behavior controlled by LSW2Seq field.
 */

/* Boost Enable - Register 0x62 (BoostEna)
 * To manually control Boost:
 * 1. Set BoostSeq to 111 (controlled by BoostEn)
 * 2. Set BoostEn to 01 (enable) or 00 (disable)
 * Default boot behavior controlled by BoostSeq field.
 */

/* ===== Current Limit Specifications ===== */

/* Buck1 Inductor Peak Current - Register 0x1E (Buck1Iset)
 * BIT 7: Buck1ISetLookUpDis
 *   0 = Inductor current set by lookup table (default)
 *   1 = Inductor current set by Buck1ISet[3:0]
 * BITS 3:0: Buck1ISet - Linear scale, 25mA increments (0-375mA)
 *   0000 = 0mA,    0001 = 25mA,   0010 = 50mA,   0011 = 75mA
 *   0100 = 100mA,  0101 = 125mA,  0110 = 150mA,  0111 = 175mA
 *   1000 = 200mA,  1001 = 225mA,  1010 = 250mA,  1011 = 275mA
 *   1100 = 300mA,  1101 = 325mA,  1110 = 350mA,  1111 = 375mA
 * Best efficiency: 150-200mA. Settings <75mA limited by minimum tON.
 * Not yet implemented in driver (returns -ENOTSUP).
 */

/* Buck2 Inductor Peak Current - Register 0x2A (Buck2Iset)
 * Same structure as Buck1:
 * BIT 7: Buck2ISetLookUpDis (0=lookup table, 1=manual Buck2ISet[3:0])
 * BITS 3:0: Buck2ISet - Linear scale, 25mA increments (0-375mA)
 * Best efficiency: 150-200mA. Settings <75mA limited by minimum tON.
 * Not yet implemented in driver (returns -ENOTSUP).
 */

/* Buck3 Inductor Peak Current - Register 0x37 (Buck3Iset)
 * Same structure as Buck1 and Buck2:
 * BIT 7: Buck3ISetLookUpDis (0=lookup table, 1=manual Buck3ISet[3:0])
 * BITS 3:0: Buck3ISet - Linear scale, 25mA increments (0-375mA)
 * Best efficiency: 150-200mA. Settings <75mA limited by minimum tON.
 * Not yet implemented in driver (returns -ENOTSUP).
 */

/* Buck-Boost Inductor Peak Current - Register 0x43 (BBstISet)
 * Complex dual-phase control:
 * BITS 7:4: BBstIPSet2 - Nominal maximum peak current (0-375mA, 25mA steps)
 * BITS 3:0: BBstIPSet1 - Nominal peak current (0-375mA, 25mA steps)
 *
 * Recommended BBstIPSet1 (charging inductor VIN->GND):
 *   VBBOUT <= 2.65V: 50mA
 *   2.7V < VBBOUT <= 3.05V: 75mA
 *   3.1V < VBBOUT <= 3.4V: 100mA
 *   3.45V < VBBOUT <= 3.8V: 125mA
 *   3.85V < VBBOUT <= 4.15V: 150mA
 *   4.2V < VBBOUT <= 4.55V: 175mA
 *   4.6V < VBBOUT <= 4.9V: 200mA
 *   4.95V < VBBOUT <= 5.3V: 225mA
 *   VBBOUT > 5.35V: 250mA
 *
 * BBstIPSet2 = BBstIPSet1 + offset (0-375mA total):
 *   Recommended offset by VBBOUT:
 *   VBBOUT <= 2.65V: +0mA (total 50mA)
 *   2.7V < VBBOUT <= 3.05V: +25mA (total 100mA)
 *   3.1V < VBBOUT <= 3.6V: +50mA (total 200mA)
 *   3.65V < VBBOUT <= 4.35V: +75mA (total 250mA)
 *   VBBOUT > 4.4V: +100mA (total 250mA)
 *
 * Not yet implemented in driver (returns -ENOTSUP).
 */

/* ===== Voltage Ranges - Variant-Specific Step Sizes ===== */

/* Buck1 voltage ranges by step size (variant-dependent) */
/* For 10mV step: 0.55V to 1.20V (0x00 to 0x41 = 65 steps) */
/* 0x00 = 0.55V, 0x01 = 0.56V, ..., 0x41 = 1.20V (65 steps * 10mV) */
static const struct linear_range bk1_voltage_range_10mv = 
	LINEAR_RANGE_INIT(MAX20360_BK1_MIN_UV, MAX20360_BUCK_STEP_10MV, 0x00, 0x41);
static const struct linear_range bk1_voltage_range_25mv = 
	LINEAR_RANGE_INIT(MAX20360_BK1_MIN_UV, MAX20360_BUCK_STEP_25MV, 0x00, 0x3F);
static const struct linear_range bk1_voltage_range_50mv = 
	LINEAR_RANGE_INIT(MAX20360_BK1_MIN_UV, MAX20360_BUCK_STEP_50MV, 0x00, 0x3F);

/* Buck2 voltage ranges by step size (variant-dependent) */
static const struct linear_range bk2_voltage_range_10mv = 
	LINEAR_RANGE_INIT(MAX20360_BK2_MIN_UV, MAX20360_BUCK_STEP_10MV, 0x00, 0x3F);
static const struct linear_range bk2_voltage_range_25mv = 
	LINEAR_RANGE_INIT(MAX20360_BK2_MIN_UV, MAX20360_BUCK_STEP_25MV, 0x00, 0x3F);
static const struct linear_range bk2_voltage_range_50mv = 
	LINEAR_RANGE_INIT(MAX20360_BK2_MIN_UV, MAX20360_BUCK_STEP_50MV, 0x00, 0x3F);

/* Buck3 (0x38): Fixed step size 50mV (all variants) */
static const struct linear_range bk3_voltage_range = 
	LINEAR_RANGE_INIT(MAX20360_BK3_MIN_UV, MAX20360_BUCK_STEP_50MV, 0x00, 0x3F);

/* Buck-Boost/BBOUT (0x42): 50 mV steps, EC range 2.6 V–5.5 V (datasheet VBBOUT).
 * Codes below ~2.6 V interfere with VBBOUT_UVLO — clamp min to 2.6 V (idx 2 if
 * silicon 0x00 were 2.5 V); use 2.6 V base / idx 0 for software encoding.
 */
static const struct linear_range bbout_voltage_range =
	LINEAR_RANGE_INIT(2600000, 50000, 0x00, 0x3A);

/* LDO1 (0x53): Fixed step size 25mV, limited by input supply
 * 0.5V to 1.95V (0x00 to 0x3A = 58 decimal)
 * Register: 6-bit (0x00-0x3F = 0-63)
 * Example: 0x00=0.5V, 0x01=0.525V, ..., 0x3A=1.95V
 */
static const struct linear_range l1out_voltage_range = 
	LINEAR_RANGE_INIT(500000, 25000, 0x00, 0x3A);

/* LDO2 (0x57): Fixed step size 100mV
 * 0.9V to 4.0V (0.9V + 63*100mV = 7.2V theoretical, but limited to 4.0V in practice)
 * Register: 6-bit (0x00-0x3F = 0-63)
 * Example: 0x00=0.9V, 0x01=1.0V, ..., 0x1F=4.0V (0x1F = 31 decimal)
 */
static const struct linear_range l2out_voltage_range = 
	LINEAR_RANGE_INIT(900000, 100000, 0x00, 0x1F);

/* Boost (0x65): Full range 5.0V to 20.0V in 250mV steps
 * Register: 6-bit (0x00-0x3F = 0-63)
 * 0x00 = 5.0V, 0x01 = 5.25V, ..., 0x3C = 20.0V (60 steps * 250mV)
 * Note: Automatic frequency scaling and inductor current management via lookup table
 */
static const struct linear_range boost_voltage_range = 
	LINEAR_RANGE_INIT(5000000, 250000, 0x00, 0x3C);

/* SFOUT (0x86): Fixed outputs only (3.3V or 5V), not a linear range
 * SFOUT register controls mode:
 * BIT 0: SFOUTEn - 0=Disabled, 1=Enabled
 * BIT 1: SFOUTSel - 0=3.3V output, 1=5.0V output
 * For regulator API compatibility, we'll treat it as 2-step range
 */
static const struct linear_range sfout_voltage_range = 
	LINEAR_RANGE_INIT(3300000, 1700000, 0x00, 0x01);  /* 3.3V at 0x00, 5.0V at 0x01 */

/* ===== Regulator Descriptors (ordered by lock mask bits) ===== */

/* BK1 Regulator (Lock mask bit 1) - voltage range set dynamically based on variant */
static struct max20360_regulator_desc max20360_bk1_desc = {
	.ena_reg = MAX20360_REG_BUCK1ENA,
	.vset_reg = MAX20360_REG_BUCK1VSET,
	.status_reg = MAX20360_REG_STATUS4,
	.voltage_range = &bk1_voltage_range_10mv, /* Default, updated during variant detection */
	.name = "BUCK1",
	.source = MAX20360_SOURCE_BK1,
};

/* BK2 Regulator (Lock mask bit 2) - voltage range set dynamically based on variant */
static struct max20360_regulator_desc max20360_bk2_desc = {
	.ena_reg = MAX20360_REG_BUCK2ENA,
	.vset_reg = MAX20360_REG_BUCK2VSET,
	.status_reg = MAX20360_REG_STATUS4,
	.voltage_range = &bk2_voltage_range_25mv, /* Default, updated during variant detection */
	.name = "BUCK2",
	.source = MAX20360_SOURCE_BK2,
};

/* BK3 Regulator (Lock mask bit 3) */
static const struct max20360_regulator_desc __maybe_unused max20360_bk3_desc = {
	.ena_reg = MAX20360_REG_BUCK3ENA,
	.vset_reg = MAX20360_REG_BUCK3VSET,
	.status_reg = MAX20360_REG_STATUS4,
	.voltage_range = &bk3_voltage_range,
	.name = "BUCK3",
	.source = MAX20360_SOURCE_BK3,
};

/* BST/Boost Regulator (Lock mask bit 4) */
static const struct max20360_regulator_desc __maybe_unused max20360_boost_desc = {
	.ena_reg = MAX20360_REG_BOOSTENA,
	.vset_reg = MAX20360_REG_BOOSTVSET,
	.status_reg = MAX20360_REG_STATUS4,
	.voltage_range = &boost_voltage_range,
	.name = "BOOST",
	.source = MAX20360_SOURCE_BOOST,
};

/* BBOUT/Buck-Boost Regulator (Lock mask bit 5) */
static const struct max20360_regulator_desc __maybe_unused max20360_bbout_desc = {
	.ena_reg = MAX20360_REG_BBSTENA,
	.vset_reg = MAX20360_REG_BBSTVSET,
	.status_reg = MAX20360_REG_STATUS4,
	.voltage_range = &bbout_voltage_range,
	.name = "BBOUT",
	.source = MAX20360_SOURCE_BBOUT,
};

/* LDO1 Regulator (Lock mask bit 6) */
static const struct max20360_regulator_desc __maybe_unused max20360_l1out_desc = {
	.ena_reg = MAX20360_REG_LDO1ENA,
	.vset_reg = MAX20360_REG_LDO1VSET,
	.status_reg = MAX20360_REG_STATUS2,
	.voltage_range = &l1out_voltage_range,
	.name = "LDO1",
	.source = MAX20360_SOURCE_L1OUT,
};

/* LDO2 Regulator (Lock mask bit 7) */
static const struct max20360_regulator_desc __maybe_unused max20360_l2out_desc = {
	.ena_reg = MAX20360_REG_LDO2ENA,
	.vset_reg = MAX20360_REG_LDO2VSET,
	.status_reg = MAX20360_REG_STATUS2,
	.voltage_range = &l2out_voltage_range,
	.name = "LDO2",
	.source = MAX20360_SOURCE_L2OUT,
};

/* SFOUT Regulator (No lock mask bit - fixed outputs) */
static const struct max20360_regulator_desc __maybe_unused max20360_sfout_desc = {
	.ena_reg = MAX20360_REG_SFOUTCTR,  /* Enable and voltage selection in same register */
	.vset_reg = MAX20360_REG_SFOUTCTR, /* Same register controls voltage selection */
	.status_reg = MAX20360_REG_STATUS2,
	.voltage_range = &sfout_voltage_range,
	.name = "SFOUT",
	.source = MAX20360_SOURCE_SFOUT,
};

/* Load Switches (No lock mask bits - pass-through switches) */
static const struct max20360_regulator_desc __maybe_unused max20360_lsw1_desc = {
	.ena_reg = MAX20360_REG_LSW1ENA,
	.vset_reg = 0,  /* Load switches don't have voltage setting */
	.status_reg = MAX20360_REG_STATUS2,
	.voltage_range = NULL,
	.name = "LSW1",
	.source = MAX20360_SOURCE_LSW1,
};

static const struct max20360_regulator_desc __maybe_unused max20360_lsw2_desc = {
	.ena_reg = MAX20360_REG_LSW2ENA,
	.vset_reg = 0,  /* Load switches don't have voltage setting */
	.status_reg = MAX20360_REG_STATUS2,
	.voltage_range = NULL,
	.name = "LSW2",
	.source = MAX20360_SOURCE_LSW2,
};

/* ===== Variant Detection ===== */

/* MAX20360 variant identifiers */
enum max20360_variant {
	MAX20360_VARIANT_EV_KIT = 0,
	MAX20360_VARIANT_EV_KIT_HARVESTER,
	MAX20360_VARIANT_A,
	MAX20360_VARIANT_B,
	MAX20360_VARIANT_C,
	MAX20360_VARIANT_F,
	MAX20360_VARIANT_G,
	MAX20360_VARIANT_I,
	MAX20360_VARIANT_J,
	MAX20360_VARIANT_K,
	MAX20360_VARIANT_L,
	MAX20360_VARIANT_M,
	MAX20360_VARIANT_O,
	MAX20360_VARIANT_P,
	MAX20360_VARIANT_UNKNOWN,
};

/* Variant step size configuration */
struct max20360_variant_info {
	enum max20360_variant variant;
	uint32_t bk1_step_uv;      /* BK1 step size in microvolts */
	uint32_t bk2_step_uv;      /* BK2 step size in microvolts */
	uint8_t bk1_default_vset;  /* Default BK1 VSet register value */
	uint8_t bk2_default_vset;  /* Default BK2 VSet register value */
	uint8_t bk3_default_vset;  /* Default BK3 VSet register value */
	bool i2ccrcena;            /* Fletcher-16 CRC enabled in OTP (datasheet Table 8-9) */
	const char *name;
};

/* Variant lookup table from Table 8/9 of MAX20360 datasheet */
static const struct max20360_variant_info variant_table[] = {
	{MAX20360_VARIANT_EV_KIT,           MAX20360_BUCK_STEP_10MV, MAX20360_BUCK_STEP_25MV, 0x37, 0x32, 0x32, true,  "EV KIT"},
	{MAX20360_VARIANT_EV_KIT_HARVESTER, MAX20360_BUCK_STEP_10MV, MAX20360_BUCK_STEP_25MV, 0x37, 0x32, 0x32, true,  "EV KIT WITH HARVESTER"},
	{MAX20360_VARIANT_A,                MAX20360_BUCK_STEP_10MV, MAX20360_BUCK_STEP_10MV, 0x0F, 0x32, 0x19, true,  "MAX20360A"},
	{MAX20360_VARIANT_B,                MAX20360_BUCK_STEP_10MV, MAX20360_BUCK_STEP_10MV, 0x19, 0x32, 0x19, true,  "MAX20360B"},
	{MAX20360_VARIANT_C,                MAX20360_BUCK_STEP_10MV, MAX20360_BUCK_STEP_10MV, 0x0F, 0x32, 0x19, true,  "MAX20360C"},
	{MAX20360_VARIANT_F,                MAX20360_BUCK_STEP_25MV, MAX20360_BUCK_STEP_25MV, 0x32, 0x32, 0x37, false, "MAX20360F"},
	{MAX20360_VARIANT_G,                MAX20360_BUCK_STEP_10MV, MAX20360_BUCK_STEP_25MV, 0x37, 0x32, 0x32, false, "MAX20360G"},
	{MAX20360_VARIANT_I,                MAX20360_BUCK_STEP_25MV, MAX20360_BUCK_STEP_25MV, 0x0D, 0x32, 0x35, true,  "MAX20360I"},
	{MAX20360_VARIANT_J,                MAX20360_BUCK_STEP_10MV, MAX20360_BUCK_STEP_50MV, 0x0D, 0x32, 0x37, true,  "MAX20360J"},
	{MAX20360_VARIANT_K,                MAX20360_BUCK_STEP_50MV, MAX20360_BUCK_STEP_25MV, 0x37, 0x32, 0x1A, true,  "MAX20360K"},
	{MAX20360_VARIANT_L,                MAX20360_BUCK_STEP_10MV, MAX20360_BUCK_STEP_25MV, 0x0D, 0x32, 0x17, true,  "MAX20360L"},
	{MAX20360_VARIANT_M,                MAX20360_BUCK_STEP_50MV, MAX20360_BUCK_STEP_10MV, 0x0D, 0x03, 0x19, true,  "MAX20360M"},
	{MAX20360_VARIANT_O,                MAX20360_BUCK_STEP_10MV, MAX20360_BUCK_STEP_50MV, 0x0D, 0x32, 0x1D, true,  "MAX20360O"},
	{MAX20360_VARIANT_P,                MAX20360_BUCK_STEP_25MV, MAX20360_BUCK_STEP_25MV, 0x0D, 0x32, 0x1D, false, "MAX20360P"},
};

/* ===== Data Structures ===== */

/* Common parent data (one per PMIC instance) — mutable runtime state */
struct regulator_max20360_common_data {
	const struct device *dev;
	struct k_mutex i2c_lock;
	struct k_work interrupt_work;
	struct gpio_callback gpio_cb;
	max20360_pmic_int_cb_t int_callback;
	enum max20360_variant detected_variant;
	const struct max20360_variant_info *variant_info;
	uint8_t int_status[4];
	bool int_gpio_ready;
	bool lock_inited;
};

/* Common parent config (one per PMIC instance) — const DT / bus only */
struct regulator_max20360_common_config {
	struct i2c_dt_spec bus;
	struct gpio_dt_spec int_gpio;
	struct gpio_dt_spec ivmon_gpio;
	const char *dt_variant_str;
};

/* ===== I2C Helper Functions ===== */

static int max20360_i2c_read(const struct device *dev, uint8_t reg, uint8_t *val)
{
	const struct regulator_max20360_config *cfg = dev->config;
	int ret;

	if (!i2c_is_ready_dt(&cfg->bus)) {
		LOG_ERR("I2C device not ready for %s", dev->name);
		return -ENODEV;
	}

	ret = i2c_reg_read_byte_dt(&cfg->bus, reg, val);
	if (ret) {
		LOG_ERR("I2C read failed: reg=0x%02x ret=%d addr=0x%02x", reg, ret, cfg->bus.addr);
	} else {
		LOG_DBG("I2C read: reg=0x%02x val=0x%02x", reg, *val);
	}

	return ret;
}

static int max20360_i2c_write(const struct device *dev, uint8_t reg, uint8_t val)
{
	const struct regulator_max20360_config *cfg = dev->config;
	int ret;

	if (!i2c_is_ready_dt(&cfg->bus)) {
		LOG_ERR("I2C device not ready for %s", dev->name);
		return -ENODEV;
	}

	LOG_DBG("I2C write: reg=0x%02x val=0x%02x addr=0x%02x", reg, val, cfg->bus.addr);
	ret = i2c_reg_write_byte_dt(&cfg->bus, reg, val);
	if (ret) {
		LOG_ERR("I2C write failed: reg=0x%02x val=0x%02x ret=%d addr=0x%02x", reg, val, ret, cfg->bus.addr);
	}

	return ret;
}

/* Burst write - for CRC writes (register + data + crc) */
static int max20360_i2c_write_burst(const struct device *dev, const uint8_t *buf, size_t len)
{
	const struct regulator_max20360_config *cfg = dev->config;
	struct i2c_msg msg;
	int ret;

	if (!i2c_is_ready_dt(&cfg->bus)) {
		LOG_ERR("I2C bus not ready for %s", dev->name);
		return -ENODEV;
	}

	msg.buf = (uint8_t *)buf;
	msg.len = len;
	msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	ret = i2c_transfer_dt(&cfg->bus, &msg, 1);
	if (ret) {
		LOG_ERR("I2C burst write failed: len=%zu ret=%d", len, ret);
	}

	return ret;
}

/* ===== Get Common Config Helper ===== */

static bool max20360_dev_is_parent(const struct device *dev);

/** Get common config from device (works for both parent and child devices) */
static const struct regulator_max20360_common_config *
max20360_get_common_config(const struct device *dev)
{
	const struct regulator_max20360_config *child_cfg;

	if (dev == NULL || dev->config == NULL) {
		return NULL;
	}

	if (max20360_dev_is_parent(dev)) {
		return dev->config;
	}

	child_cfg = dev->config;
	if (child_cfg->parent_dev != NULL && child_cfg->parent_dev->config != NULL) {
		return child_cfg->parent_dev->config;
	}

	return NULL;
}

static struct regulator_max20360_common_data *
max20360_get_common_data(const struct device *dev)
{
	const struct regulator_max20360_config *child_cfg;

	if (dev == NULL) {
		return NULL;
	}

	if (max20360_dev_is_parent(dev)) {
		return dev->data;
	}

	child_cfg = dev->config;
	if (child_cfg != NULL && child_cfg->parent_dev != NULL) {
		return child_cfg->parent_dev->data;
	}

	return NULL;
}

int max20360_pmic_lock(const struct device *dev)
{
	struct regulator_max20360_common_data *data = max20360_get_common_data(dev);

	if (data == NULL) {
		return -ENODEV;
	}
	if (!data->lock_inited) {
		k_mutex_init(&data->i2c_lock);
		data->lock_inited = true;
	}
	return k_mutex_lock(&data->i2c_lock, K_FOREVER);
}

void max20360_pmic_unlock(const struct device *dev)
{
	struct regulator_max20360_common_data *data = max20360_get_common_data(dev);

	if (data != NULL && data->lock_inited) {
		k_mutex_unlock(&data->i2c_lock);
	}
}

/* ===== Secure Write with Fletcher-16 CRC Support ===== */

/** Calculate Fletcher-16 CRC for secure writes
 * From MAX20360 datasheet page 193:
 * CSUM1 = (SLAVEID + REGADD + DATA) mod 255
 * CSUM2 = (3*SLAVEID + 2*REGADD + DATA) mod 255
 */
static void max20360_fletcher16_crc(uint8_t slave_addr, uint8_t reg, uint8_t data,
				    uint8_t *crc1, uint8_t *crc2)
{
	uint16_t sum1 = (uint16_t)slave_addr + (uint16_t)reg + (uint16_t)data;
	uint16_t sum2 = (3 * (uint16_t)slave_addr) + (2 * (uint16_t)reg) + (uint16_t)data;
	
	*crc1 = (uint8_t)(sum1 % 255);
	*crc2 = (uint8_t)(sum2 % 255);
	
	LOG_DBG("Fletcher-16 CRC: addr=0x%02x reg=0x%02x data=0x%02x -> crc1=0x%02x crc2=0x%02x",
		slave_addr, reg, data, *crc1, *crc2);
}

/** Secure I2C write with Fletcher-16 CRC (when i2ccrcena=1)
 * Checks variant CRC flag and appends CRC if required
 * Works like max20360_i2c_write - uses device's own bus from config
 */
static int max20360_secure_write(const struct device *dev, uint8_t reg, uint8_t val)
{
	struct regulator_max20360_common_data *common_data = max20360_get_common_data(dev);
	const struct regulator_max20360_config *cfg;
	uint8_t crc1, crc2;
	uint8_t write_buf[4]; /* reg + data + CRC1 + CRC2 (Fig 42) */
	bool crc_required = false;
	int ret;

	if (common_data == NULL) {
		LOG_ERR("Failed to get common data for secure write");
		return -ENODEV;
	}

	if (common_data->variant_info) {
		crc_required = common_data->variant_info->i2ccrcena;
	}

	if (!crc_required) {
		return max20360_i2c_write(dev, reg, val);
	}

	cfg = max20360_dev_is_parent(dev) ? NULL : dev->config;
	{
		const struct regulator_max20360_common_config *common_cfg =
			max20360_get_common_config(dev);
		uint8_t slave7 = common_cfg ? common_cfg->bus.addr :
				 (cfg ? cfg->bus.addr : 0);

		/* Datasheet Figure 42: SLAVE_ID is 8-bit write address */
		max20360_fletcher16_crc(slave7 << 1, reg, val, &crc1, &crc2);
	}

	write_buf[0] = reg;
	write_buf[1] = val;
	write_buf[2] = crc1;
	write_buf[3] = crc2;

	ret = max20360_i2c_write_burst(dev, write_buf, 4);
	if (ret != 0) {
		LOG_ERR("Secure write failed: reg=0x%02x val=0x%02x crc1=0x%02x crc2=0x%02x ret=%d",
			reg, val, crc1, crc2, ret);
		if (ret == -EIO || ret == -ENXIO) {
			uint8_t int_status;

			if (max20360_i2c_read(dev, MAX20360_REG_INT3, &int_status) == 0 &&
			    (int_status & MAX20360_INT3_I2CCRCININT)) {
				LOG_ERR("I2C CRC failure (INT3 bit3): 0x%02x", int_status);
			}
		}
		return ret;
	}

	LOG_DBG("Secure write OK: reg=0x%02x val=0x%02x (CRC1+CRC2)", reg, val);
	return 0;
}

/* ===== PMIC Diagnostic and Control Functions ===== */

/* max20360_enable_adc() removed - duplicate of max20360_adc_enable() at line 1276 which has proper unlock/lock */

/** Get chip ID for verification (works with parent device) */
static int max20360_get_chip_id(const struct device *dev, uint8_t *chip_id)
{
	const struct regulator_max20360_common_config *common_cfg = dev->config;
	int ret;

	ret = i2c_reg_read_byte_dt(&common_cfg->bus, MAX20360_REG_CHIPID, chip_id);
	if (ret) {
		LOG_ERR("I2C read failed for chip ID: reg=0x%02x ret=%d", MAX20360_REG_CHIPID, ret);
	} else {
		LOG_DBG("Chip ID read: reg=0x%02x val=0x%02x", MAX20360_REG_CHIPID, *chip_id);
	}

	return ret;
}

/** Simple case-insensitive character comparison */
static int char_tolower(int c)
{
	if (c >= 'A' && c <= 'Z') {
		return c + ('a' - 'A');
	}
	return c;
}

/** Case-insensitive string comparison */
static int str_icmp(const char *s1, const char *s2)
{
	int c1, c2;
	
	do {
		c1 = char_tolower((unsigned char)*s1++);
		c2 = char_tolower((unsigned char)*s2++);
		if (c1 == '\0') {
			break;
		}
	} while (c1 == c2);
	
	return c1 - c2;
}

/** Convert device tree variant string to enum
 * Maps device tree property values to max20360_variant enum
 */
static enum max20360_variant max20360_variant_from_string(const char *variant_str)
{
	if (variant_str == NULL) {
		return MAX20360_VARIANT_UNKNOWN;
	}
	
	/* Case-insensitive string comparison */
	if (str_icmp(variant_str, "ev-kit") == 0) {
		return MAX20360_VARIANT_EV_KIT;
	} else if (str_icmp(variant_str, "ev-kit-harvester") == 0) {
		return MAX20360_VARIANT_EV_KIT_HARVESTER;
	} else if (str_icmp(variant_str, "max20360a") == 0) {
		return MAX20360_VARIANT_A;
	} else if (str_icmp(variant_str, "max20360b") == 0) {
		return MAX20360_VARIANT_B;
	} else if (str_icmp(variant_str, "max20360c") == 0) {
		return MAX20360_VARIANT_C;
	} else if (str_icmp(variant_str, "max20360f") == 0) {
		return MAX20360_VARIANT_F;
	} else if (str_icmp(variant_str, "max20360g") == 0) {
		return MAX20360_VARIANT_G;
	} else if (str_icmp(variant_str, "max20360i") == 0) {
		return MAX20360_VARIANT_I;
	} else if (str_icmp(variant_str, "max20360j") == 0) {
		return MAX20360_VARIANT_J;
	} else if (str_icmp(variant_str, "max20360k") == 0) {
		return MAX20360_VARIANT_K;
	} else if (str_icmp(variant_str, "max20360l") == 0) {
		return MAX20360_VARIANT_L;
	} else if (str_icmp(variant_str, "max20360m") == 0) {
		return MAX20360_VARIANT_M;
	} else if (str_icmp(variant_str, "max20360o") == 0) {
		return MAX20360_VARIANT_O;
	} else if (str_icmp(variant_str, "max20360p") == 0) {
		return MAX20360_VARIANT_P;
	}
	
	return MAX20360_VARIANT_UNKNOWN;
}

/** Find all variants matching VSet values (for debugging ambiguous matches) */
static int max20360_find_matching_variants(uint8_t bk1_vset, uint8_t bk2_vset, uint8_t bk3_vset,
					    enum max20360_variant *matches, int max_matches)
{
	int count = 0;
	
	for (int i = 0; i < ARRAY_SIZE(variant_table) && count < max_matches; i++) {
		if (variant_table[i].bk1_default_vset == bk1_vset &&
		    variant_table[i].bk2_default_vset == bk2_vset &&
		    variant_table[i].bk3_default_vset == bk3_vset) {
			matches[count++] = variant_table[i].variant;
		}
	}
	
	return count;
}

/** Detect MAX20360 variant and configure step sizes
 * Detection methods (in priority order):
 * 1. Device tree override (maxim,variant property) - PRIMARY METHOD
 * 2. VSet register matching (BK1/BK2/BK3 default values) - FALLBACK
 * 3. EV KIT default - LAST RESORT
 */
static enum max20360_variant max20360_detect_variant(const struct device *dev)
{
	const struct regulator_max20360_common_config *common_cfg = dev->config;
	enum max20360_variant variant = MAX20360_VARIANT_UNKNOWN;
	uint8_t bk1_vset = 0, bk2_vset = 0, bk3_vset = 0;
	int ret;
	
	/* Method 1: Check device tree override (PRIMARY) */
	if (common_cfg->dt_variant_str != NULL) {
		variant = max20360_variant_from_string(common_cfg->dt_variant_str);
		if (variant != MAX20360_VARIANT_UNKNOWN) {
			for (int i = 0; i < ARRAY_SIZE(variant_table); i++) {
				if (variant_table[i].variant == variant) {
					LOG_INF("Variant specified in device tree: %s", variant_table[i].name);
					return variant;
				}
			}
		} else {
			LOG_WRN("Invalid variant in device tree: '%s', falling back to VSet detection",
				common_cfg->dt_variant_str);
		}
	}
	
	/* Method 2: VSet register matching (FALLBACK) */
	ret = i2c_reg_read_byte_dt(&common_cfg->bus, MAX20360_REG_BUCK1VSET, &bk1_vset);
	if (ret) {
		LOG_WRN("Failed to read BK1 VSet for variant detection: %d", ret);
		goto fallback;
	}
	
	ret = i2c_reg_read_byte_dt(&common_cfg->bus, MAX20360_REG_BUCK2VSET, &bk2_vset);
	if (ret) {
		LOG_WRN("Failed to read BK2 VSet for variant detection: %d", ret);
		goto fallback;
	}
	
	ret = i2c_reg_read_byte_dt(&common_cfg->bus, MAX20360_REG_BUCK3VSET, &bk3_vset);
	if (ret) {
		LOG_WRN("Failed to read BK3 VSet for variant detection: %d", ret);
		goto fallback;
	}
	
	/* Find all matching variants to detect ambiguous cases */
	enum max20360_variant matches[4];  /* Max 4 variants can match (EV_KIT, EV_KIT_HARVESTER, G) */
	int match_count = max20360_find_matching_variants(bk1_vset, bk2_vset, bk3_vset, matches, 4);
	
	if (match_count > 0) {
		variant = matches[0];  /* Use first match */
		
		if (match_count > 1) {
			/* Ambiguous match - log warning with all possibilities */
			LOG_WRN("Ambiguous VSet match (BK1=0x%02x, BK2=0x%02x, BK3=0x%02x): %d variants match",
				bk1_vset, bk2_vset, bk3_vset, match_count);
			for (int i = 0; i < match_count; i++) {
				for (int j = 0; j < ARRAY_SIZE(variant_table); j++) {
					if (variant_table[j].variant == matches[i]) {
						LOG_WRN("  - %s", variant_table[j].name);
						break;
					}
				}
			}
			/* Find name of first match for warning message */
			const char *first_match_name = "UNKNOWN";
			for (int j = 0; j < ARRAY_SIZE(variant_table); j++) {
				if (variant_table[j].variant == variant) {
					first_match_name = variant_table[j].name;
					break;
				}
			}
			LOG_WRN("Using first match: %s. Consider specifying maxim,variant in device tree.",
				first_match_name);
		} else {
			/* Unique match */
			for (int i = 0; i < ARRAY_SIZE(variant_table); i++) {
				if (variant_table[i].variant == variant) {
					LOG_INF("Detected variant via VSet match: %s (BK1=0x%02x, BK2=0x%02x, BK3=0x%02x)",
						variant_table[i].name, bk1_vset, bk2_vset, bk3_vset);
					break;
				}
			}
		}
		
		return variant;
	}
	
	/* No VSet match found */
	LOG_WRN("No variant match for VSet values (BK1=0x%02x, BK2=0x%02x, BK3=0x%02x)",
		bk1_vset, bk2_vset, bk3_vset);
	
fallback:
	/* Method 3: Fallback to EV KIT (LAST RESORT) */
	LOG_WRN("Variant detection failed, using EV KIT defaults (BK1=0x%02x, BK2=0x%02x, BK3=0x%02x)",
		bk1_vset, bk2_vset, bk3_vset);
	LOG_WRN("Consider specifying maxim,variant property in device tree for accurate detection");
	return MAX20360_VARIANT_EV_KIT;
}

/** Configure regulator voltage ranges based on detected variant */
static void max20360_configure_variant_ranges(enum max20360_variant variant)
{
	const struct max20360_variant_info *info = NULL;
	
	/* Find variant info */
	for (int i = 0; i < ARRAY_SIZE(variant_table); i++) {
		if (variant_table[i].variant == variant) {
			info = &variant_table[i];
			break;
		}
	}
	
	if (!info) {
		LOG_ERR("Invalid variant %d, using EV KIT defaults", variant);
		info = &variant_table[0]; /* EV KIT */
	}
	
	/* Update BK1 voltage range based on step size */
	if (info->bk1_step_uv == MAX20360_BUCK_STEP_10MV) {
		max20360_bk1_desc.voltage_range = &bk1_voltage_range_10mv;
	} else if (info->bk1_step_uv == MAX20360_BUCK_STEP_25MV) {
		max20360_bk1_desc.voltage_range = &bk1_voltage_range_25mv;
	} else if (info->bk1_step_uv == MAX20360_BUCK_STEP_50MV) {
		max20360_bk1_desc.voltage_range = &bk1_voltage_range_50mv;
	}
	
	/* Update BK2 voltage range based on step size */
	if (info->bk2_step_uv == MAX20360_BUCK_STEP_10MV) {
		max20360_bk2_desc.voltage_range = &bk2_voltage_range_10mv;
	} else if (info->bk2_step_uv == MAX20360_BUCK_STEP_25MV) {
		max20360_bk2_desc.voltage_range = &bk2_voltage_range_25mv;
	} else if (info->bk2_step_uv == MAX20360_BUCK_STEP_50MV) {
		max20360_bk2_desc.voltage_range = &bk2_voltage_range_50mv;
	}
	
	LOG_INF("Configured %s: BK1=%d mV step, BK2=%d mV step",
		info->name, info->bk1_step_uv / 1000, info->bk2_step_uv / 1000);
}

/* ===== Interrupt Handling Functions ===== */

/** Read all interrupt status registers */
static int max20360_read_interrupt_status(const struct device *dev, uint8_t int_status[4])
{
	const struct regulator_max20360_common_config *common_cfg = dev->config;
	int ret;
	
	for (int i = 0; i < 4; i++) {
		ret = i2c_reg_read_byte_dt(&common_cfg->bus, MAX20360_REG_INT0 + i, &int_status[i]);
		if (ret) {
			LOG_ERR("Failed to read interrupt status register %d: %d", i, ret);
			return ret;
		}
	}
	
	return 0;
}

/** Configure interrupt masks: datasheet 0=masked, 1=unmasked (not masked). */
static int max20360_configure_interrupt_masks(const struct device *dev,
					      uint8_t mask0, uint8_t mask1,
					      uint8_t mask2, uint8_t mask3)
{
	const struct regulator_max20360_common_config *common_cfg = dev->config;
	int ret;

	ret = i2c_reg_write_byte_dt(&common_cfg->bus, MAX20360_REG_INTMASK0, mask0);
	if (ret) {
		return ret;
	}
	ret = i2c_reg_write_byte_dt(&common_cfg->bus, MAX20360_REG_INTMASK1, mask1);
	if (ret) {
		return ret;
	}
	ret = i2c_reg_write_byte_dt(&common_cfg->bus, MAX20360_REG_INTMASK2, mask2);
	if (ret) {
		return ret;
	}
	ret = i2c_reg_write_byte_dt(&common_cfg->bus, MAX20360_REG_INTMASK3, mask3);
	if (ret) {
		return ret;
	}

	LOG_DBG("Interrupt masks (1=unmasked): 0x%02x 0x%02x 0x%02x 0x%02x",
		mask0, mask1, mask2, mask3);
	return 0;
}

/** Process interrupt in work queue context — clear-on-read (no W1C). */
static void max20360_interrupt_work_handler(struct k_work *work)
{
	struct regulator_max20360_common_data *common_data =
		CONTAINER_OF(work, struct regulator_max20360_common_data, interrupt_work);
	const struct device *dev = common_data->dev;
	uint8_t *int_status = common_data->int_status;

	if (max20360_pmic_lock(dev) != 0) {
		return;
	}

	/* Reading Int0–Int3 clears sticky bits (datasheet §Interrupt). */
	if (max20360_read_interrupt_status(dev, int_status) != 0) {
		LOG_ERR("Failed to read interrupt status");
		max20360_pmic_unlock(dev);
		return;
	}

	if (int_status[0] & MAX20360_INT0_USBOKINT) {
		LOG_INF("USB OK interrupt: CHGIN valid");
	}
	if (int_status[0] & MAX20360_INT0_THMSTATINT) {
		LOG_INF("Temperature status interrupt");
	}
	if (int_status[0] & MAX20360_INT0_CHGTMOINT) {
		LOG_WRN("Charger timeout interrupt");
	}
	if (int_status[2] & MAX20360_INT2_SYSBATLIMINT) {
		LOG_WRN("SysBatLim interrupt: charger limiting VSYS");
	}
	if (int_status[2] & (MAX20360_INT2_THMBK1INT | MAX20360_INT2_THMBK2INT |
			     MAX20360_INT2_THMBK3INT)) {
		LOG_ERR("Buck thermal shutdown: BK1=%d BK2=%d BK3=%d",
			!!(int_status[2] & MAX20360_INT2_THMBK1INT),
			!!(int_status[2] & MAX20360_INT2_THMBK2INT),
			!!(int_status[2] & MAX20360_INT2_THMBK3INT));
	}
	if (int_status[3] & MAX20360_INT3_BSTFAULTINT) {
		LOG_ERR("Boost regulator fault interrupt");
	}
	if (int_status[3] & MAX20360_INT3_BATGOODINT) {
		LOG_INF("Battery good interrupt");
	}

	if (common_data->int_callback) {
		common_data->int_callback(dev, int_status);
	}

	max20360_pmic_unlock(dev);
}

static void max20360_gpio_callback(const struct device *port, struct gpio_callback *cb,
				   gpio_port_pins_t pins)
{
	struct regulator_max20360_common_data *common_data =
		CONTAINER_OF(cb, struct regulator_max20360_common_data, gpio_cb);

	ARG_UNUSED(port);
	ARG_UNUSED(pins);
	k_work_submit(&common_data->interrupt_work);
}

static int max20360_setup_interrupt_gpio(const struct device *dev)
{
	const struct regulator_max20360_common_config *common_cfg = dev->config;
	struct regulator_max20360_common_data *common_data = dev->data;
	int ret;

	if (common_data->int_gpio_ready) {
		return 0;
	}
	if (common_cfg->int_gpio.port == NULL) {
		LOG_DBG("Interrupt GPIO not configured");
		return 0;
	}
	if (!gpio_is_ready_dt(&common_cfg->int_gpio)) {
		LOG_DBG("Interrupt GPIO port not ready");
		return 0;
	}

	common_data->dev = dev;
	k_work_init(&common_data->interrupt_work, max20360_interrupt_work_handler);

	ret = gpio_pin_configure_dt(&common_cfg->int_gpio, GPIO_INPUT);
	if (ret) {
		LOG_ERR("Failed to configure interrupt GPIO: %d", ret);
		return ret;
	}

	gpio_init_callback(&common_data->gpio_cb, max20360_gpio_callback,
			   BIT(common_cfg->int_gpio.pin));
	ret = gpio_add_callback(common_cfg->int_gpio.port, &common_data->gpio_cb);
	if (ret) {
		LOG_ERR("Failed to add GPIO callback: %d", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&common_cfg->int_gpio, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret) {
		LOG_ERR("Failed to configure interrupt: %d", ret);
		return ret;
	}

	/* IntMask: 1 = unmasked. Only enable bits with handlers. */
	ret = max20360_configure_interrupt_masks(dev,
		MAX20360_INT0_USBOKINT | MAX20360_INT0_THMSTATINT | MAX20360_INT0_CHGTMOINT,
		0x00,
		MAX20360_INT2_SYSBATLIMINT | MAX20360_INT2_THMBK1INT |
			MAX20360_INT2_THMBK2INT | MAX20360_INT2_THMBK3INT,
		MAX20360_INT3_BSTFAULTINT | MAX20360_INT3_BATGOODINT);
	if (ret) {
		LOG_WRN("Failed to configure interrupt masks: %d", ret);
	}

	common_data->int_gpio_ready = true;
	LOG_INF("Interrupt GPIO configured on pin %d", common_cfg->int_gpio.pin);
	return 0;
}

int max20360_pmic_int_init(const struct device *dev, max20360_pmic_int_cb_t cb)
{
	struct regulator_max20360_common_data *common_data;

	if (dev == NULL) {
		return -EINVAL;
	}

	common_data = max20360_get_common_data(dev);
	if (common_data == NULL) {
		return -ENODEV;
	}
	if (!device_is_ready(dev)) {
		return -ENODEV;
	}

	common_data->int_callback = cb;
	return max20360_setup_interrupt_gpio(dev);
}

/* ===== PMIC Lock/Unlock Functions (per-regulator VSET/charger) ===== */

/* ===== IVMON Mux Control (exported for adc_max20360) =====
 *
 * IVMONCfg (0x1A) is in the PMIC register map but is not covered by LockMsk
 * (only regulator VSET and charger EOC are lockable).  Plain I2C write is
 * sufficient.  Conversion is done by adc_max20360 on the haptic/ADC slave
 * (7-bit 0x50) via the standard Zephyr ADC API.
 */
int max20360_ivmon_select(const struct device *dev, uint8_t channel, uint8_t ratio)
{
	const struct regulator_max20360_common_config *common_cfg =
		max20360_get_common_config(dev);
	uint8_t ivmoncfg_val;
	int ret;

	if (!common_cfg) {
		return -ENODEV;
	}

	ivmoncfg_val = MAX20360_IVMONCFG_OFFHIZ |
		(channel & MAX20360_IVMONCFG_CNTL_MASK) |
		((ratio & 0x03) << MAX20360_IVMONCFG_RATIO_SHIFT);

	ret = max20360_pmic_lock(dev);
	if (ret) {
		return ret;
	}
	ret = i2c_reg_write_byte_dt(&common_cfg->bus, MAX20360_REG_IVMONCFG, ivmoncfg_val);
	max20360_pmic_unlock(dev);
	if (ret) {
		LOG_ERR("Failed to configure IVMON channel: %d", ret);
	}

	return ret;
}

int max20360_read_status4(const struct device *dev, uint8_t *status4)
{
	const struct regulator_max20360_common_config *common_cfg =
		max20360_get_common_config(dev);
	int ret;

	if (!common_cfg || status4 == NULL) {
		return -EINVAL;
	}

	ret = max20360_pmic_lock(dev);
	if (ret) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&common_cfg->bus, MAX20360_REG_STATUS4, status4);
	max20360_pmic_unlock(dev);
	return ret;
}

int max20360_read_status3(const struct device *dev, uint8_t *status3)
{
	const struct regulator_max20360_common_config *common_cfg =
		max20360_get_common_config(dev);
	int ret;

	if (!common_cfg || status3 == NULL) {
		return -EINVAL;
	}

	ret = max20360_pmic_lock(dev);
	if (ret) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&common_cfg->bus, MAX20360_REG_STATUS3, status3);
	max20360_pmic_unlock(dev);
	return ret;
}

/* ===== PMIC Lock/Unlock Functions ===== */

/* Lock mask bits for each regulator - per MAX20360 datasheet LockMsk (0x83) register */
#define MAX20360_LOCK_BK1       BIT(1)  /* BK1Lck - Buck1 registers */
#define MAX20360_LOCK_BK2       BIT(2)  /* BK2Lck - Buck2 registers */
#define MAX20360_LOCK_BK3       BIT(3)  /* BK3Lck - Buck3 registers */
#define MAX20360_LOCK_BST       BIT(4)  /* BstLck - Boost registers */
#define MAX20360_LOCK_BBOUT     BIT(5)  /* BBLck - Buck-boost registers */
#define MAX20360_LOCK_LDO1      BIT(6)  /* LD1Lck - LDO1 registers */
#define MAX20360_LOCK_LDO2      BIT(7)  /* LD2Lck - LDO2 registers */

static uint8_t get_regulator_lock_mask(enum max20360_regulator_source source)
{
	switch (source) {
	case MAX20360_SOURCE_BK1:
		return MAX20360_LOCK_BK1;
	case MAX20360_SOURCE_BK2:
		return MAX20360_LOCK_BK2;
	case MAX20360_SOURCE_BK3:
		return MAX20360_LOCK_BK3;
	case MAX20360_SOURCE_BBOUT:
		return MAX20360_LOCK_BBOUT;
	case MAX20360_SOURCE_L1OUT:
		return MAX20360_LOCK_LDO1;
	case MAX20360_SOURCE_L2OUT:
		return MAX20360_LOCK_LDO2;
	case MAX20360_SOURCE_LSW1:
		return 0;  /* Load switches don't have dedicated lock bits */
	case MAX20360_SOURCE_LSW2:
		return 0;  /* Load switches don't have dedicated lock bits */
	case MAX20360_SOURCE_BOOST:
		return MAX20360_LOCK_BST;
	case MAX20360_SOURCE_SFOUT:
		return 0;  /* SFOUT doesn't need locking (fixed outputs) */
	default:
		return 0;
	}
}

static int max20360_unlock_regulator(const struct device *dev, enum max20360_regulator_source source)
{
	const struct regulator_max20360_config *cfg = dev->config;
	uint8_t lock_mask, current_mask, verify_status;
	int ret;

	lock_mask = get_regulator_lock_mask(source);
	if (lock_mask == 0) {
		LOG_WRN("No lock mask defined for regulator source %d", source);
		return 0; /* Some regulators might not need locking */
	}

	LOG_DBG("Unlocking regulator source %d (mask=0x%02x)", source, lock_mask);

	/* Read current lock mask */
	ret = i2c_reg_read_byte_dt(&cfg->bus, MAX20360_REG_LOCKMASK, &current_mask);
	if (ret) {
		LOG_ERR("Failed to read current lock mask: %d", ret);
		return ret;
	}
	LOG_INF("LOCKMASK before: 0x%02x", current_mask);

	/* CRITICAL FIX: Write 0x00 to disable ALL lock masks at once
	 * Per README_PMIC_UNLOCK_SOLUTION.md: MUST clear all masks, not individual bits
	 * The hardware requires 0x00 to LOCKMASK before unlock password works */
	current_mask = 0x00;
	LOG_INF("Writing LOCKMASK = 0x00 (disable all locks)");
	ret = max20360_secure_write(dev, MAX20360_REG_LOCKMASK, current_mask);
	if (ret) {
		LOG_ERR("Failed to clear lock mask: %d", ret);
		return ret;
	}

	/* CRITICAL: Wait for register state to settle (hardware requirement) */
	k_msleep(1);

	/* Verify LOCKMASK write succeeded */
	uint8_t verify_mask;
	ret = i2c_reg_read_byte_dt(&cfg->bus, MAX20360_REG_LOCKMASK, &verify_mask);
	if (ret == 0) {
		LOG_INF("LOCKMASK verify read: 0x%02x (expected 0x00)", verify_mask);
		if (verify_mask != 0x00) {
			LOG_ERR("LOCKMASK write failed - hardware still locked at 0x%02x", verify_mask);
			return -EACCES;
		}
	}

	/* Write unlock password (0x55) */
	LOG_INF("Writing UNLOCK password 0x55 to reg 0x84");
	ret = max20360_secure_write(dev, MAX20360_REG_LOCKUNLOCK, MAX20360_UNLOCK_PASSWORD);
	if (ret) {
		LOG_ERR("Failed to write unlock password: %d", ret);
		return ret;
	}

	/* Wait for unlock to process */
	k_msleep(1);

	/* Verify unlock succeeded (register reads 0x00 when unlocked) */
	ret = i2c_reg_read_byte_dt(&cfg->bus, MAX20360_REG_LOCKUNLOCK, &verify_status);
	if (ret != 0) {
		LOG_ERR("Failed to verify unlock status: %d", ret);
		return ret;
	}
	LOG_INF("LOCKUNLOCK verify read: 0x%02x (expected 0x00)", verify_status);
	
	if (verify_status != 0x00) {
		LOG_ERR("Regulator unlock FAILED: status 0x%02x (expected 0x00), source %d", 
			verify_status, source);
		return -EACCES;  /* Access denied - hardware protection active */
	}

	LOG_DBG("Regulator unlock successful: source %d verified", source);
	return 0;
}

static int max20360_lock_regulator(const struct device *dev, enum max20360_regulator_source source)
{
	const struct regulator_max20360_config *cfg = dev->config;
	uint8_t lock_mask = get_regulator_lock_mask(source);
	uint8_t current_mask;
	int ret;

	if (lock_mask == 0) {
		return 0; /* Some regulators might not need locking */
	}

	LOG_DBG("Locking regulator source %d (mask=0x%02x)", source, lock_mask);

	/* Read current lock mask */
	ret = i2c_reg_read_byte_dt(&cfg->bus, MAX20360_REG_LOCKMASK, &current_mask);
	if (ret) {
		LOG_ERR("Failed to read current lock mask: %d", ret);
		return ret;
	}

	/* LockMsk: 0 = not masked from lock/unlock. Clear bit then write 0xAA. */
	current_mask &= (uint8_t)~lock_mask;
	ret = max20360_secure_write(dev, MAX20360_REG_LOCKMASK, current_mask);
	if (ret) {
		LOG_ERR("Failed to clear lock mask bit: %d", ret);
		return ret;
	}

	k_msleep(1);

	ret = max20360_secure_write(dev, MAX20360_REG_LOCKUNLOCK, MAX20360_LOCK_PASSWORD);
	if (ret) {
		LOG_ERR("Failed to write lock password: %d", ret);
		return ret;
	}

	LOG_DBG("Regulator locked successfully: source %d", source);
	return 0;
}

/** ===== Regulator API Implementation ===== */

static int max20360_enable(const struct device *dev)
{
	const struct regulator_max20360_config *cfg = dev->config;
	struct regulator_max20360_data *data = dev->data;
	int ret;
	uint8_t ena_val;

	if (!cfg->desc->ena_reg) {
		LOG_ERR("Regulator %s has no enable register", dev->name);
		return -ENOTSUP;
	}

	ret = max20360_pmic_lock(dev);
	if (ret) {
		return ret;
	}

	/* Unlock -> Operation -> Lock sequence */
	ret = max20360_unlock_regulator(dev, cfg->desc->source);
	if (ret) {
		max20360_pmic_unlock(dev);
		return ret;
	}

	ret = max20360_i2c_read(dev, cfg->desc->ena_reg, &ena_val);
	if (ret) {
		goto lock_and_exit;
	}

	/* BuckEn[1:0]=01 enable when Seq==111; SFOUT uses En field on SFOUTCtrl */
	if (cfg->desc->source == MAX20360_SOURCE_SFOUT) {
		ena_val = (ena_val & ~GENMASK(1, 0)) | 0x01;
	} else {
		ena_val = (ena_val & ~GENMASK(1, 0)) | MAX20360_EN_BIT;
	}

	ret = max20360_i2c_write(dev, cfg->desc->ena_reg, ena_val);
	if (ret) {
		goto lock_and_exit;
	}

	data->enabled = true;
	LOG_INF("Enabled regulator %s", cfg->desc->name);

lock_and_exit:
	max20360_lock_regulator(dev, cfg->desc->source);
	max20360_pmic_unlock(dev);
	return ret;
}

static int max20360_disable(const struct device *dev)
{
	const struct regulator_max20360_config *cfg = dev->config;
	struct regulator_max20360_data *data = dev->data;
	int ret;
	uint8_t ena_val;

	if (!cfg->desc->ena_reg) {
		LOG_ERR("Regulator %s has no enable register", dev->name);
		return -ENOTSUP;
	}

	ret = max20360_pmic_lock(dev);
	if (ret) {
		return ret;
	}

	ret = max20360_unlock_regulator(dev, cfg->desc->source);
	if (ret) {
		max20360_pmic_unlock(dev);
		return ret;
	}

	ret = max20360_i2c_read(dev, cfg->desc->ena_reg, &ena_val);
	if (ret) {
		goto lock_and_exit;
	}

	if (cfg->desc->source == MAX20360_SOURCE_SFOUT) {
		ena_val &= (uint8_t)~GENMASK(1, 0);
	} else {
		ena_val &= (uint8_t)~GENMASK(1, 0);
	}

	ret = max20360_i2c_write(dev, cfg->desc->ena_reg, ena_val);
	if (ret) {
		goto lock_and_exit;
	}

	data->enabled = false;
	LOG_INF("Disabled regulator %s", cfg->desc->name);

lock_and_exit:
	max20360_lock_regulator(dev, cfg->desc->source);
	max20360_pmic_unlock(dev);
	return ret;
}

static int max20360_set_voltage(const struct device *dev, int32_t min_uv, int32_t max_uv)
{
	const struct regulator_max20360_config *cfg = dev->config;
	struct regulator_max20360_data *data = dev->data;
	const struct linear_range *range = cfg->desc->voltage_range;
	uint16_t vset_code;
	int ret;

	if (!cfg->desc->vset_reg || !range) {
		LOG_ERR("Regulator %s does not support voltage setting", cfg->desc->name);
		return -ENOTSUP;
	}

	ret = linear_range_get_win_index(range, min_uv, max_uv, &vset_code);
	if (ret) {
		LOG_ERR("Voltage %d-%d uV out of range for %s (ret=%d)",
			min_uv, max_uv, cfg->desc->name, ret);
		return ret;
	}

	ret = max20360_pmic_lock(dev);
	if (ret) {
		return ret;
	}

	ret = max20360_unlock_regulator(dev, cfg->desc->source);
	if (ret) {
		max20360_pmic_unlock(dev);
		return ret;
	}

	/* SFOUTVSet bit7: 0=5.0 V, 1=3.3 V */
	if (cfg->desc->source == MAX20360_SOURCE_SFOUT) {
		uint8_t sfout_val;

		ret = max20360_i2c_read(dev, cfg->desc->vset_reg, &sfout_val);
		if (ret) {
			goto lock_and_exit;
		}

		if (min_uv <= 4000000) {
			sfout_val |= BIT(7); /* 3.3 V */
		} else {
			sfout_val &= (uint8_t)~BIT(7); /* 5.0 V */
		}

		ret = max20360_i2c_write(dev, cfg->desc->vset_reg, sfout_val);
	} else {
		if (vset_code > 0x3F) {
			ret = -EINVAL;
			goto lock_and_exit;
		}
		ret = max20360_i2c_write(dev, cfg->desc->vset_reg, (uint8_t)vset_code);
	}

	if (ret) {
		goto lock_and_exit;
	}

	data->current_voltage_uv = min_uv;
	LOG_INF("Set %s to %d uV (code 0x%02x)", cfg->desc->name, min_uv, vset_code);
	k_msleep(10);

lock_and_exit:
	max20360_lock_regulator(dev, cfg->desc->source);
	max20360_pmic_unlock(dev);
	return ret;
}

static int max20360_get_voltage(const struct device *dev, int32_t *voltage_uv)
{
	const struct regulator_max20360_config *cfg = dev->config;
	struct regulator_max20360_data *data = dev->data;
	const struct linear_range *range = cfg->desc->voltage_range;
	uint8_t vset_code;
	int ret;

	if (!cfg->desc->vset_reg || !range) {
		LOG_ERR("Regulator %s does not support voltage reading", cfg->desc->name);
		return -ENOTSUP;
	}

	ret = max20360_i2c_read(dev, cfg->desc->vset_reg, &vset_code);
	if (ret) {
		LOG_ERR("Failed to read vset register 0x%02x for %s", cfg->desc->vset_reg, cfg->desc->name);
		return ret;
	}

	LOG_DBG("Read vset_code=0x%02x from reg=0x%02x for %s", vset_code, cfg->desc->vset_reg, cfg->desc->name);

	/* Special handling for SFOUT - extract voltage selection bit */
	if (cfg->desc->source == MAX20360_SOURCE_SFOUT) {
		/* Extract BIT 1: 0=3.3V, 1=5.0V */
		uint8_t raw_reg = vset_code;  /* Save raw register value for logging */
		uint8_t voltage_select = (vset_code >> 1) & 0x01;
		vset_code = voltage_select;  /* Convert to 0 or 1 for linear range */
		LOG_DBG("SFOUT voltage select bit: %d (raw reg=0x%02x)", voltage_select, raw_reg);
	}

	ret = linear_range_get_value(range, vset_code, voltage_uv);
	if (ret) {
		LOG_ERR("Failed to get voltage from code 0x%02x for %s", vset_code, cfg->desc->name);
		return ret;
	}

	data->current_voltage_uv = *voltage_uv;
	LOG_DBG("Read %s voltage: code=0x%02x -> %d uV", cfg->desc->name, vset_code, *voltage_uv);
	return 0;
}

static unsigned int max20360_count_voltages(const struct device *dev)
{
	const struct regulator_max20360_config *cfg = dev->config;
	const struct linear_range *range = cfg->desc->voltage_range;

	if (!range) {
		return 0;
	}

	return linear_range_values_count(range);
}

static int max20360_list_voltage(const struct device *dev, unsigned int idx, int32_t *voltage_uv)
{
	const struct regulator_max20360_config *cfg = dev->config;
	const struct linear_range *range = cfg->desc->voltage_range;
	int ret;

	if (!range || idx >= linear_range_values_count(range)) {
		return -EINVAL;
	}

	ret = linear_range_get_value(range, (uint16_t)idx, voltage_uv);
	return ret;
}

/* Set operating mode (if supported) */
static int max20360_set_mode(const struct device *dev, regulator_mode_t mode)
{
	/* Operating mode control: MAX20360 regulators operate in fixed voltage mode.
	 * The Zephyr regulator API supports mode switching (e.g., LDO vs. BUCK),
	 * but MAX20360 regulators are configured at initialization and don't support
	 * runtime mode switching. Mode is determined by regulator type (LDO/BUCK).
	 */
	ARG_UNUSED(dev);
	ARG_UNUSED(mode);
	return -ENOTSUP;
}

/* Get operating mode */
static int max20360_get_mode(const struct device *dev, regulator_mode_t *mode)
{
	/* Operating mode is fixed based on regulator type (LDO/BUCK) and doesn't
	 * change at runtime. Mode switching is not supported by MAX20360 hardware.
	 */
	ARG_UNUSED(dev);
	ARG_UNUSED(mode);
	return -ENOTSUP;
}

/* Current limit settings for Buck1/Buck2/Buck3: 25mA increments, 0-375mA */
static const int32_t max20360_buck_current_limits_ua[] = {
	0,     25000,  50000,  75000,   /* 0-75mA */
	100000, 125000, 150000, 175000,  /* 100-175mA */
	200000, 225000, 250000, 275000,  /* 200-275mA */
	300000, 325000, 350000, 375000,  /* 300-375mA */
};

/* Count current limits */
static unsigned int max20360_count_current_limits(const struct device *dev)
{
	const struct regulator_max20360_config *cfg = dev->config;
	
	/* Only Buck1, Buck2, Buck3, Buck-Boost, and Boost support current limits */
	switch (cfg->desc->source) {
	case MAX20360_SOURCE_BK1:
	case MAX20360_SOURCE_BK2:
	case MAX20360_SOURCE_BK3:
		return ARRAY_SIZE(max20360_buck_current_limits_ua);
	case MAX20360_SOURCE_BBOUT:
	case MAX20360_SOURCE_BOOST:
		/* Buck-Boost and Boost have more complex current limit settings */
		return ARRAY_SIZE(max20360_buck_current_limits_ua);  /* Simplified for now */
	default:
		return 0;  /* LDOs and load switches don't support current limits */
	}
}

/* List current limit */
static int max20360_list_current_limit(const struct device *dev, unsigned int idx, int32_t *current_ua)
{
	const struct regulator_max20360_config *cfg = dev->config;
	
	/* Only Buck1, Buck2, Buck3, Buck-Boost, and Boost support current limits */
	switch (cfg->desc->source) {
	case MAX20360_SOURCE_BK1:
	case MAX20360_SOURCE_BK2:
	case MAX20360_SOURCE_BK3:
	case MAX20360_SOURCE_BBOUT:
	case MAX20360_SOURCE_BOOST:
		if (idx >= ARRAY_SIZE(max20360_buck_current_limits_ua)) {
			return -EINVAL;
		}
		*current_ua = max20360_buck_current_limits_ua[idx];
		return 0;
	default:
		return -ENOTSUP;
	}
}

/* Set current limit for Buck1/Buck2/Buck3 */
static int max20360_set_buck_current_limit(const struct device *dev, int32_t min_ua, int32_t max_ua)
{
	const struct regulator_max20360_config *cfg = dev->config;
	uint8_t iset_reg;
	uint8_t iset_val;
	uint8_t reg_val;
	int best_idx = 0;
	int best_diff;
	int ret;
	
	/* Determine ISet register based on regulator */
	switch (cfg->desc->source) {
	case MAX20360_SOURCE_BK1:
		iset_reg = MAX20360_REG_BUCK1ISET;
		break;
	case MAX20360_SOURCE_BK2:
		iset_reg = MAX20360_REG_BUCK2ISET;
		break;
	case MAX20360_SOURCE_BK3:
		iset_reg = MAX20360_REG_BUCK3ISET;
		break;
	default:
		return -ENOTSUP;
	}
	
	/* Find closest current setting */
	best_diff = abs(min_ua - max20360_buck_current_limits_ua[0]);
	for (int i = 1; i < ARRAY_SIZE(max20360_buck_current_limits_ua); i++) {
		int diff = abs(min_ua - max20360_buck_current_limits_ua[i]);
		if (diff < best_diff) {
			best_diff = diff;
			best_idx = i;
		}
	}
	
	iset_val = best_idx & 0x0F;  /* Lower 4 bits */
	
	/* Unlock -> Operation -> Lock sequence */
	ret = max20360_unlock_regulator(dev, cfg->desc->source);
	if (ret) {
		return ret;
	}
	
	/* Read current ISet register */
	ret = max20360_i2c_read(dev, iset_reg, &reg_val);
	if (ret) {
		goto lock_and_exit;
	}
	
	/* Set manual current limit mode (BIT 7 = 1) and current value (BITS 3:0) */
	reg_val = (reg_val & 0xF0) | (iset_val & 0x0F);  /* Preserve upper bits, set lower 4 */
	reg_val |= BIT(7);  /* Enable manual mode (disable lookup table) */
	
	ret = max20360_i2c_write(dev, iset_reg, reg_val);
	if (ret) {
		goto lock_and_exit;
	}
	
	LOG_INF("Set %s current limit to %d uA (code 0x%02x)", cfg->desc->name,
		max20360_buck_current_limits_ua[best_idx], iset_val);
	
lock_and_exit:
	max20360_lock_regulator(dev, cfg->desc->source);
	return ret;
}

/* Set current limit */
static int max20360_set_current_limit(const struct device *dev, int32_t min_ua, int32_t max_ua)
{
	const struct regulator_max20360_config *cfg = dev->config;
	
	switch (cfg->desc->source) {
	case MAX20360_SOURCE_BK1:
	case MAX20360_SOURCE_BK2:
	case MAX20360_SOURCE_BK3:
		return max20360_set_buck_current_limit(dev, min_ua, max_ua);
	case MAX20360_SOURCE_BBOUT:
	case MAX20360_SOURCE_BOOST:
		/* Buck-Boost and Boost have voltage-dependent automatic current limits.
		 * Per MAX20360 datasheet register 0x43 (BBstISet):
		 * Hardware automatically adjusts inductor peak current based on VBBOUT.
		 * Manual override via BBstISet[3:0] not recommended - automatic mode is optimal.
		 * Current limits range: 50mA (VBBOUT≤2.65V) to 250mA (VBBOUT>5.35V).
		 * See register documentation lines 190-212 for complete voltage-current table.
		 */
		LOG_DBG("%s uses automatic voltage-dependent current limiting", cfg->desc->name);
		return -ENOTSUP;  /* Feature not needed - hardware handles automatically */
	default:
		return -ENOTSUP;  /* LDOs and load switches don't support current limits */
	}
}

/* Get current limit */
static int max20360_get_current_limit(const struct device *dev, int32_t *current_ua)
{
	const struct regulator_max20360_config *cfg = dev->config;
	uint8_t iset_reg;
	uint8_t reg_val;
	uint8_t iset_code;
	int ret;
	
	/* Determine ISet register based on regulator */
	switch (cfg->desc->source) {
	case MAX20360_SOURCE_BK1:
		iset_reg = MAX20360_REG_BUCK1ISET;
		break;
	case MAX20360_SOURCE_BK2:
		iset_reg = MAX20360_REG_BUCK2ISET;
		break;
	case MAX20360_SOURCE_BK3:
		iset_reg = MAX20360_REG_BUCK3ISET;
		break;
	default:
		return -ENOTSUP;
	}
	
	ret = max20360_i2c_read(dev, iset_reg, &reg_val);
	if (ret) {
		return ret;
	}
	
	/* Check if manual mode is enabled (BIT 7) */
	if (!(reg_val & BIT(7))) {
		/* Lookup table mode - current depends on voltage, return -ENOTSUP */
		LOG_DBG("%s using automatic lookup table mode", cfg->desc->name);
		return -ENOTSUP;
	}
	
	/* Extract current code from lower 4 bits */
	iset_code = reg_val & 0x0F;
	if (iset_code >= ARRAY_SIZE(max20360_buck_current_limits_ua)) {
		return -EIO;
	}
	
	*current_ua = max20360_buck_current_limits_ua[iset_code];
	LOG_DBG("Read %s current limit: %d uA (code 0x%02x)", cfg->desc->name, *current_ua, iset_code);
	return 0;
}

/* Set active discharge */
static int max20360_set_active_discharge(const struct device *dev, bool enable)
{
	/* Active discharge: Allows regulator output to discharge when disabled.
	 * MAX20360 datasheet should be consulted to verify if this feature is
	 * supported via register control. If supported, implementation would modify
	 * regulator control register bits to enable/disable discharge path.
	 * Currently not implemented pending hardware verification.
	 */
	ARG_UNUSED(dev);
	ARG_UNUSED(enable);
	return -ENOTSUP;
}

/* Get active discharge */
static int max20360_get_active_discharge(const struct device *dev, bool *enabled)
{
	/* Read active discharge status from regulator control register.
	 * Requires hardware verification to determine if MAX20360 supports
	 * this feature and which register/bit controls it.
	 */
	ARG_UNUSED(dev);
	ARG_UNUSED(enabled);
	return -ENOTSUP;
}

/* Get error flags */
static int max20360_get_error_flags(const struct device *dev, regulator_error_flags_t *flags)
{
	const struct regulator_max20360_config *cfg = dev->config;
	uint8_t status_val;
	int ret;

	if (!cfg->desc->status_reg) {
		LOG_ERR("Regulator %s has no status register", dev->name);
		return -ENOTSUP;
	}

	ret = max20360_i2c_read(dev, cfg->desc->status_reg, &status_val);
	if (ret) {
		return ret;
	}

	*flags = 0;

	/* Check for common fault conditions - use generic bits for now */
	if (status_val & MAX20360_STAT_UVLO) {
		*flags |= BIT(0); /* Under voltage */
	}
	if (status_val & MAX20360_FAULT_MASK) {
		*flags |= BIT(1); /* Over current/fault */
	}

	LOG_DBG("Read %s error flags: status=0x%02x flags=0x%08x", 
		cfg->desc->name, status_val, *flags);
	return 0;
}

/* Power off / Ship mode via PwrCmd (0x80). Device may lose MCU power after accept. */
static int max20360_power_off(const struct device *dev)
{
	const struct regulator_max20360_config *cfg = dev->config;
	uint8_t cmd;
	uint8_t verify_status;
	int ret;

#if defined(CONFIG_REGULATOR_MAX20360_SHIP_USE_SEAL)
	cmd = MAX20360_PWR_SEAL_CMD;
#else
	cmd = MAX20360_PWR_OFF_CMD;
#endif

	LOG_WRN("MAX20360 ship_mode: unlocking then PwrCmd=0x%02x (%s)",
		cmd, (cmd == MAX20360_PWR_SEAL_CMD) ? "SEAL" : "OFF");

	/* Global unlock: clear all lock masks then password (same as rail unlock). */
	ret = max20360_secure_write(dev, MAX20360_REG_LOCKMASK, 0x00);
	if (ret) {
		LOG_ERR("ship_mode: LOCKMASK clear failed: %d", ret);
		return ret;
	}
	k_msleep(1);

	ret = max20360_secure_write(dev, MAX20360_REG_LOCKUNLOCK,
				    MAX20360_UNLOCK_PASSWORD);
	if (ret) {
		LOG_ERR("ship_mode: unlock password failed: %d", ret);
		return ret;
	}
	k_msleep(1);

	ret = i2c_reg_read_byte_dt(&cfg->bus, MAX20360_REG_LOCKUNLOCK, &verify_status);
	if (ret == 0 && verify_status != 0x00) {
		LOG_ERR("ship_mode: unlock verify 0x%02x (expected 0x00)", verify_status);
		return -EACCES;
	}

	ret = max20360_secure_write(dev, MAX20360_REG_PWRCMD, cmd);
	if (ret) {
		LOG_ERR("ship_mode: PwrCmd write failed: %d", ret);
		return ret;
	}

	/* If we return, rails may still be collapsing — treat write success as OK. */
	return 0;
}

/* ===== Regulator Initialization ===== */

static int max20360_regulator_init(const struct device *dev)
{
	struct regulator_max20360_config *cfg = (struct regulator_max20360_config *)dev->config;
	int ret;

	if (!i2c_is_ready_dt(&cfg->bus)) {
		LOG_WRN("I2C bus not ready for %s", dev->name);
		return -ENODEV;
	}

	/* Parent device pointer is set at compile time via DEVICE_DT_GET macro */
	if (cfg->parent_dev != NULL) {
		if (device_is_ready(cfg->parent_dev)) {
			LOG_DBG("%s: Parent device ready at I2C addr 0x%02x", 
				dev->name, cfg->bus.addr);
		} else {
			LOG_WRN("%s: Parent device not ready yet", dev->name);
		}
	} else {
		LOG_ERR("%s: Parent device not configured in device tree", dev->name);
		return -ENODEV;
	}

	LOG_INF("MAX20360 %s: I2C slave addr=0x%02x", 
		cfg->desc->name, cfg->bus.addr);

	/* Initialize common regulator data (enable tracking, voltage tracking, etc.) */
	regulator_common_data_init(dev);

	/* ===== POWER CYCLE LOGIC ===== */
	/* If power-cycle-on-init is set, perform clean power cycle before normal init.
	 * This helps peripherals that get stuck in reset state (e.g., MAX32664 sensor hub).
	 * Sequence: Disable → Wait → Configure voltage → Enable
	 */
	if (cfg->power_cycle_on_init) {
		LOG_INF("%s: Performing power cycle (delay: %u ms)", 
			cfg->desc->name, cfg->power_cycle_delay_ms);
		
		/* Step 1: Disable regulator to ensure clean power-down */
		ret = max20360_disable(dev);
		if (ret && ret != -EALREADY) {  /* -EALREADY means already disabled, which is OK */
			LOG_WRN("%s: Disable during power cycle failed: %d (continuing anyway)", 
				cfg->desc->name, ret);
			/* Don't fail init - continue with power cycle */
		}
		
		/* Step 2: Wait for power-down settling (allows peripheral to fully reset) */
		k_sleep(K_MSEC(cfg->power_cycle_delay_ms));
		
		/* Step 3: Configure voltage if specified in device tree */
		if (cfg->common.init_uv > 0) {
			ret = max20360_set_voltage(dev, cfg->common.init_uv, cfg->common.init_uv);
			if (ret) {
				LOG_ERR("%s: Voltage config during power cycle failed: %d", 
					cfg->desc->name, ret);
				return ret;  /* Fail init if voltage setting fails */
			}
			LOG_DBG("%s: Voltage configured to %d.%03dV", 
				cfg->desc->name, 
				cfg->common.init_uv / 1000000, 
				(cfg->common.init_uv % 1000000) / 1000);
		}
		
		/* Step 4: Re-enable regulator (clean power-on) */
		ret = max20360_enable(dev);
		if (ret) {
			LOG_ERR("%s: Enable during power cycle failed: %d", cfg->desc->name, ret);
			return ret;
		}
		
		LOG_INF("%s: Power cycle complete - peripheral should be reset", cfg->desc->name);
		
		/* Skip regulator_common_init() since we already handled voltage and enable */
		return 0;
	}

	/* ===== NORMAL INITIALIZATION (no power cycle) ===== */
	/* Now that PMIC unlock sequence is implemented in common_init, we can safely
	 * call regulator_common_init() to handle boot-on voltages and other initialization
	 */
	ret = regulator_common_init(dev, false);
	if (ret) {
		LOG_ERR("Failed to initialize common regulator data for %s: %d", cfg->desc->name, ret);
		return ret;
	}

	LOG_INF("Initialized MAX20360 regulator %s with common framework", cfg->desc->name);
	return 0;
}

/** CRITICAL: MAX20360 Per-Regulator Unlock/Lock Sequence
 * The MAX20360 has write protection that locks regulator control registers by default.
 * Each regulator operation requires unlock -> operation -> lock sequence.
 */

static int max20360_regulator_common_init(const struct device *dev)
{
	const struct regulator_max20360_common_config *common_cfg = dev->config;
	struct regulator_max20360_common_data *common_data = dev->data;
	uint8_t chip_id;
	enum max20360_variant variant;
	int ret;

	if (!common_data->lock_inited) {
		k_mutex_init(&common_data->i2c_lock);
		common_data->lock_inited = true;
	}
	common_data->dev = dev;

	if (!i2c_is_ready_dt(&common_cfg->bus)) {
		LOG_DBG("I2C bus not ready yet, deferring MAX20360 initialization");
		return -EAGAIN;
	}

	ret = max20360_get_chip_id(dev, &chip_id);
	if (ret) {
		if (ret == -ENODEV) {
			LOG_DBG("I2C operation failed with ENODEV, deferring initialization");
			return -EAGAIN;
		}
		LOG_ERR("Failed to read MAX20360 chip ID: %d", ret);
		return ret;
	}

	k_msleep(10);

	variant = max20360_detect_variant(dev);
	common_data->detected_variant = variant;

	for (int i = 0; i < ARRAY_SIZE(variant_table); i++) {
		if (variant_table[i].variant == variant) {
			common_data->variant_info = &variant_table[i];
			break;
		}
	}

	max20360_configure_variant_ranges(variant);

	LOG_INF("MAX20360 PMIC initialized - Chip ID: 0x%02x, Variant: %s",
		chip_id, common_data->variant_info ? common_data->variant_info->name : "UNKNOWN");
	return 0;
}

/* ===== Device Registration ===== */

/* API for child regulators */
static DEVICE_API(regulator, max20360_regulator_api) = {
	.enable = max20360_enable,
	.disable = max20360_disable,
	.set_voltage = max20360_set_voltage,
	.get_voltage = max20360_get_voltage,
	.set_mode = max20360_set_mode,
	.get_mode = max20360_get_mode,
	.count_voltages = max20360_count_voltages,
	.list_voltage = max20360_list_voltage,
	.count_current_limits = max20360_count_current_limits,
	.list_current_limit = max20360_list_current_limit,
	.set_current_limit = max20360_set_current_limit,
	.get_current_limit = max20360_get_current_limit,
	.set_active_discharge = max20360_set_active_discharge,
	.get_active_discharge = max20360_get_active_discharge,
	.get_error_flags = max20360_get_error_flags,
};

/* API for parent device - ship mode support */
static DEVICE_API(regulator_parent, max20360_parent_api) = {
	.ship_mode = max20360_power_off,
};

static bool max20360_dev_is_parent(const struct device *dev)
{
	return dev != NULL && dev->api == &max20360_parent_api;
}

/* Child regulator registration macro */
#define REGULATOR_MAX20360_DEFINE(node_id, id, child_name, _source)  \
	static struct regulator_max20360_config                              \
	regulator_max20360_config_##id = {                                         \
		.common = REGULATOR_DT_COMMON_CONFIG_INIT(node_id),                \
		.bus = I2C_DT_SPEC_GET(DT_GPARENT(node_id)),                      \
		.desc = &max20360_##child_name##_desc,                            \
		.source = MAX20360_SOURCE_##_source,                              \
		.parent_dev = DEVICE_DT_GET(DT_PARENT(node_id)),  /* Parent at compile time */ \
		.power_cycle_on_init = DT_PROP_OR(node_id, maxim_power_cycle_on_init, false), \
		.power_cycle_delay_ms = DT_PROP_OR(node_id, maxim_power_cycle_delay_ms, 100), \
	};                                                                         \
	static struct regulator_max20360_data regulator_max20360_data_##id;        \
	DEVICE_DT_DEFINE(node_id, max20360_regulator_init, NULL,                  \
			 &regulator_max20360_data_##id,                            \
			 &regulator_max20360_config_##id,                          \
			 POST_KERNEL,                                              \
			 CONFIG_REGULATOR_MAX20360_INIT_PRIORITY,                  \
			 &max20360_regulator_api);

/* Conditional child registration - only if node exists */
#define REGULATOR_MAX20360_DEFINE_COND(inst, child, source)           \
	COND_CODE_1(DT_NODE_EXISTS(DT_INST_CHILD(inst, child)),            \
		    (REGULATOR_MAX20360_DEFINE(DT_INST_CHILD(inst, child),    \
					       child##inst, child, source)),   \
		    ())

/* Parent PMIC device registration - called once per PMIC instance */
#define REGULATOR_MAX20360_DEFINE_ALL(inst)                            \
	static struct regulator_max20360_common_data                        \
	regulator_max20360_common_data_##inst;                              \
	static struct regulator_max20360_common_config                      \
	regulator_max20360_common_config_##inst = {                        \
		.bus = I2C_DT_SPEC_GET(DT_INST_PARENT(inst)),                 \
		.int_gpio = GPIO_DT_SPEC_GET_OR(DT_INST_PARENT(inst), interrupt_gpios, {0}), \
		.ivmon_gpio = GPIO_DT_SPEC_GET_OR(DT_INST_PARENT(inst), ivmon_gpios, {0}), \
		.dt_variant_str = COND_CODE_1(DT_PROP_HAS_IDX(DT_INST_PARENT(inst), maxim_variant, 0), \
			(DT_PROP(DT_INST_PARENT(inst), maxim_variant)), \
			(NULL)), \
	};                                                                  \
	DEVICE_DT_INST_DEFINE(inst, max20360_regulator_common_init, NULL,  \
			      &regulator_max20360_common_data_##inst,      \
			      &regulator_max20360_common_config_##inst,    \
			      POST_KERNEL,                                      \
			      CONFIG_REGULATOR_MAX20360_COMMON_INIT_PRIORITY,   \
			      &max20360_parent_api);                            \
	REGULATOR_MAX20360_DEFINE_COND(inst, bk1, BK1)                     \
	REGULATOR_MAX20360_DEFINE_COND(inst, bk2, BK2)                     \
	REGULATOR_MAX20360_DEFINE_COND(inst, bk3, BK3)                     \
	REGULATOR_MAX20360_DEFINE_COND(inst, bbout, BBOUT)                 \
	REGULATOR_MAX20360_DEFINE_COND(inst, l1out, L1OUT)                 \
	REGULATOR_MAX20360_DEFINE_COND(inst, l2out, L2OUT)                 \
	REGULATOR_MAX20360_DEFINE_COND(inst, lsw1, LSW1)                   \
	REGULATOR_MAX20360_DEFINE_COND(inst, lsw2, LSW2)                   \
	REGULATOR_MAX20360_DEFINE_COND(inst, boost, BOOST)                 \
	REGULATOR_MAX20360_DEFINE_COND(inst, sfout, SFOUT)

/* Instantiate regulators for all enabled MAX20360 PMIC devices */
DT_INST_FOREACH_STATUS_OKAY(REGULATOR_MAX20360_DEFINE_ALL)
