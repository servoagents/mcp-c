#ifndef MCP_H
#define MCP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

// Basic error codes
#define MCP_OK 0
#define MCP_ERR -1

// Forward declarations
struct mcp_server;
struct mcp_session;
struct mcp_message;

// JSON-RPC message type
typedef enum {
    MCP_MSG_REQUEST,
    MCP_MSG_RESPONSE,
    MCP_MSG_NOTIFICATION,
    MCP_MSG_INVALID
} mcp_message_type_t;

// Minimal message container with raw JSON text and cached fields
typedef struct mcp_message {
    mcp_message_type_t type;
    const char *id;     // points into buffer
    const char *method; // for requests/notifications
    const char *raw;    // owned buffer with JSON text
    size_t raw_len;
} mcp_message_t;

// Handler signature
typedef int (*mcp_handler_t)(struct mcp_session *session, const mcp_message_t *req, char *resp_buf,
                             size_t resp_buf_sz);

// Message helpers
int mcp_message_parse(const char *raw, size_t raw_len, mcp_message_t *out);
void mcp_message_free(mcp_message_t *msg);

#ifdef __cplusplus
}
#endif

#endif // MCP_H
