# SPDX-License-Identifier: Apache-2.0

# Only create the library if MAX20360 charger driver is enabled
# This prevents creating an empty library when no charger drivers are enabled
if(CONFIG_CHARGER_MAX20360)
	zephyr_library()
	zephyr_syscall_header(${ZEPHYR_BASE}/include/zephyr/drivers/charger.h)
	zephyr_library_sources(charger_max20360.c)
endif()