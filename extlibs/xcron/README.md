# xcron (developing)

Independent pure-C scheduling extension using public XRT APIs.
**Only bounded expression compilation is currently implemented.** Calendar
search, IANA zones/DST, live scheduling, executors, misfire/overlap and durable
recovery remain pending; production/performance acceptance is not claimed.

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
lists and star steps. Calendar matching will require both day masks when either
contains a star, otherwise either restricted day mask suffices. An explicit
numeric full range remains restricted. This follows the documented
[Cronie expression model](https://www.man7.org/linux/man-pages/man5/crontab.5.html),
with the six-field form explicitly defined by this library. Randomization,
`@reboot`, Quartz operators/year fields, crontab commands/environment assignments
and multiline input are rejected. A separate Quartz dialect can be added later.

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
compilation benchmark is workload evidence, not scheduler throughput or tail
latency acceptance. The [full goal](../../docs/development/service-extensions-plan.md)
and [progress ledger](../../docs/development/service-extensions-progress.md)
preserve the remaining delivery scope.
