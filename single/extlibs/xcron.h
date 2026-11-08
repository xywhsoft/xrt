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
#ifndef XCRON_MODULE_XCRON_TIMEZONE
#define XCRON_MODULE_XCRON_TIMEZONE
#endif
#ifndef XCRON_MODULE_XCRON_ZONED
#define XCRON_MODULE_XCRON_ZONED
#endif
#ifndef XCRON_MODULE_XCRON_SCHEDULER
#define XCRON_MODULE_XCRON_SCHEDULER
#endif
#ifndef XCRON_MODULE_XCRON_EXECUTOR
#define XCRON_MODULE_XCRON_EXECUTOR
#endif
#ifndef XCRON_MODULE_XCRON_SERVICE
#define XCRON_MODULE_XCRON_SERVICE
#endif
#ifndef XCRON_MODULE_XCRON_TZDB
#define XCRON_MODULE_XCRON_TZDB
#endif
#ifndef XCRON_MODULE_XCRON_TZDB_FILE
#define XCRON_MODULE_XCRON_TZDB_FILE
#endif
#endif

/* xcron_tzdb_file 及其直接依赖。 */
#if defined(XCRON_MODULE_ALL) || defined(XCRON_MODULE_XCRON_TZDB_FILE)
#ifndef XCRON_FEATURE_XCRON_TZDB_FILE
#define XCRON_FEATURE_XCRON_TZDB_FILE
#endif
#ifndef XCRON_MODULE_XCRON_TZDB
#define XCRON_MODULE_XCRON_TZDB
#endif
#ifndef XRT_MODULE_FILE_ROOT
#define XRT_MODULE_FILE_ROOT
#endif
#endif

/* xcron_tzdb 及其直接依赖。 */
#if defined(XCRON_MODULE_ALL) || defined(XCRON_MODULE_XCRON_TZDB)
#ifndef XCRON_FEATURE_XCRON_TZDB
#define XCRON_FEATURE_XCRON_TZDB
#endif
#ifndef XCRON_MODULE_XCRON_TIMEZONE
#define XCRON_MODULE_XCRON_TIMEZONE
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#endif

/* xcron_service 及其直接依赖。 */
#if defined(XCRON_MODULE_ALL) || defined(XCRON_MODULE_XCRON_SERVICE)
#ifndef XCRON_FEATURE_XCRON_SERVICE
#define XCRON_FEATURE_XCRON_SERVICE
#endif
#ifndef XCRON_MODULE_XCRON_EXECUTOR
#define XCRON_MODULE_XCRON_EXECUTOR
#endif
#ifndef XRT_MODULE_COND
#define XRT_MODULE_COND
#endif
#ifndef XRT_MODULE_EVENT_WAITER
#define XRT_MODULE_EVENT_WAITER
#endif
#endif

/* xcron_executor 及其直接依赖。 */
#if defined(XCRON_MODULE_ALL) || defined(XCRON_MODULE_XCRON_EXECUTOR)
#ifndef XCRON_FEATURE_XCRON_EXECUTOR
#define XCRON_FEATURE_XCRON_EXECUTOR
#endif
#ifndef XCRON_MODULE_XCRON_SCHEDULER
#define XCRON_MODULE_XCRON_SCHEDULER
#endif
#ifndef XRT_MODULE_EXECUTOR
#define XRT_MODULE_EXECUTOR
#endif
#endif

/* xcron_scheduler 及其直接依赖。 */
#if defined(XCRON_MODULE_ALL) || defined(XCRON_MODULE_XCRON_SCHEDULER)
#ifndef XCRON_FEATURE_XCRON_SCHEDULER
#define XCRON_FEATURE_XCRON_SCHEDULER
#endif
#ifndef XCRON_MODULE_XCRON_ZONED
#define XCRON_MODULE_XCRON_ZONED
#endif
#endif

/* xcron_zoned 及其直接依赖。 */
#if defined(XCRON_MODULE_ALL) || defined(XCRON_MODULE_XCRON_ZONED)
#ifndef XCRON_FEATURE_XCRON_ZONED
#define XCRON_FEATURE_XCRON_ZONED
#endif
#ifndef XCRON_MODULE_XCRON_CALENDAR
#define XCRON_MODULE_XCRON_CALENDAR
#endif
#ifndef XCRON_MODULE_XCRON_TIMEZONE
#define XCRON_MODULE_XCRON_TIMEZONE
#endif
#endif

/* xcron_timezone 及其直接依赖。 */
#if defined(XCRON_MODULE_ALL) || defined(XCRON_MODULE_XCRON_TIMEZONE)
#ifndef XCRON_FEATURE_XCRON_TIMEZONE
#define XCRON_FEATURE_XCRON_TIMEZONE
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
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
/* public: extlibs/xcron/include/xcron/timezone.h */
/* ========================================================================== */

#ifndef XCRON_TIMEZONE_H
#define XCRON_TIMEZONE_H
#if defined(XCRON_FEATURE_XCRON_TIMEZONE) && (!defined(XRT_FEATURE_TIME) || !defined(XRT_FEATURE_ATOMIC))
#error "cron timezone requires time and atomic"
#endif
#if defined(XCRON_FEATURE_XCRON_TIMEZONE)
#define XCRON_ZONE_TEXT_MAX 4096u
#define XCRON_ZONE_NAME_MAX 255u
#define XCRON_ZONE_TZIF_MAX 16777216u
#define XCRON_ZONE_TRANSITIONS_MAX 1048576u
/* RFC 9636 forbids INT32_MIN as a real offset. Used only in transitions
 * entering/leaving an unspecified interval; its IsDST is false and name -00. */
#define XCRON_ZONE_OFFSET_UNKNOWN INT32_MIN
typedef struct xcronzone xcronzone;
typedef enum xcronzonegrammar {XCRON_ZONE_POSIX=2,XCRON_ZONE_TZIF3=3} xcronzonegrammar;
typedef enum xcronzonestatus {XCRON_ZONE_CAPACITY=-2,XCRON_ZONE_ERROR=-1,XCRON_ZONE_NONE=0,XCRON_ZONE_FOUND=1} xcronzonestatus;
typedef enum xcronzoneerror {XCRON_ZONE_OK=0,XCRON_ZONE_ARGUMENT=2001,XCRON_ZONE_SYNTAX,XCRON_ZONE_LIMIT,XCRON_ZONE_DATE,XCRON_ZONE_MEMORY,XCRON_ZONE_RULES_REQUIRED,XCRON_ZONE_TZIF,XCRON_ZONE_SCALE,XCRON_ZONE_UNKNOWN} xcronzoneerror;
typedef struct xcronzoneissue {xcronzoneerror Code;size_t Offset;uint32 YearsExamined;} xcronzoneissue;
/* Abbreviation borrows this immutable zone and requires a live retained owner. */
typedef struct xcronzonestate {int32 Offset;bool IsDST;xstrview Abbreviation;} xcronzonestate;
typedef struct xcronzonetransition {xtime When;xcronzonestate Before,After;} xcronzonetransition;
XRT_EXTERN_C_BEGIN
/* Independent POSIX TZ rules, not a path, TZ environment or system-zone lookup.
 * Std/dst ASCII names (including quoted alphanumeric +/-), offsets, Jn/n/Mm.w.d,
 * optional transition times and negative/all-year DST. TZIF3 permits signed
 * -167..167 transition hours; POSIX permits unsigned 0..24. DST without explicit
 * rules is RULES_REQUIRED, never an implementation-defined ambient default.
 * One allocation owns names and an optional immutable 400-year transition cache.
 * Full parse precedes allocation; outputs change on success only. Value failures
 * publish Issue independently of diagnostic OOM; invalid spans/aliases leave
 * both outputs unchanged. Success preserves ambient error. No reference code or
 * timezone data is embedded. Use CreateTzif for owned binary history data. */
XCRON_API bool xcronZoneCreatePosix(xstrview Text,xcronzonegrammar Grammar,xcronzone** Zone,xcronzoneissue* Issue);
/* RFC 9636 application/tzif v1..4 bytes, not a pathname or host database name.
 * Validate/decode an immutable owned snapshot, skipping the legacy v1 block in
 * newer files. Full int32 offsets except INT32_MIN, suffix-shared/empty names,
 * strict counts/order/flags/footer consistency and full int64 transition seconds.
 * Unsafe/nonportable designation bytes use a derived numeric designation.
 * Resource limits: 16MiB file, 1048576 transitions, 256 types, 65536 designation
 * bytes, 4096 leap records, 255-byte normalized names and 4096-byte footer.
 * Leap-aware files are structurally checked but currently return SCALE: XRT timestamps
 * exclude leap corrections. Leap-aware conversion/expiry remains pending.
 * No file IO, TZ state or retained input. One history allocation plus an
 * optional independently owned footer rule allocation. Outputs/OOM follow the
 * same atomic factory contract as CreatePosix; Issue.Offset is a file offset. */
XCRON_API bool xcronZoneCreateTzif(xbytesview Data,xcronzone** Zone,xcronzoneissue* Issue);
XCRON_API bool xcronZoneRetain(const xcronzone* Zone);
XCRON_API void xcronZoneRelease(const xcronzone* Zone);
/* Thread-safe immutable queries, full signed-int64 XRT millisecond timeline
 * (0001-01-01 epoch; Unix epoch is XRT_TIME_UNIX_EPOCH, not zero).
 * No allocation on normal success/NONE/CAPACITY, no TZ/global libc state.
 * Caller holds Zone throughout each call. Outputs/Issue must be disjoint from
 * owned Zone storage, civil input and one another. Invalid API args preserve all
 * outputs. Offset is seconds east of UTC; abbreviation scope is documented above.
 * At/Split fail atomically with UNKNOWN for -00 or missing future rules. They
 * never extrapolate the final table type beyond RFC's advertised validity. */
XCRON_API bool xcronZoneAt(const xcronzone* Zone,xtime When,xcronzonestate* State,xcronzoneissue* Issue);
/* First effective state change strictly after After, or NONE without changing
 * Transition. Coincident transitions are evaluated together; perpetual rules
 * return NONE. Gregorian footer completeness is bounded at 403 rule years.
 * Unknown-interval boundaries are changes whose state Offset uses the reserved
 * sentinel above. History is prefiltered to effective changes at creation. */
XCRON_API xcronzonestatus xcronZoneNextTransition(const xcronzone* Zone,xtime After,xcronzonetransition* Transition,xcronzoneissue* Issue);
/* Gregorian decomposition using the zone; offsets beyond 24h, int64 timestamp
 * endpoints and the no-civil-year-zero convention are supported independently
 * of the core fixed-offset API's narrower offset range. */
XCRON_API bool xcronZoneSplit(const xcronzone* Zone,xtime When,xdatetime* Date,xcronzoneissue* Issue);
/* Resolve a floating label, ignoring input Offset/IsDST/derived fields. Returns
 * every matching instant in ascending order, with no hidden fold/gap policy.
 * A gap/out-of-timeline label returns NONE and Count=0. CAPACITY sets required
 * Count and Issue but leaves Candidates unchanged; NULL,0 is a sizing query.
 * FOUND writes Count and candidates. ERROR preserves Count/Candidates. Abnormal
 * value errors may allocate a diagnostic, never affect the typed Issue.
 * TZif may produce more than two candidates. Resolution fails UNKNOWN if an
 * unspecified interval could map to the label under any legal int32 offset;
 * it does not silently publish an incomplete list or misclassify it as a gap. */
XCRON_API xcronzonestatus xcronZoneResolve(const xcronzone* Zone,const xdatetime* Local,xtime* Candidates,size_t Capacity,size_t* Count,xcronzoneissue* Issue);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xcron/include/xcron/zoned.h */
/* ========================================================================== */

#ifndef XCRON_ZONED_H
#define XCRON_ZONED_H
#if defined(XCRON_FEATURE_XCRON_ZONED) && (!defined(XCRON_FEATURE_XCRON_CALENDAR) || !defined(XCRON_FEATURE_XCRON_TIMEZONE))
#error "zoned cron requires calendar and timezone"
#endif
#if defined(XCRON_FEATURE_XCRON_ZONED)
typedef enum xcronzonedstatus {XCRON_ZONED_ERROR=-1,XCRON_ZONED_NONE=0,XCRON_ZONED_FOUND=1,XCRON_ZONED_PENDING=2} xcronzonedstatus;
typedef enum xcronzonederror {XCRON_ZONED_OK=0,XCRON_ZONED_ARGUMENT=3001,XCRON_ZONED_EXPRESSION,XCRON_ZONED_WINDOW,XCRON_ZONED_ZONE,XCRON_ZONED_RANGE,XCRON_ZONED_STATE} xcronzonederror;
typedef struct xcronzonedissue {
    xcronzonederror Code;
    xcronfield Field;
    xcronzoneerror Zone;
    uint32 SegmentsExamined;
    uint64 MonthsExamined;
} xcronzonedissue;
/* Independent positive per-Step budgets. A segment start includes bounded
 * state/directory lookup and a ready-match check. RuleYears counts footer/POSIX
 * transition-search years, not the fixed five-year state lookup. Months counts
 * calendar visits (or one for an immediate match). Counters are cumulative. */
typedef struct xcronzonedbudget {uint32 Segments,Months,RuleYears;} xcronzonedbudget;
/* Separate type preserves the existing synchronous Issue ABI. */
typedef struct xcronzonedcursorissue {
    xcronzonederror Code;
    xcronfield Field;
    xcronzoneerror Zone;
    uint32 SegmentsExamined;
    uint64 MonthsExamined,RuleYearsExamined;
} xcronzonedcursorissue;
#define XCRON_ZONED_CURSOR_BYTES 512u
typedef union xcronzonedcursor {uint64 Alignment;uint8 Storage[XCRON_ZONED_CURSOR_BYTES];} xcronzonedcursor;
XRT_EXTERN_C_BEGIN
/* Actual-instant predicate, not nominal-time compensation. Local labels absent
 * from the zone have no occurrence; all repeated instants match independently,
 * including histories with more than two folds. Fractional instants never match.
 * Unknown mapping at a whole-second instant is a ZONE/UNKNOWN error; a fractional
 * instant is false without needing an offset. Caller owns live Zone and keeps
 * both inputs immutable throughout. No input is retained or mutated. Outputs
 * and Issue must be disjoint from Expression, all owned Zone/footer storage,
 * and each other. Invalid arguments/spans/aliases preserve both outputs.
 * Value failures publish typed Issue even if diagnostic allocation fails;
 * Matches changes on success only. Normal success preserves error/allocates none. */
XCRON_API bool xcronZonedMatch(const xcronexpression* Expression,const xcronzone* Zone,xtime When,bool* Matches,xcronzonedissue* Issue);
/* Earliest actual whole-second occurrence in inclusive [Begin,End], or
 * (Begin,End] with XCRON_SEARCH_EXCLUSIVE. Fractional/negative endpoints and the
 * entire int64 XRT timeline are supported. NONE/ERROR leave Next unchanged.
 * Ordered UTC fixed-offset segments prevent a later fold from being missed.
 * Search completes history first; only a proven periodic suffix permits a full
 * 400-year recurrence bound. There is no arbitrary lookahead cutoff. Reaching
 * unknown coverage before proving the earliest result is ZONE/UNKNOWN, never a
 * guessed NONE. Empty rounded windows need no mapping. Normal FOUND/NONE allocate
 * nothing and preserve ambient error; no clock, waiting, executor or storage.
 * Remapped gap runs, fold selection, misfires and delivery policies belong to
 * the scheduler layer and remain separate from this actual-occurrence primitive.
 * Issue counters include visited UTC segments/calendar month work; Field applies
 * to EXPRESSION errors, Zone to ZONE errors. Error domain is xrt.cron.zoned. */
XCRON_API xcronzonedstatus xcronZonedNext(const xcronexpression* Expression,const xcronzone* Zone,xtime Begin,xtime End,uint32 Flags,xtime* Next,xcronzonedissue* Issue);
/* Resumable earliest actual occurrence with the same gap/fold/unknown/history
 * semantics as Next. Init copies Expression and bounds, records Zone identity,
 * rounds bounds and calculates the periodic suffix limit; no segment, rule-year
 * or calendar search is performed. No allocation, retention, lock or Unit duty.
 * Keep Cursor at a stable address; copying/relocation/modification is invalid.
 * Init may reset exclusive storage. Expression storage may subsequently change.
 * Cursor BORROWS Zone: caller keeps that exact immutable owner alive and passes
 * it to every Step, including terminal reads and serialized thread handoff.
 * Valid outputs/Issue must avoid Cursor, all owned zone/footer storage, each
 * other, and (for Init) Expression. Structural errors preserve all outputs;
 * value errors publish typed Issue even under diagnostic OOM. Init failure
 * preserves Cursor. Existing synchronous functions never return PENDING. */
XCRON_API bool xcronZonedCursorInit(xcronzonedcursor* Cursor,const xcronexpression* Expression,const xcronzone* Zone,
    xtime Begin,xtime End,uint32 Flags,xcronzonedcursorissue* Issue);
/* Every budget field must be positive. Each cumulative counter increases by at
 * most its budget per call. PENDING leaves Next unchanged; FOUND alone writes
 * the earliest result. Terminal results/issues are repeatable without more work.
 * Argument/state failures do not advance Cursor. Normal PENDING/FOUND/NONE
 * preserve ambient error and request no heap. Other errors may allocate a
 * diagnostic. Work units bound algorithmic traversal, not elapsed CPU or OS
 * scheduling; this primitive alone does not bound live scheduler admission. */
XCRON_API xcronzonedstatus xcronZonedCursorStep(xcronzonedcursor* Cursor,const xcronzone* Zone,xcronzonedbudget Budget,
    xtime* Next,xcronzonedcursorissue* Issue);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xcron/include/xcron/scheduler.h */
/* ========================================================================== */

#ifndef XCRON_SCHEDULER_H
#define XCRON_SCHEDULER_H
#if defined(XCRON_FEATURE_XCRON_SCHEDULER) && !defined(XCRON_FEATURE_XCRON_ZONED)
#error "cron scheduler requires zoned calculation"
#endif
#if defined(XCRON_FEATURE_XCRON_SCHEDULER)
typedef struct xcronscheduler xcronscheduler;
typedef uint64 xcronjobid;
typedef uint64 xcronrunid;
typedef enum xcronmisfire {XCRON_MISFIRE_CATCH_UP=0,XCRON_MISFIRE_SKIP,XCRON_MISFIRE_COALESCE} xcronmisfire;
typedef enum xcronoverlap {XCRON_OVERLAP_WAIT=0,XCRON_OVERLAP_ALLOW,XCRON_OVERLAP_SKIP} xcronoverlap;
typedef enum xcronjobstate {XCRON_JOB_SCHEDULED=0,XCRON_JOB_EXHAUSTED,XCRON_JOB_CANCELLED,XCRON_JOB_FAULTED,XCRON_JOB_SEARCHING} xcronjobstate;
typedef enum xcronrunresult {XCRON_RUN_SUCCEEDED=0,XCRON_RUN_FAILED,XCRON_RUN_CANCELLED} xcronrunresult;
typedef enum xcronschedulererror {
    XCRON_SCHEDULER_OK=0,XCRON_SCHEDULER_ARGUMENT=4001,XCRON_SCHEDULER_LIMIT,
    XCRON_SCHEDULER_MEMORY,XCRON_SCHEDULER_CLOSED,XCRON_SCHEDULER_STALE,
    XCRON_SCHEDULER_BUSY,XCRON_SCHEDULER_CLOCK,XCRON_SCHEDULER_CALCULATION
} xcronschedulererror;
typedef struct xcronschedulerissue {xcronschedulererror Code;xcronzonedissue Zoned;} xcronschedulerissue;
typedef struct xcronschedulerconfig {
    uint32 JobLimit,RunLimit,WorkLimit,Reserved;
    size_t BytesLimit;
} xcronschedulerconfig;
typedef struct xcronclock {xtime Wall;uint64 MonotonicMilliseconds;} xcronclock;
typedef struct xcronjobconfig {
    xcronexpression Expression;
    xcronzone* Zone;
    xtime Begin,End;
    uint32 SearchFlags;
    xcronmisfire Misfire;
    xcronoverlap Overlap;
    uint32 RunningLimit;
    uint64 GraceMilliseconds;
    ptr Data;
} xcronjobconfig;
typedef struct xcronjobinfo {
    xcronjobstate State;
    uint32 Running;
    bool HasNext;
    xtime Next;
    uint64 Dispatched,Succeeded,Failed,Cancelled,SkippedRanges,CoalescedRanges;
    xcronzonedissue LastIssue;
} xcronjobinfo;
#define XCRON_RUN_COALESCED UINT32_C(1)
typedef struct xcronrun {
    xcronrunid Id;
    xcronjobid Job;
    ptr Data;
    xtime ScheduledAt,Through;
    xcronclock Observed;
    uint32 Flags;
} xcronrun;
typedef struct xcronpoll {
    size_t Count;
    uint32 Work,Faults,SkippedRanges;
    bool More;
} xcronpoll;
typedef struct xcronpollbudgeted {
    xcronpoll Poll;
    xcronzonedbudget Used;
    uint32 Searching;
} xcronpollbudgeted;
typedef struct xcronwake {
    uint64 MonotonicMilliseconds,DelayMilliseconds;
    bool Immediate,WaitingForCompletion;
} xcronwake;
typedef struct xcronretired {xcronjobid Id;ptr Data;xcronjobinfo Info;} xcronretired;
typedef struct xcronschedulerstats {
    size_t Bytes;
    uint32 Jobs,Running,Heap,Ready,Blocked,Retired,PeakJobs,PeakRunning;
    bool Closed,HasClock;
    xcronclock LastClock;
    uint64 Dispatched,Completed,SkippedRanges,CoalescedRanges,Faults,ClockSamples,WallRegressions;
} xcronschedulerstats;
XRT_EXTERN_C_BEGIN
/* Single externally serialized owner; no timer, worker, callback or storage is
 * created. Limits must be nonzero, Reserved zero. One allocation preallocates
 * all job/run/query slots, heap and queues within BytesLimit (excluding retained zones
 * and XRT diagnostics). Query storage is416 bytes per job on verified x64 targets,
 * separate from hot job slots. Normal operations and recorded job faults allocate none.
 * IDs are scoped to this scheduler. Job/run IDs have a generation; exhausted
 * generations use all bits not needed by the configured slot index, then retire
 * rather than wrap. Never use an ID with another instance.
 * Invalid argument/spans/aliases preserve every output. Other failures publish
 * typed Issue despite diagnostic OOM, preserving other outputs. Success preserves
 * ambient error. Writable outputs must be disjoint from scheduler-owned storage,
 * each other and input structs. The caller also keeps them disjoint from borrowed
 * job Data and retained immutable zones. Error domain: xrt.cron.scheduler. */
XCRON_API bool xcronSchedulerCreate(const xcronschedulerconfig* Config,xcronscheduler** Scheduler,xcronschedulerissue* Issue);
/* Copies Expression and retains Zone on success only; borrows Data until Reap.
 * Begin/End and SearchFlags have ZonedNext semantics. An empty window result is
 * a successful EXHAUSTED job awaiting Reap; a calculation error admits nothing.
 * WAIT/SKIP require RunningLimit=1; ALLOW uses its explicit limit <=RunLimit.
 * SKIP overlap discards due work through the wall sample when evaluated while
 * a lease is live. WAIT/ALLOW preserve backlog while at their running limit.
 * Misfires are due instants strictly older than Wall-GraceMilliseconds (saturates
 * at INT64_MIN). SKIP seeks at that cutoff; COALESCE emits the oldest due instant
 * with Through=min(Wall,End), then seeks strictly after Through; CATCH_UP keeps
 * every actual instant. Range counters do not pretend to count lost occurrences.
 * Actual folds all participate; missing labels never fire. No nominal compensation. */
XCRON_API bool xcronSchedulerAdd(xcronscheduler* Scheduler,const xcronjobconfig* Config,xcronjobid* Id,xcronschedulerissue* Issue);
/* Samples must have nondecreasing MonotonicMilliseconds; wall rollback is legal.
 * Already consumed cursors never rewind. Capacity must be positive. Pulls at most
 * Capacity leases with at most WorkLimit queue/heap steps; calculation inside one
 * step may traverse a full Gregorian cycle/history. Round-robin ready jobs and
 * alternating heap promotion prevent a catch-up job monopolizing ready work.
 * A lease is running immediately upon publication, including executor queue wait.
 * The caller must eventually Complete every lease, even if executor submission
 * fails. A future calculation fault terminates that job, but previously published
 * leases remain valid; Poll succeeds, records typed LastIssue and reports Faults.
 * More requests another poll at the same sample if output/work budgets stopped
 * immediately actionable work. All outputs are caller storage; none are retained. */
XCRON_API bool xcronSchedulerPoll(xcronscheduler* Scheduler,xcronclock Now,xcronrun* Runs,size_t Capacity,xcronpoll* Poll,xcronschedulerissue* Issue);
/* Resumable recurrence advancement. Positive global SearchBudget bounds the
 * sum of segment/month/rule-year work across every job in this call; WorkLimit
 * still bounds queue/heap actions. Heap, ready and pending queries alternate;
 * pending queries use FIFO one-unit quanta. A ready action may eagerly use one
 * quantum to preserve the dense fast path, charging the same global budget.
 * Capacity=0 (Runs may be NULL) advances calculations without publishing leases,
 * including while completion output is full. A job awaiting its next occurrence
 * is SEARCHING with HasNext=false; its live leases/Data remain valid. Cancel
 * removes its pending query without traversal. Completed calculation faults are
 * recorded per job without ambient diagnostics, as for Poll. Used reports actual
 * per-call work, Searching the remaining query count. More includes queries even
 * when lease slots are full. Poll may synchronously finish such queries if callers
 * mix APIs; only this entry point bounds recurrence work. No elapsed CPU/OS or
 * callback-duration guarantee; Close remains O(JobLimit). */
XCRON_API bool xcronSchedulerPollBudgeted(xcronscheduler* Scheduler,xcronclock Now,xcronzonedbudget SearchBudget,
    xcronrun* Runs,size_t Capacity,xcronpollbudgeted* Poll,xcronschedulerissue* Issue);
/* Completion means the physical body and all its data-using tails have stopped.
 * It is exactly once; stale/duplicate IDs fail without changing state.
 * Worker threads must deliver completions to the serialized scheduler owner.
 * Execution outcomes are counted; they do not implicitly stop future triggers. */
XCRON_API bool xcronSchedulerComplete(xcronscheduler* Scheduler,xcronrunid Id,xcronrunresult Result,xcronschedulerissue* Issue);
/* Cancels future dispatch only. Live leases remain valid and retain job storage.
 * Cancel is idempotent while the job exists; an already terminal reason remains. */
XCRON_API bool xcronSchedulerCancel(xcronscheduler* Scheduler,xcronjobid Id,xcronschedulerissue* Issue);
XCRON_API bool xcronSchedulerJob(const xcronscheduler* Scheduler,xcronjobid Id,xcronjobinfo* Info,xcronschedulerissue* Issue);
/* Successful empty reap sets Found=false and preserves Retired. Only terminal
 * jobs with zero running leases can be reaped; returns original borrowed Data,
 * releases the scheduler's zone reference, invalidates the ID and frees the slot.
 * Caller may destroy Data after this receipt. No destruction callbacks run here. */
XCRON_API bool xcronSchedulerReap(xcronscheduler* Scheduler,xcronretired* Retired,bool* Found,xcronschedulerissue* Issue);
/* O(JobLimit), idempotent: rejects Add and cancels scheduled jobs; does not join
 * bodies or discard completions/receipts. Reap all jobs before Destroy. */
XCRON_API bool xcronSchedulerClose(xcronscheduler* Scheduler,xcronschedulerissue* Issue);
/* Pure projection from explicit sample, which must not precede the last Poll's
 * monotonic sample. MaxWaitMilliseconds must be positive: bounds wall resampling
 * even while idle/blocked. Addition saturates at UINT64_MAX. WaitingForCompletion
 * tells an adapter to also wake on Complete, Add, Cancel or Close. No busy timer
 * loop for jobs blocked by running limits; Immediate means useful owner work. */
XCRON_API bool xcronSchedulerWake(const xcronscheduler* Scheduler,xcronclock Now,uint64 MaxWaitMilliseconds,xcronwake* Wake,xcronschedulerissue* Issue);
XCRON_API bool xcronSchedulerInspect(const xcronscheduler* Scheduler,xcronschedulerstats* Stats,xcronschedulerissue* Issue);
/* Requires zero jobs and leases. BUSY leaves the scheduler live and unchanged. */
XCRON_API bool xcronSchedulerDestroy(xcronscheduler* Scheduler,xcronschedulerissue* Issue);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xcron/include/xcron/executor.h */
/* ========================================================================== */

#ifndef XCRON_EXECUTOR_H
#define XCRON_EXECUTOR_H
#if defined(XCRON_FEATURE_XCRON_EXECUTOR) && (!defined(XCRON_FEATURE_XCRON_SCHEDULER) || !defined(XRT_FEATURE_EXECUTOR))
#error "cron executor adapter requires scheduler and public XRT executor"
#endif
#if defined(XCRON_FEATURE_XCRON_EXECUTOR)
typedef struct xcronexecutor xcronexecutor;
typedef struct xcronexecution xcronexecution;
typedef xcronrunresult (*xcronexecuteproc)(const xcronrun* Run,const xcronexecution* Execution,ptr Context);
typedef void (*xcronexecutornotify)(ptr Context);
typedef enum xcronexecutorerror {
    XCRON_EXECUTOR_OK=0,XCRON_EXECUTOR_ARGUMENT=5001,XCRON_EXECUTOR_LIMIT,
    XCRON_EXECUTOR_MEMORY,XCRON_EXECUTOR_CLOSED,XCRON_EXECUTOR_STALE,
    XCRON_EXECUTOR_BUSY,XCRON_EXECUTOR_THREAD,XCRON_EXECUTOR_REENTRY,
    XCRON_EXECUTOR_DUPLICATE,XCRON_EXECUTOR_REJECTED,XCRON_EXECUTOR_RESULT,
    XCRON_EXECUTOR_OWNER_CONFLICT
} xcronexecutorerror;
typedef struct xcronexecutorissue {xcronexecutorerror Code;xerrkind Cause;} xcronexecutorissue;
typedef struct xcronexecutorconfig {
    xexecutor* Executor;
    xcronexecuteproc Proc;
    ptr Context;
    xcronexecutornotify Notify;
    ptr NotifyContext;
    uint32 RunLimit,Reserved;
    size_t BytesLimit;
} xcronexecutorconfig;
typedef struct xcroncompletion {
    xcronrun Run;
    xcronrunresult Result;
    xcronexecutorerror Code;
    xerror* Error;
} xcroncompletion;
typedef struct xcronexecutorstats {
    size_t Bytes;
    uint32 Limit,Active,Peak,Producers;
    bool Closed;
    uint64 Submitted,Rejected,Drained,CancelRequests;
} xcronexecutorstats;
XRT_EXTERN_C_BEGIN
/* A preallocated execution adapter for leases from ONE originating scheduler.
 * This thread owns all adapter operations except ExecutionCancelled. Executor,
 * callback code and both contexts are borrowed through successful Destroy.
 * Executor must remain live and eventually run/drop every accepted submission;
 * adapter Close does not close a shared executor. One allocation bounds slots,
 * the lease-ID hash and completion queues within BytesLimit (not borrowed data,
 * executor storage or callback/diagnostic allocations). Limits positive,
 * Reserved zero. No normal steady allocation. No timer, thread or runtime is
 * created. Invalid arguments/spans/output aliases preserve all outputs. Other
 * failures publish typed Issue despite diagnostic OOM. Success preserves error.
 * Outputs must be disjoint from owned adapter storage, inputs and each other;
 * caller also keeps them disjoint from borrowed contexts/job data/scheduler.
 * Error domain xrt.cron.executor; rejected XRT submissions preserve their cause. */
XCRON_API bool xcronExecutorCreate(const xcronexecutorconfig* Config,xcronexecutor** Adapter,xcronexecutorissue* Issue);
/* Success transfers the duty to return this exact lease in Drain; caller must
 * not Complete it or release its job Data earlier. Failure transfers nothing,
 * invokes no Proc and leaves the caller responsible for Complete/retry. Active
 * duplicate IDs fail; IDs are scoped to the originating scheduler, not global.
 * Proc borrows copied Run/Execution only during its call, on an XRT worker.
 * Callback must finish ALL its data-using tails before returning. FAILED takes
 * that worker's ambient error into the receipt; no owner-thread ambient error
 * supplies another run's cause. Invalid return enums become FAILED/RESULT.
 * A callback selected on the adapter's owner thread is refused with a typed
 * OWNER_CONFLICT completion, preventing owner-state reentry. Callback code stays
 * resident through executor drain/join before unloading the extension library. */
XCRON_API bool xcronExecutorSubmit(xcronexecutor* Adapter,const xcronrun* Run,xcronexecutorissue* Issue);
/* Cooperative request. A queued callback observes it before Proc; running Proc
 * decides when to stop and which result to return. A late request never changes
 * an already completed outcome. Repeated requests while the ID exists succeed.
 * These do not cancel the scheduler job; coordinate SchedulerCancel separately. */
XCRON_API bool xcronExecutorCancel(xcronexecutor* Adapter,xcronrunid Id,xcronexecutorissue* Issue);
XCRON_API bool xcronExecutorCancelJob(xcronexecutor* Adapter,xcronjobid Id,uint32* Requested,xcronexecutorissue* Issue);
/* Capacity positive. Nonblocking bounded pull of at most Capacity completions.
 * Each receipt transfers Error ownership (possibly NULL); caller frees it with
 * xrtErrorFree. Call SchedulerComplete exactly once after receiving the receipt,
 * then a terminal job can be reaped. No order between concurrent completions is
 * promised. Detached finite batches drain before newer batches; no starvation.
 * This releases adapter capacity, not the scheduler's lease. Data is borrowed
 * through SchedulerReap, never owned by this copied receipt. */
XCRON_API bool xcronExecutorDrain(xcronexecutor* Adapter,xcroncompletion* Completed,size_t Capacity,size_t* Count,xcronexecutorissue* Issue);
/* Idempotent, stops Submit. CancelRuns=true also requests all existing tokens.
 * Notify is optional, runs after publication on the executor's completion/drop
 * thread (possibly this owner when that thread cancels queued XRT work). It must
 * not block or enter adapter operations. Owner reentry and other-thread access
 * are rejected. It may signal a caller-owned event/post a wake; that resource
 * lives through Destroy. Notifications may coalesce/be stale: Drain is authority.
 * The owner must resample with a bounded wait to handle wake-delivery failures. */
XCRON_API bool xcronExecutorClose(xcronexecutor* Adapter,bool CancelRuns,xcronexecutorissue* Issue);
XCRON_API bool xcronExecutorInspect(const xcronexecutor* Adapter,xcronexecutorstats* Stats,xcronexecutorissue* Issue);
/* BUSY until every accepted receipt is drained AND all publication/Notify tails
 * have left adapter memory. Does not join/destroy the borrowed executor. */
XCRON_API bool xcronExecutorDestroy(xcronexecutor* Adapter,xcronexecutorissue* Issue);
/* Any callback thread; NULL means false. Execution cannot escape Proc's scope. */
XCRON_API bool xcronExecutionCancelled(const xcronexecution* Execution);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xcron/include/xcron/service.h */
/* ========================================================================== */

