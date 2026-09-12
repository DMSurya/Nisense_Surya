# SPDX-License-Identifier: Apache-2.0

zephyr_library()

# Core driver - always included
zephyr_library_sources_ifdef(CONFIG_MAX32664
	max32664.c         # Common core: init, FIFO dispatch, sensor API
	max32664_transport.c # Common transport: I2C command path + bus locking
	max32664_protocol.c # Common protocol: register/FIFO/status/algo helpers
	max32664_init.c    # Common init/probe helpers
	max32664_runtime.c # Common runtime: output modes + HUB sampling lifecycle
	max32664_sampling.c # Common sampling path: poll/IRQ/FIFO parse/channel get
)

# Host-fed accelerometer feeder — compiled into any mode that needs WHRM
# motion-rejection when the board uses LIS2DS12 on firmware that only
# auto-drives KX122. See Kconfig: MAX32664_EXTERNAL_ACCEL_FEEDER.
zephyr_library_sources_ifdef(CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER
	max32664_accel_feeder.c
)

# HUB mode: Include variant-specific algorithm wrappers.
# Variant B is gated: FIFO parse is still a stub (see Kconfig
# MAX32664_HUB_VARIANT_B / MAX32664_HUB_VARIANT_B_ALLOW_INCOMPLETE).
if(CONFIG_MAX32664_MODE_HUB)
	zephyr_library_sources(
		max32664a.c     # Variant A: MaximFast (HR+SpO2, 100Hz, MAX30101, AGC)
		max32664c.c     # Variant C: WHRM+WSpO2 (HR+SpO2, 25Hz, MAX86141, accel)
		max32664d.c     # Variant D: BPT (Blood Pressure, 100Hz, MAX30101, calibration)
	)
	zephyr_library_sources_ifdef(CONFIG_MAX32664_HUB_VARIANT_B
		max32664b.c     # Variant B: WHRM only — incomplete until parse_fifo lands
	)
endif()

# RAW mode: Include passthrough sensor drivers
if(CONFIG_MAX32664_MODE_RAW)
	zephyr_library_sources(
		max32664_raw.c     # RAW mode core: passthrough, FIFO access
	)
	
	# PPG sensor exposed via hub passthrough
	zephyr_library_sources_ifdef(CONFIG_MAX32664_RAW_PPG_SENSOR
		max86141_ppg.c     # MAX86141 PPG sensor driver (via hub)
	)
	
	# Accel sensor exposed via hub passthrough  
	zephyr_library_sources_ifdef(CONFIG_MAX32664_RAW_ACCEL_SENSOR
		lis2ds12_accel.c   # LIS2DS12 accel sensor driver (via hub)
	)
endif()

# PASSTHROUGH mode: Direct sensor access with hub disabled
if(CONFIG_MAX32664_MODE_PASSTHROUGH)
	zephyr_library_sources(
		max32664_passthrough.c   # PASSTHROUGH mode: hub bypassed, direct FIFO reads
		max86141_ppg.c           # MAX86141 child sensor device
		lis2ds12_accel.c         # LIS2DS12 child sensor device
	)
endif()

# Shared MAX86141 green LED (LED1) power gating — RAW drain + PASSTHROUGH child
if(CONFIG_MAX32664_MODE_RAW OR CONFIG_MAX32664_MODE_PASSTHROUGH)
	zephyr_library_sources(
		max86141_green.c
	)
endif()

# Add include directory for public API headers
zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../include)

