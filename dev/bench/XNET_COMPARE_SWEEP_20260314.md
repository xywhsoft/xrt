# XNET Compare Sweep Report (2026-03-14)

This report captures the first repeated `xnet-v1` vs `xnet-v2` compare sweep
after the benchmark harness repair, the larger UDP lane, the real
`PauseRead()` backpressure semantics, and the compare/log summarizers.

Primary artifacts:

- `dev/bench/run_xnet_compare_sweep_windows.ps1`
- `dev/bench/run_xnet_compare_sweep_linux.sh`
- `dev/bench/summarize_xnet_compare_sweep_windows.ps1`
- `dev/bench/summarize_xnet_compare_sweep_linux.sh`
- `dev/bench/sweeps/windows/SUMMARY_20260314.md`
- `dev/bench/sweeps/linux/SUMMARY_20260314.md`
- `dev/bench/sweeps/windows_fresh_20260314_owner_b/SUMMARY.md`
- `dev/bench/sweeps/linux_taskset_20260314_a/SUMMARY.md`
- `dev/bench/sweeps/linux_unpinned_20260314_b/SUMMARY.md`

## Sweep Summary

### Windows repeat sweep

Samples:

- 8 valid compare logs
- 1 truncated log skipped

| Metric | Avg | Min | Max | CV % | Read |
|---|---:|---:|---:|---:|---|
| `xnet-v2` TCP total msg/s | 81,979.4 | 80,098.7 | 83,245.9 | 1.1 | TCP transport throughput is already very stable |
| `xnet-v2` TCP steady msg/s | 81,980.6 | 80,100.0 | 83,245.9 | 1.1 | steady-state tracks total cleanly |
| `xnet-v2` TLS total msg/s | 53,276.6 | 33.7 | 61,602.9 | 37.8 | one catastrophic outlier still contaminates the full compare sweep |
| `xnet-v2` TLS steady msg/s | 53,279.9 | 33.7 | 61,606.7 | 37.8 | the outlier is not handshake-only; it drags the message loop too |
| `xnet-v2` UDP send pps | 1,735,195.9 | 1,667,569.9 | 1,792,717.1 | 2.3 | UDP send path is both fast and fairly repeatable |
| `xnet-v2` queue drain ms | 80.9 | 15.0 | 90.9 | 30.8 | queue-pressure is still materially more variable than TCP/UDP |

Legacy reference:

- `xnet-v1` TCP msg/s avg: `103.9`
- `xnet-v1` TLS msg/s avg: `74.9`
- `xnet-v1` UDP send pps avg: `322,837.6`

### Debian 13 repeat sweep

Samples:

- 4 valid compare logs
- `xnet-v1 tls` incomplete in `4/4` runs
- `xnet-v1 tcp` incomplete in `1/4` runs

| Metric | Avg | Min | Max | CV % | Read |
|---|---:|---:|---:|---:|---|
| `xnet-v2` TCP total msg/s | 59,572.9 | 58,444.9 | 61,082.7 | 1.7 | Linux TCP steady-state is already fairly tight |
| `xnet-v2` TCP steady msg/s | 59,573.7 | 58,445.7 | 61,083.6 | 1.7 | steady-state stays aligned with total |
| `xnet-v2` TLS total msg/s | 36,138.5 | 31,856.5 | 40,037.4 | 9.3 | Linux TLS is slower than Windows but much more stable than legacy |
| `xnet-v2` TLS steady msg/s | 36,140.3 | 31,859.5 | 40,039.0 | 9.3 | steady-state is consistent enough for tuning |
| `xnet-v2` UDP send pps | 1,307,177.0 | 1,041,300.6 | 1,409,259.5 | 11.8 | UDP send is strong, but variance is still visible |
| `xnet-v2` queue drain ms | 1,272.6 | 594.0 | 2,068.1 | 47.3 | queue-pressure drain remains the noisiest Linux metric by far |

Legacy reference:

- `xnet-v1` TCP msg/s avg: `62,699.2`
- `xnet-v1` UDP send pps avg: `349,928.8`
- `xnet-v1 tls` did not produce a stable baseline

## Focused Probes

### Windows `xnet-v2` TLS standalone rerun

I reran `dev/bench/xnet2/bench_echo_tls.c` six times outside the full compare
workflow after the sweep exposed the `33.7 msg/s` outlier.

| Metric | Avg | Min | Max | CV % |
|---|---:|---:|---:|---:|
| TLS total msg/s | 60,381.8 | 55,564.8 | 61,919.5 | 3.8 |

Interpretation:

- the catastrophic Windows TLS sweep outlier does not reproduce in the focused
  standalone lane
- the remaining suspicion is now “compare-context interference” rather than a
  permanent TLS hot-path collapse

### Debian `xnet-v2` queue-pressure standalone rerun

I reran `dev/bench/xnet2/bench_send_queue_pressure.c` five times on Debian 13.

| Metric | Avg | Min | Max | CV % |
|---|---:|---:|---:|---:|
| queue drain ms | 1,548.5 | 584.6 | 4,132.9 | 84.5 |

Interpretation:

