#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifndef __ZEPHYR__
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#else
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
// Map POSIX names to Zephyr zsock_* when building under Zephyr
#define socket zsock_socket
#define setsockopt zsock_setsockopt
#define bind zsock_bind
#define listen zsock_listen
#define accept zsock_accept
#define recv zsock_recv
#define send zsock_send
#define close zsock_close
#endif

#include "mcp_transport.h"
#include "mcp_server.h"
#include "mcp_session.h"

#ifndef MCP_HTTP_PORT
#define MCP_HTTP_PORT 8080
#endif

#ifndef MCP_HTTP_BACKLOG
#define MCP_HTTP_BACKLOG 4
#endif

// Minimal HTTP POST parser for application/json bodies
static int parse_http_request(const char *buf, size_t len, const char **body, size_t *body_len) {
    const char *hdr_end = strstr(buf, "\r\n\r\n");
    if (!hdr_end)
        return -1;
    hdr_end += 4;
    *body = hdr_end;
    *body_len = len - (size_t) (hdr_end - buf);
    return 0;
}

static int http_send_json(mcp_session_t *s, const char *json, size_t len) {
    char header[256];
    int hlen = snprintf(header, sizeof(header),
                        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                        "%zu\r\nConnection: close\r\n\r\n",
                        len);
    if (send(s->sock, header, hlen, 0) < 0)
        return -1;
    if (send(s->sock, json, len, 0) < 0)
        return -1;
    return 0;
}

static int g_listen_fd = -1;

static int http_init(struct mcp_server *srv, void *cfg) {
    (void) cfg;
    (void) srv;
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)
        return -1;
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(MCP_HTTP_PORT);
    if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, MCP_HTTP_BACKLOG) < 0) {
        close(fd);
        return -1;
    }
    g_listen_fd = fd;
    printf("MCP HTTP listening on :%d\n", MCP_HTTP_PORT);
    return 0;
}

// forward declaration from server.c
int mcp_server_dispatch(struct mcp_server *srv, mcp_session_t *sess, const char *json, size_t len);

static int http_poll(struct mcp_server *srv) {
    struct sockaddr_in cli;
    socklen_t cl = sizeof(cli);
    int cfd = accept(g_listen_fd, (struct sockaddr *) &cli, &cl);
    if (cfd < 0) {
        if (errno == EINTR)
            return 0;
        return -1;
    }

    mcp_session_t sess_local = {.sock = cfd};

    char buf[4096];
    ssize_t r = recv(cfd, buf, sizeof(buf), 0);
    if (r <= 0) {
        close(cfd);
        return 0;
    }

    const char *body = NULL;
    size_t body_len = 0;
    if (parse_http_request(buf, (size_t) r, &body, &body_len) == 0) {
        // Dispatch JSON body
        mcp_server_dispatch(srv, &sess_local, body, body_len);
    } else {
        const char *bad =
            "HTTP/1.1 400 Bad Request\r\nContent-Length:0\r\nConnection: close\r\n\r\n";
        send(cfd, bad, strlen(bad), 0);
    }

    close(cfd);
    return 0;
}

static void http_close(struct mcp_server *srv) {
    (void) srv;
    if (g_listen_fd >= 0)
        close(g_listen_fd);
    g_listen_fd = -1;
}

static int http_send(struct mcp_session *s, const uint8_t *data, size_t len) {
    return http_send_json(s, (const char *) data, len);
}

static const mcp_transport_t HTTP_TRANSPORT = {
    .name = "http",
    .init = http_init,
    .poll = http_poll,
    .close = http_close,
    .send = http_send,
};

const mcp_transport_t *mcp_transport_http(void) {
    return &HTTP_TRANSPORT;
}
