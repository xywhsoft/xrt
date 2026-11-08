# Expression compiler evidence

The expression leaf is a bounded immutable compiler. A separate calendar leaf
implements Gregorian UTC/civil matching/search; a developing timezone leaf
provides explicit POSIX rules and owned non-leap TZif history. Explicit named
database snapshots/reload are available. Leap-aware conversion, nominal delivery
policies and persistence are pending. Live service and executor stages are
described below. Expression syntax
and deliberate dialect boundaries are documented in [the guide](../README.md).
Implementation follows the public [Cronie manual](https://www.man7.org/linux/man-pages/man5/crontab.5.html)
independently; no reference scheduler code is embedded.

| Contract | Test/evidence asset |
| --- | --- |
| Explicit Unix five / seconds six fields | Exact field-count rejection and fixed second-zero behavior |
| Field ranges and masks | Handwritten masks, natural value bit positions, Sunday 0/7 normalization |
| Numeric/name ranges, lists, steps | All 111,630 minute range/step combinations against independent membership oracle; 10,000 generated six/five-field expressions |
| Day restriction semantics retained | Star vs numeric full ranges, mixed star lists, star steps; flags are not inferred from equivalent bitmaps |
| Names and calendar aliases | Every month/weekday name, ASCII case folding, each alias in both dialects |
| Strict grammar | Empty/missing/extra fields, reverse ranges, invalid steps, overflow, NUL/CRLF/non-ASCII, unsupported Quartz/random/reboot forms |
| Bounded work and atomic output | 4096-byte boundary, exact issue offsets, 20,000 raw byte inputs, unchanged compiled storage on every parse failure |
| Lifetime | Caller value retains no input; input mutation cannot change compiled masks |
| Invalid API calls | Missing/wrapped spans, invalid dialect, input/output and output/output aliases leave outputs unchanged |
| Allocation and error contract | 10,000 compiles with unchanged allocation counters/error identity; alias under one-byte quota; diagnostic OOM preserves local issue and output |
| Distribution | Dedicated modular/single tests, runnable example, independent static/shared consumer, trim and performance/size profiles |

Verified environments and reproducible commands are recorded in the
[progress ledger](../../../docs/development/service-extensions-progress.md).
The compilation benchmark changes input steps and observes compiled masks;
it measures only this leaf. The calendar leaf has its own search workload.
The later zone/kernel/executor/service sections cover implemented runtime stages;
durable recovery and native server performance qualification remain open.

## Nominal-label selection foundation (developing)

The separate [delivery leaf](delivery.md) adds explicit all/first/last/reject
fold selection and skip/reject missing labels after complete zone resolution.
`test_delivery.c` constructs256 real candidates in a shared-designation TZif,
then verifies every policy, required selected capacity and untouched output tails.
Independent UTC stepping checks768 labels through64 arbitrary16-transition
histories under all8 policies, including strict error/capacity results. Golden
negative/half-hour/zero-delta DST, fractions, full-int64 endpoints, globally unknown
coverage, aliased/wrapped outputs, diagnostic OOM and30000 warmed zero-heap/error-
preserving calls accompany concurrent immutable owners. Golden/brute cases pair
the whole output array/count with and without Issue. Delegated NULL/span/owner/
footer aliases and argument versus date/unknown error identity are checked.
The warmed loop includes capacity discovery and alternating optional Issue under
one-byte quota. The IANA probe pairs both forms for every policy and requires its
new protocol, rather than assuming an old executable performs the second call.
Modular/single, runnable
example and real external static/shared consumers exercise the new public API.
This does not change scheduler selection or implement gap compensation yet.

## Gregorian calendar contracts

The [calendar guide](calendar.md) defines exact fractional-second and interval
semantics, floating civil labels, complete recurrence bounds and remaining DST
responsibilities. Tests include:

- Ten DOM/DOW/month patterns over every day in a full 400-year Gregorian cycle,
  with independently stepped calendar dates/weekdays and star-vs-range semantics.
- Five hundred random earliest-match queries against a brute one-second oracle,
  and one thousand sparse searches against an independently stepped daily oracle.
- Century leap rules, invalid month dates, field carries, BCE/CE/year-zero rules,
  year 12000, full UTC int64 endpoints and civil int64 year endpoints, exact bounds,
  exclusive/inclusive starts, fractional/negative rounding and whole-second Match.
- Month/work limits, complete-cycle impossible dates, normal NONE publication,
  ignored civil offset/derived fields and canonical output metadata.
- Expression shape/flag/dialect validation, invalid dates/windows, null/wrapped
  spans, input/output aliases and unchanged outputs on failure.
- Ten thousand warm cycles across all four APIs preserve allocation counters and
  ambient error; whole-cycle NONE also preserves both. One-byte allocation quota
  supports valid/none calls; diagnostic OOM retains typed expression/date/window
  issues and leaves no live quota bytes.
- Modular/single tests, deterministic offline example, root examples/trim,
  external static/shared consumers, dedicated workload and isolated-size profile.
- All 1500 independent earliest-match oracles also exercise the resumable UTC
  cursor with varying month budgets; known cases use one-month steps.
- An independent full-cycle day table checks 5208 month/day/weekday conjunction
  and disjunction existence cases. Constant February30/other impossible-day
  proofs preserve OR semantics; the raw resumable engine separately exhausts
  all 4801 fallback months under one-month budgets.
- UTC/civil cursors copy inputs, reject relocation/wrong modes, preserve outputs
  across pending and invalid spans/aliases, repeat terminal results and support
  serialized same-address thread handoff. A 94-month finite sparse absence query
  checks every yield, unchanged ambient error and zero requested heap. Typed
  init/state issues remain available under diagnostic OOM.

These are calendar calculations using public XRT time APIs. No IANA/DST mapping,
timer/executor, runtime race/clock-jump or durable recovery acceptance is implied.

## Explicit timezone rules (developing)

`test_timezone.c` covers named northern/southern rules, half-hour folds, negative
and zero-delta DST, Jn/n leap differences, leap-only seasons across a non-leap
century, signed extended transition times, perpetual/empty seasons, sorted
folds/gaps/capacity, owned names, diagnostic/factory OOM, aliases and error
atomicity. Fifteen thousand split/resolve roundtrips include BCE and both int64
endpoints with offsets beyond 24 hours. Ten thousand warm cycles check ambient
error identity and allocation counters. Four concurrent retained owners each
run 12,000 split/state/resolve cycles while acquiring/releasing extra references.

The independent `test_timezone_reference.py` compares state, civil labels, all
resolution candidates and exact next changes against Python `zoneinfo` reading
synthetic TZif v3 footers, plus a separate Linux libc `tzset` pass. The production
library never changes `TZ` or calls libc timezone functions. CPython 3.12's
observed unprefixed-n one-based behavior is excluded from that oracle only;
the exact zero-based rule is still checked by libc and fixed C expectations.
The probe explicitly bridges XRT's 0001 epoch to the oracle's Unix epoch.
The zoneinfo run covers years 1000..9000; libc is checked from 1971 because its
direct-TZ behavior is not proleptic. Its direct parser also differs for the two
RFC perpetual-footer fixtures, which remain covered by zoneinfo and exact C
expectations. No oracle's unsupported behavior defines the production contract.

Those are rule-engine checks. Binary history has the additional evidence below;
the actual zoned search has the additional checks below. Scheduler/durability
and accepted P99 remain pending.

## TZif binary history (developing)

The dedicated modular/single `test_tzif.c` covers triple fold candidates, type0
and footer handoff, redundant transition elimination, full int64 transition
seconds, extreme legal int32 offsets, suffix/empty names, numeric fallback,
unknown coverage, every truncated prefix, 7000 mutated/raw inputs, invalid
counts/order/flags/indices/offsets, copied source bytes, owned footer/unknown
aliases, footer consistency, typed leap-scale rejection, partial factory OOM
rollback, 10000 allocation/error-preserving query cycles and four retained
12000-cycle concurrent owners. The root package consumer invokes the new factory.

`test_tzif_reference.py` reads the same frozen compiled IANA data independently
through Python zoneinfo. Every stored transition in 1800..2500 is queried before,
at and after its boundary, with additional historical/future random dates.
Raw RFC coverage is checked separately because zoneinfo otherwise extends the
last table type when a footer is missing. Unknown/partial outcomes are verified
as such; they are not counted as successful complete resolutions. No upstream
implementation source is read or embedded. Explicit named database coverage is
described below; full time-scale/scheduler/production qualification remains pending.

## Ordered actual zoned occurrences (developing)

The resumable cursor runs the golden fold/gap cases,160 brute windows and1000
full-int64 UTC-specialization comparisons with varied segment/month/rule-year
budgets. Both the400-year periodic absence and500-year historical prefix are
completed across resumptions. Rare leap-only rules yield within transition
search; the raw general engine separately exhausts403 actual one-year turns.
Snapshot/identity/alias/wrapped-span, zero budgets, repeated terminals, unknown
coverage after a known prefix, diagnostic OOM,1024 zero-heap resets and serialized
explicit-owner handoff are covered. The native single fixture runs the same
cases. The frozen IANA oracle's cursor probe checks all three budget deltas.

`test_zoned.c` verifies later folds after the local label moves backwards,
half-hour and negative DST, missing labels, all three synthetic fold instances,
fractional/negative rounding, full int64 endpoints and second-level offsets.
It compares 160 bounded searches with independent second stepping and 1000
full-range UTC specializations with the original calendar entrypoint. An annual
always-missing label proves NONE over a complete periodic cycle; a 500-year
history followed by a different footer proves the search cannot use a cutoff
from Begin. Unknown intervals inside/outside the query, aliases/flags/windows,
diagnostic OOM, typed output atomicity, 10000 warm error/allocation-preserving
cycles and four 12000-cycle retained concurrent owners are included.

`test_zoned_reference.py` enumerates independently specified local labels, maps
both zoneinfo folds, checks roundtrips, and sorts UTC candidates for earliest
search. This is independent of the library's ordered-segment algorithm. Nine
profiles cover Unix5/seconds6, DOM/DOW OR and star-step AND behavior, weekday
hours, leap/impossible dates and sparse seconds around frozen IANA transitions.
Unknown coverage is verified over the actual window, including empty/fractional
windows, rather than using global label resolution to invent a complete list.
This is calculation evidence; configurable compensation/selection and runtime
clock/misfire/executor/durable delivery policies remain later work.
# Bounded pull scheduler stage

The developing `xcron_scheduler` leaf has actual sources, direct modular header,
selected single test, package consumers, example, size and performance profiles.
It provides copied expressions/retained zones, bounded job/run slots and bytes,
generation IDs, explicit paired clocks, catch-up/skip/coalesced misfires,
wait/allow/skip overlap, round-robin ready dispatch, work budgets, per-job typed
calculation faults, physical completion leases and cancellation/retirement receipts.
It creates no timer/thread/executor and supplies no durable recovery. See
[the complete contract and remaining gaps](scheduler.md). The root and leaf remain
developing. Qualification evidence is recorded separately from implementation.

Budgeted advancement adds a second92160-sample finite-list model run, fixed
skip/coalesce boundaries, calculation-only polls at full lease capacity, future
faults with valid live leases and pending-query cancellation/retirement. Tests
check global segment/month/rule-year/action/output limits across mixed heavy/light
jobs. A Capacity1/WorkLimit3 dense catch-up counterexample verifies that eager
probes reserve credit instead of starving an older query. Provisional admission
IDs cannot dispatch, be publicly queried/cancelled or Reap; prepare/commit/rollback
and typed diagnostic OOM retain the original Data/zone duties.128 sparse bounded
lifecycles perform more than10000 real yields with zero requested heap.

The actual service clock-turn fixture holds completion output full while a
400-year absent initial query remains pending. Later Reap/light ADD work proceeds,
and close returns original Data with no published job. This behavioral witness
fails against archived parent libraries. LastPoll checks each actual owner turn;
the test uses callback barriers rather than a latency threshold. A separate
deferred unknown-coverage failure leaves no job/body/retirement duty. Existing
service adversarial, OOM, metadata-credit and concurrency tests run on this path.

# Actual executor adapter stage

The [named database](tzdb.md) has separate memory/file leaves. Tests cover exact
case-sensitive names, in-place sorted lookup, copied input/version, old snapshots
and independent zones surviving replacement/manager destruction, CAS races,
bounded builders, close during cold work, aggregate old+candidate quotas, typed
diagnostic OOM, reference/revision exhaustion and warmed zero-heap lookup.
Four actual readers also query coherent snapshots during250 real publications.
Root tests load real files and contained aliases, reject escape/missing/invalid/
nonregular/oversize members and keep old publication on failure. A separate
modular link-wrap unit injects Core read/trailer/metadata/stat/close outcomes;
it asserts every injected branch ran and checks publication/resource rollback.
These fault injections are distinct from real native filesystem behavior and
do not certify native failed-close resource handling or production acceptance.

The developing `xcron_executor` leaf adds real public-XRT worker execution,
bounded preallocation/ID lookup/completion publication, cooperative cancellation,
shared queue rejection with success-only transfer, worker-error ownership, owner
thread/reentry checks and separate notification-tail protection. It owns no
executor/thread/timer. Direct header/single/package/example, many-worker lifetime
and zero-heap tests plus minimal-XRT/adapter smoke and size profiles are registered.
See [contracts, cost accounting and remaining qualification](executor.md).

# Actual live service stage

The developing `xcron_service` leaf adds a dedicated public-XRT owner and paired
system/custom clocks. Commands/replies and completion reservations are bounded;
full output stops new dispatch without losing results or blocking final close.
Original Data returns only through a failed Add reply or physical retirement;
ready metadata contains no borrowed Data pointer. Positive blocking waits reject
the owner and unrelated callbacks on the same borrowed Executor. Retry is limited
to no-effect native overload, with one original monotonic deadline and explicit
attempt/expiry results. Clock failure/regression triggers typed controlled close.

Tests include real UTC firing, actual fold instants, forward/backward wall jumps,
catch-up/skip/coalesce, held-owner queued Add/close, ticket reuse/racing collectors,
512 concurrent Info requests per full unit, eight four-producer Add/Close races,
physical cancellation/results, attempt/elapsed retry bounds and safe retry success,
join before output consumption, observed cold allocation prefixes, alias/overflow
rejection and 2048 warmed zero-requested-heap cycles. Separate module/single,
root/leaf static/shared consumers and runnable example are registered. Fixed UTC
arrival start-lateness and owner-to-worker measurements use millisecond clocks;
smoke has no production/throughput/P99 acceptance. See [full service contract](service.md).

## Live precise waiting (2026-11-05)

The live owner now selects the public event waiter. Native timer startup rollback,
wait failure shutdown, cleanup failure/retry, joined handoff, actual Windows
standard-timer fallback and 64 handle-balanced service lifetimes are exercised.
Backend capacity and live timers are separately inspectable; normal stop releases
the native timer before consumers must drain results or reap jobs. Both full
modular/single suites, static/shared consumers, runnable example and separate
TSan units pass; WSL is not native Linux qualification. Millisecond unpinned
fixed-arrival/CPU results retain their failed noise gates. See
[the stage record](../tests/results/precise-service-20261105.json).
