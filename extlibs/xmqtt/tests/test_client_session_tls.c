#include "test.h"
#include "support/tls_peer.h"
typedef struct stLog {unsigned Flags,Id;size_t Topic,Payload;bool Alias;} stLog;
typedef struct stPeer {mqttTlsPeer TLS;bool Present,Incoming;unsigned Hold;stLog Log[8];xatomic32 Publishes,Releases,Comps;} stPeer;
static void stConnack(xtlsstream* S,mqttTlsPeer* P)
{
    stPeer* T=P->FrameData;unsigned char A[]={0x20,6,T->Present?1:0,0,3,0x22,0,2};
    unsigned char B[]={0x20,2,T->Present?1:0,0};
    unsigned char Auth[]={0x20,14,T->Present?1:0,0,11,0x15,0,1,'m',0x16,0,1,0x43,0x22,0,2};
    mqttTlsSend(S,P->Auth?Auth:P->Version==XMQTT_VERSION_5?A:B,P->Auth?sizeof(Auth):P->Version==XMQTT_VERSION_5?sizeof(A):sizeof(B));
    if(T->Incoming){unsigned char I5[]={T->Present?0x3c:0x34,10,0,2,'i','n',0,77,0,'x',0,'z'};
        unsigned char I311[]={T->Present?0x3c:0x34,9,0,2,'i','n',0,77,'x',0,'z'};
        mqttTlsSend(S,P->Version==XMQTT_VERSION_5?I5:I311,P->Version==XMQTT_VERSION_5?sizeof(I5):sizeof(I311));}
}
static void stFrame(xtlsstream* S,mqttTlsPeer* P,unsigned char Flags,const unsigned char* B,size_t N)
{
    stPeer* T=P->FrameData;unsigned Type=Flags>>4;
    if(Type==1){testRequire(N>8 && !(B[7]&2),"persistent CONNECT");if(P->Auth){static const unsigned char A[]={0xf0,10,0x18,8,0x15,0,1,'m',0x16,0,1,0x42};mqttTlsSend(S,A,sizeof(A));}else stConnack(S,P);}
    else if(Type==15){testRequire(P->Auth && N>=2,"TLS Session auth frame");if(B[0]==0x19){static const unsigned char A[]={0xf0,6,0,4,0x15,0,1,'m'};mqttTlsSend(S,A,sizeof(A));}else {testRequire(N==10 && B[9]==0x44,"TLS Session challenge response");stConnack(S,P);}}
    else if(Type==3){testRequire(N>=2,"TLS Session topic");size_t Topic=((size_t)B[0]<<8)|B[1],At=Topic+2;unsigned Qos=(Flags>>1)&3;
        testRequire(At<=N && Qos && At+2<=N,"TLS Session publish bounds");unsigned Id=((unsigned)B[At]<<8)|B[At+1];At+=2;bool Alias=false;
        if(P->Version==XMQTT_VERSION_5){testRequire(At<N && B[At]<128,"small independent properties");size_t Properties=B[At++];testRequire(Properties<=N-At && (Properties==0 || (Properties==3 && B[At]==0x23 && B[At+1]==0 && B[At+2]==1)),"literal canonical/alias properties");Alias=Properties!=0;At+=Properties;}
        testRequire(N-At==257,"TLS Session binary payload length");for(size_t I=0;I<257;++I)testRequire(B[At+I]==(uint8)(I*37u),"TLS Session immutable binary snapshot");
        unsigned Index=xrtAtomic32Load(&T->Publishes,XMEMORY_RELAXED);testRequire(Index<8,"bounded TLS Session trace");T->Log[Index]=(stLog){Flags,Id,Topic,N-At,Alias};
        if(T->Hold!=1){unsigned char A[]={Qos==1?0x40:0x50,2,(uint8)(Id>>8),(uint8)Id};mqttTlsSend(S,A,sizeof(A));}
        xrtAtomic32FetchAdd(&T->Publishes,1,XMEMORY_RELEASE);
    }else if(Type==6){testRequire(N==2,"TLS Session PUBREL");if(T->Hold!=2){unsigned char A[]={0x70,2,B[0],B[1]};mqttTlsSend(S,A,sizeof(A));}xrtAtomic32FetchAdd(&T->Releases,1,XMEMORY_RELEASE);}
    else if(Type==5){testRequire(N==2 && B[0]==0 && B[1]==77,"incoming PUBREC identity");if(T->Present || !T->Hold){static const unsigned char A[]={0x62,2,0,77};mqttTlsSend(S,A,sizeof(A));}}
    else if(Type==7){testRequire(N==2 && B[0]==0 && B[1]==77,"incoming PUBCOMP identity");xrtAtomic32FetchAdd(&T->Comps,1,XMEMORY_RELEASE);}
    else mqttTlsFrame(S,P,Flags,B,N);
}
static void stStart(stPeer* P,xmqttversion V,unsigned Flags,bool Present,unsigned Hold,bool Incoming)
{memset(P,0,sizeof(*P));P->Present=Present;P->Hold=Hold;P->Incoming=Incoming;mqttTlsStartWith(&P->TLS,V,Flags,stFrame,P);}
static const void* stResult(xfuture* F)
{testRequire(F && xrtFutureWaitFor(F,15000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"TLS Session typed result");return xrtFutureValue(F);}
static xmqttsessioninfo stInfo(xmqttsession* S)
{xmqttsessioninfo I;testRequire(xmqttSessionInfo(S,&I),"TLS Session gauges");return I;}
static void stPair(xmqttversion V,xmqttsession** S,xmqttclientinbox** I)
{xmqttsessionconfig K;xmqttSessionConfigInit(&K);K.Version=V;K.ClientId=XRT_STR_LITERAL("tls-session");testRequire(xmqttSessionCreate(&K,S),"TLS passive Session");xmqttclientinboxconfig B;xmqttClientInboxConfigInit(&B);B.Version=V;testRequire(xmqttClientInboxCreate(&B,I),"TLS passive Inbox");}
static void stClose(xmqttclient* C)
{testRequire(xmqttClientClose(C) && xmqttClientWaitFor(C,15000)==XWAIT_OK && xmqttClientDestroy(C),"TLS Session physical retirement");}
static void stDone(xmqttsession* S,xmqttclient* C)
{double End=xrtTimer()+15;for(;;){xmqttsessioninfo I=stInfo(S);xmqttclientinfo CI;testRequire(xmqttClientInfo(C,&CI),"TLS native request gauges");if(!I.ActiveOutgoing && !I.Objects && !I.Incoming && !I.Prepared && !CI.Requests)break;testRequire(CI.State==XMQTT_CLIENT_OPEN && xrtTimer()<End,"TLS Session/native obligations drained");xrtSleep(1);}}
static xmqttclient* stConnect(stPeer* P,xmqttsession* S,xmqttclientinbox* I,unsigned Factory,bool Tiny,const xmqttclientconfig* Given,xfuture** Ready)
{
    xmqttclientconfig K;if(Given)K=*Given;else xmqttClientConfigInit(&K);K.Version=P->TLS.Version;if(Tiny){K.WriteBytes=K.WriteChunkBytes=K.ReadBytes=1;K.WorkPackets=1;}
    xmqttclienttlsconfig T;char Name[12],Alpn[4];xstrview Protocol;xtlsverifier* V=mqttTlsConfigure(&P->TLS,&T,Name,&Protocol,Alpn);
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=false;Q.Body.Connect.ClientId=XRT_STR_LITERAL("tls-session");xmqttproperty Props[2]={{0}};
    if(K.Version==XMQTT_VERSION_5){Props[0].Id=XMQTT_PROPERTY_SESSION_EXPIRY;Props[0].Type=XMQTT_PROPERTY_UINT32;Props[0].Value.Number=60;Q.Properties=(xmqttpropertylist){Props,1};}
    if(P->TLS.Auth){Props[1].Id=XMQTT_PROPERTY_AUTH_METHOD;Props[1].Type=XMQTT_PROPERTY_STRING;Props[1].Value.Text=XRT_STR_LITERAL("m");Q.Properties.Count=2;}
    xmqttclient* C=NULL;bool OK=false;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(P->TLS.Auth){if(!Factory)OK=xmqttClientTlsConnectSessionAuth(P->TLS.Engine,&P->TLS.Address,1,&K,&T,NULL,S,I,&Q,&C);
        else if(Factory==1)OK=xmqttClientTlsDialSessionAuth(P->TLS.Engine,P->TLS.Resolver,XRT_STR_LITERAL("example.com"),P->TLS.Address.Port,1,&K,&T,NULL,S,I,&Q,&C);
        else {xnetaddrlist* L=xrtNetAddrListCreate(&P->TLS.Address,1);testRequire(L!=NULL,"TLS Session Auth endpoints");OK=xmqttClientTlsDialAddressesSessionAuth(P->TLS.Engine,L,1,&K,&T,NULL,S,I,&Q,&C);xrtNetAddrListDestroy(L);}}
    else
#endif
    if(!Factory)OK=xmqttClientTlsConnectSession(P->TLS.Engine,&P->TLS.Address,1,&K,&T,S,I,&Q,&C);
    else if(Factory==1){char Host[11];memcpy(Host,"example.com",11);T.Session.ServerName=(xstrview){0};OK=xmqttClientTlsDialSession(P->TLS.Engine,P->TLS.Resolver,(xstrview){Host,11},P->TLS.Address.Port,1,&K,&T,S,I,&Q,&C);memset(Host,'z',sizeof(Host));}
    else {xnetaddrlist* L=xrtNetAddrListCreate(&P->TLS.Address,1);testRequire(L!=NULL,"TLS Session endpoints");OK=xmqttClientTlsDialAddressesSession(P->TLS.Engine,L,1,&K,&T,S,I,&Q,&C);xrtNetAddrListDestroy(L);}
    xrtTlsVerifierRelease(V);memset(Name,'x',sizeof(Name));memset(Alpn,'y',sizeof(Alpn));Protocol=(xstrview){0};testRequire(OK,"TLS Session factory");*Ready=xmqttClientReady(C);return C;
}
static void stDecide(xmqttclient* C,xfuture* F,xmqttsession* S,bool Auth,bool Accept)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(Auth){double End=xrtTimer()+15;while(!xrtFutureDone(F)){xmqttauthticket* T=NULL;
        if(xmqttClientTakeAuth(C,&T)){if(S){testRequire(!stInfo(S).Connected,"MQTT Session waits for final proof gate");}
            else {xmqttclientinfo I;xmqttmessage* M=(xmqttmessage*)(uintptr_t)1;testRequire(xmqttClientInfo(C,&I) && !I.Ready && !xmqttClientTakeMessage(C,&M) && M==(xmqttmessage*)(uintptr_t)1,"ordinary Client keeps post-CONNACK data quarantined before proof");}
            if(xmqttAuthPhase(T)==XMQTT_AUTH_CHALLENGE){xmqttproperty P[2]={{0}};P[0].Id=XMQTT_PROPERTY_AUTH_METHOD;P[0].Type=XMQTT_PROPERTY_STRING;P[0].Value.Text=XRT_STR_LITERAL("m");P[1].Id=XMQTT_PROPERTY_AUTH_DATA;P[1].Type=XMQTT_PROPERTY_BINARY;P[1].Value.Binary=XRT_BYTES_LITERAL("D");xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_AUTH,&Q);Q.Reason=0x18;Q.Properties=(xmqttpropertylist){P,2};testRequire(xmqttAuthRespond(T,&Q)==XMQTT_ADMISSION_ACCEPTED,"Session challenge response");}
            else {testRequire(xmqttAuthComplete(T,Accept)==XMQTT_ADMISSION_ACCEPTED,"Session final proof decision");}xmqttAuthRelease(T);
        }else {testRequire(xrtTimer()<End,"TLS Session auth deadline");xrtSleep(1);}}}
