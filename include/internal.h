#ifndef MCP_INTERNAL_H
#define MCP_INTERNAL_H

#include "mcp_server.h"

int mcp_json_value_is_valid(const char *json, size_t length, mcp_json_type_t required_type);

#endif
