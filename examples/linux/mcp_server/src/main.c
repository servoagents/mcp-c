#include "demo_tools.h"
#include "mcp_transport.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile sig_atomic_t running = 1;
static void stop_server(int signal_number) { (void)signal_number; running = 0; }

static void usage(const char *program) {
    fprintf(stderr, "Usage: %s [--transport http|stdio|coap|all] [--bind ADDRESS]\n", program);
}

int main(int argc, char **argv) {
    const char *mode = "http";
    const char *bind_address = "127.0.0.1";
    mcp_server_config_t server_config = {
        .name = "mcp-c-demo",
        .version = MCP_VERSION,
        .instructions = "Control a bounded servo and inspect a simulated sensor.",
        .max_request_size = 4096,
    };
    mcp_http_config_t http_config;
    mcp_coap_config_t coap_config;
    mcp_transport_t transports[3];
    size_t transport_count = 0;
    size_t i;
    mcp_server_t *server;
    servo_backend_t servo;
    demo_fake_servo_t fake_servo;

    for (i = 1; i < (size_t)argc; ++i) {
        if (strcmp(argv[i], "--transport") == 0 && i + 1 < (size_t)argc) mode = argv[++i];
        else if (strcmp(argv[i], "--bind") == 0 && i + 1 < (size_t)argc) bind_address = argv[++i];
        else { usage(argv[0]); return 2; }
    }
    server = mcp_server_init(&server_config);
    if (server == NULL) { fprintf(stderr, "Unable to create MCP server\n"); return 1; }
    demo_fake_servo_init(&servo, &fake_servo);
    if (demo_register_tools(server, &servo) != MCP_OK) {
        fprintf(stderr, "Unable to register demo tools\n"); mcp_server_deinit(server); return 1;
    }
    memset(transports, 0, sizeof(transports));
    memset(&http_config, 0, sizeof(http_config));
    http_config.bind_address = bind_address;
    http_config.port = 8080;
    memset(&coap_config, 0, sizeof(coap_config));
    coap_config.bind_address = bind_address;
    coap_config.port = 5683;

    if (strcmp(mode, "http") == 0 || strcmp(mode, "all") == 0) {
        if (mcp_http_transport_init(&transports[transport_count++], &http_config) != MCP_OK) goto fail;
    }
    if (strcmp(mode, "stdio") == 0) {
        if (mcp_stdio_transport_init(&transports[transport_count++], NULL) != MCP_OK) goto fail;
    }
    if (strcmp(mode, "coap") == 0 || strcmp(mode, "all") == 0) {
#ifdef MCP_HAVE_COAP
        if (mcp_coap_transport_init(&transports[transport_count++], &coap_config) != MCP_OK) goto fail;
#else
        fprintf(stderr, "This build does not include libcoap support\n"); goto fail;
#endif
    }
    if (transport_count == 0) { usage(argv[0]); goto fail; }
    for (i = 0; i < transport_count; ++i) {
        if (mcp_transport_open(&transports[i], server) != MCP_OK) {
            fprintf(stderr, "Unable to open %s transport\n", transports[i].name); goto fail;
        }
    }
    signal(SIGINT, stop_server);
    signal(SIGTERM, stop_server);
    while (running) {
        for (i = 0; i < transport_count; ++i) {
            if (mcp_transport_poll(&transports[i], transport_count > 1 ? 10 : 250) != MCP_OK)
                running = 0;
        }
    }
    for (i = 0; i < transport_count; ++i) mcp_transport_close(&transports[i]);
    mcp_server_deinit(server);
    return 0;

fail:
    for (i = 0; i < transport_count; ++i) mcp_transport_close(&transports[i]);
    mcp_server_deinit(server);
    return 1;
}
