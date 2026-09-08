#include "mcp_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define META "\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\"," \
             "\"io.modelcontextprotocol/clientCapabilities\":{}}"

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

static int echo_handler(const mcp_request_ctx_t *ctx, const mcp_json_value_t *arguments,
                        mcp_json_writer_t *result, void *data) {
    mcp_json_value_t text;
    char decoded[128];
    (void)ctx;
    (void)data;
    if (mcp_json_object_get(arguments, "text", &text) != MCP_OK ||
        mcp_json_get_string(&text, decoded, sizeof(decoded)) != MCP_OK) return MCP_ERR;
    return mcp_tool_result_text(result, decoded, 0);
}

static mcp_server_t *make_server(size_t maximum) {
    mcp_server_config_t config = {
        .name = "test-server",
        .version = "1.2.3",
        .instructions = "test",
        .max_request_size = maximum,
    };
    mcp_tool_t tool = {
        .name = "echo",
        .description = "Echo text",
        .input_schema_json =
            "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},"
            "\"required\":[\"text\"]}",
        .handler = echo_handler,
    };
    mcp_server_t *server = mcp_server_init(&config);
    CHECK(server != NULL);
    CHECK(mcp_server_register_tool(server, &tool) == MCP_OK);
    return server;
}

static mcp_dispatch_result_t dispatch(mcp_server_t *server, const char *request,
                                      char *response, size_t capacity) {
    mcp_request_ctx_t context;
    memset(&context, 0, sizeof(context));
    context.transport_name = "test";
    return mcp_server_handle(server, &context, request, strlen(request), response, capacity);
}

static void test_parser(void) {
    const char *numeric = "{\"jsonrpc\":\"2.0\",\"id\":42,\"method\":\"tools/call\","
                          "\"params\":{\"name\":\"echo\",\"arguments\":{\"nested\":{\"x\":1}}," META "}}";
    const char *escaped = "{\"jsonrpc\":\"2.0\",\"id\":\"a\\\\b\\\"c\","
                          "\"method\":\"tools\\u002fcall\",\"params\":{" META "}}";
    mcp_message_t message;
    mcp_json_value_t arguments;
    mcp_json_value_t nested;
    CHECK(mcp_message_parse(numeric, strlen(numeric), &message) == MCP_OK);
    CHECK(message.type == MCP_MSG_REQUEST);
    CHECK(message.id_type == MCP_ID_NUMBER);
    CHECK(strcmp(message.method, "tools/call") == 0);
    CHECK(strcmp(message.name, "echo") == 0);
    CHECK(mcp_json_object_get(&message.params, "arguments", &arguments) == MCP_OK);
    CHECK(mcp_json_object_get(&arguments, "nested", &nested) == MCP_OK);
    CHECK(nested.type == MCP_JSON_OBJECT);
    CHECK(mcp_message_parse(escaped, strlen(escaped), &message) == MCP_OK);
    CHECK(message.id_type == MCP_ID_STRING);
    CHECK(strcmp(message.method, "tools/call") == 0);
    CHECK(mcp_message_parse("{bad", 4, &message) == MCP_ERR_JSON);
    {
        const char *malformed[] = {
            "{\"jsonrpc\":\"2.0\" \"id\":1}",
            "{\"jsonrpc\" \"2.0\"}",
            "{\"jsonrpc\":\"2.0\",}",
            "{\"jsonrpc\":\"\\u12",
        };
        size_t i;
        for (i = 0; i < sizeof(malformed) / sizeof(malformed[0]); ++i) {
            CHECK(mcp_message_parse(malformed[i], strlen(malformed[i]), &message) == MCP_ERR_JSON);
        }
    }
    CHECK(mcp_message_parse("[]", 2, &message) == MCP_ERR_INVALID);
    CHECK(mcp_message_parse("{\"jsonrpc\":\"1.0\",\"id\":1,\"method\":\"x\"}",
                            strlen("{\"jsonrpc\":\"1.0\",\"id\":1,\"method\":\"x\"}"),
                            &message) == MCP_ERR_INVALID);
}

