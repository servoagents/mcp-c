# MCP Zephyr Example (ESP32)

This example connects to Wi‑Fi and starts an HTTP MCP server on port 8080.

## Build for ESP32

Wi‑Fi credentials are provided via CMake definitions passed from the build script using environment variables `WIFI_SSID` and `WIFI_PASS`.

```bash
# From mcp-c/
./scripts/build-zephyr.sh --init
./scripts/build-zephyr.sh -b esp32_devkitc/esp32/procpu \
  --wifi-ssid "YourSSID" --wifi-pass "YourPassword"

cd examples/zephyr/mcp_server
west flash
west espressif monitor
```

Once connected, test from your LAN:

```bash
# Replace 192.168.x.x with the printed IP
curl -s http://192.168.x.x:8080 -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":"1","method":"initialize","params":{}}'
```

## Local Testing with native_sim

For local development and testing without hardware:

```bash
# Build for native_sim (uses localhost, no sudo required)
./scripts/build-zephyr.sh -b native_sim

# Run (from mcp-c/ root directory)
./examples/zephyr/mcp_server/build/zephyr/zephyr.exe
```

Test with curl (in another terminal):

```bash
curl -s http://localhost:8080 -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":"1","method":"initialize","params":{}}'
```

**Note:** The `native_sim` board uses Zephyr's native offloaded sockets, which bypass the Zephyr network stack and use the host OS network directly for simple localhost testing.
