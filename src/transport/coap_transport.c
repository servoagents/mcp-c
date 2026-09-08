#include "mcp_transport.h"

#include <coap3/coap.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    mcp_coap_config_t config;
    coap_context_t *context;
    coap_endpoint_t *endpoint;
    mcp_server_t *server;
} coap_state_t;

static void release_response_data(coap_session_t *session, void *app_ptr) {
    (void)session;
    coap_delete_binary((coap_binary_t *)app_ptr);
}

static int request_is_json(const coap_pdu_t *request) {
    coap_opt_iterator_t iterator;
    coap_opt_t *option = coap_check_option(request, COAP_OPTION_CONTENT_FORMAT, &iterator);
    return option != NULL &&
           coap_decode_var_bytes(coap_opt_value(option), coap_opt_length(option)) ==
               COAP_MEDIATYPE_APPLICATION_JSON;
}

static coap_pdu_code_t response_code(const mcp_dispatch_result_t *result) {
    if (result->status == MCP_DISPATCH_NOTIFICATION) return COAP_RESPONSE_CODE_CHANGED;
    if (result->status == MCP_DISPATCH_TOO_LARGE) return COAP_RESPONSE_CODE_REQUEST_TOO_LARGE;
    if (result->status == MCP_DISPATCH_INTERNAL_ERROR) return COAP_RESPONSE_CODE_INTERNAL_ERROR;
    if (result->status < 0) return COAP_RESPONSE_CODE_BAD_REQUEST;
    return COAP_RESPONSE_CODE_CONTENT;
}

static void handle_mcp_post(coap_resource_t *resource, coap_session_t *session,
                            const coap_pdu_t *request, const coap_string_t *query,
                            coap_pdu_t *response) {
    coap_state_t *state = (coap_state_t *)coap_resource_get_userdata(resource);
    coap_binary_t *body;
    const uint8_t *request_data;
    size_t request_length;
    size_t offset;
    size_t total;
    mcp_request_ctx_t context;
    mcp_dispatch_result_t result;
    if (!request_is_json(request)) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_UNSUPPORTED_CONTENT_FORMAT);
        return;
    }
    if (!coap_get_data_large(request, &request_length, &request_data, &offset, &total) ||
        offset != 0 || request_length != total) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }
    if (request_length > mcp_server_max_request_size(state->server)) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_REQUEST_TOO_LARGE);
        return;
    }
    body = coap_new_binary(state->config.max_response_size);
    if (body == NULL) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_INTERNAL_ERROR);
        return;
    }
    memset(&context, 0, sizeof(context));
    context.transport_name = "coap";
    context.transport_context = session;
    result = mcp_server_handle(state->server, &context, (const char *)request_data,
                               request_length, (char *)body->s, body->length);
    coap_pdu_set_code(response, response_code(&result));
    if (result.status == MCP_DISPATCH_NOTIFICATION || result.response_length == 0) {
        coap_delete_binary(body);
        return;
    }
    body->length = result.response_length;
    if (!coap_add_data_large_response(resource, session, request, response, query,
                                      COAP_MEDIATYPE_APPLICATION_JSON, -1, 0,
                                      body->length, body->s,
                                      release_response_data, body)) {
        coap_delete_binary(body);
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_INTERNAL_ERROR);
    }
}

static int coap_open(mcp_transport_t *transport, mcp_server_t *server) {
    coap_state_t *state = (coap_state_t *)transport->state;
    coap_addr_info_t *addresses;
    coap_addr_info_t *address;
    coap_str_const_t bind_name;
    coap_resource_t *resource;
    coap_startup();
    state->server = server;
    state->context = coap_new_context(NULL);
    if (state->context == NULL) return MCP_ERR;
    coap_context_set_block_mode(state->context,
                                COAP_BLOCK_USE_LIBCOAP | COAP_BLOCK_SINGLE_BODY);
    bind_name.s = (const uint8_t *)state->config.bind_address;
    bind_name.length = strlen(state->config.bind_address);
    addresses = coap_resolve_address_info(&bind_name, state->config.port, 0, 0, 0,
                                          AI_PASSIVE | AI_NUMERICHOST,
                                          COAP_URI_SCHEME_COAP_BIT,
                                          COAP_RESOLVE_TYPE_LOCAL);
    if (addresses == NULL) return MCP_ERR;
    for (address = addresses; address != NULL; address = address->next) {
        state->endpoint = coap_new_endpoint(state->context, &address->addr, address->proto);
        if (state->endpoint != NULL) break;
    }
    coap_free_address_info(addresses);
    if (state->endpoint == NULL) return MCP_ERR;

    resource = coap_resource_init(coap_make_str_const("mcp"), 0);
    if (resource == NULL) return MCP_ERR;
    coap_resource_set_userdata(resource, state);
    coap_register_request_handler(resource, COAP_REQUEST_POST, handle_mcp_post);
    coap_add_attr(resource, coap_make_str_const("rt"),
                  coap_make_str_const("\"mcp.server\""), 0);
    coap_add_attr(resource, coap_make_str_const("ct"), coap_make_str_const("50"), 0);
    coap_add_resource(state->context, resource);
    printf("Experimental MCP CoAP listening on coap://%s:%u/mcp\n",
           state->config.bind_address, (unsigned int)state->config.port);
    return MCP_OK;
}

static int coap_poll_transport(mcp_transport_t *transport, int timeout_ms) {
    coap_state_t *state = (coap_state_t *)transport->state;
    int result = coap_io_process(state->context, timeout_ms < 0 ?
                                 COAP_IO_WAIT : (uint32_t)timeout_ms);
    return result < 0 ? MCP_ERR : MCP_OK;
}

static void coap_close_transport(mcp_transport_t *transport) {
    coap_state_t *state = (coap_state_t *)transport->state;
    if (state == NULL) return;
    if (state->context != NULL) coap_free_context(state->context);
    coap_cleanup();
    free(state);
    transport->state = NULL;
}

static const mcp_transport_vtable_t COAP_VTABLE = {
    .open = coap_open,
    .poll = coap_poll_transport,
    .close = coap_close_transport,
};

int mcp_coap_transport_init(mcp_transport_t *transport, const mcp_coap_config_t *config) {
    coap_state_t *state;
    if (transport == NULL) return MCP_ERR;
    memset(transport, 0, sizeof(*transport));
    state = (coap_state_t *)calloc(1, sizeof(*state));
    if (state == NULL) return MCP_ERR;
    state->config.bind_address = "127.0.0.1";
    state->config.port = 5683;
    state->config.max_response_size = 8192;
    if (config != NULL) {
        if (config->bind_address != NULL) state->config.bind_address = config->bind_address;
        if (config->port != 0) state->config.port = config->port;
        if (config->max_response_size != 0)
            state->config.max_response_size = config->max_response_size;
    }
    transport->name = "coap";
    transport->vtable = &COAP_VTABLE;
    transport->state = state;
    return MCP_OK;
}
