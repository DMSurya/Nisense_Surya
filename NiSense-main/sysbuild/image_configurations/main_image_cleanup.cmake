# Remove app-side derived DFU footer metadata that sysbuild persists into the
# main image config. The application intentionally keeps IMG_MANAGER disabled,
# so this derived assignment only creates a Kconfig warning during merge.

get_target_property(main_image_config ${ZCMAKE_APPLICATION} CONFIG)

if(main_image_config)
  string(REGEX REPLACE "CONFIG_MCUBOOT_UPDATE_FOOTER_SIZE=[^\n]*\n" ""
         main_image_config "${main_image_config}")
  set_property(TARGET ${ZCMAKE_APPLICATION} PROPERTY CONFIG "${main_image_config}")
endif()