#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

CERT="dev/xnet2_tls_test_cert.pem"
KEY="dev/xnet2_tls_test_key.pem"
CFLAGS=(-O2 -I . -pthread)
TCP_ITER="${XNET_COMPARE_TCP_ITER:-500}"
TLS_ITER="${XNET_COMPARE_TLS_ITER:-100}"
UDP_PACKETS="${XNET_COMPARE_UDP_PACKETS:-8000}"
MESSAGE_SIZE="${XNET_COMPARE_MESSAGE_SIZE:-64}"
UDP_PACKET_SIZE="${XNET_COMPARE_UDP_PACKET_SIZE:-128}"
PRESSURE_MESSAGES="${XNET_COMPARE_PRESSURE_MESSAGES:-512}"
PRESSURE_MESSAGE_SIZE="${XNET_COMPARE_PRESSURE_MESSAGE_SIZE:-1024}"
PRESSURE_PAUSE_MS="${XNET_COMPARE_PRESSURE_PAUSE_MS:-250}"
PRESSURE_DRAIN_TIMEOUT_MS="${XNET_COMPARE_PRESSURE_DRAIN_TIMEOUT_MS:-120000}"
CPU_PIN="${XBENCH_PIN_CPU:-disabled}"
TASKSET_CPUS="${XNET_COMPARE_TASKSET_CPUS:-}"
POLICY_NAME="baseline-unpinned"
if [ -n "$TASKSET_CPUS" ]; then
	POLICY_NAME="diagnostic-taskset"
elif [ "$CPU_PIN" != "disabled" ]; then
	POLICY_NAME="diagnostic-process-pin"
fi

printf '%s\n' "xnet compare matrix (linux)"
printf '%s\n' "tcp: ${TCP_ITER} x ${MESSAGE_SIZE}B"
printf '%s\n' "tls: ${TLS_ITER} x ${MESSAGE_SIZE}B"
printf '%s\n' "udp: ${UDP_PACKETS} x ${UDP_PACKET_SIZE}B"
printf '%s\n' "pressure: messages=${PRESSURE_MESSAGES} size=${PRESSURE_MESSAGE_SIZE} pause_ms=${PRESSURE_PAUSE_MS} drain_timeout_ms=${PRESSURE_DRAIN_TIMEOUT_MS}"
printf '%s\n' "policy: ${POLICY_NAME}"
printf '%s\n' "cpu-pin: ${CPU_PIN}"
printf '%s\n' "taskset-cpus: ${TASKSET_CPUS:-disabled}"

run_bench() {
	local name="$1"
	shift
	if [ -n "$TASKSET_CPUS" ] && command -v taskset >/dev/null 2>&1; then
		if ! taskset -c "$TASKSET_CPUS" "$@"; then
			printf '%s\n' "benchmark_incomplete: ${name}"
		fi
	elif ! "$@"; then
		printf '%s\n' "benchmark_incomplete: ${name}"
	fi
}

gcc dev/bench/xnet_v1/bench_echo_tcp_v1.c xrt.c "${CFLAGS[@]}" -o dev/bench/xnet_v1/bench_echo_tcp_v1_cmp_linux
gcc dev/bench/xnet_v1/bench_echo_tls_v1.c xrt.c "${CFLAGS[@]}" -o dev/bench/xnet_v1/bench_echo_tls_v1_cmp_linux
gcc dev/bench/xnet_v1/bench_udp_burst_v1.c xrt.c "${CFLAGS[@]}" -o dev/bench/xnet_v1/bench_udp_burst_v1_cmp_linux
gcc dev/bench/xnet2/bench_echo_tcp.c xrt.c "${CFLAGS[@]}" -o dev/bench/xnet2/bench_echo_tcp_cmp_linux
gcc dev/bench/xnet2/bench_echo_tls.c xrt.c "${CFLAGS[@]}" -o dev/bench/xnet2/bench_echo_tls_cmp_linux
gcc dev/bench/xnet2/bench_udp_burst.c xrt.c "${CFLAGS[@]}" -o dev/bench/xnet2/bench_udp_burst_cmp_linux
gcc dev/bench/xnet2/bench_send_queue_pressure.c xrt.c "${CFLAGS[@]}" -o dev/bench/xnet2/bench_send_queue_pressure_cmp_linux

run_bench "xnet-v1 tcp" ./dev/bench/xnet_v1/bench_echo_tcp_v1_cmp_linux "$TCP_ITER" "$MESSAGE_SIZE" 19281
run_bench "xnet-v2 tcp" ./dev/bench/xnet2/bench_echo_tcp_cmp_linux "$TCP_ITER" "$MESSAGE_SIZE"
run_bench "xnet-v1 tls" timeout 20s ./dev/bench/xnet_v1/bench_echo_tls_v1_cmp_linux "$TLS_ITER" "$MESSAGE_SIZE" "$CERT" "$KEY" 19282
run_bench "xnet-v2 tls" ./dev/bench/xnet2/bench_echo_tls_cmp_linux "$TLS_ITER" "$MESSAGE_SIZE" "$CERT" "$KEY"
run_bench "xnet-v1 udp" ./dev/bench/xnet_v1/bench_udp_burst_v1_cmp_linux "$UDP_PACKETS" "$UDP_PACKET_SIZE" 19283
run_bench "xnet-v2 udp" ./dev/bench/xnet2/bench_udp_burst_cmp_linux "$UDP_PACKETS" "$UDP_PACKET_SIZE"
run_bench "xnet-v2 queue-pressure" timeout 180s ./dev/bench/xnet2/bench_send_queue_pressure_cmp_linux "$PRESSURE_MESSAGES" "$PRESSURE_MESSAGE_SIZE" "$PRESSURE_PAUSE_MS" "$PRESSURE_DRAIN_TIMEOUT_MS"
