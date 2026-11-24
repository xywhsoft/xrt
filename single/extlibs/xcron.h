/*
 * MIT License
 *
 * Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */
/* Supply XRT and selected extension dependencies before this header. */
#if !defined(XRT_CORE_H)
#error "xcron requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XCRON_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XCRON_IMPLEMENTATION) && \
	!defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
		#define _DARWIN_C_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif
#ifndef XCRON_SINGLE_HEADER_H
#define XCRON_SINGLE_HEADER_H
#define XCRON_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xcron/include/xcron/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XCRON_FEATURES_H
#define XCRON_FEATURES_H

/* xcron_scheduler 及其直接依赖。 */
#if defined(XCRON_MODULE_ALL) || defined(XCRON_MODULE_XCRON_SCHEDULER)
#ifndef XCRON_FEATURE_XCRON_SCHEDULER
#define XCRON_FEATURE_XCRON_SCHEDULER
#endif
#ifndef XCRON_MODULE_XCRON_CALENDAR
#define XCRON_MODULE_XCRON_CALENDAR
#endif
#ifndef XRT_MODULE_EVENT_WAITER
#define XRT_MODULE_EVENT_WAITER
#endif
#endif

/* xcron 及其直接依赖。 */
#if defined(XCRON_MODULE_ALL) || defined(XCRON_MODULE_XCRON)
#ifndef XCRON_FEATURE_XCRON
#define XCRON_FEATURE_XCRON
#endif
#ifndef XCRON_MODULE_XCRON_EXPRESSION
#define XCRON_MODULE_XCRON_EXPRESSION
#endif
#ifndef XCRON_MODULE_XCRON_CALENDAR
#define XCRON_MODULE_XCRON_CALENDAR
#endif
#endif

/* xcron_calendar 及其直接依赖。 */
#if defined(XCRON_MODULE_ALL) || defined(XCRON_MODULE_XCRON_CALENDAR)
#ifndef XCRON_FEATURE_XCRON_CALENDAR
#define XCRON_FEATURE_XCRON_CALENDAR
#endif
#ifndef XCRON_MODULE_XCRON_EXPRESSION
#define XCRON_MODULE_XCRON_EXPRESSION
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#endif

/* xcron_expression 及其直接依赖。 */
#if defined(XCRON_MODULE_ALL) || defined(XCRON_MODULE_XCRON_EXPRESSION)
#ifndef XCRON_FEATURE_XCRON_EXPRESSION
#define XCRON_FEATURE_XCRON_EXPRESSION
#endif
#endif

#endif /* XCRON_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xcron/include/xcron/export.h */
/* ========================================================================== */

#ifndef XCRON_EXPORT_H
#define XCRON_EXPORT_H


#if defined(_WIN32) || defined(_WIN64)
 #if defined(XCRON_BUILD_SHARED)
  #define XCRON_API __declspec(dllexport)
 #elif defined(XCRON_USE_SHARED)
  #define XCRON_API __declspec(dllimport)
 #else
  #define XCRON_API
 #endif
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(XCRON_BUILD_SHARED)
  #define XCRON_API __attribute__((visibility("default")))
 #else
  #define XCRON_API
 #endif
#else
 #define XCRON_API
#endif

#endif


/* ========================================================================== */
/* public: extlibs/xcron/include/xcron/expression.h */
/* ========================================================================== */

#ifndef XCRON_EXPRESSION_H
#define XCRON_EXPRESSION_H


#if defined(XCRON_FEATURE_XCRON_EXPRESSION)

#define XCRON_EXPRESSION_MAX_BYTES 4096u
#define XCRON_DAY_MONTH_STAR 0x01u
#define XCRON_DAY_WEEK_STAR 0x02u

typedef enum xcrondialect {
	/* minute hour day-of-month month day-of-week; second is exactly zero. */
	XCRON_UNIX5 = 1,
	/* second minute hour day-of-month month day-of-week; same day semantics. */
	XCRON_SECONDS6
} xcrondialect;

typedef enum xcronfield {
	XCRON_FIELD_SECOND = 0,
	XCRON_FIELD_MINUTE,
	XCRON_FIELD_HOUR,
	XCRON_FIELD_DAY,
	XCRON_FIELD_MONTH,
	XCRON_FIELD_WEEKDAY,
	XCRON_FIELD_COUNT,
	XCRON_FIELD_NONE = XCRON_FIELD_COUNT
} xcronfield;

/* Caller-owned value, immutable after successful compilation and freely
 * shareable/copied between threads. Fields use natural value bit positions:
 * seconds/minutes 0..59, hours 0..23, days 1..31, months 1..12, weekdays 0..6
 * (Sunday=0; input 7 is normalized to bit 0). No input/storage is retained.
 * Flags remember whether a day field contains '*' (including steps/lists).
 * Calendar day rule: if either day field has a star, both masks must match;
 * otherwise either day mask matches. An explicit full numeric range does not
 * become a star. No public structure layout depends on selected features. */
typedef struct xcronexpression {
	uint64 Fields[XCRON_FIELD_COUNT];
	uint32 Flags;
	xcrondialect Dialect;
} xcronexpression;

typedef enum xcronexpressionerror {
	XCRON_EXPRESSION_OK = 0,
	XCRON_EXPRESSION_ARGUMENT,
	XCRON_EXPRESSION_SYNTAX,
	XCRON_EXPRESSION_RANGE,
	XCRON_EXPRESSION_OVERFLOW,
	XCRON_EXPRESSION_LIMIT,
	XCRON_EXPRESSION_UNSUPPORTED
} xcronexpressionerror;

/* Zero-based byte offset at error detection; Field is NONE for a whole-input
 * error. This caller-owned diagnostic survives XRT error-allocation failure. */
typedef struct xcronexpressionissue {
	xcronexpressionerror Code;
	xcronfield Field;
	size_t Offset;
} xcronexpressionissue;

XRT_EXTERN_C_BEGIN

/* Compile exactly the explicit dialect's fields. Supports ASCII space/tab
 * separators, leading/trailing space/tab, decimal values, inclusive ascending
 * ranges, comma lists, range/star steps (positive uint32, anchored at range
 * start), case-insensitive JAN..DEC and SUN..SAT names, and Sunday 0/7. A single
 * value/step is rejected: write an explicit range/step. Steps do not carry
 * across field boundaries. Duplicate values collapse in the bitmap.
 * Supports exact lowercase @yearly/@annually/@monthly/@weekly/@daily/
 * @midnight/@hourly; seconds dialect adds second zero to those aliases.
 * @reboot, random '~', Quartz '?', 'L', 'W', '#' and year fields are outside
 * these dialects and are rejected, never silently reinterpreted. Shell commands,
 * environment assignments, CR/LF, NUL and non-ASCII input are not expressions.
 * Input is bounded by MAX_BYTES. NULL/0 is an empty expression (syntax error).
 * Expression is unchanged on failure. Issue is optional: on a valid parse call
 * it receives OK/NONE/Text.Size on success or the exact local failure; invalid
 * pointers/spans, output/input aliases or invalid dialect leave outputs intact.
 * Actual readable/writable objects are required. Success allocates nothing,
 * retains nothing and preserves ambient error. Failures report xrt.cron errors;
 * diagnostic allocation can fall back to XERR_MEMORY without losing Issue. */
XCRON_API bool xcronExpressionCompile(xstrview Text, xcrondialect Dialect,
	xcronexpression* Expression, xcronexpressionissue* Issue);

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* public: extlibs/xcron/include/xcron/calendar.h */
/* ========================================================================== */

#ifndef XCRON_CALENDAR_H
#define XCRON_CALENDAR_H

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


/* ========================================================================== */
/* public: extlibs/xcron/include/xcron/scheduler.h */
/* ========================================================================== */

#ifndef XCRON_SCHEDULER_H
#define XCRON_SCHEDULER_H


#if defined(XCRON_FEATURE_XCRON_SCHEDULER) && \
    (!defined(XCRON_FEATURE_XCRON_CALENDAR) || !defined(XRT_FEATURE_EVENT_WAITER))
#error "xcron scheduler requires calendar and XRT event waiter"
#endif

#if defined(XCRON_FEATURE_XCRON_SCHEDULER)
typedef struct xcronscheduler xcronscheduler;
typedef uint64 xcronjobid;
typedef void (*xcroncallback)(void* Context);

typedef enum xcronschedulestatus {
    XCRON_SCHEDULE_ERROR = -1,
    XCRON_SCHEDULE_IDLE = 0,
    XCRON_SCHEDULE_DISPATCHED,
    XCRON_SCHEDULE_TIMEOUT,
    XCRON_SCHEDULE_WOKEN
} xcronschedulestatus;

typedef struct xcronpoll {
    size_t Count;
    bool HasNext;
    xtime Next;
} xcronpoll;

XRT_EXTERN_C_BEGIN

