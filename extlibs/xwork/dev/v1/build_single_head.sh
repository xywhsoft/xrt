#!/bin/sh

set -e

cd "$(dirname "$0")"

mkdir -p singlehead

gcc -std=c11 -O2 -s singlehead/single_head_maker.c -o singlehead/single_head_maker
./singlehead/single_head_maker

echo
echo "Single header generated: singlehead/xwork.h"
