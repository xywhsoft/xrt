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
#if defined(XMQTT_CLIENT_SESSION_SINGLE_TEST)
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
    testRequire(xrtAtomic32Load(&csProbeCopies,XMEMORY_ACQUIRE)==(Alias?2u:1u),"unaliased publish copies payload once; alias fallback remains measured twice");xrtAtomic64Store(&csProbeCopySize,0,XMEMORY_RELEASE);
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
