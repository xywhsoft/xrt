#include "support/inbox_peer.h"

static xmqttclient* receiveConnect(inboxPeer* Peer,const xmqttclientconfig* Config,xfuture** Ready)
{
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);
    Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("receive-queue");
    xmqttclient* Client=NULL;
    testRequire(xmqttClientConnect(Peer->Engine,&Peer->Address,0,Config,&Connect,&Client),"receive queue Client");
    *Ready=xmqttClientReady(Client);
    testRequire(*Ready&&xrtFutureWaitFor(*Ready,5000)==XWAIT_OK&&xrtFutureState(*Ready)==XFUTURE_RESOLVED,"receive queue Ready");
    const xmqttreadyresult* Result=xrtFutureValue(*Ready);
    testRequire(Result->Status==XMQTT_STATUS_OK,"receive queue CONNACK");
    return Client;
}

static xmqttclientinfo receiveInfo(xmqttclient* Client)
{
    xmqttclientinfo Info;
    testRequire(xmqttClientInfo(Client,&Info),"receive queue snapshot");
    return Info;
}

static void receivePublish(inboxPeer* Peer,unsigned Qos,unsigned char Byte)
{
    xmqttpacket Packet;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet);
    Packet.Flags=(uint8)(Qos<<1);Packet.Identifier=Qos?7:0;
    Packet.Body.Publish.Topic=XRT_STR_LITERAL("inbox/topic");Packet.Body.Publish.Payload=(xbytesview){&Byte,1};
    unsigned char Wire[128];size_t Length=0;
    bool Encoded=Peer->Version==XMQTT_VERSION_5?
        xmqttPacketWrite5(Wire,sizeof(Wire),&Packet,XMQTT_SERVER_TO_CLIENT,NULL,&Length):
        xmqttPacketWrite311(Wire,sizeof(Wire),&Packet,XMQTT_SERVER_TO_CLIENT,NULL,&Length);
    testRequire(Encoded,"encode inbound publication");inboxSend(Peer,Wire,Length);
}

static void receiveCheck(xmqttmessage* Message,unsigned Value)
{
    const xmqttpacket* Packet=xmqttMessagePacket(Message);
    testRequire(Packet&&Packet->Body.Publish.Topic.Size==11&&!memcmp(Packet->Body.Publish.Topic.Data,"inbox/topic",11)&&
        Packet->Body.Publish.Payload.Size==1&&Packet->Body.Publish.Payload.Data[0]==Value,"retained inbound publication");
}

static void receiveLimits(xmqttversion Version,unsigned Limit)
{
    xmqttclientconfig Config;xmqttClientConfigInit(&Config);Config.Version=Version;
    if(Limit==0)Config.MaxMessages=1;
    if(Limit==2)Config.MaxObjects=3;
    for(unsigned Pass=0;Pass<(Limit==1?2u:1u);++Pass){
        inboxPeer Peer;inboxStart(&Peer,Version);xfuture* Ready=NULL;xmqttclient* Client=receiveConnect(&Peer,&Config,&Ready);
        receivePublish(&Peer,1,31);inboxWait(&Peer.Acks,1);
        xmqttclientinfo Info=receiveInfo(Client);
        testRequire(Info.Messages==1&&Info.Received==1,"accepted message charged and acknowledged");
        xmqttmessage* Saved=NULL;
        if(Limit==2){
            testRequire(Info.Objects==3&&xmqttClientTakeMessage(Client,&Saved),"retained object keeps its connection charge");
        }
        if(Limit==1&&!Pass){
            Config.MaxMessageBytes=Info.MessageBytes;
        }else{
            receivePublish(&Peer,1,32);
            testRequire(xmqttClientWaitFor(Client,5000)==XWAIT_OK,"receive limit retires the connection");
            Info=receiveInfo(Client);
            testRequire(Info.CloseStatus==XMQTT_STATUS_LIMIT&&Info.Received==1&&
                xrtAtomic32Load(&Peer.Acks,XMEMORY_ACQUIRE)==1,"refused publication has no acknowledgement");
            if(!Saved)testRequire(xmqttClientTakeMessage(Client,&Saved),"APP drains accepted message after retirement");
        }
        testRequire(xmqttClientDestroy(Client),"retire receive limit Client");
        inboxStop(&Peer);xrtFutureDestroy(Ready);
        if(Saved){receiveCheck(Saved,31);xmqttMessageRelease(Saved);}
    }
}

