#include "test.h"
#include <xrpc/ws_dial.h>
#define XRPC_WS_HTTP_SERVICE_LISTEN_ONLY
#include "../examples/common/ws_http_service.h"
typedef struct wsDialLookup {rpcWsHttpService* S;xatomic32 Entered,Permit,Calls;bool Hold,Fail;} wsDialLookup;
static xnetaddrlist* wsDialLookupCall(cstr Name,xnetfamily Family,ptr Data)
{
    wsDialLookup* L=Data;testRequire(!xrtNetEngineCurrent(L->S->Engine)&&!strcmp(Name,"route.test")&&(Family==XNET_FAMILY_UNSPEC||Family==XNET_FAMILY_IPV4),"copied DNS route and off-IO lookup");xrtAtomic32FetchAdd(&L->Calls,1,XMEMORY_RELAXED);xrtAtomic32Store(&L->Entered,1,XMEMORY_RELEASE);
    while(L->Hold&&!xrtAtomic32Load(&L->Permit,XMEMORY_ACQUIRE))xrtSleep(1);
    if(L->Fail){xrtSetErrorInfo(XERR_IO,"ws.dns.fixture",17,"route lookup failed");return NULL;}
    xnetaddr A=L->S->Address;A.Port=0;return xrtNetAddrListCreate(&A,1);
}
static xnetresolver* wsDialResolver(rpcWsHttpService* S,wsDialLookup* L,bool Hold,bool Fail)
{
    *L=(wsDialLookup){.S=S,.Hold=Hold,.Fail=Fail};xrtAtomic32Init(&L->Entered,0);xrtAtomic32Init(&L->Permit,0);xrtAtomic32Init(&L->Calls,0);xnetresolverconfig C;xrtNetResolverConfigInit(&C);C.Workers=1;C.CacheEntries=0;C.Lookup=wsDialLookupCall;C.LookupData=L;xnetresolver* R=xrtNetResolverCreate(&C);testRequire(R!=NULL,"borrowed host Resolver");return R;
}
static void wsDialResult(xfuture* F,xrpccallstatus Status,xrpcdelivery Delivery)
{const xrpccallresult* R=rpcWsHttpServiceResult(F);if(R->Status!=Status||R->Delivery!=Delivery)fprintf(stderr,"got%d/%d expected%d/%d\n",R->Status,R->Delivery,Status,Delivery);testRequire(R->Status==Status&&R->Delivery==Delivery,"typed dial result and delivery");}
static xfuture* wsDialCall(xrpcws* P,int64 Timeout)
{xfuture* F=NULL;testRequire(xrpcWsCall(P,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[7]"),Timeout,&F)==XRPC_DUPLEX_ACCEPTED,"queued WS dial call");return F;}
static bool wsDialFactory(rpcWsHttpService* S,unsigned Mode,xnetresolver* Resolver,xnetaddrlist* List,xstrview Name,const xrpcwshttpconfig* H,const xrpcwsdialconfig* D,xrpcws** Out)
{
    #if defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS)
    if(S->TLS){xtlsclientconfig T;xrtTlsClientConfigInit(&T);T.Context=S->Context;T.Verifier=S->Verifier;T.ServerName=XRT_STR_LITERAL("rpc.fixture");
        if(Mode==0)return xrpcWsConnectTls(S->Engine,&S->Address,1,&T,NULL,&S->ClientConfig,H,D,Out);
        if(Mode==1)return xrpcWsDialTls(S->Engine,Resolver,Name,S->Address.Port,1,&T,NULL,&S->ClientConfig,H,D,Out);
        return xrpcWsDialAddressesTls(S->Engine,List,1,&T,NULL,&S->ClientConfig,H,D,Out);}
    #endif
    if(Mode==0)return xrpcWsConnect(S->Engine,&S->Address,1,&S->ClientConfig,H,D,Out);
    if(Mode==1)return xrpcWsDial(S->Engine,Resolver,Name,S->Address.Port,1,&S->ClientConfig,H,D,Out);
    return xrpcWsDialAddresses(S->Engine,List,1,&S->ClientConfig,H,D,Out);
}
static void wsDialStop(rpcWsHttpService* S,xnetresolver* Resolver)
{
    rpcWsHttpServicePeerClose(S->Client);S->Client=NULL;rpcWsHttpServicePeerClose(S->Accepted);S->Accepted=NULL;if(Resolver)testRequire(xrtNetResolverDestroy(Resolver),"independent Resolver physical join");rpcWsHttpServiceStop(S);
}
static void wsDialBasic(bool TLS,unsigned Mode,xnetportbackend Backend)
{
    rpcWsHttpService S;rpcWsHttpServiceListen(&S,TLS,NULL,false,0,NULL,NULL,Backend);wsDialLookup Lookup;xnetresolver* Resolver=Mode==1?wsDialResolver(&S,&Lookup,false,false):NULL;
    xnetaddrlist* List=Mode==2?xrtNetAddrListCreate(&S.Address,1):NULL;xrpcwsdialconfig D;xrpcWsDialConfigInit(&D);D.FallbackDelay=1;
    testRequire(wsDialFactory(&S,Mode,Resolver,List,XRT_STR_LITERAL("route.test"),&S.ClientHTTP,&D,&S.Client),"complete client factory");xrtNetAddrListDestroy(List);xfuture* F[5];F[0]=wsDialCall(S.Client,5000);
    if(rpcWsHttpServiceResult(F[0])->Status!=XRPC_CALL_OK){xrpcwsinfo Diag={.Size=sizeof(Diag)};xrpcwshttpinfo Head={.Size=sizeof(Head)};testRequire(xrpcWsInfo(S.Client,&Diag)&&xrpcWsHttpInfo(S.Client,&Head),"failure diagnostic snapshots");fprintf(stderr,"dial TLS%u mode%u state%d reason%d native%d/%d/%s http%d status%u write%zu header%zu\n",TLS,Mode,Diag.State,Diag.Reason,Diag.NativeErrorKind,Diag.NativeErrorCode,Diag.NativeErrorDomain,Head.Outcome,Head.Status,Head.WriteBytes,Head.HeaderBytes);}
    wsDialResult(F[0],XRPC_CALL_OK,XRPC_REPLIED);rpcWsHttpServiceAwait(&S.ServerReady);
    testRequire(xrpcWsCall(S.Accepted,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[8]"),5000,F+1)==XRPC_DUPLEX_ACCEPTED,"dial reverse call");wsDialResult(F[1],XRPC_CALL_OK,XRPC_REPLIED);
    testRequire(xrpcWsNotify(S.Client,XRT_STR_LITERAL("note"),(xstrview){0},5000,F+2)==XRPC_DUPLEX_ACCEPTED,"dial notification");wsDialResult(F[2],XRPC_CALL_NOTIFY_DONE,XRPC_MAY_HAVE_EXECUTED);
    xrpcduplexitem Items[]={{false,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]")},{true,XRT_STR_LITERAL("note"),{0}}};testRequire(xrpcWsBatch(S.Client,Items,2,5000,F+3)==XRPC_DUPLEX_ACCEPTED,"dial batch");wsDialResult(F[3],XRPC_CALL_BATCH_DONE,XRPC_REPLIED);
    xrpcwshttpinfo H={.Size=sizeof(H)};double End=xrtTimer()+5;do{testRequire(xrpcWsHttpInfo(S.Client,&H)&&xrtTimer()<End,"cold controller/HTTP owners retire");if(H.ColdBytes)xrtSleep(1);}while(H.ColdBytes);testRequire(H.Outcome==XRPC_WS_HTTP_OPEN&&H.Status==101&&H.Secure==TLS,"actual same-handle setup facts");
    xrpcwsinfo I={.Size=sizeof(I)};testRequire(xrpcWsInfo(S.Client,&I)&&I.State==XRPC_WS_OPEN&&I.Encrypted==TLS&&I.ProtocolBytes==7,"native peer facts after actual readiness");wsDialStop(&S,Resolver);wsDialResult(F[0],XRPC_CALL_OK,XRPC_REPLIED);for(unsigned N=0;N<4;++N)xrtFutureDestroy(F[N]);
    printf("[PASS] WS dial TLS%u mode%u reverse/batch/notify/cold retirement/native joins\n",TLS,Mode);
}
static void wsDialHeld(bool TLS,unsigned Mode,xnetportbackend Backend)
{
    rpcWsHttpService S;rpcWsHttpServiceListen(&S,TLS,NULL,false,0,NULL,NULL,Backend);wsDialLookup L;xnetresolver* Resolver=wsDialResolver(&S,&L,true,false);xrpcwshttpconfig H=S.ClientHTTP;H.SetupTimeout=Mode==0?150:5000;
    testRequire(wsDialFactory(&S,1,Resolver,NULL,XRT_STR_LITERAL("route.test"),&H,NULL,&S.Client),"held DNS factory");rpcWsHttpServiceAwait(&L.Entered);xfuture* F=wsDialCall(S.Client,5000);xfuture* Cancel=wsDialCall(S.Client,5000);testRequire(xrtFutureCancel(Cancel),"cancel queued operation during DNS");wsDialResult(Cancel,XRPC_CALL_CANCELLED,XRPC_NOT_SENT);
    if(Mode==1)testRequire(xrpcWsClose(S.Client),"close while foreign DNS lookup remains running");
    wsDialResult(F,Mode==0?XRPC_CALL_TIMEOUT:XRPC_CALL_CLOSED,XRPC_NOT_SENT);testRequire(!xrtAtomic32Load(&S.Calls,XMEMORY_ACQUIRE)&&!xrtAtomic32Load(&S.ServerReady,XMEMORY_ACQUIRE),"no RPC business/TLS connection before DNS result");
    xrtAtomic32Store(&L.Permit,1,XMEMORY_RELEASE);wsDialStop(&S,Resolver);xrtFutureDestroy(F);xrtFutureDestroy(Cancel);puts("[PASS] held DNS cancel/original setup deadline/close, NOT_SENT and separate Resolver join");
}
static void wsDialInvalid(xnetportbackend Backend)
{
    rpcWsHttpService S;rpcWsHttpServiceListen(&S,false,NULL,false,0,NULL,NULL,Backend);xrpcws* P=(xrpcws*)(uintptr_t)17;xrpcwsdialconfig D;xrpcWsDialConfigInit(&D);D.MaxAttempts=65;
    testRequire(!xrpcWsConnect(S.Engine,&S.Address,1,NULL,&S.ClientHTTP,&D,&P)&&P==(xrpcws*)(uintptr_t)17&&xrtErrorKind(xrtGetError())==XERR_ARGUMENT,"invalid bounds preserve output");xrtClearError();xrpcWsDialConfigInit(&D);xnetaddr A=S.Address;A.Port=0;
    testRequire(!xrpcWsConnect(S.Engine,&A,1,NULL,&S.ClientHTTP,&D,&P)&&P==(xrpcws*)(uintptr_t)17,"zero numeric port rejected");xrtClearError();
    testRequire(!xrpcWsDial(S.Engine,NULL,XRT_STR_LITERAL("route.test"),1,1,NULL,&S.ClientHTTP,&D,&P)&&P==(xrpcws*)(uintptr_t)17,"DNS requires explicit Resolver");xrtClearError();
    xnetaddrlist* List=xrtNetAddrListCreate(&A,1);testRequire(List&&!xrpcWsDialAddresses(S.Engine,List,1,NULL,&S.ClientHTTP,&D,&P)&&P==(xrpcws*)(uintptr_t)17,"prepared zero port rejected");xrtNetAddrListDestroy(List);xrtClearError();
    wsDialStop(&S,NULL);puts("[PASS] checked configuration, routing and failure ownership");
}
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);xnetportbackend B=Argc>1&&!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:Argc>1&&!strcmp(Argv[1],"epoll")?XNET_PORT_EPOLL:XNET_PORT_SELECT;wsDialInvalid(B);
    for(unsigned TLS=0;TLS<
        #if defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS)
        2
        #else
        1
        #endif
        ;++TLS){for(unsigned Mode=0;Mode<3;++Mode)wsDialBasic(TLS!=0,Mode,B);for(unsigned Mode=0;Mode<2;++Mode)wsDialHeld(TLS!=0,Mode,B);}
    return 0;
}
