#!/bin/sh

set -eu

CC=${CC:-cc}
XRT_DIR=${XRT_DIR:-../xrt}
XRT_INCLUDE=${XRT_INCLUDE:-$XRT_DIR/single}
BUILD_DIR=${BUILD_DIR:-build}
RELEASE_DIR=${RELEASE_DIR:-release}
RUN_TESTS=${RUN_TESTS:-1}
CFLAGS=${CFLAGS:-"-D_GNU_SOURCE -std=c11 -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections"}
XRT_CFLAGS=${XRT_CFLAGS:-"$CFLAGS -Wno-pointer-sign -Wno-unused-function"}
LDFLAGS=${LDFLAGS:-"-Wl,--gc-sections"}
LIBS=${LIBS:-"-pthread -ldl -lm"}

mkdir -p "$BUILD_DIR" "$RELEASE_DIR"

$CC $CFLAGS -I. -I"$XRT_INCLUDE" -c xllm.c -o "$RELEASE_DIR/xllm.o"
$CC $CFLAGS -I. -I"$XRT_INCLUDE" -c xllm-session.c -o "$RELEASE_DIR/xllm-session.o"
$CC $CFLAGS -I. -I"$XRT_INCLUDE" -c xllm-memory.c -o "$RELEASE_DIR/xllm-memory.o"
$CC $XRT_CFLAGS -I. -I"$XRT_INCLUDE" -c xllm-xrt.c -o "$RELEASE_DIR/xllm-xrt.o"

$CC $XRT_CFLAGS $LDFLAGS -I. -I"$XRT_INCLUDE" tests/test_xllm.c "$RELEASE_DIR/xllm-xrt.o" $LIBS -o "$BUILD_DIR/test_xllm"
$CC $XRT_CFLAGS $LDFLAGS -I. -I"$XRT_INCLUDE" tests/test_xllm_session.c "$RELEASE_DIR/xllm-xrt.o" $LIBS -o "$BUILD_DIR/test_xllm_session"
$CC $XRT_CFLAGS $LDFLAGS -I. -I"$XRT_INCLUDE" tests/test_xllm_memory.c "$RELEASE_DIR/xllm-xrt.o" $LIBS -o "$BUILD_DIR/test_xllm_memory"

if [ "$RUN_TESTS" = "1" ]; then
    "$BUILD_DIR/test_xllm"
    "$BUILD_DIR/test_xllm_session"
    "$BUILD_DIR/test_xllm_memory"
fi

printf '\nxllm build: PASS\n'
