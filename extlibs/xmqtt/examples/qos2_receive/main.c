/* Offline MQTT5 QoS2 receiver: one owned client and one local TCP peer.
 * The peer sends QoS2, observes PUBREC/PUBCOMP, then DISCONNECT/EOF. No broker,
 * credentials or external network is needed. This is not a performance test. */
#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct peer {xatomicptr Stream;xatomic32 Closed,ListenerClosed,Disconnects,Completions;unsigned char Input[4096];size_t Used;} peer;
static void require(bool B,const char* M){if(!B){fprintf(stderr,"[FAIL] %s (%s)\n",M,xrtErrorMessage(xrtGetError()));exit(1);}}
static void await(xatomic32* A){double End=xrtTimer()+5;while(!xrtAtomic32Load(A,XMEMORY_ACQUIRE)){require(xrtTimer()<End,"peer progress");xrtSleep(1);}}
static void readPeer(xnetstream* S,xnetbuf* B,ptr Data){peer* P=Data;size_t N=xrtNetBufSize(B);require(N<=sizeof(P->Input)-P->Used,"bounded peer input");P->Used+=xrtNetBufRead(B,P->Input+P->Used,N);
    while(P->Used){xmqttframe F;xmqttwirestatus R=xmqttFrameRead((xbytesview){P->Input,P->Used},NULL,&F);require(R!=XMQTT_WIRE_ERROR,"peer frame");if(R==XMQTT_WIRE_MORE)break;
        if(F.Header.Type==XMQTT_PACKET_CONNECT){const unsigned char Ack[]={0x20,3,0,0,0};require(xrtNetStreamSend(S,Ack,sizeof(Ack))==XNET_RESULT_OK,"CONNACK");xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=4;P.Identifier=7;P.Body.Publish.Topic=XRT_STR_LITERAL("example");P.Body.Publish.Payload=XRT_BYTES_LITERAL("one received message");unsigned char Wire[128];size_t N=0;require(xmqttPacketWrite5(Wire,sizeof(Wire),&P,XMQTT_SERVER_TO_CLIENT,NULL,&N) && xrtNetStreamSend(S,Wire,N)==XNET_RESULT_OK,"server PUBLISH");}
        else if(F.Header.Type==XMQTT_PACKET_PUBREC){require(F.Wire.Size==4 && F.Wire.Data[2]==0 && F.Wire.Data[3]==7,"PUBREC");const unsigned char Rel[]={0x62,2,0,7};require(xrtNetStreamSend(S,Rel,sizeof(Rel))==XNET_RESULT_OK,"PUBREL");}
        else if(F.Header.Type==XMQTT_PACKET_PUBCOMP){require(F.Wire.Size==4 && F.Wire.Data[2]==0 && F.Wire.Data[3]==7,"PUBCOMP");xrtAtomic32Store(&P->Completions,1,XMEMORY_RELEASE);}
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
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("qos2-receive-example");xmqttclient* C=NULL;require(xmqttClientConnect(Engine,&Address,1,NULL,&Connect,&C),"connect");xfuture* Ready=xmqttClientReady(C);require(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"readiness");
    xmqttmessage* Message=NULL;double ReceiveEnd=xrtTimer()+5;while(!xmqttClientTakeMessage(C,&Message)){require(xrtTimer()<ReceiveEnd,"incoming message");xrtSleep(1);}await(&P.Completions);
    xfuture* Closed=NULL;require(xmqttClientDisconnect(C,NULL,5000,&Closed)==XMQTT_ADMISSION_ACCEPTED,"graceful admission");const xmqttwriteresult* R=xrtFutureValue(Closed);require(R && R->Status==XMQTT_STATUS_OK && R->AcceptedBytes==2 && R->Delivery==XMQTT_MAY_HAVE_REACHED_PEER,"local graceful outcome");require(xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"physical client retirement");await(&P.Disconnects);await(&P.Closed);
    xnetstream* S=xrtAtomicPtrLoad(&P.Stream,XMEMORY_ACQUIRE);require(xrtNetStreamWaitRetired(S,5000)==XWAIT_OK,"physical peer retirement");xrtNetStreamDestroy(S);require(xrtNetListenerClose(Listener),"listener close");await(&P.ListenerClosed);xrtNetListenerDestroy(Listener);
    double End=xrtTimer()+5;while(!xrtNetEngineStop(Engine)){require(xrtTimer()<End,"listener retirement");xrtClearError();xrtSleep(1);}require(xrtNetEngineDestroy(Engine),"Engine destruction");require(R->Status==XMQTT_STATUS_OK,"retained result");xrtFutureDestroy(Closed);const xmqttpacket* Received=xmqttMessagePacket(Message);require(Received->Identifier==7 && Received->Body.Publish.Payload.Size==20 && !memcmp(Received->Body.Publish.Payload.Data,"one received message",20),"retained QoS2 payload");xmqttMessageRelease(Message);xrtFutureDestroy(Ready);puts("[PASS] QoS2 received, peer observed PUBCOMP and final DISCONNECT, all owners retired");return 0;
}
