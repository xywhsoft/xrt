# Zoned actual-occurrence calculation (developing)

`XCRON_MODULE_XCRON_ZONED` selects the calendar and timezone leaves. The two
read-only synchronous APIs match an actual instant and find the earliest actual
occurrence in an explicit UTC window. They retain neither the expression nor
zone. The caller holds an existing live zone reference for the entire call and
keeps both inputs immutable. No clock, waiting, executor, callback or store is
involved. Root and zoned modules remain developing while scheduler policies and
the original full delivery scope are pending.

## Time semantics

Every selected field is evaluated on the zone's local Gregorian label. Missing
labels have no actual occurrence, and all repeated instants are eligible,
including multi-fold histories with more than two instances. This actual-time
predicate corresponds to the matching model described in the upstream
[Cronie crontab manual](https://github.com/cronie-crond/cronie/blob/master/man/crontab.5).
The separate [delivery leaf](delivery.md) can select all/first/last or reject a
single nominal label after complete resolution. It does not change this ordered
actual-occurrence search. Nominal-time gap compensation, recurring fold selection and delivery after
clock jumps/misfires remain explicit scheduler policy work; this primitive
neither invents shifted labels nor implements execution.

Match is exact at millisecond precision: a nonzero fraction is false, including
in an unknown interval, since legal offsets are whole seconds. Next searches
`[Begin,End]`, or `(Begin,End]` with `XCRON_SEARCH_EXCLUSIVE`, and returns only
whole-second instants. Negative/fractional bounds and both int64 XRT endpoints
are supported. End is inclusive. Unix5's second zero applies locally: an offset
of +17 seconds gives local-minute occurrences at UTC second43, not UTC minute
boundaries. XRT retains its 0001 epoch; use `XRT_TIME_UNIX_EPOCH` for Unix data.

FOUND alone writes Next; NONE and ERROR preserve it. Reversed windows are errors.
Empty rounded windows return NONE without needing a mapping. Match changes its
output only on success. All output spans and optional Issue must be mutually
disjoint and avoid the expression and every owned zone/footer/name allocation.
Invalid API arguments leave both outputs unchanged. Value failures preserve the
typed Issue even if diagnostic allocation fails. Normal success/FOUND/NONE
allocate nothing and preserve ambient error. Error domain is `xrt.cron.zoned`;
Field describes bad compiled masks and Zone describes mapping failures.

## Ordered UTC search and completeness

Local labels do not order instants across a rollback. Starting after New York's
first 01:30 can still find the later 01:30; advancing labels from a current 01:45
would miss it. Search instead visits UTC segments with constant state. Each
segment maps monotonically into civil labels, so its first calendar match is
its earliest UTC candidate. Checking segments in UTC order proves the global
earliest result. Gap labels appear in no segment; repeated labels occur in
multiple segments. No global Resolve call or two-fold assumption is needed.
In particular, a bounded known window can be searched even when a global label
resolution cannot prove a complete list because other coverage is unspecified.

Each segment reuses validated calendar bitmap search. A match at the rounded
start returns immediately, avoiding an unnecessary next-transition/end-date
calculation. Other paths end the segment immediately before the next effective
transition, and safely map results back with the fixed offset. Whole-range
offset arithmetic uses the existing public-XRT-based Gregorian decomposition.
Unknown coverage encountered before proving a result is a typed ZONE/UNKNOWN
error. Unknown intervals are never silently skipped or extrapolated.

Only the proven periodic suffix permits a global recurrence bound: a POSIX zone
is periodic everywhere, a fixed no-table zone is constant, and a TZif zone with
history becomes periodic when its footer starts. The history is examined first,
even if it exceeds 400 years. A complete Gregorian cycle is 146097 days, divisible
by seven, and both the year-free expression and footer repeat over that cycle.
Searching one whole cycle therefore proves NONE in that suffix. A whole
constant-offset civil cycle with no label can also prove global impossibility.
The [Gregorian rule](https://aa.usno.navy.mil/faq/leap_years) and
[TZif footer definition](https://www.rfc-editor.org/rfc/rfc9636.html#section-3.3)
support this bound; it is not a policy cutoff from Begin.

Tests include 400 years of annual missing labels proving NONE, and a 500-year
history of missing labels followed by a fixed footer whose first occurrence is
in year2500. The latter would fail under a global Begin+400 cutoff. History has
at most the configured transition count; a periodic suffix has at most800 state
changes per cycle. SegmentsExamined is consequently bounded and uses uint32;
MonthsExamined accumulates calendar work in uint64. The public issue occupies
24 bytes on the verified x64 targets.

## Costs, examples and evidence

Normal calls use validation, fixed stack values, immutable zone reads, bitmap
search and public XRT calendar arithmetic. They perform no parsing, retention,
copies of timezone data, heap allocation or shared synchronization. Inputs still
require a live owner; this is not permission to race a final release. Product-
private calendar/zone operations reuse validated state without introducing a
new shared runtime or accessing private XRT implementation. Invalid zone-output
checks now publish one diagnostic through their API boundary, avoiding a nested
duplicate diagnostic. Public layouts/ownership of existing APIs are preserved.

The [offline example](../examples/zoned/README.md) proves later-fold ordering and
missing-time NONE/cleanup. Tests cover golden DST/negative/half-hour/triple folds,
unknown intervals, second-level offsets, full-range UTC specialization, brute
search, endpoints, aliases, diagnostic OOM, warm allocation/error counters and
concurrent retained owners. The independent IANA oracle enumerates local labels
with Python zoneinfo, maps both folds, then sorts UTC candidates; it does not use
the library matcher or its segment algorithm. Frozen version/hashes are checked.

Separate dense/fold/full-cycle smoke profiles measure precompiled queries, with
an isolated-size profile excluding ambient local-time/network/thread symbols.
They do not establish production throughput, P99, cold costs, native Linux or
long-duration acceptance. Explicit [database reload](tzdb.md), the
[scheduler](scheduler.md) and [executor](executor.md) are available. Leap-aware
conversion, nominal policies, durability and full product qualification remain.

## Resumable ordered search

`xcronZonedCursorInit` snapshots the expression/window and records the identity of
an existing zone. It rounds the start and derives the periodic suffix bound, but
performs no segment, calendar or transition-year traversal. The caller holds the
same immutable zone alive and passes it to every `xcronZonedCursorStep`, including
terminal reads and serialized handoff to another thread. The cursor does not
retain the zone, copy its data or require Unit. Its512-byte caller storage must
stay at one address; copying/relocation is rejected. Exclusive Init can reset it.
This explicitly borrowed lifetime is intended to compose with a scheduler job's
already held zone owner; a cursor is not an independent lifetime reference.

```c
xcronzonedcursor cursor;
xcronzonedcursorissue issue;
if (!xcronZonedCursorInit(&cursor, &expression, zone, begin, end, 0, &issue))
    return false;
xcronzonedstatus status;
do {
    /* Host does other work between calls; retain zone throughout. */
    status = xcronZonedCursorStep(&cursor, zone,
        (xcronzonedbudget){1, 16, 2}, &next, &issue);
} while (status == XCRON_ZONED_PENDING);
/* FOUND alone publishes next. No cursor cleanup; release the caller's zone
 * only after its final use, including any later terminal reads. */
```

Each budget field must be positive. Segments bounds new ordered UTC segment
starts; each start includes capped logarithmic TZif lookup, constant state lookup
and a ready-match check. Months bounds calendar visits, or one ready match.
RuleYears independently bounds POSIX/footer next-transition rule years. The
fixed state lookup evaluates at most five surrounding years; it is bounded
segment overhead, not a transition-year visit. This distinction prevents a
nominal one-segment budget from hiding the403-year transition-search fallback.
Rule-year and calendar engines resume inside their loops. Directory searches
are logarithmic in the capped transition table, not linear scans of history.

The new32-byte cursor issue adds a cumulative RuleYearsExamined counter. The
existing24-byte synchronous issue/layout/signatures are preserved. Each counter
increases by at most its budget in a Step. PENDING preserves Next; terminal
FOUND/NONE/mapping-error results and counters can be read repeatedly without
traversal. Normal PENDING/FOUND/NONE preserve ambient error. Diagnostics on errors
may allocate, with typed issues retained under OOM. Span/alias failures preserve
all outputs; argument/state failures preserve cursor progress. Every call checks
outputs against Cursor and all owned zone/footer storage. Init also checks the
expression. Passing a different zone, even one with identical rules, is STATE.

The public wrapper decodes/encodes its private state using memcpy (at most512
bytes each way), validates spans/identity/budgets, and publishes a32-byte optional
issue. Init copies the expression/dates and initializes context bytes. These
explicit call/copy/validation costs are amortized by larger budgets. The typed
product-private engine avoids opaque wrapper copies for later kernel integration.
The synchronous loops stay specialized, sharing calendar/event/tie rules with
the resumable engines; parity/oracle tests constrain duplicated control flow.
There is no zero-overhead, elapsed CPU or OS scheduling guarantee.

Tests run existing fold/gap/brute/UTC endpoint cases through budgeted queries,
including the500-year history before footer periodicity. They cover rare leap-
only rules across2100,403 actual fallback yields, unknown-prefix and resumed
unknown-boundary errors, copies/identity/aliases, snapshots, explicit owner
handoff and diagnostic OOM. The frozen IANA label-enumeration oracle also drives
the real cursor with varied budgets. Dedicated dense/fold/full-cycle cursor
profiles at budgets1 and16 include Init, every Step and result inspection.
They measure query work. The [scheduler](scheduler.md) and [live owner](service.md)
use the shared typed engine with global budgets, fair queries and cancellation.
The [named database](tzdb.md) supplies explicit retained zones without changing
the occurrence engine or silently rebasing existing jobs.
