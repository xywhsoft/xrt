# Executor stop notification rendezvous

The Windows GCC O2 native single xcron service regression hung in
`testServiceRetries -> servicePoolEnd -> xrtExecutorDestroy -> xrtThreadWait`.
Its only worker remained in `SleepConditionVariableSRW` with Closed=1,
Shutdown=1, Sleepers=1, Queued=0 and Outstanding=0. The failed suite and attached,
then detached debugger captures are retained. The owned hung process was explicitly
stopped; that run is a failure, not a passing receipt.

The release Shutdown store and acquire Sleepers load do not form a handshake.
A publisher may see zero while a worker still sees the old stop predicate and
then parks. All lifecycle callers request a broadcast. Such broadcasts now always
acquire SleepLock, rendezvousing with the worker's predicate check and atomic
condition wait. No user callback is enclosed and no ownership lock order changes.
Scalar admission keeps its existing queue-publication fast path. Batch wake adds
an uncontended lock/broadcast/unlock when zero sleepers would previously skip it.
No API, allocation, thread, layout or global timer change is introduced.

Both modular and native single executor tests now run 1024 public-API creation,
admission, optional close/drain and physical destroy rounds, with one/two workers
and zero through three jobs. A separate driver thread and 30-second joining
watchdog bound a future Destroy regression. This stress is not a deterministic
reproducer: eight old-implementation runs passed. The original natural deadlock
and the legal publication/parking race are the evidence for this fix.

Windows and WSL executor/current-context/ownership/memory suites pass. WSL uses
O2 ASan/UBSan/leak-halt plus separate modular and native O1 TSan runs; the existing
compiler fence warning allowance remains. The original Windows native service
regression passes after rebuilding, followed by 16 bounded repetitions.

Five alternating unpinned Windows executor samples (131072 tasks, four workers)
give median after/before throughput ratios 0.9947 at batch1 and 0.9836 at batch32.
All samples are retained. This is a local diagnostic probe, not qualified
throughput, tail-latency, native Linux or production acceptance.

Portable receipts, hashes, limitations and raw throughput values:
[executor-stop-wake-20261105.json](../../tests/results/executor-stop-wake-20261105.json).
Local build commands, binaries and debugger logs are retained under
`out/service-extensions/executor-stop-wake/` and the calendar cursor stage's
`pre-executor-fix/` archive. The cursor API is a separate extension stage.
