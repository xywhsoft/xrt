#!/bin/sh

set -e

mkdir -p release/x64

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*)
		OUT="release/x64/xrt_test.exe"
		LIBS="-lws2_32 -liphlpapi"
		;;
	*)
		OUT="release/x64/xrt_test"
		LIBS=""
		;;
esac

gcc -m64 test.c xrt.c $LIBS -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o "$OUT"
"$OUT"
