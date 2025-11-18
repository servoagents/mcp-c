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

You can find more details about the architecture and transport implementations in `docs/architecture.md`.

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

#### ESP32 (WiFi)

```bash
# Setup
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

# Build and flash
./scripts/build-zephyr.sh --init
./scripts/build-zephyr.sh -b esp32_devkitc/esp32/procpu --wifi-ssid "SSID" --wifi-pass "PASS"
cd examples/zephyr/mcp_server
west flash && west espressif monitor
```

#### native_sim (Local Testing)

```bash
# Build and run
./scripts/build-zephyr.sh -b native_sim
./examples/zephyr/mcp_server/build/zephyr/zephyr.exe
```

Uses localhost:8080 with native offloaded sockets.

See `examples/zephyr/mcp_server/README.md` for details.

### Test with curl (Linux and Zephyr)

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

Replace HOST with `localhost` (Linux/native_sim) or the Zephyr device IP (ESP32).

## Transports

All transports conform to the same minimal interface (`mcp_transport_t`: init, poll, send, close). Current and planned support:

| Transport | Status      | Notes |
|-----------|-------------|-------|
| HTTP      | Implemented | Simple POST -> JSON-RPC, one request per connection (no SSE yet) |
| MQTT      | Planned     | Topic model (e.g. `mcp/<server>/req`, `mcp/<client>/resp`) for pub/sub edge devices |
| CoAP      | Planned     | Lightweight UDP for constrained devices; maps JSON-RPC to confirmable/non-confirmable messages |
| Zenoh     | Planned     | Ultra‑low latency distributed key/value; minimal pub/sub pattern similar to MQTT |

Spec reference (MCP): defines stdio and streamable HTTP (with optional SSE). This implementation currently covers only the POST portion of HTTP. Future work: SSE streaming, stdio transport, and the planned protocols above.

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

## Documentation

Rendered docs ([Just the Docs](https://just-the-docs.com/) theme): <https://servoagents.github.io/mcp-c/>

Preview locally:

```bash
docker run --rm -it -p 4000:4000 -v "$PWD/docs":/site -w /site ruby:3.2 bash -lc 'gem install bundler && bundle install && bundle exec jekyll serve -H 0.0.0.0'
```

Then open <http://localhost:4000/mcp-c/>

## Contribute

Contributions are welcome.

If unsure about a direction (e.g. new transport), open an issue first for alignment.

1. Fork the repository and create a feature branch: `git checkout -b feature/xyz`.
2. Build and run examples (Linux and/or Zephyr) to validate changes.
3. Keep changes minimal; avoid introducing heavy dependencies. Keep transports small.
4. Follow existing C style (see `.clang-format`).
5. Add/update documentation in `docs/` if you change public behavior.
6. Open a PR describing rationale, testing steps, and any limitations.

## License

MIT License – see [`LICENSE.txt`](LICENSE.txt).
