#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "mcp_server.h"
#include "mcp_transport.h"
#include "mcp.h"
#include "wifi.h"
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>

LOG_MODULE_REGISTER(mcp_zephyr_app, LOG_LEVEL_INF);

#ifdef CONFIG_WIFI
/* IPv4 address event-based logger */
static struct net_mgmt_event_callback ipv4_cb;

static void ipv4_event_handler(struct net_mgmt_event_callback *cb, uint64_t event,
                               struct net_if *iface) {
    ARG_UNUSED(cb);
    if (event != NET_EVENT_IPV4_ADDR_ADD) {
        return;
    }
    struct in_addr *addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
    if (addr && addr->s_addr != 0) {
        char ipbuf[NET_IPV4_ADDR_LEN];
        net_addr_ntop(AF_INET, addr, ipbuf, sizeof(ipbuf));
        LOG_INF("WiFi IPv4 address: %s", ipbuf);
    }
}
#endif

static int handle_initialize(struct mcp_session *s, const mcp_message_t *req, char *resp,
                             size_t cap) {
    ARG_UNUSED(s);
    const char *id = (req && req->id) ? req->id : "1";
    snprintk(resp, cap,
             "{\"jsonrpc\":\"2.0\",\"id\":\"%s\",\"result\":{\"capabilities\":{\"tools\":true,"
             "\"resources\":true},\"protocolVersion\":\"1.0\"}}",
             id);
    return 0;
}

static int handle_tools_list(struct mcp_session *s, const mcp_message_t *req, char *resp,
                             size_t cap) {
    ARG_UNUSED(s);
    const char *id = (req && req->id) ? req->id : "1";
    const char *result = "{\"tools\":[{\"name\":\"echo\",\"description\":\"Echo "
                         "text\",\"input_schema\":{\"type\":\"object\",\"properties\":{\"text\":{"
                         "\"type\":\"string\"}},\"required\":[\"text\"]}}]}";
    snprintk(resp, cap, "{\"jsonrpc\":\"2.0\",\"id\":\"%s\",\"result\":%s}", id, result);
    return 0;
}

static int handle_tools_call(struct mcp_session *s, const mcp_message_t *req, char *resp,
                             size_t cap) {
    ARG_UNUSED(s);
    const char *id = (req && req->id) ? req->id : "1";
    const char *out = "Hello from Zephyr MCP!";
    snprintk(resp, cap,
             "{\"jsonrpc\":\"2.0\",\"id\":\"%s\",\"result\":{\"content\":[{\"type\":\"text\","
             "\"text\":\"%s\"}]}}",
             id, out);
    return 0;
}

int main(void) {
    LOG_INF("Starting MCP Zephyr app (HTTP :8080)");

#ifdef CONFIG_WIFI
    /* Initialize and connect WiFi before starting server */
    (void) wifi_init(NULL);
    if (connect_to_wifi() < 0) {
        LOG_ERR("WiFi connect request failed");
        return 0;
    }
    if (wait_for_wifi_connection() < 0) {
        LOG_ERR("WiFi did not connect in time");
        return 0;
    }

    /* Log IPv4 once DHCP completes using a net_mgmt event callback. */
    net_mgmt_init_event_callback(&ipv4_cb, ipv4_event_handler, NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&ipv4_cb);
    /* If DHCP already completed, log immediately. */
    {
        struct in_addr *addr =
            net_if_ipv4_get_global_addr(net_if_get_default(), NET_ADDR_PREFERRED);
        if (addr && addr->s_addr != 0) {
            char ipbuf[NET_IPV4_ADDR_LEN];
            net_addr_ntop(AF_INET, addr, ipbuf, sizeof(ipbuf));
            LOG_INF("WiFi IPv4 address: %s", ipbuf);
        }
    }
#else
    /* For native_sim, just log the configured IP */
    LOG_INF("Running on native_sim - network interface ready");
    struct net_if *iface = net_if_get_default();
    if (iface) {
        struct in_addr *addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
        if (addr && addr->s_addr != 0) {
            char ipbuf[NET_IPV4_ADDR_LEN];
            net_addr_ntop(AF_INET, addr, ipbuf, sizeof(ipbuf));
            LOG_INF("IPv4 address: %s", ipbuf);
        }
    }
#endif

    mcp_server_config_t cfg = {
        .transport = mcp_transport_http(),
        .transport_cfg = NULL,
        .max_body = 4096,
    };
    mcp_server_t *srv = mcp_server_init(&cfg);
    if (!srv) {
        LOG_ERR("Failed to init MCP server");
        return 0;
    }

    mcp_server_register(srv, "initialize", handle_initialize);
    mcp_server_register(srv, "tools/list", handle_tools_list);
    mcp_server_register(srv, "tools/call", handle_tools_call);

    mcp_server_run(srv);
    mcp_server_deinit(srv);
#ifdef CONFIG_WIFI
    wifi_disconnect();
#endif
    return 0;
}