- the Linux queue-pressure drain variance is real even outside the compare lane
- this remains one of the clearest unfinished transport/perf items in
  `xnet-v2`

### Post-fix queue-pressure rerun

I then tightened `dev/bench/xnet2/bench_send_queue_pressure.c` again so the
drain loop no longer stops at “client user-space send queue is empty”; it now
waits for both:

- `xrtNetStreamPendingSend(pClient) == 0`
- `server_recv_bytes >= expected_recv_bytes`

This removed a clear measurement bug where some Windows runs reported a short
drain even though the server had not actually consumed every byte yet.

Focused post-fix reruns:

- Windows standalone queue-pressure: `~102.5ms` and `~105.5ms`, both with
  `server_recv_bytes == expected_recv_bytes == 8388608`
- Debian standalone queue-pressure: `~1631.5ms` and `~1630.7ms`, both with
  `server_recv_bytes == expected_recv_bytes == 524288`

Integrated post-fix compare sweeps still show spread:

- Windows compare queue-pressure: `~14.7ms` and `~105.2ms`, both now with full
  receive completion
- Debian compare queue-pressure: `~1419.6ms` and `~584.7ms`, both now with full
  receive completion

Interpretation:

- the old “short drain with partial receive” bug is gone
- the remaining spread now looks like a real scheduler / buffering /
  compare-context variability issue rather than a simple benchmark accounting
  error

### Owner-worker batched queue-pressure rerun

I then changed the queue-pressure benchmark one more time:

- the pressure-producing send phase no longer floods the stream from an
  external thread via thousands of async `xrtNetStreamSend()` posts
- instead, it now posts one owner-worker task and uses batched
  `xrtNetStreamSendVec()` calls so high-water is triggered synchronously on the
  stream owner
- the queue-pressure benchmark also now pins `iTimerTickMs = 1` for the
  benchmark engines, so the drain lane is less exposed to a coarse `10ms`
  worker harvest cadence

That removes most of the command-queue noise from this benchmark and makes the
pressure lane much closer to “send queue / transport backpressure” and much
less “cross-thread task posting throughput”.

Focused owner-worker reruns:

- Windows standalone queue-pressure:
  - drain: `~14.7ms`, `~15.0ms`, `~15.0ms`, `~15.2ms`
  - `sent_messages = 512`
  - `high_water = 16`, `low_water = 16`, `peak_queue = 32768`
- Debian standalone queue-pressure with `1ms` tick:
  - drain: `~372.4ms`, `~574.0ms`, `~583.9ms`
  - `sent_messages = 512`
  - `high_water = 3`, `low_water = 3`, `peak_queue = 458779`

Integrated compare reruns after this change:

- Windows compare queue-pressure:
  - `~15.3ms`
  - `~12.0ms`
- Debian compare queue-pressure:
  - `~2044.6ms`
  - `~372.7ms`
  - later clean rerun: `~372.4ms`

Interpretation:

- Windows queue-pressure is now essentially stable enough to treat as solved
  for this benchmark lane
- Debian queue-pressure is much better than the earlier multi-second / highly
  noisy runs, but integrated compare still needs a broader rerun before calling
  it closed

### Fresh repeated reruns on the current baseline

I then reran the updated compare lane on both hosts again.

Windows fresh repeated sweep:

- artifact: `dev/bench/sweeps/windows_fresh_20260314_owner_b/SUMMARY.md`
- 3 valid logs
- `xnet-v2` TCP total avg `~82.5k msg/s`, CV `~1.0%`
- `xnet-v2` TLS total avg `~60.5k msg/s`, CV `~1.8%`
- `xnet-v2` UDP send avg `~1.72M pps`, CV `~0.3%`
- `xnet-v2` queue drain avg `~15.2ms`, CV `~1.9%`

Debian affinity probes:

- single-process `XBENCH_PIN_CPU=1` sharply reduced host noise, but it also
  over-constrained the benchmark: TCP/TLS moved, queue drain rose back to
  `~1.97s`, and UDP receive completion degraded badly enough that it is not a
  suitable official baseline
- dual-core `taskset -c 0,1` is much healthier than single-core pinning and
  produces a very tight queue-pressure lane:
  - artifact: `dev/bench/sweeps/linux_taskset_20260314_a/SUMMARY.md`
  - 3 valid logs
  - `xnet-v2` TCP total avg `~60.9k msg/s`, CV `~3.7%`
  - `xnet-v2` TLS total avg `~42.4k msg/s`, CV `~5.6%`
  - `xnet-v2` queue drain avg `~372.5ms`, CV `~0.1%`
  - but `xnet-v2` UDP send still shows wide spread (CV `~33.3%`) and
    `xnet-v1` TCP also swings heavily (CV `~25.9%`)

Interpretation:

- the earlier Windows catastrophic TLS outlier has not reproduced on the
  current queue-pressure baseline
- the queue-pressure lane itself is now trustworthy enough that it should no
  longer be treated as the primary open blocker
- Linux repeated-compare variance now looks more like a host / scheduling /
  benchmark-environment problem than a single obvious `xnet-v2` transport bug
- CPU-affinity controls are useful for diagnosis, but single-core pinning is
  too distortive and even dual-core `taskset` is not yet a clean default
  baseline policy

