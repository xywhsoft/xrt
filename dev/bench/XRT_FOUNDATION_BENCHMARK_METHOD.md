# XRT Foundation Benchmark Methodology

## Purpose

This document defines the benchmark methodology used by the finalized XRT
foundation work.

It exists to keep benchmark claims repeatable across:

- thread/runtime work
- coroutine runtime work
- xnet transport work
- builtin TLS integration
- HTTP and WebSocket modules that sit on the same transport core


## General Rules

1. Benchmark binaries must be built with optimized settings.
2. Smoke runs and baseline runs must be labeled separately.
3. Setup cost and steady-state message-loop cost must not be conflated unless a
   benchmark explicitly says it is an end-to-end total-path measurement.
4. Latency tables must call out the percentile definition they use.
5. Old archived stacks may be used for comparison, but they are not production
   guidance and must be called out as archive-only baselines.
6. Any benchmark that is known to be noisy must record the environment policy
   that was used to judge it.


## Build Policy

- Windows benchmark runners compile with `gcc -O2`.
- Linux benchmark runners compile with `gcc -O2`.
- Local smoke binaries are acceptable for quick validation, but benchmark
  reports should cite the runner scripts used to build and execute the matrix.


## Benchmark Classes

### Coroutine

Primary report:

- `dev/bench/COROUTINE_BENCH_20260314.md`

Primary runners:

- `dev/bench/run_coroutine_bench_windows.ps1`
- `dev/bench/run_coroutine_bench_linux.sh`

Current scope:

- context switch
- create/destroy
- create/resume/destroy
- timer churn
- cross-thread scheduler post wake


### Transport Compare

Primary reports:

- `dev/bench/XNET_COMPARE_20260314.md`
- `dev/bench/XNET_COMPARE_SWEEP_20260314.md`

Primary runners:

- `dev/bench/run_xnet_compare_windows.ps1`
- `dev/bench/run_xnet_compare_linux.sh`
- `dev/bench/run_xnet_compare_sweep_windows.ps1`
- `dev/bench/run_xnet_compare_sweep_linux.sh`

Current scope:

- TCP echo
- TLS echo
- UDP burst
- queue pressure

Interpretation rules:

- `total` and `steady-state` are both message-loop metrics for the modern xnet
  benches.
- setup/connect/handshake time must be reported separately when the benchmark
  exposes it.
- legacy `xnet-v1` figures are archive-only comparison points.


### TLS-Specific Validation

Primary report:

- `dev/bench/TLS_BENCH_20260315.md`

Primary bench/test entrypoints:

- `dev/bench/xnet2/bench_tls_handshake.c`
- `dev/bench/xnet2/bench_tls_resume.c`
- `dev/test_tls_boundary_core.c`

Current scope:

- repeated real TLS handshake latency and throughput over xnet
- TLS 1.2 session-resumed reconnect latency and throughput over xnet
- fragmented handshake/data/close boundary stress without socket I/O

Current note:

- the current resume lane measures mainline TLS 1.2 session-id resumption
- if a future mainline ticket-resume path is added, it should be reported as a
  separate lane instead of silently merging into the current resume baseline


### Queue Throughput

Primary report:

- `dev/bench/QUEUE_BENCH_20260324.md`
- `dev/bench/QUEUE_BATCH_BENCH_20260325.md`
- `dev/bench/QUEUE_BATCH_SWEEP_20260325.md`
- `dev/bench/QUEUE_BENCH_SWEEP_20260325.md`
- `dev/bench/QUEUE_LATENCY_20260325.md`
- `dev/bench/QUEUE_LATENCY_SWEEP_20260325.md`
- `dev/bench/QUEUE_LATENCY_PINNED_WINDOWS_20260325.md`

Primary bench/test entrypoints:

