#include "support/ws_connect_peer.h"
#include <xmqtt/client_ws_uri.h>
typedef struct mqttUriGate {xnetpost Post;xatomic32 Entered,Permit;} mqttUriGate;
static void mqttUriGateRun(xnetworker* W,ptr Data)
{(void)W;mqttUriGate* G=Data;xrtAtomic32Store(&G->Entered,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&G->Permit,XMEMORY_ACQUIRE))xrtSleep(1);}
static void mqttUriGateStart(mqttWsConnectPeer* P,mqttUriGate* G)
{xrtAtomic32Init(&G->Entered,0);xrtAtomic32Init(&G->Permit,0);testRequire(xrtNetPostInit(&G->Post)&&xrtNetPost(xrtNetEngineWorker(P->Base.Engine,1),&G->Post,mqttUriGateRun,G),"URI owner barrier");mqttWsWait(&G->Entered,1,"owner held before URI constructor");}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
typedef struct mqttUriTrust {mqttWsConnectPeer* Peer;xstrview Identity,SNI;xatomic32 Verified,Selected;} mqttUriTrust;
static xtlsverifydecision mqttUriVerify(const xtlspeer* Peer,ptr Data)
{mqttUriTrust* T=Data;xbytesview Pin={0};bool OK=Peer&&Peer->CertificateCount&&Peer->Name.Size==T->Identity.Size&&!memcmp(Peer->Name.Data,T->Identity.Data,Peer->Name.Size)&&xrtTlsIdentityCertificate(T->Peer->Base.Identity,0,&Pin)&&Peer->Certificates[0].Raw.Size==Pin.Size&&!memcmp(Peer->Certificates[0].Raw.Data,Pin.Data,Pin.Size);xrtAtomic32FetchAdd(&T->Verified,1,XMEMORY_RELEASE);return OK?XTLS_VERIFY_ACCEPT:XTLS_VERIFY_REJECT;}
static bool mqttUriSelect(ptr Data,const xtlsserverrequest* R,xtlsserverchoice* C)
{(void)C;mqttUriTrust* T=Data;testRequire(R->ServerName.Size==T->SNI.Size&&(!T->SNI.Size||!memcmp(R->ServerName.Data,T->SNI.Data,T->SNI.Size)),"actual wire SNI matches independent URI policy");xrtAtomic32FetchAdd(&T->Selected,1,XMEMORY_RELEASE);return true;}
#endif
static void mqttUriRoundtrip(bool TLS,xmqttversion V,unsigned Mode,xnetportbackend B)
{
    mqttWsConnectPeer P;mqttWsConnectInit(&P,TLS,V,B);bool Numeric=Mode==0||Mode==7||Mode==8;
    const char* Host=Mode==0?"127.0.0.1":Mode==2?"ex%61mple.com":Mode==6?"caf%C3%A9.example":Mode==7?"%31%32%37.0.0.1":Mode==8?"[::1]":Mode==12?"ex%2561mple.com":"example.com";
    const char* Suffix=Mode==3?"?x=%2f&v=%00":Mode==4?"/mqtt?":Mode==5?"//mqtt/%23?x=1?2":Mode==9?"":"/mqtt?x=%00";
    const char* Target=Mode==3?"/?x=%2f&v=%00":Mode==4?"/mqtt":Mode==9?"/":Suffix;P.ExpectedTarget=(xstrview){Target,strlen(Target)};
    if(Mode==6){P.ExpectedLookup=XRT_STR_LITERAL("caf\xc3\xa9.example");}if(Mode==12){P.ExpectedLookup=XRT_STR_LITERAL("ex%61mple.com");}
    if(Mode==8){testRequire(xrtNetListenerClose(P.Base.Listener),"close empty IPv4 listener");mqttWsWait(&P.Base.ListenerClosed,1,"old listener physical close");xrtNetListenerDestroy(P.Base.Listener);xrtAtomic32Store(&P.Base.ListenerClosed,0,XMEMORY_RELEASE);xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV6,0);L.Affinity=0;L.Distribution=XNET_ACCEPT_LOCAL;xnetlistenerevents E={mqttWsAccept,NULL,mqttWsListenerClosed};P.Base.Listener=xrtNetListen(P.Base.Engine,&L,&E,NULL,&P.Base);testRequire(P.Base.Listener&&xrtNetListenerLocal(P.Base.Listener,&P.Base.Address),"actual IPv6 URI listener");}
    char Authority[128],URI[256];int A=snprintf(Authority,sizeof(Authority),"%s:%u",Host,(unsigned)P.Base.Address.Port);int N=snprintf(URI,sizeof(URI),"%s://%s%s",TLS?"wSs":"Ws",Authority,Suffix);testRequire(A>0&&N>0&&(size_t)N<sizeof(URI),"bounded URI fixture");P.ExpectedHost=(xstrview){Authority,(size_t)A};P.Mode=Mode%3;P.SplitResponse=Mode==2;
    mqttUriGate G;mqttUriGateStart(&P,&G);xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=V;if(Mode==2)K.ReadBytes=K.WriteBytes=K.WriteChunkBytes=K.WorkPackets=1;
    xmqttclientwsconnectconfig W;xmqttClientWsConnectConfigInit(&W);W.Host=XRT_STR_LITERAL("ignored.invalid");W.Target=XRT_STR_LITERAL("/ignored");xmqttclient* C=NULL;bool OK;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
    mqttUriTrust Trust={.Peer=&P,.Identity=Numeric?(Mode==8?XRT_STR_LITERAL("::1"):XRT_STR_LITERAL("127.0.0.1")):XRT_STR_LITERAL("example.com"),.SNI=Numeric?(xstrview){0}:XRT_STR_LITERAL("example.com")};xrtAtomic32Init(&Trust.Verified,0);xrtAtomic32Init(&Trust.Selected,0);xtlsverifier* Verifier=NULL;
    if(TLS){if(Mode==10){Trust.Identity=XRT_STR_LITERAL("manual.fixture");Trust.SNI=(xstrview){0};}if(Mode==11)Trust.SNI=XRT_STR_LITERAL("route.fixture");if(Mode==6||Mode==12){Trust.Identity=XRT_STR_LITERAL("explicit.fixture");Trust.SNI=XRT_STR_LITERAL("route.fixture");}xtlsverifierconfig VC;xrtTlsVerifierConfigInit(&VC);VC.Verify=mqttUriVerify;VC.Context=&Trust;Verifier=xrtTlsVerifierCreate(&VC);testRequire(Verifier!=NULL,"URI pinned identity observer");P.Base.NativeServer.Select=mqttUriSelect;P.Base.NativeServer.SelectContext=&Trust;xmqttclienttlsconfig T;xmqttClientTlsConfigInit(&T);T.Session.Context=P.Base.Context;T.Session.Verifier=Verifier;T.Session.ResumeLimit=0;if(Mode==6||Mode==10||Mode==12){T.Session.ServerName=Trust.SNI;T.Session.VerifyName=Trust.Identity;}if(Mode==10)T.ServerNameFromHost=false;if(Mode==11)T.Session.ServerName=Trust.SNI;OK=xmqttClientWssConnectUri(P.Base.Engine,Numeric?NULL:P.Resolver,(xstrview){URI,(size_t)N},1,&K,Mode==9?NULL:&W,&T,&P.Base.Connect,&C);}else
