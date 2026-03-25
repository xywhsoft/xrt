#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

OUT_DIR="${1:-$ROOT/dev/bench/sweeps/queue/linux}"
ITEMS_PER_PRODUCER="${XQUEUE_SWEEP_ITEMS_PER_PRODUCER:-250000}"
FIXED_CAPACITY="${XQUEUE_SWEEP_FIXED_CAPACITY:-4096}"
DEFAULT_MPSC_PRODUCERS="${XQUEUE_SWEEP_DEFAULT_MPSC_PRODUCERS:-4}"
DEFAULT_MPMC_PRODUCERS="${XQUEUE_SWEEP_DEFAULT_MPMC_PRODUCERS:-4}"
DEFAULT_MPMC_CONSUMERS="${XQUEUE_SWEEP_DEFAULT_MPMC_CONSUMERS:-4}"
BATCH_SIZE="${XQUEUE_SWEEP_BATCH_SIZE:-32}"
CPU_PIN="${XBENCH_PIN_CPU:-disabled}"
POLICY_NAME="baseline-unpinned"
if [ "$CPU_PIN" != "disabled" ]; then
	POLICY_NAME="diagnostic-process-pin"
fi
CAPACITY_LIST_RAW="${XQUEUE_SWEEP_CAPACITY_LIST:-256 1024 4096 16384}"
MPSC_PRODUCER_LIST_RAW="${XQUEUE_SWEEP_MPSC_PRODUCER_LIST:-1 2 4 8}"
MPMC_PAIR_LIST_RAW="${XQUEUE_SWEEP_MPMC_PAIR_LIST:-1x1 2x2 4x4 8x8}"
BENCH_BIN="dev/bench/queue/bench_queue_linux_${$}_$(date '+%Y%m%d_%H%M%S_%N')"

cleanup() {
	rm -f "$BENCH_BIN"
}
trap cleanup EXIT

mkdir -p "$OUT_DIR"

read -r -a CAPACITY_LIST <<< "$(printf '%s' "$CAPACITY_LIST_RAW" | tr ',' ' ')"
read -r -a MPSC_PRODUCER_LIST <<< "$(printf '%s' "$MPSC_PRODUCER_LIST_RAW" | tr ',' ' ')"
read -r -a MPMC_PAIR_LIST <<< "$(printf '%s' "$MPMC_PAIR_LIST_RAW" | tr ',' ' ')"

printf '%s\n' "queue bench sweep (linux)"
printf '%s\n' "out_dir: ${OUT_DIR}"
printf '%s\n' "items_per_producer: ${ITEMS_PER_PRODUCER}"
printf '%s\n' "fixed_capacity: ${FIXED_CAPACITY}"
printf '%s\n' "batch_size: ${BATCH_SIZE}"
printf '%s\n' "policy: ${POLICY_NAME}"
printf '%s\n' "cpu_pin: ${CPU_PIN}"
printf '%s\n' "capacity_list: ${CAPACITY_LIST[*]}"
printf '%s\n' "mpsc_producer_list: ${MPSC_PRODUCER_LIST[*]}"
printf '%s\n' "mpmc_pair_list: ${MPMC_PAIR_LIST[*]}"

gcc dev/bench/queue/bench_queue_pointer.c xrt.c -O2 -I . -pthread -o "$BENCH_BIN"

run_case() {
	local family="$1"
	local label="$2"
	local capacity="$3"
	local mpsc_producers="$4"
	local mpmc_producers="$5"
	local mpmc_consumers="$6"
	local stamp
	local log

	stamp="$(date '+%Y%m%d_%H%M%S_%3N')"
	log="${OUT_DIR}/queue_${family}_${label}_${stamp}.log"

	{
		printf '%s\n' "queue sweep matrix (linux)"
		printf '%s\n' "family: ${family}"
		printf '%s\n' "label: ${label}"
		printf '%s\n' "items_per_producer: ${ITEMS_PER_PRODUCER}"
		printf '%s\n' "capacity: ${capacity}"
		printf '%s\n' "mpsc_producers: ${mpsc_producers}"
		printf '%s\n' "mpmc_producers: ${mpmc_producers}"
		printf '%s\n' "mpmc_consumers: ${mpmc_consumers}"
		printf '%s\n' "batch_size: ${BATCH_SIZE}"
		printf '%s\n' "policy: ${POLICY_NAME}"
		printf '%s\n' "cpu_pin: ${CPU_PIN}"
		"$BENCH_BIN" \
			"${ITEMS_PER_PRODUCER}" \
			"${capacity}" \
			"${mpsc_producers}" \
			"${mpmc_producers}" \
			"${mpmc_consumers}" \
			"${BATCH_SIZE}"
	} | tee "$log"
}

for capacity in "${CAPACITY_LIST[@]}"; do
	run_case "capacity" "cap-${capacity}" "$capacity" "$DEFAULT_MPSC_PRODUCERS" "$DEFAULT_MPMC_PRODUCERS" "$DEFAULT_MPMC_CONSUMERS"
done

for mpsc_producers in "${MPSC_PRODUCER_LIST[@]}"; do
	run_case "mpsc_producers" "mpsc-p${mpsc_producers}" "$FIXED_CAPACITY" "$mpsc_producers" "$DEFAULT_MPMC_PRODUCERS" "$DEFAULT_MPMC_CONSUMERS"
done

for pair in "${MPMC_PAIR_LIST[@]}"; do
	if [[ ! "$pair" =~ ^([0-9]+)[xX]([0-9]+)$ ]]; then
		printf '%s\n' "invalid MPMC pair token: $pair" >&2
		exit 1
	fi
	run_case "mpmc_pairs" "mpmc-${BASH_REMATCH[1]}x${BASH_REMATCH[2]}" "$FIXED_CAPACITY" "$DEFAULT_MPSC_PRODUCERS" "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}"
done
