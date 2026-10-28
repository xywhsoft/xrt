# Expression compiler evidence

The expression leaf is a bounded immutable compiler. A separate calendar leaf
implements Gregorian UTC/civil matching/search; timezone resolution, waiting,
dispatch and persistence are pending. Expression syntax
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
