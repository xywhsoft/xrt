#ifndef XMQTT_TEST_INBOX_PEER_H
#define XMQTT_TEST_INBOX_PEER_H
#include "../test.h"
/* Literal local peer, not a broker or protocol conformance oracle. */
typedef struct inboxPeer {
    xnetengine* Engine;xnetlistener* Listener;xnetaddr Address;xmqttversion Version;
    xatomicptr Stream;xatomic32 Closed,ListenerClosed,Acks,Recs,Comps;
    unsigned char Input[4096];size_t Used;
} inboxPeer;
static void inboxWait(xatomic32* A,uint32 N)
{double End=xrtTimer()+5;while(xrtAtomic32Load(A,XMEMORY_ACQUIRE)<N){testRequire(xrtTimer()<End,"Inbox peer timeout");xrtSleep(1);}}
static void inboxSend(inboxPeer* P,const void* Data,size_t Bytes)
{testRequire(xrtNetStreamSend(xrtAtomicPtrLoad(&P->Stream,XMEMORY_ACQUIRE),Data,Bytes)==XNET_RESULT_OK,"Inbox peer send");}
static void inboxRead(xnetstream* S,xnetbuf* Buffer,ptr Data)
{
    (void)S;inboxPeer* P=Data;size_t N=xrtNetBufSize(Buffer);testRequire(N<=sizeof(P->Input)-P->Used,"Inbox peer capacity");P->Used+=xrtNetBufRead(Buffer,P->Input+P->Used,N);
    while(P->Used){xmqttwireconfig K;xmqttWireConfigInit(&K);K.Version=P->Version;xmqttframe F;
        xmqttwirestatus T=xmqttFrameRead((xbytesview){P->Input,P->Used},&K,&F);testRequire(T!=XMQTT_WIRE_ERROR,"Inbox peer frame");if(T==XMQTT_WIRE_MORE)break;
        const unsigned char* W=F.Wire.Data;
        if(F.Header.Type==XMQTT_PACKET_CONNECT){const unsigned char A5[]={0x20,3,0,0,0},A3[]={0x20,2,0,0};inboxSend(P,P->Version==XMQTT_VERSION_5?A5:A3,P->Version==XMQTT_VERSION_5?sizeof(A5):sizeof(A3));}
        else if(F.Header.Type==XMQTT_PACKET_PUBLISH){xmqttpacket Q;xmqttproperty Props[8];xmqttcodecbuffers B={Props,8,NULL,0};
            testRequire((P->Version==XMQTT_VERSION_5?xmqttPacketRead5(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&B,&Q,NULL):xmqttPacketRead311(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&B,&Q,NULL))==XMQTT_CODEC_DONE,"Inbox peer publish");
            if((Q.Flags>>1)&3u){unsigned char Ack[]={0x40,2,(unsigned char)(Q.Identifier>>8),(unsigned char)Q.Identifier};inboxSend(P,Ack,sizeof(Ack));}}
        else if(F.Header.Type==XMQTT_PACKET_PUBACK)xrtAtomic32FetchAdd(&P->Acks,1,XMEMORY_RELEASE);
        else if(F.Header.Type==XMQTT_PACKET_PUBREC){testRequire(F.Wire.Size==4,"Inbox PUBREC");unsigned char Rel[]={0x62,2,W[2],W[3]};inboxSend(P,Rel,sizeof(Rel));xrtAtomic32FetchAdd(&P->Recs,1,XMEMORY_RELEASE);}
        else if(F.Header.Type==XMQTT_PACKET_PUBCOMP)xrtAtomic32FetchAdd(&P->Comps,1,XMEMORY_RELEASE);
        else if(F.Header.Type==XMQTT_PACKET_PINGREQ){const unsigned char Pong[]={0xd0,0};inboxSend(P,Pong,sizeof(Pong));}
        else testRequire(F.Header.Type==XMQTT_PACKET_DISCONNECT,"Inbox peer packet type");
        P->Used-=F.Wire.Size;if(P->Used)memmove(P->Input,P->Input+F.Wire.Size,P->Used);
    }
}
static void inboxEnd(xnetstream* S,ptr Data){(void)Data;xrtNetStreamAbort(S);}
static void inboxClosed(xnetstream* S,xnetresult R,const xerror* Error,ptr Data)
{(void)S;(void)R;(void)Error;xrtAtomic32Store(&((inboxPeer*)Data)->Closed,1,XMEMORY_RELEASE);}
static bool inboxAccept(xnetlistener* L,xnetstream* S,ptr Data)
{
    (void)L;inboxPeer* P=Data;static const xnetstreamevents E={NULL,inboxRead,inboxEnd,NULL,NULL,NULL,inboxClosed};
    testRequire(!xrtAtomicPtrLoad(&P->Stream,XMEMORY_ACQUIRE) && xrtNetStreamSetEvents(S,&E,P),"Inbox one accepted stream");xrtAtomicPtrStore(&P->Stream,S,XMEMORY_RELEASE);return true;
}
static void inboxListenerClosed(xnetlistener* L,ptr Data){(void)L;xrtAtomic32Store(&((inboxPeer*)Data)->ListenerClosed,1,XMEMORY_RELEASE);}
static void inboxStart(inboxPeer* P,xmqttversion V)
{
    memset(P,0,sizeof(*P));P->Version=V;xrtAtomicPtrInit(&P->Stream,NULL);xrtAtomic32Init(&P->Closed,0);xrtAtomic32Init(&P->ListenerClosed,0);xrtAtomic32Init(&P->Acks,0);xrtAtomic32Init(&P->Recs,0);xrtAtomic32Init(&P->Comps,0);
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;P->Engine=xrtNetEngineCreate(&E);testRequire(P->Engine && xrtNetEngineStart(P->Engine),"Inbox Engine");
    xnetlistenconfig K;xrtNetListenConfigInit(&K);xrtNetAddrParse(&K.Address,"127.0.0.1",0);K.Distribution=XNET_ACCEPT_LOCAL;
    static const xnetlistenerevents L={inboxAccept,NULL,inboxListenerClosed};P->Listener=xrtNetListen(P->Engine,&K,&L,NULL,P);testRequire(P->Listener && xrtNetListenerLocal(P->Listener,&P->Address),"Inbox listen");
}
static void inboxStop(inboxPeer* P)
{
    testRequire(xrtNetListenerClose(P->Listener),"Inbox listener close");inboxWait(&P->ListenerClosed,1);
    xnetstream* S=xrtAtomicPtrLoad(&P->Stream,XMEMORY_ACQUIRE);if(S)inboxWait(&P->Closed,1);xrtNetStreamDestroy(S);xrtNetListenerDestroy(P->Listener);
    testRequire(xrtNetEngineStop(P->Engine) && xrtNetEngineDestroy(P->Engine),"Inbox Engine retirement");
}
#endif
