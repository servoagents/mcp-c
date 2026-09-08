#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
workspace="${project_dir}/examples/zephyr"
app_dir="${workspace}/mcp_server"
build_dir="${project_dir}/build-zephyr"
board="esp32_devkitc/esp32/procpu"
device="/dev/ttyUSB0"
do_init=false
do_flash=false
do_monitor=false
wifi_ssid="${WIFI_SSID:-}"
wifi_pass="${WIFI_PASS:-}"

usage() {
  printf '%s\n' \
    "Usage: $0 [--init] [--flash] [--monitor] [-b BOARD] [--device PORT]" \
    "          [--wifi-ssid SSID] [--wifi-pass PASSWORD]" \
    "" \
    "One-command ESP32 build + flash:" \
    "  WIFI_SSID=... WIFI_PASS=... $0 --flash"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --init) do_init=true; shift ;;
    --flash) do_flash=true; shift ;;
    --monitor) do_monitor=true; shift ;;
    --wifi-ssid) wifi_ssid=$2; shift 2 ;;
    --wifi-pass) wifi_pass=$2; shift 2 ;;
    --device) device=$2; shift 2 ;;
    -b) board=$2; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) usage; exit 2 ;;
  esac
done

if [[ ! -x "${project_dir}/.venv/bin/west" ]]; then
  python3 -m venv "${project_dir}/.venv"
  "${project_dir}/.venv/bin/pip" install -r "${project_dir}/requirements.txt"
fi
export PATH="${project_dir}/.venv/bin:${PATH}"

if [[ ! -d "${workspace}/.west" ]]; then
  west init -l "${app_dir}" "${workspace}"
  do_init=true
fi
if ${do_init}; then
  (cd "${workspace}" && west update && west zephyr-export &&
    west packages pip --install && west blobs fetch hal_espressif)
fi

if [[ "${board}" == native_sim* ]]; then
  build_dir="${project_dir}/build-zephyr-native"
else
  if [[ -z "${wifi_ssid}" || -z "${wifi_pass}" ]]; then
    printf 'WIFI_SSID and WIFI_PASS are required for an ESP32 build.\n' >&2
    exit 2
  fi
fi

(cd "${workspace}" && WIFI_SSID="${wifi_ssid}" WIFI_PASS="${wifi_pass}" \
  west build -p auto -b "${board}" -d "${build_dir}" "${app_dir}")

if ${do_flash}; then
  (cd "${workspace}" && west flash -d "${build_dir}" --esp-device "${device}")
fi
if ${do_monitor}; then
  exec python -m serial.tools.miniterm "${device}" 115200 --raw
fi

printf 'Zephyr build: %s\n' "${build_dir}/zephyr/zephyr.elf"
