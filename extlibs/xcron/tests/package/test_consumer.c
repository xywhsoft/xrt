#define XCRON_MODULE_XCRON
#include <xcron.h>
#include <string.h>

int main(void)
{
	xcronexpression Expression; xcronexpressionissue Issue;
	if (!xcronExpressionCompile(XRT_STR_LITERAL("0,30 9-17 * JAN-MAR MON-FRI"), XCRON_UNIX5, &Expression, &Issue)) return 1;
	if (Expression.Fields[XCRON_FIELD_MINUTE] != ((uint64)1u | ((uint64)1u << 30u)) ||
		Expression.Fields[XCRON_FIELD_WEEKDAY] != 0x3eu || Expression.Flags != XCRON_DAY_MONTH_STAR) return 2;
	xtime Begin, End, Next; bool Matches; xcroncalendarissue CalendarIssue;
	if (!xrtDateTime(2024,1,1,9,0,0,123,&Begin) || !xrtDate(2024,1,2,&End) ||
		xcronCalendarNext(&Expression,Begin,End,0,&Next,&CalendarIssue)!=XCRON_CALENDAR_FOUND || Next!=Begin-123+30*XRT_TIME_MINUTE ||
		!xcronCalendarMatch(&Expression,Next,&Matches,NULL) || !Matches) return 5;
	xdatetime First, Last, Civil, Canonical; xrtTimeSplit(Begin,&First); xrtTimeSplit(End,&Last); xrtTimeSplit(Next,&Canonical); First.Offset=3600; First.Weekday=-1;
	if (xcronCalendarNextCivil(&Expression,&First,&Last,0,&Civil,&CalendarIssue)!=XCRON_CALENDAR_FOUND || Civil.Year!=2024 || Civil.Day!=1 ||
		Civil.Hour!=9 || Civil.Minute!=30 || Civil.Weekday!=1 || Civil.Offset!=0 || Civil.YearDay!=1 ||
		!xcronCalendarMatchCivil(&Expression,&Civil,&Matches,NULL) || !Matches || Civil.Second!=Canonical.Second) return 6;
	xcronzone* Zone=NULL; xcronzoneissue ZoneIssue; xtime Candidates[2]; size_t Count;
	xdatetime Fold={.Year=2024,.Month=11,.Day=3,.Hour=1,.Minute=30};
	if (!xcronZoneCreatePosix(XRT_STR_LITERAL("EST5EDT,M3.2.0,M11.1.0"),XCRON_ZONE_TZIF3,&Zone,&ZoneIssue) ||
		xcronZoneResolve(Zone,&Fold,Candidates,2,&Count,&ZoneIssue)!=XCRON_ZONE_FOUND || Count!=2 || Candidates[1]-Candidates[0]!=XRT_TIME_HOUR) return 7;
	xcronZoneRelease(Zone);
	static const uint8 Tzif[54]={[0]='T',[1]='Z',[2]='i',[3]='f',[39]=1,[43]=4,[50]='U',[51]='T',[52]='C'};
	xcronzonestate State;
	if (!xcronZoneCreateTzif((xbytesview){Tzif,sizeof(Tzif)},&Zone,&ZoneIssue) ||
		!xcronZoneAt(Zone,XRT_TIME_UNIX_EPOCH,&State,&ZoneIssue) || State.Offset!=0) return 8;
	xcronZoneRelease(Zone);
	if (xcronExpressionCompile(XRT_STR_LITERAL("60 * * * *"), XCRON_UNIX5, &Expression, &Issue)) return 3;
	if (Issue.Code != XCRON_EXPRESSION_RANGE || xrtErrorKind(xrtGetError()) != XERR_RANGE ||
		strcmp(xrtErrorDomain(xrtGetError()), "xrt.cron") != 0) return 4;
	xrtClearError();
	return 0;
}
