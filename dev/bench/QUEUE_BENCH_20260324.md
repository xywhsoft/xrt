# XRT Queue Throughput Baseline (2026-03-24)

This report records the first repeatable benchmark baseline for the new
pointer-queue lane in XRT.

It exists to keep queue throughput claims anchored to a concrete runner and
matrix, without widening the queue rollout into a large benchmark framework.


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
  - compiler: `gcc 14.2.0`
  - build mode: optimized (`gcc -O2`)


## Matrix

Run used for this report:

- `powershell -ExecutionPolicy Bypass -File dev/bench/run_queue_bench_windows.ps1`

Default matrix:

- items per producer: `500000`
- capacity: `4096`
- MPSC producers: `4`
- MPMC producers: `4`
- MPMC consumers: `4`


## Results

| Lane | Items | Windows elapsed ns | Windows items/s | Debian 13 elapsed ns | Debian 13 items/s | Read |
|---|---:|---:|---:|---:|---:|---|
| `SPSC` | `500000` | `67064400` | `7455520.365` | `90829158` | `5504840.197` | Windows currently leads in this local baseline |
| `MPSC` | `2000000` | `323773700` | `6177153.981` | `372469711` | `5369564.131` | both remain in the same broad throughput range under four producers |
| `MPMC` | `2000000` | `443828100` | `4506249.154` | `450417299` | `4440326.791` | the two platforms are very close in this matrix |


## Interpretation

- The current pointer-queue implementation is already stable enough to support
  a dedicated throughput lane.
- `SPSC` is predictably the fastest path in the current matrix.
- `MPSC` holds up reasonably well under four producers on both platforms, which
  is a useful sign for the current `xnet` command-queue integration.
- `MPMC` is slower than `MPSC` in both runs, which is expected given the extra
  synchronization and multi-consumer contention.
- The current Linux baseline is competitive with Windows in the `MPMC` lane,
  while Windows still leads in `SPSC` and `MPSC` for this unpinned run.


## Notes

- The runner currently targets throughput only; it does not report percentile
  latency.
- Small run-to-run variation is expected under the default unpinned local
  policy; compare future runs using the same matrix and policy.
- The first Linux queue-bench attempt on the same day exposed a 32-bit atomic
  width bug in `lib/queue.h` on 64-bit Linux; the numbers above were recorded
  after switching queue-local 32-bit atomics away from `volatile long*`
  reinterprets.
- If a future queue report changes the matrix or policy, it should say so
  explicitly instead of silently replacing this baseline.
