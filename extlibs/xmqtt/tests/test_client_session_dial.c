#include "test.h"
#include "../examples/common/session_dial_peer.h"
static const void* sdResult(xfuture* F)
{testRequire(F && xrtFutureWaitFor(F,12000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"Session Dial typed result");return xrtFutureValue(F);}
static xmqttsessioninfo sdInfo(xmqttsession* S)
{xmqttsessioninfo I;testRequire(xmqttSessionInfo(S,&I),"Session Dial gauges");return I;}
static void sdPair(xmqttversion V,xmqttsession** S,xmqttclientinbox** I)
{xmqttsessionconfig K;xmqttSessionConfigInit(&K);K.Version=V;K.ClientId=XRT_STR_LITERAL("session-dial");xmqttclientinboxconfig B;xmqttClientInboxConfigInit(&B);B.Version=V;testRequire(xmqttSessionCreate(&K,S) && xmqttClientInboxCreate(&B,I),"Session Dial passive owners");}
static void sdClose(xmqttclient* C)
{testRequire(xmqttClientClose(C) && xmqttClientWaitFor(C,12000)==XWAIT_OK && xmqttClientDestroy(C),"Session Dial physical close");}
static void sdDone(xmqttsession* S,xmqttclient* C)
{double End=xrtTimer()+12;for(;;){xmqttclientinfo I;testRequire(xmqttClientInfo(C,&I),"Session Dial request gauges");xmqttsessioninfo Q=sdInfo(S);if(!Q.Objects && !Q.ActiveOutgoing && !Q.Incoming && !I.Requests)break;testRequire(I.State==XMQTT_CLIENT_OPEN && xrtTimer()<End,"Session Dial native/protocol drain");xrtSleep(1);}}
static xmqttpacket sdConnectPacket(xmqttversion V,bool Auth,xmqttproperty Props[2])
{
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=XRT_STR_LITERAL("session-dial");
    if(V==XMQTT_VERSION_5){Props[0]=mqttResumeNumber(XMQTT_PROPERTY_SESSION_EXPIRY,60);Q.Properties=(xmqttpropertylist){Props,1};}
    if(Auth){Props[1]=(xmqttproperty){0};Props[1].Id=XMQTT_PROPERTY_AUTH_METHOD;Props[1].Type=XMQTT_PROPERTY_STRING;Props[1].Value.Text=XRT_STR_LITERAL("m");Q.Properties.Count=2;}
    return Q;
}
static bool sdFactory(mqttSessionDialPeer* P,unsigned Factory,const xmqttclientconfig* K,const void* Auth,
    xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,xmqttclient** C)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL)
    xmqttclientdialconfig D;xmqttClientDialConfigInit(&D);D.FallbackDelay=1;D.Timeout=0;
    if(Factory==1){char Host[12];memcpy(Host,"mqtt.fixture",12);bool OK;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
        if(P->Auth)OK=xmqttClientDialSessionAuth(P->Peer.Engine,P->Resolver,(xstrview){Host,12},P->Peer.Address.Port,1,K,&D,Auth,S,I,Q,C);else
#endif
        OK=xmqttClientDialSession(P->Peer.Engine,P->Resolver,(xstrview){Host,12},P->Peer.Address.Port,1,K,&D,S,I,Q,C);
        memset(Host,'z',sizeof(Host));memset(&D,0,sizeof(D));return OK;}
    if(Factory==2){xnetaddr A[]={P->RefusedAddress,P->Peer.Address};xnetaddrlist* L=xrtNetAddrListCreate(A,2);testRequire(L!=NULL,"Session prepared endpoints");bool OK;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
        if(P->Auth)OK=xmqttClientDialAddressesSessionAuth(P->Peer.Engine,L,1,K,&D,Auth,S,I,Q,C);else
#endif
        OK=xmqttClientDialAddressesSession(P->Peer.Engine,L,1,K,&D,S,I,Q,C);
        xrtNetAddrListDestroy(L);memset(&D,0,sizeof(D));return OK;}
#endif
    (void)Factory;(void)Auth;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(P->Auth)return xmqttClientConnectSessionAuth(P->Peer.Engine,&P->Peer.Address,1,K,Auth,S,I,Q,C);
