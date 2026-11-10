#include "test.h"
#include "../examples/common/session_peer.h"
static const void* csResult(xfuture* F)
{testRequire(F && xrtFutureWaitFor(F,8000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"Client Session typed result");return xrtFutureValue(F);}
static xmqttsessioninfo csInfo(xmqttsession* S)
{xmqttsessioninfo I;testRequire(xmqttSessionInfo(S,&I),"Client Session snapshot");return I;}
static void csPair(xmqttversion V,xmqttsession** S,xmqttclientinbox** I)
{
    xmqttsessionconfig K;testRequire(xmqttSessionConfigInit(&K),"Session defaults");K.Version=V;K.ClientId=XRT_STR_LITERAL("session-client");testRequire(xmqttSessionCreate(&K,S),"Client Session create");
    xmqttclientinboxconfig B;xmqttClientInboxConfigInit(&B);B.Version=V;testRequire(xmqttClientInboxCreate(&B,I),"Client Session Inbox");
}
static xmqttclient* csConnect(mqttResumePeer* P,xmqttsession* S,xmqttclientinbox* I,xfuture** F,const xmqttclientconfig* Config)
{
    xmqttclientconfig K={0};if(Config)K=*Config;else xmqttClientConfigInit(&K);K.Version=P->Version;xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=XRT_STR_LITERAL("session-client");Q.Body.Connect.CleanStart=false;xmqttproperty Expiry=mqttResumeNumber(XMQTT_PROPERTY_SESSION_EXPIRY,60);if(P->Version==XMQTT_VERSION_5)Q.Properties=(xmqttpropertylist){&Expiry,1};
    xmqttclient* C=NULL;testRequire(xmqttClientConnectSession(P->Engine,&P->Address,1,&K,S,I,&Q,&C),"Client Session factory");*F=xmqttClientReady(C);return C;
}
static void csReady(xfuture* F)
{const xmqttreadyresult* R=csResult(F);if(R->Status!=XMQTT_STATUS_OK)fprintf(stderr,"Session readiness status=%d cause=%s\n",R->Status,R->Cause?xrtErrorMessage(R->Cause):"none");testRequire(R->Status==XMQTT_STATUS_OK,"Client Session accepted readiness");}
static void csClose(xmqttclient* C)
{testRequire(xmqttClientClose(C) && xmqttClientWaitFor(C,8000)==XWAIT_OK && xmqttClientDestroy(C),"Client Session real retirement");}
static void csDone(xmqttsession* S,xmqttclient* C)
{
    double End=xrtTimer()+8;for(;;){xmqttclientinfo I;xmqttClientInfo(C,&I);xmqttsessioninfo State=csInfo(S);if(!State.ActiveOutgoing && !State.Objects && !I.Requests)break;testRequire(I.State==XMQTT_CLIENT_OPEN && xrtTimer()<End,"Client Session protocol progress");xrtSleep(1);}
}
static void csOutgoing(xmqttversion V,unsigned Phase,bool Alias)
{
    xmqttsession* S;xmqttclientinbox* I;csPair(V,&S,&I);mqttResumePeer P;
    mqttResumeCaps Caps={0};Caps.MaximumQos=2;Caps.AliasMaximum=Alias?1:0;
    mqttResumeStartWith(&P,V,Phase==0?MQTT_RESUME_HOLD_ACK:Phase==1?MQTT_RESUME_HOLD_REC:MQTT_RESUME_HOLD_COMP,false,&Caps);
    xfuture* Ready;xmqttclient* C=csConnect(&P,S,I,&Ready,NULL);csReady(Ready);
    testRequire(xmqttSessionReset(S)==XMQTT_SESSION_STATE_ERROR && !xmqttSessionDestroy(S),"claimed Session blocks public mutations/destruction");xrtClearError();
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=Phase?4:2;Q.Identifier=23;Q.Body.Publish.Topic=XRT_STR_LITERAL("recover/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("retained payload");xmqttproperty A=mqttResumeNumber(XMQTT_PROPERTY_TOPIC_ALIAS,1);if(Alias)Q.Properties=(xmqttpropertylist){&A,1};
    xfuture* F=NULL;testRequire(xmqttClientPublishAsync(C,&Q,8000,&F)==XMQTT_ADMISSION_ACCEPTED,"Session publication");mqttResumeWait(Phase==2?&P.Releases:&P.Publishes,1);
    testRequire(csInfo(S).ActiveOutgoing==1 && xrtFutureState(F)==XFUTURE_PENDING,"unacknowledged obligation is in Session");
    csClose(C);mqttResumeStop(&P);testRequire(!csInfo(S).Connected && csInfo(S).ActiveOutgoing==1,"obligation outlives old Engine");
    const xmqttwriteresult* W=csResult(F);testRequire(W->Status==XMQTT_STATUS_CLOSED && W->AcceptedBytes && W->Delivery==XMQTT_MAY_HAVE_REACHED_PEER && W->PubrecReceived==(Phase==2),"old Future settles with original connection prefix");
    mqttResumeStart(&P,V,0,true);xfuture* Next;C=csConnect(&P,S,I,&Next,NULL);csReady(Next);csDone(S,C);
    if(Phase==2){mqttResumeWait(&P.Releases,1);testRequire(!xrtAtomic32Load(&P.Publishes,XMEMORY_ACQUIRE) && xrtAtomic32Load(&P.Releases,XMEMORY_ACQUIRE)==1,"PUBREL-only recovery");}
    else {mqttResumeWait(&P.Publishes,1);testRequire(P.Log[0].Id==23 && (P.Log[0].Flags&8u) && P.Log[0].Topic==13 && P.Log[0].Payload==16 && !P.Log[0].Alias,"DUP replay has canonical full topic without old aliases");}
    testRequire(!csInfo(S).Objects,"recovery physical packet references retired");csClose(C);mqttResumeStop(&P);testRequire(xmqttClientInboxDestroy(I) && xmqttSessionDestroy(S),"destroy passive owners before old results");
    W=csResult(F);if(Phase==2)testRequire(W->Pubrec && xmqttMessagePacket(W->Pubrec)->Type==XMQTT_PACKET_PUBREC,"old ACK metadata remains owned");xrtFutureDestroy(F);xrtFutureDestroy(Ready);xrtFutureDestroy(Next);
}
static void csIncomingPacket(mqttResumePeer* P,uint16 Id,bool Duplicate,const char* Payload)
{
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=Duplicate?12:4;Q.Identifier=Id;Q.Body.Publish.Topic=XRT_STR_LITERAL("inbound");Q.Body.Publish.Payload=(xbytesview){(const unsigned char*)Payload,strlen(Payload)};mqttResumePacket(P,&Q);
}
static void csAliasExpansion(void)
{
    xmqttsessionconfig SK;xmqttSessionConfigInit(&SK);SK.ClientId=XRT_STR_LITERAL("session-client");SK.Codec.MaxPacketBytes=64;xmqttsession* S=NULL;xmqttclientinbox* I=NULL;testRequire(xmqttSessionCreate(&SK,&S) && xmqttClientInboxCreate(NULL,&I),"small-packet Session");
    mqttResumeCaps Caps={0};Caps.MaximumQos=2;Caps.AliasMaximum=1;mqttResumePeer P;mqttResumeStartWith(&P,XMQTT_VERSION_5,MQTT_RESUME_HOLD_ACK,false,&Caps);xmqttclientconfig K={0};xmqttClientConfigInit(&K);K.Codec.MaxPacketBytes=K.InitialInputBytes=64;xfuture *Ready,*Next;xmqttclient* C=csConnect(&P,S,I,&Ready,&K);csReady(Ready);
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Body.Publish.Topic=XRT_STR_LITERAL("0123456789abcdef0123456789abcdef");xmqttproperty Alias=mqttResumeNumber(XMQTT_PROPERTY_TOPIC_ALIAS,1);Q.Properties=(xmqttpropertylist){&Alias,1};xfuture* Bind=NULL;testRequire(xmqttClientPublish(C,&Q,8000,&Bind)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)csResult(Bind))->Status==XMQTT_STATUS_OK,"bind full topic within local cap");
    unsigned char Payload[48];memset(Payload,'Z',sizeof(Payload));Q.Flags=2;Q.Identifier=51;Q.Body.Publish.Topic=(xstrview){0};Q.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};xfuture* F=NULL;testRequire(xmqttClientPublishAsync(C,&Q,8000,&F)==XMQTT_ADMISSION_ACCEPTED,"compact logical snapshot");memset(Payload,'X',sizeof(Payload));mqttResumeWait(&P.Publishes,2);testRequire(P.Log[1].Topic==0 && P.Log[1].Alias==1,"actual compact wire fits64");csClose(C);mqttResumeStop(&P);
    mqttResumeStart(&P,XMQTT_VERSION_5,0,true);K.Codec.MaxPacketBytes=128;C=csConnect(&P,S,I,&Next,&K);csReady(Next);csDone(S,C);mqttResumeWait(&P.Publishes,1);testRequire(P.Log[0].Topic==32 && P.Log[0].Payload==48 && P.Log[0].First=='Z' && !P.Log[0].Alias && (P.Log[0].Flags&8u),"raised connection cap renders canonical replay above original64-byte cap without stale caller/alias data");
    csClose(C);mqttResumeStop(&P);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"alias expansion cleanup");xrtFutureDestroy(Bind);xrtFutureDestroy(F);xrtFutureDestroy(Ready);xrtFutureDestroy(Next);
}
static xmqttclientinboxinfo csInbox(xmqttclientinbox* I)
{xmqttclientinboxinfo B;testRequire(xmqttClientInboxInfo(I,&B),"Session Inbox snapshot");return B;}
static void csMessage(xmqttmessage* M,const char* Text)
{const xmqttpacket* Q=xmqttMessagePacket(M);testRequire(Q && Q->Body.Publish.Topic.Size==7 && !memcmp(Q->Body.Publish.Topic.Data,"inbound",7) && Q->Body.Publish.Payload.Size==strlen(Text) && !memcmp(Q->Body.Publish.Payload.Data,Text,strlen(Text)),"Session accepted payload lifetime");}
static void csIncoming(xmqttversion V,bool Duplicate)
{
    xmqttsession* S;xmqttclientinbox* I;csPair(V,&S,&I);mqttResumePeer P;mqttResumeStart(&P,V,0,false);xfuture *Ready,*Next;xmqttclient* C=csConnect(&P,S,I,&Ready,NULL);csReady(Ready);
    csIncomingPacket(&P,77,false,"held");mqttResumeWait(&P.Pubrecs,1);testRequire(csInfo(S).Incoming==1 && csInbox(I).Messages==1,"commit precedes incoming PUBREC and queue visibility");csClose(C);mqttResumeStop(&P);
    mqttResumeStart(&P,V,0,true);C=csConnect(&P,S,I,&Next,NULL);csReady(Next);
    if(Duplicate){csIncomingPacket(&P,77,true,"held");mqttResumeWait(&P.Pubrecs,1);testRequire(csInbox(I).Messages==1 && csInfo(S).Incoming==1,"duplicate after reconnect has no second delivery");}
    mqttResumeAck(&P,XMQTT_PACKET_PUBREL,77);mqttResumeWait(&P.Pubcomps,1);testRequire(!csInfo(S).Incoming,"full PUBCOMP ends old identity");
    xmqttmessage* M=NULL;testRequire(xmqttClientTakeMessage(C,&M),"take earlier accepted message");csMessage(M,"held");
    xfuture* D=NULL;testRequire(xmqttClientDisconnect(C,NULL,5000,&D)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)csResult(D))->Status==XMQTT_STATUS_OK,"known old PUBREL does not underflow new receive-window accounting");
    testRequire(xmqttSessionReset(S)==XMQTT_SESSION_STATE_ERROR,"exclusive lease lasts after graceful native end");xrtClearError();csClose(C);mqttResumeStop(&P);
    testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"destroy states with taken payload and old readiness retained");csMessage(M,"held");xmqttMessageRelease(M);xrtFutureDestroy(D);xrtFutureDestroy(Ready);xrtFutureDestroy(Next);
}
static void csMissingState(xmqttversion V)
{
    xmqttsession* S;xmqttclientinbox* I;csPair(V,&S,&I);mqttResumePeer P;mqttResumeStart(&P,V,MQTT_RESUME_HOLD_ACK,false);xfuture *Ready,*Next;xmqttclient* C=csConnect(&P,S,I,&Ready,NULL);csReady(Ready);
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Identifier=77;Q.Body.Publish.Topic=XRT_STR_LITERAL("outgoing");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("old");xfuture* F=NULL;testRequire(xmqttClientPublishAsync(C,&Q,8000,&F)==XMQTT_ADMISSION_ACCEPTED,"outgoing/incoming ids have independent namespaces");mqttResumeWait(&P.Publishes,1);
    csIncomingPacket(&P,77,false,"old");mqttResumeWait(&P.Pubrecs,1);csClose(C);mqttResumeStop(&P);
    testRequire(csInfo(S).ActiveOutgoing==1 && csInfo(S).Incoming==1 && csInbox(I).Messages==1,"both protocol directions retained");
    mqttResumeStart(&P,V,0,false);C=csConnect(&P,S,I,&Next,NULL);const xmqttreadyresult* R=csResult(Next);
    if(V==XMQTT_VERSION_311){testRequire(R->Status==XMQTT_STATUS_SESSION && csInfo(S).ActiveOutgoing==1 && csInfo(S).Incoming==1,"311 lost server state fails closed");csClose(C);mqttResumeStop(&P);xrtFutureDestroy(Next);testRequire(xmqttSessionReset(S)==XMQTT_SESSION_DONE,"explicit 311 administrative reset");mqttResumeStart(&P,V,0,false);C=csConnect(&P,S,I,&Next,NULL);csReady(Next);}
    else testRequire(R->Status==XMQTT_STATUS_OK,"MQTT5 accepts missing server state as reset");
    testRequire(!csInfo(S).ActiveOutgoing && !csInfo(S).Incoming && !csInfo(S).Objects && csInbox(I).Messages==1 && !xrtAtomic32Load(&P.Publishes,XMEMORY_ACQUIRE),"reset discards protocol obligations, preserves accepted application FIFO");
    csIncomingPacket(&P,77,false,"new");mqttResumeWait(&P.Pubrecs,1);mqttResumeAck(&P,XMQTT_PACKET_PUBREL,77);mqttResumeWait(&P.Pubcomps,1);testRequire(csInbox(I).Messages==2,"same id in a new session delivers a new message");
    xmqttmessage* M;testRequire(xmqttClientInboxTake(I,&M),"reset FIFO old");csMessage(M,"old");xmqttMessageRelease(M);testRequire(xmqttClientInboxTake(I,&M),"reset FIFO new");csMessage(M,"new");xmqttMessageRelease(M);
    csClose(C);mqttResumeStop(&P);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"reset cleanup");xrtFutureDestroy(Ready);xrtFutureDestroy(Next);xrtFutureDestroy(F);
}
static void csQueuedCancel(xmqttversion V)
{
    xmqttsession* S;xmqttclientinbox* I;csPair(V,&S,&I);mqttResumePeer P;mqttResumeStart(&P,V,MQTT_RESUME_HOLD_READY,false);xfuture* Ready;xmqttclient* C=csConnect(&P,S,I,&Ready,NULL);mqttResumeWait(&P.Connects,1);
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("queued");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("cancelled");xfuture* F=NULL;
    testRequire(xmqttClientPublishAsync(C,&Q,8000,&F)==XMQTT_ADMISSION_ACCEPTED && xrtFutureCancel(F),"cancel accepted pre-CONNACK request");const xmqttwriteresult* W=csResult(F);
    testRequire(W->Status==XMQTT_STATUS_CANCELLED && W->Delivery==XMQTT_NOT_SENT && !W->AcceptedBytes && !csInfo(S).ActiveOutgoing,"unsent cancellation creates no retained obligation");
    double End=xrtTimer()+8;while(csInfo(S).Objects){testRequire(xrtTimer()<End,"cancelled snapshot physical retirement");xrtSleep(1);}mqttResumeConnack(&P);csReady(Ready);
    testRequire(!csInfo(S).ActiveOutgoing && !xrtAtomic32Load(&P.Publishes,XMEMORY_ACQUIRE),"cancelled payload is never replayed");
    xrtFutureDestroy(F);F=NULL;
    testRequire(xmqttClientPublish(C,&Q,8000,&F)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)csResult(F))->Status==XMQTT_STATUS_OK,"new publish still works after cancellation");
    csClose(C);mqttResumeStop(&P);xrtFutureDestroy(F);xrtFutureDestroy(Ready);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"queued cancel cleanup");
}
static void csAdmissionAndWindow(xmqttversion V)
{
    xmqttsession* S;xmqttclientinbox* I;csPair(V,&S,&I);mqttResumePeer P;mqttResumeStart(&P,V,MQTT_RESUME_HOLD_ACK,false);xfuture *Ready,*Next;xmqttclient* C=csConnect(&P,S,I,&Ready,NULL);csReady(Ready);
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Identifier=23;Q.Body.Publish.Topic=XRT_STR_LITERAL("reserved");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("old");xfuture* Old=NULL;testRequire(xmqttClientPublishAsync(C,&Q,8000,&Old)==XMQTT_ADMISSION_ACCEPTED,"reserved old flow");mqttResumeWait(&P.Publishes,1);csClose(C);mqttResumeStop(&P);
    mqttResumeCaps Caps={0};Caps.MaximumQos=2;Caps.ReceiveMaximum=1;mqttResumeStartWith(&P,V,MQTT_RESUME_HOLD_READY|MQTT_RESUME_HOLD_ACK,true,&Caps);xmqttclientconfig K={0};xmqttClientConfigInit(&K);K.Version=V;
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.ClientId=XRT_STR_LITERAL("session-client");xmqttproperty Expiry=mqttResumeNumber(XMQTT_PROPERTY_SESSION_EXPIRY,60);if(V==XMQTT_VERSION_5)Connect.Properties=(xmqttpropertylist){&Expiry,1};
    K.MaxOutgoingBytes=((const xmqttwriteresult*)csResult(Old))->AcceptedBytes-1;C=(xmqttclient*)(uintptr_t)1;testRequire(!xmqttClientConnectSession(P.Engine,&P.Address,1,&K,S,I,&Connect,&C) && C==(xmqttclient*)(uintptr_t)1 && !csInbox(I).Attached && csInfo(S).ActiveOutgoing==1,"failed replay-byte reservation preserves output, lease and state");xrtClearError();
    xmqttClientConfigInit(&K);K.MaxRequests=2;C=csConnect(&P,S,I,&Next,&K);mqttResumeWait(&P.Connects,1);xmqttclientinfo CI;xmqttClientInfo(C,&CI);testRequire(CI.Requests==1 && CI.OutgoingBytes && !CI.Ready,"old flows reserve setup admission capacity");
    xfuture* Out=(xfuture*)(uintptr_t)1;testRequire(xmqttClientPublishAsync(C,&Q,8000,&Out)==XMQTT_ADMISSION_OVERLOADED && Out==(xfuture*)(uintptr_t)1,"old physical packet id cannot be reused during setup");
    Q.Flags=0;Q.Identifier=0;Q.Body.Publish.Topic=XRT_STR_LITERAL("bypass");xfuture* Zero=NULL;testRequire(xmqttClientPublishAsync(C,&Q,8000,&Zero)==XMQTT_ADMISSION_ACCEPTED,"queue QoS0 behind old reserved flow");
    testRequire(xmqttClientPublishAsync(C,&Q,8000,&Out)==XMQTT_ADMISSION_OVERLOADED && Out==(xfuture*)(uintptr_t)1,"old plus queued request use bounded shared slots");mqttResumeConnack(&P);csReady(Next);testRequire(((const xmqttwriteresult*)csResult(Zero))->Status==XMQTT_STATUS_OK,"QoS0 progresses while recovered publish waits for PUBACK");mqttResumeWait(&P.Publishes,2);
    testRequire(P.Log[0].Id==23 && (P.Log[0].Flags&8u) && P.Log[1].Qos==0 && csInfo(S).ActiveOutgoing==1,"replay order and independent control/zero traffic");mqttResumeAck(&P,XMQTT_PACKET_PUBACK,23);csDone(S,C);xmqttClientInfo(C,&CI);testRequire(CI.Accepted==1 && CI.Completed==1,"recovered internal records do not invent application operation counters");
    csClose(C);mqttResumeStop(&P);xrtFutureDestroy(Old);xrtFutureDestroy(Zero);xrtFutureDestroy(Ready);xrtFutureDestroy(Next);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"admission/window cleanup");
}
static void csFactoryOom(void)
{
    const size_t Limits[]={1,256,1024,4096,16384,65536,262144};bool Failed=false,Passed=false;
    for(size_t N=0;N<sizeof(Limits)/sizeof(*Limits);++N){xmqttsession* S;xmqttclientinbox* I;csPair(XMQTT_VERSION_5,&S,&I);mqttResumePeer P;mqttResumeStart(&P,XMQTT_VERSION_5,0,false);xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=XRT_STR_LITERAL("session-client");
        xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[N],&B) && xrtAllocationBudgetEnter(B,&Scope),"Session constructor allocation scope");xmqttclient* C=(xmqttclient*)(uintptr_t)1;bool OK=xmqttClientConnectSession(P.Engine,&P.Address,1,NULL,S,I,&Q,&C);testRequire(xrtAllocationBudgetLeave(&Scope),"leave Session constructor scope");
        if(OK){Passed=true;csClose(C);}else {Failed=true;testRequire(C==(xmqttclient*)(uintptr_t)1 && !csInbox(I).Attached && !csInfo(S).Connected && !csInfo(S).HasState,"failed constructor leaves no lease/protocol state");xfuture* Ready;C=csConnect(&P,S,I,&Ready,NULL);csReady(Ready);csClose(C);xrtFutureDestroy(Ready);}
        mqttResumeStop(&P);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"constructor OOM owners retired");xrtClearError();xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);testRequire(xrtAllocationBudgetInfo(B,&Info) && !Info.LiveBytes && !Info.LiveCount,"constructor OOM has no surviving allocations");xrtAllocationBudgetFree(B);
    }testRequire(Failed && Passed,"constructor failures and successful controls covered");
}
static void csCancellationBudget(xmqttversion V)
{
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(64u*1024u*1024u,&B) && xrtAllocationBudgetEnter(B,&Scope),"retained-result budget scope");csQueuedCancel(V);xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"leave retained-result scope");xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);testRequire(xrtAllocationBudgetInfo(B,&Info) && !Info.LiveBytes && !Info.LiveCount,"all main-owned Futures, state and quota headers physically retired");xrtAllocationBudgetFree(B);
}
#if defined(XMQTT_CLIENT_SESSION_SINGLE_TEST)
typedef struct csProbeArm {bool Armed;} csProbeArm;
static void csIdentifierWordScan(void)
{
    static const uint16 Boundary[]={1,2,7,8,15,16,30,31,32,33,63,64,65,32767,32768,65534,65535};
    unsigned char Ids[8192],Used[8192];xmqttsession S={0};S.Ids=Ids;
    memset(Ids,0xa5,sizeof(Ids));memset(Used,0x5a,sizeof(Used));
    for(size_t I=0;I<sizeof(Boundary)/sizeof(*Boundary);++I){
        testRequire(!xmqttSessionIdFind(&S,Used,Boundary[I]),"combined full maps wrap without selecting zero");
        for(size_t J=0;J<sizeof(Boundary)/sizeof(*Boundary);++J){uint16 Id=Boundary[J];unsigned char Mask=(unsigned char)(1u<<(Id&7u)),A=Ids[Id>>3],B=Used[Id>>3];Ids[Id>>3]&=(unsigned char)~Mask;Used[Id>>3]&=(unsigned char)~Mask;
            testRequire(xmqttSessionIdFind(&S,Used,Boundary[I])==Id,"sole free bit across first/last partial groups and byte/word boundaries");Ids[Id>>3]=A;Used[Id>>3]=B;}
    }
    memset(Ids,0,sizeof(Ids));memset(Used,0,sizeof(Used));testRequire(xmqttSessionIdFind(&S,Used,1)==1,"zero is excluded on empty maps");
}
static void csIdentifierFullMap(xmqttversion V)
{
    xmqttsessionconfig K;xmqttSessionConfigInit(&K);K.Version=V;K.ClientId=XRT_STR_LITERAL("session-client");K.MaxOutgoing=65535;K.MaxIncoming=0;K.MaxBytes=64u*1024u*1024u;xmqttsession* S=NULL;xmqttclientinbox* Inbox=NULL;
    xmqttclientinboxconfig IK;xmqttClientInboxConfigInit(&IK);IK.Version=V;testRequire(xmqttSessionCreate(&K,&S) && xmqttClientInboxCreate(&IK,&Inbox),"full physical id map");
    xmqttpacket Q,A;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=K.ClientId;xmqttproperty E=mqttResumeNumber(XMQTT_PROPERTY_SESSION_EXPIRY,60);if(V==XMQTT_VERSION_5)Q.Properties=(xmqttpropertylist){&E,1};xmqttPacketInit(XMQTT_PACKET_CONNACK,&A);xmqttnegotiated N;testRequire(xmqttSessionOpen(S,&Q,&A,xrtTimer(),&N)==XMQTT_SESSION_DONE,"full id map open");
    const xmqttsessionpacket** Held=xrtCalloc(65535,sizeof(*Held));testRequire(Held!=NULL,"full id map handles");xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("full");
    for(unsigned Id=1;Id<=65535;++Id){Q.Identifier=(uint16)Id;xmqttsessionframe F;testRequire(xmqttSessionPublish(S,&Q,Held+Id-1)==XMQTT_SESSION_DONE && xmqttSessionNext(S,&F)==XMQTT_SESSION_DONE && xmqttSessionSent(S,&F)==XMQTT_SESSION_DONE,"reserve every physical id");xmqttSessionPacketRelease(F.Packet);xmqttPacketInit(XMQTT_PACKET_PUBACK,&A);A.Identifier=(uint16)Id;testRequire(xmqttSessionAcknowledge(S,&A)==XMQTT_SESSION_DONE,"logical completion retains id storage");}
    testRequire(xmqttSessionEnd(S,NULL,xrtTimer())==XMQTT_SESSION_DONE,"full map offline");mqttResumePeer P;mqttResumeStart(&P,V,MQTT_RESUME_HOLD_READY,true);xmqttclientconfig CK;xmqttClientConfigInit(&CK);CK.SetupTimeout=30000;xfuture* Ready;xmqttclient* C=csConnect(&P,S,Inbox,&Ready,&CK);mqttResumeWait(&P.Connects,1);
    Q.Identifier=0;xfuture* F=(xfuture*)(uintptr_t)1;
    testRequire(xmqttClientPublishAsync(C,&Q,8000,&F)==XMQTT_ADMISSION_OVERLOADED && F==(xfuture*)(uintptr_t)1,"saturated id lookup refuses all retained physical ids");
    xmqttSessionPacketRelease(Held[32766]);Held[32766]=NULL;F=NULL;
    testRequire(xmqttClientPublishAsync(C,&Q,8000,&F)==XMQTT_ADMISSION_ACCEPTED,"find sole free high bit");testRequire(xrtFutureCancel(F),"cancel full-map candidate");const xmqttwriteresult* R=csResult(F);testRequire(R->Status==XMQTT_STATUS_CANCELLED && R->Identifier==32767 && !R->AcceptedBytes,"sole physically freed id selected");
    double End=xrtTimer()+8;for(;;){xmqttclientinfo Info;xmqttClientInfo(C,&Info);if(!Info.Requests)break;testRequire(xrtTimer()<End,"full-map cancelled request physically retires");xrtSleep(1);}xrtFutureDestroy(F);
    for(unsigned I=0;I<65535;++I){xmqttSessionPacketRelease(Held[I]);}xrtFree(Held);mqttResumeConnack(&P);csReady(Ready);csDone(S,C);csClose(C);mqttResumeStop(&P);xrtFutureDestroy(Ready);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(Inbox),"full id map cleanup");
}
static void csProbeArmReady(xmqttclient* C,ptr Data)
{csProbeArm* A=Data;xmqttclientinfo I;xmqttClientInfo(C,&I);if(I.Ready && !A->Armed){A->Armed=true;csProbeReset(2);}}
static void csNativeBoundary(xmqttversion V,bool Partial)
{
    xmqttsession* S;xmqttclientinbox* I;csPair(V,&S,&I);mqttResumePeer P;mqttResumeStart(&P,V,0,false);xmqttclientconfig K={0};xmqttClientConfigInit(&K);K.WriteChunkBytes=K.WriteBytes=1;xfuture* Ready;xmqttclient* C=csConnect(&P,S,I,&Ready,&K);csReady(Ready);
    csProbeReset(Partial?2:1);xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Identifier=29;Q.Body.Publish.Topic=XRT_STR_LITERAL("boundary");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("native prefix");xfuture* F=NULL;
    testRequire(xmqttClientPublishAsync(C,&Q,8000,&F)==XMQTT_ADMISSION_ACCEPTED,"controlled native prefix admission");mqttResumeWait(Partial?&csProbeAccepted:&csProbeSeen,1);
    testRequire(csInfo(S).ActiveOutgoing==1 && csInfo(S).Objects==1,"frame selected before native refusal");
    if(Partial){testRequire(xmqttClientClose(C),"close after exactly one native byte");const xmqttwriteresult* W=csResult(F);testRequire(W->AcceptedBytes==1 && W->Delivery==XMQTT_MAY_HAVE_REACHED_PEER,"one-byte uncertainty");}
    else {testRequire(xrtFutureCancel(F),"cancel exposed frame with zero native bytes");const xmqttwriteresult* W=csResult(F);csDone(S,C);testRequire(W->Status==XMQTT_STATUS_CANCELLED && !W->AcceptedBytes && W->Delivery==XMQTT_NOT_SENT && !csInfo(S).ActiveOutgoing && !csInfo(S).Objects && csInfo(S).SendQuota==csInfo(S).InitialSendQuota,"withdraw zero-output selection and refund quota/refcounts");}
    csClose(C);mqttResumeStop(&P);csProbeReset(0);testRequire(csInfo(S).ActiveOutgoing==(Partial?1u:0u),"only observed prefix remains for recovery");
    mqttResumeStart(&P,V,0,true);xfuture* Next;C=csConnect(&P,S,I,&Next,NULL);csReady(Next);csDone(S,C);
    testRequire(xrtAtomic32Load(&P.Publishes,XMEMORY_ACQUIRE)==(Partial?1u:0u),"only partial output replays");if(Partial)testRequire(P.Log[0].Flags&8u,"partial original uses DUP on reconnect");
    csClose(C);mqttResumeStop(&P);xrtFutureDestroy(F);xrtFutureDestroy(Ready);xrtFutureDestroy(Next);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"native boundary cleanup");
}
static void csEarlyAckPartial(xmqttversion V)
{
    xmqttsession* S;xmqttclientinbox* I;csPair(V,&S,&I);mqttResumePeer P;mqttResumeStart(&P,V,MQTT_RESUME_HOLD_ACK,false);xfuture *Ready,*Next;xmqttclient* C=csConnect(&P,S,I,&Ready,NULL);csReady(Ready);
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Identifier=31;Q.Body.Publish.Topic=XRT_STR_LITERAL("early");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("previous complete packet");xfuture* F=NULL;testRequire(xmqttClientPublishAsync(C,&Q,8000,&F)==XMQTT_ADMISSION_ACCEPTED,"old complete publication");mqttResumeWait(&P.Publishes,1);csClose(C);mqttResumeStop(&P);
    csProbeReset(0);mqttResumeCaps Caps={0};Caps.MaximumQos=2;Caps.EarlyId=31;mqttResumeStartWith(&P,V,MQTT_RESUME_HOLD_ACK|MQTT_RESUME_EARLY_ACK,true,&Caps);xmqttclientconfig K={0};xmqttClientConfigInit(&K);K.WriteChunkBytes=K.WriteBytes=1;csProbeArm Arm={0};K.Notify=csProbeArmReady;K.User=&Arm;C=csConnect(&P,S,I,&Next,&K);csReady(Next);
    testRequire(xmqttClientWaitFor(C,8000)==XWAIT_OK,"early prior-generation ACK retires a partial replay transport");xmqttclientinfo CI;xmqttClientInfo(C,&CI);
    testRequire(CI.CloseStatus==XMQTT_STATUS_SESSION && !csInfo(S).ActiveOutgoing && !csInfo(S).Objects && !xrtAtomic32Load(&P.Publishes,XMEMORY_ACQUIRE),"commit old ACK, release native/frame ownership, do not finish duplicate packet");csClose(C);mqttResumeStop(&P);csProbeReset(0);
    xrtFutureDestroy(F);xrtFutureDestroy(Ready);xrtFutureDestroy(Next);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"early ACK cleanup");
}
static int32 csReleasePacketThread(ptr Data)
{xmqttSessionPacketRelease(Data);return 0;}
static void csPacketPhysicalRetirement(xmqttversion V)
{
    xmqttsessionconfig K;xmqttSessionConfigInit(&K);K.Version=V;K.MaxOutgoing=1;K.MaxIncoming=0;K.ClientId=XRT_STR_LITERAL("session-client");xmqttsession* S=NULL;testRequire(xmqttSessionCreate(&K,&S),"physical packet state");
    xmqttpacket C,A;xmqttPacketInit(XMQTT_PACKET_CONNECT,&C);xmqttPacketInit(XMQTT_PACKET_CONNACK,&A);C.Body.Connect.ClientId=K.ClientId;xmqttproperty Expiry=mqttResumeNumber(XMQTT_PROPERTY_SESSION_EXPIRY,60);if(V==XMQTT_VERSION_5)C.Properties=(xmqttpropertylist){&Expiry,1};xmqttnegotiated N;testRequire(xmqttSessionOpen(S,&C,&A,xrtTimer(),&N)==XMQTT_SESSION_DONE,"physical packet open");
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Identifier=7;Q.Body.Publish.Topic=XRT_STR_LITERAL("free");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("physical");const xmqttsessionpacket* P=NULL;testRequire(xmqttSessionPublish(S,&Q,&P)==XMQTT_SESSION_DONE,"physical packet admit");xmqttsessionframe F;testRequire(xmqttSessionNext(S,&F)==XMQTT_SESSION_DONE && xmqttSessionSent(S,&F)==XMQTT_SESSION_DONE,"physical packet sent");xmqttSessionPacketRelease(F.Packet);xmqttPacketInit(XMQTT_PACKET_PUBACK,&A);A.Identifier=7;testRequire(xmqttSessionAcknowledge(S,&A)==XMQTT_SESSION_DONE,"physical packet logical completion");
    csProbeDelayFree((void*)P);xthread* T=xrtThreadCreate(csReleasePacketThread,(void*)P,0);testRequire(T!=NULL,"packet release thread");mqttResumeWait(&csProbeFreeEntered,1);xmqttsessioninfo Before=csInfo(S);const xmqttsessionpacket* Other=(const xmqttsessionpacket*)(uintptr_t)1;
    testRequire(Before.Objects==1 && Before.LiveBytes>Before.BaseBytes && xmqttSessionPublish(S,&Q,&Other)==XMQTT_SESSION_LIMIT && Other==(const xmqttsessionpacket*)(uintptr_t)1,"packet object/bytes/id stay charged until actual allocator retirement");
    xrtAtomic32Store(&csProbeFreeRelease,1,XMEMORY_RELEASE);testRequire(xrtThreadWaitFor(T,8000)==XWAIT_OK && xrtThreadExitCode(T)==0,"physical packet release join");xrtThreadDestroy(T);xrtAtomicPtrStore(&csProbeFreeTarget,NULL,XMEMORY_RELEASE);
    testRequire(!csInfo(S).Objects && csInfo(S).LiveBytes==csInfo(S).BaseBytes && xmqttSessionPublish(S,&Q,&Other)==XMQTT_SESSION_DONE,"physical packet capacity becomes reusable afterward");testRequire(xmqttSessionEnd(S,NULL,xrtTimer())==XMQTT_SESSION_DONE && xmqttSessionDestroy(S),"physical packet end");xmqttSessionPacketRelease(Other);
}
static void csRequestPhysicalRetirement(xmqttversion V)
{
    xmqttsession* S;xmqttclientinbox* I;csPair(V,&S,&I);mqttResumePeer P;mqttResumeStart(&P,V,0,false);xmqttclientconfig K={0};xmqttClientConfigInit(&K);K.MaxRequests=1;K.WriteChunkBytes=K.WriteBytes=1;xfuture* Ready;xmqttclient* C=csConnect(&P,S,I,&Ready,&K);csReady(Ready);csProbeReset(1);
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Identifier=33;Q.Body.Publish.Topic=XRT_STR_LITERAL("bounded");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("request");xfuture* F=NULL;testRequire(xmqttClientPublishAsync(C,&Q,8000,&F)==XMQTT_ADMISSION_ACCEPTED,"physical request admission");mqttResumeWait(&csProbeSeen,1);
    testRequire(C->Session->Selected!=NULL,"native gate synchronizes selected request");csProbeDelayFree(C->Session->Selected);testRequire(xrtFutureCancel(F),"physical request cancel");mqttResumeWait(&csProbeFreeEntered,1);xmqttclientinfo CI;xmqttClientInfo(C,&CI);xfuture* Other=(xfuture*)(uintptr_t)1;
    testRequire(CI.Requests==1 && CI.OutgoingBytes && xmqttClientPublishAsync(C,&Q,8000,&Other)==XMQTT_ADMISSION_OVERLOADED && Other==(xfuture*)(uintptr_t)1,"request storage remains charged while allocator free is held");
    xrtAtomic32Store(&csProbeFreeRelease,1,XMEMORY_RELEASE);csDone(S,C);xrtAtomicPtrStore(&csProbeFreeTarget,NULL,XMEMORY_RELEASE);csProbeReset(0);testRequire(((const xmqttwriteresult*)csResult(F))->Status==XMQTT_STATUS_CANCELLED,"logical cancellation remains typed");Other=NULL;
    testRequire(xmqttClientPublish(C,&Q,8000,&Other)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)csResult(Other))->Status==XMQTT_STATUS_OK,"same id/request capacity reused after real free");csDone(S,C);csClose(C);mqttResumeStop(&P);xrtFutureDestroy(F);xrtFutureDestroy(Other);xrtFutureDestroy(Ready);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"physical request cleanup");
}
#endif
#include "client_session_cases.h"
#include "client_session_assigned_cases.h"
int main(void)
{
    csAssignedTests();
    csIdentifierPhysicalRace(XMQTT_VERSION_5);csIdentifierPhysicalRace(XMQTT_VERSION_311);
    for(unsigned V=0;V<2;++V)for(unsigned Phase=0;Phase<3;++Phase)csOutgoing(V?XMQTT_VERSION_311:XMQTT_VERSION_5,Phase,false);
    csOutgoing(XMQTT_VERSION_5,0,true);csAliasExpansion();
    for(unsigned V=0;V<2;++V){xmqttversion Version=V?XMQTT_VERSION_311:XMQTT_VERSION_5;csIncoming(Version,false);csIncoming(Version,true);csMissingState(Version);csCancellationBudget(Version);csAdmissionAndWindow(Version);}
    csFactoryOom();csExpiry();csPendingPhysical(XMQTT_VERSION_5);csPendingPhysical(XMQTT_VERSION_311);csSnapshotOom(XMQTT_VERSION_5);csSnapshotOom(XMQTT_VERSION_311);csConcurrentSnapshots(XMQTT_VERSION_5);csConcurrentSnapshots(XMQTT_VERSION_311);csAliasRollbackBudget();csConcurrentAliases();csOutgoing(XMQTT_VERSION_5,1,true);csOutgoing(XMQTT_VERSION_5,2,true);
#if defined(XMQTT_CLIENT_SESSION_SINGLE_TEST)
    csIdentifierWordScan();csIdentifierFullMap(XMQTT_VERSION_5);csIdentifierFullMap(XMQTT_VERSION_311);
    csSharedCopy(XMQTT_VERSION_5,1,false);csSharedCopy(XMQTT_VERSION_5,2,false);csSharedCopy(XMQTT_VERSION_311,1,false);csSharedCopy(XMQTT_VERSION_311,2,false);csSharedCopy(XMQTT_VERSION_5,1,true);
    csSharedCopy(XMQTT_VERSION_5,2,true);
    for(unsigned Q=1;Q<=2;++Q)for(unsigned P=0;P<3;++P){csAliasVectors(Q,P,true,false);csAliasVectors(Q,P,false,true);}
    csAliasGateClose();
    csSnapshotCloseRace(XMQTT_VERSION_5);csSnapshotCloseRace(XMQTT_VERSION_311);
    for(unsigned V=0;V<2;++V){xmqttversion Version=V?XMQTT_VERSION_311:XMQTT_VERSION_5;csNativeBoundary(Version,false);csNativeBoundary(Version,true);csEarlyAckPartial(Version);csPacketPhysicalRetirement(Version);csRequestPhysicalRetirement(Version);for(unsigned N=0;N<4;++N)csReplayAllocation(Version,N==3?4:N);}
#endif
    puts("Client Session outgoing/incoming/reset/cancel recovery tests passed");return 0;
}
