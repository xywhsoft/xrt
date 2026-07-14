#!/bin/sh

set -eu

CC=${CC:-gcc}
CFLAGS=${CFLAGS:-"-D_GNU_SOURCE -std=c11 -Wall -Wextra -O2 -fPIC -ffunction-sections -fdata-sections"}
TARGET_OS=${TARGET_OS:-}

if [ -z "$TARGET_OS" ]; then
	case "$(uname -s)" in
		MINGW*|MSYS*|CYGWIN*|Windows_NT) TARGET_OS=windows ;;
		Darwin) TARGET_OS=macos ;;
		*) TARGET_OS=linux ;;
	esac
fi

mkdir -p release/x64

case "$TARGET_OS" in
	windows)
		OUT="release/x64/xrt.dll"
		LIBS="-lws2_32 -liphlpapi"
		DEFINES="-DBUILD_DLL"
		LDFLAGS="-Wl,--gc-sections"
		STRIP_FLAGS="-s"
		;;
	macos)
		OUT="release/x64/xrt.dylib"
		LIBS=""
		DEFINES=""
		LDFLAGS=""
		STRIP_FLAGS=""
		;;
	linux)
		OUT="release/x64/xrt.so"
		LIBS="-pthread -ldl -lm"
		DEFINES=""
		LDFLAGS="-Wl,--gc-sections"
		STRIP_FLAGS="-s"
		;;
	*)
		echo "unsupported TARGET_OS: $TARGET_OS" >&2
		exit 2
		;;
esac

$CC -shared -m64 xrt.c $CFLAGS $STRIP_FLAGS $LDFLAGS $DEFINES $LIBS -o "$OUT"
