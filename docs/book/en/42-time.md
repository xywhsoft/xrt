---
num: 42
slug: time
title: Time and Time Zones
volume: 卷五 系统服务
type: practice
lead: CE UTC milliseconds, monotonic-second timing, timezone splitting, and calendar arithmetic with explicit units and epochs.
api: time, math
---

## Orientation

This chapter puts event timestamps, elapsed-time measurements, and business calendars on one map. `xtime` stores integer UTC milliseconds from January 1 of year 1 CE; `xrtTimer` reads a monotonic clock in double seconds. The two representations serve persistent instants and measurements within the current process respectively. A timer reading cannot be interpreted as a calendar date, and a difference across a wall-clock correction cannot reliably decide a network timeout.

After this chapter, you should be able to display one instant at a fixed offset using `xrtTimeSplitAt`, handle month ends correctly using `xrtTimeAdd`, and explicitly convert the Unix epoch at protocol boundaries. Reliable time code needs a stated unit, epoch, and clock source, rather than an integer that merely looks plausible. The complete examples demonstrate calendar operations first and timing second. The contracts and testing methods then connect both paths to logs, retries, and billing.

## Introduction

Imagine a service upgrade where the database still contains milliseconds but the reader treats them as Unix milliseconds. The number looks plausible, yet the displayed date differs by almost two thousand years: an epoch mismatch. A second service stores the server's local date as text without an offset. Across zones, the same event then differs by hours: timezone information was lost. A third service implements a monthly renewal by adding thirty days, placing a January 31 bill on March 2 in a non-leap year: a business calendar was mistaken for a fixed duration.

All three failures concern interpretation. State that stored values are CE UTC milliseconds, use conversion functions when an external system expects Unix time, and apply offsets only for presentation. Business cycles use calendar units; physical waits use relative milliseconds; elapsed measurements use monotonic seconds. Naming parameters with these meanings helps reviewers find accidental mixing. An integer does not record its unit or clock source, and a type cast does not convert its epoch.

The same discipline applies to caches and networking. Cache expiry instants need an interpretable calendar time, while a connection's wait budget needs monotonic elapsed time. An administrator changing the clock should not extend a handshake, and a timezone change should not reorder persisted events. Identify whether the value is an instant, duration, or cycle before choosing a tool; that is usually more effective than repairing the display afterward.

## Concepts

### Representation: integer milliseconds

`xtime` is signed int64 milliseconds, with zero at January 1, year 1 CE, 00:00:00.000 UTC. One millisecond before zero belongs to the last day of year 1 BCE; there is no year zero. The integer range covers approximately 292 million years on either side using the proleptic Gregorian calendar. This is a representation range. Host timezone facilities and text protocols often support less; do not assume the operating system can handle every ancient date.

`xrtNow()` returns a wall-clock instant suitable for logs and database indexes. Unix starts in 1970; `XRT_TIME_UNIX_EPOCH` represents that instant on the CE millisecond axis. Convert at boundaries with `xrtTimeFromUnixMs` and `xrtTimeToUnixMs` and check the result. Equal integer types do not imply equal epochs. Raw subtraction of two instants is safe only when the difference is representable. For extreme ranges, use overflow-checked `xrtDateDiff` and state the result unit.

`xrtTimer()` returns high-resolution monotonic double seconds. Its origin has no calendar meaning and cannot support persisted comparisons between machines or processes. Subtract two readings for elapsed seconds; multiply by one thousand for milliseconds. Integer wait arguments and timer readings are not interchangeable. Calendar corrections do not affect this clock, but a failed read returns NaN. Reliable measurement code checks reading validity rather than treating failure as zero elapsed time.

### Splitting: UTC and fixed offsets

```diagram flow
- Storage: CE UTC milliseconds, with a stated epoch and unit
- Arithmetic: calendars for business cycles, monotonic seconds for elapsed time
- Splitting: xrtTimeSplit for UTC, xrtTimeSplitAt for an explicit offset
- Display: decompose last; never write local text back as the timeline
```

