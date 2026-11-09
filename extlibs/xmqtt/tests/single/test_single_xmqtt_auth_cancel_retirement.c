#define XMQTT_MODULE_XMQTT_CLIENT_AUTH
#include <xmqtt/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#include "../../examples/common/auth_peer.h"
/* White-box lifetime regression: model a producer cancellation dispatch retained
 * before unbinding, then delivered after a later exchange is already active. */
typedef struct authCancelGate {xatomic32 Hold;} authCancelGate;
static void authCancelNotify(xmqttclient* C,ptr User)
{
    authCancelGate* G=User;if(xrtAtomic32Load(&G->Hold,XMEMORY_ACQUIRE))return;
    xmqttauthticket* T=NULL;while(xmqttClientTakeAuth(C,&T))mqttAuthAnswer(T,false);
}
int main(void)
{
    mqttAuthPeer P;mqttAuthStart(&P,MQTT_AUTH_DIRECT);authCancelGate G={0};xrtAtomic32Init(&G.Hold,0);
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=authCancelNotify;K.User=&G;
    xmqttproperty Props[2];xmqttpacket Q=mqttAuthPacket(XMQTT_PACKET_CONNECT,0,Props,false);xmqttclient* C=NULL;
    mqttAuthCheck(xmqttClientConnectAuth(P.Engine,&P.Address,1,&K,NULL,&Q,&C),"cancel-generation factory");xfuture* Ready=xmqttClientReady(C);
    mqttAuthCheck(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"verified connection");
    Q=mqttAuthPacket(XMQTT_PACKET_AUTH,0x19,Props,false);xfuture* Old=NULL;
    mqttAuthCheck(xmqttClientReauthenticateAsync(C,&Q,5000,&Old)==XMQTT_ADMISSION_ACCEPTED && xrtFutureWaitFor(Old,5000)==XWAIT_OK && ((const xmqttauthresult*)xrtFutureValue(Old))->Status==XMQTT_STATUS_OK,"old completed exchange");
    xrtAtomic32Store(&G.Hold,1,XMEMORY_RELEASE);xfuture* Next=NULL;
    mqttAuthCheck(xmqttClientReauthenticateAsync(C,&Q,5000,&Next)==XMQTT_ADMISSION_ACCEPTED,"next active exchange");xmqttauthticket* T=mqttAuthTicket(C);
    mqttAuthCheck(xmqttAuthCancellation.Retain(C),"retained late producer dispatch");xmqttAuthCancellation.Request(C);xmqttClientDrop(C);
    xmqttclientinfo I;xmqttClientInfo(C,&I);bool KeptOpen=I.State==XMQTT_CLIENT_OPEN;
    if(KeptOpen)mqttAuthCheck(xmqttAuthComplete(T,true)==XMQTT_ADMISSION_ACCEPTED,"new exchange proof remains valid");
    xmqttAuthRelease(T);
    mqttAuthCheck(xrtFutureWaitFor(Next,5000)==XWAIT_OK,"next typed completion");bool Succeeded=((const xmqttauthresult*)xrtFutureValue(Next))->Status==XMQTT_STATUS_OK;
    mqttAuthCheck(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"physical cancellation-tail retirement");mqttAuthStop(&P);
    xrtFutureDestroy(Next);xrtFutureDestroy(Old);xrtFutureDestroy(Ready);
    mqttAuthCheck(KeptOpen && Succeeded,"old retained cancel dispatch cannot cancel a later generation");puts("[PASS] MQTT auth cancellation dispatch retirement preserves the current exchange");return 0;
}
