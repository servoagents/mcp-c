#ifndef MCP_DEMO_TOOLS_H
#define MCP_DEMO_TOOLS_H

#include "mcp_server.h"

typedef struct servo_backend servo_backend_t;

struct servo_backend {
    int (*set_angle)(servo_backend_t *backend, int angle);
    int (*get_angle)(servo_backend_t *backend, int *angle);
    void *state;
};

typedef struct {
    int angle;
} demo_fake_servo_t;

void demo_fake_servo_init(servo_backend_t *backend, demo_fake_servo_t *state);
int demo_register_tools(mcp_server_t *server, servo_backend_t *servo);

#endif
