# XNET Compare Baseline (2026-03-14)

This report records the latest repeatable `xnet-v1` vs `xnet-v2` comparison
after repairing the `xnet-v2` benchmark harness, switching the runner
scripts to optimized (`-O2`) benchmark builds, separating setup cost from the
message loop in the `xnet-v2` echo benches, and scoping GCC
`no-strict-aliasing` around the in-tree crypto/TLS implementation used by
`xrt.c`.

Companion scripts:

- `dev/bench/run_xnet_compare_windows.ps1`
- `dev/bench/run_xnet_compare_linux.sh`
- `dev/bench/run_xnet_compare_sweep_windows.ps1`
- `dev/bench/run_xnet_compare_sweep_linux.sh`
- `dev/bench/summarize_xnet_compare_sweep_windows.ps1`
- `dev/bench/summarize_xnet_compare_sweep_linux.sh`
- `dev/bench/XNET_COMPARE_SWEEP_20260314.md`

## Matrix

Windows:

- TCP echo: `500 x 64B`
- TLS echo: `100 x 64B`
- UDP burst: `8000 x 128B`
- Queue pressure (`v2` only): `512 x 1024B pause=250ms drain_timeout=120000ms`

Debian 13:

- TCP echo: `500 x 64B`
- TLS echo: `100 x 64B`
- UDP burst: `8000 x 128B`
- Queue pressure (`v2` only): `512 x 1024B pause=250ms drain_timeout=120000ms`

## Results

### Windows

| Test | xnet-v1 | xnet-v2 | Read |
|---|---:|---:|---|
| TCP echo total msg/s | 91.7 | 83178.1 | `v2` now wins cleanly after the fairer message-loop timing split |
| TCP echo steady msg/s | n/a | 83179.5 | `v2` steady-state remains very strong |
| TCP p50 / p95 us | 14916.4 / 15281.5 | 11.7 / 12.7 | `v2` latency is dramatically tighter |
| TLS echo total msg/s | 99.9 | 59343.7 | the Windows `-O2` TLS regression is gone after scoping out strict-aliasing for crypto/TLS |
| TLS echo steady msg/s | n/a | 59347.2 | `v2` steady-state is now consistent with the total message-loop timing |
| TLS p50 / p95 us | 14883.0 / 15365.6 | 16.5 / 18.2 | `v2` is dramatically better in the optimized Windows matrix |
| UDP burst send pps | 322825.4 | 1718877.6 | `v2` send path is faster |
| UDP burst recv packets | 8000/8000 | 8000/8000 | both passed at the larger default lane too |
| Queue pressure (`v2`) | n/a | PASS | after switching `PauseRead` to real socket backpressure and relaxing the synthetic socket clamp to `16KB`, the lane still hits `high_water=1` and now drains in ~90.0ms with zero pending bytes |

### Debian 13

| Test | xnet-v1 | xnet-v2 | Read |
|---|---:|---:|---|
| TCP echo total msg/s | 36628.1 | 60240.8 | this rerun now shows `v2` ahead on Linux TCP total throughput too |
| TCP echo steady msg/s | n/a | 60241.6 | `v2` steady-state stays aligned with the fairer total metric |
| TCP p50 / p95 us | 26.9 / 32.8 | 15.8 / 22.6 | `v2` now leads both median and tail latency in this rerun |
| TLS echo total msg/s | incomplete | 39332.4 | legacy Linux TLS still flakes, while `v2` completes cleanly |
| TLS echo steady msg/s | n/a | 39333.9 | `v2` remains the only implementation here exposing a reliable steady-state number |
| TLS p50 / p95 us | incomplete | 21.5 / 40.5 | `v2` completed cleanly with healthy tails in the latest rerun |
| UDP burst send pps | 340122.5 | 1355287.2 | `v2` send path is faster |
| UDP recv pps | 793733.3 | 397383.4 | legacy receive-side looks higher in this single rerun, but both lanes completed cleanly |
| UDP burst recv packets | 8000/8000 | 8000/8000 | both passed at the larger default lane |
| Queue pressure (`v2`) | n/a | PASS | the stronger default lane still hits `high_water=1`; with real backpressure and a `16KB` clamp, Debian compare drain is now down near ~1.21s with zero pending bytes |

## Notes

- The compare runners now build benchmark binaries with `-O2`, so these numbers
  are much closer to the actual transport/runtime behavior than the earlier
  unoptimized script outputs.
- The Windows `-O2` TLS message-loop regression turned out to be tied to GCC
  strict-aliasing assumptions inside the in-tree crypto/TLS implementation.
  The current baseline scopes `no-strict-aliasing` to the `crypto.h` /
  `nettls.h` region inside `xrt.c`, instead of disabling optimization for the
  entire build.
