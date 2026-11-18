#include <xredis/cluster_client.h>
#include "support/cluster_client_peer.h"
#define testRequire redisPeerRequire
static void ccBasic(void)
{
    rffixture* F=rfStart(3);xredisclusterclient* C=cfCreate(F,NULL);xfuture* R=cfEcho(C,5000);const xredisclusterclientresult* V=cfWait(R);
    testRequire(V->Status==XREDIS_CLUSTER_CLIENT_COMPLETE && V->RouteObserved && V->Generation==1 && V->Route.Status==XREDIS_CLUSTER_ROUTE_COMPLETE && V->Route.Query.Status==XREDIS_REQUEST_OK && V->Route.Attempts==1,"managed cold call result");
    xredisreplyvalue Text;testRequire(xredisReplyGet(V->Route.Query.Reply,0,&Text) && Text.Data.Size==13 && !memcmp(Text.Data.Data,"managed\0route",13),"managed binary echo");cfReady(C);
    xredisclustermap* Map=NULL;bool Stale=true;testRequire(xredisClusterClientSnapshot(C,&Map,&Stale)==XREDIS_CLUSTER_VALUE && !Stale,"managed published map");xredisclusterclientresult Frozen=*V;
    cfStop(F,C);testRequire(!memcmp(V,&Frozen,sizeof(Frozen)) && xredisReplyGet(V->Route.Query.Reply,0,&Text) && Text.Data.Size==13,"retained managed result after engine teardown");xredisclustermapinfo M={0};M.Size=sizeof(M);testRequire(xredisClusterMapInfo(Map,&M) && M.Generation==1,"retained managed map");xredisClusterMapRelease(Map);xrtFutureDestroy(R);
}
static void ccRedirects(void)
{
    for(unsigned Kind=1;Kind<=2;++Kind){rffixture* F=rfStart(3);xredisclusterclient* C=cfCreate(F,NULL);cfReady(C);xrtAtomic32Store(&F->Base->Nodes[0].Target,1,XMEMORY_RELEASE);xrtAtomic32Store(&F->Base->Nodes[0].Redirect,Kind,XMEMORY_RELEASE);xrtAtomic32Store(&F->Nodes[0].Target,1,XMEMORY_RELEASE);
        xfuture* R=cfEcho(C,5000);const xredisclusterclientresult* V=cfWait(R);testRequire(V->Status==XREDIS_CLUSTER_CLIENT_COMPLETE && V->Route.Query.Status==XREDIS_REQUEST_OK && V->Route.Attempts==2 && V->Route.Redirects==1 && V->Route.Moved==(Kind==1) && V->Route.Asks==(Kind==2),"managed real redirect route");
        if(Kind==1){double Due=xrtTimer()+10;while(cfInfo(C).Generation<2 || cfInfo(C).Dirty){testRequire(xrtTimer()<Due,"automatic MOVED refresh");xrtSleep(1);}testRequire(cfInfo(C).Moved==1 && xrtAtomic32Load(&F->Nodes[0].Queries,XMEMORY_ACQUIRE)==2,"MOVED refresh did not coalesce");}
        else testRequire(cfInfo(C).Generation==1 && !cfInfo(C).Moved && xrtAtomic32Load(&F->Nodes[0].Queries,XMEMORY_ACQUIRE)==1,"ASK changed shared topology");
        xfuture* Next=cfEcho(C,5000);const xredisclusterclientresult* W=cfWait(Next);testRequire(W->Route.Query.Status==XREDIS_REQUEST_OK && W->Route.Attempts==(Kind==1?1u:2u),"next call did not use proper shared/ephemeral map");cfStop(F,C);xrtFutureDestroy(R);xrtFutureDestroy(Next);
    }
}
static xfuture* ccReadReplica(xredisclusterclient* C)
{
    const xbytesview A[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("replica\0read")};
    xfuture* F=NULL;
    testRequire(xredisClusterClientCallReplica(C,1,A,2,5000,&F)==XREDIS_ADMISSION_ACCEPTED,"stale-tolerant read admission");
    return F;
}
static const xredisclusterrouteresult* ccReadResult(xfuture* F)
{
    const xredisclusterclientresult* V=cfWait(F);
    testRequire(V->Status==XREDIS_CLUSTER_CLIENT_COMPLETE && V->Route.Status==XREDIS_CLUSTER_ROUTE_COMPLETE &&
        V->Route.Query.Status==XREDIS_REQUEST_OK && V->Route.Query.Delivery==XREDIS_REPLIED,"replica read observed");
    return &V->Route;
}
static xredispool* ccReplicaPool(rffixture* F,size_t Index)
{
    xredisclusteraddress A=nnAddress(F->Base,Index);
    A.Reason=XREDIS_CLUSTER_REPLICA_READ;
    xredispool* P=NULL;
    testRequire(xredisClusterNodesGetPool(&A,5000,F->Base->Cache,&P)==XREDIS_CLUSTER_POOL_READY,"replica pool");
    return P;
}
static void ccReplicaRoutes(void)
{
    for(unsigned Legacy=0;Legacy<2;++Legacy){
        rffixture* F=rfStart(5);
        xrtAtomic32Store(&F->Nodes[0].Replicas,6,XMEMORY_RELEASE);
        xrtAtomic32Store(&F->Nodes[0].Mode,RF_HOLD,XMEMORY_RELEASE);
        xredispool* A=ccReplicaPool(F,1),*B=ccReplicaPool(F,2);
        nnReady(A,1);nnReady(B,1);
        xredisclusterclientconfig K;xredisClusterClientConfigInit(&K);
        if(Legacy)K.Refresh.Format=XREDIS_CLUSTER_LEGACY_SLOTS;
        xredisclusterclient* C=cfCreate(F,&K);
        xfuture* Cold=ccReadReplica(C);
        redisPeerWait(&F->Nodes[0].Queries,1);rfRelease(F,0);
        testRequire(ccReadResult(Cold)->AttemptAddress.Reason==XREDIS_CLUSTER_REPLICA_READ,"bootstrap read selects replica");
        xrtFutureDestroy(Cold);
        unsigned Seen=0;
        for(unsigned N=0;N<6;++N){
            xfuture* R=ccReadReplica(C);
            const xredisclusterrouteresult* V=ccReadResult(R);
            testRequire(V->AttemptAddress.Reason==XREDIS_CLUSTER_REPLICA_READ && V->Attempts==1 && V->ProviderCalls==1,"warm read takes one replica attempt");
            Seen|=V->AttemptAddress.Port==F->Base->Peers[1].Address.Port?1u:
                V->AttemptAddress.Port==F->Base->Peers[2].Address.Port?2u:0u;
            xrtFutureDestroy(R);
        }
        testRequire(Seen==3 && xrtAtomic32Load(&F->Base->Nodes[1].Readonly,XMEMORY_ACQUIRE)==1 &&
            xrtAtomic32Load(&F->Base->Nodes[2].Readonly,XMEMORY_ACQUIRE)==1,"rotation and READONLY reuse");
        xfuture* R=cfEcho(C,5000);
        testRequire(ccReadResult(R)->AttemptAddress.Port==F->Base->Peers[0].Address.Port,"ordinary calls keep primary routing");
        xrtFutureDestroy(R);
        xrtAtomic32Store(&F->Base->DenyReplica,1,XMEMORY_RELEASE);
        R=ccReadReplica(C);const xredisclusterrouteresult* V=ccReadResult(R);
        testRequire(V->AttemptAddress.Port==F->Base->Peers[0].Address.Port && !V->AttemptAddress.Reason &&
            V->Attempts==1 && V->ProviderCalls==2,"replica denial falls back before native acceptance");
        xrtFutureDestroy(R);
        xrtAtomic32Store(&F->Base->DenyReplica,0,XMEMORY_RELEASE);
        xrtAtomic32Store(&F->Base->Nodes[1].Redirect,XREDIS_CLUSTER_MOVED,XMEMORY_RELEASE);
        xrtAtomic32Store(&F->Base->Nodes[2].Redirect,XREDIS_CLUSTER_MOVED,XMEMORY_RELEASE);
        xrtAtomic32Store(&F->Nodes[0].Mode,RF_NORMAL,XMEMORY_RELEASE);
        R=ccReadReplica(C);V=ccReadResult(R);
        testRequire(V->AttemptAddress.Port==F->Base->Peers[0].Address.Port &&
            V->AttemptAddress.Reason==XREDIS_CLUSTER_MOVED && V->Attempts==2 && V->Moved==1,
            "validated replica MOVED targets the ordinary pool");
        xrtFutureDestroy(R);
        if(!Legacy){
            double Due=xrtTimer()+5;
            while(cfInfo(C).Generation<2 || cfInfo(C).Dirty){testRequire(xrtTimer()<Due,"MOVED map published");xrtSleep(1);}
            xrtAtomic32Store(&F->Nodes[0].Loading,6,XMEMORY_RELEASE);
            uint64 Before=cfInfo(C).Generation;Due=xrtTimer()+5;
            testRequire(xredisClusterClientInvalidate(C),"publish replicas loading");
            while(cfInfo(C).Generation<=Before || cfInfo(C).Dirty){testRequire(xrtTimer()<Due,"loading map published");xrtSleep(1);}
            R=ccReadReplica(C);V=ccReadResult(R);
            testRequire(V->AttemptAddress.Port==F->Base->Peers[0].Address.Port && !V->AttemptAddress.Reason &&
                V->Attempts==1 && V->ProviderCalls==1,"loading replicas skipped before any lookup");
            xrtFutureDestroy(R);
        }
        xredisPoolRelease(A);xredisPoolRelease(B);cfStop(F,C);
    }
}
static void ccReplicaProfile(void)
{
    rffixture* F=rfStart(4);
    xrtAtomic32Store(&F->Nodes[0].Replicas,2,XMEMORY_RELEASE);
    xrtAtomic32Store(&F->Base->BadReplica,1,XMEMORY_RELEASE);
    xredisclusteraddress Address=nnAddress(F->Base,1);
    xredispool* Primary=NULL,*Replica=ccReplicaPool(F,1);
    testRequire(xredisClusterNodesGetPool(&Address,5000,F->Base->Cache,&Primary)==XREDIS_CLUSTER_POOL_READY &&
        Primary!=Replica,"same address has separate ordinary and replica pools");
    nnReady(Primary,1);
    xredispoolconnectinfo Info={.Size=sizeof(Info)};
    double Due=xrtTimer()+5;
    do {
        testRequire(xredisPoolConnectInfo(Replica,&Info) && xrtTimer()<Due && Info.Connected==0,"unflagged replica rejected");
        if(!Info.Failures)xrtSleep(1);
    }while(!Info.Failures);
    xredisclusterclient* C=cfCreate(F,NULL);cfReady(C);
    xfuture* R=ccReadReplica(C);const xredisclusterrouteresult* V=ccReadResult(R);
    testRequire(V->AttemptAddress.Port==F->Base->Peers[0].Address.Port && V->Attempts==1 && V->ProviderCalls==2,
        "unavailable replica pool falls back without sending on wrong profile");
    xrtFutureDestroy(R);
    testRequire(xredisClusterNodesRetire(F->Base->Cache,&Address),"retire ordinary profile independently");
    xfuture* Closed=xredisPoolClosed(Primary);
    testRequire(xrtFutureWaitFor(Closed,5000)==XWAIT_OK,"ordinary physical retirement");
    xrtFutureDestroy(Closed);xredisPoolRelease(Primary);
    xrtAtomic32Store(&F->Base->BadReplica,0,XMEMORY_RELEASE);nnReady(Replica,1);
    R=ccReadReplica(C);V=ccReadResult(R);
    testRequire(V->AttemptAddress.Reason==XREDIS_CLUSTER_REPLICA_READ && V->Attempts==1,"replacement runs READONLY");
    xrtFutureDestroy(R);
    uint32 Business=xrtAtomic32Load(&F->Base->Business,XMEMORY_ACQUIRE);
    xrtAtomic32Store(&F->Base->HoldBusiness,1,XMEMORY_RELEASE);R=ccReadReplica(C);redisPeerWait(&F->Base->Business,Business+1);
    testRequire(xrtFutureCancel(R),"cancel accepted replica read");
    const xredisclusterclientresult* Stopped=cfWait(R);
    testRequire(Stopped->Status==XREDIS_CLUSTER_CLIENT_CANCELLED && Stopped->Route.Attempts==1 &&
        Stopped->Route.AttemptAddress.Reason==XREDIS_CLUSTER_REPLICA_READ,"accepted replica stop preserves attempt");
    xredisPoolRelease(Replica);cfStop(F,C);
    xrtFutureDestroy(R);
}
typedef struct ccbarrier {xatomic32 Entered,Continue;} ccbarrier;
static void ccBlock(xnetworker* W,ptr Data)
{(void)W;ccbarrier* B=Data;xrtAtomic32Store(&B->Entered,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&B->Continue,XMEMORY_ACQUIRE))xrtSleep(1);}
static void ccQueueBounds(void)
{
    rffixture* F=rfStart(3);xredisclusterclientconfig K;xredisClusterClientConfigInit(&K);K.MaxRequests=1;xredisclusterclient* C=cfCreate(F,&K);cfReady(C);ccbarrier B={0};
    testRequire(xrtNetEnginePost(F->Base->Peers[0].Engine,1,ccBlock,&B),"block progress worker for deadline/limits");redisPeerWait(&B.Entered,1);xfuture* R=cfEcho(C,20);xbytesview A[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("quota")};xfuture* Rejected=(void*)(uintptr_t)1;
    testRequire(xredisClusterClientCall(C,1,A,2,5000,&Rejected)==XREDIS_ADMISSION_OVERLOADED && Rejected==(void*)(uintptr_t)1 && cfInfo(C).Requests==1,"managed request quota/output ownership");xrtSleep(30);xrtAtomic32Store(&B.Continue,1,XMEMORY_RELEASE);
    const xredisclusterclientresult* V=cfWait(R);testRequire(V->Status==XREDIS_CLUSTER_CLIENT_TIMEOUT && !V->Native && (!V->RouteObserved || !V->Route.Attempts) && !xrtAtomic32Load(&F->Base->Business,XMEMORY_ACQUIRE),"queue wait reset deadline/sent business");
    xfuture* Next=cfEcho(C,5000);testRequire(cfWait(Next)->Route.Query.Status==XREDIS_REQUEST_OK,"quota did not retire");cfStop(F,C);xrtFutureDestroy(R);xrtFutureDestroy(Next);
    for(unsigned Kind=0;Kind<2;++Kind){F=rfStart(3);xredisClusterClientConfigInit(&K);if(Kind)K.MaxStorageBytes=1;else K.MaxCommandBytes=1;C=cfCreate(F,&K);cfReady(C);Rejected=(void*)(uintptr_t)1;
        testRequire(xredisClusterClientCall(C,1,A,2,5000,&Rejected)==XREDIS_ADMISSION_OVERLOADED && Rejected==(void*)(uintptr_t)1 && !cfInfo(C).Requests && !cfInfo(C).StorageBytes,"managed aggregate byte quotas");cfStop(F,C);}
}
static void ccBusyPoolDoesNotRefresh(void)
{
    rffixture* F=rfStart(3);
    nnClose(F->Base);
    xredisClusterNodesRelease(F->Base->Cache);
    xredisclusternodesconfig NC;nnConfig(F->Base,&NC);
    NC.Pool.MaxRequests=1;
    NC.Authorize=rfAuthorize;NC.Connect=rfConnect;NC.Drop=rfDrop;NC.Data=F;
    nnCreate(F->Base,&NC);
    xredisclusterclientconfig K;xredisClusterClientConfigInit(&K);K.Route.MaxWaits=0;
    xredisclusterclient* C=cfCreate(F,&K);cfReady(C);
    xredisclusteraddress Address=nnAddress(F->Base,0);xredispool* Pool=NULL;
    testRequire(xredisClusterNodesGetPool(&Address,5000,F->Base->Cache,&Pool)==XREDIS_CLUSTER_POOL_READY,"busy pool warm lookup");
    double Deadline=xrtTimer()+5;
    for(;;){
        xredispoolinfo Info={.Size=sizeof(Info)};
        testRequire(xredisPoolInfo(Pool,&Info) && xrtTimer()<Deadline,"bootstrap quota released");
        if(Info.Idle && !Info.Requests)break;
        xrtSleep(1);
    }
    xredisPoolRelease(Pool);
    xredisclusterclientinfo Before=cfInfo(C);
    xrtAtomic32Store(&F->Base->HoldBusiness,1,XMEMORY_RELEASE);
    xfuture* Held=cfEcho(C,5000);redisPeerWait(&F->Base->Business,1);
    xfuture* Rejected=cfEcho(C,5000);const xredisclusterclientresult* V=cfWait(Rejected);
    xredisclusterclientinfo After=cfInfo(C);
    testRequire(V->Status==XREDIS_CLUSTER_CLIENT_COMPLETE && V->RouteObserved &&
        V->Route.Status==XREDIS_CLUSTER_ROUTE_OVERLOADED && !V->Route.Attempts && !V->Native &&
        After.Generation==Before.Generation && After.Refreshes==Before.Refreshes && !After.Dirty,
        "saturated healthy pool preserves overload without topology recovery");
    cfStop(F,C);
    xrtFutureDestroy(Rejected);xrtFutureDestroy(Held);
}
static void ccStops(void)
{
    for(unsigned Kind=0;Kind<3;++Kind){rffixture* F=rfStart(3);xredisclusterclient* C=cfCreate(F,NULL);cfReady(C);xrtAtomic32Store(&F->Base->HoldBusiness,1,XMEMORY_RELEASE);xfuture* R=cfEcho(C,Kind==0?200:5000);redisPeerWait(&F->Base->Business,1);
        if(Kind==1)testRequire(xrtFutureCancel(R),"managed cancellation request");else if(Kind==2)cfClose(C);
        const xredisclusterclientresult* V=cfWait(R);testRequire(V->Status==(Kind==0?XREDIS_CLUSTER_CLIENT_TIMEOUT:Kind==1?XREDIS_CLUSTER_CLIENT_CANCELLED:XREDIS_CLUSTER_CLIENT_CLOSED) && V->RouteObserved && V->Route.Attempts==1 && V->Native,"managed stop outcome/native retention");xredisclusterclientresult Frozen=*V;cfStop(F,C);testRequire(!memcmp(V,&Frozen,sizeof(Frozen)),"managed stop result changed after native teardown");xrtFutureDestroy(R);}
}
typedef struct ccproducer {xredisclusterclient* Client;xatomic32* Go;xfuture* Saved;} ccproducer;
static int32 ccProduce(ptr Data)
{ccproducer* P=Data;while(!xrtAtomic32Load(P->Go,XMEMORY_ACQUIRE))xrtSleep(1);for(unsigned I=0;I<16;++I){xfuture* F=cfEcho(P->Client,5000);const xredisclusterclientresult* R=cfWait(F);testRequire(R->Status==XREDIS_CLUSTER_CLIENT_COMPLETE && R->Route.Query.Status==XREDIS_REQUEST_OK,"concurrent managed route");if(!I)P->Saved=F;else xrtFutureDestroy(F);}return 0;}
static void ccConcurrent(void)
{
    rffixture* F=rfStart(3);xredisclusterclient* C=cfCreate(F,NULL);xatomic32 Go={0};ccproducer P[8];xthread* T[8];for(unsigned I=0;I<8;++I){P[I]=(ccproducer){C,&Go,NULL};xredisClusterClientRetain(C);T[I]=xrtThreadCreate(ccProduce,&P[I],0);testRequire(T[I]!=NULL,"concurrent managed producer");}xrtAtomic32Store(&Go,1,XMEMORY_RELEASE);
    for(unsigned I=0;I<8;++I){testRequire(xrtThreadWaitFor(T[I],15000)==XWAIT_OK && !xrtThreadExitCode(T[I]),"managed producer joined");xrtThreadDestroy(T[I]);xredisClusterClientRelease(C);}testRequire(xrtAtomic32Load(&F->Nodes[0].Queries,XMEMORY_ACQUIRE)==1 && cfInfo(C).Accepted==128 && cfInfo(C).Completed==128,"bootstrap cohort/concurrent accounting");cfStop(F,C);for(unsigned I=0;I<8;++I)xrtFutureDestroy(P[I].Saved);
}
static void ccRefreshFailure(void)
{
    rffixture* F=rfStart(3);xrtAtomic32Store(&F->Nodes[0].Mode,RF_DENIED,XMEMORY_RELEASE);xredisclusterclient* C=cfCreate(F,NULL);xfuture* R=cfEcho(C,30);const xredisclusterclientresult* V=cfWait(R);testRequire(V->Status==XREDIS_CLUSTER_CLIENT_TIMEOUT && V->RefreshObserved && V->RefreshStatus==XREDIS_REFRESH_SERVER && !V->RouteObserved && !xrtAtomic32Load(&F->Base->Business,XMEMORY_ACQUIRE),"bootstrap failure result provenance");
    xrtAtomic32Store(&F->Nodes[0].Mode,RF_NORMAL,XMEMORY_RELEASE);testRequire(xredisClusterClientInvalidate(C),"retry bootstrap demand");cfReady(C);xfuture* Good=cfEcho(C,5000);testRequire(cfWait(Good)->Route.Query.Status==XREDIS_REQUEST_OK,"bootstrap recovery");
    xrtAtomic32Store(&F->Nodes[0].Mode,RF_BAD,XMEMORY_RELEASE);testRequire(xredisClusterClientInvalidate(C),"failed replacement demand");uint64 Before=cfInfo(C).RefreshFailures;double Due=xrtTimer()+10;while(cfInfo(C).RefreshFailures==Before){testRequire(xrtTimer()<Due,"failed refresh observed");xrtSleep(1);}testRequire(cfInfo(C).Generation==1 && cfInfo(C).Dirty,"failed refresh replaced healthy map");xfuture* Old=cfEcho(C,5000);testRequire(cfWait(Old)->Route.Query.Status==XREDIS_REQUEST_OK,"old map unavailable during refresh failure");cfStop(F,C);xrtFutureDestroy(R);xrtFutureDestroy(Good);xrtFutureDestroy(Old);
}
static void ccMissing(void)
{
    rffixture* F=rfStart(3);xrtAtomic32Store(&F->Nodes[0].Mode,RF_PARTIAL,XMEMORY_RELEASE);xredisclusterclientconfig K;xredisClusterClientConfigInit(&K);K.Refresh.Limits.RequireFullCoverage=false;xredisclusterclient* C=cfCreate(F,&K);cfReady(C);xfuture* R=cfEcho(C,5000);const xredisclusterclientresult* V=cfWait(R);
    testRequire(V->Status==XREDIS_CLUSTER_CLIENT_COMPLETE && V->RouteObserved && V->Route.Status==XREDIS_CLUSTER_ROUTE_NO_ROUTE && !V->Route.Attempts && V->Generation==2 && xrtAtomic32Load(&F->Nodes[0].Queries,XMEMORY_ACQUIRE)==2 && !xrtAtomic32Load(&F->Base->Business,XMEMORY_ACQUIRE),"missing slot did not fetch once before error");cfStop(F,C);xrtFutureDestroy(R);
    F=rfStart(3);xrtAtomic32Store(&F->Nodes[0].Mode,RF_HOLD,XMEMORY_RELEASE);xredisClusterClientConfigInit(&K);K.Route.MaxStorageBytes=1;C=cfCreate(F,&K);R=cfEcho(C,5000);redisPeerWait(&F->Nodes[0].Queries,1);rfRelease(F,0);V=cfWait(R);testRequire(V->Status==XREDIS_CLUSTER_CLIENT_LIMIT && !V->RouteObserved && !V->Native,"cold route limit misclassified as allocation failure");cfStop(F,C);xrtFutureDestroy(R);
}
static void ccBudgetDrain(xallocationbudget* B)
{double Due=xrtTimer()+10;for(;;){xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && xrtTimer()<Due,"managed budget retirement");if(!I.LiveBytes && !I.LiveCount)return;xrtSleep(1);}}
static void ccOOM(void)
{
    rffixture* F=rfStart(3);xredisclusteraddress Seed=nnAddress(F->Base,0);Seed.Slot=0;unsigned Accepted=0,Failed=0;
    for(size_t Limit=1;Limit<=65536;Limit+=1019){xredisclusterclientconfig K;xredisClusterClientConfigInit(&K);xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&Scope),"managed constructor budget");xredisclusterclient* C=(void*)(uintptr_t)1;xredisadmission A=xredisClusterClientCreate(F->Base->Peers[0].Engine,1,F->Base->Cache,&Seed,1,&K,&C);testRequire(xrtAllocationBudgetLeave(&Scope),"constructor budget leave");
        if(A==XREDIS_ADMISSION_ACCEPTED){++Accepted;cfClose(C);xredisClusterClientRelease(C);}else {++Failed;testRequire(C==(void*)(uintptr_t)1,"constructor failure changed output");xrtClearError();}ccBudgetDrain(B);xrtAllocationBudgetFree(B);}
    testRequire(Accepted && Failed && !xrtAtomic32Load(&F->Base->Drops,XMEMORY_ACQUIRE),"constructor OOM consumed shared Nodes/context");xredisclusterclient* C=cfCreate(F,NULL);cfReady(C);Accepted=Failed=0;
    for(size_t Limit=1;Limit<=8192;Limit+=127){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&Scope),"managed call budget");xbytesview A[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("managed\0route")};xfuture* R=(void*)(uintptr_t)1;xredisadmission S=xredisClusterClientCall(C,1,A,2,5000,&R);testRequire(xrtAllocationBudgetLeave(&Scope),"managed call budget leave");
        if(S==XREDIS_ADMISSION_ACCEPTED){++Accepted;testRequire(cfWait(R)->Route.Query.Status==XREDIS_REQUEST_OK,"budget accepted route");xrtFutureDestroy(R);}else {++Failed;testRequire(R==(void*)(uintptr_t)1,"call OOM changed output");xrtClearError();}ccBudgetDrain(B);xrtAllocationBudgetFree(B);testRequire(!cfInfo(C).Requests && !cfInfo(C).StorageBytes,"call OOM leaked reservation");}
    testRequire(Accepted && Failed,"managed call OOM exercised no paths");cfStop(F,C);
}
typedef struct cccontext {rffixture* Fixture;xatomicptr Client;xatomic32 Stop;} cccontext;
static bool ccAuthorize(const xredisclusteraddress* A,ptr Data)
{cccontext* X=Data;bool OK=nnAuthorize(A,X->Fixture->Base);if(xrtAtomic32Load(&X->Stop,XMEMORY_ACQUIRE))xredisClusterClientClose(xrtAtomicPtrLoad(&X->Client,XMEMORY_ACQUIRE));return OK;}
static bool ccConnect(const xredisclusteraddress* A,xnetengine* E,size_t S,uint64 N,int64 T,ptr Data,xredisclient** Out)
{return rfConnect(A,E,S,N,T,((cccontext*)Data)->Fixture,Out);}
static void ccDrop(ptr Data){rfDrop(((cccontext*)Data)->Fixture);}
static void ccReentry(void)
{
    rffixture* F=rfStart(3);nnClose(F->Base);xredisClusterNodesRelease(F->Base->Cache);cccontext X={F,{0},{0}};xredisclusternodesconfig K;nnConfig(F->Base,&K);K.Authorize=ccAuthorize;K.Connect=ccConnect;K.Drop=ccDrop;K.Data=&X;nnCreate(F->Base,&K);
    xredisclusterclient* C=cfCreate(F,NULL);xrtAtomicPtrStore(&X.Client,C,XMEMORY_RELEASE);cfReady(C);xrtAtomic32Store(&X.Stop,1,XMEMORY_RELEASE);xfuture* R=cfEcho(C,5000);const xredisclusterclientresult* V=cfWait(R);
    testRequire(V->Status==XREDIS_CLUSTER_CLIENT_CLOSED && (!V->RouteObserved || !V->Route.Attempts) && !xrtAtomic32Load(&F->Base->Business,XMEMORY_ACQUIRE),"policy-close reentry admitted business");cfStop(F,C);xrtFutureDestroy(R);
}
static void ccValidate(void)
{
    rffixture* F=rfStart(3);xredisclusteraddress Seed=nnAddress(F->Base,0);Seed.Slot=0;
    for(unsigned Kind=0;Kind<10;++Kind){xredisclusterclientconfig K;xredisClusterClientConfigInit(&K);switch(Kind){case 0:K.MaxRequests=0;break;case 1:K.MaxRequests=65537;break;case 2:K.MaxCommandBytes=0;break;case 3:K.MaxStorageBytes=0;break;case 4:K.MaxSteps=0;break;case 5:K.RefreshMin=0;break;case 6:K.RefreshRetryMax=60001;break;case 7:K.Route.Data=F;break;case 8:K.Route.Tls=true;break;default:K.Refresh.MaxSeeds=0;break;}
        xredisclusterclient* Out=(void*)(uintptr_t)1;testRequire(xredisClusterClientCreate(F->Base->Peers[0].Engine,1,F->Base->Cache,&Seed,1,&K,&Out)==XREDIS_ADMISSION_ERROR && Out==(void*)(uintptr_t)1,"invalid client config changed output/ownership");xrtClearError();}
    xredisclusterclient* C=cfCreate(F,NULL);cfReady(C);xredisclustermap* Map=NULL;bool Stale=true;testRequire(xredisClusterClientSnapshot(C,&Map,&Stale)==XREDIS_CLUSTER_VALUE,"validation snapshot");testRequire(!xredisClusterClientInfo(C,(void*)C) && !xredisClusterClientInfo(C,(void*)Map),"client info overwrote control/map");xrtClearError();xbytesview A[]={XRT_BYTES_LITERAL("PING")};testRequire(xredisClusterClientCall(C,1,A,1,5000,(void*)Map)==XREDIS_ADMISSION_ERROR,"call output overwrote map");xrtClearError();
    xallocationbudget* B=NULL;xallocationscope S={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&S),"client snapshot no-allocation scope");xredisclustermap* Same=NULL;testRequire(xredisClusterClientSnapshot(C,&Same,&Stale)==XREDIS_CLUSTER_VALUE && Same==Map && !Stale,"snapshot allocation under exhausted budget");xredisClusterMapRelease(Same);testRequire(xrtAllocationBudgetLeave(&S),"snapshot budget leave");xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.PeakBytes && !I.Rejections,"client snapshot getter allocated");xrtAllocationBudgetFree(B);
    xfuture* Default=NULL;testRequire(xredisClusterClientCall(C,1,A,1,-1,&Default)==XREDIS_ADMISSION_ACCEPTED && cfWait(Default)->Route.Query.Status==XREDIS_REQUEST_OK,"configured default timeout");xrtFutureDestroy(Default);cfStop(F,C);xredisClusterMapRelease(Map);
}
static void ccOccupiedTimer(xnetworker* W,uint64 Id,xnetresult Result,ptr Data)
{(void)W;(void)Id;testRequire(Result==XNET_RESULT_CANCELLED,"external timer ownership");xrtAtomic32Store(Data,1,XMEMORY_RELEASE);}
static void ccResourceStop(void)
{
    rffixture* F=rfStart(3);xrtAtomic32Store(&F->Nodes[0].Mode,RF_HOLD,XMEMORY_RELEASE);xnetengineconfig EC;xrtNetEngineConfigInit(&EC);EC.Workers=1;EC.TimerLimit=1;xnetengine* E=xrtNetEngineCreate(&EC);testRequire(E && xrtNetEngineStart(E),"separate progress engine");xatomic32 Done={0};uint64 Timer=xrtNetEngineAfter(E,0,60000,ccOccupiedTimer,&Done);testRequire(Timer!=0,"occupy timer capacity");
    double Due=xrtTimer()+10;for(;;){xnetenginestats I={0};testRequire(xrtNetEngineStats(E,&I) && xrtTimer()<Due,"timer occupancy visibility");if(I.ActiveTimers==1)break;xrtSleep(1);}
    xredisclusterclientconfig K;xredisClusterClientConfigInit(&K);xredisclusteraddress Seed=nnAddress(F->Base,0);Seed.Slot=0;xredisclusterclient* C=NULL;testRequire(xredisClusterClientCreate(E,0,F->Base->Cache,&Seed,1,&K,&C)==XREDIS_ADMISSION_ACCEPTED,"cross-engine client");xfuture* Ready=xredisClusterClientReady(C);testRequire(xrtFutureWaitFor(Ready,15000)==XWAIT_OK && ((const xredisclusterclientready*)xrtFutureValue(Ready))->Status==XREDIS_CLUSTER_CLIENT_RESOURCE,"timer exhaustion lost typed resource stop");cfClose(C);testRequire(cfInfo(C).StopStatus==XREDIS_CLUSTER_CLIENT_RESOURCE,"explicit close erased resource reason");testRequire(xrtNetEngineTimerCancel(E,Timer),"cancel own occupied timer");redisPeerWait(&Done,1);testRequire(xrtNetEngineStop(E) && xrtNetEngineDestroy(E),"client progress engine pin leaked");xrtFutureDestroy(Ready);cfStop(F,C);
}
static void ccAdmissionAllocations(void)
{
    rffixture* F=rfStart(3);xredisclusterclient* C=cfCreate(F,NULL);cfReady(C);ccbarrier B={0};testRequire(xrtNetEnginePost(F->Base->Peers[0].Engine,1,ccBlock,&B),"admission allocation barrier");redisPeerWait(&B.Entered,1);
    xallocationbudget* Budget=NULL;xallocationscope S={0};testRequire(xrtAllocationBudgetCreate(1048576,&Budget) && xrtAllocationBudgetEnter(Budget,&S),"admission allocation accounting");xfuture* R=cfEcho(C,10000);testRequire(xrtAllocationBudgetLeave(&S),"admission allocation scope leave");
    xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(Budget,&I) && I.LiveCount<=4 && I.LiveCount>0 && !I.Rejections,"warm admission exceeds four caller allocations");xrtAtomic32Store(&B.Continue,1,XMEMORY_RELEASE);testRequire(cfWait(R)->Route.Query.Status==XREDIS_REQUEST_OK,"coallocated admission result");xrtFutureDestroy(R);cfStop(F,C);ccBudgetDrain(Budget);xrtAllocationBudgetFree(Budget);
}
static void ccCopiedInputs(void)
{
    for(unsigned Warm=0;Warm<2;++Warm){rffixture* F=rfStart(3);if(!Warm)xrtAtomic32Store(&F->Nodes[0].Mode,RF_HOLD,XMEMORY_RELEASE);xredisclusterclient* C=cfCreate(F,NULL);ccbarrier B={0};
        if(Warm){cfReady(C);testRequire(xrtNetEnginePost(F->Base->Peers[0].Engine,1,ccBlock,&B),"warm copy barrier");redisPeerWait(&B.Entered,1);}else redisPeerWait(&F->Nodes[0].Queries,1);
        unsigned char Name[]="ECHO",Payload[]="owned\0payload";xbytesview A[]={{Name,4},{Payload,13}};xfuture* R=NULL;testRequire(xredisClusterClientCall(C,1,A,2,5000,&R)==XREDIS_ADMISSION_ACCEPTED,"copied input admission");memset(Name,'?',4);memset(Payload,'?',13);memset(A,0,sizeof(A));
        if(Warm)xrtAtomic32Store(&B.Continue,1,XMEMORY_RELEASE);else rfRelease(F,0);const xredisclusterclientresult* V=cfWait(R);xredisreplyvalue Text;
        testRequire(V->Status==XREDIS_CLUSTER_CLIENT_COMPLETE && V->Route.Query.Status==XREDIS_REQUEST_OK && xredisReplyGet(V->Route.Query.Reply,0,&Text) && Text.Data.Size==13 && !memcmp(Text.Data.Data,"owned\0payload",13),"async command retained caller input");cfStop(F,C);xrtFutureDestroy(R);}
    rffixture* F=rfStart(3);xredisclusterclientconfig K;xredisClusterClientConfigInit(&K);K.Refresh.RetryDelay=K.Route.RetryDelay=K.RefreshMin=1;K.RefreshRetryMax=5;xredisclusteraddress Seed=nnAddress(F->Base,0);Seed.Slot=0;xredisclusterclient* C=NULL;
    testRequire(xredisClusterClientCreate(F->Base->Peers[0].Engine,0,F->Base->Cache,&Seed,1,&K,&C)==XREDIS_ADMISSION_ACCEPTED,"same-worker client");xfuture* R=cfEcho(C,5000);testRequire(cfWait(R)->Route.Query.Status==XREDIS_REQUEST_OK,"same-worker progress");cfStop(F,C);xrtFutureDestroy(R);
}
static void ccSeedEvolution(void)
{
    rffixture* F=rfStart(3);xrtAtomic32Store(&F->Nodes[0].Target,1,XMEMORY_RELEASE);xredisclusterclient* C=cfCreate(F,NULL);cfReady(C);xrtAtomic32Store(&F->Nodes[0].Mode,RF_DENIED,XMEMORY_RELEASE);testRequire(xredisClusterClientInvalidate(C),"evolved seed demand");double Due=xrtTimer()+10;
    while(cfInfo(C).Generation<2 || cfInfo(C).Dirty){testRequire(xrtTimer()<Due,"metadata stranded on original seed");xrtSleep(1);}xredisclustermap* Map=NULL;bool Stale=true;testRequire(xredisClusterClientSnapshot(C,&Map,&Stale)==XREDIS_CLUSTER_VALUE && !Stale,"evolved seed snapshot");xredisclustermapinfo M={0};M.Size=sizeof(M);testRequire(xredisClusterMapInfo(Map,&M) && M.Origin.Port==F->Base->Peers[1].Address.Port && xrtAtomic32Load(&F->Nodes[1].Queries,XMEMORY_ACQUIRE)>=1,"automatic seed discovery origin");
    xrtAtomic32Store(&F->Nodes[1].Mode,RF_DENIED,XMEMORY_RELEASE);xrtAtomic32Store(&F->Nodes[0].Mode,RF_NORMAL,XMEMORY_RELEASE);testRequire(xredisClusterClientInvalidate(C),"bootstrap fallback demand");Due=xrtTimer()+10;while(cfInfo(C).Generation<3 || cfInfo(C).Dirty){testRequire(xrtTimer()<Due,"bootstrap fallback stranded");xrtSleep(1);}xfuture* R=cfEcho(C,5000);testRequire(cfWait(R)->Route.Query.Status==XREDIS_REQUEST_OK,"business after seed evolution");cfStop(F,C);testRequire(xredisClusterMapInfo(Map,&M) && M.Generation==2,"retained evolved map changed");xredisClusterMapRelease(Map);xrtFutureDestroy(R);
}
#if defined(XREDIS_CLUSTER_CLIENT_WHITEBOX)
typedef struct ccdeadlineprobe {rffixture* Fixture;xredisclusterclient* Client;xatomic32 Done;} ccdeadlineprobe;
static void ccInspectDeadline(xnetworker* W,ptr Data)
{
    (void)W;ccdeadlineprobe* P=Data;xredisclient* Native=xrtAtomicPtrLoad(&P->Fixture->Nodes[0].Client,XMEMORY_ACQUIRE);xredisrequest* Q=Native->SentHead;xrtMutexLock(&P->Client->Gate);xredisclusteroperation* O=P->Client->All;
    testRequire(Q && O && O->Route && Q->Deadline==O->Deadline && O->Route->Entered==O->Entered && O->Deadline==xredisClientDeadline(O->Entered,O->Timeout),"managed queue/bootstrap reset original native deadline");xrtMutexUnlock(&P->Client->Gate);xrtAtomic32Store(&P->Done,1,XMEMORY_RELEASE);
}
static void ccDeadline(void)
{
    rffixture* F=rfStart(3);xredisclusterclient* C=cfCreate(F,NULL);cfReady(C);ccbarrier B={0};testRequire(xrtNetEnginePost(F->Base->Peers[0].Engine,1,ccBlock,&B),"managed exact deadline barrier");redisPeerWait(&B.Entered,1);xrtAtomic32Store(&F->Base->HoldBusiness,1,XMEMORY_RELEASE);xfuture* R=cfEcho(C,10001);xrtSleep(20);xrtAtomic32Store(&B.Continue,1,XMEMORY_RELEASE);redisPeerWait(&F->Base->Business,1);ccdeadlineprobe Probe={F,C,{0}};
    testRequire(xrtNetEnginePost(F->Base->Peers[0].Engine,0,ccInspectDeadline,&Probe),"native owner deadline inspection");redisPeerWait(&Probe.Done,1);cfStop(F,C);xrtFutureDestroy(R);
}
#endif
int main(void)
{ccValidate();ccBasic();ccRedirects();ccReplicaRoutes();ccReplicaProfile();ccQueueBounds();ccBusyPoolDoesNotRefresh();ccStops();ccConcurrent();ccRefreshFailure();ccMissing();ccOOM();ccReentry();ccResourceStop();ccAdmissionAllocations();ccCopiedInputs();ccSeedEvolution();
#if defined(XREDIS_CLUSTER_CLIENT_WHITEBOX)
ccDeadline();
#endif
puts("[PASS] managed Cluster client: automatic bootstrap, bounded asynchronous calls, MOVED refresh, ephemeral ASK, queue deadlines, quotas, cancellation, recovery, producers, OOM and retained outcomes");return 0;}
