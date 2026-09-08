---
layout: default
title: MCP-C Documentation
nav_order: 1
permalink: /
---

Welcome to the MCP-C documentation. MCP-C is a small portable implementation of
the stateless MCP `2026-07-28` server core with pluggable transports.

### Quick Start

Use the repository README as the primary getting-started guide:

* [Getting Started (repo README)](https://github.com/servoagents/mcp-c#readme)

### Core

* **Core** – bounded JSON-RPC parsing and response writing
* **Transport** – independent bounded-poll instances over one tool registry
* **Server** – built-in `server/discover`, `tools/list`, and `tools/call`

### Key Pages

* [Architecture](architecture.md)
* [Transports Overview](transports/README.md)
  * [HTTP Transport](transports/http.md)
  * [CoAP](transports/coap.md)
  * [MQTT 5](transports/mqtt.md)
* [Benchmarks and network impairment](benchmarking.md)

### Platforms

Linux and Zephyr/ESP32 are supported. CoAP currently uses the POSIX libcoap
adapter; the MQTT binding core is portable and awaits native platform adapters.

### License

This project is released under the [MIT License (LICENSE.txt)](https://github.com/servoagents/mcp-c/blob/main/LICENSE.txt).
