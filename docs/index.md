---
layout: default
title: MCP-C Documentation
nav_order: 1
permalink: /
---

Welcome to the MCP-C documentation. MCP-C is a tiny, portable C implementation of an MCP-style JSON-RPC server with pluggable transports.

### Quick Start

Use the repository README as the primary getting-started guide:

* [Getting Started (repo README)](https://github.com/servoagents/mcp-c#readme)

### Core

* **Core** – JSON-RPC parsing & session management
* **Transport** – Minimal interface (`mcp_transport_t`) for protocol adaptation
* **Server** – Registers handlers (initialize, tools/list, tools/call) and dispatches via `mcp_server_dispatch()`

### Key Pages

* [Architecture](architecture.md)
* [Transports Overview](transports/README.md)
  * [HTTP Transport](transports/http.md)
  * MQTT (planned)

### Platforms

Linux and Zephyr (ESP32, native_sim) supported; other embedded ports are straightforward if they provide sockets or an event API.

### License

This project is released under the [MIT License (LICENSE.txt)](https://github.com/servoagents/mcp-c/blob/main/LICENSE.txt).
