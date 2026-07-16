#!/bin/sh

set -eu

CC=${CC:-clang}
CFLAGS=${CFLAGS:-"-std=c11 -O1 -g -fno-omit-frame-pointer"}
SANITIZERS=${SANITIZERS:-"fuzzer,address,undefined"}
RUNS=${RUNS:-0}

mkdir -p release/fuzz

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		OUT="release/fuzz/xhttp_protocol_fuzz.exe"
		LIBS="-lws2_32 -liphlpapi"
		;;
	Darwin)
		OUT="release/fuzz/xhttp_protocol_fuzz"
		LIBS="-pthread"
		;;
	*)
		OUT="release/fuzz/xhttp_protocol_fuzz"
		LIBS="-pthread -ldl -lm"
		;;
esac

$CC -m64 test/fuzz_xhttp_protocol.c -I . $CFLAGS -fsanitize="$SANITIZERS" $LIBS -o "$OUT"

if [ "$RUNS" -gt 0 ]; then
	"$OUT" -runs="$RUNS" -max_len=65536
fi