#else
    (void)C;(void)S;(void)Auth;(void)Accept;(void)F;
#endif
}
static void stReady(xmqttclient* C,xfuture* F,xmqttsession* S,bool Present,bool Auth)
{
    stDecide(C,F,S,Auth,true);
    const xmqttreadyresult* R=stResult(F);
    if(R->Status!=XMQTT_STATUS_OK)fprintf(stderr,"TLS Session readiness: status=%d present=%u auth=%u cause=%s\n",R->Status,Present?1u:0u,Auth?1u:0u,R->Cause?xrtErrorMessage(R->Cause):"none");
    testRequire(R->Status==XMQTT_STATUS_OK && R->Negotiated.SessionPresent==Present,"TLS Session accepted CONNACK");
}
static xfuture* stPublish(xmqttclient* C,xmqttversion V,unsigned Qos,bool Alias,bool Compact,unsigned Id)
{
    uint8 Payload[257];for(size_t J=0;J<sizeof(Payload);++J)Payload[J]=(uint8)(J*37u);
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=(uint8)(Qos<<1);Q.Identifier=(uint16)Id;Q.Body.Publish.Topic=Compact?(xstrview){0}:XRT_STR_LITERAL("tls/topic");Q.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};xmqttproperty P={0};
    if(V==XMQTT_VERSION_5 && Alias){P.Id=XMQTT_PROPERTY_TOPIC_ALIAS;P.Type=XMQTT_PROPERTY_UINT16;P.Value.Number=1;Q.Properties=(xmqttpropertylist){&P,1};}
    xfuture* F=NULL;
