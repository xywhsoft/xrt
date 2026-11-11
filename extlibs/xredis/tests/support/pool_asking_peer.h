#ifndef XREDIS_POOL_ASKING_PEER_H
#define XREDIS_POOL_ASKING_PEER_H
#include "client_peer.h"
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
#include "tls_peer.h"
#endif
enum {AK_NORMAL,AK_CONTROL_ERROR,AK_QUERY_ERROR,AK_BAD_CONTROL,AK_HOLD_CONTROL,AK_HOLD_QUERY,AK_DROP_PREFIX,AK_DROP_QUERY,AK_PUSH,AK_SYNC,AK_BAD_WIRE,AK_HOLD_BAD_CONTROL};
typedef struct redisaskingpeer {
    redispeer Plain;redispeer* Peer;redispeerhook Hook;xredisclient* Client;xredispool* Pool;xredislease* Lease;
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    redistlspeer TLS;
#endif
    unsigned Mode;xredisinitmode Protocol;bool Secure,Armed,Reentered;xfuture* Reentry;
    xatomic32 Prefixes,Queries,Ordinary,SyncChecked;
} redisaskingpeer;
static bool redisAskingCommand(redispeer* P,xredisreply* R,ptr Data)
{
    redisaskingpeer* F=Data;xredisreplyvalue Root,Name,Arg;xredisReplyGet(R,0,&Root);xredisReplyGet(R,Root.FirstChild,&Name);
    if(redisPeerName(Name,"HELLO")){static const char Hello[]="%3\r\n+server\r\n+redis\r\n+version\r\n+fixture\r\n+proto\r\n:3\r\n";redisPeerSend(P,Hello,sizeof(Hello)-1);return true;}
    if(redisPeerName(Name,"ASKING")){
        redisPeerRequire(!F->Armed && Root.ChildCount==1,"ASKING cannot interleave with a preceding group");F->Armed=true;
        if(F->Mode==AK_SYNC && !F->Reentered){
            F->Reentered=true;xfuture* Failed=(xfuture*)(uintptr_t)1;xbytesview Body[]={XRT_BYTES_LITERAL("APPLY"),XRT_BYTES_LITERAL("nested")};
            redisPeerRequire(xredisLeaseAskingCall(F->Lease,Body,2,5000,&Failed)==XREDIS_ADMISSION_ERROR && Failed==(void*)(uintptr_t)1,"sync ASKING must reject all engine workers");xrtClearError();
            redisPeerRequire(xredisLeaseAskingCallAsync(F->Lease,Body,2,5000,&F->Reentry)==XREDIS_ADMISSION_ACCEPTED,"async ASKING callback reentry");xrtAtomic32Store(&F->SyncChecked,1,XMEMORY_RELEASE);
        }
        if(F->Mode==AK_DROP_PREFIX){xrtNetStreamAbort(P->Stream);xrtAtomic32FetchAdd(&F->Prefixes,1,XMEMORY_RELEASE);return true;}
        if(F->Mode==AK_HOLD_CONTROL || F->Mode==AK_HOLD_BAD_CONTROL)P->Blocked=true;
        if(F->Mode==AK_BAD_CONTROL || F->Mode==AK_HOLD_BAD_CONTROL)redisPeerSend(P,"+wrong\r\n",8);
        else if(F->Mode==AK_BAD_WIRE)redisPeerSend(P,"$1\r\na\rX",7);
        else if(F->Mode==AK_CONTROL_ERROR)redisPeerSend(P,"-NOPERM ASKING denied\r\n",23);
        else if(F->Protocol==XREDIS_INIT_RESP2)redisPeerSend(P,"+OK\r\n",5);else redisPeerSend(P,"|0\r\n+OK\r\n",9);
        if(F->Mode==AK_PUSH || F->Mode==AK_HOLD_QUERY){static const char Push[]=">1\r\n+barrier\r\n";redisPeerSend(P,Push,sizeof(Push)-1);}
        xrtAtomic32FetchAdd(&F->Prefixes,1,XMEMORY_RELEASE);return true;
    }
    if(redisPeerName(Name,"APPLY") || redisPeerName(Name,"APPLYLARGE")){
        redisPeerRequire(F->Armed && Root.ChildCount==2,"query must consume its own adjacent ASKING");F->Armed=false;xredisReplyGet(R,Name.Next,&Arg);
        if(redisPeerName(Name,"APPLYLARGE"))redisPeerRequire(Arg.Data.Size==65536 && Arg.Data.Data[0]=='a' && Arg.Data.Data[65535]=='z',"complete owned fragmented TLS query");
        if(F->Mode==AK_DROP_QUERY)xrtNetStreamAbort(P->Stream);
        else if(F->Mode!=AK_BAD_CONTROL && F->Mode!=AK_BAD_WIRE && F->Mode!=AK_DROP_PREFIX && F->Mode!=AK_HOLD_BAD_CONTROL){
            if(F->Mode==AK_HOLD_QUERY)P->Blocked=true;
            if(F->Mode==AK_QUERY_ERROR)redisPeerSend(P,"-MOVED 1 node:6379\r\n",20);
            else{size_t Bytes=Arg.Data.Size;if(Bytes>16)Bytes=3;char H[40];int N=snprintf(H,sizeof(H),"$%zu\r\n",Bytes);redisPeerSend(P,H,(size_t)N);redisPeerSend(P,Arg.Data.Data,Bytes);redisPeerSend(P,"\r\n",2);}
        }
        xrtAtomic32FetchAdd(&F->Queries,1,XMEMORY_RELEASE);return true;
    }
    redisPeerRequire(!F->Armed,"ordinary request consumed another query's ASKING flag");xrtAtomic32FetchAdd(&F->Ordinary,1,XMEMORY_RELEASE);return false;
}
static inline redisaskingpeer* redisAskingPeerStart(unsigned Mode,bool Secure,xredisinitmode Protocol,const xredisclientconfig* C,const xredispoolconfig* K)
{
    redisaskingpeer* F=xrtCalloc(1,sizeof(*F));redisPeerRequire(F!=NULL,"ASKING fixture allocation");F->Mode=Mode;F->Secure=Secure;F->Protocol=Protocol;
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    if(Secure){redisTlsPeerStart(&F->TLS,false,false,0);F->Peer=&F->TLS.Dial.Init.Peer;}else
#else
    redisPeerRequire(!Secure,"TLS test selection missing");
#endif
    {redisPeerStart(&F->Plain);F->Peer=&F->Plain;}
    F->Hook=(redispeerhook){redisAskingCommand,F};xrtAtomicPtrStore(&F->Peer->Hook,&F->Hook,XMEMORY_RELEASE);
    xredisclientinitconfig I;xredisClientInitConfigInit(&I);I.Mode=Protocol;
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    if(Secure){xredisclienttlsconfig T;redisTlsPeerConfig(&F->TLS,&T);T.WriteBytes=3;redisPeerRequire(xredisClientTlsDial(F->Peer->Engine,F->TLS.Dial.Resolver,XRT_STR_LITERAL("redis.fixture"),F->Peer->Address.Port,0,C,&I,&T,&F->Client),"ASKING TLS connect");}else
#endif
    redisPeerRequire(xredisClientConnectInit(F->Peer->Engine,&F->Peer->Address,0,C,&I,&F->Client),"ASKING numeric connect");
    F->Peer->Client=F->Client;xfuture* Ready=xredisClientReady(F->Client);redisPeerRequire(Ready && xrtFutureWaitFor(Ready,10000)==XWAIT_OK && ((const xredisreadyresult*)xrtFutureValue(Ready))->Status==XREDIS_READY_OK,"ASKING managed readiness");xrtFutureDestroy(Ready);
    redisPeerRequire(xredisPoolCreate(F->Peer->Engine,1,&F->Client,1,K,&F->Pool)==XREDIS_ADMISSION_ACCEPTED,"ASKING native pool");return F;
}
static inline xfuture* redisAskingAcquire(redisaskingpeer* F)
{
    xfuture* A=NULL;redisPeerRequire(xredisPoolAcquire(F->Pool,5000,&A)==XREDIS_ADMISSION_ACCEPTED && ((const xredisleaseresult*)xrtFutureValue(A))->Lease,"ASKING native acquisition");F->Lease=((const xredisleaseresult*)xrtFutureValue(A))->Lease;return A;
}
static inline void redisAskingPeerStop(redisaskingpeer* F)
{
    xfuture* Closed=xredisPoolClosed(F->Pool);redisPeerRequire(xredisPoolClose(F->Pool) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"ASKING physical pool close");
    xredispoolinfo I={0};I.Size=sizeof(I);redisPeerRequire(xredisPoolInfo(F->Pool,&I) && !I.Requests && !I.CommandBytes && !I.Pending && !I.Leased && I.Retired==1,"ASKING pool accounting");
    xredisclientinfo C={0};C.Size=sizeof(C);redisPeerRequire(xredisClientInfo(F->Client,&C) && !C.Requests && !C.CommandBytes && C.Accepted==C.Completed,"ASKING native tails");xredisPoolRelease(F->Pool);xredisClientRelease(F->Client);xrtFutureDestroy(Closed);
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    if(F->Secure)redisTlsPeerStop(&F->TLS);else
#endif
    {redisPeerStop(F->Peer);}
    xrtFree(F);
}
#endif
