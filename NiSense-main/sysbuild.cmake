# Append a local MAIN image config cleanup script after Zephyr's default
# sysbuild image configuration so we can remove noisy derived app settings.

get_property(main_image_conf_scripts TARGET ${DEFAULT_IMAGE} PROPERTY IMAGE_CONF_SCRIPT)
list(APPEND main_image_conf_scripts
    "${CMAKE_CURRENT_LIST_DIR}/sysbuild/image_configurations/main_image_cleanup.cmake"
)
set_target_properties(${DEFAULT_IMAGE} PROPERTIES IMAGE_CONF_SCRIPT "${main_image_conf_scripts}")

# Sysbuild emits board-scoped merged hex files (merged_<board_target>.hex).
# Create merged.hex as a compatibility alias so IDE/nRF Connect builds and
# existing flash tools can use the legacy filename.
if(SB_CONFIG_MERGED_HEX_FILES)
    set(board_target)
    sysbuild_get(board_target IMAGE ${DEFAULT_IMAGE} VAR CONFIG_BOARD_TARGET KCONFIG)

    if(board_target)
        string(REPLACE "/" "_" board_target ${board_target})
        string(REPLACE "." "_" board_target ${board_target})
        string(REPLACE "@" "_" board_target ${board_target})

        set(merged_board_hex "${CMAKE_BINARY_DIR}/merged_${board_target}.hex")
        set(merged_hex_alias "${CMAKE_BINARY_DIR}/merged.hex")

        add_custom_command(
            OUTPUT
                ${merged_hex_alias}
            COMMAND
                ${CMAKE_COMMAND} -E copy_if_different ${merged_board_hex} ${merged_hex_alias}
            DEPENDS
                ${merged_board_hex}
            COMMENT
                "Creating merged.hex compatibility alias"
        )

        add_custom_target(
            merged_hex_alias_target
            ALL DEPENDS
            ${merged_hex_alias}
        )
    endif()
endif()