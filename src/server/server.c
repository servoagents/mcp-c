#include "mcp_server.h"
#include "internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    char name[MCP_MAX_NAME_LENGTH];
    mcp_tool_t definition;
} tool_entry_t;

struct mcp_server {
    mcp_server_config_t config;
    tool_entry_t tools[MCP_MAX_TOOLS];
    size_t tool_count;
};

static const char *server_name(const mcp_server_t *server) {
    return server->config.name ? server->config.name : "mcp-c";
}

static const char *server_version(const mcp_server_t *server) {
    return server->config.version ? server->config.version : "0.2.0";
}

static int valid_tool_name(const char *name) {
    size_t length;
    size_t i;
    if (name == NULL) return 0;
    length = strlen(name);
    if (length == 0 || length >= MCP_MAX_NAME_LENGTH || length > 128) return 0;
    for (i = 0; i < length; ++i) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return 0;
    }
    return 1;
}

mcp_server_t *mcp_server_init(const mcp_server_config_t *config) {
    mcp_server_t *server = (mcp_server_t *)calloc(1, sizeof(*server));
    if (server == NULL) return NULL;
    if (config != NULL) server->config = *config;
    if (server->config.max_request_size == 0) server->config.max_request_size = 4096;
    if (server->config.discovery_ttl_ms == 0) server->config.discovery_ttl_ms = 3600000;
    if (server->config.tools_ttl_ms == 0) server->config.tools_ttl_ms = 300000;
    return server;
}

void mcp_server_deinit(mcp_server_t *server) { free(server); }

size_t mcp_server_max_request_size(const mcp_server_t *server) {
    return server ? server->config.max_request_size : 0;
}

int mcp_server_register_tool(mcp_server_t *server, const mcp_tool_t *tool) {
    tool_entry_t *entry;
    size_t i;
    if (server == NULL || tool == NULL || tool->handler == NULL ||
        !valid_tool_name(tool->name) || tool->description == NULL ||
        tool->input_schema_json == NULL || server->tool_count >= MCP_MAX_TOOLS) return MCP_ERR;
    if (mcp_json_value_is_valid(tool->input_schema_json, strlen(tool->input_schema_json),
                                MCP_JSON_OBJECT) != MCP_OK) return MCP_ERR_JSON;
    if (tool->output_schema_json != NULL &&
        mcp_json_value_is_valid(tool->output_schema_json, strlen(tool->output_schema_json),
                                MCP_JSON_OBJECT) != MCP_OK) return MCP_ERR_JSON;
    for (i = 0; i < server->tool_count; ++i) {
        if (strcmp(server->tools[i].name, tool->name) == 0) return MCP_ERR;
    }
    entry = &server->tools[server->tool_count++];
    memcpy(entry->name, tool->name, strlen(tool->name) + 1);
    entry->definition = *tool;
    entry->definition.name = entry->name;
    return MCP_OK;
}

static tool_entry_t *find_tool(mcp_server_t *server, const char *name) {
    size_t i;
    for (i = 0; i < server->tool_count; ++i) {
        if (strcmp(server->tools[i].name, name) == 0) return &server->tools[i];
    }
    return NULL;
}

static int write_id(mcp_json_writer_t *writer, const mcp_message_t *message) {
    if (message == NULL || message->id_type == MCP_ID_NONE) return mcp_json_null(writer);
    if (message->id_type == MCP_ID_STRING) {
        if (mcp_json_key(writer, "id") != MCP_OK) return MCP_ERR;
        /* Preserve the exact JSON representation, including escapes. */
        return mcp_json_raw(writer, message->id.ptr - 1, message->id.len + 2);
    }
    if (mcp_json_key(writer, "id") != MCP_OK) return MCP_ERR;
    return mcp_json_raw(writer, message->id.ptr, message->id.len);
}