#endif
    return xmqttClientConnectSession(P->Peer.Engine,&P->Peer.Address,1,K,S,I,Q,C);
}
static void sdDecide(xmqttclient* C,xfuture* F,xmqttsession* S,bool Auth,bool Accept,bool Reauth)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(Auth){double End=xrtTimer()+12;while(!xrtFutureDone(F)){xmqttauthticket* T=NULL;if(xmqttClientTakeAuth(C,&T)){
            testRequire(sdInfo(S).Connected==Reauth,"Session open waits for initial proof only");
            if(xmqttAuthPhase(T)==XMQTT_AUTH_CHALLENGE){xmqttproperty P[2]={{0}};char Method='m';unsigned char Data='D';P[0].Id=XMQTT_PROPERTY_AUTH_METHOD;P[0].Type=XMQTT_PROPERTY_STRING;P[0].Value.Text=(xstrview){&Method,1};P[1].Id=XMQTT_PROPERTY_AUTH_DATA;P[1].Type=XMQTT_PROPERTY_BINARY;P[1].Value.Binary=(xbytesview){&Data,1};xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_AUTH,&Q);Q.Reason=0x18;Q.Properties=(xmqttpropertylist){P,2};testRequire(xmqttAuthRespond(T,&Q)==XMQTT_ADMISSION_ACCEPTED,"Session Dial immutable auth response");Method='z';Data=0;}
            else {testRequire(xmqttAuthPhase(T)==(Reauth?XMQTT_AUTH_VERIFY_REAUTH:XMQTT_AUTH_VERIFY_CONNECT),"Session proof phase");testRequire(xmqttAuthComplete(T,Accept)==XMQTT_ADMISSION_ACCEPTED,"Session local proof decision");}xmqttAuthRelease(T);
        }else {testRequire(xrtTimer()<End,"Session Auth ticket deadline");xrtSleep(1);}}}
#else
    (void)C;(void)F;(void)S;(void)Auth;(void)Accept;(void)Reauth;
#endif
}
static xmqttclient* sdConnect(mqttSessionDialPeer* P,unsigned Factory,bool Tiny,const xmqttclientconfig* Given,xmqttsession* S,xmqttclientinbox* I,xfuture** F)
{
    xmqttclientconfig K;if(Given)K=*Given;else xmqttClientConfigInit(&K);K.Version=P->Peer.Version;if(Tiny){K.ReadBytes=K.WriteBytes=K.WriteChunkBytes=1;K.WorkPackets=1;}
    xmqttproperty Props[2];xmqttpacket Q=sdConnectPacket(K.Version,P->Auth,Props);xmqttclient* C=NULL;
    testRequire(sdFactory(P,Factory,&K,NULL,S,I,&Q,&C),"Session Dial/Auth factory");memset(Props,0,sizeof(Props));memset(&K,0,sizeof(K));memset(&Q,0,sizeof(Q));*F=xmqttClientReady(C);return C;
}
static void sdReady(xmqttclient* C,xfuture* F,xmqttsession* S,bool Auth,bool Present)
{sdDecide(C,F,S,Auth,true,false);const xmqttreadyresult* R=sdResult(F);if(R->Status!=XMQTT_STATUS_OK)fprintf(stderr,"Session Dial readiness status=%d cause=%s\n",R->Status,R->Cause?xrtErrorMessage(R->Cause):"none");testRequire(R->Status==XMQTT_STATUS_OK && R->Negotiated.SessionPresent==Present && sdInfo(S).Connected,"Session Dial accepted readiness");}
static void sdRoundtrip(xmqttversion V,unsigned Factory,unsigned Phase,bool Tiny,bool Auth)
{
    xmqttsession* S;xmqttclientinbox* I;sdPair(V,&S,&I);mqttSessionDialPeer P;mqttSessionDialStart(&P,V,Phase==0?MQTT_RESUME_HOLD_ACK:Phase==1?MQTT_RESUME_HOLD_REC:MQTT_RESUME_HOLD_COMP,false,Auth,true,0);
    xfuture *Ready,*Next,*F=NULL;xmqttclient* C=sdConnect(&P,Factory,Tiny,NULL,S,I,&Ready);sdReady(C,Ready,S,Auth,false);
    unsigned char Payload[257];for(size_t J=0;J<sizeof(Payload);++J)Payload[J]=(unsigned char)(J*37u+5u);xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=Phase?4:2;Q.Identifier=23;Q.Body.Publish.Topic=XRT_STR_LITERAL("dial/recover");Q.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};
    testRequire(xmqttClientPublishAsync(C,&Q,10000,&F)==XMQTT_ADMISSION_ACCEPTED,"Session Dial outgoing snapshot");memset(Payload,0,sizeof(Payload));mqttResumeWait(&P.Peer.Publishes,1);if(Phase==2)mqttResumeWait(&P.Peer.Releases,1);mqttResumeWait(&P.Peer.Pubrecs,1);
    sdClose(C);mqttSessionDialStop(&P);const xmqttwriteresult* Old=sdResult(F);testRequire(Old->Status==XMQTT_STATUS_CLOSED && Old->AcceptedBytes && Old->PubrecReceived==(Phase==2) && sdInfo(S).ActiveOutgoing==1 && sdInfo(S).Incoming==1,"Session obligations outlive original Engine/Future");
    mqttSessionDialStart(&P,V,0,true,Auth,true,0);C=sdConnect(&P,Factory,Tiny,NULL,S,I,&Next);sdReady(C,Next,S,Auth,true);sdDone(S,C);
    if(Phase==2){mqttResumeWait(&P.Peer.Releases,1);testRequire(!xrtAtomic32Load(&P.Peer.Publishes,XMEMORY_ACQUIRE),"resumed Dial sends PUBREL only");}
    else {mqttResumeWait(&P.Peer.Publishes,1);testRequire(P.Peer.Log[0].Id==23 && (P.Peer.Log[0].Flags&8) && P.Peer.Log[0].Topic==12 && P.Peer.Log[0].Payload==257 && P.Peer.Log[0].First==5 && !P.Peer.Log[0].Alias,"Dial canonical id/DUP/full topic and copied payload");}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(Auth && !Tiny){uint64 Generation=sdInfo(S).Generation;xmqttproperty M={0};M.Id=XMQTT_PROPERTY_AUTH_METHOD;M.Type=XMQTT_PROPERTY_STRING;M.Value.Text=XRT_STR_LITERAL("m");xmqttPacketInit(XMQTT_PACKET_AUTH,&Q);Q.Reason=0x19;Q.Properties=(xmqttpropertylist){&M,1};xfuture* A=NULL;testRequire(xmqttClientReauthenticateAsync(C,&Q,10000,&A)==XMQTT_ADMISSION_ACCEPTED,"attached Session reauthentication");sdDecide(C,A,S,true,true,true);testRequire(((const xmqttauthresult*)sdResult(A))->Status==XMQTT_STATUS_OK && sdInfo(S).Generation==Generation,"reauthentication never reopens Session");xrtFutureDestroy(A);}
