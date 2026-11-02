/* Offline MQTT5 incoming aliases: full-topic bind, compact reference, retained
 * resolved messages after Client/Engine destruction. No external broker. */
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
        if(F.Header.Type==XMQTT_PACKET_CONNECT){const unsigned char Frames[]={0x20,3,0,0,0,
            0x30,17,0,10,'d','e','m','o','/','t','o','p','i','c',3,0x23,0,1,'v',
            0x30,7,0,0,3,0x23,0,1,'v'};require(xrtNetStreamSend(S,Frames,sizeof(Frames))==XNET_RESULT_OK,"CONNACK, alias bind and compact publication");}
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
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("graceful-example");xmqttproperty Alias={XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM,XMQTT_PROPERTY_UINT16,{0}};Alias.Value.Number=16;Connect.Properties=(xmqttpropertylist){&Alias,1};xmqttclient* C=NULL;require(xmqttClientConnect(Engine,&Address,1,NULL,&Connect,&C),"connect");xfuture* Ready=xmqttClientReady(C);require(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"readiness");
    xmqttmessage* Messages[2]={0};double Until=xrtTimer()+5;for(unsigned I=0;I<2;++I){while(!xmqttClientTakeMessage(C,&Messages[I])){require(xrtTimer()<Until,"two alias deliveries");xrtSleep(1);}}
    xfuture* Closed=NULL;require(xmqttClientDisconnect(C,NULL,5000,&Closed)==XMQTT_ADMISSION_ACCEPTED,"graceful admission");const xmqttwriteresult* R=xrtFutureValue(Closed);require(R && R->Status==XMQTT_STATUS_OK && R->AcceptedBytes==2 && R->Delivery==XMQTT_MAY_HAVE_REACHED_PEER,"local graceful outcome");require(xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"physical client retirement");await(&P.Disconnects);await(&P.Closed);
    xnetstream* S=xrtAtomicPtrLoad(&P.Stream,XMEMORY_ACQUIRE);require(xrtNetStreamWaitRetired(S,5000)==XWAIT_OK,"physical peer retirement");xrtNetStreamDestroy(S);require(xrtNetListenerClose(Listener),"listener close");await(&P.ListenerClosed);xrtNetListenerDestroy(Listener);
    double End=xrtTimer()+5;while(!xrtNetEngineStop(Engine)){require(xrtTimer()<End,"listener retirement");xrtClearError();xrtSleep(1);}require(xrtNetEngineDestroy(Engine),"Engine destruction");require(R->Status==XMQTT_STATUS_OK,"retained result");xrtFutureDestroy(Closed);for(unsigned I=0;I<2;++I){const xmqttpacket* Packet=xmqttMessagePacket(Messages[I]);require(Packet->Body.Publish.Topic.Size==10 && !memcmp(Packet->Body.Publish.Topic.Data,"demo/topic",10),"owned logical topic after alias-map destruction");xmqttMessageRelease(Messages[I]);}xrtFutureDestroy(Ready);puts("[PASS] full and compact alias publications resolve to demo/topic, retained messages survive all owners");return 0;
}
