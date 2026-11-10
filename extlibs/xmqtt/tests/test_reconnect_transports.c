#include "test.h"
#include "support/tls_peer.h"
#include <xmqtt/reconnect.h>
/* Literal m/B/C/D tokens exercise protocol/proof ownership, not an auth algorithm. */
typedef struct rtState {
    mqttTlsPeer Peer;xtaskpool* Pool;unsigned Factory;
    xatomicptr Ticket,Client;xatomic32 Generation,Sent;
} rtState;
static void rtFrame(xtlsstream* Stream,mqttTlsPeer* Peer,unsigned char Flags,const unsigned char* Body,size_t Size)
{
    rtState* S=Peer->FrameData;
    if((Flags>>4)==15 && Size && Body[0]==0x18){
        testRequire(Size==10 && Body[9]=='D',"literal challenge response");
        unsigned char Ack[]={0x20,11,xrtAtomic32Load(&S->Generation,XMEMORY_ACQUIRE)?1:0,0,8,0x15,0,1,'m',0x16,0,1,'C'};
        static const unsigned char Incoming[]={0x30,8,0,2,'i','n',0,'x',0,'z'};
        mqttTlsSend(Stream,Ack,sizeof(Ack));mqttTlsSend(Stream,Incoming,sizeof(Incoming));xrtAtomic32FetchAdd(&S->Sent,1,XMEMORY_RELEASE);
    }else mqttTlsFrame(Stream,Peer,Flags,Body,Size);
}
static void rtNotice(xmqttclient* C,ptr Data)
{
    rtState* S=Data;xmqttauthticket* T=NULL;
    if(xmqttClientTakeAuth(C,&T)){
        testRequire(!xrtAtomicPtrLoad(&S->Ticket,XMEMORY_ACQUIRE),"one unique application ticket");
        xrtAtomicPtrStore(&S->Client,C,XMEMORY_RELEASE);xrtAtomicPtrStore(&S->Ticket,T,XMEMORY_RELEASE);
    }
}
static xmqttclientstatus rtFactory(xnetengine* E,uint64 Affinity,const xmqttclientconfig* K,xmqttsession* Session,xmqttclientinbox* Inbox,const xmqttpacket* Q,ptr Data,xmqttclient** C)
{
    rtState* S=Data;xmqttclienttlsconfig TLS;char Name[12],Alpn[4];xstrview Protocol;xtlsverifier* V=mqttTlsConfigure(&S->Peer,&TLS,Name,&Protocol,Alpn);bool OK;
    if(!S->Factory)OK=xmqttClientTlsConnectSessionAuth(E,&S->Peer.Address,Affinity,K,&TLS,NULL,Session,Inbox,Q,C);
    else if(S->Factory==1)OK=xmqttClientTlsDialSessionAuth(E,S->Peer.Resolver,XRT_STR_LITERAL("example.com"),S->Peer.Address.Port,Affinity,K,&TLS,NULL,Session,Inbox,Q,C);
    else {xnetaddrlist* L=xrtNetAddrListCreate(&S->Peer.Address,1);testRequire(L!=NULL,"bounded prepared addresses");OK=xmqttClientTlsDialAddressesSessionAuth(E,L,Affinity,K,&TLS,NULL,Session,Inbox,Q,C);xrtNetAddrListDestroy(L);}
    xrtTlsVerifierRelease(V);memset(Name,'x',sizeof(Name));memset(Alpn,'y',sizeof(Alpn));return OK?XMQTT_STATUS_OK:XMQTT_STATUS_INITIALIZATION;
}
static void rtStart(rtState* S,unsigned Factory,unsigned Flags)
{
    memset(S,0,sizeof(*S));S->Factory=Factory;xrtAtomicPtrInit(&S->Ticket,NULL);xrtAtomicPtrInit(&S->Client,NULL);xrtAtomic32Init(&S->Generation,0);xrtAtomic32Init(&S->Sent,0);
    mqttTlsStartWith(&S->Peer,XMQTT_VERSION_5,MQTT_TLS_AUTH|Flags,rtFrame,S);xtaskpoolconfig P={1,8,0};S->Pool=xrtTaskPoolCreate(&P);testRequire(S->Pool!=NULL,"borrowed cleanup Pool");
}
static xmqttreconnect* rtCreate(rtState* S,int64 Recovery)
{
    xmqttreconnectconfig K;testRequire(xmqttReconnectConfigInit(&K),"controller defaults");K.Factory=rtFactory;K.FactoryData=S;K.Client.Notify=rtNotice;K.Client.User=S;K.Client.SetupTimeout=5000;K.RecoveryTimeout=Recovery;K.MaxAttempts=S->Peer.Reject?1:8;
    /* Keep the default attempt budget for proof failures; bound the intentionally
     * bad-certificate transport fixture to one connection. */
    K.InitialDelay=5;K.MaxDelay=20;K.JitterPermille=0;
    xmqttproperty Props[2]={{XMQTT_PROPERTY_SESSION_EXPIRY,XMQTT_PROPERTY_UINT32,{0}},{XMQTT_PROPERTY_AUTH_METHOD,XMQTT_PROPERTY_STRING,{0}}};Props[0].Value.Number=60;Props[1].Value.Text=XRT_STR_LITERAL("m");
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=XRT_STR_LITERAL("automatic-auth");Q.Properties=(xmqttpropertylist){Props,2};xmqttreconnect* R=NULL;testRequire(xmqttReconnectCreate(S->Peer.Engine,S->Pool,1,&K,&Q,&R),"authenticated controller");return R;
}
static xmqttauthticket* rtTicket(rtState* S,xmqttauthphase Phase)
{
    double End=xrtTimer()+15;xmqttauthticket* T;
    while(!(T=xrtAtomicPtrExchange(&S->Ticket,NULL,XMEMORY_ACQ_REL))){testRequire(xrtTimer()<End,"application auth ticket deadline");xrtSleep(1);}
    testRequire(xmqttAuthPhase(T)==Phase,"expected application proof phase");return T;
}
static xmqttclientadmission rtResponse(xmqttauthticket* T)
{
    xmqttproperty P[2]={{XMQTT_PROPERTY_AUTH_METHOD,XMQTT_PROPERTY_STRING,{0}},{XMQTT_PROPERTY_AUTH_DATA,XMQTT_PROPERTY_BINARY,{0}}};P[0].Value.Text=XRT_STR_LITERAL("m");P[1].Value.Binary=XRT_BYTES_LITERAL("D");xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_AUTH,&Q);Q.Reason=0x18;Q.Properties=(xmqttpropertylist){P,2};return xmqttAuthRespond(T,&Q);
}
static void rtRespond(xmqttauthticket* T)
{testRequire(rtResponse(T)==XMQTT_ADMISSION_ACCEPTED,"respond to borrowed challenge");}
static void rtQuarantine(rtState* S,xmqttreconnect* R,unsigned Generation)
{
    mqttTlsWait(&S->Sent,Generation,"proof and coalesced data sent");xmqttclientlease L={0};xmqttmessage* M=(xmqttmessage*)(uintptr_t)1;xmqttreconnectinfo I;
    for(unsigned J=0;J<100;++J){testRequire(xmqttReconnectInfo(R,&I) && I.State==XMQTT_RECONNECT_CONNECTING && I.Connections==Generation-1 && !xmqttReconnectAcquire(R,&L) && !L.Owner,"controller never leases unverified connection");testRequire(!xmqttReconnectTakeMessage(R,&M) && M==(xmqttmessage*)(uintptr_t)1,"private Inbox never exposes unverified data");}
}
static xfuture* rtAccept(rtState* S,xmqttreconnect* R,unsigned Generation,xmqttclientlease* L)
{
    xmqttauthticket* T=rtTicket(S,XMQTT_AUTH_CHALLENGE);rtRespond(T);xmqttAuthRelease(T);T=rtTicket(S,XMQTT_AUTH_VERIFY_CONNECT);rtQuarantine(S,R,Generation);
    xmqttclient* C=xrtAtomicPtrLoad(&S->Client,XMEMORY_ACQUIRE);xfuture* Ready=xmqttClientReady(C);testRequire(xrtFutureWaitFor(Ready,0)==XWAIT_TIMEOUT,"Ready waits for final application proof");testRequire(xmqttAuthComplete(T,true)==XMQTT_ADMISSION_ACCEPTED,"accept final proof");xmqttAuthRelease(T);
    testRequire(xrtFutureWaitFor(Ready,15000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"authenticated typed Ready");double End=xrtTimer()+15;while(!xmqttReconnectAcquire(R,L)){testRequire(xrtTimer()<End,"verified generation lease");xrtSleep(1);}testRequire(L->Generation==Generation,"only expected generation published");return Ready;
}
static void rtStop(rtState* S,xmqttreconnect* R)
{
    testRequire(!xrtAtomicPtrLoad(&S->Ticket,XMEMORY_ACQUIRE),"all application tickets returned");testRequire(xmqttReconnectClose(R) && xmqttReconnectWaitFor(R,15000)==XWAIT_OK && xmqttReconnectDestroy(R),"all controller/Client/native retirement");testRequire(xrtTaskPoolClose(S->Pool) && xrtTaskPoolWaitFor(S->Pool,5000)==XWAIT_OK && xrtTaskPoolDestroy(S->Pool),"borrowed cleanup Pool joined");mqttTlsStop(&S->Peer);
}
static void rtSuccess(unsigned Factory)
{
    rtState S;rtStart(&S,Factory,0);xmqttreconnect* R=rtCreate(&S,60000);xmqttclientlease L={0};xfuture* First=rtAccept(&S,R,1,&L);xmqttmessage* Held=NULL;double End=xrtTimer()+15;
    while(!xmqttReconnectTakeMessage(R,&Held)){testRequire(xrtTimer()<End,"verified incoming message");xrtSleep(1);}
    xtlsstream* Peer=xrtAtomicPtrLoad(&S.Peer.Server,XMEMORY_ACQUIRE);xnetstream* Native=xrtNetStreamRef(xrtTlsStreamTransport(Peer));testRequire(Native!=NULL,"retain fixture native owner for physical join");testRequire(xrtTlsStreamAbort(Peer) && xmqttClientWaitFor(L.Client,15000)==XWAIT_OK,"interrupt verified generation while leased");mqttTlsWait(&S.Peer.Closed,1,"old TLS peer closed");xrtTlsStreamDestroy(Peer);testRequire(xrtNetStreamWaitRetired(Native,15000)==XWAIT_OK,"all old TLS server/native notification tails retired");xrtNetStreamDestroy(Native);testRequire(!S.Peer.Used,"old fixture has no partial frame");
    xrtAtomicPtrStore(&S.Peer.Server,NULL,XMEMORY_RELEASE);xrtAtomic32Store(&S.Peer.Closed,0,XMEMORY_RELEASE);xrtAtomic32Store(&S.Generation,1,XMEMORY_RELEASE);testRequire(xmqttReconnectRelease(&L),"release permits physical replacement");xfuture* Second=rtAccept(&S,R,2,&L);
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_AUTH,&Q);Q.Reason=0x19;xmqttproperty Method={XMQTT_PROPERTY_AUTH_METHOD,XMQTT_PROPERTY_STRING,{0}};Method.Value.Text=XRT_STR_LITERAL("m");Q.Properties=(xmqttpropertylist){&Method,1};xfuture* Auth=NULL;
    testRequire(xmqttClientReauthenticateAsync(L.Client,&Q,5000,&Auth)==XMQTT_ADMISSION_ACCEPTED,"reauthenticate same generation");xmqttauthticket* T=rtTicket(&S,XMQTT_AUTH_VERIFY_REAUTH);testRequire(xmqttAuthComplete(T,true)==XMQTT_ADMISSION_ACCEPTED,"reauth proof accepted");xmqttAuthRelease(T);testRequire(xrtFutureWaitFor(Auth,15000)==XWAIT_OK && ((const xmqttauthresult*)xrtFutureValue(Auth))->Status==XMQTT_STATUS_OK,"reauth typed result");
    xmqttreconnectinfo I;testRequire(xmqttReconnectInfo(R,&I) && I.Connections==2 && I.Generation==2,"reauth never creates another generation");testRequire(xmqttReconnectClose(R) && xmqttReconnectWaitFor(R,0)==XWAIT_TIMEOUT,"active application lease prevents final retirement");testRequire(xmqttReconnectRelease(&L),"return verified generation lease");rtStop(&S,R);
    testRequire(xmqttMessagePacket(Held)->Body.Publish.Payload.Size==3 && ((const xmqttreadyresult*)xrtFutureValue(First))->Status==XMQTT_STATUS_OK && ((const xmqttreadyresult*)xrtFutureValue(Second))->Negotiated.SessionPresent,"message/Ready views survive all owners");xmqttMessageRelease(Held);xrtFutureDestroy(First);xrtFutureDestroy(Second);xrtFutureDestroy(Auth);
}
static void rtDenied(unsigned Factory,bool Challenge)
{
    rtState S;rtStart(&S,Factory,0);xmqttreconnect* R=rtCreate(&S,5000);xmqttauthticket* T=rtTicket(&S,XMQTT_AUTH_CHALLENGE);
    if(!Challenge){rtRespond(T);xmqttAuthRelease(T);T=rtTicket(&S,XMQTT_AUTH_VERIFY_CONNECT);rtQuarantine(&S,R,1);}
    xmqttclient* C=xrtAtomicPtrLoad(&S.Client,XMEMORY_ACQUIRE);xfuture* F=xmqttClientReady(C);
    if(Challenge)testRequire(xmqttReconnectClose(R),"stop while challenge held");else testRequire(xmqttAuthComplete(T,false)==XMQTT_ADMISSION_ACCEPTED,"reject final proof");
    testRequire(xrtFutureWaitFor(F,15000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(F))->Status==(Challenge?XMQTT_STATUS_CLOSED:XMQTT_STATUS_AUTHENTICATION),"initial proof failure retained");
    testRequire(xmqttReconnectWaitFor(R,10)==XWAIT_TIMEOUT,"logical closure cannot consume held auth ticket");xmqttclientlease L={0};xmqttmessage* M=(xmqttmessage*)(uintptr_t)1;testRequire(!xmqttReconnectAcquire(R,&L) && !xmqttReconnectTakeMessage(R,&M) && M==(xmqttmessage*)(uintptr_t)1,"failed proof never publishes lease/message");
    if(Challenge)testRequire(rtResponse(T)!=XMQTT_ADMISSION_ACCEPTED,"stale valid challenge response cannot revive stopped generation");
    xmqttAuthRelease(T);
    testRequire(xmqttReconnectWaitFor(R,15000)==XWAIT_OK,"real ticket release permits retirement");xmqttreconnectinfo I;testRequire(xmqttReconnectInfo(R,&I) && !I.Connections && I.Attempts==1 && I.LastStatus==(Challenge?XMQTT_STATUS_CLOSED:XMQTT_STATUS_AUTHENTICATION) && I.State==(Challenge?XMQTT_RECONNECT_STOPPED:XMQTT_RECONNECT_FAILED),"no retry of default authentication failure");rtStop(&S,R);xrtFutureDestroy(F);
}
static void rtPendingSetup(unsigned Factory,unsigned Flags)
{
    rtState S;rtStart(&S,Factory,Flags);xmqttreconnect* R=rtCreate(&S,200);
    if(Flags&MQTT_TLS_BLOCK_DNS){mqttTlsWait(&S.Peer.DnsEntered,1,"independent resolver is blocked");testRequire(!xrtAtomic32Load(&S.Peer.VerifierDrops,XMEMORY_ACQUIRE),"prepared TLS retains verifier before DNS");}
    testRequire(xmqttReconnectWaitFor(R,15000)==XWAIT_OK,"one outage deadline retires pending DNS/TLS setup");xmqttreconnectinfo I;testRequire(xmqttReconnectInfo(R,&I) && I.State==XMQTT_RECONNECT_FAILED && I.LastStatus==XMQTT_STATUS_TIMEOUT && !I.Connections && I.Attempts==1,"no false online after timeout");testRequire(xrtAtomic32Load(&S.Peer.VerifierDrops,XMEMORY_ACQUIRE)==1,"all prepared/attached verifier references retired");rtStop(&S,R);
}
static void rtRejectedTls(unsigned Factory)
{
    rtState S;rtStart(&S,Factory,MQTT_TLS_REJECT);xmqttreconnect* R=rtCreate(&S,5000);testRequire(xmqttReconnectWaitFor(R,15000)==XWAIT_OK,"rejected transport retires");xmqttreconnectinfo I;testRequire(xmqttReconnectInfo(R,&I) && I.State==XMQTT_RECONNECT_FAILED && I.LastStatus==XMQTT_STATUS_TRANSPORT && I.Attempts==1 && !I.Connections,"failed TLS verification cannot publish ONLINE");
    xmqttclientlease L={0};xmqttmessage* M=(xmqttmessage*)(uintptr_t)1;testRequire(!xmqttReconnectAcquire(R,&L) && !xmqttReconnectTakeMessage(R,&M) && M==(xmqttmessage*)(uintptr_t)1 && !xrtAtomicPtrLoad(&S.Ticket,XMEMORY_ACQUIRE),"no MQTT state escapes rejected transport");testRequire(xrtAtomic32Load(&S.Peer.VerifierDrops,XMEMORY_ACQUIRE)==1,"rejected verifier retired exactly once");rtStop(&S,R);
}
int main(void)
{
    for(unsigned F=0;F<3;++F){rtSuccess(F);rtDenied(F,false);rtDenied(F,true);rtPendingSetup(F,MQTT_TLS_SILENT);rtRejectedTls(F);}
    rtPendingSetup(1,MQTT_TLS_BLOCK_DNS);puts("[PASS] automatic TLS/Dial/Auth: two generations, final-proof quarantine, reauth, held-ticket stop, TLS/DNS deadlines and physical joins");return 0;
}
