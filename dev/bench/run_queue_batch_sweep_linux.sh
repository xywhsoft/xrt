#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

OUT_DIR="${1:-$ROOT/dev/bench/sweeps/queue_batch/linux}"
ITEMS_PER_PRODUCER="${XQUEUE_BATCH_SWEEP_ITEMS_PER_PRODUCER:-500000}"
CAPACITY="${XQUEUE_BATCH_SWEEP_CAPACITY:-4096}"
MPSC_PRODUCERS="${XQUEUE_BATCH_SWEEP_MPSC_PRODUCERS:-4}"
MPMC_PRODUCERS="${XQUEUE_BATCH_SWEEP_MPMC_PRODUCERS:-4}"
MPMC_CONSUMERS="${XQUEUE_BATCH_SWEEP_MPMC_CONSUMERS:-4}"
BATCH_SIZE_LIST_RAW="${XQUEUE_BATCH_SWEEP_BATCH_SIZE_LIST:-1 4 8 16 32 64}"
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

mkdir -p "$OUT_DIR"

read -r -a BATCH_SIZE_LIST <<< "$(printf '%s' "$BATCH_SIZE_LIST_RAW" | tr ',' ' ')"

printf '%s\n' "queue batch sweep (linux)"
printf '%s\n' "out_dir: ${OUT_DIR}"
printf '%s\n' "items_per_producer: ${ITEMS_PER_PRODUCER}"
printf '%s\n' "capacity: ${CAPACITY}"
printf '%s\n' "mpsc_producers: ${MPSC_PRODUCERS}"
printf '%s\n' "mpmc_producers: ${MPMC_PRODUCERS}"
printf '%s\n' "mpmc_consumers: ${MPMC_CONSUMERS}"
printf '%s\n' "batch_size_list: ${BATCH_SIZE_LIST[*]}"
printf '%s\n' "policy: ${POLICY_NAME}"
printf '%s\n' "cpu_pin: ${CPU_PIN}"

gcc dev/bench/queue/bench_queue_pointer.c xrt.c -O2 -I . -pthread -o "$BENCH_BIN"

for batch_size in "${BATCH_SIZE_LIST[@]}"; do
	stamp="$(date '+%Y%m%d_%H%M%S_%3N')"
	label="batch-${batch_size}"
	log="${OUT_DIR}/queue_batch_${label}_${stamp}.log"

	{
		printf '%s\n' "queue batch sweep matrix (linux)"
		printf '%s\n' "label: ${label}"
		printf '%s\n' "items_per_producer: ${ITEMS_PER_PRODUCER}"
		printf '%s\n' "capacity: ${CAPACITY}"
		printf '%s\n' "mpsc_producers: ${MPSC_PRODUCERS}"
		printf '%s\n' "mpmc_producers: ${MPMC_PRODUCERS}"
		printf '%s\n' "mpmc_consumers: ${MPMC_CONSUMERS}"
		printf '%s\n' "batch_size: ${batch_size}"
		printf '%s\n' "policy: ${POLICY_NAME}"
		printf '%s\n' "cpu_pin: ${CPU_PIN}"
		"$BENCH_BIN" \
			"${ITEMS_PER_PRODUCER}" \
			"${CAPACITY}" \
			"${MPSC_PRODUCERS}" \
			"${MPMC_PRODUCERS}" \
			"${MPMC_CONSUMERS}" \
			"${batch_size}"
	} | tee "$log"
done
