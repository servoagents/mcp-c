#include "mcp_mqtt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MCP_MQTT_MAX_TOPIC
#define MCP_MQTT_MAX_TOPIC 256
#endif

typedef struct {
    uint64_t fingerprint;
    uint64_t expires_at_ms;
    size_t response_length;
    uint8_t *response;
    int valid;
} replay_entry_t;

struct mcp_mqtt_bridge {
    mcp_server_t *server;
    mcp_mqtt_bridge_config_t config;
    char request_topic[MCP_MQTT_MAX_TOPIC];
    replay_entry_t *entries;
    uint8_t *responses;
    size_t next_entry;
};

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t length) {
    const uint8_t *bytes = (const uint8_t *)data;
    size_t i;
    for (i = 0; i < length; ++i) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t fingerprint(const mcp_mqtt_request_t *request,
                            const mcp_message_t *message) {
    uint64_t hash = UINT64_C(14695981039346656037);
    hash = hash_bytes(hash, request->client_identity, strlen(request->client_identity));
    hash = hash_bytes(hash, request->correlation_data, request->correlation_length);
    hash = hash_bytes(hash, &message->id_type, sizeof(message->id_type));
    if (message->id_type == MCP_ID_STRING)
        hash = hash_bytes(hash, message->id.ptr, message->id.len);
    else if (message->id_type == MCP_ID_NUMBER)
        hash = hash_bytes(hash, message->id.ptr, message->id.len);
    return hash;
}

static int valid_topic(const char *topic) {
    const unsigned char *p = (const unsigned char *)topic;
    if (topic == NULL || *topic == '\0' || strlen(topic) >= MCP_MQTT_MAX_TOPIC) return 0;
    for (; *p; ++p) if (*p < 0x20 || *p == '+' || *p == '#') return 0;
    return 1;
}

static int starts_with(const char *text, const char *prefix) {
    size_t length = strlen(prefix);
    return strncmp(text, prefix, length) == 0 && text[length] != '\0';
}

mcp_mqtt_bridge_t *mcp_mqtt_bridge_init(mcp_server_t *server,
                                         const mcp_mqtt_bridge_config_t *config) {
    mcp_mqtt_bridge_t *bridge;
    size_t i;
    int length;
    if (server == NULL || config == NULL || !valid_topic(config->server_id) ||
        !valid_topic(config->allowed_response_topic_prefix) || config->publish == NULL)
        return NULL;
    if (strchr(config->server_id, '/') != NULL ||
        config->allowed_response_topic_prefix[strlen(config->allowed_response_topic_prefix) - 1] != '/')
        return NULL;
    bridge = (mcp_mqtt_bridge_t *)calloc(1, sizeof(*bridge));
    if (bridge == NULL) return NULL;
    bridge->server = server;
    bridge->config = *config;
    if (bridge->config.duplicate_cache_entries == 0) bridge->config.duplicate_cache_entries = 8;
    if (bridge->config.duplicate_lifetime_ms == 0) bridge->config.duplicate_lifetime_ms = 60000;
    if (bridge->config.max_response_size == 0) bridge->config.max_response_size = 8192;
    length = snprintf(bridge->request_topic, sizeof(bridge->request_topic),
                      "mcp/v1/servers/%s/requests", config->server_id);
    if (length <= 0 || (size_t)length >= sizeof(bridge->request_topic)) goto fail;
    bridge->entries = (replay_entry_t *)calloc(bridge->config.duplicate_cache_entries,
                                               sizeof(*bridge->entries));
    bridge->responses = (uint8_t *)calloc(bridge->config.duplicate_cache_entries,
                                          bridge->config.max_response_size);
    if (bridge->entries == NULL || bridge->responses == NULL) goto fail;
    for (i = 0; i < bridge->config.duplicate_cache_entries; ++i)
        bridge->entries[i].response = bridge->responses + i * bridge->config.max_response_size;
    return bridge;
fail:
    mcp_mqtt_bridge_deinit(bridge);
    return NULL;
}

void mcp_mqtt_bridge_deinit(mcp_mqtt_bridge_t *bridge) {
    if (bridge == NULL) return;
    free(bridge->responses);
    free(bridge->entries);
    free(bridge);
}

const char *mcp_mqtt_bridge_request_topic(const mcp_mqtt_bridge_t *bridge) {
    return bridge ? bridge->request_topic : NULL;
}

static uint64_t current_time(const mcp_mqtt_bridge_t *bridge,
                             const mcp_mqtt_request_t *request) {
    return bridge->config.now_ms ? bridge->config.now_ms(bridge->config.user_data)
                                 : request->received_at_ms;
}

static replay_entry_t *find_replay(mcp_mqtt_bridge_t *bridge, uint64_t key, uint64_t now) {
    size_t i;
    for (i = 0; i < bridge->config.duplicate_cache_entries; ++i) {
        replay_entry_t *entry = &bridge->entries[i];
        if (entry->valid && entry->expires_at_ms <= now) entry->valid = 0;
        if (entry->valid && entry->fingerprint == key) return entry;
    }
    return NULL;
}

static int publish_response(mcp_mqtt_bridge_t *bridge, const mcp_mqtt_request_t *request,
                            const uint8_t *response, size_t response_length) {
    return bridge->config.publish(request->response_topic, response, response_length,
                                  request->correlation_data, request->correlation_length,
                                  1, 0, bridge->config.user_data);
}

mcp_mqtt_status_t mcp_mqtt_bridge_handle(mcp_mqtt_bridge_t *bridge,
                                          const mcp_mqtt_request_t *request) {
    mcp_message_t message;
    mcp_request_ctx_t context;
    mcp_dispatch_result_t dispatch;
    replay_entry_t *entry;
    uint64_t now;
    uint64_t key;
    uint64_t expiry_ms;
    if (bridge == NULL || request == NULL || request->topic == NULL ||
        strcmp(request->topic, bridge->request_topic) != 0) return MCP_MQTT_IGNORED;
    if (request->qos != 1 || !request->payload_is_utf8 || request->content_type == NULL ||
        strcmp(request->content_type, "application/json") != 0 ||
        !valid_topic(request->response_topic) ||
        !starts_with(request->response_topic, bridge->config.allowed_response_topic_prefix) ||
        request->client_identity == NULL || *request->client_identity == '\0' ||
        request->correlation_data == NULL || request->correlation_length == 0 ||
        request->payload == NULL || request->payload_length == 0 ||
        request->message_expiry_interval_s == 0) return MCP_MQTT_INVALID;
    now = current_time(bridge, request);
    expiry_ms = (uint64_t)request->message_expiry_interval_s * UINT64_C(1000);
    if (now < request->received_at_ms || now - request->received_at_ms >= expiry_ms)
        return MCP_MQTT_EXPIRED;
    if (mcp_message_parse((const char *)request->payload, request->payload_length,
                          &message) != MCP_OK) {
        /* Let the core publish the normative parse/invalid-request response. */
        memset(&message, 0, sizeof(message));
    }
    key = fingerprint(request, &message);
    if (message.id_type == MCP_ID_NONE)
        key = hash_bytes(key, request->payload, request->payload_length);
    entry = find_replay(bridge, key, now);
    if (entry != NULL) {
        return publish_response(bridge, request, entry->response, entry->response_length) == MCP_OK
                   ? MCP_MQTT_DUPLICATE_REPLIED : MCP_MQTT_PUBLISH_FAILED;
    }
    entry = &bridge->entries[bridge->next_entry++ % bridge->config.duplicate_cache_entries];
    memset(&context, 0, sizeof(context));
    context.transport_name = "mqtt5";
    context.transport_context = (void *)request;
    dispatch = mcp_server_handle(bridge->server, &context,
                                 (const char *)request->payload, request->payload_length,
                                 (char *)entry->response, bridge->config.max_response_size);
    if (dispatch.status == MCP_DISPATCH_NOTIFICATION) return MCP_MQTT_ACCEPTED;
    if (dispatch.response_length == 0) return MCP_MQTT_INVALID;
    entry->fingerprint = key;
    entry->expires_at_ms = now + bridge->config.duplicate_lifetime_ms;
    entry->response_length = dispatch.response_length;
    entry->valid = 1;
    return publish_response(bridge, request, entry->response, entry->response_length) == MCP_OK
               ? MCP_MQTT_ACCEPTED : MCP_MQTT_PUBLISH_FAILED;
}
