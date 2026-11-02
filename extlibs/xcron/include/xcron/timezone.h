#ifndef XCRON_TIMEZONE_H
#define XCRON_TIMEZONE_H
#include <xcron/features.h>
#include <xcron/export.h>
#include <xrt/time.h>
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
