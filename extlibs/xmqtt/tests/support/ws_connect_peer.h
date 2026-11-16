#ifndef XMQTT_TEST_WS_CONNECT_PEER_H
#define XMQTT_TEST_WS_CONNECT_PEER_H
#include "ws_peer.h"
#include <xmqtt/client_ws_connect.h>
typedef struct mqttWsConnectPeer {
    mqttWsPeer Base;char UriAuthority[128];xstrview ExpectedHost,ExpectedTarget,ExpectedLookup;xnetresolver* Resolver;unsigned Mode;bool SplitResponse,HoldResponse,BlockDns;
    xatomic32 Header,Attached,Cleanup,DnsEntered,DnsRelease;bool Responded;char Response[1024];size_t ResponseBytes;
    xnetpost UpgradePost,CleanupPost,ReplyPost;unsigned char HttpInput[16384];size_t HttpUsed;
} mqttWsConnectPeer;
static inline xnetaddrlist* mqttWsConnectLookup(cstr Host,xnetfamily Family,ptr Data)
{
    (void)Family;mqttWsConnectPeer* P=Data;testRequire(strlen(Host)==P->ExpectedLookup.Size&&!memcmp(Host,P->ExpectedLookup.Data,P->ExpectedLookup.Size),"counted WS DNS host snapshot");xrtAtomic32Store(&P->DnsEntered,1,XMEMORY_RELEASE);if(P->BlockDns)mqttWsWait(&P->DnsRelease,1,"release independent WS DNS job");xnetaddr A=P->Base.Address;A.Port=0;return xrtNetAddrListCreate(&A,1);
}
static inline bool mqttWsConnectSend(mqttWsConnectPeer* P,const void* Data,size_t N)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
    if(P->Base.TLS){size_t Sent=0;return xrtTlsStreamSend(P->Base.Native[1],Data,N,&Sent)==XTLS_OK&&Sent==N;}
#endif
    return xrtNetStreamSend(P->Base.Native[1],Data,N)==XNET_RESULT_OK;
}
static inline void mqttWsConnectAttach(xnetworker* W,ptr Data)
{(void)W;mqttWsConnectPeer* P=Data;xwsstreamconfig K;xrtWsStreamConfigInit(&K);K.Role=XWS_ROLE_SERVER;K.Protocol=XRT_STR_LITERAL("mqtt");xwsstream* S;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
    if(P->Base.TLS)S=xrtWsStreamAttachTls(P->Base.Native[1],0,&K,&MqttWsPeerEvents,&P->Base);else
#endif
    S=xrtWsStreamAttach(P->Base.Native[1],0,&K,&MqttWsPeerEvents,&P->Base);
    /* A rejecting/cancelled Client may have closed before this queued handoff.
     * Failure consumes no native reference; normal fixture cleanup still owns it. */
    if(!S){xrtClearError();return;}P->Base.Native[1]=NULL;xrtAtomicPtrStore(&P->Base.Server,S,XMEMORY_RELEASE);xrtAtomic32Store(&P->Attached,1,XMEMORY_RELEASE);}
