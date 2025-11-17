#!/usr/bin/env bash
set -euo pipefail

# Build script for MCP Zephyr example
# Supports: ESP32, native_sim

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_TOP="${PROJECT_ROOT}/examples/zephyr"
APP_DIR="${PROJECT_ROOT}/examples/zephyr/mcp_server"
BOARD_TARGET=${BOARD_TARGET:-"esp32_devkitc/esp32/procpu"}

DO_INIT=false
DO_CLEAN=false
BUILD_DIR="build"
WIFI_SSID=""
WIFI_PASS=""

usage() {
  echo "Usage: $0 [OPTIONS]"
  echo ""
  echo "Options:"
  echo "  --init              Initialize/update Zephyr workspace"
  echo "  --clean             Clean build directory"
  echo "  -b <board>          Target board (default: esp32_devkitc/esp32/procpu)"
  echo "  --wifi-ssid <ssid>  WiFi SSID (for ESP32)"
  echo "  --wifi-pass <pass>  WiFi password (for ESP32)"
  echo ""
  echo "Examples:"
  echo "  $0 --init                                           # Initialize workspace"
  echo "  $0 -b native_sim                                    # Build for local testing"
  echo "  $0 -b esp32_devkitc/esp32/procpu --wifi-ssid MySSID --wifi-pass MyPass"
}

while [[ $# -gt 0 ]]; do
  case $1 in
    --init) DO_INIT=true; shift ;;
    --clean) DO_CLEAN=true; shift ;;
    --wifi-ssid) WIFI_SSID="$2"; shift 2 ;;
    --wifi-pass) WIFI_PASS="$2"; shift 2 ;;
    -b) BOARD_TARGET="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1"; usage; exit 1 ;;
  esac
done

# Activate local venv if present
if [ -d "${PROJECT_ROOT}/.venv" ]; then
  # shellcheck disable=SC1091
  source "${PROJECT_ROOT}/.venv/bin/activate"
fi

fetch_espressif_blobs() {
  echo "Fetching Espressif binary blobs (hal_espressif) if needed..."
  west blobs fetch hal_espressif 2>/dev/null || true
}

ensure_workspace() {
  # Idempotent init: if .west exists under WORKSPACE_TOP, skip init
  if [ -d "${WORKSPACE_TOP}/.west" ]; then
    echo "West workspace already present at ${WORKSPACE_TOP}"
  else
    if [ ! -f "${APP_DIR}/west.yml" ]; then
      echo "FATAL: no west.yml found at ${APP_DIR}" >&2
      exit 1
    fi
    echo "Initializing Zephyr workspace at ${WORKSPACE_TOP} using manifest ${APP_DIR}/west.yml ..."
    mkdir -p "${WORKSPACE_TOP}"
    ( cd "${WORKSPACE_TOP}" && west init -l "${APP_DIR}" )
  fi
  ( cd "${WORKSPACE_TOP}" && west update && fetch_espressif_blobs && west zephyr-export )
}

if $DO_INIT; then
  ensure_workspace
  echo "Init/update complete."
  exit 0
fi

# Ensure workspace exists before building
ensure_workspace

if $DO_CLEAN; then
  rm -rf "${APP_DIR}/${BUILD_DIR}"
fi

# Remap deprecated board id to the official one
if [[ "${BOARD_TARGET}" == "esp32_devkitc_wroom/esp32/procpu" ]]; then
  echo "Note: remapping deprecated board '${BOARD_TARGET}' to 'esp32_devkitc/esp32/procpu'"
  BOARD_TARGET="esp32_devkitc/esp32/procpu"
fi

echo "Building MCP Zephyr app for board: ${BOARD_TARGET}"
(
  cd "${WORKSPACE_TOP}"
  WIFI_SSID="${WIFI_SSID}" WIFI_PASS="${WIFI_PASS}" \
    west build -p auto -b "${BOARD_TARGET}" -d "${APP_DIR}/${BUILD_DIR}" "${APP_DIR}"
)

echo ""
echo "Build complete!"
if [[ "${BOARD_TARGET}" == "native_sim" ]]; then
  echo "Run: cd ${APP_DIR} && ./build/zephyr/zephyr.exe"
  echo "Test: curl -s http://localhost:8080 -H 'Content-Type: application/json' -d '{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"initialize\",\"params\":{}}'"
else
  echo "Flash: cd ${APP_DIR} && west flash"
  echo "Monitor: cd ${APP_DIR} && west espressif monitor"
fi
