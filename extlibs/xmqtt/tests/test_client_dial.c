#include "test.h"
#include "../examples/common/dial_peer.h"
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
#include "../examples/common/auth_peer.h"
#endif
static const xmqttreadyresult* dialReady(xfuture* F)
{testRequire(xrtFutureWaitFor(F,5000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"Dial typed readiness");return xrtFutureValue(F);}
static xmqttpacket dialConnectPacket(void)
{xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=true;Q.Body.Connect.ClientId=XRT_STR_LITERAL("dial");return Q;}
static void dialClose(xmqttclient* C)
{testRequire(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"Dial physical Client retirement");}
static xfuture* dialPublish(xmqttclient* C)
{
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("dial/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("payload");xfuture* F=NULL;
    testRequire(xmqttClientPublishAsync(C,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED,"queue QoS1 during DNS");return F;
}
static void dialRoundtrip(void)
{
    for(unsigned I=0;I<4;++I){mqttDialPeer P;mqttDialStart(&P,I&1?XMQTT_VERSION_311:XMQTT_VERSION_5,I==2?MQTT_DIAL_DUAL:0);
        xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=P.Peer.Version;K.WriteBytes=K.WriteChunkBytes=1;K.ReadBytes=1;K.WorkPackets=1;
        xmqttclientdialconfig D;xmqttClientDialConfigInit(&D);D.FallbackDelay=1;xmqttpacket Q=dialConnectPacket();xmqttclient* C=NULL;
        if(I==3){xnetaddr A[]={P.RefusedAddress,P.Peer.Address};xnetaddrlist* L=xrtNetAddrListCreate(A,2);testRequire(L && xmqttClientDialAddresses(P.Peer.Engine,L,1,&K,&D,&Q,&C),"prepared endpoint fallback");xrtNetAddrListDestroy(L);}
        else {char Host[12]={'m','q','t','t','.','f','i','x','t','u','r','e'};testRequire(xmqttClientDial(P.Peer.Engine,P.Resolver,(xstrview){Host,sizeof(Host)},P.Peer.Address.Port,1,&K,&D,&Q,&C),"hostname Dial");memset(Host,'x',sizeof(Host));}
        xfuture* Ready=xmqttClientReady(C);xfuture* Pub=dialPublish(C);const xmqttreadyresult* R=dialReady(Ready);testRequire(R && R->Status==XMQTT_STATUS_OK,"Dial ready after CONNECT/CONNACK");
        testRequire(xrtFutureWaitFor(Pub,5000)==XWAIT_OK && ((const xmqttwriteresult*)xrtFutureValue(Pub))->Status==XMQTT_STATUS_OK,"Dial existing QoS1 path");
        dialClose(C);mqttDialStop(&P);testRequire(R->Status==XMQTT_STATUS_OK && R->Connack && R->Negotiated.Version==K.Version,"ready result outlives Client/Engine");xrtFutureDestroy(Ready);xrtFutureDestroy(Pub);
    }
}
static void dialBlocked(void)
{
    for(unsigned Action=0;Action<4;++Action){mqttDialPeer P;mqttDialStart(&P,XMQTT_VERSION_5,MQTT_DIAL_BLOCK);xmqttpacket Q=dialConnectPacket();xmqttclient* C=NULL;
        xmqttclientconfig K;xmqttClientConfigInit(&K);K.SetupTimeout=Action==2?25:5000;xmqttclientdialconfig D;xmqttClientDialConfigInit(&D);D.Timeout=Action==3?25:0;
        testRequire(xmqttClientDial(P.Peer.Engine,P.Resolver,XRT_STR_LITERAL("mqtt.fixture"),P.Peer.Address.Port,1,&K,&D,&Q,&C),"blocked DNS admission");xfuture* F=xmqttClientReady(C);xfuture* Pub=dialPublish(C);
        mqttExampleWait(&P.Entered);if(Action==0)testRequire(xmqttClientClose(C),"close during DNS");else if(Action==1)testRequire(xrtFutureCancel(F),"Ready cancel during DNS");
        const xmqttreadyresult* R=dialReady(F);testRequire(R->Status==(Action==0?XMQTT_STATUS_CLOSED:Action==1?XMQTT_STATUS_CANCELLED:XMQTT_STATUS_TIMEOUT),"first close/cancel/setup deadline wins");
        testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"cancelled subscriber and native callbacks retire independently of shared lookup");
        testRequire(!xrtAtomic32Load(&P.Exited,XMEMORY_ACQUIRE) && !xrtAtomicPtrLoad(&P.Peer.Stream,XMEMORY_ACQUIRE),"lookup still blocked without a client Stream");
        testRequire(xrtFutureWaitFor(Pub,5000)==XWAIT_OK && ((const xmqttwriteresult*)xrtFutureValue(Pub))->AcceptedBytes==0,"cancelled setup never sends application bytes");
        dialClose(C);testRequire(xrtNetResolverTryDestroy(P.Resolver)==XNET_RETIRE_BUSY,"Resolver owns the independent blocked shared lookup");
        /* No MQTT/client context remains. Retire the Engine before releasing the
         * shared lookup to catch stale callbacks or borrowed Engine accesses. */
        testRequire(xrtNetListenerClose(P.Peer.Listener),"blocked fixture listener close");mqttExampleWait(&P.Peer.ListenerClosed);xrtNetListenerDestroy(P.Peer.Listener);
        testRequire(xrtNetEngineStop(P.Peer.Engine) && xrtNetEngineDestroy(P.Peer.Engine),"Engine retires before shared DNS returns");
        xrtAtomic32Store(&P.Gate,1,XMEMORY_RELEASE);testRequire(xrtNetResolverDestroy(P.Resolver) && xrtNetSocketClose(P.Refused),"join actual shared lookup");
        testRequire(xrtAtomic32Load(&P.Exited,XMEMORY_ACQUIRE) && R->Status!=(xmqttclientstatus)-1,"retained result after all owners retire");xrtFutureDestroy(F);xrtFutureDestroy(Pub);
    }
}
typedef struct dialNotifyGate {xatomic32 Entered,Release;} dialNotifyGate;
static void dialClosingNotify(xmqttclient* C,ptr Data)
{
    dialNotifyGate* G=Data;xmqttclientinfo I;testRequire(xmqttClientInfo(C,&I),"closing callback snapshot");
    if(I.State<XMQTT_CLIENT_CLOSING || xrtAtomic32Load(&G->Entered,XMEMORY_ACQUIRE))return;
    xrtAtomic32Store(&G->Entered,1,XMEMORY_RELEASE);double End=xrtTimer()+5;
    while(!xrtAtomic32Load(&G->Release,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"release callback gate");xrtThreadYield();}
}
static void dialCallbackRetirement(void)
{
    mqttDialPeer P;mqttDialStart(&P,XMQTT_VERSION_5,MQTT_DIAL_BLOCK);dialNotifyGate G;xrtAtomic32Init(&G.Entered,0);xrtAtomic32Init(&G.Release,0);
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=dialClosingNotify;K.User=&G;xmqttpacket Q=dialConnectPacket();xmqttclient* C=NULL;
    testRequire(xmqttClientDial(P.Peer.Engine,P.Resolver,XRT_STR_LITERAL("mqtt.fixture"),P.Peer.Address.Port,1,&K,NULL,&Q,&C),"callback barrier factory");xfuture* F=xmqttClientReady(C);mqttExampleWait(&P.Entered);
    testRequire(xmqttClientClose(C),"callback barrier close");mqttExampleWait(&G.Entered);testRequire(dialReady(F)->Status==XMQTT_STATUS_CLOSED && xmqttClientWaitFor(C,0)==XWAIT_TIMEOUT,"logical Ready cannot retire an active client callback");
    xrtAtomic32Store(&G.Release,1,XMEMORY_RELEASE);dialClose(C);mqttDialStop(&P);xrtFutureDestroy(F);
}
static void dialFailures(void)
{
    for(unsigned I=0;I<3;++I){mqttDialPeer P;mqttDialStart(&P,XMQTT_VERSION_5,I==0?MQTT_DIAL_MISSING:0);xmqttpacket Q=dialConnectPacket();xmqttclient* C=NULL;
        if(!I)testRequire(xmqttClientDial(P.Peer.Engine,P.Resolver,XRT_STR_LITERAL("mqtt.fixture"),P.Peer.Address.Port,1,NULL,NULL,&Q,&C),"missing DNS is asynchronous");
        else {xnetaddr A[2]={P.RefusedAddress,P.Peer.Address};xmqttclientdialconfig D;xmqttClientDialConfigInit(&D);D.MaxAttempts=1;D.Family=I==2?XNET_FAMILY_IPV6:XNET_FAMILY_UNSPEC;
            if(I==2){xrtNetAddrLoopback(&A[0],XNET_FAMILY_IPV6,P.RefusedAddress.Port);}
            xnetaddrlist* L=xrtNetAddrListCreate(A,2);testRequire(L && xmqttClientDialAddresses(P.Peer.Engine,L,1,NULL,&D,&Q,&C),"bounded refused candidate");xrtNetAddrListDestroy(L);}
        xfuture* F=xmqttClientReady(C);const xmqttreadyresult* R=dialReady(F);testRequire(R->Status==XMQTT_STATUS_TRANSPORT && R->Cause,"DNS/refusal owned cause");
        dialClose(C);mqttDialStop(&P);testRequire(R->Cause && xrtErrorKind(R->Cause)!=XERR_NONE,"failure diagnostic outlives native resources");xrtFutureDestroy(F);xrtClearError();
    }
}
static void dialValidation(void)
{
    mqttDialPeer P;mqttDialStart(&P,XMQTT_VERSION_5,0);xmqttpacket Q=dialConnectPacket();xmqttclient* C=(xmqttclient*)(uintptr_t)1;xmqttclientdialconfig D;
    for(unsigned I=0;I<12;++I){xmqttClientDialConfigInit(&D);xstrview H=XRT_STR_LITERAL("mqtt.fixture");uint16 Port=P.Peer.Address.Port;
        if(I==0)D.Size=0;
        if(I==1)D.Family=(xnetfamily)99;
        if(I==2)D.Timeout=-2;
        if(I==3)D.FallbackDelay=-1;
        if(I==4)D.MaxAttempts=0;
        if(I==5)D.MaxAttempts=65;
        if(I==6)D.MaxHostBytes=0;
        if(I==7)D.MaxHostBytes=5;
        if(I==8)H=(xstrview){"mqtt\0fixture",12};
        if(I==9)Port=0;
        if(I==10)D.MaxHostBytes=XMQTT_DIAL_HOST_MAX+1;
        if(I==11)H.Size=0;
        testRequire(!xmqttClientDial(P.Peer.Engine,P.Resolver,H,Port,1,NULL,&D,&Q,&C) && C==(xmqttclient*)(uintptr_t)1,"invalid Dial leaves output");xrtClearError();}
    xnetaddr A=P.Peer.Address;A.Port=0;xnetaddrlist* L=xrtNetAddrListCreate(&A,1);testRequire(L && !xmqttClientDialAddresses(P.Peer.Engine,L,1,NULL,NULL,&Q,&C),"zero endpoint port rejected");xrtNetAddrListDestroy(L);xrtClearError();
    A=P.Peer.Address;L=xrtNetAddrListCreate(&A,1);xmqttClientDialConfigInit(&D);D.Family=XNET_FAMILY_IPV6;testRequire(!xmqttClientDialAddresses(P.Peer.Engine,L,1,NULL,&D,&Q,&C),"no matching family rejected");xrtClearError();xrtNetAddrListDestroy(L);
    xnetaddr Many[65];for(size_t I=0;I<65;++I){Many[I]=P.Peer.Address;Many[I].Port=(uint16)(I+1);}L=xrtNetAddrListCreate(Many,65);
    testRequire(L && xrtNetAddrListCount(L)==65 && !xmqttClientDialAddresses(P.Peer.Engine,L,1,NULL,NULL,&Q,&C),"endpoint count bound");xrtNetAddrListDestroy(L);xrtClearError();
    union {xmqttclient* Out;char Bytes[16];} Overlap;memset(&Overlap,0,sizeof(Overlap));memcpy(Overlap.Bytes,"mqtt.fixture",12);unsigned char Saved[sizeof(Overlap)];memcpy(Saved,&Overlap,sizeof(Saved));
    testRequire(!xmqttClientDial(P.Peer.Engine,P.Resolver,(xstrview){Overlap.Bytes,12},P.Peer.Address.Port,1,NULL,NULL,&Q,&Overlap.Out) && !memcmp(Saved,&Overlap,sizeof(Saved)),"host/output overlap rejected unchanged");xrtClearError();
    xmqttClientDialConfigInit(&D);xmqttclientdialconfig SavedD=D;
    testRequire(!xmqttClientDial(P.Peer.Engine,P.Resolver,XRT_STR_LITERAL("mqtt.fixture"),P.Peer.Address.Port,1,NULL,&D,&Q,(xmqttclient**)&D) && !memcmp(&D,&SavedD,sizeof(D)),"Dial descriptor/output overlap rejected unchanged");xrtClearError();
    testRequire(!xmqttClientDialAddresses(P.Peer.Engine,NULL,1,NULL,NULL,&Q,&C),"NULL list rejected");xrtClearError();mqttDialStop(&P);
}
static void dialAllocation(void)
{
    static const size_t Limits[]={1,128,2048,4096,8192,16384,32768,65536,131072,1048576};unsigned Passed=0,Failed=0;
    for(size_t I=0;I<sizeof(Limits)/sizeof(*Limits);++I){mqttDialPeer P;mqttDialStart(&P,XMQTT_VERSION_5,0);xmqttpacket Q=dialConnectPacket();xmqttclient* C=(xmqttclient*)(uintptr_t)1;
        xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[I],&B) && xrtAllocationBudgetEnter(B,&Scope),"Dial allocation scope");
        bool OK=xmqttClientDial(P.Peer.Engine,P.Resolver,XRT_STR_LITERAL("mqtt.fixture"),P.Peer.Address.Port,1,NULL,NULL,&Q,&C);testRequire(xrtAllocationBudgetLeave(&Scope),"Dial scope leave");
        if(OK){++Passed;xfuture* F=xmqttClientReady(C);testRequire(dialReady(F)->Status==XMQTT_STATUS_OK,"sufficient constructor budget");dialClose(C);xrtFutureDestroy(F);}else {++Failed;testRequire(C==(xmqttclient*)(uintptr_t)1,"OOM factory output unchanged");}
        mqttDialStop(&P);xrtClearError();xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);testRequire(xrtAllocationBudgetInfo(B,&Info) && !Info.LiveCount && !Info.LiveBytes,"Dial constructor resources retired");xrtAllocationBudgetFree(B);
    }
    testRequire(Passed && Failed,"Dial OOM and success controls");
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
static void dialAuth(void)
{
    for(unsigned Prepared=0;Prepared<2;++Prepared){mqttAuthPeer P;mqttAuthStart(&P,0);xmqttproperty Props[2];xmqttpacket Q=mqttAuthPacket(XMQTT_PACKET_CONNECT,0,Props,false);xmqttclient* C=NULL;xnetresolver* R=NULL;
        xnetaddrlist* L=xrtNetAddrListCreate(&P.Address,1);testRequire(L!=NULL,"auth endpoint list");
        if(Prepared)testRequire(xmqttClientDialAddressesAuth(P.Engine,L,1,NULL,NULL,NULL,&Q,&C),"Dial+auth prepared factory");
        else {xnetresolverconfig K;xrtNetResolverConfigInit(&K);R=xrtNetResolverCreate(&K);testRequire(R && xmqttClientDialAuth(P.Engine,R,XRT_STR_LITERAL("127.0.0.1"),P.Address.Port,1,NULL,NULL,NULL,&Q,&C),"Dial+auth numeric host");}
        xrtNetAddrListDestroy(L);xfuture* F=xmqttClientReady(C);for(unsigned I=0;I<3;++I)mqttAuthAnswer(mqttAuthTicket(C),false);testRequire(dialReady(F)->Status==XMQTT_STATUS_OK,"Dial+auth final CONNACK proof");
        Q=mqttAuthPacket(XMQTT_PACKET_AUTH,0x19,Props,false);xfuture* Reauth=NULL;testRequire(xmqttClientReauthenticateAsync(C,&Q,5000,&Reauth)==XMQTT_ADMISSION_ACCEPTED,"Dial+auth reauth");for(unsigned I=0;I<3;++I)mqttAuthAnswer(mqttAuthTicket(C),false);testRequire(xrtFutureWaitFor(Reauth,5000)==XWAIT_OK && ((const xmqttauthresult*)xrtFutureValue(Reauth))->Status==XMQTT_STATUS_OK,"Dial+auth reauth result");
        dialClose(C);if(R)testRequire(xrtNetResolverDestroy(R),"auth Resolver retirement");mqttAuthStop(&P);xrtFutureDestroy(F);xrtFutureDestroy(Reauth);
    }
}
#endif
#if defined(XMQTT_DIAL_BIND_FAULT)
static void dialBindingFailure(void)
{
    for(unsigned Mode=1;Mode<=3;++Mode){mqttDialPeer P;mqttDialStart(&P,XMQTT_VERSION_5,0);xmqttpacket Q=dialConnectPacket();xmqttclient* C=NULL;xrtAtomic32Store(&dialBindingFail,Mode,XMEMORY_RELEASE);
        testRequire(xmqttClientDial(P.Peer.Engine,P.Resolver,XRT_STR_LITERAL("mqtt.fixture"),P.Peer.Address.Port,1,NULL,NULL,&Q,&C),"binding-fault Dial factory");xfuture* F=xmqttClientReady(C);
        if(Mode==3)testRequire(xrtFutureWaitFor(F,250)==XWAIT_OK,"EOF before event binding fails promptly, not at SetupTimeout");
        testRequire(dialReady(F)->Status==(Mode==1?XMQTT_STATUS_MEMORY:Mode==2?XMQTT_STATUS_CLOSED:XMQTT_STATUS_TRANSPORT),"winning binding OOM or competing Close preserves first reason");
        xrtAtomic32Store(&dialBindingFail,0,XMEMORY_RELEASE);dialClose(C);mqttDialStop(&P);xrtFutureDestroy(F);xrtClearError();}
}
#endif
int main(void)
{
    dialRoundtrip();dialBlocked();dialCallbackRetirement();dialFailures();dialValidation();dialAllocation();
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    dialAuth();
#endif
#if defined(XMQTT_DIAL_BIND_FAULT)
    dialBindingFailure();
#endif
    puts("[PASS] MQTT bounded DNS/prepared-address Dial, deadlines, cancellation, OOM, physical retirement");return 0;
}
