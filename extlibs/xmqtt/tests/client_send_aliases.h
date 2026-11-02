static xmqttclientadmission mqttSendAlias(xmqttclient* C,xstrview Topic,uint16 Alias,unsigned Qos,uint16 Id,int64 Timeout,xfuture** Out)
{
    xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Identifier=Id;P.Flags=(uint8)(Qos<<1);P.Body.Publish.Topic=Topic;P.Body.Publish.Payload=XRT_BYTES_LITERAL("body");
    xmqttproperty A={XMQTT_PROPERTY_TOPIC_ALIAS,XMQTT_PROPERTY_UINT16,{0}};A.Value.Number=Alias;P.Properties=(xmqttpropertylist){&A,1};return xmqttClientPublishAsync(C,&P,Timeout,Out);
}
static void testMqttSendAliases(void)
{
    for(unsigned Kind=0;Kind<3;++Kind){mqttTestPeer Peer;mqttTestStart(&Peer,XMQTT_VERSION_5,MQTT_TEST_SEND_ALIAS|MQTT_TEST_HOLD_PUBACK|MQTT_TEST_WINDOW_ONE);mqttQueuedCloseGate G={0};xrtAtomic32Init(&G.Entered,0);xrtAtomic32Init(&G.Release,0);
        xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=mqttQueuedCloseNotify;K.User=&G;K.WriteBytes=K.WriteChunkBytes=1;
        xmqttclient* C=mqttTestConnect(&Peer,&K);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);mqttTestWait(&G.Entered,1);
        xfuture *F=NULL,*A=NULL,*B=NULL,*D=NULL;testRequire(mqttSendAlias(C,XRT_STR_LITERAL("alpha"),1,1,1,Kind==1?0:5000,&F)==XMQTT_ADMISSION_ACCEPTED,"alias bind admission");
        testRequire(mqttSendAlias(C,(xstrview){0},1,0,0,5000,&A)==XMQTT_ADMISSION_ACCEPTED && mqttSendAlias(C,XRT_STR_LITERAL("bravo"),1,0,0,5000,&B)==XMQTT_ADMISSION_ACCEPTED && mqttSendAlias(C,(xstrview){0},1,0,0,5000,&D)==XMQTT_ADMISSION_ACCEPTED,"snapshot aliases before remapping");
        if(!Kind){testRequire(xrtFutureCancel(F),"cancel unsent binding");}xrtAtomic32Store(&G.Release,1,XMEMORY_RELEASE);
        mqttTestWait(&Peer.Publishes,Kind==2?4:3);if(Kind==2)mqttTestPuback(&Peer,1);
        const xmqttwriteresult* R=mqttTestResult(F);testRequire(R->Status==(Kind==0?XMQTT_STATUS_CANCELLED:Kind==1?XMQTT_STATUS_TIMEOUT:XMQTT_STATUS_OK),"binding terminal outcome");
        testRequire(((const xmqttwriteresult*)mqttTestResult(A))->Status==XMQTT_STATUS_OK && ((const xmqttwriteresult*)mqttTestResult(B))->Status==XMQTT_STATUS_OK && ((const xmqttwriteresult*)mqttTestResult(D))->Status==XMQTT_STATUS_OK,"dependent snapshots are delivered despite cancellation/timeout");
        unsigned Offset=Kind==2?1:0;testRequire(!strcmp(Peer.PublishedTopics[Offset],"alpha") && !strcmp(Peer.PublishedTopics[Offset+1],"bravo") && !strcmp(Peer.PublishedTopics[Offset+2],"bravo"),"wire topics follow each operation's immutable intent");
        testRequire(xrtAtomic32Load(&Peer.CompactPublishes,XMEMORY_ACQUIRE)==(Kind==2?2u:1u),"actual wire mapping selects compact or repairs binding");
        mqttTestClose(C);mqttTestStop(&Peer);xrtFutureDestroy(F);xrtFutureDestroy(A);xrtFutureDestroy(B);xrtFutureDestroy(D);xrtFutureDestroy(Ready);
    }
}
static void testMqttSendAliasAllocation(void)
{
    const size_t Limits[]={1,128,256,512,2048,4096,8192,12000,65536};bool Failed=false,Success=false;
    for(size_t J=0;J<sizeof(Limits)/sizeof(*Limits);++J){mqttTestPeer Peer;mqttTestStart(&Peer,XMQTT_VERSION_5,MQTT_TEST_SEND_ALIAS);xmqttclient* C=mqttTestConnect(&Peer,NULL);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
        xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[J],&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"outgoing alias construction quota");xfuture* F=(xfuture*)(uintptr_t)1;
        xmqttclientadmission A=mqttSendAlias(C,XRT_STR_LITERAL("alpha"),1,0,0,5000,&F);testRequire(xrtAllocationBudgetLeave(&Scope),"outgoing alias quota leave");xrtClearError();
        if(A==XMQTT_ADMISSION_ACCEPTED){Success=true;testRequire(((const xmqttwriteresult*)mqttTestResult(F))->Status==XMQTT_STATUS_OK,"alias construction success control");}
        else {Failed=true;xmqttclientinfo I;xmqttClientInfo(C,&I);testRequire(A==XMQTT_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1 && !I.Requests && !I.OutgoingBytes,"failed alias admission refunds request and wire");
            testRequire(mqttSendAlias(C,(xstrview){0},1,0,0,1000,&F)==XMQTT_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"failed binding cannot alter admitted mapping");
            testRequire(mqttSendAlias(C,XRT_STR_LITERAL("alpha"),1,0,0,5000,&F)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)mqttTestResult(F))->Status==XMQTT_STATUS_OK,"failed construction permits later binding");}
        mqttTestClose(C);mqttTestStop(&Peer);xrtFutureDestroy(F);xrtFutureDestroy(Ready);xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(Budget,&I) && !I.LiveBytes && !I.LiveCount,"send alias cache/request/topic quota lifetime");xrtAllocationBudgetFree(Budget);
    }
    testRequire(Failed && Success,"outgoing alias construction failure and success matrix");
}
