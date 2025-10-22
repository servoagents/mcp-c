#include <zephyr/logging/log.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/kernel.h>
#include <string.h>

LOG_MODULE_REGISTER(wifi, LOG_LEVEL_INF);

static struct {
    struct k_sem connected;
    struct k_sem disconnected;
    int status;
} wifi_ctx;

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
                                    struct net_if *iface) {
    ARG_UNUSED(iface);

    if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
        const struct wifi_status *status = (const struct wifi_status *) cb->info;
        wifi_ctx.status = status ? status->status : -EIO;
        if (wifi_ctx.status == 0) {
            LOG_INF("WiFi connected");
            k_sem_give(&wifi_ctx.connected);
        } else {
            LOG_ERR("WiFi connect failed: %d", wifi_ctx.status);
        }
    } else if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
        LOG_INF("WiFi disconnected");
        k_sem_give(&wifi_ctx.disconnected);
    }
}

int wifi_init(void *unused) {
    ARG_UNUSED(unused);
    k_sem_init(&wifi_ctx.connected, 0, 1);
    k_sem_init(&wifi_ctx.disconnected, 0, 1);
    wifi_ctx.status = -1;

    static struct net_mgmt_event_callback wifi_cb;
    net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_event_handler,
                                 NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_cb);
    return 0;
}

int connect_to_wifi(void) {
    struct net_if *iface = net_if_get_default();
    struct wifi_connect_req_params cnx = {0};

#ifdef WIFI_SSID
    cnx.ssid = WIFI_SSID;
    cnx.ssid_length = strlen(WIFI_SSID);
#else
#error "WIFI_SSID is not defined"
#endif
#ifdef WIFI_PASS
    cnx.psk = WIFI_PASS;
    cnx.psk_length = strlen(WIFI_PASS);
    cnx.security = WIFI_SECURITY_TYPE_PSK;
#else
    cnx.security = WIFI_SECURITY_TYPE_NONE;
#endif
    cnx.channel = WIFI_CHANNEL_ANY;
    cnx.timeout = SYS_FOREVER_MS;

    int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &cnx, sizeof(cnx));
    if (ret) {
        LOG_ERR("net_mgmt connect failed: %d", ret);
        return ret;
    }
    return 0;
}

int wait_for_wifi_connection(void) {
    if (k_sem_take(&wifi_ctx.connected, K_SECONDS(20)) != 0) {
        return -ETIMEDOUT;
    }
    return wifi_ctx.status;
}

int wifi_disconnect(void) {
    struct net_if *iface = net_if_get_default();
    int ret = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);
    if (ret) {
        LOG_WRN("WiFi disconnect returned: %d", ret);
    }
    return ret;
}
