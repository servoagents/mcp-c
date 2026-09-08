#include "mcp_mqtt.h"

#include <stdio.h>
#include <string.h>

#define META "\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\"," \
             "\"io.modelcontextprotocol/clientCapabilities\":{}}"

typedef struct {
    uint64_t now;
    int executions;
    int publishes;
    char topic[128];
    uint8_t correlation[32];
    size_t correlation_length;
    char payload[1024];
} test_state_t;

static uint64_t now_ms(void *user_data) { return ((test_state_t *)user_data)->now; }

static int publish(const char *topic, const uint8_t *payload, size_t payload_length,
                   const uint8_t *correlation_data, size_t correlation_length,
                   int qos, int retain, void *user_data) {
    test_state_t *state = (test_state_t *)user_data;
    if (strlen(topic) >= sizeof(state->topic) || payload_length >= sizeof(state->payload) ||
        correlation_length > sizeof(state->correlation) || qos != 1 || retain != 0) return MCP_ERR;
    strcpy(state->topic, topic);
    memcpy(state->payload, payload, payload_length);
    state->payload[payload_length] = '\0';
    memcpy(state->correlation, correlation_data, correlation_length);
    state->correlation_length = correlation_length;
    state->publishes++;
    return MCP_OK;
}

static int actuator(const mcp_request_ctx_t *ctx, const mcp_json_value_t *arguments,
                    mcp_json_writer_t *result, void *tool_data) {
    test_state_t *state = (test_state_t *)tool_data;
    (void)ctx;
    (void)arguments;
    state->executions++;
    return mcp_tool_result_text(result, "executed", 0);
}

int main(void) {
    const char *json = "{\"jsonrpc\":\"2.0\",\"id\":77,\"method\":\"tools/call\","
                       "\"params\":{\"name\":\"actuator\",\"arguments\":{}," META "}}";
    const uint8_t correlation[] = {0x00, 0x2a, 0xff};
    mcp_server_config_t server_config = {.max_request_size = 2048};
    mcp_mqtt_bridge_config_t bridge_config;
    mcp_mqtt_request_t request;
    mcp_tool_t tool;
    test_state_t state;
    mcp_server_t *server;
    mcp_mqtt_bridge_t *bridge;
    memset(&state, 0, sizeof(state));
    state.now = 1000;
    server = mcp_server_init(&server_config);
    memset(&tool, 0, sizeof(tool));
    tool.name = "actuator";
    tool.description = "Test physical side effect";
    tool.input_schema_json = "{\"type\":\"object\"}";
    tool.handler = actuator;
    tool.tool_data = &state;
    if (server == NULL || mcp_server_register_tool(server, &tool) != MCP_OK) return 1;
    memset(&bridge_config, 0, sizeof(bridge_config));
    bridge_config.server_id = "esp32-lab";
    bridge_config.allowed_response_topic_prefix = "mcp/v1/clients/";
    bridge_config.duplicate_cache_entries = 2;
    bridge_config.duplicate_lifetime_ms = 5000;
    bridge_config.max_response_size = sizeof(state.payload);
    bridge_config.publish = publish;
    bridge_config.now_ms = now_ms;
    bridge_config.user_data = &state;
    bridge = mcp_mqtt_bridge_init(server, &bridge_config);
    if (bridge == NULL || strcmp(mcp_mqtt_bridge_request_topic(bridge),
                                 "mcp/v1/servers/esp32-lab/requests") != 0) return 1;
    memset(&request, 0, sizeof(request));
    request.topic = mcp_mqtt_bridge_request_topic(bridge);
    request.response_topic = "mcp/v1/clients/test/replies";
    request.client_identity = "test-client";
    request.correlation_data = correlation;
    request.correlation_length = sizeof(correlation);
    request.payload = (const uint8_t *)json;
    request.payload_length = strlen(json);
    request.content_type = "application/json";
    request.payload_is_utf8 = 1;
    request.qos = 1;
    request.message_expiry_interval_s = 30;
    request.received_at_ms = state.now;

    if (mcp_mqtt_bridge_handle(bridge, &request) != MCP_MQTT_ACCEPTED ||
        state.executions != 1 || state.publishes != 1 ||
        memcmp(state.correlation, correlation, sizeof(correlation)) != 0 ||
        strstr(state.payload, "\"id\":77") == NULL) return 1;
    if (mcp_mqtt_bridge_handle(bridge, &request) != MCP_MQTT_DUPLICATE_REPLIED ||
        state.executions != 1 || state.publishes != 2) return 1;
    request.response_topic = "attacker/reply";
    if (mcp_mqtt_bridge_handle(bridge, &request) != MCP_MQTT_INVALID ||
        state.executions != 1) return 1;
    request.response_topic = "mcp/v1/clients/test/replies";
    state.now = 32000;
    if (mcp_mqtt_bridge_handle(bridge, &request) != MCP_MQTT_EXPIRED ||
        state.executions != 1) return 1;
    puts("mqtt binding replay/correlation tests: PASS");
    mcp_mqtt_bridge_deinit(bridge);
    mcp_server_deinit(server);
    return 0;
}
