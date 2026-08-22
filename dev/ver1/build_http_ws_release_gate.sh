#!/bin/sh

set -eu

CC=${CC:-gcc}
PYTHON=${PYTHON:-}
FUZZ_ROUNDS=${FUZZ_ROUNDS:-1000}
STRESS_ROUNDS=${STRESS_ROUNDS:-500}
CFLAGS=${CFLAGS:-"-std=c11 -O2 -Wall -Wextra"}
STRICT_CFLAGS=${STRICT_CFLAGS:-"-std=c11 -O2 -Wall -Wextra -Werror"}

if [ -z "$PYTHON" ]; then
	if command -v python3 >/dev/null 2>&1; then
		PYTHON=python3
	else
		PYTHON=python
	fi
fi

mkdir -p release/gate

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		EXE=.exe
		LIBS="-lws2_32 -liphlpapi"
		;;
	Darwin)
		EXE=
		LIBS="-pthread"
		;;
	*)
		EXE=
		LIBS="-pthread -ldl -lm"
		;;
esac

TEST="release/gate/http_ws_test$EXE"
HTTP_FUZZ="release/gate/xhttp_fuzz_standalone$EXE"
WS_FUZZ="release/gate/xws_fuzz_standalone$EXE"
INTEROP="release/gate/xws_interop_peer$EXE"
STRESS="release/gate/xws_reconnect_stress$EXE"
SINGLE="release/gate/singlehead_http_ws$EXE"

$CC -m64 test.c -I . $CFLAGS $LIBS -o "$TEST"
for test_name in xurl_core http_semantics xnet_http xnet_httpd xweb xnet_ws; do
	"$TEST" "$test_name"
done

$CC -m64 -c xrt.c -I . $STRICT_CFLAGS -o release/gate/xrt_strict.o
$CC -m64 test/test_xhttp_oom.c -I . $CFLAGS $LIBS -o release/gate/xhttp_oom$EXE
release/gate/xhttp_oom$EXE

$CC -m64 test/fuzz_xhttp_protocol.c -I . $STRICT_CFLAGS \
	-DXRT_FUZZ_STANDALONE $LIBS -o "$HTTP_FUZZ"
"$HTTP_FUZZ" "$FUZZ_ROUNDS"

$CC -m64 test/fuzz_xws_protocol.c -I . $STRICT_CFLAGS \
	-DXRT_FUZZ_STANDALONE $LIBS -o "$WS_FUZZ"
"$WS_FUZZ" "$FUZZ_ROUNDS"

$CC -m64 test/interop_xws_peer.c xrt.c -I . $STRICT_CFLAGS $LIBS -o "$INTEROP"
"$PYTHON" test/run_xws_interop.py --peer "$INTEROP"

$CC -m64 test/stress_xws_reconnect.c xrt.c -I . $STRICT_CFLAGS $LIBS -o "$STRESS"
"$STRESS" "$STRESS_ROUNDS"

$CC -m64 singlehead/test_singlehead.c -I singlehead $CFLAGS $LIBS -o "$SINGLE"
"$SINGLE"

printf '\nHTTP/WebSocket release gate: PASS\n'
