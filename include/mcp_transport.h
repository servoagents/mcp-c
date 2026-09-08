#ifndef MCP_TRANSPORT_H
#define MCP_TRANSPORT_H

#include "mcp_server.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mcp_transport mcp_transport_t;

typedef struct {
    int (*open)(mcp_transport_t *transport, mcp_server_t *server);
    int (*poll)(mcp_transport_t *transport, int timeout_ms);
    void (*close)(mcp_transport_t *transport);
} mcp_transport_vtable_t;

struct mcp_transport {
    const char *name;
    const mcp_transport_vtable_t *vtable;
    mcp_server_t *server;
    void *state;
};

int mcp_transport_open(mcp_transport_t *transport, mcp_server_t *server);
int mcp_transport_poll(mcp_transport_t *transport, int timeout_ms);
void mcp_transport_close(mcp_transport_t *transport);

typedef struct {
    const char *bind_address;
    uint16_t port;
    const char *allowed_origin;
    int backlog;
} mcp_http_config_t;

int mcp_http_transport_init(mcp_transport_t *transport, const mcp_http_config_t *config);

typedef struct {
    void *input;
    void *output;
} mcp_stdio_config_t;

int mcp_stdio_transport_init(mcp_transport_t *transport, const mcp_stdio_config_t *config);

/* Experimental custom MCP-over-CoAP binding backed by external libcoap. */
typedef struct {
    const char *bind_address;
    uint16_t port;
    size_t max_response_size;
} mcp_coap_config_t;

int mcp_coap_transport_init(mcp_transport_t *transport, const mcp_coap_config_t *config);

#ifdef __cplusplus
}
#endif

#endif
