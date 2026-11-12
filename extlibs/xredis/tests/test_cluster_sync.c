#include <xredis/cluster_client_sync.h>
#include <xredis/cluster_command_sync.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND_SYNC)
#include "support/cluster_command_peer.h"
#else
#include "support/cluster_client_peer.h"
#endif
#define testRequire redisPeerRequire
typedef struct swcall {
    xredisclusterclient* Client;unsigned Mode;int64 Timeout;
    xredisclusterwait Result;int Admission;xatomic32 Started,Done;
    xallocationbudget* Budget;bool Async;
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND_SYNC)
    xrediscommandcatalog* Catalogue;
#endif
#if defined(XREDIS_CLUSTER_SYNC_WHITEBOX)
    bool Clock,WaitError;double Entered;
#endif
} swcall;
static unsigned swModes(void)
{
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND_SYNC)
    return 2;
#else
    return 1;
#endif
}
static int swInvoke(swcall* P)
{
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND_SYNC)
    if(P->Mode){xbytesview A[]={XRT_BYTES_LITERAL("GET"),XRT_BYTES_LITERAL("owned\0{22179}")};
        return xredisClusterClientCallCommandSync(P->Client,P->Catalogue,A,2,XREDIS_CLUSTER_COMMAND_NO_SLOT,P->Timeout,&P->Result);}
#endif
    xbytesview A[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("managed\0route")};
    return xredisClusterClientCallSync(P->Client,1,A,2,P->Timeout,&P->Result);
}
static swcall swPrepare(xredisclusterclient* C,unsigned Mode,int64 Timeout)
{
    swcall P={0};P.Client=C;P.Mode=Mode;P.Timeout=Timeout;
    P.Result=(xredisclusterwait){XWAIT_TIMEOUT,(void*)(uintptr_t)1};
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND_SYNC)
    if(Mode)P.Catalogue=qpCatalogue(1,NULL);
#endif
    return P;
}
static void swDrop(swcall* P)
{
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND_SYNC)
    xredisCommandCatalogRelease(P->Catalogue);
#endif
    if(P->Admission==XREDIS_ADMISSION_ACCEPTED)xrtFutureDestroy(P->Result.Future);
}
static rffixture* swFixture(void)
{
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND_SYNC)
    return qpStart();
#else
    return rfStart(3);
#endif
}
static void swValue(swcall* P)
{
    const xredisclusterclientresult* V=xrtFutureValue(P->Result.Future);xredisreplyvalue Text;
    const char* Expected=P->Mode?"owned\0{22179}":"managed\0route";
    testRequire(P->Admission==XREDIS_ADMISSION_ACCEPTED && P->Result.Wait==XWAIT_OK && V && V->Status==XREDIS_CLUSTER_CLIENT_COMPLETE && V->RouteObserved && V->Route.Query.Status==XREDIS_REQUEST_OK && xredisReplyGet(V->Route.Query.Reply,0,&Text) && Text.Data.Size==13 && !memcmp(Text.Data.Data,Expected,13),"sync retained binary value");
}
static int32 swRun(ptr Data)
{
    swcall* P=Data;xallocationscope Scope={0};if(P->Budget)testRequire(xrtAllocationBudgetEnter(P->Budget,&Scope),"sync caller allocation scope");
#if defined(XREDIS_CLUSTER_SYNC_WHITEBOX)
    if(P->Clock){P->Entered=xrtTimer();swClockBase=P->Entered;swClockCalls=0;swClockArmed=true;}
    swFailWait=P->WaitError;
#endif
    xrtAtomic32Store(&P->Started,1,XMEMORY_RELEASE);
    if(P->Async){xfuture* F=NULL;
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND_SYNC)
        if(P->Mode){xbytesview A[]={XRT_BYTES_LITERAL("GET"),XRT_BYTES_LITERAL("owned\0{22179}")};P->Admission=xredisClusterClientCallCommand(P->Client,P->Catalogue,A,2,XREDIS_CLUSTER_COMMAND_NO_SLOT,P->Timeout,&F);}
        else
#endif
        {xbytesview A[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("managed\0route")};P->Admission=xredisClusterClientCall(P->Client,1,A,2,P->Timeout,&F);}
        if(P->Admission==XREDIS_ADMISSION_ACCEPTED){xwaitresult W=xrtFutureWait(F);P->Result=(xredisclusterwait){W,F};}
    }else P->Admission=swInvoke(P);
