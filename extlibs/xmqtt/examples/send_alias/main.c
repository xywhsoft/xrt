/* Offline MQTT5 outgoing alias: bind a topic, publish an explicit compact
 * reference, drain QoS1 and retain typed results after every owner is gone. */
#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct peer {xatomicptr Stream;xatomic32 Closed,ListenerClosed,Disconnects,Publishes;bool Bound;unsigned char Input[4096];size_t Used;} peer;
static void require(bool B,const char* M){if(!B){fprintf(stderr,"[FAIL] %s (%s)\n",M,xrtErrorMessage(xrtGetError()));exit(1);}}
static void await(xatomic32* A){double End=xrtTimer()+5;while(!xrtAtomic32Load(A,XMEMORY_ACQUIRE)){require(xrtTimer()<End,"peer progress");xrtSleep(1);}}
static void readPeer(xnetstream* S,xnetbuf* B,ptr Data){peer* P=Data;size_t N=xrtNetBufSize(B);require(N<=sizeof(P->Input)-P->Used,"bounded peer input");P->Used+=xrtNetBufRead(B,P->Input+P->Used,N);
    while(P->Used){xmqttframe F;xmqttwirestatus R=xmqttFrameRead((xbytesview){P->Input,P->Used},NULL,&F);require(R!=XMQTT_WIRE_ERROR,"peer frame");if(R==XMQTT_WIRE_MORE)break;
        if(F.Header.Type==XMQTT_PACKET_CONNECT){const unsigned char Ack[]={0x20,6,0,0,3,0x22,0,16};require(xrtNetStreamSend(S,Ack,sizeof(Ack))==XNET_RESULT_OK,"CONNACK");}
        else if(F.Header.Type==XMQTT_PACKET_PUBLISH){xmqttpacket Q;xmqttproperty Props[2];xmqttcodecbuffers Buffers={Props,2,NULL,0};require(xmqttPacketRead5(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&Buffers,&Q,NULL)==XMQTT_CODEC_DONE && Q.Flags==2 && Q.Properties.Count==1 && Props[0].Id==XMQTT_PROPERTY_TOPIC_ALIAS && Props[0].Value.Number==1,"QoS1 alias frame");
            uint32 N=xrtAtomic32Load(&P->Publishes,XMEMORY_RELAXED);if(!N){require(Q.Body.Publish.Topic.Size==9 && !memcmp(Q.Body.Publish.Topic.Data,"demo/send",9),"full topic binds alias 1");P->Bound=true;}else require(P->Bound && !Q.Body.Publish.Topic.Size,"later wire topic is compact");
            const unsigned char Ack[]={0x40,2,(unsigned char)(Q.Identifier>>8),(unsigned char)Q.Identifier};require(xrtNetStreamSend(S,Ack,sizeof(Ack))==XNET_RESULT_OK,"PUBACK");xrtAtomic32FetchAdd(&P->Publishes,1,XMEMORY_RELEASE);}
        else {require(F.Header.Type==XMQTT_PACKET_DISCONNECT && F.Wire.Size==2,"normal final DISCONNECT");xrtAtomic32Store(&P->Disconnects,1,XMEMORY_RELEASE);}
        P->Used-=F.Wire.Size;if(P->Used)memmove(P->Input,P->Input+F.Wire.Size,P->Used);
    }
}
static void endPeer(xnetstream* S,ptr Data){(void)Data;require(xrtNetStreamClose(S),"peer close on client EOF");}
static void closedPeer(xnetstream* S,xnetresult R,const xerror* E,ptr Data){(void)S;(void)R;(void)E;xrtAtomic32Store(&((peer*)Data)->Closed,1,XMEMORY_RELEASE);}
static bool acceptPeer(xnetlistener* L,xnetstream* S,ptr Data){(void)L;peer* P=Data;static const xnetstreamevents E={NULL,readPeer,endPeer,NULL,NULL,NULL,closedPeer};require(xrtNetStreamSetEvents(S,&E,P),"peer callbacks");xrtAtomicPtrStore(&P->Stream,S,XMEMORY_RELEASE);return true;}
static void closedListener(xnetlistener* L,ptr Data){(void)L;xrtAtomic32Store(&((peer*)Data)->ListenerClosed,1,XMEMORY_RELEASE);}
int main(void){peer P={0};xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);require(Engine && xrtNetEngineStart(Engine),"Engine");
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);L.Distribution=XNET_ACCEPT_LOCAL;static const xnetlistenerevents Events={acceptPeer,NULL,closedListener};xnetlistener* Listener=xrtNetListen(Engine,&L,&Events,NULL,&P);xnetaddr Address;require(Listener && xrtNetListenerLocal(Listener,&Address),"local listener");
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("graceful-example");xmqttclient* C=NULL;require(xmqttClientConnect(Engine,&Address,1,NULL,&Connect,&C),"connect");xfuture* Ready=xmqttClientReady(C);require(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"readiness");
    xmqttpacket Publish;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Publish);Publish.Flags=2;Publish.Body.Publish.Topic=XRT_STR_LITERAL("demo/send");Publish.Body.Publish.Payload=XRT_BYTES_LITERAL("body");xmqttproperty Alias={XMQTT_PROPERTY_TOPIC_ALIAS,XMQTT_PROPERTY_UINT16,{0}};Alias.Value.Number=1;Publish.Properties=(xmqttpropertylist){&Alias,1};xfuture *Written=NULL,*Compact=NULL;
    require(xmqttClientPublish(C,&Publish,5000,&Written)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)xrtFutureValue(Written))->Delivery==XMQTT_PEER_ACCEPTED,"full alias binding acknowledged");Publish.Body.Publish.Topic=(xstrview){0};
    require(xmqttClientPublish(C,&Publish,5000,&Compact)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)xrtFutureValue(Compact))->Delivery==XMQTT_PEER_ACCEPTED,"compact publication acknowledged");
    xfuture* Closed=NULL;require(xmqttClientDisconnect(C,NULL,5000,&Closed)==XMQTT_ADMISSION_ACCEPTED,"graceful admission");const xmqttwriteresult* R=xrtFutureValue(Closed);require(R && R->Status==XMQTT_STATUS_OK && R->AcceptedBytes==2 && R->Delivery==XMQTT_MAY_HAVE_REACHED_PEER,"local graceful outcome");require(xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"physical client retirement");await(&P.Disconnects);await(&P.Closed);
    xnetstream* S=xrtAtomicPtrLoad(&P.Stream,XMEMORY_ACQUIRE);require(xrtNetStreamWaitRetired(S,5000)==XWAIT_OK,"physical peer retirement");xrtNetStreamDestroy(S);require(xrtNetListenerClose(Listener),"listener close");await(&P.ListenerClosed);xrtNetListenerDestroy(Listener);
    double End=xrtTimer()+5;while(!xrtNetEngineStop(Engine)){require(xrtTimer()<End,"listener retirement");xrtClearError();xrtSleep(1);}require(xrtNetEngineDestroy(Engine),"Engine destruction");require(R->Status==XMQTT_STATUS_OK,"retained result");xrtFutureDestroy(Closed);require(((const xmqttwriteresult*)xrtFutureValue(Written))->AcceptedBytes==23 && ((const xmqttwriteresult*)xrtFutureValue(Compact))->AcceptedBytes==14 && xrtAtomic32Load(&P.Publishes,XMEMORY_ACQUIRE)==2,"retained actual wire byte counts");xrtFutureDestroy(Written);xrtFutureDestroy(Compact);xrtFutureDestroy(Ready);puts("[PASS] alias bound, compact QoS1 sent, 23/14 wire bytes, all owners retired");return 0;
}
