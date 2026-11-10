#ifndef XMQTT_CLIENT_SESSION_DIAL_CASES_H
#define XMQTT_CLIENT_SESSION_DIAL_CASES_H
static xmqttsessioninfo sdSeed(xmqttsession* S,xmqttversion V)
{
    xmqttproperty Props[2];xmqttpacket Q=sdConnectPacket(V,false,Props),A;xmqttPacketInit(XMQTT_PACKET_CONNACK,&A);xmqttnegotiated N;
    testRequire(xmqttSessionOpen(S,&Q,&A,xrtTimer(),&N)==XMQTT_SESSION_DONE,"seed previously verified offline Session");
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Identifier=23;Q.Body.Publish.Topic=XRT_STR_LITERAL("old/state");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("retained");const xmqttsessionpacket* P=NULL;xmqttsessionframe F;
    testRequire(xmqttSessionPublish(S,&Q,&P)==XMQTT_SESSION_DONE && xmqttSessionNext(S,&F)==XMQTT_SESSION_DONE && xmqttSessionSent(S,&F)==XMQTT_SESSION_DONE,"seed sent QoS1 obligation");xmqttSessionPacketRelease(F.Packet);xmqttSessionPacketRelease(P);testRequire(xmqttSessionEnd(S,NULL,xrtTimer())==XMQTT_SESSION_DONE,"seed actual prior termination");return sdInfo(S);
}
static void sdPreserved(xmqttsession* S,xmqttsessioninfo Before)
{xmqttsessioninfo I=sdInfo(S);testRequire(!I.Connected && I.Generation==Before.Generation && I.ActiveOutgoing==Before.ActiveOutgoing && I.Objects==Before.Objects && I.LiveBytes==Before.LiveBytes && I.Incoming==Before.Incoming && I.Prepared==Before.Prepared && I.ExpiresAt==Before.ExpiresAt && I.HasState==Before.HasState,"failed setup/proof preserves old physical/protocol state and expiry");}
static void sdRetire(xmqttsession* S,xmqttclientinbox* I)
{testRequire(xmqttSessionReset(S)==XMQTT_SESSION_DONE && !sdInfo(S).Objects && xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"offline ownership/allocations refunded");}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
static void sdProofFailure(unsigned Factory,unsigned Action)
{
    xmqttsession* S;xmqttclientinbox* I;sdPair(XMQTT_VERSION_5,&S,&I);xmqttsessioninfo Before=sdSeed(S,XMQTT_VERSION_5);mqttSessionDialPeer P;
    mqttSessionDialStart(&P,XMQTT_VERSION_5,0,Action>1,true,true,0);xmqttclientconfig K;xmqttClientConfigInit(&K);K.SetupTimeout=10000;xmqttauthconfig Auth;xmqttAuthConfigInit(&Auth);Auth.ResponseTimeout=Action==1?250:5000;
    xmqttproperty Props[2];xmqttpacket Q=sdConnectPacket(XMQTT_VERSION_5,true,Props);xmqttclient* C=NULL;testRequire(sdFactory(&P,Factory,&K,&Auth,S,I,&Q,&C),"old Session authentication attempt");xfuture* F=xmqttClientReady(C);xmqttauthticket* Held=NULL;double End=xrtTimer()+10;
    while(!Held){xmqttauthticket* T=NULL;if(xmqttClientTakeAuth(C,&T)){
            if(xmqttAuthPhase(T)==XMQTT_AUTH_CHALLENGE){xmqttproperty A[2]={{0}};A[0].Id=XMQTT_PROPERTY_AUTH_METHOD;A[0].Type=XMQTT_PROPERTY_STRING;A[0].Value.Text=XRT_STR_LITERAL("m");A[1].Id=XMQTT_PROPERTY_AUTH_DATA;A[1].Type=XMQTT_PROPERTY_BINARY;A[1].Value.Binary=XRT_BYTES_LITERAL("D");xmqttPacketInit(XMQTT_PACKET_AUTH,&Q);Q.Reason=0x18;Q.Properties=(xmqttpropertylist){A,2};testRequire(xmqttAuthRespond(T,&Q)==XMQTT_ADMISSION_ACCEPTED,"failure case challenge response");xmqttAuthRelease(T);}
            else Held=T;
        }else {testRequire(!xrtFutureDone(F) && xrtTimer()<End,"final proof arrives before decision timeout");xrtSleep(1);}}
    sdPreserved(S,Before);xmqttmessage* M=(xmqttmessage*)(uintptr_t)1;testRequire(!xmqttClientInboxTake(I,&M) && M==(xmqttmessage*)(uintptr_t)1 && !xrtAtomic32Load(&P.Peer.Publishes,XMEMORY_ACQUIRE) && !xrtAtomic32Load(&P.Peer.Pubrecs,XMEMORY_ACQUIRE),"unverified reset/replay/receive is gated");
    if(Action==0)testRequire(xmqttAuthComplete(Held,false)==XMQTT_ADMISSION_ACCEPTED,"reject final proof");
    if(Action==2)testRequire(xrtFutureCancel(F),"cancel final proof Ready");
    if(Action==3)testRequire(xmqttClientClose(C),"close during final proof");
    const xmqttreadyresult* R=sdResult(F);testRequire(R->Status==(Action==0?XMQTT_STATUS_AUTHENTICATION:Action==1?XMQTT_STATUS_TIMEOUT:Action==2?XMQTT_STATUS_CANCELLED:XMQTT_STATUS_CLOSED),"typed proof failure outcome");
    testRequire(xmqttClientWaitFor(C,0)==XWAIT_TIMEOUT && !xmqttSessionDestroy(S),"held auth ticket pins actual Client/passive claim");xrtClearError();xmqttAuthRelease(Held);sdClose(C);sdPreserved(S,Before);mqttSessionDialStop(&P);sdRetire(S,I);testRequire(R->Status!=XMQTT_STATUS_OK,"held Ready failure outlives owners");xrtFutureDestroy(F);
}
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL)
static void sdSetupFailure(bool Auth,unsigned Action)
{
    xmqttsession* S;xmqttclientinbox* I;sdPair(XMQTT_VERSION_5,&S,&I);xmqttsessioninfo Before=sdSeed(S,XMQTT_VERSION_5);mqttSessionDialPeer P;
    mqttSessionDialStart(&P,XMQTT_VERSION_5,0,true,Auth,false,Action==0?MQTT_SESSION_DNS_MISSING:Action>1?MQTT_SESSION_DNS_BLOCK:0);
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.SetupTimeout=Action==2?50:10000;xmqttproperty Props[2];xmqttpacket Q=sdConnectPacket(XMQTT_VERSION_5,Auth,Props);xmqttclient* C=NULL;bool OK;
    if(Action==1){xnetaddrlist* L=xrtNetAddrListCreate(&P.RefusedAddress,1);testRequire(L!=NULL,"refused Session endpoints");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
        if(Auth)OK=xmqttClientDialAddressesSessionAuth(P.Peer.Engine,L,1,&K,NULL,NULL,S,I,&Q,&C);else
#endif
        {OK=xmqttClientDialAddressesSession(P.Peer.Engine,L,1,&K,NULL,S,I,&Q,&C);}
        xrtNetAddrListDestroy(L);
    }else OK=sdFactory(&P,1,&K,NULL,S,I,&Q,&C);
    testRequire(OK,"asynchronous Session Dial failure admission");xfuture* F=xmqttClientReady(C);
    if(Action>1){mqttResumeWait(&P.Entered,1);if(Action==3)testRequire(xrtFutureCancel(F),"cancel blocked Session DNS");if(Action==4)testRequire(xmqttClientClose(C),"close blocked Session DNS");}
    const xmqttreadyresult* R=sdResult(F);testRequire(R->Status==(Action<2?XMQTT_STATUS_TRANSPORT:Action==2?XMQTT_STATUS_TIMEOUT:Action==3?XMQTT_STATUS_CANCELLED:XMQTT_STATUS_CLOSED),"typed Session DNS/connect failure");
    sdClose(C);sdPreserved(S,Before);
    if(Action>1)testRequire(!xrtAtomic32Load(&P.Exited,XMEMORY_ACQUIRE) && xrtNetResolverTryDestroy(P.Resolver)==XNET_RETIRE_BUSY,"shared lookup outlives Client and its passive claims");
    sdRetire(S,I);mqttSessionDialStop(&P);testRequire(R->Status!=XMQTT_STATUS_OK,"failure result survives Resolver/Engine/passive owners");xrtFutureDestroy(F);xrtClearError();
}
#endif
static void sdConstructors(bool Auth,unsigned Factory)
{
    xmqttsession* S;xmqttclientinbox* I;sdPair(XMQTT_VERSION_5,&S,&I);mqttSessionDialPeer P;mqttSessionDialStart(&P,XMQTT_VERSION_5,MQTT_RESUME_HOLD_READY,false,Auth,false,0);
    xmqttproperty Props[2];xmqttpacket Q=sdConnectPacket(XMQTT_VERSION_5,Auth,Props);xmqttclientconfig K;xmqttclient* Out=(xmqttclient*)(uintptr_t)1;
    for(unsigned Case=0;Case<6;++Case){xmqttClientConfigInit(&K);xmqttpacket Saved=Q;
        if(Case==2)K.MaxRequests=0;
        if(Case==3)K.Version=XMQTT_VERSION_311;
        if(Case==4)Q.Body.Connect.ClientId=XRT_STR_LITERAL("wrong");
        if(Case==5)K.MaxPrivateBytes=1;
        testRequire(!sdFactory(&P,Factory,&K,NULL,Case==0?NULL:S,Case==1?NULL:I,&Q,&Out) && Out==(xmqttclient*)(uintptr_t)1,"invalid Session/Dial/Auth constructor changes no output");Q=Saved;xrtClearError();}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(Auth){xmqttauthconfig A;xmqttAuthConfigInit(&A);A.Size=0;xmqttClientConfigInit(&K);testRequire(!sdFactory(&P,Factory,&K,&A,S,I,&Q,&Out) && Out==(xmqttclient*)(uintptr_t)1,"invalid Auth config no output/claim");xrtClearError();Q.Properties.Count=1;testRequire(!sdFactory(&P,Factory,&K,NULL,S,I,&Q,&Out),"missing enhanced method rejected");Q.Properties.Count=2;xrtClearError();}
#endif
    mqttSessionDialStop(&P);sdRetire(S,I);
}
static void sdAllocation(bool Auth,unsigned Factory)
{
    static const size_t Limits[]={1,512,4096,16384,65536,262144,1048576};unsigned Failed=0,Passed=0;
    for(size_t At=0;At<sizeof(Limits)/sizeof(*Limits);++At){xmqttsession* S;xmqttclientinbox* I;sdPair(XMQTT_VERSION_5,&S,&I);mqttSessionDialPeer P;mqttSessionDialStart(&P,XMQTT_VERSION_5,0,false,Auth,false,0);xmqttproperty Props[2];xmqttpacket Q=sdConnectPacket(XMQTT_VERSION_5,Auth,Props);xmqttclient* C=(xmqttclient*)(uintptr_t)1;xallocationbudget* B=NULL;xallocationscope Scope={0};
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL)
        xnetaddrlist* L=Factory==2?xrtNetAddrListCreate(&P.Peer.Address,1):NULL;if(Factory==2)testRequire(L!=NULL,"OOM input list prepared outside caller scope");
#endif
        testRequire(xrtAllocationBudgetCreate(Limits[At],&B) && xrtAllocationBudgetEnter(B,&Scope),"Session Dial constructor allocation prefix");bool OK;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL)
        if(Factory==2){
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
            if(Auth)OK=xmqttClientDialAddressesSessionAuth(P.Peer.Engine,L,1,NULL,NULL,NULL,S,I,&Q,&C);else
#endif
            OK=xmqttClientDialAddressesSession(P.Peer.Engine,L,1,NULL,NULL,S,I,&Q,&C);
        }else {
#endif
        OK=sdFactory(&P,Factory,NULL,NULL,S,I,&Q,&C);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL)
        }
