#!/bin/sh

set -e

mkdir -p release/x64

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		OUT="release/x64/xrt_test.exe"
		OUT_SINGLEHEAD="release/x64/test_singlehead_queue.exe"
		LIBS="-lws2_32 -liphlpapi"
		;;
	*)
		OUT="release/x64/xrt_test"
		mkdir -p release/linux
		OUT_SINGLEHEAD="release/linux/test_singlehead_queue_linux"
		LIBS="-pthread"
		;;
esac

gcc -m64 test.c -I . $LIBS -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o "$OUT"
"$OUT" preset:runtime_smoke
"$OUT" xurl_core
"$OUT" preset:xnet2_stage

gcc -m64 singlehead/test_singlehead.c -I singlehead $LIBS -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o "$OUT_SINGLEHEAD"
"$OUT_SINGLEHEAD"
