#include <xcron.h>
#include <inttypes.h>
#include <stdio.h>

int main(void)
{
	/* Deterministic UTC calculation only; no clock, thread, timezone or job. */
	xcronexpression Expression; xtime Begin, End, Next; xcroncalendarissue Issue; xdatetime Date;
	if (!xcronExpressionCompile(XRT_STR_LITERAL("0 9 * * MON-FRI"),XCRON_UNIX5,&Expression,NULL) ||
		!xrtDateTime(2024,3,1,9,0,0,123,&Begin) || !xrtDate(2024,3,10,&End) ||
		xcronCalendarNext(&Expression,Begin,End,0,&Next,&Issue)!=XCRON_CALENDAR_FOUND || !xrtTimeSplit(Next,&Date) ||
		Date.Year!=2024 || Date.Month!=3 || Date.Day!=4 || Date.Hour!=9 || Date.Minute || Date.Second || Date.Millisecond) return 1;
	printf("next UTC=%" PRId64 "-%02d-%02d %02d:%02d:%02d months=%u\n",Date.Year,Date.Month,Date.Day,Date.Hour,Date.Minute,Date.Second,(unsigned)Issue.MonthsExamined);
	bool Matches; if (!xcronCalendarMatch(&Expression,Next,&Matches,NULL) || !Matches) return 2;
	if (!xcronExpressionCompile(XRT_STR_LITERAL("0 0 30 FEB *"),XCRON_UNIX5,&Expression,NULL) ||
		xcronCalendarNext(&Expression,INT64_MIN,INT64_MAX,0,&Next,&Issue)!=XCRON_CALENDAR_NONE || Issue.MonthsExamined>XCRON_CALENDAR_MAX_MONTHS) return 3;
	printf("impossible February 30: months=%u\n",(unsigned)Issue.MonthsExamined);
    if(!xcronExpressionCompile(XRT_STR_LITERAL("0 0 29 FEB */7"),XCRON_UNIX5,&Expression,NULL) || !xrtDate(2040,1,1,&End))return 4;
    xcroncalendarcursor Cursor;if(!xcronCalendarCursorInit(&Cursor,&Expression,Begin,End,0,&Issue))return 4;
    uint32 HostTurns=0,Previous=Issue.MonthsExamined;xcroncalendarstatus Status;
    do{
        /* A host can do other work here; each call visits at most 16 months. */
        Status=xcronCalendarCursorStep(&Cursor,16,&Next,&Issue);++HostTurns;
        if(Issue.MonthsExamined-Previous>16)return 5;
        Previous=Issue.MonthsExamined;
    }while(Status==XCRON_CALENDAR_PENDING);
    if(Status!=XCRON_CALENDAR_FOUND || !xrtTimeSplit(Next,&Date) || Date.Year!=2032 || Date.Month!=2 || Date.Day!=29)return 6;
    printf("resumed leap-Sunday search: host-turns=%u months=%u\n",HostTurns,Issue.MonthsExamined);return 0;
}
