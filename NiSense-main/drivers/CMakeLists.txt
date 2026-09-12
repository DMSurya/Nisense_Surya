add_subdirectory_ifdef(CONFIG_ADC adc)
add_subdirectory_ifdef(CONFIG_COUNTER counter)
add_subdirectory_ifdef(CONFIG_REGULATOR regulator)
add_subdirectory_ifdef(CONFIG_FUEL_GAUGE fuel_gauge)
add_subdirectory_ifdef(CONFIG_CHARGER charger)
add_subdirectory_ifdef(CONFIG_SENSOR sensor)
add_subdirectory_ifdef(CONFIG_WIFI_WEXX wifi)

# This module CMakeLists.txt is processed by Zephyr AFTER the upstream
# add_subdirectory(drivers) (line 788) but BEFORE the empty-library foreach
# warning check (line ~1110) in zephyr/CMakeLists.txt.  That ordering means
# the upstream driver targets already exist here, so we can mark them as
# intentionally empty without touching any SDK file.
#
#  - drivers__charger / drivers__counter / drivers__fuel_gauge
#      Our out-of-tree implementations (in this drivers/ tree) fully replace
#      the upstream sources; the upstream targets contain no .c files.
#  - drivers__console
#      CONFIG_UART_CONSOLE=n (RTT is used), so the console driver has no sources.
#  - drivers__disk
#      No disk-access backend is selected for this product, so upstream disk
#      driver target can be intentionally empty.
foreach(_sdk_empty_lib
    drivers__charger
    drivers__console
    drivers__disk
    drivers__counter
    drivers__fuel_gauge)
  if(TARGET ${_sdk_empty_lib})
    set_property(TARGET ${_sdk_empty_lib} PROPERTY ALLOW_EMPTY TRUE)
  endif()
endforeach()