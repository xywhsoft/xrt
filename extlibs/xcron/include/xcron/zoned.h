#ifndef XCRON_ZONED_H
#define XCRON_ZONED_H
#include <xcron/features.h>
#include <xcron/calendar.h>
#include <xcron/timezone.h>
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
