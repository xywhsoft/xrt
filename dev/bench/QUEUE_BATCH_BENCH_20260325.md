# XRT Queue Batch Throughput Compare (2026-03-25)

This report compares the current single-item queue hot path against the current
batch-aware fast path under the same default throughput matrix.

It exists to answer the practical question after the range-claim batch work:
does `PushBatch/PopBatch` now buy real throughput at the default matrix, or is
the gain still too small or too platform-specific to trust?


## Companion Sources

- `dev/bench/queue/bench_queue_pointer.c`
- `dev/bench/run_queue_bench_windows.ps1`
- `dev/bench/run_queue_bench_linux.sh`
- `dev/bench/XRT_FOUNDATION_BENCHMARK_METHOD.md`


## Environment Policy

- Windows local x64
  - policy: unpinned
  - build mode: optimized (`gcc -O2`)
- Debian 13 x64 remote
  - host: `192.168.0.20`
  - policy: unpinned
  - build mode: optimized (`gcc -O2`)


## Matrix

Run used for this report:

- `powershell -ExecutionPolicy Bypass -File dev/bench/run_queue_bench_windows.ps1`
- `bash dev/bench/run_queue_bench_linux.sh`

Collection policy:

- report values below were collected serially per host
- parallel runner execution is acceptable for smoke validation, but not for
  report-grade throughput numbers

Default matrix:

- items per producer: `500000`
- capacity: `4096`
- MPSC producers: `4`
- MPMC producers: `4`
- MPMC consumers: `4`
- batch size: `32`


## Results

Throughput in `M items/s`.

| Lane | Windows single | Windows batch | Windows batch/single | Linux single | Linux batch | Linux batch/single |
|---|---:|---:|---:|---:|---:|---:|
| `MPSC` | `6.22` | `33.29` | `5.35x` | `5.49` | `10.17` | `1.85x` |
| `MPMC` | `4.55` | `33.51` | `7.36x` | `4.55` | `14.44` | `3.17x` |

Raw reference:

- Windows
  - `mpsc_items_per_sec = 6220211.021`
  - `mpsc_batch_items_per_sec = 33292162.026`
  - `mpmc_items_per_sec = 4553038.459`
  - `mpmc_batch_items_per_sec = 33510267.546`
- Linux
  - `mpsc_items_per_sec = 5488458.646`
  - `mpsc_batch_items_per_sec = 10165839.301`
  - `mpmc_items_per_sec = 4554380.873`
  - `mpmc_batch_items_per_sec = 14436104.883`


## Interpretation

- The current batch API is no longer just a V1 contract layer; with the current
  range-claim implementation it is a real throughput fast path.
- `batch_size = 32` is now decisively positive on both hosts and on both
  contended lanes.
- Windows shows the strongest uplift in this matrix, especially on `MPMC`,
  where the batch path is roughly `7.4x` the single-item lane.
- Linux is more conservative but still clearly positive: roughly `1.9x` on
  `MPSC` and `3.2x` on `MPMC`.
- The cross-platform read is now straightforward: batch is no longer “maybe
  measurable”; it is materially faster under the default throughput matrix.
- What is still not frozen is batch-size guidance. The sweep data still matters
  because “batch helps” is now proven, but “which batch size is best” remains a
  matrix- and platform-sensitive question.


## Notes

- `SPSC` is not part of this compare because V1 does not expose a separate
  `SPSC` batch API.
- This report is still throughput-only; it does not include latency metrics.
- The current bench binary now emits both single-item and batch metrics, and
  the batch size is configurable through `XQUEUE_BENCH_BATCH_SIZE`.
