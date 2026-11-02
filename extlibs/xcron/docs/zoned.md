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
Nominal-time gap compensation, choosing a subset of folds and delivery after
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
long-duration acceptance. Database reload, leap-aware conversion and the complete
scheduler/executor/persistence/policy layers remain pending.
