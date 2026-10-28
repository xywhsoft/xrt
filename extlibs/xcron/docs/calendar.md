# Gregorian calendar calculation

`XCRON_MODULE_XCRON_CALENDAR` selects expression compilation and public XRT time
APIs. It implements exact UTC/civil matching and earliest recurrence search for
the explicit Unix5/seconds6 dialects. It does not select system-local time,
time-text parsing, IANA zones, scheduling, waiting, callbacks or storage. The
product root remains developing until the original full scope is delivered.

The day rule follows the [Cronie manual](https://man7.org/linux/man-pages/man5/crontab.5.html):
month/hour/minute/second must match; if either day field contains a star, both day
masks must match, otherwise either day mask is enough. Compiler star provenance
is retained. Numeric full ranges stay restricted. A star step and a numeric range
step can consequently give different DOM/DOW results, even with equal masks.
Calendar arithmetic uses XRT's proleptic Gregorian calendar with no civil year
zero, including BCE; it does not switch to Julian dates before 1582. Leap seconds
are not expression or XRT time fields.

## Match and next

`xcronCalendarMatch` tests an exact UTC `xtime` millisecond instant. All selected
fields must match and Millisecond must be zero. Unix5 additionally has second
zero. Match at a fractional instant returns success with `Matches=false`; it does
not floor the instant or repeatedly match an entire minute.

`xcronCalendarNext` returns the earliest whole-second match in `[Begin,End]`.
`XCRON_SEARCH_EXCLUSIVE` changes this to `(Begin,End]`. Fractional Begin rounds
upward, including negative timestamps; End is always inclusive. Reverse windows
are errors. FOUND writes Next, while normal NONE and ERROR preserve it. The full
int64 `xtime` range is accepted, including its partial boundary days. No invalid
midnight is constructed at an endpoint.

The Civil counterparts operate on floating labels, independently of UTC offset
or zone. Only Year/Month/Day/Hour/Minute/Second/Millisecond are inputs. Offset,
Weekday, YearDay and IsDST are ignored and Gregorian metadata is recomputed.
All nonzero int64 civil years are supported, even outside timestamp range.
Outputs have Offset=0, IsDST=-1 and canonical weekday/year-day. Gregorian metadata
is derived through public XRT conversion in an equivalent safe year of the cycle;
calendar iteration uses public month lengths. No private XRT implementation is
accessed. UTC wrappers convert only candidates known to lie within their bounds.

Every call independently validates all expression masks, nonempty fields,
allowed flag bits, dialect and Unix5 second-zero invariant. Bitmaps use natural
value positions and Sunday bit zero; an extra weekday bit 7 is invalid here.
Callers keep expressions and input objects immutable during calls. Successful
compilation is not used as a reason to trust subsequently corrupted values.

## Complete bounded search

Search does not scan elapsed seconds or minutes. It visits calendar months,
forms valid date masks using DOM and rotated/repeated weekday bits, selects the
next date bit, then advances hour/minute/second masks lexicographically. Missing
February 30 or April 31 are excluded before time construction. A portable binary
bit scan has bounded work and no private runtime intrinsics.

These dialects have no year field. A Gregorian 400-year cycle has 146097 days,
divisible by seven, so the entire date/weekday/time predicate repeats. See the
[US Naval Observatory calendar reference](https://aa.usno.navy.mil/downloads/c15_usb_online.pdf)
and [leap-year rule](https://aa.usno.navy.mil/faq/leap_years). Search examines at most
4801 months, from the starting partial month through the same month 400 years
later. This proves no recurrence when the window is longer; it
is not an arbitrary lookahead cutoff. An earlier End or the last int64 civil
year stops the search sooner. `MonthsExamined` includes skipped months and is
bounded by `XCRON_CALENDAR_MAX_MONTHS`. Dense schedules can match the rounded
start directly. No unbounded allocation, recursion or astronomical-year arithmetic
overflow is needed.

## Ownership, errors and costs

The four APIs are synchronous, read-only over inputs and retain nothing. All
storage is caller-owned or stack-local; separate outputs permit concurrent use
of one immutable expression. Outputs/Issue must be mutually disjoint and cannot
overlap the expression or any civil input. Actual readable/writable objects are
required. Bad pointers/spans/flags/aliases leave all outputs unchanged. Well-
formed calls with bad expression/date/window values publish typed Issue even
when allocating the cold `xrt.cron` diagnostic fails. Normal Match/FOUND/NONE
publish OK, allocate nothing and preserve ambient error. No result requires a
destructor. Scratch/callback/worker ownership is not part of this leaf.

Costs remain explicit: field/flag validation, range/alias checks, fixed date
copies, public XRT date conversion and month-length calls, bitmap operations,
bounded month iteration and optional work-count writes. UTC uses full-domain
split/construction; civil canonicalization uses a safe equivalent year. Separate
Match/Next calls repeat validation. Cold diagnostics can allocate and fall back
to XERR_MEMORY. Zero successful allocations do not imply zero CPU/bridge overhead
or scheduler throughput/tail-latency acceptance.

## Running and later timezone work

From the repository root:

```sh
python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron_tests --cflag=-O2
python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron --no-single --cflag=-O2
python tools/measure_performance.py --config extlibs/xcron/config/performance_profiles.json --smoke
```

`examples/calendar/main.c` deterministically finds Monday 2024-03-04 09:00 UTC
after a Friday fractional firing and proves an impossible February 30 search
stops at 4801 months. It needs no service, clock or cleanup. Tests use a complete
400-year independent calendar/weekday oracle, brute earliest-time searches,
int64/BCE endpoints, atomicity and allocation/OOM checks. The workload measures
only dense/sparse/impossible local calendar search over precompiled expressions.

IANA/DST remains required. A timezone adapter must search ordered UTC transition
segments with fixed offsets and map civil results with explicit gap/fold policy.
Simply searching forward from the current local label misses a later fold whose
label has moved backwards. This leaf does not choose a DST policy or treat civil
labels as monotonic elapsed time. Misfires, overlap, execution, clock jumps and
durable recovery remain separate scheduler responsibilities. Current evidence is
in the [progress ledger](../../../docs/development/service-extensions-progress.md).
