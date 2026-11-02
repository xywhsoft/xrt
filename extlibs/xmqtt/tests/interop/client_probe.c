/* Independent Python wire peer drives this real TCP client executable. */
#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void require(bool OK,const char* Message)
{if(!OK){fprintf(stderr,"[FAIL] MQTT wire probe: %s (%s)\n",Message,xrtErrorMessage(xrtGetError()));exit(1);}}
static const void* result(xfuture* F)
{require(F && xrtFutureWaitFor(F,5000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"typed Future");return xrtFutureValue(F);}
static uint64 checksum(const xmqttmessage* M)
{
    const xmqttpacket* P=xmqttMessagePacket(M);uint64 Sum=0;
    for(size_t I=0;I<P->Wire.Size;++I)Sum=Sum*33+((const unsigned char*)P->Wire.Data)[I];
    if(P->Type==XMQTT_PACKET_PUBLISH){for(size_t I=0;I<P->Body.Publish.Payload.Size;++I)Sum=Sum*33+((const unsigned char*)P->Body.Publish.Payload.Data)[I];}
    for(size_t I=0;I<P->Properties.Count;++I){const xmqttproperty* V=P->Properties.Data+I;
        if(V->Type==XMQTT_PROPERTY_STRING_PAIR){for(size_t J=0;J<V->Value.Pair.Name.Size;++J)Sum=Sum*33+(unsigned char)V->Value.Pair.Name.Data[J];for(size_t J=0;J<V->Value.Pair.Value.Size;++J)Sum=Sum*33+(unsigned char)V->Value.Pair.Value.Data[J];}
        if(V->Type==XMQTT_PROPERTY_BINARY)for(size_t J=0;J<V->Value.Binary.Size;++J)Sum=Sum*33+((const unsigned char*)V->Value.Binary.Data)[J];
    }
    return Sum;
}
int main(int Argc,char** Argv)
{
    require(Argc==6,"usage: client_probe PORT VERSION CASE READY_STATUS CLOSE_STATUS");
    const char* Case=Argv[3];xmqttclientstatus ReadyStatus=(xmqttclientstatus)atoi(Argv[4]),CloseStatus=(xmqttclientstatus)atoi(Argv[5]);
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);require(Engine && xrtNetEngineStart(Engine),"Engine");
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=(xmqttversion)atoi(Argv[2]);K.SetupTimeout=500;K.InitialInputBytes=5;K.Codec.MaxPacketBytes=8192;
    if(!strcmp(Case,"receive_limit"))K.MaxMessages=1;
    if(!strcmp(Case,"object_limit"))K.MaxObjects=2;
    if(!strcmp(Case,"private_limit"))K.MaxPrivateBytes=20000;
    bool Roundtrip=!strcmp(Case,"roundtrip"),Partial=!strcmp(Case,"partial_close"),ReceiveOne=!strcmp(Case,"qos1");
    if(Roundtrip || Partial){K.Stream.WriteLimit=1;K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;K.WriteChunkBytes=1;K.WriteBytes=1;}
    if(Partial)K.Codec.MaxPacketBytes=1024u*1024u;
    char Id[]="probe-copy",Username[]="user-copy",Password[]="password-copy";
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;
    Connect.Body.Connect.ClientId=(xstrview){Id,strlen(Id)};if(!strcmp(Case,"assigned"))Connect.Body.Connect.ClientId=(xstrview){0};
    Connect.Body.Connect.HasUsername=Connect.Body.Connect.HasPassword=true;Connect.Body.Connect.Username=(xstrview){Username,strlen(Username)};Connect.Body.Connect.Password=(xbytesview){(const unsigned char*)Password,strlen(Password)};
    xmqttproperty Receive={XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE,XMQTT_PROPERTY_UINT32,{0}};Receive.Value.Number=4096;
    if(!strcmp(Case,"advertised_limit"))Connect.Properties=(xmqttpropertylist){&Receive,1};
    xnetaddr Address;require(xrtNetAddrParse(&Address,"127.0.0.1",(uint16)atoi(Argv[1])),"address");xmqttclient* C=NULL;
    require(xmqttClientConnect(Engine,&Address,1,&K,&Connect,&C),"factory");memset(Id,'z',sizeof(Id));memset(Username,'z',sizeof(Username));memset(Password,'z',sizeof(Password));
    xfuture* Ready=xmqttClientReady(C);const xmqttreadyresult* R=result(Ready);
    if(R->Status!=ReadyStatus)fprintf(stderr,"actual ready=%d expected=%d\n",(int)R->Status,(int)ReadyStatus);
    require(R->Status==ReadyStatus,"readiness status");
    if(ReadyStatus==XMQTT_STATUS_OK){const char* Expected=!strcmp(Case,"assigned")?"assigned-wire":"probe-copy";
        require(R->Negotiated.ClientId.Size==strlen(Expected) && !memcmp(R->Negotiated.ClientId.Data,Expected,strlen(Expected)),"owned original/assigned id");}
    xfuture* Write=NULL;const xmqttwriteresult* W=NULL;
    if(Roundtrip || Partial){size_t N=Partial?262144:7;char* Payload=malloc(N);require(Payload!=NULL,"probe payload");memset(Payload,'p',N);
        xmqttpacket Pub;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);Pub.Body.Publish.Topic=XRT_STR_LITERAL("wire/test");Pub.Body.Publish.Payload=(xbytesview){(const unsigned char*)Payload,N};
        require(xmqttClientPublishAsync(C,&Pub,5000,&Write)==XMQTT_ADMISSION_ACCEPTED,"publish");memset(Payload,'z',N);free(Payload);W=result(Write);
        require(W->Status==(Partial?XMQTT_STATUS_TRANSPORT:XMQTT_STATUS_OK) && W->AcceptedBytes && W->Delivery==XMQTT_MAY_HAVE_REACHED_PEER,"write status/uncertainty");
        if(Partial)require(W->AcceptedBytes<N,"partial frame must close with unsent suffix");
    }
    xmqttmessage* Saved=NULL;uint64 Sum=0;size_t Received=0;
    if(Roundtrip || ReceiveOne){double End=xrtTimer()+5;while(Received<(Roundtrip?3u:2u)){xmqttmessage* M=NULL;
        if(xmqttClientTakeMessage(C,&M)){++Received;if(!Saved){Saved=M;Sum=checksum(M);}else xmqttMessageRelease(M);}else {require(xrtTimer()<End,"receive split/coalesced packets");xrtSleep(1);}
    }}
    if(CloseStatus==XMQTT_STATUS_CLOSED)require(xmqttClientClose(C),"close");
    require(xmqttClientWaitFor(C,5000)==XWAIT_OK,"physical retirement");xmqttclientinfo I;require(xmqttClientInfo(C,&I),"info");
    if(I.CloseStatus!=CloseStatus)fprintf(stderr,"actual close=%d expected=%d\n",(int)I.CloseStatus,(int)CloseStatus);
    require(I.CloseStatus==CloseStatus && !I.Requests && !I.OutgoingBytes && !I.StartupBytes && I.Accepted==I.Completed,"close status/accounting");
    require(xmqttClientDestroy(C) && xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine),"Client/Engine destruction");
    if(Saved){require(checksum(Saved)==Sum,"retained payload/property views after physical destruction");xmqttMessageRelease(Saved);}
    if(R->Connack)require(xmqttMessagePacket(R->Connack)->Type==XMQTT_PACKET_CONNACK,"retained CONNACK");
    xrtFutureDestroy(Write);xrtFutureDestroy(Ready);
    printf("{\"case\":\"%s\",\"version\":%d,\"ready\":%d,\"close\":%d,\"received\":%zu}\n",Case,(int)K.Version,(int)ReadyStatus,(int)CloseStatus,Received);return 0;
}
