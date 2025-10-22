# MCP Linux Example

A minimal MCP HTTP server running on Linux that reuses the shared mcp library.

Build (from `mcp/`):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Run:

```bash
./build/examples/linux/mcp_server/mcp-linux-mcp_server
```
