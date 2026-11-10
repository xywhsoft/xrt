#ifndef XMQTT_CLIENT_SESSION_CASES_H
#define XMQTT_CLIENT_SESSION_CASES_H
static xmqttclient* csConnectExpiry(mqttResumePeer* P,xmqttsession* S,xmqttclientinbox* I,uint32 Seconds,xfuture** Ready)
{
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=XRT_STR_LITERAL("session-client");xmqttproperty E=mqttResumeNumber(XMQTT_PROPERTY_SESSION_EXPIRY,Seconds);Q.Properties=(xmqttpropertylist){&E,1};xmqttclient* C=NULL;
    testRequire(xmqttClientConnectSession(P->Engine,&P->Address,1,NULL,S,I,&Q,&C),"expiry connection");*Ready=xmqttClientReady(C);return C;
}
static void csExpiry(void)
{
    for(unsigned Pass=0;Pass<2;++Pass){xmqttsession* S;xmqttclientinbox* I;csPair(XMQTT_VERSION_5,&S,&I);mqttResumePeer P;mqttResumeStart(&P,XMQTT_VERSION_5,0,false);xfuture* Ready;xmqttclient* C=csConnectExpiry(&P,S,I,1,&Ready);csReady(Ready);
        xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_DISCONNECT,&Q);xmqttproperty E=mqttResumeNumber(XMQTT_PROPERTY_SESSION_EXPIRY,Pass?0:2);Q.Properties=(xmqttpropertylist){&E,1};xfuture* D=NULL;double Before=xrtTimer();
        testRequire(xmqttClientDisconnect(C,&Q,5000,&D)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)csResult(D))->Status==XMQTT_STATUS_OK,"complete disconnect expiry override");testRequire(xmqttClientWaitFor(C,8000)==XWAIT_OK,"expiry native end");double After=xrtTimer();xmqttsessioninfo Info=csInfo(S);
        if(Pass)testRequire(!Info.HasState && !Info.Connected,"zero expiry discards local protocol session");else testRequire(Info.HasState && !Info.Connected && Info.ExpiresAt>=Before+2 && Info.ExpiresAt<=After+2,"expiry starts at actual native close with fully sent override");
        csClose(C);mqttResumeStop(&P);xrtFutureDestroy(D);xrtFutureDestroy(Ready);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"expiry override cleanup");
    }
    xmqttsession* S;xmqttclientinbox* I;csPair(XMQTT_VERSION_5,&S,&I);mqttResumePeer P;mqttResumeStart(&P,XMQTT_VERSION_5,MQTT_RESUME_HOLD_ACK,false);xfuture *Ready,*Next;xmqttclient* C=csConnectExpiry(&P,S,I,1,&Ready);csReady(Ready);xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("expire");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("retained");xfuture* F=NULL;testRequire(xmqttClientPublishAsync(C,&Q,8000,&F)==XMQTT_ADMISSION_ACCEPTED,"expiry old flow");mqttResumeWait(&P.Publishes,1);csClose(C);mqttResumeStop(&P);double Deadline=csInfo(S).ExpiresAt;while(xrtTimer()<Deadline+0.01)xrtSleep(1);
    mqttResumeStart(&P,XMQTT_VERSION_5,0,true);C=csConnectExpiry(&P,S,I,1,&Next);testRequire(((const xmqttreadyresult*)csResult(Next))->Status==XMQTT_STATUS_SESSION,"expired local state cannot adopt server-present session");csClose(C);mqttResumeStop(&P);xrtFutureDestroy(Next);testRequire(csInfo(S).ExpiresAt==Deadline,"failed resume does not extend old expiry");
    mqttResumeStart(&P,XMQTT_VERSION_5,0,false);xmqttclientconfig K={0};xmqttClientConfigInit(&K);K.MaxOutgoingBytes=1;C=csConnect(&P,S,I,&Next,&K);csReady(Next);testRequire(!csInfo(S).ActiveOutgoing && !csInfo(S).Objects && !xrtAtomic32Load(&P.Publishes,XMEMORY_ACQUIRE),"expired old flows need no replay-byte reservation and are not sent");csClose(C);mqttResumeStop(&P);xrtFutureDestroy(F);xrtFutureDestroy(Ready);xrtFutureDestroy(Next);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"expired resume cleanup");
}
static void csPendingPhysical(xmqttversion V)
{
    xmqttsessionconfig K;xmqttSessionConfigInit(&K);K.Version=V;K.ClientId=XRT_STR_LITERAL("session-client");K.MaxOutgoing=1;xmqttsession* S=NULL;xmqttclientinbox* I=NULL;
    xmqttclientinboxconfig IK;xmqttClientInboxConfigInit(&IK);IK.Version=V;testRequire(xmqttSessionCreate(&K,&S) && xmqttClientInboxCreate(&IK,&I),"single physical snapshot owner");
    mqttResumePeer P;mqttResumeStart(&P,V,MQTT_RESUME_HOLD_READY,false);xfuture* Ready;xmqttclient* C=csConnect(&P,S,I,&Ready,NULL);mqttResumeWait(&P.Connects,1);
    unsigned char Payload[257];memset(Payload,'Z',sizeof(Payload));xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Identifier=301;Q.Body.Publish.Topic=XRT_STR_LITERAL("pending");Q.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};xfuture* F=NULL;
    testRequire(xmqttClientPublishAsync(C,&Q,8000,&F)==XMQTT_ADMISSION_ACCEPTED,"pre-CONNACK physical snapshot");xmqttsessioninfo Before=csInfo(S);testRequire(Before.Objects==1 && !Before.ActiveOutgoing && !Before.HasState && !Before.Connected,"queued physical storage is not a protocol obligation");
    Q.Identifier=302;xfuture* Other=(xfuture*)(uintptr_t)1;testRequire(xmqttClientPublishAsync(C,&Q,8000,&Other)==XMQTT_ADMISSION_OVERLOADED && Other==(xfuture*)(uintptr_t)1,"physical Session cap applies before readiness without publishing a Future");
    testRequire(xrtFutureCancel(F) && ((const xmqttwriteresult*)csResult(F))->Status==XMQTT_STATUS_CANCELLED,"pending snapshot cancellation");double End=xrtTimer()+8;
    while(csInfo(S).Objects){testRequire(xrtTimer()<End,"pending cancellation actual retirement");xrtSleep(1);}testRequire(!csInfo(S).HasState && csInfo(S).LiveBytes==csInfo(S).BaseBytes,"pending snapshot fully refunds without offline session state");
    xrtFutureDestroy(F);F=NULL;Q.Identifier=301;testRequire(xmqttClientPublishAsync(C,&Q,8000,&F)==XMQTT_ADMISSION_ACCEPTED,"same identifier/physical slot reuse");csClose(C);mqttResumeStop(&P);
    testRequire(!csInfo(S).Objects && !csInfo(S).ActiveOutgoing && !csInfo(S).HasState,"failed handshake discards pending payload rather than replaying it");xrtFutureDestroy(F);xrtFutureDestroy(Ready);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"pending snapshot cleanup");
}
static void csSnapshotOom(xmqttversion V)
{
    xmqttsession* S;xmqttclientinbox* I;csPair(V,&S,&I);mqttResumePeer P;mqttResumeStart(&P,V,MQTT_RESUME_HOLD_READY,false);xfuture* Ready;xmqttclient* C=csConnect(&P,S,I,&Ready,NULL);mqttResumeWait(&P.Connects,1);
    /* Warm the explicitly persistent identifier map outside each operation's
     * budget; the separate whole-lifetime budget test still owns that cache. */
    xmqttpacket Warm;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Warm);Warm.Flags=2;Warm.Body.Publish.Topic=XRT_STR_LITERAL("warm");xfuture* W=NULL;
    testRequire(xmqttClientPublishAsync(C,&Warm,8000,&W)==XMQTT_ADMISSION_ACCEPTED && xrtFutureCancel(W) && ((const xmqttwriteresult*)csResult(W))->Status==XMQTT_STATUS_CANCELLED,"warm snapshot admission map");xrtFutureDestroy(W);
    double WarmEnd=xrtTimer()+8;while(csInfo(S).Objects){testRequire(xrtTimer()<WarmEnd,"warm snapshot retirement");xrtSleep(1);}
    const size_t Limits[]={1,112,256,512,1024,4096,16384,65536};bool Failed=false,Passed=false;
    for(unsigned N=0;N<sizeof(Limits)/sizeof(*Limits);++N){
        xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[N],&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"snapshot allocation budget");
        unsigned char Payload[257];memset(Payload,'Z',sizeof(Payload));xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Identifier=311;Q.Body.Publish.Topic=XRT_STR_LITERAL("oom/snapshot");Q.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};xfuture* F=(xfuture*)(uintptr_t)1;
        xmqttclientadmission A=xmqttClientPublishAsync(C,&Q,8000,&F);
        if(A==XMQTT_ADMISSION_ACCEPTED){Passed=true;testRequire(xrtFutureCancel(F) && ((const xmqttwriteresult*)csResult(F))->Status==XMQTT_STATUS_CANCELLED,"cancel budget control");xrtFutureDestroy(F);}
        else {Failed=true;testRequire(A==XMQTT_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"snapshot OOM preserves Future output");}
        xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"leave snapshot allocation budget");xallocationbudgetinfo B={0};B.Size=sizeof(B);double End=xrtTimer()+8;
        do{testRequire(xrtAllocationBudgetInfo(Budget,&B) && xrtTimer()<End,"all snapshot failure/control allocations retire");if(B.LiveCount)xrtSleep(1);}while(B.LiveCount);
        xmqttclientinfo CI;xmqttClientInfo(C,&CI);testRequire(!B.LiveBytes && !CI.Requests && !CI.OutgoingBytes && !csInfo(S).Objects && !csInfo(S).ActiveOutgoing && !csInfo(S).HasState,"OOM refunds both Client and Session reservations without protocol state");xrtAllocationBudgetFree(Budget);
    }
    testRequire(Failed && Passed,"snapshot OOM prefixes and successful controls");csClose(C);mqttResumeStop(&P);xrtFutureDestroy(Ready);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"snapshot OOM cleanup");
}
typedef struct csSubmitThread {xmqttclient* Client;xatomic32* Start;} csSubmitThread;
static int32 csConcurrentSnapshotThread(ptr Data)
{
    csSubmitThread* T=Data;while(!xrtAtomic32Load(T->Start,XMEMORY_ACQUIRE))xrtThreadYield();
    for(unsigned N=0;N<16;++N){unsigned char Payload[257];memset(Payload,(int)N,sizeof(Payload));xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("concurrent");Q.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};xfuture* F=NULL;
        if(xmqttClientPublishAsync(T->Client,&Q,8000,&F)!=XMQTT_ADMISSION_ACCEPTED || !xrtFutureCancel(F))return 1;
        const xmqttwriteresult* R=csResult(F);if(R->Status!=XMQTT_STATUS_CANCELLED || R->AcceptedBytes)return 2;xrtFutureDestroy(F);}
    return 0;
}
static void csConcurrentSnapshots(xmqttversion V)
{
    xmqttsession* S;xmqttclientinbox* I;csPair(V,&S,&I);mqttResumePeer P;mqttResumeStart(&P,V,MQTT_RESUME_HOLD_READY,false);xfuture* Ready;xmqttclient* C=csConnect(&P,S,I,&Ready,NULL);mqttResumeWait(&P.Connects,1);
    xatomic32 Start;xrtAtomic32Init(&Start,0);csSubmitThread T={C,&Start};xthread* Threads[4];for(unsigned N=0;N<4;++N){Threads[N]=xrtThreadCreate(csConcurrentSnapshotThread,&T,0);testRequire(Threads[N]!=NULL,"concurrent snapshot thread");}xrtAtomic32Store(&Start,1,XMEMORY_RELEASE);
    for(unsigned N=0;N<4;++N){testRequire(xrtThreadWaitFor(Threads[N],8000)==XWAIT_OK && !xrtThreadExitCode(Threads[N]),"concurrent snapshots join");xrtThreadDestroy(Threads[N]);}
    double End=xrtTimer()+8;while(csInfo(S).Objects){testRequire(xrtTimer()<End,"concurrent snapshot retirement");xrtSleep(1);}testRequire(!csInfo(S).HasState && !csInfo(S).ActiveOutgoing,"concurrent cancelled snapshots never become protocol obligations");
    csClose(C);mqttResumeStop(&P);xrtFutureDestroy(Ready);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"concurrent snapshots cleanup");
}
static void csAliasRollbackBudget(void)
{
    const size_t Limits[]={1,192,256,512,1024,4096,16384,65536};bool Failed=false,Passed=false;
    for(unsigned N=0;N<sizeof(Limits)/sizeof(*Limits);++N){
        xmqttsession* S;xmqttclientinbox* I;csPair(XMQTT_VERSION_5,&S,&I);mqttResumePeer P;mqttResumeCaps Caps={0};Caps.MaximumQos=2;Caps.AliasMaximum=1;mqttResumeStartWith(&P,XMQTT_VERSION_5,MQTT_RESUME_HOLD_READY|MQTT_RESUME_HOLD_ACK,false,&Caps);xfuture* Ready;xmqttclient* C=csConnect(&P,S,I,&Ready,NULL);mqttResumeWait(&P.Connects,1);
        xmqttproperty Alias=mqttResumeNumber(XMQTT_PROPERTY_TOPIC_ALIAS,1);xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("A");Q.Properties=(xmqttpropertylist){&Alias,1};xfuture* Warm=NULL;
        testRequire(xmqttClientPublishAsync(C,&Q,8000,&Warm)==XMQTT_ADMISSION_ACCEPTED && xrtFutureCancel(Warm) && ((const xmqttwriteresult*)csResult(Warm))->Status==XMQTT_STATUS_CANCELLED,"warm admitted alias A, then cancel without reverting planned binding");xrtFutureDestroy(Warm);double End=xrtTimer()+8;while(csInfo(S).Objects){testRequire(xrtTimer()<End,"warm alias retirement");xrtSleep(1);}
        xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[N],&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"alias transaction budget");Q.Body.Publish.Topic=XRT_STR_LITERAL("B");unsigned char Payload[257];memset(Payload,'Z',sizeof(Payload));Q.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};xfuture* F=(xfuture*)(uintptr_t)1;xmqttclientadmission A=xmqttClientPublishAsync(C,&Q,8000,&F);xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"leave alias transaction budget");
        bool Accepted=A==XMQTT_ADMISSION_ACCEPTED;if(Accepted){Passed=true;testRequire(xrtFutureCancel(F) && ((const xmqttwriteresult*)csResult(F))->Status==XMQTT_STATUS_CANCELLED,"cancel accepted B binding");xrtFutureDestroy(F);}else {Failed=true;testRequire(A==XMQTT_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"failed B transaction leaves Future output unchanged");}
        Q.Body.Publish.Topic=(xstrview){0};xfuture* Next=NULL;testRequire(xmqttClientPublishAsync(C,&Q,8000,&Next)==XMQTT_ADMISSION_ACCEPTED,"subsequent empty alias after failed/cancelled transaction");mqttResumeConnack(&P);csReady(Ready);mqttResumeWait(&P.Publishes,1);
        testRequire(P.Log[0].LogicalFirst==(Accepted?'B':'A') && P.Log[0].Topic==1 && P.Log[0].First=='Z',"only successful admission advances planned binding; cancelled binding is restored in actual output");mqttResumeAck(&P,XMQTT_PACKET_PUBACK,P.Log[0].Id);testRequire(((const xmqttwriteresult*)csResult(Next))->Status==XMQTT_STATUS_OK,"empty alias completion");csDone(S,C);csClose(C);mqttResumeStop(&P);xrtFutureDestroy(Ready);xrtFutureDestroy(Next);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"alias rollback owner cleanup");xrtClearError();
        xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);testRequire(xrtAllocationBudgetInfo(Budget,&Info) && !Info.LiveCount && !Info.LiveBytes,"failed/successful alias transaction allocations fully retire including planned topic cache");xrtAllocationBudgetFree(Budget);
    }testRequire(Failed && Passed,"alias failure prefixes and admitted controls");
}
typedef struct csAliasProducer {xmqttclient* Client;xatomic32* Start;unsigned Index;} csAliasProducer;
static int32 csAliasProducerThread(ptr Data)
{
    csAliasProducer* T=Data;while(!xrtAtomic32Load(T->Start,XMEMORY_ACQUIRE))xrtThreadYield();
    for(unsigned N=0;N<16;++N){xmqttproperty Alias=mqttResumeNumber(XMQTT_PROPERTY_TOPIC_ALIAS,1);xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=(N&1u)?0:2;Q.Properties=(xmqttpropertylist){&Alias,1};Q.Body.Publish.Topic=(T->Index&1u)?XRT_STR_LITERAL("A"):XRT_STR_LITERAL("B");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("concurrent alias");xfuture* F=NULL;
        if(xmqttClientPublishAsync(T->Client,&Q,8000,&F)!=XMQTT_ADMISSION_ACCEPTED || !xrtFutureCancel(F) || ((const xmqttwriteresult*)csResult(F))->Status!=XMQTT_STATUS_CANCELLED)return 1;
        xrtFutureDestroy(F);}
    return 0;
}
static void csConcurrentAliases(void)
{
    xmqttsession* S;xmqttclientinbox* I;csPair(XMQTT_VERSION_5,&S,&I);mqttResumePeer P;mqttResumeStart(&P,XMQTT_VERSION_5,MQTT_RESUME_HOLD_READY,false);xfuture* Ready;xmqttclient* C=csConnect(&P,S,I,&Ready,NULL);mqttResumeWait(&P.Connects,1);
    xatomic32 Start;xrtAtomic32Init(&Start,0);csAliasProducer T[4];xthread* Threads[4];for(unsigned N=0;N<4;++N){T[N]=(csAliasProducer){C,&Start,N};Threads[N]=xrtThreadCreate(csAliasProducerThread,T+N,0);testRequire(Threads[N]!=NULL,"concurrent alias producer");}xrtAtomic32Store(&Start,1,XMEMORY_RELEASE);
    for(unsigned N=0;N<4;++N){testRequire(xrtThreadWaitFor(Threads[N],8000)==XWAIT_OK && !xrtThreadExitCode(Threads[N]),"concurrent alias join");xrtThreadDestroy(Threads[N]);}
    csClose(C);mqttResumeStop(&P);xrtFutureDestroy(Ready);testRequire(!csInfo(S).Objects && !csInfo(S).HasState && xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"all cancelled alias producers physically retire without protocol state");
}
#if defined(XMQTT_CLIENT_SESSION_SINGLE_TEST)
typedef struct csAliasGateThread {xmqttclient* Client;bool Zero;xatomic32 Done;xmqttclientadmission Admission;xfuture* Future;} csAliasGateThread;
static int32 csAliasGateCall(ptr Data)
{
    csAliasGateThread* T=Data;xmqttproperty Alias=mqttResumeNumber(XMQTT_PROPERTY_TOPIC_ALIAS,1);xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=T->Zero?0:2;Q.Properties=(xmqttpropertylist){&Alias,1};Q.Body.Publish.Topic=T->Zero?XRT_STR_LITERAL("B"):XRT_STR_LITERAL("A");unsigned char Payload[257];memset(Payload,'Z',sizeof(Payload));if(!T->Zero)Q.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};
    T->Future=(xfuture*)(uintptr_t)1;T->Admission=xmqttClientPublishAsync(T->Client,&Q,8000,&T->Future);xrtAtomic32Store(&T->Done,1,XMEMORY_RELEASE);return 0;
}
static void csAliasGateClose(void)
{
    xmqttsession* S;xmqttclientinbox* I;csPair(XMQTT_VERSION_5,&S,&I);mqttResumePeer P;mqttResumeStart(&P,XMQTT_VERSION_5,MQTT_RESUME_HOLD_READY,false);xfuture* Ready;xmqttclient* C=csConnect(&P,S,I,&Ready,NULL);mqttResumeWait(&P.Connects,1);
    xrtAtomic32Store(&csProbeCopyEntered,0,XMEMORY_RELAXED);xrtAtomic32Store(&csProbeCopyRelease,0,XMEMORY_RELAXED);xrtAtomic32Store(&csProbeCopyBlock,1,XMEMORY_RELAXED);xrtAtomic64Store(&csProbeCopySize,257,XMEMORY_RELEASE);
    csAliasGateThread A={0},B={0};A.Client=B.Client=C;B.Zero=true;xrtAtomic32Init(&A.Done,0);xrtAtomic32Init(&B.Done,0);xthread* First=xrtThreadCreate(csAliasGateCall,&A,0);testRequire(First!=NULL,"first alias transaction thread");mqttResumeWait(&csProbeCopyEntered,1);
    xthread* Second=xrtThreadCreate(csAliasGateCall,&B,0);testRequire(Second!=NULL,"second alias transaction thread");double End=xrtTimer()+8;xmqttclientinfo CI;
    do{xmqttClientInfo(C,&CI);testRequire(xrtTimer()<End,"second alias reserves before waiting");if(CI.Requests<2)xrtSleep(1);}while(CI.Requests<2);
    testRequire(!xrtAtomic32Load(&B.Done,XMEMORY_ACQUIRE) && csInfo(S).Objects==1,"QoS0 binding cannot pass an in-progress alias snapshot");
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("independent");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x");xfuture* Other=NULL;testRequire(xmqttClientPublishAsync(C,&Q,8000,&Other)==XMQTT_ADMISSION_ACCEPTED,"ordinary producer progresses outside alias transaction gate");
    testRequire(xmqttClientClose(C),"close while alias holder and waiter exist");xrtAtomic32Store(&csProbeCopyRelease,1,XMEMORY_RELEASE);
    testRequire(xrtThreadWaitFor(First,8000)==XWAIT_OK && xrtThreadWaitFor(Second,8000)==XWAIT_OK && !xrtThreadExitCode(First) && !xrtThreadExitCode(Second),"alias holder/waiter join");xrtThreadDestroy(First);xrtThreadDestroy(Second);xrtAtomic64Store(&csProbeCopySize,0,XMEMORY_RELEASE);xrtAtomic32Store(&csProbeCopyBlock,0,XMEMORY_RELEASE);
    testRequire(A.Admission==XMQTT_ADMISSION_CLOSED && B.Admission==XMQTT_ADMISSION_CLOSED && A.Future==(xfuture*)(uintptr_t)1 && B.Future==(xfuture*)(uintptr_t)1,"close refunds both unpublished alias transactions without exposing Futures");csClose(C);mqttResumeStop(&P);xrtFutureDestroy(Ready);xrtFutureDestroy(Other);
    testRequire(!csInfo(S).Objects && !csInfo(S).HasState && xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"close frees pending alias and independent snapshots and transaction mutex");
}
static void csAliasVectors(unsigned Qos,unsigned Properties,bool Bytes,bool Limited)
{
    xmqttsession* S;xmqttclientinbox* I;csPair(XMQTT_VERSION_5,&S,&I);mqttResumePeer P;mqttResumeCaps Caps={0};Caps.MaximumQos=2;Caps.AliasMaximum=1;
    mqttResumeStartWith(&P,XMQTT_VERSION_5,Qos==1?MQTT_RESUME_HOLD_ACK:MQTT_RESUME_HOLD_REC,false,&Caps);xmqttclientconfig K={0};xmqttClientConfigInit(&K);if(Bytes)K.WriteBytes=K.WriteChunkBytes=1;xfuture* Ready;xmqttclient* C=csConnect(&P,S,I,&Ready,&K);csReady(Ready);
    xmqttproperty Props[3];Props[0]=mqttResumeNumber(XMQTT_PROPERTY_TOPIC_ALIAS,1);xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Body.Publish.Topic=XRT_STR_LITERAL("A");Q.Properties=(xmqttpropertylist){Props,1};xfuture* Bind=NULL;
    testRequire(xmqttClientPublish(C,&Q,8000,&Bind)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)csResult(Bind))->Status==XMQTT_STATUS_OK,"bind alias with QoS0 in same transaction domain");mqttResumeWait(&P.Publishes,1);xrtFutureDestroy(Bind);
    char Long[119];memset(Long,'u',sizeof(Long));uint64 Expected=0;unsigned Users=0;
    if(Properties){Props[0]=(xmqttproperty){0};Props[0].Id=XMQTT_PROPERTY_USER;Props[0].Type=XMQTT_PROPERTY_STRING_PAIR;Props[0].Value.Pair.Name=XRT_STR_LITERAL("x");Props[0].Value.Pair.Value=Properties==1?(xstrview){Long,sizeof(Long)}:XRT_STR_LITERAL("first");Props[1]=mqttResumeNumber(XMQTT_PROPERTY_TOPIC_ALIAS,1);Q.Properties=(xmqttpropertylist){Props,2};
        if(Properties==2){Props[2]=Props[0];Props[2].Value.Pair.Value=XRT_STR_LITERAL("second");Q.Properties.Count=3;}
        for(size_t N=0;N<Q.Properties.Count;++N)if(Props[N].Id==XMQTT_PROPERTY_USER){++Users;xstrview V[]={Props[N].Value.Pair.Name,Props[N].Value.Pair.Value};for(unsigned J=0;J<2;++J){for(size_t A=0;A<V[J].Size;++A)Expected=Expected*131u+(unsigned char)V[J].Data[A];Expected*=131u;}}
    }
    unsigned char Payload[257];memset(Payload,'Z',sizeof(Payload));Q.Flags=(uint8)(Qos<<1);Q.Body.Publish.Topic=(xstrview){0};Q.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};xfuture *First=NULL,*Second=NULL;
    xrtAtomic32Store(&csProbeCopies,0,XMEMORY_RELAXED);xrtAtomic64Store(&csProbeCopySize,sizeof(Payload),XMEMORY_RELEASE);xrtAtomic32Store(&csProbeVectorRejected,0,XMEMORY_RELAXED);xrtAtomic32Store(&csProbeVectorLimit,Limited?1:0,XMEMORY_RELEASE);
    testRequire(xmqttClientPublishAsync(C,&Q,8000,&First)==XMQTT_ADMISSION_ACCEPTED,"compact shared alias");mqttResumeWait(&P.Publishes,2);
    testRequire(!P.Log[1].Topic && P.Log[1].LogicalFirst=='A' && P.Log[1].Users==Users && P.Log[1].UserHash==Expected,"compact header/property-length transition and ordered user properties");
    Q.Flags=0;Q.Body.Publish.Topic=XRT_STR_LITERAL("B");Q.Body.Publish.Payload=(xbytesview){0};Bind=NULL;testRequire(xmqttClientPublish(C,&Q,8000,&Bind)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)csResult(Bind))->Status==XMQTT_STATUS_OK,"intervening QoS0 rebind");mqttResumeWait(&P.Publishes,3);xrtFutureDestroy(Bind);
    Q.Flags=(uint8)(Qos<<1);Q.Body.Publish.Topic=XRT_STR_LITERAL("A");Q.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};testRequire(xmqttClientPublishAsync(C,&Q,8000,&Second)==XMQTT_ADMISSION_ACCEPTED,"full shared alias gather");memset(Payload,'X',sizeof(Payload));mqttResumeWait(&P.Publishes,4);
    testRequire(P.Log[3].Topic==1 && P.Log[3].LogicalFirst=='A' && P.Log[3].First=='Z' && P.Log[3].Users==Users && P.Log[3].UserHash==Expected && xrtAtomic32Load(&csProbeCopies,XMEMORY_ACQUIRE)==2,"full gather owns one copy per publication and preserves properties");
    if(Limited && !Bytes)testRequire(xrtAtomic32Load(&csProbeVectorRejected,XMEMORY_ACQUIRE)>0,"one-lease fallback actually exercised");
    xrtAtomic64Store(&csProbeCopySize,0,XMEMORY_RELEASE);xrtAtomic32Store(&csProbeVectorLimit,0,XMEMORY_RELEASE);
    csClose(C);mqttResumeStop(&P);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"alias gather owners retired");xrtFutureDestroy(First);xrtFutureDestroy(Second);xrtFutureDestroy(Ready);
}
typedef struct csCopyClose {xmqttclient* Client;xmqttclientadmission Admission;xfuture* Future;} csCopyClose;
static int32 csCopyCloseThread(ptr Data)
{
    csCopyClose* T=Data;unsigned char Payload[257];memset(Payload,'Z',sizeof(Payload));xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("copy/close");Q.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};
    T->Future=(xfuture*)(uintptr_t)1;T->Admission=xmqttClientPublishAsync(T->Client,&Q,8000,&T->Future);return 0;
}
static void csSnapshotCloseRace(xmqttversion V)
{
    xmqttsession* S;xmqttclientinbox* I;csPair(V,&S,&I);mqttResumePeer P;mqttResumeStart(&P,V,0,false);xfuture* Ready;xmqttclient* C=csConnect(&P,S,I,&Ready,NULL);csReady(Ready);
    xrtAtomic32Store(&csProbeCopyEntered,0,XMEMORY_RELAXED);xrtAtomic32Store(&csProbeCopyRelease,0,XMEMORY_RELAXED);xrtAtomic32Store(&csProbeCopyBlock,1,XMEMORY_RELAXED);xrtAtomic64Store(&csProbeCopySize,257,XMEMORY_RELEASE);
    csCopyClose T={C,0,NULL};xthread* Thread=xrtThreadCreate(csCopyCloseThread,&T,0);testRequire(Thread!=NULL,"copy/close thread");mqttResumeWait(&csProbeCopyEntered,1);
    xmqttclientinfo CI;testRequire(xmqttClientInfo(C,&CI) && CI.Requests==1 && csInfo(S).Objects==1 && !csInfo(S).ActiveOutgoing,"payload copy holds neither Client nor Session gate, but keeps physical reservation");testRequire(xmqttClientClose(C),"close during snapshot copy");
    xrtAtomic32Store(&csProbeCopyRelease,1,XMEMORY_RELEASE);testRequire(xrtThreadWaitFor(Thread,8000)==XWAIT_OK && !xrtThreadExitCode(Thread),"copy/close thread joined");xrtThreadDestroy(Thread);xrtAtomic64Store(&csProbeCopySize,0,XMEMORY_RELEASE);xrtAtomic32Store(&csProbeCopyBlock,0,XMEMORY_RELEASE);
    testRequire(T.Admission==XMQTT_ADMISSION_CLOSED && T.Future==(xfuture*)(uintptr_t)1,"close wins publication after snapshot without exposing a Future");csClose(C);mqttResumeStop(&P);
    testRequire(!csInfo(S).Objects && !csInfo(S).ActiveOutgoing && !xrtAtomic32Load(&P.Publishes,XMEMORY_ACQUIRE),"closed in-progress snapshot refunds physical id/bytes and sends nothing");xrtFutureDestroy(Ready);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"copy/close cleanup");
}
static void csSharedCopy(xmqttversion V,unsigned Qos,bool Alias)
{
    xmqttsession* S;xmqttclientinbox* I;csPair(V,&S,&I);mqttResumePeer P;mqttResumeCaps Caps={0};Caps.MaximumQos=2;Caps.AliasMaximum=Alias?1:0;
    mqttResumeStartWith(&P,V,Qos==1?MQTT_RESUME_HOLD_ACK:MQTT_RESUME_HOLD_REC,false,&Caps);xfuture* Ready;xmqttclient* C=csConnect(&P,S,I,&Ready,NULL);csReady(Ready);
    unsigned char Payload[257];memset(Payload,'Z',sizeof(Payload));xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=(uint8)(Qos<<1);Q.Body.Publish.Topic=XRT_STR_LITERAL("shared/snapshot");Q.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};
    xmqttproperty A=mqttResumeNumber(XMQTT_PROPERTY_TOPIC_ALIAS,1);if(Alias)Q.Properties=(xmqttpropertylist){&A,1};
    xrtAtomic32Store(&csProbeCopies,0,XMEMORY_RELAXED);xrtAtomic64Store(&csProbeCopySize,sizeof(Payload),XMEMORY_RELEASE);xfuture* F=NULL;
    testRequire(xmqttClientPublishAsync(C,&Q,8000,&F)==XMQTT_ADMISSION_ACCEPTED,"shared snapshot admission");memset(Payload,'X',sizeof(Payload));mqttResumeWait(&P.Publishes,1);
    testRequire(P.Log[0].Payload==sizeof(Payload) && P.Log[0].First=='Z',"publication owns the caller snapshot before returning");
    testRequire(xrtAtomic32Load(&csProbeCopies,XMEMORY_ACQUIRE)==1,"all Session publications copy payload once, including aliases");xrtAtomic64Store(&csProbeCopySize,0,XMEMORY_RELEASE);
    csClose(C);mqttResumeStop(&P);testRequire(csInfo(S).ActiveOutgoing==1,"shared payload outlives old request and Engine");testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"shared snapshot owners retired");xrtFutureDestroy(F);xrtFutureDestroy(Ready);
}
static void csReplayAllocation(xmqttversion V,unsigned Successful)
{
    xmqttsession* S;xmqttclientinbox* I;csPair(V,&S,&I);mqttResumePeer P;mqttResumeStart(&P,V,MQTT_RESUME_HOLD_ACK,false);xfuture *Ready,*Next,*F[4];xmqttclient* C=csConnect(&P,S,I,&Ready,NULL);csReady(Ready);
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("allocation");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("replay");
    for(unsigned N=0;N<4;++N){Q.Identifier=(uint16)(100+N);testRequire(xmqttClientPublishAsync(C,&Q,8000,F+N)==XMQTT_ADMISSION_ACCEPTED,"old allocation flow");}mqttResumeWait(&P.Publishes,4);csClose(C);mqttResumeStop(&P);testRequire(csInfo(S).ActiveOutgoing==4,"four original obligations");
    mqttResumeStart(&P,V,MQTT_RESUME_HOLD_READY,true);C=csConnect(&P,S,I,&Next,NULL);mqttResumeWait(&P.Connects,1);csProbeFailAllocation(sizeof(xmqttrequest)+xmqttSessionRequestExtra(0,false,false,true),Successful);mqttResumeConnack(&P);const xmqttreadyresult* R=csResult(Next);
    if(Successful<4){testRequire(R->Status==XMQTT_STATUS_MEMORY && xrtAtomic32Load(&csProbeAllocFailures,XMEMORY_ACQUIRE)==1,"controlled replay header OOM is typed");csClose(C);mqttResumeStop(&P);testRequire(csInfo(S).ActiveOutgoing==4 && csInfo(S).Objects==4 && !xrtAtomic32Load(&P.Publishes,XMEMORY_ACQUIRE),"partial replay construction refunds wrappers and preserves all unattempted obligations");csProbeFailAllocation(0,0);xrtFutureDestroy(Next);
        mqttResumeStart(&P,V,0,true);C=csConnect(&P,S,I,&Next,NULL);csReady(Next);
    }else {testRequire(R->Status==XMQTT_STATUS_OK && !xrtAtomic32Load(&csProbeAllocFailures,XMEMORY_ACQUIRE),"replay allocation success control");csProbeFailAllocation(0,0);}
    csDone(S,C);mqttResumeWait(&P.Publishes,4);csClose(C);mqttResumeStop(&P);for(unsigned N=0;N<4;++N)xrtFutureDestroy(F[N]);xrtFutureDestroy(Ready);xrtFutureDestroy(Next);testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"replay OOM cleanup");
}
#endif
#endif
