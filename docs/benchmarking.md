---
layout: default
title: Benchmarks and impairment
nav_order: 4
---

# Benchmarks and network impairment

`make benchmark` runs bounded `echo` payloads through HTTP and, when libcoap is
available, through confirmable CoAP. It records transport, operation, payload
and response bytes, end-to-end latency, and host binary size in
`build-benchmark/results.csv`. The default is 25 repetitions per payload;
`./scripts/benchmark.sh 100` changes the sample count.

The MQTT binding currently has deterministic semantic/replay tests rather than
wire measurements because no broker adapter is included yet. Add MQTT rows only
after a real MQTT 5 adapter and broker are in the measurement path.

For manual loss and latency experiments, use `scripts/netem.sh` on a dedicated
test interface. The script supports 1%, 5%, and 10% loss; 20, 100, and 300 ms
delay; delay with jitter; complete temporary disconnection; inspection; and
cleanup. It deliberately requires an explicit interface and root privileges.
Do not run impairment against a production or remote-management interface.

Examples:

```bash
sudo ./scripts/netem.sh eth0 loss5
make benchmark
sudo ./scripts/netem.sh eth0 clear
```

TLS/DTLS and PQC measurements belong below this harness. Future profiles should
add handshake time/bytes, packets, fragmentation, peak RAM/stack, and code-size
deltas while holding MCP messages and application operations constant.
