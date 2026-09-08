#include "mcp_transport.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifndef __ZEPHYR__
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef struct pollfd mcp_pollfd_t;
#define MCP_SOCKET socket
#define MCP_SETSOCKOPT setsockopt
#define MCP_BIND bind
#define MCP_LISTEN listen
#define MCP_ACCEPT accept
#define MCP_RECV recv
#define MCP_SEND send
#define MCP_CLOSE close
#define MCP_INET_PTON inet_pton
#define MCP_POLL poll
#else
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
typedef struct zsock_pollfd mcp_pollfd_t;
#define MCP_SOCKET zsock_socket
#define MCP_SETSOCKOPT zsock_setsockopt
#define MCP_BIND zsock_bind
#define MCP_LISTEN zsock_listen
#define MCP_ACCEPT zsock_accept
#define MCP_RECV zsock_recv
#define MCP_SEND zsock_send
#define MCP_CLOSE zsock_close
#define MCP_INET_PTON zsock_inet_pton
#define MCP_POLL zsock_poll
#endif

#ifndef MCP_HTTP_MAX_HEADERS
#define MCP_HTTP_MAX_HEADERS 4096
#endif

#ifndef MCP_HTTP_MAX_RESPONSE
#define MCP_HTTP_MAX_RESPONSE 8192
#endif

typedef struct {
    mcp_http_config_t config;
    int listen_fd;
    char *request;
    size_t request_capacity;
    char *response;
} http_state_t;

typedef struct {
    char *body;
    size_t content_length;
    char *content_type;
    char *accept;
    char *origin;
    char *protocol_version;
    char *method;
    char *name;
    char *transfer_encoding;
} http_request_t;

static int send_all(int fd, const char *data, size_t length) {
    size_t sent = 0;
    while (sent < length) {
        ssize_t result = MCP_SEND(fd, data + sent, length - sent, 0);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) return MCP_ERR;
        sent += (size_t)result;
    }
    return MCP_OK;
}

static const char *reason_phrase(int status) {
    switch (status) {
    case 200: return "OK";
    case 202: return "Accepted";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 413: return "Payload Too Large";
    case 415: return "Unsupported Media Type";
    case 431: return "Request Header Fields Too Large";
    default: return "Internal Server Error";
    }
}

static int send_http(int fd, int status, const char *content_type,
                     const char *body, size_t body_length) {
    char header[320];
    int length = snprintf(header, sizeof(header),
                          "HTTP/1.1 %d %s\r\n"
                          "Content-Type: %s\r\n"
                          "Content-Length: %zu\r\n"
                          "Connection: close\r\n"
                          "Cache-Control: no-store\r\n\r\n",
                          status, reason_phrase(status),
                          content_type ? content_type : "text/plain", body_length);
    if (length <= 0 || (size_t)length >= sizeof(header) ||
        send_all(fd, header, (size_t)length) != MCP_OK) return MCP_ERR;
    return body_length == 0 || send_all(fd, body, body_length) == MCP_OK ? MCP_OK : MCP_ERR;
}

static char *trim(char *value) {
    char *end;
    while (*value == ' ' || *value == '\t') ++value;
    end = value + strlen(value);
    while (end > value && (end[-1] == ' ' || end[-1] == '\t')) --end;
    *end = '\0';
    return value;
}

static int set_once(char **field, char *value) {
    if (*field != NULL) return MCP_ERR;
    *field = value;
    return MCP_OK;
}

static int parse_content_length(const char *value, size_t *length) {
    char *end;
    unsigned long parsed;
    const char *p;
    if (value == NULL || *value == '\0') return MCP_ERR;
    for (p = value; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') return MCP_ERR;
    }
    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno != 0 || *end != '\0' || (unsigned long)(size_t)parsed != parsed) return MCP_ERR;
    *length = (size_t)parsed;
    return MCP_OK;
}