A fixed offset is positive east of UTC, measured in seconds, and ranges from minus 86399 to plus 86399. UTC+8 uses eight times 3600. This specifies a fixed offset, not automatic daylight-saving rules for a region. The resulting millisecond field ranges from zero to 999. To reconstruct the same instant, keep both calendar fields and the offset; do not treat already shifted fields as UTC again.

When host-local rules are required, enable the local-time module and use `xrtTimeLocal` and `xrtTimeFromLocal`. Spring transitions can create nonexistent dates; autumn transitions can produce two candidate instants. For the latter, explicitly reject ambiguity or choose the earlier or later candidate. Fixed offsets and local zones are distinct choices; a fixed offset cannot replace historical timezone rules. Fixed-offset tests are independent of deployment, whereas local-rule tests must control the timezone environment and host-supported range.

### Calendar arithmetic: carry semantics

`xrtTimeAdd` chooses arithmetic by unit: milliseconds through weeks are fixed durations; months, quarters, and years use calendar semantics. Month ends clamp to the target month's last day. January 31 plus a month therefore becomes February 29 in a leap year and February 28 otherwise. February 28 plus a year remains February 28 next year. Clamping occurs only when the original day exceeds the target month; a leap-year target does not invent an extra day.

A renewal policy must distinguish adding a month from the previous actual billing date from always using the originally agreed day. After February clamping, the first policy may continue on March 28; the second must retain its original anchor in business logic. The library supplies one addition, not a product's cycle policy. Similarly, a fixed day is twenty-four hours; across a local daylight-saving transition, the next day may have a different local clock time.

`xrtDateDiff` returns an int64 difference in the requested unit. Fixed units truncate toward zero, so twenty-three hours counted in days yields zero. Month differences use consecutive month indexes, so month-end to the next month's first day may yield one. Year differences skip year zero across BCE/CE. This is not a count of completed subscription periods. Check the bool result of fallible construction and calculation. Overflow preserves outputs; never read an uninitialized output parameter.

### Formatting and parsing (the display layer's last stop)

Text conversion belongs at protocol and presentation boundaries. In general formats, `%f` is three-digit milliseconds; `%F` and `%T` produce the date and clock time. RFC 3339 parsing accepts additional fractional digits but discards sub-millisecond tails. HTTP-date has second precision and uses GMT. Protocol conversion may change precision and supported years; formatting and reparsing is not lossless encoding for arbitrary values.

General CE notation rejects year zero, whereas a protocol's four-digit zero year may have a special mapping. Read the protocol function's contract rather than guessing numeric rules. Validate the entire input and check the parser's result. Owning formatted strings require `xrtFree`. Calendar structures and integer values have no dynamic ownership, but owning strings do require release. Put release in a cleanup path reachable from both failure and success so long-running logging and reporting do not accumulate leaks.

## Examples

### Complete program: the mainline in four steps

From the repository sample `examples/time/basic/main.c` — now, split, offset, and calendar addition in one pass:

```embed path="examples/time/basic/main.c" title="examples/time/basic/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/time/basic/main.c -lws2_32 -liphlpapi
utc_ms=63924170493834
utc=2026-09-05 02:01:33.834
utc+8=2026-09-05 10:01:33
next_month=63926762493834
```

**What just happened.** (1) `xrtNow()` returns CE UTC milliseconds. The `utc_ms` output labels the unit; the file format must still state the epoch. (2) `xrtTimeSplit` decomposes the date, using three digits for milliseconds, as in `2026-09-05 02:01:33.834`. (3) `xrtTimeSplitAt(+8×3600)` displays `10:01:33` for the same instant without adding eight hours to its absolute value. (4) `next_month` is the same day of the next calendar month. September to October happens to span thirty days here; this sample does not imply all months do. Dates vary at runtime; only the output layout is fixed.

