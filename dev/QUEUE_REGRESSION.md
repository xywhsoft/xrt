# Queue Regression Notes

## Purpose

This note records the queue-related regression coverage kept after the
`xqueue` rollout was merged into the existing test system.

The goal is to keep queue behavior, wait-wrapper behavior, and `xnet`
integration easy to rerun without expanding XRT's default maintenance cost.


## Primary Scripts

- `build_GCC_TEST_x64.bat`
	- Main GCC test entry on Windows x64.
	- Queue-related coverage now comes from:
		- `preset:runtime_smoke`
		- `preset:xnet2_stage`
		- `singlehead/test_singlehead.c`

- `build_test.sh`
	- Main shell test entry.
	- Queue-related coverage now comes from:
		- `preset:runtime_smoke`
		- `preset:xnet2_stage`
		- `singlehead/test_singlehead.c`

- `singlehead/build_test.bat`
	- Local single-header convenience entry.
	- Builds and runs `test_singlehead.c`.

- `build_GCC_DLL_x64.bat`
	- GCC DLL build entry on Windows x64.
	- Produces `release/x64/xrt.dll`.

- `build_TCC_DLL_x64.bat`
	- TCC DLL build entry on Windows x64.
	- Produces `release/x64/xrt.dll`.
	- Keeps the current `-DXRT_NO_COROUTINE` workaround.


## Guarded Behaviors

### Queue API

`preset:runtime_smoke` includes `queue_core` and `thread_core`, and together
they lock down these contracts for `SPSC`, `MPSC`, and `MPMC`:

- zero-capacity and oversized-capacity creation must fail
- `NULL` queue inputs must return `XQUEUE_ERROR` or a zero/false result
- `NULL` drain callbacks must not consume queued items
- closing an empty queue must immediately make it drained
- drained closed queues must reset cleanly
- basic threaded producer/consumer paths must remain intact
- LP64 builds must not widen 32-bit queue/value atomics into adjacent-field
  corruption
- shared `__xrtAtomicLoadU32` / `__xrtAtomicStoreU32` /
  `__xrtAtomicCompareExchangeU32` helpers must stay on `uint32` fields;
  long-based atomics remain for `volatile long` state only
- the first batch cut (`xrtMPSCQPushBatch/PopBatch`, `xrtMPMCQPushBatch/PopBatch`)
  must preserve:
	- partial-success return counts
	- FIFO pop order
	- `0` on closed/full/empty boundaries without inventing a new result code
- the first wait-wrapper cut (`xmpscqwait`) must preserve:
	- empty timeout behavior
	- push-to-pop wakeups
	- close-to-pop wakeups
	- close-after-last-pop wakeups for late waiters

### XNet Integration

`preset:xnet2_stage` includes `xnet2_engine`, which now guards:

- bounded worker command queues returning `XRT_NET_AGAIN` on pressure
- delayed posts also returning `XRT_NET_AGAIN` when the command queue is full
- failed delayed posts not executing later by accident
- worker command-node freelist warm/reuse/return behavior

### Single-Header Surface

The appended `singlehead/test_singlehead.c` smoke guards:

- single-header generation staying buildable
- queue APIs matching the normal-header surface
- queue/runtime integration staying intact after regeneration

The current Linux regression lane also matters for a second reason:
the first Debian 13 queue benchmark run exposed a `uint32* -> long*` atomic
width bug that only manifested on 64-bit Linux. The direct atomic-width guards
now keep that class of regression from hiding behind later benchmark crashes.
Those guards cover both the shared `uint32` helper layer in `xrt.h` and the
`xvalue` CAS wrapper that sits on top of it.


## Update Rule

If queue-related code intentionally changes behavior:

1. Run `build_GCC_TEST_x64.bat`.
2. If the change is platform-sensitive, also run `build_test.sh`.
3. If the change only touches single-header generation flow, `singlehead/build_test.bat`
   remains the fastest local convenience entry.


## Out Of Scope

These scripts do not replace:

- full repository regression runs
- performance benchmarking
- release-candidate wide validation
- warning-clean builds across the whole tree