#if defined(XREDIS_CLUSTER_SYNC_WHITEBOX)
    swClockArmed=false;swFailWait=false;
#endif
    if(P->Budget)testRequire(xrtAllocationBudgetLeave(&Scope),"sync caller allocation leave");
    xrtAtomic32Store(&P->Done,1,XMEMORY_RELEASE);return 0;
}
static void swJoin(xthread* T)
{testRequire(T && xrtThreadWaitFor(T,15000)==XWAIT_OK && !xrtThreadExitCode(T),"sync caller joined");xrtThreadDestroy(T);}
static void swBasic(void)
{
    for(unsigned Mode=0;Mode<swModes();++Mode){rffixture* F=swFixture();xredisclusterclient* C=cfCreate(F,NULL);swcall P=swPrepare(C,Mode,-1);P.Admission=swInvoke(&P);swValue(&P);
        cfStop(F,C);swValue(&P);swDrop(&P);}
}
static void swStopsAndRedirects(void)
{
    for(unsigned Mode=0;Mode<swModes();++Mode){
        for(unsigned Kind=0;Kind<4;++Kind){rffixture* F=swFixture();xredisclusterclient* C=cfCreate(F,NULL);cfReady(C);swcall P=swPrepare(C,Mode,Kind==0?0:Kind==1?100:5000);
            if(Kind==3){cfClose(C);P.Admission=swInvoke(&P);testRequire(P.Admission==XREDIS_ADMISSION_CLOSED && P.Result.Future==(void*)(uintptr_t)1 && P.Result.Wait==XWAIT_TIMEOUT,"closed sync changed output");}
            else {xrtAtomic32Store(&F->Base->HoldBusiness,1,XMEMORY_RELEASE);xthread* T=xrtThreadCreate(swRun,&P,0);testRequire(T!=NULL,"stopped sync thread");redisPeerWait(&P.Started,1);
                if(Kind==2){redisPeerWait(&F->Base->Business,1);testRequire(!xrtAtomic32Load(&P.Done,XMEMORY_ACQUIRE) && P.Result.Future==(void*)(uintptr_t)1,"sync returned/published before outcome");cfClose(C);}swJoin(T);
                const xredisclusterclientresult* V=xrtFutureValue(P.Result.Future);testRequire(P.Admission==XREDIS_ADMISSION_ACCEPTED && P.Result.Wait==XWAIT_OK && V && V->Status==(Kind==2?XREDIS_CLUSTER_CLIENT_CLOSED:XREDIS_CLUSTER_CLIENT_TIMEOUT),"sync stop typed status");
                if(!Kind)testRequire(!V->Native && !xrtAtomic32Load(&F->Base->Business,XMEMORY_ACQUIRE),"zero timeout submitted business");
                else testRequire(V->Native && V->RouteObserved,"sync stop lost native physical outcome");}
            cfStop(F,C);swDrop(&P);}
        for(unsigned Kind=1;Kind<=2;++Kind){rffixture* F=swFixture();xredisclusterclient* C=cfCreate(F,NULL);cfReady(C);xrtAtomic32Store(&F->Base->Nodes[0].Target,1,XMEMORY_RELEASE);xrtAtomic32Store(&F->Base->Nodes[0].Redirect,Kind,XMEMORY_RELEASE);xrtAtomic32Store(&F->Nodes[0].Target,1,XMEMORY_RELEASE);
            swcall P=swPrepare(C,Mode,5000);P.Admission=swInvoke(&P);swValue(&P);const xredisclusterclientresult* V=xrtFutureValue(P.Result.Future);testRequire(V->Route.Attempts==2 && V->Route.Redirects==1 && V->Route.Asks==(Kind==2) && V->Route.Moved==(Kind==1),"sync redirect counters");cfStop(F,C);swValue(&P);swDrop(&P);}
    }
}
typedef struct swprobe {swcall* Call;xatomic32 Done;} swprobe;
static void swUnrelatedWorker(xnetworker* W,ptr Data)
{(void)W;swprobe* P=Data;P->Call->Admission=swInvoke(P->Call);swValue(P->Call);xrtAtomic32Store(&P->Done,1,XMEMORY_RELEASE);}
static void swWorker(xnetworker* W,ptr Data)
{(void)W;swprobe* P=Data;uint64 Before=cfInfo(P->Call->Client).Accepted;P->Call->Admission=swInvoke(P->Call);testRequire(P->Call->Admission==XREDIS_ADMISSION_ERROR && xrtErrorKind(xrtGetError())==XERR_STATE && P->Call->Result.Future==(void*)(uintptr_t)1 && P->Call->Result.Wait==XWAIT_TIMEOUT && cfInfo(P->Call->Client).Accepted==Before,"sync worker refusal admitted/changed output");xrtClearError();xrtAtomic32Store(&P->Done,1,XMEMORY_RELEASE);}
static void swWorkers(void)
{
    rffixture* F=swFixture();xnetengineconfig K;xrtNetEngineConfigInit(&K);K.Workers=2;xnetengine* Progress=xrtNetEngineCreate(&K);testRequire(Progress && xrtNetEngineStart(Progress),"separate progress engine");
    xredisclusterclientconfig Options;xredisClusterClientConfigInit(&Options);xredisclusteraddress Seed=nnAddress(F->Base,0);Seed.Slot=0;xredisclusterclient* C=NULL;testRequire(xredisClusterClientCreate(Progress,1,F->Base->Cache,&Seed,1,&Options,&C)==XREDIS_ADMISSION_ACCEPTED,"cross-engine sync client");cfReady(C);
    for(unsigned Mode=0;Mode<swModes();++Mode){swcall P=swPrepare(C,Mode,5000);for(unsigned Engine=0;Engine<2;++Engine)for(unsigned Worker=0;Worker<2;++Worker){swprobe Q={&P,{0}};testRequire(xrtNetEnginePost(Engine?Progress:F->Base->Peers[0].Engine,Worker,swWorker,&Q),"sync refusal worker post");redisPeerWait(&Q.Done,1);}swDrop(&P);}
    xnetengine* Unrelated=xrtNetEngineCreate(&K);testRequire(Unrelated && xrtNetEngineStart(Unrelated),"unrelated Engine");
    for(unsigned Mode=0;Mode<swModes();++Mode){swcall P=swPrepare(C,Mode,5000);swprobe Q={&P,{0}};testRequire(xrtNetEnginePost(Unrelated,0,swUnrelatedWorker,&Q),"unrelated worker sync call");redisPeerWait(&Q.Done,1);swDrop(&P);}
    testRequire(xrtNetEngineStop(Unrelated) && xrtNetEngineDestroy(Unrelated),"unrelated Engine retirement");
    cfClose(C);testRequire(xrtNetEngineStop(Progress) && xrtNetEngineDestroy(Progress),"progress Engine pin retired");
    /* Closed metadata may be used after its Engine no longer exists. */
    for(unsigned Mode=0;Mode<swModes();++Mode){swcall P=swPrepare(C,Mode,5000);P.Admission=swInvoke(&P);testRequire(P.Admission==XREDIS_ADMISSION_CLOSED && P.Result.Future==(void*)(uintptr_t)1,"closed facade dereferenced retired engine");swDrop(&P);}cfStop(F,C);
}
static void swRefusals(void)
{
    rffixture* F=swFixture();xredisclusterclientconfig Config;xredisClusterClientConfigInit(&Config);Config.MaxRequests=1;xredisclusterclient* C=cfCreate(F,&Config);cfReady(C);xrtAtomic32Store(&F->Base->HoldBusiness,1,XMEMORY_RELEASE);xfuture* Held=cfEcho(C,5000);redisPeerWait(&F->Base->Business,1);
    for(unsigned Mode=0;Mode<swModes();++Mode){swcall P=swPrepare(C,Mode,5000);P.Admission=swInvoke(&P);testRequire(P.Admission==XREDIS_ADMISSION_OVERLOADED && P.Result.Future==(void*)(uintptr_t)1 && P.Result.Wait==XWAIT_TIMEOUT,"sync overload output");swDrop(&P);}
    cfStop(F,C);xrtFutureDestroy(Held);
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND_SYNC)
    F=swFixture();C=cfCreate(F,NULL);cfReady(C);xrediscommandcatalog* K=qpCatalogue(1,NULL);xbytesview Missing[]={XRT_BYTES_LITERAL("unknown")},Exotic[]={XRT_BYTES_LITERAL("SORT"),XRT_BYTES_LITERAL("key")},Cross[]={XRT_BYTES_LITERAL("MSET"),XRT_BYTES_LITERAL("a{a}"),XRT_BYTES_LITERAL("1"),XRT_BYTES_LITERAL("b{b}"),XRT_BYTES_LITERAL("2")},Arity[]={XRT_BYTES_LITERAL("GET")},Keyless[]={XRT_BYTES_LITERAL("PING")};
    struct {xbytesview* A;size_t N;xredisclustercommandadmission S;} Cases[]={{Missing,1,XREDIS_CLUSTER_COMMAND_UNKNOWN},{Exotic,2,XREDIS_CLUSTER_COMMAND_UNSUPPORTED},{Cross,5,XREDIS_CLUSTER_COMMAND_CROSS_SLOT},{Arity,1,XREDIS_CLUSTER_COMMAND_ARGUMENTS},{Keyless,1,XREDIS_CLUSTER_COMMAND_KEYLESS}};
    xrtSetErrorKind(XERR_STATE);const xerror* Error=xrtGetError();xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"sync refusal no-heap scope");uint64 Accepted=cfInfo(C).Accepted;
    for(size_t I=0;I<sizeof(Cases)/sizeof(*Cases);++I){xredisclusterwait R={XWAIT_TIMEOUT,(void*)(uintptr_t)1};testRequire(xredisClusterClientCallCommandSync(C,K,Cases[I].A,Cases[I].N,XREDIS_CLUSTER_COMMAND_NO_SLOT,5000,&R)==Cases[I].S && R.Future==(void*)(uintptr_t)1 && R.Wait==XWAIT_TIMEOUT && xrtGetError()==Error,"sync typed refusal/ambient output");}
    testRequire(xrtAllocationBudgetLeave(&Scope),"sync refusal scope leave");xallocationbudgetinfo BI={0};BI.Size=sizeof(BI);testRequire(xrtAllocationBudgetInfo(B,&BI) && !BI.PeakBytes && !BI.Rejections && cfInfo(C).Accepted==Accepted,"sync refusal allocated/admitted");xrtAllocationBudgetFree(B);xrtClearError();
    xredisclusterwait Ping;testRequire(xredisClusterClientCallCommandSync(C,K,Keyless,1,1,-1,&Ping)==XREDIS_CLUSTER_COMMAND_ACCEPTED && Ping.Wait==XWAIT_OK && cfWait(Ping.Future)->Route.Query.Status==XREDIS_REQUEST_OK,"sync explicit keyless policy");xrtFutureDestroy(Ping.Future);xredisCommandCatalogRelease(K);cfStop(F,C);