### Complete program: timing with the monotonic clock

From `examples/time/clock/main.c` — the correct posture for elapsed-time measurement:

```embed path="examples/time/clock/main.c" title="examples/time/clock/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/time/clock/main.c -lws2_32 -liphlpapi
elapsed_s=0.014727000
```

**What just happened.** (1) `xrtTimer()` gives monotonic seconds; start and finish use the same clock. (2) `xrtSleep(10)` takes relative milliseconds and sleeps at least that duration. Scheduling may resume later, so the sample is not an assertion of exactly ten milliseconds. (3) The difference is printed directly in seconds; the value shown is one possible result. Functional tests check valid readings and a reasonable lower bound. Benchmarks need repeated samples, warmup, and statistics. The number of printed decimal places is not the accuracy of a performance conclusion.

## Contracts

- **Instants**: `xtime` is CE UTC milliseconds; state the epoch in storage and interface contracts. Local offsets belong to presentation.
- **Timing**: `xrtTimer` returns monotonic double seconds; check NaN. `xrtNow` returns a wall-clock instant subject to corrections.
- **Waiting**: public wait APIs take relative int64 milliseconds. Zero is nonblocking, `XRT_WAIT_FOREVER` means unlimited, and other negatives are invalid. Sleep does not accept the unlimited-wait flag.
- **Calendars**: months, quarters, and years in `xrtTimeAdd` use calendar arithmetic; other units are fixed durations. Check fallible results.
- **Ownership**: numbers and calendar structures are values; owning formatted strings require release. Callers synchronize concurrent access to writable outputs.

### From examples to engineering: three hosts of time

**Log timestamps**: CE milliseconds from `xrtNow()` can be persisted and displayed at the viewer's offset. Convert when sending to an existing Unix protocol rather than making receivers guess. Unit labels, epoch documentation, and timezone text complement each other: labels explain numbers to programs, offsets help people reconstruct the scene, and absolute values allow sorting. A JSON field named time does not prove both sides share a format.

**Timeouts and deadlines**: public waits take relative milliseconds; each operation uses a monotonic budget internally. Multi-step retries should share an overall budget, not restart the full wait at every step. If the application maintains its own total budget, compute remaining time from monotonic readings, round up to milliseconds, and handle infinity, NaN, range, and exhaustion. Do not use removed deadline types with current interfaces.

**Business cycles**: `xrtTimeAdd` performs one calendar operation. Original billing anchors, holiday adjustments, and local-clock policies belong to business logic. Define how January 31 renewals work in product rules, then test those rules rather than letting an incidental implementation determine the visible date. The three hosts correspond to event instants, elapsed durations, and calendar cycles. Give each explicit variable meanings and boundary tests.

### A mental model: time's three-layer view

The storage layer holds CE UTC milliseconds and documents version, unit, and epoch. The arithmetic layer chooses fixed durations or calendar units and checks overflow. The display layer selects UTC, a fixed offset, or host-local rules before formatting human-readable text. Separating these layers keeps the database stable when a report's timezone changes, keeps log timestamps stable when billing policy changes, and centralizes epoch review during an external protocol upgrade.

For example, a task that runs at nine every local morning cannot simply add twenty-four hours to its absolute instant each day. Determine the next local date and clock time under calendar rules, construct the absolute instant using local rules, and resolve gaps or repeated times according to product policy. Then use a monotonic budget for the execution wait, so clock corrections cannot arbitrarily extend an already started short wait. The flow uses all three layers while keeping each value's responsibility explicit.

### The trick to testing time

Time tests first isolate nondeterministic sources. Business functions accept an explicit `xtime` argument; tests supply fixed boundary dates, while production callers read `xrtNow`. Do not test only the current month. Include leap-year February, month ends, BCE/CE transitions, one millisecond before the Unix epoch, and target-range boundaries. Zone tests verify different presentations of one absolute value and reconstruction from offset-bearing fields. Local-rule tests include nonexistent and repeated clock times.

