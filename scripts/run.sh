#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cmake -S "${project_dir}" -B "${project_dir}/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "${project_dir}/build"
exec "${project_dir}/build/examples/linux/mcp_server/mcp-server" "$@"
