if(CONFIG_ADC_MAX20360)
  zephyr_library_named(drivers__adc__max20360)
  zephyr_library_sources(adc_max20360.c)
  # Expose max20360_adc.h (channel IDs) to consumers, e.g. src/power/power_batt.c
  zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
endif()
