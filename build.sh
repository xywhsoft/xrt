#!/bin/sh

mkdir -p release/x64
gcc -shared -m64 xrt.c -O2 -s -fPIC -ffunction-sections -fdata-sections -Wl,--gc-sections -o release/x64/xrt.so
