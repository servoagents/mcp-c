# mcp-c

`mcp-c` is a small, transport-neutral C server for MCP protocol revision
`2026-07-28`. It targets POSIX and Zephyr/ESP32 and keeps the request hot path
bounded and allocation-free inside the semantic core.

Implemented MCP methods:

- `server/discover`
- `tools/list`
- `tools/call`

The 2026 revision is stateless: there is no `initialize` handshake, protocol
session, or `Mcp-Session-Id`. Every request carries its version and client
capabilities in `params._meta`.

## Build and run

Requirements: CMake, Ninja or Make, and a C99 compiler. libcoap is detected
automatically; without it, HTTP and stdio still build.

```bash
make                  # one-command build
./scripts/run.sh      # one-command build + HTTP run
```

The default endpoint is <http://127.0.0.1:8080/mcp>. Run HTTP and CoAP
together with:

```bash
./scripts/run.sh --transport all
```

Run all reproducible checks, including ASan/UBSan and local HTTP/CoAP
integration:

```bash
make prove
```

The proof command writes a machine-readable artifact to
`build-proof/proof.json` containing the test status, binary size, and SHA-256.
Generate repeatable HTTP/CoAP latency and size measurements with
`make benchmark`; the CSV is written to `build-benchmark/results.csv`.

## Try it

```bash
curl http://127.0.0.1:8080/mcp \
  -H 'Content-Type: application/json' \
  -H 'Accept: application/json, text/event-stream' \
  -H 'MCP-Protocol-Version: 2026-07-28' \
  -H 'Mcp-Method: server/discover' \
  --data-binary '{"jsonrpc":"2.0","id":1,"method":"server/discover","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}'
```

The Linux and Zephyr examples register the same tools: `echo`,
`sensor.read_temperature`, `servo.set_angle`, and `servo.get_angle`. On Linux
and `native_sim` the servo is deterministic simulated state. The ESP32 build
uses LEDC PWM on GPIO18 with a 20 ms period and a bounded 0–180 degree input.

## ESP32

With the Zephyr SDK installed (validated with 0.17.4), the first invocation
creates a local west workspace, checks out pinned Zephyr `v4.3.0`, and installs
the Python dependencies. Build and flash in one command without storing Wi-Fi
credentials in Git:

```bash
WIFI_SSID='your-network' WIFI_PASS='your-password' \
  ./scripts/build-zephyr.sh --flash
```

Add `--monitor` to keep the serial console open. The log prints the assigned IP
and MCP URL. Connect the servo signal to GPIO18 and use a suitable external 5 V
supply with a common ground; do not power a servo motor from the ESP32's 3.3 V
pin.

After flashing, reproduce discovery plus a `servo.set_angle`/`get_angle`
round-trip (the script restores 90 degrees) and write an ESP32 proof artifact:

```bash
MCP_DEVICE_IP=192.168.x.x make prove-esp32
```

## Architecture

```text
application tools
      ↓
MCP 2026-07-28 semantic core + bounded JSON
      ↓
HTTP transport | stdio transport | CoAP adapter | MQTT 5 binding
      ↓
BSD/Zephyr sockets | libcoap | Zephyr MQTT adapter
      ↓
optional TLS/DTLS provider
```

`mcp_server_handle()` accepts one complete JSON-RPC message and returns one
complete response. `mcp_request_ctx_t` carries only opaque per-exchange data;
the core contains no sockets, CoAP tokens, MQTT topics, or broker handles.
Multiple bounded-poll transports may serve one registry in the same loop.

JSON is tokenized by a pinned MIT-licensed `jsmn` copy. IDs remain strings or
integers exactly as received. The bounded writer escapes application text and
fails closed on overflow. Limits such as `MCP_MAX_JSON_TOKENS`,
`MCP_MAX_TOOLS`, request size, response size, and MQTT replay-cache size are
configurable.

## Transport status

| Binding | Status | Notes |
|---|---|---|
| Streamable HTTP POST | Implemented | Standard MCP endpoint and 2026 routing headers; JSON responses only |
| stdio | Implemented | Standard newline-delimited MCP messages |
| CoAP | Experimental | Custom `POST /mcp`, exact MCP JSON payload, libcoap Block1/Block2 and CoRE discovery |
| MQTT 5 | Binding implemented | Custom namespace, Response Topic/Correlation Data policy, expiry and QoS 1 replay suppression; network adapter remains platform-specific |

HTTP SSE responses, `subscriptions/listen`, authorization, DTLS/TLS
configuration, and multi-round-trip `input_required` results are not yet
implemented. CoAP and MQTT are custom IoT bindings, not standard MCP
transports. See [the architecture notes](docs/architecture.md) and the
[transport documents](docs/transports/README.md) for precise behavior.

The implementation tracks the official
[MCP 2026-07-28 specification](https://modelcontextprotocol.io/specification/2026-07-28)
and its [Streamable HTTP binding](https://modelcontextprotocol.io/specification/2026-07-28/basic/transports/streamable-http).

## License

MIT. The optional external libcoap dependency is BSD-2-Clause; `jsmn` retains
its MIT license under `third_party/jsmn/`.
