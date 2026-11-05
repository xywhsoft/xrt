#ifndef XCRON_CALENDAR_INTERNAL_H
#define XCRON_CALENDAR_INTERNAL_H
#include <xcron/calendar.h>
#if defined(XCRON_FEATURE_XCRON_CALENDAR)
/* Typed product-private engine; caller supplies its immutable expression. */
typedef struct xcroncalendarvalue {
    xdatetime First,End,Date;
    uint32 Months;
    int FirstWeekday;
    xcroncalendarstatus Status;
} xcroncalendarvalue;
void xcronCalendarValueInit(xcroncalendarvalue* Value,const xdatetime* First,const xdatetime* End,xcroncalendarstatus Status);
xcroncalendarstatus xcronCalendarValueStep(const xcronexpression* Expression,xcroncalendarvalue* Value,uint32 MonthBudget);
/* Product-private validated calendar operations; no XRT-private API dependency. */
bool xcronCalendarValidateExpression(const xcronexpression* Expression,xcronfield* Field);
bool xcronCalendarPartsMatch(const xcronexpression* Expression,const xdatetime* Date);
xcroncalendarstatus xcronCalendarSearch(const xcronexpression* Expression,const xdatetime* Begin,const xdatetime* End,
    uint32 Flags,xdatetime* Next,xcroncalendarissue* Issue);
#endif
#endif
