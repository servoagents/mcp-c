# mcp-c

A tiny, portable C implementation of an MCP-style JSON-RPC server with a pluggable transport interface. It includes:

- Core server with handler registry and minimal JSON field extraction
- HTTP transport using BSD sockets
- Examples for Linux and Zephyr

This project is part of a broader effort to explore AI agents for IoT/IIoT and robotics (edge devices like ESP32 running Zephyr), so it's a learning/reference implementation for embedded targets. It focuses on a minimal subset sufficient to test with external clients. Not production-ready.

## Overview

This library cleanly separates JSON-RPC protocol logic from networking so it runs on Linux and Zephyr with minimal changes:

- Core: minimal JSON-RPC parsing (extracts only "id" and "method").
- Transport: pluggable `mcp_transport_t` (init, poll, send, close) driven by a simple loop.
- Server: tiny handler registry (e.g., "initialize", "tools/list", "tools/call"), dispatches requests and writes responses via the transport.

Currently, the HTTP transport:

- Listens on TCP port 8080 (override with `MCP_HTTP_PORT`).
- Accepts HTTP/1.1 POSTs and treats the body as JSON-RPC (path not enforced; `/` or `/mcp` both work).
- One request per connection; socket closes after the reply.

## Build

### Linux

Requirements: CMake and a C compiler

```bash
mkdir -p build && cd build
cmake .. -DMCP_ENABLE_EXAMPLES=ON
cmake --build . --config Release
./examples/linux/mcp_server/mcp-linux-mcp_server
```

The server listens on <http://0.0.0.0:8080>. See “Test with curl (Linux and Zephyr)” below.

### Zephyr

Prereqs: Zephyr SDK and Python with west installed.

First create a Python virtual environment and install west

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

From the `mcp/` folder:

```bash
./scripts/build-zephyr.sh --init
./scripts/build-zephyr.sh -b esp32_devkitc_wroom/esp32/procpu --wifi-ssid "your_wifi_ssid" --wifi-pass "your_wifi_password"
```

Then flash and monitor:

```bash
cd examples/zephyr/mcp_server
west flash
west espressif monitor
```

The board-specific overlays/configs are placed under `examples/zephyr/mcp_server/boards/`.

The app starts an HTTP server on port 8080. Ensure your board has networking configured (Ethernet or Wi‑Fi) and an IP address. For testing convenience, the example logs its DHCP‑assigned IPv4 address after connecting so you can copy/paste it for curl; you can disable or change this in `examples/zephyr/mcp_server/src/main.c`. See “Test with curl (Linux and Zephyr)” below.

### Test with curl (Linux and Zephyr)

Use these requests against either:

- Linux: <http://localhost:8080>
- Zephyr: http://DEVICE_IP:8080 (replace DEVICE_IP with the IP printed by the device)

```bash
# Initialize the session and get capabilities/protocol version
curl -s -X POST http://HOST:8080 -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":"1","method":"initialize","params":{}}'

# List available tools exposed by the server
curl -s -X POST http://HOST:8080 -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":"2","method":"tools/list","params":{}}'

# Call the "echo" tool; this demo returns a fixed greeting
curl -s -X POST http://HOST:8080 -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":"3","method":"tools/call","params":{"name":"echo","arguments":{"text":"hi"}}}'
```

Replace HOST with localhost (Linux) or the Zephyr device IP.

## Transports

MCP messages are JSON-RPC over UTF-8. The spec defines two standard transports:

- stdio: client launches the server as a subprocess and exchanges JSON-RPC lines over stdin/stdout.
- Streamable HTTP: server handles HTTP POSTs for client-to-server messages and can stream server messages using Server‑Sent Events (SSE) over GET or POST responses.

This repo implements a basic HTTP POST handler only (single JSON response per request). SSE streaming, resumability, and stdio transport are future work.

Security considerations when exposing HTTP:

- Validate the Origin header to mitigate DNS rebinding.
- Prefer binding to 127.0.0.1 for local development; add authentication/TLS before exposing beyond localhost.

## Features

- JSON-RPC 2.0 request handling (initialize, tools/list, tools/call via handlers)
- Pluggable transports (HTTP included)
- Linux and Zephyr example apps

## Library API (quick look)

- `mcp_server_init`, `mcp_server_run`, `mcp_server_register`
- `mcp_transport_http()` returns the HTTP transport
- Handlers have signature:
  `int handler(mcp_session_t *session, const mcp_message_t *req, char *resp_buf, size_t resp_buf_sz)`

Handlers must fill `resp_buf` with a valid JSON-RPC response object.

## Notes and Limitations

- Minimal JSON field extraction (id/method) to keep footprint small; no full JSON parsing of params.
- HTTP only; no TLS yet. Additional transports (CoAP, MQTT-SN) and TLS could be added by implementing `mcp_transport_t`.
- The HTTP transport currently accepts any URL path and processes one request per connection.
- Responses currently hardcode `id` in examples; adapt to use the request `id` in real code.
