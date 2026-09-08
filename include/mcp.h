#ifndef MCP_H
#define MCP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define MCP_PROTOCOL_VERSION "2026-07-28"

#ifndef MCP_MAX_JSON_TOKENS
#define MCP_MAX_JSON_TOKENS 128
#endif

#ifndef MCP_MAX_METHOD_LENGTH
#define MCP_MAX_METHOD_LENGTH 64
#endif

#ifndef MCP_MAX_NAME_LENGTH
#define MCP_MAX_NAME_LENGTH 128
#endif

#define MCP_OK 0
#define MCP_ERR (-1)
#define MCP_ERR_OVERFLOW (-2)
#define MCP_ERR_JSON (-3)
#define MCP_ERR_INVALID (-4)

typedef enum {
    MCP_JSON_UNDEFINED = 0,
    MCP_JSON_OBJECT,
    MCP_JSON_ARRAY,
    MCP_JSON_STRING,
    MCP_JSON_PRIMITIVE
} mcp_json_type_t;

/* A non-owning view. Strings exclude quotes; other values include full JSON. */
typedef struct {
    mcp_json_type_t type;
    const char *ptr;
    size_t len;
} mcp_json_value_t;

typedef enum {
    MCP_MSG_REQUEST,
    MCP_MSG_NOTIFICATION,
    MCP_MSG_RESPONSE,
    MCP_MSG_INVALID
} mcp_message_type_t;

typedef enum {
    MCP_ID_NONE,
    MCP_ID_STRING,
    MCP_ID_NUMBER
} mcp_id_type_t;

/* Parsed message fields are views into the caller-owned raw JSON buffer. */
typedef struct mcp_message {
    mcp_message_type_t type;
    mcp_id_type_t id_type;
    mcp_json_value_t id;
    mcp_json_value_t params;
    char method[MCP_MAX_METHOD_LENGTH];
    char name[MCP_MAX_NAME_LENGTH];
    char protocol_version[16];
    int has_client_capabilities;
    const char *raw;
    size_t raw_len;
} mcp_message_t;

int mcp_message_parse(const char *raw, size_t raw_len, mcp_message_t *out);

/* Bounded JSON accessors. Object lookup only examines direct children. */
int mcp_json_object_get(const mcp_json_value_t *object, const char *key,
                        mcp_json_value_t *value);
int mcp_json_get_string(const mcp_json_value_t *value, char *out, size_t out_size);
int mcp_json_get_int(const mcp_json_value_t *value, int *out);
int mcp_json_get_bool(const mcp_json_value_t *value, int *out);

#ifdef __cplusplus
}
#endif

#endif