static void test_dispatch(void) {
    const char *discover = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"server/discover\","
                           "\"params\":{" META "}}";
    const char *list = "{\"jsonrpc\":\"2.0\",\"id\":\"list-1\",\"method\":\"tools/list\","
                       "\"params\":{" META "}}";
    const char *call = "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"tools/call\","
                       "\"params\":{\"name\":\"echo\",\"arguments\":{\"text\":\"line\\n"
                       "quoted: \\\"yes\\\"\"}," META "}}";
    const char *notification = "{\"jsonrpc\":\"2.0\",\"method\":\"noop\",\"params\":{" META "}}";
    const char *unknown = "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"unknown\","
                          "\"params\":{" META "}}";
    const char *missing_meta = "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"tools/list\",\"params\":{}}";
    const char *wrong_version = "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"tools/list\","
                                "\"params\":{\"_meta\":{"
                                "\"io.modelcontextprotocol/protocolVersion\":\"2025-11-25\","
                                "\"io.modelcontextprotocol/clientCapabilities\":{}}}}";
    char response[8192];
    mcp_dispatch_result_t result;
    mcp_server_t *server = make_server(4096);

    result = dispatch(server, discover, response, sizeof(response));
    CHECK(result.status == MCP_DISPATCH_RESPONSE);
    CHECK(strstr(response, "\"id\":1") != NULL);
    CHECK(strstr(response, "\"supportedVersions\":[\"2026-07-28\"]") != NULL);
    CHECK(strstr(response, "\"resultType\":\"complete\"") != NULL);
    CHECK(strstr(response, "io.modelcontextprotocol/serverInfo") != NULL);

    result = dispatch(server, list, response, sizeof(response));
    if (result.status != MCP_DISPATCH_RESPONSE)
        fprintf(stderr, "tools/list status=%d error=%d response=%s\n",
                result.status, result.jsonrpc_error, response);
    CHECK(result.status == MCP_DISPATCH_RESPONSE);
    CHECK(strstr(response, "\"id\":\"list-1\"") != NULL);
    CHECK(strstr(response, "\"inputSchema\"") != NULL);
    CHECK(strstr(response, "\"name\":\"echo\"") != NULL);

    result = dispatch(server, call, response, sizeof(response));
    CHECK(result.status == MCP_DISPATCH_RESPONSE);
    CHECK(strstr(response, "\"id\":7") != NULL);
    CHECK(strstr(response, "line\\nquoted: \\\"yes\\\"") != NULL);
    CHECK(strstr(response, "\"isError\":false") != NULL);

    result = dispatch(server, notification, response, sizeof(response));
    CHECK(result.status == MCP_DISPATCH_NOTIFICATION);
    CHECK(result.response_length == 0);

    result = dispatch(server, unknown, response, sizeof(response));
    CHECK(result.jsonrpc_error == -32601);
    result = dispatch(server, missing_meta, response, sizeof(response));
    CHECK(result.status == MCP_DISPATCH_INVALID_PARAMS);
    CHECK(result.http_status == 400);
    result = dispatch(server, wrong_version, response, sizeof(response));
    CHECK(result.status == MCP_DISPATCH_UNSUPPORTED_VERSION);
    CHECK(strstr(response, "\"supported\":[\"2026-07-28\"]") != NULL);

    result = dispatch(server, "{", response, sizeof(response));
    CHECK(result.jsonrpc_error == -32700);
    result = dispatch(server, "{\"jsonrpc\":\"2.0\",\"id\":1}", response, sizeof(response));
    CHECK(result.jsonrpc_error == -32600);
    mcp_server_deinit(server);
}

static void test_headers_and_limits(void) {
    const char *request = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\","
                          "\"params\":{" META "}}";
    char with_space[1024];
    char response[2048];
    mcp_request_ctx_t context;
    mcp_dispatch_result_t result;
    mcp_server_t *server = make_server(strlen(request));
    memset(&context, 0, sizeof(context));
    context.protocol_version_header = MCP_PROTOCOL_VERSION;
    context.method_header = "tools/list";
    result = mcp_server_handle(server, &context, request, strlen(request), response, sizeof(response));
    CHECK(result.status == MCP_DISPATCH_RESPONSE);
    memcpy(with_space, request, strlen(request));
    with_space[strlen(request)] = ' ';
    with_space[strlen(request) + 1] = '\0';
    result = mcp_server_handle(server, &context, with_space, strlen(request) + 1,
                               response, sizeof(response));
    CHECK(result.status == MCP_DISPATCH_TOO_LARGE);
    context.method_header = "tools/call";
    result = mcp_server_handle(server, &context, request, strlen(request), response, sizeof(response));
    CHECK(result.status == MCP_DISPATCH_HEADER_MISMATCH);
    mcp_server_deinit(server);
}

static void test_writer_bounds(void) {
    char small[10];
    mcp_json_writer_t writer;
    mcp_json_writer_init(&writer, small, sizeof(small));
    CHECK(mcp_json_begin_object(&writer) == MCP_OK);
    CHECK(mcp_json_key(&writer, "long") == MCP_OK);
    CHECK(mcp_json_string(&writer, "value") == MCP_ERR_OVERFLOW);
    CHECK(mcp_json_writer_status(&writer) == MCP_ERR_OVERFLOW);
}

int main(void) {
    test_parser();
    test_dispatch();
    test_headers_and_limits();
    test_writer_bounds();
    if (failures != 0) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    puts("core protocol tests: PASS");
    return 0;
}