/* Returns zero on success or the HTTP status appropriate for the request line/header error. */
static int parse_headers(char *buffer, char *header_end, http_request_t *request) {
    char *line_end;
    char *line;
    char *colon;
    char *content_length = NULL;
    memset(request, 0, sizeof(*request));
    line_end = strstr(buffer, "\r\n");
    char *method_end;
    char *path_end;
    if (line_end == NULL || line_end >= header_end) return 400;
    *line_end = '\0';
    method_end = strchr(buffer, ' ');
    if (method_end == NULL) return 400;
    *method_end = '\0';
    path_end = strchr(method_end + 1, ' ');
    if (path_end == NULL) return 400;
    *path_end = '\0';
    if (strcmp(buffer, "POST") != 0) return 405;
    if (strcmp(method_end + 1, "/mcp") != 0) return 404;
    if (strcmp(path_end + 1, "HTTP/1.1") != 0) return 400;
    line = line_end + 2;
    while (line < header_end) {
        line_end = strstr(line, "\r\n");
        if (line_end == NULL || line_end > header_end) return 400;
        if (line_end == line) break;
        *line_end = '\0';
        colon = strchr(line, ':');
        if (colon == NULL) return 400;
        *colon = '\0';
        if (strcasecmp(line, "Content-Length") == 0) {
            if (set_once(&content_length, trim(colon + 1)) != MCP_OK) return 400;
        } else if (strcasecmp(line, "Content-Type") == 0) {
            if (set_once(&request->content_type, trim(colon + 1)) != MCP_OK) return 400;
        } else if (strcasecmp(line, "Accept") == 0) {
            if (set_once(&request->accept, trim(colon + 1)) != MCP_OK) return 400;
        } else if (strcasecmp(line, "Origin") == 0) {
            if (set_once(&request->origin, trim(colon + 1)) != MCP_OK) return 400;
        } else if (strcasecmp(line, "MCP-Protocol-Version") == 0) {
            if (set_once(&request->protocol_version, trim(colon + 1)) != MCP_OK) return 400;
        } else if (strcasecmp(line, "Mcp-Method") == 0) {
            if (set_once(&request->method, trim(colon + 1)) != MCP_OK) return 400;
        } else if (strcasecmp(line, "Mcp-Name") == 0) {
            if (set_once(&request->name, trim(colon + 1)) != MCP_OK) return 400;
        } else if (strcasecmp(line, "Transfer-Encoding") == 0) {
            if (set_once(&request->transfer_encoding, trim(colon + 1)) != MCP_OK) return 400;
        }
        line = line_end + 2;
    }
    if (parse_content_length(content_length, &request->content_length) != MCP_OK) return 400;
    request->body = header_end + 4;
    return MCP_OK;
}

static int media_type_is_json(const char *value) {
    size_t length;
    if (value == NULL) return 0;
    length = strcspn(value, "; \t");
    return length == 16 && strncasecmp(value, "application/json", 16) == 0;
}

static int accepts(const char *value, const char *media_type) {
    size_t wanted = strlen(media_type);
    const char *p = value;
    if (value == NULL) return 0;
    while (*p) {
        const char *end;
        while (*p == ' ' || *p == '\t' || *p == ',') ++p;
        end = p + strcspn(p, ",;");
        while (end > p && (end[-1] == ' ' || end[-1] == '\t')) --end;
        if ((size_t)(end - p) == wanted && strncasecmp(p, media_type, wanted) == 0) return 1;
        p += strcspn(p, ",");
        if (*p == ',') ++p;
    }
    return 0;
}

