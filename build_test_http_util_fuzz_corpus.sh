#!/bin/sh

set -e

CORPUS_DIR="dev/fuzz/http_util_corpus"

sh build_test_http_util_fuzz.sh

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT)
		EXE="release/x64/xrt_test_http_util_fuzz.exe"
		;;
	*)
		EXE="release/x64/xrt_test_http_util_fuzz"
		;;
esac

for seed in "$CORPUS_DIR"/*; do
	"$EXE" "$seed"
done

echo "HTTP util fuzz corpus run passed."
