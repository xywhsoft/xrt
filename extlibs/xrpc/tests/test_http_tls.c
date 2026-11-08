#include "test.h"
#include "../examples/common/http_tls_peer.h"
#include <xrt/memory_stats.h>
typedef struct tlsGate {xatomic32 Entered,Permit;xnetpost Post;} tlsGate;
static void tlsGateWork(xnetworker* W,ptr Data)
{(void)W;tlsGate* G=Data;xrtAtomic32Store(&G->Entered,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&G->Permit,XMEMORY_ACQUIRE))xrtSleep(1);}
static void tlsGateStart(rpcTlsPeer* P,tlsGate* G)
{memset(G,0,sizeof(*G));xrtAtomic32Init(&G->Entered,0);xrtAtomic32Init(&G->Permit,0);testRequire(xrtNetPostInit(&G->Post) && xrtNetPost(xrtNetEngineWorker(P->Engine,1),&G->Post,tlsGateWork,G),"TLS owner gate");rpcHttpWait(&G->Entered,1);}
static xrpchttpclient* tlsClient(rpcTlsPeer* P,const xrpchttpconfig* H,const xrpchttptlsconfig* K)
{xrpchttpclient* C=NULL;testRequire(xrpcHttpTlsConnect(P->Engine,&P->Address,1,XRT_STR_LITERAL("virtual.example"),XRT_STR_LITERAL("/rpc"),H,K,&C),"TLS numeric factory");return C;}
static void tlsBasic(void)
{
    for(unsigned Mode=0;Mode<3;++Mode){rpcTlsPeer P;rpcTlsPeerStart(&P,false,false);xrpchttptlsconfig K;rpcTlsPeerConfig(&P,&K);K.WriteBytes=3;
        xrpchttpconfig H;xrpcHttpConfigInit(&H);H.ReadBytes=3;H.WorkRequests=2;char Identity[]="rpc.fixture",Protocol[]="http/1.1";xstrview ALPN={Protocol,8};
        char Credential[]="Bearer public-fixture";xhttpfield Auth={XRT_STR_LITERAL("Authorization"),{Credential,sizeof(Credential)-1}};
        static const xhttpfield Expected={XRT_STR_LITERAL("Authorization"),XRT_STR_LITERAL("Bearer public-fixture")};P.ExpectedHeaders=&Expected;xrtAtomic32Store(&P.ExpectedHeaderCount,1,XMEMORY_RELEASE);H.RequestHeaders=&Auth;H.RequestHeaderCount=1;
        K.Session.VerifyName=(xstrview){Identity,11};K.Session.Protocols=&ALPN;tlsGate G;tlsGateStart(&P,&G);xrpchttpclient* C=NULL;xnetresolver* R=NULL;xnetsocket Bound=0;
        if(Mode==0){xnetresolverconfig RC;xrtNetResolverConfigInit(&RC);R=xrtNetResolverCreate(&RC);K.Dial.Family=XNET_FAMILY_IPV4;char Name[]="localhost";
            testRequire(R && xrpcHttpTlsDial(P.Engine,R,(xstrview){Name,9},P.Address.Port,1,XRT_STR_LITERAL("virtual.example"),XRT_STR_LITERAL("/rpc"),&H,&K,&C),"TLS DNS factory");memset(Name,'x',sizeof(Name));}
        else if(Mode==1)C=tlsClient(&P,&H,&K);
        else{xnetaddr A[2];testRequire(xrtNetAddrLoopback(&A[0],XNET_FAMILY_IPV4,0),"TLS refused endpoint");Bound=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_STREAM,0);
            testRequire(Bound && xrtNetSocketBind(Bound,&A[0]) && xrtNetSocketLocal(Bound,&A[0]),"reserve TLS refused endpoint");A[1]=P.Address;xnetaddrlist* List=xrtNetAddrListCreate(A,2);
            K.Dial.FallbackDelay=0;testRequire(List && xrpcHttpTlsDialAddresses(P.Engine,List,1,XRT_STR_LITERAL("virtual.example"),XRT_STR_LITERAL("/rpc"),&H,&K,&C),"TLS prepared fallback factory");xrtNetAddrListDestroy(List);}
        memset(Identity,'x',sizeof(Identity));memset(Protocol,'x',sizeof(Protocol));memset(Credential,'x',sizeof(Credential));memset(&Auth,0,sizeof(Auth));memset(&K,0,sizeof(K));xrtTlsVerifierRelease(P.Verifier);P.Verifier=NULL;
        xfuture* F[16];for(unsigned I=0;I<16;++I)F[I]=rpcHttpSubmit(C,"echo","[7]",5000);xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);
        for(unsigned I=0;I<16;++I){const xrpccallresult* V=rpcHttpResult(F[I]);xrpcresponseinfo Info={0};Info.Size=sizeof(Info);
            testRequire(V->Status==XRPC_CALL_OK && V->Delivery==XRPC_REPLIED && xrpcResponsesInfo(V->Responses,&Info) && Info.Messages[0].Result.Raw.Size==3 && !memcmp(Info.Messages[0].Result.Raw.Data,"[7]",3),"TLS copied/partial/queued correlation");if(I)xrtFutureDestroy(F[I]);}
        xfuture* Note=NULL;testRequire(xrpcHttpNotifyAsync(C,XRT_STR_LITERAL("echo"),(xstrview){0},5000,&Note)==XRPC_ADMISSION_ACCEPTED && rpcHttpResult(Note)->Status==XRPC_CALL_NOTIFY_DONE,"TLS notification");xrtFutureDestroy(Note);
        xrpchttpitem Items[]={{XRPC_MESSAGE_CALL,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]")},{XRPC_MESSAGE_NOTIFICATION,XRT_STR_LITERAL("echo"),{0}},{XRPC_MESSAGE_CALL,XRT_STR_LITERAL("error"),{0}}};
        testRequire(xrpcHttpBatchAsync(C,Items,3,5000,&Note)==XRPC_ADMISSION_ACCEPTED && rpcHttpResult(Note)->Status==XRPC_CALL_BATCH_DONE,"TLS mixed batch");xrtFutureDestroy(Note);
        Note=rpcHttpSubmit(C,"error",NULL,5000);testRequire(rpcHttpResult(Note)->Status==XRPC_CALL_REMOTE_ERROR,"TLS remote error");xrtFutureDestroy(Note);
        testRequire(xrtAtomic32Load(&P.HeadersChecked,XMEMORY_ACQUIRE)==19,"all TLS factory header documents checked");rpcTlsClientClose(C);if(R)testRequire(xrtNetResolverDestroy(R),"TLS resolver join");if(Bound)testRequire(xrtNetSocketClose(Bound),"release reserved endpoint");
        testRequire(xrtAtomic32Load(&P.Verified,XMEMORY_ACQUIRE)==1 && xrtAtomic32Load(&P.Commands,XMEMORY_ACQUIRE)==19,"TLS verification/no replay");rpcTlsPeerStop(&P);
        testRequire(rpcHttpResult(F[0])->Status==XRPC_CALL_OK,"TLS result retained client/engine");xrtFutureDestroy(F[0]);
    }
}
static void tlsFailures(void)
{
    for(unsigned Mode=0;Mode<5;++Mode){rpcTlsPeer P;rpcTlsPeerStart(&P,Mode==1 || Mode==2 || Mode==3,Mode==0);xrpchttptlsconfig K;rpcTlsPeerConfig(&P,&K);
        if(Mode==1){K.Dial.Timeout=50;K.Stream.HandshakeTimeout=0;}else if(Mode==2){K.Dial.Timeout=5000;K.Stream.HandshakeTimeout=50;}else if(Mode==4)K.Dial.Timeout=0;
        tlsGate G;tlsGateStart(&P,&G);xrpchttpclient* C=tlsClient(&P,NULL,&K);xfuture* F=rpcHttpSubmit(C,"echo","[]",5000);xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);
        if(Mode==3){double End=xrtTimer()+5;while(!xrtAtomicPtrLoad(&P.Raw,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"TLS handshake admission");xrtSleep(1);}testRequire(xrpcHttpClose(C),"close TLS handshake");}
        const xrpccallresult* V=rpcHttpResult(F);testRequire(V->Status==(Mode==0?XRPC_CALL_TRANSPORT:Mode==3?XRPC_CALL_CLOSED:XRPC_CALL_TIMEOUT) && V->Delivery==XRPC_NOT_SENT,"TLS certificate/deadline/close outcome");
        if(Mode==0 || Mode==2)testRequire(V->Cause!=NULL,"TLS failure lost cause");
        testRequire(!xrtAtomic32Load(&P.Commands,XMEMORY_ACQUIRE),"TLS failure leaked RPC/plaintext");
        rpcTlsClientClose(C);rpcTlsPeerStop(&P);testRequire(rpcHttpResult(F)->Delivery==XRPC_NOT_SENT,"TLS failed result lifetime");xrtFutureDestroy(F);
    }
}
static void tlsPartialStops(void)
{
    for(unsigned Mode=0;Mode<3;++Mode){rpcTlsPeer P;rpcTlsPeerStart(&P,false,false);xrpchttptlsconfig K;rpcTlsPeerConfig(&P,&K);K.WriteBytes=1;
        xrpchttpclient* C=tlsClient(&P,NULL,&K);char Params[32007];memcpy(Params,"[\"",2);memset(Params+2,'x',32000);memcpy(Params+32002,"\"]",3);
        xfuture* F=rpcHttpSubmit(C,"echo",Params,Mode==1?300:5000);rpcHttpWait(&P.PlainBytes,1);tlsGate G;tlsGateStart(&P,&G);
        testRequire(!xrtAtomic32Load(&P.Commands,XMEMORY_ACQUIRE),"TLS partial fixture request already complete");xfuture* Next=rpcHttpSubmit(C,"echo","[]",5000);
        if(Mode==0)testRequire(xrtFutureCancel(F),"TLS partial cancel");else if(Mode==1)xrtSleep(320);else testRequire(xrpcHttpClose(C),"TLS partial close");
        xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);const xrpccallresult* V=rpcHttpResult(F);
        testRequire(V->Status==(Mode==0?XRPC_CALL_CANCELLED:Mode==1?XRPC_CALL_TIMEOUT:XRPC_CALL_CLOSED) && V->Delivery==XRPC_MAY_HAVE_EXECUTED,"TLS accepted prefix certainty");
        testRequire(rpcHttpResult(Next)->Status==XRPC_CALL_CLOSED && rpcHttpResult(Next)->Delivery==XRPC_NOT_SENT,"TLS partial stop replayed successor");
        rpcTlsClientClose(C);rpcTlsPeerStop(&P);xrtFutureDestroy(F);xrtFutureDestroy(Next);
    }
}
static void tlsFramingAndEarlyReply(void)
{
    for(unsigned Mode=0;Mode<2;++Mode){rpcTlsPeer P;rpcTlsPeerStart(&P,false,false);xrtAtomic32Store(&P.Early,Mode!=0,XMEMORY_RELEASE);xrpchttptlsconfig K;rpcTlsPeerConfig(&P,&K);K.WriteBytes=1;xrpchttpclient* C=tlsClient(&P,NULL,&K);
        char Params[32007];memcpy(Params,"[\"",2);memset(Params+2,'x',32000);memcpy(Params+32002,"\"]",3);
        xfuture* F=rpcHttpSubmit(C,Mode?"echo":"trunc",Mode?Params:"[]",10000);const xrpccallresult* V=rpcHttpResult(F);
        testRequire(Mode?(V->Status==XRPC_CALL_OK && V->Delivery==XRPC_REPLIED):V->Status==XRPC_CALL_TRANSPORT,"TLS early reply/unauthenticated truncation");
        xfuture* Closed=xrpcHttpClosed(C);testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"TLS early/truncating connection was reused");xrtFutureDestroy(Closed);rpcTlsClientClose(C);rpcTlsPeerStop(&P);xrtFutureDestroy(F);
    }
}
static void tlsPolicyAndOOM(void)
{
    rpcTlsPeer P;rpcTlsPeerStart(&P,false,false);xrpchttptlsconfig K;
    for(unsigned Mode=0;Mode<8;++Mode){rpcTlsPeerConfig(&P,&K);xstrview H2={"h2",2};if(Mode==0)K.Size=0;else if(Mode==1)K.WriteBytes=0;else if(Mode==2)K.MaxMetadataBytes=1;else if(Mode==3)K.Session.VerifyName=(xstrview){0};
        else if(Mode==4)K.Session.Verifier=NULL;else if(Mode==5)K.Session.Protocols=&H2;else if(Mode==6)K.Session.VerifyName=(xstrview){"a\0b",3};else K.Stream.AsyncBytesLimit=0;
        xrpchttpclient* C=(xrpchttpclient*)(uintptr_t)1;testRequire(!xrpcHttpTlsConnect(P.Engine,&P.Address,1,XRT_STR_LITERAL("localhost"),XRT_STR_LITERAL("/rpc"),NULL,&K,&C) && C==(xrpchttpclient*)(uintptr_t)1,"TLS invalid policy output/verification bypass");xrtClearError();}
    rpcTlsPeerStop(&P);
    const size_t Limits[]={1,64,1024,8192,65536,131072,262144,1048576,2097152};unsigned Passed=0,Failed=0;
    for(size_t N=0;N<sizeof(Limits)/sizeof(*Limits);++N){rpcTlsPeerStart(&P,false,false);rpcTlsPeerConfig(&P,&K);xallocationbudget* B=NULL;xallocationscope Scope={0};
        testRequire(xrtAllocationBudgetCreate(Limits[N],&B) && xrtAllocationBudgetEnter(B,&Scope),"TLS constructor budget enter");xrpchttpclient* C=NULL;
        bool OK=xrpcHttpTlsConnect(P.Engine,&P.Address,1,XRT_STR_LITERAL("localhost"),XRT_STR_LITERAL("/rpc"),NULL,&K,&C);testRequire(xrtAllocationBudgetLeave(&Scope),"TLS constructor budget leave");
        if(OK){++Passed;rpcTlsClientClose(C);}else{++Failed;testRequire(!C,"TLS constructor failure published owner");}rpcTlsPeerStop(&P);xrtClearError();
        xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes,"TLS factory budget retained after full join");xrtAllocationBudgetFree(B);
    }
    testRequire(Passed && Failed,"TLS budget sweep missed boundary");
}
typedef struct tlsLookup {xnetengine* Engine;xnetaddr Address;xatomic32 Entered,Permit,Calls;uint64 Caller;} tlsLookup;
static xnetaddrlist* tlsLookupCall(cstr Name,xnetfamily Family,ptr Data)
{
    tlsLookup* L=Data;testRequire(!strcmp(Name,"copy.test") && Family==XNET_FAMILY_IPV4 && !xrtNetEngineCurrent(L->Engine) && xrtThreadCurrentId()!=L->Caller,"TLS copied DNS name/thread");
    xrtAtomic32FetchAdd(&L->Calls,1,XMEMORY_RELAXED);xrtAtomic32Store(&L->Entered,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&L->Permit,XMEMORY_ACQUIRE))xrtSleep(1);
    xnetaddr A=L->Address;A.Port=0;return xrtNetAddrListCreate(&A,1);
}
static void tlsDNSStops(void)
{
    for(unsigned Mode=0;Mode<5;++Mode){rpcTlsPeer P;rpcTlsPeerStart(&P,false,false);tlsLookup L={0};L.Engine=P.Engine;L.Address=P.Address;L.Caller=xrtThreadCurrentId();
        xrtAtomic32Init(&L.Entered,0);xrtAtomic32Init(&L.Permit,0);xrtAtomic32Init(&L.Calls,0);xnetresolverconfig RC;xrtNetResolverConfigInit(&RC);RC.Lookup=tlsLookupCall;RC.LookupData=&L;RC.CacheEntries=0;
        xnetresolver* Resolver=xrtNetResolverCreate(&RC);testRequire(Resolver!=NULL,"TLS held resolver");xrpchttptlsconfig K;rpcTlsPeerConfig(&P,&K);K.Dial.Family=XNET_FAMILY_IPV4;K.Dial.Timeout=Mode==1?50:Mode==3?30:XRT_WAIT_FOREVER;
        tlsGate G;tlsGateStart(&P,&G);char Name[]="copy.test";xrpchttpclient* C=NULL;
        testRequire(xrpcHttpTlsDial(P.Engine,Resolver,(xstrview){Name,9},P.Address.Port,1,XRT_STR_LITERAL("virtual.example"),XRT_STR_LITERAL("/rpc"),NULL,&K,&C),"TLS held DNS factory");memset(Name,'x',sizeof(Name));
        xfuture* F=rpcHttpSubmit(C,"echo","[]",Mode==0?50:5000);xfuture* Closed=xrpcHttpClosed(C);
        if(Mode==3)xrtSleep(50);
        if(Mode==4)testRequire(xrpcHttpClose(C),"TLS close before start");
        xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);
        if(Mode<3)rpcHttpWait(&L.Entered,1);
        if(Mode==2){testRequire(xrpcHttpClose(C),"TLS DNS close");xrpcHttpRelease(C);C=NULL;}
        const xrpccallresult* V=rpcHttpResult(F);testRequire(V->Status==(Mode==2 || Mode==4?XRPC_CALL_CLOSED:XRPC_CALL_TIMEOUT) && V->Delivery==XRPC_NOT_SENT,"TLS DNS stop certainty/deadline");
        if(Mode==0){xfuture* Next=rpcHttpSubmit(C,"echo","[]",5000);xrtAtomic32Store(&L.Permit,1,XMEMORY_RELEASE);testRequire(rpcHttpResult(Next)->Status==XRPC_CALL_OK,"TLS unsent timeout poisoned setup");xrtFutureDestroy(Next);}
        xrtAtomic32Store(&L.Permit,1,XMEMORY_RELEASE);if(C)rpcTlsClientClose(C);testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK && xrtNetResolverDestroy(Resolver),"TLS DNS callback/resolver retirement");
        testRequire(xrtAtomic32Load(&L.Calls,XMEMORY_ACQUIRE)==(Mode>=3?0u:1u),"TLS pre-admission stop ran DNS");xrtFutureDestroy(Closed);rpcTlsPeerStop(&P);xrtFutureDestroy(F);
    }
}
typedef struct tlsReentry {xrpchttpclient* Client;xatomic32 Calls;} tlsReentry;
static void tlsReentryClose(ptr Data)
{tlsReentry* R=Data;testRequire(xrpcHttpClose(R->Client),"TLS reentrant close");xrpcHttpRelease(R->Client);R->Client=NULL;xrtAtomic32FetchAdd(&R->Calls,1,XMEMORY_RELEASE);}
static void tlsReentryTest(void)
{
    rpcTlsPeer P;rpcTlsPeerStart(&P,false,false);xrpchttptlsconfig K;rpcTlsPeerConfig(&P,&K);tlsReentry R={0};xrtAtomic32Init(&R.Calls,0);R.Client=tlsClient(&P,NULL,&K);xfuture* Closed=xrpcHttpClosed(R.Client);
    xfuture* F=rpcHttpSubmit(R.Client,"hold",NULL,5000);rpcHttpWait(&P.Commands,1);xfuturewatch Watch;
    testRequire(xrtFutureWatchInit(&Watch,tlsReentryClose,NULL,&R) && xrtFutureWatchAdd(F,&Watch)==XFUTURE_WATCH_PENDING && xrtFutureCancel(F),"TLS reentrant watch");
    testRequire(rpcHttpResult(F)->Status==XRPC_CALL_CANCELLED && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"TLS reentrant close result");xrtFutureWatchRemove(F,&Watch);
    testRequire(xrtAtomic32Load(&R.Calls,XMEMORY_ACQUIRE)==1 && !R.Client,"TLS reentrant creator release");rpcTlsPeerStop(&P);xrtFutureDestroy(Closed);xrtFutureDestroy(F);
}
static void tlsVerificationDeadline(void)
{
    rpcTlsPeer P;rpcTlsPeerStart(&P,false,false);xrtAtomic32Store(&P.VerifyGate,1,XMEMORY_RELEASE);xrpchttptlsconfig K;rpcTlsPeerConfig(&P,&K);K.Dial.Timeout=1000;K.Stream.HandshakeTimeout=0;
    xrpchttpclient* C=tlsClient(&P,NULL,&K);xfuture* F=rpcHttpSubmit(C,"echo","[]",5000);rpcHttpWait(&P.Verified,1);
    xrtSleep(1100);xrtAtomic32Store(&P.VerifyGate,2,XMEMORY_RELEASE);const xrpccallresult* V=rpcHttpResult(F);
    testRequire(V->Status==XRPC_CALL_TIMEOUT && V->Delivery==XRPC_NOT_SENT && !xrtAtomic32Load(&P.Commands,XMEMORY_ACQUIRE),"TLS readiness bypassed expired factory deadline");
    rpcTlsClientClose(C);rpcTlsPeerStop(&P);xrtFutureDestroy(F);
}
int main(int argc,char** argv)
{
    if(argc==2 && !strcmp(argv[1],"--verification-deadline")){tlsVerificationDeadline();return 0;}
    testRequire(argc==1,"TLS test arguments");tlsBasic();tlsFailures();tlsPartialStops();tlsFramingAndEarlyReply();tlsPolicyAndOOM();tlsDNSStops();tlsReentryTest();tlsVerificationDeadline();
    puts("[PASS] xrpc HTTP TLS: verified/DNS/numeric/prepared/copied-policy/partial/notification/batch/deadline/cancel/close/truncation/early-reply/retained/OOM/held-DNS/reentry/verification-deadline");return 0;
}
