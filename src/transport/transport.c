#include "mcp_transport.h"

int mcp_transport_open(mcp_transport_t *transport, mcp_server_t *server) {
    if (transport == NULL || transport->vtable == NULL || transport->vtable->open == NULL ||
        server == NULL) return MCP_ERR;
    transport->server = server;
    return transport->vtable->open(transport, server);
}

int mcp_transport_poll(mcp_transport_t *transport, int timeout_ms) {
    if (transport == NULL || transport->vtable == NULL || transport->vtable->poll == NULL ||
        transport->server == NULL) return MCP_ERR;
    return transport->vtable->poll(transport, timeout_ms);
}

void mcp_transport_close(mcp_transport_t *transport) {
    if (transport == NULL) return;
    if (transport->vtable != NULL && transport->vtable->close != NULL)
        transport->vtable->close(transport);
    transport->server = NULL;
}
