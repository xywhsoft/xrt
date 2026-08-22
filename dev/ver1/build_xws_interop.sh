#!/bin/sh

set -eu

CC=${CC:-gcc}
CFLAGS=${CFLAGS:-"-std=c11 -O2 -Wall -Wextra -Werror"}
PYTHON=${PYTHON:-python3}

mkdir -p release/interop

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		OUT="release/interop/xws_interop_peer.exe"
		LIBS="-lws2_32 -liphlpapi"
		;;
	Darwin)
		OUT="release/interop/xws_interop_peer"
		LIBS="-pthread"
		;;
	*)
		OUT="release/interop/xws_interop_peer"
		LIBS="-pthread -ldl -lm"
		;;
esac

$CC -m64 test/interop_xws_peer.c xrt.c -I . $CFLAGS $LIBS -o "$OUT"
$PYTHON test/run_xws_interop.py --peer "$OUT"
