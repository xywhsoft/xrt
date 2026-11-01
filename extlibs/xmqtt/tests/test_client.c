#include "test.h"
#include <xmqtt/client.h>
#include <xrt/thread.h>
#include <xrt/atomic.h>
#include <xrt/time.h>

/* In-process protocol fixture. Independent wire/broker interoperability lives
 * separately; this fixture exercises ownership, limits and worker reentrancy. */
enum {MQTT_TEST_HOLD=1,MQTT_TEST_REFUSE=2,MQTT_TEST_SESSION=4,MQTT_TEST_DUPLICATE=8,MQTT_TEST_NO_PONG=16,MQTT_TEST_KEEPALIVE=32};
typedef struct mqttTestPeer {
    xnetengine* Engine;xnetlistener* Listener;xatomicptr Stream;xnetaddr Address;
    xatomic32 Closed,ListenerClosed,Connects,Publishes,Pings,NotifyChecks;
    xatomicptr CallbackFuture,CallbackMessage;xatomic32 CallbackSubmit;
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
    xmqttproperty Keep={XMQTT_PROPERTY_SERVER_KEEP_ALIVE,XMQTT_PROPERTY_UINT16,{0}};Keep.Value.Number=1;
    if(P->Version==XMQTT_VERSION_5 && (P->Mode&MQTT_TEST_KEEPALIVE))A.Properties=(xmqttpropertylist){&Keep,1};
    unsigned char Wire[64];size_t N=0;
    testRequire(P->Version==XMQTT_VERSION_5?xmqttPacketWrite5(Wire,sizeof(Wire),&A,XMQTT_SERVER_TO_CLIENT,NULL,&N):xmqttPacketWrite311(Wire,sizeof(Wire),&A,XMQTT_SERVER_TO_CLIENT,NULL,&N),"fixture CONNACK encode");
    mqttTestSend(P,Wire,N);if(P->Mode&MQTT_TEST_DUPLICATE)mqttTestSend(P,Wire,N);
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
            else if(F.Header.Type==XMQTT_PACKET_PUBLISH){mqttTestSend(P,F.Wire.Data,F.Wire.Size);xrtAtomic32FetchAdd(&P->Publishes,1,XMEMORY_RELEASE);}
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
        const unsigned char Normal[]={0x30,5,0,1,'t',0,'x'},Alias[]={0x30,8,0,1,'t',3,0x23,0,1,'x'},Qos1[]={0x32,7,0,1,'t',0,1,0,'x'};
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
int main(void)
{testMqttClientRoundtrip();testMqttClientTerminal();testMqttClientAdmissionCancel();testMqttClientKeepalive();testMqttClientReceiveBounds();testMqttClientObjectAdmission();testMqttClientReentrancy();testMqttClientAllocationFailure();puts("[PASS] xmqtt TCP: MQTT5/311 handshake, tiny writes, QoS0 roundtrip, retained results/messages, refusal/session/order, bounds/deadlines/cancel, keepalive, worker restrictions, reentrancy, OOM and physical close");return 0;}