/* Use one APP thread for create/add/remove/poll/wait/destroy. Callbacks execute
 * inline on that thread and must return before any of those operations on the
 * same scheduler. Wake is safe on other threads while the handle is live. Join
 * all Wake callers before destroying it. There is no scheduler worker. */
XCRON_API bool xcronSchedulerCreate(size_t Capacity, xcrondialect Dialect,
    xcronscheduler** Output);

/* Compile once. The first UTC occurrence is strictly after registration.
 * The string is borrowed only during Add. Context is borrowed until removal or
 * destruction. Failure preserves Output and leaves no job. A zero ID is invalid. */
XCRON_API bool xcronSchedulerAdd(xcronscheduler* Scheduler, xstrview Cron,
    xcroncallback Callback, void* Context, xcronjobid* Output);
XCRON_API bool xcronSchedulerRemove(xcronscheduler* Scheduler, xcronjobid Job);

/* Dispatch every job due at the sampled wall time once. Compute the next
 * occurrence strictly after that sample. Output may be NULL. Next is meaningful
 * only when HasNext is true. Poll never waits. */
XCRON_API xcronschedulestatus xcronSchedulerPoll(xcronscheduler* Scheduler,
    xcronpoll* Output);

/* Relative milliseconds. XRT_WAIT_FOREVER keeps waiting until due or woken.
 * A zero timeout dispatches due jobs and otherwise returns TIMEOUT. If a
 * finite wait's timeout precedes the projected occurrence, the occurrence
 * remains pending on timeout even after a late timer return. An explicit
 * wake interrupts the wait; the owner can drive again to recheck the wall clock. */
XCRON_API xcronschedulestatus xcronSchedulerWait(xcronscheduler* Scheduler,
    int64 Timeout, xcronpoll* Output);
XCRON_API bool xcronSchedulerWake(xcronscheduler* Scheduler);
XCRON_API bool xcronSchedulerDestroy(xcronscheduler* Scheduler);

XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xcron/include/xcron.h */
/* ========================================================================== */

#ifndef XCRON_H
#define XCRON_H


#if defined(XCRON_FEATURE_XCRON) && \
    (!defined(XCRON_FEATURE_XCRON_EXPRESSION) || !defined(XCRON_FEATURE_XCRON_CALENDAR))
#error "xcron requires expression and calendar"
#endif


#endif

#endif

#if defined(XCRON_IMPLEMENTATION) && !defined(XCRON_IMPLEMENTATION_ONCE)
#define XCRON_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xcron/src/calendar/calendar_internal.h */
/* ========================================================================== */

#if defined(XCRON_FEATURE_XCRON_CALENDAR)
#ifndef XCRON_CALENDAR_INTERNAL_H
#define XCRON_CALENDAR_INTERNAL_H
#if defined(XCRON_FEATURE_XCRON_CALENDAR)
/* Typed product-private engine; caller supplies its immutable expression. */
typedef struct xcroncalendarvalue {
    xdatetime First,End,Date;
    uint32 Months;
    int FirstWeekday;
    xcroncalendarstatus Status;
} xcroncalendarvalue;
void xcronCalendarValueInit(xcroncalendarvalue* Value,const xdatetime* First,const xdatetime* End,xcroncalendarstatus Status);
/* Valid canonical ordered civil bounds; constant impossibility proof only. */
void xcronCalendarValueBegin(const xcronexpression* Expression,const xdatetime* First,const xdatetime* End,xcroncalendarvalue* Value);
#if defined(XCRON_FEATURE_XCRON_OCCURRENCE)
/* Resume strictly after a canonical civil label, including outside xtime. */
void xcronCalendarValueAfter(const xcronexpression* Expression,const xdatetime* First,const xdatetime* End,xcroncalendarvalue* Value);
#endif
xcroncalendarstatus xcronCalendarValueStep(const xcronexpression* Expression,xcroncalendarvalue* Value,uint32 MonthBudget);
/* Product-private validated calendar operations; no XRT-private API dependency. */
bool xcronCalendarValidateExpression(const xcronexpression* Expression,xcronfield* Field);
bool xcronCalendarPartsMatch(const xcronexpression* Expression,const xdatetime* Date);
xcroncalendarstatus xcronCalendarSearch(const xcronexpression* Expression,const xdatetime* Begin,const xdatetime* End,
    uint32 Flags,xdatetime* Next,xcroncalendarissue* Issue);
#endif
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xcron/src/expression/expression.c */
/* ========================================================================== */

#if defined(XCRON_FEATURE_XCRON_EXPRESSION)
#include <string.h>

#if defined(XCRON_FEATURE_XCRON_EXPRESSION)

typedef struct xcronexpressionparser {
	xstrview Text;
	xcronfield Field;
	size_t Position, End;
	xcronexpressionissue Issue;
	cstr Message;
} xcronexpressionparser;

static bool xcronExpressionReport(xcronexpressionerror Code, cstr Message)
{
	xerrkind Kind = Code == XCRON_EXPRESSION_ARGUMENT ? XERR_ARGUMENT :
		(Code == XCRON_EXPRESSION_UNSUPPORTED ? XERR_UNSUPPORTED :
		(Code == XCRON_EXPRESSION_SYNTAX ? XERR_VALUE : XERR_RANGE));
	xrtSetErrorInfo(Kind, "xrt.cron", (int32)Code, Message);
	return false;
}

static bool xcronExpressionFail(xcronexpressionparser* Parser, xcronexpressionerror Code,
	size_t Offset, cstr Message)
{
	Parser->Issue = (xcronexpressionissue){Code, Parser->Field, Offset};
	Parser->Message = Message;
	return false;
}

static bool xcronExpressionBlank(char Byte)
{
	return Byte == ' ' || Byte == '\t';
}

static bool xcronExpressionUnsupported(char Byte)
{
	return Byte == '?' || Byte == 'L' || Byte == 'l' || Byte == 'W' || Byte == 'w' || Byte == '#' || Byte == '~' || Byte == '@';
}

static bool xcronExpressionNumber(xcronexpressionparser* Parser, uint32* Number)
{
	size_t Start = Parser->Position;
	uint32 Value = 0;
	while (Parser->Position < Parser->End) {
		unsigned char Byte = (unsigned char)Parser->Text.Data[Parser->Position];
		if (Byte < '0' || Byte > '9') break;
		uint32 Digit = (uint32)(Byte - '0');
		if (Value > (UINT32_MAX - Digit) / 10u)
			return xcronExpressionFail(Parser, XCRON_EXPRESSION_OVERFLOW, Parser->Position, "cron decimal exceeds uint32");
		Value = Value * 10u + Digit; ++Parser->Position;
	}
	if (Parser->Position == Start)
		return xcronExpressionFail(Parser, XCRON_EXPRESSION_SYNTAX, Start, "cron requires decimal digits");
	*Number = Value;
	return true;
}

static bool xcronExpressionValue(xcronexpressionparser* Parser, uint32 Minimum, uint32 Maximum,
	uint32* Value)
{
	static const char Months[] = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";
	static const char Weekdays[] = "SUNMONTUEWEDTHUFRISAT";
	size_t Start = Parser->Position;
	if (Start == Parser->End)
		return xcronExpressionFail(Parser, XCRON_EXPRESSION_SYNTAX, Start, "missing cron field value");
	unsigned char Byte = (unsigned char)Parser->Text.Data[Start];
	if (Byte >= '0' && Byte <= '9') {
		if (!xcronExpressionNumber(Parser, Value)) return false;
	} else {
		const char* Names = Parser->Field == XCRON_FIELD_MONTH ? Months : Weekdays;
		size_t Count = Parser->Field == XCRON_FIELD_MONTH ? 12u : 7u;
		bool Found = false;
		if ((Parser->Field == XCRON_FIELD_MONTH || Parser->Field == XCRON_FIELD_WEEKDAY) && Parser->End - Start >= 3u) {
			for (size_t Name = 0; Name < Count; ++Name) {
				bool Equal = true;
				for (size_t Index = 0; Index < 3u; ++Index) {
					unsigned char Character = (unsigned char)Parser->Text.Data[Start + Index];
					if (Character >= 'a' && Character <= 'z') Character = (unsigned char)(Character - ('a' - 'A'));
					if (Character != (unsigned char)Names[Name * 3u + Index]) Equal = false;
				}
				if (Equal) { *Value = (uint32)Name + (Parser->Field == XCRON_FIELD_MONTH ? 1u : 0u); Found = true; break; }
			}
		}
		if (!Found)
			return xcronExpressionFail(Parser, xcronExpressionUnsupported((char)Byte) ? XCRON_EXPRESSION_UNSUPPORTED : XCRON_EXPRESSION_SYNTAX,
				Start, "unknown cron value/name or unsupported dialect operator");
		Parser->Position += 3u;
	}
	if (*Value < Minimum || *Value > Maximum)
		return xcronExpressionFail(Parser, XCRON_EXPRESSION_RANGE, Start, "cron field value is out of range");
	return true;
}