static int http_open(mcp_transport_t *transport, mcp_server_t *server) {
    http_state_t *state = (http_state_t *)transport->state;
    struct sockaddr_in address;
    int reuse = 1;
    const char *failed_operation = "buffer allocation";
    size_t max_request = mcp_server_max_request_size(server);
    state->request_capacity = MCP_HTTP_MAX_HEADERS + max_request + 1;
    state->request = (char *)malloc(state->request_capacity);
    state->response = (char *)malloc(MCP_HTTP_MAX_RESPONSE);
    if (state->request == NULL || state->response == NULL) goto fail;
    failed_operation = "socket";
    state->listen_fd = MCP_SOCKET(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (state->listen_fd < 0) goto fail;
    (void)MCP_SETSOCKOPT(state->listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(state->config.port);
    failed_operation = "address parsing";
    if (MCP_INET_PTON(AF_INET, state->config.bind_address, &address.sin_addr) != 1) goto fail;
    failed_operation = "bind";
    if (MCP_BIND(state->listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) goto fail;
    failed_operation = "listen";
    if (MCP_LISTEN(state->listen_fd, state->config.backlog) < 0) goto fail;
    printf("MCP 2026-07-28 HTTP listening on http://%s:%u/mcp\n",
           state->config.bind_address, (unsigned int)state->config.port);
    return MCP_OK;
fail:
    printf("MCP HTTP %s failed (errno=%d)\n", failed_operation, errno);
    if (state->listen_fd >= 0) MCP_CLOSE(state->listen_fd);
    state->listen_fd = -1;
    free(state->request); state->request = NULL;
    free(state->response); state->response = NULL;
    return MCP_ERR;
}

static int receive_request(http_state_t *state, int fd, size_t *received,
                           char **header_end) {
    ssize_t count;
    *received = 0;
    *header_end = NULL;
    while (*received < state->request_capacity - 1) {
        count = MCP_RECV(fd, state->request + *received,
                     state->request_capacity - 1 - *received, 0);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return MCP_ERR;
        *received += (size_t)count;
        state->request[*received] = '\0';
        *header_end = strstr(state->request, "\r\n\r\n");
        if (*header_end != NULL) return MCP_OK;
        if (*received >= MCP_HTTP_MAX_HEADERS) return MCP_ERR_OVERFLOW;
    }
    return MCP_ERR_OVERFLOW;
}

static int read_body(http_state_t *state, int fd, size_t *received, size_t expected) {
    while (*received < expected) {
        ssize_t count = MCP_RECV(fd, state->request + *received, expected - *received, 0);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return MCP_ERR;
        *received += (size_t)count;
    }
    state->request[*received] = '\0';
    return MCP_OK;
}

static void handle_connection(mcp_transport_t *transport, int fd) {
    http_state_t *state = (http_state_t *)transport->state;
    struct timeval receive_timeout = {.tv_sec = 5, .tv_usec = 0};
    http_request_t request;
    mcp_request_ctx_t context;
    mcp_dispatch_result_t dispatch;
    char *header_end;
    size_t received;
    size_t header_length;
    size_t expected;
    int read_result;
    int parse_status;
    (void)MCP_SETSOCKOPT(fd, SOL_SOCKET, SO_RCVTIMEO,
                         &receive_timeout, sizeof(receive_timeout));
    read_result = receive_request(state, fd, &received, &header_end);
    if (read_result != MCP_OK) {
        send_http(fd, read_result == MCP_ERR_OVERFLOW ? 431 : 400,
                  "text/plain", NULL, 0);
        return;
    }
    header_length = (size_t)(header_end + 4 - state->request);
    if (header_length > MCP_HTTP_MAX_HEADERS) {
        send_http(fd, 431, "text/plain", NULL, 0); return;
    }
    parse_status = parse_headers(state->request, header_end, &request);
    if (parse_status != 0) {
        send_http(fd, parse_status, "text/plain", NULL, 0); return;
    }
    if (request.content_length > mcp_server_max_request_size(transport->server)) {
        send_http(fd, 413, "text/plain", NULL, 0); return;
    }
    if (request.transfer_encoding != NULL) {
        send_http(fd, 400, "text/plain", NULL, 0); return;
    }
    expected = header_length + request.content_length;
    if (expected >= state->request_capacity ||
        read_body(state, fd, &received, expected) != MCP_OK) {
        send_http(fd, 400, "text/plain", NULL, 0); return;
    }
    if (!media_type_is_json(request.content_type)) {
        send_http(fd, 415, "text/plain", NULL, 0); return;
    }
    if (!accepts(request.accept, "application/json") ||
        !accepts(request.accept, "text/event-stream") ||
        request.protocol_version == NULL || request.method == NULL ||
        ((strcmp(request.method, "tools/call") == 0 ||
          strcmp(request.method, "resources/read") == 0 ||
          strcmp(request.method, "prompts/get") == 0) && request.name == NULL)) {
        send_http(fd, 400, "text/plain", NULL, 0); return;
    }
    if (request.origin != NULL &&
        (state->config.allowed_origin == NULL ||
         strcmp(request.origin, state->config.allowed_origin) != 0)) {
        send_http(fd, 403, "text/plain", NULL, 0); return;
    }
    memset(&context, 0, sizeof(context));
    context.transport_name = "http";
    context.transport_context = &fd;
    context.user_data = transport->server;
    context.protocol_version_header = request.protocol_version;
    context.method_header = request.method;
    context.name_header = request.name;
    dispatch = mcp_server_handle(transport->server, &context, request.body,
                                 request.content_length, state->response,
                                 MCP_HTTP_MAX_RESPONSE);
    if (dispatch.status == MCP_DISPATCH_NOTIFICATION) {
        send_http(fd, 202, "text/plain", NULL, 0);
    } else if (dispatch.response_length > 0) {
        send_http(fd, dispatch.http_status, "application/json", state->response,
                  dispatch.response_length);
    } else {
        send_http(fd, 500, "text/plain", NULL, 0);
    }
}

static int http_poll(mcp_transport_t *transport, int timeout_ms) {
    http_state_t *state = (http_state_t *)transport->state;
    mcp_pollfd_t descriptor;
    struct sockaddr_in client;
    socklen_t client_length = sizeof(client);
    int ready;
    int fd;
    descriptor.fd = state->listen_fd;
    descriptor.events = POLLIN;
    descriptor.revents = 0;
    ready = MCP_POLL(&descriptor, 1, timeout_ms);
    if (ready < 0 && errno == EINTR) return MCP_OK;
    if (ready <= 0) return ready < 0 ? MCP_ERR : MCP_OK;
    fd = MCP_ACCEPT(state->listen_fd, (struct sockaddr *)&client, &client_length);
    if (fd < 0) return errno == EINTR ? MCP_OK : MCP_ERR;
    handle_connection(transport, fd);
    MCP_CLOSE(fd);
    return MCP_OK;
}

static void http_close(mcp_transport_t *transport) {
    http_state_t *state = (http_state_t *)transport->state;
    if (state == NULL) return;
    if (state->listen_fd >= 0) MCP_CLOSE(state->listen_fd);
    free(state->request);
    free(state->response);
    free(state);
    transport->state = NULL;
}

static const mcp_transport_vtable_t HTTP_VTABLE = {
    .open = http_open,
    .poll = http_poll,
    .close = http_close,
};

int mcp_http_transport_init(mcp_transport_t *transport, const mcp_http_config_t *config) {
    http_state_t *state;
    if (transport == NULL) return MCP_ERR;
    memset(transport, 0, sizeof(*transport));
    state = (http_state_t *)calloc(1, sizeof(*state));
    if (state == NULL) return MCP_ERR;
    state->listen_fd = -1;
    state->config.bind_address = "127.0.0.1";
    state->config.port = 8080;
    state->config.backlog = 4;
    if (config != NULL) {
        if (config->bind_address != NULL) state->config.bind_address = config->bind_address;
        if (config->port != 0) state->config.port = config->port;
        if (config->backlog > 0) state->config.backlog = config->backlog;
        state->config.allowed_origin = config->allowed_origin;
    }
    transport->name = "http";
    transport->vtable = &HTTP_VTABLE;
    transport->state = state;
    return MCP_OK;
}
