add_executable(mcp-core-tests test_core.c)
target_link_libraries(mcp-core-tests PRIVATE mcp)
target_compile_options(mcp-core-tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core COMMAND mcp-core-tests)

add_executable(mcp-mqtt-binding-tests test_mqtt_binding.c)
target_link_libraries(mcp-mqtt-binding-tests PRIVATE mcp)
target_compile_options(mcp-mqtt-binding-tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME mqtt-binding COMMAND mcp-mqtt-binding-tests)

find_program(BASH_EXECUTABLE bash)
if(BASH_EXECUTABLE)
  add_test(NAME stdio-integration
           COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/integration_stdio.sh
                   $<TARGET_FILE:mcp-server>)
endif()

find_program(CURL_EXECUTABLE curl)
find_program(COAP_CLIENT_EXECUTABLE coap-client)
if(MCP_HAVE_COAP AND BASH_EXECUTABLE AND CURL_EXECUTABLE AND COAP_CLIENT_EXECUTABLE)
  add_test(NAME transport-integration
           COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/integration_transports.sh
                   $<TARGET_FILE:mcp-server>)
  set_tests_properties(transport-integration PROPERTIES TIMEOUT 30)
endif()
