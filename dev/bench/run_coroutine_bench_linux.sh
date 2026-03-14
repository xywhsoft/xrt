#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

CFLAGS=(-O2 -I . -pthread)

gcc dev/bench/coroutine/bench_context_switch.c xrt.c "${CFLAGS[@]}" -o dev/bench/coroutine/bench_context_switch_linux
gcc dev/bench/coroutine/bench_create_destroy.c xrt.c "${CFLAGS[@]}" -o dev/bench/coroutine/bench_create_destroy_linux
gcc dev/bench/coroutine/bench_timer_churn.c xrt.c "${CFLAGS[@]}" -o dev/bench/coroutine/bench_timer_churn_linux
gcc dev/bench/coroutine/bench_sched_post.c xrt.c "${CFLAGS[@]}" -o dev/bench/coroutine/bench_sched_post_linux

./dev/bench/coroutine/bench_context_switch_linux 100000
./dev/bench/coroutine/bench_create_destroy_linux 50000
./dev/bench/coroutine/bench_timer_churn_linux 10000
./dev/bench/coroutine/bench_sched_post_linux 10000