#endif
}
static void swValidate(void)
{
    rffixture* F=swFixture();xredisclusterclient* C=cfCreate(F,NULL);cfReady(C);xbytesview A[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("managed\0route")};xredisclusterwait R={XWAIT_TIMEOUT,(void*)(uintptr_t)1};
    testRequire(xredisClusterClientCallSync(NULL,1,A,2,5000,&R)==XREDIS_ADMISSION_ERROR && R.Future==(void*)(uintptr_t)1,"sync null client");xrtClearError();
    testRequire(xredisClusterClientCallSync(C,1,A,2,5000,(void*)C)==XREDIS_ADMISSION_ERROR,"sync control alias");xrtClearError();
    testRequire(xredisClusterClientCallSync(C,1,A,2,5000,(void*)((char*)A+sizeof(A)-sizeof(void*)))==XREDIS_ADMISSION_ERROR,"sync descriptor-tail alias");xrtClearError();
    testRequire(xredisClusterClientCallSync(C,1,A,2,5000,(void*)(A[1].Data+12))==XREDIS_ADMISSION_ERROR,"sync input-byte alias");xrtClearError();
    xredisclustermap* Map=NULL;bool Stale=false;testRequire(xredisClusterClientSnapshot(C,&Map,&Stale)==XREDIS_CLUSTER_VALUE,"sync alias map snapshot");testRequire(xredisClusterClientCallSync(C,1,A,2,5000,(void*)Map)==XREDIS_ADMISSION_ERROR,"sync map alias");xrtClearError();xredisClusterMapRelease(Map);
    for(unsigned Kind=0;Kind<5;++Kind){testRequire(xredisClusterClientCallSync(C,Kind==0?20000:1,Kind==1?NULL:A,Kind==2?SIZE_MAX:2,Kind==3?-2:Kind==4?86400001:5000,&R)==XREDIS_ADMISSION_ERROR && R.Future==(void*)(uintptr_t)1 && R.Wait==XWAIT_TIMEOUT,"sync invalid API changed output");xrtClearError();}
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND_SYNC)
    xrediscommandcatalog* K=qpCatalogue(0,NULL);testRequire(xredisClusterClientCallCommandSync(C,K,A,2,1,5000,(void*)K)==XREDIS_CLUSTER_COMMAND_ERROR,"sync catalogue alias");xrtClearError();
    testRequire(xredisClusterClientCallCommandSync(C,K,A,SIZE_MAX,1,5000,&R)==XREDIS_CLUSTER_COMMAND_LIMIT && R.Future==(void*)(uintptr_t)1,"sync metadata argument cap");xredisCommandCatalogRelease(K);
