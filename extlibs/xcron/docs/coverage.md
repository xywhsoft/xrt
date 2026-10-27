# Expression compiler evidence

The first xcron leaf is a bounded immutable expression compiler. It does not
search calendars, resolve time zones, wait, dispatch or persist jobs. Syntax
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
it measures only this leaf. Calendar/timezone/scheduler behavior, concurrent
operation, durable recovery and native server performance remain later work.
