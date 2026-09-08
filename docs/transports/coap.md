---
layout: default
title: Experimental CoAP Binding
parent: Transports Overview
---

# MCP over CoAP (custom)

This is an experimental binding, not an MCP-standard transport.

- Endpoint: confirmable `POST coap://host:5683/mcp`
- Request and response payload: one exact MCP JSON-RPC UTF-8 message
- Content-Format: `application/json` (CoAP number 50)
- Correlation: JSON-RPC ID; CoAP token and message ID remain transport concerns
- Discovery: `GET /.well-known/core` advertises
  `</mcp>;ct=50;rt="mcp.server"`
- Reliability: normal CoAP CON retransmission
- Large payloads: libcoap-managed Block1 request reassembly and Block2 response
- Maximum size: the MCP request limit and configured CoAP response limit;
  oversized bodies fail explicitly
- Cancellation/streaming: request/response only; no Observe mapping is claimed

Example:

```bash
coap-client -m post -t application/json \
  -e '{"jsonrpc":"2.0","id":1,"method":"server/discover","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}' \
  coap://127.0.0.1:5683/mcp
```

libcoap remains an optional external dependency. DTLS configuration belongs to
libcoap and its TLS backend, not the MCP core.
