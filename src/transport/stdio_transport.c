#ifndef __ZEPHYR__

#include "mcp_transport.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    FILE *input;
    FILE *output;
    char *request;
    char *response;
    size_t request_capacity;
} stdio_state_t;

static int stdio_open(mcp_transport_t *transport, mcp_server_t *server) {
    stdio_state_t *state = (stdio_state_t *)transport->state;
    state->request_capacity = mcp_server_max_request_size(server) + 2;
    state->request = (char *)malloc(state->request_capacity);
    state->response = (char *)malloc(8192);
    return state->request != NULL && state->response != NULL ? MCP_OK : MCP_ERR;
}

static int stdio_poll(mcp_transport_t *transport, int timeout_ms) {
    stdio_state_t *state = (stdio_state_t *)transport->state;
    struct pollfd descriptor;
    mcp_request_ctx_t context;
    mcp_dispatch_result_t result;
    size_t length;
    int ready;
    descriptor.fd = fileno(state->input);
    descriptor.events = POLLIN;
    descriptor.revents = 0;
    ready = poll(&descriptor, 1, timeout_ms);
    if (ready < 0 && errno == EINTR) return MCP_OK;
    if (ready < 0) return MCP_ERR;
    if (ready == 0) return MCP_OK;
    if (fgets(state->request, (int)state->request_capacity, state->input) == NULL) return MCP_ERR;
    length = strlen(state->request);
    if (length == 0) return MCP_OK;
    if (state->request[length - 1] != '\n' && !feof(state->input)) {
        int c;
        while ((c = fgetc(state->input)) != '\n' && c != EOF) {}
        fputs("{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32600,"
              "\"message\":\"Request too large\"}}\n", state->output);
        fflush(state->output);
        return MCP_OK;
    }
    while (length > 0 && (state->request[length - 1] == '\n' ||
                          state->request[length - 1] == '\r')) --length;
    memset(&context, 0, sizeof(context));
    context.transport_name = "stdio";
    context.transport_context = state->output;
    result = mcp_server_handle(transport->server, &context, state->request, length,
                               state->response, 8192);
    if (result.status != MCP_DISPATCH_NOTIFICATION && result.response_length > 0) {
        fwrite(state->response, 1, result.response_length, state->output);
        fputc('\n', state->output);
        fflush(state->output);
    }
    return MCP_OK;
}

static void stdio_close(mcp_transport_t *transport) {
    stdio_state_t *state = (stdio_state_t *)transport->state;
    if (state == NULL) return;
    free(state->request);
    free(state->response);
    free(state);
    transport->state = NULL;
}

static const mcp_transport_vtable_t STDIO_VTABLE = {
    .open = stdio_open,
    .poll = stdio_poll,
    .close = stdio_close,
};

int mcp_stdio_transport_init(mcp_transport_t *transport, const mcp_stdio_config_t *config) {
    stdio_state_t *state;
    if (transport == NULL) return MCP_ERR;
    memset(transport, 0, sizeof(*transport));
    state = (stdio_state_t *)calloc(1, sizeof(*state));
    if (state == NULL) return MCP_ERR;
    state->input = config && config->input ? (FILE *)config->input : stdin;
    state->output = config && config->output ? (FILE *)config->output : stdout;
    transport->name = "stdio";
    transport->vtable = &STDIO_VTABLE;
    transport->state = state;
    return MCP_OK;
}

#else

#include "mcp_transport.h"
int mcp_stdio_transport_init(mcp_transport_t *transport, const mcp_stdio_config_t *config) {
    (void)transport;
    (void)config;
    return MCP_ERR;
}

#endif