static int response_start(mcp_json_writer_t *writer, const mcp_message_t *message,
                          int result) {
    if (mcp_json_begin_object(writer) != MCP_OK ||
        mcp_json_key(writer, "jsonrpc") != MCP_OK || mcp_json_string(writer, "2.0") != MCP_OK) {
        return MCP_ERR;
    }
    if (message != NULL && message->id_type != MCP_ID_NONE) {
        if (write_id(writer, message) != MCP_OK) return MCP_ERR;
    } else {
        if (mcp_json_key(writer, "id") != MCP_OK || mcp_json_null(writer) != MCP_OK) return MCP_ERR;
    }
    if (mcp_json_key(writer, result ? "result" : "error") != MCP_OK ||
        mcp_json_begin_object(writer) != MCP_OK) return MCP_ERR;
    return MCP_OK;
}

static int write_server_meta(mcp_json_writer_t *writer, const mcp_server_t *server) {
    return mcp_json_key(writer, "_meta") || mcp_json_begin_object(writer) ||
           mcp_json_key(writer, "io.modelcontextprotocol/serverInfo") ||
           mcp_json_begin_object(writer) || mcp_json_key(writer, "name") ||
           mcp_json_string(writer, server_name(server)) || mcp_json_key(writer, "version") ||
           mcp_json_string(writer, server_version(server)) || mcp_json_end_object(writer) ||
           mcp_json_end_object(writer);
}

static int response_finish(mcp_json_writer_t *writer) {
    if (mcp_json_end_object(writer) != MCP_OK || mcp_json_end_object(writer) != MCP_OK) return MCP_ERR;
    return mcp_json_writer_status(writer);
}

static mcp_dispatch_result_t result_for(mcp_dispatch_status_t status, size_t length,
                                        int error, int http_status) {
    mcp_dispatch_result_t result;
    result.status = status;
    result.response_length = length;
    result.jsonrpc_error = error;
    result.http_status = http_status;
    return result;
}

static mcp_dispatch_result_t write_error(char *response, size_t capacity,
                                         const mcp_message_t *message, int code,
                                         const char *text, mcp_dispatch_status_t status,
                                         int http_status, const mcp_server_t *server) {
    mcp_json_writer_t writer;
    mcp_json_writer_init(&writer, response, capacity);
    if (response_start(&writer, message, 0) != MCP_OK ||
        mcp_json_key(&writer, "code") != MCP_OK || mcp_json_int(&writer, code) != MCP_OK ||
        mcp_json_key(&writer, "message") != MCP_OK || mcp_json_string(&writer, text) != MCP_OK) {
        return result_for(MCP_DISPATCH_INTERNAL_ERROR, 0, -32603, 500);
    }
    if (code == -32022) {
        mcp_json_key(&writer, "data"); mcp_json_begin_object(&writer);
        mcp_json_key(&writer, "supported"); mcp_json_begin_array(&writer);
        mcp_json_string(&writer, MCP_PROTOCOL_VERSION); mcp_json_end_array(&writer);
        mcp_json_end_object(&writer);
    }
    (void)server;
    if (response_finish(&writer) != MCP_OK) {
        return result_for(MCP_DISPATCH_INTERNAL_ERROR, 0, -32603, 500);
    }
    return result_for(status, mcp_json_writer_length(&writer), code, http_status);
}

static int add_result_prefix(mcp_json_writer_t *writer) {
    return mcp_json_key(writer, "resultType") || mcp_json_string(writer, "complete");
}

static int write_discover(mcp_server_t *server, mcp_json_writer_t *writer) {
    if (add_result_prefix(writer) || mcp_json_key(writer, "supportedVersions") ||
        mcp_json_begin_array(writer) || mcp_json_string(writer, MCP_PROTOCOL_VERSION) ||
        mcp_json_end_array(writer) || mcp_json_key(writer, "capabilities") ||
        mcp_json_begin_object(writer) || mcp_json_key(writer, "tools") ||
        mcp_json_begin_object(writer) || mcp_json_end_object(writer) ||
        mcp_json_end_object(writer) || write_server_meta(writer, server)) return MCP_ERR;
    if (server->config.instructions != NULL) {
        if (mcp_json_key(writer, "instructions") ||
            mcp_json_string(writer, server->config.instructions)) return MCP_ERR;
    }
    return mcp_json_key(writer, "ttlMs") ||
           mcp_json_int(writer, (int64_t)server->config.discovery_ttl_ms) ||
           mcp_json_key(writer, "cacheScope") || mcp_json_string(writer, "public");
}

