#ifndef XCRON_CALENDAR_H
#define XCRON_CALENDAR_H
#include <xcron/features.h>
#include <xcron/export.h>
#include <xcron/expression.h>
#include <xrt/features.h>
#include <xrt/time.h>

#if defined(XCRON_FEATURE_XCRON_CALENDAR) && \
	(!defined(XCRON_FEATURE_XCRON_EXPRESSION) || !defined(XRT_FEATURE_TIME))
#error "XCRON_FEATURE_XCRON_CALENDAR requires expression and public time features"
#endif

#if defined(XCRON_FEATURE_XCRON_CALENDAR)

#define XCRON_SEARCH_EXCLUSIVE 0x01u
/* Complete year-free Gregorian recurrence period, not an arbitrary horizon. */
#define XCRON_CALENDAR_MAX_MONTHS 4801u

typedef enum xcroncalendarstatus {
	XCRON_CALENDAR_ERROR=-1, XCRON_CALENDAR_NONE=0, XCRON_CALENDAR_FOUND=1,
	XCRON_CALENDAR_PENDING=2
} xcroncalendarstatus;

typedef enum xcroncalendarerror {
	XCRON_CALENDAR_OK=0, XCRON_CALENDAR_ARGUMENT=1001, XCRON_CALENDAR_EXPRESSION,
	XCRON_CALENDAR_DATE, XCRON_CALENDAR_RANGE, XCRON_CALENDAR_WINDOW, XCRON_CALENDAR_STATE
} xcroncalendarerror;

/* Field identifies a bad expression bitmap, otherwise NONE. MonthsExamined is
 * 0 for Match/value errors/empty rounded windows/constant absence proofs, and
 * <=MAX_MONTHS for search, including skipped
 * months. NONE is a normal result with OK, not a diagnostic/error allocation. */
typedef struct xcroncalendarissue {
	xcroncalendarerror Code;
	xcronfield Field;
	uint32 MonthsExamined;
} xcroncalendarissue;

XRT_EXTERN_C_BEGIN

/* Test an exact UTC instant in the full xtime int64 millisecond domain. Seconds
 * must match, and nonzero milliseconds do not match; UNIX5 has second zero.
 * Validates immutable expression masks/flags/dialect before use. No input is
 * retained. Success (including *Matches=false) allocates nothing and preserves
 * ambient error. Failure leaves Matches unchanged. Optional Issue publishes OK
 * or a typed value/range error independently of diagnostic OOM. Invalid pointer/
 * span/output aliases leave all outputs unchanged. Error domain is xrt.cron. */
XCRON_API bool xcronCalendarMatch(const xcronexpression* Expression, xtime When,
	bool* Matches, xcroncalendarissue* Issue);

/* Earliest whole-second match in [Begin,End], or (Begin,End] with EXCLUSIVE.
 * Bounds use xtime milliseconds; fractional Begin is rounded upward safely,
 * including negative/BCE timestamps. End is always inclusive. Reverse windows
 * are WINDOW errors. FOUND alone writes Next; NONE/ERROR leave it unchanged.
 * No instant-by-instant scan: skip month/day/time masks and exhaust at most a
 * complete 400-year Gregorian cycle (MAX_MONTHS visits) to prove no recurrence.
 * Full int64 endpoints supported without constructing out-of-range midnights.
 * Normal FOUND/NONE preserve ambient error and allocate/retain nothing. Issue
 * follows Match publication, adding work count. Flags accepts EXCLUSIVE only.
 * Output/Issue must be disjoint from Expression and each other. A prior Match/
 * compile does not bypass validation. Actual readable/writable objects required.
 * This is UTC calendar calculation, not a timer/clock/scheduler/time-zone API. */
XCRON_API xcroncalendarstatus xcronCalendarNext(const xcronexpression* Expression,
	xtime Begin, xtime End, uint32 Flags, xtime* Next, xcroncalendarissue* Issue);

/* Floating civil-label counterparts for future zone-transition segment search.
 * Read Year/Month/Day/Hour/Minute/Second/Millisecond; ignore Offset, Weekday,
 * YearDay and IsDST. Derive Gregorian metadata using public XRT time APIs.
 * Civil labels support every nonzero int64 year independently of timestamp
 * range. Year zero, invalid dates and leap seconds are errors. Derived metadata
 * uses a safe equivalent year in the 400-year cycle, through public XRT APIs.
 * Next output is canonical xrtTimeSplit metadata at offset zero (IsDST=-1).
 * No system-local zone, TZ environment, gap/fold mapping or DST choice occurs.
 * All inputs must stay immutable during a call. Outputs must additionally be
 * disjoint from every civil input. Semantics otherwise match UTC counterparts.
 * A timezone adapter must examine ordered UTC transition segments: merely
 * searching forward from the current local label can miss the later fold. */
XCRON_API bool xcronCalendarMatchCivil(const xcronexpression* Expression,
	const xdatetime* When, bool* Matches, xcroncalendarissue* Issue);
XCRON_API xcroncalendarstatus xcronCalendarNextCivil(const xcronexpression* Expression,
	const xdatetime* Begin, const xdatetime* End, uint32 Flags,
	xdatetime* Next, xcroncalendarissue* Issue);

/* Allocation-free, address-bound resumable single-result search. Init snapshots
 * expression/bounds and performs only validation/rounding/a constant ready-match
 * check/constant absence proof; input storage may then change/disappear. No borrowed pointers, clock,
 * thread, zone or heap. Init may reset an existing cursor under exclusive access.
 * Keep this initialized context at a stable address; do not copy/modify its bytes.
 * A serialized thread handoff of the same live context is allowed. No Unit duty. */
#define XCRON_CALENDAR_CURSOR_BYTES 256u
typedef union xcroncalendarcursor {uint64 Alignment;uint8 Storage[XCRON_CALENDAR_CURSOR_BYTES];} xcroncalendarcursor;
XCRON_API bool xcronCalendarCursorInit(xcroncalendarcursor* Cursor,
    const xcronexpression* Expression,xtime Begin,xtime End,uint32 Flags,xcroncalendarissue* Issue);
XCRON_API bool xcronCalendarCursorInitCivil(xcroncalendarcursor* Cursor,
    const xcronexpression* Expression,const xdatetime* Begin,const xdatetime* End,uint32 Flags,xcroncalendarissue* Issue);
/* Step must match the init mode. MonthBudget>0 bounds additional month visits in
 * this call; Init's ready-match check may already count one month. Each visit uses
 * bounded bitmap/calendar arithmetic. No elapsed CPU/OS-scheduling guarantee.
 * PENDING yields with Next unchanged. FOUND alone writes Next; terminal results
 * are repeatable without more work. Issue.MonthsExamined is cumulative, at most
 * MAX_MONTHS. Normal results preserve ambient error. Argument/state errors do not
 * advance the cursor; diagnostic OOM does not erase the typed Issue. Structural
 * span/alias errors preserve all outputs. Init failures preserve Cursor. Outputs
 * must be disjoint from Cursor/each other; init also rejects aliases with inputs.
 * The existing synchronous Next functions never return PENDING. This calendar
 * primitive alone does not bound zone traversal or live scheduler owner work. */
XCRON_API xcroncalendarstatus xcronCalendarCursorStep(xcroncalendarcursor* Cursor,
    uint32 MonthBudget,xtime* Next,xcroncalendarissue* Issue);
XCRON_API xcroncalendarstatus xcronCalendarCursorStepCivil(xcroncalendarcursor* Cursor,
    uint32 MonthBudget,xdatetime* Next,xcroncalendarissue* Issue);

XRT_EXTERN_C_END
#endif
#endif
