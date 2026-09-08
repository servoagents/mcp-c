#include "demo_tools.h"
#include "mcp_transport.h"
#include "servo_backend.h"
#include "wifi.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>

LOG_MODULE_REGISTER(mcp_zephyr_app, LOG_LEVEL_INF);

#ifdef CONFIG_BOARD_NATIVE_SIM
#define MCP_ZEPHYR_SERVER_NAME "mcp-c-native-sim"
#define MCP_ZEPHYR_INSTRUCTIONS "Control a simulated servo and inspect its commanded angle."
#else
#define MCP_ZEPHYR_SERVER_NAME "mcp-c-esp32"
#define MCP_ZEPHYR_INSTRUCTIONS "Control the ESP32 servo on GPIO18 and inspect its commanded angle."
#endif

#ifdef CONFIG_WIFI
static struct net_mgmt_event_callback ipv4_cb;

static void log_ipv4(struct net_if *iface) {
    struct in_addr *address = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
    if (address && address->s_addr != 0) {
        char text[NET_IPV4_ADDR_LEN];
        net_addr_ntop(AF_INET, address, text, sizeof(text));
        LOG_INF("MCP endpoint: http://%s:8080/mcp", text);
    }
}

static void ipv4_event_handler(struct net_mgmt_event_callback *callback, uint64_t event,
                               struct net_if *iface) {
    ARG_UNUSED(callback);
    if (event == NET_EVENT_IPV4_ADDR_ADD) log_ipv4(iface);
}
#endif

int main(void) {
    mcp_server_config_t server_config = {
        .name = MCP_ZEPHYR_SERVER_NAME,
        .version = MCP_VERSION,
        .instructions = MCP_ZEPHYR_INSTRUCTIONS,
        .max_request_size = 4096,
    };
    mcp_http_config_t http_config = {
        .bind_address = "0.0.0.0",
        .port = 8080,
        .backlog = 4,
    };
    mcp_server_t *server;
    mcp_transport_t http;
    servo_backend_t servo;

#ifdef CONFIG_WIFI
    wifi_init(NULL);
    if (connect_to_wifi() < 0 || wait_for_wifi_connection() < 0) {
        LOG_ERR("Wi-Fi connection failed");
        return 0;
    }
    net_mgmt_init_event_callback(&ipv4_cb, ipv4_event_handler, NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&ipv4_cb);
    log_ipv4(net_if_get_default());
#endif

    if (zephyr_servo_backend_init(&servo) != MCP_OK) return 0;
    server = mcp_server_init(&server_config);
    if (server == NULL || demo_register_tools(server, &servo) != MCP_OK) {
        LOG_ERR("MCP core initialization failed");
        return 0;
    }
    if (mcp_http_transport_init(&http, &http_config) != MCP_OK ||
        mcp_transport_open(&http, server) != MCP_OK) {
        LOG_ERR("MCP HTTP transport initialization failed");
        mcp_server_deinit(server);
        return 0;
    }
    LOG_INF("MCP %s ready; servo backend: %s", MCP_PROTOCOL_VERSION,
            zephyr_servo_backend_is_hardware() ? "PWM hardware" : "simulated");
    while (mcp_transport_poll(&http, 250) == MCP_OK) {}
    mcp_transport_close(&http);
    mcp_server_deinit(server);
    return 0;
}
