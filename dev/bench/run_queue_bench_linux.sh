#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

ITEMS_PER_PRODUCER="${XQUEUE_BENCH_ITEMS_PER_PRODUCER:-500000}"
CAPACITY="${XQUEUE_BENCH_CAPACITY:-4096}"
MPSC_PRODUCERS="${XQUEUE_BENCH_MPSC_PRODUCERS:-4}"
MPMC_PRODUCERS="${XQUEUE_BENCH_MPMC_PRODUCERS:-4}"
MPMC_CONSUMERS="${XQUEUE_BENCH_MPMC_CONSUMERS:-4}"
BATCH_SIZE="${XQUEUE_BENCH_BATCH_SIZE:-32}"
CPU_PIN="${XBENCH_PIN_CPU:-disabled}"
POLICY_NAME="baseline-unpinned"
if [ "$CPU_PIN" != "disabled" ]; then
	POLICY_NAME="diagnostic-process-pin"
fi
BENCH_BIN="dev/bench/queue/bench_queue_linux_${$}_$(date '+%Y%m%d_%H%M%S_%N')"

cleanup() {
	rm -f "$BENCH_BIN"
}
trap cleanup EXIT

printf '%s\n' "queue bench matrix (linux)"
printf '%s\n' "items_per_producer: ${ITEMS_PER_PRODUCER}"
printf '%s\n' "capacity: ${CAPACITY}"
printf '%s\n' "mpsc_producers: ${MPSC_PRODUCERS}"
printf '%s\n' "mpmc_producers: ${MPMC_PRODUCERS}"
printf '%s\n' "mpmc_consumers: ${MPMC_CONSUMERS}"
printf '%s\n' "batch_size: ${BATCH_SIZE}"
printf '%s\n' "policy: ${POLICY_NAME}"
printf '%s\n' "cpu_pin: ${CPU_PIN}"

gcc dev/bench/queue/bench_queue_pointer.c xrt.c -O2 -I . -pthread -o "$BENCH_BIN"

"$BENCH_BIN" \
	"${ITEMS_PER_PRODUCER}" \
	"${CAPACITY}" \
	"${MPSC_PRODUCERS}" \
	"${MPMC_PRODUCERS}" \
	"${MPMC_CONSUMERS}" \
	"${BATCH_SIZE}"
