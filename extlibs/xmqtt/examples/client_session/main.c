#include "../common/session_peer.h"
static const void* result(xfuture* F)
{mqttExampleCheck(F && xrtFutureWaitFor(F,8000)==XWAIT_OK,"Session result");return xrtFutureValue(F);}
int main(void)
{
    xmqttsessionconfig K;xmqttSessionConfigInit(&K);K.ClientId=XRT_STR_LITERAL("session-example");
    xmqttsession* State=NULL;xmqttclientinbox* Inbox=NULL;
    mqttExampleCheck(xmqttSessionCreate(&K,&State) && xmqttClientInboxCreate(NULL,&Inbox),"passive Session and Inbox");
    xfuture *Old=NULL,*Ready[2]={0};
    for(unsigned Generation=0;Generation<2;++Generation){
        mqttResumePeer P;mqttResumeStart(&P,XMQTT_VERSION_5,Generation?0:MQTT_RESUME_HOLD_COMP,Generation!=0);
        xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=K.ClientId;
        xmqttproperty Expiry=mqttResumeNumber(XMQTT_PROPERTY_SESSION_EXPIRY,60);Q.Properties=(xmqttpropertylist){&Expiry,1};
        xmqttclient* C=NULL;mqttExampleCheck(xmqttClientConnectSession(P.Engine,&P.Address,0,NULL,State,Inbox,&Q,&C),"explicit Session connection");
        Ready[Generation]=xmqttClientReady(C);mqttExampleCheck(((const xmqttreadyresult*)result(Ready[Generation]))->Status==XMQTT_STATUS_OK,"accepted CONNACK");
        if(!Generation){
            xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=4;Q.Body.Publish.Topic=XRT_STR_LITERAL("example/session");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("publish once");
            mqttExampleCheck(xmqttClientPublishAsync(C,&Q,8000,&Old)==XMQTT_ADMISSION_ACCEPTED,"QoS2 admission");mqttResumeWait(&P.Releases,1);
        }else {
            mqttResumeWait(&P.Releases,1);double End=xrtTimer()+8;xmqttsessioninfo Info;
            do{mqttExampleCheck(xmqttSessionInfo(State,&Info) && xrtTimer()<End,"recovered PUBCOMP");if(Info.ActiveOutgoing || Info.Objects)xrtSleep(1);}while(Info.ActiveOutgoing || Info.Objects);
            mqttExampleCheck(!xrtAtomic32Load(&P.Publishes,XMEMORY_ACQUIRE),"resumed QoS2 sends PUBREL without another PUBLISH");
        }
        mqttExampleCheck(xmqttClientClose(C) && xmqttClientWaitFor(C,8000)==XWAIT_OK && xmqttClientDestroy(C),"physical Client retirement");mqttResumeStop(&P);
    }
    const xmqttwriteresult* Original=result(Old);mqttExampleCheck(Original->Status==XMQTT_STATUS_CLOSED && Original->PubrecReceived,"old Future stays connection scoped");
    mqttExampleCheck(xmqttSessionDestroy(State) && xmqttClientInboxDestroy(Inbox),"passive owner retirement");
    xrtFutureDestroy(Old);xrtFutureDestroy(Ready[0]);xrtFutureDestroy(Ready[1]);puts("QoS2 recovered across two Clients/Engines using PUBREL only");return 0;
}
