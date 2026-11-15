#ifndef XMQTT_TEST_WS_PEER_H
#define XMQTT_TEST_WS_PEER_H
#include "../test.h"
#include <xmqtt/client_ws.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
#include "../../../../tests/fixtures/tls_server.h"
#endif
typedef struct mqttWsPeer {
    xnetengine* Engine;xnetlistener* Listener;xnetaddr Address;xmqttversion Version;
    bool TLS,Auth,Split,Coalesced,Text,Silent,SessionPresent,HoldAck,Validate,HoldAlias,UseBudget,BudgetPassed;
    size_t AllocationLimit;xallocationbudget* Budget;ptr Alias;unsigned char* Input;size_t Used,Capacity;
    xatomicptr Server,Client;xatomic32 Owners,Constructed,Closed,ListenerClosed,Publishes,ReceivedAcks,Disconnects;
    xmqttclientconfig ClientConfig;xmqttclientwsconfig WsConfig;xmqttpacket Connect;
    xnetpost Attach[2];ptr Native[2];
    void (*NativeServerOpen)(struct mqttWsPeer*,ptr);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
    xtlscontext* Context;xtlsidentity* Identity;xtlsverifier* Verifier;xtlsserverconfig NativeServer;
#endif
} mqttWsPeer;
static inline void mqttWsWait(xatomic32* A,uint32 Count,cstr Message)
{double End=xrtTimer()+15;while(xrtAtomic32Load(A,XMEMORY_ACQUIRE)<Count){testRequire(xrtTimer()<End,Message);xrtSleep(1);}}
static inline void mqttWsSend(xwsstream* S,const unsigned char* Data,size_t Size)
{
    testRequire(xrtWsStreamBinary(S,(xbytesview){Data,Size})==XNET_RESULT_OK,"literal WS MQTT reply");
}
static inline void mqttWsConnack(xwsstream* S,mqttWsPeer* P)
{
    unsigned char A5[]={0x20,6,0,0,3,0x22,0,2},A311[]={0x20,2,0,0};
    static const unsigned char Auth[]={0x20,11,0,0,8,0x15,0,1,'m',0x16,0,1,0x43};
    if(P->Text){testRequire(xrtWsStreamText(S,XRT_STR_LITERAL("bad"))==XNET_RESULT_OK,"bad Text fixture");return;}
    if(P->Silent)return;
    if(P->SessionPresent){A5[2]=1;A311[2]=1;}
    const unsigned char* Bytes=P->Auth?Auth:P->Version==XMQTT_VERSION_5?A5:A311;size_t Count=P->Auth?sizeof(Auth):P->Version==XMQTT_VERSION_5?sizeof(A5):sizeof(A311);
    if(P->Coalesced){unsigned char Joined[32];memcpy(Joined,Bytes,Count);static const unsigned char In5[]={0x30,8,0,2,'i','n',0,'x',0,'z'},In311[]={0x30,7,0,2,'i','n','x',0,'z'};const unsigned char* In=P->Version==XMQTT_VERSION_5?In5:In311;size_t N=P->Version==XMQTT_VERSION_5?sizeof(In5):sizeof(In311);memcpy(Joined+Count,In,N);mqttWsSend(S,Joined,Count+N);}
    else if(P->Split){for(size_t I=0;I<Count;++I)mqttWsSend(S,Bytes+I,1);}
    else mqttWsSend(S,Bytes,Count);
}
static inline void mqttWsFrame(xwsstream* S,mqttWsPeer* P,unsigned char Flags,const unsigned char* B,size_t N)
{
    unsigned Type=Flags>>4;
    if(Type==1){if(P->Auth){static const unsigned char A[]={0xf0,10,0x18,8,0x15,0,1,'m',0x16,0,1,0x42};mqttWsSend(S,A,sizeof(A));}else mqttWsConnack(S,P);}
    else if(Type==15){testRequire(P->Auth && N>=2,"expected auth frame");
        if(B[0]==0x19){static const unsigned char A[]={0xf0,6,0,4,0x15,0,1,'m'};mqttWsSend(S,A,sizeof(A));}
        else {testRequire(N==10 && B[0]==0x18 && B[9]==0x44,"literal challenge response");mqttWsConnack(S,P);}}
    else if(Type==3){testRequire(N>=2,"publication topic length");size_t Topic=((size_t)B[0]<<8)|B[1];testRequire(Topic<=N-2,"publication bounded topic");
        size_t At=Topic+2;unsigned Qos=(Flags>>1)&3;
        if(Qos&&!P->HoldAck){testRequire(At+2<=N,"publication identifier");unsigned char A[]={Qos==1?0x40:0x50,2,B[At],B[At+1]};mqttWsSend(S,A,sizeof(A));}
        if(Qos)At+=2;
        if(P->Version==XMQTT_VERSION_5){size_t Properties=0;unsigned Shift=0;unsigned char Byte;
            do{testRequire(At<N && Shift<28,"bounded property length");Byte=B[At++];Properties|=(size_t)(Byte&127u)<<Shift;Shift+=7;}while(Byte&128u);
            testRequire(Properties<=N-At,"bounded properties");At+=Properties;}
        size_t Payload=N-At;
        if(Payload==3)testRequire(!memcmp(B+At,"x\0z",3),"exact binary publication snapshot");
        else if(Payload==32)testRequire(!memcmp(B+At,"0123456789abcdef0123456789abcdef",32),"benchmark binary snapshot");
        else if(Payload==5)testRequire(!memcmp(B+At,"first",5),"exact first-prefix plaintext");
        else {testRequire(Payload==2u*1024u*1024u,"exact large snapshot size");for(size_t I=0;I<Payload;++I)testRequire(B[At+I]==0x6b,"large caller snapshot bytes");}
        xrtAtomic32FetchAdd(&P->Publishes,1,XMEMORY_RELEASE);
    }else if(Type==6){testRequire(N==2,"PUBREL identifier");unsigned char A[]={0x70,2,B[0],B[1]};mqttWsSend(S,A,sizeof(A));}
    else if(Type==8 || Type==10){testRequire(N>=2,"subscription identifier");unsigned char A[]={Type==8?0x90:0xb0,4,B[0],B[1],0,Type==8?1:0};
        if(P->Version==XMQTT_VERSION_311){if(Type==8){A[1]=3;A[4]=1;mqttWsSend(S,A,5);}else{A[1]=2;mqttWsSend(S,A,4);}}
        else mqttWsSend(S,A,sizeof(A));
        if(Type==8){static const unsigned char In5[]={0x32,10,0,2,'i','n',0,7,0,'x',0,'z'},In311[]={0x32,9,0,2,'i','n',0,7,'x',0,'z'};
            mqttWsSend(S,P->Version==XMQTT_VERSION_5?In5:In311,P->Version==XMQTT_VERSION_5?sizeof(In5):sizeof(In311));}
    }else if(Type==12){static const unsigned char A[]={0xd0,0};mqttWsSend(S,A,sizeof(A));}
    else if(Type==14){xrtAtomic32FetchAdd(&P->Disconnects,1,XMEMORY_RELEASE);testRequire(xrtWsStreamClose(S,XWS_CLOSE_NORMAL,(xstrview){0})==XNET_RESULT_OK,"fixture authenticated close");}
    else if(Type==5){testRequire(N>=2,"incoming QoS2 PUBREC");unsigned char A[]={0x62,2,B[0],B[1]};mqttWsSend(S,A,sizeof(A));}
    else if(Type==4 || Type==7)xrtAtomic32FetchAdd(&P->ReceivedAcks,1,XMEMORY_RELEASE);
    else testRequire(false,"expected literal MQTT control kind");
}
static inline void mqttWsBegin(xwsstream* S,const xwsmessageinfo* Info,ptr Data)
{(void)S;(void)Data;testRequire(Info->Opcode==XWS_OPCODE_BINARY,"all client MQTT bytes use Binary");}
static inline void mqttWsData(xwsstream* S,xbytesview Data,ptr User)
{
    mqttWsPeer* P=User;testRequire(Data.Size<=P->Capacity-P->Used,"bounded literal peer input");memcpy(P->Input+P->Used,Data.Data,Data.Size);P->Used+=Data.Size;
    for(;;){if(P->Used<2)return;size_t At=1,Remaining=0;unsigned Shift=0;unsigned char Byte;
        do{if(At==P->Used)return;testRequire(At<=4,"canonical bounded MQTT variable integer");Byte=P->Input[At++];Remaining|=(size_t)(Byte&127u)<<Shift;Shift+=7;}while(Byte&128u);
        if(Remaining>P->Used-At)return;
        mqttWsFrame(S,P,P->Input[0],P->Input+At,Remaining);P->Used-=At+Remaining;if(P->Used)memmove(P->Input,P->Input+At+Remaining,P->Used);
    }
}
static inline void mqttWsClosed(xwsstream* S,const xwsstreamclose* Close,ptr Data)
{(void)S;(void)Close;xrtAtomic32Store(&((mqttWsPeer*)Data)->Closed,1,XMEMORY_RELEASE);}
static const xwsstreamevents MqttWsPeerEvents={.MessageBegin=mqttWsBegin,.MessageData=mqttWsData,.Close=mqttWsClosed};
static inline void mqttWsAttachServerData(mqttWsPeer* P,ptr Native)
{
    xwsstreamconfig K;xrtWsStreamConfigInit(&K);K.Role=XWS_ROLE_SERVER;K.Protocol=XRT_STR_LITERAL("mqtt");xwsstream* S;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
    if(P->TLS)S=xrtWsStreamAttachTls(Native,0,&K,&MqttWsPeerEvents,P);else
#endif
    S=xrtWsStreamAttach(Native,0,&K,&MqttWsPeerEvents,P);
    testRequire(S!=NULL,"literal WS server attachment");xrtAtomicPtrStore(&P->Server,S,XMEMORY_RELEASE);
}
static inline void mqttWsAttachServer(mqttWsPeer* P,ptr Native)
{if(P->NativeServerOpen)P->NativeServerOpen(P,Native);else mqttWsAttachServerData(P,Native);}
static inline void mqttWsAttachClient(mqttWsPeer* P,ptr Native)
{
    xmqttclient* C=NULL;bool OK;
    if(P->Validate){xmqttclientwsconfig K=P->WsConfig;xmqttclient* Sentinel=(xmqttclient*)(uintptr_t)1;
        for(unsigned I=0;I<6;++I){K=P->WsConfig;if(I==0)K.Stream.Role=XWS_ROLE_SERVER;else if(I==1)K.Stream.Protocol=XRT_STR_LITERAL("wrong");else if(I==2)K.MaxStagedBytes=0;else if(I==3)K.KeepStagedBytes=K.MaxStagedBytes+1;else if(I==4)K.Stream.DeflateEnabled=true;else K.Stream.FrameLimit=K.MaxStagedBytes+1;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
            if(P->TLS)OK=xmqttClientWsAttachTls(Native,NULL,&P->ClientConfig,&K,&P->Connect,&Sentinel);else
#endif
            OK=xmqttClientWsAttachTcp(Native,NULL,&P->ClientConfig,&K,&P->Connect,&Sentinel);
            testRequire(!OK&&Sentinel==(xmqttclient*)(uintptr_t)1,"transactional invalid policy leaves output/native untouched");xrtClearError();}
    }
    if(P->HoldAlias){
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
        if(P->TLS)P->Alias=xrtTlsStreamRef(Native);else
#endif
        {P->Alias=xrtNetStreamRef(Native);}testRequire(P->Alias!=NULL,"actual borrowed native alias");}
    xallocationscope Scope={0};if(P->UseBudget){testRequire(xrtAllocationBudgetCreate(P->AllocationLimit,&P->Budget)&&xrtAllocationBudgetEnter(P->Budget,&Scope),"native owner constructor allocation scope");C=(xmqttclient*)(uintptr_t)1;}

#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
    if(P->TLS)OK=xmqttClientWsAttachTls(Native,NULL,&P->ClientConfig,&P->WsConfig,&P->Connect,&C);else
#endif
    OK=xmqttClientWsAttachTcp(Native,NULL,&P->ClientConfig,&P->WsConfig,&P->Connect,&C);
    if(P->UseBudget){testRequire(xrtAllocationBudgetLeave(&Scope),"leave WS constructor allocation scope");P->BudgetPassed=OK;
        if(!OK){testRequire(C==(xmqttclient*)(uintptr_t)1,"OOM leaves output sentinel untouched");xrtClearError();C=NULL;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
            if(P->TLS)OK=xmqttClientWsAttachTls(Native,NULL,&P->ClientConfig,&P->WsConfig,&P->Connect,&C);else
#endif
            OK=xmqttClientWsAttachTcp(Native,NULL,&P->ClientConfig,&P->WsConfig,&P->Connect,&C);}}
    if(!OK)fprintf(stderr,"WS attach: %s\n",xrtGetError()?xrtErrorMessage(xrtGetError()):"none");
    testRequire(OK&&C,"ordinary MQTT Client over native WS");xrtAtomicPtrStore(&P->Client,C,XMEMORY_RELEASE);xrtAtomic32Store(&P->Constructed,1,XMEMORY_RELEASE);
}
static inline void mqttWsTcpOpen(xnetstream* S,ptr Data){mqttWsAttachClient(Data,S);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
static inline void mqttWsTlsPosted(xnetworker* Worker,ptr Data)
{mqttWsPeer* P=Data;unsigned Index=xrtNetWorkerIndex(Worker)==1?0:1;if(Index==0)mqttWsAttachClient(P,P->Native[0]);else mqttWsAttachServer(P,P->Native[1]);}
static inline void mqttWsTlsOpen(xtlsstream* S,ptr Data)
{
    mqttWsPeer* P=Data;unsigned Index=xrtTlsSessionRole(xrtTlsStreamSession(S))==XTLS_CLIENT?0:1;P->Native[Index]=S;
    testRequire(xrtNetPostInit(P->Attach+Index)&&xrtNetPost(xrtNetStreamWorker(xrtTlsStreamTransport(S)),P->Attach+Index,mqttWsTlsPosted,P),"leave native TLS driver before owned WS event handoff");
}
static const xtlsstreamevents MqttWsTlsEvents={.Open=mqttWsTlsOpen};
static inline bool mqttWsOwnerCount(const void* D,size_t* N){*N=xrtAtomic32Load(&((const mqttWsPeer*)D)->Owners,XMEMORY_ACQUIRE);return true;}
/* Stack fixture is externally rooted and deliberately ineligible for collection. */
static inline bool mqttWsOwnerTrace(const void* D,xrtownershipvisitor V,ptr U){(void)D;(void)V;(void)U;return false;}
static inline void mqttWsOwnerDrop(const void* D){xrtAtomic32FetchSub(&((mqttWsPeer*)D)->Owners,1,XMEMORY_RELEASE);}
static const xrtownershipops MqttWsOwnerOps={mqttWsOwnerCount,mqttWsOwnerTrace};
static const xtlsstreamownershipv1 MqttWsTlsOwned={sizeof(xtlsstreamownershipv1),{.Open=mqttWsTlsOpen},mqttWsOwnerDrop,&MqttWsOwnerOps};
#endif
static inline bool mqttWsAccept(xnetlistener* L,xnetstream* S,ptr Data)
{
    (void)L;mqttWsPeer* P=Data;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
    if(P->TLS){xtlsstream* T=NULL;testRequire(xrtTlsStreamAccept(S,&P->NativeServer,NULL,&MqttWsTlsEvents,P,&T),"native TLS server");P->Native[1]=T;return true;}
#endif
    mqttWsAttachServer(P,S);return true;
}
static inline void mqttWsListenerClosed(xnetlistener* L,ptr Data){(void)L;xrtAtomic32Store(&((mqttWsPeer*)Data)->ListenerClosed,1,XMEMORY_RELEASE);}
static inline void mqttWsInit(mqttWsPeer* P,bool TLS,xmqttversion Version,xnetportbackend Backend)
{
    memset(P,0,sizeof(*P));P->TLS=TLS;P->Version=Version;P->Capacity=4u*1024u*1024u;P->Input=malloc(P->Capacity);testRequire(P->Input!=NULL,"peer bounded input");xrtAtomicPtrInit(&P->Server,NULL);xrtAtomicPtrInit(&P->Client,NULL);xrtAtomic32Init(&P->Owners,0);xrtAtomic32Init(&P->Constructed,0);xrtAtomic32Init(&P->Closed,0);xrtAtomic32Init(&P->ListenerClosed,0);xrtAtomic32Init(&P->Publishes,0);xrtAtomic32Init(&P->ReceivedAcks,0);xrtAtomic32Init(&P->Disconnects,0);
    xmqttClientConfigInit(&P->ClientConfig);P->ClientConfig.Version=Version;xmqttClientWsConfigInit(&P->WsConfig);xmqttPacketInit(XMQTT_PACKET_CONNECT,&P->Connect);P->Connect.Body.Connect.CleanStart=true;P->Connect.Body.Connect.ClientId=XRT_STR_LITERAL("ws-fixture");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
    if(TLS){P->Context=testTlsServerContext();P->Identity=testTlsServerIdentity();xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=testTlsServerAccept;P->Verifier=xrtTlsVerifierCreate(&V);testRequire(P->Context&&P->Identity&&P->Verifier,"real native TLS policy");xrtTlsServerConfigInit(&P->NativeServer);P->NativeServer.Context=P->Context;P->NativeServer.Identity=P->Identity;}
#else
    testRequire(!TLS,"WSS fixture feature");
#endif
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;E.Backend=Backend;P->Engine=xrtNetEngineCreate(&E);testRequire(P->Engine&&xrtNetEngineStart(P->Engine),"WS Engine");xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0);L.Affinity=0;L.Distribution=XNET_ACCEPT_LOCAL;xnetlistenerevents Events={mqttWsAccept,NULL,mqttWsListenerClosed};P->Listener=xrtNetListen(P->Engine,&L,&Events,NULL,P);testRequire(P->Listener&&xrtNetListenerLocal(P->Listener,&P->Address),"WS listener");
}
static inline xmqttclient* mqttWsStart(mqttWsPeer* P)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
    if(P->TLS){xtlsclientconfig K;xrtTlsClientConfigInit(&K);K.Context=P->Context;K.Verifier=P->Verifier;K.ServerName=XRT_STR_LITERAL("example.com");xrtAtomic32FetchAdd(&P->Owners,1,XMEMORY_RELEASE);testRequire(xrtTlsStreamConnectOwnedV1(P->Engine,&P->Address,1,NULL,&K,NULL,P,&MqttWsTlsOwned)!=NULL,"native ownership-certified WSS client");}else
