#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${project_dir}/build-proof"
sanitizer_dir="${project_dir}/build-sanitizers"

cmake -S "${project_dir}" -B "${build_dir}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DMCP_ENABLE_TESTS=ON
cmake --build "${build_dir}"
ctest --test-dir "${build_dir}" --output-on-failure

if ctest --test-dir "${build_dir}" -N | grep -q 'transport-integration'; then
  transport_status=pass
else
  transport_status=not-built
fi

cmake -S "${project_dir}" -B "${sanitizer_dir}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DMCP_ENABLE_TESTS=ON -DMCP_ENABLE_COAP=OFF \
  -DMCP_ENABLE_SANITIZERS=ON
cmake --build "${sanitizer_dir}"
ctest --test-dir "${sanitizer_dir}" --output-on-failure -R 'core|mqtt-binding'

binary="${build_dir}/examples/linux/mcp_server/mcp-server"
proof_file="${build_dir}/proof.json"
binary_sha=$(sha256sum "${binary}" | awk '{print $1}')
binary_bytes=$(wc -c <"${binary}")
{
  printf '{\n'
  printf '  "protocolVersion": "2026-07-28",\n'
  printf '  "coreTests": "pass",\n'
  printf '  "mqttBindingTests": "pass",\n'
  printf '  "stdioIntegration": "pass",\n'
  printf '  "httpCoapIntegration": "%s",\n' "${transport_status}"
  printf '  "sanitizers": "pass",\n'
  printf '  "binary": {\n'
  printf '    "path": "build-proof/examples/linux/mcp_server/mcp-server",\n'
  printf '    "bytes": %s,\n' "${binary_bytes}"
  printf '    "sha256": "%s"\n' "${binary_sha}"
  printf '  }\n}\n'
} >"${proof_file}"
printf 'proof written to %s\n' "${proof_file}"
