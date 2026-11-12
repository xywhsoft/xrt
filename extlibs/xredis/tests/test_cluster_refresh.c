#include "test.h"
#include "support/cluster_refresh_peer.h"
static xredisclustermapinfo frMap(xredisclustermap* M)
{xredisclustermapinfo I={0};I.Size=sizeof(I);testRequire(xredisClusterMapInfo(M,&I),"retained map view");return I;}
static void frBasic(void)
{
    for(unsigned Legacy=0;Legacy<2;++Legacy){rffixture* F=rfStart(3);xredisclusterrefreshconfig C;xredisClusterRefreshConfigInit(&C);C.Format=Legacy?XREDIS_CLUSTER_LEGACY_SLOTS:XREDIS_CLUSTER_SHARDS;rfCreate(F,1,&C);
        xredisclustermap* M=(void*)(uintptr_t)1;bool Stale=false;testRequire(xredisClusterRefreshSnapshot(F->Refresh,&M,&Stale)==XREDIS_CLUSTER_ABSENT && M==(void*)(uintptr_t)1 && !Stale,"empty snapshot ownership");
        xfuture* R=rfBegin(F,false);xfuture* Same=rfBegin(F,false);testRequire(Same==R && !xrtAtomic32Load(&F->Nodes[0].Queries,XMEMORY_ACQUIRE),"cohort creation/coalescing performs no I/O");xrtFutureDestroy(Same);
        const xredisclusterrefreshresult* V=rfDrive(F,R);testRequire(V->Status==XREDIS_REFRESH_OK && V->Published && !V->StaleAtPublish && V->Epoch==1 && V->Generation==1 && V->Attempts==1 && V->QueryObserved,"fresh metadata publication");
        testRequire(xredisClusterRefreshSnapshot(F->Refresh,&M,&Stale)==XREDIS_CLUSTER_VALUE && !Stale && M==V->Map,"published immutable map");xredisclustermapinfo I=frMap(M);xredisclusternode N;testRequire(I.Epoch==1 && I.Generation==1 && I.Origin.Host.Size==9 && I.Origin.Port==F->Base->Peers[0].Address.Port && xredisClusterTopologyLookup(I.Topology,123,&N)==XREDIS_CLUSTER_VALUE && N.Endpoint==XREDIS_CLUSTER_ENDPOINT_SOURCE,"source origin retained");
        Same=rfBegin(F,false);testRequire(Same==R && rfInfo(F).CacheHits==1,"fresh start reused result");xrtFutureDestroy(Same);
        rfStop(F);testRequire(frMap(M).Generation==1 && ((const xredisclusterrefreshresult*)xrtFutureValue(R))->QueryObserved,"retained map/result outlived coordinator/cache/engine");xredisClusterMapRelease(M);xrtFutureDestroy(R);}
}
static void frInvalidation(void)
{
    rffixture* F=rfStart(3);rfCreate(F,1,NULL);xrtAtomic32Store(&F->Nodes[0].Mode,RF_HOLD,XMEMORY_RELEASE);xfuture* R=rfBegin(F,false);rfHeld(F,0);testRequire(xredisClusterRefreshInvalidate(F->Refresh),"invalidation raced native query");rfRelease(F,0);
    const xredisclusterrefreshresult* V=rfDrive(F,R);testRequire(V->Status==XREDIS_REFRESH_STALE && V->Published && V->StaleAtPublish && rfInfo(F).Dirty && rfInfo(F).PublishedEpoch==1 && rfInfo(F).DesiredEpoch==2,"old refresh cleared newer demand");
    xrtAtomic32Store(&F->Nodes[0].Mode,RF_NORMAL,XMEMORY_RELEASE);xfuture* Next=rfBegin(F,false);V=rfDrive(F,Next);testRequire(V->Status==XREDIS_REFRESH_OK && V->Epoch==2 && V->Generation==2 && !rfInfo(F).Dirty,"new demand did not refresh");
    testRequire(((const xredisclusterrefreshresult*)xrtFutureValue(R))->Status==XREDIS_REFRESH_STALE && frMap(((const xredisclusterrefreshresult*)xrtFutureValue(R))->Map).Generation==1,"old publication mutated");rfStop(F);xrtFutureDestroy(R);xrtFutureDestroy(Next);
}
static void frFailures(void)
{
    for(unsigned Kind=0;Kind<6;++Kind){rffixture* F=rfStart(3);xredisclusterrefreshconfig C;xredisClusterRefreshConfigInit(&C);if(Kind==2)C.MaxSnapshotBytes=1;if(Kind==3)C.Timeout=0;if(Kind==4)C.MaxAttempts=1;if(Kind==5)C.MaxWaits=0;
        if(Kind==2){C.MaxSnapshotBytes=1024;}
        rfCreate(F,1,&C);if(Kind==0)xrtAtomic32Store(&F->Nodes[0].Mode,RF_BAD,XMEMORY_RELEASE);if(Kind==1)xrtAtomic32Store(&F->Nodes[0].Mode,RF_DENIED,XMEMORY_RELEASE);if(Kind==4)xrtAtomic32Store(&F->Nodes[0].Mode,RF_DROP,XMEMORY_RELEASE);if(Kind==5)xrtAtomic32Store(&F->Base->Deny,1,XMEMORY_RELEASE);
        xfuture* R=rfBegin(F,false);const xredisclusterrefreshresult* V=rfDrive(F,R);xredisclusterrefreshstatus Expected=Kind==0?XREDIS_REFRESH_PROTOCOL:Kind==1?XREDIS_REFRESH_SERVER:Kind==2?XREDIS_REFRESH_LIMIT:Kind==3?XREDIS_REFRESH_TIMEOUT:Kind==4?XREDIS_REFRESH_TRANSPORT:XREDIS_REFRESH_POLICY;
        testRequire(V->Status==Expected && !V->Published && !V->Map && rfInfo(F).Dirty,"invalid/denied/bounded refresh published data");if(Kind==1)testRequire(xrtAtomic32Load(&F->Nodes[0].Slots,XMEMORY_ACQUIRE)==0,"server ACL error downgraded command");if(Kind==3 || Kind==5)testRequire(!xrtAtomic32Load(&F->Nodes[0].Queries,XMEMORY_ACQUIRE),"due/denied refresh performed I/O");rfStop(F);xrtFutureDestroy(R);}
}
static void frSeedRetry(void)
{
    for(unsigned Limited=0;Limited<2;++Limited){rffixture* F=rfStart(Limited?1:3);xredisclusterrefreshconfig C;xredisClusterRefreshConfigInit(&C);C.MaxWaits=Limited?1:1024;rfCreate(F,3,&C);{xredisclusteraddress A=nnAddress(F->Base,0);xredispool* P=NULL;testRequire(xredisClusterNodesGetPool(&A,5000,F->Base->Cache,&P)==XREDIS_CLUSTER_POOL_READY,"prewarm retry control");nnReady(P,1);xredisPoolRelease(P);}if(!Limited){for(unsigned I=1;I<3;++I){xredisclusteraddress A=nnAddress(F->Base,I);xredispool* P=NULL;testRequire(xredisClusterNodesGetPool(&A,5000,F->Base->Cache,&P)==XREDIS_CLUSTER_POOL_READY,"prewarm failover peer");nnReady(P,1);xredisPoolRelease(P);}}xrtAtomic32Store(&F->Nodes[0].Mode,RF_DROP,XMEMORY_RELEASE);xfuture* R=rfBegin(F,false);const xredisclusterrefreshresult* V=rfDrive(F,R);
        if(Limited)testRequire(V->Status==XREDIS_REFRESH_OVERLOADED && V->Attempts==1 && V->Origin.Port==F->Base->Peers[0].Address.Port && V->QueryObserved,"unaccepted seed misattributed native outcome");
        else testRequire(V->Status==XREDIS_REFRESH_OK && V->Attempts==2 && V->Origin.Port==F->Base->Peers[1].Address.Port && frMap(V->Map).Origin.Port==V->Origin.Port,"metadata seed failover");
        rfStop(F);xrtFutureDestroy(R);}
}
static void frStops(void)
{
    for(unsigned Timeout=0;Timeout<2;++Timeout){rffixture* F=rfStart(3);xredisclusterrefreshconfig C;xredisClusterRefreshConfigInit(&C);C.Timeout=Timeout?100:5000;rfCreate(F,1,&C);xredisclusteraddress Warm=nnAddress(F->Base,0);xredispool* Pool=NULL;testRequire(xredisClusterNodesGetPool(&Warm,5000,F->Base->Cache,&Pool)==XREDIS_CLUSTER_POOL_READY,"prewarm sent stop");nnReady(Pool,1);xredisPoolRelease(Pool);xrtAtomic32Store(&F->Nodes[0].Mode,RF_HOLD,XMEMORY_RELEASE);xfuture* R=rfBegin(F,false);rfHeld(F,0);
        if(!Timeout)rfClose(F);
        const xredisclusterrefreshresult* V=rfDrive(F,R);testRequire(V->Status==(Timeout?XREDIS_REFRESH_TIMEOUT:XREDIS_REFRESH_CLOSED) && !V->Published,"logical refresh stop");xredisclusterrefreshresult Copy=*V;rfRelease(F,0);rfStop(F);testRequire(!memcmp(&Copy,xrtFutureValue(R),sizeof(Copy)),"late native completion changed refresh snapshot");xrtFutureDestroy(R);}
}
typedef struct frwork {rffixture* F;xfuture* Future;} frwork;
static int32 frJoin(ptr Data)
{frwork* W=Data;testRequire(xredisClusterRefreshStart(W->F->Refresh,false,&W->Future)==XREDIS_ADMISSION_ACCEPTED,"concurrent refresh join");return 0;}
static void frConcurrent(void)
{
    rffixture* F=rfStart(3);rfCreate(F,1,NULL);xfuture* R=rfBegin(F,false);frwork W[12]={0};xthread* T[12];
    for(unsigned I=0;I<12;++I){W[I].F=F;xredisClusterRefreshRetain(F->Refresh);T[I]=xrtThreadCreate(frJoin,&W[I],0);testRequire(T[I]!=NULL,"refresh join thread");}
    for(unsigned I=0;I<12;++I){testRequire(xrtThreadWaitFor(T[I],10000)==XWAIT_OK && W[I].Future==R,"refresh cohort was duplicated");xrtThreadDestroy(T[I]);xredisClusterRefreshRelease(F->Refresh);}
    testRequire(xrtFutureCancel(W[0].Future),"observer cancellation request");
    testRequire(rfDrive(F,R)->Status==XREDIS_REFRESH_OK && xrtAtomic32Load(&F->Nodes[0].Queries,XMEMORY_ACQUIRE)==1,"coalesced refresh submitted extra queries");rfStop(F);for(unsigned I=0;I<12;++I)xrtFutureDestroy(W[I].Future);xrtFutureDestroy(R);
}
static void frKeepOldAndOOM(void)
{
    rffixture* F=rfStart(3);rfCreate(F,1,NULL);xfuture* First=rfBegin(F,false);testRequire(rfDrive(F,First)->Status==XREDIS_REFRESH_OK,"old map bootstrap");xredisclustermap* Old=NULL;bool Stale=false;testRequire(xredisClusterRefreshSnapshot(F->Refresh,&Old,&Stale)==XREDIS_CLUSTER_VALUE && !Stale,"old map ref");
    xrtAtomic32Store(&F->Nodes[0].Mode,RF_BAD,XMEMORY_RELEASE);xfuture* Bad=rfBegin(F,true);testRequire(rfDrive(F,Bad)->Status==XREDIS_REFRESH_PROTOCOL,"bad replacement reply");xredisclustermap* Current=NULL;testRequire(xredisClusterRefreshSnapshot(F->Refresh,&Current,&Stale)==XREDIS_CLUSTER_VALUE && Current==Old && Stale,"bad refresh destroyed old map");xredisClusterMapRelease(Current);xrtFutureDestroy(Bad);
    xrtAtomic32Store(&F->Nodes[0].Mode,RF_HOLD,XMEMORY_RELEASE);xfuture* OOM=rfBegin(F,false);uint32 Before=xrtAtomic32Load(&F->Nodes[0].Queries,XMEMORY_ACQUIRE);double Due=xrtTimer()+5;while(xrtAtomic32Load(&F->Nodes[0].Queries,XMEMORY_ACQUIRE)==Before){testRequire(xrtTimer()<Due,"OOM query pending");xredisClusterRefreshStep(F->Refresh);xrtSleep(1);}rfRelease(F,0);xfuture* Native=xredisClusterRefreshPending(F->Refresh);testRequire(Native && xrtFutureWaitFor(Native,5000)==XWAIT_OK,"settled native handle available");xrtFutureDestroy(Native);
    xallocationbudget* B=NULL;xallocationscope S={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&S),"refresh parser OOM scope");testRequire(xredisClusterRefreshStep(F->Refresh)==XREDIS_REFRESH_DONE,"OOM refresh parse done");testRequire(xrtAllocationBudgetLeave(&S),"refresh parser OOM leave");const xredisclusterrefreshresult* V=xrtFutureValue(OOM);testRequire(V && V->Status==XREDIS_REFRESH_MEMORY && !V->Published && V->QueryObserved && V->Query.Status==XREDIS_REQUEST_OK,"OOM parse published/erased native observation");xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes && I.Rejections,"refresh parse OOM budget");xrtAllocationBudgetFree(B);xrtClearError();
    rfStop(F);testRequire(frMap(Old).Generation==1,"retained old map after failed refreshes");xredisClusterMapRelease(Old);xrtFutureDestroy(First);xrtFutureDestroy(OOM);
}
static int32 frStepWorker(ptr Data)
{rffixture* F=Data;return (int32)xredisClusterRefreshStep(F->Refresh);}
static void frCloseProvider(void)
{
    rffixture* F=rfStart(3);rfCreate(F,1,NULL);xfuture* R=rfBegin(F,false);xrtAtomic32Store(&F->Base->Block,1,XMEMORY_RELEASE);xredisClusterRefreshRetain(F->Refresh);xthread* T=xrtThreadCreate(frStepWorker,F,0);testRequire(T!=NULL,"provider phase thread");redisPeerWait(&F->Base->Entered,1);
    testRequire(rfInfo(F).Busy,"provider not marked active");xfuture* Same=rfBegin(F,false);testRequire(Same==R,"busy provider duplicated cohort");xrtFutureDestroy(Same);xfuture* Closed=xredisClusterRefreshClosed(F->Refresh);xredisClusterRefreshClose(F->Refresh);
    testRequire(xrtFutureState(Closed)==XFUTURE_PENDING && xrtFutureState(R)==XFUTURE_PENDING,"close escaped provider phase");xrtAtomic32Store(&F->Base->Continue,1,XMEMORY_RELEASE);
    testRequire(xrtThreadWaitFor(T,10000)==XWAIT_OK && xrtThreadExitCode(T)==XREDIS_REFRESH_DONE && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"provider phase did not finish close");xrtThreadDestroy(T);xredisClusterRefreshRelease(F->Refresh);
    const xredisclusterrefreshresult* V=xrtFutureValue(R);testRequire(V && V->Status==XREDIS_REFRESH_CLOSED && !V->Attempts && !xrtAtomic32Load(&F->Nodes[0].Queries,XMEMORY_ACQUIRE),"sealed provider sent metadata");xrtFutureDestroy(Closed);rfStop(F);xrtFutureDestroy(R);
}
static void frValidate(void)
{
    rffixture* F=rfStart(3);xredisclusteraddress A=nnAddress(F->Base,0);A.Slot=0;
    for(unsigned Kind=0;Kind<11;++Kind){xredisclusterrefreshconfig C;xredisClusterRefreshConfigInit(&C);xredisclusteraddress Seed=A;
        switch(Kind){case 0:C.MaxSeeds=0;break;case 1:C.MaxSeeds=1025;break;case 2:C.MaxHostBytes=0;break;case 3:C.MaxControlBytes=1;break;case 4:C.MaxSnapshotBytes=1;break;case 5:C.MaxAttempts=0;break;case 6:C.MaxWaits=65537;break;case 7:C.Timeout=-1;break;case 8:C.Format=0;break;case 9:Seed.Slot=1;break;default:Seed.Host=(xbytesview)XRT_BYTES_LITERAL("?");break;}
        xredisclusterrefresh* Out=(void*)(uintptr_t)1;testRequire(xredisClusterRefreshCreate(F->Base->Cache,&Seed,1,&C,&Out)==XREDIS_ADMISSION_ERROR && Out==(void*)(uintptr_t)1,"invalid refresh bounds/seed/output");xrtClearError();}
    rfCreate(F,1,NULL);testRequire(!xredisClusterRefreshInfo(F->Refresh,(void*)F->Refresh),"refresh info overwrote control");xrtClearError();xfuture* R=rfBegin(F,false);testRequire(rfDrive(F,R)->Status==XREDIS_REFRESH_OK,"validation snapshot");const xredisclusterrefreshresult* V=xrtFutureValue(R);testRequire(!xredisClusterMapInfo(V->Map,(void*)V->Map),"map info overwrote immutable map");xrtClearError();
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"snapshot no allocation scope");xredisclustermap* Map=NULL;bool Stale=true;testRequire(xredisClusterRefreshSnapshot(F->Refresh,&Map,&Stale)==XREDIS_CLUSTER_VALUE && Map==V->Map && !Stale,"snapshot acquired under exhausted budget");xredisClusterMapRelease(Map);testRequire(xrtAllocationBudgetLeave(&Scope),"snapshot scope leave");xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);testRequire(xrtAllocationBudgetInfo(B,&Info) && !Info.PeakBytes && !Info.Rejections,"snapshot getter allocated");xrtAllocationBudgetFree(B);rfStop(F);xrtFutureDestroy(R);
}
static void frColdSeeds(void)
{
    rffixture* F=rfStart(3);rfCreate(F,3,NULL);xfuture* R=rfBegin(F,false);const xredisclusterrefreshresult* V=rfDrive(F,R);testRequire(V->Status==XREDIS_REFRESH_OK && V->Attempts==1,"cold seeds did not bootstrap");uint32 Total=0;for(unsigned I=0;I<3;++I)Total+=xrtAtomic32Load(&F->Nodes[I].Queries,XMEMORY_ACQUIRE);testRequire(Total==1,"cold setup replayed metadata query");rfStop(F);xrtFutureDestroy(R);
}
static void frBudgetZero(xallocationbudget* B)
{double Due=xrtTimer()+10;for(;;){xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && xrtTimer()<Due,"refresh budget retirement");if(!I.LiveBytes && !I.LiveCount)return;xrtSleep(1);}}
static void frConstructorOOM(void)
{
    rffixture* F=rfStart(3);unsigned Failed=0,Accepted=0;uint32 Auth=xrtAtomic32Load(&F->Base->Auth,XMEMORY_ACQUIRE);
    for(size_t Limit=1;Limit<=8192;Limit+=127){xredisclusterrefreshconfig C;xredisClusterRefreshConfigInit(&C);xredisclusteraddress A=nnAddress(F->Base,0);A.Slot=0;xallocationbudget* B=NULL;xallocationscope S={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&S),"refresh constructor OOM scope");xredisclusterrefresh* Out=(void*)(uintptr_t)1;
        xredisadmission Status=xredisClusterRefreshCreate(F->Base->Cache,&A,1,&C,&Out);testRequire(xrtAllocationBudgetLeave(&S),"constructor OOM leave");
        if(Status==XREDIS_ADMISSION_ACCEPTED){++Accepted;F->Refresh=Out;rfClose(F);xredisClusterRefreshRelease(Out);}else{++Failed;testRequire(Out==(void*)(uintptr_t)1,"failed constructor changed output");xrtClearError();}frBudgetZero(B);xrtAllocationBudgetFree(B);}
    testRequire(Failed && Accepted && xrtAtomic32Load(&F->Base->Auth,XMEMORY_ACQUIRE)==Auth,"constructor OOM ownership/inline policy");Failed=Accepted=0;
    for(size_t Limit=1;Limit<=8192;Limit+=127){rfCreate(F,1,NULL);xallocationbudget* B=NULL;xallocationscope S={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&S),"cohort OOM scope");xfuture* Out=(void*)(uintptr_t)1;xredisadmission Status=xredisClusterRefreshStart(F->Refresh,false,&Out);testRequire(xrtAllocationBudgetLeave(&S),"cohort OOM leave");
        if(Status==XREDIS_ADMISSION_ACCEPTED){++Accepted;rfClose(F);testRequire(((const xredisclusterrefreshresult*)xrtFutureValue(Out))->Status==XREDIS_REFRESH_CLOSED,"unstepped cohort stop");xrtFutureDestroy(Out);}else{++Failed;testRequire(Out==(void*)(uintptr_t)1 && !rfInfo(F).Active && !rfInfo(F).Busy,"failed cohort ownership/state");rfClose(F);xrtClearError();}
        xredisClusterRefreshRelease(F->Refresh);frBudgetZero(B);xrtAllocationBudgetFree(B);}
    testRequire(Failed && Accepted && xrtAtomic32Load(&F->Base->Auth,XMEMORY_ACQUIRE)==Auth,"cohort OOM/no inline policy");F->Refresh=NULL;nnClose(F->Base);xredisClusterNodesRelease(F->Base->Cache);nnStop(F->Base);xrtFree(F);
}
#if defined(XREDIS_REFRESH_WHITEBOX)
static void frParseDeadline(void)
{
    rffixture* F=rfStart(3);rfCreate(F,1,NULL);xrtAtomic32Store(&F->Nodes[0].Mode,RF_HOLD,XMEMORY_RELEASE);xfuture* R=rfBegin(F,false);rfHeld(F,0);rfRelease(F,0);xfuture* Native=xredisClusterRefreshPending(F->Refresh);testRequire(Native && xrtFutureWaitFor(Native,5000)==XWAIT_OK,"metadata ready before parse clock probe");xrtFutureDestroy(Native);
    frClockAfter=F->Refresh->Work->Deadline+1;frClockTicks=2;xredisclusterrefreshstep S=xredisClusterRefreshStep(F->Refresh);frClockTicks=0;
    const xredisclusterrefreshresult* V=xrtFutureValue(R);testRequire(S==XREDIS_REFRESH_DONE && V && V->Status==XREDIS_REFRESH_TIMEOUT && !V->Published && !V->Map && V->QueryObserved && V->Query.Status==XREDIS_REQUEST_OK && !rfInfo(F).Generation && rfInfo(F).Dirty,"parse crossing original deadline published a successful map");rfStop(F);xrtFutureDestroy(R);
}
typedef struct frdeadlineprobe {rffixture* Fixture;xatomic32 Done;} frdeadlineprobe;
static void frInspectDeadline(xnetworker* Worker,ptr Data)
{
    (void)Worker;frdeadlineprobe* P=Data;rffixture* F=P->Fixture;xredisclient* Client=xrtAtomicPtrLoad(&F->Nodes[1].Client,XMEMORY_ACQUIRE);xredisrequest* Q=Client->SentHead;xredisrefreshwork* W=F->Refresh->Work;
    testRequire(Q && W && W->Result.Attempts==2 && Q->Deadline==W->Deadline && W->Deadline==xredisClientDeadline(W->Entered,F->Refresh->Config.Timeout),"metadata failover resets original deadline");xrtAtomic32Store(&P->Done,1,XMEMORY_RELEASE);
}
static void frDeadlineExact(void)
{
    rffixture* F=rfStart(3);xredisclusterrefreshconfig C;xredisClusterRefreshConfigInit(&C);C.Timeout=10001;rfCreate(F,2,&C);xredisclusteraddress A=nnAddress(F->Base,0);xredispool* P=NULL;testRequire(xredisClusterNodesGetPool(&A,5000,F->Base->Cache,&P)==XREDIS_CLUSTER_POOL_READY,"prewarm deadline failover");nnReady(P,1);xredisPoolRelease(P);A=nnAddress(F->Base,1);P=NULL;testRequire(xredisClusterNodesGetPool(&A,5000,F->Base->Cache,&P)==XREDIS_CLUSTER_POOL_READY,"prewarm second deadline seed");nnReady(P,1);xredisPoolRelease(P);xrtAtomic32Store(&F->Nodes[0].Mode,RF_DROP,XMEMORY_RELEASE);xrtAtomic32Store(&F->Nodes[1].Mode,RF_HOLD,XMEMORY_RELEASE);xfuture* R=rfBegin(F,false);rfHeld(F,1);frdeadlineprobe Probe={F,{0}};
    testRequire(xrtNetEnginePost(F->Base->Peers[0].Engine,0,frInspectDeadline,&Probe),"metadata deadline owner probe");redisPeerWait(&Probe.Done,1);rfClose(F);rfRelease(F,1);rfStop(F);xrtFutureDestroy(R);
}
#endif
int main(void)
{frValidate();frBasic();frColdSeeds();frInvalidation();frFailures();frSeedRetry();frStops();frConcurrent();frKeepOldAndOOM();frCloseProvider();frConstructorOOM();
#if defined(XREDIS_REFRESH_WHITEBOX)
frParseDeadline();frDeadlineExact();
#endif
puts("[PASS] Cluster refresh: real metadata queries, coalesced futures, immutable generations, invalidation epochs, deadlines/close, seed failover and OOM");return 0;}
