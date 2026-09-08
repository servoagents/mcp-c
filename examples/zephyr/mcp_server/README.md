# MCP Zephyr example (ESP32)

This example connects to Wi‑Fi and starts an HTTP MCP server on port 8080.

Wi-Fi credentials are injected at build time and are not stored in source.
From the repository root, initialize dependencies if necessary, build, and
flash in one command:

```bash
WIFI_SSID='YourSSID' WIFI_PASS='YourPassword' \
  ./scripts/build-zephyr.sh --flash --monitor
```

Once connected, test from your LAN:

```bash
# Replace 192.168.x.x with the printed IP
curl -sS http://192.168.x.x:8080/mcp \
  -H 'Content-Type: application/json' \
  -H 'Accept: application/json, text/event-stream' \
  -H 'MCP-Protocol-Version: 2026-07-28' \
  -H 'Mcp-Method: server/discover' \
  --data-binary '{"jsonrpc":"2.0","id":1,"method":"server/discover","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}'
```

## Local Testing with native_sim

For local development and testing without hardware:

```bash
./scripts/build-zephyr.sh -b native_sim
./build-zephyr-native/zephyr/zephyr.exe
```

Test with curl (in another terminal):

```bash
Use the same request as above with host `127.0.0.1`.
```

The ESP32 devicetree overlay drives a hobby-servo PWM signal on GPIO18. Use a
separate suitable 5 V supply and common ground; do not power a servo motor from
the ESP32 3.3 V rail. `native_sim` uses deterministic simulated servo state.
