#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

TCC_BIN="${TCC:-tcc}"
TCC_LIBDIR="${TCC_LIBDIR:-}"
OUT="release/linux/test_coroutine_tcc_linux"
LOG="${OUT}.log"
CFLAGS=(
	-I .
	-O2
	-DXRT_NO_NETWORK
	-DXRT_NO_NETTLS
)
LIBS=(
	-pthread
	-lm
	-ldl
)
TCC_FLAGS=()

if [[ -n "$TCC_LIBDIR" ]]; then
	TCC_FLAGS+=("-B$TCC_LIBDIR")
fi

mkdir -p release/linux

"$TCC_BIN" "${TCC_FLAGS[@]}" dev/test_coroutine_core.c xrt.c "${CFLAGS[@]}" "${LIBS[@]}" -o "$OUT"
"$OUT" | tee "$LOG"

if grep -q "FAIL" "$LOG"; then
	echo "coroutine log contains FAIL" >&2
	exit 2
fi
