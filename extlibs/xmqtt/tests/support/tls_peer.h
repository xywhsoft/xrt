#ifndef XMQTT_TEST_TLS_PEER_H
#define XMQTT_TEST_TLS_PEER_H
#include "../../../../tests/fixtures/tls_server.h"
#ifndef MQTT_TLS_BACKEND
#define MQTT_TLS_BACKEND XNET_PORT_AUTO
#endif
/* Local healthy fixture, not an interoperability oracle. MQTT frames/replies
 * below use literal protocol bytes independently of the product codec. */
typedef struct mqttTlsPeer {
    xnetengine* Engine;xnetlistener* Listener;xnetresolver* Resolver;xnetaddr Address;
    xtlscontext* Context;xtlsidentity* Identity;xtlsserverconfig Config;xmqttversion Version;
    xatomicptr Server,Raw;xatomic32 Closed,ListenerClosed,Publishes,Lookups,VerifierDrops,ReceivedAcks,Disconnects,Resume,DnsEntered,DnsGate;
    unsigned char* Input;size_t Used,Capacity;bool Auth,PauseAfterFirst,Silent,Reject,BlockDns;
} mqttTlsPeer;
static inline void mqttTlsWait(xatomic32* A,uint32 Count,cstr Message)
{double End=xrtTimer()+15;while(xrtAtomic32Load(A,XMEMORY_ACQUIRE)<Count){testRequire(xrtTimer()<End,Message);xrtSleep(1);}}
static inline void mqttTlsSend(xtlsstream* S,const unsigned char* Data,size_t Size)
{size_t N=0;testRequire(xrtTlsStreamSend(S,Data,Size,&N)==XTLS_OK && N==Size,"fixture bounded TLS reply");}
static inline void mqttTlsConnack(xtlsstream* S,mqttTlsPeer* P)
{
    static const unsigned char A5[]={0x20,6,0,0,3,0x22,0,2},A311[]={0x20,2,0,0};
    static const unsigned char Auth[]={0x20,11,0,0,8,0x15,0,1,'m',0x16,0,1,0x43};
    mqttTlsSend(S,P->Auth?Auth:P->Version==XMQTT_VERSION_5?A5:A311,P->Auth?sizeof(Auth):P->Version==XMQTT_VERSION_5?sizeof(A5):sizeof(A311));
}
static inline void mqttTlsFrame(xtlsstream* S,mqttTlsPeer* P,unsigned char Flags,const unsigned char* B,size_t N)
{
    unsigned Type=Flags>>4;
    if(Type==1){if(P->Auth){static const unsigned char A[]={0xf0,10,0x18,8,0x15,0,1,'m',0x16,0,1,0x42};mqttTlsSend(S,A,sizeof(A));}else mqttTlsConnack(S,P);}
    else if(Type==15){testRequire(P->Auth && N>=2,"expected auth frame");
        if(B[0]==0x19){static const unsigned char A[]={0xf0,6,0,4,0x15,0,1,'m'};mqttTlsSend(S,A,sizeof(A));}
        else {testRequire(N==10 && B[0]==0x18 && B[9]==0x44,"literal challenge response");mqttTlsConnack(S,P);}}
    else if(Type==3){testRequire(N>=2,"publication topic length");size_t Topic=((size_t)B[0]<<8)|B[1];testRequire(Topic<=N-2,"publication bounded topic");
        size_t At=Topic+2;unsigned Qos=(Flags>>1)&3;
        if(Qos){testRequire(At+2<=N,"publication identifier");unsigned char A[]={Qos==1?0x40:0x50,2,B[At],B[At+1]};mqttTlsSend(S,A,sizeof(A));}
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
    }else if(Type==6){testRequire(N==2,"PUBREL identifier");unsigned char A[]={0x70,2,B[0],B[1]};mqttTlsSend(S,A,sizeof(A));}
    else if(Type==8 || Type==10){testRequire(N>=2,"subscription identifier");unsigned char A[]={Type==8?0x90:0xb0,4,B[0],B[1],0,Type==8?1:0};
        if(P->Version==XMQTT_VERSION_311){if(Type==8){A[1]=3;A[4]=1;mqttTlsSend(S,A,5);}else{A[1]=2;mqttTlsSend(S,A,4);}}
        else mqttTlsSend(S,A,sizeof(A));
        if(Type==8){static const unsigned char In5[]={0x32,10,0,2,'i','n',0,7,0,'x',0,'z'},In311[]={0x32,9,0,2,'i','n',0,7,'x',0,'z'};
            mqttTlsSend(S,P->Version==XMQTT_VERSION_5?In5:In311,P->Version==XMQTT_VERSION_5?sizeof(In5):sizeof(In311));}
    }else if(Type==12){static const unsigned char A[]={0xd0,0};mqttTlsSend(S,A,sizeof(A));}
    else if(Type==14){xrtAtomic32FetchAdd(&P->Disconnects,1,XMEMORY_RELEASE);testRequire(xrtTlsStreamClose(S),"fixture authenticated close");}
    else if(Type==4 || Type==5 || Type==7)xrtAtomic32FetchAdd(&P->ReceivedAcks,1,XMEMORY_RELEASE);
    else testRequire(false,"expected literal MQTT control kind");
}
static inline void mqttTlsRead(xtlsstream* S,const xnetbuf* Buffer,ptr Data)
{
    (void)Buffer;mqttTlsPeer* P=Data;
    if(P->PauseAfterFirst && xrtAtomic32Load(&P->Publishes,XMEMORY_ACQUIRE) && !xrtAtomic32Load(&P->Resume,XMEMORY_ACQUIRE))return;
    for(;;){size_t At=0;
        while(P->Used-At>=2){size_t H=1,N=0;unsigned Shift=0;unsigned char B;
            do{if(At+H==P->Used)goto ReadMore;B=P->Input[At+H++];N|=(size_t)(B&127u)<<Shift;Shift+=7;testRequire(H<=5,"bounded remaining length");}while(B&128u);
            if(N>P->Used-At-H)break;
            mqttTlsFrame(S,P,P->Input[At],P->Input+At+H,N);At+=H+N;
            if(P->PauseAfterFirst && xrtAtomic32Load(&P->Publishes,XMEMORY_ACQUIRE) && !xrtAtomic32Load(&P->Resume,XMEMORY_ACQUIRE))break;
        }
ReadMore:
        if(At){P->Used-=At;if(P->Used)memmove(P->Input,P->Input+At,P->Used);}
        if(P->PauseAfterFirst && xrtAtomic32Load(&P->Publishes,XMEMORY_ACQUIRE) && !xrtAtomic32Load(&P->Resume,XMEMORY_ACQUIRE))return;
        if(!xrtTlsStreamAvailable(S))return;
        size_t N=0;testRequire(P->Used<P->Capacity && xrtTlsStreamRead(S,P->Input+P->Used,P->Capacity-P->Used,&N)==XTLS_OK && N,"fixture plaintext read");P->Used+=N;
    }
}
static inline void mqttTlsClosed(xtlsstream* S,xnetresult R,const xerror* E,ptr Data)
{(void)S;(void)R;(void)E;xrtAtomic32Store(&((mqttTlsPeer*)Data)->Closed,1,XMEMORY_RELEASE);}
static const xtlsstreamevents MqttTlsEvents={NULL,mqttTlsRead,NULL,NULL,NULL,mqttTlsClosed,NULL};
static inline bool mqttTlsAccept(xnetlistener* L,xnetstream* Stream,ptr Data)
{
    (void)L;mqttTlsPeer* P=Data;xtlsstream* S=NULL;
    /* Keep a bounded window while paused, without Linux's tiny-window threshold
     * persisting after SO_RCVBUF is restored and throttling the healthy phase. */
    if(P->PauseAfterFirst)testRequire(xrtNetSocketSet(xrtNetStreamSocket(Stream),XNET_OPTION_RECEIVE_BUFFER,32768),"fixture bounded receive window");
    if(P->Silent){xrtAtomicPtrStore(&P->Raw,Stream,XMEMORY_RELEASE);return true;}
    if(!xrtTlsStreamAccept(Stream,&P->Config,NULL,&MqttTlsEvents,P,&S))return false;
    xrtAtomicPtrStore(&P->Server,S,XMEMORY_RELEASE);return true;
}
static inline void mqttTlsListenerClosed(xnetlistener* L,ptr Data)
{(void)L;xrtAtomic32Store(&((mqttTlsPeer*)Data)->ListenerClosed,1,XMEMORY_RELEASE);}
static inline xnetaddrlist* mqttTlsLookup(cstr Host,xnetfamily Family,ptr Data)
{
    (void)Family;mqttTlsPeer* P=Data;testRequire(!strcmp(Host,"example.com"),"counted hostname snapshot");
    xrtAtomic32Store(&P->DnsEntered,1,XMEMORY_RELEASE);
    if(P->BlockDns)mqttTlsWait(&P->DnsGate,1,"release independent DNS lookup");
    xrtAtomic32FetchAdd(&P->Lookups,1,XMEMORY_RELEASE);xnetaddr A=P->Address;A.Port=0;return xrtNetAddrListCreate(&A,1);
}
static inline void mqttTlsVerifierDrop(ptr Data)
{xrtAtomic32FetchAdd(&((mqttTlsPeer*)Data)->VerifierDrops,1,XMEMORY_RELEASE);}
static inline xtlsverifydecision mqttTlsVerify(const xtlspeer* Peer,ptr Data)
{mqttTlsPeer* P=Data;testRequire(Peer && Peer->Name.Size==11 && !memcmp(Peer->Name.Data,"example.com",11),"retained verification identity");return P->Reject?XTLS_VERIFY_REJECT:testTlsServerAccept(Peer,NULL);}
enum {MQTT_TLS_AUTH=1,MQTT_TLS_PAUSE=2,MQTT_TLS_SILENT=4,MQTT_TLS_REJECT=8,MQTT_TLS_BLOCK_DNS=16};
static inline void mqttTlsStart(mqttTlsPeer* P,xmqttversion Version,unsigned Flags)
{
    memset(P,0,sizeof(*P));P->Version=Version;P->Auth=(Flags&MQTT_TLS_AUTH)!=0;P->PauseAfterFirst=(Flags&MQTT_TLS_PAUSE)!=0;
    P->Silent=(Flags&MQTT_TLS_SILENT)!=0;P->Reject=(Flags&MQTT_TLS_REJECT)!=0;P->BlockDns=(Flags&MQTT_TLS_BLOCK_DNS)!=0;
    P->Capacity=3u*1024u*1024u;P->Input=xrtMalloc(P->Capacity);testRequire(P->Input!=NULL,"fixture bounded input");
    xrtAtomicPtrInit(&P->Server,NULL);P->Context=testTlsServerContext();P->Identity=testTlsServerIdentity();testRequire(P->Context && P->Identity,"TLS fixture resources");
    static const xstrview Protocols[]={{"mqtt",4}};xrtTlsServerConfigInit(&P->Config);P->Config.Context=P->Context;P->Config.Identity=P->Identity;P->Config.Protocols=Protocols;P->Config.ProtocolCount=1;P->Config.RequireProtocol=true;
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;E.Backend=MQTT_TLS_BACKEND;P->Engine=xrtNetEngineCreate(&E);testRequire(P->Engine && xrtNetEngineStart(P->Engine),"TLS MQTT engine");
    xnetlistenconfig L;xrtNetListenConfigInit(&L);L.Distribution=XNET_ACCEPT_LOCAL;xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0);
    static const xnetlistenerevents Events={mqttTlsAccept,NULL,mqttTlsListenerClosed};P->Listener=xrtNetListen(P->Engine,&L,&Events,NULL,P);testRequire(P->Listener && xrtNetListenerLocal(P->Listener,&P->Address),"TLS MQTT listener");
    xnetresolverconfig R;xrtNetResolverConfigInit(&R);R.Workers=1;R.Lookup=mqttTlsLookup;R.LookupData=P;P->Resolver=xrtNetResolverCreate(&R);testRequire(P->Resolver!=NULL,"TLS MQTT resolver");
}
static inline void mqttTlsStop(mqttTlsPeer* P)
{
    xrtAtomic32Store(&P->DnsGate,1,XMEMORY_RELEASE);
    xtlsstream* S=xrtAtomicPtrLoad(&P->Server,XMEMORY_ACQUIRE);
    if(S){testRequire(xrtTlsStreamAbort(S),"fixture TLS abort");mqttTlsWait(&P->Closed,1,"fixture TLS close");xrtTlsStreamDestroy(S);}
    xnetstream* Raw=xrtAtomicPtrLoad(&P->Raw,XMEMORY_ACQUIRE);if(Raw){testRequire(xrtNetStreamAbort(Raw) && xrtNetStreamWaitRetired(Raw,15000)==XWAIT_OK,"silent peer retires");xrtNetStreamDestroy(Raw);}
    testRequire(xrtNetListenerClose(P->Listener),"fixture listener close");mqttTlsWait(&P->ListenerClosed,1,"fixture listener callback");xrtNetListenerDestroy(P->Listener);
    testRequire(xrtNetResolverDestroy(P->Resolver) && xrtNetEngineDestroy(P->Engine),"actual fixture Resolver/Engine join");
    xrtTlsIdentityRelease(P->Identity);xrtTlsContextRelease(P->Context);xrtFree(P->Input);
}
static inline xtlsverifier* mqttTlsConfigure(mqttTlsPeer* P,xmqttclienttlsconfig* T,char Name[12],xstrview* Protocol,char Alpn[4])
{
    xmqttClientTlsConfigInit(T);memcpy(Name,"example.com",12);memcpy(Alpn,"mqtt",4);*Protocol=(xstrview){Alpn,4};
    T->Session.Context=P->Context;T->Session.ServerName=(xstrview){Name,11};T->Session.Protocols=Protocol;T->Session.ProtocolCount=1;
    xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=mqttTlsVerify;V.Release=mqttTlsVerifierDrop;V.Context=P;xtlsverifier* Verifier=xrtTlsVerifierCreate(&V);testRequire(Verifier!=NULL,"fixture verifier");T->Session.Verifier=Verifier;return Verifier;
}
#endif
