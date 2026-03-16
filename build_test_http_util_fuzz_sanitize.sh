#!/bin/sh

set -e

CORPUS_DIR="dev/fuzz/http_util_corpus"
OUT="release/x64/xrt_test_http_util_fuzz_asan"
CC_BIN="${CC:-}"
CFLAGS_SAN="-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined"

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		echo "HTTP util sanitizer fuzz runner is intended for Unix/Linux toolchains."
		exit 0
		;;
esac

if [ -z "$CC_BIN" ]; then
	if command -v clang >/dev/null 2>&1; then
		CC_BIN=clang
	elif command -v gcc >/dev/null 2>&1; then
		CC_BIN=gcc
	else
		echo "No suitable compiler found (clang/gcc)." >&2
		exit 1
	fi
fi

mkdir -p release/x64

ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=1:halt_on_error=1:allocator_may_return_null=1}"
UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}"
export ASAN_OPTIONS
export UBSAN_OPTIONS

"$CC_BIN" $CFLAGS_SAN dev/fuzz_http_util_core.c xrt.c -I . -pthread -o "$OUT"

for seed in "$CORPUS_DIR"/*; do
	"$OUT" "$seed"
done

echo "HTTP util sanitizer fuzz corpus run passed."
