#define main mqttUriPositiveMain
#include "test_client_ws_uri.c"
#undef main
#include <xrt/memory_stats.h>
static bool mqttUriStart(mqttWsConnectPeer* P,bool TLS,xstrview URI,const xmqttclientconfig* K,const xmqttclientwsconnectconfig* W,xmqttclient** C)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
    if(TLS){xmqttclienttlsconfig T;xmqttClientTlsConfigInit(&T);T.Session.Context=P->Base.Context;T.Session.Verifier=P->Base.Verifier;T.Session.ResumeLimit=0;return xmqttClientWssConnectUri(P->Base.Engine,P->Resolver,URI,1,K,W,&T,&P->Base.Connect,C);}
#else
    (void)TLS;
#endif
    return xmqttClientWsConnectUri(P->Base.Engine,P->Resolver,URI,1,K,W,&P->Base.Connect,C);
}
static void mqttUriInvalid(xnetportbackend B)
{
    mqttWsConnectPeer P;mqttWsConnectInit(&P,false,XMQTT_VERSION_5,B);xmqttclient* C=(xmqttclient*)(uintptr_t)17;
    const char* Bad[]={"wss://host/mqtt","ws://%00/","ws://%20/","ws://%7f/","ws://%0d/","ws://a%2fb/","ws://%5c/","ws://%3a/","ws://%40/","ws://%5b/","ws://%ff/","ws://%c0%80/","ws://host/#x","ws://u:p@host/","ws://host:0/"};
    for(size_t I=0;I<sizeof(Bad)/sizeof(*Bad);++I){testRequire(!mqttUriStart(&P,false,(xstrview){Bad[I],strlen(Bad[I])},NULL,NULL,&C)&&C==(xmqttclient*)(uintptr_t)17,"invalid/scheme/decoded URI preserves actual output");xrtClearError();}
    testRequire(!mqttUriStart(&P,false,XRT_STR_LITERAL("ws://[v1.a:b]/"),NULL,NULL,&C)&&C==(xmqttclient*)(uintptr_t)17&&xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED,"IPvFuture is explicit unsupported without DNS guessing");xrtClearError();
    union {xmqttclientwsconnectconfig W;xmqttclient* C;} Alias;xmqttClientWsConnectConfigInit(&Alias.W);unsigned char Before[sizeof(Alias)];memcpy(Before,&Alias,sizeof(Alias));testRequire(!mqttUriStart(&P,false,XRT_STR_LITERAL("ws://example.com/mqtt"),NULL,&Alias.W,&Alias.C)&&!memcmp(Before,&Alias,sizeof(Alias)),"original policy/output overlap rejected before local copy");xrtClearError();
    union {char URI[160];xmqttclient* C;} Text;memcpy(Text.URI,"ws://example.com/mqtt",22);unsigned char Saved[sizeof(Text)];memcpy(Saved,&Text,sizeof(Text));testRequire(!mqttUriStart(&P,false,(xstrview){Text.URI,21},NULL,NULL,&Text.C)&&!memcmp(Saved,&Text,sizeof(Text)),"whole URI/output overlap rejected before mutation");xrtClearError();
#if !defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    {xmqttclientwsconnectconfig W;xmqttClientWsConnectConfigInit(&W);W.WebSocket.Auth=(struct xmqttauthconfig*)(uintptr_t)1;testRequire(!mqttUriStart(&P,false,XRT_STR_LITERAL("ws://example.com/mqtt"),NULL,&W,&C)&&C==(xmqttclient*)(uintptr_t)17&&xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED,"URI excluded Auth refuses without dereferencing opaque descriptor");xrtClearError();}
#endif
#if !defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    {xmqttclientwsconnectconfig W;xmqttClientWsConnectConfigInit(&W);W.WebSocket.Session=(struct xmqttsession*)(uintptr_t)1;testRequire(!mqttUriStart(&P,false,XRT_STR_LITERAL("ws://example.com/mqtt"),NULL,&W,&C)&&C==(xmqttclient*)(uintptr_t)17&&xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED,"URI excluded Session refuses without dereferencing opaque descriptor");xrtClearError();}
#endif
    testRequire(!xrtAtomic32Load(&P.DnsEntered,XMEMORY_ACQUIRE)&&!xrtAtomic32Load(&P.Header,XMEMORY_ACQUIRE),"refused URI causes no network or lookup");mqttWsConnectStop(&P);
}
static void mqttUriDeadline(xnetportbackend B)
{
    for(unsigned Mode=0;Mode<2;++Mode){mqttWsConnectPeer P;mqttWsConnectInit(&P,false,XMQTT_VERSION_5,B);char Authority[80],URI[160];int A=snprintf(Authority,sizeof(Authority),"127.0.0.1:%u",(unsigned)P.Base.Address.Port);int N=snprintf(URI,sizeof(URI),"ws://%s/mqtt?x=%%00",Authority);P.ExpectedHost=(xstrview){Authority,(size_t)A};mqttUriGate G;mqttUriGateStart(&P,&G);xmqttclientconfig K;xmqttClientConfigInit(&K);K.SetupTimeout=Mode?5000:25;xmqttclient* C=NULL;testRequire(mqttUriStart(&P,false,(xstrview){URI,(size_t)N},&K,NULL,&C),"original URI deadline constructor");xfuture* Ready=xmqttClientReady(C),*Sent=NULL;
        if(Mode){xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("ws/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");testRequire(xmqttClientPublishAsync(C,&Q,25,&Sent)==XMQTT_ADMISSION_ACCEPTED,"URI queued request original deadline");}
        xrtSleep(60);xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);testRequire(((const xmqttreadyresult*)mqttWsFuture(Ready))->Status==(Mode?XMQTT_STATUS_OK:XMQTT_STATUS_TIMEOUT),"URI API entry clock is never refreshed");
        if(Mode){const xmqttwriteresult* R=mqttWsFuture(Sent);testRequire(R->Status==XMQTT_STATUS_TIMEOUT&&!R->AcceptedBytes&&R->Delivery==XMQTT_NOT_SENT,"URI setup queue does not refresh request clock");}else testRequire(!xrtAtomic32Load(&P.Header,XMEMORY_ACQUIRE),"expired URI never sends HTTP");mqttWsCloseClient(C);mqttWsConnectStop(&P);xrtFutureDestroy(Ready);xrtFutureDestroy(Sent);}
}
static void mqttUriDns(bool TLS,xnetportbackend B)
{
    for(unsigned Mode=0;Mode<3;++Mode){mqttWsConnectPeer P;mqttWsConnectInit(&P,TLS,XMQTT_VERSION_5,B);P.BlockDns=true;char URI[128];int N=snprintf(URI,sizeof(URI),"%s://ex%%61mple.com:%u/mqtt?x=%%00",TLS?"wss":"ws",(unsigned)P.Base.Address.Port);xmqttclientconfig K;xmqttClientConfigInit(&K);K.SetupTimeout=Mode?5000:60;xmqttclient* C=NULL;testRequire(mqttUriStart(&P,TLS,(xstrview){URI,(size_t)N},&K,NULL,&C),"URI detached DNS constructor");memset(URI,0,sizeof(URI));xfuture* F=xmqttClientReady(C);mqttWsWait(&P.DnsEntered,1,"URI copied decoded route enters host lookup");if(Mode==1)testRequire(xrtFutureCancel(F),"URI cancel during DNS");if(Mode==2)testRequire(xmqttClientClose(C),"URI close during DNS");testRequire(((const xmqttreadyresult*)mqttWsFuture(F))->Status==(Mode==0?XMQTT_STATUS_TIMEOUT:Mode==1?XMQTT_STATUS_CANCELLED:XMQTT_STATUS_CLOSED),"original URI DNS outcome");mqttWsCloseClient(C);testRequire(!xrtAtomic32Load(&P.Header,XMEMORY_ACQUIRE),"stopped URI lookup cannot later send HTTP");mqttWsConnectStop(&P);xrtFutureDestroy(F);}
}
static void mqttUriAllocationParity(xnetportbackend B)
{
    mqttWsConnectPeer P;mqttWsConnectInit(&P,false,XMQTT_VERSION_5,B);mqttUriGate G;mqttUriGateStart(&P,&G);char Authority[80],URI[160];int A=snprintf(Authority,sizeof(Authority),"127.0.0.1:%u",(unsigned)P.Base.Address.Port);int N=snprintf(URI,sizeof(URI),"ws://%s/mqtt?x=%%00",Authority);xmqttclientwsconnectconfig W;xmqttClientWsConnectConfigInit(&W);W.Host=(xstrview){Authority,(size_t)A};W.Target=XRT_STR_LITERAL("/mqtt?x=%00");xmqttclientconfig K;xmqttClientConfigInit(&K);K.SetupTimeout=0;char Key[XWS_KEY_CAPACITY];testRequire(xrtWsKeyGenerate(Key,sizeof(Key)),"warm native RNG before measured constructors");xmqttclient *Direct=NULL,*URIClient=NULL;xmemstats A0,A1,A2;xrtMemStatsEnable(true);xrtMemStatsGet(&A0);testRequire(xmqttClientWsConnect(P.Base.Engine,&P.Base.Address,1,&K,&W,&P.Base.Connect,&Direct),"same-input numeric constructor baseline");xrtMemStatsGet(&A1);testRequire(mqttUriStart(&P,false,(xstrview){URI,(size_t)N},&K,&W,&URIClient),"same-input numeric URI constructor");xrtMemStatsGet(&A2);xrtMemStatsEnable(false);
    testRequire(A1.BlockAllocCalls-A0.BlockAllocCalls==A2.BlockAllocCalls-A1.BlockAllocCalls&&A1.BlockAllocBytes-A0.BlockAllocBytes==A2.BlockAllocBytes-A1.BlockAllocBytes,"URI adaptation adds no constructor heap allocation or bytes");xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);xfuture* F=xmqttClientReady(Direct),*U=xmqttClientReady(URIClient);testRequire(((const xmqttreadyresult*)mqttWsFuture(F))->Status==XMQTT_STATUS_TIMEOUT&&((const xmqttreadyresult*)mqttWsFuture(U))->Status==XMQTT_STATUS_TIMEOUT,"allocation control preserves original zero setup deadline");mqttWsCloseClient(Direct);mqttWsCloseClient(URIClient);mqttWsConnectStop(&P);xrtFutureDestroy(F);xrtFutureDestroy(U);
}
static void mqttUriBudget(bool TLS,xnetportbackend B)
{
    const size_t Limits[]={1,128,2048,4096,8192,16384,65536,262144,1048576};bool Failed=false,Passed=false;
    for(size_t I=0;I<sizeof(Limits)/sizeof(*Limits);++I){mqttWsConnectPeer P;mqttWsConnectInit(&P,TLS,XMQTT_VERSION_5,B);char Authority[80],URI[160];int A=snprintf(Authority,sizeof(Authority),"ex%%61mple.com:%u",(unsigned)P.Base.Address.Port);int N=snprintf(URI,sizeof(URI),"%s://%s/mqtt?x=%%00",TLS?"wss":"ws",Authority);P.ExpectedHost=(xstrview){Authority,(size_t)A};xmqttclient* C=(xmqttclient*)(uintptr_t)17;xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[I],&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"URI constructor budget scope");bool OK=mqttUriStart(&P,TLS,(xstrview){URI,(size_t)N},NULL,NULL,&C);testRequire(xrtAllocationBudgetLeave(&Scope),"leave URI constructor budget");if(OK){Passed=true;xfuture* F=xmqttClientReady(C);(void)mqttWsFuture(F);mqttWsCloseClient(C);xrtFutureDestroy(F);}else{Failed=true;testRequire(C==(xmqttclient*)(uintptr_t)17,"URI OOM preserves caller output");}mqttWsConnectStop(&P);xrtClearError();xallocationbudgetinfo Info={.Size=sizeof(Info)};testRequire(xrtAllocationBudgetInfo(Budget,&Info)&&!Info.LiveBytes&&!Info.LiveCount,"URI budget credits balanced after physical host joins");xrtAllocationBudgetFree(Budget);}
    testRequire(Failed&&Passed,"URI budget ladder has actual failures and successful controls");
}
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);xnetportbackend B=Argc>1&&!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:Argc>1&&!strcmp(Argv[1],"epoll")?XNET_PORT_EPOLL:XNET_PORT_SELECT;mqttUriInvalid(B);mqttUriDeadline(B);mqttUriAllocationParity(B);
    for(unsigned TLS=0;TLS<
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
        2
#else
        1
#endif
        ;++TLS){mqttUriDns(TLS!=0,B);mqttUriBudget(TLS!=0,B);}puts("[PASS] URI atomicity, original clocks, detached DNS, allocation parity/OOM and real joins");return 0;
}