- `dev/bench/queue/bench_queue_pointer.c`
- `dev/bench/queue/bench_queue_latency.c`
- `dev/bench/run_queue_bench_windows.ps1`
- `dev/bench/run_queue_bench_linux.sh`
- `dev/bench/run_queue_latency_windows.ps1`
- `dev/bench/run_queue_latency_pinned_windows.ps1`
- `dev/bench/run_queue_latency_linux.sh`
- `dev/bench/run_queue_latency_sweep_windows.ps1`
- `dev/bench/run_queue_latency_sweep_linux.sh`
- `dev/bench/run_queue_batch_sweep_windows.ps1`
- `dev/bench/run_queue_batch_sweep_linux.sh`
- `dev/bench/run_queue_bench_sweep_windows.ps1`
- `dev/bench/run_queue_bench_sweep_linux.sh`

Current scope:

- `SPSC` pointer-queue throughput
- `MPSC` pointer-queue throughput
- `MPMC` pointer-queue throughput
- `MPSC` single-item vs batch compare
- `MPMC` single-item vs batch compare
- `MPSC` batch-size sweep under the default matrix
- `MPMC` batch-size sweep under the default matrix
- `MPSC` single-item vs batch latency compare
- `MPMC` single-item vs batch latency compare
- `MPMC` consumer fairness compare under the default matrix
- `MPSC` batch-size latency sweep under the default matrix
- `MPMC` batch-size latency/fairness sweep under the default matrix
- Windows diagnostic pinned latency sweep to separate queue effects from host scheduling noise

Current note:

- the current queue baseline is throughput-oriented and currently has Windows
  and Linux baseline runs under the same default matrix
- the current batch compare now measures the implemented batch-aware fast path
  under the same default matrix; it is no longer just API-shape accounting
- the current batch sweep exists to identify useful `batch_size` regions after
  the fast path work; it does not freeze latency-safe defaults
- the current latency compare is the first explicit check that batch throughput
  gains are not being misread as universal latency/fairness wins
- the current latency sweep extends that work across `batch_size` so we can
  talk about acceptable regions instead of only comparing `batch=32`
- the current pinned Windows follow-up is explicitly diagnostic; it explains
  odd unpinned shapes, but it is not the baseline policy report
- the current queue sweep is still throughput-only; it broadens the matrix
  across capacity and producer/consumer counts, but it is not a latency report
- queue sweep summaries keep the latest sample for each matrix key when the
  sweep directory contains older logs
- queue sweep summary keys now include `policy` and `cpu_pin`, so diagnostic
  pinned runs can coexist with baseline logs without silently replacing them
- queue runners also honor `XBENCH_PIN_CPU` as a diagnostic lane, but pinned
  results must be labeled separately because they can change fairness behavior
  dramatically under contended queue benches
- if future queue reports change policy, platform, or matrix, they should
  explicitly cite the runner and the matrix used


## Environment Policy

### Windows

- default compare policy is unpinned
- optimized builds are mandatory
- repeated sweep output is preferred over one-off claims
- report-grade queue compares should be collected serially on a host, even if
  the runners themselves can now coexist safely

### Linux

- official compare policy is `baseline-unpinned`
- affinity pinning remains diagnostic, not the official baseline policy
- repeated sweep output must record the policy and whether it was pinned
- report-grade queue compares should be collected serially on a host, even if
  the runners themselves can now coexist safely


## Reporting Rules

Every benchmark report should include:

- the matrix
- the runner or build path used
- the environment policy
- the raw metrics that matter for the lane
- a short interpretation section
- any known caveat that affects how the numbers should be read


## Current Source of Truth

The current benchmark/report set for the finalized foundation is:

- `dev/bench/COROUTINE_BENCH_20260314.md`
- `dev/bench/XNET_COMPARE_20260314.md`
- `dev/bench/XNET_COMPARE_SWEEP_20260314.md`
- `dev/bench/TLS_BENCH_20260315.md`
- `dev/bench/QUEUE_BENCH_20260324.md`
- `dev/bench/QUEUE_BATCH_BENCH_20260325.md`
- `dev/bench/QUEUE_BATCH_SWEEP_20260325.md`
- `dev/bench/QUEUE_BENCH_SWEEP_20260325.md`
- `dev/bench/QUEUE_LATENCY_20260325.md`
- `dev/bench/QUEUE_LATENCY_SWEEP_20260325.md`
- `dev/bench/QUEUE_LATENCY_PINNED_WINDOWS_20260325.md`

If a future report supersedes one of these, it should say so explicitly instead
of silently replacing the methodology.
