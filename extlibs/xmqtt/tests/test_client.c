#include "test.h"
#include <xmqtt/client.h>
#include <xrt/thread.h>
#include <xrt/atomic.h>
#include <xrt/time.h>

/* In-process protocol fixture. Independent wire/broker interoperability lives
 * separately; this fixture exercises ownership, limits and worker reentrancy. */
enum {MQTT_TEST_HOLD=1,MQTT_TEST_REFUSE=2,MQTT_TEST_SESSION=4,MQTT_TEST_DUPLICATE=8,MQTT_TEST_NO_PONG=16,MQTT_TEST_KEEPALIVE=32,
    MQTT_TEST_HOLD_PUBACK=64,MQTT_TEST_REJECT_PUBACK=128,MQTT_TEST_WINDOW_ONE=256,MQTT_TEST_MAX_QOS_ZERO=512,MQTT_TEST_UNCACHED_NODES=1024,MQTT_TEST_GRACEFUL=2048,MQTT_TEST_HOLD_COMP=4096,MQTT_TEST_REJECT_REC=8192,MQTT_TEST_REJECT_COMP=16384,MQTT_TEST_SEND_ALIAS=32768};
typedef struct mqttTestPeer {
    xnetengine* Engine;xnetlistener* Listener;xatomicptr Stream;xnetaddr Address;
    xatomic32 Closed,ListenerClosed,Connects,Publishes,Pings,NotifyChecks;
    xatomicptr CallbackFuture,CallbackMessage;xatomic32 CallbackSubmit;
    xatomic32 LastIdentifier,QosZeroes,Acks,AckIdentifier,Disconnects,DisconnectReason,ReceiveRecs,ReceiveComps,CompReason,Subscriptions;
    char SendAliases[16][64],PublishedTopics[16][64];xatomic32 CompactPublishes;
    unsigned Mode;xmqttversion Version;unsigned char Input[65536];size_t Used;
} mqttTestPeer;
static void mqttTestWait(xatomic32* A,uint32 N)
{double End=xrtTimer()+5;while(xrtAtomic32Load(A,XMEMORY_ACQUIRE)<N){testRequire(xrtTimer()<End,"MQTT fixture wait expired");xrtSleep(1);}}
static xnetstream* mqttTestStream(mqttTestPeer* P)
{return xrtAtomicPtrLoad(&P->Stream,XMEMORY_ACQUIRE);}
static void mqttTestSend(mqttTestPeer* P,const void* Bytes,size_t Size)
{xnetresult R=xrtNetStreamSend(mqttTestStream(P),Bytes,Size);testRequire(R==XNET_RESULT_OK || R==XNET_RESULT_CLOSED || xrtNetStreamState(mqttTestStream(P))>=XNET_STREAM_CLOSING,"fixture send");}
static void mqttTestAck(mqttTestPeer* P)
{
    xmqttpacket A;xmqttPacketInit(XMQTT_PACKET_CONNACK,&A);A.SessionPresent=(P->Mode&MQTT_TEST_SESSION)!=0;
    A.Reason=(P->Mode&MQTT_TEST_REFUSE)?(P->Version==XMQTT_VERSION_5?0x87:5):0;
    xmqttproperty Props[4]={{XMQTT_PROPERTY_SERVER_KEEP_ALIVE,XMQTT_PROPERTY_UINT16,{0}},
        {XMQTT_PROPERTY_RECEIVE_MAXIMUM,XMQTT_PROPERTY_UINT16,{0}},
        {XMQTT_PROPERTY_MAXIMUM_QOS,XMQTT_PROPERTY_BYTE,{0}}};size_t Count=0;
    if(P->Mode&MQTT_TEST_KEEPALIVE){Props[Count]=Props[0];Props[Count++].Value.Number=1;}
    if(P->Mode&MQTT_TEST_WINDOW_ONE){Props[Count]=(xmqttproperty){XMQTT_PROPERTY_RECEIVE_MAXIMUM,XMQTT_PROPERTY_UINT16,{0}};Props[Count++].Value.Number=1;}
    if(P->Mode&MQTT_TEST_MAX_QOS_ZERO)Props[Count++]=(xmqttproperty){XMQTT_PROPERTY_MAXIMUM_QOS,XMQTT_PROPERTY_BYTE,{0}};
    if(P->Mode&MQTT_TEST_SEND_ALIAS){Props[Count]=(xmqttproperty){XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM,XMQTT_PROPERTY_UINT16,{0}};Props[Count++].Value.Number=16;}
    if(P->Version==XMQTT_VERSION_5)A.Properties=(xmqttpropertylist){Props,Count};
    unsigned char Wire[64];size_t N=0;
    testRequire(P->Version==XMQTT_VERSION_5?xmqttPacketWrite5(Wire,sizeof(Wire),&A,XMQTT_SERVER_TO_CLIENT,NULL,&N):xmqttPacketWrite311(Wire,sizeof(Wire),&A,XMQTT_SERVER_TO_CLIENT,NULL,&N),"fixture CONNACK encode");
    mqttTestSend(P,Wire,N);if(P->Mode&MQTT_TEST_DUPLICATE)mqttTestSend(P,Wire,N);
}
static void mqttTestPuback(mqttTestPeer* P,uint16 Identifier)
{
    xmqttpacket A;xmqttPacketInit(XMQTT_PACKET_PUBACK,&A);A.Identifier=Identifier;
    xmqttproperty Props[2]={{XMQTT_PROPERTY_REASON_STRING,XMQTT_PROPERTY_STRING,{0}},{XMQTT_PROPERTY_USER,XMQTT_PROPERTY_STRING_PAIR,{0}}};
    Props[0].Value.Text=XRT_STR_LITERAL("acknowledged");Props[1].Value.Pair=(xmqttstringpair){XRT_STR_LITERAL("node"),XRT_STR_LITERAL("peer")};
    if(P->Version==XMQTT_VERSION_5){A.Reason=(P->Mode&MQTT_TEST_REJECT_PUBACK)?0x87:0;A.Properties=(xmqttpropertylist){Props,2};}
    unsigned char Wire[128];size_t N=0;
    testRequire(P->Version==XMQTT_VERSION_5?xmqttPacketWrite5(Wire,sizeof(Wire),&A,XMQTT_SERVER_TO_CLIENT,NULL,&N):xmqttPacketWrite311(Wire,sizeof(Wire),&A,XMQTT_SERVER_TO_CLIENT,NULL,&N),"fixture PUBACK encode");mqttTestSend(P,Wire,N);
}
static void mqttTestQosTwoAck(mqttTestPeer* P,xmqttpackettype Type,uint16 Id)
{
    xmqttpacket A;xmqttPacketInit(Type,&A);A.Identifier=Id;
    if(P->Version==XMQTT_VERSION_5)A.Reason=Type==XMQTT_PACKET_PUBREC && (P->Mode&MQTT_TEST_REJECT_REC)?0x87:Type==XMQTT_PACKET_PUBCOMP && (P->Mode&MQTT_TEST_REJECT_COMP)?0x92:0;
    unsigned char Wire[32];size_t Size=0;testRequire(P->Version==XMQTT_VERSION_5?xmqttPacketWrite5(Wire,sizeof(Wire),&A,XMQTT_SERVER_TO_CLIENT,NULL,&Size):xmqttPacketWrite311(Wire,sizeof(Wire),&A,XMQTT_SERVER_TO_CLIENT,NULL,&Size),"QoS2 fixture ACK encode");mqttTestSend(P,Wire,Size);
}
static void mqttTestRead(xnetstream* S,xnetbuf* Buffer,ptr Data)
{
    (void)S;mqttTestPeer* P=Data;size_t N=xrtNetBufSize(Buffer);
    testRequire(N<=sizeof(P->Input)-P->Used,"fixture input capacity");
    P->Used+=xrtNetBufRead(Buffer,P->Input+P->Used,N);
        while(P->Used){xmqttwireconfig K;xmqttWireConfigInit(&K);K.Version=P->Version;xmqttframe F;
            xmqttwirestatus Status=xmqttFrameRead((xbytesview){P->Input,P->Used},&K,&F);
            testRequire(Status!=XMQTT_WIRE_ERROR,"fixture received malformed framing");if(Status==XMQTT_WIRE_MORE)break;
            if(F.Header.Type==XMQTT_PACKET_CONNECT){xrtAtomic32FetchAdd(&P->Connects,1,XMEMORY_RELEASE);if(!(P->Mode&MQTT_TEST_HOLD))mqttTestAck(P);}
            else if(F.Header.Type==XMQTT_PACKET_PUBLISH){
                if(P->Mode&MQTT_TEST_SEND_ALIAS){xmqttpacket Pub;xmqttproperty Props[8];xmqttcodecbuffers B={Props,8,NULL,0};testRequire(xmqttPacketRead5(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&B,&Pub,NULL)==XMQTT_CODEC_DONE,"outgoing alias fixture decode");
                    uint16 Alias=0;for(size_t I=0;I<Pub.Properties.Count;++I)if(Props[I].Id==XMQTT_PROPERTY_TOPIC_ALIAS)Alias=(uint16)Props[I].Value.Number;
                    testRequire(Alias && Alias<=16,"fixture outgoing alias limit");xstrview Topic=Pub.Body.Publish.Topic;
                    if(Topic.Size){testRequire(Topic.Size<64,"fixture topic cap");memcpy(P->SendAliases[Alias-1],Topic.Data,Topic.Size);P->SendAliases[Alias-1][Topic.Size]=0;}
                    else {testRequire(P->SendAliases[Alias-1][0],"no unresolved alias on wire");xrtAtomic32FetchAdd(&P->CompactPublishes,1,XMEMORY_RELEASE);}
                    uint32 N=xrtAtomic32Load(&P->Publishes,XMEMORY_RELAXED);testRequire(N<16,"fixture publish log");strcpy(P->PublishedTopics[N],P->SendAliases[Alias-1]);
                    if(((Pub.Flags>>1)&3u)==1 && !(P->Mode&MQTT_TEST_HOLD_PUBACK))mqttTestPuback(P,Pub.Identifier);
                    if(((Pub.Flags>>1)&3u)==2)mqttTestQosTwoAck(P,XMQTT_PACKET_PUBREC,Pub.Identifier);
                }else if((F.Header.Flags>>1)&3u){xmqttpacket Pub;xmqttproperty Props[8];xmqttcodecbuffers B={Props,8,NULL,0};
                    testRequire((P->Version==XMQTT_VERSION_5?xmqttPacketRead5(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&B,&Pub,NULL):xmqttPacketRead311(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&B,&Pub,NULL))==XMQTT_CODEC_DONE,"fixture QoS1 publish decode");
                    xrtAtomic32Store(&P->LastIdentifier,Pub.Identifier,XMEMORY_RELEASE);
                    if(!(P->Mode&MQTT_TEST_HOLD_PUBACK)){if(((Pub.Flags>>1)&3u)==2)mqttTestQosTwoAck(P,XMQTT_PACKET_PUBREC,Pub.Identifier);else mqttTestPuback(P,Pub.Identifier);}
                }else {mqttTestSend(P,F.Wire.Data,F.Wire.Size);xrtAtomic32FetchAdd(&P->QosZeroes,1,XMEMORY_RELEASE);}
                xrtAtomic32FetchAdd(&P->Publishes,1,XMEMORY_RELEASE);
            }
            else if(F.Header.Type==XMQTT_PACKET_SUBSCRIBE || F.Header.Type==XMQTT_PACKET_UNSUBSCRIBE){
                xmqttpacket Q;xmqttproperty Props[8];xmqttsubscription Topics[16];xmqttcodecbuffers B={Props,8,Topics,16};
                testRequire((P->Version==XMQTT_VERSION_5?xmqttPacketRead5(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&B,&Q,NULL):xmqttPacketRead311(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&B,&Q,NULL))==XMQTT_CODEC_DONE,"fixture subscription decode");
                xrtAtomic32Store(&P->LastIdentifier,Q.Identifier,XMEMORY_RELEASE);
                if(!(P->Mode&MQTT_TEST_HOLD_PUBACK)){
                    xmqttpacket A;xmqttPacketInit(Q.Type==XMQTT_PACKET_SUBSCRIBE?XMQTT_PACKET_SUBACK:XMQTT_PACKET_UNSUBACK,&A);A.Identifier=Q.Identifier;
                    unsigned char Reasons[16];for(size_t I=0;I<Q.Body.Topics.Count;++I)Reasons[I]=Q.Type==XMQTT_PACKET_SUBSCRIBE?(Topics[I].Options&3u):0;
                    if(A.Type==XMQTT_PACKET_SUBACK || P->Version==XMQTT_VERSION_5)A.Body.Reasons=(xbytesview){Reasons,Q.Body.Topics.Count};
                    unsigned char Wire[128];size_t Size=0;
                    testRequire(P->Version==XMQTT_VERSION_5?xmqttPacketWrite5(Wire,sizeof(Wire),&A,XMQTT_SERVER_TO_CLIENT,NULL,&Size):xmqttPacketWrite311(Wire,sizeof(Wire),&A,XMQTT_SERVER_TO_CLIENT,NULL,&Size),"fixture subscription ACK encode");mqttTestSend(P,Wire,Size);
                }
                xrtAtomic32FetchAdd(&P->Subscriptions,1,XMEMORY_RELEASE);
            }
            else if(F.Header.Type==XMQTT_PACKET_PUBREL){testRequire(F.Wire.Size==4 && F.Header.Flags==2,"minimal outgoing PUBREL");const unsigned char* W=F.Wire.Data;uint16 Id=(uint16)(((uint16)W[2]<<8)|W[3]);xrtAtomic32FetchAdd(&P->Acks,1,XMEMORY_RELEASE);if(!(P->Mode&MQTT_TEST_HOLD_COMP))mqttTestQosTwoAck(P,XMQTT_PACKET_PUBCOMP,Id);}
            else if(F.Header.Type==XMQTT_PACKET_PUBREC || F.Header.Type==XMQTT_PACKET_PUBCOMP){const unsigned char* W=F.Wire.Data;testRequire(F.Wire.Size==4 || (P->Version==XMQTT_VERSION_5 && F.Header.Type==XMQTT_PACKET_PUBCOMP && F.Wire.Size==5 && W[4]==0x92),"receiver minimal QoS2 acknowledgement");xrtAtomic32Store(&P->AckIdentifier,((uint32)W[2]<<8)|W[3],XMEMORY_RELEASE);if(F.Header.Type==XMQTT_PACKET_PUBREC)xrtAtomic32FetchAdd(&P->ReceiveRecs,1,XMEMORY_RELEASE);else {xrtAtomic32Store(&P->CompReason,F.Wire.Size==5?W[4]:0,XMEMORY_RELEASE);xrtAtomic32FetchAdd(&P->ReceiveComps,1,XMEMORY_RELEASE);}}
            else if(F.Header.Type==XMQTT_PACKET_PUBACK){testRequire(F.Wire.Size==4,"minimal receiver PUBACK");const unsigned char* W=F.Wire.Data;xrtAtomic32Store(&P->AckIdentifier,((uint32)W[2]<<8)|W[3],XMEMORY_RELEASE);xrtAtomic32FetchAdd(&P->Acks,1,XMEMORY_RELEASE);}
            else if(F.Header.Type==XMQTT_PACKET_DISCONNECT){const unsigned char* W=F.Wire.Data;if(!(P->Mode&MQTT_TEST_GRACEFUL))testRequire(P->Version==XMQTT_VERSION_5 && F.Wire.Size==3 && W[1]==1,"minimal diagnostic DISCONNECT");xrtAtomic32Store(&P->DisconnectReason,F.Wire.Size>2?W[2]:0,XMEMORY_RELEASE);xrtAtomic32FetchAdd(&P->Disconnects,1,XMEMORY_RELEASE);testRequire(xrtNetStreamClose(S),"peer diagnostic close");}
            else {testRequire(F.Header.Type==XMQTT_PACKET_PINGREQ,"fixture unexpected client packet");xrtAtomic32FetchAdd(&P->Pings,1,XMEMORY_RELEASE);if(!(P->Mode&MQTT_TEST_NO_PONG)){const unsigned char Pong[]={0xd0,0};mqttTestSend(P,Pong,sizeof(Pong));}}
            P->Used-=F.Wire.Size;if(P->Used)memmove(P->Input,P->Input+F.Wire.Size,P->Used);
        }
}
static void mqttTestEnd(xnetstream* S,ptr Data)
{(void)Data;xrtNetStreamAbort(S);}
static void mqttTestClosed(xnetstream* S,xnetresult R,const xerror* Error,ptr Data)
{(void)S;(void)R;(void)Error;xrtAtomic32Store(&((mqttTestPeer*)Data)->Closed,1,XMEMORY_RELEASE);}
static bool mqttTestAccept(xnetlistener* L,xnetstream* S,ptr Data)
{
    (void)L;mqttTestPeer* P=Data;testRequire(!mqttTestStream(P),"unexpected replay/second connection");
    static const xnetstreamevents E={NULL,mqttTestRead,mqttTestEnd,NULL,NULL,NULL,mqttTestClosed};
    testRequire(xrtNetStreamSetEvents(S,&E,P),"fixture accepted stream events");xrtAtomicPtrStore(&P->Stream,S,XMEMORY_RELEASE);return true;
}
static void mqttTestListenerClosed(xnetlistener* L,ptr Data)
{(void)L;xrtAtomic32Store(&((mqttTestPeer*)Data)->ListenerClosed,1,XMEMORY_RELEASE);}
static void mqttTestStart(mqttTestPeer* P,xmqttversion Version,unsigned Mode)
{
    memset(P,0,sizeof(*P));P->Version=Version;P->Mode=Mode;xrtAtomicPtrInit(&P->Stream,NULL);
    xrtAtomic32Init(&P->Closed,0);xrtAtomic32Init(&P->ListenerClosed,0);xrtAtomic32Init(&P->Connects,0);xrtAtomic32Init(&P->Publishes,0);xrtAtomic32Init(&P->Pings,0);xrtAtomic32Init(&P->NotifyChecks,0);
    xrtAtomicPtrInit(&P->CallbackFuture,NULL);xrtAtomicPtrInit(&P->CallbackMessage,NULL);xrtAtomic32Init(&P->CallbackSubmit,0);
    xrtAtomic32Init(&P->CompactPublishes,0);xrtAtomic32Init(&P->LastIdentifier,0);xrtAtomic32Init(&P->QosZeroes,0);xrtAtomic32Init(&P->Acks,0);xrtAtomic32Init(&P->AckIdentifier,0);xrtAtomic32Init(&P->Disconnects,0);xrtAtomic32Init(&P->DisconnectReason,0);
    xrtAtomic32Init(&P->ReceiveRecs,0);xrtAtomic32Init(&P->ReceiveComps,0);xrtAtomic32Init(&P->CompReason,0);xrtAtomic32Init(&P->Subscriptions,0);
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;if(Mode&MQTT_TEST_UNCACHED_NODES)E.NodeCacheBytes=0;P->Engine=xrtNetEngineCreate(&E);testRequire(P->Engine && xrtNetEngineStart(P->Engine),"MQTT fixture engine");
    xnetlistenconfig K;xrtNetListenConfigInit(&K);xrtNetAddrParse(&K.Address,"127.0.0.1",0);K.Distribution=XNET_ACCEPT_LOCAL;
    static const xnetlistenerevents L={mqttTestAccept,NULL,mqttTestListenerClosed};
    P->Listener=xrtNetListen(P->Engine,&K,&L,NULL,P);testRequire(P->Listener && xrtNetListenerLocal(P->Listener,&P->Address),"MQTT fixture listener");
}
static void mqttTestStop(mqttTestPeer* P)
{
    testRequire(xrtNetListenerClose(P->Listener),"fixture listener close");mqttTestWait(&P->ListenerClosed,1);
    xnetstream* S=mqttTestStream(P);if(S)mqttTestWait(&P->Closed,1);
    xrtNetStreamDestroy(S);xrtNetListenerDestroy(P->Listener);testRequire(xrtNetEngineStop(P->Engine) && xrtNetEngineDestroy(P->Engine),"MQTT engine physical owners leaked");
}
static xmqttpacket mqttTestConnectPacket(void)
{xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_CONNECT,&P);P.Body.Connect.ClientId=XRT_STR_LITERAL("client");P.Body.Connect.CleanStart=true;return P;}
static xmqttclient* mqttTestConnect(mqttTestPeer* P,xmqttclientconfig* Config)
{
    xmqttclientconfig K;if(Config)K=*Config;else xmqttClientConfigInit(&K);K.Version=P->Version;
    xmqttpacket Connect=mqttTestConnectPacket();if(P->Mode&MQTT_TEST_SESSION)Connect.Body.Connect.CleanStart=false;
    xmqttclient* C=NULL;bool OK=xmqttClientConnect(P->Engine,&P->Address,1,&K,&Connect,&C);
    if(!OK)fprintf(stderr,"connect: %s\n",xrtErrorMessage(xrtGetError()));
    testRequire(OK,"MQTT connect factory");return C;
}
static const void* mqttTestResult(xfuture* F)
{testRequire(F && xrtFutureWaitFor(F,5000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"MQTT typed result wait");return xrtFutureValue(F);}
static xfuture* mqttTestPublish(xmqttclient* C,int64 Timeout)
{
    xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Body.Publish.Topic=XRT_STR_LITERAL("topic");P.Body.Publish.Payload=XRT_BYTES_LITERAL("payload");
    xfuture* F=NULL;testRequire(xmqttClientPublishAsync(C,&P,Timeout,&F)==XMQTT_ADMISSION_ACCEPTED,"MQTT publish admission");return F;
}
static void mqttTestClose(xmqttclient* C)
{
    testRequire(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK,"MQTT physical close wait");xmqttclientinfo I;
    testRequire(xmqttClientInfo(C,&I) && I.State==XMQTT_CLIENT_CLOSED && !I.Requests && !I.OutgoingBytes && !I.StartupBytes && I.Accepted==I.Completed,"MQTT retired accounting");
    testRequire(xmqttClientDestroy(C),"MQTT destroy");
}
static void mqttTestNotify(xmqttclient* C,ptr Data)
{
    mqttTestPeer* P=Data;
    testRequire(xmqttClientWaitFor(C,0)==XWAIT_ERROR && !xmqttClientDestroy(C),"IO worker Wait/Destroy restriction");
    xrtClearError();xrtAtomic32FetchAdd(&P->NotifyChecks,1,XMEMORY_RELEASE);
}
static void testMqttClientRoundtrip(void)
{
    for(unsigned V=4;V<=5;++V)for(unsigned Tiny=0;Tiny<2;++Tiny){mqttTestPeer P;mqttTestStart(&P,(xmqttversion)V,0);xmqttclientconfig K;xmqttClientConfigInit(&K);
        K.Notify=mqttTestNotify;K.User=&P;if(Tiny){K.Stream.WriteLimit=1;K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;K.WriteBytes=K.WriteChunkBytes=1;K.ReadBytes=1;K.WorkPackets=1;}
        xmqttclient* C=mqttTestConnect(&P,&K);xfuture* Ready=xmqttClientReady(C);const xmqttreadyresult* R=mqttTestResult(Ready);
        testRequire(R->Status==XMQTT_STATUS_OK && R->Negotiated.Version==(xmqttversion)V && R->Connack && R->Negotiated.ClientId.Size==6 && !memcmp(R->Negotiated.ClientId.Data,"client",6),"MQTT CONNACK negotiation");
        xfuture* F=mqttTestPublish(C,2000);const xmqttwriteresult* W=mqttTestResult(F);
        testRequire(W->Status==XMQTT_STATUS_OK && W->AcceptedBytes>7 && W->Delivery==XMQTT_MAY_HAVE_REACHED_PEER,"QoS-0 native retirement result");mqttTestWait(&P.Publishes,1);
        xmqttmessage* M=NULL;double End=xrtTimer()+5;while(!xmqttClientTakeMessage(C,&M)){testRequire(xrtTimer()<End,"MQTT receive wait");xrtSleep(1);}
        testRequire(xmqttMessageRetain(M),"MQTT message retain");xmqttMessageRelease(M);mqttTestClose(C);mqttTestStop(&P);
        const xmqttpacket* Q=xmqttMessagePacket(M);testRequire(Q && Q->Body.Publish.Topic.Size==5 && !memcmp(Q->Body.Publish.Topic.Data,"topic",5) && Q->Body.Publish.Payload.Size==7 && !memcmp(Q->Body.Publish.Payload.Data,"payload",7),"retained receive after Client/Engine destruction");
        testRequire(W->Status==XMQTT_STATUS_OK && R->Negotiated.ClientId.Size==6 && !memcmp(R->Negotiated.ClientId.Data,"client",6) && xmqttMessagePacket(R->Connack)->Type==XMQTT_PACKET_CONNACK,"retained Future results after Client/Engine destruction");
        xmqttMessageRelease(M);xrtFutureDestroy(F);xrtFutureDestroy(Ready);
    }
}
static void testMqttClientTerminal(void)
{
    const unsigned Modes[]={MQTT_TEST_REFUSE,MQTT_TEST_SESSION,MQTT_TEST_DUPLICATE,MQTT_TEST_HOLD};
    const xmqttclientstatus Expected[]={XMQTT_STATUS_REJECTED,XMQTT_STATUS_SESSION,XMQTT_STATUS_PROTOCOL,XMQTT_STATUS_TIMEOUT};
    for(unsigned V=4;V<=5;++V)for(size_t J=0;J<4;++J){mqttTestPeer P;mqttTestStart(&P,(xmqttversion)V,Modes[J]);xmqttclientconfig K;xmqttClientConfigInit(&K);K.SetupTimeout=100;
        xmqttclient* C=mqttTestConnect(&P,&K);xfuture* Ready=xmqttClientReady(C);const xmqttreadyresult* R=mqttTestResult(Ready);
        if(J!=2)testRequire(R->Status==Expected[J],"MQTT readiness typed terminal cause");
        testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"MQTT terminal physical wait");xmqttclientinfo I;xmqttClientInfo(C,&I);testRequire(I.CloseStatus==Expected[J],"MQTT connection terminal cause");
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(Ready);
    }
}
static void testMqttClientAdmissionCancel(void)
{
    mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,MQTT_TEST_HOLD);xmqttclientconfig K;xmqttClientConfigInit(&K);K.MaxRequests=1;K.SetupTimeout=2000;
    xmqttclient* C=mqttTestConnect(&P,&K);xfuture* Ready=xmqttClientReady(C);xfuture* F=mqttTestPublish(C,500);
    xmqttpacket Pub;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);Pub.Body.Publish.Topic=XRT_STR_LITERAL("topic");xfuture* Sentinel=(xfuture*)(uintptr_t)1;
    testRequire(xmqttClientPublishAsync(C,&Pub,100,&Sentinel)==XMQTT_ADMISSION_OVERLOADED && Sentinel==(xfuture*)(uintptr_t)1,"finite admission/output atomicity");
    testRequire(xrtFutureCancel(F),"explicit Future cancellation");const xmqttwriteresult* W=mqttTestResult(F);
    testRequire(W->Status==XMQTT_STATUS_CANCELLED && W->Delivery==XMQTT_NOT_SENT && !W->AcceptedBytes,"unsent cancellation must not retire connection");xrtFutureDestroy(F);
    double RetireBy=xrtTimer()+2;xmqttclientinfo Retired;
    do{xmqttClientInfo(C,&Retired);testRequire(xrtTimer()<RetireBy,"cancelled request physical retirement");if(Retired.Requests)xrtSleep(1);}while(Retired.Requests);
    F=mqttTestPublish(C,20);W=mqttTestResult(F);testRequire(W->Status==XMQTT_STATUS_TIMEOUT && !W->AcceptedBytes,"queued deadline includes handshake");
    testRequire(xrtFutureCancel(Ready),"readiness cancellation");const xmqttreadyresult* R=mqttTestResult(Ready);testRequire(R->Status==XMQTT_STATUS_CANCELLED,"readiness cancelled typed status");
    mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(F);xrtFutureDestroy(Ready);
}
static void testMqttClientKeepalive(void)
{
    for(unsigned NoPong=0;NoPong<2;++NoPong){mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,MQTT_TEST_KEEPALIVE|(NoPong?MQTT_TEST_NO_PONG:0));xmqttclientconfig K;xmqttClientConfigInit(&K);
        K.PingTimeout=100;K.Stream.WriteLimit=1;K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;K.WriteBytes=1;K.WriteChunkBytes=1;
        xmqttclient* C=mqttTestConnect(&P,&K);xfuture* F=xmqttClientReady(C);const xmqttreadyresult* R=mqttTestResult(F);testRequire(R->Status==XMQTT_STATUS_OK && R->Negotiated.KeepAlive==1,"server keepalive override");mqttTestWait(&P.Pings,1);
        if(NoPong){testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"missing PINGRESP timeout");xmqttclientinfo I;xmqttClientInfo(C,&I);testRequire(I.CloseStatus==XMQTT_STATUS_TIMEOUT,"PINGRESP timeout status");}
        else {double End=xrtTimer()+2;xmqttclientinfo I;do{xmqttClientInfo(C,&I);testRequire(xrtTimer()<End,"PONG wait");xrtSleep(1);}while(!I.Pongs);testRequire(I.Pings==1 && I.State==XMQTT_CLIENT_OPEN,"one-byte PING frame completed without interleaving");}
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(F);
    }
}
static void testMqttClientReceiveBounds(void)
{
    for(unsigned Kind=0;Kind<4;++Kind){mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,0);xmqttclientconfig K;xmqttClientConfigInit(&K);
        if(Kind==0)K.MaxMessages=1;
        if(Kind==1)K.MaxObjects=2;
        if(Kind==3)K.MaxPrivateBytes=16384;
        xmqttclient* C=mqttTestConnect(&P,&K);xfuture* F=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)mqttTestResult(F))->Status==XMQTT_STATUS_OK,"receive bounds ready");
        const unsigned char Normal[]={0x30,5,0,1,'t',0,'x'},Alias[]={0x30,8,0,1,'t',3,0x23,0,1,'x'},Qos1[]={0x34,7,0,1,'t',0,1,0,'x'};
        if(Kind<2){mqttTestSend(&P,Normal,sizeof(Normal));if(Kind==0)mqttTestSend(&P,Normal,sizeof(Normal));}
        else if(Kind==2)mqttTestSend(&P,Alias,sizeof(Alias));else mqttTestSend(&P,Qos1,sizeof(Qos1));
        testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"receive policy failure closes connection");xmqttclientinfo I;xmqttClientInfo(C,&I);
        testRequire(I.CloseStatus==(Kind==2?XMQTT_STATUS_PROTOCOL:XMQTT_STATUS_LIMIT),"receive typed bound/protocol classification");
        xmqttmessage* M=NULL;if(Kind==0){testRequire(I.Messages==1 && I.Received==1 && xmqttClientTakeMessage(C,&M),"queued receive survives abortive close");xmqttMessageRelease(M);}
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(F);
    }
}
static void testMqttClientObjectAdmission(void)
{
    mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,0);xmqttclientconfig K;xmqttClientConfigInit(&K);K.MaxObjects=2;
    xmqttclient* C=mqttTestConnect(&P,&K);xfuture* F=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)mqttTestResult(F))->Status==XMQTT_STATUS_OK,"object admission ready");
    xmqttpacket Pub;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);Pub.Body.Publish.Topic=XRT_STR_LITERAL("t");xfuture* Out=(xfuture*)(uintptr_t)1;
    xrtSetErrorInfo(XERR_STATE,"ambient",77,"preserve");const xerror* Ambient=xrtGetError();
    testRequire(xmqttClientPublishAsync(C,&Pub,100,&Out)==XMQTT_ADMISSION_OVERLOADED && Out==(xfuture*)(uintptr_t)1 && xrtGetError()==Ambient,"retained readiness objects consume independent finite quota");
    xmqttclientinfo I;xmqttClientInfo(C,&I);testRequire(!I.Requests && !I.OutgoingBytes && I.Objects==2 && I.Rejected==1,"object overload refunds physical reservation");
    xrtClearError();mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(F);
}
static void mqttTestReentrantNotify(xmqttclient* C,ptr Data)
{
    mqttTestPeer* P=Data;xmqttclientinfo I;testRequire(xmqttClientInfo(C,&I),"notification snapshot");
    if(I.Ready && !xrtAtomic32Exchange(&P->CallbackSubmit,1,XMEMORY_RELAXED)){
        xfuture* F=mqttTestPublish(C,1000);xrtAtomicPtrStore(&P->CallbackFuture,F,XMEMORY_RELEASE);
    }
    xmqttmessage* M=NULL;if(xmqttClientTakeMessage(C,&M)){
        testRequire(!xrtAtomicPtrLoad(&P->CallbackMessage,XMEMORY_ACQUIRE),"one notification receive");
        xrtAtomicPtrStore(&P->CallbackMessage,M,XMEMORY_RELEASE);testRequire(xmqttClientClose(C),"reentrant notification close");
    }
}
static void testMqttClientReentrancy(void)
{
    mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,0);xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=mqttTestReentrantNotify;K.User=&P;
    xmqttclient* C=mqttTestConnect(&P,&K);xfuture* F=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)mqttTestResult(F))->Status==XMQTT_STATUS_OK,"reentrant ready");
    testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"notification submit/take/close physical retirement");
    xfuture* W=xrtAtomicPtrLoad(&P.CallbackFuture,XMEMORY_ACQUIRE);const xmqttwriteresult* Result=mqttTestResult(W);
    testRequire(Result->Status==XMQTT_STATUS_OK || Result->Status==XMQTT_STATUS_CLOSED,"reentrant write can observe close before its physical tail");
    xmqttmessage* M=xrtAtomicPtrLoad(&P.CallbackMessage,XMEMORY_ACQUIRE);testRequire(M && xmqttMessagePacket(M)->Body.Publish.Payload.Size==7,"notification transferred immutable message");
    mqttTestClose(C);mqttTestStop(&P);xmqttMessageRelease(M);xrtFutureDestroy(W);xrtFutureDestroy(F);
}
static void testMqttClientAllocationFailure(void)
{
    const size_t Limits[]={1,1024,8192,12288,16384,32768,65536,262144};unsigned Failures=0,Successes=0;
    for(size_t J=0;J<sizeof(Limits)/sizeof(*Limits);++J){mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,0);
        xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[J],&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"factory allocation quota");
        xmqttpacket Connect=mqttTestConnectPacket();xmqttclient* C=(xmqttclient*)(uintptr_t)1;
        bool OK=xmqttClientConnect(P.Engine,&P.Address,1,NULL,&Connect,&C);testRequire(xrtAllocationBudgetLeave(&Scope),"factory quota leave");
        if(OK){++Successes;xfuture* F=xmqttClientReady(C);(void)mqttTestResult(F);mqttTestClose(C);xrtFutureDestroy(F);}
        else {++Failures;testRequire(C==(xmqttclient*)(uintptr_t)1,"factory OOM preserves output");}
        mqttTestStop(&P);xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);
        testRequire(xrtAllocationBudgetInfo(Budget,&I) && !I.LiveBytes && !I.LiveCount,"factory OOM/lifetime charged bytes leaked");xrtAllocationBudgetFree(Budget);
    }
    testRequire(Failures && Successes,"factory OOM matrix exercised failure and success");
    mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,0);xmqttclient* C=mqttTestConnect(&P,NULL);xfuture* F=xmqttClientReady(C);(void)mqttTestResult(F);
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"publish allocation quota");
    xmqttpacket Pub;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);Pub.Body.Publish.Topic=XRT_STR_LITERAL("t");xfuture* Out=(xfuture*)(uintptr_t)1;
    testRequire(xmqttClientPublishAsync(C,&Pub,100,&Out)==XMQTT_ADMISSION_ERROR && Out==(xfuture*)(uintptr_t)1,"publish OOM output atomicity");testRequire(xrtAllocationBudgetLeave(&Scope),"publish quota leave");
    xmqttclientinfo Info;xmqttClientInfo(C,&Info);testRequire(!Info.Requests && !Info.OutgoingBytes && !Info.Accepted,"publish OOM refunds reservation");
    xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(Budget,&I) && !I.LiveBytes && I.Rejections,"publish OOM charged byte leak");xrtAllocationBudgetFree(Budget);
    mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(F);
}
static xfuture* mqttTestPublishOne(xmqttclient* C,uint16 Identifier,int64 Timeout)
{
    xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=2;P.Identifier=Identifier;P.Body.Publish.Topic=XRT_STR_LITERAL("qos/one");P.Body.Publish.Payload=XRT_BYTES_LITERAL("data");xfuture* F=NULL;
    testRequire(xmqttClientPublishAsync(C,&P,Timeout,&F)==XMQTT_ADMISSION_ACCEPTED && P.Identifier==Identifier,"QoS1 admission/id input atomicity");return F;
}
static void testMqttQosOneCompletion(void)
{
    for(unsigned V=4;V<=5;++V)for(unsigned Reject=0;Reject<=(V==5?1u:0u);++Reject){mqttTestPeer P;mqttTestStart(&P,(xmqttversion)V,MQTT_TEST_HOLD_PUBACK|(Reject?MQTT_TEST_REJECT_PUBACK:0));
        xmqttclient* C=mqttTestConnect(&P,NULL);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);xfuture* F=mqttTestPublishOne(C,0,2000);
        mqttTestWait(&P.Publishes,1);testRequire(xrtFutureState(F)==XFUTURE_PENDING,"native write completion cannot complete QoS1");
        uint16 Id=(uint16)xrtAtomic32Load(&P.LastIdentifier,XMEMORY_ACQUIRE);testRequire(Id!=0,"automatic nonzero packet identifier");mqttTestPuback(&P,Id);
        const xmqttwriteresult* W=mqttTestResult(F);testRequire(W->Status==(Reject?XMQTT_STATUS_REJECTED:XMQTT_STATUS_OK) && W->Delivery==(Reject?XMQTT_PEER_REJECTED:XMQTT_PEER_ACCEPTED) && W->Identifier==Id && W->Reason==(Reject?0x87:0) && W->Puback,"QoS1 correlated broker decision");
        testRequire(xmqttMessageRetain(W->Puback),"separately retain PUBACK");const xmqttmessage* Ack=W->Puback;
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(Ready);xrtFutureDestroy(F);
        const xmqttpacket* A=xmqttMessagePacket(Ack);testRequire(A->Type==XMQTT_PACKET_PUBACK && A->Identifier==Id,"PUBACK ownership after Client/Engine/Future destruction");
        if(V==5)testRequire(A->Properties.Count==2 && A->Properties.Data[0].Value.Text.Size==12 && !memcmp(A->Properties.Data[0].Value.Text.Data,"acknowledged",12) && A->Properties.Data[1].Value.Pair.Value.Size==4 && !memcmp(A->Properties.Data[1].Value.Pair.Value.Data,"peer",4),"retained PUBACK reason/User Property views");
        xmqttMessageRelease(Ack);
    }
}
static void testMqttQosOneWindow(void)
{
    mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,MQTT_TEST_HOLD_PUBACK|MQTT_TEST_WINDOW_ONE);xmqttclient* C=mqttTestConnect(&P,NULL);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
    xfuture* F=mqttTestPublishOne(C,11,2000),*G=mqttTestPublishOne(C,22,2000),*Zero=mqttTestPublish(C,2000);
    testRequire(((const xmqttwriteresult*)mqttTestResult(Zero))->Status==XMQTT_STATUS_OK,"QoS0 proceeds at exhausted send quota");mqttTestWait(&P.QosZeroes,1);mqttTestWait(&P.Publishes,2);
    testRequire(xrtAtomic32Load(&P.Publishes,XMEMORY_ACQUIRE)==2 && xrtAtomic32Load(&P.LastIdentifier,XMEMORY_ACQUIRE)==11 && xrtFutureState(F)==XFUTURE_PENDING && xrtFutureState(G)==XFUTURE_PENDING,"Receive Maximum one blocks second QoS1 but not QoS0");
    mqttTestPuback(&P,11);testRequire(((const xmqttwriteresult*)mqttTestResult(F))->Delivery==XMQTT_PEER_ACCEPTED,"first PUBACK replenishes credit");mqttTestWait(&P.Publishes,3);
    testRequire(xrtAtomic32Load(&P.LastIdentifier,XMEMORY_ACQUIRE)==22,"second QoS1 begins after first acknowledgement");mqttTestPuback(&P,22);testRequire(((const xmqttwriteresult*)mqttTestResult(G))->Delivery==XMQTT_PEER_ACCEPTED,"second correlated PUBACK");
    mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(Ready);xrtFutureDestroy(F);xrtFutureDestroy(G);xrtFutureDestroy(Zero);
}
static void testMqttQosOneCancellationAndRestrictions(void)
{
    for(unsigned Timeout=0;Timeout<2;++Timeout){mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,MQTT_TEST_HOLD_PUBACK);xmqttclient* C=mqttTestConnect(&P,NULL);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
        xfuture* F=mqttTestPublishOne(C,42,Timeout?100:2000);mqttTestWait(&P.Publishes,1);
        xmqttpacket Pub;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);Pub.Flags=2;Pub.Identifier=42;Pub.Body.Publish.Topic=XRT_STR_LITERAL("t");xfuture* Sentinel=(xfuture*)(uintptr_t)1;
        testRequire(xmqttClientPublishAsync(C,&Pub,100,&Sentinel)==XMQTT_ADMISSION_OVERLOADED && Sentinel==(xfuture*)(uintptr_t)1,"busy identifier cannot be reused");
        if(!Timeout)testRequire(xrtFutureCancel(F),"cancel awaiting PUBACK");
        const xmqttwriteresult* W=mqttTestResult(F);
        testRequire(W->Status==(Timeout?XMQTT_STATUS_TIMEOUT:XMQTT_STATUS_CANCELLED) && W->Delivery==XMQTT_MAY_HAVE_REACHED_PEER && W->Identifier==42 && !W->Puback,"QoS1 no ACK means uncertain delivery");
        testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"sent QoS1 cancellation/timeout retires connection");mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(Ready);xrtFutureDestroy(F);
    }
    for(unsigned Kind=0;Kind<2;++Kind){mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,Kind?0:MQTT_TEST_MAX_QOS_ZERO);xmqttclientconfig K;xmqttClientConfigInit(&K);if(Kind)K.MaxObjects=3;
        xmqttclient* C=mqttTestConnect(&P,&K);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);xfuture* F=mqttTestPublishOne(C,0,1000);const xmqttwriteresult* W=mqttTestResult(F);
        testRequire(W->Status==(Kind?XMQTT_STATUS_LIMIT:XMQTT_STATUS_RESTRICTED) && W->Delivery==(Kind?XMQTT_PEER_ACCEPTED:XMQTT_NOT_SENT) && !W->Puback,"peer QoS restriction / PUBACK metadata limit retains scalar decision");
        if(Kind)testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"ACK metadata exhaustion retires connection");
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(Ready);xrtFutureDestroy(F);
    }
}
static void testMqttQosOneIdsAndMismatches(void)
{
    for(unsigned Kind=0;Kind<3;++Kind){mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,MQTT_TEST_HOLD_PUBACK|MQTT_TEST_WINDOW_ONE);xmqttclient* C=mqttTestConnect(&P,NULL);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
        xfuture* F=NULL,*G=NULL;
        if(Kind<2){F=mqttTestPublishOne(C,1,2000);G=mqttTestPublishOne(C,257,2000);mqttTestWait(&P.Publishes,1);}
        if(Kind==0){
            mqttTestPuback(&P,1);testRequire(((const xmqttwriteresult*)mqttTestResult(F))->Identifier==1,"remove colliding hash-chain predecessor");mqttTestWait(&P.Publishes,2);mqttTestPuback(&P,257);
            testRequire(((const xmqttwriteresult*)mqttTestResult(G))->Identifier==257,"colliding packet id remains mapped");
            mqttTestPuback(&P,257); /* completed/duplicate ACK cannot mint credit */
        }else mqttTestPuback(&P,Kind==1?257:1234); /* mapped unsent / unknown */
        testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"mismatched/duplicate/unsent PUBACK closes");xmqttclientinfo I;xmqttClientInfo(C,&I);testRequire(I.CloseStatus==XMQTT_STATUS_PROTOCOL,"PUBACK session mismatch classification");
        if(Kind==1){const xmqttwriteresult* A=mqttTestResult(F);const xmqttwriteresult* B=mqttTestResult(G);testRequire(A->Status==XMQTT_STATUS_PROTOCOL && B->Status==XMQTT_STATUS_PROTOCOL && B->Delivery==XMQTT_NOT_SENT,"unsent ACK must not complete another request");}
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(Ready);xrtFutureDestroy(F);xrtFutureDestroy(G);
    }
    mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_311,MQTT_TEST_HOLD_PUBACK);xmqttclient* C=mqttTestConnect(&P,NULL);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
    xfuture* F=mqttTestPublishOne(C,65535,2000),*G=mqttTestPublishOne(C,0,2000);mqttTestWait(&P.Publishes,2);uint16 Auto=(uint16)xrtAtomic32Load(&P.LastIdentifier,XMEMORY_ACQUIRE);
    testRequire(Auto && Auto!=65535,"automatic id distinct from explicit maximum");mqttTestPuback(&P,Auto);testRequire(((const xmqttwriteresult*)mqttTestResult(G))->Identifier==Auto,"out-of-order second PUBACK");
    mqttTestPuback(&P,65535);testRequire(((const xmqttwriteresult*)mqttTestResult(F))->Identifier==65535,"out-of-order first PUBACK");
    mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(Ready);xrtFutureDestroy(F);xrtFutureDestroy(G);
}
static void testMqttQosOneAllocationRollback(void)
{
    const size_t Limits[]={1,8250,8500,8750,9000,9500,12000,20000};bool ColdFailure=false,Accepted=false;
    for(size_t J=0;J<sizeof(Limits)/sizeof(*Limits);++J){mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,0);xmqttclientconfig K;xmqttClientConfigInit(&K);K.MaxRequests=1;
        xmqttclient* C=mqttTestConnect(&P,&K);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);xmqttclientinfo Before,After;xmqttClientInfo(C,&Before);
        xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[J],&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"QoS1 allocation quota");
        xmqttpacket Pub;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);Pub.Flags=2;Pub.Identifier=77;Pub.Body.Publish.Topic=XRT_STR_LITERAL("t");xfuture* F=(xfuture*)(uintptr_t)1;
        xmqttclientadmission A=xmqttClientPublishAsync(C,&Pub,1000,&F);testRequire(xrtAllocationBudgetLeave(&Scope),"QoS1 quota leave");xrtClearError();xmqttClientInfo(C,&After);
        if(A==XMQTT_ADMISSION_ACCEPTED){Accepted=true;testRequire(((const xmqttwriteresult*)mqttTestResult(F))->Delivery==XMQTT_PEER_ACCEPTED,"quota-success QoS1 acknowledgement");}
        else {testRequire(A==XMQTT_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1 && !After.Requests && !After.OutgoingBytes,"QoS1 OOM output/reservation rollback");
            if(After.PrivateBytes>Before.PrivateBytes)ColdFailure=true;
            F=mqttTestPublishOne(C,77,1000);testRequire(((const xmqttwriteresult*)mqttTestResult(F))->Identifier==77,"failed construction released its identifier");
        }
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(Ready);xrtFutureDestroy(F);xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);
        testRequire(xrtAllocationBudgetInfo(Budget,&I) && !I.LiveBytes && !I.LiveCount,"cold flow/future/request/ACK quota lifetime leak");xrtAllocationBudgetFree(Budget);
    }
    testRequire(ColdFailure && Accepted,"QoS1 matrix covers failure after cold table allocation and success");
}
static void mqttTestIncomingQos(mqttTestPeer* P,uint16 Identifier,bool Duplicate,unsigned Qos)
{
    xmqttpacket Pub;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);Pub.Flags=(uint8)((Qos<<1)|(Duplicate?8u:0u));Pub.Identifier=Identifier;
    Pub.Body.Publish.Topic=XRT_STR_LITERAL("incoming");Pub.Body.Publish.Payload=XRT_BYTES_LITERAL("received");
    xmqttproperty Props[2]={{XMQTT_PROPERTY_USER,XMQTT_PROPERTY_STRING_PAIR,{0}},{XMQTT_PROPERTY_CORRELATION_DATA,XMQTT_PROPERTY_BINARY,{0}}};
    Props[0].Value.Pair=(xmqttstringpair){XRT_STR_LITERAL("key"),XRT_STR_LITERAL("value")};Props[1].Value.Binary=XRT_BYTES_LITERAL("\0x");
    if(P->Version==XMQTT_VERSION_5)Pub.Properties=(xmqttpropertylist){Props,2};
    unsigned char Wire[128];size_t N=0;testRequire(P->Version==XMQTT_VERSION_5?xmqttPacketWrite5(Wire,sizeof(Wire),&Pub,XMQTT_SERVER_TO_CLIENT,NULL,&N):xmqttPacketWrite311(Wire,sizeof(Wire),&Pub,XMQTT_SERVER_TO_CLIENT,NULL,&N),"incoming QoS1 encode");mqttTestSend(P,Wire,N);
}
static void mqttTestIncoming(mqttTestPeer* P,uint16 Id,bool Duplicate)
{mqttTestIncomingQos(P,Id,Duplicate,1);}
static void testMqttReceiveOneOwnership(void)
{
    for(unsigned V=4;V<=5;++V){mqttTestPeer P;mqttTestStart(&P,(xmqttversion)V,0);xmqttclientconfig K;xmqttClientConfigInit(&K);
        K.Stream.WriteLimit=1;K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;K.WriteBytes=K.WriteChunkBytes=1;
        xmqttclient* C=mqttTestConnect(&P,&K);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);xmqttclientinfo Before,After;xmqttClientInfo(C,&Before);
        mqttTestIncoming(&P,65535,false);mqttTestWait(&P.Acks,1);testRequire(xrtAtomic32Load(&P.AckIdentifier,XMEMORY_ACQUIRE)==65535,"receive PUBACK id with one-byte writes");
        mqttTestIncoming(&P,65535,true);mqttTestWait(&P.Acks,2);xmqttClientInfo(C,&After);
        testRequire(After.Received==2 && After.Messages==2 && After.PrivateBytes>Before.PrivateBytes,"post-ACK DUP is a new message; lazy ACK storage charged");
        xmqttmessage *First=NULL,*Second=NULL;testRequire(xmqttClientTakeMessage(C,&First) && xmqttClientTakeMessage(C,&Second),"take both QoS1 messages");
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(Ready);
        const xmqttpacket* A=xmqttMessagePacket(First);const xmqttpacket* B=xmqttMessagePacket(Second);
        testRequire(A->Identifier==65535 && B->Identifier==65535 && !(A->Flags&8u) && (B->Flags&8u) && A->Body.Publish.Payload.Size==8 && !memcmp(A->Body.Publish.Payload.Data,"received",8),"incoming packet views after Client/Engine destruction");
        if(V==5)testRequire(A->Properties.Count==2 && !memcmp(A->Properties.Data[0].Value.Pair.Value.Data,"value",5) && A->Properties.Data[1].Value.Binary.Size==2 && !memcmp(A->Properties.Data[1].Value.Binary.Data,"\0x",2),"retained incoming User/binary properties");
        xmqttMessageRelease(First);xmqttMessageRelease(Second);
    }
}
static void testMqttReceiveOneLimits(void)
{
    for(unsigned Kind=0;Kind<3;++Kind){mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,0);xmqttclientconfig K;xmqttClientConfigInit(&K);
        if(Kind==0)K.MaxObjects=2;
        if(Kind==1)K.MaxMessages=1;
        if(Kind==2){K.MaxMessages=65535;K.MaxPrivateBytes=16384;}
        xmqttclient* C=mqttTestConnect(&P,&K);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
        mqttTestIncoming(&P,11,false);if(Kind==1){mqttTestWait(&P.Acks,1);mqttTestIncoming(&P,22,false);}
        testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"incoming QoS1 resource exhaustion retires connection");xmqttclientinfo I;xmqttClientInfo(C,&I);
        testRequire(I.CloseStatus==XMQTT_STATUS_LIMIT && I.Received==(Kind==1?1u:0u) && xrtAtomic32Load(&P.Acks,XMEMORY_ACQUIRE)==(Kind==1?1u:0u),"no success PUBACK on object/queue/private quota failure");
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(Ready);
    }
}
typedef struct mqttReceiveBudget {xallocationbudget* Budget;xallocationscope Scope;bool Active,Entered;} mqttReceiveBudget;
static void mqttReceiveBudgetNotify(xmqttclient* C,ptr User)
{
    mqttReceiveBudget* B=User;xmqttclientinfo I;xmqttClientInfo(C,&I);
    if(!B->Entered && I.Received==1 && I.State==XMQTT_CLIENT_OPEN){xmqttmessage* Marker=NULL;testRequire(xmqttClientTakeMessage(C,&Marker),"OOM marker ownership");xmqttMessageRelease(Marker);
        testRequire(xrtAllocationBudgetEnter(B->Budget,&B->Scope),"worker receive allocation scope");B->Active=B->Entered=true;}
    if(B->Active && (I.Received==2 || I.State==XMQTT_CLIENT_CLOSED)){testRequire(xrtAllocationBudgetLeave(&B->Scope),"worker receive scope leave");B->Active=false;}
}
static void testMqttReceiveOneAllocationFailure(void)
{
    const size_t Limits[]={1,128,256,512,1024,65536};bool BeforeFlow=false,AfterFlow=false,Success=false;
    for(size_t J=0;J<sizeof(Limits)/sizeof(*Limits);++J){mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,0);mqttReceiveBudget B={0};testRequire(xrtAllocationBudgetCreate(Limits[J],&B.Budget),"receive allocation budget");
        xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=mqttReceiveBudgetNotify;K.User=&B;xmqttclient* C=mqttTestConnect(&P,&K);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
        xmqttclientinfo Before,I;double End=xrtTimer()+5;do{xmqttClientInfo(C,&Before);testRequire(xrtTimer()<End,"startup retirement before OOM");if(Before.StartupBytes)xrtSleep(1);}while(Before.StartupBytes);
        /* The first QoS0 marker enables the scope on the IO worker. Both frames
         * fit existing staging, so the next allocations are ACK ring/message. */
        const unsigned char Wire[]={0x30,5,0,1,'t',0,'m',0x32,7,0,1,'t',0,1,0,'x'};mqttTestSend(&P,Wire,sizeof(Wire));
        End=xrtTimer()+5;do{xmqttClientInfo(C,&I);testRequire(xrtTimer()<End,"receive OOM/ACK progress");if(I.State!=XMQTT_CLIENT_CLOSED && !xrtAtomic32Load(&P.Acks,XMEMORY_ACQUIRE))xrtSleep(1);}while(I.State!=XMQTT_CLIENT_CLOSED && !xrtAtomic32Load(&P.Acks,XMEMORY_ACQUIRE));
        xmqttClientInfo(C,&I); /* ACK observation can follow the loop's older snapshot. */
        if(I.State==XMQTT_CLIENT_CLOSED){testRequire(I.CloseStatus==XMQTT_STATUS_MEMORY && I.Received==1 && !xrtAtomic32Load(&P.Acks,XMEMORY_ACQUIRE),"no ACK or accepted message on receive allocation failure");
            if(I.PrivateBytes>Before.PrivateBytes)AfterFlow=true;else BeforeFlow=true;}
        else {Success=true;testRequire(I.Received==2,"receive allocation-success control");}
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(Ready);testRequire(B.Entered && !B.Active,"worker scope retired");xallocationbudgetinfo Q={0};Q.Size=sizeof(Q);
        testRequire(xrtAllocationBudgetInfo(B.Budget,&Q) && !Q.LiveCount && !Q.LiveBytes,"receive ring/message OOM budget lifetime");xrtAllocationBudgetFree(B.Budget);
    }
    testRequire(BeforeFlow && AfterFlow && Success,"cold receive ring and message allocation faults plus success covered");
}
static void testMqttErrorDisconnect(void)
{
    for(unsigned V=4;V<=5;++V)for(unsigned Byte=0;Byte<2;++Byte){mqttTestPeer P;mqttTestStart(&P,(xmqttversion)V,0);xmqttclientconfig K;xmqttClientConfigInit(&K);K.CloseTimeout=1000;
        if(Byte){K.Stream.WriteLimit=1;K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;K.WriteBytes=K.WriteChunkBytes=1;}
        xmqttclient* C=mqttTestConnect(&P,&K);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
        const unsigned char Bad[]={0,0};mqttTestSend(&P,Bad,sizeof(Bad));testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"diagnostic physical retirement");xmqttclientinfo I;xmqttClientInfo(C,&I);
        testRequire(I.CloseStatus==XMQTT_STATUS_PROTOCOL && I.ProtocolErrors==1 && !I.DrainTimedOut,"original protocol error survives diagnostic closure");
        testRequire(I.DisconnectBytes==(V==5?3u:0u) && I.LocalReason==(V==5?0x81:0) && I.DrainStarted==(V==5),"MQTT5 diagnostic vs 311 abortive output");
        if(V==5){mqttTestWait(&P.Disconnects,1);testRequire(xrtAtomic32Load(&P.Disconnects,XMEMORY_ACQUIRE)==1 && xrtAtomic32Load(&P.DisconnectReason,XMEMORY_ACQUIRE)==0x81,"byte-exact final error reason");}
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(Ready);
    }
}
static void testMqttErrorDisconnectAllocationFailure(void)
{
    const size_t Limits[]={1,128,256,512,1024,8192};bool TimerFailure=false,SendFailure=false,Success=false;
    for(size_t J=0;J<sizeof(Limits)/sizeof(*Limits);++J){mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,MQTT_TEST_UNCACHED_NODES);mqttReceiveBudget B={0};testRequire(xrtAllocationBudgetCreate(Limits[J],&B.Budget),"diagnostic allocation budget");
        xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=mqttReceiveBudgetNotify;K.User=&B;K.CloseTimeout=1000;xmqttclient* C=mqttTestConnect(&P,&K);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
        const unsigned char Wire[]={0x30,5,0,1,'t',0,'m',0,0};mqttTestSend(&P,Wire,sizeof(Wire));testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"diagnostic OOM remains physically bounded");xmqttclientinfo I;xmqttClientInfo(C,&I);
        testRequire(I.CloseStatus==XMQTT_STATUS_PROTOCOL && I.LocalReason==0x81 && !I.DrainTimedOut && I.Received==1,"diagnostic OOM cannot replace protocol classification");
        printf("[diagnostic-oom] budget=%zu started=%u bytes=%zu\n",Limits[J],I.DrainStarted?1u:0u,I.DisconnectBytes);
        if(!I.DrainStarted)TimerFailure=true;else if(!I.DisconnectBytes)SendFailure=true;else {Success=true;testRequire(I.DisconnectBytes==3,"diagnostic success-control bytes");}
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(Ready);testRequire(B.Entered && !B.Active,"diagnostic worker allocation scope retired");xallocationbudgetinfo Q={0};Q.Size=sizeof(Q);
        testRequire(xrtAllocationBudgetInfo(B.Budget,&Q) && !Q.LiveCount && !Q.LiveBytes,"diagnostic/timer/native OOM allocation lifetime");xrtAllocationBudgetFree(B.Budget);
    }
    testRequire(TimerFailure && SendFailure && Success,"diagnostic timer/send allocation failure and success controls");
}
/* Waiters borrow one stable unique Client owner. A held CLOSED notification
 * proves logical state is insufficient; no test reaches into private counters. */