- The compare runners also print the active matrix and support env-var overrides
  such as `XNET_COMPARE_TCP_ITER` and `XNET_COMPARE_TLS_ITER`, but the table
  above records the current default medium matrix.
- `xnet-v2` echo benchmarks now report both `total` and `steady-state`.
  `setup_elapsed_ns` records connect / TLS handshake cost; `total` and
  `steady-state` both measure the post-open message loop, with `steady-state`
  anchored from the first send and therefore nearly identical in the current
  one-message-at-a-time echo lane.
- `xnet-v1` echo benchmarks are still blocking one-message-at-a-time tests, so
  they expose only one aggregate throughput number.
- Windows UDP no longer looks limited to the tiny baseline. In the latest local
  probes, `xnet-v2` completed `8000 x 128B`, `16000 x 128B`, and even
  `32000 x 128B` without packet loss, so the official Windows compare lane has
  been raised to `8000 x 128B`.
- The Linux `xnet-v1` TLS benchmark still looks flaky across reruns. The Linux
  script keeps it under `timeout` so a hung or incomplete legacy TLS path does
  not block the whole comparison run.
- The latest Linux rerun also includes a staged-backend watch-node freelist plus
  targeted non-owner wakeups for stream/listener/dgram watch registration. That
  removed per-watch heap churn from the hot path and improved the Linux TCP/TLS
  rerun tails without regressing UDP correctness.
- `xrtNetStreamPauseRead()` now acts as real transport-level backpressure for
  socket-driven streams instead of merely suppressing `OnRecv` dispatch while
  continuing to drain the socket into user-space buffers. That makes the
  queue-pressure lane a more faithful backpressure probe, and also explains why
  the updated lane now reports non-trivial drain times on both Windows and
  Debian.
- The queue-pressure benchmark itself now uses a `16KB` send/receive socket
  clamp instead of the earlier `4KB` pathological clamp. It still reaches the
  configured `high_water`, but its drain phase is much more representative and
  less dominated by synthetic socket-buffer starvation.

## Takeaways

- `xnet-v2` is now benchmarkable and reproducible on both Windows and Debian.
- `xnet-v2` already shows clear wins in:
  - Windows TCP steady-state behavior and tail latency
  - Windows TLS throughput and latency in the optimized runner
  - Linux TCP throughput and latency in the latest fair-timing rerun
  - Windows and Linux UDP send path, now on a larger `8000 x 128B` default lane
  - Linux TLS stability plus steady-state visibility
- `xnet-v2` still needs more work in:
  - Linux queue-pressure drain time and broader repeatability sweeps
  - A larger curated sweep before claiming final performance leadership

## Affinity Follow-up

The latest follow-up reruns added optional affinity controls for diagnosis:

- `XBENCH_PIN_CPU` can pin an individual benchmark process from inside the
  shared bench harness
- `XNET_COMPARE_TASKSET_CPUS` can wrap each Linux compare lane with
  `taskset -c`

Those knobs are useful, but they are not part of the default baseline:

- single-core `XBENCH_PIN_CPU=1` on Debian reduces host noise but over-
  constrains the workload enough to distort throughput and hurt UDP completion
- dual-core Linux `taskset -c 0,1` is much healthier and can make the
  queue-pressure lane extremely tight, but it still changes the shape of the
  broader compare matrix
- Windows compare remains unpinned by default because the legacy
  `xnet-v1` TLS benchmark can hang under single-core pinning

So the official baseline remains the unpinned default compare lane, while
affinity stays an opt-in diagnostic tool until the project settles on one
controlled benchmark policy for Linux repeated sweeps.

That policy is now settled for the current project state:

- Linux official repeated-compare baseline: `policy: baseline-unpinned`
- Linux affinity controls remain diagnostic-only
- The current best repeated unpinned artifact is
  `dev/bench/sweeps/linux_unpinned_20260314_b/SUMMARY.md`

That 6-run Debian sweep shows:

- `xnet-v2` TCP total avg `~58.3k msg/s`, CV `~6.9%`
- `xnet-v2` TLS total avg `~40.1k msg/s`, CV `~4.3%`
- `xnet-v2` UDP send avg `~1.37M pps`, CV `~1.8%`
- `xnet-v2` queue drain avg `~933.1ms`, CV `~55.8%`

So the Linux baseline is now good enough to treat TCP/TLS/UDP throughput as
reasonably stable under the default policy, while still calling out
queue-pressure as the remaining noisy lane.

Focused queue-pressure diagnostics also now show that this remaining Linux
noise is concentrated in the post-resume drain interval itself:

- `resume_to_first_recv_ns` stays tiny
- `pending_zero_to_full_recv_ns` stays negligible
- the spread lives almost entirely in
  `resume_to_low_water_ns` / `resume_to_pending_zero_ns`

So the current evidence points much more toward host/socket-buffer scheduling
effects in the synthetic pressure lane than a clear `xnet-v2` correctness
problem.
