/*
 * jsmn.h - Minimalistic JSON parser in C.
 * Copyright (c) 2010 Serge Zaitsev. MIT licensed; see LICENSE.
 */
#ifndef JSMN_H
#define JSMN_H

#include <stddef.h>

typedef enum {
    JSMN_UNDEFINED = 0,
    JSMN_OBJECT = 1 << 0,
    JSMN_ARRAY = 1 << 1,
    JSMN_STRING = 1 << 2,
    JSMN_PRIMITIVE = 1 << 3
} jsmntype_t;

enum jsmnerr {
    JSMN_ERROR_NOMEM = -1,
    JSMN_ERROR_INVAL = -2,
    JSMN_ERROR_PART = -3
};

typedef struct {
    jsmntype_t type;
    int start;
    int end;
    int size;
#ifdef JSMN_PARENT_LINKS
    int parent;
#endif
} jsmntok_t;

typedef struct {
    unsigned int pos;
    unsigned int toknext;
    int toksuper;
} jsmn_parser;

static void jsmn_init(jsmn_parser *parser) {
    parser->pos = 0;
    parser->toknext = 0;
    parser->toksuper = -1;
}

static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser, jsmntok_t *tokens,
                                   size_t num_tokens) {
    jsmntok_t *tok;
    if (parser->toknext >= num_tokens) {
        return NULL;
    }
    tok = &tokens[parser->toknext++];
    tok->start = tok->end = -1;
    tok->size = 0;
#ifdef JSMN_PARENT_LINKS
    tok->parent = -1;
#endif
    return tok;
}

static void jsmn_fill_token(jsmntok_t *token, jsmntype_t type, int start, int end) {
    token->type = type;
    token->start = start;
    token->end = end;
    token->size = 0;
}

static int jsmn_parse_primitive(jsmn_parser *parser, const char *js, size_t len,
                                jsmntok_t *tokens, size_t num_tokens) {
    int start = (int)parser->pos;
    jsmntok_t *token;
    for (; parser->pos < len; parser->pos++) {
        switch (js[parser->pos]) {
        case '\t': case '\r': case '\n': case ' ': case ',': case ']': case '}':
            goto found;
        case ':':
            goto found;
        default:
            if ((unsigned char)js[parser->pos] < 32 || (unsigned char)js[parser->pos] >= 127) {
                parser->pos = (unsigned int)start;
                return JSMN_ERROR_INVAL;
            }
        }
    }
#ifdef JSMN_STRICT
    parser->pos = (unsigned int)start;
    return JSMN_ERROR_PART;
#endif
found:
    if (tokens == NULL) {
        parser->pos--;
        return 0;
    }
    token = jsmn_alloc_token(parser, tokens, num_tokens);
    if (token == NULL) {
        parser->pos = (unsigned int)start;
        return JSMN_ERROR_NOMEM;
    }
    jsmn_fill_token(token, JSMN_PRIMITIVE, start, (int)parser->pos);
#ifdef JSMN_PARENT_LINKS
    token->parent = parser->toksuper;
#endif
    parser->pos--;
    return 0;
}

static int jsmn_parse_string(jsmn_parser *parser, const char *js, size_t len,
                             jsmntok_t *tokens, size_t num_tokens) {
    int start = (int)parser->pos;
    jsmntok_t *token;
    parser->pos++;
    for (; parser->pos < len; parser->pos++) {
        char c = js[parser->pos];
        if (c == '"') {
            if (tokens == NULL) return 0;
            token = jsmn_alloc_token(parser, tokens, num_tokens);
            if (token == NULL) {
                parser->pos = (unsigned int)start;
                return JSMN_ERROR_NOMEM;
            }
            jsmn_fill_token(token, JSMN_STRING, start + 1, (int)parser->pos);
#ifdef JSMN_PARENT_LINKS
            token->parent = parser->toksuper;
#endif
            return 0;
        }
        if ((unsigned char)c < 32) {
            parser->pos = (unsigned int)start;
            return JSMN_ERROR_INVAL;
        }
        if (c == '\\') {
            parser->pos++;
            if (parser->pos >= len) break;
            switch (js[parser->pos]) {
            case '"': case '/': case '\\': case 'b': case 'f': case 'r': case 'n': case 't':
                break;
            case 'u': {
                int i;
                for (i = 0; i < 4; i++) {
                    char h;
                    parser->pos++;
                    if (parser->pos >= len) {
                        parser->pos = (unsigned int)start;
                        return JSMN_ERROR_PART;
                    }
                    h = js[parser->pos];
                    if (!((h >= '0' && h <= '9') || (h >= 'A' && h <= 'F') ||
                          (h >= 'a' && h <= 'f'))) {
                        parser->pos = (unsigned int)start;
                        return JSMN_ERROR_INVAL;
                    }
                }
                break;
            }
            default:
                parser->pos = (unsigned int)start;
                return JSMN_ERROR_INVAL;
            }
        }
    }
    parser->pos = (unsigned int)start;
    return JSMN_ERROR_PART;
}

