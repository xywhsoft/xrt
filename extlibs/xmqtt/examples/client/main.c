/* Offline TCP example: real QoS0/1 echo and PUBACKs. No broker installation.
 * python tools/build.py --compiler gcc --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client --no-single
 * The healthy local peer is a fixture, not a broker or interoperability oracle. */
#include "../common/client_peer.h"
int main(void)
{
    mqttExamplePeer P;mqttExampleStart(&P,XMQTT_VERSION_5);P.EchoQosOne=true;
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("example");
    xmqttclient* C=NULL;mqttExampleCheck(xmqttClientConnect(P.Engine,&P.Address,1,NULL,&Connect,&C),"client factory");xfuture* Ready=xmqttClientReady(C);
    mqttExampleCheck(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"CONNACK");
    xmqttpacket Publish;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Publish);Publish.Body.Publish.Topic=XRT_STR_LITERAL("example/topic");Publish.Body.Publish.Payload=XRT_BYTES_LITERAL("hello");
    xfuture *Written=NULL,*Acknowledged=NULL;mqttExampleCheck(xmqttClientPublish(C,&Publish,5000,&Written)==XMQTT_ADMISSION_ACCEPTED,"QoS0 admission");
    const xmqttwriteresult* Result=xrtFutureValue(Written);mqttExampleCheck(Result && Result->Status==XMQTT_STATUS_OK,"QoS0 native write retirement");
    xmqttmessage* Message=NULL;double End=xrtTimer()+5;while(!xmqttClientTakeMessage(C,&Message)){mqttExampleCheck(xrtTimer()<End,"echo receive");xrtSleep(1);}
    Publish.Flags=2; /* Identifier=0 assigns an unused id. */
    mqttExampleCheck(xmqttClientPublish(C,&Publish,5000,&Acknowledged)==XMQTT_ADMISSION_ACCEPTED,"QoS1 admission");
    const xmqttwriteresult* Ack=xrtFutureValue(Acknowledged);mqttExampleCheck(Ack && Ack->Status==XMQTT_STATUS_OK && Ack->Delivery==XMQTT_PEER_ACCEPTED && Ack->Identifier && Ack->Puback,"QoS1 acknowledgement");
    xmqttmessage* One=NULL;End=xrtTimer()+5;while(!xmqttClientTakeMessage(C,&One)){mqttExampleCheck(xrtTimer()<End,"QoS1 echo receive");xrtSleep(1);}
    mqttExampleCheck(xmqttMessagePacket(One)->Identifier==Ack->Identifier,"independent send/receive packet-id spaces");mqttExampleWait(&P.Acks);
    mqttExampleCheck(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"Client physical shutdown");mqttExampleStop(&P);
    const xmqttpacket* Received=xmqttMessagePacket(Message);mqttExampleCheck(Received->Body.Publish.Payload.Size==5 && !memcmp(Received->Body.Publish.Payload.Data,"hello",5) && xmqttMessagePacket(Ack->Puback)->Identifier==Ack->Identifier,"retained receive/PUBACK after destruction");
    mqttExampleCheck(xmqttMessagePacket(One)->Body.Publish.Payload.Size==5,"retained QoS1 receive after destruction");
    xmqttMessageRelease(Message);xmqttMessageRelease(One);xrtFutureDestroy(Written);xrtFutureDestroy(Acknowledged);xrtFutureDestroy(Ready);puts("MQTT5 TCP: QoS0/1 echo, ordered PUBACKs, shutdown and retained messages verified");return 0;
}
