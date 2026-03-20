#!/bin/sh

set -e

mkdir -p release/x64

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		OUT="release/x64/xrt_test.exe"
		LIBS="-lws2_32 -liphlpapi"
		;;
	*)
		OUT="release/x64/xrt_test"
		LIBS="-pthread"
		;;
esac

gcc -m64 test.c -I . $LIBS -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o "$OUT"
"$OUT" coroutine_min
"$OUT" coroutine
"$OUT" xurl_core
"$OUT" preset:xnet2_stage
