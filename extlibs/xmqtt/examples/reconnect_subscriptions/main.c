#define MQTT_EXAMPLE_SUBSCRIPTIONS
#include "../common/client_peer.h"
#include <xmqtt/reconnect_subscriptions.h>
static xmqttclientstatus connectAttempt(xnetengine* E,uint64 Affinity,const xmqttclientconfig* C,
    xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,ptr Data,xmqttclient** Client)
{mqttExamplePeer* P=Data;return xmqttClientConnectSession(E,&P->Address,Affinity,C,S,I,Q,Client)?XMQTT_STATUS_OK:XMQTT_STATUS_INITIALIZATION;}
int main(void)
{
    mqttExamplePeer P;mqttExampleStart(&P,XMQTT_VERSION_5);xtaskpoolconfig T={1,8,0};xtaskpool* Pool=xrtTaskPoolCreate(&T);mqttExampleCheck(Pool!=NULL,"borrowed cleanup Pool");
    xmqttreconnectconfig K;xmqttReconnectConfigInit(&K);K.Factory=connectAttempt;K.FactoryData=&P;
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=XRT_STR_LITERAL("managed-example");Q.Body.Connect.CleanStart=true;xmqttreconnect* R=NULL;
    xmqttsubscriptionsconfig S;xmqttSubscriptionsConfigInit(&S);
    xmqttsubscription Topic={XRT_STR_LITERAL("example/+"),1};xmqttpacket Initial;xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&Initial);Initial.Body.Topics=(xmqttsubscriptionlist){&Topic,1};
    mqttExampleCheck(xmqttReconnectCreateSubscribed(P.Engine,Pool,1,&K,&S,&Q,&Initial,1,&R),"automatic subscription controller");xmqttclientlease L={0};double End=xrtTimer()+5;
    while(!xmqttReconnectAcquire(R,&L)){mqttExampleCheck(xrtTimer()<End,"online lease");xrtSleep(1);}
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Body.Publish.Topic=XRT_STR_LITERAL("example/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("managed");xfuture* F=NULL;
    mqttExampleCheck(xmqttClientPublish(L.Client,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)xrtFutureValue(F))->Status==XMQTT_STATUS_OK,"leased normal publication");
    xmqttmessage* Held=NULL;End=xrtTimer()+5;while(!xmqttReconnectTakeMessage(R,&Held)){mqttExampleCheck(xrtTimer()<End,"first-generation echo");xrtSleep(1);}
    /* Simulate a peer outage while the old application lease remains live.
     * Retire the fixture's unique peer owner before resetting its atomic slots. */
    xnetstream* OldPeer=xrtAtomicPtrLoad(&P.Stream,XMEMORY_ACQUIRE);
    mqttExampleCheck(xrtNetStreamAbort(OldPeer) && xmqttClientWaitFor(L.Client,5000)==XWAIT_OK,"interrupted old Client stays owned by its lease");mqttExampleWait(&P.Closed);
    mqttExampleCheck(xrtNetStreamWaitRetired(OldPeer,5000)==XWAIT_OK && !P.Used,"fixture peer fully retired");xrtNetStreamDestroy(OldPeer);
    xrtAtomicPtrStore(&P.Stream,NULL,XMEMORY_RELEASE);xrtAtomic32Store(&P.Closed,0,XMEMORY_RELEASE);
    mqttExampleCheck(xmqttReconnectRelease(&L),"old lease release enables replacement");End=xrtTimer()+5;xmqttreconnectinfo I;
    for(;;){mqttExampleCheck(xmqttReconnectInfo(R,&I) && I.State<XMQTT_RECONNECT_STOPPED && xrtTimer()<End,"automatic second generation");
        if(I.Connections==2 && xmqttReconnectAcquire(R,&L))break;
        xrtSleep(1);}
    mqttExampleCheck(L.Generation==2,"replacement lease generation");
    xmqttsubscriptionsinfo SI;xmqttsubscriptionintentinfo TI;
    mqttExampleCheck(xmqttReconnectSubscriptionsInfo(R,&SI) && SI.Synchronized && SI.Requests==2 && xmqttReconnectSubscriptionInfo(R,Topic.Filter,&TI) && TI.Confirmed && xrtAtomic32Load(&P.SubscriptionPackets,XMEMORY_ACQUIRE)==2,"subscription restored on both fresh broker sessions");
    mqttExampleCheck(xmqttReconnectRelease(&L) && xmqttReconnectClose(R) && xmqttReconnectWaitFor(R,5000)==XWAIT_OK && xmqttReconnectDestroy(R),"physical controller cleanup");
    mqttExampleCheck(xrtTaskPoolClose(Pool) && xrtTaskPoolWaitFor(Pool,5000)==XWAIT_OK && xrtTaskPoolDestroy(Pool),"borrowed Pool join");mqttExampleStop(&P);
    mqttExampleCheck(xmqttMessagePacket(Held)->Body.Publish.Payload.Size==7 && ((const xmqttwriteresult*)xrtFutureValue(F))->Status==XMQTT_STATUS_OK,"old message/result survive both generations");xmqttMessageRelease(Held);xrtFutureDestroy(F);puts("MQTT5 subscription recovery: initial intent, actual outage, lost-session restoration, retained outcomes and physical joins verified");return 0;
}
