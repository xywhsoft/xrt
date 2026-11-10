/* A worker queries readiness; the unique external owner performs cleanup. */
#include "../common/client_peer.h"
typedef struct pollExample {xmqttclient* Client;xnetpost Post;xatomic32 Done;xwaitresult Result;} pollExample;
static void pollOnWorker(xnetworker* Worker,ptr Data)
{
    (void)Worker;pollExample* P=Data;P->Result=xmqttClientPollRetired(P->Client);
    xrtAtomic32Store(&P->Done,1,XMEMORY_RELEASE);
}
int main(void)
{
    mqttExamplePeer Peer;mqttExampleStart(&Peer,XMQTT_VERSION_5);pollExample P={0};xrtAtomic32Init(&P.Done,0);
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("retirement-query");
    mqttExampleCheck(xmqttClientConnect(Peer.Engine,&Peer.Address,1,NULL,&Connect,&P.Client),"Client factory");xfuture* Ready=xmqttClientReady(P.Client);
    mqttExampleCheck(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"CONNACK");
    mqttExampleCheck(xmqttClientPollRetired(P.Client)==XWAIT_TIMEOUT,"open owner is pending");
    mqttExampleCheck(xmqttClientClose(P.Client) && xmqttClientWaitFor(P.Client,5000)==XWAIT_OK,"external owner waits for physical retirement");
    mqttExampleCheck(xrtNetPostInit(&P.Post) && xrtNetPost(xrtNetEngineWorker(Peer.Engine,1),&P.Post,pollOnWorker,&P),"worker query post");mqttExampleWait(&P.Done);
    mqttExampleCheck(P.Result==XWAIT_OK && xmqttClientDestroy(P.Client),"external owner destroys after worker observation");
    mqttExampleStop(&Peer);xrtFutureDestroy(Ready);puts("MQTT5: nonwaiting worker query, external cleanup and physical Engine join verified");return 0;
}
