#include "../common/client_peer.h"

int main(void)
{
    const xbytesview Expected[]={XRT_BYTES_LITERAL("first Engine"),XRT_BYTES_LITERAL("second Engine")};
    xmqttmessage* Saved[sizeof(Expected)/sizeof(*Expected)]={0};
    for(size_t I=0;I<sizeof(Saved)/sizeof(*Saved);++I){
        mqttExamplePeer Peer;
        mqttExampleStartMode(&Peer,XMQTT_VERSION_5,true,false);

        xmqttpacket Packet;
        xmqttPacketInit(XMQTT_PACKET_CONNECT,&Packet);
        Packet.Body.Connect.CleanStart=true;
        Packet.Body.Connect.ClientId=XRT_STR_LITERAL("inbox-example");
        xmqttclient* Client=NULL;
        mqttExampleCheck(xmqttClientConnect(Peer.Engine,&Peer.Address,0,NULL,&Packet,&Client),"connect");
        xfuture* Ready=xmqttClientReady(Client);
        mqttExampleCheck(Ready&&xrtFutureWaitFor(Ready,5000)==XWAIT_OK&&
            ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"CONNACK");

        xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet);
        Packet.Flags=2;
        Packet.Body.Publish.Topic=XRT_STR_LITERAL("example/inbox");
        Packet.Body.Publish.Payload=Expected[I];
        xfuture* Publish=NULL;
        mqttExampleCheck(xmqttClientPublish(Client,&Packet,5000,&Publish)==XMQTT_ADMISSION_ACCEPTED&&
            ((const xmqttwriteresult*)xrtFutureValue(Publish))->Status==XMQTT_STATUS_OK,"QoS 1 echo");

        /* Take transfers the existing message reference to the APP. */
        double Deadline=xrtTimer()+5;
        while(!xmqttClientTakeMessage(Client,Saved+I)){
            mqttExampleCheck(xrtTimer()<Deadline,"receive echo");
            xrtSleep(1);
        }
        xrtFutureDestroy(Publish);
        xrtFutureDestroy(Ready);
        mqttExampleCheck(xmqttClientDestroy(Client),"retire Client");
        mqttExampleStop(&Peer);
    }

    for(size_t I=0;I<sizeof(Saved)/sizeof(*Saved);++I){
        const xmqttpacket* Packet=xmqttMessagePacket(Saved[I]);
        mqttExampleCheck(Packet&&Packet->Type==XMQTT_PACKET_PUBLISH&&
            Packet->Body.Publish.Payload.Size==Expected[I].Size&&
            !memcmp(Packet->Body.Publish.Payload.Data,Expected[I].Data,Expected[I].Size),"retained echo");
        printf("saved: %.*s\n",(int)Expected[I].Size,(const char*)Packet->Body.Publish.Payload.Data);
        xmqttMessageRelease(Saved[I]);
    }
    return 0;
}
