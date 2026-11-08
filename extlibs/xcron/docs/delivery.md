# Explicit nominal label selection (developing)

Select `XCRON_MODULE_XCRON_DELIVERY` and include `<xcron/delivery.h>`. This optional
leaf depends only on timezone and public Core time/atomic. It adds no filesystem,
database, calendar, scheduler, worker, timer, callback, storage or shared runtime.

`xcronDeliveryResolve` resolves one floating civil label against a caller-retained
immutable zone, then selects from the **complete** ordered actual candidates:

| Policy | Result |
| --- | --- |
| FOLD_ALL | Every candidate, in increasing UTC order |
| FOLD_FIRST | The globally earliest candidate |
| FOLD_LAST | The globally latest candidate |
| FOLD_REJECT | AMBIGUOUS if more than one candidate exists |
| MISSING_SKIP | NONE and Count0 for a label with no actual candidate |
| MISSING_REJECT | MISSING for that same absent label |

FIRST/LAST apply before a caller's later search window. A later fold does not
become FIRST merely because the earlier one was consumed or lies outside that
window. Never infer folds from IsDST: negative/zero DST, offset/name changes and
binary histories with more than two repeated instants are supported. Offset,
IsDST, Weekday and YearDay input fields are ignored. Millisecond fractions are
preserved. Full signed-int64 timestamp endpoints work; a valid civil label
outside the representable domain is missing under this API, not a guessed date.

UNKNOWN is distinct from missing. If any unspecified interval could contribute
under a legal offset, complete resolution fails ZONE/UNKNOWN. FIRST, LAST and
REJECT cannot waive that completeness check. Invalid civil fields fail ZONE/DATE.
No host TZ, locale, implicit UTC mapping or two-candidate assumption is used.

Count means required **selected** capacity. Issue.Candidates reports the complete
actual multiplicity before selection, including on ambiguous rejection and short
capacity. A NULL/zero candidate span is a sizing query. CAPACITY writes Count
and Issue but leaves the whole array intact; FOUND writes selected candidates.
NONE writes Count0 and no array bytes. ERROR leaves Count/array untouched.
Structural arguments, invalid policy values, overflowed spans and output aliases
leave every output, including Issue, untouched. Other failures publish typed
issues even under diagnostic OOM. Output spans must avoid the complete zone/
footer graph, Local and one another. Zone/Local remain live and stable during
the synchronous call. Success/NONE/CAPACITY preserve the ambient error identity.

The ALL+SKIP path without Issue delegates all span/alias validation and resolution
to original ZoneResolve once. An initialized typed-issue sentinel distinguishes
structural errors (which leave that issue untouched) from value failures without
reading ambient diagnostics. The outward delivery error identity is preserved
even on delegated argument rejection. With Issue, full delivery output validation
remains before the original resolver may write other outputs.
Other policies resolve once into a258-timestamp stack scratch and copy only the
selected output after checking capacity. The underlying TZif resolver has its
own temporary and may examine up to258 distinct offsets plus conservative unknown
coverage. POSIX has at most two offset candidates. There is no heap on normal
paths, but extra calls, validation, stack storage and copies remain; no zero-cost
adaptation or owner-turn latency guarantee is claimed. Error paths may construct
both a lower-level diagnostic and the outward delivery diagnostic, including
delegated structural failures with no requested Issue.

This is the nominal-label selection foundation. It does not match expressions,
enumerate recurring labels, change existing jobs, rewind consumed cursors or
implement missing-label shift/compensation. The actual zoned search and kernel/
live service still return all actual repeated instants and skip absent labels.
Budgeted recurring selection, gap compensation, physical delivery metadata and
durable recovery remain in the full development scope. No placeholder policy is
accepted as implemented. Performance/native Linux/>=72h gates remain open.

Run the [offline example](../examples/delivery/README.md). Separate module/single
tests cover256 actual folds, independent UTC stepping, all policy combinations,
capacity/atomicity, unknown coverage, signed-int64 endpoints, alias/OOM,
no-allocation/error identity and concurrent immutable owners. Benchmarks separate
the original complete resolver, direct ALL and selected FIRST/LAST paths, timing
whole batches rather than inventing per-operation percentiles.

## Resumable label calculation

CursorInit/Step add budgeted resolution of the same one nominal label. Init
copies its canonical civil seconds/fraction and policy, borrows the exact zone
and initializes2560 bytes of caller storage. No zone retention or Unit duty is
introduced. Local may be changed afterward; the zone must stay immutable/live
for every Step and terminal read. Cursor address is its identity; serialized
same-address handoff is supported, copying/relocation/modification is invalid.

Checks budgets one bounded initial offset-directory/coverage setup and every
completed footer transition search. A setup may binary-search the capped history
directory and perform fixed five-year state lookup. Offsets budgets individual
distinct offset probes; known binary offsets are already sorted, POSIX offsets
are ordered/deduplicated without a scan. RuleYears budgets actual incremental
footer transition-search years, including the complete403-year fallback. Each
cumulative delta is at most its corresponding budget. Zero in a needed dimension
yields PENDING; all-zero pending calls do no traversal. These units do not bound
elapsed CPU/OS time, diagnostics or a complete scheduler owner turn.

Unknown coverage is established before candidate selection. Stored unspecified
ranges use the full legal offset window, clipped to representable milliseconds.
An unspecified footer is checked through resumable transition walking, rather
than hiding an unbounded synchronous Resolve call inside Step. First/last cannot
stop at the first plausible candidate or weaken global completeness.

PENDING leaves Count/the public result array unchanged and reports no partial
candidate multiplicity. Terminal results repeat without traversal at zero budget;
CAPACITY can be retried with a larger span. At most258 final timestamps are copied
outside the traversal counters. Argument/alias failure preserves all outputs;
wrong identity publishes STATE without advancing. Callers keep outputs disjoint
from all borrowed owners, including an original zone on invalid identity calls.

The public cursor has internal result staging for ALL. Its compact private
resolver retains first/last/count independently and can omit that array; future
scheduler query state need not embed the public2560-byte object. Step copies
only control state and newly found ALL timestamps, rather than copying the whole
opaque object on every yield. Cold zeroing, validation, control copies and final
result copies remain real costs. Original synchronous APIs never return PENDING.
This stage still does not implement recurring policy search, gap compensation,
physical delivery or durable recovery. Their full scope remains required.
