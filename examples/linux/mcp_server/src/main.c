#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mcp_server.h"
#include "mcp_transport.h"
#include "mcp.h"

static int handle_initialize(struct mcp_session *s, const mcp_message_t *req, char *resp,
                             size_t cap) {
    (void) s;
    (void) req;
    const char *id = req && req->id ? req->id : "1";
    snprintf(resp, cap,
             "{\"jsonrpc\":\"2.0\",\"id\":\"%s\",\"result\":{\"capabilities\":{\"tools\":true,"
             "\"resources\":true},\"protocolVersion\":\"1.0\"}}",
             id);
    return 0;
}

static int handle_tools_list(struct mcp_session *s, const mcp_message_t *req, char *resp,
                             size_t cap) {
    (void) s;
    (void) req;
    const char *result = "{\"tools\":[{\"name\":\"echo\",\"description\":\"Echo "
                         "text\",\"input_schema\":{\"type\":\"object\",\"properties\":{\"text\":{"
                         "\"type\":\"string\"}},\"required\":[\"text\"]}}]}";
    const char *id = req && req->id ? req->id : "1";
    snprintf(resp, cap, "{\"jsonrpc\":\"2.0\",\"id\":\"%s\",\"result\":%s}", id, result);
    return 0;
}

static int handle_tools_call(struct mcp_session *s, const mcp_message_t *req, char *resp,
                             size_t cap) {
    (void) s;
    // naive parse: if method contains "echo", return given text or a default
    const char *out = "Hello from MCP!";
    const char *id = req && req->id ? req->id : "1";
    snprintf(resp, cap,
             "{\"jsonrpc\":\"2.0\",\"id\":\"%s\",\"result\":{\"content\":[{\"type\":\"text\","
             "\"text\":\"%s\"}]}}",
             id, out);
    return 0;
}

int main(void) {
    mcp_server_config_t cfg = {
        .transport = mcp_transport_http(),
        .transport_cfg = NULL,
        .max_body = 4096,
    };
    mcp_server_t *srv = mcp_server_init(&cfg);
    if (!srv) {
        fprintf(stderr, "Failed to init server\n");
        return 1;
    }

    mcp_server_register(srv, "initialize", handle_initialize);
    mcp_server_register(srv, "tools/list", handle_tools_list);
    mcp_server_register(srv, "tools/call", handle_tools_call);

    printf("MCP simple server running on HTTP :8080\n");
    mcp_server_run(srv);
    mcp_server_deinit(srv);
    return 0;
}
