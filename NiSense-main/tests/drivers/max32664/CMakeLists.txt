# SPDX-License-Identifier: Apache-2.0
# MAX32664 transport/status unit tests (host-side helpers, no board).
#
# Run (from NCS/west workspace):
#   west twister -T tests/drivers/max32664 -p native_sim
#
# Or document-only if native_sim is unavailable in this product west topdir.

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(max32664_status_test)

target_sources(app PRIVATE src/main.c)
target_include_directories(app PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
