#ifndef MCP_SERVER_H
#define MCP_SERVER_H

#include "mcp.h"
#include "mcp_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MCP_MAX_TOOLS
#define MCP_MAX_TOOLS 16
#endif

typedef struct mcp_server mcp_server_t;

/* Per-exchange metadata. The core never interprets transport_context. */
typedef struct {
    const char *transport_name;
    void *transport_context;
    void *user_data;
    const char *protocol_version_header;
    const char *method_header;
    const char *name_header;
} mcp_request_ctx_t;

typedef enum {
    MCP_DISPATCH_RESPONSE = 0,
    MCP_DISPATCH_NOTIFICATION = 1,
    MCP_DISPATCH_PARSE_ERROR = -1,
    MCP_DISPATCH_INVALID_REQUEST = -2,
    MCP_DISPATCH_INVALID_PARAMS = -3,
    MCP_DISPATCH_UNSUPPORTED_VERSION = -4,
    MCP_DISPATCH_HEADER_MISMATCH = -5,
    MCP_DISPATCH_TOO_LARGE = -6,
    MCP_DISPATCH_INTERNAL_ERROR = -7
} mcp_dispatch_status_t;

typedef struct {
    mcp_dispatch_status_t status;
    size_t response_length;
    int jsonrpc_error;
    int http_status;
} mcp_dispatch_result_t;

typedef struct {
    const char *name;
    const char *version;
    const char *instructions;
    size_t max_request_size;
    unsigned long discovery_ttl_ms;
    unsigned long tools_ttl_ms;
    void *user_data;
} mcp_server_config_t;

/* Callback writes members inside an already-open MCP result object. */
typedef int (*mcp_tool_handler_t)(const mcp_request_ctx_t *ctx,
                                  const mcp_json_value_t *arguments,
                                  mcp_json_writer_t *result, void *tool_data);

typedef struct {
    const char *name;
    const char *title;
    const char *description;
    const char *input_schema_json;
    const char *output_schema_json;
    mcp_tool_handler_t handler;
    void *tool_data;
} mcp_tool_t;

mcp_server_t *mcp_server_init(const mcp_server_config_t *config);
void mcp_server_deinit(mcp_server_t *server);
int mcp_server_register_tool(mcp_server_t *server, const mcp_tool_t *tool);
size_t mcp_server_max_request_size(const mcp_server_t *server);

mcp_dispatch_result_t mcp_server_handle(mcp_server_t *server,
                                        const mcp_request_ctx_t *ctx,
                                        const char *request, size_t request_length,
                                        char *response, size_t response_capacity);

/* Convenience helpers for common tool results. */
int mcp_tool_result_text(mcp_json_writer_t *result, const char *text, int is_error);
int mcp_tool_result_number(mcp_json_writer_t *result, const char *text, int64_t value,
                           int is_error);

#ifdef __cplusplus
}
#endif

#endif
