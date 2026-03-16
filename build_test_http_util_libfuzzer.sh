#!/bin/sh

set -e

OUT="release/x64/xrt_test_http_util_libfuzzer"
CC_BIN="${CC:-clang}"
CFLAGS_FUZZ="-O1 -g -fno-omit-frame-pointer -fsanitize=fuzzer,address"

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		echo "HTTP util libFuzzer build is intended for Unix/Linux clang toolchains."
		exit 0
		;;
esac

if ! command -v "$CC_BIN" >/dev/null 2>&1; then
	echo "clang not found for libFuzzer build." >&2
	exit 1
fi

mkdir -p release/x64

"$CC_BIN" $CFLAGS_FUZZ -DXRT_FUZZ_LIBFUZZER_ONLY dev/fuzz_http_util_core.c xrt.c -I . -pthread -o "$OUT"

echo "HTTP util libFuzzer target built: $OUT"
