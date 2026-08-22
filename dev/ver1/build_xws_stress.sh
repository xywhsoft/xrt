#!/bin/sh

set -eu

CC=${CC:-gcc}
CFLAGS=${CFLAGS:-"-std=c11 -O2 -Wall -Wextra -Werror"}
ROUNDS=${ROUNDS:-500}

mkdir -p release/stress

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		OUT="release/stress/xws_reconnect_stress.exe"
		LIBS="-lws2_32 -liphlpapi"
		;;
	Darwin)
		OUT="release/stress/xws_reconnect_stress"
		LIBS="-pthread"
		;;
	*)
		OUT="release/stress/xws_reconnect_stress"
		LIBS="-pthread -ldl -lm"
		;;
esac

$CC -m64 test/stress_xws_reconnect.c xrt.c -I . $CFLAGS $LIBS -o "$OUT"
"$OUT" "$ROUNDS"
