/* Local post-Upgrade attachment demonstration; the literal peer is a test host,
 * not an HTTP handshake implementation. No external broker or trust override. */
#include "../../tests/support/ws_peer.h"
int main(void)
{
    mqttWsPeer P;mqttWsInit(&P,false,XMQTT_VERSION_5,XNET_PORT_SELECT);xmqttclient* C=mqttWsStart(&P);xfuture* Ready=xmqttClientReady(C);
    testRequire(((const xmqttreadyresult*)mqttWsFuture(Ready))->Status==XMQTT_STATUS_OK,"MQTT Ready");
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("example/ws");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");xfuture* Sent=NULL;
    testRequire(xmqttClientPublishAsync(C,&Q,5000,&Sent)==XMQTT_ADMISSION_ACCEPTED,"publication");testRequire(((const xmqttwriteresult*)mqttWsFuture(Sent))->Status==XMQTT_STATUS_OK,"PUBACK");
    xfuture* Bye=NULL;testRequire(xmqttClientDisconnectAsync(C,NULL,5000,&Bye)==XMQTT_ADMISSION_ACCEPTED,"graceful MQTT then WS close");testRequire(((const xmqttwriteresult*)mqttWsFuture(Bye))->Status==XMQTT_STATUS_OK,"DISCONNECT completion");
    mqttWsCloseClient(C);mqttWsStop(&P);xrtFutureDestroy(Ready);xrtFutureDestroy(Sent);xrtFutureDestroy(Bye);puts("[PASS] local MQTT5 post-Upgrade WS attachment, QoS1 and physical shutdown");return 0;
}
