#include "support/ws_reconnect_peer.h"
static xmqttauthticket* wrTicket(wrService* S,xmqttauthphase Phase)
{double End=xrtTimer()+15;xmqttauthticket* T;while(!(T=xrtAtomicPtrExchange(&S->Ticket,NULL,XMEMORY_ACQ_REL))){testRequire(xrtTimer()<End,"bounded application WS auth ticket");xrtSleep(1);}testRequire(xmqttAuthPhase(T)==Phase,"expected automatic WS proof phase");return T;}
static xmqttclientadmission wrRespond(xmqttauthticket* T)
{xmqttproperty P[2]={{0}};P[0].Id=XMQTT_PROPERTY_AUTH_METHOD;P[0].Type=XMQTT_PROPERTY_STRING;P[0].Value.Text=XRT_STR_LITERAL("m");P[1].Id=XMQTT_PROPERTY_AUTH_DATA;P[1].Type=XMQTT_PROPERTY_BINARY;P[1].Value.Binary=XRT_BYTES_LITERAL("D");xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_AUTH,&Q);Q.Reason=0x18;Q.Properties=(xmqttpropertylist){P,2};return xmqttAuthRespond(T,&Q);}
static xfuture* wrProve(wrService* S,xmqttreconnect* R,uint64 Generation,xmqttclientlease* L)
{
    xmqttauthticket* T=wrTicket(S,XMQTT_AUTH_CHALLENGE);xmqttclient* C=xrtAtomicPtrLoad(&S->Client,XMEMORY_ACQUIRE);xfuture* Ready=xmqttClientReady(C);xmqttmessage* M=(xmqttmessage*)(uintptr_t)1;xmqttreconnectinfo I;
    testRequire(!xmqttReconnectAcquire(R,L)&&!xmqttClientInboxTake(S->Inbox,&M)&&M==(xmqttmessage*)(uintptr_t)1&&!xrtFutureDone(Ready),"initial WS challenge quarantines ordinary leases and data");testRequire(wrRespond(T)==XMQTT_ADMISSION_ACCEPTED,"WS challenge response on original generation");xmqttAuthRelease(T);T=wrTicket(S,XMQTT_AUTH_VERIFY_CONNECT);testRequire(!xmqttReconnectAcquire(R,L)&&!xrtFutureDone(Ready)&&xmqttReconnectInfo(R,&I)&&I.Connections==Generation-1,"local final proof still quarantines controller ONLINE");testRequire(xmqttAuthComplete(T,true)==XMQTT_ADMISSION_ACCEPTED,"complete local WS final proof");xmqttAuthRelease(T);wrOnline(R,Generation,L);testRequire(((const xmqttreadyresult*)mqttWsFuture(Ready))->Status==XMQTT_STATUS_OK,"authenticated WS generation accepted");return Ready;
}
static void wrAuth(bool TLS,unsigned Route,xnetportbackend B)
{
    wrService S;wrStart(&S,TLS,XMQTT_VERSION_5,Route,B);xrtAtomic32Store(&S.Auth,1,XMEMORY_RELEASE);xmqttreconnect* R=wrCreate(&S,10000,false);xmqttclientlease L={0};xfuture* First=wrProve(&S,R,1,&L);xwsstream* Peer=xrtAtomicPtrLoad(&S.Connections[0].Peer.Base.Server,XMEMORY_ACQUIRE);testRequire(xrtWsStreamAbort(Peer)&&xmqttClientWaitFor(L.Client,15000)==XWAIT_OK&&xmqttReconnectRelease(&L),"authenticated first-generation outage");xfuture* Second=wrProve(&S,R,2,&L);
    xmqttproperty Method={0};Method.Id=XMQTT_PROPERTY_AUTH_METHOD;Method.Type=XMQTT_PROPERTY_STRING;Method.Value.Text=XRT_STR_LITERAL("m");xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_AUTH,&Q);Q.Reason=0x19;Q.Properties=(xmqttpropertylist){&Method,1};xfuture* Auth=NULL;testRequire(xmqttClientReauthenticateAsync(L.Client,&Q,10000,&Auth)==XMQTT_ADMISSION_ACCEPTED,"reauthenticate automatic WS generation");xmqttauthticket* T=wrTicket(&S,XMQTT_AUTH_VERIFY_REAUTH);xmqttclientlease Other={0};testRequire(xmqttReconnectAcquire(R,&Other)&&Other.Generation==2,"ordinary lease stays on accepted generation during reauth proof");xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("ws/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");xfuture* Sent=NULL;testRequire(xmqttClientPublishAsync(Other.Client,&Q,10000,&Sent)==XMQTT_ADMISSION_ACCEPTED&&((const xmqttwriteresult*)mqttWsFuture(Sent))->Status==XMQTT_STATUS_OK,"ordinary WS traffic remains direct during reauth");testRequire(xmqttReconnectRelease(&Other)&&xmqttAuthComplete(T,true)==XMQTT_ADMISSION_ACCEPTED,"accepted reauth proof");xmqttAuthRelease(T);testRequire(((const xmqttauthresult*)mqttWsFuture(Auth))->Status==XMQTT_STATUS_OK&&xmqttReconnectRelease(&L),"reauth result retains ordinary Client outcome");wrStop(&S,R);testRequire(((const xmqttreadyresult*)xrtFutureValue(First))->Status==XMQTT_STATUS_OK&&((const xmqttreadyresult*)xrtFutureValue(Second))->Negotiated.SessionPresent,"authenticated outcomes survive all physical owners");xrtFutureDestroy(First);xrtFutureDestroy(Second);xrtFutureDestroy(Auth);xrtFutureDestroy(Sent);
}
static void wrAuthDeny(bool TLS,bool StopChallenge,xnetportbackend B)
{
    wrService S;wrStart(&S,TLS,XMQTT_VERSION_5,4,B);xrtAtomic32Store(&S.Auth,1,XMEMORY_RELEASE);xmqttreconnect* R=wrCreate(&S,10000,false);xmqttauthticket* T=wrTicket(&S,XMQTT_AUTH_CHALLENGE);
    if(!StopChallenge){testRequire(wrRespond(T)==XMQTT_ADMISSION_ACCEPTED,"deny fixture challenge first");xmqttAuthRelease(T);T=wrTicket(&S,XMQTT_AUTH_VERIFY_CONNECT);}
    xmqttclient* C=xrtAtomicPtrLoad(&S.Client,XMEMORY_ACQUIRE);xfuture* F=xmqttClientReady(C);if(StopChallenge)testRequire(xmqttReconnectClose(R),"close while real challenge is held");else testRequire(xmqttAuthComplete(T,false)==XMQTT_ADMISSION_ACCEPTED,"deny final WS proof");testRequire(((const xmqttreadyresult*)mqttWsFuture(F))->Status==(StopChallenge?XMQTT_STATUS_CLOSED:XMQTT_STATUS_AUTHENTICATION),"typed WS stop/proof failure");testRequire(xmqttReconnectWaitFor(R,15)==XWAIT_TIMEOUT,"held auth ticket blocks physical controller retirement");xmqttclientlease L={0};testRequire(!xmqttReconnectAcquire(R,&L),"failed proof never creates an online lease");if(StopChallenge)testRequire(wrRespond(T)!=XMQTT_ADMISSION_ACCEPTED,"stale challenge cannot revive stopped generation");xmqttAuthRelease(T);testRequire(xmqttReconnectWaitFor(R,15000)==XWAIT_OK,"ticket release permits actual physical cleanup");xmqttreconnectinfo I;testRequire(xmqttReconnectInfo(R,&I)&&!I.Connections&&I.Attempts==1&&I.LastStatus==(StopChallenge?XMQTT_STATUS_CLOSED:XMQTT_STATUS_AUTHENTICATION),"no retry of default proof rejection");wrStop(&S,R);xrtFutureDestroy(F);
}
#if defined(XMQTT_WR_SINGLE_PRIVATE)
typedef struct wrAlias {xnetpost Post;xmqttclient* Client;xwsstream* Native;xatomic32 Done;} wrAlias;
static void wrCaptureAlias(xnetworker* W,ptr Data)
{(void)W;wrAlias* A=Data;A->Native=xrtWsStreamRef(A->Client->WS->Stream);testRequire(A->Native!=NULL,"actual native WS alias captured on owner");xrtAtomic32Store(&A->Done,1,XMEMORY_RELEASE);}
static void wrAliasBarrier(bool TLS,xnetportbackend B)
{
    wrService S;wrStart(&S,TLS,XMQTT_VERSION_5,3,B);xmqttreconnect* R=wrCreate(&S,10000,false);xmqttclientlease L={0};wrOnline(R,1,&L);wrAlias A={.Client=L.Client};xrtAtomic32Init(&A.Done,0);testRequire(xrtNetPostInit(&A.Post)&&xrtNetPost(xrtNetEngineWorker(S.Engine,1),&A.Post,wrCaptureAlias,&A),"capture physical WS owner");wrWait(&A.Done,1,"native WS alias published");xwsstream* Peer=xrtAtomicPtrLoad(&S.Connections[0].Peer.Base.Server,XMEMORY_ACQUIRE);testRequire(xrtWsStreamAbort(Peer),"alias barrier outage");xrtSleep(30);testRequire(xmqttClientWaitFor(L.Client,30)==XWAIT_TIMEOUT&&xmqttReconnectRelease(&L),"old native alias blocks Client even after application lease release");xrtSleep(30);testRequire(xrtAtomic32Load(&S.Factories,XMEMORY_ACQUIRE)==1,"no replacement while actual old WS/native owner is retained");xrtWsStreamDestroy(A.Native);wrOnline(R,2,&L);testRequire(xmqttReconnectRelease(&L),"replacement after actual native alias release");wrStop(&S,R);
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
        ;++TLS){wrAuth(TLS!=0,0,B);wrAuth(TLS!=0,4,B);wrAuthDeny(TLS!=0,false,B);wrAuthDeny(TLS!=0,true,B);
#if defined(XMQTT_WR_SINGLE_PRIVATE)
        wrAliasBarrier(TLS!=0,B);
#endif
    }puts("[PASS] automatic WS/WSS proof quarantine, reauth, held tickets/native aliases and physical joins");return 0;
}
