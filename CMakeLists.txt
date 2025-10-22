cmake_minimum_required(VERSION 3.13)
project(mcp-c C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS ON)

option(MCP_ENABLE_EXAMPLES "Build MCP examples" ON)

add_library(mcp src/core/message.c src/core/session.c src/server/server.c
                src/transport/http_transport.c)

target_include_directories(mcp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (MCP_ENABLE_EXAMPLES)
    # Linux example: examples/linux/mcp_server
    add_subdirectory(examples/linux/mcp_server)
endif ()