#endif
    {xnetstreamevents E={.Open=mqttWsTcpOpen};testRequire(xrtNetStreamConnect(P->Engine,&P->Address,1,NULL,&E,P)!=NULL,"native WS client");}
    mqttWsWait(&P->Constructed,1,"MQTT WS construction");return xrtAtomicPtrLoad(&P->Client,XMEMORY_ACQUIRE);
}
static inline const void* mqttWsFuture(xfuture* F)
{xwaitresult Wait=F?xrtFutureWaitFor(F,15000):XWAIT_ERROR;if(!F||Wait!=XWAIT_OK||xrtFutureState(F)!=XFUTURE_RESOLVED)fprintf(stderr,"WS Future wait=%d state=%d error=%s\n",Wait,F?(int)xrtFutureState(F):-1,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");testRequire(F&&Wait==XWAIT_OK&&xrtFutureState(F)==XFUTURE_RESOLVED,"typed WS Future");return xrtFutureValue(F);}
static inline void mqttWsCloseClient(xmqttclient* C)
{testRequire(xmqttClientClose(C)&&xmqttClientWaitFor(C,15000)==XWAIT_OK&&xmqttClientPollRetired(C)==XWAIT_OK&&xmqttClientDestroy(C),"MQTT/WS/TLS/TCP actual physical joins");}
static inline void mqttWsStop(mqttWsPeer* P)
{
    xwsstream* S=xrtAtomicPtrLoad(&P->Server,XMEMORY_ACQUIRE);testRequire(S!=NULL,"WS server published");xrtWsStreamAbort(S);testRequire(xrtWsStreamWaitRetired(S,5000)==XWAIT_OK,"peer physical WS retirement");xrtWsStreamDestroy(S);xrtNetListenerClose(P->Listener);mqttWsWait(&P->ListenerClosed,1,"listener close");xrtNetListenerDestroy(P->Listener);testRequire(xrtNetEngineDestroy(P->Engine),"join actual host workers/resources");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
    xrtTlsVerifierRelease(P->Verifier);xrtTlsIdentityRelease(P->Identity);xrtTlsContextRelease(P->Context);
#endif
    if(P->Budget){xrtClearError();xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(xrtAllocationBudgetInfo(P->Budget,&I)&&!I.LiveCount&&!I.LiveBytes,"OOM constructor budget physically balanced after Engine join");xrtAllocationBudgetFree(P->Budget);}
    testRequire(xrtAtomic32Load(&P->Owners,XMEMORY_ACQUIRE)==0,"native TLS original owner physically released");free(P->Input);
}
#endif
