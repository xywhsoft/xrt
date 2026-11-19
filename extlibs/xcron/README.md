# xcron

Compile a Unix five-field or seconds-first six-field Cron expression and find
the next matching time. The calendar API works with UTC instants and floating
civil labels. A compiled expression is a caller-owned value. Reuse it for
matching and search without allocating on the normal calculation path.

The default `XCRON_MODULE_XCRON` selects expression and calendar. Explicitly
enable `XCRON_MODULE_XCRON_SCHEDULER` to register UTC jobs with an APP callback
and context. Drive it from the APP's loop with `xcronSchedulerPoll` or block the
APP's thread using `xcronSchedulerWait`. One instance supports both paths.

- [Calendar contracts](docs/calendar.md)
- [Scheduler contract](docs/scheduler.md)
- [Runnable APP examples](examples/README.md)
- [API reference](docs/api/reference.md)

Select `--suite xcron` for calendar packages or `--suite xcron_scheduler` for
packages with the scheduler. For single-header builds, include the selected
extension features, the XRT single header, and then the xcron single header with
`XCRON_IMPLEMENTATION` defined in one translation unit.