Retry, throttle, and timeout tests inject monotonic double-second readings and advance a virtual clock rather than putting calendar milliseconds into the same callback. Real sleep tests assert a lower bound with a finite tolerance; busy scheduling may wait longer. Asynchronous tests distinguish submission, send completion, and receive completion. Even after correcting timeout units, a wrong completion condition can leave unconsumed bytes that break later parsing. Define completion with states and received byte counts rather than pretending a fixed sleep proves it.

## Pitfalls

### Pitfall 1: storing log times in the local zone (display parameters leaking into storage)

Symptom: multi-node log times don't match; after cross-zone deployment, "the same event" differs by hours; on daylight-saving switch days the logs show "a repeated hour".

Cause: the storage layer absorbed the display layer's zone — every machine's "local time" is a different display parameter.

```c bad
xtime Now = xrtNow();
xdatetime T;
xrtTimeSplitAt(Now, LocalOffset(), &T);   /* split by local offset, then store a string */
SaveLog("%lld-%02d-%02d ...", (long long)T.Year, ...); /* what's stored is "local-time text" - the zone is lost */
```

```c good
xtime Now = xrtNow();
SaveLog("%lld", (long long)Now);          /* store the UTC millisecond integer */
/* split only for display: xrtTimeSplitAt(offset per viewer) - the zone is a viewing parameter */
```

### Pitfall 2: approximating calendar arithmetic with second counts (mixing the two tracks)

Symptom: subscription/billing cycles drift at month ends — January 31 + 30 days = March 2; the user side shows "charged two extra days".

Cause: a calendar month is not a fixed second count — approximating "one month" with 30×86400 is wrong in both 31-day months and February.

```c bad
xtime Next = Now + 30LL * 86400 * 1000;   /* "add a month" = 30-day approximation */
```

```c good
xtime Next;
xrtTimeAdd(Now, 1, XTIME_UNIT_MONTH, &Next);   /* calendar carry: Jan 31 + 1 month = Feb 28 */
```

## Exercises

### Basic: dual-zone contrast (one instant, two presentations)

Take the current instant; split and print it by UTC and by +8 offset — the two presentations share one millisecond value; then compute the hour difference to verify the offset.

### Advanced: a subscription renewal calculator (calendar semantics, visualized)

Implement `next_billing(当前, 周期月数)` (current, cycle months): calendar-addition carry; for a monthly user starting January 31, compute twelve consecutive billing dates, observe the month-end clamp, and print an explanation.

### Challenge: an elapsed-time benchmark tool (monotonic clock + median)

Implement `measure(函数, 次数)` (function, count) with the monotonic clock: warm up N times, then measure officially, take the median (Chapter 10's Near tolerance idea decides whether two measurements are "effectively identical"), and output both millisecond and human-readable forms. Use it to measure Chapter 23's array vs linked-list traversal (linked list vs array); the numbers should be the same order of magnitude as that chapter's experiments.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Representation | `xtime` is int64 UTC milliseconds from January 1, year 1 CE; no year zero |
| Two clocks | `xrtNow` records events; `xrtTimer` reads monotonic double seconds for elapsed differences |
| Unix boundary | `XRT_TIME_UNIX_EPOCH` identifies 1970; conversion functions check range |
| Splitting | `xrtTimeSplit` uses UTC; `xrtTimeSplitAt` uses seconds positive east of UTC |
| Calendar addition | `xrtTimeAdd` clamps month ends; a fixed day need not be the next local day |
| Difference | `xrtDateDiff` returns an integer in the requested unit, not completed subscription periods |
| Waiting | Relative milliseconds; `XRT_WAIT_FOREVER` is unlimited and zero is nonblocking |
| Text | Three-digit milliseconds; protocols may reduce precision; release owning strings |
| Testing | Fixed calendar boundaries, virtual monotonic clocks, explicit asynchronous completion |