#endif
    OK=xmqttClientWsConnectUri(P.Base.Engine,Numeric?NULL:P.Resolver,(xstrview){URI,(size_t)N},1,&K,Mode==9?NULL:&W,&P.Base.Connect,&C);
    if(!OK){fprintf(stderr,"URI admission: %s\n",xrtGetError()?xrtErrorMessage(xrtGetError()):"none");}testRequire(OK&&C,"URI factory snapshots before owner runs");memset(URI,'x',(size_t)N);memset(&W,0,sizeof(W));xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
    xrtTlsVerifierRelease(Verifier);
#endif
    xfuture* Ready=xmqttClientReady(C);const xmqttreadyresult* R=mqttWsFuture(Ready);if(R->Status!=XMQTT_STATUS_OK){fprintf(stderr,"URI Ready=%d cause=%s mode=%u tls=%u\n",R->Status,R->Cause?xrtErrorMessage(R->Cause):"none",Mode,TLS);}testRequire(R->Status==XMQTT_STATUS_OK,"URI HTTP/mqtt/CONNACK readiness");
    testRequire(xrtAtomic32Load(&P.DnsEntered,XMEMORY_ACQUIRE)==(Numeric?0u:1u),"numeric URI bypasses Resolver and names use exact decoded route");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
    testRequire(xrtAtomic32Load(&Trust.Verified,XMEMORY_ACQUIRE)==(TLS?1u:0u)&&xrtAtomic32Load(&Trust.Selected,XMEMORY_ACQUIRE)==(TLS?1u:0u),"actual URI certificate identity and wire SNI");
#endif
    for(unsigned Qos=0;Qos<3;++Qos){xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=(uint8)(Qos<<1);Q.Body.Publish.Topic=XRT_STR_LITERAL("ws/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");xfuture* F=NULL;testRequire(xmqttClientPublishAsync(C,&Q,10000,&F)==XMQTT_ADMISSION_ACCEPTED&&((const xmqttwriteresult*)mqttWsFuture(F))->Status==XMQTT_STATUS_OK,"URI established QoS0/1/2 physical outcome");xrtFutureDestroy(F);}
    xfuture* Bye=NULL;testRequire(xmqttClientDisconnectAsync(C,NULL,10000,&Bye)==XMQTT_ADMISSION_ACCEPTED&&((const xmqttwriteresult*)mqttWsFuture(Bye))->Status==XMQTT_STATUS_OK,"URI MQTT/WS/TLS clean shutdown");mqttWsCloseClient(C);mqttWsConnectStop(&P);xrtFutureDestroy(Ready);xrtFutureDestroy(Bye);printf("[PASS] URI TLS%u version%u mode%u routing/authority/identity/resource/snapshot and joins\n",TLS,V,Mode);
}
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);xnetportbackend B=Argc>1&&!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:Argc>1&&!strcmp(Argv[1],"epoll")?XNET_PORT_EPOLL:XNET_PORT_SELECT;
    for(unsigned TLS=0;TLS<
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
        2
#else
        1
#endif
        ;++TLS)for(unsigned V=0;V<2;++V)for(unsigned Mode=0;Mode<13;++Mode)mqttUriRoundtrip(TLS!=0,V?XMQTT_VERSION_311:XMQTT_VERSION_5,Mode,B);
    return 0;
}
