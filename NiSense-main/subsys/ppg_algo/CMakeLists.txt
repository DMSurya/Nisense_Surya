# SPDX-License-Identifier: Apache-2.0

# PPG Algorithm Subsystem

if(CONFIG_PPG_ALGO)

zephyr_library()

# Core subsystem orchestrator
zephyr_library_sources(ppg_algo.c)

# Sample buffer management
zephyr_library_sources(ppg_samples.c)

# Processing pipeline components
zephyr_library_sources(ppg_preprocessing.c)
zephyr_library_sources(ppg_peak_detect.c)
zephyr_library_sources(ppg_quality.c)

# Algorithm implementations
zephyr_library_sources_ifdef(CONFIG_PPG_ALGO_HR_ENABLED ppg_hr_calc.c)
zephyr_library_sources_ifdef(CONFIG_PPG_ALGO_SPO2_ENABLED ppg_spo2_calc.c)
zephyr_library_sources_ifdef(CONFIG_PPG_ALGO_HB_ENABLED ppg_hb_calc.c)
zephyr_library_sources_ifdef(CONFIG_PPG_ALGO_RESP_ENABLED ppg_resp_calc.c)
# HRV (SDNN/RMSSD) + cuffless BP — always available when PPG algo is on
zephyr_library_sources(ppg_hrv_bp_calc.c)
zephyr_library_sources(ppg_spreadsheet_hrv_bp.c)

# Trigger mechanism
zephyr_library_sources(ppg_triggers.c)

# Shell commands
zephyr_library_sources_ifdef(CONFIG_PPG_ALGO_SHELL ppg_shell.c)

# Include directories
zephyr_library_include_directories(.)
zephyr_library_include_directories(../../include)
# App thematic headers (rtc, led_status, config_manager, health_sched, …)
zephyr_library_include_directories(../../src)
zephyr_library_include_directories(../../src/core)
zephyr_library_include_directories(../../src/storage)
zephyr_library_include_directories(../../src/sensors)

endif()
