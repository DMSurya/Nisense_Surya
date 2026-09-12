# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_GLUCOSE_SENSOR
	glucose_sensor.c
	glucose_algorithm.c
	glucose_algorithm_profiles.c
	glucose_model_xip.c
)

# Add include directory for public API headers
# From drivers/sensor/glucose to project root: up 3 levels (glucose->sensor->drivers->root)
zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../include)
