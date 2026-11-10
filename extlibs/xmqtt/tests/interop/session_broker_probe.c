#include <xmqtt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/* Public API probe. The independent proxy cuts exactly one acknowledgement. */
static xmqttversion Version;
static unsigned Mode;
static size_t PayloadBytes;
static cstr Host="127.0.0.1";
static xstrview Topic=XRT_STR_LITERAL("xrt/qual/session/binary");
static void check(bool OK,cstr Text)
{if(!OK){fprintf(stderr,"[FAIL] session broker: %s cause=%s\n",Text,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");exit(1);}}
static const void* outcome(xfuture* F)
{check(F && xrtFutureWaitFor(F,15000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"bounded typed outcome");return xrtFutureValue(F);}
static void barrier(cstr Prompt,cstr Command)
{char Line[32];puts(Prompt);fflush(stdout);check(fgets(Line,sizeof(Line),stdin) && !strcmp(Line,Command),"independent proxy barrier");}
static void subscribe(xmqttclient* C,xstrview T)
{
    xmqttsubscription S={T,2};xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&Q);Q.Body.Topics=(xmqttsubscriptionlist){&S,1};xfuture* F=NULL;
    check(xmqttClientSubscribeAsync(C,&Q,10000,&F)==XMQTT_ADMISSION_ACCEPTED,"subscribe admission");
    const xmqttsubscriptionresult* R=outcome(F);check(R->Status==XMQTT_STATUS_OK && R->Acknowledged && R->Granted==1 && !R->Rejected,"broker SUBACK");xrtFutureDestroy(F);
}
static xnetengine* engine(void)
{xnetengineconfig K;xrtNetEngineConfigInit(&K);K.Workers=2;xnetengine* E=xrtNetEngineCreate(&K);check(E && xrtNetEngineStart(E),"independent host Engine");return E;}
static xmqttclient* connectSession(xnetengine* E,uint16 Port,xmqttsession* S,xmqttclientinbox* Inbox,bool Present,bool Fragment,xfuture** Ready)
{
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=Version;K.SetupTimeout=10000;
    if(Fragment){K.ReadBytes=37;K.WriteBytes=41;K.WriteChunkBytes=13;K.WorkPackets=4;}
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=false;Q.Body.Connect.KeepAlive=5;Q.Body.Connect.ClientId=XRT_STR_LITERAL("xrt-session-recovery");
    xmqttproperty P[2]={{0}};
    if(Version==XMQTT_VERSION_5){P[0].Id=XMQTT_PROPERTY_SESSION_EXPIRY;P[0].Type=XMQTT_PROPERTY_UINT32;P[0].Value.Number=60;
        P[1].Id=XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM;P[1].Type=XMQTT_PROPERTY_UINT16;P[1].Value.Number=4;Q.Properties=(xmqttpropertylist){P,2};}
    xnetaddr A;check(xrtNetAddrParse(&A,Host,Port),"numeric proxy address");xmqttclient* C=NULL;
    check(xmqttClientConnectSession(E,&A,1,&K,S,Inbox,&Q,&C),"Session connection factory");*Ready=xmqttClientReady(C);
    const xmqttreadyresult* R=outcome(*Ready);if(R->Status!=XMQTT_STATUS_OK)fprintf(stderr,"readiness status=%u cause=%s\n",(unsigned)R->Status,R->Cause?xrtErrorMessage(R->Cause):"none");check(R->Status==XMQTT_STATUS_OK && R->Negotiated.Accepted && R->Negotiated.SessionPresent==Present,"broker session continuity");
#if defined(XMQTT_SESSION_BROKER_READY)
    XMQTT_SESSION_BROKER_READY(C,R);
#endif
    return C;
}
static void retire(xmqttclient* C,xnetengine* E,bool Graceful)
{
    if(Graceful){xfuture* F=NULL;check(xmqttClientDisconnectAsync(C,NULL,10000,&F)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)outcome(F))->Status==XMQTT_STATUS_OK,"graceful disconnect");xrtFutureDestroy(F);}
    check(xmqttClientClose(C) && xmqttClientWaitFor(C,15000)==XWAIT_OK && xmqttClientDestroy(C),"physical Client retirement");
    check(xrtNetEngineStop(E) && xrtNetEngineDestroy(E),"physical Engine retirement");
}
static void message(xmqttmessage* M)
{
    const xmqttpacket* P=xmqttMessagePacket(M);check(P && P->Type==XMQTT_PACKET_PUBLISH && P->Body.Publish.Topic.Size==Topic.Size && !memcmp(P->Body.Publish.Topic.Data,Topic.Data,Topic.Size) && P->Body.Publish.Payload.Size==PayloadBytes && ((P->Flags>>1)&3u)==(Mode?2u:1u),"logical topic/QoS/binary length");
    for(size_t I=0;I<PayloadBytes;++I)check(P->Body.Publish.Payload.Data[I]==(uint8)(I*37u+Mode),"immutable binary payload");
}
int main(int Argc,char** Argv)
{
    /* PORT MQTT(5|311) drop(0..4) fragmented(0|1) [numeric proxy HOST]. */
    if(Argc!=5 && Argc!=6)return 2;
    if(Argc==6)Host=Argv[5];
    unsigned long Port=strtoul(Argv[1],NULL,10),V=strtoul(Argv[2],NULL,10);Mode=(unsigned)strtoul(Argv[3],NULL,10);bool Fragment=atoi(Argv[4])!=0;
    check(Port && Port<=65535 && (V==5 || V==311) && Mode<5,"arguments");Version=V==5?XMQTT_VERSION_5:XMQTT_VERSION_311;PayloadBytes=Fragment?257:65537;
#if defined(XMQTT_SESSION_BROKER_PAYLOAD_SIZE)
    PayloadBytes=XMQTT_SESSION_BROKER_PAYLOAD_SIZE(PayloadBytes);
#endif
    check(PayloadBytes && PayloadBytes<=1024u*1024u,"bounded probe payload");
    xmqttsessionconfig SK;xmqttSessionConfigInit(&SK);SK.Version=Version;SK.ClientId=XRT_STR_LITERAL("xrt-session-recovery");xmqttsession* S=NULL;check(xmqttSessionCreate(&SK,&S),"passive Session");
    xmqttclientinboxconfig IK;xmqttClientInboxConfigInit(&IK);IK.Version=Version;xmqttclientinbox* Inbox=NULL;check(xmqttClientInboxCreate(&IK,&Inbox),"passive Inbox");
    xfuture *R1=NULL,*R2=NULL,*F=NULL;xnetengine* E=engine();xmqttclient* C=connectSession(E,(uint16)Port,S,Inbox,false,Fragment,&R1);subscribe(C,Topic);
    uint8* Payload=malloc(PayloadBytes);check(Payload!=NULL,"bounded caller payload allocation");for(size_t I=0;I<PayloadBytes;++I)Payload[I]=(uint8)(I*37u+Mode);
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=Mode?4:2;Q.Body.Publish.Topic=Topic;Q.Body.Publish.Payload=(xbytesview){Payload,PayloadBytes};
    xmqttproperty Alias={0};if(Version==XMQTT_VERSION_5){Alias.Id=XMQTT_PROPERTY_TOPIC_ALIAS;Alias.Type=XMQTT_PROPERTY_UINT16;Alias.Value.Number=1;Q.Properties=(xmqttpropertylist){&Alias,1};}
    check(xmqttClientPublishAsync(C,&Q,10000,&F)==XMQTT_ADMISSION_ACCEPTED,"shared Session snapshot admission");memset(Payload,0,PayloadBytes);
    barrier("WAIT_CUT","cut\n");
    if(Mode>=3){const xmqttwriteresult* R=outcome(F);check(R->Status==XMQTT_STATUS_OK && R->PubcompReceived,"independent outgoing QoS2 completes before incoming interruption");}
    retire(C,E,false);const xmqttwriteresult* Old=outcome(F);
#if defined(XMQTT_SESSION_BROKER_OLD_RESULT)
    XMQTT_SESSION_BROKER_OLD_RESULT(Old,&Q);
#endif
    check(Old->AcceptedBytes && Old->Identifier && Old->Status==(Mode>=3?XMQTT_STATUS_OK:XMQTT_STATUS_CLOSED),"original connection outcome retained");
    check(Old->PubrecReceived==(Mode>=2) && Old->PubcompReceived==(Mode>=3),"original observed QoS2 phase retained");
    free(Payload);
    E=engine();C=connectSession(E,(uint16)Port,S,Inbox,true,Fragment,&R2);
    barrier("WAIT_DRAIN","drain\n");
    /* Independent wire barrier first; then a broker round trip on another topic. */
    subscribe(C,XRT_STR_LITERAL("xrt/qual/session/barrier"));
    double End=xrtTimer()+15;
    for(;;){xmqttsessioninfo SI={0};xmqttclientinfo CI={0};xmqttclientinboxinfo II={0};SI.Size=sizeof(SI);CI.Size=sizeof(CI);II.Size=sizeof(II);
        check(xmqttSessionInfo(S,&SI) && xmqttClientInfo(C,&CI) && xmqttClientInboxInfo(Inbox,&II),"live gauges");
        if(!SI.ActiveOutgoing && !SI.Objects && !SI.Incoming && !SI.Prepared && !CI.Requests && II.Messages)break;
        check(CI.State==XMQTT_CLIENT_OPEN && xrtTimer()<End,"recovery protocol/native quiescence");xrtSleep(1);}
    retire(C,E,true);xmqttmessage *Held=NULL,*M=NULL;unsigned Count=0;
    while(xmqttClientInboxTake(Inbox,&M)){message(M);++Count;if(!Held)Held=M;else xmqttMessageRelease(M);}
    check(Mode?Count==1:Count>=1,"QoS2 exactly one local delivery; QoS1 at least once");
    check(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(Inbox),"passive owners destroyed");message(Held);
    check(Old==xrtFutureValue(F) && Old->Status==(Mode>=3?XMQTT_STATUS_OK:XMQTT_STATUS_CLOSED) && ((const xmqttreadyresult*)xrtFutureValue(R2))->Negotiated.SessionPresent,"results survive both Clients/Engines and passive owners");
    xmqttMessageRelease(Held);xrtFutureDestroy(F);xrtFutureDestroy(R1);xrtFutureDestroy(R2);
    printf("[PASS] broker Session recovery MQTT%lu mode%u fragmented%u messages%u\n",V,Mode,Fragment?1u:0u,Count);return 0;
}
