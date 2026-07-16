#!/bin/sh

set -eu

CC=${CC:-gcc}
CFLAGS=${CFLAGS:-"-D_GNU_SOURCE -std=c11 -Wall -Wextra -O2 -ffunction-sections -fdata-sections"}
RUN_TESTS=${RUN_TESTS:-1}
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
		OUT="release/x64/xrt_test.exe"
		OUT_SINGLEHEAD="release/x64/test_singlehead_queue.exe"
		OUT_TYPED="release/x64/test_typed_special.exe"
		OUT_STREAM="release/x64/test_stream.exe"
		OUT_XHTTP_OOM="release/x64/test_xhttp_oom.exe"
		LIBS="-lws2_32 -liphlpapi"
		LDFLAGS="-Wl,--gc-sections"
		STRIP_FLAGS="-s"
		;;
	macos)
		OUT="release/x64/xrt_test"
		OUT_SINGLEHEAD="release/x64/test_singlehead_queue"
		OUT_TYPED="release/x64/test_typed_special"
		OUT_STREAM="release/x64/test_stream"
		OUT_XHTTP_OOM="release/x64/test_xhttp_oom"
		LIBS="-pthread"
		LDFLAGS=""
		STRIP_FLAGS=""
		;;
	linux)
		OUT="release/x64/xrt_test"
		mkdir -p release/linux
		OUT_SINGLEHEAD="release/linux/test_singlehead_queue_linux"
		OUT_TYPED="release/linux/test_typed_special"
		OUT_STREAM="release/linux/test_stream"
		OUT_XHTTP_OOM="release/linux/test_xhttp_oom"
		LIBS="-pthread -ldl -lm"
		LDFLAGS="-Wl,--gc-sections"
		STRIP_FLAGS="-s"
		;;
	*)
		echo "unsupported TARGET_OS: $TARGET_OS" >&2
		exit 2
		;;
esac

$CC -m64 test.c -I . $CFLAGS $STRIP_FLAGS $LDFLAGS $LIBS -o "$OUT"
if [ "$RUN_TESTS" = "1" ]; then
	"$OUT" preset:runtime_smoke
	"$OUT" xurl_core
	"$OUT" http_semantics
	"$OUT" preset:xnet2_stage
fi

$CC -m64 test/test_typed_special.c xrt.c -I . $CFLAGS $STRIP_FLAGS $LDFLAGS $LIBS -o "$OUT_TYPED"
if [ "$RUN_TESTS" = "1" ]; then "$OUT_TYPED"; fi

$CC -m64 test/test_stream.c xrt.c -I . $CFLAGS $STRIP_FLAGS $LDFLAGS $LIBS -o "$OUT_STREAM"
if [ "$RUN_TESTS" = "1" ]; then "$OUT_STREAM"; fi

$CC -m64 test/test_xhttp_oom.c -I . $CFLAGS $STRIP_FLAGS $LDFLAGS $LIBS -o "$OUT_XHTTP_OOM"
if [ "$RUN_TESTS" = "1" ]; then "$OUT_XHTTP_OOM"; fi

$CC -m64 singlehead/test_singlehead.c -I singlehead $CFLAGS $STRIP_FLAGS $LDFLAGS $LIBS -o "$OUT_SINGLEHEAD"
if [ "$RUN_TESTS" = "1" ]; then "$OUT_SINGLEHEAD"; fi