#endif
    cfStop(F,C);
}
static void swDrain(xallocationbudget* B)
{double Due=xrtTimer()+10;for(;;){xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && xrtTimer()<Due,"sync caller budget drain");if(!I.LiveBytes && !I.LiveCount)return;xrtSleep(1);}}
static void swOOM(void)
{
    rffixture* F=swFixture();xredisclusterclient* C=cfCreate(F,NULL);cfReady(C);
    for(unsigned Mode=0;Mode<swModes();++Mode){unsigned OK=0,Failed=0;swcall P=swPrepare(C,Mode,5000);
        for(size_t Limit=1;Limit<=8192;Limit+=127){P.Result=(xredisclusterwait){XWAIT_TIMEOUT,(void*)(uintptr_t)1};xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&Scope),"sync OOM scope");P.Admission=swInvoke(&P);testRequire(xrtAllocationBudgetLeave(&Scope),"sync OOM leave");
            if(P.Admission==XREDIS_ADMISSION_ACCEPTED){++OK;swValue(&P);xrtFutureDestroy(P.Result.Future);}else {++Failed;testRequire(P.Admission==XREDIS_ADMISSION_ERROR && P.Result.Future==(void*)(uintptr_t)1 && P.Result.Wait==XWAIT_TIMEOUT,"sync OOM output");xrtClearError();}swDrain(B);xrtAllocationBudgetFree(B);}
        testRequire(OK && Failed,"sync OOM no accepted/failure paths");P.Admission=XREDIS_ADMISSION_ERROR;swDrop(&P);}
    cfStop(F,C);
}
static void swAllocationBound(void)
{
    for(unsigned Mode=0;Mode<swModes();++Mode){size_t Blocks=0,Bytes=0;
        for(unsigned Sync=0;Sync<2;++Sync){rffixture* F=swFixture();xredisclusterclient* C=cfCreate(F,NULL);cfReady(C);xrtAtomic32Store(&F->Base->HoldBusiness,1,XMEMORY_RELEASE);swcall P=swPrepare(C,Mode,5000);P.Async=!Sync;
            testRequire(xrtAllocationBudgetCreate(1048576,&P.Budget),"sync held caller budget");xthread* T=xrtThreadCreate(swRun,&P,0);testRequire(T!=NULL,"sync bounded caller thread");redisPeerWait(&P.Started,1);redisPeerWait(&F->Base->Business,1);xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(P.Budget,&I) && I.LiveCount && I.LiveCount<=4 && !I.Rejections,"sync warm caller allocation bound");
            if(Sync)testRequire(I.LiveCount==Blocks && I.LiveBytes==Bytes,"sync added caller allocation/storage");else {Blocks=I.LiveCount;Bytes=I.LiveBytes;}
            cfClose(C);swJoin(T);xrtFutureDestroy(P.Result.Future);P.Admission=XREDIS_ADMISSION_ERROR;cfStop(F,C);swDrain(P.Budget);xrtAllocationBudgetFree(P.Budget);P.Budget=NULL;swDrop(&P);}
    }
}
static int32 swProducer(ptr Data)
{swcall* P=Data;for(unsigned I=0;I<16;++I){P->Admission=swInvoke(P);swValue(P);if(I!=15)xrtFutureDestroy(P->Result.Future);}return 0;}
static void swConcurrent(void)
{
    rffixture* F=swFixture();xredisclusterclient* C=cfCreate(F,NULL);swcall P[4];xthread* T[4];for(unsigned I=0;I<4;++I){P[I]=swPrepare(C,I%swModes(),5000);T[I]=xrtThreadCreate(swProducer,&P[I],0);testRequire(T[I]!=NULL,"sync producer");}for(unsigned I=0;I<4;++I)swJoin(T[I]);cfStop(F,C);for(unsigned I=0;I<4;++I){swValue(&P[I]);swDrop(&P[I]);}
}
#if defined(XREDIS_CLUSTER_SYNC_WHITEBOX)
typedef struct swdeadline {rffixture* F;swcall* P;xatomic32 Done;} swdeadline;
static void swInspect(xnetworker* W,ptr Data)
{
    (void)W;swdeadline* X=Data;xredisclient* Native=xrtAtomicPtrLoad(&X->F->Nodes[0].Client,XMEMORY_ACQUIRE);xredisrequest* Q=Native->SentHead;xrtMutexLock(&X->P->Client->Gate);xredisclusteroperation* O=X->P->Client->All;
    int64 Timeout=X->P->Timeout==-1?5007:X->P->Timeout;testRequire(Q && O && O->Entered==X->P->Entered && O->Route->Entered==X->P->Entered && Q->Deadline==xredisClientDeadline(X->P->Entered,Timeout) && O->Deadline==Q->Deadline,"sync facade reset entry/native deadline");xrtMutexUnlock(&X->P->Client->Gate);xrtAtomic32Store(&X->Done,1,XMEMORY_RELEASE);
}
static void swWhitebox(void)
{
    for(unsigned Mode=0;Mode<swModes();++Mode){
        for(unsigned Default=0;Default<2;++Default){rffixture* F=swFixture();xredisclusterclientconfig K;xredisClusterClientConfigInit(&K);K.Route.Timeout=5007;xredisclusterclient* C=cfCreate(F,&K);cfReady(C);xrtAtomic32Store(&F->Base->HoldBusiness,1,XMEMORY_RELEASE);swcall P=swPrepare(C,Mode,Default?-1:10001);P.Clock=true;xthread* T=xrtThreadCreate(swRun,&P,0);testRequire(T!=NULL,"sync clock caller");redisPeerWait(&P.Started,1);redisPeerWait(&F->Base->Business,1);swdeadline X={F,&P,{0}};testRequire(xrtNetEnginePost(F->Base->Peers[0].Engine,0,swInspect,&X),"sync exact native deadline post");redisPeerWait(&X.Done,1);cfClose(C);swJoin(T);testRequire(P.Result.Wait==XWAIT_OK && cfWait(P.Result.Future)->Status==XREDIS_CLUSTER_CLIENT_CLOSED,"sync clock close");cfStop(F,C);swDrop(&P);}
        rffixture* F=swFixture();xredisclusterclient* C=cfCreate(F,NULL);cfReady(C);xrtAtomic32Store(&F->Base->HoldBusiness,1,XMEMORY_RELEASE);swcall P=swPrepare(C,Mode,5000);P.WaitError=true;xthread* T=xrtThreadCreate(swRun,&P,0);swJoin(T);testRequire(P.Admission==XREDIS_ADMISSION_ACCEPTED && P.Result.Wait==XWAIT_ERROR && P.Result.Future && xrtFutureState(P.Result.Future)==XFUTURE_PENDING,"native wait failure lost admitted Future");redisPeerWait(&F->Base->Business,1);cfStop(F,C);testRequire(cfWait(P.Result.Future)->Status==XREDIS_CLUSTER_CLIENT_CLOSED,"wait-error Future stopped progressing");swDrop(&P);
    }
}
#endif
int main(void)
{
    swBasic();swStopsAndRedirects();swWorkers();swRefusals();swValidate();swOOM();swAllocationBound();swConcurrent();
#if defined(XREDIS_CLUSTER_SYNC_WHITEBOX)
    swWhitebox();
#endif
    puts("[PASS] synchronous Cluster: cold/default/zero calls, full metadata refusals, both Engine worker guards, MOVED/ASK, timeout/close, owned outcomes, alias bounds, allocation faults, concurrent callers and exact deadlines/wait failure");return 0;
}
