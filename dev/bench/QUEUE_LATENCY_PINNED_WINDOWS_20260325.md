# XRT Queue Latency Pinned Follow-Up (Windows, 2026-03-25)

This report is a diagnostic follow-up to the Windows side of the queue latency
batch-size sweep.

It exists to answer one narrow question from the earlier unpinned pass: how
much of the strange Windows tail-latency shape is queue behavior, and how much
is host scheduling noise?


## Companion Sources

- `dev/bench/queue/bench_queue_latency.c`
- `dev/bench/run_queue_latency_pinned_windows.ps1`
- `dev/bench/run_queue_latency_sweep_windows.ps1`
- `dev/bench/summarize_queue_latency_sweep_windows.ps1`
- `dev/bench/sweeps/queue_latency/windows_pinned_cpu0/SUMMARY_20260325.md`
- `dev/bench/QUEUE_LATENCY_SWEEP_20260325.md`
- `dev/bench/XRT_FOUNDATION_BENCHMARK_METHOD.md`


## Environment Policy

- Windows local x64
  - policy: diagnostic process pin
  - CPU pin: `0`
  - build mode: optimized (`gcc -O2`)


## Matrix

Run used for this report:

- `powershell -ExecutionPolicy Bypass -File dev/bench/run_queue_latency_pinned_windows.ps1 -Cpu 0`

Collection policy:

- same default `4x4` contended matrix as the unpinned latency sweep
- batch sizes: `1`, `4`, `8`, `16`, `32`, `64`
- this is a diagnostic pass, not a replacement baseline


## Results

### Tail Latency

Latency in `us` (`p95`).

| Batch size | `MPSC` single | `MPSC` batch | `MPSC` batch/single | `MPMC` single | `MPMC` batch | `MPMC` batch/single |
|---|---:|---:|---:|---:|---:|---:|
| `1` | `137.400` | `136.200` | `0.99x` | `138.200` | `138.300` | `1.00x` |
| `4` | `139.400` | `59.300` | `0.43x` | `138.500` | `57.200` | `0.41x` |
| `8` | `140.800` | `41.000` | `0.29x` | `136.200` | `44.000` | `0.32x` |
| `16` | `150.000` | `37.400` | `0.25x` | `137.800` | `42.400` | `0.31x` |
| `32` | `138.700` | `31.200` | `0.22x` | `136.800` | `32.200` | `0.24x` |
| `64` | `139.800` | `29.400` | `0.21x` | `137.200` | `32.600` | `0.24x` |


### Fairness

Pinned fairness is included for completeness, but it is not policy-grade data.
On this run, a single CPU lets one consumer hold the lane long enough that the
`MPMC` distribution collapses.

| Batch size | `MPMC` spread single pct | `MPMC` spread batch pct | `MPMC` max/min single | `MPMC` max/min batch |
|---|---:|---:|---:|---:|
| `1` | `300.000` | `300.000` | `0.000` | `0.000` |
| `4` | `300.000` | `300.000` | `0.000` | `0.000` |
| `8` | `204.096` | `300.000` | `0.000` | `0.000` |
| `16` | `300.000` | `300.000` | `0.000` | `0.000` |
| `32` | `204.096` | `204.096` | `0.000` | `0.000` |
| `64` | `204.096` | `300.000` | `0.000` | `0.000` |


## Interpretation

- The pinned Windows read is much cleaner than the earlier unpinned sweep.
- Under `cpu=0` pinning, `batch=4` and above improve `p95` in both `MPSC` and
  `MPMC`, and the curve is broadly monotonic through `32/64`.
- That strongly suggests the weird unpinned Windows sweep shape was not purely
  queue logic; host scheduling noise was a large part of it.
- This does not make pinned numbers the new default. Single-CPU pinning also
  destroys the `MPMC` fairness signal, so it cannot be used to pick a shipping
  batch size by itself.
- The practical read is:
  - unpinned Windows tells us what the host actually feels like
  - pinned Windows tells us the batch fast path itself is sound
  - the default batch-size decision still has to be made from unpinned data,
    with pinned runs used only to explain anomalies


## Source Summary

- `dev/bench/sweeps/queue_latency/windows_pinned_cpu0/SUMMARY_20260325.md`
