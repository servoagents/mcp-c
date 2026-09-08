#include "mcp_json.h"
#include "internal.h"

#include <stdio.h>
#include <string.h>

#define MCP_JSON_OBJECT_CONTAINER 1
#define MCP_JSON_ARRAY_CONTAINER 2

static int append_n(mcp_json_writer_t *writer, const char *text, size_t length) {
    if (writer->error) return writer->error;
    if (writer->length + length >= writer->capacity) {
        writer->error = MCP_ERR_OVERFLOW;
        return writer->error;
    }
    memcpy(writer->buffer + writer->length, text, length);
    writer->length += length;
    writer->buffer[writer->length] = '\0';
    return MCP_OK;
}

static int append_char(mcp_json_writer_t *writer, char c) {
    return append_n(writer, &c, 1);
}

static int before_value(mcp_json_writer_t *writer) {
    unsigned int level;
    if (writer->depth == 0) return writer->length == 0 ? MCP_OK : MCP_ERR_INVALID;
    level = writer->depth - 1;
    if (writer->container[level] == MCP_JSON_OBJECT_CONTAINER) {
        if (!writer->after_key[level]) return MCP_ERR_INVALID;
        writer->after_key[level] = 0;
        return MCP_OK;
    }
    if (writer->count[level] && append_char(writer, ',') != MCP_OK) return writer->error;
    writer->count[level]++;
    return MCP_OK;
}

static int append_escaped(mcp_json_writer_t *writer, const char *value, size_t length) {
    static const char hex[] = "0123456789abcdef";
    size_t i;
    if (append_char(writer, '"') != MCP_OK) return writer->error;
    for (i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)value[i];
        const char *escape = NULL;
        switch (c) {
        case '"': escape = "\\\""; break;
        case '\\': escape = "\\\\"; break;
        case '\b': escape = "\\b"; break;
        case '\f': escape = "\\f"; break;
        case '\n': escape = "\\n"; break;
        case '\r': escape = "\\r"; break;
        case '\t': escape = "\\t"; break;
        default: break;
        }
        if (escape) {
            if (append_n(writer, escape, 2) != MCP_OK) return writer->error;
        } else if (c < 0x20) {
            char sequence[] = {'\\', 'u', '0', '0', hex[c >> 4], hex[c & 15]};
            if (append_n(writer, sequence, sizeof(sequence)) != MCP_OK) return writer->error;
        } else if (append_char(writer, (char)c) != MCP_OK) return writer->error;
    }
    return append_char(writer, '"');
}

void mcp_json_writer_init(mcp_json_writer_t *writer, char *buffer, size_t capacity) {
    memset(writer, 0, sizeof(*writer));
    writer->buffer = buffer;
    writer->capacity = capacity;
    if (buffer == NULL || capacity == 0) writer->error = MCP_ERR_INVALID;
    else buffer[0] = '\0';
}

static int begin_container(mcp_json_writer_t *writer, unsigned char type, char opening) {
    if (before_value(writer) != MCP_OK) {
        writer->error = MCP_ERR_INVALID;
        return writer->error;
    }
    if (writer->depth >= MCP_JSON_MAX_DEPTH) {
        writer->error = MCP_ERR_OVERFLOW;
        return writer->error;
    }
    if (append_char(writer, opening) != MCP_OK) return writer->error;
    writer->container[writer->depth] = type;
    writer->count[writer->depth] = 0;
    writer->after_key[writer->depth] = 0;
    writer->depth++;
    return MCP_OK;
}

static int end_container(mcp_json_writer_t *writer, unsigned char type, char closing) {
    unsigned int level;
    if (writer->depth == 0) return writer->error = MCP_ERR_INVALID;
    level = writer->depth - 1;
    if (writer->container[level] != type || writer->after_key[level]) {
        return writer->error = MCP_ERR_INVALID;
    }
    writer->depth--;
    return append_char(writer, closing);
}

int mcp_json_begin_object(mcp_json_writer_t *writer) {
    return begin_container(writer, MCP_JSON_OBJECT_CONTAINER, '{');
}
int mcp_json_end_object(mcp_json_writer_t *writer) {
    return end_container(writer, MCP_JSON_OBJECT_CONTAINER, '}');
}
int mcp_json_begin_array(mcp_json_writer_t *writer) {
    return begin_container(writer, MCP_JSON_ARRAY_CONTAINER, '[');
}
int mcp_json_end_array(mcp_json_writer_t *writer) {
    return end_container(writer, MCP_JSON_ARRAY_CONTAINER, ']');
}

int mcp_json_key(mcp_json_writer_t *writer, const char *key) {
    unsigned int level;
    if (writer->depth == 0 || key == NULL) return writer->error = MCP_ERR_INVALID;
    level = writer->depth - 1;
    if (writer->container[level] != MCP_JSON_OBJECT_CONTAINER || writer->after_key[level]) {
        return writer->error = MCP_ERR_INVALID;
    }
    if (writer->count[level] && append_char(writer, ',') != MCP_OK) return writer->error;
    if (append_escaped(writer, key, strlen(key)) != MCP_OK || append_char(writer, ':') != MCP_OK) {
        return writer->error;
    }
    writer->count[level]++;
    writer->after_key[level] = 1;
    return MCP_OK;
}

int mcp_json_string_n(mcp_json_writer_t *writer, const char *value, size_t length) {
    if (value == NULL || before_value(writer) != MCP_OK) return writer->error = MCP_ERR_INVALID;
    return append_escaped(writer, value, length);
}
int mcp_json_string(mcp_json_writer_t *writer, const char *value) {
    return value ? mcp_json_string_n(writer, value, strlen(value)) : MCP_ERR_INVALID;
}
int mcp_json_int(mcp_json_writer_t *writer, int64_t value) {
    char buffer[32];
    int length;
    if (before_value(writer) != MCP_OK) return writer->error = MCP_ERR_INVALID;
    length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value);
    return length > 0 ? append_n(writer, buffer, (size_t)length) : (writer->error = MCP_ERR_INVALID);
}
int mcp_json_bool(mcp_json_writer_t *writer, int value) {
    if (before_value(writer) != MCP_OK) return writer->error = MCP_ERR_INVALID;
    return append_n(writer, value ? "true" : "false", value ? 4 : 5);
}
int mcp_json_null(mcp_json_writer_t *writer) {
    if (before_value(writer) != MCP_OK) return writer->error = MCP_ERR_INVALID;
    return append_n(writer, "null", 4);
}
int mcp_json_raw(mcp_json_writer_t *writer, const char *json, size_t length) {
    if (json == NULL || mcp_json_value_is_valid(json, length, MCP_JSON_UNDEFINED) != MCP_OK ||
        before_value(writer) != MCP_OK) return writer->error = MCP_ERR_INVALID;
    return append_n(writer, json, length);
}
int mcp_json_writer_status(const mcp_json_writer_t *writer) {
    return writer->error ? writer->error : (writer->depth == 0 ? MCP_OK : MCP_ERR_INVALID);
}
size_t mcp_json_writer_length(const mcp_json_writer_t *writer) { return writer->length; }
