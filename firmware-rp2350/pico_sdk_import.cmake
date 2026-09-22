if (DEFINED PICO_SDK_PATH)
    set(PICO_SDK_PATH "${PICO_SDK_PATH}" CACHE PATH "Path to the Raspberry Pi Pico SDK")
elseif (DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}" CACHE PATH "Path to the Raspberry Pi Pico SDK")
else ()
    message(FATAL_ERROR
        "PICO_SDK_PATH not defined.\n"
        "Usage: cmake -DPICO_SDK_PATH=/path/to/pico-sdk ...\n"
        "   or: set PICO_SDK_PATH=/path/to/pico-sdk (env var)"
    )
endif ()

if (NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    message(FATAL_ERROR "PICO_SDK_PATH '${PICO_SDK_PATH}' does not contain pico_sdk_import.cmake")
endif ()

include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
