# Queue Bench

This directory contains the queue throughput, latency, and fairness gates for
the XRT 2 queue family. Both benchmarks build directly from `single/xrt.h` and
use the current queue, thread, atomic, wait, and time APIs.

Current entrypoint:

- `bench_queue_pointer.c`
- `bench_queue_latency.c`
- `../QUEUE_BENCH_20260728_XRT2.md`
- `../QUEUE_BENCH_20260324.md`
- `../QUEUE_BATCH_BENCH_20260325.md`
- `../QUEUE_BATCH_SWEEP_20260325.md`
- `../QUEUE_BENCH_SWEEP_20260325.md`
- `../QUEUE_LATENCY_20260325.md`
- `../QUEUE_LATENCY_SWEEP_20260325.md`
- `../QUEUE_LATENCY_PINNED_WINDOWS_20260325.md`

Current scope:

- `SPSC` steady-state item throughput
- `MPSC` multi-producer item throughput
- `MPMC` multi-producer/multi-consumer item throughput
- `MPSC` single-item vs batch compare
- `MPMC` single-item vs batch compare
- `MPSC` batch-size sweep under the default matrix
- `MPMC` batch-size sweep under the default matrix
- `MPSC` single-item vs batch latency compare
- `MPMC` single-item vs batch latency compare
- `MPMC` consumer fairness compare under the default matrix
- `MPSC` batch-size latency sweep under the default matrix
- `MPMC` batch-size latency/fairness sweep under the default matrix
- Windows diagnostic pinned latency sweep for scheduler-noise readback

This lane is intentionally narrow for now:

- pointer queue only
- optimized builds
- throughput-oriented baselines plus targeted latency/fairness checks

For Windows local runs, use:

- `dev/bench/run_queue_bench_windows.ps1`
- `dev/bench/run_queue_latency_windows.ps1`
- `dev/bench/run_queue_latency_pinned_windows.ps1`
- `dev/bench/run_queue_latency_sweep_windows.ps1`
- `dev/bench/run_queue_bench_sweep_windows.ps1`
- `dev/bench/run_queue_batch_sweep_windows.ps1`
- `dev/bench/summarize_queue_latency_sweep_windows.ps1`
- `dev/bench/summarize_queue_bench_sweep_windows.ps1`
- `dev/bench/summarize_queue_batch_sweep_windows.ps1`

For Linux local runs, use:

- `dev/bench/run_queue_bench_linux.sh`
- `dev/bench/run_queue_latency_linux.sh`
- `dev/bench/run_queue_latency_sweep_linux.sh`
- `dev/bench/run_queue_bench_sweep_linux.sh`
- `dev/bench/run_queue_batch_sweep_linux.sh`
- `dev/bench/summarize_queue_latency_sweep_linux.sh`
- `dev/bench/summarize_queue_bench_sweep_linux.sh`
- `dev/bench/summarize_queue_batch_sweep_linux.sh`

Runner notes:

- queue bench runners now build per-run temporary binaries so different bench
  scripts can execute concurrently without fighting over a shared output path
- sweep summaries keep only the latest sample for each matrix key
- sweep summary keys now include `policy` and `cpu_pin`, so diagnostic pinned
  runs do not overwrite baseline rows when they share a directory
- report-grade throughput compares should still be collected serially per host
- queue runners honor `XBENCH_PIN_CPU` for diagnostic process pinning
- pinned queue runs are useful for scheduler debugging, but they can severely
  distort `MPMC` fairness and should not be mixed into baseline summaries

Historical baseline reports:

- `dev/bench/QUEUE_BENCH_20260324.md`
- `dev/bench/QUEUE_BATCH_BENCH_20260325.md`
- `dev/bench/QUEUE_BATCH_SWEEP_20260325.md`
- `dev/bench/QUEUE_BENCH_SWEEP_20260325.md`
- `dev/bench/QUEUE_LATENCY_20260325.md`
- `dev/bench/QUEUE_LATENCY_SWEEP_20260325.md`
- `dev/bench/QUEUE_LATENCY_PINNED_WINDOWS_20260325.md`

These reports were produced by the legacy tree and remain comparison evidence.
Do not treat them as XRT 2 results. A new dated report must come from a
controlled serial run on a documented host and compiler.

Current XRT 2 baseline report:

- `dev/bench/QUEUE_BENCH_20260728_XRT2.md`

Build contract:

- C11 optimized build
- `-Wall -Wextra -Werror`
- no network headers or network link libraries
- Linux runners retain `-pthread` because the queue benchmarks create threads
- Linux latency runners retain `-lm` for statistical calculations
