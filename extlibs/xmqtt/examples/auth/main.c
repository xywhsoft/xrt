#include "../common/auth_peer.h"
int main(void)
{
    mqttAuthPeer P;mqttAuthStart(&P,0);xmqttproperty Props[2];xmqttpacket Q=mqttAuthPacket(XMQTT_PACKET_CONNECT,0,Props,false);xmqttclient* C=NULL;
    mqttAuthCheck(xmqttClientConnectAuth(P.Engine,&P.Address,1,NULL,NULL,&Q,&C),"client factory");xfuture* Ready=xmqttClientReady(C);
    for(unsigned I=0;I<3;++I)mqttAuthAnswer(mqttAuthTicket(C),false);
    mqttAuthCheck(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"verified connection");
    Q=mqttAuthPacket(XMQTT_PACKET_AUTH,0x19,Props,false);xfuture* Again=NULL;
    mqttAuthCheck(xmqttClientReauthenticateAsync(C,&Q,5000,&Again)==XMQTT_ADMISSION_ACCEPTED,"reauthentication start");
    for(unsigned I=0;I<3;++I)mqttAuthAnswer(mqttAuthTicket(C),false);
    mqttAuthCheck(xrtFutureWaitFor(Again,5000)==XWAIT_OK && ((const xmqttauthresult*)xrtFutureValue(Again))->Status==XMQTT_STATUS_OK,"verified reauthentication");
    mqttAuthCheck(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"client physical shutdown");mqttAuthStop(&P);
    mqttAuthCheck(xmqttMessagePacket(((const xmqttauthresult*)xrtFutureValue(Again))->Final)->Reason==0,"retained authentication metadata");
    xrtFutureDestroy(Again);xrtFutureDestroy(Ready);puts("MQTT5: two-round initial authentication and reauthentication, explicit local final proof, physical shutdown verified");return 0;
}
