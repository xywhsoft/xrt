# Expression compiler evidence

The expression leaf is a bounded immutable compiler. A separate calendar leaf
implements Gregorian UTC/civil matching/search; a developing timezone leaf
provides explicit POSIX rules and owned non-leap TZif history. Database lookup/
reload, leap-aware conversion, nominal delivery policies, live timer/executor
integration and persistence are pending. Expression syntax
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
Timezone/scheduler behavior, concurrent runtime operation, durable recovery and
native server performance remain later work.

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
implementation source is read or embedded. Full database/time-scale/scheduler
qualification remains pending.

## Ordered actual zoned occurrences (developing)

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

# Actual executor adapter stage

The developing `xcron_executor` leaf adds real public-XRT worker execution,
bounded preallocation/ID lookup/completion publication, cooperative cancellation,
shared queue rejection with success-only transfer, worker-error ownership, owner
thread/reentry checks and separate notification-tail protection. It owns no
executor/thread/timer. Direct header/single/package/example, many-worker lifetime
and zero-heap tests plus minimal-XRT/adapter smoke and size profiles are registered.
See [contracts, cost accounting and remaining qualification](executor.md).
