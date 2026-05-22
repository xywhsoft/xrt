#!/bin/sh

set -e

WARN_FLAGS="-Wall -Wextra"

mkdir -p release/x64

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		OUT="release/x64/xrt.dll"
		LIBS="-lws2_32 -liphlpapi"
		DEFINES="-DBUILD_DLL"
		LDFLAGS="-Wl,--gc-sections"
		STRIP_FLAGS="-s"
		;;
	Darwin)
		OUT="release/x64/xrt.dylib"
		LIBS=""
		DEFINES=""
		LDFLAGS=""
		STRIP_FLAGS=""
		;;
	*)
		OUT="release/x64/xrt.so"
		LIBS=""
		DEFINES=""
		LDFLAGS="-Wl,--gc-sections"
		STRIP_FLAGS="-s"
		;;
esac

gcc -shared -m64 xrt.c -O2 $STRIP_FLAGS -fPIC -ffunction-sections -fdata-sections $LDFLAGS $WARN_FLAGS $DEFINES $LIBS -o "$OUT"
