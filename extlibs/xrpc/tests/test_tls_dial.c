#include "test.h"
#include <xrpc/tls_dial.h>
#include <xrt/memory_stats.h>
#include "../examples/common/tls_service.h"
typedef struct tlsDialLookup {rpcTlsService* Service;xatomic32 Entered,Permit,Calls;bool Hold,Fail;uint64 Caller;} tlsDialLookup;
static void tlsDialWait(xatomic32* V)
{double End=xrtTimer()+10;while(!xrtAtomic32Load(V,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"TLS dial barrier timeout");xrtSleep(1);}}
static xnetaddrlist* tlsDialLookupCall(cstr Name,xnetfamily Family,ptr Data)
{
    tlsDialLookup* L=Data;testRequire(!xrtNetEngineCurrent(L->Service->Engine) && xrtThreadCurrentId()!=L->Caller,"TLS DNS ran on caller/network worker");
    testRequire(!strcmp(Name,"route.test") && (Family==XNET_FAMILY_UNSPEC || Family==XNET_FAMILY_IPV4),"copied TLS routing name/family");
    xrtAtomic32FetchAdd(&L->Calls,1,XMEMORY_RELAXED);xrtAtomic32Store(&L->Entered,1,XMEMORY_RELEASE);while(L->Hold && !xrtAtomic32Load(&L->Permit,XMEMORY_ACQUIRE))xrtSleep(1);
    if(L->Fail){xrtSetErrorInfo(XERR_IO,"tls.dns.fixture",17,"DNS lookup failed");return NULL;}xnetaddr A=L->Service->Address;A.Port=0;return xrtNetAddrListCreate(&A,1);
}
static xnetresolver* tlsDialResolver(rpcTlsService* S,tlsDialLookup* L,bool Hold,bool Fail)
{
    memset(L,0,sizeof(*L));L->Service=S;L->Hold=Hold;L->Fail=Fail;L->Caller=xrtThreadCurrentId();xrtAtomic32Init(&L->Entered,0);xrtAtomic32Init(&L->Permit,0);xrtAtomic32Init(&L->Calls,0);
    xnetresolverconfig C;xrtNetResolverConfigInit(&C);C.Workers=1;C.CacheEntries=0;C.RequestLimit=8;C.QueryLimit=8;C.Lookup=tlsDialLookupCall;C.LookupData=L;xnetresolver* R=xrtNetResolverCreate(&C);testRequire(R!=NULL,"host TLS DNS resolver");return R;
}
typedef struct tlsDialGate {xnetpost Post;xatomic32 Entered,Permit;} tlsDialGate;
static void tlsDialGateRun(xnetworker* W,ptr Data)
{(void)W;tlsDialGate* G=Data;xrtAtomic32Store(&G->Entered,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&G->Permit,XMEMORY_ACQUIRE))xrtSleep(1);}
static void tlsDialGateStart(rpcTlsService* S,tlsDialGate* G)
{xrtAtomic32Init(&G->Entered,0);xrtAtomic32Init(&G->Permit,0);testRequire(xrtNetPostInit(&G->Post) && xrtNetPost(xrtNetEngineWorker(S->Engine,1),&G->Post,tlsDialGateRun,G),"TLS owner gate");tlsDialWait(&G->Entered);}
static xfuture* tlsDialCall(xrpctls* P,int64 Timeout)
{xfuture* F=NULL;testRequire(xrpcTlsCall(P,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[7]"),Timeout,&F)==XRPC_DUPLEX_ACCEPTED,"TLS dial queued call");return F;}
static void tlsDialResult(xfuture* F,xrpccallstatus Status,xrpcdelivery Delivery)
{const xrpccallresult* R=rpcTlsServiceResult(F);if(R->Status!=Status || R->Delivery!=Delivery)fprintf(stderr,"result %d/%d wanted %d/%d\n",R->Status,R->Delivery,Status,Delivery);testRequire(R->Status==Status && R->Delivery==Delivery,"TLS dial typed result");}
static xrpctls* tlsDialPeer(rpcTlsService* S,xnetresolver* R,const xrpctlsconfig* Config)
{xtlsclientconfig C;rpcTlsServiceClient(S,&C);xrpctls* P=NULL;testRequire(xrpcTlsDial(S->Engine,R,XRT_STR_LITERAL("route.test"),S->Address.Port,1,&C,Config?Config:&S->Config,NULL,&P),"TLS DNS factory");return P;}
static void testTlsDialSnapshot(void)
{
    for(unsigned Format=1;Format<=2;++Format){rpcTlsService S;rpcTlsServiceStart(&S,Format,true,false,false);tlsDialLookup L;xnetresolver* R=tlsDialResolver(&S,&L,false,false);tlsDialGate G;tlsDialGateStart(&S,&G);
        xtlsclientconfig C;rpcTlsServiceClient(&S,&C);char DNS[]="route.test",Identity[]="rpc.fixture",Alpn[]="rpc-test";xstrview Protocol={Alpn,8};C.ServerName=(xstrview){Identity,11};C.Protocols=&Protocol;
        xrpctls* P=NULL;testRequire(xrpcTlsDial(S.Engine,R,(xstrview){DNS,10},S.Address.Port,1,&C,&S.Config,NULL,&P),"TLS copied routing/identity policy");
        memset(DNS,'x',10);memset(Identity,'x',11);memset(Alpn,'x',8);xrtTlsVerifierRelease(S.Verifier);S.Verifier=NULL; /* Session owns the real remaining verifier reference. */
        xfuture* F[12];for(unsigned I=0;I<8;++I)F[I]=tlsDialCall(P,5000);xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);
        for(unsigned I=0;I<8;++I)tlsDialResult(F[I],XRPC_CALL_OK,XRPC_REPLIED);
        testRequire(xrtAtomic32Load(&S.Ready,XMEMORY_ACQUIRE) && xrtAtomic32Load(&S.VerifyCalls,XMEMORY_RELAXED)==1,"DNS/TLS winner verification count");
        testRequire(xrpcTlsCall(S.Accepted,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[8]"),5000,F+8)==XRPC_DUPLEX_ACCEPTED,"TLS DNS reverse call");tlsDialResult(F[8],XRPC_CALL_OK,XRPC_REPLIED);
        testRequire(xrpcTlsNotify(P,XRT_STR_LITERAL("note"),(xstrview){0},5000,F+9)==XRPC_DUPLEX_ACCEPTED,"TLS DNS notification");tlsDialResult(F[9],XRPC_CALL_NOTIFY_DONE,XRPC_MAY_HAVE_EXECUTED);
        xrpcduplexitem Items[]={{false,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]")},{true,XRT_STR_LITERAL("note"),{0}}};testRequire(xrpcTlsBatch(P,Items,2,5000,F+10)==XRPC_DUPLEX_ACCEPTED,"TLS DNS batch");tlsDialResult(F[10],XRPC_CALL_BATCH_DONE,XRPC_REPLIED);
        xrpctlsinfo I={0};I.Size=sizeof(I);testRequire(xrpcTlsInfo(P,&I) && I.Version && I.Cipher && I.ProtocolBytes==8 && !memcmp(I.Protocol,"rpc-test",8) && !I.PostFailures,"copied TLS ALPN/snapshot");
        rpcTlsServiceCycleClose(&S,P);testRequire(xrtAtomic32Load(&L.Calls,XMEMORY_RELAXED)==1 && xrtNetResolverDestroy(R),"TLS DNS replay/host join");rpcTlsServiceStop(&S);tlsDialResult(F[0],XRPC_CALL_OK,XRPC_REPLIED);for(unsigned N=0;N<11;++N)xrtFutureDestroy(F[N]);
    }
}
static void testTlsDialPrepared(void)
{
    rpcTlsService S;rpcTlsServiceStart(&S,2,false,false,false);xnetaddr A[2];xrtNetAddrLoopback(A,XNET_FAMILY_IPV4,0);xnetsocket Bound=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_STREAM,0);
    testRequire(Bound && xrtNetSocketBind(Bound,A) && xrtNetSocketLocal(Bound,A),"TLS refused endpoint reserve");A[1]=S.Address;xnetaddrlist* List=xrtNetAddrListCreate(A,2);
    xtlsclientconfig C;rpcTlsServiceClient(&S,&C);xrpctlsdialconfig D;xrpcTlsDialConfigInit(&D);D.Family=XNET_FAMILY_IPV4;D.FallbackDelay=0;xrpctls* P=NULL;
    testRequire(List && xrpcTlsDialAddresses(S.Engine,List,1,&C,&S.Config,&D,&P),"prepared TLS factory");xrtNetAddrListDestroy(List);xfuture* F=tlsDialCall(P,5000);tlsDialResult(F,XRPC_CALL_OK,XRPC_REPLIED);
    rpcTlsServiceCycleClose(&S,P);testRequire(xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED)==1 && xrtAtomic32Load(&S.VerifyCalls,XMEMORY_RELAXED)==1,"TLS candidates replayed/verified losers");xrtFutureDestroy(F);testRequire(xrtNetSocketClose(Bound),"TLS reserve close");rpcTlsServiceStop(&S);
}
static void testTlsDialStops(void)
{
    for(unsigned Mode=0;Mode<4;++Mode){rpcTlsService S;rpcTlsServiceStart(&S,1,false,false,false);tlsDialLookup L;xnetresolver* R=tlsDialResolver(&S,&L,true,false);xrpctlsconfig C=S.Config;C.Stream.ConnectTimeout=Mode==2?50:XRT_WAIT_FOREVER;
        xrpctls* P=tlsDialPeer(&S,R,&C);tlsDialWait(&L.Entered);xfuture* F=tlsDialCall(P,Mode==0?40:5000);
        if(Mode==0)tlsDialResult(F,XRPC_CALL_TIMEOUT,XRPC_NOT_SENT);else if(Mode==1){testRequire(xrtFutureCancel(F),"TLS DNS queued cancel");tlsDialResult(F,XRPC_CALL_CANCELLED,XRPC_NOT_SENT);}
        else if(Mode==2)tlsDialResult(F,XRPC_CALL_TRANSPORT,XRPC_NOT_SENT);else{testRequire(xrpcTlsClose(P),"close held TLS DNS");tlsDialResult(F,XRPC_CALL_CLOSED,XRPC_NOT_SENT);}
        xrtFutureDestroy(F);
        if(Mode<2){F=tlsDialCall(P,5000);xrtAtomic32Store(&L.Permit,1,XMEMORY_RELEASE);tlsDialResult(F,XRPC_CALL_OK,XRPC_REPLIED);xrtFutureDestroy(F);}
        else {testRequire(xrpcTlsClose(P) && xrpcTlsWaitFor(P,5000)==XWAIT_OK && xrpcTlsDestroy(P),"cancelled TLS source callback retirement");P=NULL;testRequire(!xrtAtomic32Load(&S.VerifyCalls,XMEMORY_RELAXED) && xrtNetResolverTryDestroy(R)==XNET_RETIRE_BUSY,"TLS peer confused with host lookup join");xrtAtomic32Store(&L.Permit,1,XMEMORY_RELEASE);}
        rpcTlsServiceCycleClose(&S,P);testRequire(xrtNetResolverDestroy(R),"TLS host resolver join");rpcTlsServiceStop(&S);
    }
    for(unsigned Mode=0;Mode<3;++Mode){rpcTlsService S;rpcTlsServiceStart(&S,1,false,false,false);tlsDialLookup L;xnetresolver* R=tlsDialResolver(&S,&L,false,false);tlsDialGate G;tlsDialGateStart(&S,&G);xrpctlsconfig C=S.Config;C.Stream.ConnectTimeout=Mode==0?0:Mode==1?40:5000;
        xrpctls* P=tlsDialPeer(&S,R,&C);xfuture* F=tlsDialCall(P,5000);if(Mode==2)testRequire(xrpcTlsClose(P),"TLS pre-start close");else if(Mode==1)xrtSleep(60);xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);
        tlsDialResult(F,Mode==2?XRPC_CALL_CLOSED:XRPC_CALL_TRANSPORT,XRPC_NOT_SENT);xrtFutureDestroy(F);rpcTlsServiceCycleClose(&S,P);testRequire(!xrtAtomic32Load(&L.Calls,XMEMORY_RELAXED) && xrtNetResolverDestroy(R),"expired TLS factory started DNS");rpcTlsServiceStop(&S);
    }
}
static void testTlsDialVerification(void)
{
    for(unsigned Mode=0;Mode<3;++Mode){rpcTlsService S;rpcTlsServiceStart(&S,1,false,Mode==0,Mode!=0);tlsDialLookup L;xnetresolver* R=tlsDialResolver(&S,&L,Mode==1,false);xrpctlsconfig C=S.Config;C.Stream.ConnectTimeout=Mode==1?250:5000;C.TLS.HandshakeTimeout=2000;
        xrpctls* P=tlsDialPeer(&S,R,&C);xfuture* F=tlsDialCall(P,5000);
        if(Mode==1){tlsDialWait(&L.Entered);xrtSleep(80);xrtAtomic32Store(&L.Permit,1,XMEMORY_RELEASE);}
        if(Mode){tlsDialWait(&S.VerifyEntered);if(Mode==1)xrtSleep(220);else testRequire(xrpcTlsClose(P) && xrpcTlsWaitFor(P,20)==XWAIT_TIMEOUT,"TLS physical callback not held");xrtAtomic32Store(&S.VerifyPermit,1,XMEMORY_RELEASE);}
        tlsDialResult(F,Mode==2?XRPC_CALL_CLOSED:XRPC_CALL_TRANSPORT,XRPC_NOT_SENT);testRequire(xrpcTlsWaitFor(P,5000)==XWAIT_OK,"TLS verify fail retirement");xrpctlsinfo I={0};I.Size=sizeof(I);
        testRequire(xrpcTlsInfo(P,&I) && !I.WriteBytes && !I.WriteFrames && !xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED),"TLS RPC escaped verification/deadline/close");
        testRequire(I.Reason==(Mode==0?XRPC_TLS_HANDSHAKE:Mode==1?XRPC_TLS_TIMEOUT:XRPC_TLS_LOCAL),"TLS verify typed channel reason");rpcTlsServiceCycleClose(&S,P);xrtFutureDestroy(F);testRequire(xrtNetResolverDestroy(R),"verify Resolver join");rpcTlsServiceStop(&S);
    }
}
static void tlsDialNoop(xnetresolveop* Op,ptr Data){(void)Op;(void)Data;}
static void testTlsDialAdmissionAndOOM(void)
{
    for(unsigned Mode=0;Mode<2;++Mode){rpcTlsService S;rpcTlsServiceStart(&S,1,false,false,false);tlsDialLookup L;xnetresolver* R=tlsDialResolver(&S,&L,Mode==0,Mode==1);xnetresolveop* Ops[8]={0};if(!Mode){for(unsigned N=0;N<8;++N){Ops[N]=xrtNetResolverResolve(R,"route.test",XNET_FAMILY_IPV4,tlsDialNoop,NULL);testRequire(Ops[N]!=NULL,"TLS DNS admission fill");}tlsDialWait(&L.Entered);}
        xrpctls* P=tlsDialPeer(&S,R,NULL);xfuture* F=tlsDialCall(P,5000);tlsDialResult(F,Mode?XRPC_CALL_TRANSPORT:XRPC_CALL_LIMIT,XRPC_NOT_SENT);testRequire(xrpcTlsWaitFor(P,5000)==XWAIT_OK,"failed TLS source retirement");xrpctlsinfo I={0};I.Size=sizeof(I);testRequire(xrpcTlsInfo(P,&I) && I.NativeErrorKind && I.NativeErrorDomain[0] && !I.WriteFrames,"TLS native DNS diagnostics");
        rpcTlsServiceCycleClose(&S,P);xrtFutureDestroy(F);xrtAtomic32Store(&L.Permit,1,XMEMORY_RELEASE);for(unsigned N=0;N<8;++N)xrtNetResolveOpDestroy(Ops[N]);testRequire(xrtNetResolverDestroy(R),"TLS failed source host join");rpcTlsServiceStop(&S);
    }
    rpcTlsService S;rpcTlsServiceStart(&S,1,false,false,false);tlsDialLookup L;xnetresolver* R=tlsDialResolver(&S,&L,false,false);unsigned Passed=0,Failed=0;static const size_t Limits[]={1,64,1024,8192,65536,131072,262144,1048576};
    for(unsigned N=0;N<sizeof(Limits)/sizeof(Limits[0]);++N){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[N],&B) && xrtAllocationBudgetEnter(B,&Scope),"TLS constructor budget");xtlsclientconfig C;rpcTlsServiceClient(&S,&C);xrpctls* P=NULL;bool OK=xrpcTlsDial(S.Engine,R,XRT_STR_LITERAL("route.test"),S.Address.Port,1,&C,&S.Config,NULL,&P);testRequire(xrtAllocationBudgetLeave(&Scope),"TLS constructor budget leave");
        if(OK){++Passed;xfuture* F=tlsDialCall(P,5000);tlsDialResult(F,XRPC_CALL_OK,XRPC_REPLIED);rpcTlsServiceCycleClose(&S,P);xrtFutureDestroy(F);}else{++Failed;testRequire(!P && xrtErrorKind(xrtGetError())==XERR_MEMORY,"TLS failed constructor output/cause");}xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes && !I.LiveCount,"TLS constructor snapshot/source leak");xrtAllocationBudgetFree(B);
    }
    testRequire(Passed && Failed && xrtNetResolverDestroy(R),"TLS allocation boundary/host join");rpcTlsServiceStop(&S);
}

