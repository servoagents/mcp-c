#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
device_host=${1:-}
proof_dir="${project_dir}/build-proof"
scratch="$(mktemp -d)"
servo_changed=false

case "${device_host}" in
  ''|*[!A-Za-z0-9.-]*)
    printf 'usage: %s DEVICE_IP_OR_HOSTNAME\n' "$0" >&2
    exit 2
    ;;
esac
endpoint="http://${device_host}:8080/mcp"
meta='"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}'

post() {
  local method=$1
  local name=$2
  local body=$3
  local output=$4
  local headers=(
    -H 'Content-Type: application/json'
    -H 'Accept: application/json, text/event-stream'
    -H 'MCP-Protocol-Version: 2026-07-28'
    -H "Mcp-Method: ${method}"
  )
  if [[ -n "${name}" ]]; then headers+=(-H "Mcp-Name: ${name}"); fi
  curl --fail-with-body --silent --show-error --max-time 10 "${endpoint}" \
    "${headers[@]}" --data-binary "${body}" >"${output}"
}

restore() {
  if ${servo_changed}; then
    body="{\"jsonrpc\":\"2.0\",\"id\":904,\"method\":\"tools/call\",\"params\":{\"name\":\"servo.set_angle\",\"arguments\":{\"angle\":90},${meta}}}"
    post tools/call servo.set_angle "${body}" "${scratch}/restore.json" || true
  fi
  rm -rf "${scratch}"
}
trap restore EXIT INT TERM

body="{\"jsonrpc\":\"2.0\",\"id\":901,\"method\":\"server/discover\",\"params\":{${meta}}}"
post server/discover '' "${body}" "${scratch}/discover.json"
grep -q '"supportedVersions":\["2026-07-28"\]' "${scratch}/discover.json"

body="{\"jsonrpc\":\"2.0\",\"id\":902,\"method\":\"tools/call\",\"params\":{\"name\":\"servo.set_angle\",\"arguments\":{\"angle\":110},${meta}}}"
post tools/call servo.set_angle "${body}" "${scratch}/set.json"
grep -q '"isError":false' "${scratch}/set.json"
grep -q '"value":110' "${scratch}/set.json"
servo_changed=true

body="{\"jsonrpc\":\"2.0\",\"id\":903,\"method\":\"tools/call\",\"params\":{\"name\":\"servo.get_angle\",\"arguments\":{},${meta}}}"
post tools/call servo.get_angle "${body}" "${scratch}/get.json"
grep -q '"isError":false' "${scratch}/get.json"
grep -q '"value":110' "${scratch}/get.json"

body="{\"jsonrpc\":\"2.0\",\"id\":904,\"method\":\"tools/call\",\"params\":{\"name\":\"servo.set_angle\",\"arguments\":{\"angle\":90},${meta}}}"
post tools/call servo.set_angle "${body}" "${scratch}/restore.json"
grep -q '"value":90' "${scratch}/restore.json"
servo_changed=false

mkdir -p "${proof_dir}"
firmware="${project_dir}/build-zephyr-esp32/zephyr/zephyr.bin"
firmware_sha=not-built
firmware_bytes=0
if [[ -f "${firmware}" ]]; then
  firmware_sha=$(sha256sum "${firmware}" | awk '{print $1}')
  firmware_bytes=$(wc -c <"${firmware}")
fi
{
  printf '{\n'
  printf '  "protocolVersion": "2026-07-28",\n'
  printf '  "device": "%s",\n' "${device_host}"
  printf '  "discovery": "pass",\n'
  printf '  "servoSetGetRoundTrip": "pass",\n'
  printf '  "servoRestoredDegrees": 90,\n'
  printf '  "firmware": {"bytes": %s, "sha256": "%s"}\n' \
    "${firmware_bytes}" "${firmware_sha}"
  printf '}\n'
} >"${proof_dir}/esp32-proof.json"

printf 'ESP32 proof: PASS (discovery, set/get 110 degrees; restoring 90)\n'
printf 'proof written to %s\n' "${proof_dir}/esp32-proof.json"
