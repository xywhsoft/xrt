#ifndef XMQTT_TEST_WS_RECONNECT_PEER_H
#define XMQTT_TEST_WS_RECONNECT_PEER_H
#include "ws_connect_peer.h"
#include <xmqtt/client_ws_uri.h>
#include <xmqtt/reconnect_subscriptions.h>
#include <xrt/memory_stats.h>
typedef struct wrService wrService;
typedef struct wrConnection {mqttWsConnectPeer Peer;wrService* Service;unsigned Index;} wrConnection;
struct wrService {
    xnetengine* Engine;xnetlistener* Listener;xnetresolver* Resolver;xtaskpool* Pool;xnetaddr Address;
    xmqttversion Version;bool TLS;unsigned Route;xatomic32 Auth,Present,BlockDns,HoldHttp,RejectTLS,Cut,RejectHttp,Initialized,Cleanup,NoNotify,NoIncoming; xnetpost CleanupPost;
    char Authority[128],URI[256];wrConnection Connections[8];
    xatomic32 Accepted,Factories,Connacks,Publishes,Replays,Pubrels,Received,Subscriptions,ListenerClosed,DnsEntered,DnsRelease;
    xatomicptr Client,Ticket;uint16 OutgoingId;bool Clean[8];xallocationbudget* Budget;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
    xtlscontext* Context;xtlsidentity* Identity;xtlsverifier* Verifier;xtlsserverconfig Server;
#endif
};
#define WR_GET(S,F) xrtAtomic32Load(&(S)->F,XMEMORY_ACQUIRE)
static inline void wrWait(xatomic32* A,uint32 N,cstr Why){mqttWsWait(A,N,Why);}
static inline void wrIdReply(xwsstream* W,unsigned char Kind,uint16 Id)
{unsigned char B[]={Kind,2,(unsigned char)(Id>>8),(unsigned char)Id};mqttWsSend(W,B,sizeof(B));}
static inline void wrIncoming(xwsstream* W,wrService* S,bool Qos2)
{
    unsigned char B5[]={0x34,10,0,2,'i','n',0,29,0,'x',0,'z'},B311[]={0x34,9,0,2,'i','n',0,29,'x',0,'z'};
    unsigned char* B=S->Version==XMQTT_VERSION_5?B5:B311;size_t N=S->Version==XMQTT_VERSION_5?sizeof(B5):sizeof(B311);if(!Qos2)B[0]=0x30;
    if(!Qos2){B[1]-=2;memmove(B+6,B+8,N-8);N-=2;}mqttWsSend(W,B,N);
}
static inline bool wrControl(xwsstream* W,mqttWsPeer* Base,unsigned char Flags,const unsigned char* B,size_t N)
{
    wrConnection* C=(wrConnection*)Base;wrService* S=C->Service;unsigned Type=Flags>>4;
    if(Type==1){testRequire(N>7&&!memcmp(B,"\0\4MQTT",6),"literal reconnect CONNECT name");unsigned Level=B[6];testRequire(Level==(S->Version==XMQTT_VERSION_5?5u:4u),"same reconnect wire version");S->Clean[C->Index]=(B[7]&2)!=0;
        unsigned Round=xrtAtomic32FetchAdd(&S->Connacks,1,XMEMORY_ACQ_REL);Base->SessionPresent=Round&&WR_GET(S,Present);
        if(WR_GET(S,Auth)){static const unsigned char Challenge[]={0xf0,10,0x18,8,0x15,0,1,'m',0x16,0,1,0x42};mqttWsSend(W,Challenge,sizeof(Challenge));return true;}
        mqttWsConnack(W,Base);if(!Round&&!WR_GET(S,NoIncoming))wrIncoming(W,S,false);return true;
    }
    if(Type==15)return false;
    if(Type==3){testRequire(N>=2,"literal reconnect PUBLISH length");size_t At=2+((size_t)B[0]<<8)+B[1];unsigned Qos=(Flags>>1)&3;testRequire(At+(Qos?2:0)<=N,"literal reconnect PUBLISH id");uint16 Id=Qos?(uint16)(((unsigned)B[At]<<8)|B[At+1]):0;
        if(!C->Index){S->OutgoingId=Id;if(WR_GET(S,Cut)==0&&Qos==1){xrtAtomic32FetchAdd(&S->Publishes,1,XMEMORY_RELEASE);return true;}if(WR_GET(S,Cut)==1&&Qos==2){xrtAtomic32FetchAdd(&S->Publishes,1,XMEMORY_RELEASE);return true;}}
        else if(WR_GET(S,Cut)<2){testRequire((Flags&8)&&Id==S->OutgoingId,"automatic WS replay keeps identifier and DUP");xrtAtomic32FetchAdd(&S->Replays,1,XMEMORY_RELEASE);}
        if(Qos)wrIdReply(W,Qos==1?0x40:0x50,Id);
        xrtAtomic32FetchAdd(&S->Publishes,1,XMEMORY_RELEASE);return true;
    }
    if(Type==6){testRequire(N==2,"recovered PUBREL id");uint16 Id=(uint16)(((unsigned)B[0]<<8)|B[1]);if(C->Index&&WR_GET(S,Cut)==2)testRequire(Id==S->OutgoingId,"automatic WS PUBREL keeps old id");
        if(C->Index||WR_GET(S,Cut)!=2)wrIdReply(W,0x70,Id);
        xrtAtomic32FetchAdd(&S->Pubrels,1,XMEMORY_RELEASE);return true;}
    if(Type==8||Type==10){testRequire(N>=2,"automatic subscription id");unsigned char A5[]={Type==8?0x90:0xb0,4,B[0],B[1],0,Type==8?1:0},A311[]={Type==8?0x90:0xb0,Type==8?3:2,B[0],B[1],1};mqttWsSend(W,S->Version==XMQTT_VERSION_5?A5:A311,S->Version==XMQTT_VERSION_5?sizeof(A5):Type==8?sizeof(A311):4);xrtAtomic32FetchAdd(&S->Subscriptions,1,XMEMORY_RELEASE);return true;}
    if(Type==4||Type==7){xrtAtomic32FetchAdd(&S->Received,1,XMEMORY_RELEASE);return true;}
    return false;
}
static inline bool wrAccept(xnetlistener* L,xnetstream* Stream,ptr Data)
{wrService* S=Data;testRequire(WR_GET(S,Initialized),"fixture configuration published before accept");unsigned Index=xrtAtomic32Load(&S->Accepted,XMEMORY_RELAXED);testRequire(Index<8,"bounded WS recovery native connections");wrConnection* C=S->Connections+Index;C->Index=Index;C->Peer.Base.Auth=WR_GET(S,Auth)!=0;C->Peer.Mode=Index<WR_GET(S,RejectHttp)?3:0;C->Peer.HoldResponse=WR_GET(S,HoldHttp);bool OK=mqttWsAccept(L,Stream,&C->Peer.Base);xrtAtomic32Store(&S->Accepted,Index+1,XMEMORY_RELEASE);return OK;}
static inline void wrListenerClosed(xnetlistener* L,ptr Data){(void)L;xrtAtomic32Store(&((wrService*)Data)->ListenerClosed,1,XMEMORY_RELEASE);}
static inline xnetaddrlist* wrLookup(cstr Name,xnetfamily F,ptr Data)
{(void)F;wrService* S=Data;testRequire(!strcmp(Name,"example.com"),"same decoded WS recovery resolver name");xrtAtomic32FetchAdd(&S->DnsEntered,1,XMEMORY_RELEASE);if(WR_GET(S,BlockDns))wrWait(&S->DnsRelease,1,"release shared detached WS lookup");xnetaddr A=S->Address;A.Port=0;return xrtNetAddrListCreate(&A,1);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
static inline xtlsverifydecision wrVerify(const xtlspeer* P,ptr Data)
{wrService* S=Data;return !WR_GET(S,RejectTLS)?testTlsServerAccept(P,NULL):XTLS_VERIFY_REJECT;}
#endif
static inline void wrStart(wrService* S,bool TLS,xmqttversion V,unsigned Route,xnetportbackend Backend)
{
    memset(S,0,sizeof(*S));S->TLS=TLS;S->Version=V;S->Route=Route;xrtAtomic32Init(&S->Auth,0);xrtAtomic32Init(&S->Present,1);xrtAtomic32Init(&S->BlockDns,0);xrtAtomic32Init(&S->HoldHttp,0);xrtAtomic32Init(&S->RejectTLS,0);xrtAtomic32Init(&S->Cut,9);xrtAtomic32Init(&S->RejectHttp,0);xrtAtomic32Init(&S->Initialized,0);xrtAtomic32Init(&S->Cleanup,0);xrtAtomic32Init(&S->NoNotify,0);xrtAtomic32Init(&S->NoIncoming,0);
    xrtAtomic32Init(&S->Accepted,0);xrtAtomic32Init(&S->Factories,0);xrtAtomic32Init(&S->Connacks,0);xrtAtomic32Init(&S->Publishes,0);xrtAtomic32Init(&S->Replays,0);xrtAtomic32Init(&S->Pubrels,0);xrtAtomic32Init(&S->Received,0);xrtAtomic32Init(&S->Subscriptions,0);xrtAtomic32Init(&S->ListenerClosed,0);xrtAtomic32Init(&S->DnsEntered,0);xrtAtomic32Init(&S->DnsRelease,0);xrtAtomicPtrInit(&S->Client,NULL);xrtAtomicPtrInit(&S->Ticket,NULL);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
    if(TLS){S->Context=testTlsServerContext();S->Identity=testTlsServerIdentity();xtlsverifierconfig VC;xrtTlsVerifierConfigInit(&VC);VC.Verify=wrVerify;VC.Context=S;S->Verifier=xrtTlsVerifierCreate(&VC);testRequire(S->Context&&S->Identity&&S->Verifier,"native WS recovery TLS policy");xrtTlsServerConfigInit(&S->Server);S->Server.Context=S->Context;S->Server.Identity=S->Identity;}
#else
    testRequire(!TLS,"WS recovery TLS selection");
#endif
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;E.Backend=Backend;S->Engine=xrtNetEngineCreate(&E);testRequire(S->Engine&&xrtNetEngineStart(S->Engine),"one recovery Engine");xtaskpoolconfig P={1,8,0};S->Pool=xrtTaskPoolCreate(&P);testRequire(S->Pool!=NULL,"borrowed recovery cleanup Pool");
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0);L.Affinity=0;L.Distribution=XNET_ACCEPT_LOCAL;xnetlistenerevents Events={wrAccept,NULL,wrListenerClosed};S->Listener=xrtNetListen(S->Engine,&L,&Events,NULL,S);testRequire(S->Listener&&xrtNetListenerLocal(S->Listener,&S->Address),"WS recovery listener");
    int A=snprintf(S->Authority,sizeof(S->Authority),"%s:%u",Route==4?"ex%61mple.com":Route==5?"%31%32%37.0.0.1":"127.0.0.1",(unsigned)S->Address.Port);int N=snprintf(S->URI,sizeof(S->URI),"%s://%s/mqtt?x=%%00",TLS?"wss":"ws",S->Authority);testRequire(A>0&&N>0&&(size_t)N<sizeof(S->URI),"recovery URI fixture");
    for(unsigned I=0;I<8;++I){wrConnection* C=S->Connections+I;mqttWsPeer* B=&C->Peer.Base;C->Service=S;B->Engine=S->Engine;B->TLS=TLS;B->Version=V;B->Capacity=65536;B->Input=malloc(B->Capacity);testRequire(B->Input!=NULL,"bounded server MQTT input");B->NativeServerOpen=mqttWsConnectNative;B->Control=wrControl;xrtAtomicPtrInit(&B->Server,NULL);xrtAtomicPtrInit(&B->Client,NULL);xrtAtomic32Init(&B->Closed,0);xrtAtomic32Init(&B->Owners,0);xrtAtomic32Init(&B->ListenerClosed,0);xrtAtomic32Init(&B->Publishes,0);xrtAtomic32Init(&B->ReceivedAcks,0);xrtAtomic32Init(&B->Disconnects,0);xrtAtomic32Init(&C->Peer.Header,0);xrtAtomic32Init(&C->Peer.Attached,0);C->Peer.ExpectedHost=Route>=3?(xstrview){S->Authority,(size_t)A}:XRT_STR_LITERAL("example.com");C->Peer.ExpectedTarget=XRT_STR_LITERAL("/mqtt?x=%00");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
        B->NativeServer=S->Server;
#endif
    }
    xnetresolverconfig RC;xrtNetResolverConfigInit(&RC);RC.Workers=1;RC.CacheEntries=0;RC.Lookup=wrLookup;RC.LookupData=S;S->Resolver=xrtNetResolverCreate(&RC);testRequire(S->Resolver!=NULL,"one shared recovery Resolver");
    xrtAtomic32Store(&S->Initialized,1,XMEMORY_RELEASE);
}
static inline void wrNotice(xmqttclient* C,ptr Data)
{wrService* S=Data;xfuture* F=xmqttClientReady(C);if(F&&xrtFutureDone(F)&&xrtFutureState(F)==XFUTURE_RESOLVED){const xmqttreadyresult* Ready=xrtFutureValue(F);if(Ready->Status!=XMQTT_STATUS_OK){fprintf(stderr,"[ready] status=%d cause=%s\n",Ready->Status,Ready->Cause?xrtErrorMessage(Ready->Cause):"none");}}xrtFutureDestroy(F);xmqttauthticket* T=NULL;if(xmqttClientTakeAuth(C,&T)){testRequire(!xrtAtomicPtrLoad(&S->Ticket,XMEMORY_ACQUIRE),"one active recovery auth ticket");xrtAtomicPtrStore(&S->Client,C,XMEMORY_RELEASE);xrtAtomicPtrStore(&S->Ticket,T,XMEMORY_RELEASE);}}
static inline xmqttclientstatus wrFactory(xnetengine* E,uint64 Affinity,const xmqttclientconfig* K,xmqttsession* Session,const xmqttpacket* Q,ptr Data,xmqttclient** C)
{
    wrService* S=Data;testRequire(E==S->Engine&&xrtNetEngineCurrent(E)==xrtNetEngineWorker(E,Affinity),"factory runs on the existing owner");xmqttclientwsconnectconfig W;xmqttClientWsConnectConfigInit(&W);W.Host=XRT_STR_LITERAL("example.com");W.Target=XRT_STR_LITERAL("/mqtt?x=%00");W.WebSocket.Session=Session;xmqttauthconfig Auth;if(WR_GET(S,Auth)){xmqttAuthConfigInit(&Auth);W.WebSocket.Auth=&Auth;}bool OK=false;xerror* Saved=xrtTakeError();xallocationscope Scope={0};if(S->Budget)testRequire(xrtAllocationBudgetEnter(S->Budget,&Scope),"recovery factory budget");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
    xmqttclienttlsconfig T;if(S->TLS){xmqttClientTlsConfigInit(&T);T.Session.ResumeLimit=0;T.Session.Context=S->Context;T.Session.Verifier=S->Verifier;if(S->Route<3)T.Session.ServerName=XRT_STR_LITERAL("example.com");
        if(S->Route==0)OK=xmqttClientWssConnect(E,&S->Address,Affinity,K,&W,&T,Q,C);else if(S->Route==1)OK=xmqttClientWssDial(E,S->Resolver,XRT_STR_LITERAL("example.com"),S->Address.Port,Affinity,K,&W,&T,Q,C);else if(S->Route==2){xnetaddrlist* L=xrtNetAddrListCreate(&S->Address,1);if(L){OK=xmqttClientWssDialAddresses(E,L,Affinity,K,&W,&T,Q,C);xrtNetAddrListDestroy(L);}}else OK=xmqttClientWssConnectUri(E,S->Resolver,(xstrview){S->URI,strlen(S->URI)},Affinity,K,&W,&T,Q,C);
    }else
#endif
    {if(S->Route==0)OK=xmqttClientWsConnect(E,&S->Address,Affinity,K,&W,Q,C);else if(S->Route==1)OK=xmqttClientWsDial(E,S->Resolver,XRT_STR_LITERAL("example.com"),S->Address.Port,Affinity,K,&W,Q,C);else if(S->Route==2){xnetaddrlist* L=xrtNetAddrListCreate(&S->Address,1);if(L){OK=xmqttClientWsDialAddresses(E,L,Affinity,K,&W,Q,C);xrtNetAddrListDestroy(L);}}else OK=xmqttClientWsConnectUri(E,S->Resolver,(xstrview){S->URI,strlen(S->URI)},Affinity,K,&W,Q,C);}
    if(S->Budget){testRequire(xrtAllocationBudgetLeave(&Scope),"recovery factory budget leave");}xerror* Fresh=xrtTakeError();xmqttclientstatus Status=OK?XMQTT_STATUS_OK:xrtErrorKind(Fresh)==XERR_MEMORY?XMQTT_STATUS_MEMORY:XMQTT_STATUS_INITIALIZATION;xrtErrorFree(Fresh);xrtSetErrorTake(Saved);xrtAtomic32FetchAdd(&S->Factories,1,XMEMORY_RELEASE);if(OK)xrtAtomicPtrStore(&S->Client,*C,XMEMORY_RELEASE);return Status;
}
static inline xmqttreconnect* wrCreate(wrService* S,int64 Recovery,bool Subscribed)
{
    xmqttreconnectconfig K;xmqttReconnectConfigInit(&K);K.Factory=wrFactory;K.FactoryData=S;K.Client.Version=S->Version;K.Client.Notify=WR_GET(S,NoNotify)?NULL:wrNotice;K.Client.User=S;K.Client.SetupTimeout=10000;K.InitialDelay=5;K.MaxDelay=20;K.JitterPermille=0;K.RecoveryTimeout=Recovery;K.MaxAttempts=WR_GET(S,RejectTLS)?1:4;if(S->Version==XMQTT_VERSION_311&&!WR_GET(S,Present))K.CleanStartFirstOnly=false;
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=S->Version==XMQTT_VERSION_5||!WR_GET(S,Present);Q.Body.Connect.ClientId=XRT_STR_LITERAL("ws-managed");xmqttproperty P[2]={{0}};size_t Count=0;if(S->Version==XMQTT_VERSION_5){P[Count].Id=XMQTT_PROPERTY_SESSION_EXPIRY;P[Count].Type=XMQTT_PROPERTY_UINT32;P[Count++].Value.Number=60;}if(WR_GET(S,Auth)){P[Count].Id=XMQTT_PROPERTY_AUTH_METHOD;P[Count].Type=XMQTT_PROPERTY_STRING;P[Count++].Value.Text=XRT_STR_LITERAL("m");}Q.Properties=(xmqttpropertylist){P,Count};xmqttreconnect* R=NULL;
    xmqttsubscriptionsconfig SC;
    xmqttsubscription Topic={XRT_STR_LITERAL("managed/#"),1};
    xmqttpacket Initial;
    const xmqttreconnectsubscriptionplan Plan={.Config=&SC,.Initial=&Initial,.Count=1};
    if(Subscribed){
        xmqttSubscriptionsConfigInit(&SC);
        SC.MaxFilters=4;SC.MaxBatch=1;
        xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&Initial);
        Initial.Body.Topics=(xmqttsubscriptionlist){&Topic,1};
        K.Subscriptions=&Plan;
    }
    testRequire(xmqttReconnectCreate(S->Engine,S->Pool,1,&K,&Q,&R),"automatic WS controller");
    return R;
}
static inline void wrOnline(xmqttreconnect* R,uint64 Connections,xmqttclientlease* Lease)
{double End=xrtTimer()+15;for(;;){xmqttreconnectinfo I;testRequire(xmqttReconnectInfo(R,&I),"controller info");if(I.State>=XMQTT_RECONNECT_STOPPED||xrtTimer()>=End){fprintf(stderr,"[online] state=%d status=%d gen=%llu attempts=%llu connections=%llu target=%llu\n",I.State,I.LastStatus,(unsigned long long)I.Generation,(unsigned long long)I.Attempts,(unsigned long long)I.Connections,(unsigned long long)Connections);testRequire(false,"automatic WS online deadline");}if(I.State==XMQTT_RECONNECT_ONLINE&&I.Connections==Connections&&xmqttReconnectAcquire(R,Lease))return;xrtSleep(1);}}
static inline void wrServerCleanup(xnetworker* Worker,ptr Data)
{
    (void)Worker;wrService* S=Data;unsigned N=xrtAtomic32Load(&S->Accepted,XMEMORY_ACQUIRE);
    for(unsigned I=0;I<N;++I){mqttWsPeer* P=&S->Connections[I].Peer.Base;xwsstream* W=xrtAtomicPtrLoad(&P->Server,XMEMORY_ACQUIRE);if(W){testRequire(xrtWsStreamState(W)==XWS_STREAM_CLOSED||xrtWsStreamAbort(W),"server WS abort or already closed");xrtWsStreamDestroy(W);xrtAtomicPtrStore(&P->Server,NULL,XMEMORY_RELEASE);}else if(P->Native[1]){
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
            if(S->TLS){testRequire(xrtTlsStreamState(P->Native[1])==XTLS_STREAM_CLOSED||xrtTlsStreamAbort(P->Native[1]),"server TLS abort or already closed");xrtTlsStreamDestroy(P->Native[1]);}else
#endif
            {testRequire(xrtNetStreamState(P->Native[1])==XNET_STREAM_CLOSED||xrtNetStreamAbort(P->Native[1]),"server HTTP abort or already closed");xrtNetStreamDestroy(P->Native[1]);}P->Native[1]=NULL;}}
    xrtAtomic32Store(&S->Cleanup,1,XMEMORY_RELEASE);
}
static inline void wrStopService(wrService* S)
{
    xrtAtomic32Store(&S->DnsRelease,1,XMEMORY_RELEASE);testRequire(xrtNetResolverDestroy(S->Resolver),"shared detached lookup joined");testRequire(xrtNetListenerClose(S->Listener),"WS recovery listener close");wrWait(&S->ListenerClosed,1,"WS recovery listener closed");
    testRequire(xrtNetPostInit(&S->CleanupPost)&&xrtNetPost(xrtNetEngineWorker(S->Engine,0),&S->CleanupPost,wrServerCleanup,S),"native cleanup on actual server owner");wrWait(&S->Cleanup,1,"server cleanup queued all physical native duties");
    xrtNetListenerDestroy(S->Listener);testRequire(xrtTaskPoolClose(S->Pool)&&xrtTaskPoolWaitFor(S->Pool,15000)==XWAIT_OK&&xrtTaskPoolDestroy(S->Pool)&&xrtNetEngineDestroy(S->Engine),"Pool/Engine real joins");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
    xrtTlsVerifierRelease(S->Verifier);xrtTlsIdentityRelease(S->Identity);xrtTlsContextRelease(S->Context);
#endif
    for(unsigned I=0;I<8;++I)free(S->Connections[I].Peer.Base.Input);
    if(S->Budget){xallocationbudgetinfo I={.Size=sizeof(I)};xrtClearError();testRequire(xrtAllocationBudgetInfo(S->Budget,&I)&&!I.LiveBytes&&!I.LiveCount,"all recovery budget credits retired");xrtAllocationBudgetFree(S->Budget);}
}
static inline void wrStop(wrService* S,xmqttreconnect* R)
{testRequire(xmqttReconnectClose(R)&&xmqttReconnectWaitFor(R,15000)==XWAIT_OK&&xmqttReconnectDestroy(R),"WS controller and all Client duties joined");wrStopService(S);}
#endif
