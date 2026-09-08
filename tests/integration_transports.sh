#!/usr/bin/env bash
set -euo pipefail

server=${1:?mcp-server path required}
test_dir="$(mktemp -d)"
server_pid=""

cleanup() {
  if [[ -n "${server_pid}" ]]; then
    kill "${server_pid}" 2>/dev/null || true
    wait "${server_pid}" 2>/dev/null || true
  fi
  rm -rf "${test_dir}"
}
trap cleanup EXIT INT TERM

"${server}" --transport all >"${test_dir}/server.log" 2>&1 &
server_pid=$!

meta='"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}'
discover="{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"server/discover\",\"params\":{${meta}}}"
set_angle="{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"servo.set_angle\",\"arguments\":{\"angle\":133},${meta}}}"
get_angle="{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"servo.get_angle\",\"arguments\":{},${meta}}}"

for _ in {1..30}; do
  if curl -sS --max-time 1 http://127.0.0.1:8080/mcp \
      -H 'Content-Type: application/json' \
      -H 'Accept: application/json, text/event-stream' \
      -H 'MCP-Protocol-Version: 2026-07-28' \
      -H 'Mcp-Method: server/discover' --data-binary "${discover}" \
      >"${test_dir}/discover.json" 2>/dev/null; then
    break
  fi
  sleep 0.1
done

grep -q '"supportedVersions":\["2026-07-28"\]' "${test_dir}/discover.json"

curl -fsS http://127.0.0.1:8080/mcp \
  -H 'Content-Type: application/json' \
  -H 'Accept: application/json, text/event-stream' \
  -H 'MCP-Protocol-Version: 2026-07-28' \
  -H 'Mcp-Method: tools/call' -H 'Mcp-Name: servo.set_angle' \
  --data-binary "${set_angle}" >"${test_dir}/set.json"
grep -q '"value":133' "${test_dir}/set.json"

coap-client -m post -t application/json -e "${get_angle}" \
  coap://127.0.0.1:5683/mcp >"${test_dir}/get.json"
grep -q '"value":133' "${test_dir}/get.json"

coap-client -m get coap://127.0.0.1:5683/.well-known/core \
  >"${test_dir}/core-links.txt"
grep -q '</mcp>;ct=50;rt="mcp.server"' "${test_dir}/core-links.txt"

awk -v meta="${meta}" 'BEGIN {
  text=""; for (i=0; i<1800; i++) text=text "x";
  printf "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"echo\",\"arguments\":{\"text\":\"%s\"},%s}}", text, meta
}' >"${test_dir}/large.json"
coap-client -m post -b 64 -t application/json -f "${test_dir}/large.json" \
  coap://127.0.0.1:5683/mcp >"${test_dir}/large-response.json"
test "$(wc -c <"${test_dir}/large-response.json")" -gt 1800
grep -q '"id":4' "${test_dir}/large-response.json"

coap-client -m post -t application/json -e '{' \
  coap://127.0.0.1:5683/mcp >"${test_dir}/malformed-response.json" 2>&1 || true
grep -q '"code":-32700' "${test_dir}/malformed-response.json"

bad_status=$(curl -sS -o /dev/null -w '%{http_code}' http://127.0.0.1:8080/mcp \
  -H 'Content-Type: application/json' -H 'Accept: application/json, text/event-stream' \
  --data-binary "${discover}")
test "${bad_status}" = 400

method_status=$(curl -sS -o /dev/null -w '%{http_code}' -X GET \
  http://127.0.0.1:8080/mcp)
test "${method_status}" = 405
path_status=$(curl -sS -o /dev/null -w '%{http_code}' http://127.0.0.1:8080/other \
  -H 'Content-Type: application/json' -H 'Accept: application/json, text/event-stream' \
  -H 'MCP-Protocol-Version: 2026-07-28' -H 'Mcp-Method: server/discover' \
  --data-binary "${discover}")
test "${path_status}" = 404
origin_status=$(curl -sS -o /dev/null -w '%{http_code}' http://127.0.0.1:8080/mcp \
  -H 'Content-Type: application/json' -H 'Accept: application/json, text/event-stream' \
  -H 'MCP-Protocol-Version: 2026-07-28' -H 'Mcp-Method: server/discover' \
  -H 'Origin: https://untrusted.example' --data-binary "${discover}")
test "${origin_status}" = 403

printf 'transport integration: PASS (HTTP strict routing, CoAP CON/discovery/Block1+Block2/malformed body, shared servo state)\n'
