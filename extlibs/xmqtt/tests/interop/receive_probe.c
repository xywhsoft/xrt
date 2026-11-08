/* Independent stdlib peer drives incoming QoS1, bounds and partial frames. */
#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct receiveProbe {xatomic32 Count;xmqttmessage* Messages[8];bool CloseOnMessage;} receiveProbe;
static void require(bool OK,const char* Message)
{if(!OK){fprintf(stderr,"[FAIL] receive probe: %s (%s)\n",Message,xrtErrorMessage(xrtGetError()));exit(1);}}
static void notify(xmqttclient* C,ptr User)
{
    receiveProbe* P=User;xmqttmessage* M=NULL;
    while(xmqttClientTakeMessage(C,&M)){uint32 N=xrtAtomic32Load(&P->Count,XMEMORY_RELAXED);require(N<8,"callback capacity");P->Messages[N]=M;xrtAtomic32Store(&P->Count,N+1,XMEMORY_RELEASE);
        if(P->CloseOnMessage)require(xmqttClientClose(C),"callback close");}
}
static const void* result(xfuture* F)
{require(F && xrtFutureWaitFor(F,5000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"typed Future");return xrtFutureValue(F);}
int main(int Argc,char** Argv)
{
    require(Argc==6,"usage: PORT VERSION CASE CLOSE_STATUS RECEIVED");const char* Case=Argv[3];xmqttclientstatus Expected=(xmqttclientstatus)atoi(Argv[4]);unsigned Target=(unsigned)atoi(Argv[5]);
    bool Backpressure=!strcmp(Case,"backpressure"),Reentrant=!strcmp(Case,"reentrant"),Duplex=!strcmp(Case,"duplex"),Partial=!strcmp(Case,"partial_outgoing");
    receiveProbe Probe={0};xrtAtomic32Init(&Probe.Count,0);Probe.CloseOnMessage=Reentrant;
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);require(Engine && xrtNetEngineStart(Engine),"Engine");
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=(xmqttversion)atoi(Argv[2]);K.InitialInputBytes=256;K.SetupTimeout=2000;
    if(!strcmp(Case,"partial_ack") || !strcmp(Case,"close_partial_ack") || Partial){K.Stream.WriteLimit=1;K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;K.WriteBytes=K.WriteChunkBytes=1;}
    if(!strcmp(Case,"queue_limit") || Backpressure)K.MaxMessages=1;
    if(!strcmp(Case,"object_limit"))K.MaxObjects=2;
    if(!strcmp(Case,"private_limit")){K.MaxMessages=65535;K.MaxPrivateBytes=16384;}
    if(Backpressure || Reentrant){K.Notify=notify;K.User=&Probe;}
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("receive-probe");
    xmqttproperty Maximum={XMQTT_PROPERTY_RECEIVE_MAXIMUM,XMQTT_PROPERTY_UINT16,{0}};Maximum.Value.Number=1;
    if(!strcmp(Case,"receive_window"))Connect.Properties=(xmqttpropertylist){&Maximum,1};
    xnetaddr Address;require(xrtNetAddrParse(&Address,"127.0.0.1",(uint16)atoi(Argv[1])),"address");xmqttclient* C=NULL;require(xmqttClientConnect(Engine,&Address,1,&K,&Connect,&C),"factory");
    xfuture* Ready=xmqttClientReady(C);require(((const xmqttreadyresult*)result(Ready))->Status==XMQTT_STATUS_OK,"CONNACK");xfuture* Write=NULL;
    if(Duplex || Partial){unsigned char Payload[1024];memset(Payload,'p',sizeof(Payload));xmqttpacket Pub;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);Pub.Flags=Duplex?2:0;Pub.Identifier=Duplex?7:0;Pub.Body.Publish.Topic=XRT_STR_LITERAL("outgoing");Pub.Body.Publish.Payload=(xbytesview){Payload,Partial?sizeof(Payload):8};
        require(xmqttClientPublishAsync(C,&Pub,5000,&Write)==XMQTT_ADMISSION_ACCEPTED,"duplex/partial admission");const xmqttwriteresult* W=result(Write);require(W->Status==XMQTT_STATUS_OK && (!Duplex || W->Delivery==XMQTT_PEER_ACCEPTED),"duplex/partial completion");}
    if(Expected==XMQTT_STATUS_CLOSED && !Reentrant){double End=xrtTimer()+5;
        while(xrtAtomic32Load(&Probe.Count,XMEMORY_ACQUIRE)<Target){if(!Backpressure)notify(C,&Probe);require(xrtTimer()<End,"incoming messages/barrier");xrtSleep(1);}
        require(xmqttClientClose(C),"close after peer observed acknowledgements");}
    require(xmqttClientWaitFor(C,5000)==XWAIT_OK,"physical retirement");xmqttclientinfo I;require(xmqttClientInfo(C,&I),"snapshot");
    if(I.CloseStatus!=Expected)fprintf(stderr,"actual close=%d expected=%d received=%llu\n",(int)I.CloseStatus,(int)Expected,(unsigned long long)I.Received);
    require(I.CloseStatus==Expected && I.Received==Target && !I.Requests && !I.OutgoingBytes && !I.StartupBytes,"terminal status/accounting");if(!Backpressure && !Reentrant)notify(C,&Probe);
    require(xmqttClientDestroy(C) && xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine),"Client/Engine destruction");
    unsigned Count=xrtAtomic32Load(&Probe.Count,XMEMORY_ACQUIRE);require(Count==Target,"all accepted messages retained");
    for(unsigned J=0;J<Count;++J){const xmqttpacket* P=xmqttMessagePacket(Probe.Messages[J]);require(P->Type==XMQTT_PACKET_PUBLISH && P->Body.Publish.Topic.Size==8 && !memcmp(P->Body.Publish.Topic.Data,"incoming",8),"retained topic");
        if(P->Identifier){require(P->Body.Publish.Payload.Size==8 && !memcmp(P->Body.Publish.Payload.Data,"received",8),"retained QoS1 payload");
            if(K.Version==XMQTT_VERSION_5)require(P->Properties.Count==2 && P->Properties.Data[0].Value.Pair.Value.Size==5 && !memcmp(P->Properties.Data[0].Value.Pair.Value.Data,"value",5) && P->Properties.Data[1].Value.Binary.Size==2 && !memcmp(P->Properties.Data[1].Value.Binary.Data,"\0x",2),"retained properties");}
        xmqttMessageRelease(Probe.Messages[J]);}
    xrtFutureDestroy(Write);xrtFutureDestroy(Ready);printf("{\"case\":\"%s\",\"version\":%d,\"received\":%u,\"close\":%d}\n",Case,(int)K.Version,Count,(int)I.CloseStatus);return 0;
}
