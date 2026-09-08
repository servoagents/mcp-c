cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mcp_zephyr_app)

set(MCP_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)
target_include_directories(
  app PRIVATE ${MCP_ROOT}/include ${MCP_ROOT}/third_party/jsmn
              ${MCP_ROOT}/examples/common ${CMAKE_CURRENT_LIST_DIR}/include)
target_sources(
  app PRIVATE
  ${MCP_ROOT}/src/core/message.c
  ${MCP_ROOT}/src/core/json_writer.c
  ${MCP_ROOT}/src/server/server.c
  ${MCP_ROOT}/src/transport/transport.c
  ${MCP_ROOT}/src/transport/http_transport.c
  ${MCP_ROOT}/src/transport/mqtt_binding.c
  ${MCP_ROOT}/examples/common/demo_tools.c
  ${CMAKE_CURRENT_LIST_DIR}/src/main.c
  ${CMAKE_CURRENT_LIST_DIR}/src/servo_backend.c)
if(CONFIG_WIFI)
  target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/wifi.c)
endif()
target_compile_definitions(app PRIVATE MCP_VERSION="0.2.0")

set(WIFI_SSID_VALUE $ENV{WIFI_SSID})
set(WIFI_PASS_VALUE $ENV{WIFI_PASS})
if(DEFINED WIFI_SSID)
  set(WIFI_SSID_VALUE ${WIFI_SSID})
endif()
if(DEFINED WIFI_PASS)
  set(WIFI_PASS_VALUE ${WIFI_PASS})
endif()
if(NOT WIFI_SSID_VALUE)
  set(WIFI_SSID_VALUE "WIFI_SSID_NOT_SET")
endif()
if(NOT WIFI_PASS_VALUE)
  set(WIFI_PASS_VALUE "WIFI_PASS_NOT_SET")
endif()
target_compile_definitions(app PRIVATE WIFI_SSID="${WIFI_SSID_VALUE}"
                                       WIFI_PASS="${WIFI_PASS_VALUE}")
