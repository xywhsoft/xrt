typedef struct servicebudgetclock {
    servicefixture* Fixture;
    xatomic32 Entered,Credits,All;
} servicebudgetclock;
static bool serviceBudgetClock(xcronclock* Clock,ptr Raw)
{
    servicebudgetclock* C=Raw;(void)xrtAtomic32FetchAdd(&C->Entered,1,XMEMORY_RELEASE);double End=xrtTimer()+10;
    for(;;){
        if(xrtAtomic32Load(&C->All,XMEMORY_ACQUIRE))break;
        uint32 Credit=xrtAtomic32Load(&C->Credits,XMEMORY_ACQUIRE);
        if(Credit && xrtAtomic32CompareExchange(&C->Credits,&Credit,Credit-1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE))break;
        testRequire(xrtTimer()<End,"bounded-turn clock release");xrtThreadYield();
    }
    *Clock=(xcronclock){(xtime)xrtAtomic64Load(&C->Fixture->Wall,XMEMORY_ACQUIRE),xrtAtomic64Load(&C->Fixture->Mono,XMEMORY_ACQUIRE)};return true;
}
static void serviceBudgetTurn(servicebudgetclock* Clock)
{
    uint32 Entered=xrtAtomic32Load(&Clock->Entered,XMEMORY_ACQUIRE);
    (void)xrtAtomic32FetchAdd(&Clock->Credits,1,XMEMORY_RELEASE);double End=xrtTimer()+10;
    while(xrtAtomic32Load(&Clock->Entered,XMEMORY_ACQUIRE)==Entered){testRequire(xrtTimer()<End,"next actual owner turn");xrtThreadYield();}
}
static void serviceBudgetCheck(xcronservice* S)
{
#if defined(XCRON_BUDGET_LEGACY_PROBE)
    /* Old libraries do not publish the appended LastPoll statistics. The
     * separate old-code witness tests the ticket/clock boundary, not new ABI. */
    (void)S;
#else
    xcronservicestats T=serviceStats(S);testRequire(T.LastPoll.Poll.Work<=1 && T.LastPoll.Used.Segments<=1 && T.LastPoll.Used.Months<=1 && T.LastPoll.Used.RuleYears<=1,"actual owner global action/segment/month/rule budgets");
#endif
}
static void testServiceBudgetedTurns(void)
{
    servicefixture F=serviceFixture();servicebudgetclock Clock={.Fixture=&F};xexecutor* Pool=servicePool(1,8);
    xcronserviceconfig C=serviceConfig(Pool,&F);C.Scheduler.JobLimit=2;C.Scheduler.RunLimit=1;C.Scheduler.WorkLimit=1;
    C.CompletionLimit=C.BatchLimit=1;C.CommandLimit=4;C.CommandWorkLimit=4;C.Clock=serviceBudgetClock;C.ClockContext=&Clock;
    testRequire(xcronServiceCreate(&C,&F.Service,NULL),"controlled bounded service create");serviceUntil(&Clock.Entered,1,"first owner callback boundary");
    xcronzone* UTC=serviceZone();xtime Wall=(xtime)xrtAtomic64Load(&F.Wall,XMEMORY_ACQUIRE);xcronjobconfig Light=serviceJob(UTC,&F,Wall,Wall);xcronrequestid First;
    testRequire(xcronServiceAddAsync(F.Service,&Light,&First,NULL),"initial light admission");
    double End=xrtTimer()+10;
    while(serviceStats(F.Service).ReadyCompletions!=1){testRequire(xrtTimer()<End,"physical initial receipt");serviceBudgetTurn(&Clock);serviceBudgetCheck(F.Service);}
    xcronreply R=serviceReply(F.Service,First);testRequire(R.Code==XCRON_SERVICE_OK,"initial light job admitted");
    /* Leave its reserved completion full. Calculation/command work must continue. */
    xcronzone* Zone=NULL;testRequire(xcronZoneCreatePosix(XRT_STR_LITERAL("STD0DST,J60/2,J300/2"),XCRON_ZONE_TZIF3,&Zone,NULL),"live periodic missing-label fixture");
    xcronjobconfig Heavy=serviceJob(Zone,&F,0,INT64_MAX);testRequire(xrtDate(2000,1,1,&Heavy.Begin) && xcronExpressionCompile(XRT_STR_LITERAL("0 30 2 1 MAR *"),XCRON_SECONDS6,&Heavy.Expression,NULL),"live400-year missing-label query");
    xcronrequestid Slow,Quick;testRequire(xcronServiceAddAsync(F.Service,&Heavy,&Slow,NULL) && xcronServiceReapAsync(F.Service,&Quick,NULL),"slow Add followed by quick FIFO command");xcronZoneRelease(Zone);
    serviceBudgetTurn(&Clock);
    testRequire(xcronServiceCollect(F.Service,Slow,0,&R,NULL)==XWAIT_TIMEOUT,"slow initial calculation stays pending at the next actual clock boundary");
    testRequire(xcronServiceCollect(F.Service,Quick,0,&R,NULL)==XWAIT_OK && R.Code==XCRON_SERVICE_OK && R.Found && R.Retired.Data==&F,"later command runs before a slow admission finishes");
    serviceBudgetTurn(&Clock);serviceBudgetCheck(F.Service);xcronservicestats T=serviceStats(F.Service);
    testRequire(T.ReadyCompletions==1 && T.LastPoll.Searching && (T.LastPoll.Used.Segments || T.LastPoll.Used.Months || T.LastPoll.Used.RuleYears),"real traversal proceeds with zero output reservations");
    xcronrequestid Later;testRequire(xcronServiceAddAsync(F.Service,&Light,&Later,NULL),"later light Add beside slow query");xcronZoneRelease(UTC);
    bool Admitted=false;
    for(unsigned Turn=0;Turn<16 && !Admitted;++Turn){serviceBudgetTurn(&Clock);serviceBudgetCheck(F.Service);xwaitresult Result=xcronServiceCollect(F.Service,Later,0,&R,NULL);
        testRequire(Result==XWAIT_TIMEOUT || Result==XWAIT_OK,"later ticket remains valid");if(Result==XWAIT_OK){testRequire(R.Code==XCRON_SERVICE_OK && R.Job && !R.Data && !R.Error,"later light Add commits transactionally");Admitted=true;}}
    testRequire(Admitted && xcronServiceCollect(F.Service,Slow,0,&R,NULL)==XWAIT_TIMEOUT,"query FIFO quanta prevent admission starvation");
    testRequire(xrtAtomic64Load(&F.Calls,XMEMORY_ACQUIRE)==1,"full completion output never publishes an unreserved body");
    testRequire(xcronServiceClose(F.Service,false,NULL),"close pending initial query");xrtAtomic32Store(&Clock.All,1,XMEMORY_RELEASE);
    testRequire(xcronServiceWaitFor(F.Service,10000,NULL)==XWAIT_OK,"join after provisional rollback");R=serviceReply(F.Service,Slow);
    testRequire(R.Code==XCRON_SERVICE_CLOSED && !R.Job && R.Data==&F && R.Error,"pending Add close returns original Data without publishing a Job");xrtErrorFree(R.Error);
    serviceClear(&F);servicePoolEnd(Pool);
    puts("[PASS] actual budgeted owner: clock-turn barriers/slow admission/quick command/zero-output progress/light fairness/transactional close");
}
static void testServiceDeferredFailure(void)
{
    servicefixture F=serviceFixture();xexecutor* E=servicePool(1,8);xcronserviceconfig C=serviceConfig(E,&F);C.Scheduler.WorkLimit=1;serviceStart(&F,&C);
    tzifblob Blob;tzifkind Kind[]={{0,false,"-00"}};tzifBuild(&Blob,0,NULL,NULL,0,Kind,1,"");xcronzone* Z=NULL;testRequire(xcronZoneCreateTzif((xbytesview){Blob.Data,Blob.Size},&Z,NULL),"deferred service unknown fixture");
    xcronjobconfig J=serviceJob(Z,&F,0,1000);xcronrequestid Q;testRequire(xcronServiceAddAsync(F.Service,&J,&Q,NULL),"asynchronous unknown initial query");xcronZoneRelease(Z);
    xcronreply R=serviceReply(F.Service,Q);testRequire(R.Code==XCRON_SERVICE_KERNEL && R.Scheduler.Code==XCRON_SCHEDULER_CALCULATION && R.Scheduler.Zoned.Zone==XCRON_ZONE_UNKNOWN && R.Data==&F && !R.Job && R.Error,"failed deferred calculation returns original Data and no admitted ID");xrtErrorFree(R.Error);
    testRequire(!serviceStats(F.Service).Kernel.Jobs && !xrtAtomic64Load(&F.Calls,XMEMORY_ACQUIRE),"failed initial mapping has no job/body/retirement duty");serviceStop(&F);serviceClear(&F);servicePoolEnd(E);
}