static int write_tools_list(mcp_server_t *server, mcp_json_writer_t *writer) {
    size_t i;
    if (add_result_prefix(writer) || mcp_json_key(writer, "tools") ||
        mcp_json_begin_array(writer)) return MCP_ERR;
    for (i = 0; i < server->tool_count; ++i) {
        const mcp_tool_t *tool = &server->tools[i].definition;
        if (mcp_json_begin_object(writer) || mcp_json_key(writer, "name") ||
            mcp_json_string(writer, tool->name)) return MCP_ERR;
        if (tool->title && (mcp_json_key(writer, "title") || mcp_json_string(writer, tool->title)))
            return MCP_ERR;
        if (mcp_json_key(writer, "description") || mcp_json_string(writer, tool->description) ||
            mcp_json_key(writer, "inputSchema") ||
            mcp_json_raw(writer, tool->input_schema_json, strlen(tool->input_schema_json)))
            return MCP_ERR;
        if (tool->output_schema_json &&
            (mcp_json_key(writer, "outputSchema") ||
             mcp_json_raw(writer, tool->output_schema_json, strlen(tool->output_schema_json))))
            return MCP_ERR;
        if (mcp_json_end_object(writer)) return MCP_ERR;
    }
    return mcp_json_end_array(writer) ||
           mcp_json_key(writer, "ttlMs") ||
           mcp_json_int(writer, (int64_t)server->config.tools_ttl_ms) ||
           mcp_json_key(writer, "cacheScope") || mcp_json_string(writer, "public") ||
           write_server_meta(writer, server);
}

static int validate_context_headers(const mcp_request_ctx_t *ctx,
                                    const mcp_message_t *message) {
    if (ctx == NULL) return MCP_OK;
    if (ctx->protocol_version_header != NULL &&
        strcmp(ctx->protocol_version_header, message->protocol_version) != 0) return MCP_ERR;
    if (ctx->method_header != NULL && strcmp(ctx->method_header, message->method) != 0)
        return MCP_ERR;
    if (ctx->name_header != NULL && strcmp(ctx->name_header, message->name) != 0)
        return MCP_ERR;
    return MCP_OK;
}

