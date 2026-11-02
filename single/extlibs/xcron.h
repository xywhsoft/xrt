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
	XCRON_CALENDAR_ERROR=-1, XCRON_CALENDAR_NONE=0, XCRON_CALENDAR_FOUND=1
} xcroncalendarstatus;

typedef enum xcroncalendarerror {
	XCRON_CALENDAR_OK=0, XCRON_CALENDAR_ARGUMENT=1001, XCRON_CALENDAR_EXPRESSION,
	XCRON_CALENDAR_DATE, XCRON_CALENDAR_RANGE, XCRON_CALENDAR_WINDOW
} xcroncalendarerror;

/* Field identifies a bad expression bitmap, otherwise NONE. MonthsExamined is
 * 0 for Match/value errors/empty rounded windows and <=MAX_MONTHS for search, including skipped
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
typedef enum xcronzonedstatus {XCRON_ZONED_ERROR=-1,XCRON_ZONED_NONE=0,XCRON_ZONED_FOUND=1} xcronzonedstatus;
typedef enum xcronzonederror {XCRON_ZONED_OK=0,XCRON_ZONED_ARGUMENT=3001,XCRON_ZONED_EXPRESSION,XCRON_ZONED_WINDOW,XCRON_ZONED_ZONE,XCRON_ZONED_RANGE} xcronzonederror;
typedef struct xcronzonedissue {
    xcronzonederror Code;
    xcronfield Field;
    xcronzoneerror Zone;
    uint32 SegmentsExamined;
    uint64 MonthsExamined;
} xcronzonedissue;
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
typedef enum xcronjobstate {XCRON_JOB_SCHEDULED=0,XCRON_JOB_EXHAUSTED,XCRON_JOB_CANCELLED,XCRON_JOB_FAULTED} xcronjobstate;
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
 * all job/run slots, heap and queues within BytesLimit (excluding retained zones
 * and XRT diagnostics). Normal operations and recorded job faults allocate none.
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

#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_ZONED)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_ZONED"
#endif

#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_SCHEDULER)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_SCHEDULER"
#endif

#if defined(XCRON_FEATURE_XCRON) && !defined(XCRON_FEATURE_XCRON_EXECUTOR)
#error "XCRON_FEATURE_XCRON requires XCRON_FEATURE_XCRON_EXECUTOR"
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

#if defined(XCRON_FEATURE_XCRON_TIMEZONE)
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
/* Earliest >=Begin from which the complete zone repeats every Gregorian cycle.
 * False means no proven periodic suffix inside the xtime domain. */
