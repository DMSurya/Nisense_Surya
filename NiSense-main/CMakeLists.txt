cmake_minimum_required(VERSION 3.20.0)

# Acknowledge NCS_TOOLCHAIN_VERSION if set (from nRF Connect SDK)
# This variable is set by nRF Connect extension but not used by Zephyr
# Marking it as used suppresses CMake warning
if(NCS_TOOLCHAIN_VERSION)
  # Variable acknowledged, no action needed
endif()

# CRITICAL: Tell Zephyr device tree compiler where to find custom bindings
# This must be set BEFORE find_package(Zephyr)
list(APPEND DTS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/dts")

# Add custom drivers to Zephyr's driver path
list(APPEND ZEPHYR_EXTRA_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/drivers")

# Add custom subsystems to Zephyr's subsystem path
list(APPEND ZEPHYR_EXTRA_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/subsys")

# Overlay fallback logging control.
# Keep normal extension output clean; allow explicit opt-in and auto-enable for
# CMAKE_BUILD_TYPE=Debug workflows.
option(NISENSE_LOG_OVERLAY_FALLBACK "Print default overlay list when fallback selection is used" OFF)

# Default Kconfig overlays for IDE/extension/sysbuild builds.
# Always normalize NiSense_EXTRA_CONF_FILE by appending required NiSense
# overlays, then deduplicating. This keeps parity with build_project.ps1 even
# when the caller/extension predefines EXTRA_CONF_FILE or NiSense_EXTRA_CONF_FILE.
if(TRUE)
    set(_core_overlays
        "conf/core/bootloader.conf"
        "conf/core/memory.conf"
        "conf/core/hardware.conf"
        "conf/core/storage.conf"
        "conf/core/debug.conf"
        "conf/core/pmic.conf"
        "conf/core/sensors.conf"
        "conf/core/time.conf")

    # Display is mandatory on this product path (same default as build script).
    set(_feature_overlays "conf/features/display.conf")

    set(_prj_conf_text "")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/prj.conf")
        file(READ "${CMAKE_CURRENT_SOURCE_DIR}/prj.conf" _prj_conf_text)
        # Normalize CRLF/LF so regex checks work reliably on Windows.
        string(REPLACE "\r\n" "\n" _prj_conf_text "${_prj_conf_text}")
        string(REPLACE "\r" "\n" _prj_conf_text "${_prj_conf_text}")
    endif()

    # Prefer line-equality over REGEX (^/$ are string-wide in CMake, and
    # REGEX MATCH is unreliable on large multiline prj.conf text).
    macro(_prj_has_y symbol out_var)
        set(_lines "${_prj_conf_text}")
        string(REPLACE "\n" ";" _lines "${_lines}")
        list(FILTER _lines INCLUDE REGEX "^${symbol}=y$")
        if(_lines)
            set(${out_var} TRUE)
        else()
            set(${out_var} FALSE)
        endif()
    endmacro()

    _prj_has_y("CONFIG_APP_FEATURE_FILE_SYSTEM" _fs)
    _prj_has_y("CONFIG_APP_FEATURE_CONFIG_PERSIST" _cfg_persist)
    _prj_has_y("CONFIG_APP_FEATURE_GLUCOSE" _glucose)
    _prj_has_y("CONFIG_APP_FEATURE_WIFI" _wifi)
    _prj_has_y("CONFIG_APP_FEATURE_CLOUD_TELEMETRY" _cloud)
    _prj_has_y("CONFIG_APP_FEATURE_BLE" _ble)
    _prj_has_y("CONFIG_APP_FEATURE_USB" _usb)
    _prj_has_y("CONFIG_APP_FEATURE_SYSTEMVIEW" _sysview)
    _prj_has_y("CONFIG_APP_FEATURE_FAULT_DEBUG" _fault)
    _prj_has_y("CONFIG_APP_FEATURE_SHELL" _shell)
    _prj_has_y("CONFIG_APP_FEATURE_MAX32664" _max32664)
    _prj_has_y("CONFIG_APP_FEATURE_MAX3010X" _max3010x)
    _prj_has_y("CONFIG_APP_FEATURE_PPG" _ppg)
    _prj_has_y("CONFIG_APP_FEATURE_HR" _hr)
    _prj_has_y("CONFIG_APP_FEATURE_SPO2" _spo2)
    _prj_has_y("CONFIG_APP_FEATURE_MAX32664_UPDATE" _max32664_update)
    _prj_has_y("CONFIG_APP_FEATURE_DIAG_MONITOR" _diag)
    _prj_has_y("CONFIG_APP_FEATURE_RTT_LOG" _rtt_log)
    _prj_has_y("CONFIG_APP_FEATURE_BLE_DFU" _ble_dfu)

    # FILE_SYSTEM is opt-in only — glucose/config persist no longer force FatFS.
    if(_hr OR _spo2)
        set(_ppg TRUE)
    endif()
    if(_ppg AND NOT _max3010x AND NOT _max32664)
        set(_max32664 TRUE)
    endif()
    # BLE DFU requires the BLE feature overlay (parity with build_project.ps1).
    if(_ble_dfu)
        set(_ble TRUE)
    endif()
    # Shell is USB CDC VCOM — enabling shell always pulls the USB feature overlay
    # (Kconfig also select APP_FEATURE_USB).
    if(_shell)
        set(_usb TRUE)
    endif()

    # prj.conf wins over sticky -DCONFIG_APP_FEATURE_*=y from IDE / west build_info.
    # Those CLI overrides survive in CMakeCache and ignore later prj.conf =n edits.
    macro(_prj_wins_over_cli symbol enabled)
        if(${enabled})
            # Keep any explicit =y; prj already sets it.
        else()
            unset(CONFIG_${symbol} CACHE)
            set(CLI_CONFIG_${symbol} "n" CACHE INTERNAL
                "Forced n from prj.conf (clears sticky IDE/west -D)" FORCE)
        endif()
    endmacro()
    _prj_wins_over_cli("APP_FEATURE_SHELL" _shell)
    _prj_wins_over_cli("APP_FEATURE_USB" _usb)

    if(_fs)
        list(APPEND _feature_overlays "conf/features/filesystem.conf")
    endif()
    if(_cfg_persist)
        list(APPEND _feature_overlays "conf/features/config_persist.conf")
    endif()
    if(_glucose)
        list(APPEND _feature_overlays "conf/features/glucose.conf")
    endif()
    if(_cloud)
        set(_wifi TRUE)
        list(APPEND _feature_overlays "conf/features/cloud.conf")
    endif()
    if(_wifi)
        list(APPEND _feature_overlays "conf/features/wifi.conf")
    endif()
    if(_ble)
        list(APPEND _feature_overlays "conf/features/ble.conf")
    endif()
    if(_ble_dfu)
        list(APPEND _feature_overlays "conf/features/ble_dfu.conf")
    endif()
    if(_usb)
        list(APPEND _feature_overlays "conf/features/usb.conf")
    endif()
    if(_sysview)
        list(APPEND _feature_overlays "conf/features/systemview.conf")
    endif()
    if(_fault)
        list(APPEND _feature_overlays "conf/features/fault_debug.conf")
    endif()
    if(_shell)
        list(APPEND _feature_overlays "conf/features/shell.conf")
    endif()
    if(_max32664 AND _max3010x)
        message(STATUS "NiSense: MAX32664 + MAX3010x both enabled (runtime Watch/Pulse SKU detect)")
    endif()
    if(_max32664)
        list(APPEND _feature_overlays "conf/features/max32664.conf")
    endif()
    if(_max3010x)
        list(APPEND _feature_overlays "conf/features/max3010x.conf")
    endif()
    if(_ppg)
        list(APPEND _feature_overlays "conf/features/ppg.conf")
    endif()
    if(_max32664_update)
        list(APPEND _feature_overlays "conf/features/max32664_update.conf")
    endif()
    if(_diag)
        list(APPEND _feature_overlays "conf/features/diag_monitor.conf")
    endif()
    if(_rtt_log)
        list(APPEND _feature_overlays "conf/features/rtt_log.conf")
    endif()

    set(_default_overlays ${_core_overlays} ${_feature_overlays})

    # Rebuild conf list from feature flags only. Do not re-ingest
    # NiSense_EXTRA_CONF_FILE (previous FORCE output would sticky-keep overlays).
    set(_nisense_overlay_list ${_default_overlays})
    if(DEFINED NiSense_USER_EXTRA_CONF_FILE AND NOT "${NiSense_USER_EXTRA_CONF_FILE}" STREQUAL "")
        list(APPEND _nisense_overlay_list ${NiSense_USER_EXTRA_CONF_FILE})
    endif()
    list(REMOVE_DUPLICATES _nisense_overlay_list)
    string(REPLACE ";" "\\;" _nisense_overlay_list_escaped "${_nisense_overlay_list}")

    set(NiSense_EXTRA_CONF_FILE
        "${_nisense_overlay_list_escaped}"
        CACHE STRING "Default NiSense overlay configuration files" FORCE)
    set(EXTRA_CONF_FILE
        "${_nisense_overlay_list_escaped}"
        CACHE STRING "Default overlay configuration files" FORCE)

    # Always print — EXTRA_CONF is only refreshed on CMake reconfigure; this
    # makes missing usb.conf/shell.conf obvious after toggling APP_FEATURE_*.
    message(STATUS "NiSense EXTRA_CONF (${_usb} USB / ${_shell} shell): ${_nisense_overlay_list}")
endif()

# Suppress CMake warning about assertions being enabled (we want them for debugging)
# This sets a cache variable that Zephyr checks before printing the informational message
# Assertions are intentionally enabled for debugging purposes
set(BOARD_ASSERT_OFF FALSE CACHE INTERNAL "" FORCE)
set(SUPPRESS_ASSERT_MESSAGE TRUE CACHE INTERNAL "" FORCE)

# Note: Partition Manager configuration is set in sysbuild.cmake for sysbuild projects

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(NiSense)

# App headers (calibration.h, etc.) for out-of-tree drivers and subsys
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Embed Git commit identity for RTT logs and on-screen version display.
set(NISENSE_GIT_VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/include/git_version.h")
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/git_version.cmake)
nisense_configure_git_version("${NISENSE_GIT_VERSION_HEADER}")

set(_git_version_deps
    ${CMAKE_SOURCE_DIR}/include/git_version.h.in
    ${CMAKE_SOURCE_DIR}/cmake/git_version.cmake
    ${CMAKE_SOURCE_DIR}/cmake/git_version_script.cmake
    ${CMAKE_SOURCE_DIR}/VERSION
)
if(EXISTS "${CMAKE_SOURCE_DIR}/.git/HEAD")
    list(APPEND _git_version_deps "${CMAKE_SOURCE_DIR}/.git/HEAD")
endif()
add_custom_command(
    OUTPUT "${NISENSE_GIT_VERSION_HEADER}"
    COMMAND ${CMAKE_COMMAND}
        -DCMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DOUTPUT_HEADER=${NISENSE_GIT_VERSION_HEADER}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/git_version_script.cmake
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS ${_git_version_deps}
    COMMENT "Refreshing git_version.h"
    VERBATIM
)
add_custom_target(nisense_git_version DEPENDS "${NISENSE_GIT_VERSION_HEADER}")
add_dependencies(app nisense_git_version)

# Core application sources (always included)
target_sources(app PRIVATE 
    src/main.c
    src/core/rgb.c
    src/core/led_status.c
    src/core/buzzer.c
    src/core/i2c_probe.c
    src/core/storage_config.c
    src/core/logo_stubs_resource.c
    src/diag/diag_monitor.c
    src/core/ram_budget.c
)

# UI framework (LVGL/display/touch) — gated on display feature
target_sources_ifdef(CONFIG_APP_FEATURE_DISPLAY app PRIVATE src/ui/ui.c)
target_sources_ifdef(CONFIG_APP_FEATURE_DISPLAY app PRIVATE src/ui/ui_theme.c)
target_sources_ifdef(CONFIG_APP_FEATURE_DISPLAY app PRIVATE src/ui/ui_measure_hold.c)
target_sources_ifdef(CONFIG_APP_FEATURE_DISPLAY app PRIVATE src/ui/ui_hold_overlay.c)
target_sources_ifdef(CONFIG_APP_FEATURE_DISPLAY app PRIVATE src/ui/ui_buttons.c)
target_sources_ifdef(CONFIG_APP_FEATURE_DISPLAY app PRIVATE src/ui/hw_button.c)
target_sources_ifdef(CONFIG_APP_FEATURE_DISPLAY app PRIVATE src/ui/power_button.c)
target_sources_ifdef(CONFIG_APP_FEATURE_DISPLAY app PRIVATE src/ui/nav_button.c)
target_sources_ifdef(CONFIG_APP_FEATURE_DISPLAY app PRIVATE src/ui/ui_shell.c)
target_sources_ifdef(CONFIG_APP_FEATURE_DISPLAY app PRIVATE src/ui/ui_obj_track.c)
target_sources_ifdef(CONFIG_APP_FEATURE_DISPLAY app PRIVATE src/ota/resource_icons.c)

# Logo/icons in Resource A/B store (QSPI mmap).

# Power subsystem (always needed for PMIC)
target_sources(app PRIVATE 
    src/power/power.c
    src/power/power_batt.c
)

# QSPI XIP vs record-store program/erase exclusivity (share the same NOR die)
target_sources(app PRIVATE src/storage/nor_qspi_busy.c)

# Structured NOR record store (replaces FatFS CSV loggers)
target_sources(app PRIVATE src/storage/record_store.c)
target_sources(app PRIVATE src/storage/record_sync_engine.c)

# Diagnostics shell (USB CDC-ACM / RTT) — record-store browse/gc/reset
target_sources_ifdef(CONFIG_SHELL app PRIVATE src/diag/diag_shell.c)

# Configuration manager (NVS / settings-backed app_config)
target_sources_ifdef(CONFIG_APP_FEATURE_CONFIG_PERSIST app PRIVATE src/storage/config_manager.c)

# Wi-Fi app transport: keep default Wi-Fi builds on STA + HTTP bulk sync.
target_sources_ifdef(CONFIG_APP_FEATURE_WIFI app PRIVATE
    src/net/http_client.c
    src/net/wifi_bulk_session.c
    src/net/wifi_ota_transfer.c
)
target_sources_ifdef(CONFIG_APP_FEATURE_CLOUD_TELEMETRY app PRIVATE
    src/net/mqtt_client.c
)

target_sources_ifdef(CONFIG_APP_FEATURE_CLOUD_TELEMETRY app PRIVATE
    src/net/cloud_telemetry.c
)

# RTC (always needed for timestamps) + product Watch/Pulse SKU detect
target_sources(app PRIVATE 
    src/core/rtc.c
    src/core/device_identity.c
    src/core/product_hw_detect.c
    src/power/power_mgr.c
)

# UI event bus (agentic UI foundation; always compiled, tiny)
target_sources(app PRIVATE src/ui/ui_events.c)

# Temperature monitor (SoC die + skin sensors)
target_sources_ifdef(CONFIG_APP_TEMP_MONITOR app PRIVATE src/sensors/temp.c)

# Agentic health measurement scheduler
target_sources_ifdef(CONFIG_APP_HEALTH_SCHED app PRIVATE src/sensors/health_sched.c)

# Agentic UI presenter (results slideshow)
target_sources_ifdef(CONFIG_APP_UI_AGENT app PRIVATE src/ui/ui_agent.c)

# Unified manual Measure screen
target_sources_ifdef(CONFIG_UI_SCREEN_MEASURE app PRIVATE src/ui/measure_ui.c)

# Glucose sensor (core feature)
target_sources_ifdef(CONFIG_GLUCOSE_SENSOR app PRIVATE src/sensors/glucose.c)
target_sources_ifdef(CONFIG_GLUCOSE_SENSOR app PRIVATE src/sensors/glucose_logger.c)

# MAX32664 hub firmware update (buffer / app-partition / optional FatFS path)
if(CONFIG_UI_SCREEN_MAX32664_UPDATE OR CONFIG_APP_FEATURE_MAX32664_UPDATE)
    target_sources(app PRIVATE
        src/sensors/max32664_update.c
        src/sensors/max32664_msbl_policy.c
    )
endif()

# PPG measurement system (requires APP_FEATURE_PPG umbrella)
# HR and SpO2 are sub-features of PPG; enabling either auto-enables PPG.
if(CONFIG_APP_FEATURE_PPG)
    target_sources(app PRIVATE src/sensors/ppg.c)
    target_sources(app PRIVATE src/sensors/ppg_logger.c)
endif()

# =============================================================================
# Conditional UI Screens (based on Kconfig selection)
# Note: Device Info is ALWAYS included as home screen when display is enabled
# =============================================================================

# Device Info screen - home screen (index 0), always on with display
target_sources_ifdef(CONFIG_UI_SCREEN_DEVICE_INFO app PRIVATE src/ui/device_info_ui.c)
target_sources_ifdef(CONFIG_UI_SCREEN_HOME_INFO app PRIVATE src/ui/home_info_ui.c)
# Last-results cache (NVS) — used by loggers + home / vitals / metabolic screens
target_sources(app PRIVATE src/ui/home_last_results.c)

# Power UI screens (Power, Regulator, Charger) — shared module
if(CONFIG_UI_SCREEN_POWER OR CONFIG_UI_SCREEN_CHARGER OR CONFIG_UI_SCREEN_REGULATOR)
    target_sources(app PRIVATE src/ui/power_ui.c)
endif()

# RTC screen
target_sources_ifdef(CONFIG_UI_SCREEN_RTC app PRIVATE src/ui/rtc_ui.c)

# Glucose / Metabolic screen
target_sources_ifdef(CONFIG_UI_SCREEN_GLUCOSE app PRIVATE src/ui/glucose_ui.c)

# Vital screen (HR + SpO2 + Resp + Body Temp)
target_sources_ifdef(CONFIG_UI_SCREEN_VITALS app PRIVATE src/ui/vitals_ui.c)

# Vascular screen (SDNN + RMSSD + BP estimate)
target_sources_ifdef(CONFIG_UI_SCREEN_VASCULAR app PRIVATE src/ui/vascular_ui.c)

# PPG sample waveform screen
target_sources_ifdef(CONFIG_UI_SCREEN_PPG_SAMPLE app PRIVATE src/ui/ppg_sample_ui.c)

# Temperature screen
target_sources_ifdef(CONFIG_UI_SCREEN_TEMPERATURE app PRIVATE src/ui/temp_ui.c)

target_sources_ifdef(CONFIG_UI_SCREEN_NOTICE app PRIVATE src/ui/ui_notice.c)

# Storage-aware measurement pre-check (record store readiness)
target_sources_ifdef(CONFIG_APP_HEALTH_SCHED app PRIVATE src/sensors/meas_precheck.c)

# Proximity screen
target_sources_ifdef(CONFIG_UI_SCREEN_PROXIMITY app PRIVATE src/ui/proximity_ui.c)

# Buzzer screen
target_sources_ifdef(CONFIG_UI_SCREEN_BUZZER app PRIVATE src/ui/buzzer_ui.c)

# Wi-Fi screen
target_sources_ifdef(CONFIG_UI_SCREEN_WIFI app PRIVATE src/ui/wifi_ui.c)

# BLE screen
# device_prefs: backlight (display / power_mgr) and volume (BLE GATT)
if(CONFIG_APP_FEATURE_DISPLAY OR CONFIG_BT)
  target_sources(app PRIVATE src/core/device_prefs.c)
endif()
target_sources_ifdef(CONFIG_BT app PRIVATE src/ble/ble_gatt.c)
if(CONFIG_BT AND CONFIG_GLUCOSE_MATRICES_FROM_XIP)
    target_sources(app PRIVATE src/ble/ble_model_transfer.c)
endif()
if(CONFIG_BT AND CONFIG_RESOURCE)
    target_sources(app PRIVATE src/ble/ble_resource_transfer.c)
endif()
if(CONFIG_BT)
    target_sources(app PRIVATE src/ble/ble_record_sync.c)
endif()
# OTA progress UI + FW↔asset/model compat (always useful with BT or XIP)
target_sources(app PRIVATE src/ota/ota_compat.c)
target_sources(app PRIVATE src/ota/ota_progress_ui.c)
target_sources_ifdef(CONFIG_BT_CTS app PRIVATE src/ble/ble_cts.c)
target_sources_ifdef(CONFIG_APP_FEATURE_BLE app PRIVATE src/ble/ble.c)
target_sources_ifdef(CONFIG_UI_SCREEN_BLE app PRIVATE src/ble/ble_ui.c)

# MAX32664 Update screen (FatFS .msbl scan — requires FILE_SYSTEM)
if(CONFIG_UI_SCREEN_MAX32664_UPDATE AND CONFIG_FILE_SYSTEM)
    target_sources(app PRIVATE src/ui/max32664_update_ui.c)
endif()

# Accelerometer screen
target_sources_ifdef(CONFIG_UI_SCREEN_ACCEL app PRIVATE src/ui/accel_ui.c)

# =============================================================================
# Resource store & Calibration Modules
# =============================================================================

# Resource store (UI fonts/logos/icons — A/B NOR, XIP mmap)
target_sources_ifdef(CONFIG_RESOURCE app PRIVATE src/ota/resource.c)

# Resource fonts (TinyTTF from Resource partition)
target_sources_ifdef(CONFIG_RESOURCE_FONTS app PRIVATE src/ota/resource_fonts.c)

# Production Calibration module (read-only factory calibration data)
target_sources_ifdef(CONFIG_CALIBRATION app PRIVATE src/ota/calibration.c)

# Add include directory for custom headers
target_include_directories(app PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ble
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ota
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/power
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sensors
    ${CMAKE_CURRENT_SOURCE_DIR}/src/storage
    ${CMAKE_CURRENT_SOURCE_DIR}/src/net
    ${CMAKE_CURRENT_SOURCE_DIR}/src/diag
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

# Add RTT debug definitions
target_compile_definitions(app PRIVATE
    -DRTT_USE_ASM=0
)

# TinyTTF / stb_truetype: prevent internal assert() from kernel-panicking (issue #4)
# NOTE: file(GENERATE) uses a [=[=]=] bracket argument — backslashes are NOT
# CMake-escaped. Write "\n" (one backslash) so the generated C string has a
# real newline. "\\n" becomes a literal backslash-n in RTT (what you saw).
if(CONFIG_RESOURCE_FONTS AND CONFIG_LV_USE_TINY_TTF)
    set(_STBTT_ASSERT_HDR "${CMAKE_CURRENT_BINARY_DIR}/generated/stbtt_assert_nisense.h")
    file(GENERATE OUTPUT "${_STBTT_ASSERT_HDR}" CONTENT [=[/* Auto-generated by CMake for TinyTTF assert softening. */
#ifndef STBTT_ASSERT_NISENSE_H_
#define STBTT_ASSERT_NISENSE_H_

#include <zephyr/sys/printk.h>

#ifndef STBTT_assert
#define STBTT_assert(cond) \
	do { \
		if (!(cond)) { \
			static int _stbtt_ni_ignored; \
			if (_stbtt_ni_ignored < 8) { \
				_stbtt_ni_ignored++; \
				printk("[resource_fonts] STBTT soft-assert ignored #%d cond=%s\n", \
				       _stbtt_ni_ignored, #cond); \
			} \
		} \
	} while (0)
#endif

#ifndef STBRP_ASSERT
#define STBRP_ASSERT(cond) STBTT_assert(cond)
#endif

#endif /* STBTT_ASSERT_NISENSE_H_ */
]=])
    zephyr_compile_options(
        "$<$<COMPILE_LANGUAGE:C>:-include${_STBTT_ASSERT_HDR}>"
    )
endif()

# =============================================================================
# Resource store conversion (logos / fonts / icons → resource.bin)
# =============================================================================
# Convert LVGL logo C sources and optional fonts to the Resource binary during build
if(PYTHON_EXECUTABLE)
    set(_RESOURCE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/../resource.bin")
    set(_FONT_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Montserrat-Regular.ttf")
    set(_FONT_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Montserrat-Subset.ttf")
    
    # Font subsetting target (only when Resource fonts enabled)
    if(CONFIG_RESOURCE_FONTS)
        add_custom_command(
            OUTPUT "${_FONT_OUTPUT}"
            COMMAND ${PYTHON_EXECUTABLE}
                "${CMAKE_CURRENT_SOURCE_DIR}/scripts/tools/subset_fonts.py"
                --input "${_FONT_INPUT}"
                --output "${_FONT_OUTPUT}"
                --charset hcm
            DEPENDS "${_FONT_INPUT}"
            COMMENT "Subsetting Montserrat font for Resource store..."
            VERBATIM
        )
        
        add_custom_target(font_subset
            DEPENDS "${_FONT_OUTPUT}"
        )
        
        message(STATUS "Font subsetting: ENABLED")
        message(STATUS "  Input:  ${_FONT_INPUT}")
        message(STATUS "  Output: ${_FONT_OUTPUT}")
    endif()
    
    # Build command arguments for Resource store
    # Logos live under src/core/ after thematic regroup (logo.c).
    set(_RESOURCE_ARGS
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/tools/convert_logos_to_resource.py"
        --src-dir "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
        --output "${_RESOURCE_OUTPUT}"
    )
    
    # Add build ID if available
    if(PROJECT_VERSION)
        list(APPEND _RESOURCE_ARGS --build-id "${PROJECT_VERSION}")
    endif()
    
    # Only include fonts if CONFIG_RESOURCE_FONTS is enabled
    # Otherwise pass --no-fonts to keep binary small
    if(NOT CONFIG_RESOURCE_FONTS)
        list(APPEND _RESOURCE_ARGS --no-fonts)
    endif()
    
    # Resource store target
    add_custom_target(resource ALL
        COMMAND ${PYTHON_EXECUTABLE} ${_RESOURCE_ARGS}
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/core/logo.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/logo/charge.png"
        COMMENT "Converting LVGL logo to Resource binary..."
        VERBATIM
    )
    
    # Make Resource store depend on font subset when enabled
    if(CONFIG_RESOURCE_FONTS)
        add_dependencies(resource font_subset)
    endif()
    
    # Make the main app depend on Resource store being built first
    add_dependencies(app resource)
    
    message(STATUS "Resource store conversion: ENABLED")
    message(STATUS "  Output: ${_RESOURCE_OUTPUT}")
else()
    message(WARNING "Python not found - Resource store conversion disabled")
endif()

# =============================================================================
# Glucose Model Packing (CSV matrices -> memory-mapped XIP binary)
# =============================================================================
# Produces per-variant GMDL bins plus model_pack.bin (wearable+pulse) for
# model-primary @ 0x1F6000 (512 KB A/B with secondary @ 0x276000).
if(PYTHON_EXECUTABLE)
    set(_GLUCOSE_MODEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/model/glucose")
    set(_GLUCOSE_WEARABLE_OUT "${CMAKE_CURRENT_BINARY_DIR}/../glucose_model_wearable.bin")
    set(_GLUCOSE_PULSE_OUT "${CMAKE_CURRENT_BINARY_DIR}/../glucose_model_pulse.bin")
    set(_MODEL_PACK_OUT "${CMAKE_CURRENT_BINARY_DIR}/../model_pack.bin")
    set(_GLUCOSE_PACKER "${CMAKE_CURRENT_SOURCE_DIR}/scripts/tools/pack_glucose_model.py")

    set(_GLUCOSE_BUILD_ID "dev")
    if(PROJECT_VERSION)
        set(_GLUCOSE_BUILD_ID "${PROJECT_VERSION}")
    endif()

    add_custom_target(glucose_model ALL
        COMMAND ${PYTHON_EXECUTABLE} "${_GLUCOSE_PACKER}"
            --variant wearable --model-dir "${_GLUCOSE_MODEL_DIR}"
            --output "${_GLUCOSE_WEARABLE_OUT}" --build-id "${_GLUCOSE_BUILD_ID}"
        COMMAND ${PYTHON_EXECUTABLE} "${_GLUCOSE_PACKER}"
            --variant pulse --model-dir "${_GLUCOSE_MODEL_DIR}"
            --output "${_GLUCOSE_PULSE_OUT}" --build-id "${_GLUCOSE_BUILD_ID}"
        COMMAND ${PYTHON_EXECUTABLE} "${_GLUCOSE_PACKER}"
            --pack-all --model-dir "${_GLUCOSE_MODEL_DIR}"
            --output "${_MODEL_PACK_OUT}" --build-id "${_GLUCOSE_BUILD_ID}"
        COMMENT "Packing clinical model pack (wearable+pulse) for NOR..."
        VERBATIM
    )
    add_dependencies(app glucose_model)

    message(STATUS "Clinical model packing: ENABLED")
    message(STATUS "  Wearable: ${_GLUCOSE_WEARABLE_OUT}")
    message(STATUS "  Pulse:    ${_GLUCOSE_PULSE_OUT}")
    message(STATUS "  Pack:     ${_MODEL_PACK_OUT}")
else()
    message(WARNING "Python not found - model packing disabled")
endif()

# MCUBoot signing post-build command
# Note: In nRF Connect SDK sysbuild, signing is handled via build script (scripts/build.sh)
# The post-build command here is optional - uncomment if you want automatic signing in CMake
# add_custom_command(TARGET app POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E echo "Signing image with MCUBoot..."
#     COMMAND imgtool
#         sign
#         -k "${CMAKE_CURRENT_SOURCE_DIR}/mcuboot/root-rsa-2048.key"
#         --align 4
#         --header-size 0x200
#         --pad-header
#         --slot-size 0x60000
#         --version 1.0.0
#         "${CMAKE_CURRENT_BINARY_DIR}/zephyr/zephyr.bin"
#         "${CMAKE_CURRENT_BINARY_DIR}/zephyr/zephyr.signed.bin"
#     COMMENT "Signing firmware image with MCUBoot"
# )