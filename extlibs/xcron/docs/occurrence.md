# Selected recurring occurrence search

Select `XCRON_MODULE_XCRON_OCCURRENCE` and include `<xcron/occurrence.h>`.
This developing optional leaf composes calendar, ordered actual-zone search and
complete nominal resolution. It adds no scheduler, executor, file, network,
clock, callback, ownership reference or shared runtime. The smaller delivery
leaf keeps its original timezone-only dependency closure.

`OccurrenceNext` returns the earliest selected actual instant in the inclusive
UTC window, or excludes Begin with `XCRON_SEARCH_EXCLUSIVE`. Whole-second
rounding, BCE/fractional bounds and signed-int64 endpoints match ZonedNext.
FOUND alone writes the occurrence. Its Local is a canonical floating civil
label: Offset0, IsDST-1, fraction0, derived Weekday/YearDay retained. Candidates
counts the COMPLETE global multiplicity before policy or window filtering.

ALL returns every actual matching instant through successive queries. FIRST
and LAST use the globally first/last candidate for each label. A later fold
does not become FIRST because the earlier one lies outside the query. REJECT
fails AMBIGUOUS at the first actual matching instant in the window having more
than one global candidate. Arbitrary histories with256 folds work; IsDST is not
a fold identity. Every actual label is completely resolved, even ALL. Potential
unspecified coverage under any legal offset is ZONE/UNKNOWN, not waived by
finding a plausible candidate. Empty rounded windows need no zone mapping.

## Missing labels and ordering

SKIP ignores absent labels. REJECT associates missing-label checks with each
positive-offset transition T in the rounded UTC window. It enumerates matching
labels in `[T+Before.Offset,T+After.Offset)`, then completely resolves each one.
A label is missing only if its global candidate set is empty: another historical
or future segment can fill an apparent local gap. The earliest actually missing
matching label causes MISSING anchored at T. T==Begin is included unless exclusive.
A query starting after T does not retroactively reject that gap.

Gap failures at T precede an actual result/error at the same T. Unknown before/
after mapping needed to decide a gap is ZONE/UNKNOWN. The Issue exposes its UTC
anchor and canonical nominal label when known; unknown state without a known
label leaves Local zero. Other policy/mapping errors carry their actual search
anchor. This is explicit rejection, not shifted or compensated delivery.

The current implementation first computes a provisional actual selected result/
error or absence, then checks gaps from the rounded start through that anchor
(or the proof limit for absence). It publishes only after both stages establish
ordering. Thus an earlier missing event supersedes a later actual error. This
sequential composition can traverse a complete400-year absence proof before
reporting an early missing label for an always-gap expression. Interleaving these
streams and accelerating dense rejected folds remain performance optimization
work; no production acceptance is claimed for that worst case.

History is completed before applying periodicity. The global proof limit includes
`2*INT32_MAX` seconds after the proven periodic suffix begins, then a full400-year
Gregorian cycle. This ensures the whole legal candidate-offset window lies in
the periodic suffix. No arbitrary lookahead discards a500-year historical prefix.
The original actual engine may also prove actual absence earlier, which implies
selected actual absence independently of fold selection. Gaps are still checked.

## Budgeted execution and lifetime

Init snapshots the expression, bounds and policy, borrows the exact immutable
zone, rounds the window and derives the proof limit without traversal. The
1024-byte cursor stays at one address; copying/relocation/modification is invalid.
Serialized same-address thread handoff is supported. The expression may change
after Init. Keep the same zone alive through all Steps and terminal reads; no
retention or Unit duty is introduced. Exclusive Init may reset caller storage.

Each Step has five independent nonnegative budgets. Exhausting a needed
component yields PENDING; all-zero pending calls change no cursor bytes.

| Budget | Actual charged traversal |
| --- | --- |
| Segments | Actual ordered segment starts plus initial gap mapping checks and gap transition-walk initializations |
| Months | Calendar visits and actual ready-match checks |
| RuleYears | Incremental transition-search years across every engine; excludes fixed five-year state lookups |
| Checks | Complete-label initial coverage/directory checks and completed footer searches |
| Offsets | Distinct candidate offset probes, at most258 per label |

Cumulative deltas are at most the corresponding budgets. LabelsExamined is
separate accounting for completed resolution attempts, including unknown errors;
it has no additional budget and reflects already charged resolution work.
PENDING preserves Next and exposes only counters, never a provisional result,
error anchor or multiplicity. Terminal results/issues repeat without traversal
at zero budget. NONE/ERROR leave Next unchanged. Argument/state errors do not
advance the cursor. Normal results preserve ambient error identity and request
no heap/IO/locks. Error diagnostics can allocate; typed issues survive OOM.

Outputs avoid the cursor, complete borrowed zone/footer graph, one another and
Expression at Init/synchronous calls. Invalid structural flags/policies/spans/
aliases preserve all outputs. Wrong identity is STATE. On invalid-identity calls,
outputs must still avoid every borrowed owner, including the original zone.

The private typed engine reuses dormant actual calendar/transition storage for
the later gap stage and omits full candidate arrays. The public wrapper copies
its private control state, initializes fixed storage, validates spans and emits
result/Issue metadata. Those copies, bounded directory/state checks and calendar
arithmetic remain costs. Budgets bound algorithmic traversal, not elapsed CPU/
OS time, diagnostic work or a whole scheduler owner turn. The synchronous
convenience loop can do large work and never returns PENDING.

Run the [offline example](../examples/occurrence/README.md). Separate protocol/
adversarial/OOM/lifetime and performance assets cover complete fold selection,
global gap filling, tie/window behavior, budget counters, large histories and
independent frozen IANA/UTC models. Benchmark batches include requested Issue
metadata, cursor Init/Step work and diagnostic construction/clearing for expected
REJECT errors. The public-API composition comparator covers LAST+SKIP only.
These calculations do not reconfigure existing scheduler/service jobs. Gap
compensation, physical dispatch integration and durable recovery remain required,
along with qualified native Windows/Linux performance and>=72h mixed-load gates.
