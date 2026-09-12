/**
 * @file i2c_probe.h
 * @brief I2C bus probing utility header
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Probe all I2C buses and log detected devices
 * 
 * Scans I2C0 and I2C1 for all connected devices and logs their addresses.
 * Useful for hardware verification during initialization.
 * 
 * @return 0 on success, negative errno on error
 */
int i2c_probe_all_buses(void);

#ifdef __cplusplus
}
#endif
