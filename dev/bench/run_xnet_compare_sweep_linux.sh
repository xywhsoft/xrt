#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

RUNS="${XNET_COMPARE_SWEEP_RUNS:-3}"
OUT_DIR="$ROOT/dev/bench/sweeps/linux"

mkdir -p "$OUT_DIR"

printf '%s\n' "xnet compare sweep (linux)"
printf '%s\n' "runs: ${RUNS}"
printf '%s\n' "out:  ${OUT_DIR}"

i=1
while [ "$i" -le "$RUNS" ]; do
	stamp="$(date +%Y%m%d_%H%M%S)"
	log="${OUT_DIR}/xnet_compare_run_${i}_${stamp}.log"
	printf '\n%s\n' "=== run ${i}/${RUNS} ==="
	bash ./dev/bench/run_xnet_compare_linux.sh | tee "$log"
	i=$((i + 1))
done

