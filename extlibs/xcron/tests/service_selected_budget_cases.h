typedef struct ssbudgetclock {ssfixture* Fixture;xatomic32 Entered,Credits,All;} ssbudgetclock;
static bool ssBudgetClock(xcronclock* Clock,ptr Raw)
{
    ssbudgetclock* C=Raw;xrtAtomic32FetchAdd(&C->Entered,1,XMEMORY_RELEASE);double End=xrtTimer()+10;
    for(;;){if(xrtAtomic32Load(&C->All,XMEMORY_ACQUIRE))break;uint32 N=xrtAtomic32Load(&C->Credits,XMEMORY_ACQUIRE);if(N && xrtAtomic32CompareExchange(&C->Credits,&N,N-1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE))break;testRequire(xrtTimer()<End,"selected bounded owner turn release");xrtThreadYield();}
    *Clock=(xcronclock){(xtime)xrtAtomic64Load(&C->Fixture->Wall,XMEMORY_ACQUIRE),xrtAtomic64Load(&C->Fixture->Mono,XMEMORY_ACQUIRE)};return true;
}
static void ssBudgetTurn(ssbudgetclock* C)
{
    uint32 N=xrtAtomic32Load(&C->Entered,XMEMORY_ACQUIRE);xrtAtomic32FetchAdd(&C->Credits,1,XMEMORY_RELEASE);double End=xrtTimer()+10;
    while(xrtAtomic32Load(&C->Entered,XMEMORY_ACQUIRE)==N){testRequire(xrtTimer()<End,"selected next actual clock boundary");xrtThreadYield();}
}
static void ssBudgetCheck(xcronservice* S)
{
    xcronselectedservicestats T=ssStats(S);testRequire(T.LastPoll.Poll.Work<=1 && T.LastPoll.Used.Segments<=1 && T.LastPoll.Used.Months<=1 && T.LastPoll.Used.RuleYears<=1 && T.LastPoll.Used.Checks<=1 && T.LastPoll.Used.Offsets<=1,"selected owner five GLOBAL budgets per real turn");
}
static void testSSBudgetedTurns(void)
{
    xdatetime Local;xrtTimeSplit(XRT_TIME_UNIX_EPOCH,&Local);Local.Offset=0;Local.IsDST=-1;ssfixture F=ssFixture(XRT_TIME_UNIX_EPOCH,&Local,1);ssbudgetclock Clock={.Fixture=&F};xexecutor* E=ssPool(1,8);xcronselectedserviceconfig C=ssConfig(E,&F);C.Service.Scheduler.JobLimit=3;C.Service.Scheduler.RunLimit=1;C.Service.Scheduler.WorkLimit=1;C.Service.CompletionLimit=C.Service.BatchLimit=1;C.Service.Clock=ssBudgetClock;C.Service.ClockContext=&Clock;
    testRequire(xcronServiceCreateSelected(&C,&F.Service,NULL),"controlled selected owner create");ssUntil(&Clock.Entered,1,"first selected clock boundary");xcronzone *UTC=NULL,*Zone=NULL;testRequire(xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_TZIF3,&UTC,NULL),"bounded selected UTC fixture");xcronjobconfig J=ssJob(UTC,&F,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH);testRequire(xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&J.Expression,NULL),"bounded selected light expression");J.RunningLimit=1;xcronrequestid Q;testRequire(xcronServiceAddSelectedAsync(F.Service,&J,(xcrondeliverypolicy){0,0},&Q,NULL),"bounded selected light admission");
    double End=xrtTimer()+10;while(ssStats(F.Service).Service.ReadyCompletions!=1){testRequire(xrtTimer()<End,"selected initial physical receipt");ssBudgetTurn(&Clock);ssBudgetCheck(F.Service);}xcronselectedreply R=ssReply(F.Service,Q);testRequire(!R.Code && !R.Error,"selected light admitted before full output");
    testRequire(xcronZoneCreatePosix(XRT_STR_LITERAL("STD0DST,J60/2,J300/2"),XCRON_ZONE_TZIF3,&Zone,NULL),"selected400-year missing-label fixture");xcronjobconfig Heavy=ssJob(Zone,&F,0,INT64_MAX);testRequire(xrtDate(2000,1,1,&Heavy.Begin) && xcronExpressionCompile(XRT_STR_LITERAL("0 30 2 1 MAR *"),XCRON_SECONDS6,&Heavy.Expression,NULL),"selected expensive missing-label search");Heavy.RunningLimit=1;xcronrequestid Slow,Quick;testRequire(xcronServiceAddSelectedAsync(F.Service,&Heavy,(xcrondeliverypolicy){0,0},&Slow,NULL) && xcronServiceReapAsync(F.Service,&Quick,NULL),"selected expensive Add beside quick command");xcronZoneRelease(Zone);ssBudgetTurn(&Clock);ssBudgetCheck(F.Service);
    R=ssReply(F.Service,Slow);testRequire(!R.Code && R.Job && !R.Error,"selected admission does not traverse recurrence");xcronjobid SlowJob=R.Job;R=ssReply(F.Service,Quick);testRequire(!R.Code && R.Found && R.Retired.Data==&F,"selected quick command progresses with output full");
    ssBudgetTurn(&Clock);ssBudgetCheck(F.Service);xcronselectedservicestats T=ssStats(F.Service);testRequire(T.Service.ReadyCompletions==1 && T.LastPoll.Searching && (T.LastPoll.Used.Segments || T.LastPoll.Used.Months || T.LastPoll.Used.RuleYears || T.LastPoll.Used.Checks || T.LastPoll.Used.Offsets),"selected recurrence progresses at zero output capacity");
    testRequire(xcronServiceAddSelectedAsync(F.Service,&J,(xcrondeliverypolicy){0,0},&Q,NULL),"selected later light beside expensive query");xcronZoneRelease(UTC);ssBudgetTurn(&Clock);R=ssReply(F.Service,Q);testRequire(!R.Code && R.Job && !R.Error,"selected light admission not starved");xcronjobid Light=R.Job;bool Ready=false;
    for(unsigned N=0;N<40 && !Ready;++N){xcronrequestid I;testRequire(xcronServiceInfoAsync(F.Service,Light,&I,NULL),"selected bounded Info");ssBudgetTurn(&Clock);ssBudgetCheck(F.Service);R=ssReply(F.Service,I);testRequire(!R.Code && !R.Error,"selected Info progresses during heavy traversal");Ready=R.Info.HasNext;}
    testRequire(Ready && xrtAtomic64Load(&F.Calls,XMEMORY_ACQUIRE)==1,"selected fair light query completes without unreserved dispatch");testRequire(xcronServiceInfoAsync(F.Service,SlowJob,&Q,NULL),"selected slow Info");ssBudgetTurn(&Clock);R=ssReply(F.Service,Q);testRequire(!R.Code && R.Info.State==XCRON_JOB_SEARCHING,"selected heavy query still pending after light progress");testRequire(xcronServiceClose(F.Service,false,NULL),"close expensive selected search");xrtAtomic32Store(&Clock.All,1,XMEMORY_RELEASE);testRequire(xcronServiceWaitFor(F.Service,10000,NULL)==XWAIT_OK,"selected bounded close joins");ssClear(&F);ssPoolEnd(E);
}
static void testSSFactoryArguments(void)
{
    xdatetime Local={0};ssfixture F=ssFixture(0,&Local,1);xexecutor* E=ssPool(1,8);xcronselectedserviceconfig C=ssConfig(E,&F);xcronservice* S=(xcronservice*)(uintptr_t)1;xcronserviceissue I={XCRON_SERVICE_STALE,XERR_IO},Before=I;
    const uint32 Widths[]={0,65,UINT32_MAX};for(unsigned N=0;N<3;++N){C.SubmissionBatchLimit=Widths[N];testRequire(!xcronServiceCreateSelected(&C,&S,&I) && S==(xcronservice*)(uintptr_t)1 && !memcmp(&I,&Before,sizeof(I)),"selected invalid width preserves outputs");xrtClearError();}
    C.SubmissionBatchLimit=1;C.Reserved=1;testRequire(!xcronServiceCreateSelected(&C,&S,&I) && S==(xcronservice*)(uintptr_t)1 && !memcmp(&I,&Before,sizeof(I)),"selected reserved value rejected");xrtClearError();C.Reserved=0;xcronselectedserviceconfig Saved=C;
    testRequire(!xcronServiceCreateSelected(&C,(xcronservice**)&C.Reserved,&I) && !memcmp(&C,&Saved,sizeof(C)) && !memcmp(&I,&Before,sizeof(I)),"selected factory protects full extended input span");xrtClearError();
    ssStart(&F,&C);xcronzone* Z=NULL;testRequire(xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_TZIF3,&Z,NULL),"selected invalid expression fixture");xcronjobconfig J={.Zone=Z,.RunningLimit=1,.Data=&F};xcronrequestid Q;testRequire(xcronServiceAddSelectedAsync(F.Service,&J,(xcrondeliverypolicy){0,0},&Q,NULL),"queue invalid selected expression for owner validation");xcronZoneRelease(Z);xcronselectedreply R=ssReply(F.Service,Q);testRequire(R.Code==XCRON_SERVICE_KERNEL && !R.Job && R.Data==&F && R.Error && !ssStats(F.Service).Service.Kernel.Jobs,"failed selected Add returns Data once without retirement duty");xrtErrorFree(R.Error);ssStop(&F);ssClear(&F);ssPoolEnd(E);
}
