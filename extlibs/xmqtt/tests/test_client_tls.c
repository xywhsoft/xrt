#include "test.h"
#include "support/tls_peer.h"
static xmqttpacket tlsConnect(void)
{xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=true;Q.Body.Connect.ClientId=XRT_STR_LITERAL("tls");return Q;}
static const void* tlsFuture(xfuture* F)
{testRequire(F && xrtFutureWaitFor(F,15000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"typed TLS outcome");return xrtFutureValue(F);}
static void tlsClose(xmqttclient* C)
{testRequire(xmqttClientClose(C) && xmqttClientWaitFor(C,15000)==XWAIT_OK && xmqttClientPollRetired(C)==XWAIT_OK && xmqttClientDestroy(C),"all actual TLS/client/native tails retire");}
static void tlsRoundtrip(void)
{
    for(unsigned Mode=0;Mode<6;++Mode){mqttTlsPeer P;mqttTlsStart(&P,Mode&1?XMQTT_VERSION_311:XMQTT_VERSION_5,0);
        xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=P.Version;K.WriteBytes=K.WriteChunkBytes=1;K.ReadBytes=1;K.WorkPackets=1;
        xmqttclienttlsconfig T;char Name[12],Alpn[4];xstrview Protocol;xtlsverifier* V=mqttTlsConfigure(&P,&T,Name,&Protocol,Alpn);
        xmqttpacket Q=tlsConnect();xmqttclient* C=NULL;
        if(Mode<2)testRequire(xmqttClientTlsConnect(P.Engine,&P.Address,1,&K,&T,&Q,&C),"numeric TLS factory");
        else if(Mode<4){char Host[11];memcpy(Host,"example.com",11);T.Session.ServerName=(xstrview){0};testRequire(xmqttClientTlsDial(P.Engine,P.Resolver,(xstrview){Host,11},P.Address.Port,1,&K,&T,&Q,&C),"counted-host TLS factory");memset(Host,'x',sizeof(Host));}
        else {xnetaddrlist* L=xrtNetAddrListCreate(&P.Address,1);testRequire(L && xmqttClientTlsDialAddresses(P.Engine,L,1,&K,&T,&Q,&C),"prepared TLS factory");xrtNetAddrListDestroy(L);}
        xrtTlsVerifierRelease(V);memset(Name,'x',sizeof(Name));memset(Alpn,'y',sizeof(Alpn));Protocol=(xstrview){0};
        xfuture* Ready=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)tlsFuture(Ready))->Status==XMQTT_STATUS_OK,"TLS plus MQTT readiness");
        for(unsigned Qos=0;Qos<3;++Qos){xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=(uint8)(Qos<<1);Q.Body.Publish.Topic=XRT_STR_LITERAL("tls/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");xfuture* F=NULL;
            testRequire(xmqttClientPublishAsync(C,&Q,10000,&F)==XMQTT_ADMISSION_ACCEPTED,"TLS QoS admission");const xmqttwriteresult* W=tlsFuture(F);testRequire(W->Status==XMQTT_STATUS_OK && W->AcceptedBytes && (Qos?W->Delivery==XMQTT_PEER_ACCEPTED:W->Delivery==XMQTT_MAY_HAVE_REACHED_PEER),"TLS QoS physical/protocol outcome");xrtFutureDestroy(F);}
        xfuture* Bye=NULL;testRequire(xmqttClientDisconnectAsync(C,NULL,10000,&Bye)==XMQTT_ADMISSION_ACCEPTED,"TLS graceful request");testRequire(((const xmqttwriteresult*)tlsFuture(Bye))->Status==XMQTT_STATUS_OK,"authenticated TLS shutdown");
        tlsClose(C);testRequire(xrtAtomic32Load(&P.VerifierDrops,XMEMORY_ACQUIRE)==1,"Wait includes final verifier release");mqttTlsStop(&P);xrtFutureDestroy(Ready);xrtFutureDestroy(Bye);
    }
}
static void tlsResumePeer(xnetworker* W,ptr Data)
{
    (void)W;mqttTlsPeer* P=Data;xtlsstream* S=xrtAtomicPtrLoad(&P->Server,XMEMORY_ACQUIRE);
    testRequire(xrtNetSocketSet(xrtNetStreamSocket(xrtTlsStreamTransport(S)),XNET_OPTION_RECEIVE_BUFFER,4*1024*1024),"restore receive window after observed backpressure");
    mqttTlsRead(S,NULL,P);
}
static void tlsPrefix(void)
{
    for(unsigned Mode=0;Mode<3;++Mode){mqttTlsPeer P;mqttTlsStart(&P,XMQTT_VERSION_5,MQTT_TLS_PAUSE);xmqttclienttlsconfig T;char Name[12],Alpn[4];xstrview Protocol;
    xtlsverifier* V=mqttTlsConfigure(&P,&T,Name,&Protocol,Alpn);xmqttclient* C=NULL;xmqttpacket Q=tlsConnect();
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Codec.MaxPacketBytes=3u*1024u*1024u;K.Codec.MaxPayloadBytes=3u*1024u*1024u;
    testRequire(xmqttClientTlsConnect(P.Engine,&P.Address,1,&K,&T,&Q,&C),"prefix TLS factory");xrtTlsVerifierRelease(V);xfuture* Ready=xmqttClientReady(C);
    testRequire(((const xmqttreadyresult*)tlsFuture(Ready))->Status==XMQTT_STATUS_OK,"prefix ready");
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Body.Publish.Topic=XRT_STR_LITERAL("p");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("first");xfuture *First=NULL,*Later=NULL;
    testRequire(xmqttClientPublishAsync(C,&Q,15000,&First)==XMQTT_ADMISSION_ACCEPTED,"first prefix admission");
    size_t N=2u*1024u*1024u;unsigned char* Bytes=xrtMalloc(N);testRequire(Bytes!=NULL,"large caller payload");memset(Bytes,0x6b,N);Q.Body.Publish.Payload=(xbytesview){Bytes,N};
    testRequire(xmqttClientPublishAsync(C,&Q,15000,&Later)==XMQTT_ADMISSION_ACCEPTED,"later bounded output admission");memset(Bytes,0,N);xrtFree(Bytes);
    testRequire(((const xmqttwriteresult*)tlsFuture(First))->Status==XMQTT_STATUS_OK,"first QoS0 retires independently of later output");
    testRequire(!xrtFutureDone(Later),"later output remains backpressured");xmqttclientinfo I;testRequire(xmqttClientInfo(C,&I) && I.Requests==1 && I.OutgoingBytes>N,"only later physical request quota remains");
    if(!Mode){xrtAtomic32Store(&P.Resume,1,XMEMORY_RELEASE);testRequire(xrtNetEnginePost(P.Engine,0,tlsResumePeer,&P),"explicit slow-reader application resume");
        testRequire(((const xmqttwriteresult*)tlsFuture(Later))->Status==XMQTT_STATUS_OK,"later cut eventually retires");mqttTlsWait(&P.Publishes,2,"both plaintext frames arrive");}
    else {testRequire(Mode==1?xrtFutureCancel(Later):xmqttClientClose(C),"interrupt pending ciphertext prefix");const xmqttwriteresult* R=tlsFuture(Later);
        testRequire(R->Status==(Mode==1?XMQTT_STATUS_CANCELLED:XMQTT_STATUS_CLOSED) && R->AcceptedBytes && R->Delivery==XMQTT_MAY_HAVE_REACHED_PEER,"accepted partial TLS output remains uncertain after interruption");}
    tlsClose(C);mqttTlsStop(&P);xrtFutureDestroy(First);xrtFutureDestroy(Later);xrtFutureDestroy(Ready);
    }
}
static void tlsAliasesAndSubscriptions(void)
{
    for(unsigned Version=0;Version<2;++Version){mqttTlsPeer P;mqttTlsStart(&P,Version?XMQTT_VERSION_311:XMQTT_VERSION_5,0);xmqttclienttlsconfig T;char Name[12],Alpn[4];xstrview Protocol;
        xtlsverifier* V=mqttTlsConfigure(&P,&T,Name,&Protocol,Alpn);xmqttclient* C=NULL;xmqttpacket Q=tlsConnect();xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=P.Version;
        testRequire(xmqttClientTlsConnect(P.Engine,&P.Address,1,&K,&T,&Q,&C),"duplex TLS factory");xrtTlsVerifierRelease(V);xfuture* Ready=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)tlsFuture(Ready))->Status==XMQTT_STATUS_OK,"duplex ready");
        xmqttproperty A={0};A.Id=XMQTT_PROPERTY_TOPIC_ALIAS;A.Type=XMQTT_PROPERTY_UINT16;A.Value.Number=1;
        for(unsigned I=0;I<3;++I){xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=I==2 && !Version?(xstrview){0}:XRT_STR_LITERAL("alias/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");
            if(!Version)Q.Properties=(xmqttpropertylist){&A,1};
            xfuture* F=NULL;testRequire(xmqttClientPublishAsync(C,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED,"TLS alias/gather request");
            testRequire(((const xmqttwriteresult*)tlsFuture(F))->Status==XMQTT_STATUS_OK,"TLS full/compacted alias physical tails");xrtFutureDestroy(F);}
        xmqttsubscription S={XRT_STR_LITERAL("sensor/+"),1};xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&Q);Q.Body.Topics=(xmqttsubscriptionlist){&S,1};xfuture* F=NULL;
        testRequire(xmqttClientSubscribeAsync(C,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED,"TLS subscription");testRequire(((const xmqttsubscriptionresult*)tlsFuture(F))->Status==XMQTT_STATUS_OK,"TLS ordered SUBACK");xrtFutureDestroy(F);
        xmqttmessage* M=NULL;double End=xrtTimer()+5;while(!xmqttClientTakeMessage(C,&M)){testRequire(xrtTimer()<End,"TLS inbound publication");xrtSleep(1);}
        const xmqttpacket* In=xmqttMessagePacket(M);testRequire(In->Body.Publish.Payload.Size==3 && !memcmp(In->Body.Publish.Payload.Data,"x\0z",3),"owned TLS receive bytes");mqttTlsWait(&P.ReceivedAcks,1,"incoming TLS PUBACK");
        xmqttPacketInit(XMQTT_PACKET_UNSUBSCRIBE,&Q);Q.Body.Topics=(xmqttsubscriptionlist){&S,1};S.Options=0;F=NULL;
        testRequire(xmqttClientUnsubscribeAsync(C,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED,"TLS unsubscribe");testRequire(((const xmqttsubscriptionresult*)tlsFuture(F))->Status==XMQTT_STATUS_OK,"TLS UNSUBACK");xrtFutureDestroy(F);
        tlsClose(C);mqttTlsStop(&P);testRequire(In->Body.Publish.Payload.Size==3 && !memcmp(In->Body.Publish.Payload.Data,"x\0z",3),"TLS message outlives Client/Engine");xmqttMessageRelease(M);xrtFutureDestroy(Ready);
    }
}
static void tlsSetupFailures(void)
{
    for(unsigned Mode=0;Mode<4;++Mode){mqttTlsPeer P;mqttTlsStart(&P,XMQTT_VERSION_5,Mode==3?MQTT_TLS_REJECT:MQTT_TLS_SILENT);
        xmqttclienttlsconfig T;char Name[12],Alpn[4];xstrview Protocol;xtlsverifier* V=mqttTlsConfigure(&P,&T,Name,&Protocol,Alpn);
        xmqttclientconfig K;xmqttClientConfigInit(&K);K.SetupTimeout=Mode==1?40:5000;T.Stream.HandshakeTimeout=Mode==2?40:5000;
        xmqttclient* C=NULL;xmqttpacket Q=tlsConnect();testRequire(xmqttClientTlsConnect(P.Engine,&P.Address,1,&K,&T,&Q,&C),"failing TLS setup admission");xrtTlsVerifierRelease(V);xfuture* F=xmqttClientReady(C);
        if(!Mode){double End=xrtTimer()+5;while(!xrtAtomicPtrLoad(&P.Raw,XMEMORY_ACQUIRE) || !xrtNetStreamAvailable(xrtAtomicPtrLoad(&P.Raw,XMEMORY_ACQUIRE))){testRequire(xrtTimer()<End,"TLS ClientHello reaches silent peer");xrtSleep(1);}testRequire(xrtFutureCancel(F),"cancel during native TLS handshake");}
        const xmqttreadyresult* R=tlsFuture(F);testRequire(R->Status==(Mode==0?XMQTT_STATUS_CANCELLED:Mode==3?XMQTT_STATUS_TRANSPORT:XMQTT_STATUS_TIMEOUT),"typed TLS verification/whole/handshake/cancel failure");
        tlsClose(C);testRequire(xrtAtomic32Load(&P.VerifierDrops,XMEMORY_ACQUIRE)==1,"failed attached session fully releases verifier before Wait");mqttTlsStop(&P);
        if(Mode==3)testRequire(R->Cause && xrtErrorKind(R->Cause)!=XERR_NONE,"retained native TLS root cause");
        xrtFutureDestroy(F);
    }
}
static void tlsDnsRetirement(void)
{
    for(unsigned Mode=0;Mode<3;++Mode){mqttTlsPeer P;mqttTlsStart(&P,XMQTT_VERSION_5,MQTT_TLS_BLOCK_DNS);xmqttclienttlsconfig T;char Name[12],Alpn[4];xstrview Protocol;
        xtlsverifier* V=mqttTlsConfigure(&P,&T,Name,&Protocol,Alpn);xmqttclientconfig K;xmqttClientConfigInit(&K);K.SetupTimeout=Mode==2?40:5000;
        xmqttpacket Q=tlsConnect();xmqttclient* C=NULL;testRequire(xmqttClientTlsDial(P.Engine,P.Resolver,XRT_STR_LITERAL("example.com"),P.Address.Port,1,&K,&T,&Q,&C),"TLS prepared before blocked DNS");
        xrtTlsVerifierRelease(V);xfuture* F=xmqttClientReady(C);mqttTlsWait(&P.DnsEntered,1,"blocked TLS DNS entered");testRequire(!xrtAtomic32Load(&P.VerifierDrops,XMEMORY_ACQUIRE),"prepared session retains verifier before DNS");
        if(Mode==0)testRequire(xmqttClientClose(C),"close before TLS attach");
        if(Mode==1)testRequire(xrtFutureCancel(F),"cancel before TLS attach");
        testRequire(((const xmqttreadyresult*)tlsFuture(F))->Status==(Mode==0?XMQTT_STATUS_CLOSED:Mode==1?XMQTT_STATUS_CANCELLED:XMQTT_STATUS_TIMEOUT),"TLS/DNS original outcome");
        testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK && xrtAtomic32Load(&P.VerifierDrops,XMEMORY_ACQUIRE)==1,"unattached session retires before independent lookup returns");
        tlsClose(C);mqttTlsStop(&P);xrtFutureDestroy(F);
    }
}
static void tlsValidation(void)
{
    mqttTlsPeer P;mqttTlsStart(&P,XMQTT_VERSION_5,0);xmqttclienttlsconfig T;char Name[12],Alpn[4];xstrview Protocol;
    xtlsverifier* V=mqttTlsConfigure(&P,&T,Name,&Protocol,Alpn);xmqttclienttlsconfig Base=T;xmqttpacket Q=tlsConnect();xmqttclient* C=(xmqttclient*)(uintptr_t)1;
    for(unsigned I=0;I<12;++I){T=Base;
        if(I==0)T.Size=0;
        if(I==1)T.MaxMetadataBytes=0;
        if(I==2)T.MaxMetadataBytes=2;
        if(I==3)T.MaxMetadataBytes=1048577;
        if(I==4)T.Stream.HandshakeTimeout=-1;
        if(I==5)T.Stream.CloseTimeout=-1;
        if(I==6)T.Stream.AsyncBytesLimit=0;
        if(I==7)T.Session.ProtocolCount=257;
        if(I==8)T.Session.Protocols=NULL;
        if(I==9)T.Session.ServerName.Size=12;
        if(I==10){T.Session.ServerName=(xstrview){0};T.Session.VerifyName=(xstrview){0};}
        if(I==11)T.Session.Verifier=NULL;
        testRequire(!xmqttClientTlsConnect(P.Engine,&P.Address,1,NULL,&T,&Q,&C) && C==(xmqttclient*)(uintptr_t)1,"invalid TLS admission preserves output");xrtClearError();
    }
    T=Base;unsigned char Saved[sizeof(T)];memcpy(Saved,&T,sizeof(T));testRequire(!xmqttClientTlsConnect(P.Engine,&P.Address,1,NULL,&T,&Q,(xmqttclient**)&T) && !memcmp(Saved,&T,sizeof(T)),"TLS descriptor/output overlap preserves all bytes");
    xrtClearError();xrtTlsVerifierRelease(V);mqttTlsStop(&P);
}
static void tlsPrivateQuota(void)
{
    mqttTlsPeer P;mqttTlsStart(&P,XMQTT_VERSION_5,MQTT_TLS_BLOCK_DNS);xmqttclienttlsconfig T;char Name[12],Alpn[4];xstrview Protocol;xtlsverifier* V=mqttTlsConfigure(&P,&T,Name,&Protocol,Alpn);
    xmqttclient* C=NULL;xmqttpacket Q=tlsConnect();testRequire(xmqttClientTlsDial(P.Engine,P.Resolver,XRT_STR_LITERAL("example.com"),P.Address.Port,1,NULL,&T,&Q,&C),"measure bounded constructor footprint");
    xrtTlsVerifierRelease(V);mqttTlsWait(&P.DnsEntered,1,"private quota blocked lookup");xmqttclientinfo Before;testRequire(xmqttClientInfo(C,&Before),"constructor private bytes");tlsClose(C);
    V=mqttTlsConfigure(&P,&T,Name,&Protocol,Alpn);xmqttclientconfig K;xmqttClientConfigInit(&K);K.MaxPrivateBytes=Before.PrivateBytes+12;
    Q=tlsConnect();testRequire(xmqttClientTlsDial(P.Engine,P.Resolver,XRT_STR_LITERAL("example.com"),P.Address.Port,1,&K,&T,&Q,&C),"exact constructor plus host scratch budget");xrtTlsVerifierRelease(V);
    testRequire(xmqttClientInfo(C,&Before),"before fence reservation");xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Body.Publish.Topic=XRT_STR_LITERAL("p");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");xfuture* F=(xfuture*)(uintptr_t)1;
    xrtSetErrorInfo(XERR_STATE,"mqtt.tls.sentinel",1,"preserve overload error");const xerror* Error=xrtGetError();
    testRequire(xmqttClientPublishAsync(C,&Q,5000,&F)==XMQTT_ADMISSION_OVERLOADED && F==(xfuture*)(uintptr_t)1 && xrtGetError()==Error,"TLS fence metadata obeys private quota without changing Future/error");
    xmqttclientinfo After;testRequire(xmqttClientInfo(C,&After) && After.Requests==Before.Requests && After.OutgoingBytes==Before.OutgoingBytes && After.PrivateBytes==Before.PrivateBytes,"refused TLS tail transfers no quota or identifier duty");
    xrtClearError();tlsClose(C);mqttTlsStop(&P);
}
static void tlsAllocation(void)
{
    static const size_t Limits[]={1,128,4096,16384,65536,131072,1048576};unsigned Failed=0,Passed=0;
    for(size_t I=0;I<sizeof(Limits)/sizeof(*Limits);++I){mqttTlsPeer P;mqttTlsStart(&P,XMQTT_VERSION_5,0);xmqttclienttlsconfig T;char Name[12],Alpn[4];xstrview Protocol;xtlsverifier* V=mqttTlsConfigure(&P,&T,Name,&Protocol,Alpn);
        xmqttpacket Q=tlsConnect();xmqttclient* C=(xmqttclient*)(uintptr_t)1;xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[I],&B) && xrtAllocationBudgetEnter(B,&Scope),"TLS factory allocator budget");
        bool OK=xmqttClientTlsConnect(P.Engine,&P.Address,1,NULL,&T,&Q,&C);testRequire(xrtAllocationBudgetLeave(&Scope),"leave factory allocator budget");xrtTlsVerifierRelease(V);
        if(OK){++Passed;xfuture* F=xmqttClientReady(C);(void)tlsFuture(F);tlsClose(C);xrtFutureDestroy(F);}else{++Failed;testRequire(C==(xmqttclient*)(uintptr_t)1,"TLS factory OOM preserves output");}
        mqttTlsStop(&P);xrtClearError();xallocationbudgetinfo Info={.Size=sizeof(Info)};testRequire(xrtAllocationBudgetInfo(B,&Info) && !Info.LiveBytes && !Info.LiveCount,"all allocations charged to the TLS factory scope retire");xrtAllocationBudgetFree(B);
    }
    testRequire(Failed && Passed,"TLS allocator prefixes cover admission failures and success");
}
typedef struct tlsWorkerBudget {xallocationbudget* Budget;xallocationscope Scope;xatomic32 Armed;bool Entered,Active;} tlsWorkerBudget;
static void tlsWorkerNotify(xmqttclient* C,ptr Data)
{
    tlsWorkerBudget* B=Data;xmqttclientinfo I;testRequire(xmqttClientInfo(C,&I),"worker budget state");
    if(!B->Entered && I.Ready && I.State==XMQTT_CLIENT_OPEN){testRequire(xrtAllocationBudgetEnter(B->Budget,&B->Scope),"TLS worker allocation scope");B->Entered=B->Active=true;xrtAtomic32Store(&B->Armed,1,XMEMORY_RELEASE);}
    else if(B->Active && I.State>=XMQTT_CLIENT_CLOSING){testRequire(xrtAllocationBudgetLeave(&B->Scope),"TLS worker scope retirement");B->Active=false;}
}
static void tlsWorkerAllocation(void)
{
    const size_t Limits[]={1,65536,8u*1024u*1024u};unsigned Failed=0,Passed=0;
    for(size_t I=0;I<sizeof(Limits)/sizeof(*Limits);++I){mqttTlsPeer P;mqttTlsStart(&P,XMQTT_VERSION_5,0);tlsWorkerBudget B={0};testRequire(xrtAllocationBudgetCreate(Limits[I],&B.Budget),"worker allocator budget");
        xmqttclienttlsconfig T;char Name[12],Alpn[4];xstrview Protocol;xtlsverifier* V=mqttTlsConfigure(&P,&T,Name,&Protocol,Alpn);xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=tlsWorkerNotify;K.User=&B;K.Codec.MaxPacketBytes=K.Codec.MaxPayloadBytes=3u*1024u*1024u;
        xmqttclient* C=NULL;xmqttpacket Q=tlsConnect();testRequire(xmqttClientTlsConnect(P.Engine,&P.Address,1,&K,&T,&Q,&C),"worker OOM fixture");xrtTlsVerifierRelease(V);xfuture* Ready=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)tlsFuture(Ready))->Status==XMQTT_STATUS_OK,"worker OOM starts after authenticated readiness");mqttTlsWait(&B.Armed,1,"worker scope armed");
        size_t N=2u*1024u*1024u;bytes Payload=xrtMalloc(N);testRequire(Payload!=NULL,"worker OOM caller snapshot");memset(Payload,0x6b,N);xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Body.Publish.Topic=XRT_STR_LITERAL("p");Q.Body.Publish.Payload=(xbytesview){Payload,N};xfuture* F=NULL;
        xmqttclientadmission A=xmqttClientPublishAsync(C,&Q,15000,&F);xrtFree(Payload);
        if(A==XMQTT_ADMISSION_ACCEPTED){const xmqttwriteresult* R=tlsFuture(F);if(R->Status==XMQTT_STATUS_OK)++Passed;else{testRequire(R->Status==XMQTT_STATUS_MEMORY,"worker TLS OOM has typed memory outcome");++Failed;}xrtFutureDestroy(F);}
        else {xmqttclientinfo State;testRequire(A==XMQTT_ADMISSION_CLOSED && xmqttClientInfo(C,&State) && State.CloseStatus==XMQTT_STATUS_MEMORY,"worker IO OOM can precede producer admission");++Failed;}
        tlsClose(C);testRequire(B.Entered && !B.Active,"worker scope leaves on its actual owner");mqttTlsStop(&P);xrtFutureDestroy(Ready);xrtClearError();xallocationbudgetinfo Info={.Size=sizeof(Info)};testRequire(xrtAllocationBudgetInfo(B.Budget,&Info) && !Info.LiveBytes && !Info.LiveCount,"worker cipher/IO/error allocation duties retire after actual join");xrtAllocationBudgetFree(B.Budget);
    }
    testRequire(Failed && Passed,"worker allocation budgets cover real failures and successful traffic");
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
static void tlsAuthDecide(xmqttclient* C,xfuture* F)
{
    double End=xrtTimer()+10;
    while(!xrtFutureDone(F)){xmqttauthticket* T=NULL;
        if(xmqttClientTakeAuth(C,&T)){if(xmqttAuthPhase(T)==XMQTT_AUTH_CHALLENGE){xmqttproperty Props[2]={{0}};Props[0].Id=XMQTT_PROPERTY_AUTH_METHOD;Props[0].Type=XMQTT_PROPERTY_STRING;Props[0].Value.Text=XRT_STR_LITERAL("m");Props[1].Id=XMQTT_PROPERTY_AUTH_DATA;Props[1].Type=XMQTT_PROPERTY_BINARY;Props[1].Value.Binary=XRT_BYTES_LITERAL("D");
                xmqttpacket R;xmqttPacketInit(XMQTT_PACKET_AUTH,&R);R.Reason=0x18;R.Properties=(xmqttpropertylist){Props,2};testRequire(xmqttAuthRespond(T,&R)==XMQTT_ADMISSION_ACCEPTED,"TLS enhanced-auth continuation");}
            else testRequire(xmqttAuthComplete(T,true)==XMQTT_ADMISSION_ACCEPTED,"TLS final-proof gate");
            xmqttAuthRelease(T);
        }else {testRequire(xrtTimer()<End,"bounded TLS auth decision");xrtSleep(1);}}
}
static void tlsEnhanced(void)
{
    for(unsigned Mode=0;Mode<3;++Mode){mqttTlsPeer P;mqttTlsStart(&P,XMQTT_VERSION_5,MQTT_TLS_AUTH);xmqttclienttlsconfig T;char Name[12],Alpn[4];xstrview Protocol;
        xtlsverifier* V=mqttTlsConfigure(&P,&T,Name,&Protocol,Alpn);xmqttclientconfig K;xmqttClientConfigInit(&K);K.WriteBytes=K.WriteChunkBytes=1;
        xmqttproperty Property={0};Property.Id=XMQTT_PROPERTY_AUTH_METHOD;Property.Type=XMQTT_PROPERTY_STRING;Property.Value.Text=XRT_STR_LITERAL("m");xmqttpacket Q=tlsConnect();Q.Properties=(xmqttpropertylist){&Property,1};xmqttclient* C=NULL;
        if(!Mode)testRequire(xmqttClientTlsConnectAuth(P.Engine,&P.Address,1,&K,&T,NULL,&Q,&C),"TLS plus enhanced auth numeric factory");
        else if(Mode==1)testRequire(xmqttClientTlsDialAuth(P.Engine,P.Resolver,XRT_STR_LITERAL("example.com"),P.Address.Port,1,&K,&T,NULL,&Q,&C),"TLS plus enhanced auth hostname factory");
        else {xnetaddrlist* L=xrtNetAddrListCreate(&P.Address,1);testRequire(L && xmqttClientTlsDialAddressesAuth(P.Engine,L,1,&K,&T,NULL,&Q,&C),"TLS plus enhanced auth endpoint factory");xrtNetAddrListDestroy(L);}
        xrtTlsVerifierRelease(V);xfuture* Ready=xmqttClientReady(C);tlsAuthDecide(C,Ready);testRequire(((const xmqttreadyresult*)tlsFuture(Ready))->Status==XMQTT_STATUS_OK,"TLS plus all initial MQTT proof gates");
        xmqttPacketInit(XMQTT_PACKET_AUTH,&Q);Q.Reason=0x19;Q.Properties=(xmqttpropertylist){&Property,1};xfuture* F=NULL;testRequire(xmqttClientReauthenticateAsync(C,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED,"TLS reauth admission");tlsAuthDecide(C,F);
        testRequire(((const xmqttauthresult*)tlsFuture(F))->Status==XMQTT_STATUS_OK,"TLS reauth proof");tlsClose(C);mqttTlsStop(&P);xrtFutureDestroy(F);xrtFutureDestroy(Ready);
    }
}
#endif
int main(void)
{tlsRoundtrip();tlsPrefix();tlsAliasesAndSubscriptions();tlsSetupFailures();tlsDnsRetirement();tlsValidation();tlsPrivateQuota();tlsAllocation();tlsWorkerAllocation();
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    tlsEnhanced();
#endif
    puts("[PASS] MQTT TLS: snapshots/factories/QoS/prefix/verification/deadlines/DNS/physical retirement/auth composition");return 0;}
