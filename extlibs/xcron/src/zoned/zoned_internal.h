#ifndef XCRON_ZONED_INTERNAL_H
#define XCRON_ZONED_INTERNAL_H
#include <xcron/zoned.h>
#include "../calendar/calendar_internal.h"
#include "../timezone/timezone_internal.h"
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
