#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mcp.h"

// Minimal helpers to find fields in raw JSON without full parse (assumes well-formed small JSON)
static const char *find_json_string_field(const char *json, const char *key) {
    const char *p = strstr(json, key);
    if (!p)
        return NULL;
    p = strchr(p, ':');
    if (!p)
        return NULL;
    // find first quote after colon
    const char *q = strchr(p, '"');
    if (!q)
        return NULL;
    q++;
    const char *end = strchr(q, '"');
    if (!end)
        return NULL;
    size_t len = (size_t) (end - q);
    char *buf = (char *) malloc(len + 1);
    if (!buf)
        return NULL;
    memcpy(buf, q, len);
    buf[len] = '\0';
    return buf; // caller must free
}

int mcp_message_parse(const char *raw, size_t raw_len, mcp_message_t *out) {
    if (!raw || !out)
        return MCP_ERR;
    char *copy = (char *) malloc(raw_len + 1);
    if (!copy)
        return MCP_ERR;
    memcpy(copy, raw, raw_len);
    copy[raw_len] = '\0';

    memset(out, 0, sizeof(*out));
    out->raw = copy;
    out->raw_len = raw_len;

    const char *method = find_json_string_field(copy, "\"method\"");
    const char *id = find_json_string_field(copy, "\"id\"");

    if (method && id) {
        out->type = MCP_MSG_REQUEST;
        out->method = method;
        out->id = id;
    } else if (method && !id) {
        out->type = MCP_MSG_NOTIFICATION;
        out->method = method;
    } else if (!method && id) {
        out->type = MCP_MSG_RESPONSE;
        out->id = id;
    } else {
        out->type = MCP_MSG_INVALID;
    }
    return MCP_OK;
}

void mcp_message_free(mcp_message_t *msg) {
    if (!msg)
        return;
    if (msg->raw)
        free((void *) msg->raw);
    if (msg->id)
        free((void *) msg->id);
    if (msg->method)
        free((void *) msg->method);
    memset(msg, 0, sizeof(*msg));
}
