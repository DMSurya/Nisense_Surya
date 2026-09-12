# SPDX-License-Identifier: Apache-2.0

if(CONFIG_COUNTER_MCP79XX)
	zephyr_library()
	zephyr_library_sources(mcp79xx.c)
endif()
