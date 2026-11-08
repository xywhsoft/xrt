/* Included by test_client.c: unit/fault/lifetime tests, separate from byte peer. */
static xmqttpacket mqttSubscriptionPacket(xmqttpackettype Type,uint16 Id,xmqttsubscription* Topics)
{
    xmqttpacket P;xmqttPacketInit(Type,&P);P.Identifier=Id;
    Topics[0]=(xmqttsubscription){XRT_STR_LITERAL("unit/zero"),0};
    Topics[1]=(xmqttsubscription){XRT_STR_LITERAL("unit/+"),Type==XMQTT_PACKET_SUBSCRIBE?1:0};
    Topics[2]=(xmqttsubscription){XRT_STR_LITERAL("unit/#"),Type==XMQTT_PACKET_SUBSCRIBE?2:0};
    P.Body.Topics=(xmqttsubscriptionlist){Topics,3};return P;
}
static void mqttSubscriptionRetired(xmqttclient* C)
{
    xmqttclientinfo I;double End=xrtTimer()+5;
    do{xmqttClientInfo(C,&I);testRequire(xrtTimer()<End,"subscription reservation retirement");if(I.Requests)xrtSleep(1);}while(I.Requests);
}
static void testMqttSubscriptions(void)
{
    for(unsigned V=4;V<=5;++V){mqttTestPeer Peer;mqttTestStart(&Peer,(xmqttversion)V,0);xmqttclientconfig K;xmqttClientConfigInit(&K);
        K.WriteBytes=K.WriteChunkBytes=1;K.ReadBytes=1;xmqttclient* C=mqttTestConnect(&Peer,&K);xfuture* Ready=xmqttClientReady(C);
        xmqttsubscription Topics[3];xmqttpacket P=mqttSubscriptionPacket(XMQTT_PACKET_SUBSCRIBE,0,Topics);xfuture *F=NULL,*G=NULL;
        testRequire(xmqttClientSubscribeAsync(C,&P,5000,&F)==XMQTT_ADMISSION_ACCEPTED && !P.Identifier,"subscribe queued during setup and auto id does not mutate input");
        memset(Topics,0,sizeof(Topics));memset(&P,0,sizeof(P));
        const xmqttsubscriptionresult* R=mqttTestResult(F);testRequire(R->Status==XMQTT_STATUS_OK && R->Acknowledged && R->Requested==3 && R->Granted==3 && !R->Rejected && R->Ack && R->Identifier,"subscription waits for corresponding full ACK");
        mqttSubscriptionRetired(C);P=mqttSubscriptionPacket(XMQTT_PACKET_UNSUBSCRIBE,R->Identifier,Topics);
        testRequire(xmqttClientUnsubscribe(C,&P,5000,&G)==XMQTT_ADMISSION_ACCEPTED,"blocking unsubscribe and reused physical id");
        const xmqttsubscriptionresult* S=mqttTestResult(G);testRequire(S->Status==XMQTT_STATUS_OK && S->Acknowledged && S->Granted==3 && !S->Rejected && S->Ack,"unsubscribe acknowledgement");
        testRequire(xmqttMessageRetain(R->Ack),"independent subscription ACK owner");const xmqttmessage* Ack=R->Ack;mqttTestClose(C);mqttTestStop(&Peer);xrtFutureDestroy(F);
        const xmqttpacket* A=xmqttMessagePacket(Ack);testRequire(A->Type==XMQTT_PACKET_SUBACK && A->Body.Reasons.Size==3 && !memcmp(A->Body.Reasons.Data,"\0\1\2",3),"owned reason view survives input reuse, Future, Client and Engine");
        A=xmqttMessagePacket(S->Ack);testRequire(A->Type==XMQTT_PACKET_UNSUBACK && A->Body.Reasons.Size==(V==5?3u:0u),"version-specific retained UNSUBACK");
        xmqttMessageRelease(Ack);xrtFutureDestroy(G);xrtFutureDestroy(Ready);
    }
}
static void testMqttSubscriptionFailures(void)
{
    /* Cancel, deadline and Close must settle typed operations after write. */
    for(unsigned V=4;V<=5;++V)for(unsigned Kind=0;Kind<4;++Kind){mqttTestPeer Peer;mqttTestStart(&Peer,(xmqttversion)V,MQTT_TEST_HOLD_PUBACK);xmqttclient* C=mqttTestConnect(&Peer,NULL);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
        xmqttsubscription Topics[3];xmqttpacket P=mqttSubscriptionPacket(Kind&1?XMQTT_PACKET_UNSUBSCRIBE:XMQTT_PACKET_SUBSCRIBE,77,Topics);xfuture* F=NULL;
        testRequire((Kind&1?xmqttClientUnsubscribeAsync(C,&P,Kind==1?100:5000,&F):xmqttClientSubscribeAsync(C,&P,5000,&F))==XMQTT_ADMISSION_ACCEPTED,"subscription failure admission");mqttTestWait(&Peer.Subscriptions,1);
        /* Outgoing controls and publications share ids. */
        xmqttpacket Pub;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);Pub.Identifier=77;Pub.Flags=2;Pub.Body.Publish.Topic=XRT_STR_LITERAL("t");xfuture* Out=(xfuture*)(uintptr_t)1;
        testRequire(xmqttClientPublishAsync(C,&Pub,100,&Out)==XMQTT_ADMISSION_OVERLOADED && Out==(xfuture*)(uintptr_t)1,"subscription id excludes a QoS1 publish");
        if(Kind==0)testRequire(xrtFutureCancel(F),"subscription Future cancellation wakes owner");
        if(Kind==2)testRequire(xmqttClientClose(C),"subscription explicit Close");
        if(Kind==3){const unsigned char Wrong[]={0x90,3,0,77,0};mqttTestSend(&Peer,Wrong,sizeof(Wrong));}
        const xmqttsubscriptionresult* R=mqttTestResult(F);xmqttclientstatus Status=Kind==0?XMQTT_STATUS_CANCELLED:Kind==1?XMQTT_STATUS_TIMEOUT:Kind==2?XMQTT_STATUS_CLOSED:XMQTT_STATUS_PROTOCOL;
        testRequire(R->Status==Status && R->AcceptedBytes && !R->Acknowledged && !R->Ack && R->Requested==3 && !R->Granted,"unacknowledged effects are uncertain, never successful");
        testRequire(xmqttClientWaitFor(C,5000)==XWAIT_OK,"accepted subscription failure retires connection");mqttTestClose(C);mqttTestStop(&Peer);xrtFutureDestroy(F);xrtFutureDestroy(Ready);
    }
}
static void testMqttSubscriptionAllocation(void)
{
    const size_t Limits[]={1,8250,8500,8750,9000,9500,12000,20000,65536};bool Failed=false,Accepted=false;
    for(size_t J=0;J<sizeof(Limits)/sizeof(*Limits);++J){mqttTestPeer Peer;mqttTestStart(&Peer,XMQTT_VERSION_5,0);xmqttclient* C=mqttTestConnect(&Peer,NULL);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
        xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[J],&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"subscription construction budget");
        xmqttsubscription Topics[3];xmqttpacket P=mqttSubscriptionPacket(XMQTT_PACKET_SUBSCRIBE,77,Topics);xfuture* F=(xfuture*)(uintptr_t)1;
        xmqttclientadmission A=xmqttClientSubscribeAsync(C,&P,5000,&F);testRequire(xrtAllocationBudgetLeave(&Scope),"subscription budget leave");xrtClearError();
        if(A==XMQTT_ADMISSION_ACCEPTED){Accepted=true;testRequire(((const xmqttsubscriptionresult*)mqttTestResult(F))->Acknowledged,"budget-success acknowledged");}
        else {xmqttclientinfo I;xmqttClientInfo(C,&I);Failed=true;testRequire(A==XMQTT_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1 && !I.Requests && !I.OutgoingBytes,"construction failure refunds request/wire/id and leaves output unchanged");
            testRequire(xmqttClientSubscribeAsync(C,&P,5000,&F)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttsubscriptionresult*)mqttTestResult(F))->Identifier==77,"construction failure releases id");}
        mqttTestClose(C);mqttTestStop(&Peer);xrtFutureDestroy(F);xrtFutureDestroy(Ready);xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);
        testRequire(xrtAllocationBudgetInfo(Budget,&I) && !I.LiveBytes && !I.LiveCount,"subscription allocation ownership balance");xrtAllocationBudgetFree(Budget);
    }
    testRequire(Failed && Accepted,"subscription allocation matrix failure and success");
}
static void testMqttSubscriptionUnsent(void)
{
    for(unsigned Type=0;Type<2;++Type)for(unsigned Kind=0;Kind<4;++Kind){
        mqttTestPeer Peer;mqttTestStart(&Peer,XMQTT_VERSION_5,MQTT_TEST_GRACEFUL);mqttQueuedCloseGate G={0};
        xrtAtomic32Init(&G.Entered,0);xrtAtomic32Init(&G.Release,0);xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=mqttQueuedCloseNotify;K.User=&G;
        xmqttclient* C=mqttTestConnect(&Peer,&K);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);mqttTestWait(&G.Entered,1);
        xmqttsubscription Topics[3];xmqttpacket P=mqttSubscriptionPacket(Type?XMQTT_PACKET_UNSUBSCRIBE:XMQTT_PACKET_SUBSCRIBE,77,Topics);xfuture *F=NULL,*Closed=NULL;
        testRequire((Type?xmqttClientUnsubscribeAsync(C,&P,Kind==1?0:5000,&F):xmqttClientSubscribeAsync(C,&P,Kind==1?0:5000,&F))==XMQTT_ADMISSION_ACCEPTED,"admit while protocol worker is gated");
        if(Kind==0)testRequire(xrtFutureCancel(F),"cancel queued control");
        if(Kind==3)testRequire(xmqttClientDisconnectAsync(C,NULL,5000,&Closed)==XMQTT_ADMISSION_ACCEPTED,"graceful with a queued control");
        if(Kind>=2)testRequire(xmqttClientClose(C),"Close queued control");
        xrtAtomic32Store(&G.Release,1,XMEMORY_RELEASE);const xmqttsubscriptionresult* R=mqttTestResult(F);
        testRequire(R->Status==(Kind==0?XMQTT_STATUS_CANCELLED:Kind==1?XMQTT_STATUS_TIMEOUT:XMQTT_STATUS_CLOSED) && !R->AcceptedBytes && !R->Acknowledged && !R->Ack && R->Requested==3,"unsent control remains typed and has no effects");
        if(Closed)testRequire(((const xmqttwriteresult*)mqttTestResult(Closed))->Status==XMQTT_STATUS_CLOSED,"graceful abort also settles disconnect");
        testRequire(!xrtAtomic32Load(&Peer.Subscriptions,XMEMORY_ACQUIRE),"no stale subscription submitted after terminal result");
        mqttTestClose(C);mqttTestStop(&Peer);xrtFutureDestroy(F);xrtFutureDestroy(Closed);xrtFutureDestroy(Ready);
    }
}