#ifndef XCRON_SERVICE_H
#define XCRON_SERVICE_H
#if defined(XCRON_FEATURE_XCRON_SERVICE) && (!defined(XCRON_FEATURE_XCRON_EXECUTOR) || !defined(XRT_FEATURE_EVENT_WAITER) || !defined(XRT_FEATURE_COND) || !defined(XRT_FEATURE_THREAD))
#error "cron service requires execution adapter and public XRT thread/condition/event deadline waiter"
#endif
#if defined(XCRON_FEATURE_XCRON_SERVICE)
typedef struct xcronservice xcronservice;
typedef uint64 xcronrequestid;
typedef bool (*xcronclockproc)(xcronclock* Clock,ptr Context);
typedef enum xcronservicestate {XCRON_SERVICE_STARTING=0,XCRON_SERVICE_RUNNING,XCRON_SERVICE_STOPPING,XCRON_SERVICE_STOPPED,XCRON_SERVICE_FAILED} xcronservicestate;
typedef enum xcronrequestkind {XCRON_REQUEST_ADD=0,XCRON_REQUEST_CANCEL,XCRON_REQUEST_INFO,XCRON_REQUEST_REAP} xcronrequestkind;
typedef enum xcronserviceerror {
    XCRON_SERVICE_OK=0,XCRON_SERVICE_ARGUMENT=6001,XCRON_SERVICE_LIMIT,
    XCRON_SERVICE_MEMORY,XCRON_SERVICE_CLOSED,XCRON_SERVICE_STALE,
    XCRON_SERVICE_BUSY,XCRON_SERVICE_THREAD,XCRON_SERVICE_CLOCK,
    XCRON_SERVICE_SYSTEM,XCRON_SERVICE_KERNEL,XCRON_SERVICE_EXECUTOR
} xcronserviceerror;
typedef enum xcronservicecompletioncode {XCRON_COMPLETION_NORMAL=0,XCRON_COMPLETION_REJECTED,XCRON_COMPLETION_EXPIRED,XCRON_COMPLETION_CANCELLED,XCRON_COMPLETION_INTERNAL} xcronservicecompletioncode;
typedef struct xcronserviceissue {xcronserviceerror Code;xerrkind Cause;} xcronserviceissue;
typedef struct xcronserviceconfig {
    xcronschedulerconfig Scheduler;
    xexecutor* Executor;
    xcronexecuteproc Proc;
    ptr Context;
    xcronclockproc Clock;
    ptr ClockContext;
    uint32 CommandLimit,CompletionLimit,BatchLimit,CommandWorkLimit;
    uint32 ResampleMilliseconds,RetryIntervalMilliseconds,RetryAttempts,Reserved;
    uint64 AdmissionMilliseconds;
    size_t BytesLimit,AdapterBytesLimit,StackSize;
    xallocationbudget* InitializationBudget;
} xcronserviceconfig;
typedef struct xcronreply {
    xcronrequestid Request;
    xcronrequestkind Kind;
    xcronserviceerror Code;
    xcronjobid Job;
    ptr Data;
    bool Found;
    xcronjobinfo Info;
    xcronretired Retired;
    xcronschedulerissue Scheduler;
    xcronexecutorissue Executor;
    xerror* Error;
} xcronreply;
typedef struct xcronservicecompletion {
    xcronrunid Id;
    xcronjobid Job;
    xtime ScheduledAt,Through;
    xcronclock Observed;
    uint32 Flags;
    xcronrunresult Result;
    xcronservicecompletioncode Code;
    xcronexecutorerror Executor;
    uint32 Attempts;
    xerror* Error;
} xcronservicecompletion;
typedef struct xcronservicestats {
    /* Requested retained component allocations: complete before publication,
     * AdapterBytes/Executor.Bytes become zero on physical adapter destruction.
     * Other executor fields retain their final counters/configuration at stop. */
    size_t Bytes,ArenaBytes,KernelBytes,AdapterBytes;
    xcronservicestate State;
    uint32 PendingRequests,ReadyReplies,ReservedCompletions,ReadyCompletions,Unsent;
    xcronserviceissue Failure;
    xcronclock LastClock;
    xcronschedulerstats Kernel;
    xcronexecutorstats Executor;
    uint64 SubmitAttempts,RejectedSubmissions,Retries,AdmissionExpired,Published,Collected,WakeFailures;
    /* Selected backend/capacity persists after stop; live timers becomes zero on
     * successful owner cleanup. Failed cleanup stays visible and Destroy retries.
     * Waiter storage is already included in ArenaBytes, not added to Bytes again. */
    xeventwaiterinfo Waiter;
    uint32 ActiveNativeTimers;
    /* Historical latest owner poll, including zero-output calculation turns;
     * Searching describes that poll, not a live post-close gauge. Each search
     * budget equals Scheduler.WorkLimit globally across admissions/advancement. */
    xcronpollbudgeted LastPoll;
} xcronservicestats;
XRT_EXTERN_C_BEGIN
/* Dedicated public-XRT owner thread, bounded command tickets, kernel, borrowed
 * executor adapter and reserved completion output. No general/shared runtime.
 * Executor, Proc/Context and Clock/ClockContext are borrowed through successful
 * Destroy; Executor stays live until all accepted work/drop returns. Clock=NULL
 * samples xrtNow/xrtTimer; a custom clock runs only on the owner and can run before
 * Create returns, after *Service is published. It must return promptly with a
 * valid pair, retain no output and finish all context-using tails. Monotonic
 * regression/read failure causes observable controlled shutdown. Wall jumps are
 * governed by each job's existing misfire policy. Recurrence admissions and
 * advancement use global segment/month/rule-year budgets of Scheduler.WorkLimit
 * per owner turn. Foreign callbacks, diagnostics and configured Close/cancel
 * scans still preclude a hard whole-owner CPU/P99 bound.
 * All limits positive, Reserved zero; BatchLimit <=RunLimit and CompletionLimit.
 * BytesLimit reserves arena + Scheduler.BytesLimit + AdapterBytesLimit without
 * overflow. Stats reports actual requested component bytes. Borrowed data/zones,
 * diagnostics, callbacks and native/runtime bootstrap storage are outside this
 * cap. One owner-bound reusable event waiter prefers precise native deadlines,
 * with observable platform fallback in Stats.Waiter and no global resolution
 * change. Its storage is inside ArenaBytes; native timer handles are outside the
 * byte cap and counted separately. Normal stop releases it before STOPPED; a
 * release failure publishes FAILED and Destroy retries after physical join.
 * Optional InitializationBudget charges direct creator and kernel/adapter
 * startup allocations; borrowed only during Create, not business work/bootstrap.
 * Create failure consumes no Data/context/Executor and leaves Service unchanged.
 * Invalid API spans/output aliases preserve outputs; value failures publish typed
 * Issue despite diagnostic OOM. Success/normal timeout preserves ambient error.
 * Output buffers must be disjoint from service arena/inputs/each other and, by
 * caller contract, every borrowed context/zone/data and private component storage.
 * Error domain xrt.cron.service. No API grants safe concurrent Destroy. */
XCRON_API bool xcronServiceCreate(const xcronserviceconfig* Config,xcronservice** Service,xcronserviceissue* Issue);
/* Thread-safe bounded admission. ADD copies its config and success-only retains
 * Zone for the command. Queue acceptance is distinct from job admission. Data is
 * borrowed until a failed ADD reply is collected, or a successful job is reaped.
 * ADD success reply has Job and Data=NULL; failure returns original Data. Other
 * commands carry no Data duty. Close rejects new commands; queued ADDs not yet
 * processed complete CLOSED. A running initial calculation may also roll back
 * CLOSED; a committed Add can succeed then be cancelled. Commands dequeue FIFO,
 * while slow calculations may publish replies after later commands. No Job ID
 * is published before transactional commit. Generation tickets remain occupied
 * until exactly one Collect. */
XCRON_API bool xcronServiceAddAsync(xcronservice* Service,const xcronjobconfig* Config,xcronrequestid* Request,xcronserviceissue* Issue);
XCRON_API bool xcronServiceCancelAsync(xcronservice* Service,xcronjobid Job,bool CancelRuns,xcronrequestid* Request,xcronserviceissue* Issue);
XCRON_API bool xcronServiceInfoAsync(xcronservice* Service,xcronjobid Job,xcronrequestid* Request,xcronserviceissue* Issue);
XCRON_API bool xcronServiceReapAsync(xcronservice* Service,xcronrequestid* Request,xcronserviceissue* Issue);
/* Timeout >=0, one total monotonic budget including contention/spurious wakeups.
 * Ready wins over timeout. XWAIT_OK transfers exactly one reply and its owned
 * optional Error; its Code is the asynchronous operation's result. Timeout leaves
 * output and accepted command/Data duty intact. Stale/already collected tickets
 * fail. Pending blocking waits reject owner and ANY borrowed-executor callback,
 * not only service bodies. Timeout=0 permits nonblocking callback collection. */
XCRON_API xwaitresult xcronServiceCollect(xcronservice* Service,xcronrequestid Request,int64 Timeout,xcronreply* Reply,xcronserviceissue* Issue);
/* Thread-safe nonblocking metadata drain, Capacity positive. Every pulled kernel
 * lease reserved its output slot first, including unsent/native-running work.
 * Full output applies backpressure; it never drops a result. Error ownership is
 * transferred per record and must be xrtErrorFree'd. Records contain NO job Data
 * pointer, so later consumption is safe after a separate retirement receipt.
 * No order between concurrent workers is promised. Retry only no-effect AGAIN;
 * RetryAttempts includes the initial attempt. AdmissionMilliseconds=0 disables
 * retries; elapsed/attempt bounds produce EXPIRED, never silent replay. */
XCRON_API bool xcronServiceDrain(xcronservice* Service,xcronservicecompletion* Completed,size_t Capacity,size_t* Count,xcronserviceissue* Issue);
/* Asynchronously stops future dispatch/admission. Always cancels unsent leases;
 * CancelRuns=true also requests accepted tokens, and can upgrade an earlier close.
 * No forced termination and no close of a borrowed shared Executor. A current
 * owner turn may finish before observing Close; it is not a no-new-body barrier. */
XCRON_API bool xcronServiceClose(xcronservice* Service,bool CancelRuns,xcronserviceissue* Issue);
/* Finite Timeout>=0; rejects owner/borrowed-executor callbacks before a blocking
 * join. Includes native thread/XRT context cleanup. Close/Wait can finish before
 * clients consume outputs/reap jobs, because all final outputs are pre-reserved. */
XCRON_API xwaitresult xcronServiceWaitFor(xcronservice* Service,int64 Timeout,xcronserviceissue* Issue);
/* Only after owner stop; safe serialized kernel handoff. Running callers use
 * ReapAsync. Successful empty reap preserves Retired, sets Found=false. Original
 * Data is returned only after all bodies/data-using tails have ended. */
XCRON_API bool xcronServiceReapStopped(xcronservice* Service,xcronretired* Retired,bool* Found,xcronserviceissue* Issue);
XCRON_API bool xcronServiceInspect(xcronservice* Service,xcronservicestats* Stats,xcronserviceissue* Issue);
/* Returns an owned reference to the first fatal owner error, or NULL. Typed
 * Failure in Stats survives diagnostic OOM and is authoritative even if NULL. */
XCRON_API bool xcronServiceFailure(xcronservice* Service,xerror** Error,xcronserviceissue* Issue);
XCRON_API bool xcronServiceWake(xcronservice* Service,xcronserviceissue* Issue);
/* Caller-exclusive. Requires stopped/joined owner, zero outstanding tickets,
 * completion reservations and jobs. BUSY preserves all duties. No implicit reaping
 * or Data destruction. Borrowed callback/module code remains resident through the
 * host Executor's physical drain/join before unloading its extension library. */
XCRON_API bool xcronServiceDestroy(xcronservice* Service,xcronserviceissue* Issue);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xcron/include/xcron/tzdb.h */
/* ========================================================================== */

#ifndef XCRON_TZDB_H
#define XCRON_TZDB_H
#if defined(XCRON_FEATURE_XCRON_TZDB) && (!defined(XCRON_FEATURE_XCRON_TIMEZONE) || !defined(XRT_FEATURE_MUTEX))
#error "cron tzdb requires timezone and mutex"
#endif
#if defined(XCRON_FEATURE_XCRON_TZDB)
#define XCRON_TZDB_NAMES_MAX 65535u
#define XCRON_TZDB_NAME_MAX 255u
#define XCRON_TZDB_VERSION_MAX 64u
#define XCRON_TZDB_LOADS_MAX 1024u
#define XCRON_TZDB_ANY_REVISION UINT64_MAX
#define XCRON_TZDB_NO_INDEX SIZE_MAX
typedef struct xcrontzdb xcrontzdb;
typedef struct xcrontzdbsnapshot xcrontzdbsnapshot;
typedef enum xcrontzdbstatus {XCRON_TZDB_ERROR=-1,XCRON_TZDB_NONE=0,XCRON_TZDB_FOUND=1} xcrontzdbstatus;
typedef enum xcrontzdberror {
    XCRON_TZDB_OK=0,XCRON_TZDB_ARGUMENT=7001,XCRON_TZDB_NAME,XCRON_TZDB_VERSION,
    XCRON_TZDB_DUPLICATE,XCRON_TZDB_LIMIT,XCRON_TZDB_MEMORY,XCRON_TZDB_ZONE,
    XCRON_TZDB_IO,XCRON_TZDB_STALE,XCRON_TZDB_EMPTY,XCRON_TZDB_CLOSED,XCRON_TZDB_BUSY,XCRON_TZDB_SYSTEM
} xcrontzdberror;
/* Index is the original input position (not sorted order), or NO_INDEX. Zone
 * keeps typed parser failures even if its diagnostic allocation fails. Cause/
 * SystemCode describe a failed underlying IO operation when available. */
typedef struct xcrontzdbissue {xcrontzdberror Code;size_t Index;xcronzoneissue Zone;xerrkind Cause;int32 SystemCode;} xcrontzdbissue;
typedef struct xcrontzdbconfig {uint32 NamesLimit,LoadLimit;size_t BytesLimit;} xcrontzdbconfig;
/* Name is an exact case-sensitive identifier; Tzif is borrowed only during Load. */
typedef struct xcrontzdbentry {xstrview Name;xbytesview Tzif;} xcrontzdbentry;
/* Version borrows this retained snapshot. Bytes includes its arena/decoded zones. */
typedef struct xcrontzdbsnapshotinfo {uint64 Revision;size_t Names,Bytes;xstrview Version;} xcrontzdbsnapshotinfo;
typedef struct xcrontzdbstats {
    uint64 Revision;uint32 Loading,Names;size_t BytesLimit,LiveBytes,PeakBytes,LiveAllocations;uint64 Rejections;bool Closed;
} xcrontzdbstats;
XRT_EXTERN_C_BEGIN
/* Explicit named TZif database; no embedded data, ambient TZ, filesystem, worker
 * or timer. Limits are positive and bounded by the constants above. One shared
 * XRT quota charges manager, all candidate/published/retired snapshot arenas,
 * decoded zones/footer rules and temporary allocations until actually freed,
 * including zones retained independently after snapshot release. One fixed Core
 * quota-control allocation, mutex native state, allocator backing/headers and
 * final outward diagnostics are outside BytesLimit. Lower-level diagnostics
 * built inside a load DO consume quota until released. Explicit quota replaces the caller's
 * ambient quota while constructing database-owned data (XRT innermost rule).
 * Invalid spans/aliases preserve every output; other failures publish typed
 * Issue even under diagnostic OOM and preserve other outputs. Success/NONE
 * preserve ambient error. Outputs cannot overlap manager/snapshot arena, each
 * other or inputs; caller keeps them disjoint from all borrowed/retained zones
 * and any other retained snapshots. Error domain: xrt.cron.tzdb. */
XCRON_API bool xcronTzdbCreate(const xcrontzdbconfig* Config,xcrontzdb** Database,xcrontzdbissue* Issue);
/* Build/validate/copy all input names, version and decoded TZif off the publish
 * lock. Names are1..255 ASCII letters/digits/_/./-/+ with slash separators;
 * empty/dot/dotdot components, leading/trailing slash, components starting '-'
 * or ending '.' are rejected. Legacy names such as Etc/GMT+5 remain valid.
 * Version is1..64 visible ASCII bytes. Input descriptors and bytes stay stable
 * throughout this synchronous call. Empty input publishes an empty snapshot.
 * Duplicate exact names fail atomically; names are not locale/case normalized.
 * ExpectedRevision is0 for initial publication, the prior revision for CAS, or
 * ANY_REVISION for deliberate last-publication-wins. Revision never wraps or
 * uses the ANY sentinel. Failures/stale/close leave the current snapshot and
 * Revision output intact. LoadLimit bounds admitted concurrent builders; excess
 * calls fail LIMIT rather than wait for a builder slot. Close stops subsequent
 * build entries/publication; it cannot interrupt a current syscall/decode.
 * This is cold work: run it outside a latency-sensitive scheduler owner. */
XCRON_API bool xcronTzdbLoad(xcrontzdb* Database,xstrview Version,const xcrontzdbentry* Entries,size_t Count,
    uint64 ExpectedRevision,uint64* Revision,xcrontzdbissue* Issue);
/* Acquire takes the publication mutex once and retains a coherent immutable
 * snapshot; EMPTY before first publication, CLOSED after Close. Retain/Release
 * permit explicit cross-thread ownership. Snapshot readers need no database or
 * publication lock and may outlive its physical destruction. */
XCRON_API bool xcronTzdbAcquire(xcrontzdb* Database,xcrontzdbsnapshot** Snapshot,xcrontzdbissue* Issue);
XCRON_API bool xcronTzdbSnapshotRetain(const xcrontzdbsnapshot* Snapshot);
XCRON_API void xcronTzdbSnapshotRelease(const xcrontzdbsnapshot* Snapshot);
XCRON_API bool xcronTzdbSnapshotInfo(const xcrontzdbsnapshot* Snapshot,xcrontzdbsnapshotinfo* Info,xcrontzdbissue* Issue);
/* Exact bounded binary search. FOUND returns one retained zone, independently
 * releasable with ZoneRelease; NONE preserves Zone and publishes OK Issue.
 * No IO/allocation/lock on normal lookup. New snapshots affect new lookups only;
 * existing scheduler jobs/leases keep their immutable zone, never silently
 * rebase consumed occurrences. A caller explicitly replaces jobs when desired. */
XCRON_API xcrontzdbstatus xcronTzdbFind(const xcrontzdbsnapshot* Snapshot,xstrview Name,xcronzone** Zone,xcrontzdbissue* Issue);
/* Revision/name/closed/loading are one publication-state sample; quota gauges
 * are independently synchronized and can include concurrent cold build/free.
 * LiveBytes includes the manager itself and every retained generation. */
XCRON_API bool xcronTzdbInspect(xcrontzdb* Database,xcrontzdbstats* Stats,xcrontzdbissue* Issue);
/* Idempotent, rejects new loads/acquires and releases the current reference
 * outside the lock. Old snapshots/zones remain usable. Release may be O(names).
 * Destroy requires Close and zero admitted builders, else BUSY; caller must also
 * physically exclude/join ALL concurrent database calls before Destroy. A failed
 * native mutex cleanup keeps the closed object live for retry. */
XCRON_API bool xcronTzdbClose(xcrontzdb* Database,xcrontzdbissue* Issue);
XCRON_API bool xcronTzdbDestroy(xcrontzdb* Database,xcrontzdbissue* Issue);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xcron/include/xcron/tzdb_file.h */
/* ========================================================================== */

#ifndef XCRON_TZDB_FILE_H
#define XCRON_TZDB_FILE_H
#if defined(XCRON_FEATURE_XCRON_TZDB_FILE) && (!defined(XCRON_FEATURE_XCRON_TZDB) || !defined(XRT_FEATURE_FILE_ROOT))
#error "cron tzdb file requires tzdb and file-root"
#endif
#if defined(XCRON_FEATURE_XCRON_TZDB_FILE)
XRT_EXTERN_C_BEGIN
/* Explicit manifest of names under a borrowed anchored XRT root. Uses secure
 * RootFileOpen (including contained relative aliases), regular files only and
 * one <=16MiB temporary file at a time inside the SAME aggregate database quota.
 * Version/names/CAS/output rules are Load's. Caller owns Root for the full call,
 * and supplies a trusted immutable release directory/manifest: publication is
 * atomic, but reads across several files are not a filesystem-wide transaction.
 * File size/identity/time are rechecked after reading, and growth/short reads
 * fail; these checks alone cannot prove an immutable source or authenticate it.
 * No default directory, TZ environment, download, directory walk or leap-scale
 * fallback is guessed. Missing/invalid/escaping files reject the entire reload.
 * Root resolution/open/read/stat/close allocations/syscalls are cold costs.
 * Root itself and its native directory handle are caller-owned, outside quota. */
XCRON_API bool xcronTzdbLoadRoot(xcrontzdb* Database,xroot Root,xstrview Version,const xstrview* Names,size_t Count,
    uint64 ExpectedRevision,uint64* Revision,xcrontzdbissue* Issue);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xcron/include/xcron.h */
/* ========================================================================== */

#ifndef XCRON_H
#define XCRON_H


#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_EXPRESSION)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_EXPRESSION"
#endif

#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_CALENDAR)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_CALENDAR"
#endif

#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_TIMEZONE)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_TIMEZONE"
#endif

#if defined(XCRON_FEATURE_XCRON) && (!defined(XCRON_FEATURE_XCRON_TZDB) || !defined(XCRON_FEATURE_XCRON_TZDB_FILE))
#error "XCRON_FEATURE_XCRON requires tzdb and tzdb_file"
#endif

#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_ZONED)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_ZONED"
#endif

#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_SCHEDULER)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_SCHEDULER"
#endif

#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_EXECUTOR)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_EXECUTOR"
#endif

#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_SERVICE)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_SERVICE"
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
/* internal: extlibs/xcron/src/timezone/timezone_internal.h */
/* ========================================================================== */

#if defined(XCRON_FEATURE_XCRON_TIMEZONE) || \
	defined(XCRON_FEATURE_XCRON_SERVICE)
#ifndef XCRON_TIMEZONE_INTERNAL_H
#define XCRON_TIMEZONE_INTERNAL_H
#include <limits.h>
#if defined(XCRON_FEATURE_XCRON_TIMEZONE)
#define XCRON_ZONE_MAGIC UINT32_C(0x58435a31)
typedef struct xcronzonehistory xcronzonehistory;
struct xcronzone {
    xatomic32 Refs;
    uint32 Magic;
    size_t Bytes;
    int32 Offsets[2];
    uint16 Names[2];
    uint8 Mode,HasDST;
    xcronzonehistory* History;
    /* 800 start/end UTC offsets for 2000..2399, then owned names (POSIX only). */
    int32 Changes[];
};
bool xcronZoneFail(xcronzoneerror Code,size_t Offset,xcronzoneissue* Issue);
bool xcronZoneCivilSeconds(const xdatetime* Date,int64* Seconds);
/* Pure span/owner/alias checks; callers publish their own single diagnostic. */
bool xcronZoneOutputs(const xcronzone* Zone,const void* Input,size_t InputSize,void* const* Outputs,const size_t* Sizes,size_t Count);
xcronzonestate xcronZoneStateValue(const xcronzone* Zone,xtime When);
void xcronZoneDecompose(xtime When,int32 Offset,bool DST,xdatetime* Date);
xcronzonestatus xcronZoneNextValue(const xcronzone* Zone,xtime After,xcronzonetransition* Transition,xcronzoneissue* Issue);
/* Product-private, borrowed-owner transition walk. Init does bounded history
 * lookup/constant setup; Step charges at most YearBudget additional rule years.
 * History directory lookup is logarithmic in the capped transition count.
 * PENDING is private: public synchronous zone APIs never return it. */
#define XCRON_ZONE_WALK_PENDING ((xcronzonestatus)2)
typedef struct xcronzonewalk {
    const xcronzone *Rules,*History;
    int64 Year,Day,Second,Best;
    uint32 Years;
    xcronzonestatus Status;
    bool Before,After;
    xcronzonetransition Result;
} xcronzonewalk;
void xcronZoneWalkInit(const xcronzone* Zone,xtime After,xcronzonewalk* Walk);
xcronzonestatus xcronZoneWalkStep(xcronzonewalk* Walk,uint32 YearBudget);
void xcronZoneHistoryWalkInit(const xcronzone* Zone,xtime After,xcronzonewalk* Walk);
void xcronZoneHistoryWalkResult(const xcronzone* Zone,xcronzonewalk* Walk);
/* Earliest >=Begin from which the complete zone repeats every Gregorian cycle.
 * False means no proven periodic suffix inside the xtime domain. */
bool xcronZoneRepeat(const xcronzone* Zone,xtime Begin,xtime* First);
bool xcronZoneHistoryRepeat(const xcronzone* Zone,xtime Begin,xtime* First);
xcronzonestate xcronZoneHistoryAt(const xcronzone* Zone,xtime When);
xcronzonestatus xcronZoneHistoryNext(const xcronzone* Zone,xtime After,xcronzonetransition* Transition,xcronzoneissue* Issue);
xcronzonestatus xcronZoneHistoryResolve(const xcronzone* Zone,const xdatetime* Date,xtime* Candidates,size_t Capacity,size_t* Count,xcronzoneissue* Issue);
bool xcronZoneHistoryOverlap(const xcronzone* Zone,const void* Output,size_t Size);
void xcronZoneHistoryRelease(const xcronzone* Zone);
/* Requested bytes owned by this zone, including an independently owned footer. */
#if defined(XCRON_FEATURE_XCRON_TZDB)
size_t xcronZoneOwnedBytes(const xcronzone* Zone);
#endif
/* Fraction stays nonnegative at the asymmetric signed-int64 millisecond ends. */
static inline bool xcronZoneSecondValue(int64 Seconds,int Milli,xtime* Out)
{
    int64 Minimum=INT64_MIN/1000-1,Maximum=INT64_MAX/1000;
    if(Seconds<Minimum || Seconds>Maximum)return false;
    if(Seconds==Minimum){int Limit=(int)(1000+INT64_MIN%1000);
        if(Milli<Limit)return false;
        *Out=INT64_MIN+(Milli-Limit);return true;
    }
    if(Seconds==Maximum && Milli>INT64_MAX%1000)return false;
    *Out=Seconds*1000+Milli;return true;
}
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xcron/src/zoned/zoned_internal.h */
/* ========================================================================== */

#if defined(XCRON_FEATURE_XCRON_ZONED)
#ifndef XCRON_ZONED_INTERNAL_H
#define XCRON_ZONED_INTERNAL_H
#if defined(XCRON_FEATURE_XCRON_ZONED)
/* Valid immutable expression/live zone, ordered window and disjoint valid
 * outputs are preconditions. Pure typed result; no ambient error or allocation. */
xcronzonedstatus xcronZonedNextValue(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,xtime* Next,xcronzonedissue* Issue);
typedef struct xcronzonedvalue {
    xcronexpression Expression;
    xcroncalendarvalue Calendar;
    xcronzonewalk Transition;
    xcronzonedcursorissue Work;
    xtime First,Limit,SegmentEnd,Result;
    int32 Offset;
    uint32 Phase;
    xcronzonedstatus Status;
} xcronzonedvalue;
/* Valid immutable inputs, ordered window. No diagnostics, retention or heap. */
void xcronZonedValueInit(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,xcronzonedvalue* V);
/* Reuse initialized private arena storage; reset every phase-live field without
 * clearing dormant calendar/transition bytes. Public opaque Init still zeros. */
void xcronZonedValueRestart(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,xcronzonedvalue* V);
/* Constant earliest-rounded-instant probe. On PENDING, Local/Offset seed
 * subsequent traversal; Work reports its actual segment/month cost. */
xcronzonedstatus xcronZonedReadyValue(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,
    xtime* First,xdatetime* Local,int32* Offset,xcronzonedcursorissue* Work);
xcronzonedstatus xcronZonedValueStep(xcronzonedvalue* V,const xcronzone* Z,xcronzonedbudget Budget);
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xcron/src/scheduler/scheduler_internal.h */
/* ========================================================================== */

#if defined(XCRON_FEATURE_XCRON_SCHEDULER)
#ifndef XCRON_SCHEDULER_INTERNAL_H
#define XCRON_SCHEDULER_INTERNAL_H
#if defined(XCRON_FEATURE_XCRON_SCHEDULER)
/* Owner-only provisional admission for the live service. ID is internal until
 * Finish accepts. Pending/failed/cancelled admissions cannot dispatch or Reap.
 * Begin copies/retains into a bounded kernel slot, then queues only calculation.
 * Finish either commits it or releases the provisional slot/zone; business Data
 * remains the service command's duty. PollBudgeted performs all traversal. */
typedef enum xcronadmissionstatus {XCRON_ADMISSION_REJECTED=-1,XCRON_ADMISSION_PENDING=0,XCRON_ADMISSION_ACCEPTED=1} xcronadmissionstatus;
bool xcronSchedulerBeginAdmission(xcronscheduler* S,const xcronjobconfig* C,xcronjobid* Id,xcronschedulerissue* Issue);
xcronadmissionstatus xcronSchedulerFinishAdmission(xcronscheduler* S,xcronjobid Id,bool Cancel,xcronschedulerissue* Issue);
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xcron/src/tzdb/tzdb_internal.h */
/* ========================================================================== */

#if defined(XCRON_FEATURE_XCRON_TZDB)
#ifndef XCRON_TZDB_INTERNAL_H
#define XCRON_TZDB_INTERNAL_H
#if defined(XCRON_FEATURE_XCRON_TZDB)
#define XCRON_TZDB_MAGIC UINT32_C(0x58434431)
#define XCRON_TZDB_SNAPSHOT_MAGIC UINT32_C(0x58434453)
typedef struct xcrontzdbrow {xstrview Name;xcronzone* Zone;uint32 Index;} xcrontzdbrow;
struct xcrontzdbsnapshot {uint32 Magic;xatomic32 Refs;size_t ArenaBytes,Bytes,Count;uint64 Revision;xstrview Version;xcrontzdbrow Rows[];};
struct xcrontzdb {uint32 Magic;xcrontzdbconfig Config;xallocationbudget* Budget;xmutex Mutex;xcrontzdbsnapshot* Current;uint64 Revision;uint32 Loading;xatomic32 Closed;};
/* Product-private cold reader. Allocates only with xrtMalloc; success returns a
 * borrowed view and optional buffer released by the common builder. Failure
 * closes/frees its own partial resources and fills typed Work without publishing
 * an application diagnostic. No callback occurs under the publication lock. */
typedef bool (*xcrontzdbread)(ptr Context,xstrview Name,xbytesview* Data,ptr* Owned,xcrontzdbissue* Work);
typedef struct xcrontzdbsource {const xcrontzdbentry* Entries;const xstrview* Names;size_t Count;xcrontzdbread Read;ptr Context;} xcrontzdbsource;
bool xcronTzdbLoadSource(xcrontzdb* DB,xstrview Version,const xcrontzdbsource* Source,uint64 Expected,uint64* Revision,xcrontzdbissue* Issue);
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
/* source: extlibs/xcron/src/timezone/timezone.c */
/* ========================================================================== */

