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
typedef struct xcronzone xcronzone;
typedef enum xcronzonegrammar {XCRON_ZONE_POSIX=2,XCRON_ZONE_TZIF3=3} xcronzonegrammar;
typedef enum xcronzonestatus {XCRON_ZONE_CAPACITY=-2,XCRON_ZONE_ERROR=-1,XCRON_ZONE_NONE=0,XCRON_ZONE_FOUND=1} xcronzonestatus;
typedef enum xcronzoneerror {XCRON_ZONE_OK=0,XCRON_ZONE_ARGUMENT=2001,XCRON_ZONE_SYNTAX,XCRON_ZONE_LIMIT,XCRON_ZONE_DATE,XCRON_ZONE_MEMORY,XCRON_ZONE_RULES_REQUIRED} xcronzoneerror;
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
 * timezone data is embedded. IANA/TZif loading is a separate pending capability. */
XCRON_API bool xcronZoneCreatePosix(xstrview Text,xcronzonegrammar Grammar,xcronzone** Zone,xcronzoneissue* Issue);
XCRON_API bool xcronZoneRetain(const xcronzone* Zone);
XCRON_API void xcronZoneRelease(const xcronzone* Zone);
/* Thread-safe immutable queries, full signed-int64 XRT millisecond timeline
 * (0001-01-01 epoch; Unix epoch is XRT_TIME_UNIX_EPOCH, not zero).
 * No allocation on normal success/NONE/CAPACITY, no TZ/global libc state.
 * Caller holds Zone throughout each call. Outputs/Issue must be disjoint from
 * owned Zone storage, civil input and one another. Invalid API args preserve all
 * outputs. Offset is seconds east of UTC; abbreviation scope is documented above. */
XCRON_API bool xcronZoneAt(const xcronzone* Zone,xtime When,xcronzonestate* State,xcronzoneissue* Issue);
/* First effective state change strictly after After, or NONE without changing
 * Transition. Coincident transitions are evaluated together; perpetual rules
 * return NONE. Gregorian completeness is bounded at 403 rule years. */
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
 * value errors may allocate a diagnostic, never affect the typed Issue. */
XCRON_API xcronzonestatus xcronZoneResolve(const xcronzone* Zone,const xdatetime* Local,xtime* Candidates,size_t Capacity,size_t* Count,xcronzoneissue* Issue);
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

#endif

#endif

#if defined(XCRON_IMPLEMENTATION) && !defined(XCRON_IMPLEMENTATION_ONCE)
#define XCRON_IMPLEMENTATION_ONCE 1


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

