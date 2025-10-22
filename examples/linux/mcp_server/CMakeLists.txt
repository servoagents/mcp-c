cmake_minimum_required(VERSION 3.13)
project(mcp_linux_mcp_server C)

add_executable(mcp-linux-mcp_server src/main.c)

target_link_libraries(mcp-linux-mcp_server PRIVATE mcp)

target_include_directories(mcp-linux-mcp_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
