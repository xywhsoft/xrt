/* Offline MQTT5 subscribe/unsubscribe. One local peer grants three filters;
 * ordered reasons remain readable after Client/Engine destruction. */
#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct peer {xatomicptr Stream;xatomic32 Closed,ListenerClosed,Disconnects;unsigned char Input[4096];size_t Used;} peer;
static void require(bool B,const char* M){if(!B){fprintf(stderr,"[FAIL] %s (%s)\n",M,xrtErrorMessage(xrtGetError()));exit(1);}}
static void await(xatomic32* A){double End=xrtTimer()+5;while(!xrtAtomic32Load(A,XMEMORY_ACQUIRE)){require(xrtTimer()<End,"peer progress");xrtSleep(1);}}
static void readPeer(xnetstream* S,xnetbuf* B,ptr Data){peer* P=Data;size_t N=xrtNetBufSize(B);require(N<=sizeof(P->Input)-P->Used,"bounded peer input");P->Used+=xrtNetBufRead(B,P->Input+P->Used,N);
    while(P->Used){xmqttframe F;xmqttwirestatus R=xmqttFrameRead((xbytesview){P->Input,P->Used},NULL,&F);require(R!=XMQTT_WIRE_ERROR,"peer frame");if(R==XMQTT_WIRE_MORE)break;
        if(F.Header.Type==XMQTT_PACKET_CONNECT){const unsigned char Ack[]={0x20,3,0,0,0};require(xrtNetStreamSend(S,Ack,sizeof(Ack))==XNET_RESULT_OK,"CONNACK");}
        else if(F.Header.Type==XMQTT_PACKET_SUBSCRIBE || F.Header.Type==XMQTT_PACKET_UNSUBSCRIBE){const unsigned char* W=F.Wire.Data+F.Header.HeaderBytes;bool Sub=F.Header.Type==XMQTT_PACKET_SUBSCRIBE;
            const unsigned char Ack[]={Sub?0x90:0xb0,6,W[0],W[1],0,0,Sub?1:0,Sub?2:0};require(xrtNetStreamSend(S,Ack,sizeof(Ack))==XNET_RESULT_OK,"ordered subscription ACK");}
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
    xmqttsubscription Topics[3]={{XRT_STR_LITERAL("example/zero"),0},{XRT_STR_LITERAL("example/+"),1},{XRT_STR_LITERAL("example/#"),2}};
    xmqttpacket Subscribe;xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&Subscribe);Subscribe.Body.Topics=(xmqttsubscriptionlist){Topics,3};xfuture *Subscribed=NULL,*Unsubscribed=NULL;
    require(xmqttClientSubscribe(C,&Subscribe,5000,&Subscribed)==XMQTT_ADMISSION_ACCEPTED,"subscribe admission");const xmqttsubscriptionresult* Subscription=xrtFutureValue(Subscribed);require(Subscription->Status==XMQTT_STATUS_OK && Subscription->Acknowledged && Subscription->Granted==3 && Subscription->Ack,"three granted filters");
    xmqttpacket Unsubscribe;xmqttPacketInit(XMQTT_PACKET_UNSUBSCRIBE,&Unsubscribe);for(size_t I=0;I<3;++I)Topics[I].Options=0;Unsubscribe.Body.Topics=(xmqttsubscriptionlist){Topics,3};
    require(xmqttClientUnsubscribe(C,&Unsubscribe,5000,&Unsubscribed)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttsubscriptionresult*)xrtFutureValue(Unsubscribed))->Status==XMQTT_STATUS_OK,"unsubscribe completion");
    xfuture* Closed=NULL;require(xmqttClientDisconnect(C,NULL,5000,&Closed)==XMQTT_ADMISSION_ACCEPTED,"graceful admission");const xmqttwriteresult* R=xrtFutureValue(Closed);require(R && R->Status==XMQTT_STATUS_OK && R->AcceptedBytes==2 && R->Delivery==XMQTT_MAY_HAVE_REACHED_PEER,"local graceful outcome");require(xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"physical client retirement");await(&P.Disconnects);await(&P.Closed);
    xnetstream* S=xrtAtomicPtrLoad(&P.Stream,XMEMORY_ACQUIRE);require(xrtNetStreamWaitRetired(S,5000)==XWAIT_OK,"physical peer retirement");xrtNetStreamDestroy(S);require(xrtNetListenerClose(Listener),"listener close");await(&P.ListenerClosed);xrtNetListenerDestroy(Listener);
    double End=xrtTimer()+5;while(!xrtNetEngineStop(Engine)){require(xrtTimer()<End,"listener retirement");xrtClearError();xrtSleep(1);}require(xrtNetEngineDestroy(Engine),"Engine destruction");require(R->Status==XMQTT_STATUS_OK,"retained result");xrtFutureDestroy(Closed);const xmqttpacket* Ack=xmqttMessagePacket(Subscription->Ack);require(Ack->Body.Reasons.Size==3 && !memcmp(Ack->Body.Reasons.Data,"\0\1\2",3),"retained ordered reason bytes");xrtFutureDestroy(Subscribed);xrtFutureDestroy(Unsubscribed);xrtFutureDestroy(Ready);puts("[PASS] three subscriptions granted and removed, retained ACK readable, all owners retired");return 0;
}
