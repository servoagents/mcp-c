#ifndef MCP_JSON_H
#define MCP_JSON_H

#include "mcp.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MCP_JSON_MAX_DEPTH
#define MCP_JSON_MAX_DEPTH 12
#endif

typedef struct {
    char *buffer;
    size_t capacity;
    size_t length;
    unsigned char depth;
    unsigned char container[MCP_JSON_MAX_DEPTH];
    unsigned char count[MCP_JSON_MAX_DEPTH];
    unsigned char after_key[MCP_JSON_MAX_DEPTH];
    int error;
} mcp_json_writer_t;

void mcp_json_writer_init(mcp_json_writer_t *writer, char *buffer, size_t capacity);
int mcp_json_begin_object(mcp_json_writer_t *writer);
int mcp_json_end_object(mcp_json_writer_t *writer);
int mcp_json_begin_array(mcp_json_writer_t *writer);
int mcp_json_end_array(mcp_json_writer_t *writer);
int mcp_json_key(mcp_json_writer_t *writer, const char *key);
int mcp_json_string(mcp_json_writer_t *writer, const char *value);
int mcp_json_string_n(mcp_json_writer_t *writer, const char *value, size_t length);
int mcp_json_int(mcp_json_writer_t *writer, int64_t value);
int mcp_json_bool(mcp_json_writer_t *writer, int value);
int mcp_json_null(mcp_json_writer_t *writer);
int mcp_json_raw(mcp_json_writer_t *writer, const char *json, size_t length);
int mcp_json_writer_status(const mcp_json_writer_t *writer);
size_t mcp_json_writer_length(const mcp_json_writer_t *writer);

#ifdef __cplusplus
}
#endif

#endif
