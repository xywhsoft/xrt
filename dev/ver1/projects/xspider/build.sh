#!/bin/sh

set -e

mkdir -p bin

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		gcc -m64 main.c -I ../../singlehead -lws2_32 -liphlpapi -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o bin/xspider.exe
		;;
	*)
		gcc -m64 main.c -I ../../singlehead -pthread -lm -O2 -s -ffunction-sections -fdata-sections -Wl,--gc-sections -o bin/xspider
		;;
esac

echo
echo "build.sh: PASS"
