---
num: 42
slug: time
title: Time and Time Zones
volume: 卷五 系统服务
type: practice
lead: The integer-microsecond mainline, calendar splitting with fixed-offset zones, and calendar-semantics addition — time handling without guesswork.
api: time, math
---

## Orientation

Time looks simple but hides the deepest pits: floating-point seconds losing precision, the daylight-saving labyrinth of time zones, and "add one month" meaning 30 days or calendar carry. XRT's time module answers with three designs: the **integer-microsecond mainline** (`xtime` is int64 microseconds — comparisons and differences with zero floating-point error, seamless with the timeout/deadline system), **fixed-offset time zones** (`xrtTimeSplitAt(+8×3600)` splits by offset — no daylight-saving complexity, covering most server scenarios), and **calendar-semantics arithmetic** (`xrtTimeAdd` carries by month/day units — August 31 + 1 month = September 30, not 30×86400 seconds added).

## Introduction

Three classic bugs. Bug one: storing time as double seconds — the `0.1 + 0.2` floating-point error (Chapter 10) magnifies on the timeline into "the timer occasionally fires a microsecond early or late", flipping randomly against deadline comparisons. Bug two: the server displays logs in "local time" — nodes deployed across zones produce mismatched timestamps, and the ordering of cross-node events cannot be reconstructed — distributed troubleshooting's first need dies right there. Bug three: a subscription's "add one month" approximated as 30 days — a January 31 renewal lands on March 2, and user complaints and the reconciliation department arrive together.

The antidotes, respectively: integer microseconds (the representation layer kills floating point), UTC storage + fixed-offset display (the storage layer kills timezone ambiguity), calendar carry (the arithmetic layer kills month-length ambiguity) — one discipline per layer, and the disciplines are more memorable than the APIs. Every API of the time module stands on these three disciplines — once the disciplines are understood, the API names are nearly self-explanatory; conversely, memorizing APIs without the disciplines leaves every accident in the pitfalls section waiting for you.

## Concepts

### Representation: integer microseconds

