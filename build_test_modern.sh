#!/bin/sh

set -e

sh ./build_test_coroutine_stage.sh
sh ./build_test_xnet_native_core.sh
sh ./build_test_xnet2_stage.sh