#if defined(XMQTT_SESSION_TLS_COPY_TEST)
    xrtAtomic32Store(&stCopyCount,0,XMEMORY_RELAXED);
#endif
    testRequire(xmqttClientPublishAsync(C,&Q,10000,&F)==XMQTT_ADMISSION_ACCEPTED,"TLS Session publication snapshot");
#if defined(XMQTT_SESSION_TLS_COPY_TEST)
    testRequire(xrtAtomic32Load(&stCopyCount,XMEMORY_RELAXED)==1,"TLS Session shares one product plaintext snapshot; native record copies are separate");
#endif
    memset(Payload,0,sizeof(Payload));return F;
}
static void stRoundtrip(xmqttversion V,unsigned Factory,unsigned Phase,bool Tiny,bool Auth)
{
    xmqttsession* S;xmqttclientinbox* I;stPair(V,&S,&I);stPeer P;stStart(&P,V,Auth?MQTT_TLS_AUTH:0,false,Phase==2?2:1,true);xfuture *Ready,*Next;xmqttclient* C=stConnect(&P,S,I,Factory,Tiny,NULL,&Ready);stReady(C,Ready,S,false,Auth);
    xfuture* F=stPublish(C,V,Phase?2:1,V==XMQTT_VERSION_5,false,23);mqttTlsWait(&P.Publishes,1,"old TLS PUBLISH observed");if(Phase==2)mqttTlsWait(&P.Releases,1,"old TLS PUBREL observed");
    double End=xrtTimer()+15;while(stInfo(S).Incoming!=1){testRequire(xrtTimer()<End,"old incoming TLS QoS2 committed");xrtSleep(1);}
    stClose(C);mqttTlsStop(&P.TLS);const xmqttwriteresult* Old=stResult(F);testRequire(Old->Status==XMQTT_STATUS_CLOSED && Old->AcceptedBytes && Old->PubrecReceived==(Phase==2) && !Old->PubcompReceived && stInfo(S).ActiveOutgoing==1,"old TLS Future and protocol obligation survive old Engine");
    stStart(&P,V,Auth?MQTT_TLS_AUTH:0,true,0,true);C=stConnect(&P,S,I,Factory,Tiny,NULL,&Next);stReady(C,Next,S,true,Auth);stDone(S,C);
    if(Phase==2){mqttTlsWait(&P.Releases,1,"resumed TLS PUBREL");testRequire(!xrtAtomic32Load(&P.Publishes,XMEMORY_ACQUIRE),"QoS2 never replays completed PUBLISH");}
    else {mqttTlsWait(&P.Publishes,1,"resumed TLS DUP");testRequire(P.Log[0].Id==23 && (P.Log[0].Flags&8) && P.Log[0].Topic==9 && !P.Log[0].Alias,"TLS replay canonical full topic/id/DUP without old alias");}
    if(V==XMQTT_VERSION_5){xfuture* A=stPublish(C,V,1,true,false,24);testRequire(((const xmqttwriteresult*)stResult(A))->Status==XMQTT_STATUS_OK,"four-span shared TLS alias");xrtFutureDestroy(A);A=stPublish(C,V,2,true,true,25);testRequire(((const xmqttwriteresult*)stResult(A))->Status==XMQTT_STATUS_OK,"compact two-span shared TLS alias");xrtFutureDestroy(A);}
    stDone(S,C);xmqttmessage *M=NULL,*Other=NULL;testRequire(xmqttClientInboxTake(I,&M) && !xmqttClientInboxTake(I,&Other),"incoming resumed TLS QoS2 delivered once");
    xfuture* D=NULL;testRequire(xmqttClientDisconnectAsync(C,NULL,10000,&D)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)stResult(D))->Status==XMQTT_STATUS_OK,"TLS Session authenticated graceful close");stClose(C);mqttTlsStop(&P.TLS);
    testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"TLS passive owners after physical close");const xmqttpacket* Held=xmqttMessagePacket(M);testRequire(Held->Body.Publish.Payload.Size==3 && !memcmp(Held->Body.Publish.Payload.Data,"x\0z",3) && Old->Status==XMQTT_STATUS_CLOSED,"TLS retained data after all owners/Engines");xmqttMessageRelease(M);xrtFutureDestroy(D);xrtFutureDestroy(F);xrtFutureDestroy(Ready);xrtFutureDestroy(Next);
}
#include "client_session_tls_cases.h"
int main(int Argc,char** Argv)
{
    bool Only311=Argc==2 && !strcmp(Argv[1],"--311");
    for(unsigned V=Only311?1:0;V<2;++V)for(unsigned F=0;F<3;++F)for(unsigned P=0;P<3;++P)for(unsigned Tiny=0;Tiny<2;++Tiny)stRoundtrip(V?XMQTT_VERSION_311:XMQTT_VERSION_5,F,P,Tiny!=0,false);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(!Only311)for(unsigned F=0;F<3;++F)for(unsigned P=0;P<3;++P)stRoundtrip(XMQTT_VERSION_5,F,P,false,true);
    if(!Only311){stAuthFailure();stOrdinaryAuth();}
#endif
    if(!Only311){stFailures(XMQTT_VERSION_5);stFailures(XMQTT_VERSION_311);stCtor();stAllocation();}
    puts("[PASS] MQTT TLS Session: factories, proof gates, canonical replay, shared aliases, incoming dedup, ciphertext and physical lifetime");return 0;
}
