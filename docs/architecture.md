---
layout: default
title: Architecture
nav_order: 2
---

# Architecture

The semantic boundary is one complete MCP JSON-RPC message in and zero or one
complete JSON-RPC response out:

```text
tool callbacks
    ↓
server/discover · tools/list · tools/call
    ↓
message parser · bounded JSON writer · tool registry
    ↓
mcp_server_handle(server, request_context, request, response)
    ↓
HTTP · stdio · CoAP · MQTT 5 binding
    ↓
sockets · libcoap · platform MQTT library
    ↓
TLS/DTLS provider · network
```

The core owns no transport. `mcp_request_ctx_t.transport_context` is opaque and
valid only for the exchange. Transport-specific concepts therefore stay at the
edge: file descriptors in HTTP, CoAP session/token state in libcoap, and MQTT
topics/correlation properties in an MQTT adapter.

## Core lifecycle

1. Initialize a server with immutable identity and bounded request limits.
2. Register tool definitions and callbacks in deterministic order.
3. Initialize any number of transport instances.
4. Open each transport against the same server and call its bounded `poll()`.
5. Close transports, then destroy the server.

The 2026 protocol is stateless. A tool can maintain explicit application state
(the servo's commanded angle, for example), but the server never infers client
identity, capabilities, or protocol version from a previous request.

## JSON and memory

The parser uses fixed stack token arrays with `MCP_MAX_JSON_TOKENS`. Parsed
values are non-owning spans into the request and stay valid only during the
call. No DOM is built. The writer tracks object/array structure and escapes
strings into a caller-supplied bounded response buffer.

The server object and transport buffers allocate during initialization. The
semantic request path performs no allocation. libcoap owns CoAP reassembly and
large-response lifetime; the MQTT replay cache allocates all slots at startup.

## Security layer

Cryptography belongs under transports. The normal Zephyr secure-socket/Mbed TLS
path remains the classical baseline. A future PQC profile should use external
wolfSSL (not vendored) and compare X25519 with `X25519MLKEM768` while keeping
ordinary certificate authentication constant. CoAP should use libcoap's
wolfSSL DTLS backend. No MCP message or MQTT/CoAP binding changes are needed to
add those profiles.
