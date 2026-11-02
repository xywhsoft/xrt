#ifndef XCRON_CALENDAR_INTERNAL_H
#define XCRON_CALENDAR_INTERNAL_H
#include <xcron/calendar.h>
#if defined(XCRON_FEATURE_XCRON_CALENDAR)
/* Product-private validated calendar operations; no XRT-private API dependency. */
bool xcronCalendarValidateExpression(const xcronexpression* Expression,xcronfield* Field);
bool xcronCalendarPartsMatch(const xcronexpression* Expression,const xdatetime* Date);
xcroncalendarstatus xcronCalendarSearch(const xcronexpression* Expression,const xdatetime* Begin,const xdatetime* End,
    uint32 Flags,xdatetime* Next,xcroncalendarissue* Issue);
#endif
#endif
