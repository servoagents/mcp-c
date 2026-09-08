#include "mcp.h"
#include "internal.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define JSMN_STRICT
#include "jsmn.h"

static mcp_json_type_t public_type(jsmntype_t type) {
    switch (type) {
    case JSMN_OBJECT: return MCP_JSON_OBJECT;
    case JSMN_ARRAY: return MCP_JSON_ARRAY;
    case JSMN_STRING: return MCP_JSON_STRING;
    case JSMN_PRIMITIVE: return MCP_JSON_PRIMITIVE;
    default: return MCP_JSON_UNDEFINED;
    }
}

static int whitespace_only(const char *text, size_t begin, size_t end) {
    size_t i;
    for (i = begin; i < end; ++i) {
        if (text[i] != ' ' && text[i] != '\t' && text[i] != '\r' && text[i] != '\n') {
            return 0;
        }
    }
    return 1;
}

static int primitive_valid(const char *text, size_t length) {
    size_t i = 0;
    if ((length == 4 && memcmp(text, "true", 4) == 0) ||
        (length == 5 && memcmp(text, "false", 5) == 0) ||
        (length == 4 && memcmp(text, "null", 4) == 0)) {
        return 1;
    }
    if (length == 0) return 0;
    if (text[i] == '-') {
        if (++i == length) return 0;
    }
    if (text[i] == '0') {
        ++i;
    } else if (text[i] >= '1' && text[i] <= '9') {
        while (i < length && text[i] >= '0' && text[i] <= '9') ++i;
    } else {
        return 0;
    }
    if (i < length && text[i] == '.') {
        ++i;
        if (i == length || text[i] < '0' || text[i] > '9') return 0;
        while (i < length && text[i] >= '0' && text[i] <= '9') ++i;
    }
    if (i < length && (text[i] == 'e' || text[i] == 'E')) {
        ++i;
        if (i < length && (text[i] == '+' || text[i] == '-')) ++i;
        if (i == length || text[i] < '0' || text[i] > '9') return 0;
        while (i < length && text[i] >= '0' && text[i] <= '9') ++i;
    }
    return i == length;
}

static size_t skip_whitespace(const char *json, size_t position, size_t limit) {
    while (position < limit &&
           (json[position] == ' ' || json[position] == '\t' ||
            json[position] == '\r' || json[position] == '\n')) {
        ++position;
    }
    return position;
}

/*
 * jsmn deliberately tokenizes rather than enforcing every JSON separator.
 * Verify the gaps between its tokens so inputs such as missing commas or
 * trailing commas cannot become ambiguous protocol messages.
 */
static int validate_token_layout(const char *json, const jsmntok_t *tokens,
                                 int count, int index, size_t *position,
                                 int *next_index) {
    const jsmntok_t *token;
    int child;
    int i;
    size_t pos;
    if (index < 0 || index >= count) return MCP_ERR_JSON;
    token = &tokens[index];
    pos = skip_whitespace(json, *position, (size_t)token->end + 1U);

    if (token->type == JSMN_STRING) {
        if (token->start <= 0 || pos + 1U != (size_t)token->start ||
            json[pos] != '"' || (size_t)token->end >= (size_t)INT_MAX ||
            json[token->end] != '"') return MCP_ERR_JSON;
        *position = (size_t)token->end + 1U;
        *next_index = index + 1;
        return MCP_OK;
    }
    if (token->type == JSMN_PRIMITIVE) {
        if (pos != (size_t)token->start || token->end < token->start ||
            !primitive_valid(json + token->start,
                             (size_t)(token->end - token->start))) return MCP_ERR_JSON;
        *position = (size_t)token->end;
        *next_index = index + 1;
        return MCP_OK;
    }
    if (token->type != JSMN_OBJECT && token->type != JSMN_ARRAY) return MCP_ERR_JSON;
    if (pos != (size_t)token->start || token->end <= token->start + 1 ||
        json[pos] != (token->type == JSMN_OBJECT ? '{' : '[') ||
        json[token->end - 1] != (token->type == JSMN_OBJECT ? '}' : ']')) {
        return MCP_ERR_JSON;
    }

    pos++;
    i = index + 1;
    for (child = 0; child < token->size; ++child) {
        pos = skip_whitespace(json, pos, (size_t)token->end);
        if (child != 0) {
            if (pos >= (size_t)token->end || json[pos] != ',') return MCP_ERR_JSON;
            pos = skip_whitespace(json, pos + 1U, (size_t)token->end);
        }
        if (token->type == JSMN_OBJECT) {
            if (i >= count || tokens[i].type != JSMN_STRING) return MCP_ERR_JSON;
            if (validate_token_layout(json, tokens, count, i, &pos, &i) != MCP_OK) {
                return MCP_ERR_JSON;
            }
            pos = skip_whitespace(json, pos, (size_t)token->end);
            if (pos >= (size_t)token->end || json[pos] != ':') return MCP_ERR_JSON;
            pos++;
            if (validate_token_layout(json, tokens, count, i, &pos, &i) != MCP_OK) {
                return MCP_ERR_JSON;
            }
        } else if (validate_token_layout(json, tokens, count, i, &pos, &i) != MCP_OK) {
            return MCP_ERR_JSON;
        }
    }
    pos = skip_whitespace(json, pos, (size_t)token->end);
    if (pos + 1U != (size_t)token->end) return MCP_ERR_JSON;
    *position = (size_t)token->end;
    *next_index = i;
    return MCP_OK;
}

