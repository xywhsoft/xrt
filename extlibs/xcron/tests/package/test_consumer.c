#define XCRON_MODULE_XCRON
#include <xcron.h>
#include <string.h>
#include <xrt/thread.h>

static xcronrunresult consumerExecute(const xcronrun* Run,const xcronexecution* Token,ptr Context)
{(void)Run;(void)Context;return xcronExecutionCancelled(Token)?XCRON_RUN_CANCELLED:XCRON_RUN_SUCCEEDED;}
static bool consumerClock(xcronclock* Clock,ptr Context)
{(void)Context;*Clock=(xcronclock){XRT_TIME_UNIX_EPOCH,1};return true;}
static bool consumerService(void)
{
    xexecutorconfig P={1,4,0};xexecutor* Pool=xrtExecutorCreate(&P);if(!Pool)return false;
    xcronserviceconfig C={.Scheduler={1,1,8,0,65536},.Executor=Pool,.Proc=consumerExecute,.Clock=consumerClock,.CommandLimit=2,.CompletionLimit=2,.BatchLimit=1,.CommandWorkLimit=2,.ResampleMilliseconds=5,.RetryIntervalMilliseconds=5,.RetryAttempts=1,.BytesLimit=262144,.AdapterBytesLimit=65536};
    xcronservice* S=NULL;xcronzone* Z=NULL;xcronexpression E;
    if(!xcronServiceCreate(&C,&S,NULL) || !xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_TZIF3,&Z,NULL) || !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&E,NULL))return false;
    xcronjobconfig J={.Expression=E,.Zone=Z,.Begin=XRT_TIME_UNIX_EPOCH,.End=XRT_TIME_UNIX_EPOCH,.RunningLimit=1};xcronrequestid Q;xcronreply R;
    if(!xcronServiceAddAsync(S,&J,&Q,NULL) || xcronServiceCollect(S,Q,10000,&R,NULL)!=XWAIT_OK || R.Code || R.Error)return false;
    xcronservicestats Backend;if(!xcronServiceInspect(S,&Backend,NULL) || Backend.Waiter.StorageBytes!=sizeof(xeventwaiter) || Backend.ActiveNativeTimers!=Backend.Waiter.NativeTimers)return false;
    xcronZoneRelease(Z);size_t N=0;xcronservicecompletion D;double Limit=xrtTimer()+10;
    while(!N){if(!xcronServiceDrain(S,&D,1,&N,NULL) || xrtTimer()>Limit)return false;if(!N)xrtThreadYield();}
    if(D.Result!=XCRON_RUN_SUCCEEDED || D.Error || D.Job!=R.Job || !xcronServiceClose(S,false,NULL) || xcronServiceWaitFor(S,10000,NULL)!=XWAIT_OK)return false;
    xcronretired Retired;bool Found=false;
    return xcronServiceReapStopped(S,&Retired,&Found,NULL) && Found && Retired.Id==R.Job && xcronServiceDestroy(S,NULL) && xrtExecutorClose(Pool) && xrtExecutorWaitFor(Pool,10000)==XWAIT_OK && xrtExecutorDestroy(Pool);
}

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
    xcroncalendarcursor Cursor;xtime Stepped=777;xcroncalendarissue CursorIssue;
    if(!xcronCalendarCursorInit(&Cursor,&Expression,Begin,End,0,&CursorIssue))return 21;
    xcroncalendarstatus CursorStatus;do{uint32 Previous=CursorIssue.MonthsExamined;CursorStatus=xcronCalendarCursorStep(&Cursor,1,&Stepped,&CursorIssue);if(CursorIssue.MonthsExamined-Previous>1)return 22;}while(CursorStatus==XCRON_CALENDAR_PENDING);
    if(CursorStatus!=XCRON_CALENDAR_FOUND || Stepped!=Next)return 23;
	xdatetime First, Last, Civil, Canonical; xrtTimeSplit(Begin,&First); xrtTimeSplit(End,&Last); xrtTimeSplit(Next,&Canonical); First.Offset=3600; First.Weekday=-1;
	if (xcronCalendarNextCivil(&Expression,&First,&Last,0,&Civil,&CalendarIssue)!=XCRON_CALENDAR_FOUND || Civil.Year!=2024 || Civil.Day!=1 ||
		Civil.Hour!=9 || Civil.Minute!=30 || Civil.Weekday!=1 || Civil.Offset!=0 || Civil.YearDay!=1 ||
		!xcronCalendarMatchCivil(&Expression,&Civil,&Matches,NULL) || !Matches || Civil.Second!=Canonical.Second) return 6;
    if(!xcronCalendarCursorInitCivil(&Cursor,&Expression,&First,&Last,0,&CursorIssue))return 24;
    do{CursorStatus=xcronCalendarCursorStepCivil(&Cursor,1,&Civil,&CursorIssue);}while(CursorStatus==XCRON_CALENDAR_PENDING);
    if(CursorStatus!=XCRON_CALENDAR_FOUND || Civil.Year!=Canonical.Year || Civil.Month!=Canonical.Month || Civil.Day!=Canonical.Day || Civil.Hour!=Canonical.Hour || Civil.Minute!=Canonical.Minute || Civil.Weekday!=Canonical.Weekday)return 25;
	xcronzone* Zone=NULL; xcronzoneissue ZoneIssue; xtime Candidates[2]; size_t Count;
	xdatetime Fold={.Year=2024,.Month=11,.Day=3,.Hour=1,.Minute=30};
	if (!xcronZoneCreatePosix(XRT_STR_LITERAL("EST5EDT,M3.2.0,M11.1.0"),XCRON_ZONE_TZIF3,&Zone,&ZoneIssue) ||
		xcronZoneResolve(Zone,&Fold,Candidates,2,&Count,&ZoneIssue)!=XCRON_ZONE_FOUND || Count!=2 || Candidates[1]-Candidates[0]!=XRT_TIME_HOUR) return 7;
	xcronexpression ZonedExpression; xcronzonedissue ZonedIssue;
	if (!xcronExpressionCompile(XRT_STR_LITERAL("30 1 * * *"),XCRON_UNIX5,&ZonedExpression,NULL) ||
		!xcronZonedMatch(&ZonedExpression,Zone,Candidates[0],&Matches,&ZonedIssue) || !Matches ||
		xcronZonedNext(&ZonedExpression,Zone,Candidates[0],Candidates[1],XCRON_SEARCH_EXCLUSIVE,&Next,&ZonedIssue)!=XCRON_ZONED_FOUND || Next!=Candidates[1]) return 9;
	xcronscheduler* Scheduler=NULL; xcronschedulerconfig Config={1,2,32,0,65536};
	xcronjobconfig Job={.Expression=ZonedExpression,.Zone=Zone,.Begin=Candidates[0],.End=Candidates[1],.Overlap=XCRON_OVERLAP_ALLOW,.RunningLimit=2};
	xcronjobid JobId; xcronrun Runs[2]; xcronpoll Poll; xcronretired Receipt; bool Found=false;
	if (!xcronSchedulerCreate(&Config,&Scheduler,NULL) || !xcronSchedulerAdd(Scheduler,&Job,&JobId,NULL) ||
		!xcronSchedulerPoll(Scheduler,(xcronclock){Candidates[1],0},Runs,2,&Poll,NULL) || Poll.Count!=2 || Runs[0].ScheduledAt!=Candidates[0] || Runs[1].ScheduledAt!=Candidates[1]) return 10;
	xexecutorconfig PoolConfig={1,4,0}; xexecutor* Pool=xrtExecutorCreate(&PoolConfig); xcronexecutor* Adapter=NULL;
	xcronexecutorconfig ExecutionConfig={Pool,consumerExecute,NULL,NULL,NULL,2,0,65536};
	if (!Pool || !xcronExecutorCreate(&ExecutionConfig,&Adapter,NULL) || !xcronExecutorSubmit(Adapter,&Runs[0],NULL) || !xcronExecutorSubmit(Adapter,&Runs[1],NULL)) return 11;
	unsigned Returned=0; double Limit=xrtTimer()+10;
	while (Returned<2) {xcroncompletion Completion; size_t N;
		if (!xcronExecutorDrain(Adapter,&Completion,1,&N,NULL) || xrtTimer()>Limit) return 12;
		if (!N) {xrtThreadYield();continue;}
		if (Completion.Result!=XCRON_RUN_SUCCEEDED || Completion.Error || !xcronSchedulerComplete(Scheduler,Completion.Run.Id,Completion.Result,NULL)) return 13;
		++Returned;
	}
	if (!xrtExecutorClose(Pool) || xrtExecutorWaitFor(Pool,10000)!=XWAIT_OK || !xcronExecutorDestroy(Adapter,NULL) || !xrtExecutorDestroy(Pool) ||
		!xcronSchedulerReap(Scheduler,&Receipt,&Found,NULL) || !Found || Receipt.Id!=JobId || Receipt.Info.State!=XCRON_JOB_EXHAUSTED ||
		!xcronSchedulerDestroy(Scheduler,NULL)) return 10;
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
	if (!consumerService()) return 20;
	return 0;
}
