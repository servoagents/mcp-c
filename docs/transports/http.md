---
layout: default
title: HTTP Transport
parent: Transports Overview
---

# Streamable HTTP

The endpoint is `POST /mcp`. Each connection handles one stateless request and
closes after its response. The implementation validates:

- method, endpoint, bounded headers, `Content-Length`, and complete body;
- `Content-Type: application/json`;
- `Accept` containing both `application/json` and `text/event-stream`;
- `MCP-Protocol-Version` and `Mcp-Method` on every request;
- `Mcp-Name` for named operations and all header/body equality;
- `Origin` against the configured exact allow value (an unconfigured server
  rejects requests that carry `Origin`);
- request size and a five-second receive timeout.

Notifications receive `202 Accepted` with no body. JSON-RPC protocol errors use
the status required by MCP where defined. There is no session header.

The current server always chooses a single `application/json` response. It does
not yet emit SSE, request-scoped progress notifications, or
`subscriptions/listen` streams.
