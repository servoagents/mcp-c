cmake_minimum_required(VERSION 3.16)
project(mcp-c VERSION 0.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

option(MCP_ENABLE_EXAMPLES "Build examples" ON)
option(MCP_ENABLE_TESTS "Build tests" ON)
option(MCP_ENABLE_COAP "Build the optional libcoap transport" ON)
option(MCP_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(
  mcp
  src/core/message.c
  src/core/json_writer.c
  src/server/server.c
  src/transport/transport.c
  src/transport/mqtt_binding.c
  src/transport/http_transport.c
  src/transport/stdio_transport.c)

target_include_directories(
  mcp
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/jsmn)
target_compile_definitions(mcp PUBLIC MCP_VERSION="${PROJECT_VERSION}")
target_compile_options(mcp PRIVATE -Wall -Wextra -Wpedantic)

if(MCP_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mcp PUBLIC -fno-omit-frame-pointer -fsanitize=address,undefined)
  target_link_options(mcp PUBLIC -fno-omit-frame-pointer -fsanitize=address,undefined)
endif()

set(MCP_HAVE_COAP OFF)
if(MCP_ENABLE_COAP)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(LIBCOAP QUIET IMPORTED_TARGET libcoap-3)
  endif()
  if(LIBCOAP_FOUND)
    target_sources(mcp PRIVATE src/transport/coap_transport.c)
    target_link_libraries(mcp PUBLIC PkgConfig::LIBCOAP)
    target_compile_definitions(mcp PUBLIC MCP_HAVE_COAP=1)
    set(MCP_HAVE_COAP ON)
    message(STATUS "libcoap transport enabled (${LIBCOAP_VERSION})")
  else()
    message(STATUS "libcoap not found; CoAP transport disabled")
  endif()
endif()

if(MCP_ENABLE_EXAMPLES)
  add_subdirectory(examples/linux/mcp_server)
endif()

if(MCP_ENABLE_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
