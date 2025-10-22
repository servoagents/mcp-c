#ifndef MCP_SESSION_H
#define MCP_SESSION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mcp_session {
    int sock;            // transport socket or handle
    void *transport_ctx; // optional transport-specific
    void *user;          // user context
} mcp_session_t;

#ifdef __cplusplus
}
#endif

#endif // MCP_SESSION_H
