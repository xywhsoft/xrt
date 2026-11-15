#define main wsDialBasicMain
#include "test_ws_dial.c"
#undef main
typedef struct wsDialGate {xnetpost Post;xatomic32 Entered,Permit;} wsDialGate;
static void wsDialGateRun(xnetworker* W,ptr Data)
{(void)W;wsDialGate* G=Data;xrtAtomic32Store(&G->Entered,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&G->Permit,XMEMORY_ACQUIRE))xrtSleep(1);}
static void wsDialGateStart(rpcWsHttpService* S,wsDialGate* G)
{xrtAtomic32Init(&G->Entered,0);xrtAtomic32Init(&G->Permit,0);testRequire(xrtNetPostInit(&G->Post)&&xrtNetPost(xrtNetEngineWorker(S->Engine,1),&G->Post,wsDialGateRun,G),"test-only owner queue barrier");rpcWsHttpServiceAwait(&G->Entered);}
static void wsDialSnapshot(bool TLS,xnetportbackend Backend)
{
    rpcWsHttpService S;rpcWsHttpServiceListen(&S,TLS,NULL,false,0,NULL,NULL,Backend);wsDialLookup L;xnetresolver* R=wsDialResolver(&S,&L,false,false);wsDialGate G;wsDialGateStart(&S,&G);
    char Name[]="route.test",Host[]="rpc.fixture",Target[]="/rpc",Context[]="[11]";S.ClientConfig.RequestContext=(xbytesview){(cbytes)Context,4};xrpcwshttpconfig H=S.ClientHTTP;H.Host=(xstrview){Host,11};H.Target=(xstrview){Target,4};bool OK;
    #if defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS)
    char Identity[]="rpc.fixture",Alpn[]="http/1.1";xstrview Protocol={Alpn,8};
    if(TLS){xtlsclientconfig T;xrtTlsClientConfigInit(&T);T.Context=S.Context;T.Verifier=S.Verifier;T.ServerName=(xstrview){Identity,11};T.Protocols=&Protocol;T.ProtocolCount=1;OK=xrpcWsDialTls(S.Engine,R,(xstrview){Name,10},S.Address.Port,1,&T,NULL,&S.ClientConfig,&H,NULL,&S.Client);}
    else
    #endif
    OK=xrpcWsDial(S.Engine,R,(xstrview){Name,10},S.Address.Port,1,&S.ClientConfig,&H,NULL,&S.Client);
    testRequire(OK,"snapshot before queued native establishment");memset(Name,'x',10);memset(Host,'x',11);memset(Target,'x',4);memset(Context,'x',4);
    #if defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS)
    if(TLS){memset(Identity,'x',11);memset(Alpn,'x',8);xrtTlsVerifierRelease(S.Verifier);S.Verifier=NULL;}
    #endif
    xfuture* F[5];F[0]=wsDialCall(S.Client,5000);F[1]=wsDialCall(S.Client,0);F[2]=wsDialCall(S.Client,5000);testRequire(xrtFutureCancel(F[2]),"cancel before owner admission");xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);
    wsDialResult(F[0],XRPC_CALL_OK,XRPC_REPLIED);wsDialResult(F[1],XRPC_CALL_TIMEOUT,XRPC_NOT_SENT);wsDialResult(F[2],XRPC_CALL_CANCELLED,XRPC_NOT_SENT);rpcWsHttpServiceAwait(&S.ServerReady);
    testRequire(xrpcWsCall(S.Accepted,XRT_STR_LITERAL("who"),(xstrview){0},5000,F+3)==XRPC_DUPLEX_ACCEPTED,"reverse copied request context");wsDialResult(F[3],XRPC_CALL_OK,XRPC_REPLIED);const xrpccallresult* Result=rpcWsHttpServiceResult(F[3]);xrpcresponseinfo I={.Size=sizeof(I)};testRequire(xrpcResponsesInfo(Result->Responses,&I)&&Result->Index<I.Count&&I.Messages[Result->Index].Result.Raw.Size==4&&!memcmp(I.Messages[Result->Index].Result.Raw.Data,"[11]",4),"immutable context survives mutated source");
    wsDialStop(&S,R);for(unsigned N=0;N<4;++N)xrtFutureDestroy(F[N]);puts("[PASS] queued DNS/HTTP/TLS/context snapshots, released verifier alias, independent call clocks");
}
static void wsDialBudget(bool TLS,xnetportbackend Backend)
{
    rpcWsHttpService S;rpcWsHttpServiceListen(&S,TLS,NULL,false,0,NULL,NULL,Backend);wsDialGate G;wsDialGateStart(&S,&G);static const size_t Limits[]={1,256,4096,16384,32768,65536,131072,262144,1048576,16777216};xallocationbudget* Held=NULL;unsigned Refused=0;
    for(size_t N=0;N<sizeof(Limits)/sizeof(*Limits);++N){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[N],&B)&&xrtAllocationBudgetEnter(B,&Scope),"original constructor allocation budget");xrpcws* P=(xrpcws*)(uintptr_t)17;bool OK=wsDialFactory(&S,0,NULL,NULL,(xstrview){0},&S.ClientHTTP,NULL,&P);xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"leave original constructor budget");xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(xrtAllocationBudgetInfo(B,&I),"constructor budget ledger");
        if(OK){S.Client=P;Held=B;break;}testRequire(P==(xrpcws*)(uintptr_t)17&&I.Rejections&&!I.LiveCount&&!I.LiveBytes,"all failed constructor allocations/aliases/output roll back");xrtAllocationBudgetFree(B);++Refused;}
    testRequire(Held&&Refused>=4,"one real sufficient budget after actual refusals");xfuture* F=wsDialCall(S.Client,5000);xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);wsDialResult(F,XRPC_CALL_OK,XRPC_REPLIED);wsDialStop(&S,NULL);xrtFutureDestroy(F);xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(xrtAllocationBudgetInfo(Held,&I)&&!I.LiveCount&&!I.LiveBytes,"successful original budget returns to zero after physical native/peer joins");xrtAllocationBudgetFree(Held);puts("[PASS] actual constructor allocation ladder, transactional failure and physical credit return");
}
static void wsDialFallback(bool TLS,xnetportbackend Backend)
{
    rpcWsHttpService S;rpcWsHttpServiceListen(&S,TLS,NULL,false,0,NULL,NULL,Backend);xnetaddr A[2];xrtNetAddrLoopback(A,XNET_FAMILY_IPV4,0);xnetsocket Bound=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_STREAM,0);testRequire(Bound&&xrtNetSocketBind(Bound,A)&&xrtNetSocketLocal(Bound,A),"reserve a real refused candidate");A[1]=S.Address;xnetaddrlist* List=xrtNetAddrListCreate(A,2);xrpcwsdialconfig D;xrpcWsDialConfigInit(&D);D.Family=XNET_FAMILY_IPV4;D.FallbackDelay=0;D.MaxAttempts=2;
    testRequire(List&&wsDialFactory(&S,2,NULL,List,(xstrview){0},&S.ClientHTTP,&D,&S.Client),"candidate failover factory");xrtNetAddrListDestroy(List);xfuture* F=wsDialCall(S.Client,5000);wsDialResult(F,XRPC_CALL_OK,XRPC_REPLIED);testRequire(xrtAtomic32Load(&S.Calls,XMEMORY_ACQUIRE)==1,"losers never send RPC or replay business");wsDialStop(&S,NULL);xrtFutureDestroy(F);testRequire(xrtNetSocketClose(Bound),"reserved candidate close");puts("[PASS] actual refused candidate then winner, retained prepared list and no replay");
}
static void wsDialQueuedDeadline(bool TLS,xnetportbackend Backend)
{
    rpcWsHttpService S;rpcWsHttpServiceListen(&S,TLS,NULL,false,0,NULL,NULL,Backend);wsDialGate G;wsDialGateStart(&S,&G);xrpcwshttpconfig H=S.ClientHTTP;H.SetupTimeout=0;
    testRequire(wsDialFactory(&S,0,NULL,NULL,(xstrview){0},&H,NULL,&S.Client),"zero original setup deadline factory");xfuture* F=wsDialCall(S.Client,5000);xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);wsDialResult(F,XRPC_CALL_TIMEOUT,XRPC_NOT_SENT);xrpcwshttpinfo I={.Size=sizeof(I)};testRequire(xrpcWsHttpInfo(S.Client,&I)&&I.Outcome==XRPC_WS_HTTP_TIMED_OUT&&!I.HeaderBytes&&!I.WriteBytes&&!xrtAtomic32Load(&S.ServerReady,XMEMORY_ACQUIRE),"owner queue never refreshes setup clock or starts a connection");wsDialStop(&S,NULL);xrtFutureDestroy(F);puts("[PASS] zero factory-entry deadline expires before native establishment");
}
static void wsDialDNSFailure(bool TLS,xnetportbackend Backend)
{
    rpcWsHttpService S;rpcWsHttpServiceListen(&S,TLS,NULL,false,0,NULL,NULL,Backend);wsDialLookup L;xnetresolver* R=wsDialResolver(&S,&L,false,true);testRequire(wsDialFactory(&S,1,R,NULL,XRT_STR_LITERAL("route.test"),&S.ClientHTTP,NULL,&S.Client),"DNS failure admitted asynchronously");xfuture* F=wsDialCall(S.Client,5000);wsDialResult(F,XRPC_CALL_TRANSPORT,XRPC_NOT_SENT);xrpcwsinfo I={.Size=sizeof(I)};testRequire(xrpcWsInfo(S.Client,&I),"DNS native diagnostic snapshot");printf("[DNS] kind%d code%d domain%s\n",I.NativeErrorKind,I.NativeErrorCode,I.NativeErrorDomain);testRequire(I.NativeErrorKind==XERR_IO&&I.NativeErrorCode==XNET_ERROR_DIAL_RESOLVE&&!strcmp(I.NativeErrorDomain,"xrt.net")&&!xrtAtomic32Load(&S.Calls,XMEMORY_ACQUIRE),"owned DNS error survives native source/controller retirement without business");wsDialStop(&S,R);xrtFutureDestroy(F);puts("[PASS] owned DNS diagnostic and unsent failure after source retirement");
}
#if defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS)
typedef struct wsDialSilent {xatomicptr Native;bool TLS;xtlsserverconfig Config;xatomic32 AppReads;} wsDialSilent;
static void wsDialSilentRead(xtlsstream* T,const xnetbuf* B,ptr Data)
{(void)T;(void)B;wsDialSilent* S=Data;xrtAtomic32FetchAdd(&S->AppReads,1,XMEMORY_RELAXED);}
static bool wsDialSilentAccept(xnetlistener* L,xnetstream* Native,ptr Data)
{
    (void)L;wsDialSilent* S=Data;if(S->TLS){xtlsstreamevents Events={.Read=wsDialSilentRead};xtlsstream* T=NULL;testRequire(xrtTlsStreamAccept(Native,&S->Config,NULL,&Events,S,&T),"borrowed fixture TLS accept");xrtAtomicPtrStore(&S->Native,T,XMEMORY_RELEASE);}else xrtAtomicPtrStore(&S->Native,Native,XMEMORY_RELEASE);return true;
}
static void wsDialHandshake(unsigned Mode,xnetportbackend Backend)
{
    rpcWsHttpService S;rpcWsHttpServiceListen(&S,true,NULL,false,0,NULL,NULL,Backend);testRequire(xrtNetListenerClose(S.Listener),"replace empty fixture listener");xrtNetListenerDestroy(S.Listener);wsDialSilent Raw={.TLS=Mode>=2,.Config=S.NativeServer};xrtAtomicPtrInit(&Raw.Native,NULL);xrtAtomic32Init(&Raw.AppReads,0);xstrview Protocol=XRT_STR_LITERAL("h2");if(Mode==2){Raw.Config.Protocols=&Protocol;Raw.Config.ProtocolCount=1;}
    xnetlistenconfig Listen;xrtNetListenConfigInit(&Listen);xrtNetAddrLoopback(&Listen.Address,XNET_FAMILY_IPV4,0);xnetlistenerevents Events={wsDialSilentAccept,NULL,NULL};S.Listener=xrtNetListen(S.Engine,&Listen,&Events,NULL,&Raw);testRequire(S.Listener&&xrtNetListenerLocal(S.Listener,&S.Address),"explicit silent handshake listener");
    xtlsclientconfig T;xrtTlsClientConfigInit(&T);T.Context=S.Context;T.Verifier=S.Verifier;T.ServerName=Mode==3?XRT_STR_LITERAL("wrong.fixture"):XRT_STR_LITERAL("rpc.fixture");if(Mode==2){T.Protocols=&Protocol;T.ProtocolCount=1;}xrpcwshttpconfig H=S.ClientHTTP;H.SetupTimeout=Mode==0?250:5000;
    testRequire(xrpcWsConnectTls(S.Engine,&S.Address,1,&T,NULL,&S.ClientConfig,&H,NULL,&S.Client),"native TLS setup factory");xfuture* F=wsDialCall(S.Client,5000);
    if(Mode<2){double End=xrtTimer()+5;xnetstream* Native;while(!(Native=xrtAtomicPtrLoad(&Raw.Native,XMEMORY_ACQUIRE))||!xrtNetStreamAvailable(Native)){testRequire(xrtTimer()<End,"real TLS ClientHello reached native TCP");xrtSleep(1);}if(Mode==1)testRequire(xrpcWsClose(S.Client),"abort during actual TLS handshake");}
    wsDialResult(F,Mode==0?XRPC_CALL_TIMEOUT:Mode==1?XRPC_CALL_CLOSED:Mode==2?XRPC_CALL_PROTOCOL:XRPC_CALL_TRANSPORT,XRPC_NOT_SENT);testRequire(!xrtAtomic32Load(&Raw.AppReads,XMEMORY_ACQUIRE),"no HTTP/RPC bytes on rejected or unfinished TLS/ALPN");rpcWsHttpServicePeerClose(S.Client);S.Client=NULL;ptr Native=xrtAtomicPtrLoad(&Raw.Native,XMEMORY_ACQUIRE);testRequire(Native!=NULL,"actual native accept ownership");
    if(Raw.TLS){testRequire(xrtTlsStreamAbort(Native)&&xrtTlsStreamWaitRetired(Native,5000)==XWAIT_OK,"borrowed fixture TLS physical tail");xrtTlsStreamDestroy(Native);}else{testRequire(xrtNetStreamAbort(Native)&&xrtNetStreamWaitRetired(Native,5000)==XWAIT_OK,"silent native TCP physical tail");xrtNetStreamDestroy(Native);}rpcWsHttpServiceStop(&S);xrtFutureDestroy(F);puts("[PASS] actual TLS handshake timeout/close and ALPN/identity rejection before HTTP");
}
#endif
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);xnetportbackend B=Argc>1&&!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:Argc>1&&!strcmp(Argv[1],"epoll")?XNET_PORT_EPOLL:XNET_PORT_SELECT;
    for(unsigned TLS=0;TLS<
        #if defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS)
        2
        #else
        1
        #endif
        ;++TLS){wsDialSnapshot(TLS!=0,B);wsDialBudget(TLS!=0,B);wsDialFallback(TLS!=0,B);wsDialQueuedDeadline(TLS!=0,B);wsDialDNSFailure(TLS!=0,B);}
    #if defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS)
    for(unsigned Mode=0;Mode<4;++Mode)wsDialHandshake(Mode,B);
    #endif
    return 0;
}
