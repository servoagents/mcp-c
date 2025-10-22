#ifndef MCP_SERVER_H
#define MCP_SERVER_H

#include "mcp.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque types
typedef struct mcp_server mcp_server_t;
typedef struct mcp_session mcp_session_t;

// Transport vtable forward decl
struct mcp_transport;

// Server config
typedef struct {
    const struct mcp_transport *transport; // selected transport
    void *transport_cfg;                   // transport-specific configuration
    size_t max_body;                       // maximum JSON body size
} mcp_server_config_t;

mcp_server_t *mcp_server_init(const mcp_server_config_t *cfg);
int mcp_server_register(mcp_server_t *srv, const char *method, mcp_handler_t handler);
int mcp_server_run(mcp_server_t *srv);
void mcp_server_deinit(mcp_server_t *srv);

// Session helpers
int mcp_session_send_json(mcp_session_t *s, const char *json, size_t len);

#ifdef __cplusplus
}
#endif

#endif // MCP_SERVER_H
