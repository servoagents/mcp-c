# MCP Linux example

From the repository root, build and run the HTTP server with one command:

```bash
./scripts/run.sh
```

The endpoint is `http://127.0.0.1:8080/mcp`. To run every available local
transport against the same tool registry:

```bash
./scripts/run.sh --transport all
```

Modes are `http`, `stdio`, `coap`, and `all`; CoAP is present when libcoap was
detected. See the repository README for a complete `server/discover` request.
