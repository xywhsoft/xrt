# XRT Queue Latency Batch-Size Sweep (2026-03-25)

This report extends the first queue latency/fairness compare into a small
`batch_size` sweep under the default `4x4` contended matrix.

It exists to answer the next practical question after the batch throughput and
single-point latency reports: which batch sizes stay interesting once tail
latency and `MPMC` consumer fairness are included in the read?


## Companion Sources

- `dev/bench/queue/bench_queue_latency.c`
- `dev/bench/run_queue_latency_sweep_windows.ps1`
- `dev/bench/run_queue_latency_sweep_linux.sh`
- `dev/bench/summarize_queue_latency_sweep_windows.ps1`
- `dev/bench/summarize_queue_latency_sweep_linux.sh`
- `dev/bench/sweeps/queue_latency/windows/SUMMARY_20260325.md`
- `dev/bench/sweeps/queue_latency/linux/SUMMARY_20260325.md`
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

- items per producer: `100000`
- capacity: `4096`
- `MPSC` producers: `4`
- `MPMC` producers: `4`
- `MPMC` consumers: `4`

Batch sizes:

- `1`, `4`, `8`, `16`, `32`, `64`


## Notes

- latency values below are `p95` queue wait in `us`
- fairness values are `MPMC` consumer-side spread; lower is more even
- sweep summaries keep the latest sample for each matrix key
- this is still an unpinned region-finding sweep, not a policy freeze


## Results

### Windows

| Batch size | `MPSC` p95 single | `MPSC` p95 batch | `MPSC` batch/single | `MPMC` p95 single | `MPMC` p95 batch | `MPMC` batch/single | `MPMC` spread single pct | `MPMC` spread batch pct |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `1` | `1.900` | `1.500` | `0.79x` | `3.800` | `4.100` | `1.08x` | `5.452` | `2.376` |
| `4` | `1.500` | `4.200` | `2.80x` | `4.100` | `424.200` | `103.46x` | `3.428` | `2.798` |
| `8` | `1.500` | `6.000` | `4.00x` | `50.400` | `96.100` | `1.91x` | `48.507` | `110.029` |
| `16` | `152.200` | `42.900` | `0.28x` | `223.100` | `69.500` | `0.31x` | `12.444` | `144.395` |
| `32` | `201.100` | `34.100` | `0.17x` | `252.300` | `64.800` | `0.26x` | `82.411` | `26.888` |
| `64` | `1.800` | `8.900` | `4.94x` | `5.000` | `22.500` | `4.50x` | `2.584` | `37.561` |

Read:

- Windows is highly non-monotonic in this unpinned sweep.
- `batch=4` is clearly hostile to `MPMC` tail latency on this host.
- `batch=16` and `32` are the only tested sizes that improve `p95` in both
  `MPSC` and `MPMC`, but they do not give a clean fairness story.
- `batch=8` and `16` show fairness pathologies large enough that they should
  not be treated as default candidates from this pass alone.


### Linux

| Batch size | `MPSC` p95 single | `MPSC` p95 batch | `MPSC` batch/single | `MPMC` p95 single | `MPMC` p95 batch | `MPMC` batch/single | `MPMC` spread single pct | `MPMC` spread batch pct |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `1` | `827.809` | `817.122` | `0.99x` | `986.061` | `980.136` | `0.99x` | `3.347` | `2.832` |
| `4` | `835.173` | `664.729` | `0.80x` | `990.108` | `574.457` | `0.58x` | `3.159` | `4.679` |
| `8` | `982.715` | `560.378` | `0.57x` | `987.902` | `455.278` | `0.46x` | `5.436` | `7.744` |
| `16` | `956.330` | `12.661` | `0.01x` | `987.657` | `53.806` | `0.05x` | `6.685` | `29.417` |
| `32` | `813.357` | `507.879` | `0.62x` | `996.239` | `83.766` | `0.08x` | `3.443` | `17.421` |
| `64` | `962.790` | `498.343` | `0.52x` | `989.040` | `214.679` | `0.22x` | `4.215` | `5.501` |

Read:

- Linux turns positive for tail latency from `batch=4` onward on both lanes.
- `batch=16` and `32` are the strongest tail-latency points in this run.
- fairness cost is modest at `4` and `8`, spikes at `16` and `32`, then softens
  again at `64`.
- the Linux read is much more coherent than Windows: batch is a real latency
  fast path here, but larger batches still trade away fairness.


## Interpretation

- The combined cross-platform answer is no longer "bigger batch is always
  better." The useful region depends on which risk matters more.
- `batch=1` is just the plumbing baseline. It is not the target operating
  point for the batch fast path.
- `batch=4` is the first size that looks broadly acceptable on Linux, but the
  Windows `MPMC` tail blow-up makes it unsafe as a global default from this
  sweep alone.
- `batch=16` and `32` are the strongest latency winners on Linux and can also
  look good on Windows in isolated runs, but both sizes still show fairness
  regressions large enough that they should be treated as throughput-first or
  tail-latency-first tuning points, not neutral defaults.
- If we want one conservative next-step candidate, `batch=8` still looks like a
  reasonable investigation point: clearly positive on Linux, less aggressive
  than `16/32`, but it still needs a cleaner Windows read before being frozen.
- The practical next move is not another throughput pass. It is a repeated or
  pinned latency/fairness sweep on Windows so we can separate real batch-size
  effects from host scheduling noise.


## Source Summaries

- `dev/bench/sweeps/queue_latency/windows/SUMMARY_20260325.md`
- `dev/bench/sweeps/queue_latency/linux/SUMMARY_20260325.md`
