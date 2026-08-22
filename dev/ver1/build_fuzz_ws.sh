#!/bin/sh

set -eu

CC=${CC:-clang}
CFLAGS=${CFLAGS:-"-std=c11 -O1 -g -fno-omit-frame-pointer"}
SANITIZERS=${SANITIZERS:-"fuzzer,address,undefined"}
RUNS=${RUNS:-0}
STANDALONE=${STANDALONE:-0}

mkdir -p release/fuzz

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		OUT="release/fuzz/xws_protocol_fuzz.exe"
		LIBS="-lws2_32 -liphlpapi"
		;;
	Darwin)
		OUT="release/fuzz/xws_protocol_fuzz"
		LIBS="-pthread"
		;;
	*)
		OUT="release/fuzz/xws_protocol_fuzz"
		LIBS="-pthread -ldl -lm"
		;;
esac

if [ "$STANDALONE" -eq 1 ]; then
	$CC -m64 test/fuzz_xws_protocol.c -I . $CFLAGS \
		-DXRT_FUZZ_STANDALONE -fsanitize="address,undefined" $LIBS -o "$OUT"
else
	$CC -m64 test/fuzz_xws_protocol.c -I . $CFLAGS \
		-fsanitize="$SANITIZERS" $LIBS -o "$OUT"
fi

if [ "$RUNS" -gt 0 ]; then
	if [ "$STANDALONE" -eq 1 ]; then
		"$OUT" "$RUNS"
	else
		"$OUT" -runs="$RUNS" -max_len=65536
	fi
fi
