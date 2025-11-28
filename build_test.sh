#!/bin/sh

gcc -m64 test.c xrt.c -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o xrt_test
./xrt_test