#endif
    xmqttmessage *Message=NULL,*Other=NULL;testRequire(xmqttClientInboxTake(I,&Message) && !xmqttClientInboxTake(I,&Other),"Dial resumed QoS2 delivered once");sdClose(C);mqttSessionDialStop(&P);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"Dial passive owners retire");
    testRequire(xmqttMessagePacket(Message)->Body.Publish.Payload.Size==3 && !memcmp(xmqttMessagePacket(Message)->Body.Publish.Payload.Data,"x\0z",3) && Old->Status==XMQTT_STATUS_CLOSED,"Dial result/message survive every owner");xmqttMessageRelease(Message);xrtFutureDestroy(F);xrtFutureDestroy(Ready);xrtFutureDestroy(Next);
}

#include "client_session_dial_cases.h"
int main(void)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL)
    for(unsigned V=0;V<2;++V)for(unsigned F=1;F<3;++F)for(unsigned P=0;P<3;++P)for(unsigned T=0;T<2;++T)sdRoundtrip(V?XMQTT_VERSION_311:XMQTT_VERSION_5,F,P,T!=0,false);
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    for(unsigned F=0;F<
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL)
        3
#else
        1
#endif
        ;++F)for(unsigned P=0;P<3;++P)for(unsigned T=0;T<2;++T)sdRoundtrip(XMQTT_VERSION_5,F,P,T!=0,true);
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL)
    for(unsigned A=0;A<5;++A)sdSetupFailure(false,A);
    for(unsigned F=1;F<3;++F){sdConstructors(false,F);sdAllocation(false,F);sdClaims(false,F);}
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL)
    for(unsigned A=0;A<5;++A)sdSetupFailure(true,A);
    const unsigned Factories=3;
#else
    const unsigned Factories=1;
#endif
    for(unsigned F=0;F<Factories;++F){for(unsigned A=0;A<4;++A)sdProofFailure(F,A);sdConstructors(true,F);sdAllocation(true,F);sdClaims(true,F);}
#endif
    puts("[PASS] Session Dial/Auth: two Engines, three QoS stages, proof gates, reauthentication, dedup and lifetime");return 0;
}
