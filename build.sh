#!/bin/sh

gcc -shared -m64 xrt.c -O2 -s -fPIC -ffunction-sections -fdata-sections -Wl,--gc-sections -o xrt.so
