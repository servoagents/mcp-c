#ifndef MCP_INTERNAL_H
#define MCP_INTERNAL_H

#include "mcp_server.h"
#include "mcp_session.h"

// Internal helper exported for transport->poll to dispatch
int mcp_server_dispatch(mcp_server_t *srv, mcp_session_t *sess, const char *json, size_t len);

#endif
