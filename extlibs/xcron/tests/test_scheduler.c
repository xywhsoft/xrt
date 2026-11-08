#include "test.h"
#include "tzif_fixture.h"
#include <limits.h>
static bool SchedulerBudgetModel;
static xcronexpression schedulerExpression(cstr Text)
{xcronexpression E;testRequire(xcronExpressionCompile((xstrview){Text,strlen(Text)},XCRON_SECONDS6,&E,NULL),"scheduler expression");return E;}
static xcronzone* schedulerZone(cstr Text)
{xcronzone* Z=NULL;testRequire(xcronZoneCreatePosix((xstrview){Text,strlen(Text)},XCRON_ZONE_TZIF3,&Z,NULL),"scheduler zone");return Z;}
static xcronscheduler* schedulerCreate(uint32 Jobs,uint32 Runs,uint32 Work)
{xcronscheduler* S=NULL;xcronschedulerconfig C={Jobs,Runs,Work,0,SIZE_MAX};testRequire(xcronSchedulerCreate(&C,&S,NULL),"scheduler create");return S;}
static xcronjobconfig schedulerConfig(xcronzone* Z,xtime Begin,xtime End)
{return (xcronjobconfig){.Expression=schedulerExpression("* * * * * *"),.Zone=Z,.Begin=Begin,.End=End,.RunningLimit=1};}
static xcronjobid schedulerAdd(xcronscheduler* S,const xcronjobconfig* C)
{xcronjobid Id=0;testRequire(xcronSchedulerAdd(S,C,&Id,NULL) && Id,"scheduler add");return Id;}
static xcronpoll schedulerPoll(xcronscheduler* S,xtime Wall,uint64 Mono,xcronrun* R,size_t Capacity)
{
    xcronpoll P;
    if(!SchedulerBudgetModel)testRequire(xcronSchedulerPoll(S,(xcronclock){Wall,Mono},R,Capacity,&P,NULL),"scheduler poll");
    else {
        xcronpollbudgeted Q;testRequire(xcronSchedulerPollBudgeted(S,(xcronclock){Wall,Mono},(xcronzonedbudget){1,1,1},R,Capacity,&Q,NULL),"budgeted model poll");P=Q.Poll;
        unsigned Turns=0;
        while(Q.Searching){testRequire(++Turns<1000 && xcronSchedulerPollBudgeted(S,(xcronclock){Wall,Mono},(xcronzonedbudget){1,1,1},NULL,0,&Q,NULL),"calculation-only model drain");P.Faults+=Q.Poll.Faults;}
    }
    return P;
}
static xcronjobinfo schedulerInfo(xcronscheduler* S,xcronjobid Id)
{xcronjobinfo I;testRequire(xcronSchedulerJob(S,Id,&I,NULL),"scheduler job info");return I;}
static void schedulerComplete(xcronscheduler* S,xcronrunid Id)
{testRequire(xcronSchedulerComplete(S,Id,XCRON_RUN_SUCCEEDED,NULL),"physical run complete");}
static xcronretired schedulerReap(xcronscheduler* S)
{xcronretired R;bool Found=false;testRequire(xcronSchedulerReap(S,&R,&Found,NULL) && Found,"terminal job receipt");return R;}
static void schedulerClean(xcronscheduler* S)
{testRequire(xcronSchedulerClose(S,NULL),"scheduler close");xcronretired R;bool Found=true;while(Found)testRequire(xcronSchedulerReap(S,&R,&Found,NULL),"scheduler drain");testRequire(xcronSchedulerDestroy(S,NULL),"scheduler destroy");}
static void schedulerError(bool Result,xcronschedulerissue* I,xcronschedulererror Code,cstr Text)
{testRequire(!Result && I->Code==Code && xrtErrorCode(xrtGetError())==(int32)Code && !strcmp(xrtErrorDomain(xrtGetError()),"xrt.cron.scheduler"),Text);xrtClearError();}
#include "scheduler_budget_cases.h"
static void testSchedulerLifetime(void)
{
    xcronscheduler* S=schedulerCreate(1,2,64);xcronzone* Z=schedulerZone("UTC0");int Data=37;
    xcronjobconfig C=schedulerConfig(Z,0,3000);C.Data=&Data;xcronjobid Id=schedulerAdd(S,&C);xcronZoneRelease(Z);C.Expression.Fields[0]=0;
    xcronrun R[2];xcronpoll P=schedulerPoll(S,0,0,R,2);testRequire(P.Count==1 && R[0].Job==Id && R[0].Data==&Data && !R[0].ScheduledAt && !R[0].Flags && !P.More,"copied expression and retained zone dispatch");
    xcronjobinfo J=schedulerInfo(S,Id);testRequire(J.Running==1 && J.Next==1000 && J.HasNext && J.State==XCRON_JOB_SCHEDULED,"wait cursor retained while running");
    xcronwake W;testRequire(xcronSchedulerWake(S,(xcronclock){10000,1},2000,&W,NULL) && W.WaitingForCompletion && !W.Immediate && W.DelayMilliseconds==2000,"per-job blocking has no immediate timer spin");
    testRequire(xcronSchedulerCancel(S,Id,NULL) && xcronSchedulerCancel(S,Id,NULL),"cancel idempotence");
    xcronschedulerissue I;schedulerError(xcronSchedulerDestroy(S,&I),&I,XCRON_SCHEDULER_BUSY,"busy destroy preserves owner");
    xcronretired Retired={.Id=999};bool Found=true;testRequire(xcronSchedulerReap(S,&Retired,&Found,NULL) && !Found && Retired.Id==999 && Data==37,"cancel cannot reclaim active data");
    schedulerComplete(S,R[0].Id);schedulerError(xcronSchedulerComplete(S,R[0].Id,XCRON_RUN_SUCCEEDED,&I),&I,XCRON_SCHEDULER_STALE,"duplicate completion rejected");
    Retired=schedulerReap(S);testRequire(Retired.Id==Id && Retired.Data==&Data && Retired.Info.State==XCRON_JOB_CANCELLED && Retired.Info.Succeeded==1 && !Retired.Info.Running,"physical completion retirement receipt");
    schedulerError(xcronSchedulerJob(S,Id,&J,&I),&I,XCRON_SCHEDULER_STALE,"reaped job ID invalid");
    Z=schedulerZone("UTC0");C=schedulerConfig(Z,0,0);xcronjobid New=schedulerAdd(S,&C);testRequire(New!=Id,"job slot generation changes");
    P=schedulerPoll(S,0,1,R,2);testRequire(P.Count==1,"reused slot dispatch");schedulerError(xcronSchedulerCancel(S,Id,&I),&I,XCRON_SCHEDULER_STALE,"old job cannot cancel reused slot");
    testRequire(xcronSchedulerClose(S,NULL) && xcronSchedulerClose(S,NULL),"close idempotence");J=schedulerInfo(S,New);testRequire(J.State==XCRON_JOB_EXHAUSTED && J.Running==1,"close preserves earlier terminal reason");
    schedulerComplete(S,R[0].Id);Retired=schedulerReap(S);testRequire(Retired.Id==New && Retired.Info.State==XCRON_JOB_EXHAUSTED,"exhaustion waits for physical completion");
    schedulerError(xcronSchedulerAdd(S,&C,&New,&I),&I,XCRON_SCHEDULER_CLOSED,"closed add rejected");
    testRequire(xcronSchedulerDestroy(S,NULL),"destroy drained owner");
    S=schedulerCreate(1,1,64);C=schedulerConfig(Z,0,10000);Id=schedulerAdd(S,&C);P=schedulerPoll(S,0,0,R,1);testRequire(P.Count==1,"close-active lease fixture");
    testRequire(xcronSchedulerClose(S,NULL),"close scheduled active job");J=schedulerInfo(S,Id);testRequire(J.State==XCRON_JOB_CANCELLED && J.Running==1 && !J.HasNext,"close cancels future cursor but preserves physical lease");
    xcronrun Saved=R[0];P=schedulerPoll(S,10000,1,R,1);testRequire(!P.Count && !P.More,"closed scheduler cannot dispatch future work");
    testRequire(xcronSchedulerReap(S,&Retired,&Found,NULL) && !Found,"close cannot retire active job");
    testRequire(xcronSchedulerComplete(S,Saved.Id,XCRON_RUN_CANCELLED,NULL),"cancelled physical run completion");Retired=schedulerReap(S);testRequire(Retired.Info.Cancelled==1 && !Retired.Info.Succeeded && Retired.Info.State==XCRON_JOB_CANCELLED,"cancelled outcome survives close receipt");
    testRequire(xcronSchedulerDestroy(S,NULL),"close-active owner drained");xcronZoneRelease(Z);
}
static void testSchedulerMisfires(void)
{
    xcronzone* Z=schedulerZone("UTC0");
    for(int Mode=0;Mode<3;++Mode){xcronscheduler* S=schedulerCreate(1,1,64);xcronjobconfig C=schedulerConfig(Z,0,10000);C.Misfire=(xcronmisfire)Mode;C.GraceMilliseconds=1000;xcronjobid Id=schedulerAdd(S,&C);xcronrun R[1];
        xcronpoll P=schedulerPoll(S,5500,0,R,1);testRequire(P.Count==1 && R[0].ScheduledAt==(Mode==XCRON_MISFIRE_SKIP?5000:0),"explicit oldest/skip misfire instant");
        testRequire(R[0].Through==(Mode==XCRON_MISFIRE_COALESCE?5500:R[0].ScheduledAt) && R[0].Flags==(Mode==XCRON_MISFIRE_COALESCE?XCRON_RUN_COALESCED:0),"coalesced range is explicit, not an invented occurrence count");
        xcronjobinfo J=schedulerInfo(S,Id);testRequire(J.Next==(Mode==XCRON_MISFIRE_CATCH_UP?1000:6000) && J.SkippedRanges==(Mode==XCRON_MISFIRE_SKIP) && J.CoalescedRanges==(Mode==XCRON_MISFIRE_COALESCE),"misfire cursor and range counters");
        schedulerComplete(S,R[0].Id);P=schedulerPoll(S,5500,1,R,1);testRequire(P.Count==(Mode==XCRON_MISFIRE_CATCH_UP),"catch-up retains all occurrences");if(P.Count)schedulerComplete(S,R[0].Id);schedulerClean(S);
    }
    xcronscheduler* S=schedulerCreate(1,1,64);xcronjobconfig C=schedulerConfig(Z,5000,6000);C.Misfire=XCRON_MISFIRE_SKIP;C.GraceMilliseconds=1000;schedulerAdd(S,&C);xcronrun R[1];xcronpoll P=schedulerPoll(S,6000,0,R,1);testRequire(P.Count==1 && R[0].ScheduledAt==5000,"grace boundary is inclusive");schedulerComplete(S,R[0].Id);schedulerClean(S);
    S=schedulerCreate(1,1,64);C=schedulerConfig(Z,0,2500);C.Misfire=XCRON_MISFIRE_COALESCE;xcronjobid Id=schedulerAdd(S,&C);P=schedulerPoll(S,10000,0,R,1);testRequire(P.Count==1 && R[0].Through==2500 && schedulerInfo(S,Id).State==XCRON_JOB_EXHAUSTED,"coalesced range stops at job end");schedulerComplete(S,R[0].Id);schedulerClean(S);
    S=schedulerCreate(1,1,64);C=schedulerConfig(Z,INT64_MIN,INT64_MAX);C.Misfire=XCRON_MISFIRE_SKIP;C.GraceMilliseconds=UINT64_MAX;schedulerAdd(S,&C);P=schedulerPoll(S,INT64_MIN+5000,0,R,1);testRequire(P.Count==1 && R[0].ScheduledAt==INT64_MIN-INT64_MIN%1000,"full-range grace cutoff saturates without signed overflow");schedulerComplete(S,R[0].Id);schedulerClean(S);xcronZoneRelease(Z);
}
static void testSchedulerOverlapAndFairness(void)
{
    xcronzone* Z=schedulerZone("UTC0");xcronscheduler* S=schedulerCreate(1,3,64);xcronjobconfig C=schedulerConfig(Z,0,10000);C.Overlap=XCRON_OVERLAP_ALLOW;C.RunningLimit=2;xcronjobid Id=schedulerAdd(S,&C);xcronrun R[8];
    xcronpoll P=schedulerPoll(S,3500,0,R,8);testRequire(P.Count==2 && R[0].ScheduledAt==0 && R[1].ScheduledAt==1000 && schedulerInfo(S,Id).Running==2 && !P.More,"allow overlap obeys per-job cap");
    xcronrunid First=R[0].Id,Second=R[1].Id;schedulerComplete(S,First);P=schedulerPoll(S,3500,1,R,8);testRequire(P.Count==1 && R[0].ScheduledAt==2000,"completion unblocks one preserved cursor");schedulerComplete(S,Second);schedulerComplete(S,R[0].Id);schedulerClean(S);
    S=schedulerCreate(1,1,64);C=schedulerConfig(Z,0,10000);C.Overlap=XCRON_OVERLAP_SKIP;Id=schedulerAdd(S,&C);P=schedulerPoll(S,0,0,R,8);testRequire(P.Count==1,"skip overlap first lease");First=R[0].Id;
    P=schedulerPoll(S,3500,1,R,8);xcronjobinfo J=schedulerInfo(S,Id);testRequire(!P.Count && P.SkippedRanges==1 && J.Next==4000 && J.SkippedRanges==1,"active overlap skips one bounded range");
    schedulerComplete(S,First);P=schedulerPoll(S,3500,2,R,8);testRequire(!P.Count,"skipped overlap is not replayed");P=schedulerPoll(S,4000,3,R,8);testRequire(P.Count==1 && R[0].ScheduledAt==4000,"next nonoverlapping occurrence");schedulerComplete(S,R[0].Id);schedulerClean(S);
    S=schedulerCreate(12,1,1);xcronjobid Jobs[12];bool Seen[12]={false};
    for(unsigned I=0;I<12;++I){C=schedulerConfig(Z,I*1000,1000000);Jobs[I]=schedulerAdd(S,&C);}
    unsigned Total=0;
    for(unsigned Iter=0;Iter<80 && Total<12;++Iter){P=schedulerPoll(S,100000,Iter,R,1);testRequire(P.Work<=1 && P.Count<=1,"strict per-poll work budget");if(P.Count){unsigned I;for(I=0;I<12 && R[0].Job!=Jobs[I];++I){}testRequire(I<12,"fair lease belongs to admitted job");if(!Seen[I]){Seen[I]=true;++Total;}schedulerComplete(S,R[0].Id);}}
    testRequire(Total==12,"one old catch-up cursor cannot starve newly due jobs across work-limited polls");schedulerClean(S);
    S=schedulerCreate(3,1,64);for(unsigned I=0;I<3;++I){C=schedulerConfig(Z,0,10000);schedulerAdd(S,&C);}P=schedulerPoll(S,0,0,R,8);testRequire(P.Count==1 && !P.More,"global cap stops actionable ready leases");
    xcronwake W;testRequire(xcronSchedulerWake(S,(xcronclock){0,0},100,&W,NULL) && !W.Immediate && W.DelayMilliseconds==100 && W.WaitingForCompletion,"global full queue waits on completion without busy-spin");schedulerComplete(S,R[0].Id);schedulerClean(S);xcronZoneRelease(Z);
}
static void testSchedulerClocks(void)
{
    xcronzone* Z=schedulerZone("UTC0");xcronscheduler* S=schedulerCreate(1,1,1);xcronjobconfig C=schedulerConfig(Z,0,10000);xcronjobid Id=schedulerAdd(S,&C);xcronrun R[1];xcronpoll P=schedulerPoll(S,0,0,R,1);testRequire(!P.Count && P.More,"promotion budget reports continuation");
    P=schedulerPoll(S,-1000,1,R,1);testRequire(!P.Count && !P.More,"wall rollback moves future ready work back to heap");xcronwake W;testRequire(xcronSchedulerWake(S,(xcronclock){-1000,1},5000,&W,NULL) && !W.Immediate && W.MonotonicMilliseconds==1001 && W.DelayMilliseconds==1000,"wall due projected onto explicit monotonic sample");
    P=schedulerPoll(S,0,2,R,1);testRequire(!P.Count && P.More,"promotion after forward wall correction");P=schedulerPoll(S,0,2,R,1);testRequire(P.Count==1 && !R[0].ScheduledAt,"equal monotonic sample allowed");schedulerComplete(S,R[0].Id);
    P=schedulerPoll(S,-1000,3,R,1);testRequire(!P.Count && schedulerInfo(S,Id).Next==1000,"consumed cursor never rewinds");xcronschedulerstats Stats;testRequire(xcronSchedulerInspect(S,&Stats,NULL) && Stats.WallRegressions==2 && Stats.ClockSamples==5,"clock observations are explicit");
    memset(&P,0x7c,sizeof(P));xcronpoll Old=P;memset(R,0x55,sizeof(R));xcronrun OldRun=R[0];xcronschedulerissue I;
    schedulerError(xcronSchedulerPoll(S,(xcronclock){INT64_MAX,2},R,1,&P,&I),&I,XCRON_SCHEDULER_CLOCK,"monotonic rollback rejected");testRequire(!memcmp(&P,&Old,sizeof(P)) && !memcmp(&R[0],&OldRun,sizeof(OldRun)),"clock rejection is atomic");
    schedulerError(xcronSchedulerWake(S,(xcronclock){0,2},100,&W,&I),&I,XCRON_SCHEDULER_CLOCK,"wake rejects stale monotonic sample");
    testRequire(xcronSchedulerWake(S,(xcronclock){0,UINT64_MAX-2},5000,&W,NULL) && W.MonotonicMilliseconds==UINT64_MAX,"wake deadline saturates at monotonic endpoint");schedulerClean(S);xcronZoneRelease(Z);
}
static void testSchedulerFaults(void)
{
    tzifblob B;const int64 Times[]={0,2};const uint8 Indices[]={0,0};const tzifkind Types[]={{0,false,"UTC"}};tzifBuild(&B,'3',Times,Indices,2,Types,1,"");
    xcronzone* Z=NULL;testRequire(xcronZoneCreateTzif((xbytesview){B.Data,B.Size},&Z,NULL),"scheduler truncated TZif");xcronscheduler* S=schedulerCreate(2,4,64);xcronjobconfig C=schedulerConfig(Z,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH+10000);C.Overlap=XCRON_OVERLAP_ALLOW;C.RunningLimit=4;xcronjobid Id=schedulerAdd(S,&C);
    C.Begin+=2000;xcronjobid Invalid=777;xcronschedulerissue I;schedulerError(xcronSchedulerAdd(S,&C,&Invalid,&I),&I,XCRON_SCHEDULER_CALCULATION,"initial unknown coverage rejects transaction");testRequire(Invalid==777 && I.Zoned.Code==XCRON_ZONED_ZONE && I.Zoned.Zone==XCRON_ZONE_UNKNOWN,"typed initial zone issue");
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget),"fault budget");xrtSetErrorInfo(XERR_STATE,"sentinel",7,"ambient");const xerror* Ambient=xrtGetError();testRequire(xrtAllocationBudgetEnter(Budget,&Scope),"enter fault budget");
    xcronrun R[4];xcronpoll P=schedulerPoll(S,XRT_TIME_UNIX_EPOCH+3000,0,R,4);testRequire(P.Count==2 && P.Faults==1 && xrtGetError()==Ambient,"later unknown coverage preserves leases and ambient error without allocating");
    xcronjobinfo J=schedulerInfo(S,Id);testRequire(J.State==XCRON_JOB_FAULTED && J.Running==2 && J.LastIssue.Zone==XCRON_ZONE_UNKNOWN && !J.HasNext,"per-job failure is observable separately from successful poll");
    schedulerComplete(S,R[0].Id);schedulerComplete(S,R[1].Id);xcronretired Receipt=schedulerReap(S);testRequire(Receipt.Info.State==XCRON_JOB_FAULTED && Receipt.Info.LastIssue.Zone==XCRON_ZONE_UNKNOWN,"fault receipt preserves typed cause");
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave fault budget");xrtClearError();xrtAllocationBudgetFree(Budget);schedulerClean(S);xcronZoneRelease(Z);
}
static uint64 SchedulerRandom=UINT64_C(0xc5b917e37ab6d024);
static uint32 schedulerRandom(void)
{SchedulerRandom^=SchedulerRandom<<13;SchedulerRandom^=SchedulerRandom>>7;SchedulerRandom^=SchedulerRandom<<17;return (uint32)SchedulerRandom;}
static void testSchedulerHeapOrder(void)
{
    xcronzone* Z=schedulerZone("UTC0");
    for(unsigned Trial=0;Trial<20;++Trial){
        xcronscheduler* S=schedulerCreate(127,127,512);xcronjobid Ids[127];xtime Due[127];bool Live[127];
        for(unsigned I=0;I<127;++I){Due[I]=((xtime)(schedulerRandom()%201)-100)*1000;xcronjobconfig C=schedulerConfig(Z,Due[I],Due[I]);Ids[I]=schedulerAdd(S,&C);Live[I]=true;}
        for(unsigned I=0;I<127;++I)if(schedulerRandom()%3==0){testRequire(xcronSchedulerCancel(S,Ids[I],NULL),"remove arbitrary non-root heap entry");schedulerReap(S);Live[I]=false;}
        /* Reuse removed slots; equal due times now have mixed ID generations. */
        for(unsigned I=0;I<127;++I)if(!Live[I] && schedulerRandom()%2){Due[I]=((xtime)(schedulerRandom()%201)-100)*1000;xcronjobconfig C=schedulerConfig(Z,Due[I],Due[I]);Ids[I]=schedulerAdd(S,&C);Live[I]=true;}
        xcronrun R[127];xcronpoll P=schedulerPoll(S,100000,0,R,127);unsigned Count=0;for(unsigned I=0;I<127;++I)if(Live[I])++Count;
        testRequire(P.Count==Count && !P.More,"complete bounded finite heap dispatch");
        for(size_t N=0;N<P.Count;++N){unsigned Best=127;for(unsigned I=0;I<127;++I)if(Live[I] && (Best==127 || Due[I]<Due[Best] || (Due[I]==Due[Best] && Ids[I]<Ids[Best])))Best=I;
            testRequire(Best<127 && R[N].ScheduledAt==Due[Best] && R[N].Job==Ids[Best],"independent linear minimum after arbitrary heap deletion/reuse");Live[Best]=false;schedulerComplete(S,R[N].Id);}
        schedulerClean(S);
    }
    xcronZoneRelease(Z);
}
static void testSchedulerModel(void)
{
    /* Independent finite occurrence list. It contains no heap/ready queues,
     * compiled matcher, zoned search or scheduler private implementation. */
    xcronzone* Z=schedulerZone("UTC0");uint64 Samples=0;
    for(int M=0;M<3;++M)for(int O=0;O<3;++O)for(unsigned Trial=0;Trial<64;++Trial){
        xcronscheduler* S=schedulerCreate(1,3,64);xcronjobconfig C=schedulerConfig(Z,-10000,30000);C.Misfire=(xcronmisfire)M;C.Overlap=(xcronoverlap)O;C.RunningLimit=O==XCRON_OVERLAP_ALLOW?3:1;C.GraceMilliseconds=schedulerRandom()%7000;xcronjobid Id=schedulerAdd(S,&C);
        xtime Due[41];for(unsigned I=0;I<41;++I)Due[I]=-10000+(xtime)I*1000;unsigned Cursor=0,Active=0;xcronrunid Leases[3];uint64 Skips=0,Coalesces=0,Dispatched=0,Successes=0,Failures=0,Cancellations=0;
        for(unsigned Step=0;Step<160;++Step){
            if(Active && schedulerRandom()%3==0){unsigned I=schedulerRandom()%Active;xcronrunresult Outcome=(xcronrunresult)(schedulerRandom()%3);
                testRequire(xcronSchedulerComplete(S,Leases[I],Outcome,NULL),"model physical completion result");if(Outcome==XCRON_RUN_SUCCEEDED)++Successes;else if(Outcome==XCRON_RUN_FAILED)++Failures;else ++Cancellations;Leases[I]=Leases[--Active];}
            xtime Wall=(xtime)(schedulerRandom()%60001)-15000;bool Expected=false,Coalesced=false;xtime At=0,Through=0;
            if(Cursor<41 && Due[Cursor]<=Wall){
                if(O==XCRON_OVERLAP_SKIP && Active){++Skips;while(Cursor<41 && Due[Cursor]<=Wall)++Cursor;}
                else if(Active<C.RunningLimit){
                    xtime Cutoff=Wall-(xtime)C.GraceMilliseconds;
                    if(M==XCRON_MISFIRE_SKIP && Due[Cursor]<Cutoff){++Skips;while(Cursor<41 && Due[Cursor]<Cutoff)++Cursor;}
                    if(Cursor<41 && Due[Cursor]<=Wall){Expected=true;At=Due[Cursor];Coalesced=M==XCRON_MISFIRE_COALESCE && Due[Cursor]<Cutoff;Through=Coalesced?(Wall<C.End?Wall:C.End):At;
                        do{++Cursor;}while(Coalesced && Cursor<41 && Due[Cursor]<=Through);++Dispatched;if(Coalesced)++Coalesces;
                    }
                }
            }
            xcronrun R;xcronpoll P=schedulerPoll(S,Wall,Step,&R,1);testRequire(P.Count==(size_t)Expected && !P.Faults,"finite occurrence-list model dispatch decision");
            if(Expected){testRequire(R.ScheduledAt==At && R.Through==Through && R.Flags==(Coalesced?XCRON_RUN_COALESCED:0) && R.Observed.Wall==Wall && R.Observed.MonotonicMilliseconds==Step,"finite list model lease range and sample");Leases[Active++]=R.Id;}
            xcronjobinfo J=schedulerInfo(S,Id);testRequire(J.Running==Active && J.Dispatched==Dispatched && J.Succeeded==Successes && J.Failed==Failures && J.Cancelled==Cancellations && J.SkippedRanges==Skips && J.CoalescedRanges==Coalesces && J.HasNext==(Cursor<41) && (Cursor==41 || J.Next==Due[Cursor]) && J.State==(Cursor<41?XCRON_JOB_SCHEDULED:XCRON_JOB_EXHAUSTED),"finite list model cursor/state/counters");++Samples;
        }
        while(Active)schedulerComplete(S,Leases[--Active]);
        schedulerClean(S);
    }
    testRequire(Samples==92160,"complete deterministic policy model matrix");xcronZoneRelease(Z);
}
static void testSchedulerZones(void)
{
    xcronzone* Z=schedulerZone("EST5EDT,M3.2.0,M11.1.0");xtime A,B;testRequire(xrtDateTime(2024,11,3,5,30,0,0,&A) && xrtDateTime(2024,11,3,6,30,0,0,&B),"fold scheduler instants");
    xcronscheduler* S=schedulerCreate(1,2,64);xcronjobconfig C=schedulerConfig(Z,A,B);C.Expression=schedulerExpression("0 30 1 * * *");C.Overlap=XCRON_OVERLAP_ALLOW;C.RunningLimit=2;schedulerAdd(S,&C);xcronZoneRelease(Z);xcronrun R[2];xcronpoll P=schedulerPoll(S,B,0,R,2);testRequire(P.Count==2 && R[0].ScheduledAt==A && R[1].ScheduledAt==B,"scheduler preserves actual fold order");schedulerComplete(S,R[0].Id);schedulerComplete(S,R[1].Id);schedulerClean(S);
    Z=schedulerZone("UTC0");S=schedulerCreate(1,1,64);C=schedulerConfig(Z,INT64_MAX-1000,INT64_MAX);xcronjobid Id=schedulerAdd(S,&C);P=schedulerPoll(S,INT64_MAX,0,R,1);testRequire(P.Count==1 && R[0].ScheduledAt==INT64_MAX-INT64_MAX%1000 && schedulerInfo(S,Id).State==XCRON_JOB_EXHAUSTED,"last whole second exhausts without overflow");schedulerComplete(S,R[0].Id);schedulerClean(S);
    S=schedulerCreate(1,1,64);C=schedulerConfig(Z,0,INT64_MAX);C.Expression=schedulerExpression("0 0 0 30 FEB *");Id=schedulerAdd(S,&C);testRequire(schedulerInfo(S,Id).State==XCRON_JOB_EXHAUSTED,"calendar-impossible job has explicit terminal receipt");schedulerClean(S);xcronZoneRelease(Z);
}
static void testSchedulerArgumentsAndOOM(void)
{
    xcronscheduler* S=schedulerCreate(1,1,64);xcronschedulerstats Stats;testRequire(xcronSchedulerInspect(S,&Stats,NULL),"scheduler byte accounting");xcronschedulerconfig Config={1,1,64,0,Stats.Bytes-1};xcronscheduler* Out=S;xcronschedulerissue I;
    schedulerError(xcronSchedulerCreate(&Config,&Out,&I),&I,XCRON_SCHEDULER_LIMIT,"exact scheduler byte cap");testRequire(Out==S,"failed factory preserves owner output");
    Config.BytesLimit=SIZE_MAX;Config.Reserved=1;memset(&I,0x55,sizeof(I));xcronschedulerissue Old=I;testRequire(!xcronSchedulerCreate(&Config,&Out,&I) && !memcmp(&I,&Old,sizeof(I)),"invalid configuration preserves issue");xrtClearError();Config.Reserved=0;
    testRequire(!xcronSchedulerCreate(&Config,(xcronscheduler**)&Config,&I) && !memcmp(&I,&Old,sizeof(I)),"factory output/input alias");xrtClearError();
    xcronzone* Z=schedulerZone("UTC0");xcronjobconfig C=schedulerConfig(Z,0,1000);xcronjobid Id=schedulerAdd(S,&C),Sentinel=999;
    schedulerError(xcronSchedulerAdd(S,&C,&Sentinel,&I),&I,XCRON_SCHEDULER_LIMIT,"bounded job admission");testRequire(Sentinel==999,"failed add output atomicity");
    memset(&I,0x55,sizeof(I));Old=I;testRequire(!xcronSchedulerAdd(S,&C,(xcronjobid*)&C,&I) && !memcmp(&I,&Old,sizeof(I)),"job config/output alias");xrtClearError();
    xcronrun R[2];xcronpoll P={.Count=999};testRequire(!xcronSchedulerPoll(S,(xcronclock){0,0},R,2,(xcronpoll*)R,&I) && !memcmp(&I,&Old,sizeof(I)),"poll mutually overlapping output rejected");xrtClearError();
    testRequire(!xcronSchedulerPoll(S,(xcronclock){0,0},(xcronrun*)S,1,&P,&I) && P.Count==999 && !memcmp(&I,&Old,sizeof(I)),"scheduler-owned output rejected");xrtClearError();
    testRequire(!xcronSchedulerPoll(S,(xcronclock){0,0},R,SIZE_MAX,&P,&I) && P.Count==999,"poll span multiplication overflow rejected");xrtClearError();
    testRequire(!xcronSchedulerPoll(S,(xcronclock){0,0},R,0,&P,&I),"zero output capacity rejected");xrtClearError();
    schedulerError(xcronSchedulerComplete(S,0,XCRON_RUN_SUCCEEDED,&I),&I,XCRON_SCHEDULER_STALE,"zero run ID stale");testRequire(xcronSchedulerCancel(S,Id,NULL),"cancel argument fixture");schedulerReap(S);
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"create OOM budget");Out=NULL;
    testRequire(!xcronSchedulerCreate(&Config,&Out,&I) && I.Code==XCRON_SCHEDULER_MEMORY && !Out && xrtErrorKind(xrtGetError())==XERR_MEMORY,"typed factory failure survives diagnostic OOM");testRequire(xrtAllocationBudgetLeave(&Scope),"leave OOM budget");xrtClearError();xrtAllocationBudgetFree(Budget);schedulerClean(S);xcronZoneRelease(Z);
}
static void testSchedulerWarm(void)
{
    xcronscheduler* S=schedulerCreate(1,1,64);xcronzone* Z=schedulerZone("UTC0");xcronjobconfig C=schedulerConfig(Z,0,0);xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget),"warm scheduler budget");xrtSetErrorInfo(XERR_STATE,"sentinel",7,"ambient");const xerror* Ambient=xrtGetError();testRequire(xrtAllocationBudgetEnter(Budget,&Scope),"enter warm scheduler scope");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
