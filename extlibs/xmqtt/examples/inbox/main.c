#include "../common/client_peer.h"
int main(void)
{
    xmqttclientinbox* Inbox=NULL;mqttExampleCheck(xmqttClientInboxCreate(NULL,&Inbox),"Inbox create");
    for(unsigned G=0;G<2;++G){mqttExamplePeer P;mqttExampleStartMode(&P,XMQTT_VERSION_5,true,false);
        xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=true;Q.Body.Connect.ClientId=XRT_STR_LITERAL("inbox-example");xmqttclient* C=NULL;
        mqttExampleCheck(xmqttClientConnectInbox(P.Engine,&P.Address,0,NULL,Inbox,&Q,&C),"Inbox connect");xfuture* Ready=xmqttClientReady(C);
        mqttExampleCheck(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"Inbox ready");
        xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("example/inbox");Q.Body.Publish.Payload=G?XRT_BYTES_LITERAL("second Engine"):XRT_BYTES_LITERAL("first Engine");xfuture* F=NULL;
        mqttExampleCheck(xmqttClientPublish(C,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)xrtFutureValue(F))->Status==XMQTT_STATUS_OK,"QoS1 echo publish");
        double End=xrtTimer()+5;xmqttclientinboxinfo Info;
        do{mqttExampleCheck(xmqttClientInboxInfo(Inbox,&Info) && xrtTimer()<End,"Inbox receive");if(Info.Messages<G+1)xrtSleep(1);}while(Info.Messages<G+1);
        xrtFutureDestroy(F);xrtFutureDestroy(Ready);mqttExampleCheck(xmqttClientDestroy(C),"Client destroy");mqttExampleStop(&P);
    }
    xmqttmessage* M=NULL;while(xmqttClientInboxTake(Inbox,&M)){const xmqttpacket* Q=xmqttMessagePacket(M);printf("saved: %.*s\n",(int)Q->Body.Publish.Payload.Size,(const char*)Q->Body.Publish.Payload.Data);xmqttMessageRelease(M);}
    mqttExampleCheck(xmqttClientInboxDestroy(Inbox),"Inbox destroy");return 0;
}
