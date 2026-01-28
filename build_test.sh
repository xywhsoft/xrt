#!/bin/sh

mkdir -p release/x64
gcc -m64 test.c xrt.c -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o release/x64/xrt_test
./release/x64/xrt_test