static bool xcronExpressionField(xcronexpressionparser* Parser, xcronexpression* Expression)
{
	static const uint32 Minimum[] = {0, 0, 0, 1, 1, 0};
	static const uint32 Maximum[] = {59, 59, 23, 31, 12, 7};
	uint64 Mask = 0;
	for (;;) {
		uint32 First, Last, Step = 1;
		bool Range;
		if (Parser->Position == Parser->End)
			return xcronExpressionFail(Parser, XCRON_EXPRESSION_SYNTAX, Parser->Position, "empty cron list item");
		if (Parser->Text.Data[Parser->Position] == '*') {
			++Parser->Position; First = Minimum[Parser->Field]; Last = Maximum[Parser->Field]; Range = true;
			if (Parser->Field == XCRON_FIELD_DAY) Expression->Flags |= XCRON_DAY_MONTH_STAR;
			if (Parser->Field == XCRON_FIELD_WEEKDAY) Expression->Flags |= XCRON_DAY_WEEK_STAR;
		} else {
			if (!xcronExpressionValue(Parser, Minimum[Parser->Field], Maximum[Parser->Field], &First)) return false;
			Last = First; Range = false;
			if (Parser->Position < Parser->End && Parser->Text.Data[Parser->Position] == '-') {
				++Parser->Position; Range = true;
				if (!xcronExpressionValue(Parser, Minimum[Parser->Field], Maximum[Parser->Field], &Last)) return false;
				if (Last < First)
					return xcronExpressionFail(Parser, XCRON_EXPRESSION_RANGE, Parser->Position, "cron ranges must be ascending");
			}
		}
		if (Parser->Position < Parser->End && Parser->Text.Data[Parser->Position] == '/') {
			size_t Slash = Parser->Position++;
			if (!Range) return xcronExpressionFail(Parser, XCRON_EXPRESSION_SYNTAX, Slash, "cron step requires a range or star");
			if (!xcronExpressionNumber(Parser, &Step)) return false;
			if (Step == 0) return xcronExpressionFail(Parser, XCRON_EXPRESSION_RANGE, Parser->Position, "cron step must be positive");
		}
		for (uint32 Value = First;;) {
			Mask |= (uint64)1u << ((Parser->Field == XCRON_FIELD_WEEKDAY && Value == 7u) ? 0u : Value);
			if (Step > Last - Value) break;
			Value += Step;
		}
		if (Parser->Position == Parser->End) { Expression->Fields[Parser->Field] = Mask; return true; }
		char Separator = Parser->Text.Data[Parser->Position];
		if (Separator != ',') return xcronExpressionFail(Parser,
			xcronExpressionUnsupported(Separator) ? XCRON_EXPRESSION_UNSUPPORTED : XCRON_EXPRESSION_SYNTAX,
			Parser->Position, "invalid cron field separator or unsupported dialect operator");
		++Parser->Position;
	}
}

static bool xcronExpressionParse(xcronexpressionparser* Parser, xcronexpression* Expression)
{
	size_t Position = 0;
	for (int Field = Expression->Dialect == XCRON_UNIX5 ? XCRON_FIELD_MINUTE : XCRON_FIELD_SECOND; Field < XCRON_FIELD_COUNT; ++Field) {
		Parser->Field = (xcronfield)Field;
		while (Position < Parser->Text.Size && xcronExpressionBlank(Parser->Text.Data[Position])) ++Position;
		if (Position == Parser->Text.Size)
			return xcronExpressionFail(Parser, XCRON_EXPRESSION_SYNTAX, Position, "missing cron expression field");
		Parser->Position = Position;
		while (Position < Parser->Text.Size && !xcronExpressionBlank(Parser->Text.Data[Position])) ++Position;
		Parser->End = Position;
		if (!xcronExpressionField(Parser, Expression)) return false;
	}
	while (Position < Parser->Text.Size && xcronExpressionBlank(Parser->Text.Data[Position])) ++Position;
	if (Position != Parser->Text.Size) {
		Parser->Field = XCRON_FIELD_NONE;
		return xcronExpressionFail(Parser, XCRON_EXPRESSION_SYNTAX, Position, "extra cron field or command text");
	}
	return true;
}

static cstr xcronExpressionAlias(xstrview Text, xcrondialect Dialect)
{
	static const struct { cstr Name; size_t Size; cstr Unix; cstr Seconds; } Aliases[] = {
		{"@yearly", 7, "0 0 1 1 *", "0 0 0 1 1 *"}, {"@annually", 9, "0 0 1 1 *", "0 0 0 1 1 *"},
		{"@monthly", 8, "0 0 1 * *", "0 0 0 1 * *"}, {"@weekly", 7, "0 0 * * 0", "0 0 0 * * 0"},
		{"@daily", 6, "0 0 * * *", "0 0 0 * * *"}, {"@midnight", 9, "0 0 * * *", "0 0 0 * * *"},
		{"@hourly", 7, "0 * * * *", "0 0 * * * *"}
	};
	for (size_t Index = 0; Index < sizeof(Aliases) / sizeof(Aliases[0]); ++Index)
		if (Text.Size == Aliases[Index].Size && memcmp(Text.Data, Aliases[Index].Name, Text.Size) == 0)
			return Dialect == XCRON_UNIX5 ? Aliases[Index].Unix : Aliases[Index].Seconds;
	return NULL;
}

