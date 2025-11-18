---
layout: default
title: HTTP Transport
nav_order: 4
parent: Transports Overview
---

The HTTP transport provides standard MCP JSON-RPC over HTTP POST requests. This is the default transport and offers maximum compatibility with existing MCP clients and tools.

## Overview

The HTTP transport implements a basic HTTP/1.1 server that:

- Listens on TCP port 8080 (configurable via `MCP_HTTP_PORT`)
- Accepts POST requests with JSON-RPC payloads
- Returns JSON-RPC responses in the HTTP body
- Uses one request per connection (connection closes after reply)

This is a **minimal implementation** focused on MCP compliance, not a full-featured HTTP server.

## Supported Platforms

- Linux (native BSD sockets)
- Zephyr RTOS (native_sim, ESP32 with WiFi)

## Usage

### Linux

```bash
# Build with HTTP transport (default)
mkdir -p build && cd build
cmake .. -DMCP_ENABLE_EXAMPLES=ON
cmake --build .

# Run server
./examples/linux/mcp_server/mcp-linux-mcp_server
# Server listens on http://0.0.0.0:8080
```

### Zephyr

```bash
# Build for ESP32
./scripts/build-zephyr.sh -b esp32_devkitc/esp32/procpu \
  --wifi-ssid "YOUR_SSID" --wifi-pass "YOUR_PASSWORD"

# Flash and monitor
cd examples/zephyr/mcp_server
west flash && west espressif monitor
```

## Testing

```bash
# Initialize session
curl -X POST http://localhost:8080 \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":"1","method":"initialize","params":{}}'

# List tools
curl -X POST http://localhost:8080 \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":"2","method":"tools/list","params":{}}'

# Call tool
curl -X POST http://localhost:8080 \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":"3","method":"tools/call","params":{"name":"echo","arguments":{"text":"hello"}}}'
```

## Configuration

```c
typedef struct {
    uint16_t port;        // TCP port (default: 8080)
    const char *bind_addr; // Bind address (default: "0.0.0.0")
} mcp_http_config_t;
```

