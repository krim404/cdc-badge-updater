cmake_minimum_required(VERSION 3.16.0)

# Patch CalEPD for CDC Badge (MISO support + tolerant spi_bus_initialize)
set(CALEPD_DIR "${CMAKE_SOURCE_DIR}/components/CalEPD")
set(CALEPD_PATCH_MARKER "${CALEPD_DIR}/.patched")

if(NOT EXISTS ${CALEPD_PATCH_MARKER})
    file(WRITE "${CALEPD_DIR}/CMakeLists.txt"
"# Minimal CalEPD for CDC Badge (only GDEY029T94)
set(srcs
    \"models/goodisplay/gdey029T94.cpp\"
    \"epd.cpp\"
    \"epdspi.cpp\"
)

idf_component_register(SRCS \${srcs}
    REQUIRES \"Adafruit-GFX\" esp_timer driver
    INCLUDE_DIRS \"include\"
)
")
    message(STATUS "CalEPD: minimal CMakeLists.txt written")

    set(PATCH_MISO "${CMAKE_SOURCE_DIR}/patches/calepd_spi_miso.patch")
    if(EXISTS ${PATCH_MISO})
        execute_process(
            COMMAND patch -p1 -N -d ${CALEPD_DIR} -i ${PATCH_MISO}
            RESULT_VARIABLE PATCH_RESULT
        )
        if(PATCH_RESULT EQUAL 0)
            message(STATUS "CalEPD: MISO patch applied")
        endif()
    endif()

    file(WRITE ${CALEPD_PATCH_MARKER} "Patched for CDC Badge")
endif()

# Patch libtropic for ESP-IDF Hardware GCM workaround
set(LIBTROPIC_DIR "${CMAKE_SOURCE_DIR}/third_party/libtropic")
set(LIBTROPIC_PATCH_MARKER "${LIBTROPIC_DIR}/.patched")

if(NOT EXISTS ${LIBTROPIC_PATCH_MARKER})
    set(PATCH_GCM "${CMAKE_SOURCE_DIR}/patches/libtropic_espidf_gcm_workaround.patch")
    if(EXISTS ${PATCH_GCM})
        execute_process(
            COMMAND patch -p1 -N -d ${LIBTROPIC_DIR} -i ${PATCH_GCM}
            RESULT_VARIABLE PATCH_RESULT
        )
        if(PATCH_RESULT EQUAL 0)
            message(STATUS "libtropic: ESP-IDF GCM workaround patch applied")
        endif()
    endif()

    file(WRITE ${LIBTROPIC_PATCH_MARKER} "Patched for ESP-IDF GCM")
endif()

# Exclude libtropic's upstream CMake project so only our libtropic_sdk wrapper compiles its sources.
set(EXCLUDE_COMPONENTS libtropic)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cdc-badge-updater)
