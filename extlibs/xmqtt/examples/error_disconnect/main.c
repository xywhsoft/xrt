/* Offline diagnostic-close example. The peer deliberately emits a bad header.
 * python tools/build.py --compiler gcc --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client --no-single
 * Expected: reason 0x81 is final, original PROTOCOL status survives, all owners retire. */
#include "../common/client_peer.h"
int main(void)
{
    mqttExamplePeer P;mqttExampleStartMode(&P,XMQTT_VERSION_5,false,true);
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.ClientId=XRT_STR_LITERAL("error-example");Connect.Body.Connect.CleanStart=true;
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.CloseTimeout=1000;xmqttclient* C=NULL;mqttExampleCheck(xmqttClientConnect(P.Engine,&P.Address,1,&K,&Connect,&C),"diagnostic client");xfuture* Ready=xmqttClientReady(C);
    mqttExampleCheck(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"accepted handshake before error");
    mqttExampleCheck(xmqttClientWaitFor(C,5000)==XWAIT_OK,"automatic diagnostic physical retirement");xmqttclientinfo I;mqttExampleCheck(xmqttClientInfo(C,&I) && I.CloseStatus==XMQTT_STATUS_PROTOCOL && I.LocalReason==0x81 && I.DrainStarted && !I.DrainTimedOut && I.DisconnectBytes==3,"typed original error and local diagnostic progress");
    mqttExampleWait(&P.Closed);mqttExampleCheck(xrtAtomic32Load(&P.Disconnects,XMEMORY_ACQUIRE)==1,"peer observed final diagnostic");
    mqttExampleCheck(xmqttClientDestroy(C),"diagnostic Client destruction");mqttExampleStop(&P);xrtFutureDestroy(Ready);puts("MQTT5 error: final DISCONNECT 0x81, original PROTOCOL result and physical cleanup verified");return 0;
}