#endif
    xcronjobid PreviousJob=0;xcronrunid PreviousRun=0;
    for(unsigned Iter=0;Iter<10000;++Iter){xcronjobid Id=schedulerAdd(S,&C);xcronrun R;xcronpoll P=schedulerPoll(S,0,Iter,&R,1);testRequire(P.Count==1 && Id!=PreviousJob && R.Id!=PreviousRun && xrtGetError()==Ambient,"repeated bounded scheduler leases preserve generations/error");PreviousJob=Id;PreviousRun=R.Id;schedulerComplete(S,R.Id);xcronretired Receipt=schedulerReap(S);testRequire(Receipt.Id==Id && Receipt.Info.Dispatched==1 && Receipt.Info.Succeeded==1,"repeated lifecycle receipt");}
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(Before.MallocCalls==After.MallocCalls && Before.CallocCalls==After.CallocCalls && Before.ReallocCalls==After.ReallocCalls && Before.BlockAllocCalls==After.BlockAllocCalls,"steady scheduler allocates no heap storage");
#endif
    testRequire(xrtGetError()==Ambient && xrtAllocationBudgetLeave(&Scope),"warm scheduler preserves error");xrtClearError();xallocationbudgetinfo Info={.Size=sizeof(Info)};testRequire(xrtAllocationBudgetInfo(Budget,&Info) && !Info.LiveBytes && !Info.LiveCount,"warm allocation balance");xrtAllocationBudgetFree(Budget);schedulerClean(S);xcronZoneRelease(Z);
}
int main(void)
{
    testSchedulerLifetime();testSchedulerMisfires();testSchedulerOverlapAndFairness();testSchedulerClocks();testSchedulerFaults();testSchedulerHeapOrder();testSchedulerModel();testSchedulerZones();testSchedulerArgumentsAndOOM();testSchedulerWarm();
    SchedulerBudgetModel=true;testSchedulerModel();SchedulerBudgetModel=false;testSchedulerBudgetFairness();testSchedulerAdmission();testSchedulerBudgetArguments();testSchedulerBudgetFaultsAndWarm();testSchedulerBudgetCredit();
    puts("[PASS] scheduler: bounded leases/misfires/overlap/fairness/clocks/92160 model samples/faults/OOM/zero-heap");return 0;
}
