/* Runnable offline TCP/QoS-0 example. No broker installation is required.
 * The local peer accepts CONNECT and echoes PUBLISH; it is deliberately not
 * a production broker or an independent interoperability implementation.
 * python tools/build.py --compiler gcc --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client --no-single
 */
#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct peer {xatomicptr Stream;xatomic32 Closed,ListenerClosed;unsigned char Input[2048];size_t Used;} peer;
static void check(bool OK,const char* Message)
{if(!OK){fprintf(stderr,"MQTT example failed: %s\n",Message);exit(1);}}
static void wait_flag(xatomic32* A)
{double End=xrtTimer()+5;while(!xrtAtomic32Load(A,XMEMORY_ACQUIRE)){check(xrtTimer()<End,"peer retirement");xrtSleep(1);}}
static void peer_read(xnetstream* S,xnetbuf* Buffer,ptr Data)
{
    peer* P=Data;size_t N=xrtNetBufSize(Buffer);check(N<=sizeof(P->Input)-P->Used,"peer input capacity");
    P->Used+=xrtNetBufRead(Buffer,P->Input+P->Used,N);
        while(P->Used){xmqttframe F;xmqttwirestatus Status=xmqttFrameRead((xbytesview){P->Input,P->Used},NULL,&F);check(Status!=XMQTT_WIRE_ERROR,"peer framing");if(Status==XMQTT_WIRE_MORE)break;
            const unsigned char Ack[]={0x20,3,0,0,0};
            check(F.Header.Type==XMQTT_PACKET_CONNECT || F.Header.Type==XMQTT_PACKET_PUBLISH,"peer packet type");
            check(xrtNetStreamSend(S,F.Header.Type==XMQTT_PACKET_CONNECT?Ack:F.Wire.Data,F.Header.Type==XMQTT_PACKET_CONNECT?sizeof(Ack):F.Wire.Size)==XNET_RESULT_OK,"peer reply");
            P->Used-=F.Wire.Size;if(P->Used)memmove(P->Input,P->Input+F.Wire.Size,P->Used);
        }
}
static void peer_end(xnetstream* S,ptr Data)
{(void)Data;xrtNetStreamAbort(S);}
static void peer_closed(xnetstream* S,xnetresult R,const xerror* Error,ptr Data)
{(void)S;(void)R;(void)Error;xrtAtomic32Store(&((peer*)Data)->Closed,1,XMEMORY_RELEASE);}
static bool peer_accept(xnetlistener* L,xnetstream* S,ptr Data)
{
    (void)L;peer* P=Data;static const xnetstreamevents Events={NULL,peer_read,peer_end,NULL,NULL,NULL,peer_closed};
    check(!xrtAtomicPtrLoad(&P->Stream,XMEMORY_ACQUIRE) && xrtNetStreamSetEvents(S,&Events,P),"one peer connection");xrtAtomicPtrStore(&P->Stream,S,XMEMORY_RELEASE);return true;
}
static void peer_listener_closed(xnetlistener* L,ptr Data)
{(void)L;xrtAtomic32Store(&((peer*)Data)->ListenerClosed,1,XMEMORY_RELEASE);}
int main(void)
{
    peer P={0};xrtAtomicPtrInit(&P.Stream,NULL);xrtAtomic32Init(&P.Closed,0);xrtAtomic32Init(&P.ListenerClosed,0);
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);check(Engine && xrtNetEngineStart(Engine),"engine");
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);L.Distribution=XNET_ACCEPT_LOCAL;
    static const xnetlistenerevents Events={peer_accept,NULL,peer_listener_closed};
    xnetlistener* Listener=xrtNetListen(Engine,&L,&Events,NULL,&P);xnetaddr Address;check(Listener && xrtNetListenerLocal(Listener,&Address),"listener");
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("example");
    xmqttclient* C=NULL;check(xmqttClientConnect(Engine,&Address,1,NULL,&Connect,&C),"client factory");xfuture* Ready=xmqttClientReady(C);
    check(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"negotiated CONNACK");
    xmqttpacket Publish;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Publish);Publish.Body.Publish.Topic=XRT_STR_LITERAL("example/topic");Publish.Body.Publish.Payload=XRT_BYTES_LITERAL("hello");
    xfuture* Written=NULL;check(xmqttClientPublish(C,&Publish,5000,&Written)==XMQTT_ADMISSION_ACCEPTED,"publish admission");
    const xmqttwriteresult* Result=xrtFutureValue(Written);check(Result && Result->Status==XMQTT_STATUS_OK,"native write retirement (not a broker ACK)");
    xmqttmessage* Message=NULL;double End=xrtTimer()+5;while(!xmqttClientTakeMessage(C,&Message)){check(xrtTimer()<End,"echo receive");xrtSleep(1);}
    check(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"client physical shutdown");
    check(xrtNetListenerClose(Listener),"listener close");wait_flag(&P.ListenerClosed);wait_flag(&P.Closed);
    xrtNetStreamDestroy(xrtAtomicPtrLoad(&P.Stream,XMEMORY_ACQUIRE));xrtNetListenerDestroy(Listener);check(xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine),"engine shutdown");
    const xmqttpacket* Received=xmqttMessagePacket(Message);check(Received->Body.Publish.Payload.Size==5 && !memcmp(Received->Body.Publish.Payload.Data,"hello",5),"message survives Client/Engine destruction");
    xmqttMessageRelease(Message);xrtFutureDestroy(Written);xrtFutureDestroy(Ready);puts("MQTT 5 TCP: negotiated, QoS-0 sent/received, owners retired, retained payload verified");return 0;
}
