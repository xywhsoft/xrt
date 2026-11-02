/* Independent peer verifies receiver protocol and owned delivery separately. */
#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct probe {xatomic32 Count;xmqttmessage* Messages[64];bool Close;} probe;
static void check(bool B,const char* S){if(!B){fprintf(stderr,"[FAIL] QoS2 receive: %s (%s)\n",S,xrtErrorMessage(xrtGetError()));exit(1);}}
static bool mode(const char* S,const char* T){return !strcmp(S,T);}
static void notify(xmqttclient* C,ptr Data){probe* B=Data;xmqttmessage* M=NULL;while(xmqttClientTakeMessage(C,&M)){unsigned I=xrtAtomic32Load(&B->Count,XMEMORY_RELAXED);check(I<64,"retained capacity");B->Messages[I]=M;if(B->Close)check(xmqttClientClose(C),"reentrant close");xrtAtomic32Store(&B->Count,I+1,XMEMORY_RELEASE);}}
static const void* result(xfuture* F){check(F && xrtFutureWaitFor(F,7000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"typed result");return xrtFutureValue(F);}
int main(int Argc,char** Argv){if(Argc!=6)return 2;const char* Case=Argv[3];unsigned V=(unsigned)atoi(Argv[2]),Target=(unsigned)atoi(Argv[5]);xmqttclientstatus Expected=(xmqttclientstatus)atoi(Argv[4]);probe B={0};xrtAtomic32Init(&B.Count,0);B.Close=mode(Case,"reentrant");
    bool Tiny=mode(Case,"ack_fairness") || mode(Case,"byte") || !strncmp(Case,"partial_",8) || !strncmp(Case,"close_partial",13),Graceful=!strncmp(Case,"graceful",8),Duplex=mode(Case,"duplex"),Partial=mode(Case,"partial_outgoing");
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);check(Engine && xrtNetEngineStart(Engine),"Engine");xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=(xmqttversion)V;K.Notify=notify;K.User=&B;
    if(Tiny){K.WriteBytes=K.WriteChunkBytes=1;K.Stream.WriteLimit=1;K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;}
    if(mode(Case,"queue_limit")){K.MaxMessages=1;K.Notify=NULL;}
    if(mode(Case,"backpressure") || mode(Case,"ack_fairness"))K.MaxMessages=1;
    if(mode(Case,"object_limit"))K.MaxObjects=2;
    if(mode(Case,"byte_limit"))K.MaxMessageBytes=1;
    if(mode(Case,"private_limit"))K.MaxPrivateBytes=16384;
    if(mode(Case,"work_budget"))K.WorkPackets=1;
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("qos2-receive");xmqttproperty Props[2];size_t N=0;
    if(!strncmp(Case,"window_",7) || mode(Case,"duplicate_window")){Props[N]=(xmqttproperty){XMQTT_PROPERTY_RECEIVE_MAXIMUM,XMQTT_PROPERTY_UINT16,{0}};Props[N++].Value.Number=1;}
    if(mode(Case,"problem"))Props[N++]=(xmqttproperty){XMQTT_PROPERTY_REQUEST_PROBLEM_INFO,XMQTT_PROPERTY_BYTE,{0}};
    Connect.Properties=(xmqttpropertylist){Props,N};xnetaddr A;check(xrtNetAddrParse(&A,"127.0.0.1",(uint16)atoi(Argv[1])),"address");xmqttclient* C=NULL;check(xmqttClientConnect(Engine,&A,1,&K,&Connect,&C),"factory");xfuture* Ready=xmqttClientReady(C);check(((const xmqttreadyresult*)result(Ready))->Status==XMQTT_STATUS_OK,"CONNACK");xfuture *Write=NULL,*Closed=NULL;
    if(Duplex || Partial){unsigned char Payload[32768];memset(Payload,'p',sizeof(Payload));xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=Duplex?4:0;P.Identifier=Duplex?9:0;P.Body.Publish.Topic=XRT_STR_LITERAL("outgoing");P.Body.Publish.Payload=(xbytesview){Payload,Partial?sizeof(Payload):8};check(xmqttClientPublishAsync(C,&P,5000,&Write)==XMQTT_ADMISSION_ACCEPTED,"outgoing admission");const xmqttwriteresult* R=result(Write);check(R->Status==XMQTT_STATUS_OK && (!Duplex || (R->PubrecReceived && R->PubcompReceived && R->Delivery==XMQTT_PEER_ACCEPTED)),"duplex/partial outcome");}
    if(Graceful || (Expected==XMQTT_STATUS_CLOSED && !B.Close)){double End=xrtTimer()+7;while(xrtAtomic32Load(&B.Count,XMEMORY_ACQUIRE)<Target){check(xrtTimer()<End,"message/barrier observation");xrtSleep(1);}
        if(Graceful){check(xmqttClientDisconnectAsync(C,NULL,mode(Case,"graceful_timeout")?100:5000,&Closed)==XMQTT_ADMISSION_ACCEPTED && xrtFutureWaitFor(Closed,0)==XWAIT_TIMEOUT,"incoming flow drains before final packet");if(mode(Case,"graceful_cancel"))check(xrtFutureCancel(Closed),"drain cancellation");check(((const xmqttwriteresult*)result(Closed))->Status==Expected,"graceful typed outcome");}
        else check(xmqttClientClose(C),"cleanup close");}
    check(xmqttClientWaitFor(C,7000)==XWAIT_OK,"physical retirement");xmqttclientinfo I;xmqttClientInfo(C,&I);if(I.CloseStatus!=Expected || I.Received!=Target)fprintf(stderr,"case=%s status=%d expected=%d received=%llu target=%u\n",Case,I.CloseStatus,Expected,(unsigned long long)I.Received,Target);
    check(I.CloseStatus==Expected && I.Received==Target && I.Accepted==I.Completed && !I.Requests && !I.OutgoingBytes,"terminal accounting");notify(C,&B);check(xmqttClientDestroy(C) && xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine),"destruction");check(xrtAtomic32Load(&B.Count,XMEMORY_ACQUIRE)==Target,"every unique delivery retained");
    for(unsigned J=0;J<Target;++J){const xmqttpacket* P=xmqttMessagePacket(B.Messages[J]);check(P->Type==XMQTT_PACKET_PUBLISH,"retained publication");if(P->Identifier){check(P->Body.Publish.Topic.Size==8 && !memcmp(P->Body.Publish.Topic.Data,"incoming",8) && P->Body.Publish.Payload.Size==8 && !memcmp(P->Body.Publish.Payload.Data,"received",8),"owned topic/payload");if(V==5)check(P->Properties.Count==2 && P->Properties.Data[0].Value.Pair.Value.Size==5 && !memcmp(P->Properties.Data[0].Value.Pair.Value.Data,"value",5) && P->Properties.Data[1].Value.Binary.Size==2 && !memcmp(P->Properties.Data[1].Value.Binary.Data,"\0x",2),"owned properties");}xmqttMessageRelease(B.Messages[J]);}
    xrtFutureDestroy(Closed);xrtFutureDestroy(Write);xrtFutureDestroy(Ready);printf("{\"case\":\"%s\",\"version\":%u,\"received\":%u,\"close\":%d}\n",Case,V,Target,Expected);return 0;
}
