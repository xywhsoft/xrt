#!/bin/sh

set -eu

CC=${CC:-cc}
XLLM_DIR=${XLLM_DIR:-../xllm}
XLLM_SESSION_DIR=${XLLM_SESSION_DIR:-../xllm-session}
XRT_DIR=${XRT_DIR:-../xrt}
XRT_INCLUDE=${XRT_INCLUDE:-$XRT_DIR/single}
BUILD_DIR=${BUILD_DIR:-build}
RELEASE_DIR=${RELEASE_DIR:-release}
RUN_TESTS=${RUN_TESTS:-1}
CFLAGS=${CFLAGS:-"-D_GNU_SOURCE -std=c11 -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections"}
XRT_CFLAGS=${XRT_CFLAGS:-"$CFLAGS -Wno-pointer-sign -Wno-unused-function"}
LDFLAGS=${LDFLAGS:-"-Wl,--gc-sections"}
LIBS=${LIBS:-"-pthread -ldl -lm"}

INCLUDES="-I. -I$XLLM_DIR -I$XLLM_SESSION_DIR -I$XRT_INCLUDE"

mkdir -p "$BUILD_DIR" "$RELEASE_DIR"

$CC $CFLAGS $INCLUDES -c xwork.c -o "$RELEASE_DIR/xwork.o"
$CC $XRT_CFLAGS $INCLUDES -c xwork-xrt.c -o "$RELEASE_DIR/xwork-xrt.o"
$CC $XRT_CFLAGS $LDFLAGS $INCLUDES \
    tests/test_xwork.c "$RELEASE_DIR/xwork-xrt.o" $LIBS -o "$BUILD_DIR/test_xwork"

if [ "$RUN_TESTS" = "1" ]; then
    "$BUILD_DIR/test_xwork"
fi

printf '\nxwork build: PASS\n'
