---
layout: default
title: stdio Transport
parent: Transports Overview
---

# stdio

Each MCP JSON-RPC message occupies one line on stdin. Responses occupy one line
on stdout; diagnostics go elsewhere. Embedded newlines must be JSON escapes.
The same mandatory 2026 `_meta` envelope is validated as on every transport.

Run with `./scripts/run.sh --transport stdio`.
