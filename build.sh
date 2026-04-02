#!/bin/sh

set -e

WARN_FLAGS="-Wall -Wextra"

mkdir -p release/x64

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		OUT="release/x64/xrt.dll"
		LIBS="-lws2_32 -liphlpapi"
		DEFINES="-DBUILD_DLL"
		;;
	*)
		OUT="release/x64/xrt.so"
		LIBS=""
		DEFINES=""
		;;
esac

gcc -shared -m64 xrt.c -O2 -s -fPIC -ffunction-sections -fdata-sections -Wl,--gc-sections $WARN_FLAGS $DEFINES $LIBS -o "$OUT"
