#include <xrpc/ws_uri.h>
#include <xrt/memory_stats.h>
#define main wsUriExistingDialMain
#include "test_ws_dial.c"
#undef main
typedef struct uriGate {xnetpost Post;xatomic32 Entered,Permit;} uriGate;
static void uriGateRun(xnetworker* W,ptr Data)
{(void)W;uriGate* G=Data;xrtAtomic32Store(&G->Entered,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&G->Permit,XMEMORY_ACQUIRE))xrtSleep(1);}
static void uriGateStart(rpcWsHttpService* S,uriGate* G)
{xrtAtomic32Init(&G->Entered,0);xrtAtomic32Init(&G->Permit,0);testRequire(xrtNetPostInit(&G->Post)&&xrtNetPost(xrtNetEngineWorker(S->Engine,1),&G->Post,uriGateRun,G),"URI owner barrier");rpcWsHttpServiceAwait(&G->Entered);}
typedef struct uriScene {rpcWsHttpService S;char Authority[160];xstrview Route,Identity;xatomic32 Lookups,Authorized,Verified;} uriScene;
static xnetaddrlist* uriLookup(cstr Name,xnetfamily Family,ptr Data)
{uriScene* U=Data;testRequire(!xrtNetEngineCurrent(U->S.Engine)&&strlen(Name)==U->Route.Size&&!memcmp(Name,U->Route.Data,U->Route.Size)&&(Family==XNET_FAMILY_UNSPEC||Family==XNET_FAMILY_IPV4),"decoded URI route on host resolver");xrtAtomic32FetchAdd(&U->Lookups,1,XMEMORY_RELAXED);xnetaddr A=U->S.Address;A.Port=0;return xrtNetAddrListCreate(&A,1);}
static xrpcwshttpauthstatus uriAuthorize(const xrpcwshttpauthrequest* R,bytes B,size_t Capacity,size_t* Used,ptr Data)
{(void)B;(void)Capacity;uriScene* U=Data;const xhttpfield* H=xrtHttp1Field(R->Head,XRT_STR_LITERAL("Host"));testRequire(!xrtNetEngineCurrent(U->S.Engine)&&H&&H->Value.Size==strlen(U->Authority)&&!memcmp(H->Value.Data,U->Authority,H->Value.Size),"original URI authority survives caller mutation and reaches HTTP");*Used=0;xrtAtomic32FetchAdd(&U->Authorized,1,XMEMORY_RELAXED);return XRPC_WS_HTTP_ALLOW;}
#if defined(XRPC_FEATURE_XRPC_WS_URI_TLS)
static xtlsverifydecision uriVerify(const xtlspeer* P,ptr Data)
{uriScene* U=Data;xbytesview Pin={0};bool OK=P&&P->CertificateCount&&P->Name.Size==U->Identity.Size&&!memcmp(P->Name.Data,U->Identity.Data,P->Name.Size)&&xrtTlsIdentityCertificate(U->S.Identity,0,&Pin)&&P->Certificates[0].Raw.Size==Pin.Size&&!memcmp(P->Certificates[0].Raw.Data,Pin.Data,Pin.Size);xrtAtomic32FetchAdd(&U->Verified,1,XMEMORY_RELAXED);return OK?XTLS_VERIFY_ACCEPT:XTLS_VERIFY_REJECT;}
#endif
static void uriBasic(bool TLS,unsigned Mode,xnetportbackend Backend)
{
    uriScene U={0};xrtAtomic32Init(&U.Lookups,0);xrtAtomic32Init(&U.Authorized,0);xrtAtomic32Init(&U.Verified,0);U.Route=XRT_STR_LITERAL("route.test");U.Identity=U.Route;
    cstr Host=Mode==0?"127.0.0.1":Mode==2?"r%6fute.test":Mode==6?"caf%C3%A9.test":Mode==7?"%31%32%37.0.0.1":Mode==8?"[::1]":"route.test";
    cstr Suffix=Mode==3?"?x=%2f&v=%00":Mode==4?"/rpc?":Mode==5?"//rpc/%23?x=1?2":"/rpc";
    cstr Target=Mode==3?"/?x=%2f&v=%00":Mode==4?"/rpc":Suffix;
    bool Numeric=Mode==0||Mode==7||Mode==8;
    if(Numeric)U.Identity=Mode==8?XRT_STR_LITERAL("::1"):XRT_STR_LITERAL("127.0.0.1");
    if(Mode==6){U.Route=XRT_STR_LITERAL("caf\xc3\xa9.test");U.Identity=XRT_STR_LITERAL("explicit.fixture");}
    xrpcwshttpconfig Server;xrpcWsHttpConfigInit(&Server);Server.Target=(xstrview){Target,strlen(Target)};Server.Authorize=uriAuthorize;Server.AuthorizationUser=&U;
    rpcWsHttpServiceListen(&U.S,TLS,NULL,false,0,NULL,&Server,Backend);
    if(Mode==8){testRequire(xrtNetListenerClose(U.S.Listener),"replace empty IPv4 URI listener");xrtNetListenerDestroy(U.S.Listener);xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV6,0);xnetlistenerevents E={rpcWsHttpServiceAccept,NULL,NULL};U.S.Listener=xrtNetListen(U.S.Engine,&L,&E,NULL,&U.S);testRequire(U.S.Listener&&xrtNetListenerLocal(U.S.Listener,&U.S.Address),"actual IPv6 URI listener");}
    xnetresolverconfig RC;xrtNetResolverConfigInit(&RC);RC.Workers=1;RC.CacheEntries=0;RC.Lookup=uriLookup;RC.LookupData=&U;xnetresolver* R=xrtNetResolverCreate(&RC);testRequire(R!=NULL,"URI host Resolver");
    snprintf(U.Authority,sizeof(U.Authority),"%s:%u",Host,(unsigned)U.S.Address.Port);char URI[300];int Bytes=snprintf(URI,sizeof(URI),"%s://%s%s",TLS?"wss":"Ws",U.Authority,Suffix);testRequire(Bytes>0&&(size_t)Bytes<sizeof(URI),"URI fixture bound");uriGate G;uriGateStart(&U.S,&G);bool OK;
    #if defined(XRPC_FEATURE_XRPC_WS_URI_TLS)
    xtlsverifier* V=NULL;
    if(TLS){xtlsverifierconfig VC;xrtTlsVerifierConfigInit(&VC);VC.Verify=uriVerify;VC.Context=&U;V=xrtTlsVerifierCreate(&VC);testRequire(V!=NULL,"URI trust policy");xtlsclientconfig C;xrtTlsClientConfigInit(&C);C.Context=U.S.Context;C.Verifier=V;if(Mode==6){C.ServerName=XRT_STR_LITERAL("rpc.fixture");C.VerifyName=U.Identity;}OK=xrpcWsConnectUriTls(U.S.Engine,Numeric?NULL:R,(xstrview){URI,(size_t)Bytes},1,&C,NULL,&U.S.ClientConfig,NULL,NULL,&U.S.Client);}else
    #endif
    OK=xrpcWsConnectUri(U.S.Engine,Numeric?NULL:R,(xstrview){URI,(size_t)Bytes},1,&U.S.ClientConfig,NULL,NULL,&U.S.Client);
    testRequire(OK,"URI factory before owner/network setup");memset(URI,'x',(size_t)Bytes);
    #if defined(XRPC_FEATURE_XRPC_WS_URI_TLS)
    xrtTlsVerifierRelease(V);
    #endif
    xfuture* F=wsDialCall(U.S.Client,5000);xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);wsDialResult(F,XRPC_CALL_OK,XRPC_REPLIED);testRequire(xrtAtomic32Load(&U.Authorized,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&U.Lookups,XMEMORY_ACQUIRE)==(Numeric?0:1)&&xrtAtomic32Load(&U.Verified,XMEMORY_ACQUIRE)==(TLS?1:0),"actual URI routing/authority/identity and numeric resolver bypass");wsDialStop(&U.S,R);wsDialResult(F,XRPC_CALL_OK,XRPC_REPLIED);xrtFutureDestroy(F);printf("[PASS] URI TLS%u mode%u exact host/resource, copied input and physical joins\n",TLS,Mode);
}
static void uriDeadlineAndAllocations(xnetportbackend Backend)
{
    rpcWsHttpService S;rpcWsHttpServiceListen(&S,false,NULL,false,0,NULL,NULL,Backend);uriGate G;uriGateStart(&S,&G);char Authority[64],URI[100];snprintf(Authority,sizeof(Authority),"127.0.0.1:%u",(unsigned)S.Address.Port);int N=snprintf(URI,sizeof(URI),"ws://%s/rpc",Authority);xrpcwshttpconfig H=S.ClientHTTP;H.Host=(xstrview){Authority,strlen(Authority)};H.SetupTimeout=0;xrpcws* Direct=NULL;
    xmemstats A,B,C;xrtMemStatsEnable(true);xrtMemStatsGet(&A);testRequire(xrpcWsConnect(S.Engine,&S.Address,1,&S.ClientConfig,&H,NULL,&Direct),"direct baseline under owner barrier");xrtMemStatsGet(&B);testRequire(xrpcWsConnectUri(S.Engine,NULL,(xstrview){URI,(size_t)N},1,&S.ClientConfig,&H,NULL,&S.Client),"URI baseline under owner barrier");xrtMemStatsGet(&C);xrtMemStatsEnable(false);testRequire(B.BlockAllocCalls-A.BlockAllocCalls==C.BlockAllocCalls-B.BlockAllocCalls&&B.BlockAllocBytes-A.BlockAllocBytes==C.BlockAllocBytes-B.BlockAllocBytes,"URI composition introduces no additional constructor heap allocation or bytes");
    xfuture* F=wsDialCall(S.Client,5000);xfuture* D=wsDialCall(Direct,5000);xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);wsDialResult(F,XRPC_CALL_TIMEOUT,XRPC_NOT_SENT);wsDialResult(D,XRPC_CALL_TIMEOUT,XRPC_NOT_SENT);rpcWsHttpServicePeerClose(Direct);wsDialStop(&S,NULL);xrtFutureDestroy(F);xrtFutureDestroy(D);puts("[PASS] original URI factory deadline and actual same-input constructor allocation parity");
}
static void uriInvalid(xnetportbackend Backend)
{
    rpcWsHttpService S;rpcWsHttpServiceListen(&S,false,NULL,false,0,NULL,NULL,Backend);xrpcws* P=(xrpcws*)(uintptr_t)17;
    const char* Bad[]={"wss://host/rpc","ws://%00/rpc","ws://%0d/rpc","ws://a%2fb/rpc","ws://%ff/rpc","ws://host/rpc#x"};
    for(size_t I=0;I<sizeof(Bad)/sizeof(*Bad);++I){testRequire(!xrpcWsConnectUri(S.Engine,NULL,(xstrview){Bad[I],strlen(Bad[I])},1,NULL,NULL,NULL,&P)&&P==(xrpcws*)(uintptr_t)17,"URI invalid/scheme/decoded host preserves peer output");xrtClearError();}
    testRequire(!xrpcWsConnectUri(S.Engine,NULL,XRT_STR_LITERAL("ws://[v1.a:b]/"),1,NULL,NULL,NULL,&P)&&P==(xrpcws*)(uintptr_t)17&&xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED,"IPvFuture returns unsupported without DNS guessing");xrtClearError();wsDialStop(&S,NULL);puts("[PASS] URI failure ownership, explicit scheme and safe decoded routing");
}
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);xnetportbackend B=Argc>1&&!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:Argc>1&&!strcmp(Argv[1],"epoll")?XNET_PORT_EPOLL:XNET_PORT_SELECT;uriInvalid(B);uriDeadlineAndAllocations(B);
    for(unsigned TLS=0;TLS<
        #if defined(XRPC_FEATURE_XRPC_WS_URI_TLS)
        2
        #else
        1
        #endif
        ;++TLS){for(unsigned Mode=0;Mode<=8;++Mode)uriBasic(TLS!=0,Mode,B);}
    return 0;
}
