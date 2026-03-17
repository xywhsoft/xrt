#!/bin/sh

set -e

mkdir -p release/x64

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		OUT="release/x64/xrt_test_xnet2_stage.exe"
		LIBS="-lws2_32 -liphlpapi"
		;;
	*)
		OUT="release/x64/xrt_test_xnet2_stage"
		LIBS="-pthread"
		;;
esac

gcc -m64 dev/test_xnet2_stage.c -I . $LIBS -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o "$OUT"
"$OUT"
