# XRT Memory Pool Validation Bench (2026-03-16)

## Scope

This report records the first focused validation pass after the global memory
pool, debug allocator, object-lifetime diagnostics, and temp-arena redesign
landed on mainline.

Environment for this pass:

- Host: local Windows x64 development workstation
- Build mode: release `-O2` unless otherwise noted
- Runtime mode: production allocator path unless explicitly built with
  `XRT_MEM_DEBUG`

This report is intentionally conservative:

- allocator-focused and container-focused benches are treated as authoritative
  for the memory-pool work itself
- coroutine and xnet results are treated as spot regression checks against the
  previously published foundation baselines
- the `xnet` pressure lane required a dedicated rerun after two fixes: the
  benchmark timeout loops were converted from sleep-iteration counts to
  monotonic wall-clock deadlines, and the paused-read recv path in `xnet_stream`
  was repaired to re-arm receive work after buffered data drained

## 1. Focused Allocator Bench

Command:

```powershell
gcc dev/bench/bench_memglobal_small.c xrt.c -O2 -I . -o dev/bench/bench_memglobal_small.exe -lws2_32 -liphlpapi
.\dev\bench\bench_memglobal_small.exe 200000
```

Results:

- Raw `malloc/free` fixed `32B`: `28.70M ops/s`
- XRT pooled `xrtMalloc/xrtFree` fixed `32B`: `29.43M ops/s`
- Fixed `32B` speedup vs raw: `1.025x`
- Raw mixed `16..128B`: `28.45M ops/s`
- XRT pooled mixed `16..128B`: `28.48M ops/s`
- Mixed small-class speedup vs raw: `1.001x`
- Raw `calloc/free` fixed `32B`: `28.08M ops/s`
- XRT pooled `xrtCalloc/xrtFree` fixed `32B`: `27.03M ops/s`
- Fixed `32B calloc` speedup vs raw: `0.963x`

Interpretation:

- the new pooled path is already slightly faster on the hottest `malloc/free`
  lane
- mixed pooled sizes are effectively neutral vs raw allocator on this host
- pooled `calloc` is slightly slower on this first pass and remains a follow-up
  tuning target

## 2. Container Churn Bench

Command:

```powershell
gcc dev/bench/bench_container_churn.c xrt.c -O2 -I . -o dev/bench/bench_container_churn.exe -lws2_32 -liphlpapi
.\dev\bench\bench_container_churn.exe 1000 96
```

Results:

- Array churn: `597,979 containers/s`, `57.41M items/s`
- Dynstack churn: `2.03M containers/s`, `194.77M items/s`
- List churn: `134,503 containers/s`, `12.91M items/s`
- Dict churn: `64,197 containers/s`, `6.16M items/s`
- AVLTree churn: `183,830 containers/s`, `17.65M items/s`

Interpretation:

- the current allocator is sustaining high root-object churn without functional
  regressions
- pointer-heavy keyed structures (`dict`, `list`, `avltree`) remain the most
  allocation-sensitive container lanes
- the bench is now available as a repeatable regression target for future
  allocator changes

## 3. Coroutine Spot Regression Check

Command:

```powershell
powershell -ExecutionPolicy Bypass -File dev/bench/run_coroutine_bench_windows.ps1
```

Current spot results:

- Context switches: `25.23M/s`
- Create/destroy: `372.35k/s`
- Create/resume/destroy: `230.31k/s`
- Timer waits: `7.74M/s`
- Scheduler post wakeups: `1.72M/s`

Reference baseline from [COROUTINE_BENCH_20260314.md](/D:/Git/xrt/dev/bench/COROUTINE_BENCH_20260314.md):

- Context switches: `26.2M/s`
- Create/destroy: `370k/s`
- Create/resume/destroy: `246k/s`
- Timer waits: `7.91M/s`
- Scheduler post wakeups: `1.64M/s`

Interpretation:

- context switch, timer, and create/destroy remain in the same envelope
- create/resume/destroy is modestly lower in this spot run
- scheduler post is slightly better in this spot run
- no broad allocator-linked coroutine regression is evident from this pass

## 4. XNet Spot Regression Check

Commands:

```powershell
gcc dev/bench/xnet2/bench_echo_tcp.c xrt.c -O2 -I . -o dev/bench/xnet2/bench_echo_tcp_mem.exe -lws2_32 -liphlpapi
gcc dev/bench/xnet2/bench_echo_tls.c xrt.c -O2 -I . -o dev/bench/xnet2/bench_echo_tls_mem.exe -lws2_32 -liphlpapi
gcc dev/bench/xnet2/bench_udp_burst.c xrt.c -O2 -I . -o dev/bench/xnet2/bench_udp_burst_mem.exe -lws2_32 -liphlpapi
.\dev\bench\xnet2\bench_echo_tcp_mem.exe 500 64
.\dev\bench\xnet2\bench_echo_tls_mem.exe 100 64 dev\xnet2_tls_test_cert.pem dev\xnet2_tls_test_key.pem
.\dev\bench\xnet2\bench_udp_burst_mem.exe 8000 128
```

Current spot results:

- TCP echo steady-state: `94.21k msg/s`
- TCP latency `p50/p95/p99`: `10.7 / 12.9 / 19.7 us`
- TLS echo steady-state: `47.78k msg/s`
- TLS latency `p50/p95/p99`: `19.1 / 27.7 / 49.9 us`
- UDP send: `1.41M packets/s`

Reference baseline from [XNET_COMPARE_20260314.md](/D:/Git/xrt/dev/bench/XNET_COMPARE_20260314.md):

- TCP total: `83.2k msg/s`
- TLS total: `59.3k msg/s`
- UDP send: `1.72M packets/s`

Interpretation:

- TCP remains healthy and is above the earlier published Windows spot result
- TLS and UDP are lower in this pass than the older published spot numbers
- because this pass did not complete a full repeated xnet compare sweep, these
  results should be treated as spot health checks, not as a new official xnet
  baseline

## 5. Validation Gates Covered by This Pass

Completed in this pass:

- allocator-focused microbench
- container churn bench
- coroutine benchmark rerun
- xnet TCP/TLS/UDP spot rerun
- release temp-arena focused test
- debug temp-arena focused test
- debug allocator/object focused test
- release and debug xnet stage harness rerun

Completed after a dedicated rerun:

- `xnet` send-queue-pressure benchmark lane
  - the first run exposed two real issues:
    - timeout loops were based on sleep-iteration counts, which stretched wall
      time under Windows timer granularity
    - paused-read recv dispatch did not reliably re-arm receive work after the
      buffered chain drained, which could leave the server side short on bytes
      even after the client send queue had reached zero
  - the dedicated acceptance rerun now completes successfully with:
    - enqueue elapsed: `15.999 ms`
    - drain elapsed: `16.002 ms`
    - sent messages: `512`
    - expected/server recv bytes: `524288 / 524288`
    - server recv calls: `64`
    - final pending send bytes: `0`
    - `high_water=1 low_water=0 drain=0 peak_queue=524288`
    - `client_errors=0 server_errors=0`

## 6. Current Conclusion

This validation pass supports the following conclusions:

- the global small-object allocator is at least neutral-to-positive on the
  hottest `malloc/free` lanes
- the current implementation does not show a broad coroutine regression
- the allocator change did not break xnet/TLS/HTTP/WS functional validation
- container churn is now covered by a repeatable focused bench
- the `xnet` pressure lane is now accepted after the dedicated rerun and the
  two underlying issues were fixed in code, not worked around in the report

This report now supports declaring the benchmark/documentation phase accepted
for the current memory-pool spec.
