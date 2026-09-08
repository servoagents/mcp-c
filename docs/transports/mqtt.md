---
layout: default
title: Experimental MQTT 5 Binding
parent: Transports Overview
---

# MCP over MQTT 5 (custom)

This is an experimental binding, not an MCP-standard transport. The portable
`mcp_mqtt_bridge_t` enforces its semantic and safety rules while a platform
adapter performs broker I/O (Zephyr's native MQTT 5 library is the intended
embedded adapter).

## Namespace

- Requests: `mcp/v1/servers/<server-id>/requests`
- Discovery: `mcp/v1/discovery/<server-id>` (retained adapter publication)
- Presence: `mcp/v1/status/<server-id>` (retained online state and LWT offline)
- Response Topic: supplied by the client and restricted to a configured prefix

The request payload is one exact MCP JSON-RPC UTF-8 message. A valid request
uses QoS 1 and MQTT 5 properties:

- Response Topic
- non-empty binary Correlation Data
- Payload Format Indicator = UTF-8
- Content Type = `application/json`
- non-zero Message Expiry Interval

The response is published at QoS 1 to the validated Response Topic and copies
Correlation Data byte-for-byte. QoS 2 is not the default.

## Duplicate and stale command safety

QoS 1 can redeliver. The bridge fingerprints client identity, Correlation Data,
and JSON-RPC ID, then keeps a bounded time-limited cache of complete replies. A
duplicate receives the cached reply without executing the tool again. The
cache entry count, lifetime, and response size are configured at startup.
Expired commands, wildcard response topics, response topics outside the
allowlisted prefix, missing properties, and oversized payloads fail closed.

Broker discovery/LWT publication and reconnect logic are adapter duties. The
binding tests use a counted actuator callback and prove that two identical
deliveries execute it once while producing two correlated replies.

## Roadmap

A Zephyr native MQTT 5 adapter is the next required step; it must publish
retained discovery/status records, configure LWT, reconnect safely, and feed
decoded MQTT 5 properties into this binding. A later MQTT-SN profile must use a
separate topic convention and JSON-RPC ID correlation because MQTT-SN does not
provide MQTT 5 Response Topic or Correlation Data properties. Neither adapter
is claimed by the current implementation.
