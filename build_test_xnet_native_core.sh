#!/bin/sh

set -e

mkdir -p release/x64

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		SRC="dev/test_xnet_windows_native_core.c"
		OUT="release/x64/xrt_test_xnet_windows_native_core.exe"
		LIBS="-lws2_32 -liphlpapi"
		ARGS="16 4"
		;;
	*)
		SRC="dev/test_xnet_linux_native_core.c"
		OUT="release/x64/xrt_test_xnet_linux_native_core"
		LIBS="-pthread"
		ARGS="16 4"
		;;
esac

gcc -m64 "$SRC" -I . $LIBS -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o "$OUT"
"$OUT" $ARGS
