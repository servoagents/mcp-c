#!/usr/bin/env bash
set -euo pipefail

interface=${1:-}
profile=${2:-show}

if [[ -z "${interface}" || ! -d "/sys/class/net/${interface}" ]]; then
  printf 'usage: sudo %s INTERFACE {show|clear|loss1|loss5|loss10|delay20|delay100|delay300|jitter|disconnect}\n' "$0" >&2
  exit 2
fi

case "${profile}" in
  show) tc qdisc show dev "${interface}" ;;
  clear) tc qdisc del dev "${interface}" root 2>/dev/null || true ;;
  loss1) tc qdisc replace dev "${interface}" root netem loss 1% ;;
  loss5) tc qdisc replace dev "${interface}" root netem loss 5% ;;
  loss10) tc qdisc replace dev "${interface}" root netem loss 10% ;;
  delay20) tc qdisc replace dev "${interface}" root netem delay 20ms ;;
  delay100) tc qdisc replace dev "${interface}" root netem delay 100ms ;;
  delay300) tc qdisc replace dev "${interface}" root netem delay 300ms ;;
  jitter) tc qdisc replace dev "${interface}" root netem delay 100ms 25ms distribution normal ;;
  disconnect) tc qdisc replace dev "${interface}" root netem loss 100% ;;
  *) printf 'unknown profile: %s\n' "${profile}" >&2; exit 2 ;;
esac
