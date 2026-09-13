#!/bin/sh

set -eu

CC=${CC:-cc}
XLLM_DIR=${XLLM_DIR:-../xllm}
XRT_DIR=${XRT_DIR:-../..}
XRT_INCLUDE=${XRT_INCLUDE:-$XRT_DIR/single}
BUILD_DIR=${BUILD_DIR:-build}
RELEASE_DIR=${RELEASE_DIR:-release}
RUN_TESTS=${RUN_TESTS:-1}
CFLAGS=${CFLAGS:-"-D_GNU_SOURCE -std=c11 -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections"}
XRT_CFLAGS=${XRT_CFLAGS:-"$CFLAGS -Wno-pointer-sign -Wno-unused-function"}
LDFLAGS=${LDFLAGS:-"-Wl,--gc-sections"}
LIBS=${LIBS:-"-pthread -ldl -lm"}

mkdir -p "$BUILD_DIR" "$RELEASE_DIR"

$CC $CFLAGS -I. -I"$XLLM_DIR" -I"$XRT_INCLUDE" -c xllm-session.c -o "$RELEASE_DIR/xllm-session.o"
$CC $XRT_CFLAGS -I. -I"$XRT_INCLUDE" -c xllm-session-xrt.c -o "$RELEASE_DIR/xllm-session-xrt.o"

$CC $XRT_CFLAGS $LDFLAGS -I. -I"$XLLM_DIR" -I"$XRT_INCLUDE" \
    tests/test_xllm_session.c "$RELEASE_DIR/xllm-session-xrt.o" $LIBS -o "$BUILD_DIR/test_xllm_session"

if [ "$RUN_TESTS" = "1" ]; then
    "$BUILD_DIR/test_xllm_session"
fi

printf '\nxllm-session build: PASS\n'
