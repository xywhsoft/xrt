#!/bin/sh

set -e

rm -f \
	release/x64/test*.exe \
	release/x64/xrt_test* \
	release/x64/test/out_*.txt \
	release/x64/test/test_ptr.txt \
	release/x64/test/test_put.txt \
	release/x64/test/test_putall.txt \
	release/x64/test/test_copy.txt \
	release/x64/test/test_move.txt \
	release/x64/test/test_size.txt \
	release/x64/test/test_write_u16.txt \
	release/x64/test/test_write_u16be.txt \
	release/x64/test/test_write_u32.txt
