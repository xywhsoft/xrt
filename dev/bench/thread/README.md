# Thread Bench

This directory contains focused benchmarks for the finalized XRT thread/runtime
layer.

Initial benchmark set:

- `bench_thread_create_join.c`
- `bench_thread_wait_primitives.c`

These benchmarks are intended to validate:

- thread creation and join throughput
- semaphore/condition wake-and-wait throughput
- regression behavior after runtime semantic changes