static bool xcronCalendarExpression(const xcronexpression* Expression, xcroncalendarissue* Issue)
{
	static const uint64 Allowed[]={UINT64_C(0x0fffffffffffffff),UINT64_C(0x0fffffffffffffff),UINT64_C(0xffffff),
		UINT64_C(0xfffffffe),UINT64_C(0x1ffe),UINT64_C(0x7f)};
	if ((Expression->Dialect!=XCRON_UNIX5 && Expression->Dialect!=XCRON_SECONDS6) || (Expression->Flags&~(XCRON_DAY_MONTH_STAR|XCRON_DAY_WEEK_STAR)))
		return xcronCalendarReport(XCRON_CALENDAR_EXPRESSION,XCRON_FIELD_NONE,Issue,"invalid compiled cron dialect/day flags");
	for (unsigned Field=0; Field<XCRON_FIELD_COUNT; ++Field)
		if (!Expression->Fields[Field] || (Expression->Fields[Field]&~Allowed[Field]) ||
			(Field==XCRON_FIELD_SECOND && Expression->Dialect==XCRON_UNIX5 && Expression->Fields[Field]!=1))
			return xcronCalendarReport(XCRON_CALENDAR_EXPRESSION,(xcronfield)Field,Issue,"invalid compiled cron field bitmap");
	return true;
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

static bool xcronCalendarPartsMatch(const xcronexpression* Expression, const xdatetime* Date)
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

static xcroncalendarstatus xcronCalendarSearch(const xcronexpression* Expression, const xdatetime* Begin, const xdatetime* End,
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
#define XCRON_ZONE_MAGIC UINT32_C(0x58435a31)
typedef struct xcronzonerule {unsigned Kind,A,B,C;
    int32 Time;
} xcronzonerule;
typedef struct xcronzoneparser {xstrview Text;
    size_t At;
    xcronzonegrammar Grammar;
    xcronzoneissue* Issue;
} xcronzoneparser;
struct xcronzone {
    xatomic32 Refs;
    uint32 Magic;
    size_t Bytes;
    int32 Offsets[2];
    uint16 Names[2];
    uint8 Mode,HasDST;
    /* Start/end UTC seconds relative to January 1, for 2000..2399. Each
     * entry fits int32 even with a 365th-day carry and signed 167h times. */
    int32 Changes[];
};
static bool xcronZoneFail(xcronzoneerror Code,size_t Offset,xcronzoneissue* Issue)
{
    if(Issue)*Issue=(xcronzoneissue){Code,Offset,0};
    xrtSetErrorInfo(Code==XCRON_ZONE_ARGUMENT?XERR_ARGUMENT:Code==XCRON_ZONE_MEMORY?XERR_MEMORY:Code==XCRON_ZONE_LIMIT?XERR_RANGE:XERR_VALUE,"xrt.cron.zone",Code,
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
static bool xcronZoneOutputs(const xcronzone* Z,const void* Input,size_t InputSize,void* const* Outputs,const size_t* Sizes,size_t Count)
{
    if(!xcronZoneHandle(Z) || (Input && !xrtMemRangeValid(Input,InputSize)))return xcronZoneFail(XCRON_ZONE_ARGUMENT,0,NULL);
    for(size_t I=0;I<Count;++I){if(!xrtMemRangeValid(Outputs[I],Sizes[I]) || xrtMemRangesOverlap(Outputs[I],Sizes[I],Z,Z->Bytes) || xrtMemRangesOverlap(Outputs[I],Sizes[I],Input,InputSize))return xcronZoneFail(XCRON_ZONE_ARGUMENT,0,NULL);
        for(size_t J=0;J<I;++J)if(xrtMemRangesOverlap(Outputs[I],Sizes[I],Outputs[J],Sizes[J]))return xcronZoneFail(XCRON_ZONE_ARGUMENT,0,NULL);
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
XCRON_API bool xcronZoneAt(const xcronzone* Z,xtime When,xcronzonestate* State,xcronzoneissue* Issue)
{
    void* Outputs[]={State,Issue};
    size_t Sizes[]={State?sizeof(*State):0,Issue?sizeof(*Issue):0};
    if(!State || !xcronZoneOutputs(Z,NULL,0,Outputs,Sizes,2))return xcronZoneFail(XCRON_ZONE_ARGUMENT,0,NULL);
    *State=xcronZoneState(Z,xcronZoneLookup(Z,When));
    if(Issue)*Issue=(xcronzoneissue){XCRON_ZONE_OK,0,Z->Mode==2?5:0};
    return true;
}
static void xcronZoneDecompose(xtime When,int32 Offset,bool DST,xdatetime* Out)
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
    bool DST=xcronZoneLookup(Z,When);
    xcronZoneDecompose(When,Z->Offsets[DST?1:0],DST,Date);
    if(Issue)*Issue=(xcronzoneissue){XCRON_ZONE_OK,0,Z->Mode==2?5:0};
    return true;
}
static bool xcronZoneCivil(const xdatetime* D)
{return D->Year && D->Month>=1 && D->Month<=12 && D->Day>=1 && D->Day<=xrtDaysInMonth(D->Year,D->Month) && D->Hour>=0 && D->Hour<=23 && D->Minute>=0 && D->Minute<=59 && D->Second>=0 && D->Second<=59 && D->Millisecond>=0 && D->Millisecond<=999;
}
static bool xcronZoneInstant(const xdatetime* D,int32 Offset,xtime* Out)
{
    if(D->Year<-400000000 || D->Year>400000000)return false;
    int64 Year=D->Year<0?D->Year+1:D->Year,Cycles=xcronZoneFloor(Year,400),SafeYear=2000+Year-Cycles*400;
    xtime Safe;
    (void)xrtDateTime(SafeYear,D->Month,D->Day,D->Hour,D->Minute,D->Second,0,&Safe);
    int64 Seconds=Safe/1000+(Cycles-5)*INT64_C(12622780800)-Offset;
    int64 Minimum=INT64_MIN/1000-1,Maximum=INT64_MAX/1000;
    if(Seconds<Minimum || Seconds>Maximum)return false;
    if(Seconds==Minimum){int Limit=(int)(1000+INT64_MIN%1000);
        if(D->Millisecond<Limit)return false;
        *Out=INT64_MIN+(D->Millisecond-Limit);
        return true;
    }
    if(Seconds==Maximum && D->Millisecond>INT64_MAX%1000)return false;
    *Out=Seconds*1000+D->Millisecond;
    return true;
}
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
    if(Issue)*Issue=(xcronzoneissue){XCRON_ZONE_OK,0,0};
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

#endif
