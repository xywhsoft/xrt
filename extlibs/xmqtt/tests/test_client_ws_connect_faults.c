#include "support/ws_connect_peer.h"
static xmqttclientwsconnectconfig wsFaultPolicy(void)
{xmqttclientwsconnectconfig W;xmqttClientWsConnectConfigInit(&W);W.Host=XRT_STR_LITERAL("example.com");W.Target=XRT_STR_LITERAL("/mqtt?x=%00");return W;}
static void wsFaultReady(xmqttclient* C,xmqttclientstatus Expected)
{xfuture* F=xmqttClientReady(C);const xmqttreadyresult* R=mqttWsFuture(F);if(R->Status!=Expected)fprintf(stderr,"HTTP fault expected%d actual%d\n",Expected,R->Status);testRequire(R->Status==Expected,"typed HTTP setup outcome");xrtFutureDestroy(F);}
static void wsHandshakeFailures(bool TLS,xnetportbackend B)
{
    for(unsigned Mode=3;Mode<10;++Mode){mqttWsConnectPeer P;mqttWsConnectInit(&P,TLS,XMQTT_VERSION_5,B);P.Mode=Mode;P.SplitResponse=(Mode&1)!=0;xmqttclientwsconnectconfig W=wsFaultPolicy();xmqttclient* C=wsConnectStart(&P,0,NULL,&W);wsFaultReady(C,Mode<5?XMQTT_STATUS_REJECTED:XMQTT_STATUS_PROTOCOL);xmqttclientwsconnectinfo I={.Size=sizeof(I)};testRequire(xmqttClientWsConnectInfo(C,&I)&&!I.Upgraded&&I.Status==(Mode==3?401:Mode==4?302:101)&&I.RequestBytes,"retained HTTP rejection facts");testRequire(!xrtAtomic32Load(&P.Base.Publishes,XMEMORY_ACQUIRE),"no MQTT publication before checked Upgrade");mqttWsCloseClient(C);mqttWsConnectStop(&P);}
}
static void wsSetupAbort(bool TLS,xnetportbackend B)
{
    for(unsigned Mode=0;Mode<4;++Mode){mqttWsConnectPeer P;mqttWsConnectInit(&P,TLS,XMQTT_VERSION_5,B);P.HoldResponse=true;xmqttclientconfig K;xmqttClientConfigInit(&K);K.SetupTimeout=Mode==0?80:5000;xmqttclientwsconnectconfig W=wsFaultPolicy();W.MaxInformationalResponses=0;xmqttclient* C=wsConnectStart(&P,0,&K,&W);xfuture* Ready=xmqttClientReady(C);/* The original short deadline may expire during TLS under instrumentation.
         * Only cancellation/queue controls require observing HTTP first. */
        if(Mode)mqttWsWait(&P.Header,1,"request completed before held Upgrade");
        if(Mode==1)testRequire(xrtFutureCancel(Ready),"cancel during HTTP Upgrade");
        if(Mode==2)testRequire(xmqttClientClose(C),"close during HTTP Upgrade");
        if(Mode==3){xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("ws/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");xfuture* F=NULL;testRequire(xmqttClientPublishAsync(C,&Q,25,&F)==XMQTT_ADMISSION_ACCEPTED,"request queues with original deadline during HTTP");const xmqttwriteresult* R=mqttWsFuture(F);testRequire(R->Status==XMQTT_STATUS_TIMEOUT&&!R->AcceptedBytes&&R->Delivery==XMQTT_NOT_SENT,"request setup queue cannot refresh its deadline");xrtFutureDestroy(F);
            testRequire(xrtNetPostInit(&P.ReplyPost)&&xrtNetPost(xrtNetEngineWorker(P.Base.Engine,0),&P.ReplyPost,mqttWsConnectRespond,&P),"release real Upgrade response");testRequire(((const xmqttreadyresult*)mqttWsFuture(Ready))->Status==XMQTT_STATUS_OK,"timed-out queued request leaves setup healthy");}
        else testRequire(((const xmqttreadyresult*)mqttWsFuture(Ready))->Status==(Mode==0?XMQTT_STATUS_TIMEOUT:Mode==1?XMQTT_STATUS_CANCELLED:XMQTT_STATUS_CLOSED),"original setup timeout/cancel/close");
        mqttWsCloseClient(C);mqttWsConnectStop(&P);xrtFutureDestroy(Ready);
    }
}
static void wsDnsAbort(bool TLS,xnetportbackend B)
{
    for(unsigned Mode=0;Mode<3;++Mode){mqttWsConnectPeer P;mqttWsConnectInit(&P,TLS,XMQTT_VERSION_5,B);P.BlockDns=true;xmqttclientconfig K;xmqttClientConfigInit(&K);K.SetupTimeout=Mode==0?40:5000;xmqttclientwsconnectconfig W=wsFaultPolicy();xmqttclient* C=wsConnectStart(&P,1,&K,&W);xfuture* F=xmqttClientReady(C);mqttWsWait(&P.DnsEntered,1,"original WS DNS job entered");if(Mode==1)testRequire(xrtFutureCancel(F),"cancel original DNS setup");if(Mode==2)testRequire(xmqttClientClose(C),"close original DNS setup");testRequire(((const xmqttreadyresult*)mqttWsFuture(F))->Status==(Mode==0?XMQTT_STATUS_TIMEOUT:Mode==1?XMQTT_STATUS_CANCELLED:XMQTT_STATUS_CLOSED),"whole setup original DNS outcome");mqttWsCloseClient(C);testRequire(!xrtAtomic32Load(&P.Header,XMEMORY_ACQUIRE),"no network HTTP after canceled lookup");mqttWsConnectStop(&P);xrtFutureDestroy(F);}
}
static void wsHttpBounds(bool TLS,xnetportbackend B)
{
    for(unsigned Mode=0;Mode<4;++Mode){mqttWsConnectPeer P;mqttWsConnectInit(&P,TLS,XMQTT_VERSION_5,B);P.Mode=Mode==0?1:0;xmqttclientwsconnectconfig W=wsFaultPolicy();xmqttclientconfig K;xmqttClientConfigInit(&K);
        if(Mode==0)W.MaxInformationalResponses=0;
        else if(Mode==1){W.Head.MaxHead=W.Head.MaxStartLine=W.Head.MaxFieldLine=64;}
        else if(Mode==2)W.Head.MaxFields=1;
        else {K.Stream.WriteLimit=K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;}
        xmqttclient* C=wsConnectStart(&P,0,&K,&W);xfuture* F=xmqttClientReady(C);const xmqttreadyresult* R=mqttWsFuture(F);testRequire(R->Status==XMQTT_STATUS_PROTOCOL||R->Status==XMQTT_STATUS_LIMIT,"bounded interim/head/field/native-cap failure");mqttWsCloseClient(C);mqttWsConnectStop(&P);xrtFutureDestroy(F);}
}
static void wsConstructBudget(bool TLS,xnetportbackend B)
{
    static const size_t Limits[]={1,128,2048,4096,8192,16384,65536,262144,1048576};bool Failed=false,Passed=false;
    for(size_t N=0;N<sizeof(Limits)/sizeof(*Limits);++N){mqttWsConnectPeer P;mqttWsConnectInit(&P,TLS,XMQTT_VERSION_5,B);xmqttclientwsconnectconfig W=wsFaultPolicy();xmqttclient* C=(xmqttclient*)(uintptr_t)1;xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[N],&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"WS setup constructor allocation scope");bool OK;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
        if(TLS){xmqttclienttlsconfig T;xmqttClientTlsConfigInit(&T);T.Session.ResumeLimit=0;T.Session.Context=P.Base.Context;T.Session.Verifier=P.Base.Verifier;T.Session.ServerName=XRT_STR_LITERAL("example.com");OK=xmqttClientWssConnect(P.Base.Engine,&P.Base.Address,1,NULL,&W,&T,&P.Base.Connect,&C);}else
#endif
        OK=xmqttClientWsConnect(P.Base.Engine,&P.Base.Address,1,NULL,&W,&P.Base.Connect,&C);
        testRequire(xrtAllocationBudgetLeave(&Scope),"leave WS setup constructor allocation scope");if(OK){Passed=true;xfuture* F=xmqttClientReady(C);(void)mqttWsFuture(F);mqttWsCloseClient(C);xrtFutureDestroy(F);}else{Failed=true;testRequire(C==(xmqttclient*)(uintptr_t)1,"failed constructor leaves output unchanged");}
        mqttWsConnectStop(&P);xrtClearError();xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(xrtAllocationBudgetInfo(Budget,&I)&&!I.LiveBytes&&!I.LiveCount,"all WS setup budget credits retired after actual host joins");xrtAllocationBudgetFree(Budget);
    }testRequire(Failed&&Passed,"real failure prefixes and successful allocation controls");
}
#if defined(XMQTT_CONNECT_SINGLE_TEST)
typedef struct wsHttpAlias {xnetpost Post;xmqttclient* Client;ptr Alias;bool TLS;xatomic32 Done;} wsHttpAlias;
static void wsHttpAliasHold(xnetworker* W,ptr Data)
{
    (void)W;wsHttpAlias* T=Data;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
    if(T->TLS)T->Alias=xrtTlsStreamRef(T->Client->TLS->Stream);else
#endif
    {T->Alias=xrtNetStreamRef(T->Client->Stream);}testRequire(T->Alias!=NULL,"held actual setup native alias");xrtAtomic32Store(&T->Done,1,XMEMORY_RELEASE);
}
static void wsSetupAlias(bool TLS,xnetportbackend B)
{
    mqttWsConnectPeer P;mqttWsConnectInit(&P,TLS,XMQTT_VERSION_5,B);P.HoldResponse=true;xmqttclientwsconnectconfig W=wsFaultPolicy();xmqttclient* C=wsConnectStart(&P,0,NULL,&W);mqttWsWait(&P.Header,1,"held setup native source");wsHttpAlias T={.Client=C,.TLS=TLS};xrtAtomic32Init(&T.Done,0);testRequire(xrtNetPostInit(&T.Post)&&xrtNetPost(C->Worker,&T.Post,wsHttpAliasHold,&T),"hold setup native alias on owner");mqttWsWait(&T.Done,1,"setup alias captured");testRequire(xmqttClientClose(C)&&xmqttClientWaitFor(C,30)==XWAIT_TIMEOUT,"HTTP failure cannot bypass raw/TLS alias retirement");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
    if(TLS)xrtTlsStreamDestroy(T.Alias);else
#endif
    {xrtNetStreamDestroy(T.Alias);}mqttWsCloseClient(C);mqttWsConnectStop(&P);
}
#endif
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);xnetportbackend B=Argc>1&&!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:Argc>1&&!strcmp(Argv[1],"epoll")?XNET_PORT_EPOLL:XNET_PORT_SELECT;
    for(unsigned TLS=0;TLS<
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
        2
#else
        1
#endif
        ;++TLS){printf("[faults] TLS%u\n",TLS);wsHandshakeFailures(TLS!=0,B);wsSetupAbort(TLS!=0,B);wsDnsAbort(TLS!=0,B);wsHttpBounds(TLS!=0,B);wsConstructBudget(TLS!=0,B);
#if defined(XMQTT_CONNECT_SINGLE_TEST)
        wsSetupAlias(TLS!=0,B);
#endif
    }puts("[PASS] WS/WSS HTTP failure/deadline/DNS/queue/bounds/constructor OOM/native lifetime");return 0;
}
