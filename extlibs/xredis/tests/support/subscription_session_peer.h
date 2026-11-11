#ifndef XREDIS_SUBSCRIPTION_SESSION_PEER_H
#define XREDIS_SUBSCRIPTION_SESSION_PEER_H
#include "client_peer.h"
#include <xredis/subscription_session.h>
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
#include "tls_peer.h"
#endif
typedef struct sessionkey {bool Present;unsigned Family;size_t Bytes;unsigned char Name[64];} sessionkey;
typedef struct sessionmodel {sessionkey* Keys;size_t Capacity;} sessionmodel;
typedef struct sessionpeer {
    xnetengine* Engine;xnetlistener* Listener;xnetaddr Address;redispeer* Peers;
    sessionmodel Models[16];redispeerhook Hook;xatomic32 Accepted,ListenerClosed,HoldHello,HoldControl,BadControl;
    xatomic32 Controls,Adds,Removes,StableRemoves;bool Resp2;size_t KeyLimit;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
    redistlsoptions* TLS;
#endif
} sessionpeer;
static size_t sessionPeerCount(sessionmodel* M,unsigned Family)
{size_t Count=0;for(size_t N=0;N<M->Capacity;++N)if(M->Keys[N].Present && (Family==2?M->Keys[N].Family==2:M->Keys[N].Family!=2))++Count;return Count;}
static void sessionPeerBulk(redispeer* P,xbytesview V)
{char H[32];int N=snprintf(H,sizeof(H),"$%zu\r\n",V.Size);redisPeerRequire(N>0 && (size_t)N<sizeof(H),"fixture bulk size");redisPeerSend(P,H,(size_t)N);if(V.Size)redisPeerSend(P,V.Data,V.Size);redisPeerSend(P,"\r\n",2);}
static bool sessionPeerCommand(redispeer* P,xredisreply* R,ptr Data)
{
    sessionpeer* F=Data;size_t Index=(size_t)(P-F->Peers);sessionmodel* M=F->Models+Index;
    if(!xrtAtomic32Load(&P->Commands,XMEMORY_RELAXED))memset(M->Keys,0,M->Capacity*sizeof(*M->Keys));
    xredisreplyvalue Root,Word;xredisReplyGet(R,0,&Root);xredisReplyGet(R,Root.FirstChild,&Word);
    if(redisPeerName(Word,"HELLO")){if(!xrtAtomic32Load(&F->HoldHello,XMEMORY_ACQUIRE)){static const char Hello[]="%3\r\n+server\r\n+redis\r\n+version\r\n+fixture\r\n+proto\r\n:3\r\n";redisPeerSend(P,Hello,sizeof(Hello)-1);}return true;}
    static const char* const Commands[]={"SUBSCRIBE","PSUBSCRIBE","SSUBSCRIBE","UNSUBSCRIBE","PUNSUBSCRIBE","SUNSUBSCRIBE"};
    static const char* const Acks[]={"subscribe","psubscribe","ssubscribe","unsubscribe","punsubscribe","sunsubscribe"};
    unsigned Op=0;while(Op<6 && !redisPeerName(Word,Commands[Op]))++Op;if(Op==6)return false;
    xrtAtomic32FetchAdd(&F->Controls,1,XMEMORY_RELEASE);
    if(xrtAtomic32Load(&F->HoldControl,XMEMORY_ACQUIRE))P->Blocked=true;
    if(xrtAtomic32Load(&F->BadControl,XMEMORY_ACQUIRE)){redisPeerSend(P,"-ERR fixture denied\r\n",21);return true;}
    for(size_t Node=Word.Next;Node!=XREDIS_REPLY_NONE;){xredisreplyvalue V;xredisReplyGet(R,Node,&V);sessionkey *Found=NULL,*Empty=NULL;
        for(size_t N=0;N<M->Capacity;++N){sessionkey* K=M->Keys+N;if(!K->Present){if(!Empty)Empty=K;}else if(K->Family==Op%3 && K->Bytes==V.Data.Size && (!K->Bytes || !memcmp(K->Name,V.Data.Data,K->Bytes)))Found=K;}
        if(Op<3){if(!Found){redisPeerRequire(Empty && V.Data.Size<=sizeof(Empty->Name),"fixture key capacity");Found=Empty;Found->Family=Op;Found->Bytes=V.Data.Size;if(V.Data.Size)memcpy(Found->Name,V.Data.Data,V.Data.Size);Found->Present=true;}xrtAtomic32FetchAdd(&F->Adds,1,XMEMORY_RELEASE);}
        else{if(Found)Found->Present=false;xrtAtomic32FetchAdd(&F->Removes,1,XMEMORY_RELEASE);if(V.Data.Size==6 && !memcmp(V.Data.Data,"stable",6))xrtAtomic32FetchAdd(&F->StableRemoves,1,XMEMORY_RELEASE);}
        redisPeerSend(P,F->Resp2?"*3\r\n":">3\r\n",4);sessionPeerBulk(P,(xbytesview){(const unsigned char*)Acks[Op],strlen(Acks[Op])});sessionPeerBulk(P,V.Data);
        char Count[40];int Bytes=snprintf(Count,sizeof(Count),":%zu\r\n",sessionPeerCount(M,Op%3));redisPeerRequire(Bytes>0 && (size_t)Bytes<sizeof(Count),"fixture count");redisPeerSend(P,Count,(size_t)Bytes);Node=V.Next;
    }
    return true;
}
static bool sessionPeerAccept(xnetlistener* L,xnetstream* Stream,ptr Data)
{
    (void)L;sessionpeer* F=Data;uint32 Count=xrtAtomic32Load(&F->Accepted,XMEMORY_RELAXED);redispeer* P=F->Peers+Count%16;
    if(P->Stream){redisPeerRequire(xrtAtomic32Load(&P->Closed,XMEMORY_ACQUIRE)!=0,"old server generation still live");
#if defined(XREDIS_FEATURE_XREDIS_TLS)
        if(P->Secure)xrtTlsStreamDestroy(P->Secure);else
#endif
        xrtNetStreamDestroy(P->Stream);
        xredisReplyReaderDestroy(P->Reader);}
    sessionmodel* M=F->Models+Count%16;if(M->Capacity!=F->KeyLimit){xrtFree(M->Keys);M->Keys=xrtCalloc(F->KeyLimit,sizeof(*M->Keys));redisPeerRequire(M->Keys!=NULL,"model storage");M->Capacity=F->KeyLimit;}
    memset(P,0,sizeof(*P));P->Engine=F->Engine;xredisreplyreaderconfig K;xredisReplyReaderConfigInit(&K);redisPeerRequire(xredisReplyReaderCreate(&K,&P->Reader),"session fixture parser");xrtAtomicPtrStore(&P->Hook,&F->Hook,XMEMORY_RELEASE);
#if defined(XREDIS_FEATURE_XREDIS_TLS)
    xrtAtomicPtrStore(&P->TlsOptions,F->TLS,XMEMORY_RELEASE);
#endif
    redisPeerRequire(redisPeerAccept(NULL,Stream,P),"session fixture accept");xrtAtomic32Store(&F->Accepted,Count+1,XMEMORY_RELEASE);return true;
}
static void sessionPeerClosed(xnetlistener* L,ptr Data){(void)L;xrtAtomic32Store(&((sessionpeer*)Data)->ListenerClosed,1,XMEMORY_RELEASE);}
static const xnetlistenerevents sessionPeerEvents={sessionPeerAccept,NULL,sessionPeerClosed};
static void sessionPeerStart(sessionpeer* F)
{
    (void)redisPeerStart;(void)redisPeerStop;memset(F,0,sizeof(*F));F->KeyLimit=64;F->Peers=xrtCalloc(16,sizeof(*F->Peers));redisPeerRequire(F->Peers!=NULL,"session peers");F->Hook=(redispeerhook){sessionPeerCommand,F};
    xnetengineconfig K;xrtNetEngineConfigInit(&K);K.Workers=2;K.NodeCacheBytes=0;F->Engine=xrtNetEngineCreate(&K);redisPeerRequire(F->Engine && xrtNetEngineStart(F->Engine),"session engine");
    xnetlistenconfig L;xrtNetListenConfigInit(&L);L.Affinity=1;L.Distribution=XNET_ACCEPT_LOCAL;xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0);F->Listener=xrtNetListen(F->Engine,&L,&sessionPeerEvents,NULL,F);redisPeerRequire(F->Listener && xrtNetListenerLocal(F->Listener,&F->Address),"session listener");
}
static void sessionPeerStop(sessionpeer* F)
{
    redisPeerRequire(xrtNetListenerClose(F->Listener),"session listener close");redisPeerWait(&F->ListenerClosed,1);
    for(size_t N=0;N<16;++N)if(F->Peers[N].Stream){xrtNetStreamAbort(F->Peers[N].Stream);redisPeerWait(&F->Peers[N].Closed,1);}
    redisPeerRequire(xrtNetEngineStop(F->Engine),"session engine stop");
    for(size_t N=0;N<16;++N){redispeer* P=F->Peers+N;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
        if(P->Secure)xrtTlsStreamDestroy(P->Secure);else
#endif
        xrtNetStreamDestroy(P->Stream);
        xredisReplyReaderDestroy(P->Reader);xrtFree(F->Models[N].Keys);}
    xrtNetListenerDestroy(F->Listener);xrtFree(F->Peers);redisPeerRequire(xrtNetEngineDestroy(F->Engine),"session engine destroy");
}
typedef struct sessionfactory {
    sessionpeer* Peer;xatomic32 Calls,Drops,Notifications;uint32 FailFirst;bool Raw,CloseOnline;
    double Times[256];xfuture* Closed[256];xredisclient* Preclosed;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
    const xredisclienttlsconfig* TLS;
#endif
} sessionfactory;
static bool sessionFactoryConnect(xnetengine* E,uint64 Attempt,int64 Timeout,ptr Data,xredisclient** C)
{
    sessionfactory* F=Data;uint32 N=xrtAtomic32Load(&F->Calls,XMEMORY_RELAXED);redisPeerRequire(xrtNetEngineCurrent(E)!=NULL && Attempt==(uint64)N+1,"serialized worker factory attempt");if(N<256)F->Times[N]=xrtTimer();xrtAtomic32Store(&F->Calls,N+1,XMEMORY_RELEASE);if(N<F->FailFirst)return false;
    xredisclientconfig K;xredisClientConfigInit(&K);K.ReadBytes=3;K.ReadFrames=1;K.WorkRequests=1;K.MaxPushes=128;xredisclientinitconfig Init;xredisClientInitConfigInit(&Init);Init.Timeout=Timeout;if(F->Peer->Resp2)Init.Mode=XREDIS_INIT_RESP2;bool OK;
    if(F->Preclosed){*C=F->Preclosed;F->Preclosed=NULL;OK=true;}
#if defined(XREDIS_FEATURE_XREDIS_TLS)
    else if(F->TLS)OK=xredisClientTlsConnect(E,&F->Peer->Address,1,&K,&Init,F->TLS,C);
#endif
    else OK=F->Raw?xredisClientConnect(E,&F->Peer->Address,1,&K,C):xredisClientConnectInit(E,&F->Peer->Address,1,&K,&Init,C);
    if(OK && N<256)F->Closed[N]=xredisClientClosed(*C);
    return OK;
}
static void sessionFactoryNotify(xredissubscriptionsession* S,const xredissubscriptionsessioninfo* I,ptr Data)
{
    sessionfactory* F=Data;redisPeerRequire(xrtNetEngineCurrent(F->Peer->Engine)!=NULL,"notification worker");xrtAtomic32FetchAdd(&F->Notifications,1,XMEMORY_RELEASE);
    xredissubscriptionsessioninfo V={0};V.Size=sizeof(V);redisPeerRequire(xredisSubscriptionSessionInfo(S,&V),"callback reentrant info");
    if(F->CloseOnline && I->State==XREDIS_SESSION_ONLINE)redisPeerRequire(xredisSubscriptionSessionClose(S),"callback close");
}
static void sessionFactoryDrop(ptr Data)
{
    sessionfactory* F=Data;redisPeerRequire(xrtNetEngineCurrent(F->Peer->Engine)!=NULL,"context drop worker");
    if(F->Preclosed){xredisClientRelease(F->Preclosed);F->Preclosed=NULL;}
    for(size_t N=0;N<256;++N)if(F->Closed[N]){redisPeerRequire(xrtFutureDone(F->Closed[N]),"drop before candidate physical close");xrtFutureDestroy(F->Closed[N]);F->Closed[N]=NULL;}
    xrtAtomic32FetchAdd(&F->Drops,1,XMEMORY_RELEASE);
}
static void sessionFactoryConfig(sessionfactory* F,sessionpeer* P,xredissubscriptionsessionconfig* K)
{memset(F,0,sizeof(*F));F->Peer=P;xredisSubscriptionSessionConfigInit(K);K->Connect=sessionFactoryConnect;K->Notify=sessionFactoryNotify;K->Drop=sessionFactoryDrop;K->Data=F;K->RetryMin=K->RetryMax=10;if(P->Resp2)K->Protocol=XREDIS_RESP2;}
#endif
