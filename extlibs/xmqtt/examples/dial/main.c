#include "../common/dial_peer.h"
int main(void)
{
    mqttDialPeer P;mqttDialStart(&P,XMQTT_VERSION_5,MQTT_DIAL_DUAL);
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=true;Q.Body.Connect.ClientId=XRT_STR_LITERAL("dial-example");
    xmqttclient* C=NULL;mqttExampleCheck(xmqttClientDial(P.Peer.Engine,P.Resolver,XRT_STR_LITERAL("mqtt.fixture"),P.Peer.Address.Port,1,NULL,NULL,&Q,&C),"hostname factory");
    xfuture* Ready=xmqttClientReady(C);mqttExampleCheck(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"verified CONNACK");
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("dial/example");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("hello");xfuture* F=NULL;
    mqttExampleCheck(xmqttClientPublish(C,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)xrtFutureValue(F))->Status==XMQTT_STATUS_OK,"QoS1 through resolved connection");
    mqttExampleCheck(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"Client physical retirement");mqttDialStop(&P);xrtFutureDestroy(Ready);xrtFutureDestroy(F);
    puts("MQTT5 Dial: bounded dual-stack DNS, QoS1 and physical retirement verified");return 0;
}
