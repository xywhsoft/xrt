#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Public automatic-controller probe for run_session_broker.case's independent
 * five ACK cuts. The first lease deliberately holds retirement at the cut. */
static void managedCheck(bool OK,const char* Message)
{if(!OK){fprintf(stderr,"[FAIL] managed broker: %s cause=%s\n",Message,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");exit(1);}}
static const void* managedOutcome(xfuture* F)
{managedCheck(F && xrtFutureWaitFor(F,15000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"typed result");return xrtFutureValue(F);}
static void managedBarrier(const char* Prompt,const char* Command)
{char Line[32];puts(Prompt);fflush(stdout);managedCheck(fgets(Line,sizeof(Line),stdin) && !strcmp(Line,Command),"independent proxy barrier");}
static xmqttclientstatus managedFactory(xnetengine* E,uint64 A,const xmqttclientconfig* K,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,ptr Data,xmqttclient** C)
{return xmqttClientConnectSession(E,Data,A,K,S,I,Q,C)?XMQTT_STATUS_OK:XMQTT_STATUS_INITIALIZATION;}
static void managedSubscribe(xmqttclient* C,xstrview Topic)
{
    xmqttsubscription S={Topic,2};xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&Q);Q.Body.Topics=(xmqttsubscriptionlist){&S,1};xfuture* F=NULL;
    managedCheck(xmqttClientSubscribeAsync(C,&Q,10000,&F)==XMQTT_ADMISSION_ACCEPTED,"broker subscription");const xmqttsubscriptionresult* R=managedOutcome(F);managedCheck(R->Status==XMQTT_STATUS_OK && R->Acknowledged && R->Granted==1,"broker SUBACK");xrtFutureDestroy(F);
}
static xfuture* managedOnline(xmqttreconnect* R,uint64 Connections,bool Present,xmqttclientlease* L)
{
    xmqttreconnectinfo I;double End=xrtTimer()+15;
    for(;;){managedCheck(xmqttReconnectInfo(R,&I) && I.State<XMQTT_RECONNECT_STOPPED && xrtTimer()<End,"automatic connection deadline");
        if(I.State==XMQTT_RECONNECT_ONLINE && I.Connections==Connections && xmqttReconnectAcquire(R,L))break;
        xrtSleep(1);}
    managedCheck(L->Generation==Connections,"one generation per automatic attempt");xfuture* Ready=xmqttClientReady(L->Client);const xmqttreadyresult* V=managedOutcome(Ready);
    managedCheck(V->Status==XMQTT_STATUS_OK && V->Negotiated.Accepted && V->Negotiated.SessionPresent==Present,"real broker Session continuity");
#if defined(XMQTT_RECONNECT_BROKER_READY)
    XMQTT_RECONNECT_BROKER_READY(L->Client,V);
#endif
    return Ready;
}
int main(int Argc,char** Argv)
{
    if(Argc!=5 && Argc!=6)return 2;
    unsigned long Port=strtoul(Argv[1],NULL,10),V=strtoul(Argv[2],NULL,10);unsigned Mode=(unsigned)strtoul(Argv[3],NULL,10);bool Fragment=atoi(Argv[4])!=0;const char* Host=Argc==6?Argv[5]:"127.0.0.1";
    managedCheck(Port && Port<=65535 && (V==5 || V==311) && Mode<5,"arguments");xmqttversion Version=V==5?XMQTT_VERSION_5:XMQTT_VERSION_311;size_t Bytes=Fragment?257:65537;
#if defined(XMQTT_RECONNECT_BROKER_PAYLOAD_SIZE)
    Bytes=XMQTT_RECONNECT_BROKER_PAYLOAD_SIZE(Bytes);
#endif
    xnetengineconfig EC;xrtNetEngineConfigInit(&EC);EC.Workers=2;xnetengine* E=xrtNetEngineCreate(&EC);managedCheck(E && xrtNetEngineStart(E),"Engine");xtaskpoolconfig TC={1,8,0};xtaskpool* Pool=xrtTaskPoolCreate(&TC);managedCheck(Pool!=NULL,"cleanup Pool");
    xnetaddr Address;managedCheck(xrtNetAddrParse(&Address,Host,(uint16)Port),"proxy address");xmqttreconnectconfig K;managedCheck(xmqttReconnectConfigInit(&K),"controller defaults");K.Factory=managedFactory;K.FactoryData=&Address;K.Client.Version=Version;K.Client.SetupTimeout=10000;K.InitialDelay=5;K.MaxDelay=20;K.JitterPermille=0;K.RetryStatuses|=UINT32_C(1)<<XMQTT_STATUS_CLOSED;
    if(Fragment){K.Client.ReadBytes=37;K.Client.WriteBytes=41;K.Client.WriteChunkBytes=13;K.Client.WorkPackets=4;}
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=false;Q.Body.Connect.KeepAlive=5;Q.Body.Connect.ClientId=XRT_STR_LITERAL("xrt-session-recovery");
    xmqttproperty P[2]={{XMQTT_PROPERTY_SESSION_EXPIRY,XMQTT_PROPERTY_UINT32,{0}},{XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM,XMQTT_PROPERTY_UINT16,{0}}};P[0].Value.Number=60;P[1].Value.Number=4;if(Version==XMQTT_VERSION_5)Q.Properties=(xmqttpropertylist){P,2};
    xmqttreconnect* R=NULL;managedCheck(xmqttReconnectCreate(E,Pool,1,&K,&Q,&R),"automatic controller");xmqttclientlease L={0};xfuture* R1=managedOnline(R,1,false,&L);xstrview Topic=XRT_STR_LITERAL("xrt/qual/session/binary");managedSubscribe(L.Client,Topic);
    unsigned char* Payload=malloc(Bytes);managedCheck(Payload!=NULL,"bounded payload");for(size_t I=0;I<Bytes;++I)Payload[I]=(uint8)(I*37u+Mode);
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=Mode?4:2;Q.Body.Publish.Topic=Topic;Q.Body.Publish.Payload=(xbytesview){Payload,Bytes};xmqttproperty Alias={XMQTT_PROPERTY_TOPIC_ALIAS,XMQTT_PROPERTY_UINT16,{0}};Alias.Value.Number=1;if(Version==XMQTT_VERSION_5)Q.Properties=(xmqttpropertylist){&Alias,1};
    xfuture* F=NULL;managedCheck(xmqttClientPublishAsync(L.Client,&Q,10000,&F)==XMQTT_ADMISSION_ACCEPTED,"original publication");memset(Payload,0,Bytes);managedBarrier("WAIT_CUT","cut\n");
    if(Mode>=3){const xmqttwriteresult* W=managedOutcome(F);managedCheck(W->Status==XMQTT_STATUS_OK && W->PubcompReceived,"outgoing QoS2 completed before incoming cut");}
    managedCheck(xmqttClientClose(L.Client) && xmqttClientWaitFor(L.Client,15000)==XWAIT_OK,"physical first Client under retained lease");const xmqttwriteresult* Old=managedOutcome(F);
    managedCheck(Old->AcceptedBytes && Old->Identifier && Old->Status==(Mode>=3?XMQTT_STATUS_OK:XMQTT_STATUS_CLOSED) && Old->PubrecReceived==(Mode>=2) && Old->PubcompReceived==(Mode>=3),"immutable original connection result");
#if defined(XMQTT_RECONNECT_BROKER_OLD_RESULT)
    XMQTT_RECONNECT_BROKER_OLD_RESULT(Old,&Q);
#endif
    managedCheck(xmqttReconnectRelease(&L),"release enables automatic replacement");free(Payload);xfuture* R2=managedOnline(R,2,true,&L);managedBarrier("WAIT_DRAIN","drain\n");managedSubscribe(L.Client,XRT_STR_LITERAL("xrt/qual/session/barrier"));
    xmqttmessage *Held=NULL,*M=NULL;unsigned Count=0;double End=xrtTimer()+15;
    while(!xmqttReconnectTakeMessage(R,&Held)){managedCheck(xrtTimer()<End,"recovered Inbox");xrtSleep(1);}
    for(;;){const xmqttpacket* A=xmqttMessagePacket(Count?M:Held);managedCheck(A->Body.Publish.Topic.Size==Topic.Size && !memcmp(A->Body.Publish.Topic.Data,Topic.Data,Topic.Size) && A->Body.Publish.Payload.Size==Bytes && ((A->Flags>>1)&3u)==(Mode?2u:1u),"logical recovered message");
        for(size_t I=0;I<Bytes;++I)managedCheck(A->Body.Publish.Payload.Data[I]==(uint8)(I*37u+Mode),"binary snapshot survived automatic replacement");
        if(Count)xmqttMessageRelease(M);
        ++Count;if(!xmqttReconnectTakeMessage(R,&M))break;}
    managedCheck(!Mode || Count==1,"QoS2 one local delivery");xfuture* Closed=NULL;managedCheck(xmqttClientDisconnectAsync(L.Client,NULL,10000,&Closed)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)managedOutcome(Closed))->Status==XMQTT_STATUS_OK,"normal final DISCONNECT");
    managedCheck(xmqttReconnectRelease(&L) && xmqttReconnectClose(R) && xmqttReconnectWaitFor(R,15000)==XWAIT_OK,"controller stopped");xmqttreconnectinfo Info;managedCheck(xmqttReconnectInfo(R,&Info) && Info.Attempts==2 && Info.Connections==2,"exactly two automatic generations");
    managedCheck(xmqttReconnectDestroy(R) && xrtTaskPoolClose(Pool) && xrtTaskPoolWaitFor(Pool,5000)==XWAIT_OK && xrtTaskPoolDestroy(Pool) && xrtNetEngineStop(E) && xrtNetEngineDestroy(E),"all physical owners joined");
    managedCheck(Old==xrtFutureValue(F) && xmqttMessagePacket(Held)->Body.Publish.Payload.Size==Bytes && ((const xmqttreadyresult*)xrtFutureValue(R2))->Negotiated.SessionPresent,"retained outcomes after all owners");
    xmqttMessageRelease(Held);xrtFutureDestroy(F);xrtFutureDestroy(R1);xrtFutureDestroy(R2);xrtFutureDestroy(Closed);printf("[PASS] automatic broker MQTT%lu mode%u fragmented%u messages%u\n",V,Mode,Fragment?1u:0u,Count);return 0;
}