static int parse_tokens(const char *json, size_t length, jsmntok_t *tokens, size_t capacity) {
    jsmn_parser parser;
    int count;
    int i;
    int next;
    size_t position = 0;
    if (json == NULL || length == 0 || length > (size_t)INT_MAX) return MCP_ERR_JSON;
    jsmn_init(&parser);
    count = jsmn_parse(&parser, json, length, tokens, (unsigned int)capacity);
    if (count <= 0) return count == JSMN_ERROR_NOMEM ? MCP_ERR_OVERFLOW : MCP_ERR_JSON;
    if (validate_token_layout(json, tokens, count, 0, &position, &next) != MCP_OK ||
        next != count || !whitespace_only(json, position, length)) return MCP_ERR_JSON;
    {
        size_t root_start = (size_t)tokens[0].start;
        size_t root_end = (size_t)tokens[0].end;
        if (tokens[0].type == JSMN_STRING) {
            if (root_start == 0 || root_end >= length) return MCP_ERR_JSON;
            root_start--;
            root_end++;
        }
        if (!whitespace_only(json, 0, root_start) ||
            !whitespace_only(json, root_end, length)) return MCP_ERR_JSON;
    }
    for (i = 0; i < count; ++i) {
        if (tokens[i].type == JSMN_PRIMITIVE &&
            !primitive_valid(json + tokens[i].start,
                             (size_t)(tokens[i].end - tokens[i].start))) {
            return MCP_ERR_JSON;
        }
    }
    return count;
}

static mcp_json_value_t token_value(const char *json, const jsmntok_t *token) {
    mcp_json_value_t value;
    value.type = public_type(token->type);
    value.ptr = json + token->start;
    value.len = (size_t)(token->end - token->start);
    return value;
}

static int object_get_tokens(const char *json, const jsmntok_t *tokens, int count,
                             const char *key, mcp_json_value_t *value) {
    int i = 1;
    size_t key_len = strlen(key);
    if (tokens[0].type != JSMN_OBJECT) return MCP_ERR_INVALID;
    while (i + 1 < count && tokens[i].start < tokens[0].end) {
        int value_index = i + 1;
        int next = value_index + 1;
        if (tokens[i].type != JSMN_STRING) return MCP_ERR_JSON;
        if ((size_t)(tokens[i].end - tokens[i].start) == key_len &&
            memcmp(json + tokens[i].start, key, key_len) == 0) {
            *value = token_value(json, &tokens[value_index]);
            return MCP_OK;
        }
        while (next < count && tokens[next].start < tokens[value_index].end) ++next;
        i = next;
    }
    return MCP_ERR_INVALID;
}

int mcp_json_object_get(const mcp_json_value_t *object, const char *key,
                        mcp_json_value_t *value) {
    jsmntok_t tokens[MCP_MAX_JSON_TOKENS];
    int count;
    if (object == NULL || key == NULL || value == NULL || object->type != MCP_JSON_OBJECT) {
        return MCP_ERR_INVALID;
    }
    count = parse_tokens(object->ptr, object->len, tokens, MCP_MAX_JSON_TOKENS);
    if (count < 0) return count;
    return object_get_tokens(object->ptr, tokens, count, key, value);
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int append_utf8(unsigned int cp, char *out, size_t out_size, size_t *position) {
    unsigned char bytes[4];
    size_t count;
    size_t i;
    if (cp <= 0x7f) {
        bytes[0] = (unsigned char)cp; count = 1;
    } else if (cp <= 0x7ff) {
        bytes[0] = (unsigned char)(0xc0 | (cp >> 6));
        bytes[1] = (unsigned char)(0x80 | (cp & 0x3f)); count = 2;
    } else if (cp <= 0xffff && !(cp >= 0xd800 && cp <= 0xdfff)) {
        bytes[0] = (unsigned char)(0xe0 | (cp >> 12));
        bytes[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3f));
        bytes[2] = (unsigned char)(0x80 | (cp & 0x3f)); count = 3;
    } else if (cp <= 0x10ffff) {
        bytes[0] = (unsigned char)(0xf0 | (cp >> 18));
        bytes[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3f));
        bytes[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3f));
        bytes[3] = (unsigned char)(0x80 | (cp & 0x3f)); count = 4;
    } else {
        return MCP_ERR_JSON;
    }
    if (*position + count >= out_size) return MCP_ERR_OVERFLOW;
    for (i = 0; i < count; ++i) out[(*position)++] = (char)bytes[i];
    return MCP_OK;
}

