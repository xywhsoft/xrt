#ifndef XREDIS_TEST_CLUSTER_NODES_PEER_H
#define XREDIS_TEST_CLUSTER_NODES_PEER_H
#include "pool_connect_peer.h"
#include "cluster_route_peer.h"
typedef struct nnfixture nnfixture;
typedef struct nnnode {nnfixture* Owner;size_t Index;xatomic32 Redirect,Target,Prefixes,Installed;} nnnode;
struct nnfixture {
    redisconnectpeer Peers[3];nnnode Nodes[3];xredisclusternodes* Cache;
    xatomic32 Calls,Auth,Drops,Deny,Block,Entered,Continue,CloseInAuth,Go,HoldBusiness,Business,AuthExited;
    xfuture* ClientClosed[1024];
};
static bool nnCommand(redispeer* P,xredisreply* Reply,ptr Data)
{
    nnnode* N=Data;nnfixture* F=N->Owner;xredisreplyvalue Root,Name;
    redisPeerRequire(xredisReplyGet(Reply,0,&Root) && xredisReplyGet(Reply,Root.FirstChild,&Name),"nodes fixture command");
    if(redisPeerName(Name,"HELLO"))return redisConnectPeerCommand(P,Reply,&F->Peers[N->Index]);
    if(redisPeerName(Name,"ASKING")){redisPeerSend(P,"+OK\r\n",5);xrtAtomic32FetchAdd(&N->Prefixes,1,XMEMORY_RELEASE);return true;}
    uint32 Kind=xrtAtomic32Load(&N->Redirect,XMEMORY_ACQUIRE);
    if(Kind){uint32 Target=xrtAtomic32Load(&N->Target,XMEMORY_ACQUIRE);redisPeerRequire(Target<3,"nodes redirect target");char Wire[100];int Size=snprintf(Wire,sizeof(Wire),"-%s 1 127.0.0.1:%u\r\n",Kind==1?"MOVED":"ASK",(unsigned)F->Peers[Target].Address.Port);redisPeerSend(P,Wire,(size_t)Size);return true;}
    xrtAtomic32FetchAdd(&F->Business,1,XMEMORY_RELEASE);if(xrtAtomic32Load(&F->HoldBusiness,XMEMORY_ACQUIRE))P->Blocked=true;
    return false;
}
static bool nnAuthorize(const xredisclusteraddress* A,ptr Data)
{
    nnfixture* F=Data;xrtAtomic32FetchAdd(&F->Auth,1,XMEMORY_RELAXED);
    xredisclusternodesinfo I={0};I.Size=sizeof(I);redisPeerRequire(xredisClusterNodesInfo(F->Cache,&I),"authorization outside gate/reentrant info");
    if(xrtAtomic32Load(&F->CloseInAuth,XMEMORY_ACQUIRE))xredisClusterNodesClose(F->Cache);
    if(xrtAtomic32Load(&F->Block,XMEMORY_ACQUIRE)){xrtAtomic32Store(&F->Entered,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&F->Continue,XMEMORY_ACQUIRE))xrtSleep(1);}
    bool Allowed=!A->Tls && !xrtAtomic32Load(&F->Deny,XMEMORY_ACQUIRE) && A->Host.Size==9 && !memcmp(A->Host.Data,"127.0.0.1",9),Known=false;
    if(Allowed)for(unsigned N=0;N<3;++N)if(A->Port==F->Peers[N].Address.Port){Known=true;break;}
    xrtAtomic32FetchAdd(&F->AuthExited,1,XMEMORY_RELEASE);return Allowed && Known;
}
static bool nnConnect(const xredisclusteraddress* A,xnetengine* E,size_t Slot,uint64 Attempt,int64 Timeout,ptr Data,xredisclient** Out)
{
    nnfixture* F=Data;(void)Attempt;redisPeerRequire(A->Host.Size==9 && !memcmp(A->Host.Data,"127.0.0.1",9) && !A->Tls && !A->Slot && !A->Reason,"stable copied node identity");
    uint32 Call=xrtAtomic32FetchAdd(&F->Calls,1,XMEMORY_ACQ_REL);redisPeerRequire(Call<1024,"nodes fixture churn bound");xnetaddr Address;redisPeerRequire(xrtNetAddrParse(&Address,"127.0.0.1",A->Port),"nodes numeric factory");xredisclientinitconfig C;xredisClientInitConfigInit(&C);C.Timeout=Timeout;
    bool OK=xredisClientConnectInit(E,&Address,Slot%2,NULL,&C,Out);if(OK)F->ClientClosed[Call]=xredisClientClosed(*Out);return OK;
}
static void nnDrop(ptr Data)
{
    nnfixture* F=Data;for(size_t I=0;I<1024;++I)if(F->ClientClosed[I]){redisPeerRequire(xrtFutureState(F->ClientClosed[I])==XFUTURE_RESOLVED,"cache Drop preceded client physical close");xrtFutureDestroy(F->ClientClosed[I]);F->ClientClosed[I]=NULL;}
    xredisclusternodesinfo Info={0};Info.Size=sizeof(Info);redisPeerRequire(xredisClusterNodesInfo(F->Cache,&Info) && !Info.Nodes && !Info.Opening && !Info.APIs,"cache Drop reentry/context duties");xrtAtomic32FetchAdd(&F->Drops,1,XMEMORY_RELEASE);
}
static void nnInstallHook(xnetworker* Worker,ptr Data)
{
    (void)Worker;nnnode* N=Data;N->Owner->Peers[N->Index].Hook=(redispeerhook){nnCommand,N};
    xrtAtomic32Store(&N->Installed,1,XMEMORY_RELEASE);
}
static inline nnfixture* nnStart(void)
{
    (void)redisConnectFactoryConfig;nnfixture* F=xrtCalloc(1,sizeof(*F));redisPeerRequire(F!=NULL,"nodes fixture allocation");
    for(size_t I=0;I<3;++I){redisConnectPeerStart(&F->Peers[I]);F->Nodes[I].Owner=F;F->Nodes[I].Index=I;redisPeerRequire(xrtNetEnginePost(F->Peers[I].Engine,1,nnInstallHook,&F->Nodes[I]),"install hook on listener owner");redisPeerWait(&F->Nodes[I].Installed,1);}return F;
}
static inline void nnConfig(nnfixture* F,xredisclusternodesconfig* C)
{xredisClusterNodesConfigInit(C);C->MaxNodes=3;C->MaxHostBytes=64;C->Authorize=nnAuthorize;C->Connect=nnConnect;C->Drop=nnDrop;C->Data=F;C->RetryMin=C->RetryMax=5;C->Pool.MaxRequests=64;C->Pool.MaxCommandBytes=1048576;}
static inline xredisclusteraddress nnAddress(nnfixture* F,size_t I)
{return (xredisclusteraddress){XRT_BYTES_LITERAL("127.0.0.1"),F->Peers[I].Address.Port,1,false,0};}
static inline xredisclusternodes* nnCreate(nnfixture* F,xredisclusternodesconfig* C)
{redisPeerRequire(xredisClusterNodesCreate(F->Peers[0].Engine,0,C,&F->Cache)==XREDIS_ADMISSION_ACCEPTED,"node cache creation");return F->Cache;}
static inline xredisclusternodesinfo nnInfo(xredisclusternodes* C)
{xredisclusternodesinfo I={0};I.Size=sizeof(I);redisPeerRequire(xredisClusterNodesInfo(C,&I),"nodes info");return I;}
static inline void nnReady(xredispool* P,size_t Count)
{double Due=xrtTimer()+10;for(;;){xredispoolinfo I={0};I.Size=sizeof(I);redisPeerRequire(xredisPoolInfo(P,&I) && I.State==XREDIS_POOL_OPEN && xrtTimer()<Due,"node pool readiness");if(I.Idle==Count)return;xrtSleep(1);}}
static inline xfuture* nnEcho(xredispool* P)
{xbytesview A[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("node\0bytes")};xfuture* F=NULL;redisPeerRequire(xredisSharedPoolCall(P,A,2,5000,&F)==XREDIS_ADMISSION_ACCEPTED && ((const xredisrequestresult*)xrtFutureValue(F))->Status==XREDIS_REQUEST_OK,"cached native call");return F;}
static inline void nnClose(nnfixture* F)
{xfuture* Closed=xredisClusterNodesClosed(F->Cache);redisPeerRequire(xredisClusterNodesClose(F->Cache) && xrtFutureWaitFor(Closed,15000)==XWAIT_OK && nnInfo(F->Cache).State==XREDIS_CLUSTER_NODES_CLOSED,"node cache physical close");xrtFutureDestroy(Closed);}
static inline void nnStop(nnfixture* F)
{for(size_t I=0;I<3;++I)redisConnectPeerStop(&F->Peers[I]);xrtFree(F);}
#endif