#if defined(XCRON_FEATURE_XCRON_TIMEZONE)
#include <limits.h>
#include <string.h>
#if defined(XCRON_FEATURE_XCRON_TIMEZONE)
typedef struct xcronzonerule {unsigned Kind,A,B,C;
    int32 Time;
} xcronzonerule;
typedef struct xcronzoneparser {xstrview Text;
    size_t At;
    xcronzonegrammar Grammar;
    xcronzoneissue* Issue;
} xcronzoneparser;
bool xcronZoneFail(xcronzoneerror Code,size_t Offset,xcronzoneissue* Issue)
{
    if(Issue)*Issue=(xcronzoneissue){Code,Offset,0};
    xrtSetErrorInfo(Code==XCRON_ZONE_ARGUMENT?XERR_ARGUMENT:Code==XCRON_ZONE_MEMORY?XERR_MEMORY:Code==XCRON_ZONE_LIMIT?XERR_RANGE:Code==XCRON_ZONE_UNKNOWN?XERR_STATE:XERR_VALUE,"xrt.cron.zone",Code,
    Code==XCRON_ZONE_RULES_REQUIRED?"explicit DST rules are required":"invalid cron timezone input or value");
    return false;
}
static int64 xcronZoneFloor(int64 N,int64 D){int64 Q=N/D;
    return Q-(N%D<0);
}
static int64 xcronZoneYearStep(int64 Year,int Step){Year+=Step;
    if(!Year)Year+=Step;
    return Year;
}
static unsigned xcronZoneCycle(int64 Year){int64 N=(Year<0?Year+1:Year)%400;
    return (unsigned)(N<0?N+400:N);
}
static bool xcronZoneName(xcronzoneparser* P,xstrview* Name)
{
    bool Quoted=P->At<P->Text.Size && P->Text.Data[P->At]=='<';
    if(Quoted)++P->At;
    size_t Start=P->At;
    while(P->At<P->Text.Size){unsigned char C=(unsigned char)P->Text.Data[P->At];
        bool Alpha=(C>='A' && C<='Z') || (C>='a' && C<='z');
        if(!Alpha && !(Quoted && ((C>='0' && C<='9') || C=='+' || C=='-')))break;
        ++P->At;
    }
    size_t Size=P->At-Start;
    if(Size<3 || Size>XCRON_ZONE_NAME_MAX || (Quoted && (P->At==P->Text.Size || P->Text.Data[P->At++]!='>')))return xcronZoneFail(Size>XCRON_ZONE_NAME_MAX?XCRON_ZONE_LIMIT:XCRON_ZONE_SYNTAX,Start,P->Issue);
    *Name=(xstrview){P->Text.Data+Start,Size};
    return true;
}
static bool xcronZoneNumber(xcronzoneparser* P,unsigned Limit,unsigned* Out)
{
    size_t Start=P->At;
    unsigned N=0;
    while(P->At<P->Text.Size && P->Text.Data[P->At]>='0' && P->Text.Data[P->At]<='9'){
        unsigned D=(unsigned)(P->Text.Data[P->At]-'0');
        if(N>Limit/10 || (N==Limit/10 && D>Limit%10))return xcronZoneFail(XCRON_ZONE_SYNTAX,P->At,P->Issue);
        N=N*10+D;
        ++P->At;
    }
    if(P->At==Start)return xcronZoneFail(XCRON_ZONE_SYNTAX,Start,P->Issue);
    *Out=N;
    return true;
}
static bool xcronZoneClock(xcronzoneparser* P,bool Transition,int32* Out)
{
    int Sign=1;
    size_t Start=P->At;
    bool Signed=P->At<P->Text.Size && (P->Text.Data[P->At]=='+' || P->Text.Data[P->At]=='-');
    if(Signed){if(Transition && P->Grammar==XCRON_ZONE_POSIX)return xcronZoneFail(XCRON_ZONE_SYNTAX,Start,P->Issue);
        if(P->Text.Data[P->At]=='-')Sign=-1;
        ++P->At;
    }
    unsigned H,M=0,S=0;
    if(!xcronZoneNumber(P,Transition && P->Grammar==XCRON_ZONE_TZIF3?167:24,&H))return false;
    if(P->At<P->Text.Size && P->Text.Data[P->At]==':'){++P->At;
        if(!xcronZoneNumber(P,59,&M))return false;
        if(P->At<P->Text.Size && P->Text.Data[P->At]==':'){++P->At;
            if(!xcronZoneNumber(P,59,&S))return false;
    }}
    *Out=Sign*(int32)(H*3600+M*60+S);
    return true;
}
static bool xcronZoneRule(xcronzoneparser* P,xcronzonerule* R)
{
    *R=(xcronzonerule){0,0,0,0,7200};
    if(P->At<P->Text.Size && P->Text.Data[P->At]=='M'){
        ++P->At;
        R->Kind=2;
        if(!xcronZoneNumber(P,12,&R->A) || !R->A)goto Invalid;
        if(P->At==P->Text.Size || P->Text.Data[P->At++]!='.' || !xcronZoneNumber(P,5,&R->B) || !R->B)goto Invalid;
        if(P->At==P->Text.Size || P->Text.Data[P->At++]!='.' || !xcronZoneNumber(P,6,&R->C))goto Invalid;
    }else {if(P->At<P->Text.Size && P->Text.Data[P->At]=='J'){++P->At;
            R->Kind=1;
        }
        if(!xcronZoneNumber(P,365,&R->A) || (R->Kind && !R->A))goto Invalid;
    }
    if(P->At<P->Text.Size && P->Text.Data[P->At]=='/'){++P->At;
        return xcronZoneClock(P,true,&R->Time);
    }return true;
    Invalid:return xcronZoneFail(XCRON_ZONE_SYNTAX,P->At,P->Issue);
}
static int32 xcronZoneRuleSecond(unsigned Cycle,const xcronzonerule* R,int32 Before)
{
    int64 Year=2000+(int64)Cycle;
    bool Leap=xrtDaysInYear(Year)==366;
    unsigned Day=R->A;
    if(R->Kind==1)Day=R->A-1+(Leap && R->A>=60);
    else if(R->Kind==2){xtime T;
        xdatetime D;
        (void)xrtDate(Year,(int)R->A,1,&T);
        (void)xrtTimeSplit(T,&D);
        unsigned Dom=1+(R->C+7-(unsigned)D.Weekday)%7+7*(R->B-1);
        if(Dom>(unsigned)xrtDaysInMonth(Year,(int)R->A))Dom-=7;
        Day=Dom-1;
        for(unsigned M=1;M<R->A;++M)Day+=(unsigned)xrtDaysInMonth(Year,(int)M);
    }
    return (int32)Day*86400+R->Time-Before;
}
static bool xcronZoneHandle(const xcronzone* Z)
{return Z && xrtMemRangeValid(Z,sizeof(*Z)) && Z->Magic==XCRON_ZONE_MAGIC;
}
bool xcronZoneOutputs(const xcronzone* Z,const void* Input,size_t InputSize,void* const* Outputs,const size_t* Sizes,size_t Count)
{
    if(!xcronZoneHandle(Z) || (Input && !xrtMemRangeValid(Input,InputSize)))return false;
    for(size_t I=0;I<Count;++I){if(!xrtMemRangeValid(Outputs[I],Sizes[I]) || xrtMemRangesOverlap(Outputs[I],Sizes[I],Z,Z->Bytes) || (Z->History && xcronZoneHistoryOverlap(Z,Outputs[I],Sizes[I])) || xrtMemRangesOverlap(Outputs[I],Sizes[I],Input,InputSize))return false;
        for(size_t J=0;J<I;++J)if(xrtMemRangesOverlap(Outputs[I],Sizes[I],Outputs[J],Sizes[J]))return false;
    }return true;
}
XCRON_API bool xcronZoneCreatePosix(xstrview Text,xcronzonegrammar Grammar,xcronzone** Out,xcronzoneissue* Issue)
{
    if(!Out || !xrtMemRangeValid(Out,sizeof(*Out)) || (!Text.Data && Text.Size) || !xrtMemRangeValid(Text.Data,Text.Size) ||
    (Issue && !xrtMemRangeValid(Issue,sizeof(*Issue))) || xrtMemRangesOverlap(Out,sizeof(*Out),Text.Data,Text.Size) ||
    (Issue && (xrtMemRangesOverlap(Issue,sizeof(*Issue),Text.Data,Text.Size) || xrtMemRangesOverlap(Issue,sizeof(*Issue),Out,sizeof(*Out)))) ||
    (Grammar!=XCRON_ZONE_POSIX && Grammar!=XCRON_ZONE_TZIF3))return xcronZoneFail(XCRON_ZONE_ARGUMENT,0,NULL);
    if(Text.Size>XCRON_ZONE_TEXT_MAX)return xcronZoneFail(XCRON_ZONE_LIMIT,XCRON_ZONE_TEXT_MAX,Issue);
    xcronzoneparser P={Text,0,Grammar,Issue};
    xstrview Names[2]={{0}};
    int32 Offsets[2]={0};
    xcronzonerule Rules[2];
    bool DST=false;
    if(!xcronZoneName(&P,&Names[0]) || !xcronZoneClock(&P,false,&Offsets[0]))return false;
    Offsets[0]=-Offsets[0];
    if(P.At<Text.Size){DST=true;
        if(!xcronZoneName(&P,&Names[1]))return false;
        Offsets[1]=Offsets[0]+3600;
        if(P.At<Text.Size && Text.Data[P.At]!=','){if(!xcronZoneClock(&P,false,&Offsets[1]))return false;
            Offsets[1]=-Offsets[1];
        }
        if(P.At==Text.Size)return xcronZoneFail(XCRON_ZONE_RULES_REQUIRED,P.At,Issue);
        if(Text.Data[P.At++]!=',' || !xcronZoneRule(&P,&Rules[0]) || P.At==Text.Size || Text.Data[P.At++]!=',' || !xcronZoneRule(&P,&Rules[1]))return xcronZoneFail(XCRON_ZONE_SYNTAX,P.At,Issue);
    }
    if(P.At!=Text.Size)return xcronZoneFail(XCRON_ZONE_SYNTAX,P.At,Issue);
    size_t Bytes=sizeof(xcronzone)+(DST?800*sizeof(int32):0)+Names[0].Size+1+(DST?Names[1].Size+1:0);
    xcronzone* Z=xrtCalloc(1,Bytes);
    if(!Z)return xcronZoneFail(XCRON_ZONE_MEMORY,P.At,Issue);
    xrtAtomic32Init(&Z->Refs,1);
    Z->Magic=XCRON_ZONE_MAGIC;
    Z->Bytes=Bytes;
    Z->Offsets[0]=Offsets[0];
    Z->Offsets[1]=Offsets[1];
    Z->HasDST=(uint8)DST;
    Z->Mode=DST?2:0;
    Z->Names[0]=(uint16)Names[0].Size;
    Z->Names[1]=(uint16)Names[1].Size;
    char* N=(char*)(Z->Changes+(DST?800:0));
    memcpy(N,Names[0].Data,Names[0].Size);
    if(DST)memcpy(N+Names[0].Size+1,Names[1].Data,Names[1].Size);
    if(DST){bool AlwaysSTD=true,AlwaysDST=true;
        for(unsigned Y=0;Y<400;++Y){Z->Changes[Y*2]=xcronZoneRuleSecond(Y,&Rules[0],Offsets[0]);
            Z->Changes[Y*2+1]=xcronZoneRuleSecond(Y,&Rules[1],Offsets[1]);
            if(Z->Changes[Y*2]!=Z->Changes[Y*2+1])AlwaysSTD=false;
            int64 Next=(int64)xrtDaysInYear(2000+Y)*86400+xcronZoneRuleSecond((Y+1)%400,&Rules[0],Offsets[0]);
            if(Z->Changes[Y*2+1]!=Next)AlwaysDST=false;
        }
        if(AlwaysSTD)Z->Mode=0;
        else if(AlwaysDST)Z->Mode=1;
    }
    if(Issue)*Issue=(xcronzoneissue){XCRON_ZONE_OK,Text.Size,DST?400:0};
    *Out=Z;
    return true;
}
XCRON_API bool xcronZoneRetain(const xcronzone* Zone)
{
    if(!xcronZoneHandle(Zone))return xcronZoneFail(XCRON_ZONE_ARGUMENT,0,NULL);
    xcronzone* Z=(xcronzone*)Zone;
    uint32 N=xrtAtomic32Load(&Z->Refs,XMEMORY_RELAXED);
    while(N && N<UINT32_MAX/2u)if(xrtAtomic32CompareExchange(&Z->Refs,&N,N+1,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;
    return xcronZoneFail(XCRON_ZONE_LIMIT,0,NULL);
}
XCRON_API void xcronZoneRelease(const xcronzone* Zone)
{xcronzone* Z=(xcronzone*)Zone;
    if(Z && xrtAtomic32FetchSub(&Z->Refs,1,XMEMORY_ACQ_REL)==1){Z->Magic=0;
        if(Z->History)xcronZoneHistoryRelease(Z);
        xrtFree(Z);
}}
static bool xcronZoneLookup(const xcronzone* Z,xtime When)
{
    if(Z->Mode!=2)return Z->Mode==1;
    xdatetime D;
    (void)xrtTimeSplit(When,&D);
    int64 Day=xcronZoneFloor(When,XRT_TIME_DAY)-(D.YearDay-1),Year=D.Year;
    for(unsigned I=0;I<2;++I){Year=xcronZoneYearStep(Year,-1);
        Day-=xrtDaysInYear(Year);
    }int64 Best=INT64_MIN,Second=xcronZoneFloor(When,1000);
    bool DST=false;
    /* A rule can carry less than nine days across its nominal year. The
     * surrounding two years therefore contain the latest applicable event.
     * At ties, a later rule-year start wins; same-year end wins over start. */
    for(unsigned Y=0;Y<5;++Y){unsigned Cycle=xcronZoneCycle(Year);
        for(unsigned K=0;K<2;++K){int64 T=Day*86400+Z->Changes[Cycle*2+K];
            if(T<=Second && T>=Best){Best=T;
                DST=K==0;
        }}
        Day+=xrtDaysInYear(Year);
        Year=xcronZoneYearStep(Year,1);
    }return DST;
}
static xcronzonestate xcronZoneState(const xcronzone* Z,bool DST)
{const char* Names=(const char*)(Z->Changes+(Z->HasDST?800:0));
    return (xcronzonestate){Z->Offsets[DST?1:0],DST,{Names+(DST?Z->Names[0]+1:0),Z->Names[DST?1:0]}};
}
xcronzonestate xcronZoneStateValue(const xcronzone* Z,xtime When)
{return Z->History?xcronZoneHistoryAt(Z,When):xcronZoneState(Z,xcronZoneLookup(Z,When));}
bool xcronZoneRepeat(const xcronzone* Z,xtime Begin,xtime* First)
{if(Z->History)return xcronZoneHistoryRepeat(Z,Begin,First);*First=Begin;return true;}
XCRON_API bool xcronZoneAt(const xcronzone* Z,xtime When,xcronzonestate* State,xcronzoneissue* Issue)
{
    void* Outputs[]={State,Issue};
    size_t Sizes[]={State?sizeof(*State):0,Issue?sizeof(*Issue):0};
    if(!State || !xcronZoneOutputs(Z,NULL,0,Outputs,Sizes,2))return xcronZoneFail(XCRON_ZONE_ARGUMENT,0,NULL);
    xcronzonestate Value=xcronZoneStateValue(Z,When);
    if(Value.Offset==XCRON_ZONE_OFFSET_UNKNOWN)return xcronZoneFail(XCRON_ZONE_UNKNOWN,0,Issue);
    *State=Value;
    if(Issue)*Issue=(xcronzoneissue){XCRON_ZONE_OK,0,!Z->History && Z->Mode==2?5:0};
    return true;
}
void xcronZoneDecompose(xtime When,int32 Offset,bool DST,xdatetime* Out)
{
    int64 Seconds=xcronZoneFloor(When,1000),Milli=When%1000;
    if(Milli<0)Milli+=1000;
    /* XRT's epoch is 0001-01-01, not the Unix epoch. Shift whole Gregorian
    * cycles to 2000..2399 before calling the public core decomposition. */
    const int64 Anchor=XRT_TIME_UNIX_EPOCH/XRT_TIME_DAY+10957;
    int64 Local=Seconds+Offset,Day=xcronZoneFloor(Local,86400),TOD=Local-Day*86400,Relative=Day-Anchor,Cycles=xcronZoneFloor(Relative,146097);
    xtime Safe=(Anchor+Relative-Cycles*146097)*XRT_TIME_DAY+TOD*1000+Milli;
    (void)xrtTimeSplit(Safe,Out);
    int64 Year=Out->Year+Cycles*400;
    Out->Year=Year>0?Year:Year-1;
    Out->Offset=Offset;
    Out->IsDST=DST?1:0;
}
XCRON_API bool xcronZoneSplit(const xcronzone* Z,xtime When,xdatetime* Date,xcronzoneissue* Issue)
{
    void* Outputs[]={Date,Issue};
    size_t Sizes[]={Date?sizeof(*Date):0,Issue?sizeof(*Issue):0};
    if(!Date || !xcronZoneOutputs(Z,NULL,0,Outputs,Sizes,2))return xcronZoneFail(XCRON_ZONE_ARGUMENT,0,NULL);
    xcronzonestate State=xcronZoneStateValue(Z,When);
    if(State.Offset==XCRON_ZONE_OFFSET_UNKNOWN)return xcronZoneFail(XCRON_ZONE_UNKNOWN,0,Issue);
    xcronZoneDecompose(When,State.Offset,State.IsDST,Date);
    if(Issue)*Issue=(xcronzoneissue){XCRON_ZONE_OK,0,!Z->History && Z->Mode==2?5:0};
    return true;
}
static bool xcronZoneCivil(const xdatetime* D)
{return D->Year && D->Month>=1 && D->Month<=12 && D->Day>=1 && D->Day<=xrtDaysInMonth(D->Year,D->Month) && D->Hour>=0 && D->Hour<=23 && D->Minute>=0 && D->Minute<=59 && D->Second>=0 && D->Second<=59 && D->Millisecond>=0 && D->Millisecond<=999;
}
bool xcronZoneCivilSeconds(const xdatetime* D,int64* Seconds)
{
    if(D->Year<-400000000 || D->Year>400000000)return false;
    int64 Year=D->Year<0?D->Year+1:D->Year,Cycles=xcronZoneFloor(Year,400),SafeYear=2000+Year-Cycles*400;
    xtime Safe;
    (void)xrtDateTime(SafeYear,D->Month,D->Day,D->Hour,D->Minute,D->Second,0,&Safe);
    *Seconds=Safe/1000+(Cycles-5)*INT64_C(12622780800);
    return true;
}
static bool xcronZoneInstant(const xdatetime* D,int32 Offset,xtime* Out)
{int64 Seconds;return xcronZoneCivilSeconds(D,&Seconds) && xcronZoneSecondValue(Seconds-Offset,D->Millisecond,Out);}
XCRON_API xcronzonestatus xcronZoneResolve(const xcronzone* Z,const xdatetime* Local,xtime* Candidates,size_t Capacity,size_t* Count,xcronzoneissue* Issue)
{
    void* Outputs[]={Candidates,Count,Issue};
    size_t Sizes[]={Capacity<=SIZE_MAX/sizeof(xtime)?Capacity*sizeof(xtime):0,Count?sizeof(*Count):0,Issue?sizeof(*Issue):0};
    if(!Local || !Count || Capacity>SIZE_MAX/sizeof(xtime) || (!Candidates && Capacity) || !xcronZoneOutputs(Z,Local,sizeof(*Local),Outputs,Sizes,3)){xcronZoneFail(XCRON_ZONE_ARGUMENT,0,NULL);
        return XCRON_ZONE_ERROR;
    }
    if(!xcronZoneCivil(Local)){xcronZoneFail(XCRON_ZONE_DATE,0,Issue);
        return XCRON_ZONE_ERROR;
    }
    if(Z->History)return xcronZoneHistoryResolve(Z,Local,Candidates,Capacity,Count,Issue);
    xtime Values[2];
    size_t N=0;
    for(unsigned K=0;K<(Z->HasDST?2u:1u);++K){xtime T;
        if(xcronZoneInstant(Local,Z->Offsets[K],&T) && Z->Offsets[xcronZoneLookup(Z,T)?1:0]==Z->Offsets[K] && (!N || Values[0]!=T))Values[N++]=T;
    }
    if(N==2 && Values[0]>Values[1]){xtime T=Values[0];
        Values[0]=Values[1];
        Values[1]=T;
    }*Count=N;
    if(Issue)*Issue=(xcronzoneissue){N>Capacity?XCRON_ZONE_LIMIT:XCRON_ZONE_OK,0,Z->Mode==2?10:0};
    if(N>Capacity)return XCRON_ZONE_CAPACITY;
    if(N)memcpy(Candidates,Values,N*sizeof(xtime));
    return N?XCRON_ZONE_FOUND:XCRON_ZONE_NONE;
}
XCRON_API xcronzonestatus xcronZoneNextTransition(const xcronzone* Z,xtime After,xcronzonetransition* Transition,xcronzoneissue* Issue)
{
    void* Outputs[]={Transition,Issue};
    size_t Sizes[]={Transition?sizeof(*Transition):0,Issue?sizeof(*Issue):0};
    if(!Transition || !xcronZoneOutputs(Z,NULL,0,Outputs,Sizes,2)){
        xcronZoneFail(XCRON_ZONE_ARGUMENT,0,NULL);
        return XCRON_ZONE_ERROR;
    }
    return xcronZoneNextValue(Z,After,Transition,Issue);
}
#if defined(_MSC_VER)
#define XCRON_ZONE_INLINE static __forceinline
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
#define XCRON_ZONE_INLINE static inline __attribute__((always_inline))
#else
#define XCRON_ZONE_INLINE static inline
#endif
/* Shared event/tie rules, specialized synchronous and resumable control loops. */
XCRON_ZONE_INLINE void xcronZoneCandidates(const xcronzone* Z,int64 Year,int64 Day,int64 Second,
    int64* Best,bool* Before,bool* Next)
{
    unsigned C=xcronZoneCycle(Year);
    for(unsigned K=0;K<2;++K){int64 T=Day*86400+Z->Changes[C*2+K];
        if(T<=Second || T>=*Best || T>INT64_MAX/1000)continue;
        bool A=xcronZoneLookup(Z,T*1000),B=xcronZoneLookup(Z,T*1000-1);
        if(A!=B){*Best=T;*Before=B;*Next=A;}
    }
}
void xcronZoneWalkInit(const xcronzone* Z,xtime After,xcronzonewalk* W)
{
    memset(W,0,sizeof(*W));W->Rules=Z;W->Status=XCRON_ZONE_NONE;
    if(Z->History){xcronZoneHistoryWalkInit(Z,After,W);return;}
    if(Z->Mode!=2)return;
    xdatetime D;(void)xrtTimeSplit(After,&D);
    W->Year=xcronZoneYearStep(D.Year,-1);
    W->Day=xcronZoneFloor(After,XRT_TIME_DAY)-(D.YearDay-1)-xrtDaysInYear(W->Year);
    W->Second=xcronZoneFloor(After,1000);W->Best=INT64_MAX;W->Status=XCRON_ZONE_WALK_PENDING;
}
xcronzonestatus xcronZoneWalkStep(xcronzonewalk* W,uint32 Budget)
{
    if(W->Status!=XCRON_ZONE_WALK_PENDING)return W->Status;
    uint32 End=403;if(Budget<End-W->Years)End=W->Years+Budget;
    while(W->Years<End){
        xcronZoneCandidates(W->Rules,W->Year,W->Day,W->Second,&W->Best,&W->Before,&W->After);
        ++W->Years;W->Day+=xrtDaysInYear(W->Year);W->Year=xcronZoneYearStep(W->Year,1);
        if((W->Best!=INT64_MAX && W->Day*86400-700000>W->Best) ||
            W->Day*86400-700000>INT64_MAX/1000 || W->Years==403){
            W->Status=W->Best==INT64_MAX?XCRON_ZONE_NONE:XCRON_ZONE_FOUND;
            if(W->Status==XCRON_ZONE_FOUND){
                W->Result=(xcronzonetransition){W->Best*1000,xcronZoneState(W->Rules,W->Before),xcronZoneState(W->Rules,W->After)};
                if(W->History)xcronZoneHistoryWalkResult(W->History,W);
            }
            break;
        }
    }
    return W->Status;
}
xcronzonestatus xcronZoneNextValue(const xcronzone* Z,xtime After,xcronzonetransition* Transition,xcronzoneissue* Issue)
{
    if(Issue)*Issue=(xcronzoneissue){XCRON_ZONE_OK,0,0};
    if(Z->History)return xcronZoneHistoryNext(Z,After,Transition,Issue);
    if(Z->Mode!=2)return XCRON_ZONE_NONE;
    xdatetime D;
    (void)xrtTimeSplit(After,&D);
    int64 Year=xcronZoneYearStep(D.Year,-1),Day=xcronZoneFloor(After,XRT_TIME_DAY)-(D.YearDay-1)-xrtDaysInYear(Year),Second=xcronZoneFloor(After,1000);
    int64 Best=INT64_MAX;
    bool Before=false,Next=false;
    /* UTC-year-1 through UTC-year+401 covers every Gregorian pattern and
     * cross-year carry. 700000s exceeds 167:59:59 + the largest offset (93599).
     * Compare actual states across ties before reporting an effective change. */
    for(unsigned Y=0;Y<403;++Y){if(Issue)Issue->YearsExamined=Y+1;
        xcronZoneCandidates(Z,Year,Day,Second,&Best,&Before,&Next);
        Day+=xrtDaysInYear(Year);
        Year=xcronZoneYearStep(Year,1);
        if(Best!=INT64_MAX && Day*86400-700000>Best)break;
        if(Day*86400-700000>INT64_MAX/1000)break;
    }
    if(Best==INT64_MAX)return XCRON_ZONE_NONE;
    *Transition=(xcronzonetransition){Best*1000,xcronZoneState(Z,Before),xcronZoneState(Z,Next)};
    return XCRON_ZONE_FOUND;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xcron/src/timezone/tzif.c */
/* ========================================================================== */

#if defined(XCRON_FEATURE_XCRON_TIMEZONE)
#include <string.h>
#if defined(XCRON_FEATURE_XCRON_TIMEZONE)
#define XCRON_TZIF_EPOCH_SECONDS (XRT_TIME_UNIX_EPOCH/1000)
#define XCRON_TZIF_CYCLE_SECONDS INT64_C(12622780800)
typedef struct xcrontziftype {int32 Offset;uint32 Name;uint16 Length;uint8 Flags;} xcrontziftype;
typedef struct xcrontzifrange {int64 Begin,End;} xcrontzifrange;
struct xcronzonehistory {
    xcronzone* Footer;
    int64 Last;
    uint32 Count,TypesCount,OffsetsCount,RangesCount;
    bool AllFooter,FooterUnknown;
    char Unknown[4];
    int64* Times;
    uint16* Indices;
    xcrontziftype* Types;
    int32* Offsets;
    xcrontzifrange* Ranges;
    char* Names;
};
typedef struct xcrontzifblock {
    uint8 Version,Width;
    uint32 Count,TypesCount,Chars,LeapsCount,StandardCount,UniversalCount;
    size_t At,End;
    const uint8 *Times,*Indices,*Types,*Names,*Leaps,*Standard,*Universal;
} xcrontzifblock;
typedef struct xcrontzifparsed {int32 Offset;bool DST,Known,Numeric;xstrview Name;uint32 NameOffset;} xcrontzifparsed;
static uint32 xcronTzifU32(const uint8* P)
{return ((uint32)P[0]<<24)|((uint32)P[1]<<16)|((uint32)P[2]<<8)|P[3];}
static int32 xcronTzifI32(const uint8* P)
{uint32 U=xcronTzifU32(P);return U<=INT32_MAX?(int32)U:-1-(int32)(UINT32_MAX-U);}
static int64 xcronTzifSecond(const uint8* P,unsigned Width)
{
    if(Width==4)return xcronTzifI32(P);
    uint64 U=((uint64)xcronTzifU32(P)<<32)|xcronTzifU32(P+4);
    return U<=INT64_MAX?(int64)U:-1-(int64)(UINT64_MAX-U);
}
static int64 xcronTzifUnix(xtime When)
{int64 S=When/1000-(When%1000<0);return S-XCRON_TZIF_EPOCH_SECONDS;}
static bool xcronTzifNameKnown(xstrview Name)
{return Name.Size!=3 || memcmp(Name.Data,"-00",3)!=0;}
static bool xcronTzifBlock(xbytesview Data,size_t At,unsigned Width,bool Main,xcrontzifblock* B,xcronzoneissue* Issue)
{
    if(At>Data.Size || Data.Size-At<44 || memcmp(Data.Data+At,"TZif",4))return xcronZoneFail(XCRON_ZONE_TZIF,At,Issue);
    const uint8* P=Data.Data+At;
    *B=(xcrontzifblock){0};B->At=At;B->Version=P[4];B->Width=(uint8)Width;
    B->UniversalCount=xcronTzifU32(P+20);B->StandardCount=xcronTzifU32(P+24);B->LeapsCount=xcronTzifU32(P+28);
    B->Count=xcronTzifU32(P+32);B->TypesCount=xcronTzifU32(P+36);B->Chars=xcronTzifU32(P+40);
    if(Main){
        if(!B->TypesCount || !B->Chars || (B->StandardCount && B->StandardCount!=B->TypesCount) ||
           (B->UniversalCount && B->UniversalCount!=B->TypesCount))return xcronZoneFail(XCRON_ZONE_TZIF,At+20,Issue);
        if(B->Count>XCRON_ZONE_TRANSITIONS_MAX || B->TypesCount>256 || B->Chars>65536 || B->LeapsCount>4096)return xcronZoneFail(XCRON_ZONE_LIMIT,At+28,Issue);
    }
    uint64 Size=(uint64)B->Count*(Width+1)+(uint64)B->TypesCount*6+B->Chars+(uint64)B->LeapsCount*(Width+4)+B->StandardCount+B->UniversalCount;
    if(Size>Data.Size-At-44)return xcronZoneFail(XCRON_ZONE_TZIF,At,Issue);
    B->Times=P+44;B->Indices=B->Times+(size_t)B->Count*Width;B->Types=B->Indices+B->Count;
    B->Names=B->Types+(size_t)B->TypesCount*6;B->Leaps=B->Names+B->Chars;
    B->Standard=B->Leaps+(size_t)B->LeapsCount*(Width+4);B->Universal=B->Standard+B->StandardCount;
    B->End=At+44+(size_t)Size;return true;
}
static size_t xcronTzifNumeric(int32 Offset,char* Out)
{
    uint32 N=(uint32)(Offset<0?-(int64)Offset:Offset),Hour=N/3600,Minute=N/60%60,Second=N%60;
    size_t At=0,Digits=0;char Reverse[10];Out[At++]=Offset<0?'-':'+';
    do {Reverse[Digits++]=(char)('0'+Hour%10);Hour/=10;} while(Hour);
    if(Digits<2)Out[At++]='0';
    while(Digits)Out[At++]=Reverse[--Digits];
    if(Minute || Second){Out[At++]=(char)('0'+Minute/10);Out[At++]=(char)('0'+Minute%10);}
    if(Second){Out[At++]=(char)('0'+Second/10);Out[At++]=(char)('0'+Second%10);}
    Out[At]=0;return At;
}
static bool xcronTzifValidate(const xcrontzifblock* B,const uint8* Base,xcrontzifparsed* Types,char Numeric[256][16],size_t* Extra,xcronzoneissue* Issue)
{
    int64 Previous=0;
    for(uint32 I=0;I<B->Count;++I){int64 S=xcronTzifSecond(B->Times+(size_t)I*B->Width,B->Width);
        if((I && S<=Previous) || B->Indices[I]>=B->TypesCount)return xcronZoneFail(XCRON_ZONE_TZIF,(size_t)(B->Times-Base)+(size_t)I*B->Width,Issue);
        Previous=S;
    }
    if(B->Names[B->Chars-1])return xcronZoneFail(XCRON_ZONE_TZIF,(size_t)(B->Names-Base)+B->Chars-1,Issue);
    *Extra=0;
    for(uint32 I=0;I<B->TypesCount;++I){const uint8* P=B->Types+(size_t)I*6;uint32 Index=P[5];int32 Offset=xcronTzifI32(P);
        if(Offset==INT32_MIN || P[4]>1 || Index>=B->Chars)return xcronZoneFail(XCRON_ZONE_TZIF,(size_t)(P-Base),Issue);
        const char* Name=(const char*)B->Names+Index;const char* End=memchr(Name,0,B->Chars-Index);
        if(!End)return xcronZoneFail(XCRON_ZONE_TZIF,(size_t)(P-Base)+5,Issue);
        size_t Length=(size_t)(End-Name);bool Safe=true;
        for(size_t K=0;K<Length;++K){unsigned char C=(unsigned char)Name[K];
            if(!((C>='A' && C<='Z') || (C>='a' && C<='z') || (C>='0' && C<='9') || C=='+' || C=='-')){Safe=false;break;}}
        Types[I]=(xcrontzifparsed){Offset,P[4]!=0,true,!Safe,{Name,Length},Index};
        if(!Safe){Length=xcronTzifNumeric(Offset,Numeric[I]);Types[I].Name=(xstrview){Numeric[I],Length};Types[I].NameOffset=(uint32)(B->Chars+*Extra);*Extra+=Length+1;}
        if(Length>XCRON_ZONE_NAME_MAX)return xcronZoneFail(XCRON_ZONE_LIMIT,(size_t)(P-Base)+5,Issue);
        Types[I].Known=xcronTzifNameKnown(Types[I].Name);
        uint8 Standard=B->StandardCount?B->Standard[I]:0,Universal=B->UniversalCount?B->Universal[I]:0;
        if(Standard>1 || Universal>1 || (Universal && !Standard))return xcronZoneFail(XCRON_ZONE_TZIF,(size_t)(B->Standard-Base),Issue);
    }
    int32 Correction=0;Previous=0;
    for(uint32 I=0;I<B->LeapsCount;++I){const uint8* P=B->Leaps+(size_t)I*(B->Width+4);int64 S=xcronTzifSecond(P,B->Width);int32 C=xcronTzifI32(P+B->Width);
        int64 Difference=(int64)C-Correction;bool Expiration=I && I+1==B->LeapsCount && B->Version=='4' && !Difference;
        if((I && S<=Previous) || (!I && S<0) || (!I && B->Version!='4' && C!=1 && C!=-1) ||
           (I && Difference!=1 && Difference!=-1 && !Expiration))return xcronZoneFail(XCRON_ZONE_TZIF,(size_t)(P-Base),Issue);
        if(!Expiration){
            bool Positive=I?Difference>0:C>0;
            int64 R=(S%XCRON_TZIF_CYCLE_SECONDS-C%XCRON_TZIF_CYCLE_SECONDS+(Positive?1:0))%XCRON_TZIF_CYCLE_SECONDS;
            if(R<0)R+=XCRON_TZIF_CYCLE_SECONDS;
            xdatetime D;(void)xrtTimeSplit(XRT_TIME_UNIX_EPOCH+R*1000,&D);
            if(D.Day!=1 || D.Hour || D.Minute || D.Second)return xcronZoneFail(XCRON_ZONE_TZIF,(size_t)(P-Base),Issue);
        }
        Previous=S;Correction=C;
    }
    return true;
}
static xcronzonestate xcronTzifUnknown(const xcronzonehistory* H)
{return (xcronzonestate){XCRON_ZONE_OFFSET_UNKNOWN,false,{H->Unknown,3}};}
static xcronzonestate xcronTzifFooterAt(const xcronzone* Footer,int64 Unix)
{
    int64 R=Unix%XCRON_TZIF_CYCLE_SECONDS;if(R<0)R+=XCRON_TZIF_CYCLE_SECONDS;
    xcronzonestate State;(void)xcronZoneAt(Footer,XRT_TIME_UNIX_EPOCH+R*1000,&State,NULL);
    return State;
}
static bool xcronTzifEqualParsed(const xcrontzifparsed* A,const xcrontzifparsed* B)
{return (!A->Known && !B->Known) || (A->Known==B->Known && A->Offset==B->Offset && A->DST==B->DST && A->Name.Size==B->Name.Size && !memcmp(A->Name.Data,B->Name.Data,A->Name.Size));}
static xcronzonestate xcronTzifType(const xcronzonehistory* H,uint32 Index)
{
    const xcrontziftype* T=H->Types+Index;
    if(!(T->Flags&2))return xcronTzifUnknown(H);
    return (xcronzonestate){T->Offset,(T->Flags&1)!=0,{H->Names+T->Name,T->Length}};
}
static uint32 xcronTzifUpper(const int64* Times,uint32 Count,int64 Second)
{uint32 Low=0,High=Count;while(Low<High){uint32 Middle=Low+(High-Low)/2;if(Times[Middle]<=Second)Low=Middle+1;else High=Middle;}return Low;}
static xcronzonestate xcronTzifAt(const xcronzonehistory* H,int64 Second)
{
    if(H->Footer && (H->AllFooter || Second>=H->Last)){
        xcronzonestate S=xcronTzifFooterAt(H->Footer,Second);return xcronTzifNameKnown(S.Abbreviation)?S:xcronTzifUnknown(H);
    }
    uint32 Index=xcronTzifUpper(H->Times,H->Count,Second);
    return xcronTzifType(H,Index?H->Indices[Index-1]:0);
}
static bool xcronTzifStateEqual(const xcronzonestate* A,const xcronzonestate* B)
{return A->Offset==B->Offset && A->IsDST==B->IsDST && A->Abbreviation.Size==B->Abbreviation.Size && !memcmp(A->Abbreviation.Data,B->Abbreviation.Data,A->Abbreviation.Size);}
static void xcronTzifOffset(int32* Offsets,uint32* Count,int32 Offset)
{
    uint32 I=0;while(I<*Count && Offsets[I]>Offset)++I;
    if(I<*Count && Offsets[I]==Offset)return;
    memmove(Offsets+I+1,Offsets+I,(*Count-I)*sizeof(*Offsets));Offsets[I]=Offset;++*Count;
}
static size_t xcronTzifAlign(size_t Size,size_t Alignment)
{return (Size+Alignment-1)/Alignment*Alignment;}
XCRON_API bool xcronZoneCreateTzif(xbytesview Data,xcronzone** Out,xcronzoneissue* Issue)
{
    if(!Out || !xrtMemRangeValid(Out,sizeof(*Out)) || (!Data.Data && Data.Size) || !xrtMemRangeValid(Data.Data,Data.Size) ||
       (Issue && !xrtMemRangeValid(Issue,sizeof(*Issue))) || xrtMemRangesOverlap(Out,sizeof(*Out),Data.Data,Data.Size) ||
       (Issue && (xrtMemRangesOverlap(Issue,sizeof(*Issue),Out,sizeof(*Out)) || xrtMemRangesOverlap(Issue,sizeof(*Issue),Data.Data,Data.Size))))return xcronZoneFail(XCRON_ZONE_ARGUMENT,0,NULL);
    if(Data.Size>XCRON_ZONE_TZIF_MAX)return xcronZoneFail(XCRON_ZONE_LIMIT,XCRON_ZONE_TZIF_MAX,Issue);
    xcrontzifblock B={0};
    if(!xcronTzifBlock(Data,0,4,false,&B,Issue))return false;
    uint8 Version=B.Version;xstrview FooterText={0};size_t FooterAt=0;
    if(Version && Version!='2' && Version!='3' && Version!='4')return xcronZoneFail(XCRON_ZONE_TZIF,4,Issue);
    if(Version){size_t At=B.End;
        if(!xcronTzifBlock(Data,At,8,true,&B,Issue))return false;
        if(B.Version!=Version)return xcronZoneFail(XCRON_ZONE_TZIF,At+4,Issue);
        if(Data.Size-B.End<2 || Data.Data[B.End]!='\n' || Data.Data[Data.Size-1]!='\n')return xcronZoneFail(XCRON_ZONE_TZIF,B.End,Issue);
        FooterAt=B.End+1;FooterText=(xstrview){(const char*)Data.Data+FooterAt,Data.Size-FooterAt-1};
        if(FooterText.Size>XCRON_ZONE_TEXT_MAX)return xcronZoneFail(XCRON_ZONE_LIMIT,FooterAt,Issue);
        if(memchr(FooterText.Data,0,FooterText.Size))return xcronZoneFail(XCRON_ZONE_TZIF,FooterAt,Issue);
    }else if(!xcronTzifBlock(Data,0,4,true,&B,Issue))return false;
    else if(B.End!=Data.Size)return xcronZoneFail(XCRON_ZONE_TZIF,B.End,Issue);
    xcrontzifparsed Parsed[257]={{0}};char Numeric[256][16];size_t Extra=0;
    if(!xcronTzifValidate(&B,Data.Data,Parsed,Numeric,&Extra,Issue))return false;
    if(B.LeapsCount)return xcronZoneFail(XCRON_ZONE_SCALE,(size_t)(B.Leaps-Data.Data),Issue);
    xcronzone* Footer=NULL;
    if(FooterText.Size){xcronzoneissue Local;
        if(!xcronZoneCreatePosix(FooterText,Version=='2'?XCRON_ZONE_POSIX:XCRON_ZONE_TZIF3,&Footer,&Local)){
            Local.Offset+=FooterAt;if(Issue)*Issue=Local;return false;
        }
    }
    int64 Last=B.Count?xcronTzifSecond(B.Times+(size_t)(B.Count-1)*B.Width,B.Width):0;
    if(Footer && B.Count){xcronzonestate S=xcronTzifFooterAt(Footer,Last);const xcrontzifparsed* T=Parsed+B.Indices[B.Count-1];
        if(S.Offset!=T->Offset || S.IsDST!=T->DST || S.Abbreviation.Size!=T->Name.Size || memcmp(S.Abbreviation.Data,T->Name.Data,T->Name.Size)){
            xcronZoneRelease(Footer);return xcronZoneFail(XCRON_ZONE_TZIF,FooterAt,Issue);
        }
    }
    Parsed[B.TypesCount]=(xcrontzifparsed){INT32_MIN,false,false,false,{"-00",3},0};
    uint32 Effective=0,Previous=0,UnknownCapacity=(!Footer || B.Count) && !Parsed[0].Known?1:0;
    for(uint32 I=0;I<B.Count;++I){uint32 Next=I+1==B.Count && !Footer?B.TypesCount:B.Indices[I];
        if(!xcronTzifEqualParsed(Parsed+Previous,Parsed+Next))++Effective;
        if((!Footer || I+1<B.Count) && Parsed[Previous].Known && !Parsed[Next].Known)++UnknownCapacity;
        Previous=Next;
    }
    int32 Offsets[258];uint32 OffsetCount=0;
    for(uint32 I=0;I<B.TypesCount;++I)if(Parsed[I].Known)xcronTzifOffset(Offsets,&OffsetCount,Parsed[I].Offset);
    bool FooterUnknown=false;
    if(Footer){const char* Names=(const char*)(Footer->Changes+(Footer->HasDST?800:0));
        for(unsigned K=0;K<(Footer->HasDST?2u:1u);++K){if((Footer->Mode==0 && K) || (Footer->Mode==1 && !K))continue;
            xstrview N={Names+(K?Footer->Names[0]+1:0),Footer->Names[K]};
            if(xcronTzifNameKnown(N))xcronTzifOffset(Offsets,&OffsetCount,Footer->Offsets[K]);else FooterUnknown=true;
        }
    }
    /* Count caps above bound this whole layout below 32MiB even on size_t32. */
    size_t Bytes=sizeof(xcronzone)+sizeof(xcronzonehistory);Bytes=xcronTzifAlign(Bytes,8);size_t TimesAt=Bytes;Bytes+=(size_t)Effective*sizeof(int64);
    size_t IndicesAt=Bytes;Bytes+=(size_t)Effective*sizeof(uint16);Bytes=xcronTzifAlign(Bytes,4);size_t TypesAt=Bytes;Bytes+=(B.TypesCount+1)*sizeof(xcrontziftype);
    size_t OffsetsAt=Bytes;Bytes+=OffsetCount*sizeof(int32);Bytes=xcronTzifAlign(Bytes,8);size_t RangesAt=Bytes;Bytes+=(size_t)UnknownCapacity*sizeof(xcrontzifrange);
    size_t NamesAt=Bytes;Bytes+=B.Chars+Extra;
    xcronzone* Z=xrtCalloc(1,Bytes);
    if(!Z){xcronZoneRelease(Footer);return xcronZoneFail(XCRON_ZONE_MEMORY,B.End,Issue);}
    xrtAtomic32Init(&Z->Refs,1);Z->Magic=XCRON_ZONE_MAGIC;Z->Bytes=Bytes;Z->History=(xcronzonehistory*)((char*)Z+sizeof(*Z));
    xcronzonehistory* H=Z->History;H->Footer=Footer;H->Last=Last;H->AllFooter=Footer && !B.Count;H->FooterUnknown=FooterUnknown;H->TypesCount=B.TypesCount;H->OffsetsCount=OffsetCount;memcpy(H->Unknown,"-00",4);
    H->Times=(int64*)((char*)Z+TimesAt);H->Indices=(uint16*)((char*)Z+IndicesAt);H->Types=(xcrontziftype*)((char*)Z+TypesAt);
    H->Offsets=(int32*)((char*)Z+OffsetsAt);H->Ranges=(xcrontzifrange*)((char*)Z+RangesAt);H->Names=(char*)Z+NamesAt;
    memcpy(H->Names,B.Names,B.Chars);memcpy(H->Offsets,Offsets,OffsetCount*sizeof(int32));
    for(uint32 I=0;I<B.TypesCount;++I){const xcrontzifparsed* T=Parsed+I;
        H->Types[I]=(xcrontziftype){T->Offset,T->NameOffset,(uint16)T->Name.Size,(uint8)((T->DST?1:0)|(T->Known?2:0))};
        if(T->Numeric)memcpy(H->Names+T->NameOffset,T->Name.Data,T->Name.Size+1);
    }
    Previous=0;
    for(uint32 I=0;I<B.Count;++I){uint32 Next=I+1==B.Count && !Footer?B.TypesCount:B.Indices[I];
        if(!xcronTzifEqualParsed(Parsed+Previous,Parsed+Next)){H->Times[H->Count]=xcronTzifSecond(B.Times+(size_t)I*B.Width,B.Width);H->Indices[H->Count++]=(uint16)Next;}
        Previous=Next;
    }
    if(!H->AllFooter){bool Unknown=!Parsed[0].Known;int64 Begin=INT64_MIN;
        for(uint32 I=0;I<H->Count;++I){int64 End=H->Times[I];if(Footer && End>=Last)break;bool Next=(H->Types[H->Indices[I]].Flags&2)==0;
            if(Unknown && !Next && Begin<End)H->Ranges[H->RangesCount++]=(xcrontzifrange){Begin,End};
            if(!Unknown && Next)Begin=End;
            Unknown=Next;
        }
        int64 End=Footer?Last:INT64_MAX;if(Unknown && Begin<End)H->Ranges[H->RangesCount++]=(xcrontzifrange){Begin,End};
    }
    if(Issue)*Issue=(xcronzoneissue){XCRON_ZONE_OK,Data.Size,0};
    *Out=Z;return true;
}
xcronzonestate xcronZoneHistoryAt(const xcronzone* Z,xtime When)
{return xcronTzifAt(Z->History,xcronTzifUnix(When));}
bool xcronZoneHistoryOverlap(const xcronzone* Z,const void* Output,size_t Size)
{const xcronzone* F=Z->History->Footer;return F && xrtMemRangesOverlap(Output,Size,F,F->Bytes);}
void xcronZoneHistoryRelease(const xcronzone* Z)
{xcronZoneRelease(Z->History->Footer);}
#if defined(XCRON_FEATURE_XCRON_TZDB)
size_t xcronZoneOwnedBytes(const xcronzone* Z)
{return Z->Bytes+(Z->History && Z->History->Footer?Z->History->Footer->Bytes:0);}
#endif
bool xcronZoneHistoryRepeat(const xcronzone* Z,xtime Begin,xtime* First)
{
    const xcronzonehistory* H=Z->History;
    if((!H->Footer && !H->Count) || H->AllFooter || (H->Footer && xcronTzifUnix(Begin)>=H->Last)){*First=Begin;return true;}
    if(!H->Footer || H->Last>INT64_MAX/1000-XCRON_TZIF_EPOCH_SECONDS)return false;
    return xcronZoneSecondValue(H->Last+XCRON_TZIF_EPOCH_SECONDS,0,First);
}
xcronzonestatus xcronZoneHistoryNext(const xcronzone* Z,xtime After,xcronzonetransition* Transition,xcronzoneissue* Issue)
{
    const xcronzonehistory* H=Z->History;int64 Second=xcronTzifUnix(After);uint32 I=xcronTzifUpper(H->Times,H->Count,Second);
    if(I<H->Count){int64 S=H->Times[I];
        if(S>INT64_MAX/1000-XCRON_TZIF_EPOCH_SECONDS)return XCRON_ZONE_NONE;
        xtime T;if(!xcronZoneSecondValue(S+XCRON_TZIF_EPOCH_SECONDS,0,&T))return XCRON_ZONE_NONE;
        *Transition=(xcronzonetransition){T,xcronTzifAt(H,S-1),xcronTzifAt(H,S)};return XCRON_ZONE_FOUND;
    }
    if(!H->Footer)return XCRON_ZONE_NONE;
    xtime Start=After;
    if(!H->AllFooter && Second<H->Last){
        if(H->Last>INT64_MAX/1000-XCRON_TZIF_EPOCH_SECONDS || !xcronZoneSecondValue(H->Last+XCRON_TZIF_EPOCH_SECONDS,0,&Start))return XCRON_ZONE_NONE;
    }
    xcronzonetransition N;xcronzonestatus Status=xcronZoneNextTransition(H->Footer,Start,&N,Issue);
    if(Status==XCRON_ZONE_FOUND){
        if(!xcronTzifNameKnown(N.Before.Abbreviation))N.Before=xcronTzifUnknown(H);
        if(!xcronTzifNameKnown(N.After.Abbreviation))N.After=xcronTzifUnknown(H);
        if(!xcronTzifStateEqual(&N.Before,&N.After))*Transition=N;
        else return XCRON_ZONE_NONE;
    }
    return Status;
}
void xcronZoneHistoryWalkInit(const xcronzone* Z,xtime After,xcronzonewalk* W)
{
    const xcronzonehistory* H=Z->History;int64 Second=xcronTzifUnix(After);
    uint32 I=xcronTzifUpper(H->Times,H->Count,Second);
    if(I<H->Count){int64 S=H->Times[I];xtime T;
        if(S>INT64_MAX/1000-XCRON_TZIF_EPOCH_SECONDS || !xcronZoneSecondValue(S+XCRON_TZIF_EPOCH_SECONDS,0,&T))return;
        W->Result=(xcronzonetransition){T,xcronTzifAt(H,S-1),xcronTzifAt(H,S)};W->Status=XCRON_ZONE_FOUND;return;
    }
    if(!H->Footer)return;
    xtime Start=After;
    if(!H->AllFooter && Second<H->Last &&
        (H->Last>INT64_MAX/1000-XCRON_TZIF_EPOCH_SECONDS || !xcronZoneSecondValue(H->Last+XCRON_TZIF_EPOCH_SECONDS,0,&Start)))return;
    xcronZoneWalkInit(H->Footer,Start,W);W->History=Z;
}
void xcronZoneHistoryWalkResult(const xcronzone* Z,xcronzonewalk* W)
{
    const xcronzonehistory* H=Z->History;
    if(!xcronTzifNameKnown(W->Result.Before.Abbreviation))W->Result.Before=xcronTzifUnknown(H);
    if(!xcronTzifNameKnown(W->Result.After.Abbreviation))W->Result.After=xcronTzifUnknown(H);
    if(xcronTzifStateEqual(&W->Result.Before,&W->Result.After))W->Status=XCRON_ZONE_NONE;
}
static bool xcronTzifUncertain(const xcronzonehistory* H,int64 Low,int64 High,int Milli)
{
    uint32 A=0,B=H->RangesCount;
    while(A<B){uint32 M=A+(B-A)/2;if(H->Ranges[M].End<=Low)A=M+1;else B=M;}
    if(A<H->RangesCount && H->Ranges[A].Begin<=High)return true;
    if(!H->FooterUnknown || (!H->AllFooter && High<H->Last))return false;
    if(!H->AllFooter && Low<H->Last)Low=H->Last;
    xcronzonestate State=xcronTzifFooterAt(H->Footer,Low);if(!xcronTzifNameKnown(State.Abbreviation))return true;
    xtime Start,End;if(!xcronZoneSecondValue(Low+XCRON_TZIF_EPOCH_SECONDS,Milli,&Start) || !xcronZoneSecondValue(High+XCRON_TZIF_EPOCH_SECONDS,Milli,&End))return false;
    xcronzonetransition N;
    while(xcronZoneNextTransition(H->Footer,Start,&N,NULL)==XCRON_ZONE_FOUND && N.When<=End){
        if(!xcronTzifNameKnown(N.After.Abbreviation))return true;
        Start=N.When;
    }
    return false;
}
xcronzonestatus xcronZoneHistoryResolve(const xcronzone* Z,const xdatetime* D,xtime* Candidates,size_t Capacity,size_t* Count,xcronzoneissue* Issue)
{
    const xcronzonehistory* H=Z->History;int64 Local;
    if(!xcronZoneCivilSeconds(D,&Local)){*Count=0;if(Issue)*Issue=(xcronzoneissue){XCRON_ZONE_OK,0,0};return XCRON_ZONE_NONE;}
    int64 Low=Local-INT32_MAX-XCRON_TZIF_EPOCH_SECONDS,High=Local+INT32_MAX-XCRON_TZIF_EPOCH_SECONDS;
    int64 Minimum=INT64_MIN/1000-1+(D->Millisecond<(1000+INT64_MIN%1000)?1:0)-XCRON_TZIF_EPOCH_SECONDS;
    int64 Maximum=INT64_MAX/1000-(D->Millisecond>INT64_MAX%1000?1:0)-XCRON_TZIF_EPOCH_SECONDS;
    if(Low<Minimum)Low=Minimum;
    if(High>Maximum)High=Maximum;
    if(Low<=High && xcronTzifUncertain(H,Low,High,D->Millisecond)){xcronZoneFail(XCRON_ZONE_UNKNOWN,0,Issue);return XCRON_ZONE_ERROR;}
    xtime Values[258];size_t N=0;
    for(uint32 I=0;I<H->OffsetsCount;++I){xtime T;int32 Offset=H->Offsets[I];
        if(xcronZoneSecondValue(Local-Offset,D->Millisecond,&T) && xcronTzifAt(H,xcronTzifUnix(T)).Offset==Offset)Values[N++]=T;
    }
    *Count=N;if(Issue)*Issue=(xcronzoneissue){N>Capacity?XCRON_ZONE_LIMIT:XCRON_ZONE_OK,0,0};
    if(N>Capacity)return XCRON_ZONE_CAPACITY;
    if(N)memcpy(Candidates,Values,N*sizeof(*Candidates));
    return N?XCRON_ZONE_FOUND:XCRON_ZONE_NONE;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xcron/src/zoned/zoned.c */
/* ========================================================================== */

#if defined(XCRON_FEATURE_XCRON_ZONED)
#include <string.h>
#include <stddef.h>
#if defined(XCRON_FEATURE_XCRON_ZONED)
#define XCRON_ZONED_CYCLE_MS INT64_C(12622780800000)
static bool xcronZonedFail(xcronzonederror Code,xcronfield Field,xcronzoneerror Zone,xcronzonedissue* Work,xcronzonedissue* Issue,cstr Message)
{
    Work->Code=Code;Work->Field=Field;Work->Zone=Zone;if(Issue)*Issue=*Work;
    xrtSetErrorInfo(Code==XCRON_ZONED_ARGUMENT?XERR_ARGUMENT:Code==XCRON_ZONED_EXPRESSION?XERR_VALUE:(Code==XCRON_ZONED_ZONE || Code==XCRON_ZONED_STATE)?XERR_STATE:XERR_RANGE,
        "xrt.cron.zoned",Code,Message);return false;
}
static bool xcronZonedArgs(const xcronexpression* E,const xcronzone* Z,uint32 Flags,void* Result,size_t ResultSize,xcronzonedissue* Issue)
{
    void* Outputs[]={Result,Issue};size_t Sizes[]={ResultSize,Issue?sizeof(*Issue):0};
    return E && Result && !(Flags&~XCRON_SEARCH_EXCLUSIVE) && xcronZoneOutputs(Z,E,sizeof(*E),Outputs,Sizes,2);
}
static bool xcronZonedValidate(const xcronexpression* E,xcronzonedissue* Work,xcronzonedissue* Issue)
{
    xcronfield Field;if(xcronCalendarValidateExpression(E,&Field))return true;
    return xcronZonedFail(XCRON_ZONED_EXPRESSION,Field,XCRON_ZONE_OK,Work,Issue,"invalid compiled cron expression");
}
XCRON_API bool xcronZonedMatch(const xcronexpression* E,const xcronzone* Z,xtime When,bool* Matches,xcronzonedissue* Issue)
{
    xcronzonedissue Work={XCRON_ZONED_OK,XCRON_FIELD_NONE,XCRON_ZONE_OK,0,0};
    if(!xcronZonedArgs(E,Z,0,Matches,sizeof(*Matches),Issue))return xcronZonedFail(XCRON_ZONED_ARGUMENT,XCRON_FIELD_NONE,XCRON_ZONE_OK,&Work,NULL,"invalid zoned cron arguments/spans/output aliases");
    if(!xcronZonedValidate(E,&Work,Issue))return false;
    bool Value=false;
    if(!(When%1000)){
        ++Work.SegmentsExamined;xcronzonestate State=xcronZoneStateValue(Z,When);
        if(State.Offset==XCRON_ZONE_OFFSET_UNKNOWN)return xcronZonedFail(XCRON_ZONED_ZONE,XCRON_FIELD_NONE,XCRON_ZONE_UNKNOWN,&Work,Issue,"cron instant has unknown zone coverage");
        xdatetime Date;xcronZoneDecompose(When,State.Offset,State.IsDST,&Date);Value=xcronCalendarPartsMatch(E,&Date);
    }
    *Matches=Value;if(Issue)*Issue=Work;return true;
}
static void xcronZonedValueFail(xcronzonederror Code,xcronzoneerror Zone,xcronzonedissue* Work,xcronzonedissue* Issue)
{Work->Code=Code;Work->Zone=Zone;if(Issue)*Issue=*Work;}
xcronzonedstatus xcronZonedNextValue(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,xtime* Next,xcronzonedissue* Issue)
{
    xcronzonedissue Work={XCRON_ZONED_OK,XCRON_FIELD_NONE,XCRON_ZONE_OK,0,0};
    int64 Remainder=Begin%1000,Delta=Remainder?(Remainder>0?1000-Remainder:-Remainder):((Flags&XCRON_SEARCH_EXCLUSIVE)?1000:0);
    if(Begin>INT64_MAX-Delta || Begin+Delta>End){if(Issue)*Issue=Work;return XCRON_ZONED_NONE;}
    xtime First=Begin+Delta,Limit=End,Repeat;
    if(xcronZoneRepeat(Z,First,&Repeat) && Repeat<=End && Repeat<=INT64_MAX-(XCRON_ZONED_CYCLE_MS-1000)){
        xtime ProofEnd=Repeat+(XCRON_ZONED_CYCLE_MS-1000);if(ProofEnd<Limit)Limit=ProofEnd;
    }
    for(;;){
        ++Work.SegmentsExamined;xcronzonestate State=xcronZoneStateValue(Z,First);
        if(State.Offset==XCRON_ZONE_OFFSET_UNKNOWN){xcronZonedValueFail(XCRON_ZONED_ZONE,XCRON_ZONE_UNKNOWN,&Work,Issue);return XCRON_ZONED_ERROR;}
        xdatetime LocalBegin;xcronZoneDecompose(First,State.Offset,State.IsDST,&LocalBegin);
        if(xcronCalendarPartsMatch(E,&LocalBegin)){++Work.MonthsExamined;*Next=First;if(Issue)*Issue=Work;return XCRON_ZONED_FOUND;}
        xcronzonetransition Transition;xcronzoneissue ZI;
        xcronzonestatus ZS=xcronZoneNextValue(Z,First,&Transition,&ZI);
        if(ZS==XCRON_ZONE_ERROR){xcronZonedValueFail(XCRON_ZONED_ZONE,ZI.Code,&Work,Issue);return XCRON_ZONED_ERROR;}
        xtime SegmentEnd=Limit;if(ZS==XCRON_ZONE_FOUND && Transition.When<=Limit)SegmentEnd=Transition.When-1;
        xdatetime LocalEnd,Value;xcronZoneDecompose(SegmentEnd,State.Offset,State.IsDST,&LocalEnd);
        xcroncalendarissue CI;xcroncalendarstatus CS=xcronCalendarSearch(E,&LocalBegin,&LocalEnd,0,&Value,&CI);Work.MonthsExamined+=CI.MonthsExamined;
        if(CS==XCRON_CALENDAR_FOUND){int64 Seconds;xtime Result;
            if(!xcronZoneCivilSeconds(&Value,&Seconds) || !xcronZoneSecondValue(Seconds-State.Offset,0,&Result) || Result<First || Result>SegmentEnd){
                xcronZonedValueFail(XCRON_ZONED_RANGE,XCRON_ZONE_OK,&Work,Issue);return XCRON_ZONED_ERROR;
            }
            *Next=Result;if(Issue)*Issue=Work;return XCRON_ZONED_FOUND;
        }
        if(CS==XCRON_CALENDAR_ERROR){xcronZonedValueFail(XCRON_ZONED_RANGE,XCRON_ZONE_OK,&Work,Issue);return XCRON_ZONED_ERROR;}
        /* A complete civil cycle without a label proves global impossibility,
         * independently of all later offsets (including unknown coverage). */
        if((First<=INT64_MAX-(XCRON_ZONED_CYCLE_MS-1000) && SegmentEnd>=First+(XCRON_ZONED_CYCLE_MS-1000)) || ZS==XCRON_ZONE_NONE || Transition.When>Limit)break;
        First=Transition.When;
    }
    if(Issue)*Issue=Work;
    return XCRON_ZONED_NONE;
}
XCRON_API xcronzonedstatus xcronZonedNext(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,xtime* Next,xcronzonedissue* Issue)
{
    xcronzonedissue Work={XCRON_ZONED_OK,XCRON_FIELD_NONE,XCRON_ZONE_OK,0,0};
    if(!xcronZonedArgs(E,Z,Flags,Next,sizeof(*Next),Issue)){xcronZonedFail(XCRON_ZONED_ARGUMENT,XCRON_FIELD_NONE,XCRON_ZONE_OK,&Work,NULL,"invalid zoned cron arguments/spans/output aliases");return XCRON_ZONED_ERROR;}
    if(!xcronZonedValidate(E,&Work,Issue))return XCRON_ZONED_ERROR;
    if(Begin>End){xcronZonedFail(XCRON_ZONED_WINDOW,XCRON_FIELD_NONE,XCRON_ZONE_OK,&Work,Issue,"zoned cron window is reversed");return XCRON_ZONED_ERROR;}
    xcronzonedstatus Status=xcronZonedNextValue(E,Z,Begin,End,Flags,Next,&Work);
    if(Status==XCRON_ZONED_ERROR)xcronZonedFail(Work.Code,Work.Field,Work.Zone,&Work,NULL,
        Work.Code==XCRON_ZONED_ZONE?(Work.Zone==XCRON_ZONE_UNKNOWN?"cron search reached unknown zone coverage":"cron transition search failed"):
        "invalid derived zoned civil window or candidate");
    if(Issue)*Issue=Work;
    return Status;
}
#if defined(_MSC_VER)
#define XCRON_ZONED_INLINE static __forceinline
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
#define XCRON_ZONED_INLINE static inline __attribute__((always_inline))
#else
#define XCRON_ZONED_INLINE static inline
#endif
XCRON_ZONED_INLINE bool xcronZonedFirst(xtime Begin,xtime End,uint32 Flags,xtime* First)
{
    int64 R=Begin%1000,Delta=R?(R>0?1000-R:-R):((Flags&XCRON_SEARCH_EXCLUSIVE)?1000:0);
    if(Begin>INT64_MAX-Delta || Begin+Delta>End)return false;
    *First=Begin+Delta;return true;
}
XCRON_ZONED_INLINE xcronzonedstatus xcronZonedProbe(const xcronexpression* E,const xcronzone* Z,xtime First,xdatetime* Local,int32* Offset,xcronzonedcursorissue* Work)
{
    ++Work->SegmentsExamined;xcronzonestate State=xcronZoneStateValue(Z,First);
    if(State.Offset==XCRON_ZONE_OFFSET_UNKNOWN){Work->Code=XCRON_ZONED_ZONE;Work->Zone=XCRON_ZONE_UNKNOWN;return XCRON_ZONED_ERROR;}
    *Offset=State.Offset;xcronZoneDecompose(First,State.Offset,State.IsDST,Local);
    if(xcronCalendarPartsMatch(E,Local)){++Work->MonthsExamined;return XCRON_ZONED_FOUND;}
    return XCRON_ZONED_PENDING;
}
xcronzonedstatus xcronZonedReadyValue(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,
    xtime* First,xdatetime* Local,int32* Offset,xcronzonedcursorissue* Work)
{
    *Work=(xcronzonedcursorissue){.Field=XCRON_FIELD_NONE};
    if(!xcronZonedFirst(Begin,End,Flags,First))return XCRON_ZONED_NONE;
    return xcronZonedProbe(E,Z,*First,Local,Offset,Work);
}
void xcronZonedValueRestart(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,xcronzonedvalue* V)
{
    V->Expression=*E;V->Work=(xcronzonedcursorissue){.Field=XCRON_FIELD_NONE};V->Status=XCRON_ZONED_NONE;V->Phase=0;
    if(!xcronZonedFirst(Begin,End,Flags,&V->First))return;
    V->Limit=End;xtime Repeat;
    if(xcronZoneRepeat(Z,V->First,&Repeat) && Repeat<=End && Repeat<=INT64_MAX-(XCRON_ZONED_CYCLE_MS-1000)){
        xtime ProofEnd=Repeat+(XCRON_ZONED_CYCLE_MS-1000);if(ProofEnd<V->Limit)V->Limit=ProofEnd;
    }
    V->Status=XCRON_ZONED_PENDING;
}
void xcronZonedValueInit(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,xcronzonedvalue* V)
{memset(V,0,sizeof(*V));xcronZonedValueRestart(E,Z,Begin,End,Flags,V);}
static xcronzonedstatus xcronZonedValueError(xcronzonedvalue* V,xcronzonederror Code,xcronzoneerror Zone)
{V->Work.Code=Code;V->Work.Zone=Zone;return V->Status=XCRON_ZONED_ERROR;}
xcronzonedstatus xcronZonedValueStep(xcronzonedvalue* V,const xcronzone* Z,xcronzonedbudget B)
{
    while(V->Status==XCRON_ZONED_PENDING){
        if(V->Phase==0){
            if(!B.Segments || !B.Months)return V->Status;
            --B.Segments;xcronzonedstatus Ready=xcronZonedProbe(&V->Expression,Z,V->First,&V->Calendar.First,&V->Offset,&V->Work);
            if(Ready==XCRON_ZONED_ERROR)return V->Status=XCRON_ZONED_ERROR;
            if(Ready==XCRON_ZONED_FOUND){V->Result=V->First;return V->Status=XCRON_ZONED_FOUND;}
            xcronZoneWalkInit(Z,V->First,&V->Transition);V->Phase=1;
        }
        if(V->Phase==1){
            uint32 Before=V->Transition.Years;
            xcronzonestatus Status=xcronZoneWalkStep(&V->Transition,B.RuleYears);
            uint32 Used=V->Transition.Years-Before;B.RuleYears-=Used;V->Work.RuleYearsExamined+=Used;
            if(Status==XCRON_ZONE_WALK_PENDING)return V->Status;
            V->SegmentEnd=V->Limit;
            if(Status==XCRON_ZONE_FOUND && V->Transition.Result.When<=V->Limit)V->SegmentEnd=V->Transition.Result.When-1;
            xdatetime End,First=V->Calendar.First;xcronZoneDecompose(V->SegmentEnd,V->Offset,false,&End);
            xcronCalendarValueBegin(&V->Expression,&First,&End,&V->Calendar);V->Phase=2;
        }
        if(V->Phase==2){
            uint32 Before=V->Calendar.Months;
            xcroncalendarstatus Status=xcronCalendarValueStep(&V->Expression,&V->Calendar,B.Months);
            uint32 Used=V->Calendar.Months-Before;B.Months-=Used;V->Work.MonthsExamined+=Used;
            if(Status==XCRON_CALENDAR_PENDING)return V->Status;
            if(Status==XCRON_CALENDAR_FOUND){int64 Seconds;xtime Result;
                if(!xcronZoneCivilSeconds(&V->Calendar.Date,&Seconds) || !xcronZoneSecondValue(Seconds-V->Offset,0,&Result) || Result<V->First || Result>V->SegmentEnd)
                    return xcronZonedValueError(V,XCRON_ZONED_RANGE,XCRON_ZONE_OK);
                V->Result=Result;return V->Status=XCRON_ZONED_FOUND;
            }
            if((V->First<=INT64_MAX-(XCRON_ZONED_CYCLE_MS-1000) && V->SegmentEnd>=V->First+(XCRON_ZONED_CYCLE_MS-1000)) ||
                V->Transition.Status==XCRON_ZONE_NONE || V->Transition.Result.When>V->Limit)return V->Status=XCRON_ZONED_NONE;
            V->First=V->Transition.Result.When;V->Phase=0;
        }
    }
    return V->Status;
}
#define XCRON_ZONED_CURSOR_MAGIC UINT32_C(0x585a4331)
typedef struct xcronzonedimpl {
    uint32 Magic;
    uintptr_t Address,Zone;
    xcronzonedvalue Value;
} xcronzonedimpl;
typedef char xcron_zoned_cursor_size_check[(sizeof(xcronzonedimpl)<=sizeof(xcronzonedcursor))?1:-1];
static void xcronZonedCursorReport(xcronzonedcursorissue* Issue,xcronzonederror Code,xcronfield Field,cstr Message)
{
    if(Issue)*Issue=(xcronzonedcursorissue){Code,Field,XCRON_ZONE_OK,0,0,0};
    xcronzonedissue Work={0};xcronZonedFail(Code,Field,XCRON_ZONE_OK,&Work,NULL,Message);
}
XCRON_API bool xcronZonedCursorInit(xcronzonedcursor* C,const xcronexpression* E,const xcronzone* Z,
    xtime Begin,xtime End,uint32 Flags,xcronzonedcursorissue* Issue)
{
    void* Outputs[]={C,Issue};size_t Sizes[]={sizeof(*C),Issue?sizeof(*Issue):0};
    if(!C || !E || (Flags&~XCRON_SEARCH_EXCLUSIVE) || !xcronZoneOutputs(Z,E,sizeof(*E),Outputs,Sizes,2)){
        xcronZonedCursorReport(NULL,XCRON_ZONED_ARGUMENT,XCRON_FIELD_NONE,"invalid zoned cursor init spans/aliases/flags");return false;
    }
    xcronfield Field;
    if(!xcronCalendarValidateExpression(E,&Field)){xcronZonedCursorReport(Issue,XCRON_ZONED_EXPRESSION,Field,"invalid zoned cursor expression");return false;}
    if(Begin>End){xcronZonedCursorReport(Issue,XCRON_ZONED_WINDOW,XCRON_FIELD_NONE,"zoned cursor window is reversed");return false;}
    xcronzonedimpl I;memset(&I,0,offsetof(xcronzonedimpl,Value));I.Magic=XCRON_ZONED_CURSOR_MAGIC;I.Address=(uintptr_t)C;I.Zone=(uintptr_t)Z;
    xcronZonedValueInit(E,Z,Begin,End,Flags,&I.Value);
    memcpy(C,&I,sizeof(I));memset(C->Storage+sizeof(I),0,sizeof(*C)-sizeof(I));if(Issue)*Issue=I.Value.Work;return true;
}
XCRON_API xcronzonedstatus xcronZonedCursorStep(xcronzonedcursor* C,const xcronzone* Z,xcronzonedbudget Budget,
    xtime* Next,xcronzonedcursorissue* Issue)
{
    void* Outputs[]={C,Next,Issue};size_t Sizes[]={sizeof(*C),sizeof(*Next),Issue?sizeof(*Issue):0};
    if(!C || !Next || !xcronZoneOutputs(Z,NULL,0,Outputs,Sizes,3)){
        xcronZonedCursorReport(NULL,XCRON_ZONED_ARGUMENT,XCRON_FIELD_NONE,"invalid zoned cursor step spans/aliases");return XCRON_ZONED_ERROR;
    }
    if(!Budget.Segments || !Budget.Months || !Budget.RuleYears){
        xcronZonedCursorReport(Issue,XCRON_ZONED_ARGUMENT,XCRON_FIELD_NONE,"zoned cursor budgets must be positive");return XCRON_ZONED_ERROR;
    }
    xcronzonedimpl I;memcpy(&I,C,sizeof(I));
    if(I.Magic!=XCRON_ZONED_CURSOR_MAGIC || I.Address!=(uintptr_t)C || I.Zone!=(uintptr_t)Z || I.Value.Phase>2 ||
        (I.Value.Status!=XCRON_ZONED_PENDING && I.Value.Status!=XCRON_ZONED_FOUND && I.Value.Status!=XCRON_ZONED_NONE && I.Value.Status!=XCRON_ZONED_ERROR)){
        xcronZonedCursorReport(Issue,XCRON_ZONED_STATE,XCRON_FIELD_NONE,"uninitialized/copied cursor or wrong live zone");return XCRON_ZONED_ERROR;
    }
    xcronzonedstatus Status=xcronZonedValueStep(&I.Value,Z,Budget);
    memcpy(C,&I,sizeof(I));if(Status==XCRON_ZONED_FOUND)*Next=I.Value.Result;if(Issue)*Issue=I.Value.Work;
    if(Status==XCRON_ZONED_ERROR){xcronzonedissue Work={0};
        xcronZonedFail(I.Value.Work.Code,I.Value.Work.Field,I.Value.Work.Zone,&Work,NULL,"zoned cursor reached unknown coverage or an invalid derived candidate");
    }
    return Status;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xcron/src/scheduler/scheduler.c */
/* ========================================================================== */

#if defined(XCRON_FEATURE_XCRON_SCHEDULER)
#include <string.h>
#if defined(XCRON_FEATURE_XCRON_SCHEDULER)
#define XCRON_SCHEDULER_MAGIC UINT32_C(0x58435331)
#define XCRON_SLOT_NONE UINT32_MAX
#if defined(_MSC_VER)
#define XCRON_SCHEDULER_INLINE static __forceinline
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
#define XCRON_SCHEDULER_INLINE static inline __attribute__((always_inline))
#else
#define XCRON_SCHEDULER_INLINE static inline
#endif
typedef enum xcronlocation {XCRON_LOC_FREE=0,XCRON_LOC_HEAP,XCRON_LOC_READY,XCRON_LOC_BLOCKED,XCRON_LOC_RETIRED,XCRON_LOC_RUNNING,XCRON_LOC_SEARCH} xcronlocation;
typedef struct xcronjobslot {
    xcronjobconfig Config;
    xcronjobinfo Info;
    uint64 Generation;
    uint32 Next,Previous,Position;
    xcronlocation Location;
    bool Used,Initial;
} xcronjobslot;
typedef struct xcronrunslot {uint64 Generation;uint32 Job,Next;bool Used;} xcronrunslot;
struct xcronscheduler {
    uint32 Magic;
    xcronschedulerconfig Config;
    xcronschedulerstats Stats;
    xcronjobslot* Jobs;
    xcronrunslot* Runs;
    xcronzonedvalue* Searches;
    uint32* Heap;
    uint32 FreeJob,FreeRun,ReadyHead,ReadyTail,RetireHead,RetireTail;
    uint32 SearchHead,SearchTail,Searching;
    uint8 Turn;
    bool Promote;
    uint8 JobBits,RunBits;
    _Alignas(xcronjobslot) unsigned char Storage[];
};
static bool xcronSchedulerSpan(const void* P,size_t N)
{return (!N || P) && N<=UINTPTR_MAX-(uintptr_t)P;}
static bool xcronSchedulerOverlap(const void* A,size_t AN,const void* B,size_t BN)
{return AN && BN && (uintptr_t)A<(uintptr_t)B+BN && (uintptr_t)B<(uintptr_t)A+AN;}
static bool xcronSchedulerOutputs(const xcronscheduler* S,const void* Input,size_t InputSize,void* const* Out,const size_t* Sizes,size_t Count)
{
    if(S && (!xcronSchedulerSpan(S,sizeof(*S)) || S->Magic!=XCRON_SCHEDULER_MAGIC || !xcronSchedulerSpan(S,S->Stats.Bytes)))return false;
    if(!xcronSchedulerSpan(Input,InputSize))return false;
    for(size_t I=0;I<Count;++I){
        if(!xcronSchedulerSpan(Out[I],Sizes[I]) || (S && xcronSchedulerOverlap(S,S->Stats.Bytes,Out[I],Sizes[I])) || xcronSchedulerOverlap(Input,InputSize,Out[I],Sizes[I]))return false;
        for(size_t J=0;J<I;++J)if(xcronSchedulerOverlap(Out[J],Sizes[J],Out[I],Sizes[I]))return false;
    }
    return true;
}
static bool xcronSchedulerArgs(const xcronscheduler* S,void* Output,size_t Size,xcronschedulerissue* Issue)
{void* O[]={Output,Issue};size_t N[]={Size,Issue?sizeof(*Issue):0};return S && xcronSchedulerOutputs(S,NULL,0,O,N,2);}
static void xcronSchedulerOK(xcronschedulerissue* Issue)
{if(Issue){memset(Issue,0,sizeof(*Issue));Issue->Zoned.Field=XCRON_FIELD_NONE;}}
static bool xcronSchedulerFail(xcronschedulererror Code,const xcronzonedissue* Zoned,xcronschedulerissue* Issue)
{
    xcronSchedulerOK(Issue);if(Issue){Issue->Code=Code;if(Zoned)Issue->Zoned=*Zoned;}
    cstr Text="invalid cron scheduler arguments/spans/output aliases";xerrkind Kind=XERR_ARGUMENT;
    switch(Code){
    case XCRON_SCHEDULER_LIMIT:Text="cron scheduler capacity or byte limit exceeded";Kind=XERR_RANGE;break;
    case XCRON_SCHEDULER_MEMORY:Text="cron scheduler allocation failed";Kind=XERR_MEMORY;break;
    case XCRON_SCHEDULER_CLOSED:Text="cron scheduler is closed";Kind=XERR_STATE;break;
    case XCRON_SCHEDULER_STALE:Text="cron job or run ID is stale";Kind=XERR_STATE;break;
    case XCRON_SCHEDULER_BUSY:Text="cron scheduler has unreaped jobs or live leases";Kind=XERR_STATE;break;
    case XCRON_SCHEDULER_CLOCK:Text="cron monotonic clock sample moved backwards";Kind=XERR_STATE;break;
    case XCRON_SCHEDULER_CALCULATION:Text="cron initial occurrence calculation failed";Kind=XERR_VALUE;break;
    default:break;
    }
    xrtSetErrorInfo(Kind,"xrt.cron.scheduler",Code,Text);return false;
}
static void xcronSchedulerIncrement(uint64* Value)
{if(*Value!=UINT64_MAX)++*Value;}
static bool xcronSchedulerArrayBytes(size_t Count,size_t Element,size_t* Total)
{if(Count>(SIZE_MAX-*Total)/Element)return false;*Total+=Count*Element;return true;}
static uint8 xcronSchedulerIndexBits(uint32 Limit)
{uint8 Bits=0;for(uint32 N=Limit-1;N;N>>=1)++Bits;return Bits;}
static uint64 xcronSchedulerId(uint64 Generation,uint32 Index,uint8 Bits)
{return Generation<<Bits | Index;}
static bool xcronSchedulerJobIndex(const xcronscheduler* S,xcronjobid Id,uint32* Index)
{
    uint32 I=(uint32)(Id&((UINT64_C(1)<<S->JobBits)-1));if(!Id || I>=S->Config.JobLimit)return false;
    const xcronjobslot* J=&S->Jobs[I];
    if(!J->Used || J->Generation!=(Id>>S->JobBits))return false;
    *Index=I;return true;
}
static bool xcronSchedulerRunIndex(const xcronscheduler* S,xcronrunid Id,uint32* Index)
{
    uint32 I=(uint32)(Id&((UINT64_C(1)<<S->RunBits)-1));if(!Id || I>=S->Config.RunLimit)return false;
    const xcronrunslot* R=&S->Runs[I];
    if(!R->Used || R->Generation!=(Id>>S->RunBits))return false;
    *Index=I;return true;
}
static bool xcronSchedulerEarlier(const xcronscheduler* S,uint32 A,uint32 B)
{
    const xcronjobslot* X=&S->Jobs[A];const xcronjobslot* Y=&S->Jobs[B];
    return X->Info.Next<Y->Info.Next || (X->Info.Next==Y->Info.Next && xcronSchedulerId(X->Generation,A,S->JobBits)<xcronSchedulerId(Y->Generation,B,S->JobBits));
}
static void xcronSchedulerHeapUp(xcronscheduler* S,uint32 P)
{
    uint32 I=S->Heap[P];while(P){uint32 Parent=(P-1)/2;if(!xcronSchedulerEarlier(S,I,S->Heap[Parent]))break;
        S->Heap[P]=S->Heap[Parent];S->Jobs[S->Heap[P]].Position=P;P=Parent;}
    S->Heap[P]=I;S->Jobs[I].Position=P;
}
static void xcronSchedulerHeapDown(xcronscheduler* S,uint32 P)
{
    uint32 I=S->Heap[P],Count=S->Stats.Heap;
    while(P<Count/2){uint32 C=P*2+1;if(C+1<Count && xcronSchedulerEarlier(S,S->Heap[C+1],S->Heap[C]))++C;
        if(!xcronSchedulerEarlier(S,S->Heap[C],I))break;
        S->Heap[P]=S->Heap[C];S->Jobs[S->Heap[P]].Position=P;P=C;}
    S->Heap[P]=I;S->Jobs[I].Position=P;
}
static void xcronSchedulerHeapAdd(xcronscheduler* S,uint32 I)
{S->Jobs[I].Location=XCRON_LOC_HEAP;uint32 P=S->Stats.Heap++;S->Heap[P]=I;xcronSchedulerHeapUp(S,P);}
static void xcronSchedulerHeapRemove(xcronscheduler* S,uint32 I)
{
    uint32 P=S->Jobs[I].Position,Last=--S->Stats.Heap;
    if(P!=Last){S->Heap[P]=S->Heap[Last];S->Jobs[S->Heap[P]].Position=P;
        if(P && xcronSchedulerEarlier(S,S->Heap[P],S->Heap[(P-1)/2]))xcronSchedulerHeapUp(S,P);else xcronSchedulerHeapDown(S,P);}
    S->Jobs[I].Location=XCRON_LOC_RUNNING;
}
static void xcronSchedulerQueueAdd(xcronscheduler* S,uint32 I,bool Retired)
{
    uint32* Head=Retired?&S->RetireHead:&S->ReadyHead;uint32* Tail=Retired?&S->RetireTail:&S->ReadyTail;
    xcronjobslot* J=&S->Jobs[I];J->Previous=*Tail;J->Next=XCRON_SLOT_NONE;
    if(*Tail!=XCRON_SLOT_NONE)S->Jobs[*Tail].Next=I;else *Head=I;
    *Tail=I;J->Location=Retired?XCRON_LOC_RETIRED:XCRON_LOC_READY;
    if(Retired)++S->Stats.Retired;else ++S->Stats.Ready;
}
static void xcronSchedulerQueueRemove(xcronscheduler* S,uint32 I,bool Retired)
{
    uint32* Head=Retired?&S->RetireHead:&S->ReadyHead;uint32* Tail=Retired?&S->RetireTail:&S->ReadyTail;
    xcronjobslot* J=&S->Jobs[I];
    if(J->Previous!=XCRON_SLOT_NONE)S->Jobs[J->Previous].Next=J->Next;else *Head=J->Next;
    if(J->Next!=XCRON_SLOT_NONE)S->Jobs[J->Next].Previous=J->Previous;else *Tail=J->Previous;
    if(Retired)--S->Stats.Retired;else --S->Stats.Ready;J->Location=XCRON_LOC_RUNNING;
}
static bool xcronSchedulerLive(xcronjobstate State)
{return State==XCRON_JOB_SCHEDULED || State==XCRON_JOB_SEARCHING;}
static void xcronSchedulerSearchAdd(xcronscheduler* S,uint32 I)
{
    xcronjobslot* J=&S->Jobs[I];J->Previous=S->SearchTail;J->Next=XCRON_SLOT_NONE;
    if(S->SearchTail!=XCRON_SLOT_NONE)S->Jobs[S->SearchTail].Next=I;else S->SearchHead=I;
    S->SearchTail=I;J->Location=XCRON_LOC_SEARCH;++S->Searching;
}
static void xcronSchedulerSearchRemove(xcronscheduler* S,uint32 I)
{
    xcronjobslot* J=&S->Jobs[I];
    if(J->Previous!=XCRON_SLOT_NONE)S->Jobs[J->Previous].Next=J->Next;else S->SearchHead=J->Next;
    if(J->Next!=XCRON_SLOT_NONE)S->Jobs[J->Next].Previous=J->Previous;else S->SearchTail=J->Previous;
    --S->Searching;J->Location=XCRON_LOC_RUNNING;
}
static void xcronSchedulerDetach(xcronscheduler* S,uint32 I)
{
    switch(S->Jobs[I].Location){
    case XCRON_LOC_HEAP:xcronSchedulerHeapRemove(S,I);break;
    case XCRON_LOC_READY:xcronSchedulerQueueRemove(S,I,false);break;
    case XCRON_LOC_SEARCH:xcronSchedulerSearchRemove(S,I);break;
    case XCRON_LOC_BLOCKED:--S->Stats.Blocked;S->Jobs[I].Location=XCRON_LOC_RUNNING;break;
    default:break;
    }
}
static void xcronSchedulerPlace(xcronscheduler* S,uint32 I)
{
    xcronjobslot* J=&S->Jobs[I];
    if(J->Info.State==XCRON_JOB_SEARCHING){xcronSchedulerSearchAdd(S,I);return;}
    if(J->Initial){J->Location=XCRON_LOC_RUNNING;return;}
    if(J->Info.State!=XCRON_JOB_SCHEDULED){if(!J->Info.Running)xcronSchedulerQueueAdd(S,I,true);return;}
    if(J->Config.Overlap!=XCRON_OVERLAP_SKIP && J->Info.Running>=J->Config.RunningLimit){J->Location=XCRON_LOC_BLOCKED;++S->Stats.Blocked;}
    else if(S->Stats.HasClock && J->Info.Next<=S->Stats.LastClock.Wall)xcronSchedulerQueueAdd(S,I,false);
    else xcronSchedulerHeapAdd(S,I);
}
static void xcronSchedulerSearch(xcronscheduler* S,uint32 I,xtime Begin,uint32 Flags,xcronpoll* Poll)
{
    xcronjobslot* J=&S->Jobs[I];xcronzonedissue ZI={.Field=XCRON_FIELD_NONE};xtime Next=0;
    xcronzonedstatus Result=Begin>J->Config.End?XCRON_ZONED_NONE:
        xcronZonedNextValue(&J->Config.Expression,J->Config.Zone,Begin,J->Config.End,Flags,&Next,&ZI);
    J->Info.LastIssue=ZI;J->Info.HasNext=Result==XCRON_ZONED_FOUND;
    if(Result==XCRON_ZONED_FOUND)J->Info.Next=Next;
    else if(Result==XCRON_ZONED_NONE)J->Info.State=XCRON_JOB_EXHAUSTED;
    else {J->Info.State=XCRON_JOB_FAULTED;xcronSchedulerIncrement(&S->Stats.Faults);if(Poll)++Poll->Faults;}
}
static void xcronSchedulerSearchStep(xcronscheduler* S,uint32 I,xcronzonedbudget* Remaining,xcronzonedbudget* Used,xcronpoll* Poll,bool Full)
{
    xcronjobslot* J=&S->Jobs[I];xcronzonedvalue* V=&S->Searches[I];xcronzonedcursorissue Before=V->Work;
    xcronzonedbudget B=Full?(xcronzonedbudget){UINT32_MAX,UINT32_MAX,UINT32_MAX}:
        (xcronzonedbudget){Remaining->Segments?1u:0u,Remaining->Months?1u:0u,Remaining->RuleYears?1u:0u};
    xcronzonedstatus Status=xcronZonedValueStep(V,J->Config.Zone,B);
    if(!Full){uint32 Segments=V->Work.SegmentsExamined-Before.SegmentsExamined;
        uint32 Months=(uint32)(V->Work.MonthsExamined-Before.MonthsExamined),Years=(uint32)(V->Work.RuleYearsExamined-Before.RuleYearsExamined);
        Remaining->Segments-=Segments;Remaining->Months-=Months;Remaining->RuleYears-=Years;
        Used->Segments+=Segments;Used->Months+=Months;Used->RuleYears+=Years;
    }
    J->Info.LastIssue=(xcronzonedissue){V->Work.Code,V->Work.Field,V->Work.Zone,V->Work.SegmentsExamined,V->Work.MonthsExamined};
    J->Info.HasNext=Status==XCRON_ZONED_FOUND;
    if(Status==XCRON_ZONED_PENDING)J->Info.State=XCRON_JOB_SEARCHING;
    else if(Status==XCRON_ZONED_FOUND){J->Info.State=XCRON_JOB_SCHEDULED;J->Info.Next=V->Result;}
    else if(Status==XCRON_ZONED_NONE)J->Info.State=XCRON_JOB_EXHAUSTED;
    else {J->Info.State=XCRON_JOB_FAULTED;if(!J->Initial){xcronSchedulerIncrement(&S->Stats.Faults);if(Poll)++Poll->Faults;}}
}
XCRON_SCHEDULER_INLINE void xcronSchedulerSeek(xcronscheduler* S,uint32 I,xtime Begin,uint32 Flags,xcronpoll* Poll,xcronzonedbudget* Remaining,xcronzonedbudget* Used)
{
    if(!Remaining){xcronSchedulerSearch(S,I,Begin,Flags,Poll);return;}
    xcronjobslot* J=&S->Jobs[I];
    if(Begin>J->Config.End){J->Info.HasNext=false;J->Info.LastIssue=(xcronzonedissue){.Field=XCRON_FIELD_NONE};J->Info.State=XCRON_JOB_EXHAUSTED;return;}
    /* Ready probes cannot spend the last segment/month credit ahead of an
     * already queued query. With Capacity=1 that otherwise repeats forever
     * across fresh polls, despite individually correct global counters. */
    if(Remaining->Segments && Remaining->Months && (!S->Searching || (Remaining->Segments>1 && Remaining->Months>1))){
        xtime First;xdatetime Local;int32 Offset;xcronzonedcursorissue Work;
        xcronzonedstatus Status=xcronZonedReadyValue(&J->Config.Expression,J->Config.Zone,Begin,J->Config.End,Flags,&First,&Local,&Offset,&Work);
        Remaining->Segments-=Work.SegmentsExamined;Remaining->Months-=(uint32)Work.MonthsExamined;
        Used->Segments+=Work.SegmentsExamined;Used->Months+=(uint32)Work.MonthsExamined;
        J->Info.LastIssue=(xcronzonedissue){Work.Code,Work.Field,Work.Zone,Work.SegmentsExamined,Work.MonthsExamined};
        J->Info.HasNext=Status==XCRON_ZONED_FOUND;
        if(Status==XCRON_ZONED_FOUND){J->Info.State=XCRON_JOB_SCHEDULED;J->Info.Next=First;return;}
        if(Status==XCRON_ZONED_NONE){J->Info.State=XCRON_JOB_EXHAUSTED;return;}
        if(Status==XCRON_ZONED_ERROR){J->Info.State=XCRON_JOB_FAULTED;xcronSchedulerIncrement(&S->Stats.Faults);if(Poll)++Poll->Faults;return;}
        xcronzonedvalue* V=&S->Searches[I];xcronZonedValueRestart(&J->Config.Expression,J->Config.Zone,Begin,J->Config.End,Flags,V);
        V->Work=Work;V->Calendar.First=Local;V->Offset=Offset;xcronZoneWalkInit(J->Config.Zone,First,&V->Transition);V->Phase=1;
    }else {J->Info.HasNext=false;J->Info.LastIssue=(xcronzonedissue){.Field=XCRON_FIELD_NONE};
        xcronZonedValueRestart(&J->Config.Expression,J->Config.Zone,Begin,J->Config.End,Flags,&S->Searches[I]);}
    J->Info.State=S->Searches[I].Status==XCRON_ZONED_NONE?XCRON_JOB_EXHAUSTED:XCRON_JOB_SEARCHING;
}
static void xcronSchedulerSkip(xcronscheduler* S,uint32 I,xtime Begin,uint32 Flags,xcronpoll* Poll,xcronzonedbudget* Remaining,xcronzonedbudget* Used)
{
    xcronSchedulerIncrement(&S->Jobs[I].Info.SkippedRanges);xcronSchedulerIncrement(&S->Stats.SkippedRanges);++Poll->SkippedRanges;
    xcronSchedulerSeek(S,I,Begin,Flags,Poll,Remaining,Used);
}
static bool xcronSchedulerActionable(const xcronscheduler* S,xcronclock Now)
{
    if(S->Searching || (S->Stats.Heap && S->Jobs[S->Heap[0]].Info.Next<=Now.Wall))return true;
    return S->Stats.Ready && S->FreeRun!=XCRON_SLOT_NONE;
}
static bool xcronSchedulerClock(const xcronscheduler* S,xcronclock Now)
{return !S->Stats.HasClock || Now.MonotonicMilliseconds>=S->Stats.LastClock.MonotonicMilliseconds;}
static xtime xcronSchedulerCutoff(xtime Wall,uint64 Grace)
{
    uint64 Distance=(uint64)Wall-(uint64)INT64_MIN;
    if(Grace>=Distance)return INT64_MIN;
    return Grace<=INT64_MAX?Wall-(int64)Grace:INT64_MIN+(int64)(Distance-Grace);
}
XCRON_API bool xcronSchedulerCreate(const xcronschedulerconfig* Config,xcronscheduler** Out,xcronschedulerissue* Issue)
{
    void* O[]={Out,Issue};size_t N[]={sizeof(*Out),Issue?sizeof(*Issue):0};
    if(!Config || !Out || !xcronSchedulerOutputs(NULL,Config,sizeof(*Config),O,N,2) || !Config->JobLimit || !Config->RunLimit || !Config->WorkLimit || Config->Reserved || Config->JobLimit==UINT32_MAX || Config->RunLimit==UINT32_MAX)
        return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    size_t Bytes=sizeof(xcronscheduler);
    if(!xcronSchedulerArrayBytes(Config->JobLimit,sizeof(xcronjobslot),&Bytes) ||
        !xcronSchedulerArrayBytes(Config->RunLimit,sizeof(xcronrunslot),&Bytes) ||
        !xcronSchedulerArrayBytes(Config->JobLimit,sizeof(uint32),&Bytes))return xcronSchedulerFail(XCRON_SCHEDULER_LIMIT,NULL,Issue);
    size_t Padding=(_Alignof(xcronzonedvalue)-Bytes%_Alignof(xcronzonedvalue))%_Alignof(xcronzonedvalue);
    if(Padding>SIZE_MAX-Bytes)return xcronSchedulerFail(XCRON_SCHEDULER_LIMIT,NULL,Issue);
    Bytes+=Padding;size_t SearchAt=Bytes;
    if(!xcronSchedulerArrayBytes(Config->JobLimit,sizeof(xcronzonedvalue),&Bytes) || Bytes>Config->BytesLimit)return xcronSchedulerFail(XCRON_SCHEDULER_LIMIT,NULL,Issue);
    size_t JBytes=(size_t)Config->JobLimit*sizeof(xcronjobslot),RBytes=(size_t)Config->RunLimit*sizeof(xcronrunslot);
    xcronscheduler* S=xrtMalloc(Bytes);if(!S)return xcronSchedulerFail(XCRON_SCHEDULER_MEMORY,NULL,Issue);
    memset(S,0,Bytes);S->Magic=XCRON_SCHEDULER_MAGIC;S->Config=*Config;S->Stats.Bytes=Bytes;
    S->Jobs=(xcronjobslot*)S->Storage;S->Runs=(xcronrunslot*)(S->Storage+JBytes);S->Heap=(uint32*)(S->Storage+JBytes+RBytes);
    S->Searches=(xcronzonedvalue*)((unsigned char*)S+SearchAt);S->SearchHead=S->SearchTail=XCRON_SLOT_NONE;
    S->FreeJob=0;S->FreeRun=0;S->ReadyHead=S->ReadyTail=S->RetireHead=S->RetireTail=XCRON_SLOT_NONE;S->Promote=true;S->JobBits=xcronSchedulerIndexBits(Config->JobLimit);S->RunBits=xcronSchedulerIndexBits(Config->RunLimit);
    for(uint32 I=0;I<Config->JobLimit;++I){S->Jobs[I].Generation=1;S->Jobs[I].Next=I+1<Config->JobLimit?I+1:XCRON_SLOT_NONE;}
    for(uint32 I=0;I<Config->RunLimit;++I){S->Runs[I].Generation=1;S->Runs[I].Next=I+1<Config->RunLimit?I+1:XCRON_SLOT_NONE;}
    *Out=S;xcronSchedulerOK(Issue);return true;
}
static bool xcronSchedulerAdmissionArgs(xcronscheduler* S,const xcronjobconfig* Config,xcronjobid* Id,xcronschedulerissue* Issue)
{
    void* O[]={Id,Issue};size_t N[]={sizeof(*Id),Issue?sizeof(*Issue):0};
    if(!S || !Config || !Id || !xcronSchedulerOutputs(S,Config,sizeof(*Config),O,N,2) ||
        !xcronZoneOutputs(Config->Zone,Config,sizeof(*Config),O,N,2) ||
        (Config->SearchFlags&~XCRON_SEARCH_EXCLUSIVE) || Config->Begin>Config->End ||
        Config->Misfire<XCRON_MISFIRE_CATCH_UP || Config->Misfire>XCRON_MISFIRE_COALESCE || Config->Overlap<XCRON_OVERLAP_WAIT || Config->Overlap>XCRON_OVERLAP_SKIP ||
        !Config->RunningLimit || Config->RunningLimit>S->Config.RunLimit || (Config->Overlap!=XCRON_OVERLAP_ALLOW && Config->RunningLimit!=1))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    if(S->Stats.Closed)return xcronSchedulerFail(XCRON_SCHEDULER_CLOSED,NULL,Issue);
    if(S->FreeJob==XCRON_SLOT_NONE)return xcronSchedulerFail(XCRON_SCHEDULER_LIMIT,NULL,Issue);
    xcronfield Field;xcronzonedissue ZI={.Field=XCRON_FIELD_NONE};
    if(!xcronCalendarValidateExpression(&Config->Expression,&Field)){ZI.Code=XCRON_ZONED_EXPRESSION;ZI.Field=Field;return xcronSchedulerFail(XCRON_SCHEDULER_CALCULATION,&ZI,Issue);}
    return true;
}
static uint32 xcronSchedulerAllocateJob(xcronscheduler* S,const xcronjobconfig* Config,bool Initial)
{
    uint32 I=S->FreeJob;xcronjobslot* J=&S->Jobs[I];S->FreeJob=J->Next;J->Used=true;J->Initial=Initial;J->Location=XCRON_LOC_RUNNING;J->Config=*Config;memset(&J->Info,0,sizeof(J->Info));
    ++S->Stats.Jobs;if(S->Stats.Jobs>S->Stats.PeakJobs)S->Stats.PeakJobs=S->Stats.Jobs;return I;
}
static void xcronSchedulerFreeJob(xcronscheduler* S,uint32 I)
{
    xcronjobslot* J=&S->Jobs[I];xcronZoneRelease(J->Config.Zone);J->Config.Zone=NULL;J->Config.Data=NULL;J->Used=false;J->Initial=false;J->Location=XCRON_LOC_FREE;--S->Stats.Jobs;
    if(J->Generation!=(UINT64_MAX>>S->JobBits)){++J->Generation;J->Next=S->FreeJob;S->FreeJob=I;}
}
XCRON_API bool xcronSchedulerAdd(xcronscheduler* S,const xcronjobconfig* Config,xcronjobid* Id,xcronschedulerissue* Issue)
{
    if(!xcronSchedulerAdmissionArgs(S,Config,Id,Issue))return false;
    xcronzonedissue ZI={.Field=XCRON_FIELD_NONE};
    xtime Next=0;xcronzonedstatus Result=xcronZonedNextValue(&Config->Expression,Config->Zone,Config->Begin,Config->End,Config->SearchFlags,&Next,&ZI);
    if(Result==XCRON_ZONED_ERROR)return xcronSchedulerFail(XCRON_SCHEDULER_CALCULATION,&ZI,Issue);
    if(!xcronZoneRetain(Config->Zone))return xcronSchedulerFail(XCRON_SCHEDULER_LIMIT,NULL,Issue);
    uint32 I=xcronSchedulerAllocateJob(S,Config,false);xcronjobslot* J=&S->Jobs[I];
    J->Info.State=Result==XCRON_ZONED_FOUND?XCRON_JOB_SCHEDULED:XCRON_JOB_EXHAUSTED;J->Info.HasNext=Result==XCRON_ZONED_FOUND;J->Info.Next=Next;J->Info.LastIssue=ZI;
    xcronSchedulerPlace(S,I);*Id=xcronSchedulerId(J->Generation,I,S->JobBits);xcronSchedulerOK(Issue);return true;
}
bool xcronSchedulerBeginAdmission(xcronscheduler* S,const xcronjobconfig* Config,xcronjobid* Id,xcronschedulerissue* Issue)
{
    if(!xcronSchedulerAdmissionArgs(S,Config,Id,Issue))return false;
    if(!xcronZoneRetain(Config->Zone))return xcronSchedulerFail(XCRON_SCHEDULER_LIMIT,NULL,Issue);
    uint32 I=xcronSchedulerAllocateJob(S,Config,true);xcronjobslot* J=&S->Jobs[I];
    xcronZonedValueInit(&J->Config.Expression,J->Config.Zone,J->Config.Begin,J->Config.End,J->Config.SearchFlags,&S->Searches[I]);
    J->Info.LastIssue=(xcronzonedissue){.Field=XCRON_FIELD_NONE};J->Info.State=S->Searches[I].Status==XCRON_ZONED_PENDING?XCRON_JOB_SEARCHING:XCRON_JOB_EXHAUSTED;
    xcronSchedulerPlace(S,I);*Id=xcronSchedulerId(J->Generation,I,S->JobBits);xcronSchedulerOK(Issue);return true;
}
xcronadmissionstatus xcronSchedulerFinishAdmission(xcronscheduler* S,xcronjobid Id,bool Cancel,xcronschedulerissue* Issue)
{
    uint32 I;if(!xcronSchedulerJobIndex(S,Id,&I) || !S->Jobs[I].Initial){xcronSchedulerFail(XCRON_SCHEDULER_STALE,NULL,Issue);return XCRON_ADMISSION_REJECTED;}
    xcronjobslot* J=&S->Jobs[I];
    if(Cancel || S->Stats.Closed){xcronSchedulerDetach(S,I);xcronSchedulerFreeJob(S,I);xcronSchedulerFail(XCRON_SCHEDULER_CLOSED,NULL,Issue);return XCRON_ADMISSION_REJECTED;}
    if(J->Info.State==XCRON_JOB_SEARCHING)return XCRON_ADMISSION_PENDING;
    if(J->Info.State==XCRON_JOB_FAULTED){xcronzonedissue ZI=J->Info.LastIssue;xcronSchedulerFreeJob(S,I);xcronSchedulerFail(XCRON_SCHEDULER_CALCULATION,&ZI,Issue);return XCRON_ADMISSION_REJECTED;}
    J->Initial=false;xcronSchedulerPlace(S,I);xcronSchedulerOK(Issue);return XCRON_ADMISSION_ACCEPTED;
}
XCRON_SCHEDULER_INLINE void xcronSchedulerReady(xcronscheduler* S,xcronclock Now,xcronrun* Runs,xcronpoll* P,bool* GlobalBlocked,xcronzonedbudget* Remaining,xcronzonedbudget* Used)
{
    uint32 I=S->ReadyHead;xcronjobslot* J=&S->Jobs[I];
    if(J->Info.Next>Now.Wall){xcronSchedulerQueueRemove(S,I,false);xcronSchedulerHeapAdd(S,I);return;}
    if(J->Config.Overlap==XCRON_OVERLAP_SKIP && J->Info.Running){
        xcronSchedulerQueueRemove(S,I,false);xcronSchedulerSkip(S,I,Now.Wall,XCRON_SEARCH_EXCLUSIVE,P,Remaining,Used);xcronSchedulerPlace(S,I);return;
    }
    if(J->Info.Running>=J->Config.RunningLimit){xcronSchedulerQueueRemove(S,I,false);J->Location=XCRON_LOC_BLOCKED;++S->Stats.Blocked;return;}
    if(S->FreeRun==XCRON_SLOT_NONE){*GlobalBlocked=true;return;}
    xcronSchedulerQueueRemove(S,I,false);xtime Cutoff=xcronSchedulerCutoff(Now.Wall,J->Config.GraceMilliseconds);
    if(J->Config.Misfire==XCRON_MISFIRE_SKIP && J->Info.Next<Cutoff){
        xcronSchedulerSkip(S,I,Cutoff,0,P,Remaining,Used);xcronSchedulerPlace(S,I);return;
    }
    bool Coalesced=J->Config.Misfire==XCRON_MISFIRE_COALESCE && J->Info.Next<Cutoff;
    xtime Through=Coalesced?(Now.Wall<J->Config.End?Now.Wall:J->Config.End):J->Info.Next;
    uint32 RIndex=S->FreeRun;xcronrunslot* R=&S->Runs[RIndex];S->FreeRun=R->Next;R->Used=true;R->Job=I;
    Runs[P->Count++]=(xcronrun){xcronSchedulerId(R->Generation,RIndex,S->RunBits),xcronSchedulerId(J->Generation,I,S->JobBits),J->Config.Data,J->Info.Next,Through,Now,Coalesced?XCRON_RUN_COALESCED:0};
    ++J->Info.Running;++S->Stats.Running;if(S->Stats.Running>S->Stats.PeakRunning)S->Stats.PeakRunning=S->Stats.Running;
    xcronSchedulerIncrement(&J->Info.Dispatched);xcronSchedulerIncrement(&S->Stats.Dispatched);
    if(Coalesced){xcronSchedulerIncrement(&J->Info.CoalescedRanges);xcronSchedulerIncrement(&S->Stats.CoalescedRanges);}
    xcronSchedulerSeek(S,I,Through,XCRON_SEARCH_EXCLUSIVE,P,Remaining,Used);xcronSchedulerPlace(S,I);
}
XCRON_API bool xcronSchedulerPoll(xcronscheduler* S,xcronclock Now,xcronrun* Runs,size_t Capacity,xcronpoll* Out,xcronschedulerissue* Issue)
{
    void* O[]={Runs,Out,Issue};size_t N[]={Capacity<=SIZE_MAX/sizeof(*Runs)?Capacity*sizeof(*Runs):SIZE_MAX,sizeof(*Out),Issue?sizeof(*Issue):0};
    if(!S || !Out || !Runs || !Capacity || Capacity>SIZE_MAX/sizeof(*Runs) || !xcronSchedulerOutputs(S,NULL,0,O,N,3))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    if(!xcronSchedulerClock(S,Now))return xcronSchedulerFail(XCRON_SCHEDULER_CLOCK,NULL,Issue);
    if(S->Stats.HasClock && Now.Wall<S->Stats.LastClock.Wall)xcronSchedulerIncrement(&S->Stats.WallRegressions);
    S->Stats.LastClock=Now;S->Stats.HasClock=true;xcronSchedulerIncrement(&S->Stats.ClockSamples);xcronpoll P={0};bool GlobalBlocked=false;
    while(S->Searching && P.Work<S->Config.WorkLimit){uint32 I=S->SearchHead;xcronSchedulerSearchRemove(S,I);++P.Work;xcronSchedulerSearchStep(S,I,NULL,NULL,&P,true);xcronSchedulerPlace(S,I);}
    while(P.Work<S->Config.WorkLimit && P.Count<Capacity){
        bool Due=S->Stats.Heap && S->Jobs[S->Heap[0]].Info.Next<=Now.Wall;
        bool Ready=S->Stats.Ready && !GlobalBlocked;
        if(!Due && !Ready)break;
        ++P.Work;
        if(Due && (!Ready || S->Promote)){
            uint32 I=S->Heap[0];xcronSchedulerHeapRemove(S,I);xcronSchedulerQueueAdd(S,I,false);S->Promote=false;continue;
        }
        S->Promote=true;xcronSchedulerReady(S,Now,Runs,&P,&GlobalBlocked,NULL,NULL);
    }
    P.More=xcronSchedulerActionable(S,Now);*Out=P;xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerPollBudgeted(xcronscheduler* S,xcronclock Now,xcronzonedbudget Budget,xcronrun* Runs,size_t Capacity,xcronpollbudgeted* Out,xcronschedulerissue* Issue)
{
    void* O[]={Runs,Out,Issue};size_t N[]={Capacity<=SIZE_MAX/sizeof(*Runs)?Capacity*sizeof(*Runs):SIZE_MAX,sizeof(*Out),Issue?sizeof(*Issue):0};
    if(!S || !Out || (!Runs && Capacity) || Capacity>SIZE_MAX/sizeof(*Runs) || !Budget.Segments || !Budget.Months || !Budget.RuleYears || !xcronSchedulerOutputs(S,NULL,0,O,N,3))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    if(!xcronSchedulerClock(S,Now))return xcronSchedulerFail(XCRON_SCHEDULER_CLOCK,NULL,Issue);
    if(S->Stats.HasClock && Now.Wall<S->Stats.LastClock.Wall)xcronSchedulerIncrement(&S->Stats.WallRegressions);
    S->Stats.LastClock=Now;S->Stats.HasClock=true;xcronSchedulerIncrement(&S->Stats.ClockSamples);
    xcronpollbudgeted Q={0};xcronzonedbudget Remaining=Budget;bool GlobalBlocked=false;uint32 Stalled=0;
    while(Q.Poll.Work<S->Config.WorkLimit){
        /* After filling output, only pending calculations need more actions.
         * Avoid a full class scan (and gratuitous heap promotions) otherwise. */
        if(Q.Poll.Count>=Capacity && !S->Searching)break;
        bool Due=S->Stats.Heap && S->Jobs[S->Heap[0]].Info.Next<=Now.Wall;
        bool Ready=S->Stats.Ready && !GlobalBlocked && Q.Poll.Count<Capacity;
        bool Search=S->Searching && Stalled<S->Searching && (Remaining.Segments || Remaining.Months || Remaining.RuleYears);
        /* The three cyclic orders are 0/1/2,1/2/0,2/0/1. Select directly
         * without a per-action temporary array or modulo/index loop. */
        uint32 Class;
        if(S->Turn==0 && Due)Class=0;
        else if(S->Turn<=1 && Ready)Class=1;
        else if(Search)Class=2;
        else if(Due)Class=0;
        else if(Ready)Class=1;
        else break;
        S->Turn=Class==2?0:(uint8)(Class+1);++Q.Poll.Work;
        if(Class==0){uint32 I=S->Heap[0];xcronSchedulerHeapRemove(S,I);xcronSchedulerQueueAdd(S,I,false);}
        else if(Class==1){uint32 Before=S->Searching;xcronSchedulerReady(S,Now,Runs,&Q.Poll,&GlobalBlocked,&Remaining,&Q.Used);if(S->Searching>Before)Stalled=0;}
        else {uint32 I=S->SearchHead;xcronzonedbudget Before=Q.Used;xcronSchedulerSearchRemove(S,I);xcronSchedulerSearchStep(S,I,&Remaining,&Q.Used,&Q.Poll,false);xcronSchedulerPlace(S,I);
            if(S->Jobs[I].Info.State==XCRON_JOB_SEARCHING && Before.Segments==Q.Used.Segments && Before.Months==Q.Used.Months && Before.RuleYears==Q.Used.RuleYears)++Stalled;else Stalled=0;}
    }
    Q.Searching=S->Searching;Q.Poll.More=xcronSchedulerActionable(S,Now);*Out=Q;xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerComplete(xcronscheduler* S,xcronrunid Id,xcronrunresult Result,xcronschedulerissue* Issue)
{
    if(!xcronSchedulerArgs(S,NULL,0,Issue) || Result<XCRON_RUN_SUCCEEDED || Result>XCRON_RUN_CANCELLED)return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    uint32 RIndex;if(!xcronSchedulerRunIndex(S,Id,&RIndex))return xcronSchedulerFail(XCRON_SCHEDULER_STALE,NULL,Issue);
    xcronrunslot* R=&S->Runs[RIndex];uint32 I=R->Job;xcronjobslot* J=&S->Jobs[I];
    --J->Info.Running;--S->Stats.Running;R->Used=false;
    if(R->Generation!=(UINT64_MAX>>S->RunBits)){++R->Generation;R->Next=S->FreeRun;S->FreeRun=RIndex;}
    xcronSchedulerIncrement(&S->Stats.Completed);
    xcronSchedulerIncrement(Result==XCRON_RUN_SUCCEEDED?&J->Info.Succeeded:Result==XCRON_RUN_FAILED?&J->Info.Failed:&J->Info.Cancelled);
    if(!xcronSchedulerLive(J->Info.State)){if(!J->Info.Running)xcronSchedulerQueueAdd(S,I,true);}
    else if(J->Location==XCRON_LOC_BLOCKED && J->Info.Running<J->Config.RunningLimit){--S->Stats.Blocked;xcronSchedulerPlace(S,I);}
    xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerCancel(xcronscheduler* S,xcronjobid Id,xcronschedulerissue* Issue)
{
    if(!xcronSchedulerArgs(S,NULL,0,Issue))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    uint32 I;if(!xcronSchedulerJobIndex(S,Id,&I) || S->Jobs[I].Initial)return xcronSchedulerFail(XCRON_SCHEDULER_STALE,NULL,Issue);
    xcronjobslot* J=&S->Jobs[I];if(xcronSchedulerLive(J->Info.State)){xcronSchedulerDetach(S,I);J->Info.State=XCRON_JOB_CANCELLED;J->Info.HasNext=false;xcronSchedulerPlace(S,I);}
    xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerJob(const xcronscheduler* S,xcronjobid Id,xcronjobinfo* Info,xcronschedulerissue* Issue)
{
    if(!Info || !xcronSchedulerArgs(S,Info,sizeof(*Info),Issue))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    uint32 I;if(!xcronSchedulerJobIndex(S,Id,&I) || S->Jobs[I].Initial)return xcronSchedulerFail(XCRON_SCHEDULER_STALE,NULL,Issue);
    *Info=S->Jobs[I].Info;xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerReap(xcronscheduler* S,xcronretired* Retired,bool* Found,xcronschedulerissue* Issue)
{
    void* O[]={Retired,Found,Issue};size_t N[]={sizeof(*Retired),sizeof(*Found),Issue?sizeof(*Issue):0};
    if(!S || !Retired || !Found || !xcronSchedulerOutputs(S,NULL,0,O,N,3))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    uint32 I=S->RetireHead;
    if(I==XCRON_SLOT_NONE){*Found=false;xcronSchedulerOK(Issue);return true;}
    xcronjobslot* J=&S->Jobs[I];xcronretired Value={xcronSchedulerId(J->Generation,I,S->JobBits),J->Config.Data,J->Info};
    xcronSchedulerQueueRemove(S,I,true);xcronSchedulerFreeJob(S,I);
    *Retired=Value;*Found=true;xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerClose(xcronscheduler* S,xcronschedulerissue* Issue)
{
    if(!xcronSchedulerArgs(S,NULL,0,Issue))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    if(!S->Stats.Closed){S->Stats.Closed=true;
        for(uint32 I=0;I<S->Config.JobLimit;++I){xcronjobslot* J=&S->Jobs[I];if(J->Used && xcronSchedulerLive(J->Info.State)){xcronSchedulerDetach(S,I);J->Info.State=XCRON_JOB_CANCELLED;J->Info.HasNext=false;xcronSchedulerPlace(S,I);}}}
    xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerWake(const xcronscheduler* S,xcronclock Now,uint64 MaxWait,xcronwake* Wake,xcronschedulerissue* Issue)
{
    if(!Wake || !MaxWait || !xcronSchedulerArgs(S,Wake,sizeof(*Wake),Issue))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    if(!xcronSchedulerClock(S,Now))return xcronSchedulerFail(XCRON_SCHEDULER_CLOCK,NULL,Issue);
    uint64 Delay=MaxWait;bool Free=S->FreeRun!=XCRON_SLOT_NONE;
    bool Immediate=S->Searching || (S->Stats.Ready && Free) || (S->Stats.Heap && S->Jobs[S->Heap[0]].Info.Next<=Now.Wall);
    /* A full lease table needs an external completion or bounded resampling,
     * not repeated immediate wakes for a ready job it cannot dispatch. */
    if(!Free && !S->Stats.Heap && !S->Searching)Immediate=false;
    if(Immediate)Delay=0;
    else if(S->Stats.Heap){xtime Due=S->Jobs[S->Heap[0]].Info.Next;
        if(Due>Now.Wall){uint64 Distance=(uint64)Due-(uint64)Now.Wall;if(Distance<Delay)Delay=Distance;}}
    xcronwake W={Delay>UINT64_MAX-Now.MonotonicMilliseconds?UINT64_MAX:Now.MonotonicMilliseconds+Delay,Delay,Immediate,S->Stats.Running!=0};
    *Wake=W;xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerInspect(const xcronscheduler* S,xcronschedulerstats* Stats,xcronschedulerissue* Issue)
{
    if(!Stats || !xcronSchedulerArgs(S,Stats,sizeof(*Stats),Issue))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    *Stats=S->Stats;xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerDestroy(xcronscheduler* S,xcronschedulerissue* Issue)
{
    if(!xcronSchedulerArgs(S,NULL,0,Issue))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    if(S->Stats.Jobs || S->Stats.Running)return xcronSchedulerFail(XCRON_SCHEDULER_BUSY,NULL,Issue);
    S->Magic=0;xrtFree(S);xcronSchedulerOK(Issue);return true;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xcron/src/executor/executor.c */
/* ========================================================================== */

#if defined(XCRON_FEATURE_XCRON_EXECUTOR)
#include <string.h>
#if defined(XCRON_FEATURE_XCRON_EXECUTOR)
#define XCRON_EXECUTOR_MAGIC UINT32_C(0x58434531)
#define XCRON_EXECUTOR_NONE UINT32_MAX
struct xcronexecution {xatomic32 Requested;};
typedef struct xcronexecutorslot {
    xcroncompletion Done;
    xcronexecutor* Owner;
    xcronexecution Execution;
    uint32 Index,Next;
    bool Used;
} xcronexecutorslot;
struct xcronexecutor {
    uint32 Magic,NotifyDepth;
    uint64 Owner;
    xcronexecutorconfig Config;
    xcronexecutorstats Stats;
    xatomic32 Head,Producers;
    xcronexecutorslot* Slots;
    uint32* Table;
    uint32 Free,Pending,Buckets,Mask;
    _Alignas(xcronexecutorslot) unsigned char Storage[];
};
static bool xcronExecutorSpan(const void* P,size_t N)
{return (!N || P) && N<=UINTPTR_MAX-(uintptr_t)P;}
static bool xcronExecutorOverlap(const void* A,size_t AN,const void* B,size_t BN)
{return AN && BN && (uintptr_t)A<(uintptr_t)B+BN && (uintptr_t)B<(uintptr_t)A+AN;}
static bool xcronExecutorOutputs(const xcronexecutor* B,const void* Input,size_t InputSize,void* const* O,const size_t* N,size_t Count)
{
    if(B && (!xcronExecutorSpan(B,sizeof(*B)) || B->Magic!=XCRON_EXECUTOR_MAGIC || !xcronExecutorSpan(B,B->Stats.Bytes)))return false;
    if(!xcronExecutorSpan(Input,InputSize))return false;
    for(size_t I=0;I<Count;++I){
        if(!xcronExecutorSpan(O[I],N[I]) || (B && xcronExecutorOverlap(B,B->Stats.Bytes,O[I],N[I])) || xcronExecutorOverlap(Input,InputSize,O[I],N[I]))return false;
        for(size_t J=0;J<I;++J)if(xcronExecutorOverlap(O[J],N[J],O[I],N[I]))return false;
    }
    return true;
}
static bool xcronExecutorArgs(const xcronexecutor* B,void* Out,size_t Size,xcronexecutorissue* Issue)
{void* O[]={Out,Issue};size_t N[]={Size,Issue?sizeof(*Issue):0};return B && xcronExecutorOutputs(B,NULL,0,O,N,2);}
static void xcronExecutorOK(xcronexecutorissue* Issue)
{if(Issue)*Issue=(xcronexecutorissue){XCRON_EXECUTOR_OK,XERR_NONE};}
static bool xcronExecutorFail(xcronexecutorerror Code,xerrkind Cause,xcronexecutorissue* Issue)
{
    if(Issue)*Issue=(xcronexecutorissue){Code,Cause};
    cstr Text="invalid cron executor arguments/spans/output aliases";xerrkind Kind=XERR_ARGUMENT;
    switch(Code){
    case XCRON_EXECUTOR_LIMIT:Text="cron executor slot or byte capacity exceeded";Kind=XERR_RANGE;break;
    case XCRON_EXECUTOR_MEMORY:Text="cron executor allocation failed";Kind=XERR_MEMORY;break;
    case XCRON_EXECUTOR_CLOSED:Text="cron executor adapter is closed";Kind=XERR_CLOSED;break;
    case XCRON_EXECUTOR_STALE:Text="cron execution lease is not active";Kind=XERR_NOT_FOUND;break;
    case XCRON_EXECUTOR_BUSY:Text="cron execution receipts or publication tails remain";Kind=XERR_STATE;break;
    case XCRON_EXECUTOR_THREAD:Text="cron executor operation requires its owner thread";Kind=XERR_STATE;break;
    case XCRON_EXECUTOR_REENTRY:Text="cron executor notification cannot reenter its owner";Kind=XERR_STATE;break;
    case XCRON_EXECUTOR_DUPLICATE:Text="cron execution lease is already active";Kind=XERR_EXISTS;break;
    case XCRON_EXECUTOR_REJECTED:Text="borrowed XRT executor rejected the lease";Kind=Cause==XERR_NONE?XERR_AGAIN:Cause;break;
    default:break;
    }
    if(Cause!=XERR_NONE){xerror* Original=xrtTakeError();xerror* Wrapped=xrtErrorWrap(Original,Kind,"xrt.cron.executor",Code,Text);xrtErrorFree(Original);
        if(Wrapped)xrtSetErrorTake(Wrapped);else xrtSetErrorKind(XERR_MEMORY);}
    else xrtSetErrorInfo(Kind,"xrt.cron.executor",Code,Text);
    return false;
}
static bool xcronExecutorOwner(const xcronexecutor* B,xcronexecutorissue* Issue)
{
    if(B->Owner!=xrtThreadCurrentId())return xcronExecutorFail(XCRON_EXECUTOR_THREAD,XERR_NONE,Issue);
    if(B->NotifyDepth)return xcronExecutorFail(XCRON_EXECUTOR_REENTRY,XERR_NONE,Issue);
    return true;
}
static void xcronExecutorIncrement(uint64* N)
{if(*N!=UINT64_MAX)++*N;}
static uint32 xcronExecutorHash(xcronrunid Id,uint32 Mask)
{Id^=Id>>33;Id*=UINT64_C(0xff51afd7ed558ccd);Id^=Id>>33;Id*=UINT64_C(0xc4ceb9fe1a85ec53);Id^=Id>>33;return (uint32)Id&Mask;}
static uint32 xcronExecutorFind(const xcronexecutor* B,xcronrunid Id)
{
    uint32 P=xcronExecutorHash(Id,B->Mask);
    while(B->Table[P]!=XCRON_EXECUTOR_NONE){if(B->Slots[B->Table[P]].Done.Run.Id==Id)return P;P=(P+1)&B->Mask;}
    return P;
}
static void xcronExecutorRemove(xcronexecutor* B,uint32 P)
{
    uint32 Next=(P+1)&B->Mask;
    while(B->Table[Next]!=XCRON_EXECUTOR_NONE){
        uint32 Home=xcronExecutorHash(B->Slots[B->Table[Next]].Done.Run.Id,B->Mask);
        if(((Next-Home)&B->Mask)>=((Next-P)&B->Mask)){B->Table[P]=B->Table[Next];P=Next;}
        Next=(Next+1)&B->Mask;
    }
    B->Table[P]=XCRON_EXECUTOR_NONE;
}
static void xcronExecutorWork(ptr Data)
{
    xcronexecutorslot* S=Data;xcronexecutor* B=S->Owner;
    if(xrtThreadCurrentId()==B->Owner){S->Done.Result=XCRON_RUN_FAILED;S->Done.Code=XCRON_EXECUTOR_OWNER_CONFLICT;return;}
    if(xcronExecutionCancelled(&S->Execution))return;
    xcronrunresult Result=B->Config.Proc(&S->Done.Run,&S->Execution,B->Config.Context);
    if(Result<XCRON_RUN_SUCCEEDED || Result>XCRON_RUN_CANCELLED){Result=XCRON_RUN_FAILED;S->Done.Code=XCRON_EXECUTOR_RESULT;}
    S->Done.Result=Result;
    if(Result==XCRON_RUN_FAILED)S->Done.Error=xrtTakeError();
}
static void xcronExecutorDrop(ptr Data,ptr Context)
{
    (void)Context;xcronexecutorslot* S=Data;xcronexecutor* B=S->Owner;uint32 Index=S->Index;
    uint32 Previous=xrtAtomic32Load(&B->Head,XMEMORY_RELAXED);
    do{S->Next=Previous;}while(!xrtAtomic32CompareExchange(&B->Head,&Previous,Index,XMEMORY_RELEASE,XMEMORY_RELAXED));
    /* No slot/data/error access after publication: owner may drain/reuse it. */
    if(B->Config.Notify){bool Owner=xrtThreadCurrentId()==B->Owner;if(Owner)++B->NotifyDepth;
        B->Config.Notify(B->Config.NotifyContext);if(Owner)--B->NotifyDepth;}
    /* Last adapter access: Destroy also waits for these notification tails. */
    xrtAtomic32FetchSub(&B->Producers,1,XMEMORY_RELEASE);
}
XCRON_API bool xcronExecutorCreate(const xcronexecutorconfig* Config,xcronexecutor** Out,xcronexecutorissue* Issue)
{
    void* O[]={Out,Issue};size_t N[]={sizeof(*Out),Issue?sizeof(*Issue):0};
    if(!Config || !Out || !xcronExecutorOutputs(NULL,Config,sizeof(*Config),O,N,2) || !Config->Executor || !Config->Proc || !Config->RunLimit || Config->RunLimit>UINT32_C(1073741824) || Config->Reserved)
        return xcronExecutorFail(XCRON_EXECUTOR_ARGUMENT,XERR_NONE,NULL);
    uint32 Buckets=2;while(Buckets/2<Config->RunLimit)Buckets*=2;
    size_t Bytes=sizeof(xcronexecutor),Limit=Config->RunLimit,Count=Buckets;
    if(Limit>(SIZE_MAX-Bytes)/sizeof(xcronexecutorslot))return xcronExecutorFail(XCRON_EXECUTOR_LIMIT,XERR_NONE,Issue);
    size_t SlotBytes=Limit*sizeof(xcronexecutorslot);Bytes+=SlotBytes;
    if(Count>(SIZE_MAX-Bytes)/sizeof(uint32) || (Bytes+=Count*sizeof(uint32))>Config->BytesLimit)return xcronExecutorFail(XCRON_EXECUTOR_LIMIT,XERR_NONE,Issue);
    xcronexecutor* B=xrtMalloc(Bytes);if(!B)return xcronExecutorFail(XCRON_EXECUTOR_MEMORY,XERR_NONE,Issue);
    memset(B,0,Bytes);B->Magic=XCRON_EXECUTOR_MAGIC;B->Owner=xrtThreadCurrentId();B->Config=*Config;B->Stats.Bytes=Bytes;B->Stats.Limit=Config->RunLimit;
    B->Slots=(xcronexecutorslot*)B->Storage;B->Table=(uint32*)(B->Storage+SlotBytes);B->Buckets=Buckets;B->Mask=Buckets-1;B->Free=0;B->Pending=XCRON_EXECUTOR_NONE;
    xrtAtomic32Init(&B->Head,XCRON_EXECUTOR_NONE);xrtAtomic32Init(&B->Producers,0);
    for(uint32 I=0;I<Buckets;++I)B->Table[I]=XCRON_EXECUTOR_NONE;
    for(uint32 I=0;I<Config->RunLimit;++I){B->Slots[I].Owner=B;B->Slots[I].Index=I;B->Slots[I].Next=I+1<Config->RunLimit?I+1:XCRON_EXECUTOR_NONE;xrtAtomic32Init(&B->Slots[I].Execution.Requested,0);}
    *Out=B;xcronExecutorOK(Issue);return true;
}
XCRON_API bool xcronExecutorSubmit(xcronexecutor* B,const xcronrun* Run,xcronexecutorissue* Issue)
{
    void* O[]={Issue};size_t N[]={Issue?sizeof(*Issue):0};
    if(!B || !Run || !xcronExecutorOutputs(B,Run,sizeof(*Run),O,N,1) || !Run->Id || !Run->Job || (Run->Flags&~XCRON_RUN_COALESCED) || Run->Through<Run->ScheduledAt)
        return xcronExecutorFail(XCRON_EXECUTOR_ARGUMENT,XERR_NONE,NULL);
    if(!xcronExecutorOwner(B,Issue))return false;
    if(B->Stats.Closed){xcronExecutorIncrement(&B->Stats.Rejected);return xcronExecutorFail(XCRON_EXECUTOR_CLOSED,XERR_NONE,Issue);}
    uint32 P=xcronExecutorFind(B,Run->Id);
    if(B->Table[P]!=XCRON_EXECUTOR_NONE){xcronExecutorIncrement(&B->Stats.Rejected);return xcronExecutorFail(XCRON_EXECUTOR_DUPLICATE,XERR_NONE,Issue);}
    if(B->Free==XCRON_EXECUTOR_NONE){xcronExecutorIncrement(&B->Stats.Rejected);return xcronExecutorFail(XCRON_EXECUTOR_LIMIT,XERR_NONE,Issue);}
    uint32 I=B->Free;xcronexecutorslot* S=&B->Slots[I];B->Free=S->Next;S->Used=true;S->Done=(xcroncompletion){.Run=*Run,.Result=XCRON_RUN_CANCELLED};xrtAtomic32Store(&S->Execution.Requested,0,XMEMORY_RELAXED);B->Table[P]=I;++B->Stats.Active;
    xrtAtomic32FetchAdd(&B->Producers,1,XMEMORY_RELAXED);
    if(!xrtExecutorSubmit(B->Config.Executor,xcronExecutorWork,S,xcronExecutorDrop,NULL)){
        xerrkind Cause=xrtErrorKind(xrtGetError());xrtAtomic32FetchSub(&B->Producers,1,XMEMORY_RELAXED);--B->Stats.Active;xcronExecutorRemove(B,P);S->Used=false;S->Next=B->Free;B->Free=I;xcronExecutorIncrement(&B->Stats.Rejected);
        return xcronExecutorFail(XCRON_EXECUTOR_REJECTED,Cause,Issue);
    }
    xcronExecutorIncrement(&B->Stats.Submitted);if(B->Stats.Active>B->Stats.Peak)B->Stats.Peak=B->Stats.Active;xcronExecutorOK(Issue);return true;
}
static bool xcronExecutorRequest(xcronexecutor* B,xcronexecutorslot* S)
{if(xrtAtomic32Exchange(&S->Execution.Requested,1,XMEMORY_ACQ_REL))return false;xcronExecutorIncrement(&B->Stats.CancelRequests);return true;}
XCRON_API bool xcronExecutorCancel(xcronexecutor* B,xcronrunid Id,xcronexecutorissue* Issue)
{
    if(!xcronExecutorArgs(B,NULL,0,Issue))return xcronExecutorFail(XCRON_EXECUTOR_ARGUMENT,XERR_NONE,NULL);
    if(!xcronExecutorOwner(B,Issue))return false;
    uint32 P=xcronExecutorFind(B,Id);if(!Id || B->Table[P]==XCRON_EXECUTOR_NONE)return xcronExecutorFail(XCRON_EXECUTOR_STALE,XERR_NONE,Issue);
    xcronExecutorRequest(B,&B->Slots[B->Table[P]]);xcronExecutorOK(Issue);return true;
}
XCRON_API bool xcronExecutorCancelJob(xcronexecutor* B,xcronjobid Id,uint32* Requested,xcronexecutorissue* Issue)
{
    if(!Id || !Requested || !xcronExecutorArgs(B,Requested,sizeof(*Requested),Issue))return xcronExecutorFail(XCRON_EXECUTOR_ARGUMENT,XERR_NONE,NULL);
    if(!xcronExecutorOwner(B,Issue))return false;
    uint32 Count=0;for(uint32 I=0;I<B->Config.RunLimit;++I)if(B->Slots[I].Used && B->Slots[I].Done.Run.Job==Id && xcronExecutorRequest(B,&B->Slots[I]))++Count;
    *Requested=Count;xcronExecutorOK(Issue);return true;
}
XCRON_API bool xcronExecutorDrain(xcronexecutor* B,xcroncompletion* Completed,size_t Capacity,size_t* Count,xcronexecutorissue* Issue)
{
    void* O[]={Completed,Count,Issue};size_t N[]={Capacity<=SIZE_MAX/sizeof(*Completed)?Capacity*sizeof(*Completed):SIZE_MAX,sizeof(*Count),Issue?sizeof(*Issue):0};
    if(!B || !Completed || !Count || !Capacity || Capacity>SIZE_MAX/sizeof(*Completed) || !xcronExecutorOutputs(B,NULL,0,O,N,3))return xcronExecutorFail(XCRON_EXECUTOR_ARGUMENT,XERR_NONE,NULL);
    if(!xcronExecutorOwner(B,Issue))return false;
    if(B->Pending==XCRON_EXECUTOR_NONE)B->Pending=xrtAtomic32Exchange(&B->Head,XCRON_EXECUTOR_NONE,XMEMORY_ACQUIRE);
    size_t Written=0;
    while(Written<Capacity && B->Pending!=XCRON_EXECUTOR_NONE){
        uint32 I=B->Pending;xcronexecutorslot* S=&B->Slots[I];B->Pending=S->Next;Completed[Written++]=S->Done;S->Done.Error=NULL;
        xcronExecutorRemove(B,xcronExecutorFind(B,S->Done.Run.Id));S->Used=false;S->Next=B->Free;B->Free=I;--B->Stats.Active;xcronExecutorIncrement(&B->Stats.Drained);
    }
    *Count=Written;xcronExecutorOK(Issue);return true;
}
XCRON_API bool xcronExecutorClose(xcronexecutor* B,bool CancelRuns,xcronexecutorissue* Issue)
{
    if(!xcronExecutorArgs(B,NULL,0,Issue))return xcronExecutorFail(XCRON_EXECUTOR_ARGUMENT,XERR_NONE,NULL);
    if(!xcronExecutorOwner(B,Issue))return false;
    B->Stats.Closed=true;if(CancelRuns)for(uint32 I=0;I<B->Config.RunLimit;++I)if(B->Slots[I].Used)xcronExecutorRequest(B,&B->Slots[I]);
    xcronExecutorOK(Issue);return true;
}
XCRON_API bool xcronExecutorInspect(const xcronexecutor* B,xcronexecutorstats* Stats,xcronexecutorissue* Issue)
{
    if(!Stats || !xcronExecutorArgs(B,Stats,sizeof(*Stats),Issue))return xcronExecutorFail(XCRON_EXECUTOR_ARGUMENT,XERR_NONE,NULL);
    if(!xcronExecutorOwner(B,Issue))return false;
    *Stats=B->Stats;Stats->Producers=xrtAtomic32Load(&B->Producers,XMEMORY_ACQUIRE);xcronExecutorOK(Issue);return true;
}
XCRON_API bool xcronExecutorDestroy(xcronexecutor* B,xcronexecutorissue* Issue)
{
    if(!xcronExecutorArgs(B,NULL,0,Issue))return xcronExecutorFail(XCRON_EXECUTOR_ARGUMENT,XERR_NONE,NULL);
    if(!xcronExecutorOwner(B,Issue))return false;
    if(B->Stats.Active || xrtAtomic32Load(&B->Producers,XMEMORY_ACQUIRE))return xcronExecutorFail(XCRON_EXECUTOR_BUSY,XERR_NONE,Issue);
    B->Magic=0;xrtFree(B);xcronExecutorOK(Issue);return true;
}
XCRON_API bool xcronExecutionCancelled(const xcronexecution* Execution)
{return Execution && xrtAtomic32Load(&Execution->Requested,XMEMORY_ACQUIRE)!=0;}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xcron/src/service/service.c */
/* ========================================================================== */

#if defined(XCRON_FEATURE_XCRON_SERVICE)
#include <math.h>
#include <string.h>
#include <stdlib.h>
#if defined(XCRON_FEATURE_XCRON_SERVICE)
#define XCRON_SERVICE_MAGIC UINT32_C(0x58435631)
#define XCRON_SERVICE_NONE UINT32_MAX
typedef enum xcroncommandstate {XCRON_CMD_FREE=0,XCRON_CMD_QUEUED,XCRON_CMD_PROCESSING,XCRON_CMD_READY,XCRON_CMD_ADMITTING} xcroncommandstate;
typedef struct xcroncommand {
    uint64 Generation;
    uint32 Next;
    xcroncommandstate State;
    xcronjobconfig Add;
    xcronjobid Job;
    bool Cancel;
    xcronreply Reply;
} xcroncommand;
typedef struct xcronreservation {
    xcronrun Run;
    xcronservicecompletion Done;
    xerror* Rejection;
    uint64 Deadline,RetryAt;
    uint32 Attempts,Next,HeapPosition;
} xcronreservation;
struct xcronservice {
    uint32 Magic;
    size_t ArenaBytes;
    xcronserviceconfig Config;
    xmutex Mutex;
    xcond Changed;
    xevent Wake;
    xeventwaiter Waiter;
    bool WaiterLive;
    xthread* Thread;
    uint64 Owner;
    bool Ready,Activated,Closing,CancelRuns,Stopped;
    xcronserviceissue Startup;
    xerror* StartupError;
    xerror* FatalError;
    xcronscheduler* Kernel;
    xcronexecutor* Bridge;
    xcronservicestats Stats;
    xatomic64 WakeFailures;
    xcroncommand* Commands;
    xcronreservation* Reservations;
    uint32* ActiveMap;
    uint32* RetryHeap;
    uint32* ReservedBatch;
    xcronrun* RunBatch;
    xcroncompletion* DoneBatch;
    uint32 FreeCommand,CommandHead,CommandTail,FreeCompletion,DoneHead,DoneTail;
    uint32 RetryCount;
    uint32 AdmissionHead,AdmissionTail,AdmissionCount;
    uint8 CommandBits,RunBits;
    xcronclock Now;
    bool HasClock;
    uint64 SubmitAttempts,RejectedSubmissions,Retries,AdmissionExpired;
};
static bool xcronServiceOutputs(const xcronservice* S,const void* Input,size_t InputSize,void* const* O,const size_t* N,size_t Count)
{
    if(S && (!xrtMemRangeValid(S,sizeof(*S)) || S->Magic!=XCRON_SERVICE_MAGIC || !xrtMemRangeValid(S,S->ArenaBytes)))return false;
    if(!xrtMemRangeValid(Input,InputSize))return false;
    for(size_t I=0;I<Count;++I){
        if(!xrtMemRangeValid(O[I],N[I]) || (S && xrtMemRangesOverlap(S,S->ArenaBytes,O[I],N[I])) || xrtMemRangesOverlap(Input,InputSize,O[I],N[I]))return false;
        for(size_t J=0;J<I;++J)if(xrtMemRangesOverlap(O[J],N[J],O[I],N[I]))return false;
    }
    return true;
}
static bool xcronServiceArgs(const xcronservice* S,void* Out,size_t Size,xcronserviceissue* Issue)
{void* O[]={Out,Issue};size_t N[]={Size,Issue?sizeof(*Issue):0};return S && xcronServiceOutputs(S,NULL,0,O,N,2);}
static void xcronServiceOK(xcronserviceissue* I)
{if(I)*I=(xcronserviceissue){XCRON_SERVICE_OK,XERR_NONE};}
static bool xcronServiceFail(xcronserviceerror Code,xerrkind Cause,xcronserviceissue* I)
{
    if(I)*I=(xcronserviceissue){Code,Cause};
    xerrkind Kind=XERR_ARGUMENT;cstr Text="invalid cron service arguments/spans/output aliases";
    switch(Code){
    case XCRON_SERVICE_LIMIT:Kind=XERR_RANGE;Text="cron service bounded capacity or byte limit exceeded";break;
    case XCRON_SERVICE_MEMORY:Kind=XERR_MEMORY;Text="cron service startup allocation failed";break;
    case XCRON_SERVICE_CLOSED:Kind=XERR_CLOSED;Text="cron service is closing";break;
    case XCRON_SERVICE_STALE:Kind=XERR_NOT_FOUND;Text="cron service request ticket is stale";break;
    case XCRON_SERVICE_BUSY:Kind=XERR_STATE;Text="cron service still has owner, jobs, tickets or completion duties";break;
    case XCRON_SERVICE_THREAD:Kind=XERR_STATE;Text="cron service blocking wait would depend on its current owner/executor";break;
    case XCRON_SERVICE_CLOCK:Kind=XERR_STATE;Text="cron service clock failed or monotonic sample regressed";break;
    case XCRON_SERVICE_SYSTEM:Kind=XERR_IO;Text="cron service XRT synchronization or thread operation failed";break;
    case XCRON_SERVICE_KERNEL:Kind=XERR_STATE;Text="cron service kernel operation failed";break;
    case XCRON_SERVICE_EXECUTOR:Kind=XERR_STATE;Text="cron service executor adapter operation failed";break;
    default:break;
    }
    if(Cause!=XERR_NONE){xerror* E=xrtTakeError();xerror* W=xrtErrorWrap(E,Kind,"xrt.cron.service",Code,Text);xrtErrorFree(E);if(W)xrtSetErrorTake(W);else xrtSetErrorKind(XERR_MEMORY);}
    else xrtSetErrorInfo(Kind,"xrt.cron.service",Code,Text);
    return false;
}
static void xcronServiceIncrement(uint64* V)
{if(*V!=UINT64_MAX)++*V;}
static uint64 xcronServiceAddTime(uint64 A,uint64 B)
{return B>UINT64_MAX-A?UINT64_MAX:A+B;}
static uint8 xcronServiceBits(uint32 Limit)
{uint8 B=0;for(uint32 V=Limit-1;V;V>>=1)++B;return B;}
/* Wake must never consume a worker's business error or a caller's ambient error. */
static void xcronServiceSignal(ptr Raw)
{
    xcronservice* S=Raw;xerror* E=xrtTakeError();
    if(!xrtEventSet(&S->Wake)){uint64 Old=xrtAtomic64Load(&S->WakeFailures,XMEMORY_RELAXED);
        while(Old!=UINT64_MAX && !xrtAtomic64CompareExchange(&S->WakeFailures,&Old,Old+1,XMEMORY_RELAXED,XMEMORY_RELAXED)){} }
    xrtSetErrorTake(E);
}
static void xcronServiceFault(xcronservice* S,xcronserviceerror Code)
{
    xerrkind Cause=xrtErrorKind(xrtGetError());xcronServiceFail(Code,Cause,NULL);xerror* E=xrtTakeError();
    xrtMutexLock(&S->Mutex);
    if(S->Stats.Failure.Code==XCRON_SERVICE_OK){S->Stats.Failure=(xcronserviceissue){Code,Cause};S->FatalError=E;E=NULL;}
    S->Closing=true;S->CancelRuns=true;S->Stats.State=XCRON_SERVICE_STOPPING;
    xrtCondBroadcast(&S->Changed);xrtMutexUnlock(&S->Mutex);xrtErrorFree(E);
}
static bool xcronServiceClockRead(xcronservice* S)
{
    xrtClearError();xcronclock C={0};bool OK;
    if(S->Config.Clock)OK=S->Config.Clock(&C,S->Config.ClockContext);
    else {double T=xrtTimer()*1000.0;C.Wall=xrtNow();OK=isfinite(T) && T>=0 && T<18446744073709551616.0 && !xrtGetError();if(OK)C.MonotonicMilliseconds=(uint64)T;}
    if(!OK || (S->HasClock && C.MonotonicMilliseconds<S->Now.MonotonicMilliseconds)){xcronServiceFault(S,XCRON_SERVICE_CLOCK);return false;}
    S->Now=C;S->HasClock=true;xrtClearError();return true;
}
static bool xcronServiceEarlier(const xcronservice* S,uint32 A,uint32 B)
{
    const xcronreservation* X=&S->Reservations[A];const xcronreservation* Y=&S->Reservations[B];
    uint64 XT=X->RetryAt<X->Deadline?X->RetryAt:X->Deadline,YT=Y->RetryAt<Y->Deadline?Y->RetryAt:Y->Deadline;
    return XT<YT || (XT==YT && X->Run.Id<Y->Run.Id);
}
static void xcronServiceHeapUp(xcronservice* S,uint32 P)
{
    uint32 I=S->RetryHeap[P];while(P){uint32 Parent=(P-1)/2;if(!xcronServiceEarlier(S,I,S->RetryHeap[Parent]))break;
        S->RetryHeap[P]=S->RetryHeap[Parent];S->Reservations[S->RetryHeap[P]].HeapPosition=P;P=Parent;}
    S->RetryHeap[P]=I;S->Reservations[I].HeapPosition=P;
}
static void xcronServiceHeapDown(xcronservice* S,uint32 P)
{
    uint32 I=S->RetryHeap[P];while(P<S->RetryCount/2){uint32 C=P*2+1;if(C+1<S->RetryCount && xcronServiceEarlier(S,S->RetryHeap[C+1],S->RetryHeap[C]))++C;
        if(!xcronServiceEarlier(S,S->RetryHeap[C],I))break;
        S->RetryHeap[P]=S->RetryHeap[C];S->Reservations[S->RetryHeap[P]].HeapPosition=P;P=C;}
    S->RetryHeap[P]=I;S->Reservations[I].HeapPosition=P;
}
static void xcronServiceHeapAdd(xcronservice* S,uint32 I)
{uint32 P=S->RetryCount++;S->RetryHeap[P]=I;xcronServiceHeapUp(S,P);}
static void xcronServiceHeapRemove(xcronservice* S,uint32 I)
{
    uint32 P=S->Reservations[I].HeapPosition;--S->RetryCount;S->Reservations[I].HeapPosition=XCRON_SERVICE_NONE;
    if(P==S->RetryCount)return;
    S->RetryHeap[P]=S->RetryHeap[S->RetryCount];S->Reservations[S->RetryHeap[P]].HeapPosition=P;
    if(P && xcronServiceEarlier(S,S->RetryHeap[P],S->RetryHeap[(P-1)/2]))xcronServiceHeapUp(S,P);else xcronServiceHeapDown(S,P);
}
static uint32 xcronServiceRunIndex(const xcronservice* S,xcronrunid Id)
{return (uint32)(Id&((UINT64_C(1)<<S->RunBits)-1));}
static bool xcronServiceFinish(xcronservice* S,uint32 I,xcronrunresult Result,xcronservicecompletioncode Code,xcronexecutorerror Executor,xerror* Error)
{
    xcronreservation* R=&S->Reservations[I];xrtClearError();
    if(!xcronSchedulerComplete(S->Kernel,R->Run.Id,Result,NULL)){xrtErrorFree(Error);xcronServiceFault(S,XCRON_SERVICE_KERNEL);return false;}
    S->ActiveMap[xcronServiceRunIndex(S,R->Run.Id)]=XCRON_SERVICE_NONE;
    R->Done=(xcronservicecompletion){.Id=R->Run.Id,.Job=R->Run.Job,.ScheduledAt=R->Run.ScheduledAt,.Through=R->Run.Through,.Observed=R->Run.Observed,.Flags=R->Run.Flags,.Result=Result,.Code=Code,.Executor=Executor,.Attempts=R->Attempts,.Error=Error};
    /* No borrowed Run.Data escapes in the ready metadata record. */
    R->Run.Data=NULL;R->Next=XCRON_SERVICE_NONE;xrtMutexLock(&S->Mutex);
    if(S->DoneTail==XCRON_SERVICE_NONE)S->DoneHead=I;else S->Reservations[S->DoneTail].Next=I;S->DoneTail=I;
    ++S->Stats.ReadyCompletions;xcronServiceIncrement(&S->Stats.Published);xrtMutexUnlock(&S->Mutex);return true;
}
static void xcronServiceCancelUnsent(xcronservice* S,xcronjobid Job)
{
    /* Cold control operation; heap removal keeps retries ordered after cancellation. */
    for(uint32 Index=0;Index<S->Config.Scheduler.RunLimit;++Index){uint32 I=S->ActiveMap[Index];if(I==XCRON_SERVICE_NONE)continue;xcronreservation* R=&S->Reservations[I];
        if(R->HeapPosition==XCRON_SERVICE_NONE || (Job && R->Run.Job!=Job))continue;
        xcronServiceHeapRemove(S,I);xrtErrorFree(R->Rejection);R->Rejection=NULL;
        xcronServiceFinish(S,I,XCRON_RUN_CANCELLED,XCRON_COMPLETION_CANCELLED,XCRON_EXECUTOR_OK,NULL);
    }
}
static void xcronServiceSubmit(xcronservice* S,uint32 I,bool Retry)
{
    xcronreservation* R=&S->Reservations[I];xcronexecutorissue E;uint64 Now=S->Now.MonotonicMilliseconds;
    if(Retry && Now>=R->Deadline){xerror* Error=R->Rejection;R->Rejection=NULL;xcronServiceIncrement(&S->AdmissionExpired);
        xcronServiceFinish(S,I,XCRON_RUN_FAILED,XCRON_COMPLETION_EXPIRED,XCRON_EXECUTOR_REJECTED,Error);return;}
    ++R->Attempts;xcronServiceIncrement(&S->SubmitAttempts);if(Retry)xcronServiceIncrement(&S->Retries);
    xrtClearError();bool OK=xcronExecutorSubmit(S->Bridge,&R->Run,&E);xerror* Error=xrtTakeError();xrtErrorFree(R->Rejection);R->Rejection=NULL;
    if(OK){xrtErrorFree(Error);return;}
    xcronServiceIncrement(&S->RejectedSubmissions);
    bool Again=E.Code==XCRON_EXECUTOR_REJECTED && E.Cause==XERR_AGAIN;
    bool Enabled=S->Config.AdmissionMilliseconds!=0;
    if(Again && Enabled && R->Attempts<S->Config.RetryAttempts && Now<R->Deadline){
        R->Rejection=Error;R->RetryAt=xcronServiceAddTime(Now,S->Config.RetryIntervalMilliseconds);xcronServiceHeapAdd(S,I);return;
    }
    xcronservicecompletioncode Code=Again && Enabled?XCRON_COMPLETION_EXPIRED:XCRON_COMPLETION_REJECTED;
    if(Code==XCRON_COMPLETION_EXPIRED)xcronServiceIncrement(&S->AdmissionExpired);
    xcronServiceFinish(S,I,XCRON_RUN_FAILED,Code,E.Code,Error);
}
static void xcronServiceReplyReady(xcronservice* S,xcroncommand* C,xcronreply R,bool OK)
{
    if(!OK)R.Error=xrtTakeError();else xrtClearError();
    xrtMutexLock(&S->Mutex);C->Reply=R;C->State=XCRON_CMD_READY;--S->Stats.PendingRequests;++S->Stats.ReadyReplies;xrtCondBroadcast(&S->Changed);xrtMutexUnlock(&S->Mutex);
}
static void xcronServiceAdmissionAdd(xcronservice* S,uint32 I)
{
    S->Commands[I].Next=XCRON_SERVICE_NONE;
    if(S->AdmissionTail!=XCRON_SERVICE_NONE)S->Commands[S->AdmissionTail].Next=I;else S->AdmissionHead=I;
    S->AdmissionTail=I;++S->AdmissionCount;
}
static void xcronServiceAdmissions(xcronservice* S)
{
    uint32 Count=S->AdmissionCount;if(Count>S->Config.CommandWorkLimit)Count=S->Config.CommandWorkLimit;
    for(uint32 N=0;N<Count;++N){
        uint32 I=S->AdmissionHead;xcroncommand* C=&S->Commands[I];S->AdmissionHead=C->Next;
        if(S->AdmissionHead==XCRON_SERVICE_NONE)S->AdmissionTail=XCRON_SERVICE_NONE;
        --S->AdmissionCount;xrtMutexLock(&S->Mutex);bool Closing=S->Closing;xcronreply R=C->Reply;xrtMutexUnlock(&S->Mutex);
        xrtClearError();xcronadmissionstatus Status=xcronSchedulerFinishAdmission(S->Kernel,C->Job,Closing,&R.Scheduler);
        if(Status==XCRON_ADMISSION_PENDING){xcronServiceAdmissionAdd(S,I);continue;}
        bool OK=Status==XCRON_ADMISSION_ACCEPTED;
        if(OK)R.Job=C->Job;
        else {R.Code=R.Scheduler.Code==XCRON_SCHEDULER_CLOSED?XCRON_SERVICE_CLOSED:XCRON_SERVICE_KERNEL;R.Job=0;R.Data=C->Add.Data;}
        xcronZoneRelease(C->Add.Zone);C->Add.Zone=NULL;xcronServiceReplyReady(S,C,R,OK);
    }
}
static void xcronServiceCommand(xcronservice* S,uint32 I)
{
    xcroncommand* C=&S->Commands[I];xcronreply R={.Request=C->Reply.Request,.Kind=C->Reply.Kind};bool OK=false;
    xrtClearError();
    switch(R.Kind){
    case XCRON_REQUEST_ADD:{
        xrtMutexLock(&S->Mutex);bool Closing=S->Closing;xrtMutexUnlock(&S->Mutex);
        if(Closing){R.Code=XCRON_SERVICE_CLOSED;xcronServiceFail(R.Code,XERR_NONE,NULL);}
        else {OK=xcronSchedulerBeginAdmission(S->Kernel,&C->Add,&C->Job,&R.Scheduler);if(!OK)R.Code=XCRON_SERVICE_KERNEL;
            else {xrtMutexLock(&S->Mutex);C->Reply=R;C->State=XCRON_CMD_ADMITTING;xrtMutexUnlock(&S->Mutex);xcronServiceAdmissionAdd(S,I);return;}}
        if(!OK)R.Data=C->Add.Data;
        xcronZoneRelease(C->Add.Zone);C->Add.Zone=NULL;break;}
    case XCRON_REQUEST_CANCEL:
        R.Job=C->Job;OK=xcronSchedulerCancel(S->Kernel,C->Job,&R.Scheduler);if(!OK)R.Code=XCRON_SERVICE_KERNEL;
        if(OK && C->Cancel){uint32 Requested=0;OK=xcronExecutorCancelJob(S->Bridge,C->Job,&Requested,&R.Executor);if(!OK)R.Code=XCRON_SERVICE_EXECUTOR;xcronServiceCancelUnsent(S,C->Job);}break;
    case XCRON_REQUEST_INFO:R.Job=C->Job;OK=xcronSchedulerJob(S->Kernel,C->Job,&R.Info,&R.Scheduler);if(!OK)R.Code=XCRON_SERVICE_KERNEL;break;
    case XCRON_REQUEST_REAP:OK=xcronSchedulerReap(S->Kernel,&R.Retired,&R.Found,&R.Scheduler);if(!OK)R.Code=XCRON_SERVICE_KERNEL;break;
    }
    xcronServiceReplyReady(S,C,R,OK);
}
static void xcronServiceStatsSet(xcronservice* S,const xcronschedulerstats* K,const xcronexecutorstats* E)
{
    xrtMutexLock(&S->Mutex);S->Stats.Kernel=*K;S->Stats.Executor=*E;S->Stats.KernelBytes=K->Bytes;S->Stats.AdapterBytes=E->Bytes;
    S->Stats.Bytes=S->ArenaBytes+K->Bytes+E->Bytes;S->Stats.LastClock=S->Now;S->Stats.Unsent=S->RetryCount;
    S->Stats.SubmitAttempts=S->SubmitAttempts;S->Stats.RejectedSubmissions=S->RejectedSubmissions;S->Stats.Retries=S->Retries;S->Stats.AdmissionExpired=S->AdmissionExpired;
    xrtMutexUnlock(&S->Mutex);
}
static bool xcronServicePublishStats(xcronservice* S,xcronschedulerstats* K,xcronexecutorstats* E)
{
    if(!xcronSchedulerInspect(S->Kernel,K,NULL)){xcronServiceFault(S,XCRON_SERVICE_KERNEL);return false;}
    if(!xcronExecutorInspect(S->Bridge,E,NULL)){xcronServiceFault(S,XCRON_SERVICE_EXECUTOR);return false;}
    xcronServiceStatsSet(S,K,E);return true;
}
/* Owner during stop, or an exclusive caller after native join on retry. */
static bool xcronServiceWaiterRelease(xcronservice* S)
{
    if(!S->WaiterLive)return true;
    if(!xrtEventWaiterUnit(&S->Waiter))return false;
    S->WaiterLive=false;
    xrtMutexLock(&S->Mutex);S->Stats.ActiveNativeTimers=0;xrtMutexUnlock(&S->Mutex);
    return true;
}
static int32 xcronServiceOwner(ptr Raw)
{
    xcronservice* S=Raw;xallocationscope Scope={0};bool Budget=false,OK=true;xcronserviceerror Code=XCRON_SERVICE_OK;
    S->Owner=xrtThreadCurrentId();
    if(S->Config.InitializationBudget){Budget=xrtAllocationBudgetEnter(S->Config.InitializationBudget,&Scope);if(!Budget){OK=false;Code=XCRON_SERVICE_MEMORY;}}
    if(OK && !xcronSchedulerCreate(&S->Config.Scheduler,&S->Kernel,NULL)){OK=false;Code=xrtErrorKind(xrtGetError())==XERR_MEMORY?XCRON_SERVICE_MEMORY:XCRON_SERVICE_KERNEL;}
    xcronexecutorconfig B={.Executor=S->Config.Executor,.Proc=S->Config.Proc,.Context=S->Config.Context,.Notify=xcronServiceSignal,.NotifyContext=S,.RunLimit=S->Config.Scheduler.RunLimit,.BytesLimit=S->Config.AdapterBytesLimit};
    if(OK && !xcronExecutorCreate(&B,&S->Bridge,NULL)){OK=false;Code=xrtErrorKind(xrtGetError())==XERR_MEMORY?XCRON_SERVICE_MEMORY:XCRON_SERVICE_EXECUTOR;}
    if(OK){
        xcronschedulerstats K;xcronexecutorstats E;
        if(!xcronSchedulerInspect(S->Kernel,&K,NULL)){OK=false;Code=XCRON_SERVICE_KERNEL;}
        else if(!xcronExecutorInspect(S->Bridge,&E,NULL)){OK=false;Code=XCRON_SERVICE_EXECUTOR;}
        else xcronServiceStatsSet(S,&K,&E);
    }
    /* Last fallible startup resource: Init failure owns no native timer. The
     * owner, not the creator, binds the reusable wait context to its thread. */
    if(OK && !xrtEventWaiterInit(&S->Waiter,&S->Wake,0)){OK=false;Code=XCRON_SERVICE_SYSTEM;}
    if(OK){
        S->WaiterLive=true;
        /* Get has no native/allocation failure with these disjoint live spans. */
        if(!xrtEventWaiterGet(&S->Waiter,&S->Stats.Waiter))abort();
        S->Stats.ActiveNativeTimers=S->Stats.Waiter.NativeTimers;
    }
    xerror* StartupError=OK?NULL:xrtTakeError();
    if(Budget)xrtAllocationBudgetLeave(&Scope);
    if(!OK){if(S->Bridge){xcronExecutorDestroy(S->Bridge,NULL);S->Bridge=NULL;}
        if(S->Kernel){xcronSchedulerDestroy(S->Kernel,NULL);S->Kernel=NULL;}
        xrtMutexLock(&S->Mutex);S->Startup=(xcronserviceissue){Code,xrtErrorKind(StartupError)};S->StartupError=StartupError;S->Ready=true;xrtCondBroadcast(&S->Changed);xrtMutexUnlock(&S->Mutex);return 0;}
    xrtMutexLock(&S->Mutex);S->Ready=true;xrtCondBroadcast(&S->Changed);
    while(!S->Activated)xrtCondWaitFor(&S->Changed,&S->Mutex,100);
    S->Stats.State=S->Closing?XCRON_SERVICE_STOPPING:XCRON_SERVICE_RUNNING;xrtMutexUnlock(&S->Mutex);
    bool AppliedClose=false,AppliedCancel=false;
    for(;;){
        xrtMutexLock(&S->Mutex);bool Closing=S->Closing,Cancel=S->CancelRuns;xrtMutexUnlock(&S->Mutex);
        if(Closing && !AppliedClose){xcronSchedulerClose(S->Kernel,NULL);xcronExecutorClose(S->Bridge,Cancel,NULL);xcronServiceCancelUnsent(S,0);AppliedClose=true;AppliedCancel=Cancel;}
        else if(Closing && Cancel && !AppliedCancel){xcronExecutorClose(S->Bridge,true,NULL);AppliedCancel=true;}
        for(uint32 N=0;N<S->Config.CommandWorkLimit;++N){
            xrtMutexLock(&S->Mutex);uint32 I=S->CommandHead;
            if(I!=XCRON_SERVICE_NONE){S->CommandHead=S->Commands[I].Next;if(S->CommandHead==XCRON_SERVICE_NONE)S->CommandTail=XCRON_SERVICE_NONE;S->Commands[I].State=XCRON_CMD_PROCESSING;}
            xrtMutexUnlock(&S->Mutex);if(I==XCRON_SERVICE_NONE)break;xcronServiceCommand(S,I);
        }
        xcronServiceAdmissions(S);
        size_t Completed=0;
        if(!xcronExecutorDrain(S->Bridge,S->DoneBatch,S->Config.BatchLimit,&Completed,NULL))xcronServiceFault(S,XCRON_SERVICE_EXECUTOR);
        for(size_t N=0;N<Completed;++N){xcroncompletion* D=&S->DoneBatch[N];uint32 Index=xcronServiceRunIndex(S,D->Run.Id);
            uint32 I=Index<S->Config.Scheduler.RunLimit?S->ActiveMap[Index]:XCRON_SERVICE_NONE;
            if(I==XCRON_SERVICE_NONE || S->Reservations[I].Run.Id!=D->Run.Id){xrtErrorFree(D->Error);xrtClearError();xcronServiceFault(S,XCRON_SERVICE_EXECUTOR);continue;}
            xcronServiceFinish(S,I,D->Result,XCRON_COMPLETION_NORMAL,D->Code,D->Error);
        }
        xrtMutexLock(&S->Mutex);Closing=S->Closing;xrtMutexUnlock(&S->Mutex);
        bool More=false;uint32 Searching=0;
        if(!Closing && xcronServiceClockRead(S)){
            xrtMutexLock(&S->Mutex);Closing=S->Closing;xrtMutexUnlock(&S->Mutex);
            if(!Closing){
                for(uint32 N=0;N<S->Config.BatchLimit && S->RetryCount;++N){uint32 I=S->RetryHeap[0];xcronreservation* R=&S->Reservations[I];
                    if(S->Now.MonotonicMilliseconds<R->RetryAt && S->Now.MonotonicMilliseconds<R->Deadline)break;
                    xcronServiceHeapRemove(S,I);xcronServiceSubmit(S,I,true);}
                xcronexecutorstats E;xcronExecutorInspect(S->Bridge,&E,NULL);uint32 Capacity=S->Config.Scheduler.RunLimit-E.Active;
                if(Capacity>S->Config.BatchLimit)Capacity=S->Config.BatchLimit;
                xrtMutexLock(&S->Mutex);uint32 Reserved=0;
                while(Reserved<Capacity && S->FreeCompletion!=XCRON_SERVICE_NONE){uint32 I=S->FreeCompletion;S->FreeCompletion=S->Reservations[I].Next;S->ReservedBatch[Reserved++]=I;++S->Stats.ReservedCompletions;}
                xrtMutexUnlock(&S->Mutex);
                xcronpollbudgeted Q={0};uint32 Work=S->Config.Scheduler.WorkLimit;
                bool Polled=xcronSchedulerPollBudgeted(S->Kernel,S->Now,(xcronzonedbudget){Work,Work,Work},S->RunBatch,Reserved,&Q,NULL);
                if(!Polled)xcronServiceFault(S,XCRON_SERVICE_KERNEL);
                else {xrtMutexLock(&S->Mutex);S->Stats.LastPoll=Q;xrtMutexUnlock(&S->Mutex);}
                xcronpoll P=Q.Poll;Searching=Q.Searching;
                for(uint32 N=0;N<(uint32)P.Count;++N){uint32 I=S->ReservedBatch[N];xcronreservation* R=&S->Reservations[I];
                    R->Run=S->RunBatch[N];R->Attempts=0;R->Rejection=NULL;R->HeapPosition=XCRON_SERVICE_NONE;R->Deadline=xcronServiceAddTime(S->Now.MonotonicMilliseconds,S->Config.AdmissionMilliseconds);
                    S->ActiveMap[xcronServiceRunIndex(S,R->Run.Id)]=I;xcronServiceSubmit(S,I,false);}
                xrtMutexLock(&S->Mutex);
                for(uint32 N=(uint32)P.Count;N<Reserved;++N){uint32 I=S->ReservedBatch[N];S->Reservations[I].Next=S->FreeCompletion;S->FreeCompletion=I;--S->Stats.ReservedCompletions;}
                xrtMutexUnlock(&S->Mutex);More=Polled && P.More;
            }
        }
        xcronServiceAdmissions(S);
        xcronschedulerstats K;xcronexecutorstats E;if(!xcronServicePublishStats(S,&K,&E))continue;
        xrtMutexLock(&S->Mutex);Closing=S->Closing;bool Commands=S->CommandHead!=XCRON_SERVICE_NONE;
        bool Capacity=S->FreeCompletion!=XCRON_SERVICE_NONE && E.Active<S->Config.Scheduler.RunLimit;
        bool Stop=Closing && AppliedClose && (!S->CancelRuns || AppliedCancel) && !K.Running && !E.Active && !E.Producers && !S->RetryCount && !S->Stats.PendingRequests;
        xrtMutexUnlock(&S->Mutex);
        if(Stop){
            if(!xcronExecutorDestroy(S->Bridge,NULL)){xcronServiceFault(S,XCRON_SERVICE_EXECUTOR);continue;}S->Bridge=NULL;
            /* Publish actual retained allocations. Other executor fields remain
             * the final lifetime counters/configuration, not a live object. */
            xrtMutexLock(&S->Mutex);S->Stats.AdapterBytes=0;S->Stats.Executor.Bytes=0;
            S->Stats.Bytes=S->ArenaBytes+S->Stats.KernelBytes;xrtMutexUnlock(&S->Mutex);
            if(!xcronServiceWaiterRelease(S))xcronServiceFault(S,XCRON_SERVICE_SYSTEM);
            xrtMutexLock(&S->Mutex);S->Stopped=true;S->Stats.State=S->Stats.Failure.Code==XCRON_SERVICE_OK?XCRON_SERVICE_STOPPED:XCRON_SERVICE_FAILED;xrtCondBroadcast(&S->Changed);xrtMutexUnlock(&S->Mutex);return 0;
        }
        uint64 Delay=S->Config.ResampleMilliseconds;
        if(!Closing && S->HasClock){xcronwake W;
            if(!xcronSchedulerWake(S->Kernel,S->Now,Delay,&W,NULL)){xcronServiceFault(S,XCRON_SERVICE_KERNEL);continue;}
            if(Capacity && W.DelayMilliseconds<Delay)Delay=W.DelayMilliseconds;
            if(S->RetryCount){xcronreservation* R=&S->Reservations[S->RetryHeap[0]];uint64 At=R->RetryAt<R->Deadline?R->RetryAt:R->Deadline;
                uint64 Wait=At<=S->Now.MonotonicMilliseconds?0:At-S->Now.MonotonicMilliseconds;if(Wait<Delay)Delay=Wait;}
        }
        if(Commands || S->AdmissionCount || Searching || Completed==S->Config.BatchLimit || (More && Capacity))Delay=0;
        /* The auto-reset event is never reset after the work check. */
        if(xrtEventWaiterWaitFor(&S->Waiter,(int64)Delay)==XWAIT_ERROR)xcronServiceFault(S,XCRON_SERVICE_SYSTEM);
    }
}
static bool xcronServiceArray(size_t Count,size_t Unit,size_t Align,size_t* Total,size_t* Offset)
{
    size_t Padding=(Align-*Total%Align)%Align;if(Padding>SIZE_MAX-*Total)return false;*Total+=Padding;*Offset=*Total;
    if(Count>(SIZE_MAX-*Total)/Unit)return false;
    *Total+=Count*Unit;return true;
}
XCRON_API bool xcronServiceCreate(const xcronserviceconfig* C,xcronservice** Out,xcronserviceissue* Issue)
{
    void* O[]={Out,Issue};size_t N[]={sizeof(*Out),Issue?sizeof(*Issue):0};
    if(!C || !Out || !xcronServiceOutputs(NULL,C,sizeof(*C),O,N,2) || !C->Executor || !C->Proc || C->Reserved || C->Scheduler.Reserved ||
        !C->Scheduler.JobLimit || C->Scheduler.JobLimit>UINT32_C(1073741824) || !C->Scheduler.RunLimit || C->Scheduler.RunLimit>UINT32_C(1073741824) || !C->Scheduler.WorkLimit ||
        !C->CommandLimit || C->CommandLimit>UINT32_C(1073741824) || !C->CompletionLimit || C->CompletionLimit>UINT32_C(1073741824) ||
        !C->BatchLimit || C->BatchLimit>C->CompletionLimit || C->BatchLimit>C->Scheduler.RunLimit || !C->CommandWorkLimit || !C->ResampleMilliseconds || !C->RetryIntervalMilliseconds || !C->RetryAttempts)
        return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    size_t Bytes=sizeof(xcronservice),Offsets[7];
    if(!xcronServiceArray(C->CommandLimit,sizeof(xcroncommand),_Alignof(xcroncommand),&Bytes,&Offsets[0]) ||
        !xcronServiceArray(C->CompletionLimit,sizeof(xcronreservation),_Alignof(xcronreservation),&Bytes,&Offsets[1]) ||
        !xcronServiceArray(C->Scheduler.RunLimit,sizeof(uint32),_Alignof(uint32),&Bytes,&Offsets[2]) ||
        !xcronServiceArray(C->CompletionLimit,sizeof(uint32),_Alignof(uint32),&Bytes,&Offsets[3]) ||
        !xcronServiceArray(C->BatchLimit,sizeof(uint32),_Alignof(uint32),&Bytes,&Offsets[4]) ||
        !xcronServiceArray(C->BatchLimit,sizeof(xcronrun),_Alignof(xcronrun),&Bytes,&Offsets[5]) ||
        !xcronServiceArray(C->BatchLimit,sizeof(xcroncompletion),_Alignof(xcroncompletion),&Bytes,&Offsets[6]) ||
        Bytes>C->BytesLimit || C->Scheduler.BytesLimit>C->BytesLimit-Bytes || C->AdapterBytesLimit>C->BytesLimit-Bytes-C->Scheduler.BytesLimit)
        return xcronServiceFail(XCRON_SERVICE_LIMIT,XERR_NONE,Issue);
    xallocationscope Scope={0};bool Budget=false;
    if(C->InitializationBudget){Budget=xrtAllocationBudgetEnter(C->InitializationBudget,&Scope);if(!Budget)return xcronServiceFail(XCRON_SERVICE_MEMORY,xrtErrorKind(xrtGetError()),Issue);}
    xcronservice* S=xrtMalloc(Bytes);
    if(!S){if(Budget)xrtAllocationBudgetLeave(&Scope);return xcronServiceFail(XCRON_SERVICE_MEMORY,XERR_MEMORY,Issue);}
    memset(S,0,Bytes);S->Magic=XCRON_SERVICE_MAGIC;S->ArenaBytes=Bytes;S->Config=*C;S->Stats.ArenaBytes=Bytes;S->Stats.Bytes=Bytes;
    S->Commands=(xcroncommand*)((unsigned char*)S+Offsets[0]);S->Reservations=(xcronreservation*)((unsigned char*)S+Offsets[1]);S->ActiveMap=(uint32*)((unsigned char*)S+Offsets[2]);S->RetryHeap=(uint32*)((unsigned char*)S+Offsets[3]);S->ReservedBatch=(uint32*)((unsigned char*)S+Offsets[4]);S->RunBatch=(xcronrun*)((unsigned char*)S+Offsets[5]);S->DoneBatch=(xcroncompletion*)((unsigned char*)S+Offsets[6]);
    S->CommandHead=S->CommandTail=S->DoneHead=S->DoneTail=S->AdmissionHead=S->AdmissionTail=XCRON_SERVICE_NONE;S->CommandBits=xcronServiceBits(C->CommandLimit);S->RunBits=xcronServiceBits(C->Scheduler.RunLimit);xrtAtomic64Init(&S->WakeFailures,0);
    for(uint32 I=0;I<C->CommandLimit;++I){S->Commands[I].Generation=1;S->Commands[I].Next=I+1<C->CommandLimit?I+1:XCRON_SERVICE_NONE;}
    for(uint32 I=0;I<C->CompletionLimit;++I){S->Reservations[I].Next=I+1<C->CompletionLimit?I+1:XCRON_SERVICE_NONE;S->Reservations[I].HeapPosition=XCRON_SERVICE_NONE;}
    for(uint32 I=0;I<C->Scheduler.RunLimit;++I)S->ActiveMap[I]=XCRON_SERVICE_NONE;
    bool M=xrtMutexInit(&S->Mutex),D=false,W=false;
    if(M)D=xrtCondInit(&S->Changed);
    if(D)W=xrtEventInit(&S->Wake,false,false);
    if(W)S->Thread=xrtThreadCreate(xcronServiceOwner,S,C->StackSize);
    xcronserviceissue Failure={XCRON_SERVICE_SYSTEM,XERR_NONE};
    if(S->Thread){xrtMutexLock(&S->Mutex);while(!S->Ready)xrtCondWaitFor(&S->Changed,&S->Mutex,100);
        if(S->Startup.Code==XCRON_SERVICE_OK){*Out=S;S->Activated=true;xrtCondBroadcast(&S->Changed);xrtMutexUnlock(&S->Mutex);if(Budget)xrtAllocationBudgetLeave(&Scope);xcronServiceOK(Issue);return true;}
        Failure=S->Startup;xrtMutexUnlock(&S->Mutex);xrtThreadWait(S->Thread);xrtThreadDestroy(S->Thread);xrtSetErrorTake(S->StartupError);
    }else Failure.Cause=xrtErrorKind(xrtGetError());
    xerror* Error=xrtTakeError();if(W)xrtEventUnit(&S->Wake);if(D)xrtCondUnit(&S->Changed);if(M)xrtMutexUnit(&S->Mutex);xrtFree(S);if(Budget)xrtAllocationBudgetLeave(&Scope);xrtSetErrorTake(Error);
    return xcronServiceFail(Failure.Code,Failure.Cause,Issue);
}
static bool xcronServiceEnqueue(xcronservice* S,xcronrequestkind Kind,const xcronjobconfig* Add,xcronjobid Job,bool Cancel,xcronrequestid* Request,xcronserviceissue* Issue)
{
    if(!xrtMutexLock(&S->Mutex))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    xcronserviceerror Code=S->Closing?XCRON_SERVICE_CLOSED:S->FreeCommand==XCRON_SERVICE_NONE?XCRON_SERVICE_LIMIT:XCRON_SERVICE_OK;
    if(Code!=XCRON_SERVICE_OK){xrtMutexUnlock(&S->Mutex);return xcronServiceFail(Code,XERR_NONE,Issue);}
    if(Add && !xcronZoneRetain(Add->Zone)){xrtMutexUnlock(&S->Mutex);return xcronServiceFail(XCRON_SERVICE_LIMIT,xrtErrorKind(xrtGetError()),Issue);}
    uint32 I=S->FreeCommand;xcroncommand* C=&S->Commands[I];S->FreeCommand=C->Next;
    C->State=XCRON_CMD_QUEUED;C->Next=XCRON_SERVICE_NONE;C->Job=Job;C->Cancel=Cancel;if(Add)C->Add=*Add;
    C->Reply=(xcronreply){.Request=C->Generation<<S->CommandBits|I,.Kind=Kind};
    if(S->CommandTail==XCRON_SERVICE_NONE)S->CommandHead=I;else S->Commands[S->CommandTail].Next=I;S->CommandTail=I;++S->Stats.PendingRequests;
    *Request=C->Reply.Request;xrtMutexUnlock(&S->Mutex);xcronServiceSignal(S);xcronServiceOK(Issue);return true;
}
XCRON_API bool xcronServiceAddAsync(xcronservice* S,const xcronjobconfig* C,xcronrequestid* Request,xcronserviceissue* Issue)
{
    void* O[]={Request,Issue};size_t N[]={sizeof(*Request),Issue?sizeof(*Issue):0};
    if(!S || !C || !Request || !xcronServiceOutputs(S,C,sizeof(*C),O,N,2) || !C->Zone || !xcronZoneOutputs(C->Zone,C,sizeof(*C),O,N,2) ||
        (C->SearchFlags&~XCRON_SEARCH_EXCLUSIVE) || C->Begin>C->End || C->Misfire<XCRON_MISFIRE_CATCH_UP || C->Misfire>XCRON_MISFIRE_COALESCE ||
        C->Overlap<XCRON_OVERLAP_WAIT || C->Overlap>XCRON_OVERLAP_SKIP || !C->RunningLimit || C->RunningLimit>S->Config.Scheduler.RunLimit || (C->Overlap!=XCRON_OVERLAP_ALLOW && C->RunningLimit!=1))
        return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    return xcronServiceEnqueue(S,XCRON_REQUEST_ADD,C,0,false,Request,Issue);
}
XCRON_API bool xcronServiceCancelAsync(xcronservice* S,xcronjobid Job,bool Cancel,xcronrequestid* Request,xcronserviceissue* Issue)
{
    if(!Job || !Request || !xcronServiceArgs(S,Request,sizeof(*Request),Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    return xcronServiceEnqueue(S,XCRON_REQUEST_CANCEL,NULL,Job,Cancel,Request,Issue);
}
XCRON_API bool xcronServiceInfoAsync(xcronservice* S,xcronjobid Job,xcronrequestid* Request,xcronserviceissue* Issue)
{
    if(!Job || !Request || !xcronServiceArgs(S,Request,sizeof(*Request),Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    return xcronServiceEnqueue(S,XCRON_REQUEST_INFO,NULL,Job,false,Request,Issue);
}
XCRON_API bool xcronServiceReapAsync(xcronservice* S,xcronrequestid* Request,xcronserviceissue* Issue)
{
    if(!Request || !xcronServiceArgs(S,Request,sizeof(*Request),Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    return xcronServiceEnqueue(S,XCRON_REQUEST_REAP,NULL,0,false,Request,Issue);
}
static bool xcronServiceCurrent(const xcronservice* S)
{return xrtThreadCurrentId()==S->Owner || xrtExecutorIsCurrent(S->Config.Executor);}
XCRON_API xwaitresult xcronServiceCollect(xcronservice* S,xcronrequestid Id,int64 Timeout,xcronreply* Reply,xcronserviceissue* Issue)
{
    if(!Reply || Timeout<0 || !xcronServiceArgs(S,Reply,sizeof(*Reply),Issue)){xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);return XWAIT_ERROR;}
    double Start=xrtTimer();if(!xrtMutexLock(&S->Mutex)){xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);return XWAIT_ERROR;}
    for(;;){uint32 I=(uint32)(Id&((UINT64_C(1)<<S->CommandBits)-1));xcroncommand* C=I<S->Config.CommandLimit?&S->Commands[I]:NULL;
        if(!Id || !C || C->State==XCRON_CMD_FREE || C->Generation!=(Id>>S->CommandBits)){xrtMutexUnlock(&S->Mutex);xcronServiceFail(XCRON_SERVICE_STALE,XERR_NONE,Issue);return XWAIT_ERROR;}
        if(C->State==XCRON_CMD_READY){*Reply=C->Reply;C->Reply.Error=NULL;C->State=XCRON_CMD_FREE;--S->Stats.ReadyReplies;
            if(C->Generation<(UINT64_MAX>>S->CommandBits)){++C->Generation;C->Next=S->FreeCommand;S->FreeCommand=I;}
            xrtCondBroadcast(&S->Changed);xrtMutexUnlock(&S->Mutex);xcronServiceOK(Issue);return XWAIT_OK;}
        if(!Timeout){xrtMutexUnlock(&S->Mutex);xcronServiceOK(Issue);return XWAIT_TIMEOUT;}
        if(xcronServiceCurrent(S)){xrtMutexUnlock(&S->Mutex);xcronServiceFail(XCRON_SERVICE_THREAD,XERR_NONE,Issue);return XWAIT_ERROR;}
        double Used=(xrtTimer()-Start)*1000.0;
        if(Used>=(double)Timeout){xrtMutexUnlock(&S->Mutex);xcronServiceOK(Issue);return XWAIT_TIMEOUT;}
        double Remaining=(double)Timeout-Used;int64 Wait=Remaining>1000.0?1000:(int64)ceil(Remaining);if(Wait<1)Wait=1;
        xwaitresult R=xrtCondWaitFor(&S->Changed,&S->Mutex,Wait);
        if(R==XWAIT_ERROR){xrtMutexUnlock(&S->Mutex);xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);return XWAIT_ERROR;}
    }
}
XCRON_API bool xcronServiceDrain(xcronservice* S,xcronservicecompletion* Completed,size_t Capacity,size_t* Count,xcronserviceissue* Issue)
{
    void* O[]={Completed,Count,Issue};size_t N[]={Capacity<=SIZE_MAX/sizeof(*Completed)?Capacity*sizeof(*Completed):SIZE_MAX,sizeof(*Count),Issue?sizeof(*Issue):0};
    if(!S || !Completed || !Count || !Capacity || Capacity>SIZE_MAX/sizeof(*Completed) || !xcronServiceOutputs(S,NULL,0,O,N,3))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    if(!xrtMutexLock(&S->Mutex))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    size_t Written=0;while(Written<Capacity && S->DoneHead!=XCRON_SERVICE_NONE){uint32 I=S->DoneHead;xcronreservation* R=&S->Reservations[I];S->DoneHead=R->Next;
        if(S->DoneHead==XCRON_SERVICE_NONE)S->DoneTail=XCRON_SERVICE_NONE;
        Completed[Written++]=R->Done;R->Done.Error=NULL;R->Next=S->FreeCompletion;S->FreeCompletion=I;--S->Stats.ReadyCompletions;--S->Stats.ReservedCompletions;xcronServiceIncrement(&S->Stats.Collected);}
    *Count=Written;xrtMutexUnlock(&S->Mutex);if(Written)xcronServiceSignal(S);xcronServiceOK(Issue);return true;
}
XCRON_API bool xcronServiceClose(xcronservice* S,bool Cancel,xcronserviceissue* Issue)
{
    if(!xcronServiceArgs(S,NULL,0,Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    if(!xrtMutexLock(&S->Mutex))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    S->Closing=true;if(Cancel)S->CancelRuns=true;if(!S->Stopped)S->Stats.State=XCRON_SERVICE_STOPPING;xrtMutexUnlock(&S->Mutex);xcronServiceSignal(S);xcronServiceOK(Issue);return true;
}
XCRON_API xwaitresult xcronServiceWaitFor(xcronservice* S,int64 Timeout,xcronserviceissue* Issue)
{
    if(Timeout<0 || !xcronServiceArgs(S,NULL,0,Issue)){xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);return XWAIT_ERROR;}
    if(Timeout && xcronServiceCurrent(S)){xcronServiceFail(XCRON_SERVICE_THREAD,XERR_NONE,Issue);return XWAIT_ERROR;}
    xwaitresult R=xrtThreadWaitFor(S->Thread,Timeout);
    if(R==XWAIT_ERROR)xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);else xcronServiceOK(Issue);return R;
}
XCRON_API bool xcronServiceReapStopped(xcronservice* S,xcronretired* Retired,bool* Found,xcronserviceissue* Issue)
{
    void* O[]={Retired,Found,Issue};size_t N[]={sizeof(*Retired),sizeof(*Found),Issue?sizeof(*Issue):0};
    if(!S || !Retired || !Found || !xcronServiceOutputs(S,NULL,0,O,N,3))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    if(!xrtMutexLock(&S->Mutex))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    if(!S->Stopped){xrtMutexUnlock(&S->Mutex);return xcronServiceFail(XCRON_SERVICE_BUSY,XERR_NONE,Issue);}
    bool OK=xcronSchedulerReap(S->Kernel,Retired,Found,NULL);if(OK)xcronSchedulerInspect(S->Kernel,&S->Stats.Kernel,NULL);xrtMutexUnlock(&S->Mutex);
    if(!OK)return xcronServiceFail(XCRON_SERVICE_KERNEL,xrtErrorKind(xrtGetError()),Issue);
    xcronServiceOK(Issue);return true;
}
XCRON_API bool xcronServiceInspect(xcronservice* S,xcronservicestats* Stats,xcronserviceissue* Issue)
{
    if(!Stats || !xcronServiceArgs(S,Stats,sizeof(*Stats),Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    if(!xrtMutexLock(&S->Mutex))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    *Stats=S->Stats;Stats->WakeFailures=xrtAtomic64Load(&S->WakeFailures,XMEMORY_RELAXED);xrtMutexUnlock(&S->Mutex);xcronServiceOK(Issue);return true;
}
XCRON_API bool xcronServiceFailure(xcronservice* S,xerror** Error,xcronserviceissue* Issue)
{
    if(!Error || !xcronServiceArgs(S,Error,sizeof(*Error),Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    if(!xrtMutexLock(&S->Mutex))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    *Error=xrtErrorRef(S->FatalError);xrtMutexUnlock(&S->Mutex);xcronServiceOK(Issue);return true;
}
XCRON_API bool xcronServiceWake(xcronservice* S,xcronserviceissue* Issue)
{
    if(!xcronServiceArgs(S,NULL,0,Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    xcronServiceSignal(S);xcronServiceOK(Issue);return true;
}
XCRON_API bool xcronServiceDestroy(xcronservice* S,xcronserviceissue* Issue)
{
    if(!xcronServiceArgs(S,NULL,0,Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    xrtMutexLock(&S->Mutex);bool Busy=!S->Stopped || S->Stats.Kernel.Jobs || S->Stats.PendingRequests || S->Stats.ReadyReplies || S->Stats.ReservedCompletions;xrtMutexUnlock(&S->Mutex);
    if(Busy)return xcronServiceFail(XCRON_SERVICE_BUSY,XERR_NONE,Issue);
    if(xrtThreadWaitFor(S->Thread,0)!=XWAIT_OK)return xcronServiceFail(XCRON_SERVICE_BUSY,XERR_NONE,Issue);
    if(!xcronServiceWaiterRelease(S))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    if(!xcronSchedulerDestroy(S->Kernel,NULL))return xcronServiceFail(XCRON_SERVICE_KERNEL,xrtErrorKind(xrtGetError()),Issue);
    xrtThreadDestroy(S->Thread);xrtErrorFree(S->FatalError);xrtEventUnit(&S->Wake);xrtCondUnit(&S->Changed);xrtMutexUnit(&S->Mutex);S->Magic=0;xrtFree(S);xcronServiceOK(Issue);return true;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xcron/src/tzdb/tzdb.c */
/* ========================================================================== */

#if defined(XCRON_FEATURE_XCRON_TZDB)
#include <string.h>
#if defined(XCRON_FEATURE_XCRON_TZDB)
static xcrontzdbissue xcronTzdbWork(xcrontzdberror Code,size_t Index)
{return (xcrontzdbissue){.Code=Code,.Index=Index};}
static bool xcronTzdbReport(xcrontzdbissue Work,xcrontzdbissue* Issue)
{
    if(Issue)*Issue=Work;
    xerrkind Kind=Work.Code==XCRON_TZDB_ARGUMENT?XERR_ARGUMENT:Work.Code==XCRON_TZDB_MEMORY?XERR_MEMORY:
        Work.Code==XCRON_TZDB_LIMIT?XERR_RANGE:Work.Code==XCRON_TZDB_IO?XERR_IO:
        (Work.Code==XCRON_TZDB_NAME || Work.Code==XCRON_TZDB_VERSION || Work.Code==XCRON_TZDB_DUPLICATE || Work.Code==XCRON_TZDB_ZONE)?XERR_VALUE:XERR_STATE;
    xrtSetErrorInfo(Kind,"xrt.cron.tzdb",Work.Code,"cron named timezone database operation failed");return false;
}
static bool xcronTzdbFail(xcrontzdberror Code,xcrontzdbissue* Issue)
{return xcronTzdbReport(xcronTzdbWork(Code,XCRON_TZDB_NO_INDEX),Issue);}
static void xcronTzdbOK(xcrontzdbissue* Issue)
{if(Issue)*Issue=xcronTzdbWork(XCRON_TZDB_OK,XCRON_TZDB_NO_INDEX);}
static bool xcronTzdbOutputs(const void* Owner,size_t Bytes,const void* Input,size_t InputBytes,void* const* O,const size_t* N,size_t Count)
{
    if(!xrtMemRangeValid(Owner,Bytes) || !xrtMemRangeValid(Input,InputBytes))return false;
    for(size_t I=0;I<Count;++I){
        if(!xrtMemRangeValid(O[I],N[I]) || xrtMemRangesOverlap(Owner,Bytes,O[I],N[I]) || xrtMemRangesOverlap(Input,InputBytes,O[I],N[I]))return false;
        for(size_t J=0;J<I;++J)if(xrtMemRangesOverlap(O[I],N[I],O[J],N[J]))return false;
    }
    return true;
}
static bool xcronTzdbHandle(const xcrontzdb* D)
{return D && xrtMemRangeValid(D,sizeof(*D)) && D->Magic==XCRON_TZDB_MAGIC;}
static bool xcronTzdbSnapshotHandle(const xcrontzdbsnapshot* S)
{return S && xrtMemRangeValid(S,sizeof(*S)) && S->Magic==XCRON_TZDB_SNAPSHOT_MAGIC && xrtMemRangeValid(S,S->ArenaBytes);}
static bool xcronTzdbArgs(const xcrontzdb* D,void* Output,size_t Bytes,xcrontzdbissue* Issue)
{void* O[]={Output,Issue};size_t N[]={Bytes,Issue?sizeof(*Issue):0};return xcronTzdbHandle(D) && xcronTzdbOutputs(D,sizeof(*D),NULL,0,O,N,2);}
static bool xcronTzdbName(xstrview Name)
{
    if(!Name.Size || Name.Size>XCRON_TZDB_NAME_MAX)return false;
    size_t Start=0;
    for(size_t I=0;I<=Name.Size;++I){
        unsigned C=I==Name.Size?'/':(unsigned char)Name.Data[I];
        if(C=='/'){
            if(I==Start || Name.Data[Start]=='-' || Name.Data[I-1]=='.' ||
                (I-Start==1 && Name.Data[Start]=='.') || (I-Start==2 && Name.Data[Start]=='.' && Name.Data[Start+1]=='.'))return false;
            Start=I+1;
        }
        else if(!((C>='A' && C<='Z') || (C>='a' && C<='z') || (C>='0' && C<='9') || C=='_' || C=='.' || C=='-' || C=='+'))return false;
    }
    return true;
}
static bool xcronTzdbVersion(xstrview V)
{if(!V.Size || V.Size>XCRON_TZDB_VERSION_MAX)return false;for(size_t I=0;I<V.Size;++I)if((unsigned char)V.Data[I]<0x21 || (unsigned char)V.Data[I]>0x7e)return false;return true;}
static int xcronTzdbCompare(xstrview A,xstrview B)
{size_t N=A.Size<B.Size?A.Size:B.Size;int C=memcmp(A.Data,B.Data,N);return C?C:(A.Size>B.Size)-(A.Size<B.Size);}
static void xcronTzdbSwap(xcrontzdbrow* A,xcrontzdbrow* B)
{xcrontzdbrow T=*A;*A=*B;*B=T;}
static void xcronTzdbDown(xcrontzdbrow* Rows,size_t Count,size_t At)
{
    while(At<Count/2){size_t Child=At*2+1;
        if(Child+1<Count && xcronTzdbCompare(Rows[Child].Name,Rows[Child+1].Name)<0)++Child;
        if(xcronTzdbCompare(Rows[At].Name,Rows[Child].Name)>=0)break;
        xcronTzdbSwap(&Rows[At],&Rows[Child]);At=Child;
    }
}
static void xcronTzdbSort(xcrontzdbsnapshot* S)
{for(size_t I=S->Count/2;I;--I)xcronTzdbDown(S->Rows,S->Count,I-1);for(size_t I=S->Count;I>1;--I){xcronTzdbSwap(&S->Rows[0],&S->Rows[I-1]);xcronTzdbDown(S->Rows,I-1,0);}}
static bool xcronTzdbRetainValue(xcrontzdbsnapshot* S)
{uint32 N=xrtAtomic32Load(&S->Refs,XMEMORY_RELAXED);while(N && N<UINT32_MAX/2u)if(xrtAtomic32CompareExchange(&S->Refs,&N,N+1,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;return false;}
XCRON_API bool xcronTzdbSnapshotRetain(const xcrontzdbsnapshot* S)
{if(!xcronTzdbSnapshotHandle(S))return xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);return xcronTzdbRetainValue((xcrontzdbsnapshot*)S) || xcronTzdbFail(XCRON_TZDB_LIMIT,NULL);}
XCRON_API void xcronTzdbSnapshotRelease(const xcrontzdbsnapshot* Raw)
{
    xcrontzdbsnapshot* S=(xcrontzdbsnapshot*)Raw;
    if(S && xrtAtomic32FetchSub(&S->Refs,1,XMEMORY_ACQ_REL)==1){S->Magic=0;for(size_t I=0;I<S->Count;++I)xcronZoneRelease(S->Rows[I].Zone);xrtFree(S);}
}
XCRON_API bool xcronTzdbCreate(const xcrontzdbconfig* C,xcrontzdb** Out,xcrontzdbissue* Issue)
{
    void* O[]={Out,Issue};size_t N[]={sizeof(*Out),Issue?sizeof(*Issue):0};
    if(!C || !Out || !xcronTzdbOutputs(NULL,0,C,sizeof(*C),O,N,2) || !C->NamesLimit || C->NamesLimit>XCRON_TZDB_NAMES_MAX || !C->LoadLimit || C->LoadLimit>XCRON_TZDB_LOADS_MAX || !C->BytesLimit)return xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);
    if(C->BytesLimit<sizeof(xcrontzdb))return xcronTzdbFail(XCRON_TZDB_LIMIT,Issue);
    xallocationbudget* Budget=NULL;if(!xrtAllocationBudgetCreate(C->BytesLimit,&Budget))return xcronTzdbFail(XCRON_TZDB_MEMORY,Issue);
    xallocationscope Scope={0};if(!xrtAllocationBudgetEnter(Budget,&Scope)){xrtAllocationBudgetFree(Budget);return xcronTzdbFail(XCRON_TZDB_MEMORY,Issue);}
    xcrontzdb* D=xrtMalloc(sizeof(*D));if(D){memset(D,0,sizeof(*D));D->Config=*C;D->Budget=Budget;xrtAtomic32Init(&D->Closed,0);}
    xcrontzdberror Code=D?XCRON_TZDB_OK:Scope.Rejections?XCRON_TZDB_LIMIT:XCRON_TZDB_MEMORY;
    if(D && !xrtMutexInit(&D->Mutex)){xrtFree(D);D=NULL;Code=XCRON_TZDB_SYSTEM;}
    (void)xrtAllocationBudgetLeave(&Scope);
    if(!D){xrtAllocationBudgetFree(Budget);return xcronTzdbFail(Code,Issue);}
    D->Magic=XCRON_TZDB_MAGIC;*Out=D;xcronTzdbOK(Issue);return true;
}
static xstrview xcronTzdbSourceName(const xcrontzdbsource* S,size_t I)
{return S->Entries?S->Entries[I].Name:S->Names[I];}
bool xcronTzdbLoadSource(xcrontzdb* D,xstrview Version,const xcrontzdbsource* Source,uint64 Expected,uint64* Revision,xcrontzdbissue* Issue)
{
    void* O[]={Revision,Issue};size_t N[]={sizeof(*Revision),Issue?sizeof(*Issue):0};size_t Count=Source->Count;
    size_t Stride=Source->Entries?sizeof(*Source->Entries):sizeof(*Source->Names);const void* Items=Source->Entries?(const void*)Source->Entries:(const void*)Source->Names;
    if(!xcronTzdbHandle(D) || !Revision || Count>XCRON_TZDB_NAMES_MAX || !xcronTzdbOutputs(D,sizeof(*D),Items,Count*Stride,O,N,2) || !xcronTzdbOutputs(NULL,0,Version.Data,Version.Size,O,N,2))return xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);
    for(size_t I=0;I<Count;++I){xstrview Name=xcronTzdbSourceName(Source,I);
        if(!xcronTzdbOutputs(NULL,0,Name.Data,Name.Size,O,N,2) || (Source->Entries && !xcronTzdbOutputs(NULL,0,Source->Entries[I].Tzif.Data,Source->Entries[I].Tzif.Size,O,N,2)))return xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);}
    if(Count>D->Config.NamesLimit)return xcronTzdbFail(XCRON_TZDB_LIMIT,Issue);
    if(!xcronTzdbVersion(Version))return xcronTzdbFail(XCRON_TZDB_VERSION,Issue);
    size_t Bytes=sizeof(xcrontzdbsnapshot)+Count*sizeof(xcrontzdbrow)+Version.Size+1;
    for(size_t I=0;I<Count;++I){xstrview Name=xcronTzdbSourceName(Source,I);if(!xcronTzdbName(Name))return xcronTzdbReport(xcronTzdbWork(XCRON_TZDB_NAME,I),Issue);Bytes+=Name.Size+1;}
    if(!xrtMutexLock(&D->Mutex))return xcronTzdbFail(XCRON_TZDB_SYSTEM,Issue);
    xcrontzdberror Code=xrtAtomic32Load(&D->Closed,XMEMORY_RELAXED)?XCRON_TZDB_CLOSED:
        (Expected!=XCRON_TZDB_ANY_REVISION && Expected!=D->Revision)?XCRON_TZDB_STALE:
        (D->Loading>=D->Config.LoadLimit || D->Revision==UINT64_MAX-1)?XCRON_TZDB_LIMIT:XCRON_TZDB_OK;
    if(Code==XCRON_TZDB_OK)++D->Loading;
    (void)xrtMutexUnlock(&D->Mutex);if(Code!=XCRON_TZDB_OK)return xcronTzdbFail(Code,Issue);
    xerror* Ambient=xrtTakeError();
    xcrontzdbissue Work=xcronTzdbWork(XCRON_TZDB_OK,XCRON_TZDB_NO_INDEX);xallocationscope Scope={0};xcrontzdbsnapshot *S=NULL,*Old=NULL;uint64 Published=0;
    bool Entered=xrtAllocationBudgetEnter(D->Budget,&Scope);
    if(!Entered)Work.Code=XCRON_TZDB_MEMORY;
    else {
        S=xrtMalloc(Bytes);
        if(!S)Work.Code=XCRON_TZDB_MEMORY;
        else {memset(S,0,Bytes);S->Magic=XCRON_TZDB_SNAPSHOT_MAGIC;xrtAtomic32Init(&S->Refs,1);S->ArenaBytes=S->Bytes=Bytes;S->Count=Count;
            char* Text=(char*)(S->Rows+Count);memcpy(Text,Version.Data,Version.Size);S->Version=(xstrview){Text,Version.Size};Text+=Version.Size+1;
            for(size_t I=0;I<Count;++I){xstrview Name=xcronTzdbSourceName(Source,I);memcpy(Text,Name.Data,Name.Size);S->Rows[I].Name=(xstrview){Text,Name.Size};S->Rows[I].Index=(uint32)I;Text+=Name.Size+1;}
            xcronTzdbSort(S);
            for(size_t I=1;I<Count;++I)if(!xcronTzdbCompare(S->Rows[I-1].Name,S->Rows[I].Name)){Work=xcronTzdbWork(XCRON_TZDB_DUPLICATE,S->Rows[I].Index);break;}
            for(size_t I=0;I<Count && Work.Code==XCRON_TZDB_OK;++I){
                size_t Index=S->Rows[I].Index;xbytesview Data={0};ptr Owned=NULL;Work.Zone=(xcronzoneissue){0};Work.Cause=XERR_NONE;Work.SystemCode=0;
                if(xrtAtomic32Load(&D->Closed,XMEMORY_ACQUIRE)){Work=xcronTzdbWork(XCRON_TZDB_CLOSED,Index);break;}
                bool Read=true;if(Source->Entries)Data=Source->Entries[Index].Tzif;else Read=Source->Read(Source->Context,S->Rows[I].Name,&Data,&Owned,&Work);
                if(Read){Work.Zone=(xcronzoneissue){0};if(!xcronZoneCreateTzif(Data,&S->Rows[I].Zone,&Work.Zone))Work.Code=XCRON_TZDB_ZONE;
                    else S->Bytes+=xcronZoneOwnedBytes(S->Rows[I].Zone);}
                xrtFree(Owned);if(Work.Code!=XCRON_TZDB_OK)Work.Index=Index;
            }
        }
    }
    if(Entered && Scope.Rejections && (Work.Code==XCRON_TZDB_MEMORY || (Work.Code==XCRON_TZDB_ZONE && Work.Zone.Code==XCRON_ZONE_MEMORY))){Work.Code=XCRON_TZDB_LIMIT;Work.Cause=XERR_MEMORY;}
    if(Work.Code==XCRON_TZDB_OK){
        if(!xrtMutexLock(&D->Mutex))Work.Code=XCRON_TZDB_SYSTEM;
        else {
            Work.Code=xrtAtomic32Load(&D->Closed,XMEMORY_RELAXED)?XCRON_TZDB_CLOSED:
                (Expected!=XCRON_TZDB_ANY_REVISION && Expected!=D->Revision)?XCRON_TZDB_STALE:D->Revision==UINT64_MAX-1?XCRON_TZDB_LIMIT:XCRON_TZDB_OK;
            if(Work.Code==XCRON_TZDB_OK){Published=++D->Revision;S->Revision=Published;Old=D->Current;D->Current=S;S=NULL;}
            (void)xrtMutexUnlock(&D->Mutex);
        }
    }
    xcronTzdbSnapshotRelease(S);xcronTzdbSnapshotRelease(Old);
    if(Entered)(void)xrtAllocationBudgetLeave(&Scope);
    (void)xrtMutexLock(&D->Mutex);--D->Loading;(void)xrtMutexUnlock(&D->Mutex);
    if(Work.Code!=XCRON_TZDB_OK){xrtErrorFree(Ambient);return xcronTzdbReport(Work,Issue);}
    xrtSetErrorTake(Ambient);*Revision=Published;xcronTzdbOK(Issue);return true;
}
XCRON_API bool xcronTzdbLoad(xcrontzdb* D,xstrview Version,const xcrontzdbentry* Entries,size_t Count,uint64 Expected,uint64* Revision,xcrontzdbissue* Issue)
{xcrontzdbsource Source={.Entries=Entries,.Count=Count};return xcronTzdbLoadSource(D,Version,&Source,Expected,Revision,Issue);}
XCRON_API bool xcronTzdbAcquire(xcrontzdb* D,xcrontzdbsnapshot** Out,xcrontzdbissue* Issue)
{
    if(!Out || !xcronTzdbArgs(D,Out,sizeof(*Out),Issue))return xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);
    if(!xrtMutexLock(&D->Mutex))return xcronTzdbFail(XCRON_TZDB_SYSTEM,Issue);
    xcrontzdberror Code=xrtAtomic32Load(&D->Closed,XMEMORY_RELAXED)?XCRON_TZDB_CLOSED:!D->Current?XCRON_TZDB_EMPTY:!xcronTzdbRetainValue(D->Current)?XCRON_TZDB_LIMIT:XCRON_TZDB_OK;
    xcrontzdbsnapshot* S=D->Current;(void)xrtMutexUnlock(&D->Mutex);
    if(Code!=XCRON_TZDB_OK)return xcronTzdbFail(Code,Issue);
    *Out=S;xcronTzdbOK(Issue);return true;
}
XCRON_API bool xcronTzdbSnapshotInfo(const xcrontzdbsnapshot* S,xcrontzdbsnapshotinfo* Info,xcrontzdbissue* Issue)
{
    void* O[]={Info,Issue};size_t N[]={sizeof(*Info),Issue?sizeof(*Issue):0};
    if(!Info || !xcronTzdbSnapshotHandle(S) || !xcronTzdbOutputs(S,S->ArenaBytes,NULL,0,O,N,2))return xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);
    *Info=(xcrontzdbsnapshotinfo){S->Revision,S->Count,S->Bytes,S->Version};xcronTzdbOK(Issue);return true;
}
XCRON_API xcrontzdbstatus xcronTzdbFind(const xcrontzdbsnapshot* S,xstrview Name,xcronzone** Zone,xcrontzdbissue* Issue)
{
    void* O[]={Zone,Issue};size_t N[]={sizeof(*Zone),Issue?sizeof(*Issue):0};
    if(!Zone || !xcronTzdbSnapshotHandle(S) || !xcronTzdbOutputs(S,S->ArenaBytes,Name.Data,Name.Size,O,N,2)){xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);return XCRON_TZDB_ERROR;}
    if(!xcronTzdbName(Name)){xcronTzdbFail(XCRON_TZDB_NAME,Issue);return XCRON_TZDB_ERROR;}
    size_t Low=0,High=S->Count;
    while(Low<High){size_t Mid=Low+(High-Low)/2;int C=xcronTzdbCompare(Name,S->Rows[Mid].Name);
        if(C<0)High=Mid;else if(C>0)Low=Mid+1;else {
            if(!xcronZoneOutputs(S->Rows[Mid].Zone,Name.Data,Name.Size,O,N,2)){xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);return XCRON_TZDB_ERROR;}
            if(!xcronZoneRetain(S->Rows[Mid].Zone)){xcronTzdbFail(XCRON_TZDB_LIMIT,Issue);return XCRON_TZDB_ERROR;}
            *Zone=S->Rows[Mid].Zone;xcronTzdbOK(Issue);return XCRON_TZDB_FOUND;
        }
    }
    xcronTzdbOK(Issue);return XCRON_TZDB_NONE;
}
XCRON_API bool xcronTzdbInspect(xcrontzdb* D,xcrontzdbstats* Stats,xcrontzdbissue* Issue)
{
    if(!Stats || !xcronTzdbArgs(D,Stats,sizeof(*Stats),Issue))return xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);
    if(!xrtMutexLock(&D->Mutex))return xcronTzdbFail(XCRON_TZDB_SYSTEM,Issue);
    xcrontzdbstats Value={.Revision=D->Revision,.Loading=D->Loading,.Names=D->Current?(uint32)D->Current->Count:0,.BytesLimit=D->Config.BytesLimit,.Closed=xrtAtomic32Load(&D->Closed,XMEMORY_RELAXED)!=0};
    xallocationbudgetinfo Q={.Size=sizeof(Q)};bool OK=xrtAllocationBudgetInfo(D->Budget,&Q);(void)xrtMutexUnlock(&D->Mutex);
    if(!OK)return xcronTzdbFail(XCRON_TZDB_SYSTEM,Issue);
    Value.LiveBytes=Q.LiveBytes;Value.PeakBytes=Q.PeakBytes;Value.LiveAllocations=Q.LiveCount;Value.Rejections=Q.Rejections;
    *Stats=Value;xcronTzdbOK(Issue);return true;
}
XCRON_API bool xcronTzdbClose(xcrontzdb* D,xcrontzdbissue* Issue)
{
    if(!xcronTzdbArgs(D,NULL,0,Issue))return xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);
    if(!xrtMutexLock(&D->Mutex))return xcronTzdbFail(XCRON_TZDB_SYSTEM,Issue);
    xrtAtomic32Store(&D->Closed,1,XMEMORY_RELEASE);xcrontzdbsnapshot* Old=D->Current;D->Current=NULL;(void)xrtMutexUnlock(&D->Mutex);
    xcronTzdbSnapshotRelease(Old);xcronTzdbOK(Issue);return true;
}
XCRON_API bool xcronTzdbDestroy(xcrontzdb* D,xcrontzdbissue* Issue)
{
    if(!xcronTzdbArgs(D,NULL,0,Issue))return xcronTzdbFail(XCRON_TZDB_ARGUMENT,NULL);
    if(!xrtMutexLock(&D->Mutex))return xcronTzdbFail(XCRON_TZDB_SYSTEM,Issue);
    bool Busy=!xrtAtomic32Load(&D->Closed,XMEMORY_RELAXED) || D->Loading;(void)xrtMutexUnlock(&D->Mutex);
    if(Busy)return xcronTzdbFail(XCRON_TZDB_BUSY,Issue);
    if(!xrtMutexUnit(&D->Mutex))return xcronTzdbFail(XCRON_TZDB_SYSTEM,Issue);
    xallocationbudget* Budget=D->Budget;D->Magic=0;xrtFree(D);xrtAllocationBudgetFree(Budget);xcronTzdbOK(Issue);return true;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xcron/src/tzdb_file/tzdb_file.c */
/* ========================================================================== */

#if defined(XCRON_FEATURE_XCRON_TZDB_FILE)
#if defined(XCRON_FEATURE_XCRON_TZDB_FILE)
static void xcronTzdbFileError(xcrontzdbissue* W,xcrontzdberror Code)
{W->Code=Code;W->Cause=xrtErrorKind(xrtGetError());W->SystemCode=xrtErrorSystemCode(xrtGetError());}
static bool xcronTzdbFileSame(const xfileinfo* A,const xfileinfo* B)
{
    if(A->Type!=B->Type || !(B->Available&XFILE_INFO_SIZE) || A->Size!=B->Size)return false;
    if((A->Available&XFILE_INFO_IDENTITY) && (!(B->Available&XFILE_INFO_IDENTITY) || A->Identity!=B->Identity || A->Device!=B->Device))return false;
    if((A->Available&XFILE_INFO_MODIFY_TIME) && (!(B->Available&XFILE_INFO_MODIFY_TIME) || A->Modified!=B->Modified))return false;
    if((A->Available&XFILE_INFO_CHANGE_TIME) && (!(B->Available&XFILE_INFO_CHANGE_TIME) || A->Changed!=B->Changed))return false;
    return true;
}
static bool xcronTzdbReadRoot(ptr Raw,xstrview Name,xbytesview* Data,ptr* Owned,xcrontzdbissue* Work)
{
    xroot Root=Raw;xfileoptions Options={XFILE_READ,0,XFILE_SHARE_READ|XFILE_SHARE_DELETE};
    xfile File=xrtRootFileOpen(Root,Name.Data,&Options);
    if(!File){xcronTzdbFileError(Work,XCRON_TZDB_IO);return false;}
    xfileinfo Before,After;bool OK=xrtFileStat(File,&Before);ptr Buffer=NULL;size_t Size=0;
    if(!OK)xcronTzdbFileError(Work,XCRON_TZDB_IO);
    else if(Before.Type!=XFILE_TYPE_FILE || !(Before.Available&XFILE_INFO_SIZE)){Work->Code=XCRON_TZDB_IO;OK=false;}
    else if(Before.Size>XCRON_ZONE_TZIF_MAX){Work->Code=XCRON_TZDB_LIMIT;OK=false;}
    else {
        Size=(size_t)Before.Size;Buffer=xrtMalloc(Size?Size:1);
        if(!Buffer){Work->Code=XCRON_TZDB_MEMORY;OK=false;}
        else if(!xrtReadFull(File,Buffer,Size,NULL)){xcronTzdbFileError(Work,XCRON_TZDB_IO);OK=false;}
        else {uint8 Extra;size_t Read=0;
            if(!xrtRead(File,&Extra,1,&Read) || !xrtFileStat(File,&After)){xcronTzdbFileError(Work,XCRON_TZDB_IO);OK=false;}
            else if(Read || !xcronTzdbFileSame(&Before,&After)){Work->Code=XCRON_TZDB_IO;OK=false;}
        }
    }
    xerror* Error=OK?NULL:xrtTakeError();bool Closed=xrtClose(File);
    if(!Closed && OK){xcronTzdbFileError(Work,XCRON_TZDB_IO);OK=false;}
    if(Error)xrtSetErrorTake(Error);
    if(!OK){xrtFree(Buffer);return false;}
    *Data=(xbytesview){Buffer,Size};*Owned=Buffer;return true;
}
XCRON_API bool xcronTzdbLoadRoot(xcrontzdb* D,xroot Root,xstrview Version,const xstrview* Names,size_t Count,uint64 Expected,uint64* Revision,xcrontzdbissue* Issue)
{
    if(!Root){xrtSetErrorInfo(XERR_ARGUMENT,"xrt.cron.tzdb",XCRON_TZDB_ARGUMENT,"cron tzdb requires a live explicit root");return false;}
    xcrontzdbsource Source={.Names=Names,.Count=Count,.Read=xcronTzdbReadRoot,.Context=Root};
    return xcronTzdbLoadSource(D,Version,&Source,Expected,Revision,Issue);
}
#endif
#endif

#endif