## Takeaways

- `xnet-v2` TCP and UDP are now repeatable enough on both Windows and Debian
  to support real performance work.
- Windows TLS is fast in focused reruns, but the full compare sweep still has
  one catastrophic outlier that needs context-level diagnosis.
- Windows queue-pressure now looks good enough after the owner-worker batched
  pressure rewrite.
- Debian queue-pressure is no longer wildly pathological, but it still needs a
  broader integrated rerun before it can be called “done”.
- Legacy Linux TLS is not a credible baseline anymore; `xnet-v2` is the only
  implementation consistently finishing that lane.

## Remaining Work Before Calling The Network Rewrite Finished

- Diagnose the Windows compare-context TLS outlier until the repeated sweep
  stops producing rare collapse runs.
- Reduce Debian queue-pressure drain variance and remeasure the repeated sweep.
- Re-run the repeated compare sweep after those fixes and refresh the summary.

## Latest Position

The older sections above remain useful as the historical trail, but the newest
reruns change the priority order:

- the current fresh Windows repeated sweep in
  `dev/bench/sweeps/windows_fresh_20260314_owner_b/SUMMARY.md` does not
  reproduce the earlier catastrophic TLS collapse; on the current queue-pressure
  baseline it stays around `~60.5k msg/s` with CV `~1.8%`
- the Linux queue-pressure lane itself is no longer the main benchmark blocker;
  with the current owner-worker pressure design it can be kept extremely tight
  under a constrained environment, but Linux repeated-compare variance still
  depends heavily on the benchmark host policy
- optional affinity controls now exist for diagnosis:
  - `XBENCH_PIN_CPU` applies a single-process CPU pin inside each benchmark
  - `XNET_COMPARE_TASKSET_CPUS` applies Linux `taskset -c` at the runner level
- single-core `XBENCH_PIN_CPU=1` on Debian is too distortive to use as the
  official baseline: it sharply changes throughput and degrades UDP completion
- dual-core `taskset -c 0,1` is much healthier and produced a very tight Linux
  queue-pressure sweep in `dev/bench/sweeps/linux_taskset_20260314_a/SUMMARY.md`
  with queue drain avg `~372.5ms` and CV `~0.1%`, but it still leaves visible
  spread in other lanes such as UDP send
- a broader `taskset -c 0,1,2,3` spot probe also failed to become an obvious
  answer: `xnet-v2` still completed cleanly, but legacy TCP went incomplete in
  that run and the overall matrix was not clearly better than the unpinned
  baseline

Updated remaining work:

- decide and document the official Linux benchmark-environment policy instead
  of mixing unpinned, single-core, and taskset-based runs ad hoc
- rerun a broader Linux repeated sweep under that one policy and refresh the
  summary
- keep validating the Windows compare lane as changes land, but Windows TLS no
  longer looks like the highest-priority blocker

## Official Linux Policy Decision

The latest 6-run Debian sweep in
`dev/bench/sweeps/linux_unpinned_20260314_b/SUMMARY.md` is now the best
reference for the official Linux repeated-compare policy.

Why this becomes the default policy:

- it is the least distortive environment and matches how the main compare lane
  is normally exercised
- it keeps the runner fair to both `xnet-v1` and `xnet-v2`
- it avoids the clearly unrealistic single-core pin case
- it avoids overstating confidence by silently switching to a constrained
  environment that changes the workload shape

What that 6-run unpinned sweep shows:

- `xnet-v2` TCP total avg `~58.3k msg/s`, CV `~6.9%`
- `xnet-v2` TLS total avg `~40.1k msg/s`, CV `~4.3%`
- `xnet-v2` UDP send avg `~1.37M pps`, CV `~1.8%`
- `xnet-v2` queue drain avg `~933.1ms`, CV `~55.8%`
- `xnet-v1` TCP is actually less stable than `xnet-v2` in the same sweep,
  with CV `~34.8%`

Interpretation:

- for Linux, the official baseline should now be documented as
  `policy: baseline-unpinned`
- affinity remains diagnostic-only:
  - `XBENCH_PIN_CPU` for process-level pin experiments
  - `XNET_COMPARE_TASKSET_CPUS` for runner-level taskset experiments
- the remaining instability in the default Linux baseline is now concentrated
  mostly in the queue-pressure lane and host scheduling noise, not in TCP/TLS
  throughput repeatability

Additional queue-pressure diagnostics:

- focused Debian reruns with the new diagnostic counters show
  `resume_to_first_recv_ns` stays in the tens of microseconds
- `pending_zero_to_full_recv_ns` is effectively negligible
- the visible spread is almost entirely the
  `resume_to_low_water_ns` / `resume_to_pending_zero_ns` interval

That means the remaining Linux variance is not coming from:

- delayed resume dispatch
- slow first server-side receive after resume
- lingering application-level bytes after the client send queue is empty

Instead, it is dominated by the post-resume kernel/transport drain interval
itself. At this point that looks much more like host scheduling / socket-buffer
behavior under the chosen synthetic pressure lane than a clear correctness bug
inside `xnet-v2`.
