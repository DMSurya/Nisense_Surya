# SPDX-License-Identifier: Apache-2.0

add_subdirectory_ifdef(CONFIG_GLUCOSE_SENSOR glucose)
add_subdirectory_ifdef(CONFIG_MAX32664 max32664)
add_subdirectory_ifdef(CONFIG_MAX302XX max302xx)
add_subdirectory_ifdef(CONFIG_VCNL3040 vcnl3040)
add_subdirectory_ifdef(CONFIG_MAX3010X_PPG max3010x)
# ppg_algo moved to subsys/ppg_algo (Zephyr subsystem)

