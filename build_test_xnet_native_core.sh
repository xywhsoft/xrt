#!/bin/sh

set -e

mkdir -p release/x64

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		OUT="release/x64/xrt_test_xnet_native_core.exe"
		LIBS="-lws2_32 -liphlpapi"
		;;
	*)
		OUT="release/x64/xrt_test_xnet_native_core"
		LIBS="-pthread"
		;;
esac

if [ "$#" -gt 0 ]; then
	ARGS="$*"
else
	ARGS="16 4"
fi

gcc -m64 test.c -I . $LIBS -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o "$OUT"
"$OUT" xnet_native_core $ARGS
