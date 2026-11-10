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
#if defined(XMQTT_CLIENT_SESSION_SINGLE_TEST)
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
