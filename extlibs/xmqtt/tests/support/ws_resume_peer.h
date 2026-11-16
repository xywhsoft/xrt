#ifndef XMQTT_TEST_WS_RESUME_PEER_H
#define XMQTT_TEST_WS_RESUME_PEER_H
#include "ws_connect_peer.h"
#include <xmqtt/client_ws_uri.h>
#include <xmqtt/client_tls_resume.h>
typedef struct mrState {xtlsresume* ServerTicket;xatomic32 Lookups;bool Refuse;} mrState;
typedef struct mrPeer {mqttWsConnectPeer Peer;mrState* State;xatomic32 Configured,Issued,ServerResumed;bool Early,WaitQueued;unsigned Batch;double AttachDeadline;} mrPeer;
static inline const xtlsresume* mrLookup(ptr Data,const xtlsserverresumerequest* Request)
{mrState* S=Data;testRequire(Request&&Request->Ticket.Size,"actual WSS native PSK lookup");xrtAtomic32FetchAdd(&S->Lookups,1,XMEMORY_RELEASE);return S->Refuse?NULL:S->ServerTicket;}
static inline void mrIssueNative(mrPeer* P,xtlsstream* Native)
{xtlsresume* Next=NULL;testRequire(Native&&xrtTlsServerTicketNew(xrtTlsStreamSession(Native),&Next)==XTLS_OK&&Next,"actual WSS native ticket issue");xrtTlsResumeRelease(P->State->ServerTicket);P->State->ServerTicket=Next;testRequire(xrtTlsStreamFlush(Native)==XTLS_OK,"ticket-only native output");xrtAtomic32FetchAdd(&P->Issued,1,XMEMORY_RELEASE);}
static inline void mrEarlyServer(mqttWsPeer* Base,ptr Native)
{mrPeer* P=(mrPeer*)Base;mrIssueNative(P,Native);Base->NativeServerOpen=NULL;mqttWsAttachServer(Base,Native);}
static inline void mrConfigure(xnetworker* W,ptr Data)
{(void)W;mrPeer* P=Data;P->Peer.Base.NativeServer.Resume=mrLookup;P->Peer.Base.NativeServer.ResumeContext=P->State;if(P->Early)P->Peer.Base.NativeServerOpen=mrEarlyServer;xrtAtomic32Store(&P->Configured,1,XMEMORY_RELEASE);}
static inline void mrInit(mrPeer* P,mrState* S,xmqttversion V,unsigned Route,xnetportbackend B)
{memset(P,0,sizeof(*P));P->State=S;P->Early=Route==0;mqttWsConnectInit(&P->Peer,true,V,B);xrtAtomic32Init(&P->Configured,0);xrtAtomic32Init(&P->Issued,0);xrtAtomic32Init(&P->ServerResumed,0);if(Route==0)P->Peer.Base.NativeServerOpen=NULL;testRequire(xrtNetEnginePost(P->Peer.Base.Engine,0,mrConfigure,P),"configure immutable WSS ticket issuer on server owner");mqttWsWait(&P->Configured,1,"WSS issuer configured");}
static inline void mrAttachWhenQueued(xnetworker* W,ptr Data)
{(void)W;mrPeer* P=Data;xtlsstream* Native=P->Peer.Base.Native[0];if(P->WaitQueued&&!xrtTlsClientResumeCount(xrtTlsStreamSession(Native))){testRequire(xrtTimer()<P->AttachDeadline&&xrtNetEnginePost(P->Peer.Base.Engine,1,mrAttachWhenQueued,P),"fixture-only barrier forces a genuinely queued pre-attach credential");return;}mqttWsAttachClient(&P->Peer.Base,Native);}
static inline void mrNativeOpen(xtlsstream* Native,ptr Data)
{mrPeer* P=Data;P->Peer.Base.Native[0]=Native;testRequire(xrtNetEnginePost(P->Peer.Base.Engine,1,mrAttachWhenQueued,P),"leave actual native TLS drive before MQTT/WSS ownership handoff");}
static const xtlsstreamownershipv1 MrNativePolicy={sizeof(xtlsstreamownershipv1),{.Open=mrNativeOpen},mqttWsOwnerDrop,&MqttWsOwnerOps};
static inline xmqttclient* mrOpen(mrPeer* P,unsigned Route,xtlsresume* Resume,bool ResumeOnly,size_t Limit,const xmqttclientconfig* Config,char* Authority,size_t Capacity)
{
    xmqttclient* C=NULL;xmqttclienttlsconfig T;xmqttClientTlsConfigInit(&T);T.Session.Context=P->Peer.Base.Context;T.Session.Verifier=P->Peer.Base.Verifier;T.Session.ServerName=XRT_STR_LITERAL("example.com");T.Session.VerifyName=XRT_STR_LITERAL("example.com");T.Session.Resume=Resume;T.Session.ResumeOnly=ResumeOnly;T.Session.ResumeLimit=Limit;
    if(Route==0){P->WaitQueued=P->Early&&Limit!=0;xtlsclientconfig Native=T.Session;P->Peer.Base.ClientConfig=*Config;P->AttachDeadline=xrtTimer()+10;xrtAtomic32FetchAdd(&P->Peer.Base.Owners,1,XMEMORY_RELEASE);testRequire(xrtTlsStreamConnectOwnedV1(P->Peer.Base.Engine,&P->Peer.Base.Address,1,NULL,&Native,NULL,P,&MrNativePolicy)!=NULL,"actual native WSS attachment factory");mqttWsWait(&P->Peer.Base.Constructed,1,"direct WSS construction");return xrtAtomicPtrLoad(&P->Peer.Base.Client,XMEMORY_ACQUIRE);}
    xmqttclientwsconnectconfig W;xmqttClientWsConnectConfigInit(&W);W.WebSocket=P->Peer.Base.WsConfig;W.Host=XRT_STR_LITERAL("example.com");W.Target=XRT_STR_LITERAL("/mqtt?x=%00");bool OK;
    if(Route==1)OK=xmqttClientWssConnect(P->Peer.Base.Engine,&P->Peer.Base.Address,1,Config,&W,&T,&P->Peer.Base.Connect,&C);
    else if(Route==2)OK=xmqttClientWssDial(P->Peer.Base.Engine,P->Peer.Resolver,XRT_STR_LITERAL("example.com"),P->Peer.Base.Address.Port,1,Config,&W,&T,&P->Peer.Base.Connect,&C);
    else if(Route==3){xnetaddrlist* L=xrtNetAddrListCreate(&P->Peer.Base.Address,1);testRequire(L!=NULL,"resumed prepared addresses");OK=xmqttClientWssDialAddresses(P->Peer.Base.Engine,L,1,Config,&W,&T,&P->Peer.Base.Connect,&C);xrtNetAddrListDestroy(L);}
    else {const char* H=Route==4?"127.0.0.1":Route==5?"ex%61mple.com":"%31%32%37.0.0.1";int N=snprintf(Authority,Capacity,"%s:%u",H,(unsigned)P->Peer.Base.Address.Port);testRequire(N>0&&(size_t)N<Capacity,"counted resumed URI authority");P->Peer.ExpectedHost=(xstrview){Authority,(size_t)N};char URI[256];int M=snprintf(URI,sizeof(URI),"wss://%s/mqtt?x=%%00",Authority);testRequire(M>0&&(size_t)M<sizeof(URI),"resumed URI");OK=xmqttClientWssConnectUri(P->Peer.Base.Engine,P->Peer.Resolver,(xstrview){URI,(size_t)M},1,Config,&W,&T,&P->Peer.Base.Connect,&C);memset(URI,'?',(size_t)M);}
    if(!OK){fprintf(stderr,"WSS resume factory: %s\n",xrtGetError()?xrtErrorMessage(xrtGetError()):"none");}testRequire(OK&&C,"ordinary WSS factory accepts explicit capture/resumption");return C;
}
static inline void mrIssueTask(xnetworker* W,ptr Data)
{(void)W;mrPeer* P=Data;const unsigned Batch=P->Batch;xwsstream* S=xrtAtomicPtrLoad(&P->Peer.Base.Server,XMEMORY_ACQUIRE);xtlsstream* Native=xrtWsStreamTls(S);xrtAtomic32Store(&P->ServerResumed,xrtTlsServerResumed(xrtTlsStreamSession(Native)),XMEMORY_RELEASE);for(unsigned I=0;I<Batch;++I)mrIssueNative(P,Native);}
static inline void mrIssue(mrPeer* P,unsigned N)
{uint32 Before=xrtAtomic32Load(&P->Issued,XMEMORY_ACQUIRE);P->Batch=N;testRequire(xrtNetEnginePost(P->Peer.Base.Engine,0,mrIssueTask,P),"native ticket command");mqttWsWait(&P->Issued,Before+N,"actual WSS ticket output");}
static inline xmqttclienttlsresumeinfo mrInfo(xmqttclient* C)
{xmqttclienttlsresumeinfo I;testRequire(xmqttClientTlsResumeInfo(C,&I)&&I.Size==sizeof(I),"ordinary TLS resume API on WSS Client");return I;}
static inline xmqttclienttlsresumeinfo mrCaptured(xmqttclient* C,uint64 N)
{double End=xrtTimer()+10;xmqttclienttlsresumeinfo I;do{I=mrInfo(C);testRequire(xrtTimer()<End,"post-Upgrade ticket-only capture deadline");if(I.Captured<N)xrtSleep(1);}while(I.Captured<N);return I;}
static inline void mrPublish(xmqttclient* C,unsigned Qos)
{xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=(uint8)(Qos<<1);Q.Body.Publish.Topic=XRT_STR_LITERAL("wss/resume");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");xfuture* F=NULL;testRequire(xmqttClientPublishAsync(C,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED&&((const xmqttwriteresult*)mqttWsFuture(F))->Status==XMQTT_STATUS_OK,"fresh full/resumed MQTT QoS0/1/2");xrtFutureDestroy(F);}
static inline void mrClose(xmqttclient* C)
{xfuture* F=NULL;testRequire(xmqttClientDisconnectAsync(C,NULL,5000,&F)==XMQTT_ADMISSION_ACCEPTED&&((const xmqttwriteresult*)mqttWsFuture(F))->Status==XMQTT_STATUS_OK,"resumed MQTT/WS/TLS strict shutdown");testRequire(xmqttClientWaitFor(C,15000)==XWAIT_OK,"actual resumed WSS physical completion");xrtFutureDestroy(F);}
#endif
