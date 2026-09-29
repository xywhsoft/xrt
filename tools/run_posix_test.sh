#!/usr/bin/env sh
set -eu

# WSL 的 Windows 挂载目录不保证 POSIX 权限语义；只切换测试 cwd，不改变测试内容。
: "${XRT_TEST_WORKDIR:?Set a test directory on a native POSIX filesystem}"
mkdir -p -- "$XRT_TEST_WORKDIR"
cd -- "$XRT_TEST_WORKDIR"
exec "$@"
