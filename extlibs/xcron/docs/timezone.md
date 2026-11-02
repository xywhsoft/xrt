# Explicit timezone rule core (developing)

The timezone leaf compiles POSIX TZ strings, TZif-v3 extended rule strings,
and owned RFC9636 application/tzif v1..4 binary histories independently.
Named database lookup/reload, leap-aware conversion and zoned cron search are pending.
No host TZ environment, libc global timezone state or installed data is consulted.

An immutable POSIX zone owns its names and optional 400-year transition cache
in one allocation. Binary zones own decoded history and an optional separately
retained footer zone. Queries return state, exact next effective transition,
civil decomposition and all UTC candidates for a floating label. A gap has zero
candidates; a fold returns every candidate in order (binary history may have more
than two). No default choice hides these policies.
The API header specifies limits, allocation, aliases, normal capacity sizing and
ownership. Timestamps use XRT's signed-int64 milliseconds from 0001-01-01,
with `XRT_TIME_UNIX_EPOCH` identifying 1970-01-01; they do not include leap
corrections. Leap labels are
outside this timescale. Diagnostics do not determine semantic status.

Rules follow [RFC 9636](https://www.rfc-editor.org/rfc/rfc9636.html), section 3.3,
and its POSIX TZ reference. Full database/time-scale integration and production qualification
remain required by the four-library plan. This leaf stays developing.

## Explicit rule bounds and costs

Input is a bounded 4096-byte ASCII span; each owned designation has 3..255 bytes.
Offsets are seconds east of UTC, parsed from POSIX's opposite sign convention.
Explicit offsets allow hours 0..24 and minute/second 0..59; the default DST
offset is standard +3600 seconds. Transition times default to 02:00 local wall
time; explicit POSIX hours are unsigned 0..24, while TZif3 permits signed
-167..167. `Jn` skips February 29, zero-based `n` counts it, and `Mm.w.d` week5
selects the last weekday of the month. Missing DST rules have a typed error,
so installed host defaults cannot change behavior.

Cold creation parses fully before one allocation. DST rules precompute 800
int32 transition offsets for the 400 Gregorian years 2000..2399 (3200 bytes),
plus private owner metadata and copied NUL-terminated names. Fixed zones omit
the cache. Retain/release uses one atomic owner count; callers must already
hold a live reference. Borrowed abbreviation views do not extend lifetime.

State lookup decomposes one UTC civil year and examines at most ten cached
events across five surrounding years. Split additionally maps full-range local
days into a safe Gregorian cycle, avoiding a potentially overflowing whole
timestamp offset addition. Resolve tries at most two offsets, validates each
against the actual zone state, deduplicates and sorts. All normal queries,
capacity discovery and NONE results allocate nothing and preserve ambient
errors. Diagnostic errors may allocate; typed issues remain independent of
that allocation. Candidates/Count remain unchanged on ERROR.

Next-transition search checks actual states on either side of each candidate,
coalesces coincident events, and includes zero-offset-delta DST/name changes.
Perpetual/empty rules are detected at creation. The worst case covers 403 rule
years, including a complete recurrence and signed cross-year carries; this is
a completeness bound, not a policy lookahead limit. Existing rules can change
only in leap years, so a fixed one-year search would be incorrect.

The dedicated unpinned benchmark distributes timestamps across a modern leap
year and measures precompiled state lookup only. Cold compilation, Split,
Resolve, NextTransition, contention and end-to-end scheduler P99 remain to be
profiled before performance acceptance. It makes no libc/IANA parity or
zero-overhead claim.

## Owned binary history

`xcronZoneCreateTzif` takes an `xbytesview`, copies/decode its authoritative
history and retains nothing from the input. The application chooses and pins
the data source; the extension does no pathname lookup, IO or installed-database
fallback. [The offline binary example](../examples/tzif/README.md) demonstrates
input retirement and exact UTC resolution.

The v1 block of newer files is used only to locate the 64-bit block. Main-block
counts, file bounds, strictly ordered transition seconds, type references,
offsets, flags, name termination and nonempty-footer consistency are checked.
Suffix sharing and empty legacy names work. Unsafe designation bytes receive
a numeric designation derived from the offset. Integer offsets span the whole
legal int32 range except its minimum. Full int64 Unix transition seconds remain
comparable without multiplying unrepresentable values into milliseconds; footer
consistency outside the XRT timeline is evaluated in an equivalent Gregorian
cycle. The header documents explicit input/count/name resource limits.

Creation removes redundant state changes, decodes native int64 times and type
indices, and builds sorted unique offsets and unknown-interval metadata in one
allocation. A nonempty footer adds one POSIX owner allocation, released with the
history's final reference. Main history/names and footer-name output aliases are
both rejected. State and next-change lookup use binary search. Known-state
resolution tries at most 258 unique offsets, validates candidates by actual
history lookup, and emits sorted results without heap allocation. Normal At does
not decompose civil time while it is inside stored history. Direct POSIX lookup
adds one history-pointer branch; owner metadata grows by one pointer.

Type0 governs the time before the first record. On/after the last record, the
footer governs; without it the mapping is unspecified. With no records and no
footer, type0 applies everywhere. `-00` explicitly denotes unspecified mapping.
At/Split then return `UNKNOWN` without changing results. NextTransition exposes
coverage boundaries using `XCRON_ZONE_OFFSET_UNKNOWN`, an impossible real offset.
Those designation views are owned too. Resolution refuses an incomplete result
when an unspecified interval could map to the label using any legal offset.
That conservative full-domain completeness check can reject a known instant's
local label near a truncated interval; it is not a DST gap. Unknown ranges use
binary intersection; a footer with unspecified seasons additionally checks its
recurring changes. No host heuristic supplies missing coverage.

Leap-aware blocks receive structural/table checks and return `SCALE`; leap
conversion, truncated leap tables and expiration-aware query semantics remain
pending. They are never interpreted as ordinary Unix timestamps. This stage is
an application/tzif reader, not claimed complete application/tzif-leap support.

`test_tzif.c` includes triple folds, extreme offsets/transition seconds, owned
input/footer/unknown names, all truncation prefixes, mutated data, partial OOM,
allocation/error-preserving queries and concurrent retained owners. A frozen
IANA 2026e corpus is compared independently with Python zoneinfo and explicit
RFC coverage rules, including unknown and incomplete resolutions. Portable
evidence is recorded in the progress ledger/results. The separate 4096-change
benchmark measures decoded-history lookup only. Database reload, zoned calendar
search, full scheduler workloads and performance/production acceptance remain.
