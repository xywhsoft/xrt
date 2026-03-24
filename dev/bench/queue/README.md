# Queue Bench

This directory contains lightweight throughput benchmarks for the queue work.

Current entrypoint:

- `bench_queue_pointer.c`
- `../QUEUE_BENCH_20260324.md`

Current scope:

- `SPSC` steady-state item throughput
- `MPSC` multi-producer item throughput
- `MPMC` multi-producer/multi-consumer item throughput

This lane is intentionally narrow for now:

- pointer queue only
- optimized builds
- throughput-oriented smoke and baseline checks

For Windows local runs, use:

- `dev/bench/run_queue_bench_windows.ps1`

For Linux local runs, use:

- `dev/bench/run_queue_bench_linux.sh`

Current baseline report:

- `dev/bench/QUEUE_BENCH_20260324.md`