static inline void mqttWsConnectRespond(xnetworker* W,ptr Data)
{
    (void)W;mqttWsConnectPeer* P=Data;testRequire(!P->Responded,"one HTTP response transaction");P->Responded=true;
    if(P->SplitResponse){for(size_t I=0;I<P->ResponseBytes;++I)testRequire(mqttWsConnectSend(P,P->Response+I,1),"literal split HTTP response");}
    else testRequire(mqttWsConnectSend(P,P->Response,P->ResponseBytes),"literal HTTP response");
    if(P->Mode==0||P->Mode==1||P->Mode==2){static const unsigned char Ping[]={0x89,0};testRequire(mqttWsConnectSend(P,Ping,sizeof(Ping)),"HTTP/read-ahead WS control bytes");testRequire(xrtNetPostInit(&P->UpgradePost)&&xrtNetPost(xrtNetEngineWorker(P->Base.Engine,0),&P->UpgradePost,mqttWsConnectAttach,P),"leave HTTP/TLS driver before WS attach");}
}
static inline void mqttWsConnectHeader(mqttWsConnectPeer* P,const xnetbuf* B)
{
    if(P->Responded||xrtAtomic32Load(&P->Header,XMEMORY_ACQUIRE))return;
    size_t Old=P->HttpUsed,N=xrtNetBufPeek(B,0,P->HttpInput+Old,sizeof(P->HttpInput)-Old);P->HttpUsed+=N;xhttpfield Fields[64];xhttp1head H={.Fields=Fields,.FieldCapacity=64};xhttp1status R=xrtHttp1RequestParse((xbytesview){P->HttpInput,P->HttpUsed},&H,NULL,NULL);
    if(R==XHTTP1_MORE){testRequire(P->HttpUsed<sizeof(P->HttpInput),"bounded fixture partial HTTP input");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
        if(P->Base.TLS){testRequire(xrtTlsStreamConsume(P->Base.Native[1],N),"advance fixture TLS plaintext record");return;}
#endif
        testRequire(xrtNetStreamConsume(P->Base.Native[1],N)==N,"advance fixture TCP HTTP input");return;}
    testRequire(R==XHTTP1_READY&&H.Bytes>=Old,"strict HTTP request parse");
    testRequire(H.Target.Size==P->ExpectedTarget.Size&&!memcmp(H.Target.Data,P->ExpectedTarget.Data,H.Target.Size),"exact HTTP target snapshot");
    const xhttpfield* Host=xrtHttp1Field(&H,XRT_STR_LITERAL("Host"));const xhttpfield* Protocol=xrtHttp1Field(&H,XRT_STR_LITERAL("Sec-WebSocket-Protocol"));const xhttpfield* Key=xrtHttp1Field(&H,XRT_STR_LITERAL("Sec-WebSocket-Key"));
    testRequire(Host&&Host->Value.Size==P->ExpectedHost.Size&&!memcmp(Host->Value.Data,P->ExpectedHost.Data,Host->Value.Size)&&Protocol&&Protocol->Value.Size==4&&!memcmp(Protocol->Value.Data,"mqtt",4)&&Key,"exact authority/protocol and native fresh key");
    char Accept[XWS_ACCEPT_CAPACITY];testRequire(xrtWsAccept(Key->Value,Accept,sizeof(Accept)),"response accept digest");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
    if(P->Base.TLS)testRequire(xrtTlsStreamConsume(P->Base.Native[1],H.Bytes-Old),"consume only HTTP request head");else
#endif
    testRequire(xrtNetStreamConsume(P->Base.Native[1],H.Bytes-Old)==H.Bytes-Old,"consume HTTP request prefix");
    const char* Extra=P->Mode==1?"HTTP/1.1 103 Early Hints\r\nLink: </x>\r\n\r\n":P->Mode==2?"HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 103 Early Hints\r\n\r\n":"";
    int Count=snprintf(P->Response,sizeof(P->Response),"%sHTTP/1.1 %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n%s\r\n",Extra,P->Mode==3?"401 Unauthorized":P->Mode==4?"302 Found":"101 Switching Protocols",P->Mode==5?"aaaaaaaaaaaaaaaaaaaaaaaaaaaa":Accept,P->Mode==6?"":P->Mode==7?"Sec-WebSocket-Protocol: wrong\r\n":P->Mode==8?"Sec-WebSocket-Protocol: mqtt\r\nContent-Length: 0\r\n":P->Mode==9?"Sec-WebSocket-Protocol: mqtt\r\nSec-WebSocket-Extensions: permessage-deflate\r\n":"Sec-WebSocket-Protocol: mqtt\r\n");testRequire(Count>0&&(size_t)Count<sizeof(P->Response),"bounded literal HTTP response");P->ResponseBytes=(size_t)Count;
    xrtAtomic32Store(&P->Header,1,XMEMORY_RELEASE);if(!P->HoldResponse)mqttWsConnectRespond(NULL,P);
}
static inline void mqttWsConnectTcpRead(xnetstream* S,xnetbuf* B,ptr Data)
{(void)S;mqttWsConnectHeader(Data,B);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
static inline void mqttWsConnectTlsRead(xtlsstream* S,const xnetbuf* B,ptr Data)
{(void)S;mqttWsConnectHeader(Data,B);}
static const xtlsstreamevents MqttWsConnectTlsEvents={.Read=mqttWsConnectTlsRead};
#endif
static const xnetstreamevents MqttWsConnectTcpEvents={.Read=mqttWsConnectTcpRead};
static inline void mqttWsConnectNative(mqttWsPeer* Base,ptr Native)
{
    mqttWsConnectPeer* P=(mqttWsConnectPeer*)Base;Base->Native[1]=Native;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
    if(Base->TLS){testRequire(xrtTlsStreamSetEvents(Native,&MqttWsConnectTlsEvents,P),"server HTTP TLS events");if(xrtTlsStreamAvailable(Native))mqttWsConnectHeader(P,xrtTlsStreamBuffer(Native));return;}
#endif
    testRequire(xrtNetStreamSetEvents(Native,&MqttWsConnectTcpEvents,P),"server HTTP TCP events");if(xrtNetStreamAvailable(Native))mqttWsConnectHeader(P,xrtNetStreamBuffer(Native));
}
static inline void mqttWsConnectInit(mqttWsConnectPeer* P,bool TLS,xmqttversion V,xnetportbackend B)
{
    memset(P,0,sizeof(*P));P->ExpectedHost=P->ExpectedLookup=XRT_STR_LITERAL("example.com");P->ExpectedTarget=XRT_STR_LITERAL("/mqtt?x=%00");mqttWsInit(&P->Base,TLS,V,B);P->Base.NativeServerOpen=mqttWsConnectNative;xrtAtomic32Init(&P->Header,0);xrtAtomic32Init(&P->Attached,0);xrtAtomic32Init(&P->Cleanup,0);xrtAtomic32Init(&P->DnsEntered,0);xrtAtomic32Init(&P->DnsRelease,0);xnetresolverconfig R;xrtNetResolverConfigInit(&R);R.Workers=1;R.Lookup=mqttWsConnectLookup;R.LookupData=P;P->Resolver=xrtNetResolverCreate(&R);testRequire(P->Resolver!=NULL,"WS host resolver");
}
static inline void mqttWsConnectCleanup(xnetworker* W,ptr Data)
{
    (void)W;mqttWsConnectPeer* P=Data;xwsstream* S=xrtAtomicPtrLoad(&P->Base.Server,XMEMORY_ACQUIRE);
    if(S){xrtWsStreamAbort(S);xrtWsStreamDestroy(S);}else if(P->Base.Native[1]){
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
        if(P->Base.TLS){xrtTlsStreamAbort(P->Base.Native[1]);xrtTlsStreamDestroy(P->Base.Native[1]);}else
#endif
        {xrtNetStreamAbort(P->Base.Native[1]);xrtNetStreamDestroy(P->Base.Native[1]);}P->Base.Native[1]=NULL;}
    xrtAtomic32Store(&P->Cleanup,1,XMEMORY_RELEASE);
}
static inline void mqttWsConnectStop(mqttWsConnectPeer* P)
{
    xrtAtomic32Store(&P->DnsRelease,1,XMEMORY_RELEASE);testRequire(xrtNetResolverDestroy(P->Resolver),"join detached native DNS job");testRequire(xrtNetPostInit(&P->CleanupPost)&&xrtNetPost(xrtNetEngineWorker(P->Base.Engine,0),&P->CleanupPost,mqttWsConnectCleanup,P),"stop real fixture native owner");mqttWsWait(&P->Cleanup,1,"fixture owner cleanup");xrtNetListenerClose(P->Base.Listener);mqttWsWait(&P->Base.ListenerClosed,1,"HTTP listener close");xrtNetListenerDestroy(P->Base.Listener);testRequire(xrtNetEngineDestroy(P->Base.Engine),"HTTP/WS/TLS/TCP actual engine join");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
    xrtTlsVerifierRelease(P->Base.Verifier);xrtTlsIdentityRelease(P->Base.Identity);xrtTlsContextRelease(P->Base.Context);
#endif
    free(P->Base.Input);
}
static inline xmqttclient* wsConnectStart(mqttWsConnectPeer* P,unsigned Route,const xmqttclientconfig* Cfg,const xmqttclientwsconnectconfig* Policy)
{
    xmqttclient* C=NULL;bool OK;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
    xmqttclienttlsconfig TLS;if(P->Base.TLS){xmqttClientTlsConfigInit(&TLS);TLS.Session.ResumeLimit=0;TLS.Session.Context=P->Base.Context;TLS.Session.Verifier=P->Base.Verifier;TLS.Session.ServerName=XRT_STR_LITERAL("example.com");
        if(Route==0)OK=xmqttClientWssConnect(P->Base.Engine,&P->Base.Address,1,Cfg,Policy,&TLS,&P->Base.Connect,&C);
        else if(Route==1)OK=xmqttClientWssDial(P->Base.Engine,P->Resolver,XRT_STR_LITERAL("example.com"),P->Base.Address.Port,1,Cfg,Policy,&TLS,&P->Base.Connect,&C);
        else {xnetaddrlist* L=xrtNetAddrListCreate(&P->Base.Address,1);testRequire(L!=NULL,"WSS prepared addresses");OK=xmqttClientWssDialAddresses(P->Base.Engine,L,1,Cfg,Policy,&TLS,&P->Base.Connect,&C);xrtNetAddrListDestroy(L);}
    }else
#endif
    {
        if(Route==0)OK=xmqttClientWsConnect(P->Base.Engine,&P->Base.Address,1,Cfg,Policy,&P->Base.Connect,&C);
        else if(Route==1)OK=xmqttClientWsDial(P->Base.Engine,P->Resolver,XRT_STR_LITERAL("example.com"),P->Base.Address.Port,1,Cfg,Policy,&P->Base.Connect,&C);
        else {xnetaddrlist* L=xrtNetAddrListCreate(&P->Base.Address,1);testRequire(L!=NULL,"WS prepared addresses");OK=xmqttClientWsDialAddresses(P->Base.Engine,L,1,Cfg,Policy,&P->Base.Connect,&C);xrtNetAddrListDestroy(L);}
    }
    if(!OK){fprintf(stderr,"WS connect factory: %s\n",xrtGetError()?xrtErrorMessage(xrtGetError()):"none");}testRequire(OK&&C,"ordinary WS connect factory");return C;
}

#endif
