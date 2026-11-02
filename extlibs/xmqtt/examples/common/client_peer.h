#ifndef XMQTT_EXAMPLE_CLIENT_PEER_H
#define XMQTT_EXAMPLE_CLIENT_PEER_H
/* Healthy local fixture for examples/loopback workloads only. This is not a
 * broker, comparator, product runtime or interoperability oracle. */
#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct mqttExamplePeer {
    xnetengine* Engine;xnetlistener* Listener;xnetaddr Address;xmqttversion Version;
    xatomicptr Stream;xatomic32 Closed,ListenerClosed,Acks,Disconnects;bool EchoQosOne,ErrorAfterConnack;unsigned char Input[65536];size_t Used;
} mqttExamplePeer;
static inline void mqttExampleCheck(bool OK,const char* Message)
{if(!OK){fprintf(stderr,"MQTT fixture failed: %s\n",Message);exit(1);}}
static inline void mqttExampleWait(xatomic32* A)
{double End=xrtTimer()+5;while(!xrtAtomic32Load(A,XMEMORY_ACQUIRE)){mqttExampleCheck(xrtTimer()<End,"peer retirement");xrtSleep(1);}}
static inline void mqttExampleRead(xnetstream* S,xnetbuf* Buffer,ptr Data)
{
    mqttExamplePeer* P=Data;size_t N=xrtNetBufSize(Buffer);mqttExampleCheck(N<=sizeof(P->Input)-P->Used,"input capacity");P->Used+=xrtNetBufRead(Buffer,P->Input+P->Used,N);
    while(P->Used){xmqttwireconfig K;xmqttWireConfigInit(&K);K.Version=P->Version;xmqttframe F;xmqttwirestatus Status=xmqttFrameRead((xbytesview){P->Input,P->Used},&K,&F);
        mqttExampleCheck(Status!=XMQTT_WIRE_ERROR,"framing");if(Status==XMQTT_WIRE_MORE)break;
        if(F.Header.Type==XMQTT_PACKET_CONNECT){const unsigned char Ack5[]={0x20,3,0,0,0},Ack311[]={0x20,2,0,0};
            mqttExampleCheck(xrtNetStreamSend(S,P->Version==XMQTT_VERSION_5?Ack5:Ack311,P->Version==XMQTT_VERSION_5?sizeof(Ack5):sizeof(Ack311))==XNET_RESULT_OK,"CONNACK");
            if(P->ErrorAfterConnack){const unsigned char Bad[]={0,0};mqttExampleCheck(xrtNetStreamSend(S,Bad,sizeof(Bad))==XNET_RESULT_OK,"demonstration malformed packet");}
        }else if(F.Header.Type==XMQTT_PACKET_PUBLISH){
            if((F.Header.Flags>>1)&3u){xmqttpacket Pub;xmqttproperty Props[16];xmqttcodecbuffers B={Props,16,NULL,0};
                mqttExampleCheck((P->Version==XMQTT_VERSION_5?xmqttPacketRead5(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&B,&Pub,NULL):xmqttPacketRead311(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&B,&Pub,NULL))==XMQTT_CODEC_DONE && ((Pub.Flags>>1)&3u)==1,"QoS1 request");
                unsigned char Ack[]={0x40,2,(unsigned char)(Pub.Identifier>>8),(unsigned char)Pub.Identifier};mqttExampleCheck(xrtNetStreamSend(S,Ack,sizeof(Ack))==XNET_RESULT_OK,"PUBACK");
                if(P->EchoQosOne)mqttExampleCheck(xrtNetStreamSend(S,F.Wire.Data,F.Wire.Size)==XNET_RESULT_OK,"QoS1 echo");
            }else mqttExampleCheck(xrtNetStreamSend(S,F.Wire.Data,F.Wire.Size)==XNET_RESULT_OK,"QoS0 echo");
        }else if(F.Header.Type==XMQTT_PACKET_PUBACK){mqttExampleCheck(P->EchoQosOne && F.Wire.Size==4,"echo PUBACK");xrtAtomic32FetchAdd(&P->Acks,1,XMEMORY_RELEASE);
        }else if(F.Header.Type==XMQTT_PACKET_DISCONNECT){const unsigned char* W=F.Wire.Data;mqttExampleCheck(P->ErrorAfterConnack && P->Version==XMQTT_VERSION_5 && F.Wire.Size==3 && W[2]==0x81,"diagnostic DISCONNECT");xrtAtomic32FetchAdd(&P->Disconnects,1,XMEMORY_RELEASE);mqttExampleCheck(xrtNetStreamClose(S),"diagnostic peer close");
        }else if(F.Header.Type==XMQTT_PACKET_PINGREQ){const unsigned char Pong[]={0xd0,0};mqttExampleCheck(xrtNetStreamSend(S,Pong,sizeof(Pong))==XNET_RESULT_OK,"PINGRESP");}
        else mqttExampleCheck(false,"packet type");
        P->Used-=F.Wire.Size;if(P->Used)memmove(P->Input,P->Input+F.Wire.Size,P->Used);
    }
}
static inline void mqttExampleEnd(xnetstream* S,ptr Data)
{(void)Data;xrtNetStreamAbort(S);}
static inline void mqttExampleClosed(xnetstream* S,xnetresult R,const xerror* Error,ptr Data)
{(void)S;(void)R;(void)Error;xrtAtomic32Store(&((mqttExamplePeer*)Data)->Closed,1,XMEMORY_RELEASE);}
static inline bool mqttExampleAccept(xnetlistener* L,xnetstream* S,ptr Data)
{
    (void)L;mqttExamplePeer* P=Data;static const xnetstreamevents Events={NULL,mqttExampleRead,mqttExampleEnd,NULL,NULL,NULL,mqttExampleClosed};
    mqttExampleCheck(!xrtAtomicPtrLoad(&P->Stream,XMEMORY_ACQUIRE) && xrtNetStreamSetEvents(S,&Events,P),"one peer connection");xrtAtomicPtrStore(&P->Stream,S,XMEMORY_RELEASE);return true;
}
static inline void mqttExampleListenerClosed(xnetlistener* L,ptr Data)
{(void)L;xrtAtomic32Store(&((mqttExamplePeer*)Data)->ListenerClosed,1,XMEMORY_RELEASE);}
static inline void mqttExampleStartMode(mqttExamplePeer* P,xmqttversion Version,bool Echo,bool Error)
{
    memset(P,0,sizeof(*P));P->Version=Version;P->EchoQosOne=Echo;P->ErrorAfterConnack=Error;xrtAtomicPtrInit(&P->Stream,NULL);xrtAtomic32Init(&P->Closed,0);xrtAtomic32Init(&P->ListenerClosed,0);xrtAtomic32Init(&P->Acks,0);xrtAtomic32Init(&P->Disconnects,0);
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;P->Engine=xrtNetEngineCreate(&E);mqttExampleCheck(P->Engine && xrtNetEngineStart(P->Engine),"Engine");
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);L.Distribution=XNET_ACCEPT_LOCAL;
    static const xnetlistenerevents Events={mqttExampleAccept,NULL,mqttExampleListenerClosed};P->Listener=xrtNetListen(P->Engine,&L,&Events,NULL,P);
    mqttExampleCheck(P->Listener && xrtNetListenerLocal(P->Listener,&P->Address),"listener");
}
static inline void mqttExampleStart(mqttExamplePeer* P,xmqttversion Version)
{mqttExampleStartMode(P,Version,false,false);}
static inline void mqttExampleStop(mqttExamplePeer* P)
{
    mqttExampleCheck(xrtNetListenerClose(P->Listener),"listener close");mqttExampleWait(&P->ListenerClosed);mqttExampleWait(&P->Closed);
    xrtNetStreamDestroy(xrtAtomicPtrLoad(&P->Stream,XMEMORY_ACQUIRE));xrtNetListenerDestroy(P->Listener);
    mqttExampleCheck(xrtNetEngineStop(P->Engine) && xrtNetEngineDestroy(P->Engine),"Engine retirement");
}
#endif
