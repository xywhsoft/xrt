#include "test.h"
#include "support/cluster_nodes_peer.h"
static void ntBasic(void)
{
    nnfixture* F=nnStart();xredisclusternodesconfig C;nnConfig(F,&C);C.ConnectionsPerNode=2;xredisclusternodes* Cache=nnCreate(F,&C);
    testRequire(!xrtAtomic32Load(&F->Calls,XMEMORY_ACQUIRE),"cache constructor started a factory");xredispool* P=NULL;xredisclusteraddress A=nnAddress(F,0);unsigned char Host[]="127.0.0.1";A.Host=(xbytesview){Host,9};
    testRequire(xredisClusterNodesGetPool(&A,5000,Cache,&P)==XREDIS_CLUSTER_POOL_READY,"cache miss creates maintained shared pool");memset(Host,'x',9);nnReady(P,2);xfuture* Saved=nnEcho(P);
    A=nnAddress(F,0);A.Slot=123;A.Reason=XREDIS_CLUSTER_ASK;xredispool* Again=NULL;
    testRequire(xredisClusterNodesGetPool(&A,5000,Cache,&Again)==XREDIS_CLUSTER_POOL_READY && Again==P,"slot/reason duplicate node identity");xredisPoolRelease(Again);
    xallocationbudget* Hot=NULL;xallocationscope HotScope={0};testRequire(xrtAllocationBudgetCreate(1,&Hot),"warm lookup budget");xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);testRequire(xrtAllocationBudgetEnter(Hot,&HotScope),"warm lookup scope");
    for(unsigned N=0;N<1000;++N){Again=NULL;testRequire(xredisClusterNodesGetPool(&A,5000,Cache,&Again)==XREDIS_CLUSTER_POOL_READY && Again==P,"warm cache lookup");xredisPoolRelease(Again);}
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(xrtAllocationBudgetLeave(&HotScope),"warm lookup scope leave");xallocationbudgetinfo HotInfo={0};HotInfo.Size=sizeof(HotInfo);testRequire(xrtAllocationBudgetInfo(Hot,&HotInfo) && !HotInfo.PeakBytes && !HotInfo.LiveBytes && !HotInfo.Rejections,"cache hit allocated or rejected caller storage");printf("[NOTE] warm lookup process allocations=%llu, caller peak=%zu/rejections=%llu\n",(unsigned long long)(After.BlockAllocCalls-Before.BlockAllocCalls),HotInfo.PeakBytes,(unsigned long long)HotInfo.Rejections);xrtAllocationBudgetFree(Hot);
    xredisclusternodesinfo I=nnInfo(Cache);testRequire(I.Nodes==1 && I.Created==1 && I.Misses==1 && I.Hits==1001 && I.ConnectionBound==6 && I.RequestBound==192 && I.CommandBytesBound==3145728,"cache aggregate bounds/counters");
    testRequire(xrtAtomic32Load(&F->Calls,XMEMORY_ACQUIRE)==2,"cache duplicate connection factories");nnClose(F);testRequire(xrtAtomic32Load(&F->Drops,XMEMORY_ACQUIRE)==1,"one context Drop");xredisPoolRelease(P);nnStop(F);
    testRequire(nnInfo(Cache).State==XREDIS_CLUSTER_NODES_CLOSED && ((const xredisrequestresult*)xrtFutureValue(Saved))->Status==XREDIS_REQUEST_OK,"closed metadata/native reply retained engine");xredisClusterNodesRelease(Cache);xrtFutureDestroy(Saved);
}
static void ntCapacity(void)
{
    nnfixture* F=nnStart();xredisclusternodesconfig C;nnConfig(F,&C);C.MaxNodes=1;xredisclusternodes* Cache=nnCreate(F,&C);xredisclusteraddress A=nnAddress(F,0),B=nnAddress(F,1);xredispool* P=NULL;
    testRequire(xredisClusterNodesGetPool(&A,5000,Cache,&P)==XREDIS_CLUSTER_POOL_READY,"one-node cache seed");nnReady(P,1);xredispool* Out=(void*)(uintptr_t)1;
    testRequire(xredisClusterNodesGetPool(&B,5000,Cache,&Out)==XREDIS_CLUSTER_POOL_WAIT && Out==(void*)(uintptr_t)1,"full cache ownership/admission");
    testRequire(xredisClusterNodesRetire(Cache,&A),"explicit node retirement");xfuture* Closed=xredisPoolClosed(P);testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"retired pool physically closed");xrtFutureDestroy(Closed);xredisPoolRelease(P);
    double Due=xrtTimer()+5;while(nnInfo(Cache).Nodes){testRequire(xrtTimer()<Due,"physical cache slot reuse");xrtSleep(1);}P=NULL;
    testRequire(xredisClusterNodesGetPool(&B,5000,Cache,&P)==XREDIS_CLUSTER_POOL_READY,"reuse retired slot");nnReady(P,1);xfuture* E=nnEcho(P);xrtFutureDestroy(E);xredisPoolRelease(P);nnClose(F);testRequire(nnInfo(Cache).Created==2 && nnInfo(Cache).Retired==2,"retirement accounting");xredisClusterNodesRelease(Cache);nnStop(F);
}
typedef struct ntwork {nnfixture* F;xredispool* Pool;xredisclusterpoolstatus Status;} ntwork;
static int32 ntLookup(ptr Data)
{ntwork* W=Data;while(!xrtAtomic32Load(&W->F->Go,XMEMORY_ACQUIRE))xrtSleep(1);xredisclusteraddress A=nnAddress(W->F,0);do{W->Status=xredisClusterNodesGetPool(&A,5000,W->F->Cache,&W->Pool);if(W->Status==XREDIS_CLUSTER_POOL_WAIT)xrtSleep(1);}while(W->Status==XREDIS_CLUSTER_POOL_WAIT);return 0;}
static void ntConcurrent(void)
{
    nnfixture* F=nnStart();xredisclusternodesconfig C;nnConfig(F,&C);nnCreate(F,&C);ntwork W[12]={0};xthread* T[12];
    for(size_t I=0;I<12;++I){W[I].F=F;T[I]=xrtThreadCreate(ntLookup,&W[I],0);testRequire(T[I]!=NULL,"concurrent cache lookup");}xrtAtomic32Store(&F->Go,1,XMEMORY_RELEASE);
    for(size_t I=0;I<12;++I){testRequire(xrtThreadWaitFor(T[I],10000)==XWAIT_OK && !xrtThreadExitCode(T[I]) && W[I].Status==XREDIS_CLUSTER_POOL_READY && W[I].Pool==W[0].Pool,"coalesced one node/pool");xrtThreadDestroy(T[I]);}
    nnReady(W[0].Pool,1);testRequire(nnInfo(F->Cache).Created==1 && nnInfo(F->Cache).Misses==1 && xrtAtomic32Load(&F->Calls,XMEMORY_ACQUIRE)==1,"concurrent creators duplicated node");
    for(size_t I=0;I<12;++I)xredisPoolRelease(W[I].Pool);
    nnClose(F);xredisClusterNodesRelease(F->Cache);nnStop(F);
}
static void ntPolicy(void)
{
    nnfixture* F=nnStart();xredisclusternodesconfig C;nnConfig(F,&C);nnCreate(F,&C);xredisclusteraddress A=nnAddress(F,0);xredispool* Out=(void*)(uintptr_t)1;
    testRequire(xredisClusterNodesGetPool(&A,0,F->Cache,&Out)==XREDIS_CLUSTER_POOL_WAIT && !xrtAtomic32Load(&F->Auth,XMEMORY_ACQUIRE),"due lookup invoked policy/setup");
    xrtAtomic32Store(&F->Deny,1,XMEMORY_RELEASE);testRequire(xredisClusterNodesGetPool(&A,5000,F->Cache,&Out)==XREDIS_CLUSTER_POOL_DENIED && Out==(void*)(uintptr_t)1 && !nnInfo(F->Cache).Nodes,"denied lookup created node");
    xrtAtomic32Store(&F->Deny,0,XMEMORY_RELEASE);Out=NULL;testRequire(xredisClusterNodesGetPool(&A,5000,F->Cache,&Out)==XREDIS_CLUSTER_POOL_READY,"policy seed");nnReady(Out,1);xredisPoolRelease(Out);
    xrtAtomic32Store(&F->Deny,1,XMEMORY_RELEASE);Out=(void*)(uintptr_t)1;testRequire(xredisClusterNodesGetPool(&A,5000,F->Cache,&Out)==XREDIS_CLUSTER_POOL_DENIED && Out==(void*)(uintptr_t)1,"cache hit bypassed authorization");
    xrtAtomic32Store(&F->Deny,0,XMEMORY_RELEASE);xrtAtomic32Store(&F->CloseInAuth,1,XMEMORY_RELEASE);testRequire(xredisClusterNodesGetPool(&A,5000,F->Cache,&Out)==XREDIS_CLUSTER_POOL_CLOSED,"authorization reentrant close");nnClose(F);xredisClusterNodesRelease(F->Cache);nnStop(F);
}
static void ntCloseDuringPolicy(void)
{
    nnfixture* F=nnStart();xredisclusternodesconfig C;nnConfig(F,&C);C.MaxLookups=1;nnCreate(F,&C);xrtAtomic32Store(&F->Block,1,XMEMORY_RELEASE);xrtAtomic32Store(&F->Go,1,XMEMORY_RELEASE);ntwork W={F,NULL,0};xthread* T=xrtThreadCreate(ntLookup,&W,0);testRequire(T!=NULL,"held policy thread");redisPeerWait(&F->Entered,1);
    xredisclusteraddress A=nnAddress(F,0);xredispool* Out=(void*)(uintptr_t)1;testRequire(xredisClusterNodesGetPool(&A,5000,F->Cache,&Out)==XREDIS_CLUSTER_POOL_WAIT && Out==(void*)(uintptr_t)1 && xrtAtomic32Load(&F->Auth,XMEMORY_ACQUIRE)==1,"bounded concurrent lookup admission");
    xfuture* Closed=xredisClusterNodesClosed(F->Cache);xredisClusterNodesClose(F->Cache);testRequire(xrtFutureState(Closed)==XFUTURE_PENDING && !xrtAtomic32Load(&F->Drops,XMEMORY_ACQUIRE),"close escaped authorization duty");
    xrtAtomic32Store(&F->Continue,1,XMEMORY_RELEASE);testRequire(xrtThreadWaitFor(T,10000)==XWAIT_OK && W.Status==XREDIS_CLUSTER_POOL_CLOSED && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"held authorization retirement");xrtThreadDestroy(T);xrtFutureDestroy(Closed);testRequire(!xrtAtomic32Load(&F->Calls,XMEMORY_ACQUIRE),"close started a factory");xredisClusterNodesRelease(F->Cache);nnStop(F);
}
static void ntSealManyCallers(void)
{
    nnfixture* F=nnStart();xredisclusternodesconfig C;nnConfig(F,&C);C.MaxLookups=8;nnCreate(F,&C);xredisclusteraddress A=nnAddress(F,0);xredispool* P=NULL;
    testRequire(xredisClusterNodesGetPool(&A,5000,F->Cache,&P)==XREDIS_CLUSTER_POOL_READY,"seal stress warm node");nnReady(P,1);
    uint32 Base=xrtAtomic32Load(&F->Auth,XMEMORY_ACQUIRE);xrtAtomic32Store(&F->Block,1,XMEMORY_RELEASE);xrtAtomic32Store(&F->Go,1,XMEMORY_RELEASE);ntwork W[8]={0};xthread* T[8];
    for(size_t I=0;I<8;++I){W[I].F=F;xredisClusterNodesRetain(F->Cache);T[I]=xrtThreadCreate(ntLookup,&W[I],0);testRequire(T[I]!=NULL,"many admitted policy callers");}
    redisPeerWait(&F->Auth,Base+8);testRequire(nnInfo(F->Cache).APIs==8,"exact admitted caller bound");xredispool* Out=(void*)(uintptr_t)1;
    testRequire(xredisClusterNodesGetPool(&A,5000,F->Cache,&Out)==XREDIS_CLUSTER_POOL_WAIT && Out==(void*)(uintptr_t)1 && xrtAtomic32Load(&F->Auth,XMEMORY_ACQUIRE)==Base+8,"saturated admission invoked policy");
    xfuture* Closed=xredisClusterNodesClosed(F->Cache);xredisClusterNodesClose(F->Cache);xfuture* NodeClosed=xredisPoolClosed(P);testRequire(xrtFutureWaitFor(NodeClosed,10000)==XWAIT_OK,"node physical close while callers held");xrtFutureDestroy(NodeClosed);xredisPoolRelease(P);
    double Due=xrtTimer()+5;while(nnInfo(F->Cache).Nodes){testRequire(xrtTimer()<Due,"held callers blocked physical node retirement");xrtSleep(1);}
    testRequire(xrtFutureState(Closed)==XFUTURE_PENDING && !xrtAtomic32Load(&F->Drops,XMEMORY_ACQUIRE),"seal dropped data before all admitted scopes ended");
    testRequire(xredisClusterNodesGetPool(&A,5000,F->Cache,&Out)==XREDIS_CLUSTER_POOL_CLOSED && xrtAtomic32Load(&F->Auth,XMEMORY_ACQUIRE)==Base+8,"sealed admission called policy");
    xrtAtomic32Store(&F->Continue,1,XMEMORY_RELEASE);
    for(size_t I=0;I<8;++I){testRequire(xrtThreadWaitFor(T[I],10000)==XWAIT_OK && W[I].Status==XREDIS_CLUSTER_POOL_CLOSED,"admitted caller close result");xrtThreadDestroy(T[I]);xredisClusterNodesRelease(F->Cache);}
    testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK && xrtAtomic32Load(&F->AuthExited,XMEMORY_ACQUIRE)==Base+8 && xrtAtomic32Load(&F->Drops,XMEMORY_ACQUIRE)==1 && !nnInfo(F->Cache).APIs,"last admitted caller did not finish close exactly once");
    xrtFutureDestroy(Closed);xredisClusterNodesRelease(F->Cache);nnStop(F);
}
static void ntRoute(void)
{
    nnfixture* F=nnStart();xrtAtomic32Store(&F->Nodes[0].Redirect,1,XMEMORY_RELEASE);xrtAtomic32Store(&F->Nodes[0].Target,1,XMEMORY_RELEASE);xrtAtomic32Store(&F->Nodes[1].Redirect,2,XMEMORY_RELEASE);xrtAtomic32Store(&F->Nodes[1].Target,2,XMEMORY_RELEASE);
    xredisclusternodesconfig C;nnConfig(F,&C);nnCreate(F,&C);xredisclustertopology* T=crTopology(F->Peers[0].Address.Port,"127.0.0.1",0,16383);xredisclusterrouteconfig RC;xredisClusterRouteConfigInit(&RC);RC.GetPool=xredisClusterNodesGetPool;RC.Data=F->Cache;RC.Timeout=10000;RC.RetryDelay=1;
    xbytesview A[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("cache\0route")};xredisclusterroute* R=NULL;testRequire(xredisClusterRouteCreate(T,(xbytesview){0},1,A,2,&RC,&R)==XREDIS_ADMISSION_ACCEPTED,"cache route create");xredisClusterTopologyRelease(T);xredisclusterrouteresult V=crDrive(R);
    testRequire(V.Status==XREDIS_CLUSTER_ROUTE_COMPLETE && V.Query.Status==XREDIS_REQUEST_OK && V.Attempts==3 && V.Moved==1 && V.Asks==1 && V.ControlReceived && V.Control.Status==XREDIS_REQUEST_OK && nnInfo(F->Cache).Created==3,"lazy cached automatic MOVED/ASK");
    xfuture* Saved=xredisClusterRouteResultFuture(R);xredisClusterRouteDestroy(R);nnClose(F);xredisClusterNodesRelease(F->Cache);nnStop(F);testRequire(((const xredisrequestresult*)xrtFutureValue(Saved))->Status==XREDIS_REQUEST_OK,"cached native future lifetime");xrtFutureDestroy(Saved);
}
static void ntValidation(void)
{
    nnfixture* F=nnStart();for(unsigned Kind=0;Kind<13;++Kind){xredisclusternodesconfig C;nnConfig(F,&C);
        switch(Kind){case 0:C.MaxNodes=0;break;case 1:C.MaxNodes=65537;break;case 2:C.MaxHostBytes=0;break;case 3:C.MaxHostBytes=65537;break;case 4:C.MaxConnections=2;break;case 5:C.MaxStorageBytes=1;break;case 6:C.Protocol=0;break;case 7:C.MaxConnecting=0;break;case 8:C.AttemptTimeout=0;break;case 9:C.RetryMin=100;C.RetryMax=1;break;case 10:C.Pool.MaxRequests=0;break;case 11:C.Pool.MaxCommandBytes=SIZE_MAX;break;default:C.Connect=NULL;break;}
        xredisclusternodes* Out=(void*)(uintptr_t)1;testRequire(xredisClusterNodesCreate(F->Peers[0].Engine,0,&C,&Out)==XREDIS_ADMISSION_ERROR && Out==(void*)(uintptr_t)1,"invalid cache bounds/ownership");xrtClearError();}
    testRequire(!xrtAtomic32Load(&F->Drops,XMEMORY_ACQUIRE),"failed cache constructor consumed Data/Drop");xredisclusternodesconfig C;nnConfig(F,&C);nnCreate(F,&C);
    for(unsigned Kind=0;Kind<6;++Kind){xredisclusteraddress A=nnAddress(F,0);if(Kind==0)A.Port=0;else if(Kind==1)A.Slot=16384;else if(Kind==2)A.Reason=(xredisclusterredirectkind)123;else if(Kind==3)A.Host=(xbytesview)XRT_BYTES_LITERAL("?");else if(Kind==4)A.Host.Size=65;else A.Host=(xbytesview)XRT_BYTES_LITERAL("bad host");xredispool* Out=(void*)(uintptr_t)1;
        testRequire(xredisClusterNodesGetPool(&A,5000,F->Cache,&Out)==XREDIS_CLUSTER_POOL_ERROR && Out==(void*)(uintptr_t)1,"invalid node identity/output");xrtClearError();}
    xredisclusteraddress A=nnAddress(F,0),Before=A;testRequire(xredisClusterNodesGetPool(&A,5000,F->Cache,(xredispool**)&A)==XREDIS_CLUSTER_POOL_ERROR && !memcmp(&A,&Before,sizeof(A)),"cache lookup output aliases input");xrtClearError();
    testRequire(!xredisClusterNodesInfo(F->Cache,(xredisclusternodesinfo*)F->Cache),"cache metadata overlap");xrtClearError();testRequire(!nnInfo(F->Cache).Nodes,"invalid lookup started maintenance");nnClose(F);xredisClusterNodesRelease(F->Cache);nnStop(F);
}
static void ntCloseSetupAndBusy(void)
{
    for(unsigned Held=0;Held<2;++Held){nnfixture* F=nnStart();if(!Held)xrtAtomic32Store(&F->Peers[0].Mode,PC_HOLD,XMEMORY_RELEASE);xredisclusternodesconfig C;nnConfig(F,&C);C.AttemptTimeout=60000;nnCreate(F,&C);xredisclusteraddress A=nnAddress(F,0);xredispool* P=NULL;
        testRequire(xredisClusterNodesGetPool(&A,5000,F->Cache,&P)==XREDIS_CLUSTER_POOL_READY,"held native node setup");xfuture* Query=NULL;
        if(Held){nnReady(P,1);xrtAtomic32Store(&F->HoldBusiness,1,XMEMORY_RELEASE);xbytesview Args[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("held")};testRequire(xredisSharedPoolCallAsync(P,Args,2,5000,&Query)==XREDIS_ADMISSION_ACCEPTED,"retiring sent business admission");redisPeerWait(&F->Business,1);testRequire(xredisClusterNodesRetire(F->Cache,&A),"retire busy node");}
        else redisPeerWait(&F->Peers[0].Hello,1);
        double Began=xrtTimer();nnClose(F);testRequire(xrtTimer()-Began<5,"cache close waited for setup timeout");
        if(Query){testRequire(xrtFutureWaitFor(Query,5000)==XWAIT_OK,"retired sent request outcome");const xredisrequestresult* R=xrtFutureValue(Query);testRequire(R && R->Status==XREDIS_REQUEST_CLOSED && R->Delivery==XREDIS_MAY_HAVE_EXECUTED && xrtAtomic32Load(&F->Business,XMEMORY_ACQUIRE)==1,"retirement replayed/erased uncertain business");}
        xredisPoolRelease(P);xredisClusterNodesRelease(F->Cache);nnStop(F);xrtFutureDestroy(Query);}
}
static void ntBudgetZero(xallocationbudget* B)
{double Due=xrtTimer()+10;for(;;){xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && xrtTimer()<Due,"node budget retirement");if(!I.LiveCount && !I.LiveBytes)return;xrtSleep(1);}}
static void ntOOM(void)
{
    nnfixture* F=nnStart();unsigned Fail=0,OK=0;
    for(size_t Limit=1;Limit<=8192;Limit+=127){xredisclusternodesconfig C;nnConfig(F,&C);C.MaxNodes=1;C.MaxHostBytes=16;xallocationbudget* B=NULL;xallocationscope S={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&S),"cache constructor OOM scope");xredisclusternodes* Out=(void*)(uintptr_t)1;
        xredisadmission A=xredisClusterNodesCreate(F->Peers[0].Engine,0,&C,&Out);testRequire(xrtAllocationBudgetLeave(&S),"cache constructor OOM leave");
        if(A==XREDIS_ADMISSION_ACCEPTED){++OK;F->Cache=Out;nnClose(F);xredisClusterNodesRelease(Out);}else{++Fail;testRequire(Out==(void*)(uintptr_t)1,"failed cache constructor transferred output");xrtClearError();}
        ntBudgetZero(B);xrtAllocationBudgetFree(B);}
    testRequire(Fail && OK,"cache constructor OOM coverage");Fail=OK=0;
    for(size_t Limit=1;Limit<=8192;Limit+=127){xredisclusternodesconfig C;nnConfig(F,&C);C.MaxNodes=1;C.MaxHostBytes=16;nnCreate(F,&C);xallocationbudget* B=NULL;xallocationscope S={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&S),"cache miss OOM scope");xredisclusteraddress A=nnAddress(F,0);xredispool* P=(void*)(uintptr_t)1;xredisclusterpoolstatus Status=xredisClusterNodesGetPool(&A,5000,F->Cache,&P);testRequire(xrtAllocationBudgetLeave(&S),"cache miss OOM leave");
        if(Status==XREDIS_CLUSTER_POOL_READY){++OK;xredisPoolRelease(P);}else{++Fail;testRequire((Status==XREDIS_CLUSTER_POOL_MEMORY || Status==XREDIS_CLUSTER_POOL_WAIT) && P==(void*)(uintptr_t)1,"cache miss OOM output/status");xrtClearError();}
        nnClose(F);xredisClusterNodesRelease(F->Cache);ntBudgetZero(B);xrtAllocationBudgetFree(B);}
    testRequire(Fail && OK,"cache miss OOM coverage");nnStop(F);
}
int main(void)
{ntValidation();ntBasic();ntCapacity();ntConcurrent();ntPolicy();ntCloseDuringPolicy();ntSealManyCallers();ntCloseSetupAndBusy();ntRoute();ntOOM();puts("[PASS] bounded Cluster node pools: lazy/coalesced setup, per-hit authority, quotas, physical retirement, no hot allocations, OOM and retained outcomes");return 0;}
