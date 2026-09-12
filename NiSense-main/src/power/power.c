#include "power.h"

LOG_MODULE_REGISTER(power, LOG_LEVEL_DBG);

/* Aliases from board overlay */
#define BK1_NODE DT_ALIAS(bk1)
#define BK2_NODE DT_ALIAS(bk2)
#define BBOUT_NODE DT_ALIAS(bbout)

int power_init(void)
{
    int ret = 0;
    int32_t voltage_uv = 0;

#if DT_NODE_EXISTS(BK1_NODE)
    const struct device *bk1 = DEVICE_DT_GET(BK1_NODE);
    if (!device_is_ready(bk1)) {
        LOG_ERR("Buck1 not ready");
        return -ENODEV;
    }

    LOG_INF("Configuring Buck1 to 1.2V");
    ret = regulator_set_voltage(bk1, 1200000, 1200000);
    if (!ret) ret = regulator_enable(bk1);
    if (ret) {
        LOG_ERR("Buck1 config failed: %d", ret);
        return ret;
    }
    if (!regulator_get_voltage(bk1, &voltage_uv))
        LOG_INF("Buck1 OK: %d.%03dV",
                voltage_uv / 1000000, (voltage_uv % 1000000) / 1000);
#else
    LOG_WRN("Buck1 alias missing");
#endif

#if DT_NODE_EXISTS(BK2_NODE)
    const struct device *bk2 = DEVICE_DT_GET(BK2_NODE);
    if (!device_is_ready(bk2)) {
        LOG_ERR("Buck2 not ready");
        return -ENODEV;
    }

    LOG_INF("Configuring Buck2 to 1.8V");
    ret = regulator_set_voltage(bk2, 1800000, 1800000);
    if (!ret) ret = regulator_enable(bk2);
    if (ret) {
        LOG_ERR("Buck2 config failed: %d", ret);
        return ret;
    }
    if (!regulator_get_voltage(bk2, &voltage_uv))
        LOG_INF("Buck2 OK: %d.%03dV",
                voltage_uv / 1000000, (voltage_uv % 1000000) / 1000);
#else
    LOG_WRN("Buck2 alias missing");
#endif

#if DT_NODE_EXISTS(BBOUT_NODE)
    const struct device *bbout = DEVICE_DT_GET(BBOUT_NODE);
    if (!device_is_ready(bbout)) {
        LOG_ERR("Buck-Boost not ready");
        return -ENODEV;
    }

    LOG_INF("Configuring Buck-Boost to 5.0V");
    ret = regulator_set_voltage(bbout, 5000000, 5000000);
    if (!ret) ret = regulator_enable(bbout);
    if (ret) {
        LOG_ERR("Buck-Boost config failed: %d", ret);
        return ret;
    }
    if (!regulator_get_voltage(bbout, &voltage_uv))
        LOG_INF("BuckBoost OK: %d.%03dV",
                voltage_uv / 1000000, (voltage_uv % 1000000) / 1000);
#else
    LOG_WRN("Buck-Boost alias missing");
#endif

	k_sleep(K_MSEC(500));
    LOG_INF("All MAX20360 rails initialized successfully");
    return 0;
}

#if defined(CONFIG_FUEL_GAUGE)
void power_test(void) 
{
    fuel_gauge_prop_t prop = FUEL_GAUGE_VOLTAGE;
    union fuel_gauge_prop_val val;

    const struct device *fg_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(fuelgauge));
    if (fg_dev == NULL) {
        LOG_ERR("Fuel gauge device not found in device tree");
        return;
    }
    
    if (!device_is_ready(fg_dev)) {
        LOG_ERR("Fuel gauge device not ready");
        return;
    }

    if (fuel_gauge_get_prop(fg_dev, prop, &val)) {
        LOG_ERR("Failed to read fuel gauge property");
        return;
    }

    LOG_INF("Fuel Gauge Voltage: %d mV", val.voltage / 1000);
}
#endif  /* CONFIG_FUEL_GAUGE */