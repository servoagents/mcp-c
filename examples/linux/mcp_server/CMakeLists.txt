add_executable(mcp-server src/main.c ../../common/demo_tools.c)
target_link_libraries(mcp-server PRIVATE mcp)
target_include_directories(mcp-server PRIVATE ../../common)
target_compile_options(mcp-server PRIVATE -Wall -Wextra -Wpedantic)
