#!/usr/bin/env bash
set -euo pipefail

server=${1:?mcp-server path required}
request='{"jsonrpc":"2.0","id":"stdio-proof","method":"server/discover","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}'
response=$(printf '%s\n' "${request}" | "${server}" --transport stdio)
grep -q '"id":"stdio-proof"' <<<"${response}"
grep -q '"supportedVersions":\["2026-07-28"\]' <<<"${response}"
printf 'stdio integration: PASS\n'