static int jsmn_parse(jsmn_parser *parser, const char *js, size_t len,
                      jsmntok_t *tokens, unsigned int num_tokens) {
    int r, i;
    jsmntok_t *token;
    for (; parser->pos < len; parser->pos++) {
        char c = js[parser->pos];
        switch (c) {
        case '{': case '[':
            token = jsmn_alloc_token(parser, tokens, num_tokens);
            if (token == NULL) return JSMN_ERROR_NOMEM;
            if (parser->toksuper != -1) {
                tokens[parser->toksuper].size++;
#ifdef JSMN_PARENT_LINKS
                token->parent = parser->toksuper;
#endif
            }
            token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
            token->start = (int)parser->pos;
            parser->toksuper = (int)parser->toknext - 1;
            break;
        case '}': case ']': {
            jsmntype_t type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
            int found = 0;
            for (i = (int)parser->toknext - 1; i >= 0; i--) {
                token = &tokens[i];
                if (token->start != -1 && token->end == -1) {
                    if (token->type != type) return JSMN_ERROR_INVAL;
                    token->end = (int)parser->pos + 1;
                    found = 1;
                    parser->toksuper = -1;
#ifdef JSMN_PARENT_LINKS
                    parser->toksuper = token->parent;
#else
                    for (i--; i >= 0; i--) {
                        if (tokens[i].start != -1 && tokens[i].end == -1) {
                            parser->toksuper = i;
                            break;
                        }
                    }
#endif
                    break;
                }
            }
            if (!found) return JSMN_ERROR_INVAL;
            break;
        }
        case '"':
            r = jsmn_parse_string(parser, js, len, tokens, num_tokens);
            if (r < 0) return r;
            if (parser->toksuper != -1) tokens[parser->toksuper].size++;
            break;
        case '\t': case '\r': case '\n': case ' ':
            break;
        case ':':
            parser->toksuper = (int)parser->toknext - 1;
            break;
        case ',':
            if (parser->toksuper != -1 && tokens[parser->toksuper].type != JSMN_ARRAY &&
                tokens[parser->toksuper].type != JSMN_OBJECT) {
#ifdef JSMN_PARENT_LINKS
                parser->toksuper = tokens[parser->toksuper].parent;
#else
                for (i = (int)parser->toknext - 1; i >= 0; i--) {
                    if ((tokens[i].type == JSMN_ARRAY || tokens[i].type == JSMN_OBJECT) &&
                        tokens[i].start != -1 && tokens[i].end == -1) {
                        parser->toksuper = i;
                        break;
                    }
                }
#endif
            }
            break;
#ifdef JSMN_STRICT
        case '-': case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case 't': case 'f': case 'n':
#else
        default:
#endif
            r = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
            if (r < 0) return r;
            if (parser->toksuper != -1) tokens[parser->toksuper].size++;
            break;
#ifdef JSMN_STRICT
        default:
            return JSMN_ERROR_INVAL;
#endif
        }
    }
    for (i = (int)parser->toknext - 1; i >= 0; i--) {
        if (tokens[i].start != -1 && tokens[i].end == -1) return JSMN_ERROR_PART;
    }
    return (int)parser->toknext;
}

#endif
