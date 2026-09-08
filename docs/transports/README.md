---
layout: default
title: Transports Overview
nav_order: 3
---

# Transports

| Transport | Classification | Implementation |
|---|---|---|
| [HTTP](http.md) | Standard MCP Streamable HTTP subset | POSIX and Zephyr |
| [stdio](stdio.md) | Standard MCP | POSIX |
| [CoAP](coap.md) | Experimental custom binding | POSIX/libcoap |
| [MQTT 5](mqtt.md) | Experimental custom binding | Portable binding core; native adapter required |

All bindings carry exact UTF-8 MCP JSON-RPC messages. CoAP resources and MQTT
topics route messages; they do not replace MCP method names or JSON-RPC IDs.
