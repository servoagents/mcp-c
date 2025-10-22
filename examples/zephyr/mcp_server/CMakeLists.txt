cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mcp_zephyr_app)

# Include core headers
target_include_directories(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../../include
                                       ${CMAKE_CURRENT_LIST_DIR}/include)

# Pull in core sources from the library (keeps single codebase)
target_sources(
    app
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../../src/core/message.c
            ${CMAKE_CURRENT_LIST_DIR}/../../../src/core/session.c
            ${CMAKE_CURRENT_LIST_DIR}/../../../src/server/server.c
            ${CMAKE_CURRENT_LIST_DIR}/../../../src/transport/http_transport.c
            ${CMAKE_CURRENT_LIST_DIR}/src/main.c
            ${CMAKE_CURRENT_LIST_DIR}/src/wifi.c)

# Allow WiFi credentials to be provided via -D or environment
set(WIFI_SSID_VALUE $ENV{WIFI_SSID})
set(WIFI_PASS_VALUE $ENV{WIFI_PASS})

if (DEFINED WIFI_SSID)
    set(WIFI_SSID_VALUE ${WIFI_SSID})
endif ()

if (DEFINED WIFI_PASS)
    set(WIFI_PASS_VALUE ${WIFI_PASS})
endif ()

if (NOT WIFI_SSID_VALUE)
    set(WIFI_SSID_VALUE "WIFI_SSID_NOT_SET")
endif ()

if (NOT WIFI_PASS_VALUE)
    set(WIFI_PASS_VALUE "WIFI_PASS_NOT_SET")
endif ()

target_compile_definitions(app PRIVATE WIFI_SSID="${WIFI_SSID_VALUE}"
                                       WIFI_PASS="${WIFI_PASS_VALUE}")
