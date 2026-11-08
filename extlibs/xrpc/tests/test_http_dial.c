#include "test.h"
#include "../examples/common/http_service.h"
#include <xrpc/http_dial.h>
#include <xrt/memory_stats.h>
typedef struct dialLookup {
    xnetengine* Engine;xnetaddr Address;xatomic32 Entered,Permit,Calls;bool Hold,Fail;uint64 Caller;
} dialLookup;
static xnetaddrlist* dialLookupCall(cstr Name,xnetfamily Family,ptr Data)
{
    dialLookup* L=Data;testRequire(!xrtNetEngineCurrent(L->Engine) && xrtThreadCurrentId()!=L->Caller,"DNS ran on caller/network worker");
    testRequire(!strcmp(Name,"logical.test") && (Family==XNET_FAMILY_UNSPEC || Family==XNET_FAMILY_IPV4),"copied DNS name/family");
    xrtAtomic32FetchAdd(&L->Calls,1,XMEMORY_RELAXED);xrtAtomic32Store(&L->Entered,1,XMEMORY_RELEASE);
    while(L->Hold && !xrtAtomic32Load(&L->Permit,XMEMORY_ACQUIRE))xrtSleep(1);
    if(L->Fail){xrtSetErrorInfo(XERR_IO,"xrt.test.dns",17,"lookup failed");return NULL;}
    xnetaddr A=L->Address;A.Port=0;return xrtNetAddrListCreate(&A,1);
}
static xnetresolver* dialResolver(rpcService* S,dialLookup* L,bool Hold,bool Fail)
{
    memset(L,0,sizeof(*L));L->Engine=S->Engine;L->Address=S->Address;L->Hold=Hold;L->Fail=Fail;L->Caller=xrtThreadCurrentId();
    xrtAtomic32Init(&L->Entered,0);xrtAtomic32Init(&L->Permit,0);xrtAtomic32Init(&L->Calls,0);
    xnetresolverconfig K;xrtNetResolverConfigInit(&K);K.Workers=1;K.CacheEntries=0;K.RequestLimit=8;K.QueryLimit=8;K.Lookup=dialLookupCall;K.LookupData=L;
    xnetresolver* R=xrtNetResolverCreate(&K);testRequire(R!=NULL,"DNS resolver create");return R;
}
static void dialWait(xatomic32* V)
{double End=xrtTimer()+10;while(!xrtAtomic32Load(V,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"dial barrier timeout");xrtSleep(1);}}
static xrpchttpclient* dialClient(rpcService* S,xnetresolver* R,const xrpchttpdialconfig* K)
{xrpchttpclient* C=NULL;testRequire(xrpcHttpDial(S->Engine,R,XRT_STR_LITERAL("logical.test"),S->Address.Port,1,XRT_STR_LITERAL("virtual.example"),XRT_STR_LITERAL("/rpc"),NULL,K,&C),"DNS HTTP client");return C;}
typedef struct dialGate {xatomic32 Entered,Permit;xnetpost Post;} dialGate;
static void dialGateWork(xnetworker* Worker,ptr Data)
{(void)Worker;dialGate* G=Data;xrtAtomic32Store(&G->Entered,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&G->Permit,XMEMORY_ACQUIRE))xrtSleep(1);}
static void dialGateStart(rpcService* S,dialGate* G)
{memset(G,0,sizeof(*G));xrtAtomic32Init(&G->Entered,0);xrtAtomic32Init(&G->Permit,0);testRequire(xrtNetPostInit(&G->Post) && xrtNetPost(xrtNetEngineWorker(S->Engine,1),&G->Post,dialGateWork,G),"network test gate");dialWait(&G->Entered);}
static void testDialCopiesAndCalls(void)
{
    rpcService S;rpcServiceStart(&S,NULL);dialLookup L;xnetresolver* R=dialResolver(&S,&L,false,false);dialGate Gate;dialGateStart(&S,&Gate);
    char Name[]="logical.test";xrpchttpclient* C=NULL;testRequire(xrpcHttpDial(S.Engine,R,(xstrview){Name,strlen(Name)},S.Address.Port,1,XRT_STR_LITERAL("virtual.example"),XRT_STR_LITERAL("/rpc"),NULL,NULL,&C),"copied name factory");
    memset(Name,'x',sizeof(Name));xfuture* F[16];for(unsigned N=0;N<16;++N)F[N]=rpcServiceSubmit(C,"echo","[7]",5000);
    xrtAtomic32Store(&Gate.Permit,1,XMEMORY_RELEASE);
    for(unsigned N=0;N<16;++N){testRequire(rpcServiceResult(F[N])->Status==XRPC_CALL_OK,"queued DNS ordinary call");if(N)xrtFutureDestroy(F[N]);}
    xfuture* Note=NULL;testRequire(xrpcHttpNotifyAsync(C,XRT_STR_LITERAL("echo"),(xstrview){0},5000,&Note)==XRPC_ADMISSION_ACCEPTED && rpcServiceResult(Note)->Status==XRPC_CALL_NOTIFY_DONE,"DNS notification");xrtFutureDestroy(Note);
    xrpchttpitem Items[]={{XRPC_MESSAGE_CALL,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]")},{XRPC_MESSAGE_NOTIFICATION,XRT_STR_LITERAL("echo"),{0}}};
    testRequire(xrpcHttpBatchAsync(C,Items,2,5000,&Note)==XRPC_ADMISSION_ACCEPTED && rpcServiceResult(Note)->Status==XRPC_CALL_BATCH_DONE,"DNS batch");xrtFutureDestroy(Note);
    testRequire(xrtAtomic32Load(&L.Calls,XMEMORY_RELAXED)==1,"unexpected DNS retry/reconnect");rpcServiceClientClose(C);testRequire(xrtNetResolverDestroy(R),"resolver join");rpcServiceStop(&S);
    testRequire(rpcServiceResult(F[0])->Status==XRPC_CALL_OK,"DNS result pins client/engine");xrtFutureDestroy(F[0]);
}
static void testDialPrepared(void)
{
    rpcService S;rpcServiceStart(&S,NULL);xnetaddr A[2];testRequire(xrtNetAddrLoopback(&A[0],XNET_FAMILY_IPV4,0),"refused address");
    xnetsocket Bound=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_STREAM,0);testRequire(Bound && xrtNetSocketBind(Bound,&A[0]) && xrtNetSocketLocal(Bound,&A[0]),"reserve non-listening endpoint");A[1]=S.Address;
    xnetaddrlist* List=xrtNetAddrListCreate(A,2);testRequire(List!=NULL,"prepared list");xrpchttpdialconfig K;xrpcHttpDialConfigInit(&K);K.Family=XNET_FAMILY_IPV4;K.FallbackDelay=0;
    xrpchttpclient* C=NULL;testRequire(xrpcHttpDialAddresses(S.Engine,List,1,XRT_STR_LITERAL("localhost"),XRT_STR_LITERAL("/rpc"),NULL,&K,&C),"prepared dial factory");xrtNetAddrListDestroy(List);
    xfuture* F=rpcServiceSubmit(C,"echo","[]",5000);testRequire(rpcServiceResult(F)->Status==XRPC_CALL_OK,"candidate failure then winner");xrtFutureDestroy(F);rpcServiceClientClose(C);
    testRequire(xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED)==1 && xrtNetSocketClose(Bound),"prepared request replay/leaked socket");rpcServiceStop(&S);
}
static void testDialStopsAndBudgets(void)
{
    for(unsigned Mode=0;Mode<3;++Mode){rpcService S;rpcServiceStart(&S,NULL);dialLookup L;xnetresolver* R=dialResolver(&S,&L,true,false);xrpchttpdialconfig K;xrpcHttpDialConfigInit(&K);K.Timeout=Mode==1?100:Mode==0?XRT_WAIT_FOREVER:5000;
        xrpchttpclient* C=dialClient(&S,R,&K);dialWait(&L.Entered);xfuture* F=rpcServiceSubmit(C,"echo",NULL,Mode==0?50:5000);
        if(Mode==2){testRequire(xrtFutureCancel(F),"DNS queued cancel");testRequire(rpcServiceResult(F)->Status==XRPC_CALL_CANCELLED,"DNS cancel result");}
        else testRequire(rpcServiceResult(F)->Status==XRPC_CALL_TIMEOUT,"DNS/connection deadline did not cover queue");
        testRequire(rpcServiceResult(F)->Delivery==XRPC_NOT_SENT,"DNS failure sent request");xrtFutureDestroy(F);
        if(Mode==0){F=rpcServiceSubmit(C,"echo","[]",5000);xrtAtomic32Store(&L.Permit,1,XMEMORY_RELEASE);testRequire(rpcServiceResult(F)->Status==XRPC_CALL_OK,"queued deadline poisoned connection");xrtFutureDestroy(F);}
        xfuture* Closed=xrpcHttpClosed(C);testRequire(Closed && xrpcHttpClose(C),"DNS close request");
        if(Mode!=0){xwaitresult W=xrtFutureWaitFor(Closed,20);testRequire(W==XWAIT_TIMEOUT || W==XWAIT_OK,"DNS close observation");xrtAtomic32Store(&L.Permit,1,XMEMORY_RELEASE);}
        testRequire(xrtFutureWaitFor(Closed,5000)==XWAIT_OK,"DNS physical close barrier");xrtFutureDestroy(Closed);xrpcHttpRelease(C);
        testRequire(xrtNetResolverDestroy(R),"DNS worker/TLS join");testRequire(xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED)==(Mode==0?1u:0u),"stopped DNS call executed/replayed");rpcServiceStop(&S);
    }
    for(unsigned Mode=0;Mode<3;++Mode){rpcService S;rpcServiceStart(&S,NULL);dialLookup L;xnetresolver* R=dialResolver(&S,&L,false,false);dialGate G;dialGateStart(&S,&G);
        xrpchttpdialconfig K;xrpcHttpDialConfigInit(&K);K.Timeout=Mode==1?5000:Mode==0?50:0;xrpchttpclient* C=dialClient(&S,R,&K);xfuture* F=rpcServiceSubmit(C,"echo",NULL,5000);
        if(Mode==1)testRequire(xrpcHttpClose(C),"close before DNS start");else if(!Mode)xrtSleep(100);xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);
        testRequire(rpcServiceResult(F)->Status==(Mode==1?XRPC_CALL_CLOSED:XRPC_CALL_TIMEOUT) && rpcServiceResult(F)->Delivery==XRPC_NOT_SENT,"factory-origin deadline/early close");xrtFutureDestroy(F);rpcServiceClientClose(C);
        testRequire(!xrtAtomic32Load(&L.Calls,XMEMORY_RELAXED) && xrtNetResolverDestroy(R),"expired/closed factory performed DNS");rpcServiceStop(&S);
    }
}
static void dialResolveNoop(xnetresolveop* Op,ptr Data){(void)Op;(void)Data;}
static void testDialResolverAdmission(void)
{
    rpcService S;rpcServiceStart(&S,NULL);dialLookup L;xnetresolver* R=dialResolver(&S,&L,true,false);xnetresolveop* Ops[8];
    for(unsigned N=0;N<8;++N){Ops[N]=xrtNetResolverResolve(R,"logical.test",XNET_FAMILY_IPV4,dialResolveNoop,NULL);testRequire(Ops[N]!=NULL,"fill resolver admission");}
    dialWait(&L.Entered);dialGate G;dialGateStart(&S,&G);xrpchttpclient* C=dialClient(&S,R,NULL);xfuture* F=rpcServiceSubmit(C,"echo",NULL,5000);xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);
    const xrpccallresult* V=rpcServiceResult(F);testRequire(V->Status==XRPC_CALL_LIMIT && V->Delivery==XRPC_NOT_SENT && V->Cause,"resolver overload misclassified");
    xrtFutureDestroy(F);rpcServiceClientClose(C);xrtAtomic32Store(&L.Permit,1,XMEMORY_RELEASE);
    for(unsigned N=0;N<8;++N)xrtNetResolveOpDestroy(Ops[N]);
    testRequire(xrtNetResolverDestroy(R) && !xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED),"resolver overload sent/replayed");rpcServiceStop(&S);
}
static void testDialFailuresAndOOM(void)
{
    rpcService S;rpcServiceStart(&S,NULL);dialLookup L;xnetresolver* R=dialResolver(&S,&L,false,true);dialGate G;dialGateStart(&S,&G);xrpchttpclient* C=dialClient(&S,R,NULL);xfuture* F=rpcServiceSubmit(C,"echo",NULL,5000);xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);
    testRequire(rpcServiceResult(F)->Status==XRPC_CALL_TRANSPORT && rpcServiceResult(F)->Cause,"DNS error cause missing");xrtFutureDestroy(F);rpcServiceClientClose(C);testRequire(xrtNetResolverDestroy(R),"failed DNS join");
    R=dialResolver(&S,&L,false,false);unsigned Passed=0,Failed=0;const size_t Limits[]={1,64,1024,65536,131072,262144,1048576};
    for(size_t N=0;N<sizeof(Limits)/sizeof(Limits[0]);++N){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[N],&B) && xrtAllocationBudgetEnter(B,&Scope),"dial quota enter");
        C=NULL;bool OK=xrpcHttpDial(S.Engine,R,XRT_STR_LITERAL("logical.test"),S.Address.Port,1,XRT_STR_LITERAL("localhost"),XRT_STR_LITERAL("/rpc"),NULL,NULL,&C);testRequire(xrtAllocationBudgetLeave(&Scope),"dial quota leave");
        if(OK){++Passed;F=rpcServiceSubmit(C,"echo","[]",5000);testRequire(rpcServiceResult(F)->Status==XRPC_CALL_OK,"admitted factory budget result");xrtFutureDestroy(F);rpcServiceClientClose(C);}else{++Failed;testRequire(!C,"failed dial published output");}
        xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);double End=xrtTimer()+10;
        do{testRequire(xrtAllocationBudgetInfo(B,&I) && xrtTimer()<End,"dial plan/client allocation leaked");if(I.LiveBytes)xrtSleep(1);}while(I.LiveBytes);xrtAllocationBudgetFree(B);
    }
    testRequire(Passed && Failed,"factory quota thresholds");
    for(unsigned Mode=0;Mode<7;++Mode){xrpchttpdialconfig K;xrpcHttpDialConfigInit(&K);if(Mode==0)K.Timeout=-2;else if(Mode==1)K.MaxAttempts=65;else if(Mode==2)K.MaxHostBytes=65536;
        else if(Mode==3)K.Family=(xnetfamily)999;else if(Mode==4)K.MaxHostBytes=0;else if(Mode==5)K.FallbackDelay=-1;else K.Size=0;
        C=(xrpchttpclient*)(uintptr_t)1;testRequire(!xrpcHttpDial(S.Engine,R,XRT_STR_LITERAL("logical.test"),S.Address.Port,1,XRT_STR_LITERAL("localhost"),XRT_STR_LITERAL("/rpc"),NULL,&K,&C) && C==(xrpchttpclient*)(uintptr_t)1,"invalid dial policy changed output");xrtClearError();}
    C=(xrpchttpclient*)(uintptr_t)1;testRequire(!xrpcHttpDial(S.Engine,R,(xstrview){"a\0b",3},S.Address.Port,1,XRT_STR_LITERAL("localhost"),XRT_STR_LITERAL("/rpc"),NULL,NULL,&C) && C==(xrpchttpclient*)(uintptr_t)1,"embedded NUL hostname");xrtClearError();
    testRequire(xrtNetResolverDestroy(R),"OOM resolver join");rpcServiceStop(&S);
}
static void testDialColdStateRetirement(void)
{
    rpcService S;rpcServiceStart(&S,NULL);dialLookup L;xnetresolver* R=dialResolver(&S,&L,true,false);
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1048576,&B) && xrtAllocationBudgetEnter(B,&Scope),"cold state budget enter");
    xrpchttpclient* C=dialClient(&S,R,NULL);testRequire(xrtAllocationBudgetLeave(&Scope),"cold state budget leave");dialWait(&L.Entered);
    xallocationbudgetinfo Before={0},After={0};Before.Size=After.Size=sizeof(Before);testRequire(xrtAllocationBudgetInfo(B,&Before),"pending source bytes");
    xfuture* F=rpcServiceSubmit(C,"echo","[]",5000);xrtAtomic32Store(&L.Permit,1,XMEMORY_RELEASE);testRequire(rpcServiceResult(F)->Status==XRPC_CALL_OK,"cold-state request");
    xrpchttpinfo I={0};I.Size=sizeof(I);testRequire(xrpcHttpInfo(C,&I) && I.State==XRPC_HTTP_OPEN && xrtAllocationBudgetInfo(B,&After) && After.LiveBytes<Before.LiveBytes,"completed source retained for open connection");
    xrtFutureDestroy(F);rpcServiceClientClose(C);testRequire(xrtNetResolverDestroy(R),"cold-state resolver join");rpcServiceStop(&S);
    xrtClearError();testRequire(xrtAllocationBudgetInfo(B,&After) && !After.LiveBytes,"cold-state/client allocation leaked");xrtAllocationBudgetFree(B);
}
int main(void)
{testDialCopiesAndCalls();testDialPrepared();testDialStopsAndBudgets();testDialResolverAdmission();testDialFailuresAndOOM();testDialColdStateRetirement();puts("[PASS] xrpc HTTP dial: async-DNS/copied-name/prepared-candidates/queued-calls/notification/batch/deadline/cancel/resolver-admission/cold-retirement/physical-close/retained-result/OOM");return 0;}
