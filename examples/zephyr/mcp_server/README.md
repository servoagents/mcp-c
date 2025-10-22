# MCP Zephyr Example (ESP32)

This example connects to Wi‑Fi and starts an HTTP MCP server on port 8080.

- Wi‑Fi credentials are provided via CMake definitions passed from the build script using environment variables `WIFI_SSID` and `WIFI_PASS`.
- For testing purposes, the device logs its IPv4 address once DHCP completes. This helps you copy the IP for curl/tests. You can remove or modify this behavior in `src/main.c` (the small `net_mgmt` IPv4 event callback).

Build and flash:

```bash
# From mcp/
./scripts/build-zephyr.sh --init
./scripts/build-zephyr.sh -b esp32_devkitc_wroom/esp32/procpu \
  --wifi-ssid "YourSSID" --wifi-pass "YourPassword"

cd examples/zephyr
west flash
west espressif monitor
```

Once connected, test from your LAN:

```bash
# Replace 192.168.x.x with the printed IP
curl -s http://192.168.x.x:8080 -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":"1","method":"initialize","params":{}}'
```
