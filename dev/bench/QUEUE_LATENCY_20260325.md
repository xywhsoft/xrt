# XRT Queue Latency and Fairness Compare (2026-03-25)

This report records the first dedicated latency/fairness pass for the queue
batch work after the range-claim fast path landed.

It exists to answer the question the throughput reports cannot answer on their
own: when `PushBatch/PopBatch` wins on items/s, what happens to queue wait time
and consumer-side fairness?


## Companion Sources

- `dev/bench/queue/bench_queue_latency.c`
- `dev/bench/run_queue_latency_windows.ps1`
- `dev/bench/run_queue_latency_linux.sh`
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

- `powershell -ExecutionPolicy Bypass -File dev/bench/run_queue_latency_windows.ps1`
- `bash dev/bench/run_queue_latency_linux.sh`

Collection policy:

- report values below were collected serially per host
- this is a first-pass compare, not a latency sweep

Default matrix:

- items per producer: `100000`
- capacity: `4096`
- MPSC producers: `4`
- MPMC producers: `4`
- MPMC consumers: `4`
- batch size: `32`


## Results

Latency in `us`.

### MPSC

| Host | Mode | Avg | p50 | p95 | p99 | Elapsed ms |
|---|---|---:|---:|---:|---:|---:|
| Windows | single | `0.641` | `0.500` | `1.500` | `2.500` | `43.981` |
| Windows | batch-32 | `3.594` | `2.900` | `8.500` | `13.100` | `10.094` |
| Linux | single | `557.269` | `814.345` | `832.179` | `841.733` | `73.130` |
| Linux | batch-32 | `416.982` | `465.639` | `515.144` | `528.762` | `41.254` |


### MPMC

| Host | Mode | Avg | p50 | p95 | p99 | Elapsed ms |
|---|---|---:|---:|---:|---:|---:|
| Windows | single | `1.191` | `0.900` | `2.900` | `7.000` | `54.901` |
| Windows | batch-32 | `23.395` | `13.400` | `89.700` | `95.900` | `9.779` |
| Linux | single | `920.505` | `968.749` | `984.020` | `990.134` | `93.109` |
| Linux | batch-32 | `9.498` | `6.613` | `24.780` | `55.978` | `26.790` |


### MPMC Fairness

Fairness here is consumer-side distribution under the `4x4` default matrix.
Lower spread and lower `max/min` indicate a more even split.

| Host | Mode | Min items | Max items | Spread pct | Max/min |
|---|---|---:|---:|---:|---:|
| Windows | single | `92454` | `105705` | `7.546` | `1.143` |
| Windows | batch-32 | `78293` | `126574` | `26.574` | `1.617` |
| Linux | single | `98620` | `102898` | `2.898` | `1.043` |
| Linux | batch-32 | `72984` | `160613` | `60.613` | `2.201` |


## Interpretation

- Batch throughput wins do not imply latency wins on every host.
- On the tested Windows host, `batch=32` improves elapsed throughput time but
  clearly raises queue wait time on both `MPSC` and `MPMC`, especially at the
  `MPMC` tail.
- On the tested Debian host, `batch=32` improves both elapsed time and queue
  latency on `MPSC` and `MPMC`, with the strongest gain on `MPMC`.
- `MPMC` fairness degrades under batch on both hosts. The effect is modest on
  Windows relative to the throughput gain, but very large on the Debian host in
  this run.
- The cross-platform read is now clearer:
  - throughput says batch is a strong fast path
  - latency says the cost/benefit is lane- and platform-dependent
  - fairness says large batches can let one consumer hold the lane too long
- That means the next queue decision should not be "is batch good" anymore. It
  should be "what default batch size is acceptable once throughput, latency,
  and fairness are considered together?"


## Notes

- This report measures queue wait from successful enqueue publication to
  consumer observation.
- `MPSC` fairness is not reported because there is only one consumer in that
  lane.
- This pass is still a single optimized compare per host, not a repeated
  latency sweep.
