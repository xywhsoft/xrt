#include "test.h"
#include "support/cluster_route_peer.h"
#include "support/cluster_topology_fixture.h"
static xredisclusterroute* rtCreate(crfixture* F,xredisclustertopology* T,const xredisclusterrouteconfig* Config)
{
    xredisclusterrouteconfig C;if(Config)C=*Config;else crConfig(F,&C);
    unsigned char Bytes[]={'r',0,0xff};xbytesview A[]={XRT_BYTES_LITERAL("ECHO"),{Bytes,sizeof(Bytes)}};xredisclusterroute* R=NULL;
    testRequire(xredisClusterRouteCreate(T,(xbytesview)XRT_BYTES_LITERAL("127.0.0.1"),1,A,2,&C,&R)==XREDIS_ADMISSION_ACCEPTED,"route creation");
    memset(Bytes,'x',sizeof(Bytes));return R;
}
static void rtEcho(xredisclusterrouteresult R)
{
    xredisreplyvalue V;testRequire(R.Status==XREDIS_CLUSTER_ROUTE_COMPLETE && R.RequestObserved && R.Query.Status==XREDIS_REQUEST_OK &&
        xredisReplyGet(R.Query.Reply,0,&V) && V.Data.Size==3 && !memcmp(V.Data.Data,"r\0\xff",3),"routed copied binary result");
}
static void rtPaths(void)
{
    for(unsigned Kind=0;Kind<4;++Kind){
        crfixture* F=crStart();xredisclustertopology* T=crTopology(F->Peer.Peers[0].Address.Port,"127.0.0.1",0,16383);
        if(Kind){crMode(F,0,Kind==1?CR_MOVED:CR_ASK,1);if(Kind==3)crMode(F,1,CR_MOVED,2);}
        xredisclusterroute* R=rtCreate(F,T,NULL);xredisClusterTopologyRelease(T);xredisclusterrouteresult Result=crDrive(R);rtEcho(Result);
        testRequire(Result.Attempts==(Kind==3?3u:Kind?2u:1u) && Result.Redirects==Result.Attempts-1,"automatic route attempts");
        testRequire(Result.Moved==(Kind==1 || Kind==3) && Result.Asks==(Kind>=2),"route MOVED/ASK observations");
        testRequire(Result.UsedAsking==(Kind==2) && Result.ControlReceived==(Kind==2),"ASK applies to one attempt");
        testRequire(xrtAtomic32Load(&F->Nodes[1].Prefixes,XMEMORY_ACQUIRE)==(Kind>=2),"native ASKING on redirect target");
        testRequire(Result.AttemptAddress.Port==F->Peer.Peers[Kind==3?2:Kind?1:0].Address.Port,"native attempt address snapshot");
        xfuture* Final=xredisClusterRouteResultFuture(R);testRequire(Final!=NULL,"retained final native Future");xredisClusterRouteDestroy(R);crStop(F);
        const xredisrequestresult* Q=xrtFutureValue(Final);testRequire(Q && Q->Status==XREDIS_REQUEST_OK,"native result survives route/pools/engine");xrtFutureDestroy(Final);
    }
}
static void rtSettledWaitHandle(void)
{
    crfixture* F=crStart();crMode(F,0,CR_HOLD,0);xredisclustertopology* T=crTopology(F->Peer.Peers[0].Address.Port,"127.0.0.1",0,16383);
    xredisclusterroute* R=rtCreate(F,T,NULL);xredisClusterTopologyRelease(T);
    testRequire(xredisClusterRouteStep(R)==XREDIS_CLUSTER_ROUTE_WAIT,"held native route waits");
    double Wake=xredisClusterRouteNextWake(R);xfuture* First=xredisClusterRoutePending(R);testRequire(First && Wake>xrtTimer(),"original wait observation");
    redisPeerWait(&F->Nodes[0].Queries,1);redisPeerReleaseBlocked(&F->Peer.Peers[0]);
    testRequire(xrtFutureWaitFor(First,5000)==XWAIT_OK,"complete between wake and handle observations");
    xfuture* Settled=xredisClusterRoutePending(R);
    testRequire(Settled==First && xrtFutureWaitFor(Settled,0)==XWAIT_OK,"settled native wait handle must remain observable until Step");
    xrtFutureDestroy(Settled);xrtFutureDestroy(First);testRequire(xredisClusterRouteStep(R)==XREDIS_CLUSTER_ROUTE_DONE,"consume settled native attempt");
    testRequire(!xredisClusterRoutePending(R),"DONE exposes no pending handle");xredisclusterrouteresult V={0};V.Size=sizeof(V);testRequire(xredisClusterRouteResult(R,&V),"settled route result");rtEcho(V);
    xredisClusterRouteDestroy(R);crStop(F);
}
static void rtLimitsAndErrors(void)
{
    for(unsigned Kind=0;Kind<5;++Kind){
        crfixture* F=crStart();xredisclustertopology* T=crTopology(F->Peer.Peers[0].Address.Port,"127.0.0.1",0,16383);
        crMode(F,0,Kind==0?CR_MOVED:Kind==1?CR_WRONG_SLOT:Kind==2?CR_BAD_REDIRECT:Kind==3?CR_ASK:CR_DROP,1);
        if(Kind==0)crMode(F,1,CR_MOVED,0);
        if(Kind==3){xrtAtomic32Store(&F->Nodes[1].Denied,1,XMEMORY_RELEASE);crMode(F,1,CR_MOVED,2);}
        xredisclusterrouteconfig C;crConfig(F,&C);C.MaxRedirects=2;xredisclusterroute* R=rtCreate(F,T,&C);xredisClusterTopologyRelease(T);xredisclusterrouteresult Result=crDrive(R);
        testRequire(Result.Status==(Kind==0?XREDIS_CLUSTER_ROUTE_LIMIT:Kind==1 || Kind==2?XREDIS_CLUSTER_ROUTE_PROTOCOL:XREDIS_CLUSTER_ROUTE_COMPLETE),"typed route protocol/redirect/transport outcome");
        if(Kind==0)testRequire(Result.Attempts==3 && Result.Redirects==2 && Result.Moved==3,"redirect cycle hard bound");
        if(Kind==3)testRequire(Result.ControlReceived && Result.Control.Status==XREDIS_REQUEST_SERVER_ERROR && Result.Query.Status==XREDIS_REQUEST_SERVER_ERROR && Result.Attempts==2 && Result.Moved==0,"denied ASKING cannot authorize query replay");
        if(Kind==4)testRequire(Result.Attempts==1 && Result.Query.Status==XREDIS_REQUEST_DISCONNECTED && Result.Query.Delivery==XREDIS_MAY_HAVE_EXECUTED,"uncertain request never replays");
        xredisClusterRouteDestroy(R);crStop(F);
    }
}
static void rtWaitAndPolicy(void)
{
    for(unsigned Kind=0;Kind<5;++Kind){
        crfixture* F=crStart();xredisclustertopology* T=crTopology(F->Peer.Peers[0].Address.Port,"127.0.0.1",0,16383);xredisclusterrouteconfig C;crConfig(F,&C);
        if(Kind==0)xrtAtomic32Store(&F->WaitFirst,3,XMEMORY_RELEASE);
        if(Kind==1){xrtAtomic32Store(&F->WaitFirst,100,XMEMORY_RELEASE);C.MaxWaits=2;}
        if(Kind>=2)xrtAtomic32Store(&F->Force,Kind==2?XREDIS_CLUSTER_POOL_DENIED:Kind==3?XREDIS_CLUSTER_POOL_ERROR:XREDIS_CLUSTER_POOL_MEMORY,XMEMORY_RELEASE);
        xredisclusterroute* R=rtCreate(F,T,&C);xredisClusterTopologyRelease(T);xredisclusterrouteresult Result=crDrive(R);
        if(!Kind){rtEcho(Result);testRequire(Result.Waits==3 && Result.ProviderCalls==4 && F->Budgets[3]<F->Budgets[0],"admission waits inherit original budget");}
        else testRequire(Result.Status==(Kind==1?XREDIS_CLUSTER_ROUTE_OVERLOADED:Kind==2?XREDIS_CLUSTER_ROUTE_POLICY:Kind==3?XREDIS_CLUSTER_ROUTE_PROVIDER_ERROR:XREDIS_CLUSTER_ROUTE_MEMORY) && Result.Attempts==0,"provider wait/policy/OOM outcomes");
        xredisClusterRouteDestroy(R);crStop(F);
    }
    crfixture* F=crStart();xrtAtomic32Store(&F->Delay,200,XMEMORY_RELEASE);xredisclustertopology* T=crTopology(F->Peer.Peers[0].Address.Port,"127.0.0.1",0,16383);xredisclusterrouteconfig C;crConfig(F,&C);C.Timeout=100;
    xredisclusterroute* R=rtCreate(F,T,&C);xredisClusterTopologyRelease(T);xredisclusterrouteresult Result=crDrive(R);
    testRequire(Result.Status==XREDIS_CLUSTER_ROUTE_TIMEOUT && !Result.Attempts && !Result.AdmissionObserved && Result.ProviderCalls==1 && !xrtAtomic32Load(&F->Nodes[0].Queries,XMEMORY_ACQUIRE),"late provider cannot start business after original deadline");xredisClusterRouteDestroy(R);crStop(F);
}
static void rtStops(void)
{
    for(unsigned Kind=0;Kind<4;++Kind){
        crfixture* F=crStart();crMode(F,0,CR_HOLD,0);xredisclustertopology* T=crTopology(F->Peer.Peers[0].Address.Port,"127.0.0.1",0,16383);xredisclusterrouteconfig C;crConfig(F,&C);if(Kind==0)C.Timeout=0;if(Kind==2)C.Timeout=80;
        xredisclusterroute* R=rtCreate(F,T,&C);xredisClusterTopologyRelease(T);
        if(Kind==1)xredisClusterRouteCancel(R);
        if(Kind>=2){testRequire(xredisClusterRouteStep(R)==XREDIS_CLUSTER_ROUTE_WAIT,"native pending route");redisPeerWait(&F->Nodes[0].Queries,1);if(Kind==3)xredisClusterRouteCancel(R);}
        xredisclusterrouteresult Result=crDrive(R);testRequire(Result.Status==(Kind==0 || Kind==2?XREDIS_CLUSTER_ROUTE_TIMEOUT:XREDIS_CLUSTER_ROUTE_CANCELLED),"original deadline/cancel route");
        if(Kind<2)testRequire(!Result.ProviderCalls && !Result.Attempts,"due/cancelled route started I/O");
        xfuture* Last=xredisClusterRouteResultFuture(R);
        if(Last){testRequire(xrtFutureWaitFor(Last,10000)==XWAIT_OK,"native stop settles independently");redisPeerReleaseBlocked(&F->Peer.Peers[0]);}
        xredisclusterrouteresult After={0};After.Size=sizeof(After);testRequire(xredisClusterRouteResult(R,&After) && After.RequestObserved==Result.RequestObserved && After.Status==Result.Status,"late native reply cannot mutate route stop snapshot");
        xredisClusterRouteDestroy(R);crStop(F);xrtFutureDestroy(Last);
    }
    crfixture* F=crStart();crMode(F,0,CR_HOLD,0);xredisclustertopology* T=crTopology(F->Peer.Peers[0].Address.Port,"127.0.0.1",0,16383);xredisclusterroute* R=rtCreate(F,T,NULL);xredisClusterTopologyRelease(T);
    xredisClusterRouteStep(R);redisPeerWait(&F->Nodes[0].Queries,1);xfuture* Pending=xredisClusterRoutePending(R);testRequire(Pending!=NULL,"route native observer");xredisClusterRouteDestroy(R);
    redisPeerReleaseBlocked(&F->Peer.Peers[0]);testRequire(xrtFutureWaitFor(Pending,10000)==XWAIT_OK,"destroyed route native duties survive");crStop(F);xrtFutureDestroy(Pending);
}
static xredisclustertopology* rtParse(cstr Wire,xredisclusterformat Format)
{
    xredisreplyreader* Reader=NULL;xredisreply* Reply=NULL;size_t Used=0;xredisclustertopology* T=NULL;
    testRequire(xredisReplyReaderCreate(NULL,&Reader) && xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Wire,strlen(Wire)},&Used,&Reply)==XREDIS_REPLY_READY && Used==strlen(Wire),"route metadata parse");
    testRequire(xredisClusterTopologyCreate(Reply,Format,NULL,&T)==XREDIS_CLUSTER_VALUE,"route metadata snapshot");xredisReplyRelease(Reply);xredisReplyReaderDestroy(Reader);return T;
}
static void rtMetadata(void)
{
    crfixture* F=crStart();
    for(unsigned Kind=0;Kind<4;++Kind){xredisclustertopology* T=Kind==3?rtParse(CT_SHARDS3,XREDIS_CLUSTER_SHARDS):crTopology(F->Peer.Peers[0].Address.Port,Kind==0?"?":Kind==1?"":"127.0.0.1",Kind==2?2:0,Kind==2?3:16383);
        xredisclusterrouteconfig C;crConfig(F,&C);if(Kind==3){C.Tls=true;xrtAtomic32Store(&F->Force,XREDIS_CLUSTER_POOL_DENIED,XMEMORY_RELEASE);}
        xredisclusterroute* R=rtCreate(F,T,&C);xredisClusterTopologyRelease(T);xredisclusterrouteresult Result=crDrive(R);
        if(Kind==1)rtEcho(Result);else testRequire(Result.Status==(Kind==3?XREDIS_CLUSTER_ROUTE_POLICY:XREDIS_CLUSTER_ROUTE_NO_ROUTE),"unknown/unassigned/TLS endpoint policy");
        if(Kind==3)testRequire(Result.Address.Tls && Result.Address.Port==6380,"SHARDS uses explicit TLS port without downgrade");
        xredisClusterRouteDestroy(R);
    }
    crStop(F);
}
static void rtBudgetZero(xallocationbudget* B)
{
    double Due=xrtTimer()+10;
    for(;;){xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && xrtTimer()<Due,"route budget retirement");if(!I.LiveCount && !I.LiveBytes)return;xrtThreadYield();}
}
static void rtHostGrowth(void)
{
    for(unsigned Kind=0;Kind<4;++Kind){
        crfixture* F=crStart();F->Alias="node.example.fixture";crMode(F,0,CR_MOVED,1);
        xrtAtomicPtrStore(&F->Nodes[0].RedirectHost,(ptr)(Kind?F->Alias:""),XMEMORY_RELEASE);
        xredisclustertopology* T=crTopology(F->Peer.Peers[0].Address.Port,"127.0.0.1",0,16383);xredisclusterrouteconfig C;crConfig(F,&C);C.MaxHostBytes=64;
        xredisclusterroute* R=rtCreate(F,T,&C);xredisclusterrouteresult Initial={0};Initial.Size=sizeof(Initial);testRequire(xredisClusterRouteResult(R,&Initial),"initial controller storage");
        if(Kind==3){xredisClusterRouteDestroy(R);C.MaxStorageBytes=Initial.StorageBytes+2*C.MaxHostBytes-1;R=rtCreate(F,T,&C);}
        xredisClusterTopologyRelease(T);xredisClusterRouteStep(R);xfuture* Native=xredisClusterRoutePending(R);
        if(Native){testRequire(xrtFutureWaitFor(Native,10000)==XWAIT_OK,"host-growth redirect arrived");xrtFutureDestroy(Native);}
        xallocationbudget* Budget=NULL;xallocationscope Scope={0};
        if(Kind==2)testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"host-growth OOM scope");
        xredisClusterRouteStep(R);
        if(Kind==2)testRequire(xrtAllocationBudgetLeave(&Scope),"host-growth OOM leave");
        xredisclusterrouteresult Result=crDrive(R);
        if(Kind<2){rtEcho(Result);testRequire(Result.Address.Host.Size==(Kind?strlen(F->Alias):9),"empty-source fallback/long redirect host");
            testRequire(Result.StorageBytes==Initial.StorageBytes+(Kind?2*C.MaxHostBytes:0),"host storage grows once only when required");}
        else testRequire(Result.Status==(Kind==2?XREDIS_CLUSTER_ROUTE_MEMORY:XREDIS_CLUSTER_ROUTE_LIMIT) && Result.Attempts==1 && Result.RequestObserved && Result.Query.Status==XREDIS_REQUEST_SERVER_ERROR,"host-growth stop keeps prior native observation");
        xredisClusterRouteDestroy(R);crStop(F);if(Budget){rtBudgetZero(Budget);xrtAllocationBudgetFree(Budget);}xrtClearError();
    }
}
static void rtMemory(void)
{
    crfixture* F=crStart();xredisclustertopology* T=crTopology(F->Peer.Peers[0].Address.Port,"127.0.0.1",0,16383);xredisclusterrouteconfig C;crConfig(F,&C);xbytesview A[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("x")};unsigned Accepted=0,Failed=0;
    for(size_t Limit=1;Limit<=4096;Limit+=17){xallocationbudget* B=NULL;xallocationscope Scope={0};xredisclusterroute* R=(void*)(uintptr_t)1;
        testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&Scope),"route constructor budget");xredisadmission S=xredisClusterRouteCreate(T,(xbytesview)XRT_BYTES_LITERAL("127.0.0.1"),1,A,2,&C,&R);testRequire(xrtAllocationBudgetLeave(&Scope),"leave route budget");
        if(S==XREDIS_ADMISSION_ACCEPTED){++Accepted;xredisClusterRouteDestroy(R);}else{++Failed;testRequire(S==XREDIS_ADMISSION_ERROR && R==(void*)(uintptr_t)1,"route OOM mutated output");xrtClearError();}
        rtBudgetZero(B);xrtAllocationBudgetFree(B);}
    testRequire(Accepted && Failed && !xrtAtomic32Load(&F->Calls,XMEMORY_ACQUIRE),"route constructor OOM/no inline provider");
    C.MaxCommandBytes=1;xredisclusterroute* R=(void*)(uintptr_t)1;testRequire(xredisClusterRouteCreate(T,(xbytesview){0},1,A,2,&C,&R)==XREDIS_ADMISSION_ERROR && R==(void*)(uintptr_t)1,"route encoded command limit");xrtClearError();
    xredisClusterTopologyRelease(T);crStop(F);
}
typedef struct rtproducer {crfixture* F;const xredisclustertopology* T;} rtproducer;
static int32 rtProduce(ptr Data)
{
    rtproducer* W=Data;for(unsigned I=0;I<16;++I){xredisclusterrouteconfig C;crConfig(W->F,&C);xbytesview A[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("thread")};xredisclusterroute* R=NULL;
        testRequire(xredisClusterRouteCreate(W->T,(xbytesview)XRT_BYTES_LITERAL("127.0.0.1"),1,A,2,&C,&R)==XREDIS_ADMISSION_ACCEPTED,"concurrent route create");xredisclusterrouteresult Result=crDrive(R);testRequire(Result.Status==XREDIS_CLUSTER_ROUTE_COMPLETE && Result.Query.Status==XREDIS_REQUEST_OK,"concurrent route execution");xredisClusterRouteDestroy(R);}
    return 0;
}
static void rtConcurrent(void)
{
    crfixture* F=crStart();crMode(F,0,CR_MOVED,1);crMode(F,1,CR_ASK,2);xredisclustertopology* T=crTopology(F->Peer.Peers[0].Address.Port,"127.0.0.1",0,16383);rtproducer W={F,T};xthread* Threads[4];
    for(unsigned I=0;I<4;++I){Threads[I]=xrtThreadCreate(rtProduce,&W,0);testRequire(Threads[I]!=NULL,"route producer thread");}
    for(unsigned I=0;I<4;++I){testRequire(xrtThreadWaitFor(Threads[I],15000)==XWAIT_OK && !xrtThreadExitCode(Threads[I]),"route producer join");xrtThreadDestroy(Threads[I]);}
    testRequire(xrtAtomic32Load(&F->Nodes[2].Prefixes,XMEMORY_ACQUIRE)==64,"concurrent automatic ASK routes");xredisClusterTopologyRelease(T);crStop(F);
}
#if defined(XREDIS_CLUSTER_ROUTE_WHITEBOX)
typedef struct rtdeadlineprobe {xredisclusterroute* Route;xredisclient* Client;xatomic32 Done;} rtdeadlineprobe;
static void rtInspectDeadline(xnetworker* Worker,ptr Data)
{
    (void)Worker;rtdeadlineprobe* P=Data;xredisrequest* Q=P->Client->SentHead;
    testRequire(Q && xredisAskingPair(Q) && Q->Deadline==P->Route->Deadline &&
        P->Route->Deadline==xredisClientDeadline(P->Route->Entered,P->Route->Config.Timeout),"redirected native group inherits exact original deadline");
    xrtAtomic32Store(&P->Done,1,XMEMORY_RELEASE);
}
static void rtDeadlineExact(void)
{
    crfixture* F=crStart();crMode(F,0,CR_ASK,1);crMode(F,1,CR_HOLD,0);xrtAtomic32Store(&F->WaitFirst,3,XMEMORY_RELEASE);
    xredisclustertopology* T=crTopology(F->Peer.Peers[0].Address.Port,"127.0.0.1",0,16383);xredisclusterrouteconfig C;crConfig(F,&C);C.Timeout=10001;
    xredisclusterroute* R=rtCreate(F,T,&C);xredisClusterTopologyRelease(T);double Due=xrtTimer()+5;
    while(R->Result.Attempts<2){testRequire(xrtTimer()<Due && xredisClusterRouteStep(R)!=XREDIS_CLUSTER_ROUTE_DONE,"exact deadline redirect admission");xfuture* P=xredisClusterRoutePending(R);if(P){(void)xrtFutureWaitFor(P,1);xrtFutureDestroy(P);}else xrtSleep(1);}
    redisPeerWait(&F->Nodes[1].Queries,1);rtdeadlineprobe Probe={R,F->Peer.Clients[1],{0}};
    testRequire(xrtNetEnginePost(F->Peer.Peers[0].Engine,1,rtInspectDeadline,&Probe),"inspect on native owner worker");redisPeerWait(&Probe.Done,1);
    xredisClusterRouteCancel(R);xfuture* Final=xredisClusterRouteResultFuture(R);redisPeerReleaseBlocked(&F->Peer.Peers[1]);xredisClusterRouteDestroy(R);crStop(F);xrtFutureDestroy(Final);
}
#endif
int main(void)
{
    rtPaths();rtSettledWaitHandle();rtLimitsAndErrors();rtWaitAndPolicy();rtStops();rtMetadata();rtMemory();rtHostGrowth();rtConcurrent();
#if defined(XREDIS_CLUSTER_ROUTE_WHITEBOX)
    rtDeadlineExact();
#endif
    puts("[PASS] Cluster routes: copied input, automatic MOVED/ASK, original deadlines, bounded policy/waits, no uncertain replay, OOM and retained outcomes");return 0;
}
