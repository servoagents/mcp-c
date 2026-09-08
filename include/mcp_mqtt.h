#ifndef MCP_MQTT_H
#define MCP_MQTT_H

#include "mcp_server.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mcp_mqtt_bridge mcp_mqtt_bridge_t;

typedef int (*mcp_mqtt_publish_fn)(const char *topic, const uint8_t *payload,
                                   size_t payload_length,
                                   const uint8_t *correlation_data,
                                   size_t correlation_length, int qos,
                                   int retain, void *user_data);

typedef uint64_t (*mcp_mqtt_now_ms_fn)(void *user_data);

typedef struct {
    const char *server_id;
    const char *allowed_response_topic_prefix;
    size_t duplicate_cache_entries;
    uint64_t duplicate_lifetime_ms;
    size_t max_response_size;
    mcp_mqtt_publish_fn publish;
    mcp_mqtt_now_ms_fn now_ms;
    void *user_data;
} mcp_mqtt_bridge_config_t;

typedef struct {
    const char *topic;
    const char *response_topic;
    const char *client_identity;
    const uint8_t *correlation_data;
    size_t correlation_length;
    const uint8_t *payload;
    size_t payload_length;
    const char *content_type;
    int payload_is_utf8;
    int qos;
    uint32_t message_expiry_interval_s;
    uint64_t received_at_ms;
} mcp_mqtt_request_t;

typedef enum {
    MCP_MQTT_ACCEPTED = 0,
    MCP_MQTT_DUPLICATE_REPLIED = 1,
    MCP_MQTT_IGNORED = 2,
    MCP_MQTT_INVALID = -1,
    MCP_MQTT_EXPIRED = -2,
    MCP_MQTT_PUBLISH_FAILED = -3
} mcp_mqtt_status_t;

mcp_mqtt_bridge_t *mcp_mqtt_bridge_init(mcp_server_t *server,
                                         const mcp_mqtt_bridge_config_t *config);
void mcp_mqtt_bridge_deinit(mcp_mqtt_bridge_t *bridge);
mcp_mqtt_status_t mcp_mqtt_bridge_handle(mcp_mqtt_bridge_t *bridge,
                                          const mcp_mqtt_request_t *request);
const char *mcp_mqtt_bridge_request_topic(const mcp_mqtt_bridge_t *bridge);

#ifdef __cplusplus
}
#endif

#endif
