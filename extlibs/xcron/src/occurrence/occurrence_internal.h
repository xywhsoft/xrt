#ifndef XCRON_OCCURRENCE_INTERNAL_H
#define XCRON_OCCURRENCE_INTERNAL_H
#include <xcron/occurrence.h>
#include "../calendar/calendar_internal.h"
#include "../timezone/timezone_internal.h"
#if defined(XCRON_FEATURE_XCRON_OCCURRENCE)
/* Ordered UTC traversal checks each gap before entering its actual segment.
 * Calendar/walk storage is reused without restarting a whole zoned query after
 * each filtered candidate. Valid immutable inputs; no diagnostics or retention. */
typedef enum xcronoccurrencephase {
    XCRON_OCCURRENCE_PHASE_START,XCRON_OCCURRENCE_PHASE_READY,
    XCRON_OCCURRENCE_PHASE_WALK_BEGIN,XCRON_OCCURRENCE_PHASE_WALK,
    XCRON_OCCURRENCE_PHASE_CALENDAR,XCRON_OCCURRENCE_PHASE_RESOLVE,
    XCRON_OCCURRENCE_PHASE_NEXT,XCRON_OCCURRENCE_PHASE_GAP,
    XCRON_OCCURRENCE_PHASE_GAP_RESOLVE
} xcronoccurrencephase;
typedef struct xcronoccurrencevalue {
    xcronexpression Expression;
    xcroncalendarvalue Calendar;
    xcronzonewalk Transition;
    xcronzoneresolution Resolve;
    xcronoccurrence Candidate;
    xcronoccurrenceissue Work;
    xtime First,Limit,ActualLimit,SegmentEnd;
    int32 Offset;
    xcrondeliverypolicy Policy;
    uint32 Phase;
    xcronoccurrencestatus Status;
    bool SawActual,ActualDone,WalkReady,AfterReady;
} xcronoccurrencevalue;
void xcronOccurrenceValueInit(const xcronexpression* Expression,const xcronzone* Zone,xtime Begin,xtime End,uint32 Flags,xcrondeliverypolicy Policy,xcronoccurrencevalue* Value);
xcronoccurrencestatus xcronOccurrenceValueStep(xcronoccurrencevalue* Value,const xcronzone* Zone,xcronoccurrencebudget Budget);
#endif
#endif