static void testTlsDialValidation(void)
{
    rpcTlsService S;rpcTlsServiceStart(&S,1,false,false,false);tlsDialLookup L;xnetresolver* R=tlsDialResolver(&S,&L,false,false);xtlsclientconfig C;rpcTlsServiceClient(&S,&C);
    xrpctls* Sentinel=(xrpctls*)(uintptr_t)17; xrpctls* P=Sentinel;xrpctlsdialconfig D;xrpcTlsDialConfigInit(&D);
    C.ServerName=(xstrview){0};testRequire(!xrpcTlsDial(S.Engine,R,XRT_STR_LITERAL("route.test"),S.Address.Port,1,&C,&S.Config,&D,&P) && P==Sentinel && xrtErrorKind(xrtGetError())==XERR_ARGUMENT,"TLS identity must be explicit");rpcTlsServiceClient(&S,&C);
    static const char Embedded[]={'r',0,'t'};testRequire(!xrpcTlsDial(S.Engine,R,(xstrview){Embedded,sizeof(Embedded)},S.Address.Port,1,&C,&S.Config,&D,&P) && P==Sentinel,"embedded DNS NUL accepted");
    D.MaxHostBytes=2;testRequire(!xrpcTlsDial(S.Engine,R,XRT_STR_LITERAL("route.test"),S.Address.Port,1,&C,&S.Config,&D,&P) && P==Sentinel,"DNS name bound ignored");xrpcTlsDialConfigInit(&D);
    for(unsigned I=0;I<3;++I){D.MaxAttempts=I==0?0:I==1?65:8;D.FallbackDelay=I==2?-1:0;testRequire(!xrpcTlsDial(S.Engine,R,XRT_STR_LITERAL("route.test"),S.Address.Port,1,&C,&S.Config,&D,&P) && P==Sentinel,"invalid TLS dial policy accepted");}xrpcTlsDialConfigInit(&D);
    xnetaddr A[65];for(unsigned I=0;I<65;++I){A[I]=S.Address;A[I].Port=(uint16)(10000+I);}xnetaddrlist* List=xrtNetAddrListCreate(A,65);
    testRequire(List && xrtNetAddrListCount(List)==65 && !xrpcTlsDialAddresses(S.Engine,List,1,&C,&S.Config,&D,&P) && P==Sentinel,"prepared endpoint count bound ignored");xrtNetAddrListDestroy(List);
    A[0]=S.Address;A[0].Port=0;List=xrtNetAddrListCreate(A,1);testRequire(List && !xrpcTlsDialAddresses(S.Engine,List,1,&C,&S.Config,&D,&P) && P==Sentinel,"prepared port zero accepted");xrtNetAddrListDestroy(List);
    List=xrtNetAddrListCreate(&S.Address,1);D.Family=XNET_FAMILY_IPV6;testRequire(List && !xrpcTlsDialAddresses(S.Engine,List,1,&C,&S.Config,&D,&P) && P==Sentinel,"empty family selection accepted");xrtNetAddrListDestroy(List);
    testRequire(!xrtAtomic32Load(&L.Calls,XMEMORY_RELAXED) && xrtNetResolverDestroy(R),"validation started asynchronous DNS");xrtClearError();rpcTlsServiceStop(&S);
}
static void testTlsDialColdRelease(void)
{
    rpcTlsService S;rpcTlsServiceStart(&S,1,false,false,true);tlsDialLookup L;xnetresolver* R=tlsDialResolver(&S,&L,true,false);
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1048576,&B) && xrtAllocationBudgetEnter(B,&Scope),"cold TLS source allocation scope");
    xrpctls* P=tlsDialPeer(&S,R,NULL);testRequire(xrtAllocationBudgetLeave(&Scope),"cold TLS source scope leave");tlsDialWait(&L.Entered);
    xallocationbudgetinfo Before={0},After={0};Before.Size=After.Size=sizeof(Before);testRequire(xrtAllocationBudgetInfo(B,&Before) && Before.LiveBytes,"cold TLS source charged owners");
    xfuture* F=tlsDialCall(P,5000);xrtAtomic32Store(&L.Permit,1,XMEMORY_RELEASE);tlsDialWait(&S.VerifyEntered);
    testRequire(xrtAllocationBudgetInfo(B,&After) && After.LiveBytes<Before.LiveBytes,"cold DNS source retained throughout TLS handshake");
    xrtAtomic32Store(&S.VerifyPermit,1,XMEMORY_RELEASE);tlsDialResult(F,XRPC_CALL_OK,XRPC_REPLIED);rpcTlsServiceCycleClose(&S,P);xrtFutureDestroy(F);
    testRequire(xrtAllocationBudgetInfo(B,&After) && !After.LiveBytes && !After.LiveCount && xrtNetResolverDestroy(R),"cold TLS source/session allocation leak");xrtAllocationBudgetFree(B);rpcTlsServiceStop(&S);
}
int main(void)
{setbuf(stdout,NULL);puts("[case] TLS validation");testTlsDialValidation();puts("[case] TLS cold source retirement");testTlsDialColdRelease();puts("[case] TLS DNS snapshot/calls");testTlsDialSnapshot();puts("[case] TLS prepared");testTlsDialPrepared();puts("[case] TLS source stops");testTlsDialStops();puts("[case] TLS verification/deadline");testTlsDialVerification();puts("[case] TLS admission/OOM");testTlsDialAdmissionAndOOM();puts("[PASS] TLS dial: explicit route/identity snapshot/ALPN/creator-release/bidirectional/batch/notify/candidates/deadlines/cancel/verification/physical/native-cause/budgets");return 0;}
