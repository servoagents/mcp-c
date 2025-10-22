#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "mcp_server.h"
#include "mcp_transport.h"
#include "mcp.h"
#include "wifi.h"
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>

LOG_MODULE_REGISTER(mcp_zephyr_app, LOG_LEVEL_INF);

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

    /* Print assigned IPv4 address via UDP getsockname trick */
    do {
        int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s >= 0) {
            struct sockaddr_in dst = {0};
            dst.sin_family = AF_INET;
            dst.sin_port = htons(53);
            /* Public DNS; only used to select route, no packets sent */
            inet_pton(AF_INET, "8.8.8.8", &dst.sin_addr);
            if (connect(s, (struct sockaddr *) &dst, sizeof(dst)) == 0) {
                struct sockaddr_in src = {0};
                socklen_t slen = sizeof(src);
                if (getsockname(s, (struct sockaddr *) &src, &slen) == 0) {
                    char ipbuf[NET_IPV4_ADDR_LEN];
                    net_addr_ntop(AF_INET, &src.sin_addr, ipbuf, sizeof(ipbuf));
                    LOG_INF("WiFi IPv4 address: %s", ipbuf);
                }
            }
            (void) close(s);
        }
    } while (0);

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
    wifi_disconnect();
    return 0;
}
