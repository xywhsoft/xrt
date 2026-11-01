#include "test.h"
#include <xmqtt/client.h>
#include <xrt/thread.h>
#include <xrt/atomic.h>
#include <xrt/time.h>

/* In-process protocol fixture. Independent wire/broker interoperability lives
 * separately; this fixture exercises ownership, limits and worker reentrancy. */
enum {MQTT_TEST_HOLD=1,MQTT_TEST_REFUSE=2,MQTT_TEST_SESSION=4,MQTT_TEST_DUPLICATE=8,MQTT_TEST_NO_PONG=16,MQTT_TEST_KEEPALIVE=32,
    MQTT_TEST_HOLD_PUBACK=64,MQTT_TEST_REJECT_PUBACK=128,MQTT_TEST_WINDOW_ONE=256,MQTT_TEST_MAX_QOS_ZERO=512};
typedef struct mqttTestPeer {
    xnetengine* Engine;xnetlistener* Listener;xatomicptr Stream;xnetaddr Address;
    xatomic32 Closed,ListenerClosed,Connects,Publishes,Pings,NotifyChecks;
    xatomicptr CallbackFuture,CallbackMessage;xatomic32 CallbackSubmit;
    xatomic32 LastIdentifier,QosZeroes,Acks,AckIdentifier;
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
    xmqttproperty Props[3]={{XMQTT_PROPERTY_SERVER_KEEP_ALIVE,XMQTT_PROPERTY_UINT16,{0}},
        {XMQTT_PROPERTY_RECEIVE_MAXIMUM,XMQTT_PROPERTY_UINT16,{0}},
        {XMQTT_PROPERTY_MAXIMUM_QOS,XMQTT_PROPERTY_BYTE,{0}}};size_t Count=0;
    if(P->Mode&MQTT_TEST_KEEPALIVE){Props[Count]=Props[0];Props[Count++].Value.Number=1;}
    if(P->Mode&MQTT_TEST_WINDOW_ONE){Props[Count]=(xmqttproperty){XMQTT_PROPERTY_RECEIVE_MAXIMUM,XMQTT_PROPERTY_UINT16,{0}};Props[Count++].Value.Number=1;}
    if(P->Mode&MQTT_TEST_MAX_QOS_ZERO)Props[Count++]=(xmqttproperty){XMQTT_PROPERTY_MAXIMUM_QOS,XMQTT_PROPERTY_BYTE,{0}};
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
                if((F.Header.Flags>>1)&3u){xmqttpacket Pub;xmqttproperty Props[8];xmqttcodecbuffers B={Props,8,NULL,0};
                    testRequire((P->Version==XMQTT_VERSION_5?xmqttPacketRead5(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&B,&Pub,NULL):xmqttPacketRead311(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&B,&Pub,NULL))==XMQTT_CODEC_DONE,"fixture QoS1 publish decode");
                    xrtAtomic32Store(&P->LastIdentifier,Pub.Identifier,XMEMORY_RELEASE);
                    if(!(P->Mode&MQTT_TEST_HOLD_PUBACK))mqttTestPuback(P,Pub.Identifier);
                }else {mqttTestSend(P,F.Wire.Data,F.Wire.Size);xrtAtomic32FetchAdd(&P->QosZeroes,1,XMEMORY_RELEASE);}
                xrtAtomic32FetchAdd(&P->Publishes,1,XMEMORY_RELEASE);
            }
            else if(F.Header.Type==XMQTT_PACKET_PUBACK){testRequire(F.Wire.Size==4,"minimal receiver PUBACK");const unsigned char* W=F.Wire.Data;xrtAtomic32Store(&P->AckIdentifier,((uint32)W[2]<<8)|W[3],XMEMORY_RELEASE);xrtAtomic32FetchAdd(&P->Acks,1,XMEMORY_RELEASE);}
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
    xrtAtomic32Init(&P->LastIdentifier,0);xrtAtomic32Init(&P->QosZeroes,0);xrtAtomic32Init(&P->Acks,0);xrtAtomic32Init(&P->AckIdentifier,0);
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;P->Engine=xrtNetEngineCreate(&E);testRequire(P->Engine && xrtNetEngineStart(P->Engine),"MQTT fixture engine");
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
        xmqttclient* C=mqttTestConnect(&P,&K);xfuture* F=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)mqttTestResult(F))->Status==XMQTT_STATUS_OK,"receive bounds ready");
        const unsigned char Normal[]={0x30,5,0,1,'t',0,'x'},Alias[]={0x30,8,0,1,'t',3,0x23,0,1,'x'},Qos1[]={0x34,7,0,1,'t',0,1,0,'x'};
        if(Kind<2){mqttTestSend(&P,Normal,sizeof(Normal));if(Kind==0)mqttTestSend(&P,Normal,sizeof(Normal));}
        else if(Kind==2)mqttTestSend(&P,Alias,sizeof(Alias));else mqttTestSend(&P,Qos1,sizeof(Qos1));
        testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"receive policy failure closes connection");xmqttclientinfo I;xmqttClientInfo(C,&I);
        testRequire(I.CloseStatus==(Kind<2?XMQTT_STATUS_LIMIT:Kind==2?XMQTT_STATUS_PROTOCOL:XMQTT_STATUS_UNSUPPORTED),"receive typed bound/protocol/unsupported classification");
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
static void mqttTestIncoming(mqttTestPeer* P,uint16 Identifier,bool Duplicate)
{
    xmqttpacket Pub;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);Pub.Flags=(uint8)(2u|(Duplicate?8u:0u));Pub.Identifier=Identifier;
    Pub.Body.Publish.Topic=XRT_STR_LITERAL("incoming");Pub.Body.Publish.Payload=XRT_BYTES_LITERAL("received");
    xmqttproperty Props[2]={{XMQTT_PROPERTY_USER,XMQTT_PROPERTY_STRING_PAIR,{0}},{XMQTT_PROPERTY_CORRELATION_DATA,XMQTT_PROPERTY_BINARY,{0}}};
    Props[0].Value.Pair=(xmqttstringpair){XRT_STR_LITERAL("key"),XRT_STR_LITERAL("value")};Props[1].Value.Binary=XRT_BYTES_LITERAL("\0x");
    if(P->Version==XMQTT_VERSION_5)Pub.Properties=(xmqttpropertylist){Props,2};
    unsigned char Wire[128];size_t N=0;testRequire(P->Version==XMQTT_VERSION_5?xmqttPacketWrite5(Wire,sizeof(Wire),&Pub,XMQTT_SERVER_TO_CLIENT,NULL,&N):xmqttPacketWrite311(Wire,sizeof(Wire),&Pub,XMQTT_SERVER_TO_CLIENT,NULL,&N),"incoming QoS1 encode");mqttTestSend(P,Wire,N);
}
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
    if(B->Active && (I.Received==2 || I.State>=XMQTT_CLIENT_CLOSING)){testRequire(xrtAllocationBudgetLeave(&B->Scope),"worker receive scope leave");B->Active=false;}
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
int main(void)
{testMqttClientRoundtrip();testMqttClientTerminal();testMqttClientAdmissionCancel();testMqttClientKeepalive();testMqttClientReceiveBounds();testMqttClientObjectAdmission();testMqttClientReentrancy();testMqttClientAllocationFailure();testMqttQosOneCompletion();testMqttQosOneWindow();testMqttQosOneCancellationAndRestrictions();testMqttQosOneIdsAndMismatches();testMqttQosOneAllocationRollback();testMqttReceiveOneOwnership();testMqttReceiveOneLimits();testMqttReceiveOneAllocationFailure();puts("[PASS] xmqtt TCP/QoS1: send/receive decisions and lifetimes, ids, flow control, restrictions, metadata limits, cancel/timeout, mismatches and OOM rollback");return 0;}
