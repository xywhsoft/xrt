# XRT Queue Batch Size Sweep (2026-03-25)

This report sweeps the current batch API across a small range of batch sizes
under the default queue matrix.

It exists to answer the practical question after the batch compare turned
positive: where do the gains start, and which batch sizes are strongest on the
current implementation?


## Companion Sources

- `dev/bench/queue/bench_queue_pointer.c`
- `dev/bench/run_queue_batch_sweep_windows.ps1`
- `dev/bench/run_queue_batch_sweep_linux.sh`
- `dev/bench/summarize_queue_batch_sweep_windows.ps1`
- `dev/bench/summarize_queue_batch_sweep_linux.sh`
- `dev/bench/sweeps/queue_batch/windows/SUMMARY_20260325.md`
- `dev/bench/sweeps/queue_batch/linux/SUMMARY_20260325.md`
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

- items per producer: `500000`
- capacity: `4096`
- MPSC producers: `4`
- MPMC producers: `4`
- MPMC consumers: `4`

Batch sizes:

- `1`, `4`, `8`, `16`, `32`, `64`


## Notes

- the current batch sweep is intentionally narrow: it keeps the workload fixed
  and only moves `batch_size`
- sweep summaries keep only the latest sample for each matrix key
- older logs remain archived in the sweep directory, but they are pruned out of
  the generated summary tables


## Results

### Windows

| Batch size | `MPSC` single | `MPSC` batch | `MPSC` batch/single | `MPMC` single | `MPMC` batch | `MPMC` batch/single |
|---|---:|---:|---:|---:|---:|---:|
| `1` | `5.94` | `5.75` | `0.97x` | `4.67` | `4.39` | `0.94x` |
| `4` | `6.02` | `9.50` | `1.58x` | `4.61` | `8.25` | `1.79x` |
| `8` | `6.18` | `13.05` | `2.11x` | `4.63` | `12.15` | `2.62x` |
| `16` | `6.09` | `20.08` | `3.30x` | `5.85` | `19.91` | `3.40x` |
| `32` | `6.17` | `36.08` | `5.85x` | `4.50` | `34.86` | `7.74x` |
| `64` | `5.94` | `44.60` | `7.51x` | `4.53` | `51.41` | `11.35x` |

Read:

- Windows flips from roughly neutral at `batch = 1` to clearly positive from
  `batch = 4` onward.
- On Windows, both `MPSC` and `MPMC` keep climbing through `16`, `32`, and
  `64`; the current throughput winner is the largest tested batch.


### Linux

| Batch size | `MPSC` single | `MPSC` batch | `MPSC` batch/single | `MPMC` single | `MPMC` batch | `MPMC` batch/single |
|---|---:|---:|---:|---:|---:|---:|
| `1` | `5.34` | `4.23` | `0.79x` | `4.63` | `4.42` | `0.96x` |
| `4` | `5.21` | `9.96` | `1.91x` | `4.56` | `8.65` | `1.90x` |
| `8` | `5.49` | `9.43` | `1.72x` | `4.58` | `9.32` | `2.04x` |
| `16` | `5.54` | `10.22` | `1.85x` | `4.56` | `12.04` | `2.64x` |
| `32` | `4.90` | `10.45` | `2.13x` | `4.59` | `14.86` | `3.24x` |
| `64` | `5.54` | `30.85` | `5.57x` | `4.74` | `19.47` | `4.10x` |

Read:

- Linux also turns clearly positive once the batch is large enough, but the
  curve is less smooth than Windows.
- `batch = 1` is not useful on Linux in this run, while `4-64` are all
  positive and `64` becomes the strongest point in both `MPSC` and `MPMC`.


## Interpretation

- The practical conclusion is no longer “does batch help at all”; it clearly
  does from `4` upward on both hosts.
- `batch = 1` should still be treated as baseline plumbing, not as a useful
  fast-path size.
- `batch = 4` is the first size that consistently turns positive across both
  platforms.
- `batch = 32` and `64` are the most promising throughput-first settings in the
  current matrix, with `64` now winning outright in the latest sweep.
- That does not freeze default guidance yet. This report is still throughput
  only, and larger batches may trade away latency or fairness that this sweep
  does not measure.


## Source Summaries

- `dev/bench/sweeps/queue_batch/windows/SUMMARY_20260325.md`
- `dev/bench/sweeps/queue_batch/linux/SUMMARY_20260325.md`
