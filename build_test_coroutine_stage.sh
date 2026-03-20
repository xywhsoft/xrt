#!/bin/sh

set -e

mkdir -p release/x64

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		EXE=".exe"
		LIBS="-lws2_32 -liphlpapi"
		;;
	*)
		EXE=""
		LIBS="-pthread"
		;;
esac

OUT="release/x64/xrt_test_coroutine_stage$EXE"

gcc -m64 test.c -I . $LIBS -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o "$OUT"
"$OUT" coroutine_min
"$OUT" coroutine
"$OUT" xnet2_sync
"$OUT" xnet2_listener_accept_core
