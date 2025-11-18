---
layout: default
title: Architecture
nav_order: 2
---

MCP-C separates protocol logic from networking so the same JSON-RPC core runs on Linux and constrained platforms (Zephyr, ESP32) with minimal changes.

The system is intentionally small: the core parses and dispatches JSON-RPC, while transports carry raw payloads and map replies back to clients. Transports implement a compact interface and may be written as polling drivers or using async callbacks.

## Components

```text
Application  <-- JSON-RPC -->  MCP Server Core  <-- mcp_transport_t -->  Transport Layer
```

- MCP Server Core: handler registry, dispatch, session management
- Transport Layer: pluggable (HTTP implemented; MQTT, Zenohr and CoAP planned)

Key locations:

- `src/server/` — server core
- `src/core/` — message parsing and sessions
- `src/transport/` — transports

## Transport Interface

Transports implement a compact contract (`include/mcp_transport.h`): init, poll, send, close.

```c
typedef struct mcp_transport {
    const char *name;
    int (*init)(struct mcp_server *srv, void *cfg);
    int (*poll)(struct mcp_server *srv);
    int (*send)(struct mcp_session *s, const uint8_t *data, size_t len);
    void (*close)(struct mcp_server *srv);
} mcp_transport_t;
```

`poll()` is the integration point. Two simple patterns:

- Polling/pump: call the library pump inside `poll()`; when you have a full payload call `mcp_server_dispatch(srv, sess, buf, len)`.
- Callback/async: register network callbacks. If they run in the server thread they may call `mcp_server_dispatch()` directly. If they run off‑thread, enqueue payloads and drain them in `poll()` (preferred on embedded).

**Threading note**: the MCP core is single‑threaded. Do not call core APIs from arbitrary threads; either protect access or defer to `poll()`.

Request flow (compact): receive bytes → deliver buffer to core → parse & dispatch → send response via `transport->send()`.

## Extending the Project

- Add transport sources under `src/transport/` and expose factories as needed.
- Add a CMake option (e.g., `MCP_ENABLE_X=ON`) and conditionally compile the transport.