int mcp_json_get_string(const mcp_json_value_t *value, char *out, size_t out_size) {
    size_t i;
    size_t pos = 0;
    if (value == NULL || out == NULL || out_size == 0 || value->type != MCP_JSON_STRING) {
        return MCP_ERR_INVALID;
    }
    for (i = 0; i < value->len; ++i) {
        unsigned int cp;
        int h0, h1, h2, h3;
        char c = value->ptr[i];
        if (c != '\\') {
            if (pos + 1 >= out_size) return MCP_ERR_OVERFLOW;
            out[pos++] = c;
            continue;
        }
        if (++i >= value->len) return MCP_ERR_JSON;
        c = value->ptr[i];
        if (c == '"' || c == '\\' || c == '/') {
            if (pos + 1 >= out_size) return MCP_ERR_OVERFLOW;
            out[pos++] = c;
        } else if (c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't') {
            const char decoded[] = {'\b', '\f', '\n', '\r', '\t'};
            const char encoded[] = {'b', 'f', 'n', 'r', 't'};
            size_t k;
            if (pos + 1 >= out_size) return MCP_ERR_OVERFLOW;
            for (k = 0; k < sizeof(encoded); ++k) if (c == encoded[k]) break;
            out[pos++] = decoded[k];
        } else if (c == 'u') {
            if (i + 4 >= value->len) return MCP_ERR_JSON;
            h0 = hex_value(value->ptr[i + 1]); h1 = hex_value(value->ptr[i + 2]);
            h2 = hex_value(value->ptr[i + 3]); h3 = hex_value(value->ptr[i + 4]);
            if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0) return MCP_ERR_JSON;
            cp = (unsigned int)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
            i += 4;
            if (cp >= 0xd800 && cp <= 0xdbff) {
                unsigned int low;
                if (i + 6 >= value->len || value->ptr[i + 1] != '\\' ||
                    value->ptr[i + 2] != 'u') return MCP_ERR_JSON;
                h0 = hex_value(value->ptr[i + 3]); h1 = hex_value(value->ptr[i + 4]);
                h2 = hex_value(value->ptr[i + 5]); h3 = hex_value(value->ptr[i + 6]);
                if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0) return MCP_ERR_JSON;
                low = (unsigned int)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
                if (low < 0xdc00 || low > 0xdfff) return MCP_ERR_JSON;
                cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
                i += 6;
            } else if (cp >= 0xdc00 && cp <= 0xdfff) {
                return MCP_ERR_JSON;
            }
            if (append_utf8(cp, out, out_size, &pos) != MCP_OK) return MCP_ERR_OVERFLOW;
        } else {
            return MCP_ERR_JSON;
        }
    }
    out[pos] = '\0';
    return MCP_OK;
}

int mcp_json_get_int(const mcp_json_value_t *value, int *out) {
    char buffer[32];
    char *end;
    long parsed;
    if (value == NULL || out == NULL || value->type != MCP_JSON_PRIMITIVE ||
        value->len == 0 || value->len >= sizeof(buffer)) return MCP_ERR_INVALID;
    memcpy(buffer, value->ptr, value->len);
    buffer[value->len] = '\0';
    errno = 0;
    parsed = strtol(buffer, &end, 10);
    if (errno != 0 || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX) {
        return MCP_ERR_INVALID;
    }
    *out = (int)parsed;
    return MCP_OK;
}

int mcp_json_get_bool(const mcp_json_value_t *value, int *out) {
    if (value == NULL || out == NULL || value->type != MCP_JSON_PRIMITIVE) {
        return MCP_ERR_INVALID;
    }
    if (value->len == 4 && memcmp(value->ptr, "true", 4) == 0) *out = 1;
    else if (value->len == 5 && memcmp(value->ptr, "false", 5) == 0) *out = 0;
    else return MCP_ERR_INVALID;
    return MCP_OK;
}

