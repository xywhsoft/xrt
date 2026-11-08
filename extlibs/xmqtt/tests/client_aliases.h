/* Unit/ownership/OOM tests; the byte peer is a separate executable. */
static xmqttclient* mqttAliasConnect(mqttTestPeer* Peer,xmqttclientconfig* Config,uint16 Maximum)
{
    xmqttclientconfig K;if(Config)K=*Config;else xmqttClientConfigInit(&K);K.Version=XMQTT_VERSION_5;
    xmqttproperty Alias={XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM,XMQTT_PROPERTY_UINT16,{0}};Alias.Value.Number=Maximum;
    xmqttpacket Connect=mqttTestConnectPacket();Connect.Properties=(xmqttpropertylist){&Alias,1};xmqttclient* C=NULL;
    testRequire(xmqttClientConnect(Peer->Engine,&Peer->Address,1,&K,&Connect,&C),"advertised incoming alias maximum");return C;
}
static void mqttAliasSend(mqttTestPeer* Peer,uint16 Alias,xstrview Topic,unsigned Qos,uint16 Id,bool Duplicate)
{
    xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=(uint8)((Qos<<1)|(Duplicate?8u:0));P.Identifier=Id;P.Body.Publish.Topic=Topic;P.Body.Publish.Payload=XRT_BYTES_LITERAL("body");
    xmqttproperty A={XMQTT_PROPERTY_TOPIC_ALIAS,XMQTT_PROPERTY_UINT16,{0}};A.Value.Number=Alias;P.Properties=(xmqttpropertylist){&A,1};
    unsigned char Wire[256];size_t Size;testRequire(xmqttPacketWrite5(Wire,sizeof(Wire),&P,XMQTT_SERVER_TO_CLIENT,NULL,&Size),"alias fixture encoding");mqttTestSend(Peer,Wire,Size);
}
static void mqttAliasReceived(xmqttclient* C,size_t Count)
{
    xmqttclientinfo I;double End=xrtTimer()+5;
    do{xmqttClientInfo(C,&I);testRequire(xrtTimer()<End && I.State<XMQTT_CLIENT_CLOSING,"alias message progress");if(I.Received<Count)xrtSleep(1);}while(I.Received<Count);
}
static void testMqttAliasOwnership(void)
{
    mqttTestPeer Peer;mqttTestStart(&Peer,XMQTT_VERSION_5,0);xmqttclient* C=mqttAliasConnect(&Peer,NULL,65535);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
    mqttAliasSend(&Peer,65535,XRT_STR_LITERAL("old/topic"),0,0,false);mqttAliasSend(&Peer,65535,(xstrview){0},0,0,false);mqttAliasReceived(C,2);
    xmqttmessage *First=NULL,*Compact=NULL;testRequire(xmqttClientTakeMessage(C,&First) && xmqttClientTakeMessage(C,&Compact),"retained alias messages");
    xmqttclientinfo Before,After;xmqttClientInfo(C,&Before);mqttAliasSend(&Peer,65535,XRT_STR_LITERAL("x"),0,0,false);mqttAliasSend(&Peer,65535,(xstrview){0},0,0,false);mqttAliasReceived(C,4);xmqttClientInfo(C,&After);
    testRequire(Before.PrivateBytes==After.PrivateBytes,"shorter remap reuses charged capacity");
    mqttAliasSend(&Peer,65535,XRT_STR_LITERAL("new/topic/expanded"),0,0,false);mqttAliasSend(&Peer,65535,(xstrview){0},0,0,false);mqttAliasReceived(C,6);
    xmqttmessage *Short=NULL,*ShortCompact=NULL,*Long=NULL,*LongCompact=NULL;testRequire(xmqttClientTakeMessage(C,&Short) && xmqttClientTakeMessage(C,&ShortCompact) && xmqttClientTakeMessage(C,&Long) && xmqttClientTakeMessage(C,&LongCompact),"remapped messages");
    mqttTestClose(C);mqttTestStop(&Peer);xrtFutureDestroy(Ready);
    const xmqttpacket* P=xmqttMessagePacket(Compact);testRequire(P->Body.Publish.Topic.Size==9 && !memcmp(P->Body.Publish.Topic.Data,"old/topic",9),"resolved topic survives overwrite and Client/Engine destruction");
    testRequire(P->Body.Publish.Payload.Size==4 && !memcmp(P->Body.Publish.Payload.Data,"body",4) && P->Properties.Count==1 && P->Properties.Data[0].Value.Number==65535,"alias payload/property ownership");
    xmqttpacket Raw;xmqttproperty Props[1];xmqttcodecbuffers B={Props,1,NULL,0};testRequire(xmqttPacketRead5(P->Wire,XMQTT_SERVER_TO_CLIENT,NULL,&B,&Raw,NULL)==XMQTT_CODEC_DONE && !Raw.Body.Publish.Topic.Size,"raw wire preserves empty topic independently of resolved descriptor");
    testRequire(xmqttMessagePacket(ShortCompact)->Body.Publish.Topic.Size==1 && xmqttMessagePacket(ShortCompact)->Body.Publish.Topic.Data[0]=='x' && xmqttMessagePacket(LongCompact)->Body.Publish.Topic.Size==18 && !memcmp(xmqttMessagePacket(LongCompact)->Body.Publish.Topic.Data,"new/topic/expanded",18),"short and larger rebindings remain independent");
    xmqttMessageRelease(First);xmqttMessageRelease(Compact);xmqttMessageRelease(Short);xmqttMessageRelease(ShortCompact);xmqttMessageRelease(Long);xmqttMessageRelease(LongCompact);
}
static void testMqttAliasQosTwo(void)
{
    mqttTestPeer Peer;mqttTestStart(&Peer,XMQTT_VERSION_5,0);xmqttclient* C=mqttAliasConnect(&Peer,NULL,2);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
    mqttAliasSend(&Peer,1,XRT_STR_LITERAL("first"),2,7,false);mqttTestWait(&Peer.ReceiveRecs,1);
    mqttAliasSend(&Peer,1,XRT_STR_LITERAL("second"),2,7,true);mqttTestWait(&Peer.ReceiveRecs,2);
    mqttAliasSend(&Peer,1,(xstrview){0},0,0,false);mqttAliasReceived(C,2);xmqttmessage *First=NULL,*Next=NULL;
    testRequire(xmqttClientTakeMessage(C,&First) && xmqttClientTakeMessage(C,&Next) && xmqttMessagePacket(First)->Body.Publish.Topic.Size==5 && !memcmp(xmqttMessagePacket(Next)->Body.Publish.Topic.Data,"second",6),"QoS2 duplicate updates alias without duplicate delivery");
    mqttTestQosTwoAck(&Peer,XMQTT_PACKET_PUBREL,7);mqttTestWait(&Peer.ReceiveComps,1);mqttTestClose(C);mqttTestStop(&Peer);xrtFutureDestroy(Ready);xmqttMessageRelease(First);xmqttMessageRelease(Next);
}
static void testMqttAliasAllocation(void)
{
    const size_t Limits[]={1,8,24,64,128,152,192,256,512,2048};bool BeforeTable=false,AfterTable=false,Success=false;
    for(unsigned Stage=0;Stage<3;++Stage)for(size_t J=0;J<sizeof(Limits)/sizeof(*Limits);++J){
        mqttTestPeer Peer;mqttTestStart(&Peer,XMQTT_VERSION_5,0);mqttReceiveBudget B={0};testRequire(xrtAllocationBudgetCreate(Limits[J],&B.Budget),"alias allocation budget");xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=mqttReceiveBudgetNotify;K.User=&B;
        xmqttclient* C=mqttAliasConnect(&Peer,&K,16);xfuture* Ready=xmqttClientReady(C);(void)mqttTestResult(Ready);
        /* One buffered input batch enters the worker budget after its first
         * message, isolating alias allocations from native TCP read allocation. */
        xmqttclientinfo Before,I;double End=xrtTimer()+5;do{xmqttClientInfo(C,&Before);testRequire(xrtTimer()<End,"alias OOM startup retirement");if(Before.StartupBytes)xrtSleep(1);}while(Before.StartupBytes);
        const unsigned char Marker[]={0x30,5,0,1,'t',0,'m'},Bind[]={0x30,11,0,1,'a',3,0x23,0,1,'b','o','d','y'},Expanded[]={0x30,18,0,8,'e','x','p','a','n','d','e','d',3,0x23,0,1,'b','o','d','y'},Compact[]={0x30,10,0,0,3,0x23,0,1,'b','o','d','y'};
        unsigned char Wire[64];size_t First=Stage==0?sizeof(Marker):sizeof(Bind),Second=Stage==1?sizeof(Compact):sizeof(Expanded);memcpy(Wire,Stage==0?Marker:Bind,First);memcpy(Wire+First,Stage==1?Compact:Expanded,Second);mqttTestSend(&Peer,Wire,First+Second);
        do{xmqttClientInfo(C,&I);testRequire(xrtTimer()<End,"alias OOM outcome");if(I.State!=XMQTT_CLIENT_CLOSED && I.Received!=2)xrtSleep(1);}while(I.State!=XMQTT_CLIENT_CLOSED && I.Received!=2);
        if(I.State==XMQTT_CLIENT_CLOSED){testRequire(I.CloseStatus==XMQTT_STATUS_MEMORY && I.Received==1,"alias OOM never delivers partial message");if(Stage==0){if(I.PrivateBytes>Before.PrivateBytes)AfterTable=true;else BeforeTable=true;}}
        else Success=true;
        mqttTestClose(C);mqttTestStop(&Peer);xrtFutureDestroy(Ready);testRequire(B.Entered && !B.Active,"alias worker budget scope retired");xallocationbudgetinfo Q={0};Q.Size=sizeof(Q);testRequire(xrtAllocationBudgetInfo(B.Budget,&Q) && !Q.LiveCount && !Q.LiveBytes,"alias cache/message budget fully reclaimed");xrtAllocationBudgetFree(B.Budget);
    }
    testRequire(BeforeTable && AfterTable && Success,"alias allocation matrix covers cold table, topic/message failures and successful control");
}
