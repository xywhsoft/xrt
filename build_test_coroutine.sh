#!/bin/sh

set -e

mkdir -p release/x64

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*)
		OUT="release/x64/xrt_test_coroutine.exe"
		LIBS="-lws2_32 -liphlpapi"
		;;
	*)
		OUT="release/x64/xrt_test_coroutine"
		LIBS="-pthread"
		;;
esac

gcc -m64 dev/test_coroutine_core.c xrt.c -I . $LIBS -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o "$OUT"
"$OUT"