int mcp_json_value_is_valid(const char *json, size_t length, mcp_json_type_t required_type) {
    jsmntok_t tokens[MCP_MAX_JSON_TOKENS];
    int count;
    if ((required_type == MCP_JSON_PRIMITIVE || required_type == MCP_JSON_UNDEFINED) &&
        primitive_valid(json, length)) {
        return MCP_OK;
    }
    count = parse_tokens(json, length, tokens, MCP_MAX_JSON_TOKENS);
    if (count < 0) return count;
    return required_type == MCP_JSON_UNDEFINED || public_type(tokens[0].type) == required_type
               ? MCP_OK : MCP_ERR_INVALID;
}

static int integer_id(const mcp_json_value_t *id) {
    size_t i = 0;
    if (id->type != MCP_JSON_PRIMITIVE || id->len == 0) return 0;
    if (id->ptr[i] == '-') {
        if (++i == id->len) return 0;
    }
    if (id->ptr[i] == '0') return i + 1 == id->len;
    if (id->ptr[i] < '1' || id->ptr[i] > '9') return 0;
    for (++i; i < id->len; ++i) if (id->ptr[i] < '0' || id->ptr[i] > '9') return 0;
    return 1;
}

int mcp_message_parse(const char *raw, size_t raw_len, mcp_message_t *out) {
    jsmntok_t tokens[MCP_MAX_JSON_TOKENS];
    mcp_json_value_t root;
    mcp_json_value_t value;
    mcp_json_value_t meta;
    int count;
    int has_method;
    int has_id;
    if (raw == NULL || out == NULL) return MCP_ERR_INVALID;
    memset(out, 0, sizeof(*out));
    out->type = MCP_MSG_INVALID;
    out->raw = raw;
    out->raw_len = raw_len;
    count = parse_tokens(raw, raw_len, tokens, MCP_MAX_JSON_TOKENS);
    if (count < 0) return count;
    if (tokens[0].type != JSMN_OBJECT) return MCP_ERR_INVALID;
    root = token_value(raw, &tokens[0]);

    if (object_get_tokens(raw, tokens, count, "jsonrpc", &value) != MCP_OK ||
        value.type != MCP_JSON_STRING || value.len != 3 || memcmp(value.ptr, "2.0", 3) != 0) {
        return MCP_ERR_INVALID;
    }
    has_method = object_get_tokens(raw, tokens, count, "method", &value) == MCP_OK;
    if (has_method) {
        if (mcp_json_get_string(&value, out->method, sizeof(out->method)) != MCP_OK ||
            out->method[0] == '\0') return MCP_ERR_INVALID;
    }
    has_id = object_get_tokens(raw, tokens, count, "id", &out->id) == MCP_OK;
    if (has_id) {
        if (out->id.type == MCP_JSON_STRING) out->id_type = MCP_ID_STRING;
        else if (integer_id(&out->id)) out->id_type = MCP_ID_NUMBER;
        else return MCP_ERR_INVALID;
    }
    if (has_method) out->type = has_id ? MCP_MSG_REQUEST : MCP_MSG_NOTIFICATION;
    else if (has_id) out->type = MCP_MSG_RESPONSE;
    else return MCP_ERR_INVALID;

    if (object_get_tokens(raw, tokens, count, "params", &out->params) != MCP_OK) {
        memset(&out->params, 0, sizeof(out->params));
        return MCP_OK;
    }
    if (out->params.type != MCP_JSON_OBJECT) return MCP_ERR_INVALID;
    if (mcp_json_object_get(&out->params, "_meta", &meta) == MCP_OK &&
        meta.type == MCP_JSON_OBJECT) {
        if (mcp_json_object_get(&meta, "io.modelcontextprotocol/protocolVersion", &value) == MCP_OK &&
            value.type == MCP_JSON_STRING) {
            if (mcp_json_get_string(&value, out->protocol_version,
                                    sizeof(out->protocol_version)) != MCP_OK) return MCP_ERR_INVALID;
        }
        if (mcp_json_object_get(&meta, "io.modelcontextprotocol/clientCapabilities", &value) == MCP_OK &&
            value.type == MCP_JSON_OBJECT) out->has_client_capabilities = 1;
    }
    if ((strcmp(out->method, "tools/call") == 0 ||
         strcmp(out->method, "prompts/get") == 0) &&
        mcp_json_object_get(&out->params, "name", &value) == MCP_OK) {
        if (mcp_json_get_string(&value, out->name, sizeof(out->name)) != MCP_OK) return MCP_ERR_INVALID;
    } else if (strcmp(out->method, "resources/read") == 0 &&
               mcp_json_object_get(&out->params, "uri", &value) == MCP_OK) {
        if (mcp_json_get_string(&value, out->name, sizeof(out->name)) != MCP_OK) return MCP_ERR_INVALID;
    }
    (void)root;
    return MCP_OK;
}
