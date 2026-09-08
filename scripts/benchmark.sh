#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${project_dir}/build-benchmark"
iterations="${1:-25}"
server_pid=""
transport_mode=http

case "${iterations}" in
  *[!0-9]*|'') printf 'usage: %s [positive-iteration-count]\n' "$0" >&2; exit 2 ;;
esac
if [[ "${iterations}" -eq 0 ]]; then
  printf 'iteration count must be positive\n' >&2
  exit 2
fi

cleanup() {
  if [[ -n "${server_pid}" ]]; then
    kill "${server_pid}" 2>/dev/null || true
    wait "${server_pid}" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

cmake -S "${project_dir}" -B "${build_dir}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DMCP_ENABLE_TESTS=OFF -DMCP_ENABLE_COAP=ON
cmake --build "${build_dir}"
server="${build_dir}/examples/linux/mcp_server/mcp-server"
if grep -q '^LIBCOAP_FOUND:INTERNAL=1' "${build_dir}/CMakeCache.txt" 2>/dev/null; then
  transport_mode=all
fi
"${server}" --transport "${transport_mode}" >"${build_dir}/server.log" 2>&1 &
server_pid=$!

meta='"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}'
discover="{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"server/discover\",\"params\":{${meta}}}"
ready=false
for _ in {1..30}; do
  if curl -fsS --max-time 1 http://127.0.0.1:8080/mcp \
      -H 'Content-Type: application/json' \
      -H 'Accept: application/json, text/event-stream' \
      -H 'MCP-Protocol-Version: 2026-07-28' \
      -H 'Mcp-Method: server/discover' --data-binary "${discover}" >/dev/null; then
    ready=true
    break
  fi
  sleep 0.1
done
if ! ${ready}; then
  printf 'server did not become ready; see %s\n' "${build_dir}/server.log" >&2
  exit 1
fi

csv="${build_dir}/results.csv"
binary_bytes=$(wc -c <"${server}")
printf 'transport,operation,iteration,payload_bytes,response_bytes,latency_ms,binary_bytes\n' >"${csv}"

measure() {
  local operation=$1
  local name=$2
  local request=$3
  local iteration=$4
  local request_bytes=${#request}
  local response_bytes
  local latency_ms
  local timing
  local start_ns
  local end_ns

  timing=$(curl -fsS -o "${build_dir}/response.json" -w '%{time_total}' \
    http://127.0.0.1:8080/mcp \
    -H 'Content-Type: application/json' \
    -H 'Accept: application/json, text/event-stream' \
    -H 'MCP-Protocol-Version: 2026-07-28' \
    -H 'Mcp-Method: tools/call' -H "Mcp-Name: ${name}" \
    --data-binary "${request}")
  grep -q '"isError":false' "${build_dir}/response.json"
  response_bytes=$(wc -c <"${build_dir}/response.json")
  latency_ms=$(awk -v seconds="${timing}" 'BEGIN { printf "%.3f", seconds * 1000 }')
  printf 'http,%s,%s,%s,%s,%s,%s\n' "${operation}" "${iteration}" \
    "${request_bytes}" "${response_bytes}" "${latency_ms}" "${binary_bytes}" >>"${csv}"

  if [[ "${transport_mode}" == all ]]; then
    start_ns=$(date +%s%N)
    coap-client -m post -b 64 -t application/json -e "${request}" \
      coap://127.0.0.1:5683/mcp >"${build_dir}/response.json"
    end_ns=$(date +%s%N)
    grep -q '"isError":false' "${build_dir}/response.json"
    response_bytes=$(wc -c <"${build_dir}/response.json")
    latency_ms=$(awk -v elapsed="$((end_ns - start_ns))" \
      'BEGIN { printf "%.3f", elapsed / 1000000 }')
    printf 'coap,%s,%s,%s,%s,%s,%s\n' "${operation}" "${iteration}" \
      "${request_bytes}" "${response_bytes}" "${latency_ms}" "${binary_bytes}" >>"${csv}"
  fi
}

for payload_size in 16 512 1800; do
  text_value=$(awk -v count="${payload_size}" 'BEGIN { for (i=0; i<count; i++) printf "x" }')
  request="{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"echo\",\"arguments\":{\"text\":\"${text_value}\"},${meta}}}"
  for ((iteration=1; iteration<=iterations; iteration++)); do
    measure "echo-${payload_size}" echo "${request}" "${iteration}"
  done
done

sensor_request="{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"sensor.read_temperature\",\"arguments\":{},${meta}}}"
set_request="{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"servo.set_angle\",\"arguments\":{\"angle\":120},${meta}}}"
get_request="{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tools/call\",\"params\":{\"name\":\"servo.get_angle\",\"arguments\":{},${meta}}}"
for ((iteration=1; iteration<=iterations; iteration++)); do
  measure sensor.read_temperature sensor.read_temperature "${sensor_request}" "${iteration}"
  measure servo.set_angle servo.set_angle "${set_request}" "${iteration}"
  measure servo.get_angle servo.get_angle "${get_request}" "${iteration}"
done

request_count=$((iterations * 6))
if [[ "${transport_mode}" == all ]]; then request_count=$((request_count * 2)); fi
printf 'benchmark written to %s (%s requests; %s)\n' "${csv}" "${request_count}" "${transport_mode}"
