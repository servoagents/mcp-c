---
layout: default
title: Transports Overview
nav_order: 3
---

MCP-C uses a pluggable transport interface so JSON-RPC messages can ride over different protocols while the core logic stays identical.

## Available Transports

| Transport | Status | Platform Support | Use Case |
|-----------|--------|------------------|----------|
| [HTTP](http.md) | Implemented | Linux, Zephyr | Standard MCP, web compatibility |
| [MQTT](mqtt.md) | Planned | Linux, Zephyr, ESP32 | IoT, publish/subscribe scenarios |

## Transport Interface

All transports implement the same minimal interface defined in `include/mcp_transport.h`:

```c
typedef struct mcp_transport {
    const char *name;
    int (*init)(struct mcp_server *srv, void *cfg);
    int (*poll)(struct mcp_server *srv);
    int (*send)(struct mcp_session *s, const uint8_t *data, size_t len);
    void (*close)(struct mcp_server *srv);
} mcp_transport_t;
```

`poll()` is called by the server loop. If a transport gets data via off-thread callbacks, enqueue buffers and drain them in `poll()` before calling `mcp_server_dispatch()`.
