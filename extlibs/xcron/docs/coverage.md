# xcron coverage

Expression and calendar retain their existing modular and single-header
regressions. Calendar tests cover civil and UTC matches, extreme timestamps,
impossible dates, leap years and resumable calculations.

The APP scheduler tests cover empty polls, timeout, cross-thread wake,
callback delivery, late occurrences, bounded admission and stale job handles.
The scheduler benchmark measures idle Poll throughput, process CPU usage
during blocking Wait and sampled wake lateness.

Use `xcron_tests` for modular and single-header regression and `xcron_examples`
for the runnable APP examples. The product root selects expression and calendar.
The scheduler is selected independently. Historical result JSON records the
earlier full-service implementation at its original source revisions.