bool xcronZoneRepeat(const xcronzone* Zone,xtime Begin,xtime* First);
bool xcronZoneHistoryRepeat(const xcronzone* Zone,xtime Begin,xtime* First);
xcronzonestate xcronZoneHistoryAt(const xcronzone* Zone,xtime When);
xcronzonestatus xcronZoneHistoryNext(const xcronzone* Zone,xtime After,xcronzonetransition* Transition,xcronzoneissue* Issue);
xcronzonestatus xcronZoneHistoryResolve(const xcronzone* Zone,const xdatetime* Date,xtime* Candidates,size_t Capacity,size_t* Count,xcronzoneissue* Issue);
bool xcronZoneHistoryOverlap(const xcronzone* Zone,const void* Output,size_t Size);
void xcronZoneHistoryRelease(const xcronzone* Zone);
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
	xrtSetErrorInfo(Code==XCRON_CALENDAR_ARGUMENT ? XERR_ARGUMENT : Code==XCRON_CALENDAR_EXPRESSION ? XERR_VALUE : XERR_RANGE,
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
        unsigned C=xcronZoneCycle(Year);
        for(unsigned K=0;K<2;++K){int64 T=Day*86400+Z->Changes[C*2+K];
            if(T<=Second || T>=Best || T>INT64_MAX/1000)continue;
            bool A=xcronZoneLookup(Z,T*1000),B=xcronZoneLookup(Z,T*1000-1);
            if(A!=B){Best=T;
                Before=B;
                Next=A;
        }}
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
#if defined(XCRON_FEATURE_XCRON_ZONED)
#define XCRON_ZONED_CYCLE_MS INT64_C(12622780800000)
static bool xcronZonedFail(xcronzonederror Code,xcronfield Field,xcronzoneerror Zone,xcronzonedissue* Work,xcronzonedissue* Issue,cstr Message)
{
    Work->Code=Code;Work->Field=Field;Work->Zone=Zone;if(Issue)*Issue=*Work;
    xrtSetErrorInfo(Code==XCRON_ZONED_ARGUMENT?XERR_ARGUMENT:Code==XCRON_ZONED_EXPRESSION?XERR_VALUE:Code==XCRON_ZONED_ZONE?XERR_STATE:XERR_RANGE,
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
typedef enum xcronlocation {XCRON_LOC_FREE=0,XCRON_LOC_HEAP,XCRON_LOC_READY,XCRON_LOC_BLOCKED,XCRON_LOC_RETIRED,XCRON_LOC_RUNNING} xcronlocation;
typedef struct xcronjobslot {
    xcronjobconfig Config;
    xcronjobinfo Info;
    uint64 Generation;
    uint32 Next,Previous,Position;
    xcronlocation Location;
    bool Used;
} xcronjobslot;
typedef struct xcronrunslot {uint64 Generation;uint32 Job,Next;bool Used;} xcronrunslot;
struct xcronscheduler {
    uint32 Magic;
    xcronschedulerconfig Config;
    xcronschedulerstats Stats;
    xcronjobslot* Jobs;
    xcronrunslot* Runs;
    uint32* Heap;
    uint32 FreeJob,FreeRun,ReadyHead,ReadyTail,RetireHead,RetireTail;
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
static void xcronSchedulerDetach(xcronscheduler* S,uint32 I)
{
    switch(S->Jobs[I].Location){
    case XCRON_LOC_HEAP:xcronSchedulerHeapRemove(S,I);break;
    case XCRON_LOC_READY:xcronSchedulerQueueRemove(S,I,false);break;
    case XCRON_LOC_BLOCKED:--S->Stats.Blocked;S->Jobs[I].Location=XCRON_LOC_RUNNING;break;
    default:break;
    }
}
static void xcronSchedulerPlace(xcronscheduler* S,uint32 I)
{
    xcronjobslot* J=&S->Jobs[I];
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
static void xcronSchedulerSkip(xcronscheduler* S,uint32 I,xtime Begin,uint32 Flags,xcronpoll* Poll)
{
    xcronSchedulerIncrement(&S->Jobs[I].Info.SkippedRanges);xcronSchedulerIncrement(&S->Stats.SkippedRanges);++Poll->SkippedRanges;
    xcronSchedulerSearch(S,I,Begin,Flags,Poll);
}
static bool xcronSchedulerActionable(const xcronscheduler* S,xcronclock Now)
{
    if(S->Stats.Heap && S->Jobs[S->Heap[0]].Info.Next<=Now.Wall)return true;
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
        !xcronSchedulerArrayBytes(Config->JobLimit,sizeof(uint32),&Bytes) || Bytes>Config->BytesLimit)return xcronSchedulerFail(XCRON_SCHEDULER_LIMIT,NULL,Issue);
    size_t JBytes=(size_t)Config->JobLimit*sizeof(xcronjobslot),RBytes=(size_t)Config->RunLimit*sizeof(xcronrunslot);
    xcronscheduler* S=xrtMalloc(Bytes);if(!S)return xcronSchedulerFail(XCRON_SCHEDULER_MEMORY,NULL,Issue);
    memset(S,0,Bytes);S->Magic=XCRON_SCHEDULER_MAGIC;S->Config=*Config;S->Stats.Bytes=Bytes;
    S->Jobs=(xcronjobslot*)S->Storage;S->Runs=(xcronrunslot*)(S->Storage+JBytes);S->Heap=(uint32*)(S->Storage+JBytes+RBytes);
    S->FreeJob=0;S->FreeRun=0;S->ReadyHead=S->ReadyTail=S->RetireHead=S->RetireTail=XCRON_SLOT_NONE;S->Promote=true;S->JobBits=xcronSchedulerIndexBits(Config->JobLimit);S->RunBits=xcronSchedulerIndexBits(Config->RunLimit);
    for(uint32 I=0;I<Config->JobLimit;++I){S->Jobs[I].Generation=1;S->Jobs[I].Next=I+1<Config->JobLimit?I+1:XCRON_SLOT_NONE;}
    for(uint32 I=0;I<Config->RunLimit;++I){S->Runs[I].Generation=1;S->Runs[I].Next=I+1<Config->RunLimit?I+1:XCRON_SLOT_NONE;}
    *Out=S;xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerAdd(xcronscheduler* S,const xcronjobconfig* Config,xcronjobid* Id,xcronschedulerissue* Issue)
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
    xtime Next=0;xcronzonedstatus Result=xcronZonedNextValue(&Config->Expression,Config->Zone,Config->Begin,Config->End,Config->SearchFlags,&Next,&ZI);
    if(Result==XCRON_ZONED_ERROR)return xcronSchedulerFail(XCRON_SCHEDULER_CALCULATION,&ZI,Issue);
    if(!xcronZoneRetain(Config->Zone))return xcronSchedulerFail(XCRON_SCHEDULER_LIMIT,NULL,Issue);
    uint32 I=S->FreeJob;xcronjobslot* J=&S->Jobs[I];S->FreeJob=J->Next;J->Used=true;J->Config=*Config;memset(&J->Info,0,sizeof(J->Info));
    J->Info.State=Result==XCRON_ZONED_FOUND?XCRON_JOB_SCHEDULED:XCRON_JOB_EXHAUSTED;J->Info.HasNext=Result==XCRON_ZONED_FOUND;J->Info.Next=Next;J->Info.LastIssue=ZI;
    ++S->Stats.Jobs;if(S->Stats.Jobs>S->Stats.PeakJobs)S->Stats.PeakJobs=S->Stats.Jobs;
    xcronSchedulerPlace(S,I);*Id=xcronSchedulerId(J->Generation,I,S->JobBits);xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerPoll(xcronscheduler* S,xcronclock Now,xcronrun* Runs,size_t Capacity,xcronpoll* Out,xcronschedulerissue* Issue)
{
    void* O[]={Runs,Out,Issue};size_t N[]={Capacity<=SIZE_MAX/sizeof(*Runs)?Capacity*sizeof(*Runs):SIZE_MAX,sizeof(*Out),Issue?sizeof(*Issue):0};
    if(!S || !Out || !Runs || !Capacity || Capacity>SIZE_MAX/sizeof(*Runs) || !xcronSchedulerOutputs(S,NULL,0,O,N,3))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    if(!xcronSchedulerClock(S,Now))return xcronSchedulerFail(XCRON_SCHEDULER_CLOCK,NULL,Issue);
    if(S->Stats.HasClock && Now.Wall<S->Stats.LastClock.Wall)xcronSchedulerIncrement(&S->Stats.WallRegressions);
    S->Stats.LastClock=Now;S->Stats.HasClock=true;xcronSchedulerIncrement(&S->Stats.ClockSamples);xcronpoll P={0};bool GlobalBlocked=false;
    while(P.Work<S->Config.WorkLimit && P.Count<Capacity){
        bool Due=S->Stats.Heap && S->Jobs[S->Heap[0]].Info.Next<=Now.Wall;
        bool Ready=S->Stats.Ready && !GlobalBlocked;
        if(!Due && !Ready)break;
        ++P.Work;
        if(Due && (!Ready || S->Promote)){
            uint32 I=S->Heap[0];xcronSchedulerHeapRemove(S,I);xcronSchedulerQueueAdd(S,I,false);S->Promote=false;continue;
        }
        S->Promote=true;uint32 I=S->ReadyHead;xcronjobslot* J=&S->Jobs[I];
        /* A rolled-back wall sample cannot dispatch a previously ready future. */
        if(J->Info.Next>Now.Wall){xcronSchedulerQueueRemove(S,I,false);xcronSchedulerHeapAdd(S,I);continue;}
        if(J->Config.Overlap==XCRON_OVERLAP_SKIP && J->Info.Running){
            xcronSchedulerQueueRemove(S,I,false);xcronSchedulerSkip(S,I,Now.Wall,XCRON_SEARCH_EXCLUSIVE,&P);xcronSchedulerPlace(S,I);continue;
        }
        if(J->Info.Running>=J->Config.RunningLimit){
            xcronSchedulerQueueRemove(S,I,false);J->Location=XCRON_LOC_BLOCKED;++S->Stats.Blocked;continue;
        }
        if(S->FreeRun==XCRON_SLOT_NONE){GlobalBlocked=true;continue;}
        xcronSchedulerQueueRemove(S,I,false);xtime Cutoff=xcronSchedulerCutoff(Now.Wall,J->Config.GraceMilliseconds);
        if(J->Config.Misfire==XCRON_MISFIRE_SKIP && J->Info.Next<Cutoff){
            xcronSchedulerSkip(S,I,Cutoff,0,&P);xcronSchedulerPlace(S,I);continue;
        }
        bool Coalesced=J->Config.Misfire==XCRON_MISFIRE_COALESCE && J->Info.Next<Cutoff;
        xtime Through=Coalesced?(Now.Wall<J->Config.End?Now.Wall:J->Config.End):J->Info.Next;
        uint32 RIndex=S->FreeRun;xcronrunslot* R=&S->Runs[RIndex];S->FreeRun=R->Next;R->Used=true;R->Job=I;
        Runs[P.Count++]=(xcronrun){xcronSchedulerId(R->Generation,RIndex,S->RunBits),xcronSchedulerId(J->Generation,I,S->JobBits),J->Config.Data,J->Info.Next,Through,Now,Coalesced?XCRON_RUN_COALESCED:0};
        ++J->Info.Running;++S->Stats.Running;if(S->Stats.Running>S->Stats.PeakRunning)S->Stats.PeakRunning=S->Stats.Running;
        xcronSchedulerIncrement(&J->Info.Dispatched);xcronSchedulerIncrement(&S->Stats.Dispatched);
        if(Coalesced){xcronSchedulerIncrement(&J->Info.CoalescedRanges);xcronSchedulerIncrement(&S->Stats.CoalescedRanges);}
        xcronSchedulerSearch(S,I,Through,XCRON_SEARCH_EXCLUSIVE,&P);xcronSchedulerPlace(S,I);
    }
    P.More=xcronSchedulerActionable(S,Now);*Out=P;xcronSchedulerOK(Issue);return true;
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
    if(J->Info.State!=XCRON_JOB_SCHEDULED){if(!J->Info.Running)xcronSchedulerQueueAdd(S,I,true);}
    else if(J->Location==XCRON_LOC_BLOCKED && J->Info.Running<J->Config.RunningLimit){--S->Stats.Blocked;xcronSchedulerPlace(S,I);}
    xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerCancel(xcronscheduler* S,xcronjobid Id,xcronschedulerissue* Issue)
{
    if(!xcronSchedulerArgs(S,NULL,0,Issue))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    uint32 I;if(!xcronSchedulerJobIndex(S,Id,&I))return xcronSchedulerFail(XCRON_SCHEDULER_STALE,NULL,Issue);
    xcronjobslot* J=&S->Jobs[I];if(J->Info.State==XCRON_JOB_SCHEDULED){xcronSchedulerDetach(S,I);J->Info.State=XCRON_JOB_CANCELLED;J->Info.HasNext=false;xcronSchedulerPlace(S,I);}
    xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerJob(const xcronscheduler* S,xcronjobid Id,xcronjobinfo* Info,xcronschedulerissue* Issue)
{
    if(!Info || !xcronSchedulerArgs(S,Info,sizeof(*Info),Issue))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    uint32 I;if(!xcronSchedulerJobIndex(S,Id,&I))return xcronSchedulerFail(XCRON_SCHEDULER_STALE,NULL,Issue);
    *Info=S->Jobs[I].Info;xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerReap(xcronscheduler* S,xcronretired* Retired,bool* Found,xcronschedulerissue* Issue)
{
    void* O[]={Retired,Found,Issue};size_t N[]={sizeof(*Retired),sizeof(*Found),Issue?sizeof(*Issue):0};
    if(!S || !Retired || !Found || !xcronSchedulerOutputs(S,NULL,0,O,N,3))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    uint32 I=S->RetireHead;
    if(I==XCRON_SLOT_NONE){*Found=false;xcronSchedulerOK(Issue);return true;}
    xcronjobslot* J=&S->Jobs[I];xcronretired Value={xcronSchedulerId(J->Generation,I,S->JobBits),J->Config.Data,J->Info};
    xcronSchedulerQueueRemove(S,I,true);xcronZoneRelease(J->Config.Zone);J->Config.Zone=NULL;J->Config.Data=NULL;J->Used=false;J->Location=XCRON_LOC_FREE;--S->Stats.Jobs;
    if(J->Generation!=(UINT64_MAX>>S->JobBits)){++J->Generation;J->Next=S->FreeJob;S->FreeJob=I;}
    *Retired=Value;*Found=true;xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerClose(xcronscheduler* S,xcronschedulerissue* Issue)
{
    if(!xcronSchedulerArgs(S,NULL,0,Issue))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    if(!S->Stats.Closed){S->Stats.Closed=true;
        for(uint32 I=0;I<S->Config.JobLimit;++I){xcronjobslot* J=&S->Jobs[I];if(J->Used && J->Info.State==XCRON_JOB_SCHEDULED){xcronSchedulerDetach(S,I);J->Info.State=XCRON_JOB_CANCELLED;J->Info.HasNext=false;xcronSchedulerPlace(S,I);}}}
    xcronSchedulerOK(Issue);return true;
}
XCRON_API bool xcronSchedulerWake(const xcronscheduler* S,xcronclock Now,uint64 MaxWait,xcronwake* Wake,xcronschedulerissue* Issue)
{
    if(!Wake || !MaxWait || !xcronSchedulerArgs(S,Wake,sizeof(*Wake),Issue))return xcronSchedulerFail(XCRON_SCHEDULER_ARGUMENT,NULL,NULL);
    if(!xcronSchedulerClock(S,Now))return xcronSchedulerFail(XCRON_SCHEDULER_CLOCK,NULL,Issue);
    uint64 Delay=MaxWait;bool Free=S->FreeRun!=XCRON_SLOT_NONE;
    bool Immediate=(S->Stats.Ready && Free) || (S->Stats.Heap && S->Jobs[S->Heap[0]].Info.Next<=Now.Wall);
    /* A full lease table needs an external completion or bounded resampling,
     * not repeated immediate wakes for a ready job it cannot dispatch. */
    if(!Free && !S->Stats.Heap)Immediate=false;
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

#endif