typedef struct mqttWaitGate {xmutex Lock;xcond Ready;bool Entered,Release;unsigned Started,Finished;xmqttclient* Client;} mqttWaitGate;
typedef struct mqttWaitBorrower {mqttWaitGate* Gate;xwaitresult Result;} mqttWaitBorrower;
static void mqttWaitNotify(xmqttclient* C,ptr Data)
{
    mqttWaitGate* G=Data;xmqttclientinfo I;xmqttClientInfo(C,&I);if(I.State!=XMQTT_CLIENT_CLOSED)return;
    testRequire(xmqttClientWaitFor(C,0)==XWAIT_ERROR,"CLOSED callback still rejects worker wait");xrtClearError();
    testRequire(xmqttClientPollRetired(C)==XWAIT_TIMEOUT,"CLOSED callback is still physically owned");
    testRequire(xrtMutexLock(&G->Lock),"wait callback lock");G->Entered=true;xrtCondBroadcast(&G->Ready);
    while(!G->Release)testRequire(xrtCondWaitFor(&G->Ready,&G->Lock,5000)==XWAIT_OK,"held logical CLOSED callback");
    testRequire(xrtMutexUnlock(&G->Lock),"wait callback unlock");
}
static int32 mqttWaitThread(ptr Data)
{
    mqttWaitBorrower* W=Data;mqttWaitGate* G=W->Gate;xrtMutexLock(&G->Lock);++G->Started;xrtCondBroadcast(&G->Ready);xrtMutexUnlock(&G->Lock);
    W->Result=xmqttClientWaitFor(G->Client,5000);
    xrtMutexLock(&G->Lock);testRequire(G->Release,"wait cannot return while owned notification is held");++G->Finished;xrtCondBroadcast(&G->Ready);xrtMutexUnlock(&G->Lock);return 0;
}
static void mqttWaitCase(xmqttversion Version,bool Held)
{
    mqttTestPeer P;mqttTestStart(&P,Version,0);mqttWaitGate G={0};G.Release=!Held;
    testRequire(xrtMutexInit(&G.Lock) && xrtCondInit(&G.Ready),"MQTT retirement test primitives");xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=mqttWaitNotify;K.User=&G;
    G.Client=mqttTestConnect(&P,&K);xfuture* Ready=xmqttClientReady(G.Client);(void)mqttTestResult(Ready);
    xrtSetErrorInfo(XERR_STATE,"mqtt.wait.ambient",7,"preserve");const xerror* Ambient=xrtGetError();
    testRequire(xmqttClientWaitFor(G.Client,0)==XWAIT_TIMEOUT && xrtGetError()==Ambient,"open poll preserves owner and ambient error");xrtClearError();
    testRequire(xmqttClientPollRetired(G.Client)==XWAIT_TIMEOUT,"new query does not close an open Client");
    testRequire(xmqttClientWaitFor(G.Client,-2)==XWAIT_ERROR,"invalid MQTT wait timeout");xrtClearError();
    mqttWaitBorrower W[2]={{&G,XWAIT_ERROR},{&G,XWAIT_ERROR}};xthread* Threads[2];
    for(unsigned J=0;J<2;++J){Threads[J]=xrtThreadCreate(mqttWaitThread,W+J,0);testRequire(Threads[J]!=NULL,"MQTT retirement borrower thread");}
    xrtMutexLock(&G.Lock);while(G.Started!=2)testRequire(xrtCondWaitFor(&G.Ready,&G.Lock,5000)==XWAIT_OK,"MQTT borrowers start");xrtMutexUnlock(&G.Lock);
    testRequire(xmqttClientClose(G.Client),"close with existing waiters");
    if(Held){xrtMutexLock(&G.Lock);while(!G.Entered)testRequire(xrtCondWaitFor(&G.Ready,&G.Lock,5000)==XWAIT_OK,"logical CLOSED notification entered");xrtMutexUnlock(&G.Lock);
        xmqttclientinfo I;xmqttClientInfo(G.Client,&I);testRequire(I.State==XMQTT_CLIENT_CLOSED && xmqttClientWaitFor(G.Client,0)==XWAIT_TIMEOUT && xmqttClientWaitFor(G.Client,10)==XWAIT_TIMEOUT,"logical CLOSED does not consume a timed-out physical owner");
        testRequire(xmqttClientPollRetired(G.Client)==XWAIT_TIMEOUT,"held notification prevents immediate retirement");
        xrtMutexLock(&G.Lock);testRequire(!G.Finished,"no borrower returns before physical callback release");G.Release=true;xrtCondBroadcast(&G.Ready);xrtMutexUnlock(&G.Lock);
    }
    for(unsigned J=0;J<2;++J){testRequire(xrtThreadWaitFor(Threads[J],5000)==XWAIT_OK && W[J].Result==XWAIT_OK,"all MQTT borrowers wake on real retirement");xrtThreadDestroy(Threads[J]);}
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"ready MQTT wait budget");
    testRequire(xmqttClientWaitFor(G.Client,0)==XWAIT_OK && xmqttClientClose(G.Client) && xmqttClientWaitFor(G.Client,0)==XWAIT_OK,"ready wait and repeated Close allocate/post nothing");testRequire(xrtAllocationBudgetLeave(&Scope),"ready MQTT budget leave");
    xallocationbudgetinfo B={0};B.Size=sizeof(B);testRequire(xrtAllocationBudgetInfo(Budget,&B) && !B.LiveCount && !B.LiveBytes,"no MQTT waiter allocation debt");xrtAllocationBudgetFree(Budget);
    mqttTestClose(G.Client);mqttTestStop(&P);xrtFutureDestroy(Ready);testRequire(xrtCondUnit(&G.Ready) && xrtMutexUnit(&G.Lock),"MQTT waiter test cleanup");
}
static void testMqttRetirementSignals(void)
{
    testRequire(xmqttClientWaitFor(NULL,0)==XWAIT_ERROR,"null MQTT waiter");xrtClearError();
    for(unsigned V=4;V<=5;++V){mqttWaitCase((xmqttversion)V,true);for(unsigned J=0;J<16;++J)mqttWaitCase((xmqttversion)V,false);}
    puts("[PASS] MQTT retirement: held CLOSED callback, 32 wake races, multiple borrowers, timeout ownership and allocation-free ready waits");
}
static void testMqttGraceful(void)
{
    for(unsigned V=4;V<=5;++V)for(unsigned Tiny=0;Tiny<2;++Tiny){mqttTestPeer P;mqttTestStart(&P,(xmqttversion)V,MQTT_TEST_GRACEFUL);xmqttclientconfig K;xmqttClientConfigInit(&K);
        if(Tiny){K.WriteBytes=K.WriteChunkBytes=1;K.Stream.WriteLimit=1;K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;}
        xmqttclient* C=mqttTestConnect(&P,&K);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);xfuture* F=NULL;
        testRequire(xmqttClientDisconnectAsync(C,NULL,5000,&F)==XMQTT_ADMISSION_ACCEPTED,"normal graceful admission");
        const xmqttwriteresult* R=mqttTestResult(F);testRequire(R->Status==XMQTT_STATUS_OK && R->AcceptedBytes==2 && R->Delivery==XMQTT_MAY_HAVE_REACHED_PEER && !R->Identifier && !R->Puback && !R->Reason,"normal DISCONNECT typed local result");
        testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"graceful physical wait");mqttTestWait(&P.Disconnects,1);xmqttclientinfo I;xmqttClientInfo(C,&I);
        testRequire(I.CloseStatus==XMQTT_STATUS_OK && I.DrainStarted && !I.DrainTimedOut && I.DisconnectBytes==2 && !I.Requests && I.Accepted==I.Completed,"graceful accounting");
        mqttTestClose(C);mqttTestStop(&P);testRequire(R->Status==XMQTT_STATUS_OK,"graceful result survives Client and Engine");xrtFutureDestroy(F);xrtFutureDestroy(Ready);
    }
}
static void testMqttGracefulDrain(void)
{
    for(unsigned V=4;V<=5;++V)for(unsigned Kind=0;Kind<5;++Kind){mqttTestPeer P;mqttTestStart(&P,(xmqttversion)V,MQTT_TEST_GRACEFUL|MQTT_TEST_HOLD_PUBACK);xmqttclient* C=mqttTestConnect(&P,NULL);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
        xmqttpacket Packet;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet);Packet.Flags=2;Packet.Body.Publish.Topic=XRT_STR_LITERAL("t");xfuture *Publish=NULL,*F=NULL;
        testRequire(xmqttClientPublishAsync(C,&Packet,5000,&Publish)==XMQTT_ADMISSION_ACCEPTED,"pre-close QoS1 accepted");mqttTestWait(&P.Publishes,1);
        testRequire(xmqttClientDisconnectAsync(C,NULL,Kind==1?10:5000,&F)==XMQTT_ADMISSION_ACCEPTED && xrtFutureWaitFor(F,0)==XWAIT_TIMEOUT,"disconnect waits for prior PUBACK");
        xfuture* Sentinel=(xfuture*)(uintptr_t)1;testRequire(xmqttClientPublishAsync(C,&Packet,5000,&Sentinel)==XMQTT_ADMISSION_CLOSED && Sentinel==(xfuture*)(uintptr_t)1 && !xrtAtomic32Load(&P.Disconnects,XMEMORY_ACQUIRE),"new admission stops before final frame");
        if(Kind==0)mqttTestPuback(&P,(uint16)xrtAtomic32Load(&P.LastIdentifier,XMEMORY_ACQUIRE));
        if(Kind==2)testRequire(xrtFutureCancel(F),"cancel graceful request");
        if(Kind==3)testRequire(xmqttClientClose(C),"abort graceful request");
        if(Kind==4){const unsigned char Bad[]={0,0};mqttTestSend(&P,Bad,sizeof(Bad));}
        xmqttclientstatus Expected=Kind==0?XMQTT_STATUS_OK:Kind==1?XMQTT_STATUS_TIMEOUT:Kind==2?XMQTT_STATUS_CANCELLED:Kind==3?XMQTT_STATUS_CLOSED:XMQTT_STATUS_PROTOCOL;
        testRequire(((const xmqttwriteresult*)mqttTestResult(F))->Status==Expected && ((const xmqttwriteresult*)mqttTestResult(Publish))->Status==Expected,"graceful drain or first interrupted outcome");
        testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"graceful interrupted physical wait");if(Kind)testRequire(!xrtAtomic32Load(&P.Disconnects,XMEMORY_ACQUIRE),"no final frame before prior QoS1 settles");
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(F);xrtFutureDestroy(Publish);xrtFutureDestroy(Ready);
    }
}
static void testMqttGracefulAdmissionAndOom(void)
{
    for(unsigned Kind=0;Kind<4;++Kind){mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,MQTT_TEST_GRACEFUL|(Kind==0?MQTT_TEST_HOLD:0));xmqttclientconfig K;xmqttClientConfigInit(&K);
        if(Kind==1)K.MaxObjects=2;
        if(Kind==2)K.MaxOutgoingBytes=1;
        xmqttclient* C=mqttTestConnect(&P,&K);xfuture* Ready=xmqttClientReady(C);if(Kind)(void)mqttTestResult(Ready);
        xfuture* Out=(xfuture*)(uintptr_t)1;xmqttpacket D;xmqttPacketInit(XMQTT_PACKET_DISCONNECT,&D);xmqttproperty Expiry={XMQTT_PROPERTY_SESSION_EXPIRY,XMQTT_PROPERTY_UINT32,{0}};Expiry.Value.Number=1;
        if(Kind==3)D.Properties=(xmqttpropertylist){&Expiry,1};
        xmqttclientadmission A=xmqttClientDisconnectAsync(C,&D,1000,&Out);testRequire(A==(Kind==0 || Kind==3?XMQTT_ADMISSION_ERROR:XMQTT_ADMISSION_OVERLOADED) && Out==(xfuture*)(uintptr_t)1,"disconnect validation/quota rejection preserves output");
        xrtClearError();xmqttclientinfo I;xmqttClientInfo(C,&I);testRequire(I.State<XMQTT_CLIENT_CLOSING && !I.Requests && !I.OutgoingBytes,"rejected disconnect leaves connection owned/open");mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(Ready);
    }
    const size_t Limits[]={1,128,256,512,1024,4096,16384};bool Failed=false,Success=false;
    for(size_t J=0;J<sizeof(Limits)/sizeof(*Limits);++J){mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,MQTT_TEST_GRACEFUL);xmqttclient* C=mqttTestConnect(&P,NULL);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
        xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[J],&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"graceful admission allocation scope");xfuture* F=(xfuture*)(uintptr_t)1;
        xmqttclientadmission A=xmqttClientDisconnectAsync(C,NULL,5000,&F);testRequire(xrtAllocationBudgetLeave(&Scope),"graceful scope leave");
        if(A==XMQTT_ADMISSION_ACCEPTED){Success=true;testRequire(((const xmqttwriteresult*)mqttTestResult(F))->Status==XMQTT_STATUS_OK,"graceful allocation-success control");}
        else {Failed=true;testRequire(A==XMQTT_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"graceful allocation rollback output");F=NULL;xmqttclientinfo I;xmqttClientInfo(C,&I);testRequire(I.State==XMQTT_CLIENT_OPEN && !I.Requests && !I.OutgoingBytes,"graceful OOM reservation/producer rollback");xrtClearError();}
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(F);xrtFutureDestroy(Ready);xallocationbudgetinfo B={0};B.Size=sizeof(B);testRequire(xrtAllocationBudgetInfo(Budget,&B) && !B.LiveBytes && !B.LiveCount,"graceful OOM lifetime budget retired");xrtAllocationBudgetFree(Budget);
    }
    testRequire(Failed && Success,"graceful cold admission faults and success control");
}
typedef struct mqttCloseRace {xmqttclient* Client;} mqttCloseRace;
static int32 mqttCloseRaceThread(ptr Data)
{
    mqttCloseRace* R=Data;double End=xrtTimer()+5;
    for(;;){xmqttclientinfo I;testRequire(xmqttClientInfo(R->Client,&I),"race snapshot");
        if(I.State==XMQTT_CLIENT_CLOSED || I.DisconnectBytes){testRequire(xmqttClientClose(R->Client),"racing explicit Close");return 0;}
        testRequire(xrtTimer()<End,"race progress");xrtThreadYield();
    }
}
static void testMqttGracefulCloseRace(void)
{
    for(unsigned V=4;V<=5;++V)for(unsigned J=0;J<16;++J){mqttTestPeer P;mqttTestStart(&P,(xmqttversion)V,MQTT_TEST_GRACEFUL);xmqttclient* C=mqttTestConnect(&P,NULL);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
        mqttCloseRace Race={C};xthread* Thread=xrtThreadCreate(mqttCloseRaceThread,&Race,0);testRequire(Thread!=NULL,"graceful Close race thread");xfuture* F=NULL;testRequire(xmqttClientDisconnectAsync(C,NULL,5000,&F)==XMQTT_ADMISSION_ACCEPTED,"racing graceful admission");
        const xmqttwriteresult* R=mqttTestResult(F);testRequire(xrtThreadWaitFor(Thread,5000)==XWAIT_OK,"racing Close completed");xrtThreadDestroy(Thread);testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"race physical retirement");xmqttclientinfo I;xmqttClientInfo(C,&I);
        testRequire((R->Status==XMQTT_STATUS_OK || R->Status==XMQTT_STATUS_CLOSED) && R->Status==I.CloseStatus,"Future and Info share final Close linearization");
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(F);xrtFutureDestroy(Ready);
    }
}
static xfuture* mqttTestPublishTwo(xmqttclient* C,uint16 Id,int64 Timeout)
{
    xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=4;P.Identifier=Id;P.Body.Publish.Topic=XRT_STR_LITERAL("two");P.Body.Publish.Payload=XRT_BYTES_LITERAL("exactly once handshake");xfuture* F=NULL;
    testRequire(xmqttClientPublishAsync(C,&P,Timeout,&F)==XMQTT_ADMISSION_ACCEPTED,"QoS2 admission");return F;
}
static void testMqttQosTwoSender(void)
{
    for(unsigned V=4;V<=5;++V)for(unsigned Tiny=0;Tiny<2;++Tiny){mqttTestPeer P;mqttTestStart(&P,(xmqttversion)V,0);xmqttclientconfig K;xmqttClientConfigInit(&K);if(Tiny){K.WriteBytes=K.WriteChunkBytes=1;K.Stream.WriteLimit=1;K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;}
        xmqttclient* C=mqttTestConnect(&P,&K);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);xfuture* F=mqttTestPublishTwo(C,19,5000);const xmqttwriteresult* R=mqttTestResult(F);
        testRequire(R->Status==XMQTT_STATUS_OK && R->Delivery==XMQTT_PEER_ACCEPTED && R->Identifier==19 && R->AcceptedBytes && !R->Puback && R->PubrecReceived && R->PubcompReceived && !R->PubrecReason && !R->Reason && R->Pubrec && R->Pubcomp,"complete two-phase result");
        testRequire(xmqttMessageRetain(R->Pubrec) && xmqttMessageRetain(R->Pubcomp),"independent QoS2 ACK ownership");xmqttMessageRelease(R->Pubrec);xmqttMessageRelease(R->Pubcomp);mqttTestClose(C);mqttTestStop(&P);
        testRequire(xmqttMessagePacket(R->Pubrec)->Type==XMQTT_PACKET_PUBREC && xmqttMessagePacket(R->Pubcomp)->Type==XMQTT_PACKET_PUBCOMP,"QoS2 result after Client and Engine destruction");xrtFutureDestroy(F);xrtFutureDestroy(Ready);
    }
    for(unsigned Kind=0;Kind<4;++Kind){mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,Kind==0?MQTT_TEST_REJECT_REC:Kind==1?MQTT_TEST_REJECT_COMP:0);xmqttclientconfig K;xmqttClientConfigInit(&K);if(Kind>=2)K.MaxObjects=Kind==2?3:4;
        xmqttclient* C=mqttTestConnect(&P,&K);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);xfuture* F=mqttTestPublishTwo(C,0,5000);const xmqttwriteresult* R=mqttTestResult(F);
        testRequire(R->PubrecReceived && R->Status==(Kind<2?XMQTT_STATUS_REJECTED:XMQTT_STATUS_LIMIT),"negative/metadata phase outcomes");
        if(Kind==0)testRequire(R->PubrecReason==0x87 && R->Reason==0x87 && !R->PubcompReceived && !xrtAtomic32Load(&P.Acks,XMEMORY_ACQUIRE),"negative PUBREC sends no PUBREL");
        if(Kind==1)testRequire(R->PubcompReceived && R->Reason==0x92 && R->Delivery==XMQTT_PEER_REJECTED,"negative PUBCOMP does not imply delivery");
        if(Kind==2)testRequire(!R->Pubrec && !R->PubcompReceived && R->Delivery==XMQTT_MAY_HAVE_REACHED_PEER,"PUBREC scalar retained through metadata exhaustion");
        if(Kind==3)testRequire(R->Pubrec && !R->Pubcomp && R->PubcompReceived && R->Delivery==XMQTT_PEER_ACCEPTED,"PUBCOMP scalar retained through metadata exhaustion");
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(F);xrtFutureDestroy(Ready);
    }
}
static void testMqttQosTwoWindowAndDuplicates(void)
{
    for(unsigned V=4;V<=5;++V){mqttTestPeer P;mqttTestStart(&P,(xmqttversion)V,MQTT_TEST_WINDOW_ONE|MQTT_TEST_HOLD_COMP);xmqttclient* C=mqttTestConnect(&P,NULL);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);xfuture* F=mqttTestPublishTwo(C,71,5000);mqttTestWait(&P.Acks,1);
        xfuture* Second=V==5?mqttTestPublishTwo(C,72,5000):NULL;if(V==5){xfuture* Zero=mqttTestPublish(C,5000);(void)mqttTestResult(Zero);mqttTestWait(&P.QosZeroes,1);mqttTestWait(&P.Publishes,2);testRequire(xrtAtomic32Load(&P.Publishes,XMEMORY_ACQUIRE)==2 && xrtFutureWaitFor(Second,0)==XWAIT_TIMEOUT,"QoS2 credit remains held through PUBCOMP; QoS0 proceeds");xrtFutureDestroy(Zero);}
        mqttTestQosTwoAck(&P,XMQTT_PACKET_PUBREC,71);mqttTestWait(&P.Acks,2);mqttTestQosTwoAck(&P,XMQTT_PACKET_PUBCOMP,71);testRequire(((const xmqttwriteresult*)mqttTestResult(F))->Status==XMQTT_STATUS_OK,"duplicate PUBREC retransmits release");
        mqttTestQosTwoAck(&P,XMQTT_PACKET_PUBCOMP,71); /* Extra terminal ACK cannot mint quota. */
        if(!Second)Second=mqttTestPublishTwo(C,72,5000);
        mqttTestWait(&P.Publishes,V==5?3:2);mqttTestWait(&P.Acks,3);mqttTestQosTwoAck(&P,XMQTT_PACKET_PUBCOMP,72);testRequire(((const xmqttwriteresult*)mqttTestResult(Second))->Status==XMQTT_STATUS_OK,"following QoS2 flow");
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(F);xrtFutureDestroy(Second);xrtFutureDestroy(Ready);
    }
}
static void testMqttQosTwoTerminalAndDrain(void)
{
    for(unsigned V=4;V<=5;++V)for(unsigned Kind=0;Kind<3;++Kind){mqttTestPeer P;mqttTestStart(&P,(xmqttversion)V,MQTT_TEST_HOLD_COMP|MQTT_TEST_GRACEFUL);xmqttclient* C=mqttTestConnect(&P,NULL);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);xfuture* F=mqttTestPublishTwo(C,31,Kind==0?100:5000);mqttTestWait(&P.Acks,1);xfuture* Closed=NULL;
        if(Kind==1)testRequire(xrtFutureCancel(F),"cancel after PUBREC/PUBREL");
        if(Kind==2){testRequire(xmqttClientDisconnectAsync(C,NULL,5000,&Closed)==XMQTT_ADMISSION_ACCEPTED && xrtFutureWaitFor(Closed,0)==XWAIT_TIMEOUT && !xrtAtomic32Load(&P.Disconnects,XMEMORY_ACQUIRE),"graceful shutdown waits for PUBCOMP");mqttTestQosTwoAck(&P,XMQTT_PACKET_PUBCOMP,31);}
        const xmqttwriteresult* R=mqttTestResult(F);testRequire(R->Status==(Kind==0?XMQTT_STATUS_TIMEOUT:Kind==1?XMQTT_STATUS_CANCELLED:XMQTT_STATUS_OK) && R->PubrecReceived && R->Pubrec && R->PubcompReceived==(Kind==2),"two-phase terminal outcome retains receipt");
        if(Kind==2)testRequire(((const xmqttwriteresult*)mqttTestResult(Closed))->Status==XMQTT_STATUS_OK,"graceful after complete QoS2 handshake");
        else testRequire(R->Delivery==XMQTT_MAY_HAVE_REACHED_PEER && xmqttClientWaitFor(C,5000)==XWAIT_OK,"incomplete handshake retires without claiming completion");
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(Closed);xrtFutureDestroy(F);xrtFutureDestroy(Ready);
    }
}
static void testMqttReceiveTwoOwnership(void)
{
    for(unsigned V=4;V<=5;++V)for(unsigned Tiny=0;Tiny<2;++Tiny){mqttTestPeer P;mqttTestStart(&P,(xmqttversion)V,0);xmqttclientconfig K;xmqttClientConfigInit(&K);if(Tiny){K.WriteBytes=K.WriteChunkBytes=1;K.Stream.WriteLimit=1;K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;}
        xmqttclient* C=mqttTestConnect(&P,&K);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);xmqttclientinfo Before,I;xmqttClientInfo(C,&Before);
        mqttTestIncomingQos(&P,65535,false,2);mqttTestWait(&P.ReceiveRecs,1);xmqttmessage *First=NULL,*Second=NULL;testRequire(xmqttClientTakeMessage(C,&First),"QoS2 enqueued before release");
        xfuture* Sent=mqttTestPublishTwo(C,65535,5000);testRequire(((const xmqttwriteresult*)mqttTestResult(Sent))->Status==XMQTT_STATUS_OK,"independent incoming and outgoing QoS2 ids");
        mqttTestIncomingQos(&P,65535,false,2);mqttTestWait(&P.ReceiveRecs,2);mqttTestIncomingQos(&P,65535,true,2);mqttTestWait(&P.ReceiveRecs,3);xmqttClientInfo(C,&I);
        testRequire(I.Received==1 && !I.Messages && I.PrivateBytes>Before.PrivateBytes,"DUP flag does not control active-id duplicate suppression");
        mqttTestQosTwoAck(&P,XMQTT_PACKET_PUBREL,65535);mqttTestWait(&P.ReceiveComps,1);mqttTestQosTwoAck(&P,XMQTT_PACKET_PUBREL,65535);mqttTestWait(&P.ReceiveComps,2);
        testRequire(xrtAtomic32Load(&P.CompReason,XMEMORY_ACQUIRE)==(V==5?0x92u:0u),"unknown/repeated PUBREL is completed without delivery");
        mqttTestIncomingQos(&P,65535,true,2);mqttTestWait(&P.ReceiveRecs,4);testRequire(xmqttClientTakeMessage(C,&Second),"id reusable after PUBCOMP even with DUP set");mqttTestQosTwoAck(&P,XMQTT_PACKET_PUBREL,65535);mqttTestWait(&P.ReceiveComps,3);
        xmqttClientInfo(C,&I);testRequire(I.Received==2 && !I.Messages,"two exchanges deliver twice, retransmissions only once");mqttTestClose(C);mqttTestStop(&P);
        const xmqttpacket *A=xmqttMessagePacket(First),*B=xmqttMessagePacket(Second);testRequire(A->Identifier==65535 && !(A->Flags&8u) && (B->Flags&8u) && A->Body.Publish.Payload.Size==8 && !memcmp(A->Body.Publish.Payload.Data,"received",8),"QoS2 messages survive Client/Engine");
        if(V==5)testRequire(A->Properties.Count==2 && A->Properties.Data[0].Value.Pair.Value.Size==5 && !memcmp(A->Properties.Data[0].Value.Pair.Value.Data,"value",5),"QoS2 property ownership");
        xmqttMessageRelease(First);xmqttMessageRelease(Second);xrtFutureDestroy(Sent);xrtFutureDestroy(Ready);
    }
}
static void testMqttReceiveTwoWindow(void)
{
    for(unsigned Qos=1;Qos<=2;++Qos){mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,0);xmqttclientconfig K;xmqttClientConfigInit(&K);xmqttpacket Connect=mqttTestConnectPacket();xmqttproperty Limit={XMQTT_PROPERTY_RECEIVE_MAXIMUM,XMQTT_PROPERTY_UINT16,{0}};Limit.Value.Number=1;Connect.Properties=(xmqttpropertylist){&Limit,1};xmqttclient* C=NULL;
        testRequire(xmqttClientConnect(P.Engine,&P.Address,1,&K,&Connect,&C),"receive-window factory");xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);mqttTestIncomingQos(&P,7,false,2);mqttTestWait(&P.ReceiveRecs,1);xmqttmessage* M=NULL;testRequire(xmqttClientTakeMessage(C,&M),"taking does not return protocol credit");xmqttMessageRelease(M);
        mqttTestIncomingQos(&P,8,false,Qos);testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"window remains occupied after PUBREC");xmqttclientinfo I;xmqttClientInfo(C,&I);testRequire(I.CloseStatus==XMQTT_STATUS_PROTOCOL && I.LocalReason==0x93 && I.Received==1 && !xrtAtomic32Load(&P.Acks,XMEMORY_ACQUIRE),"shared QoS1/2 receive window");mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(Ready);
    }
}
static void testMqttReceiveTwoGraceful(void)
{
    for(unsigned V=4;V<=5;++V)for(unsigned Kind=0;Kind<3;++Kind){mqttTestPeer P;mqttTestStart(&P,(xmqttversion)V,MQTT_TEST_GRACEFUL);xmqttclient* C=mqttTestConnect(&P,NULL);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);mqttTestIncomingQos(&P,42,false,2);mqttTestWait(&P.ReceiveRecs,1);xfuture* F=NULL;
        testRequire(xmqttClientDisconnectAsync(C,NULL,Kind==1?100:5000,&F)==XMQTT_ADMISSION_ACCEPTED && xrtFutureWaitFor(F,0)==XWAIT_TIMEOUT && !xrtAtomic32Load(&P.Disconnects,XMEMORY_ACQUIRE),"graceful waits for accepted incoming QoS2");
        if(Kind==0)mqttTestQosTwoAck(&P,XMQTT_PACKET_PUBREL,42);
        if(Kind==2)testRequire(xrtFutureCancel(F),"cancel inbound drain");
        const xmqttwriteresult* R=mqttTestResult(F);testRequire(R->Status==(Kind==0?XMQTT_STATUS_OK:Kind==1?XMQTT_STATUS_TIMEOUT:XMQTT_STATUS_CANCELLED),"incoming graceful terminal status");testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"incoming drain physical retirement");
        if(Kind==0)mqttTestWait(&P.ReceiveComps,1);else testRequire(!xrtAtomic32Load(&P.ReceiveComps,XMEMORY_ACQUIRE),"no fabricated completion on interruption");mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(F);xrtFutureDestroy(Ready);
    }
}
static void testMqttReceiveTwoAllocationFailure(void)
{
    const size_t Limits[]={1,256,65536,66088,66176,66240,66432,131072};bool BeforeFlow=false,AfterFlow=false,Success=false;
    for(size_t J=0;J<sizeof(Limits)/sizeof(*Limits);++J){mqttTestPeer P;mqttTestStart(&P,XMQTT_VERSION_5,0);mqttReceiveBudget B={0};testRequire(xrtAllocationBudgetCreate(Limits[J],&B.Budget),"QoS2 receive allocation budget");xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=mqttReceiveBudgetNotify;K.User=&B;xmqttclient* C=mqttTestConnect(&P,&K);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
        xmqttclientinfo Before,I;double End=xrtTimer()+5;do{xmqttClientInfo(C,&Before);testRequire(xrtTimer()<End,"QoS2 startup retirement");if(Before.StartupBytes)xrtSleep(1);}while(Before.StartupBytes);
        const unsigned char Wire[]={0x30,5,0,1,'t',0,'m',0x34,7,0,1,'t',0,1,0,'x'};mqttTestSend(&P,Wire,sizeof(Wire));End=xrtTimer()+5;
        do{xmqttClientInfo(C,&I);testRequire(xrtTimer()<End,"QoS2 receive OOM progress");if(I.State!=XMQTT_CLIENT_CLOSED && !xrtAtomic32Load(&P.ReceiveRecs,XMEMORY_ACQUIRE))xrtSleep(1);}while(I.State!=XMQTT_CLIENT_CLOSED && !xrtAtomic32Load(&P.ReceiveRecs,XMEMORY_ACQUIRE));xmqttClientInfo(C,&I);
        if(I.State==XMQTT_CLIENT_CLOSED){testRequire(I.CloseStatus==XMQTT_STATUS_MEMORY && I.Received==1 && !xrtAtomic32Load(&P.ReceiveRecs,XMEMORY_ACQUIRE),"no PUBREC without owned state/message on OOM");if(I.PrivateBytes>Before.PrivateBytes)AfterFlow=true;else BeforeFlow=true;}
        else {Success=true;testRequire(I.Received==2,"QoS2 OOM success control");mqttTestQosTwoAck(&P,XMQTT_PACKET_PUBREL,1);mqttTestWait(&P.ReceiveComps,1);}
        mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(Ready);testRequire(B.Entered && !B.Active,"QoS2 worker allocation scope retired");xallocationbudgetinfo Q={0};Q.Size=sizeof(Q);testRequire(xrtAllocationBudgetInfo(B.Budget,&Q) && !Q.LiveCount && !Q.LiveBytes,"QoS2 receive OOM state/message owners retired");xrtAllocationBudgetFree(B.Budget);
    }
    testRequire(BeforeFlow && AfterFlow && Success,"QoS2 state and message OOM plus successful control covered");
}
typedef struct mqttQueuedCloseGate {xatomic32 Entered,Release;} mqttQueuedCloseGate;
static void mqttQueuedCloseNotify(xmqttclient* C,ptr User)
{
    mqttQueuedCloseGate* G=User;xmqttclientinfo I;xmqttClientInfo(C,&I);
    if(!I.Ready || I.State!=XMQTT_CLIENT_OPEN || xrtAtomic32Load(&G->Entered,XMEMORY_ACQUIRE))return;
    xrtAtomic32Store(&G->Entered,1,XMEMORY_RELEASE);double End=xrtTimer()+5;
    /* Test-only worker barrier between collection and shutdown processing. */
    while(!xrtAtomic32Load(&G->Release,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"queued-close test barrier");xrtThreadYield();}
}
static void testMqttGracefulQueuedClose(void)
{
    for(unsigned V=4;V<=5;++V){mqttTestPeer P;mqttTestStart(&P,(xmqttversion)V,MQTT_TEST_GRACEFUL);mqttQueuedCloseGate G={0};xrtAtomic32Init(&G.Entered,0);xrtAtomic32Init(&G.Release,0);xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=mqttQueuedCloseNotify;K.User=&G;xmqttclient* C=mqttTestConnect(&P,&K);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);mqttTestWait(&G.Entered,1);
        xfuture *Published=mqttTestPublish(C,5000),*Closed=NULL;testRequire(xmqttClientDisconnectAsync(C,NULL,5000,&Closed)==XMQTT_ADMISSION_ACCEPTED && xmqttClientClose(C),"queued publish and disconnect accepted before abort");xrtAtomic32Store(&G.Release,1,XMEMORY_RELEASE);
        testRequire(xrtFutureWaitFor(Published,1000)==XWAIT_OK && xrtFutureState(Published)==XFUTURE_RESOLVED,"accepted submit cannot be stranded by graceful abort");testRequire(((const xmqttwriteresult*)mqttTestResult(Published))->Status==XMQTT_STATUS_CLOSED && ((const xmqttwriteresult*)mqttTestResult(Closed))->Status==XMQTT_STATUS_CLOSED,"queued close has typed outcomes");
        testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"queued close physical retirement");xmqttclientinfo I;xmqttClientInfo(C,&I);testRequire(!I.Requests && !I.OutgoingBytes && I.Accepted==I.Completed,"queued close balances all accepted requests");mqttTestClose(C);mqttTestStop(&P);xrtFutureDestroy(Closed);xrtFutureDestroy(Published);xrtFutureDestroy(Ready);
    }
}
#include "client_subscriptions.h"
#include "client_aliases.h"
#include "client_send_aliases.h"
#include "client_retirement_poll.h"
int main(void)
{testMqttRetirementPoll();testMqttSendAliases();testMqttSendAliasAllocation();testMqttAliasOwnership();testMqttAliasQosTwo();testMqttAliasAllocation();testMqttSubscriptions();testMqttSubscriptionFailures();testMqttSubscriptionAllocation();testMqttSubscriptionUnsent();testMqttGracefulQueuedClose();testMqttClientRoundtrip();testMqttClientTerminal();testMqttClientAdmissionCancel();testMqttClientKeepalive();testMqttClientReceiveBounds();testMqttClientObjectAdmission();testMqttClientReentrancy();testMqttClientAllocationFailure();testMqttQosOneCompletion();testMqttQosOneWindow();testMqttQosOneCancellationAndRestrictions();testMqttQosOneIdsAndMismatches();testMqttQosOneAllocationRollback();testMqttReceiveOneOwnership();testMqttReceiveOneLimits();testMqttReceiveOneAllocationFailure();testMqttErrorDisconnect();testMqttErrorDisconnectAllocationFailure();testMqttRetirementSignals();testMqttGraceful();testMqttGracefulDrain();testMqttGracefulAdmissionAndOom();testMqttGracefulCloseRace();testMqttQosTwoSender();testMqttQosTwoWindowAndDuplicates();testMqttQosTwoTerminalAndDrain();testMqttReceiveTwoOwnership();testMqttReceiveTwoWindow();testMqttReceiveTwoGraceful();testMqttReceiveTwoAllocationFailure();puts("[PASS] xmqtt TCP/QoS1 and diagnostic DISCONNECT: flow, boundaries, ownership, limits, original errors and OOM retirement");return 0;}
