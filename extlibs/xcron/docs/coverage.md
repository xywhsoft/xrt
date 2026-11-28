# xcron coverage

Expression and calendar retain their existing modular and single-header
regressions. Calendar tests cover civil and UTC matches, extreme timestamps,
impossible dates, leap years and resumable calculations.

The APP scheduler tests cover empty polls, timeout, cross-thread wake,
callback delivery, late occurrences, bounded admission and stale job handles.
The scripted clock test covers explicit Wake after a forward step, a backward
step during a timed wait, the wall sample shared by a callback wave, clock
failure and recovery, and retirement at the last representable occurrence.
It also injects failures during lazy waiter initialization and a subsequent
wait. A native wake survives initialization failure, and a retry delivers
the original occurrence once. The fixture scripts absolute deadline waits
and clock observations while retaining native event and waiter lifetimes. The modular and
single-header scheduler tests also drive the native blocking path.

The scheduler benchmark measures idle Poll throughput, process CPU usage
during blocking Wait and sampled wake lateness.

Use `xcron_tests` for modular and single-header regression and `xcron_examples`
for the runnable APP examples. The product root selects expression and calendar.
The scheduler is selected independently. Historical result JSON records the
earlier full-service implementation at its original source revisions.

Run only the scripted clock test from the repository root with the command below.

    python tools/build.py --manifest extlibs/xcron/config/modules.json --suite xcron_tests --single-test test_single_xcron_scheduler_clock
