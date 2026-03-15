#!/bin/sh

set -e

mkdir -p release/x64

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		EXE=".exe"
		LIBS="-lws2_32 -liphlpapi"
		;;
	*)
		EXE=""
		LIBS="-pthread"
		;;
esac

gcc -m64 dev/test_coroutine_min.c xrt.c -I . $LIBS -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o "release/x64/xrt_test_coroutine_min$EXE"
"./release/x64/xrt_test_coroutine_min$EXE"

gcc -m64 dev/test_coroutine_core.c xrt.c -I . $LIBS -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o "release/x64/xrt_test_coroutine$EXE"
"./release/x64/xrt_test_coroutine$EXE"

gcc -m64 dev/test_xnet2_sync_core.c xrt.c -I . $LIBS -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o "release/x64/xrt_test_xnet2_sync_core$EXE"
"./release/x64/xrt_test_xnet2_sync_core$EXE"

gcc -m64 dev/test_xnet2_listener_accept_core.c xrt.c -I . $LIBS -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o "release/x64/xrt_test_listener_accept_core$EXE"
"./release/x64/xrt_test_listener_accept_core$EXE"
