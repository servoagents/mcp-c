#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "mcp_server.h"
#include "mcp_transport.h"
#include "mcp_session.h"
#include "mcp.h"

#define MAX_METHODS 16

typedef struct {
    char name[64];
    mcp_handler_t handler;
} method_entry_t;

struct mcp_server {
    const mcp_transport_t *transport;
    void *transport_cfg;
    size_t max_body;
    method_entry_t methods[MAX_METHODS];
    size_t method_count;
};

static method_entry_t *find_method(struct mcp_server *srv, const char *name) {
    for (size_t i = 0; i < srv->method_count; ++i) {
        if (strcmp(srv->methods[i].name, name) == 0)
            return &srv->methods[i];
    }
    return NULL;
}

mcp_server_t *mcp_server_init(const mcp_server_config_t *cfg) {
    if (!cfg || !cfg->transport)
        return NULL;
    mcp_server_t *srv = (mcp_server_t *) calloc(1, sizeof(*srv));
    if (!srv)
        return NULL;
    srv->transport = cfg->transport;
    srv->transport_cfg = cfg->transport_cfg;
    srv->max_body = cfg->max_body ? cfg->max_body : 4096;
    if (srv->transport->init(srv, srv->transport_cfg) != 0) {
        free(srv);
        return NULL;
    }
    return srv;
}

int mcp_server_register(mcp_server_t *srv, const char *method, mcp_handler_t handler) {
    if (!srv || !method || !handler)
        return MCP_ERR;
    if (srv->method_count >= MAX_METHODS)
        return MCP_ERR;
    strncpy(srv->methods[srv->method_count].name, method, sizeof(srv->methods[0].name) - 1);
    srv->methods[srv->method_count].handler = handler;
    srv->method_count++;
    return MCP_OK;
}

// Called by transports to handle a received JSON request and write response
int mcp_server_dispatch(mcp_server_t *srv, mcp_session_t *sess, const char *json, size_t len) {
    mcp_message_t msg;
    if (mcp_message_parse(json, len, &msg) != MCP_OK)
        return MCP_ERR;

    int rc = MCP_ERR;
    char resp[1024];
    resp[0] = '\0';

    if (msg.type == MCP_MSG_REQUEST && msg.method) {
        method_entry_t *me = find_method(srv, msg.method);
        if (me && me->handler) {
            rc = me->handler(sess, &msg, resp, sizeof(resp));
        } else {
            snprintf(resp, sizeof(resp),
                     "{\"jsonrpc\":\"2.0\",\"id\":\"%s\",\"error\":{\"code\":-32601,\"message\":"
                     "\"Method not found\"}}",
                     msg.id ? msg.id : "0");
            rc = MCP_OK;
        }
        if (resp[0]) {
            srv->transport->send(sess, (const uint8_t *) resp, strlen(resp));
        }
    } else if (msg.type == MCP_MSG_NOTIFICATION) {
        // ignore
        rc = MCP_OK;
    } else if (msg.type == MCP_MSG_RESPONSE) {
        // server doesn't expect responses
        rc = MCP_OK;
    } else {
        rc = MCP_ERR;
    }

    mcp_message_free(&msg);
    return rc;
}

int mcp_server_run(mcp_server_t *srv) {
    if (!srv)
        return MCP_ERR;
    while (1) {
        int r = srv->transport->poll(srv);
        if (r < 0)
            break;
    }
    return MCP_OK;
}

void mcp_server_deinit(mcp_server_t *srv) {
    if (!srv)
        return;
    if (srv->transport && srv->transport->close)
        srv->transport->close(srv);
    free(srv);
}

__attribute__((weak)) int mcp_server_dispatch(mcp_server_t *srv, mcp_session_t *sess,
                                              const char *json, size_t len);
