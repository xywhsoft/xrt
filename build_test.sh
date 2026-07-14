#!/bin/sh

set -e

WARN_FLAGS="-Wall -Wextra"

mkdir -p release/x64

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		OUT="release/x64/xrt_test.exe"
		OUT_SINGLEHEAD="release/x64/test_singlehead_queue.exe"
		OUT_TYPED="release/x64/test_typed_special.exe"
		LIBS="-lws2_32 -liphlpapi"
		LDFLAGS="-Wl,--gc-sections"
		STRIP_FLAGS="-s"
		;;
	Darwin)
		OUT="release/x64/xrt_test"
		OUT_SINGLEHEAD="release/x64/test_singlehead_queue"
		OUT_TYPED="release/x64/test_typed_special"
		LIBS="-pthread"
		LDFLAGS=""
		STRIP_FLAGS=""
		;;
	*)
		OUT="release/x64/xrt_test"
		mkdir -p release/linux
		OUT_SINGLEHEAD="release/linux/test_singlehead_queue_linux"
		OUT_TYPED="release/linux/test_typed_special"
		LIBS="-pthread"
		LDFLAGS="-Wl,--gc-sections"
		STRIP_FLAGS="-s"
		;;
esac

gcc -m64 test.c -I . $LIBS -O2 $STRIP_FLAGS -ffunction-sections -fdata-sections $LDFLAGS $WARN_FLAGS -o "$OUT"
"$OUT" preset:runtime_smoke
"$OUT" xurl_core
"$OUT" preset:xnet2_stage

gcc -m64 test/test_typed_special.c xrt.c -I . $LIBS -O2 $STRIP_FLAGS -ffunction-sections -fdata-sections $LDFLAGS $WARN_FLAGS -o "$OUT_TYPED"
"$OUT_TYPED"

gcc -m64 singlehead/test_singlehead.c -I singlehead $LIBS -O2 $STRIP_FLAGS -ffunction-sections -fdata-sections $LDFLAGS $WARN_FLAGS -o "$OUT_SINGLEHEAD"
"$OUT_SINGLEHEAD"
