# xcron (developing)

Independent pure-C scheduling extension using public XRT APIs.
**Bounded expression compilation and UTC/civil Gregorian calendar search are
implemented.** An explicit POSIX/TZif-footer rule core now supports DST state,
transitions and gap/fold resolution; see [timezone contracts](docs/timezone.md).
Owned non-leap TZif v1..4 binary history loading is implemented, including
unknown/truncated coverage and more than two fold candidates. Named database
lookup/reload, leap-aware conversion and durable
recovery remain pending; production/performance acceptance is not claimed.

`XCRON_MODULE_XCRON_SCHEDULER` adds a bounded explicit-clock pull kernel with
catch-up/skip/coalesced misfires, wait/allow/skip overlap, fair work budgets,
generation IDs and physical run leases. Cancellation/close preserve active data
until completion and a retirement receipt. The kernel creates no threads/timers.
See [scheduler contracts and example](docs/scheduler.md).

`XCRON_MODULE_XCRON_EXECUTOR` dispatches actual leases through a borrowed XRT
Executor, with preallocated slots, cooperative cancellation, owned worker-error
receipts and protected publication/notification tails. The owner drains results,
completes kernel leases and reaps jobs before releasing Data. See
[execution contracts and runnable example](docs/executor.md).

`XCRON_MODULE_XCRON_SERVICE` supplies real wall/monotonic driving on a dedicated
public-XRT owner thread and borrowed pool. Bounded asynchronous commands/replies,
reserved completion output, no-effect overload retries, self-wait protection,
controlled clock-failure shutdown and explicit post-stop Data retirement are
implemented. See [live contracts/costs](docs/service.md) and
[runnable real-UTC example](examples/service/README.md). Nominal DST delivery,
named database/reload, durable recovery and final native/performance/long-run
qualification remain pending. Actual owner admission/advancement now share global
segment/month/rule-year budgets, with provisional ADD rollback and independent
command progress. See the live guide for costs, stats ABI and remaining cold scans.

`XCRON_MODULE_XCRON_ZONED` adds exact local-field matching and earliest actual
occurrence in a UTC window. It searches ordered fixed-offset segments, returns
every actual fold instance and skips nonexistent labels; typed unknown coverage
prevents guessed results. History is completed before using a periodic suffix's
400-year completeness bound. See [zoned contracts/examples/costs](docs/zoned.md).
Nominal gap compensation, fold selection and execution remain scheduler policies.

Zoned Init/Step cursors bound segment starts, calendar months and transition-rule
years independently. They borrow an explicitly supplied live zone and copy the
expression/window into stable caller storage. See [budget/lifetime/cost details](docs/zoned.md).
The live owner uses the shared typed engine with global per-turn budgets.

Select `XCRON_MODULE_XCRON_EXPRESSION` before including `<xcron.h>`. Explicitly
choose `XCRON_UNIX5` (minute/hour/day/month/weekday, second zero) or
`XCRON_SECONDS6` (second followed by the same five fields). Compile into a
caller-owned `xcronexpression` value. It contains immutable field bitmaps and
day-star flags, borrows nothing and can be copied/shared between threads.

Supported syntax: decimal values, ascending inclusive ranges, comma lists,
range/star steps anchored at the range start, JAN..DEC and SUN..SAT names
(ASCII case insensitive), Sunday 0/7, space/tab separators, and calendar aliases
`@yearly`, `@annually`, `@monthly`, `@weekly`, `@daily`, `@midnight`, `@hourly`.
No field-count auto-detection occurs. Single-value steps are rejected; use an
explicit range. Reversed/wrapping ranges are rejected; write weekday `5-7`
for Friday through Sunday. Steps stay within a field, never mean elapsed time.

Day flags preserve whether each day field contains a star, including mixed
lists and star steps. Calendar matching requires both day masks when either
contains a star, otherwise either restricted day mask suffices. An explicit
numeric full range remains restricted. This follows the documented
[Cronie expression model](https://www.man7.org/linux/man-pages/man5/crontab.5.html),
with the six-field form explicitly defined by this library. Randomization,
`@reboot`, Quartz operators/year fields, crontab commands/environment assignments
and multiline input are rejected. A separate Quartz dialect can be added later.

Select `XCRON_MODULE_XCRON_CALENDAR` for exact matching and earliest recurrence in
an explicit inclusive/exclusive interval. UTC APIs cover the full int64 millisecond
range. Civil APIs accept floating Gregorian labels over all nonzero int64 years;
offset/DST/derived input metadata is ignored and recomputed. Candidates occur at
whole seconds (second zero for Unix5). Search skips date/time bitmaps and visits
at most 4801 months to cover a complete 400-year recurrence cycle. NONE preserves
outputs and ambient error; it is not an arbitrary lookahead failure. Normal
matching/search allocates nothing and retains no input. See [calendar contracts,
costs and examples](docs/calendar.md), including the remaining zone-transition
segment/gap/fold work. Root `XCRON_MODULE_XCRON` selects all current leaves.

UTC and civil `xcronCalendarCursorInit`/`Step` counterparts add resumable
single-result queries with a positive per-call month budget. A 256-byte stable
caller context snapshots the input; PENDING yields without changing the result.
Obvious impossible month/day conjunctions are proven in constant work. The
[calendar guide](docs/calendar.md) covers copies, handoff, costs and limits.
Zone traversal and live scheduler work still need incremental integration.

The input is bounded at 4096 bytes. Success allocates nothing and preserves the
ambient XRT error. Failure preserves the compiled output. An optional local
issue records the field and byte offset, even if allocating the `xrt.cron`
diagnostic fails and XRT falls back to XERR_MEMORY. API pointer/span/dialect and
alias errors leave both outputs intact. See [coverage and contracts](docs/coverage.md),
[public reference](docs/api/reference.md) and [example](examples/expression/main.c).

```sh
python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron_tests --cflag=-O2
python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron --no-single --cflag=-O2
python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron --trim-only
python tools/package.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron --kind static --verify
python tools/package.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron --kind shared --verify
python tools/measure_performance.py --config extlibs/xcron/config/performance_profiles.json --smoke
```

The product root stays developing even if current asset audits pass. An unpinned
compilation/calendar benchmark is workload evidence, not scheduler throughput or tail
latency acceptance. The [full goal](../../docs/development/service-extensions-plan.md)
and [progress ledger](../../docs/development/service-extensions-progress.md)
preserve the remaining delivery scope.