typedef struct receiveconsumer {
    xmqttclient* Client;
    xatomic32 Stop,Count,Sum,Callbacks;
} receiveconsumer;

static int32 receiveConsume(ptr Data)
{
    receiveconsumer* Consumer=Data;
    while(!xrtAtomic32Load(&Consumer->Stop,XMEMORY_ACQUIRE)){
        xmqttmessage* Message=NULL;
        if(xmqttClientTakeMessage(Consumer->Client,&Message)){
            const xmqttpacket* Packet=xmqttMessagePacket(Message);
            testRequire(Packet->Body.Publish.Payload.Size==1,"one-byte concurrent delivery");
            xrtAtomic32FetchAdd(&Consumer->Sum,Packet->Body.Publish.Payload.Data[0],XMEMORY_RELAXED);
            testRequire(xmqttMessageRetain(Message),"concurrent retained reference");
            xmqttMessageRelease(Message);xmqttMessageRelease(Message);
            xrtAtomic32FetchAdd(&Consumer->Count,1,XMEMORY_RELEASE);
        }else xrtThreadYield();
    }
    return 0;
}

static void receiveNotify(xmqttclient* Client,ptr Data)
{
    receiveconsumer* Consumer=Data;
    (void)receiveInfo(Client);
    xrtAtomic32FetchAdd(&Consumer->Callbacks,1,XMEMORY_RELAXED);
}

static void receiveConcurrent(xmqttversion Version)
{
    xmqttclientconfig Config;xmqttClientConfigInit(&Config);Config.Version=Version;
    receiveconsumer Consumer={0};
    xrtAtomic32Init(&Consumer.Stop,0);xrtAtomic32Init(&Consumer.Count,0);
    xrtAtomic32Init(&Consumer.Sum,0);xrtAtomic32Init(&Consumer.Callbacks,0);
    Config.Notify=receiveNotify;Config.User=&Consumer;
    inboxPeer Peer;inboxStart(&Peer,Version);xfuture* Ready=NULL;
    Consumer.Client=receiveConnect(&Peer,&Config,&Ready);
    xthread* Threads[4];
    for(unsigned I=0;I<4;++I){Threads[I]=xrtThreadCreate(receiveConsume,&Consumer,0);testRequire(Threads[I]!=NULL,"start receive consumer");}
    for(unsigned I=1;I<=128;++I){receivePublish(&Peer,0,(unsigned char)I);inboxWait(&Consumer.Count,I);}
    inboxWait(&Consumer.Callbacks,128);
    xrtAtomic32Store(&Consumer.Stop,1,XMEMORY_RELEASE);
    for(unsigned I=0;I<4;++I){
        testRequire(xrtThreadWaitFor(Threads[I],5000)==XWAIT_OK&&xrtThreadExitCode(Threads[I])==0,"join receive consumer");
        xrtThreadDestroy(Threads[I]);
    }
    testRequire(xrtAtomic32Load(&Consumer.Count,XMEMORY_ACQUIRE)==128&&
        xrtAtomic32Load(&Consumer.Sum,XMEMORY_RELAXED)==128*129/2&&!receiveInfo(Consumer.Client).Messages&&
        xrtAtomic32Load(&Consumer.Callbacks,XMEMORY_RELAXED)>=128,"concurrent transfer and reentrant notification");
    testRequire(xmqttClientDestroy(Consumer.Client),"retire after all consumers join");
    inboxStop(&Peer);xrtFutureDestroy(Ready);
}

int main(void)
{
    for(unsigned I=0;I<2;++I){
        xmqttversion Version=I?XMQTT_VERSION_311:XMQTT_VERSION_5;
        for(unsigned Limit=0;Limit<3;++Limit)receiveLimits(Version,Limit);
        receiveConcurrent(Version);
    }
    puts("xmqtt connection-local receive queue tests passed");
    return 0;
}
