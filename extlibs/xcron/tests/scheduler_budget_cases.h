#include "../src/scheduler/scheduler_internal.h"
static xtime schedulerStamp(int Year,int Month,int Day,int Hour,int Minute)
{xtime T;testRequire(xrtDateTime(Year,Month,Day,Hour,Minute,0,0,&T),"bounded scheduler date fixture");return T;}
static xcronpollbudgeted schedulerBudget(xcronscheduler* S,xcronclock Now,xcronzonedbudget Budget,xcronrun* Runs,size_t Capacity,uint32 Work)
{
    xcronpollbudgeted P;testRequire(xcronSchedulerPollBudgeted(S,Now,Budget,Runs,Capacity,&P,NULL),"bounded scheduler poll");
    testRequire(P.Poll.Work<=Work && P.Poll.Count<=Capacity && P.Used.Segments<=Budget.Segments && P.Used.Months<=Budget.Months && P.Used.RuleYears<=Budget.RuleYears,"global traversal/action/output budgets");return P;
}
static void testSchedulerBudgetFairness(void)
{
    /* One actual prefix occurrence, then400 years of annual missing labels. */
    tzifblob Blob;int64 Times[]={(schedulerStamp(2000,1,1,0,0)-XRT_TIME_UNIX_EPOCH)/1000};uint8 Indices[]={0};tzifkind Kinds[]={{0,false,"STD"}};
    tzifBuild(&Blob,'3',Times,Indices,1,Kinds,1,"STD0DST,J60/2,J300/2");xcronzone* Z=NULL;
    testRequire(xcronZoneCreateTzif((xbytesview){Blob.Data,Blob.Size},&Z,NULL),"bounded scheduler history");
    xcronscheduler* S=schedulerCreate(9,9,1);xtime Begin=schedulerStamp(1999,3,1,2,30);
    xcronjobconfig Heavy=schedulerConfig(Z,Begin,INT64_MAX);Heavy.Expression=schedulerExpression("0 30 2 1 MAR *");
    xcronjobid Ids[9];for(unsigned I=0;I<8;++I)Ids[I]=schedulerAdd(S,&Heavy);
    xcronjobconfig Light=schedulerConfig(Z,Begin,Begin);Ids[8]=schedulerAdd(S,&Light);xcronrun Leases[9];unsigned Count=0;bool SeenLight=false;
    for(unsigned Turn=0;Turn<60 && !SeenLight;++Turn){xcronrun R;
        xcronpollbudgeted P=schedulerBudget(S,(xcronclock){Begin,Turn},(xcronzonedbudget){1,1,1},&R,1,1);
        if(P.Poll.Count){Leases[Count++]=R;if(R.Job==Ids[8])SeenLight=true;}
    }
    testRequire(SeenLight && Count==9,"pending heavy successors cannot starve the later light ready job");
    xcronjobinfo Info=schedulerInfo(S,Ids[0]);testRequire(Info.State==XCRON_JOB_SEARCHING && !Info.HasNext && Info.Running==1,"lease remains valid during unknown successor calculation");
    xcronwake W;testRequire(xcronSchedulerWake(S,(xcronclock){Begin,100},1000,&W,NULL) && W.Immediate && W.WaitingForCompletion,"useful calculation wake even with every run slot full");
    uint64 Traversed=0;
    for(unsigned Turn=0;Turn<16;++Turn){xcronpollbudgeted P=schedulerBudget(S,(xcronclock){Begin,100+Turn},(xcronzonedbudget){2,3,4},NULL,0,1);Traversed+=P.Used.Segments+P.Used.Months+P.Used.RuleYears;testRequire(!P.Poll.Count && P.Searching,"zero output still advances pending work at full lease capacity");}
    testRequire(Traversed>0,"calculation-only turns do real traversal");
    schedulerComplete(S,Leases[0].Id);Info=schedulerInfo(S,Leases[0].Job);testRequire(Info.State==XCRON_JOB_SEARCHING && !Info.Running,"completion does not retire a pending successor");
    xcronretired Retired;bool Found=true;testRequire(xcronSchedulerReap(S,&Retired,&Found,NULL) && !Found,"searching zero-running job cannot be reaped");
    testRequire(xcronSchedulerCancel(S,Leases[0].Job,NULL),"cancel removes query immediately");Retired=schedulerReap(S);testRequire(Retired.Id==Leases[0].Job && Retired.Info.State==XCRON_JOB_CANCELLED,"cancelled query retires without searching remaining history");
    for(unsigned I=1;I<Count;++I)schedulerComplete(S,Leases[I].Id);
    schedulerClean(S);xcronZoneRelease(Z);
}
static void testSchedulerAdmission(void)
{
    xcronscheduler* S=schedulerCreate(2,1,1);xcronzone* Z=schedulerZone("STD0DST,J60/2,J300/2"),*UTC=schedulerZone("UTC0");
    xcronjobconfig Heavy=schedulerConfig(Z,schedulerStamp(2000,1,1,0,0),INT64_MAX);Heavy.Expression=schedulerExpression("0 30 2 1 MAR *");xcronjobid H,L;
    testRequire(xcronSchedulerBeginAdmission(S,&Heavy,&H,NULL),"provisional heavy admission");xcronschedulerissue I;xcronjobinfo Info={.Next=777};
    testRequire(!xcronSchedulerJob(S,H,&Info,&I) && I.Code==XCRON_SCHEDULER_STALE && Info.Next==777,"unpublished IDs are not public jobs");xrtClearError();
    testRequire(!xcronSchedulerCancel(S,H,&I) && I.Code==XCRON_SCHEDULER_STALE,"guessed provisional IDs cannot cancel an admission");xrtClearError();
    xcronjobconfig Light=schedulerConfig(UTC,0,0);testRequire(xcronSchedulerBeginAdmission(S,&Light,&L,NULL),"provisional light admission");xcronadmissionstatus Status=XCRON_ADMISSION_PENDING;
    for(unsigned T=0;T<12 && Status==XCRON_ADMISSION_PENDING;++T){xcronrun R;xcronpollbudgeted P=schedulerBudget(S,(xcronclock){0,T},(xcronzonedbudget){1,1,1},&R,1,1);testRequire(!P.Poll.Count,"provisional jobs never dispatch before commit");Status=xcronSchedulerFinishAdmission(S,L,false,NULL);}
    testRequire(Status==XCRON_ADMISSION_ACCEPTED && xcronSchedulerFinishAdmission(S,H,false,NULL)==XCRON_ADMISSION_PENDING,"heavy admission cannot monopolize a later one");
    xcronrun R;bool Dispatched=false;
    for(unsigned T=0;T<12 && !Dispatched;++T){xcronpollbudgeted P=schedulerBudget(S,(xcronclock){0,20+T},(xcronzonedbudget){1,1,1},&R,1,1);Dispatched=P.Poll.Count!=0;}
    testRequire(Dispatched && R.Job==L,"commit publishes the actual light lease");schedulerComplete(S,R.Id);schedulerReap(S);
    testRequire(xcronSchedulerFinishAdmission(S,H,true,&I)==XCRON_ADMISSION_REJECTED && I.Code==XCRON_SCHEDULER_CLOSED,"withdraw releases reserved job without traversal");xrtClearError();
    xcronschedulerstats Stats;testRequire(xcronSchedulerInspect(S,&Stats,NULL) && !Stats.Jobs && !Stats.Running,"admission ownership released exactly once");schedulerClean(S);xcronZoneRelease(Z);xcronZoneRelease(UTC);
    tzifblob Blob;tzifkind Unknown[]={{0,false,"-00"}};tzifBuild(&Blob,0,NULL,NULL,0,Unknown,1,"");Z=NULL;testRequire(xcronZoneCreateTzif((xbytesview){Blob.Data,Blob.Size},&Z,NULL),"unknown admission zone");S=schedulerCreate(1,1,1);Heavy=schedulerConfig(Z,0,1000);
    testRequire(xcronSchedulerBeginAdmission(S,&Heavy,&H,NULL),"deferred unknown mapping");schedulerBudget(S,(xcronclock){0,0},(xcronzonedbudget){1,1,1},NULL,0,1);
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"admission diagnostic OOM");
    testRequire(xcronSchedulerFinishAdmission(S,H,false,&I)==XCRON_ADMISSION_REJECTED && I.Code==XCRON_SCHEDULER_CALCULATION && I.Zoned.Zone==XCRON_ZONE_UNKNOWN && xrtErrorKind(xrtGetError())==XERR_MEMORY,"transactional deferred failure keeps typed cause under OOM");
    testRequire(xrtAllocationBudgetLeave(&Scope),"admission OOM leave");xrtClearError();xrtAllocationBudgetFree(B);
    testRequire(xcronSchedulerInspect(S,&Stats,NULL) && !Stats.Jobs && !Stats.Faults,"failed initial calculation admits nothing and creates no job fault/receipt");schedulerClean(S);xcronZoneRelease(Z);
}
static void testSchedulerBudgetArguments(void)
{
    xcronscheduler* S=schedulerCreate(1,1,1);xcronpollbudgeted P,Before;memset(&P,0xa5,sizeof(P));Before=P;xcronschedulerissue I={0},Old=I;
    for(unsigned N=0;N<3;++N){xcronzonedbudget B={1,1,1};if(N==0)B.Segments=0;else if(N==1)B.Months=0;else B.RuleYears=0;
        testRequire(!xcronSchedulerPollBudgeted(S,(xcronclock){0,0},B,NULL,0,&P,&I) && !memcmp(&P,&Before,sizeof(P)) && !memcmp(&I,&Old,sizeof(I)),"zero global budget rejects atomically");xrtClearError();}
    testRequire(!xcronSchedulerPollBudgeted(S,(xcronclock){0,0},(xcronzonedbudget){1,1,1},(xcronrun*)&P,1,&P,&I) && !memcmp(&P,&Before,sizeof(P)),"new report/run alias");xrtClearError();
    schedulerBudget(S,(xcronclock){0,10},(xcronzonedbudget){1,1,1},NULL,0,1);
    testRequire(!xcronSchedulerPollBudgeted(S,(xcronclock){0,9},(xcronzonedbudget){1,1,1},NULL,0,&P,&I) && I.Code==XCRON_SCHEDULER_CLOCK && !memcmp(&P,&Before,sizeof(P)),"budgeted stale clock preserves report");xrtClearError();schedulerClean(S);
}
static void testSchedulerBudgetFaultsAndWarm(void)
{
    tzifblob Blob;int64 Times[]={0,2};uint8 Indices[]={0,0};tzifkind Types[]={{0,false,"UTC"}};
    tzifBuild(&Blob,'3',Times,Indices,2,Types,1,"");xcronzone* Z=NULL;testRequire(xcronZoneCreateTzif((xbytesview){Blob.Data,Blob.Size},&Z,NULL),"bounded future-fault fixture");
    xcronscheduler* S=schedulerCreate(1,4,4);xcronjobconfig C=schedulerConfig(Z,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH+10000);C.Overlap=XCRON_OVERLAP_ALLOW;C.RunningLimit=4;xcronjobid Id=schedulerAdd(S,&C);
    xcronrun Runs[4];unsigned Count=0,Faults=0;
    for(unsigned Turn=0;Turn<16 && !Faults;++Turn){xcronpollbudgeted P=schedulerBudget(S,(xcronclock){XRT_TIME_UNIX_EPOCH+10000,Turn},(xcronzonedbudget){1,1,1},Runs+Count,4-Count,4);Count+=(unsigned)P.Poll.Count;Faults+=P.Poll.Faults;}
    xcronjobinfo Info=schedulerInfo(S,Id);testRequire(Count==2 && Faults==1 && Info.State==XCRON_JOB_FAULTED && Info.Running==2 && Info.LastIssue.Zone==XCRON_ZONE_UNKNOWN,"deferred future fault preserves both published physical leases");
    for(unsigned I=0;I<Count;++I)schedulerComplete(S,Runs[I].Id);
    schedulerClean(S);xcronZoneRelease(Z);
    Z=schedulerZone("UTC0");S=schedulerCreate(1,1,1);xtime Begin=schedulerStamp(2032,2,29,0,0);
    C=schedulerConfig(Z,Begin,schedulerStamp(2041,1,1,0,0));C.Expression=schedulerExpression("0 0 0 29 FEB */7");
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget),"warm bounded budget");xrtSetErrorInfo(XERR_VALUE,"bounded.ambient",2,"keep");const xerror* Ambient=xrtGetError();testRequire(xrtAllocationBudgetEnter(Budget,&Scope),"enter bounded warm scope");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
#endif
    unsigned Turns=0;
    for(unsigned Iter=0;Iter<128;++Iter){schedulerAdd(S,&C);xcronrun R;bool Published=false;
        do{xcronpollbudgeted P=schedulerBudget(S,(xcronclock){Begin,Iter},(xcronzonedbudget){1,1,1},&R,1,1);Published=P.Poll.Count!=0;++Turns;}while(!Published);
        xcronpollbudgeted P;do{P=schedulerBudget(S,(xcronclock){Begin,Iter},(xcronzonedbudget){1,1,1},NULL,0,1);++Turns;}while(P.Searching);
        schedulerComplete(S,R.Id);schedulerReap(S);testRequire(xrtGetError()==Ambient,"real sparse resumptions preserve ambient diagnostic");
    }
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(Before.MallocCalls==After.MallocCalls && Before.CallocCalls==After.CallocCalls && Before.ReallocCalls==After.ReallocCalls && Before.BlockAllocCalls==After.BlockAllocCalls,"bounded sparse lifecycle requests no heap");
#endif
    testRequire(Turns>10000 && xrtAllocationBudgetLeave(&Scope),"many real month yields under one-byte quota");xrtClearError();xrtAllocationBudgetFree(Budget);schedulerClean(S);xcronZoneRelease(Z);
}
static void testSchedulerBudgetCredit(void)
{
    xcronzone* Z=schedulerZone("UTC0");xcronscheduler* S=schedulerCreate(2,1,3);xcronjobconfig Hot=schedulerConfig(Z,0,INT64_MAX);schedulerAdd(S,&Hot);
    xcronjobconfig Later=schedulerConfig(Z,100001,200000);xcronjobid Pending;testRequire(xcronSchedulerBeginAdmission(S,&Later,&Pending,NULL),"pending admission beside infinite dense catch-up");
    xcronadmissionstatus Status=XCRON_ADMISSION_PENDING;
    for(unsigned Turn=0;Turn<8 && Status==XCRON_ADMISSION_PENDING;++Turn){xcronrun R;
        xcronpollbudgeted P=schedulerBudget(S,(xcronclock){100000,Turn},(xcronzonedbudget){1,1,1},&R,1,3);
        if(P.Poll.Count)schedulerComplete(S,R.Id);
        Status=xcronSchedulerFinishAdmission(S,Pending,false,NULL);
    }
    testRequire(Status==XCRON_ADMISSION_ACCEPTED,"eager ready probes must reserve traversal credit for an older query at Capacity=1/WorkLimit=3");schedulerClean(S);xcronZoneRelease(Z);
}