mcp_dispatch_result_t mcp_server_handle(mcp_server_t *server,
                                        const mcp_request_ctx_t *ctx,
                                        const char *request, size_t request_length,
                                        char *response, size_t response_capacity) {
    mcp_message_t message;
    mcp_json_writer_t writer;
    mcp_json_value_t arguments;
    tool_entry_t *tool;
    int parsed;
    int handler_result;
    if (server == NULL || request == NULL || response == NULL || response_capacity == 0)
        return result_for(MCP_DISPATCH_INTERNAL_ERROR, 0, -32603, 500);
    response[0] = '\0';
    if (request_length > server->config.max_request_size) {
        return write_error(response, response_capacity, NULL, -32600, "Request too large",
                           MCP_DISPATCH_TOO_LARGE, 413, server);
    }
    parsed = mcp_message_parse(request, request_length, &message);
    if (parsed == MCP_ERR_JSON || parsed == MCP_ERR_OVERFLOW) {
        return write_error(response, response_capacity, NULL, -32700, "Parse error",
                           MCP_DISPATCH_PARSE_ERROR, 400, server);
    }
    if (parsed != MCP_OK || message.type == MCP_MSG_RESPONSE) {
        return write_error(response, response_capacity, NULL, -32600, "Invalid Request",
                           MCP_DISPATCH_INVALID_REQUEST, 400, server);
    }
    if (message.protocol_version[0] == '\0' || !message.has_client_capabilities) {
        return write_error(response, response_capacity, &message, -32602,
                           "Missing required request metadata",
                           MCP_DISPATCH_INVALID_PARAMS, 400, server);
    }
    if (strcmp(message.protocol_version, MCP_PROTOCOL_VERSION) != 0) {
        return write_error(response, response_capacity, &message, -32022,
                           "Unsupported protocol version",
                           MCP_DISPATCH_UNSUPPORTED_VERSION, 400, server);
    }
    if (validate_context_headers(ctx, &message) != MCP_OK) {
        return write_error(response, response_capacity, &message, -32020,
                           "HTTP headers do not match the JSON-RPC body",
                           MCP_DISPATCH_HEADER_MISMATCH, 400, server);
    }
    if (message.type == MCP_MSG_NOTIFICATION) {
        return result_for(MCP_DISPATCH_NOTIFICATION, 0, 0, 202);
    }

    mcp_json_writer_init(&writer, response, response_capacity);
    if (response_start(&writer, &message, 1) != MCP_OK) goto overflow;
    if (strcmp(message.method, "server/discover") == 0) {
        if (write_discover(server, &writer) != MCP_OK) goto overflow;
    } else if (strcmp(message.method, "tools/list") == 0) {
        if (write_tools_list(server, &writer) != MCP_OK) goto overflow;
    } else if (strcmp(message.method, "tools/call") == 0) {
        if (message.name[0] == '\0') {
            return write_error(response, response_capacity, &message, -32602,
                               "tools/call requires params.name",
                               MCP_DISPATCH_INVALID_PARAMS, 200, server);
        }
        tool = find_tool(server, message.name);
        if (tool == NULL) {
            return write_error(response, response_capacity, &message, -32602,
                               "Unknown tool", MCP_DISPATCH_INVALID_PARAMS, 200, server);
        }
        if (mcp_json_object_get(&message.params, "arguments", &arguments) != MCP_OK) {
            arguments.type = MCP_JSON_OBJECT;
            arguments.ptr = "{}";
            arguments.len = 2;
        } else if (arguments.type != MCP_JSON_OBJECT) {
            return write_error(response, response_capacity, &message, -32602,
                               "Tool arguments must be an object",
                               MCP_DISPATCH_INVALID_PARAMS, 200, server);
        }
        if (add_result_prefix(&writer) != MCP_OK) goto overflow;
        handler_result = tool->definition.handler(ctx, &arguments, &writer,
                                                  tool->definition.tool_data);
        if (handler_result != MCP_OK || writer.error) {
            return write_error(response, response_capacity, &message, -32602,
                               "Invalid tool arguments",
                               MCP_DISPATCH_INVALID_PARAMS, 200, server);
        }
        if (write_server_meta(&writer, server) != MCP_OK) goto overflow;
    } else {
        return write_error(response, response_capacity, &message, -32601,
                           "Method not found", MCP_DISPATCH_RESPONSE, 200, server);
    }
    if (response_finish(&writer) != MCP_OK) goto overflow;
    return result_for(MCP_DISPATCH_RESPONSE, mcp_json_writer_length(&writer), 0, 200);

overflow:
    return write_error(response, response_capacity, &message, -32603,
                       "Response buffer exhausted", MCP_DISPATCH_INTERNAL_ERROR, 500, server);
}

int mcp_tool_result_text(mcp_json_writer_t *result, const char *text, int is_error) {
    return mcp_json_key(result, "content") || mcp_json_begin_array(result) ||
           mcp_json_begin_object(result) || mcp_json_key(result, "type") ||
           mcp_json_string(result, "text") || mcp_json_key(result, "text") ||
           mcp_json_string(result, text ? text : "") || mcp_json_end_object(result) ||
           mcp_json_end_array(result) || mcp_json_key(result, "isError") ||
           mcp_json_bool(result, is_error);
}

int mcp_tool_result_number(mcp_json_writer_t *result, const char *text, int64_t value,
                           int is_error) {
    return mcp_tool_result_text(result, text, is_error) ||
           mcp_json_key(result, "structuredContent") || mcp_json_begin_object(result) ||
           mcp_json_key(result, "value") || mcp_json_int(result, value) ||
           mcp_json_end_object(result);
}
