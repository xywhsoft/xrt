#ifndef XMQTT_CLIENT_SESSION_TLS_CASES_H
#define XMQTT_CLIENT_SESSION_TLS_CASES_H
static void stSeed(xmqttsession* S,xmqttversion V)
{
    xmqttpacket Q,A;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=XRT_STR_LITERAL("tls-session");xmqttproperty E={0};E.Id=XMQTT_PROPERTY_SESSION_EXPIRY;E.Type=XMQTT_PROPERTY_UINT32;E.Value.Number=60;if(V==XMQTT_VERSION_5)Q.Properties=(xmqttpropertylist){&E,1};xmqttPacketInit(XMQTT_PACKET_CONNACK,&A);
    xmqttnegotiated N;xmqttsessionstatus Status=xmqttSessionOpen(S,&Q,&A,xrtTimer(),&N);testRequire(Status==XMQTT_SESSION_DONE || Status==XMQTT_SESSION_RESET,"offline protocol state seed");
    uint8 Payload[257];for(size_t J=0;J<sizeof(Payload);++J)Payload[J]=(uint8)(J*37u);xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Identifier=23;Q.Body.Publish.Topic=XRT_STR_LITERAL("tls/topic");Q.Body.Publish.Payload=(xbytesview){Payload,sizeof(Payload)};
    const xmqttsessionpacket* Packet=NULL;xmqttsessionframe F;testRequire(xmqttSessionPublish(S,&Q,&Packet)==XMQTT_SESSION_DONE && xmqttSessionNext(S,&F)==XMQTT_SESSION_DONE && xmqttSessionSent(S,&F)==XMQTT_SESSION_DONE,"seed sent old obligation");xmqttSessionPacketRelease(F.Packet);xmqttSessionPacketRelease(Packet);testRequire(xmqttSessionEnd(S,NULL,xrtTimer())==XMQTT_SESSION_DONE,"seed offline expiry");
}
static void stFailures(xmqttversion V)
{
    for(unsigned Mode=0;Mode<4;++Mode){xmqttsession* S;xmqttclientinbox* I;stPair(V,&S,&I);stSeed(S,V);xmqttsessioninfo Before=stInfo(S);stPeer P;
        unsigned Flags=Mode==0?MQTT_TLS_REJECT:Mode==3?MQTT_TLS_BLOCK_DNS:MQTT_TLS_SILENT;stStart(&P,V,Flags,true,0,false);
        xmqttclientconfig K;xmqttClientConfigInit(&K);K.SetupTimeout=Mode==1?40:10000;xfuture* F=NULL;xmqttclient* C=stConnect(&P,S,I,Mode==3?1:0,false,&K,&F);
        if(Mode==2)testRequire(xrtFutureCancel(F),"TLS Session setup cancellation");
        if(Mode==3){mqttTlsWait(&P.TLS.DnsEntered,1,"Session DNS blocked");testRequire(xmqttClientClose(C),"Session close while DNS pending");}
        const xmqttreadyresult* R=stResult(F);testRequire(R->Status==(Mode==0?XMQTT_STATUS_TRANSPORT:Mode==1?XMQTT_STATUS_TIMEOUT:Mode==2?XMQTT_STATUS_CANCELLED:XMQTT_STATUS_CLOSED),"typed Session TLS verification/deadline/cancel/DNS failure");
        testRequire(xmqttSessionReset(S)==XMQTT_SESSION_STATE_ERROR && !xmqttClientInboxDestroy(I),"both exclusive claims retained until actual Destroy");xrtClearError();stClose(C);mqttTlsStop(&P.TLS);
        xmqttsessioninfo After=stInfo(S);testRequire(!After.Connected && After.HasState && After.ActiveOutgoing==1 && After.Objects==1 && Before.ExpiresAt==After.ExpiresAt && Before.Generation==After.Generation,"failed secure setup preserves old state and expiry");
        stStart(&P,V,0,true,0,false);xfuture* Ready=NULL;C=stConnect(&P,S,I,0,false,NULL,&Ready);stReady(C,Ready,S,true,false);stDone(S,C);mqttTlsWait(&P.Publishes,1,"old obligation resumes after failed TLS");stClose(C);mqttTlsStop(&P.TLS);
        testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"claims recover after failed secure setup");testRequire(R->Status!=XMQTT_STATUS_OK,"old failed TLS outcome never changes after recovery");xrtFutureDestroy(F);xrtFutureDestroy(Ready);
    }
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
static void stAuthFailure(void)
{
    for(unsigned Factory=0;Factory<3;++Factory)for(unsigned Present=0;Present<2;++Present){xmqttsession* S;xmqttclientinbox* I;stPair(XMQTT_VERSION_5,&S,&I);stSeed(S,XMQTT_VERSION_5);xmqttsessioninfo Before=stInfo(S);stPeer P;stStart(&P,XMQTT_VERSION_5,MQTT_TLS_AUTH,Present!=0,0,true);xfuture* F=NULL;
        xmqttclient* C=stConnect(&P,S,I,Factory,false,NULL,&F);stDecide(C,F,S,true,false);testRequire(((const xmqttreadyresult*)stResult(F))->Status==XMQTT_STATUS_AUTHENTICATION,"final proof rejection precedes Session reset/open/replay");stClose(C);mqttTlsStop(&P.TLS);
        xmqttsessioninfo After=stInfo(S);xmqttclientinboxinfo Inbox;testRequire(xmqttClientInboxInfo(I,&Inbox) && !Inbox.Messages && !After.Connected && After.HasState && After.ActiveOutgoing==1 && After.Objects==1 && After.Incoming==0 && Before.ExpiresAt==After.ExpiresAt && Before.Generation==After.Generation && !xrtAtomic32Load(&P.Publishes,XMEMORY_ACQUIRE),"denied proof preserves offline state/expiry and publishes no replay or incoming message");
        testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"denied proof releases passive claims at Destroy");xrtFutureDestroy(F);
    }
    xmqttsession* S;xmqttclientinbox* I;stPair(XMQTT_VERSION_5,&S,&I);stPeer P;stStart(&P,XMQTT_VERSION_5,MQTT_TLS_AUTH,true,0,false);xfuture* F=NULL;xmqttclient* C=stConnect(&P,S,I,0,false,NULL,&F);stDecide(C,F,S,true,true);
    testRequire(((const xmqttreadyresult*)stResult(F))->Status==XMQTT_STATUS_SESSION,"accepted proof cannot adopt broker-present state without local state");stClose(C);mqttTlsStop(&P.TLS);testRequire(!stInfo(S).HasState && xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"no fabricated retained Session after authenticated mismatch");xrtFutureDestroy(F);
}
static void stOrdinaryAuth(void)
{
    for(unsigned Factory=0;Factory<3;++Factory){stPeer P;stStart(&P,XMQTT_VERSION_5,MQTT_TLS_AUTH,false,0,true);xmqttclienttlsconfig T;char Name[12],Alpn[4];xstrview Protocol;xtlsverifier* V=mqttTlsConfigure(&P.TLS,&T,Name,&Protocol,Alpn);
        xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=false;Q.Body.Connect.ClientId=XRT_STR_LITERAL("ordinary-proof");xmqttproperty Method={0};Method.Id=XMQTT_PROPERTY_AUTH_METHOD;Method.Type=XMQTT_PROPERTY_STRING;Method.Value.Text=XRT_STR_LITERAL("m");Q.Properties=(xmqttpropertylist){&Method,1};xmqttclient* C=NULL;bool OK;
        if(!Factory)OK=xmqttClientTlsConnectAuth(P.TLS.Engine,&P.TLS.Address,1,NULL,&T,NULL,&Q,&C);
        else if(Factory==1)OK=xmqttClientTlsDialAuth(P.TLS.Engine,P.TLS.Resolver,XRT_STR_LITERAL("example.com"),P.TLS.Address.Port,1,NULL,&T,NULL,&Q,&C);
        else {xnetaddrlist* L=xrtNetAddrListCreate(&P.TLS.Address,1);testRequire(L!=NULL,"ordinary proof endpoints");OK=xmqttClientTlsDialAddressesAuth(P.TLS.Engine,L,1,NULL,&T,NULL,&Q,&C);xrtNetAddrListDestroy(L);}
        testRequire(OK,"ordinary TLS auth regression factory");xrtTlsVerifierRelease(V);xfuture* F=xmqttClientReady(C);stDecide(C,F,NULL,true,true);testRequire(((const xmqttreadyresult*)stResult(F))->Status==XMQTT_STATUS_OK,"ordinary proof acceptance");mqttTlsWait(&P.Comps,1,"ordinary coalesced incoming QoS2 completes after proof");xmqttmessage* M=NULL;testRequire(xmqttClientTakeMessage(C,&M),"ordinary post-proof message visible");stClose(C);mqttTlsStop(&P.TLS);const xmqttpacket* Packet=xmqttMessagePacket(M);testRequire(Packet->Body.Publish.Payload.Size==3 && !memcmp(Packet->Body.Publish.Payload.Data,"x\0z",3),"ordinary post-proof held payload");xmqttMessageRelease(M);xrtFutureDestroy(F);
    }
}
#endif
static void stCtor(void)
{
    stPeer P;stStart(&P,XMQTT_VERSION_5,0,false,0,false);xmqttsession* S;xmqttclientinbox* I;stPair(XMQTT_VERSION_5,&S,&I);
    xmqttclienttlsconfig T;char Name[12],Alpn[4];xstrview Protocol;xtlsverifier* V=mqttTlsConfigure(&P.TLS,&T,Name,&Protocol,Alpn);
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=XRT_STR_LITERAL("tls-session");xmqttclient* C=(xmqttclient*)(uintptr_t)1;
    testRequire(!xmqttClientTlsConnectSession(P.TLS.Engine,&P.TLS.Address,1,NULL,&T,NULL,I,&Q,&C) && C==(xmqttclient*)(uintptr_t)1,"NULL Session preserves constructor output");
    testRequire(!xmqttClientTlsConnectSession(P.TLS.Engine,&P.TLS.Address,1,NULL,&T,S,NULL,&Q,&C) && C==(xmqttclient*)(uintptr_t)1,"NULL Inbox preserves constructor output");
    testRequire(!xmqttClientTlsConnectSession(P.TLS.Engine,NULL,1,NULL,&T,S,I,&Q,&C) && !xmqttClientTlsDialAddressesSession(P.TLS.Engine,NULL,1,NULL,&T,S,I,&Q,&C),"invalid TLS Session endpoints");
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.MaxPrivateBytes=1;testRequire(!xmqttClientTlsConnectSession(P.TLS.Engine,&P.TLS.Address,1,&K,&T,S,I,&Q,&C) && C==(xmqttclient*)(uintptr_t)1,"combined TLS/driver private bound before claim publication");
    Q.Body.Connect.ClientId=XRT_STR_LITERAL("wrong-id");testRequire(!xmqttClientTlsConnectSession(P.TLS.Engine,&P.TLS.Address,1,NULL,&T,S,I,&Q,&C) && C==(xmqttclient*)(uintptr_t)1,"identity mismatch rolls back prepared TLS");
    xrtTlsVerifierRelease(V);testRequire(xrtAtomic32Load(&P.TLS.VerifierDrops,XMEMORY_ACQUIRE)==1,"failed constructor retains no native verifier");
    testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"invalid factory releases both passive claims");mqttTlsStop(&P.TLS);xrtClearError();
}
static void stAllocation(void)
{
    static const size_t Limits[]={1,128,4096,16384,65536,131072,1048576};unsigned Failed=0,Passed=0;
    for(unsigned J=0;J<sizeof(Limits)/sizeof(*Limits);++J){stPeer P;stStart(&P,XMQTT_VERSION_5,0,false,0,false);xmqttsession* S;xmqttclientinbox* I;stPair(XMQTT_VERSION_5,&S,&I);
        xmqttclienttlsconfig T;char Name[12],Alpn[4];xstrview Protocol;xtlsverifier* V=mqttTlsConfigure(&P.TLS,&T,Name,&Protocol,Alpn);xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=XRT_STR_LITERAL("tls-session");
        xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[J],&B) && xrtAllocationBudgetEnter(B,&Scope),"TLS Session allocator prefix");xmqttclient* C=(xmqttclient*)(uintptr_t)1;
        bool OK=xmqttClientTlsConnectSession(P.TLS.Engine,&P.TLS.Address,1,NULL,&T,S,I,&Q,&C);testRequire(xrtAllocationBudgetLeave(&Scope),"leave TLS Session caller scope");xrtTlsVerifierRelease(V);
        if(OK){++Passed;xfuture* F=xmqttClientReady(C);(void)stResult(F);stClose(C);xrtFutureDestroy(F);}else {++Failed;testRequire(C==(xmqttclient*)(uintptr_t)1,"TLS Session OOM leaves output unchanged");}
        testRequire(xmqttSessionDestroy(S) && xmqttClientInboxDestroy(I),"TLS Session constructor/physical claims retire");mqttTlsStop(&P.TLS);xrtClearError();xallocationbudgetinfo Info={.Size=sizeof(Info)};testRequire(xrtAllocationBudgetInfo(B,&Info) && !Info.LiveBytes && !Info.LiveCount,"combined factory allocator duties physically refunded");xrtAllocationBudgetFree(B);
    }testRequire(Failed && Passed,"TLS Session allocation prefixes cover failure and success");
}
#endif
