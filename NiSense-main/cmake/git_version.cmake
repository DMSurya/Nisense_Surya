# Generate include/git_version.h with the current Git commit identity and
# semantic firmware version from the repo-root VERSION file (MAJOR.MINOR.PATCH).
#
# Called from CMakeLists.txt at configure time and via add_custom_command so
# the hash is refreshed when HEAD moves between incremental builds.

function(nisense_read_fw_version major_out minor_out patch_out)
    set(_maj 1)
    set(_min 0)
    set(_pat 0)
    set(_ver_file "${CMAKE_SOURCE_DIR}/VERSION")
    if(EXISTS "${_ver_file}")
        file(READ "${_ver_file}" _ver_raw)
        # Zephyr APP VERSION format (also feeds MCUboot imgtool sign version).
        string(REGEX MATCH "VERSION_MAJOR = ([0-9]+)" _ "${_ver_raw}")
        set(_has_maj "${CMAKE_MATCH_1}")
        string(REGEX MATCH "VERSION_MINOR = ([0-9]+)" _ "${_ver_raw}")
        set(_has_min "${CMAKE_MATCH_1}")
        string(REGEX MATCH "PATCHLEVEL = ([0-9]+)" _ "${_ver_raw}")
        set(_has_pat "${CMAKE_MATCH_1}")
        if(_has_maj STREQUAL "" OR _has_min STREQUAL "" OR _has_pat STREQUAL "")
            # Fallback: single-line MAJOR.MINOR.PATCH
            string(STRIP "${_ver_raw}" _ver_raw)
            string(REGEX REPLACE "\r?\n.*" "" _ver_line "${_ver_raw}")
            string(STRIP "${_ver_line}" _ver_line)
            if(_ver_line MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
                set(_maj "${CMAKE_MATCH_1}")
                set(_min "${CMAKE_MATCH_2}")
                set(_pat "${CMAKE_MATCH_3}")
            else()
                message(WARNING "NiSense VERSION file unreadable; using 1.0.0")
            endif()
        else()
            set(_maj "${_has_maj}")
            set(_min "${_has_min}")
            set(_pat "${_has_pat}")
        endif()
    else()
        message(WARNING "NiSense VERSION file missing; using 1.0.0")
    endif()
    set(${major_out} "${_maj}" PARENT_SCOPE)
    set(${minor_out} "${_min}" PARENT_SCOPE)
    set(${patch_out} "${_pat}" PARENT_SCOPE)
endfunction()

function(nisense_configure_git_version output_header)
    set(_hash "unknown")
    set(_hash_full "unknown")
    set(_dirty 0)

    find_package(Git QUIET)
    if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short=8 HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE _hash
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE _hash_full
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        execute_process(
            COMMAND ${GIT_EXECUTABLE} diff-index --quiet HEAD --
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE _dirty_result
            ERROR_QUIET
        )
        if(NOT _dirty_result EQUAL 0)
            set(_dirty 1)
        endif()
    endif()

    if(NOT _hash)
        set(_hash "unknown")
    endif()
    if(NOT _hash_full)
        set(_hash_full "unknown")
    endif()

    if(_dirty)
        set(_version_string "${_hash}-dirty")
    else()
        set(_version_string "${_hash}")
    endif()

    nisense_read_fw_version(FW_VERSION_MAJOR FW_VERSION_MINOR FW_VERSION_PATCH)

    set(GIT_HASH "${_hash}")
    set(GIT_HASH_FULL "${_hash_full}")
    set(GIT_DIRTY "${_dirty}")
    set(GIT_VERSION_STRING "${_version_string}")

    get_filename_component(_out_dir "${output_header}" DIRECTORY)
    file(MAKE_DIRECTORY "${_out_dir}")

    configure_file(
        "${CMAKE_SOURCE_DIR}/include/git_version.h.in"
        "${output_header}"
        @ONLY
    )
endfunction()
