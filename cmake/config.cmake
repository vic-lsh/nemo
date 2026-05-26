# cmake/config.cmake

if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/config")
    file(STRINGS "${CMAKE_SOURCE_DIR}/cmake/config" CONFIG_LINES)

    foreach(LINE ${CONFIG_LINES})
        if(NOT ${LINE} MATCHES "^#" AND NOT ${LINE} STREQUAL "")
            if(${LINE} MATCHES "^([A-Za-z0-9_]+)=([yn])")
                set(CONFIG_KEY ${CMAKE_MATCH_1})
                set(CONFIG_VALUE ${CMAKE_MATCH_2})

                # 1. Get the original help string.
                get_property(HELP_STR CACHE ${CONFIG_KEY} PROPERTY HELPSTRING)

                # 2. Configure the config variable's value in CMake
                if(${CONFIG_VALUE} STREQUAL "y")
                    set(${CONFIG_KEY} ON CACHE BOOL "${HELP_STR}" FORCE)
                else()
                    set(${CONFIG_KEY} OFF CACHE BOOL "${HELP_STR}" FORCE)
                endif()
            else()
                message(WARNING "Ignoring invalid line in config file: ${LINE}")
            endif()
        endif()
    endforeach()
endif()
