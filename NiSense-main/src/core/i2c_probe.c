/**
 * @file i2c_probe.c
 * @brief I2C bus probing utility for device detection
 * 
 * Scans I2C buses at initialization to detect all connected devices.
 * Useful for hardware verification and debugging.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(i2c_probe, LOG_LEVEL_INF);

/**
 * @brief Probe I2C bus for all devices
 * 
 * Scans all possible 7-bit I2C addresses (0x03 to 0x77) and logs
 * which addresses respond. Reserved addresses are skipped.
 * Uses WRITE operation (0-byte write) for faster probing.
 * 
 * @param dev I2C device pointer
 * @param bus_name Human-readable bus name for logging
 * @return Number of devices found, or negative errno on error
 */
static int probe_i2c_bus(const struct device *dev, const char *bus_name)
{
	int found_count = 0;
	int ret;
	
	if (!device_is_ready(dev)) {
		LOG_ERR("%s: I2C bus not ready", bus_name);
		return -ENODEV;
	}
	
	LOG_INF("=== Scanning %s ===", bus_name);
	
	/* Scan all valid 7-bit I2C addresses
	 * Skip reserved ranges:
	 * - 0x00-0x07: Reserved
	 * - 0x78-0x7F: Reserved
	 */
	for (uint16_t addr = 0x08; addr < 0x78; addr++) {
		/* Use 0-byte write for faster probing (no data transfer, just ACK check)
		 * This is faster than read operations as it doesn't wait for data
		 */
		struct i2c_msg msg = {
			.buf = NULL,
			.len = 0,
			.flags = I2C_MSG_WRITE | I2C_MSG_STOP
		};
		
		ret = i2c_transfer(dev, &msg, 1, addr);
		
		if (ret == 0) {
			/* Device responded - try to identify known devices */
			const char *device_name = "Unknown";
			
			if (addr == 0x28) device_name = "MAX20360 PMIC";
			else if (addr == 0x36) device_name = "MAX20360 Fuel Gauge";
			else if (addr == 0x38) device_name = "FT6336 Touch";
			else if (addr == 0x48) device_name = "MAX30205 Temperature";
			else if (addr == 0x50) device_name = "MAX20360 Charger Control";
			else if (addr == 0x51) device_name = "MAX302xx Driver (AD0=1)";
			else if (addr == 0x52) device_name = "MAX302xx Driver (GPIO0=0)";
			else if (addr == 0x53) device_name = "MAX30208 Temperature";
			else if (addr == 0x55) device_name = "MAX32664 Sensor Hub";
			else if (addr == 0x60) device_name = "VCNL3040 Proximity";
			else if (addr == 0x6F) device_name = "MCP7940N RTC";
			
			LOG_INF("  Found device at address 0x%02X (%s)", addr, device_name);
			found_count++;
		}
	}
	
	if (found_count == 0) {
		LOG_WRN("%s: No devices found", bus_name);
	} else {
		LOG_INF("%s: Found %d device(s)", bus_name, found_count);
	}
	
	LOG_INF("=== End %s scan ===\n", bus_name);
	
	return found_count;
}

/**
 * @brief Probe all I2C buses used in the system
 * 
 * Scans both I2C0 and I2C1 buses and logs all detected devices.
 * Should be called early in initialization for hardware verification.
 * 
 * @return 0 on success, negative errno on error
 */
int i2c_probe_all_buses(void)
{
	const struct device *i2c0;
	int ret;
	
	LOG_INF("========================================");
	LOG_INF("Starting I2C Bus Scan");
	LOG_INF("========================================");
	
	/* Get I2C0 device (P0.13 SDA, P0.14 SCL)
	 * Expected devices:
	 * - 0x28: MAX20360 PMIC
	 * - 0x36: MAX20360 Fuel Gauge
	 * - 0x38: FT6336 Touch Controller
	 * - 0x48: MAX30205 Temperature Sensor (fixed address)
	 * - 0x53: MAX30208 Temperature Sensor (POR latched address)
	 * - 0x6F: MCP7940N RTC
	 */
	i2c0 = DEVICE_DT_GET(DT_NODELABEL(i2c0));
	ret = probe_i2c_bus(i2c0, "I2C0 (P0.13/P0.14)");
	if (ret < 0) {
		LOG_ERR("I2C0 probe failed: %d", ret);
		/* Continue to I2C1 even if I2C0 fails */
	}
	
	/* Get I2C1 device (P0.16 SDA, P0.15 SCL)
	 * Expected devices:
	 * - 0x55: MAX32664 Sensor Hub (GWEC or GWED variant)
	 * Note: NLSX5004 level shifter requires 1.8V (Buck2) to be enabled
	 */
#if 1 
	const struct device *i2c1;
	i2c1 = DEVICE_DT_GET(DT_NODELABEL(i2c1));
	ret = probe_i2c_bus(i2c1, "I2C1 (P0.16/P0.15)");
	if (ret < 0) {
		LOG_ERR("I2C1 probe failed: %d", ret);
	}
#endif
	LOG_INF("========================================");
	LOG_INF("I2C Bus Scan Complete");
	LOG_INF("========================================\n");
	
	return 0;
}
