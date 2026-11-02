# Explicit timezone rule core (developing)

The timezone leaf currently compiles POSIX TZ strings and TZif-v3 extended rule
strings independently. It is the rule engine required by IANA TZif footers;
IANA binary loading, historical transitions and zoned cron search are pending.
No host TZ environment, libc global timezone state or installed data is consulted.

The immutable retained zone owns its names and an optional 400-year transition
cache in one allocation. Queries return state, exact next effective transition,
civil decomposition and all UTC candidates for a floating label. A gap has zero
candidates; a fold returns both in order. No default choice hides these policies.
The API header specifies limits, allocation, aliases, normal capacity sizing and
ownership. Timestamps use XRT's signed-int64 milliseconds from 0001-01-01,
with `XRT_TIME_UNIX_EPOCH` identifying 1970-01-01; they do not include leap
corrections. Leap labels are
outside this timescale. Diagnostics do not determine semantic status.

Rules follow [RFC 9636](https://www.rfc-editor.org/rfc/rfc9636.html), section 3.3,
and its POSIX TZ reference. Full TZif integration and production qualification
remain required by the four-library plan. This leaf stays developing.

## Bounds and costs

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
