#ifndef MCP_TRANSPORT_H
#define MCP_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mcp_server;
struct mcp_session;

// Transport callbacks
typedef struct mcp_transport {
    const char *name;
    int (*init)(struct mcp_server *srv, void *cfg);
    int (*poll)(struct mcp_server *srv); // drive transport; returns 0 to continue, <0 to stop
    void (*close)(struct mcp_server *srv);

    // Send to a session/connection
    int (*send)(struct mcp_session *s, const uint8_t *data, size_t len);
} mcp_transport_t;

// HTTP transport factory
const mcp_transport_t *mcp_transport_http(void);

#ifdef __cplusplus
}
#endif

#endif // MCP_TRANSPORT_H