XCRON_API bool xcronExpressionCompile(xstrview Text, xcrondialect Dialect,
	xcronexpression* Expression, xcronexpressionissue* Issue)
{
	xcronexpressionparser Parser = {0}; xcronexpression Result = {0};
	if (Expression == NULL || !xrtMemRangeValid(Expression, sizeof(*Expression)) ||
		(Text.Data == NULL && Text.Size != 0) || !xrtMemRangeValid(Text.Data, Text.Size) ||
		(Issue != NULL && !xrtMemRangeValid(Issue, sizeof(*Issue))) ||
		xrtMemRangesOverlap(Text.Data, Text.Size, Expression, sizeof(*Expression)) ||
		(Issue != NULL && (xrtMemRangesOverlap(Text.Data, Text.Size, Issue, sizeof(*Issue)) ||
		 xrtMemRangesOverlap(Expression, sizeof(*Expression), Issue, sizeof(*Issue)))) ||
		(Dialect != XCRON_UNIX5 && Dialect != XCRON_SECONDS6))
		return xcronExpressionReport(XCRON_EXPRESSION_ARGUMENT, "invalid cron input, outputs, overlap or dialect");
	Parser.Text = Text; Parser.Field = XCRON_FIELD_NONE;
	Result.Dialect = Dialect; Result.Fields[XCRON_FIELD_SECOND] = 1u;
	if (Text.Size > XCRON_EXPRESSION_MAX_BYTES) {
		xcronExpressionFail(&Parser, XCRON_EXPRESSION_LIMIT, XCRON_EXPRESSION_MAX_BYTES, "cron expression exceeds byte limit");
		goto failed;
	}
	size_t Start = 0, End = Text.Size;
	while (Start < End && xcronExpressionBlank(Text.Data[Start])) ++Start;
	while (End > Start && xcronExpressionBlank(Text.Data[End - 1u])) --End;
	if (Start != End && Text.Data[Start] == '@') {
		cstr Expansion = xcronExpressionAlias((xstrview){Text.Data + Start, End - Start}, Dialect);
		if (Expansion == NULL) {
			xcronExpressionFail(&Parser, XCRON_EXPRESSION_UNSUPPORTED, Start, "unknown cron alias or non-calendar reboot event");
			goto failed;
		}
		Parser.Text = (xstrview){Expansion, strlen(Expansion)};
	}
	if (!xcronExpressionParse(&Parser, &Result)) goto failed;
	*Expression = Result;
	if (Issue != NULL) *Issue = (xcronexpressionissue){XCRON_EXPRESSION_OK, XCRON_FIELD_NONE, Text.Size};
	return true;
failed:
	if (Issue != NULL) *Issue = Parser.Issue;
	return xcronExpressionReport(Parser.Issue.Code, Parser.Message);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xcron/src/calendar/calendar.c */
/* ========================================================================== */

#if defined(XCRON_FEATURE_XCRON_CALENDAR)
#include <string.h>

#if defined(XCRON_FEATURE_XCRON_CALENDAR)

static bool xcronCalendarReport(xcroncalendarerror Code, xcronfield Field,
	xcroncalendarissue* Issue, cstr Message)
{
	if (Issue!=NULL) *Issue=(xcroncalendarissue){Code,Field,0};
	xrtSetErrorInfo(Code==XCRON_CALENDAR_STATE ? XERR_STATE : Code==XCRON_CALENDAR_ARGUMENT ? XERR_ARGUMENT : Code==XCRON_CALENDAR_EXPRESSION ? XERR_VALUE : XERR_RANGE,
		"xrt.cron",(int32)Code,Message); return false;
}

static bool xcronCalendarArgs(const xcronexpression* Expression, const xdatetime* Begin, const xdatetime* End,
	uint32 Flags, void* Output, size_t Size, xcroncalendarissue* Issue)
{
	if (Expression==NULL || !xrtMemRangeValid(Expression,sizeof(*Expression)) || Output==NULL || !xrtMemRangeValid(Output,Size) ||
		(Issue!=NULL && !xrtMemRangeValid(Issue,sizeof(*Issue))) || (Flags&~XCRON_SEARCH_EXCLUSIVE) ||
		(Begin!=NULL && !xrtMemRangeValid(Begin,sizeof(*Begin))) || (End!=NULL && !xrtMemRangeValid(End,sizeof(*End)))) goto Invalid;
	const void* Inputs[]={Expression,Begin,End}; size_t Sizes[]={sizeof(*Expression),Begin ? sizeof(*Begin) : 0,End ? sizeof(*End) : 0};
	for (size_t Index=0; Index<3; ++Index)
		if (xrtMemRangesOverlap(Output,Size,Inputs[Index],Sizes[Index]) || (Issue!=NULL && xrtMemRangesOverlap(Issue,sizeof(*Issue),Inputs[Index],Sizes[Index]))) goto Invalid;
	if (Issue!=NULL && xrtMemRangesOverlap(Output,Size,Issue,sizeof(*Issue))) goto Invalid;
	return true;
Invalid:
	return xcronCalendarReport(XCRON_CALENDAR_ARGUMENT,XCRON_FIELD_NONE,NULL,"invalid cron calendar arguments, spans or output aliases");
}

bool xcronCalendarValidateExpression(const xcronexpression* Expression,xcronfield* BadField)
{
	*BadField=XCRON_FIELD_NONE;
	static const uint64 Allowed[]={UINT64_C(0x0fffffffffffffff),UINT64_C(0x0fffffffffffffff),UINT64_C(0xffffff),
		UINT64_C(0xfffffffe),UINT64_C(0x1ffe),UINT64_C(0x7f)};
	if ((Expression->Dialect!=XCRON_UNIX5 && Expression->Dialect!=XCRON_SECONDS6) || (Expression->Flags&~(XCRON_DAY_MONTH_STAR|XCRON_DAY_WEEK_STAR)))
		return false;
	for (unsigned Field=0; Field<XCRON_FIELD_COUNT; ++Field)
		if (!Expression->Fields[Field] || (Expression->Fields[Field]&~Allowed[Field]) ||
			(Field==XCRON_FIELD_SECOND && Expression->Dialect==XCRON_UNIX5 && Expression->Fields[Field]!=1))
			{ *BadField=(xcronfield)Field; return false; }
	return true;
}
static bool xcronCalendarExpression(const xcronexpression* Expression,xcroncalendarissue* Issue)
{
	xcronfield Field;
	return xcronCalendarValidateExpression(Expression,&Field) || xcronCalendarReport(XCRON_CALENDAR_EXPRESSION,Field,Issue,
		Field==XCRON_FIELD_NONE?"invalid compiled cron dialect/day flags":"invalid compiled cron field bitmap");
}

static bool xcronCalendarCivil(const xdatetime* Date, xdatetime* Value, xcroncalendarissue* Issue)
{
	if (!Date->Year || Date->Month<1 || Date->Month>12 || Date->Day<1 || Date->Day>xrtDaysInMonth(Date->Year,Date->Month) ||
		Date->Hour<0 || Date->Hour>23 || Date->Minute<0 || Date->Minute>59 || Date->Second<0 || Date->Second>59 || Date->Millisecond<0 || Date->Millisecond>999)
		return xcronCalendarReport(XCRON_CALENDAR_DATE,XCRON_FIELD_NONE,Issue,"invalid Gregorian civil date/time");
	/* Astronomical-year cycle, preserving XRT's no-civil-year-zero convention.
	 * This also handles civil labels just beyond an int64 timestamp endpoint. */
	int64 Cycle=(Date->Year<0 ? Date->Year+1 : Date->Year)%400; if (Cycle<0) Cycle+=400;
	xtime Safe; (void)xrtDateTime(2000+Cycle,Date->Month,Date->Day,Date->Hour,Date->Minute,Date->Second,Date->Millisecond,&Safe);
	(void)xrtTimeSplit(Safe,Value); Value->Year=Date->Year;
	return true;
}

bool xcronCalendarPartsMatch(const xcronexpression* Expression, const xdatetime* Date)
{
	if (Date->Millisecond || !(Expression->Fields[XCRON_FIELD_MONTH]&((uint64)1u<<Date->Month)) ||
		!(Expression->Fields[XCRON_FIELD_HOUR]&((uint64)1u<<Date->Hour)) || !(Expression->Fields[XCRON_FIELD_MINUTE]&((uint64)1u<<Date->Minute)) ||
		!(Expression->Fields[XCRON_FIELD_SECOND]&((uint64)1u<<Date->Second))) return false;
	bool MonthDay=(Expression->Fields[XCRON_FIELD_DAY]&((uint64)1u<<Date->Day))!=0;
	bool WeekDay=(Expression->Fields[XCRON_FIELD_WEEKDAY]&((uint64)1u<<Date->Weekday))!=0;
	return Expression->Flags&(XCRON_DAY_MONTH_STAR|XCRON_DAY_WEEK_STAR) ? MonthDay && WeekDay : MonthDay || WeekDay;
}

/* Nonzero bitmap, portable bounded binary scan (no private XRT intrinsics). */
static int xcronCalendarFirst(uint64 Mask)
{
	int Bit=0;
	if (!(Mask&UINT64_C(0xffffffff))) { Mask>>=32; Bit+=32; }
	if (!(Mask&UINT64_C(0xffff))) { Mask>>=16; Bit+=16; }
	if (!(Mask&UINT64_C(0xff))) { Mask>>=8; Bit+=8; }
	if (!(Mask&15u)) { Mask>>=4; Bit+=4; }
	if (!(Mask&3u)) { Mask>>=2; Bit+=2; }
	return Bit+(!(Mask&1u));
}

static int xcronCalendarFind(uint64 Mask, int Begin)
{
	if (Begin>=64) return -1;
	Mask&=UINT64_MAX<<Begin; return Mask ? xcronCalendarFirst(Mask) : -1;
}

static uint64 xcronCalendarDays(const xcronexpression* Expression, int Days, int FirstWeekday)
{
	uint64 Allowed=((uint64)1u<<(Days+1))-2u, Week=Expression->Fields[XCRON_FIELD_WEEKDAY];
	Week=((Week>>FirstWeekday)|(Week<<(7-FirstWeekday)))&127u;
	uint64 WeekDays=(Week|(Week<<7)|(Week<<14)|(Week<<21)|(Week<<28))<<1;
	uint64 MonthDays=Expression->Fields[XCRON_FIELD_DAY];
	return (Expression->Flags&(XCRON_DAY_MONTH_STAR|XCRON_DAY_WEEK_STAR) ? MonthDays&WeekDays : MonthDays|WeekDays)&Allowed;
}

static bool xcronCalendarTime(const xcronexpression* Expression, int Hour, int Minute, int Second, xdatetime* Date)
{
	for (int H=xcronCalendarFind(Expression->Fields[XCRON_FIELD_HOUR],Hour); H>=0; H=xcronCalendarFind(Expression->Fields[XCRON_FIELD_HOUR],H+1))
		for (int M=xcronCalendarFind(Expression->Fields[XCRON_FIELD_MINUTE],H==Hour ? Minute : 0); M>=0; M=xcronCalendarFind(Expression->Fields[XCRON_FIELD_MINUTE],M+1)) {
			int S=xcronCalendarFind(Expression->Fields[XCRON_FIELD_SECOND],H==Hour && M==Minute ? Second : 0);
			if (S>=0) { Date->Hour=H; Date->Minute=M; Date->Second=S; Date->Millisecond=0; return true; }
		}
	return false;
}

static int xcronCalendarCompare(const xdatetime* Left, const xdatetime* Right)
{
	if (Left->Year!=Right->Year) return Left->Year<Right->Year ? -1 : 1;
	const int L[]={Left->Month,Left->Day,Left->Hour,Left->Minute,Left->Second,Left->Millisecond};
	const int R[]={Right->Month,Right->Day,Right->Hour,Right->Minute,Right->Second,Right->Millisecond};
	for (unsigned Index=0; Index<6; ++Index) if (L[Index]!=R[Index]) return L[Index]<R[Index] ? -1 : 1;
	return 0;
}

static bool xcronCalendarTick(xdatetime* Date)
{
	Date->Millisecond=0;
	if (++Date->Second<=59) return true;
	Date->Second=0; if (++Date->Minute<=59) return true;
	Date->Minute=0; if (++Date->Hour<=23) return true;
	Date->Hour=0; Date->Weekday=(Date->Weekday+1)%7;
	if (++Date->Day<=xrtDaysInMonth(Date->Year,Date->Month)) return true;
	Date->Day=1; if (++Date->Month<=12) return true;
	Date->Month=1; if (Date->Year==INT64_MAX) return false;
	if (++Date->Year==0) Date->Year=1;
	return true;
}

/* Keep the shared loop/start in the synchronous fast path on optimizing targets. */
#if defined(_MSC_VER)
#define XCRON_CALENDAR_INLINE static __forceinline
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
#define XCRON_CALENDAR_INLINE static inline __attribute__((always_inline))
#else
#define XCRON_CALENDAR_INLINE static inline
#endif
static bool xcronCalendarMayExist(const xcronexpression* E)
{
    uint64 Months=E->Fields[XCRON_FIELD_MONTH];
    if((Months&UINT64_C(0x15aa)) || !(E->Flags&(XCRON_DAY_MONTH_STAR|XCRON_DAY_WEEK_STAR)) || (E->Fields[XCRON_FIELD_DAY]&2u))return true;
    uint64 Allowed=(Months&UINT64_C(0x0a50))?UINT64_C(0x7ffffffe):UINT64_C(0x3ffffffe);
    return (E->Fields[XCRON_FIELD_DAY]&Allowed)!=0;
}
XCRON_CALENDAR_INLINE xcroncalendarstatus xcronCalendarSearchStart(const xcronexpression* E,const xdatetime* Begin,const xdatetime* End,
    uint32 Flags,xdatetime* First,xcroncalendarissue* Issue)
{
    if(xcronCalendarCompare(Begin,End)>0){xcronCalendarReport(XCRON_CALENDAR_WINDOW,XCRON_FIELD_NONE,Issue,"cron search window is reversed");return XCRON_CALENDAR_ERROR;}
    if(Issue)*Issue=(xcroncalendarissue){XCRON_CALENDAR_OK,XCRON_FIELD_NONE,0};
    *First=*Begin;
    if((First->Millisecond || (Flags&XCRON_SEARCH_EXCLUSIVE)) && !xcronCalendarTick(First))return XCRON_CALENDAR_NONE;
    if(xcronCalendarCompare(First,End)>0)return XCRON_CALENDAR_NONE;
    if(xcronCalendarPartsMatch(E,First)){if(Issue)Issue->MonthsExamined=1;return XCRON_CALENDAR_FOUND;}
    if(!xcronCalendarMayExist(E))return XCRON_CALENDAR_NONE;
    return XCRON_CALENDAR_PENDING;
}
void xcronCalendarValueInit(xcroncalendarvalue* V,const xdatetime* First,const xdatetime* End,xcroncalendarstatus Status)
{
    V->First=V->Date=*First;V->End=*End;V->Status=Status;V->Months=Status==XCRON_CALENDAR_FOUND?1:0;
    V->FirstWeekday=Status==XCRON_CALENDAR_PENDING?(First->Weekday+7-((First->Day-1)%7))%7:0;
}
XCRON_CALENDAR_INLINE xcroncalendarstatus xcronCalendarMonths(const xcronexpression* E,const xdatetime* First,
    const xdatetime* End,xdatetime* InOutDate,uint32* InOutMonths,int* InOutWeekday,uint32 Budget)
{
    xdatetime D=*InOutDate;uint32 Months=*InOutMonths;int Weekday=*InOutWeekday;
    const uint64 MonthMask=E->Fields[XCRON_FIELD_MONTH];xcroncalendarstatus Status=XCRON_CALENDAR_PENDING;
    uint32 Limit=XCRON_CALENDAR_MAX_MONTHS;if(Budget<Limit-Months)Limit=Months+Budget;
    for(;Months<Limit;++Months){
        if(D.Year>End->Year || (D.Year==End->Year && D.Month>End->Month)){Status=XCRON_CALENDAR_NONE;break;}
        int Days=xrtDaysInMonth(D.Year,D.Month);
        if(MonthMask&((uint64)1u<<D.Month)){
            uint64 Mask=xcronCalendarDays(E,Days,Weekday);
            for(int Day=xcronCalendarFind(Mask,Months==0?First->Day:1);Day>=0;Day=xcronCalendarFind(Mask,Day+1)){
                D.Day=Day;bool Same=Months==0 && Day==First->Day;
                if(!xcronCalendarTime(E,Same?First->Hour:0,Same?First->Minute:0,Same?First->Second:0,&D))continue;
                ++Months;Status=xcronCalendarCompare(&D,End)>0?XCRON_CALENDAR_NONE:XCRON_CALENDAR_FOUND;goto Done;
            }
        }
        Weekday=(Weekday+Days)%7;
        if(++D.Month>12){D.Month=1;if(D.Year==INT64_MAX){++Months;Status=XCRON_CALENDAR_NONE;goto Done;}if(++D.Year==0)D.Year=1;}
    }
    if(Months==XCRON_CALENDAR_MAX_MONTHS || D.Year>End->Year || (D.Year==End->Year && D.Month>End->Month))Status=XCRON_CALENDAR_NONE;
Done:
    *InOutDate=D;*InOutMonths=Months;*InOutWeekday=Weekday;return Status;
}
xcroncalendarstatus xcronCalendarValueStep(const xcronexpression* E,xcroncalendarvalue* V,uint32 Budget)
{
    if(V->Status!=XCRON_CALENDAR_PENDING || !Budget)return V->Status;
    V->Status=xcronCalendarMonths(E,&V->First,&V->End,&V->Date,&V->Months,&V->FirstWeekday,Budget);return V->Status;
}
void xcronCalendarValueBegin(const xcronexpression* E,const xdatetime* First,const xdatetime* End,xcroncalendarvalue* V)
{xcronCalendarValueInit(V,First,End,xcronCalendarMayExist(E)?XCRON_CALENDAR_PENDING:XCRON_CALENDAR_NONE);}
#if defined(XCRON_FEATURE_XCRON_OCCURRENCE)
void xcronCalendarValueAfter(const xcronexpression* E,const xdatetime* First,const xdatetime* End,xcroncalendarvalue* V)
{
    xdatetime Next=*First;
    if(!xcronCalendarTick(&Next)){xcronCalendarValueInit(V,&Next,End,XCRON_CALENDAR_NONE);return;}
    xcronCalendarValueBegin(E,&Next,End,V);
}
#endif
/* Keep the legacy synchronous control loop specialized: measured cursor-state
 * setup/copy overhead is material for a single-month search. Date/bitmap rules
 * are shared; independent oracles cover both control paths. */
xcroncalendarstatus xcronCalendarSearch(const xcronexpression* Expression, const xdatetime* Begin, const xdatetime* End,
	uint32 Flags, xdatetime* Next, xcroncalendarissue* Issue)
{
	if (xcronCalendarCompare(Begin,End)>0) { xcronCalendarReport(XCRON_CALENDAR_WINDOW,XCRON_FIELD_NONE,Issue,"cron search window is reversed"); return XCRON_CALENDAR_ERROR; }
	if (Issue!=NULL) *Issue=(xcroncalendarissue){XCRON_CALENDAR_OK,XCRON_FIELD_NONE,0};
	xdatetime First=*Begin;
	if ((First.Millisecond || (Flags&XCRON_SEARCH_EXCLUSIVE)) && !xcronCalendarTick(&First)) return XCRON_CALENDAR_NONE;
	if (xcronCalendarCompare(&First,End)>0) return XCRON_CALENDAR_NONE;
	if (xcronCalendarPartsMatch(Expression,&First)) {
		*Next=First; if (Issue!=NULL) Issue->MonthsExamined=1; return XCRON_CALENDAR_FOUND;
	}
	if (!xcronCalendarMayExist(Expression)) return XCRON_CALENDAR_NONE;
	xdatetime Date=First;
	int FirstWeekday=(First.Weekday+7-((First.Day-1)%7))%7;
	for (uint32 Month=0; Month<XCRON_CALENDAR_MAX_MONTHS; ++Month) {
		if (Date.Year>End->Year || (Date.Year==End->Year && Date.Month>End->Month)) return XCRON_CALENDAR_NONE;
		if (Issue!=NULL) Issue->MonthsExamined=Month+1u;
		int Days=xrtDaysInMonth(Date.Year,Date.Month);
		if (Expression->Fields[XCRON_FIELD_MONTH]&((uint64)1u<<Date.Month)) {
			uint64 Mask=xcronCalendarDays(Expression,Days,FirstWeekday);
			for (int Day=xcronCalendarFind(Mask,Month==0 ? First.Day : 1); Day>=0; Day=xcronCalendarFind(Mask,Day+1)) {
				Date.Day=Day; bool Same=Month==0 && Day==First.Day;
				if (!xcronCalendarTime(Expression,Same ? First.Hour : 0,Same ? First.Minute : 0,Same ? First.Second : 0,&Date)) continue;
				if (xcronCalendarCompare(&Date,End)>0) return XCRON_CALENDAR_NONE;
				*Next=Date;
				return XCRON_CALENDAR_FOUND;
			}
		}
		FirstWeekday=(FirstWeekday+Days)%7;
		if (++Date.Month>12) { Date.Month=1; if (Date.Year==INT64_MAX) return XCRON_CALENDAR_NONE; if (++Date.Year==0) Date.Year=1; }
	}
	return XCRON_CALENDAR_NONE;
}

XCRON_API bool xcronCalendarMatch(const xcronexpression* Expression, xtime When, bool* Matches, xcroncalendarissue* Issue)
{
	if (!xcronCalendarArgs(Expression,NULL,NULL,0,Matches,sizeof(*Matches),Issue) || !xcronCalendarExpression(Expression,Issue)) return false;
	xdatetime Date; (void)xrtTimeSplit(When,&Date); *Matches=xcronCalendarPartsMatch(Expression,&Date);
	if (Issue!=NULL) *Issue=(xcroncalendarissue){XCRON_CALENDAR_OK,XCRON_FIELD_NONE,0};
	return true;
}

XCRON_API xcroncalendarstatus xcronCalendarNext(const xcronexpression* Expression, xtime Begin, xtime End, uint32 Flags, xtime* Next, xcroncalendarissue* Issue)
{
	if (!xcronCalendarArgs(Expression,NULL,NULL,Flags,Next,sizeof(*Next),Issue) || !xcronCalendarExpression(Expression,Issue)) return XCRON_CALENDAR_ERROR;
	xdatetime First, Last, Date; (void)xrtTimeSplit(Begin,&First); (void)xrtTimeSplit(End,&Last);
	xcroncalendarstatus Status=xcronCalendarSearch(Expression,&First,&Last,Flags,&Date,Issue);
	if (Status==XCRON_CALENDAR_FOUND) {
		/* Ordered bounds guarantee a representable candidate, including partial
		 * endpoint days; never attempt an unrepresentable midnight. */
		xtime Value;
		if (!xrtDateTime(Date.Year,Date.Month,Date.Day,Date.Hour,Date.Minute,Date.Second,0,&Value)) {
			xcronCalendarReport(XCRON_CALENDAR_RANGE,XCRON_FIELD_NONE,Issue,"cron candidate exceeds xtime range"); return XCRON_CALENDAR_ERROR;
		}
		*Next=Value;
	}
	return Status;
}

XCRON_API bool xcronCalendarMatchCivil(const xcronexpression* Expression, const xdatetime* When, bool* Matches, xcroncalendarissue* Issue)
{
	if (When==NULL) return xcronCalendarReport(XCRON_CALENDAR_ARGUMENT,XCRON_FIELD_NONE,NULL,"NULL cron civil input");
	if (!xcronCalendarArgs(Expression,When,NULL,0,Matches,sizeof(*Matches),Issue) || !xcronCalendarExpression(Expression,Issue)) return false;
	xdatetime Date; if (!xcronCalendarCivil(When,&Date,Issue)) return false;
	*Matches=xcronCalendarPartsMatch(Expression,&Date);
	if (Issue!=NULL) *Issue=(xcroncalendarissue){XCRON_CALENDAR_OK,XCRON_FIELD_NONE,0};
	return true;
}

XCRON_API xcroncalendarstatus xcronCalendarNextCivil(const xcronexpression* Expression, const xdatetime* Begin, const xdatetime* End,
	uint32 Flags, xdatetime* Next, xcroncalendarissue* Issue)
{
	if (Begin==NULL || End==NULL) { xcronCalendarReport(XCRON_CALENDAR_ARGUMENT,XCRON_FIELD_NONE,NULL,"NULL cron civil window"); return XCRON_CALENDAR_ERROR; }
	if (!xcronCalendarArgs(Expression,Begin,End,Flags,Next,sizeof(*Next),Issue) || !xcronCalendarExpression(Expression,Issue)) return XCRON_CALENDAR_ERROR;
	xdatetime First, Last, Value;
	if (!xcronCalendarCivil(Begin,&First,Issue) || !xcronCalendarCivil(End,&Last,Issue)) return XCRON_CALENDAR_ERROR;
	xcroncalendarstatus Status=xcronCalendarSearch(Expression,&First,&Last,Flags,&Value,Issue);
	if (Status==XCRON_CALENDAR_FOUND) (void)xcronCalendarCivil(&Value,Next,NULL);
	return Status;
}

#define XCRON_CALENDAR_CURSOR_MAGIC UINT32_C(0x58434331)
typedef struct xcroncalendarimpl {
    uint32 Magic,Civil;
    uintptr_t Address;
    xcronexpression Expression;
    xcroncalendarvalue Search;
} xcroncalendarimpl;
typedef char xcron_calendar_cursor_size_check[(sizeof(xcroncalendarimpl)<=sizeof(xcroncalendarcursor))?1:-1];
/* Encode/decode through memcpy: no private-struct lvalue aliases opaque bytes. */
static bool xcronCalendarCursorInitValue(xcroncalendarcursor* C,const xcronexpression* E,
    const xdatetime* First,const xdatetime* Last,uint32 Flags,uint32 Civil,xcroncalendarissue* Issue)
{
    xcroncalendarimpl I;memset(&I,0,sizeof(I));I.Magic=XCRON_CALENDAR_CURSOR_MAGIC;I.Address=(uintptr_t)C;I.Civil=Civil;I.Expression=*E;
    xdatetime Rounded;xcroncalendarissue Work;xcroncalendarstatus Status=xcronCalendarSearchStart(&I.Expression,First,Last,Flags,&Rounded,&Work);
    if(Status==XCRON_CALENDAR_ERROR){if(Issue)*Issue=Work;return false;}
    xcronCalendarValueInit(&I.Search,&Rounded,Last,Status);memset(C,0,sizeof(*C));memcpy(C,&I,sizeof(I));if(Issue)*Issue=Work;return true;
}
XCRON_API bool xcronCalendarCursorInit(xcroncalendarcursor* C,const xcronexpression* E,xtime Begin,xtime End,uint32 Flags,xcroncalendarissue* Issue)
{
    if(!xcronCalendarArgs(E,NULL,NULL,Flags,C,sizeof(*C),Issue) || !xcronCalendarExpression(E,Issue))return false;
    xdatetime First,Last;(void)xrtTimeSplit(Begin,&First);(void)xrtTimeSplit(End,&Last);
    return xcronCalendarCursorInitValue(C,E,&First,&Last,Flags,0,Issue);
}
XCRON_API bool xcronCalendarCursorInitCivil(xcroncalendarcursor* C,const xcronexpression* E,const xdatetime* Begin,const xdatetime* End,uint32 Flags,xcroncalendarissue* Issue)
{
    if(!Begin || !End)return xcronCalendarReport(XCRON_CALENDAR_ARGUMENT,XCRON_FIELD_NONE,NULL,"NULL cursor civil window");
    if(!xcronCalendarArgs(E,Begin,End,Flags,C,sizeof(*C),Issue) || !xcronCalendarExpression(E,Issue))return false;
    xdatetime First,Last;if(!xcronCalendarCivil(Begin,&First,Issue) || !xcronCalendarCivil(End,&Last,Issue))return false;
    return xcronCalendarCursorInitValue(C,E,&First,&Last,Flags,1,Issue);
}
static xcroncalendarstatus xcronCalendarCursorStepValue(xcroncalendarcursor* C,uint32 Budget,void* Next,uint32 Civil,xcroncalendarissue* Issue)
{
    size_t Bytes=Civil?sizeof(xdatetime):sizeof(xtime);
    if(!C || !Next || !xrtMemRangeValid(C,sizeof(*C)) || !xrtMemRangeValid(Next,Bytes) ||
        (Issue && !xrtMemRangeValid(Issue,sizeof(*Issue))) || xrtMemRangesOverlap(C,sizeof(*C),Next,Bytes) ||
        (Issue && (xrtMemRangesOverlap(Issue,sizeof(*Issue),C,sizeof(*C)) || xrtMemRangesOverlap(Issue,sizeof(*Issue),Next,Bytes)))){
        xcronCalendarReport(XCRON_CALENDAR_ARGUMENT,XCRON_FIELD_NONE,NULL,"invalid cursor spans/output aliases");return XCRON_CALENDAR_ERROR;
    }
    if(!Budget){xcronCalendarReport(XCRON_CALENDAR_ARGUMENT,XCRON_FIELD_NONE,Issue,"cursor month budget must be positive");return XCRON_CALENDAR_ERROR;}
    xcroncalendarimpl I;memcpy(&I,C,sizeof(I));
    if(I.Magic!=XCRON_CALENDAR_CURSOR_MAGIC || I.Address!=(uintptr_t)C || I.Civil!=Civil || I.Search.Months>XCRON_CALENDAR_MAX_MONTHS ||
        (I.Search.Status!=XCRON_CALENDAR_PENDING && I.Search.Status!=XCRON_CALENDAR_FOUND && I.Search.Status!=XCRON_CALENDAR_NONE)){
        xcronCalendarReport(XCRON_CALENDAR_STATE,XCRON_FIELD_NONE,Issue,"uninitialized/copied cursor or wrong step mode");return XCRON_CALENDAR_ERROR;
    }
    xcroncalendarstatus Status=xcronCalendarValueStep(&I.Expression,&I.Search,Budget);xdatetime Canonical;xtime Time=0;
    if(Status==XCRON_CALENDAR_FOUND){
        if(Civil){if(!xcronCalendarCivil(&I.Search.Date,&Canonical,Issue))return XCRON_CALENDAR_ERROR;}
        else if(!xrtDateTime(I.Search.Date.Year,I.Search.Date.Month,I.Search.Date.Day,I.Search.Date.Hour,I.Search.Date.Minute,I.Search.Date.Second,0,&Time)){
            xcronCalendarReport(XCRON_CALENDAR_RANGE,XCRON_FIELD_NONE,Issue,"cursor candidate exceeds timestamp range");return XCRON_CALENDAR_ERROR;
        }
    }
    memcpy(C,&I,sizeof(I));if(Status==XCRON_CALENDAR_FOUND){if(Civil)memcpy(Next,&Canonical,sizeof(Canonical));else memcpy(Next,&Time,sizeof(Time));}
    if(Issue)*Issue=(xcroncalendarissue){XCRON_CALENDAR_OK,XCRON_FIELD_NONE,I.Search.Months};
    return Status;
}
XCRON_API xcroncalendarstatus xcronCalendarCursorStep(xcroncalendarcursor* C,uint32 Budget,xtime* Next,xcroncalendarissue* Issue)
{return xcronCalendarCursorStepValue(C,Budget,Next,0,Issue);}
XCRON_API xcroncalendarstatus xcronCalendarCursorStepCivil(xcroncalendarcursor* C,uint32 Budget,xdatetime* Next,xcroncalendarissue* Issue)
{return xcronCalendarCursorStepValue(C,Budget,Next,1,Issue);}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xcron/src/scheduler/scheduler.c */
/* ========================================================================== */

#if defined(XCRON_FEATURE_XCRON_SCHEDULER)
#include <math.h>
#include <string.h>

#if defined(XCRON_FEATURE_XCRON_SCHEDULER)

#define XCRON_NO_SLOT UINT32_MAX

typedef struct xcronslot {
    xcronexpression Expression;
    xcroncallback Callback;
    void* Context;
    xtime Next;
    uint32 Serial;
    uint32 Position;
} xcronslot;

struct xcronscheduler {
    xcronslot* Jobs;
    uint32* Heap;
    uint32 Capacity;
    uint32 Count;
    uint32 Free;
    uint32 Serial;
    xcrondialect Dialect;
    bool OrderedWave;
    bool Driving;
    bool HasWaiter;
    xevent Wake;
    xeventwaiter Waiter;
};

static bool xcronScheduleError(xerrkind Kind, cstr Message)
{
    xrtSetErrorInfo(Kind, "xrt.cron.scheduler", (int32)Kind, Message);
    return false;
}

static bool xcronScheduleOwner(xcronscheduler* S)
{
    if (!S) return xcronScheduleError(XERR_ARGUMENT, "scheduler required");
    if (S->Driving) return xcronScheduleError(XERR_STATE, "scheduler drive is active");
    return true;
}

static bool xcronScheduleEarlier(const xcronscheduler* S, uint32 A, uint32 B)
{
    const xcronslot* L = &S->Jobs[A];
    const xcronslot* R = &S->Jobs[B];
    return L->Next < R->Next || (L->Next == R->Next && L->Serial < R->Serial);
}

static void xcronSchedulePlace(xcronscheduler* S, uint32 Position, uint32 Slot)
{
    S->Heap[Position] = Slot;
    S->Jobs[Slot].Position = Position;
}

static void xcronScheduleUp(xcronscheduler* S, uint32 Position, uint32 Slot)
{
    while (Position) {
        uint32 Parent = (Position - 1) / 2;
        uint32 Other = S->Heap[Parent];
        if (!xcronScheduleEarlier(S, Slot, Other)) break;
        xcronSchedulePlace(S, Position, Other);
        Position = Parent;
    }
    xcronSchedulePlace(S, Position, Slot);
}

static void xcronScheduleDown(xcronscheduler* S, uint32 Position, uint32 Slot)
{
    while (Position < S->Count / 2) {
        uint32 Child = Position * 2 + 1;
        if (Child + 1 < S->Count &&
            xcronScheduleEarlier(S, S->Heap[Child + 1], S->Heap[Child]))
            ++Child;
        uint32 Other = S->Heap[Child];
        if (!xcronScheduleEarlier(S, Other, Slot)) break;
        xcronSchedulePlace(S, Position, Other);
        Position = Child;
    }
    xcronSchedulePlace(S, Position, Slot);
}

static void xcronScheduleRemoveSlot(xcronscheduler* S, uint32 Slot)
{
    xcronslot* Job = &S->Jobs[Slot];
    uint32 Position = Job->Position;
    uint32 Last = S->Heap[--S->Count];
    if (Position < S->Count) {
        if (Position && xcronScheduleEarlier(S, Last, S->Heap[(Position - 1) / 2]))
            xcronScheduleUp(S, Position, Last);
        else
            xcronScheduleDown(S, Position, Last);
    }
    *Job = (xcronslot){.Position = S->Free};
    S->Free = Slot;
    S->OrderedWave = false;
}

XCRON_API bool xcronSchedulerCreate(size_t Capacity, xcrondialect Dialect, xcronscheduler** Output)
{
    if (!xrtMemRangeValid(Output, sizeof(*Output)) || !Capacity || Capacity >= UINT32_MAX ||
        Capacity > SIZE_MAX / sizeof(xcronslot) || Capacity > SIZE_MAX / sizeof(uint32) ||
        (Dialect != XCRON_UNIX5 && Dialect != XCRON_SECONDS6))
        return xcronScheduleError(XERR_ARGUMENT, "invalid scheduler capacity, dialect or output");

    xcronscheduler* S = xrtCalloc(1, sizeof(*S));
    if (!S) return false;
    S->Jobs = xrtCalloc(Capacity, sizeof(*S->Jobs));
    if (S->Jobs) S->Heap = xrtMalloc(Capacity * sizeof(*S->Heap));
    if (!S->Heap || !xrtEventInit(&S->Wake, false, false)) {
        xrtFree(S->Heap);
        xrtFree(S->Jobs);
        xrtFree(S);
        return false;
    }
    S->Capacity = (uint32)Capacity;
    S->Dialect = Dialect;
    for (uint32 I = 0; I < S->Capacity; ++I)
        S->Jobs[I].Position = I + 1;
    S->Jobs[Capacity - 1].Position = XCRON_NO_SLOT;
    *Output = S;
    return true;
}

XCRON_API bool xcronSchedulerAdd(xcronscheduler* S, xstrview Text, xcroncallback Callback,
    void* Context, xcronjobid* Output)
{
    if (!xcronScheduleOwner(S)) return false;
    if (!Callback || !xrtMemRangeValid(Output, sizeof(*Output)) ||
        xrtMemRangesOverlap(Text.Data, Text.Size, Output, sizeof(*Output)))
        return xcronScheduleError(XERR_ARGUMENT, "invalid scheduler callback or job output");
    if (S->Free == XCRON_NO_SLOT || S->Serial == UINT32_MAX)
        return xcronScheduleError(XERR_RANGE, "scheduler job capacity or identifier limit reached");

    xcronexpression Expression;
    if (!xcronExpressionCompile(Text, S->Dialect, &Expression, NULL)) return false;
    xtime Now = xrtNow();
    if (!Now) return xcronScheduleError(XERR_IO, "wall clock unavailable");
    xtime Next;
    xcroncalendarstatus Search = xcronCalendarNext(&Expression, Now, INT64_MAX,
        XCRON_SEARCH_EXCLUSIVE, &Next, NULL);
    if (Search == XCRON_CALENDAR_ERROR) return false;
    if (Search != XCRON_CALENDAR_FOUND)
        return xcronScheduleError(XERR_RANGE, "cron expression has no future occurrence");

    uint32 Slot = S->Free;
    S->Free = S->Jobs[Slot].Position;
    uint32 Serial = ++S->Serial;
    S->Jobs[Slot] = (xcronslot){.Expression = Expression, .Callback = Callback,
        .Context = Context, .Next = Next, .Serial = Serial};
    xcronScheduleUp(S, S->Count++, Slot);
    S->OrderedWave = false;
    *Output = ((uint64)Serial << 32) | ((uint64)Slot + 1);
    return true;
}

XCRON_API bool xcronSchedulerRemove(xcronscheduler* S, xcronjobid Job)
{
    if (!xcronScheduleOwner(S)) return false;
    uint32 Slot = (uint32)Job - 1;
    uint32 Serial = (uint32)(Job >> 32);
    if (Slot >= S->Capacity || !Serial || S->Jobs[Slot].Serial != Serial)
        return xcronScheduleError(XERR_ARGUMENT, "unknown scheduler job");
    xcronScheduleRemoveSlot(S, Slot);
    return true;
}

static bool xcronScheduleOrderedWave(const xcronscheduler* S, const xcronslot* First)
{
    if (S->Jobs[S->Heap[S->Count - 1]].Next != First->Next) return false;
    uint32 Serial = First->Serial;
    for (uint32 I = 1; I < S->Count; ++I) {
        const xcronslot* Job = &S->Jobs[S->Heap[I]];
        if (Job->Next != First->Next || Job->Serial <= Serial ||
            Job->Expression.Flags != First->Expression.Flags ||
            memcmp(Job->Expression.Fields, First->Expression.Fields, sizeof(First->Expression.Fields)))
            return false;
        Serial = Job->Serial;
    }
    return true;
}

static xcronschedulestatus xcronScheduleDue(xcronscheduler* S, xtime Now, xcronpoll* Result)
{
    xcronpoll Report = {0};
    const xcronexpression* Previous = NULL;
    xtime Next = 0;
    while (S->Count) {
        uint32 Slot = S->Heap[0];
        xcronslot* Job = &S->Jobs[Slot];
        if (Job->Next > Now) break;
        xcroncallback Callback = Job->Callback;
        void* Context = Job->Context;
        /* Now and the scheduler dialect are fixed for the wave. Pending
         * expressions stay immutable while callbacks run. */
        if (!Previous || Previous->Flags != Job->Expression.Flags ||
            memcmp(Previous->Fields, Job->Expression.Fields, sizeof(Previous->Fields))) {
            xcroncalendarstatus Search = xcronCalendarNext(&Job->Expression, Now, INT64_MAX,
                XCRON_SEARCH_EXCLUSIVE, &Next, NULL);
            if (Search == XCRON_CALENDAR_ERROR) return XCRON_SCHEDULE_ERROR;
            Previous = Search == XCRON_CALENDAR_FOUND ? &Job->Expression : NULL;
        }
        if (Previous) {
            if (!Report.Count && S->Count > 1 &&
                (S->OrderedWave || xcronScheduleOrderedWave(S, Job))) {
                /* Equal future deadlines preserve the validated order until a job is added or removed. */
                S->OrderedWave = true;
                for (uint32 I = 0; I < S->Count; ++I) {
                    xcronslot* Pending = &S->Jobs[S->Heap[I]];
                    Pending->Next = Next;
                    Pending->Callback(Pending->Context);
                }
                Report.Count = S->Count;
                break;
            }
            Job->Next = Next;
            xcronScheduleDown(S, 0, Slot);
        } else {
            xcronScheduleRemoveSlot(S, Slot);
        }
        ++Report.Count;
        Callback(Context);
    }
    if (S->Count) {
        Report.HasNext = true;
        Report.Next = S->Jobs[S->Heap[0]].Next;
    }
    *Result = Report;
    return Report.Count ? XCRON_SCHEDULE_DISPATCHED : XCRON_SCHEDULE_IDLE;
}

static double xcronScheduleDeadline(double Sample, uint64 DelayMs, uint32 SubNs)
{
    double Delay = (double)(DelayMs / 1000) + (double)(DelayMs % 1000) * 0.001;
    return Sample + (Delay - (double)SubNs * 1e-9);
}

static xcronschedulestatus xcronScheduleDrive(xcronscheduler* S, int64 Timeout,
    bool Wait, xcronpoll* Output)
{
    if (!xcronScheduleOwner(S)) return XCRON_SCHEDULE_ERROR;
    if ((Output && !xrtMemRangeValid(Output, sizeof(*Output))) || Timeout < XRT_WAIT_FOREVER) {
        xcronScheduleError(XERR_ARGUMENT, "invalid scheduler timeout or output");
        return XCRON_SCHEDULE_ERROR;
    }

    S->Driving = true;
    bool Blocking = Wait && Timeout != 0;
    /* Take the monotonic anchor before UTC. A pause after the UTC sample
     * must not extend the absolute deadline. Recheck UTC after every expiry. */
    double Sample = Blocking && (Timeout > 0 || S->Count) ? xrtTimer() : 0;
    xcronpoll Report;
    xcronschedulestatus Status = XCRON_SCHEDULE_ERROR;
    if (Timeout > 0 && !isfinite(Sample)) goto Done;
    double Limit = Timeout > 0 ? xcronScheduleDeadline(Sample, (uint64)Timeout, 0) : INFINITY;
    for (;;) {
        uint32 SubNs = 0;
        xtime Now = 0;
        if (S->Count) {
            Now = Blocking ? __xrtNowWithSubMs(&SubNs) : xrtNow();
            if (!Now) {
                xcronScheduleError(XERR_IO, "wall clock unavailable");
                Status = XCRON_SCHEDULE_ERROR;
                break;
            }
        }
        Status = xcronScheduleDue(S, Now, &Report);
        if (Status != XCRON_SCHEDULE_IDLE || !Wait) break;

        if (Timeout == 0) { Status = XCRON_SCHEDULE_TIMEOUT; break; }
        double Deadline = Limit;
        bool CallerFirst = true;
        if (Timeout > 0 || Report.HasNext) {
            double Current = Timeout > 0 ? xrtTimer() : Sample;
            if (!isfinite(Sample) || !isfinite(Current)) { Status = XCRON_SCHEDULE_ERROR; break; }
            if (Report.HasNext) {
                uint64 WallDelay = (uint64)Report.Next - (uint64)Now;
                if (WallDelay > INT64_MAX) WallDelay = INT64_MAX;
                double Due = xcronScheduleDeadline(Sample, WallDelay, SubNs);
                CallerFirst = Limit < Due;
                Due = nextafter(Due, INFINITY);
                if (Due < Deadline) Deadline = Due;
            }
            if (Current >= Limit) {
                /* A pause before arming can cross both deadlines. Resample
                 * UTC once if the occurrence was projected to come first. */
                if (!CallerFirst && Sample < Limit) {
                    Sample = Current;
                    continue;
                }
                Status = XCRON_SCHEDULE_TIMEOUT;
                break;
            }
        } else {
            Status = xrtEventWait(&S->Wake) == XWAIT_OK ? XCRON_SCHEDULE_WOKEN : XCRON_SCHEDULE_ERROR;
            break;
        }
        if (!S->HasWaiter) {
            if (!xrtEventWaiterInit(&S->Waiter, &S->Wake, 0)) {
                Status = XCRON_SCHEDULE_ERROR;
                break;
            }
            S->HasWaiter = true;
        }
        xwaitresult Result = __xrtEventWaiterWaitUntil(&S->Waiter, Deadline);
        if (Result == XWAIT_OK) { Status = XCRON_SCHEDULE_WOKEN; break; }
        if (Result != XWAIT_TIMEOUT) { Status = XCRON_SCHEDULE_ERROR; break; }
        Sample = xrtTimer();
        if (!isfinite(Sample)) { Status = XCRON_SCHEDULE_ERROR; break; }
        /* A timer return can be delayed past both deadlines. */
        if (Timeout > 0 && CallerFirst && Sample >= Limit) {
            Status = XCRON_SCHEDULE_TIMEOUT;
            break;
        }
    }
Done:
    S->Driving = false;
    if (Status != XCRON_SCHEDULE_ERROR && Output) *Output = Report;
    return Status;
}

XCRON_API xcronschedulestatus xcronSchedulerPoll(xcronscheduler* S, xcronpoll* Output)
{
    return xcronScheduleDrive(S, 0, false, Output);
}

XCRON_API xcronschedulestatus xcronSchedulerWait(xcronscheduler* S, int64 Timeout,
    xcronpoll* Output)
{
    return xcronScheduleDrive(S, Timeout, true, Output);
}

XCRON_API bool xcronSchedulerWake(xcronscheduler* S)
{
    return S ? xrtEventSet(&S->Wake) : xcronScheduleError(XERR_ARGUMENT, "scheduler required");
}

XCRON_API bool xcronSchedulerDestroy(xcronscheduler* S)
{
    if (!xcronScheduleOwner(S)) return false;
    if (S->HasWaiter) {
        if (!xrtEventWaiterUnit(&S->Waiter)) return false;
        S->HasWaiter = false;
    }
    if (!xrtEventUnit(&S->Wake)) return false;
    xrtFree(S->Heap);
    xrtFree(S->Jobs);
    xrtFree(S);
    return true;
}

#endif
#endif

#endif
