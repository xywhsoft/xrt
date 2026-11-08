#ifndef XCRON_OCCURRENCE_H
#define XCRON_OCCURRENCE_H
#include <xcron/delivery.h>
#include <xcron/zoned.h>
#if defined(XCRON_FEATURE_XCRON_OCCURRENCE) && (!defined(XCRON_FEATURE_XCRON_DELIVERY) || !defined(XCRON_FEATURE_XCRON_ZONED))
#error "occurrence search requires delivery and zoned"
#endif
#if defined(XCRON_FEATURE_XCRON_OCCURRENCE)
typedef enum xcronoccurrencestatus {XCRON_OCCURRENCE_ERROR=-1,XCRON_OCCURRENCE_NONE=0,XCRON_OCCURRENCE_FOUND=1,XCRON_OCCURRENCE_PENDING=2} xcronoccurrencestatus;
typedef enum xcronoccurrenceerror {XCRON_OCCURRENCE_OK=0,XCRON_OCCURRENCE_ARGUMENT=9001,XCRON_OCCURRENCE_EXPRESSION,XCRON_OCCURRENCE_WINDOW,XCRON_OCCURRENCE_ZONE,XCRON_OCCURRENCE_AMBIGUOUS,XCRON_OCCURRENCE_MISSING,XCRON_OCCURRENCE_STATE,XCRON_OCCURRENCE_RANGE} xcronoccurrenceerror;
/* Local is canonical floating civil metadata: Offset0, IsDST-1, fraction0.
 * Candidates is its complete global multiplicity, before window/policy filtering. */
typedef struct xcronoccurrence {xtime When;xdatetime Local;size_t Candidates;} xcronoccurrence;
typedef struct xcronoccurrencebudget {uint32 Segments,Months,RuleYears,Checks,Offsets;} xcronoccurrencebudget;
typedef struct xcronoccurrenceissue {
    xcronoccurrenceerror Code;xcronfield Field;xcronzoneerror Zone;size_t Candidates;
    xtime When;xdatetime Local;
    uint64 SegmentsExamined,MonthsExamined,RuleYearsExamined,ChecksExamined,OffsetsExamined,LabelsExamined;
} xcronoccurrenceissue;
#define XCRON_OCCURRENCE_CURSOR_BYTES 1024u
typedef union xcronoccurrencecursor {uint64 Alignment;uint8 Storage[XCRON_OCCURRENCE_CURSOR_BYTES];} xcronoccurrencecursor;
XRT_EXTERN_C_BEGIN
/* Earliest selected actual occurrence in [Begin,End], or (Begin,End] with
 * EXCLUSIVE. Whole-second rounding/full-int64 bounds match ZonedNext. Each
 * matching label is resolved COMPLETELY before fold selection: FIRST/LAST refer
 * to global candidates, including candidates outside this query window. REJECT
 * fails AMBIGUOUS at the first actual matching instant in the window whose label
 * has multiple global candidates. UNKNOWN completeness is never waived by policy.
 *
 * Missing SKIP ignores absent labels. Missing REJECT checks each positive-offset
 * transition T in the rounded UTC window: enumerate matching labels in
 * [T+Before.Offset,T+After.Offset), and reject the earliest label having ZERO
 * global candidates. Another historical/future mapping can fill that apparent
 * gap, so a positive offset change alone does not prove missing. Missing errors
 * are anchored at T, including T==Begin unless exclusive. On a tie they precede
 * an actual result/error at T. Mapping failures have their first encountered UTC
 * anchor. Issue identifies that anchor/label when known. Before/after unknown
 * mapping needed to decide a gap is ZONE/UNKNOWN. No shift/compensation occurs.
 *
 * History is completed before any periodic proof. A complete400-year suffix is
 * applied only after the maximum legal offset window lies in the periodic zone;
 * no arbitrary horizon is used. NONE/ERROR leave Next untouched; FOUND alone
 * writes Next. Pending Issue exposes counters only, not provisional candidates.
 * Code/Field/Zone remain typed under diagnostic OOM. Normal results preserve
 * ambient errors and request no heap/IO/lock/retention; diagnostics may allocate.
 * Outputs avoid Expression, the complete Zone/footer ownership graph and one
 * another. Structural invalid flags/policies/spans/aliases preserve all outputs.
 * This synchronous convenience loop never returns PENDING. It can perform large
 * work: callers needing admission fairness use the cursor instead. No existing
 * scheduler/executor/service job, clock/misfire/overlap or durable recovery change.
 * Error domain: xrt.cron.occurrence. */
XCRON_API xcronoccurrencestatus xcronOccurrenceNext(const xcronexpression* Expression,const xcronzone* Zone,
    xtime Begin,xtime End,uint32 Flags,xcrondeliverypolicy Policy,xcronoccurrence* Next,xcronoccurrenceissue* Issue);
/* Init validates/snapshots Expression, bounds and Policy, records exact Zone
 * identity, rounds bounds and computes the periodic suffix bound. No traversal.
 * BORROWS Zone: keep that exact immutable owner live for every Step/terminal read.
 * Local cursor storage must stay at its initialized address; copying/relocating/
 * modifying bytes is invalid. Serialized same-address thread handoff is allowed.
 * Init failure preserves Cursor. No heap, retention or Unit duty. Expression may
 * change after Init; outputs also avoid Cursor and Expression during Init. */
XCRON_API bool xcronOccurrenceCursorInit(xcronoccurrencecursor* Cursor,const xcronexpression* Expression,const xcronzone* Zone,
    xtime Begin,xtime End,uint32 Flags,xcrondeliverypolicy Policy,xcronoccurrenceissue* Issue);
/* Independent nonnegative per-Step upper bounds; zero is allowed. Segments
 * counts ordered UTC segment starts, including their bounded state/gap setup;
 * Months counts calendar visits/ready matches; RuleYears counts actual incremental
 * transition-search years from all engines (not fixed five-year state lookups);
 * Checks/Offsets count complete-label coverage/search checks and distinct offset
 * probes as in DeliveryCursorStep. Labels counts completed resolution attempts,
 * including failures; it has no separate budget and reflects already charged
 * resolution work. Each traversal-counter delta is at most its own budget.
 * Needed exhausted dimensions yield PENDING with Next untouched; an all-zero
 * pending call changes no cursor bytes. Terminal results/issues repeat without
 * traversal even at zero budget. State/argument failures do not advance Cursor.
 * Work units do not bound elapsed CPU/OS time, diagnostics or a scheduler turn.
 * Wrapper control copies, cold zeroing, bounded directory/state/validation work
 * and terminal output copies remain costs; there is no zero-overhead claim. */
XCRON_API xcronoccurrencestatus xcronOccurrenceCursorStep(xcronoccurrencecursor* Cursor,const xcronzone* Zone,
    xcronoccurrencebudget Budget,xcronoccurrence* Next,xcronoccurrenceissue* Issue);
XRT_EXTERN_C_END
#endif
#endif
