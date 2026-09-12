# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../include)

zephyr_library_sources_ifdef(CONFIG_MAX3010X_PPG max3010x_ppg.c)
