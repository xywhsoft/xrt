#include "support/ws_peer.h"
#if defined(XMQTT_WS_SINGLE_TEST)
typedef struct wsMaxCut {xnetpost Post;xmqttclient* Client;size_t Wire;xatomic32 Done;} wsMaxCut;
static void wsMaxCutMark(xnetworker* Worker,ptr Data)
{
    (void)Worker;wsMaxCut* T=Data;xwsstream* S=T->Client->WS->Stream;size_t Native=0;
    testRequire(!T->Client->WS->Head&&!xrtNetStreamPending(xrtWsStreamTcp(S))&&xrtWsStreamSendSize(S,T->Wire,&Native),"idle native prefix epoch before exact-max control");
    S->WriteCompleted=UINT64_MAX-(uint64)Native;xrtAtomic32Store(&T->Done,1,XMEMORY_RELEASE);
}
static void wsExactMax(xnetportbackend B)
{
    mqttWsPeer P;mqttWsInit(&P,false,XMQTT_VERSION_5,B);xmqttclient* C=mqttWsStart(&P);xfuture* Ready=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)mqttWsFuture(Ready))->Status==XMQTT_STATUS_OK,"exact-max Ready");
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Body.Publish.Topic=XRT_STR_LITERAL("ws/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");wsMaxCut T={.Client=C};xrtAtomic32Init(&T.Done,0);
    testRequire(xmqttPacketSize5(&Q,XMQTT_CLIENT_TO_SERVER,NULL,&T.Wire)&&xrtNetPostInit(&T.Post)&&xrtNetPost(C->Worker,&T.Post,wsMaxCutMark,&T),"mark real native counter on its worker");mqttWsWait(&T.Done,1,"exact-max prefix mark");xfuture* F=NULL;
    testRequire(xmqttClientPublishAsync(C,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED,"exact-max QoS0 publication");testRequire(((const xmqttwriteresult*)mqttWsFuture(F))->Status==XMQTT_STATUS_OK,"valid UINT64_MAX cut retires without waiting for close");mqttWsWait(&P.Publishes,1,"exact-max actual peer bytes");mqttWsCloseClient(C);mqttWsStop(&P);xrtFutureDestroy(F);xrtFutureDestroy(Ready);puts("[PASS] real native exact UINT64_MAX prefix");
}
#endif
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);xnetportbackend B=Argc>1&&!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:Argc>1&&!strcmp(Argv[1],"epoll")?XNET_PORT_EPOLL:XNET_PORT_SELECT;
    for(unsigned TLS=0;TLS<
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
    2
#else
    1
#endif
    ;++TLS)for(unsigned Version=0;Version<2;++Version)for(unsigned Mode=0;Mode<3;++Mode){
        printf("[case] TLS%u version%u mode%u\n",TLS,Version,Mode);mqttWsPeer P;mqttWsInit(&P,TLS!=0,Version?XMQTT_VERSION_311:XMQTT_VERSION_5,B);P.Split=Mode==1;P.Coalesced=Mode==2;P.ClientConfig.ReadBytes=P.ClientConfig.WriteBytes=P.ClientConfig.WriteChunkBytes=P.ClientConfig.WorkPackets=1;
        xmqttclient* C=mqttWsStart(&P);xfuture* Ready=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)mqttWsFuture(Ready))->Status==XMQTT_STATUS_OK,"WS plus MQTT CONNACK");
        for(unsigned Qos=0;Qos<3;++Qos){printf("[qos] %u\n",Qos);xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=(uint8)(Qos<<1);Q.Body.Publish.Topic=XRT_STR_LITERAL("ws/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");xfuture* F=NULL;testRequire(xmqttClientPublishAsync(C,&Q,10000,&F)==XMQTT_ADMISSION_ACCEPTED,"ordinary QoS0/1/2 over Binary fragments");const xmqttwriteresult* R=mqttWsFuture(F);testRequire(R->Status==XMQTT_STATUS_OK&&R->AcceptedBytes&&(Qos?R->Delivery==XMQTT_PEER_ACCEPTED:R->Delivery==XMQTT_MAY_HAVE_REACHED_PEER),"actual native prefix/protocol outcome");xrtFutureDestroy(F);}
        if(P.Coalesced){xmqttmessage* M=NULL;double End=xrtTimer()+10;while(!xmqttClientTakeMessage(C,&M)){testRequire(xrtTimer()<End,"coalesced input resumes after work budget");xrtSleep(1);}testRequire(xmqttMessagePacket(M)->Body.Publish.Payload.Size==3,"retained coalesced payload");xmqttMessageRelease(M);}
        xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&Q);xmqttsubscription Sub={XRT_STR_LITERAL("in"),1};Q.Body.Topics=(xmqttsubscriptionlist){&Sub,1};xfuture* Subscription=NULL;testRequire(xmqttClientSubscribeAsync(C,&Q,5000,&Subscription)==XMQTT_ADMISSION_ACCEPTED,"WS SUBSCRIBE");testRequire(((const xmqttsubscriptionresult*)mqttWsFuture(Subscription))->Status==XMQTT_STATUS_OK,"WS SUBACK");xrtFutureDestroy(Subscription);
        xmqttmessage* Retained=NULL;double Until=xrtTimer()+5;while(!xmqttClientTakeMessage(C,&Retained)){testRequire(xrtTimer()<Until,"WS incoming QoS1");xrtSleep(1);}mqttWsWait(&P.ReceivedAcks,1,"WS incoming PUBACK");
        xmqttPacketInit(XMQTT_PACKET_UNSUBSCRIBE,&Q);Sub.Options=0;Q.Body.Topics=(xmqttsubscriptionlist){&Sub,1};testRequire(xmqttClientUnsubscribeAsync(C,&Q,5000,&Subscription)==XMQTT_ADMISSION_ACCEPTED,"WS UNSUBSCRIBE");testRequire(((const xmqttsubscriptionresult*)mqttWsFuture(Subscription))->Status==XMQTT_STATUS_OK,"WS UNSUBACK");xrtFutureDestroy(Subscription);
        puts("[graceful]");xfuture* Bye=NULL;testRequire(xmqttClientDisconnectAsync(C,NULL,10000,&Bye)==XMQTT_ADMISSION_ACCEPTED,"MQTT DISCONNECT before WS Close");testRequire(((const xmqttwriteresult*)mqttWsFuture(Bye))->Status==XMQTT_STATUS_OK,"bounded graceful WS/TLS shutdown");mqttWsCloseClient(C);mqttWsStop(&P);testRequire(xmqttMessagePacket(Retained)->Body.Publish.Payload.Size==3&&!memcmp(xmqttMessagePacket(Retained)->Body.Publish.Payload.Data,"x\0z",3),"WS retained message outlives Client/Engine");xmqttMessageRelease(Retained);xrtFutureDestroy(Bye);xrtFutureDestroy(Ready);printf("[PASS] MQTT WS TLS%u version%u mode%u\n",TLS,Version,Mode);
    }
#if defined(XMQTT_WS_SINGLE_TEST)
    wsExactMax(B);
#endif
    return 0;
}