#endif
        testRequire(xrtAllocationBudgetLeave(&Scope),"Session Dial leave allocation prefix");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL)
        xrtNetAddrListDestroy(L);
#endif
        if(OK){++Passed;xfuture* F=xmqttClientReady(C);sdDecide(C,F,S,Auth,true,false);(void)sdResult(F);sdClose(C);xrtFutureDestroy(F);}else {++Failed;testRequire(C==(xmqttclient*)(uintptr_t)1,"constructor OOM output unchanged");}
        mqttSessionDialStop(&P);sdRetire(S,I);xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);testRequire(xrtAllocationBudgetInfo(B,&Info) && !Info.LiveBytes && !Info.LiveCount,"constructor budget/claims physically refunded");xrtAllocationBudgetFree(B);xrtClearError();}
    testRequire(Failed && Passed,"constructor allocation test observes refusals and success");
}
static void sdClaims(bool Auth,unsigned Factory)
{
    xmqttsession *S,*Other;xmqttclientinbox *I,*OtherInbox;sdPair(XMQTT_VERSION_5,&S,&I);sdPair(XMQTT_VERSION_5,&Other,&OtherInbox);mqttSessionDialPeer P;
    mqttSessionDialStart(&P,XMQTT_VERSION_5,MQTT_RESUME_HOLD_READY,false,Auth,false,0);xfuture* F;xmqttclient* C=sdConnect(&P,Factory,false,NULL,S,I,&F);
    xmqttproperty Props[2];xmqttpacket Q=sdConnectPacket(XMQTT_VERSION_5,Auth,Props);xmqttclient* Out=(xmqttclient*)(uintptr_t)1;
    testRequire(!sdFactory(&P,Factory,NULL,NULL,S,OtherInbox,&Q,&Out) && Out==(xmqttclient*)(uintptr_t)1,"Session remains exclusively claimed");xrtClearError();
    testRequire(!sdFactory(&P,Factory,NULL,NULL,Other,I,&Q,&Out) && Out==(xmqttclient*)(uintptr_t)1,"Inbox claim failure unwinds new Session claim");xrtClearError();sdRetire(Other,OtherInbox);
    sdClose(C);mqttSessionDialStop(&P);sdRetire(S,I);testRequire(((const xmqttreadyresult*)sdResult(F))->Status==XMQTT_STATUS_CLOSED,"claim-test Future outlives owners");xrtFutureDestroy(F);
}
#endif
