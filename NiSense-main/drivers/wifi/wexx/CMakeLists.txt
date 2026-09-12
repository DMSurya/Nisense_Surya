# SPDX-License-Identifier: Apache-2.0
# WExx Wi-Fi Module Driver - Modular Architecture
#
# Core driver is always compiled.
# Protocol-specific modules are conditionally compiled based on Kconfig options.

zephyr_library()

# Core driver (UART, init, connect/disconnect, scan, ping)
zephyr_library_sources(wexx_core.c)

# Protocol modules - conditionally compiled
zephyr_library_sources_ifdef(CONFIG_WIFI_WEXX_HTTP      wexx_http.c)
zephyr_library_sources_ifdef(CONFIG_WIFI_WEXX_MQTT      wexx_mqtt.c)
zephyr_library_sources_ifdef(CONFIG_WIFI_WEXX_WEBSOCKET wexx_websocket.c)
zephyr_library_sources_ifdef(CONFIG_WIFI_WEXX_NTP       wexx_ntp.c)
zephyr_library_sources_ifdef(CONFIG_WIFI_WEXX_SOCKET    wexx_socket.c)
zephyr_library_sources_ifdef(CONFIG_WIFI_WEXX_AP        wexx_ap.c)
zephyr_library_sources_ifdef(CONFIG_WIFI_WEXX_OTA       wexx_ota.c)
zephyr_library_sources_ifdef(CONFIG_WIFI_WEXX_FLASH     wexx_flash.c)

zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../include)
zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
