# XRT Queue Throughput Sweep (2026-03-25)

This report widens the first queue throughput baseline into a small fixed sweep
so we can reason about capacity and contention without turning queue benching
into a heavyweight framework.


## Companion Sources

- `dev/bench/queue/bench_queue_pointer.c`
- `dev/bench/run_queue_bench_sweep_windows.ps1`
- `dev/bench/run_queue_bench_sweep_linux.sh`
- `dev/bench/summarize_queue_bench_sweep_windows.ps1`
- `dev/bench/summarize_queue_bench_sweep_linux.sh`
- `dev/bench/sweeps/queue/windows/SUMMARY_20260325.md`
- `dev/bench/sweeps/queue/linux/SUMMARY_20260325.md`
- `dev/bench/XRT_FOUNDATION_BENCHMARK_METHOD.md`


## Environment Policy

- Windows local x64
  - policy: unpinned
  - build mode: optimized (`gcc -O2`)
- Debian 13 x64 remote
  - host: `192.168.0.20`
  - policy: unpinned
  - build mode: optimized (`gcc -O2`)


## Sweep Matrix

Shared sweep policy:

- items per producer: `250000`
- fixed capacity for producer/pair sweeps: `4096`

Families:

- capacity: `256`, `1024`, `4096`, `16384`
- MPSC producers: `1`, `2`, `4`, `8`
- MPMC pairs: `1x1`, `2x2`, `4x4`, `8x8`


## Results

### Capacity Sweep

Throughput in `M items/s`.

| Capacity | Windows `SPSC` | Windows `MPSC` | Windows `MPMC` | Linux `SPSC` | Linux `MPSC` | Linux `MPMC` |
|---|---:|---:|---:|---:|---:|---:|
| `256` | `10.22` | `6.30` | `4.62` | `6.51` | `5.29` | `4.52` |
| `1024` | `9.90` | `8.51` | `6.31` | `6.34` | `5.39` | `4.56` |
| `4096` | `9.93` | `8.28` | `6.23` | `6.90` | `5.56` | `4.72` |
| `16384` | `10.42` | `8.40` | `6.38` | `6.63` | `5.39` | `4.60` |

Read:

- `256` is visibly too small for the contended lanes on both platforms.
- `1024` already removes most of the avoidable pressure; moving to `4096` and
  `16384` only gives modest additional change in this matrix.


### MPSC Producer Sweep

Throughput in `M items/s`.

| Producers | Windows `MPSC` | Linux `MPSC` | Read |
|---|---:|---:|---|
| `1` | `16.45` | `7.05` | single-producer is the clear ceiling |
| `2` | `11.29` | `6.14` | the first extra producer costs a lot on Windows, less on Linux |
| `4` | `8.30` | `5.73` | current `xnet`-like fan-in is still in a healthy band |
| `8` | `7.22` | `4.70` | more fan-in keeps working, but scaling is no longer close to linear |


### MPMC Pair Sweep

Throughput in `M items/s`.

| Pair | Windows `MPMC` | Linux `MPMC` | Read |
|---|---:|---:|---|
| `1x1` | `18.21` | `6.97` | low-contention MPMC is much cheaper than the fully contended case |
| `2x2` | `8.58` | `5.07` | most of the contention cost appears immediately |
| `4x4` | `6.42` | `4.57` | this is close to the current default baseline shape |
| `8x8` | `6.63` | `5.04` | heavier fan-out remains viable, but not cheap |


## Interpretation

- The queue lane now has a concrete capacity sweet spot: `1024+` is already
  enough to avoid the most obvious ring-pressure penalty in the current
  throughput matrix.
- The current default `4096` capacity still looks like a safe, conservative
  V1 choice, but these numbers suggest it is not a magical breakpoint; it is
  mainly a headroom choice.
- `MPSC` scales in the expected direction on both hosts: throughput drops as
  producer count rises, but the `4-producer` lane remains strong enough to
  support the current `xnet` command-queue use case.
- `MPMC` shows the expected contention curve too: `1x1` is dramatically easier
  than `2x2+`, and the `4x4` to `8x8` region is where future optimization work
  should focus if we want a better fully contended lane.
- The Windows host is still faster on the contended lanes, but the refreshed
  Debian sweep is closer than the first sample set and preserves the same broad
  curve shape.


## Notes

- This sweep is throughput-only; it does not report latency percentiles.
- Each sweep point is currently a single optimized run, not a repeated
  statistical sample set.
- The per-family summaries under `dev/bench/sweeps/queue/...` are the source
  artifacts for the tables above.
- If a future queue sweep changes the family set, item count, or benchmark
  policy, it should say so explicitly instead of silently replacing this one.