`xtime` is int64 microseconds (not double seconds, not a struct) — the reasons: **zero-error comparison** (timer and deadline verdicts are hot paths; boundary flips from floating-point comparison are unacceptable), **difference is subtraction** (measuring elapsed time `t1 - t0` directly yields microseconds), **ample span** (int64 microseconds cover about 290,000 years — from the Big Bang to the far future). `xrtNow()` fetches the current instant; `xrtClock()` fetches the monotonic clock (Chapter 10's clock sample uses it for elapsed-time measurement — the monotonic clock is immune to wall-clock jumps; always use it for timing).

### Splitting: UTC and fixed offsets

```diagram flow
- Storage and arithmetic: always UTC microseconds - one unambiguous timeline
- Display splitting: xrtTimeSplit by UTC; xrtTimeSplitAt(offset seconds) by the target zone
- Fixed offset: +8x3600 is UTC+8 - no DST tables, cross-platform consistent
- Human/machine division: machines exchange microsecond integers; decompose into human form only at the last moment
```

Fixed-offset zones are a **deliberate simplification** (in engineering terms, "trading completeness for explainability"): most server scenarios (log timestamps, report dates) need only a fixed offset; a full zone library (the IANA database, daylight-saving rules) buys its complexity a twice-yearly rule-update burden. When complex zones are needed, plug the host's timezone facilities in at the display layer — the storage layer stays UTC microseconds, unmoved.

### Calendar arithmetic: carry semantics

`xrtTimeAdd(时间, 数量, 单位, 出参)` (time, amount, unit, out-param) carries by calendar semantics: **month-end clamp** (August 31 + 1 month → September 30, not October 1), **leap-year correct** (February 28 + 1 year → February 28 next year, or 29 in a leap year — per the actual calendar of the year). Contrast the essential difference with "second-count approximation" (+30×86400 seconds): the approximation overcounts in 31-day months and undercounts in February, drifting all year — a calendar month's length is a **human convention**, not a physical quantity — subscription renewals, billing cycles, schedule reminders all need calendar semantics. Physical durations (timeouts, intervals, throttling) use microsecond addition — **never mix the two kinds of arithmetic**: calendar units for humans, microseconds for machines.

### Formatting and parsing (the display layer's last stop)

Converting between time and text (ISO 8601 as the primary format) shares ancestry with Chapter 26's strict semantics: parsing is all-or-nothing, formatting follows the declared layout. Chapter 35's template `{&path:%F}` time prefix uses exactly this formatting engine — display format declared at the template layer, the time value always a microsecond integer.

## Examples

### Complete program: the mainline in four steps

From the repository sample `examples/time/basic/main.c` — now, split, offset, and calendar addition in one pass:

```embed path="examples/time/basic/main.c" title="examples/time/basic/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/time/basic/main.c -lws2_32 -liphlpapi
unix_us=1788573693834602
utc=2026-09-05 02:01:33.834602
utc+8=2026-09-05 10:01:33
next_month=1791165693834602
```

**What just happened.** (1) `xrtNow()` returns integer microseconds — the first output line is the storage form (unreadable to humans but machines' favorite: comparable, subtractable, serialization-ready). (2) `xrtTimeSplit` splits into calendar fields by UTC and formats as `2026-09-05 02:01:33.834602` — microsecond precision preserved. (3) `xrtTimeSplitAt(+8×3600)` splits the same instant by UTC+8 — `10:01:33`: **the instant didn't change, the presentation did** — that is the whole of "UTC storage + offset display". (4) `next_month` is exactly one calendar month later — the microsecond difference is not a multiple of a fixed 30 days, empirical proof of month-end carry: calendar months vary in length, and addition follows the human calendar.

### Complete program: timing with the monotonic clock

From `examples/time/clock/main.c` — the correct posture for elapsed-time measurement:

```embed path="examples/time/clock/main.c" title="examples/time/clock/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/time/clock/main.c -lws2_32 -liphlpapi
elapsed_us=14695
elapsed_s=0.014727
```

**What just happened.** (1) `xrtClock()` is the monotonic clock — NTP corrections and users changing the system time don't affect it, and **timing always uses it** (`xrtNow` is polluted by clock jumps: an elapsed measurement spanning an NTP step can come out negative). (2) The microsecond difference comes from direct subtraction; converting to seconds is display-layer business (divide by 1e6 — Chapter 26's format strings can do it too). (3) Actual values float with scheduling (about 10~15 ms; the assertion floor is 10000µs), but monotonicity is guaranteed — exactly the property benchmarking (Chapter 23's controlled experiments) wants.

## Contracts

- **Integer microseconds**: storage, transmission, and arithmetic all in `xtime` microseconds; floating-point seconds never enter the storage layer.
- **UTC mainline**: machines exchange UTC microseconds; splitting for display takes an offset as needed; the offset is a display parameter, not a storage attribute.
- **Monotonic clock**: measure elapsed time with `xrtClock` (immune to corrections); `xrtNow` answers only "what time is it".
- **Calendar carry**: `xrtTimeAdd` clamps month-ends and handles leap years correctly; calendar units for humans, microseconds for machines, never mixed.
- **Formatting division**: machine format is the microsecond integer; human format is declared at the display layer (template prefix/format strings).

### From examples to engineering: three hosts of time

**Log timestamps**: `xrtNow()`'s microsecond integer goes straight into the log (Chapter 39's JSON Sink time field is exactly this form) — stored UTC, split by the viewer's offset when viewed (Pitfall 1's good form in full). **Timeouts and deadlines**: the `xrtDeadlineAfter(微秒)` (microseconds) family (a regular in Chapter 66's networking chapters) eats xtime — integer microseconds mate seamlessly with the deadline system, precisely the dividend of "integers at the representation layer". **Business cycles**: subscriptions, billing, and schedules all go through `xrtTimeAdd` calendar semantics — the month-end clamp rule goes into the product documentation ("a monthly plan started January 31 bills on February 28" is defined behavior, not a bug). The three hosts correspond to time's three identities: **instant** (what time is it), **duration** (how long has passed), **cycle** (when is next) — each identity has its correct tool, and identity precedes API.

### A mental model: time's three-layer view

Think of time handling as three layers, and most design decisions become obvious. **Storage layer**: the UTC microsecond integer — the only unambiguous representation; comparison, transmission, indexing all live here. **Arithmetic layer**: two tracks — physical quantities (microsecond addition/subtraction: timeouts, throttling, elapsed time) and calendar quantities (TimeAdd carry: cycles, schedules) — pick a track by need, never mix tracks. **Display layer**: splitting + formatting + zone offset — all "rendering parameters" that never flow back into storage. The price of violating any layer's discipline has an example in the pitfalls: mixed zones in storage (Pitfall 1), mixed tracks in arithmetic (Pitfall 2), premature splitting in display (logs storing local-time text). The three-layer view also explains why the time module's API surface is so small — each layer has only two or three actions; the layer disciplines absorb the complexity.

### The trick to testing time

Time-related code has a natural testing problem: `xrtNow` differs every call. The trick is **injecting the instant**: business functions take an xtime parameter instead of calling Now internally — tests pass fixed instants (boundary days like 2026-01-31 00:00:00 UTC), production passes Now from the caller. This is isomorphic with Chapter 11's "seed injection" for random numbers: all nondeterminism sources are injected at the boundary. Elapsed-measurement logic (retry backoff, throttle windows) likewise: accept an xtime parameter and advance a "virtual clock" manually in tests — the scheduler skeleton from Chapter 22's exercises uses exactly this trick.

## Pitfalls

### Pitfall 1: storing log times in the local zone (display parameters leaking into storage)

Symptom: multi-node log times don't match; after cross-zone deployment, "the same event" differs by hours; on daylight-saving switch days the logs show "a repeated hour".

Cause: the storage layer absorbed the display layer's zone — every machine's "local time" is a different display parameter.

```c bad
xtime Now = xrtNow();
xdatetime T;
xrtTimeSplitAt(Now, LocalOffset(), &T);   /* split by local offset, then store a string */
SaveLog("%04d-%02d-%02d ...", T.Year, ...); /* what's stored is "local-time text" - the zone is lost */
```

```c good
xtime Now = xrtNow();
SaveLog("%lld", (long long)Now);          /* store the UTC microsecond integer */
/* split only for display: xrtTimeSplitAt(offset per viewer) - the zone is a viewing parameter */
```

### Pitfall 2: approximating calendar arithmetic with second counts (mixing the two tracks)

Symptom: subscription/billing cycles drift at month ends — January 31 + 30 days = March 2; the user side shows "charged two extra days".

Cause: a calendar month is not a fixed second count — approximating "one month" with 30×86400 is wrong in both 31-day months and February.

```c bad
xtime Next = Now + 30LL * 86400 * 1000000;   /* "add a month" = 30-day approximation */
```

```c good
xtime Next;
xrtTimeAdd(Now, 1, XTIME_UNIT_MONTH, &Next);   /* calendar carry: Jan 31 + 1 month = Feb 28 */
```

## Exercises

### Basic: dual-zone contrast (one instant, two presentations)

Take the current instant; split and print it by UTC and by +8 offset — the two presentations share one microsecond value; then compute the hour difference to verify the offset.

### Advanced: a subscription renewal calculator (calendar semantics, visualized)

Implement `next_billing(当前, 周期月数)` (current, cycle months): calendar-addition carry; for a monthly user starting January 31, compute twelve consecutive billing dates, observe the month-end clamp, and print an explanation.

### Challenge: an elapsed-time benchmark tool (monotonic clock + median)

Implement `measure(函数, 次数)` (function, count) with the monotonic clock: warm up N times, then measure officially, take the median (Chapter 10's Near tolerance idea decides whether two measurements are "effectively identical"), and output both microsecond and human-readable forms. Use it to measure Chapter 23's array vs linked-list traversal (linked list vs array); the numbers should be the same order of magnitude as that chapter's experiments.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Representation | `xtime` = int64 microseconds; zero-error comparison, difference-is-subtraction, 290,000-year span |
| Two clocks | `xrtNow` wall clock (what time, affected by corrections) / `xrtClock` monotonic (timing only, never jumps back) |
| Splitting | `xrtTimeSplit` (UTC) / `SplitAt(偏移秒)` (offset seconds, target-zone display) |
| Calendar addition | `xrtTimeAdd` month-end clamp, leap years per the actual year; calendar units for humans, microseconds for machines |
| Storage discipline | UTC microsecond integers into storage; the zone is a viewing parameter, not a storage attribute - Pitfall 1's enforcement target |
| Formatting | ISO primary; the template `{&:fmt}` prefix rides the same engine |
| Three identities | instant (wall clock) / duration (monotonic) / cycle (calendar addition) - identity precedes API |
| Testing trick | inject the instant as a parameter, never call Now inside; advance a virtual clock - isomorphic to random seed injection |
