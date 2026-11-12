#ifndef XREDIS_TEST_CLUSTER_REFRESH_PEER_H
#define XREDIS_TEST_CLUSTER_REFRESH_PEER_H
#include "cluster_nodes_peer.h"
enum {RF_NORMAL,RF_HOLD,RF_DROP,RF_BAD,RF_DENIED};
typedef struct rffixture rffixture;
typedef struct rfnode {rffixture* Owner;size_t Index;xatomic32 Mode,Queries,Shards,Slots,Target,Installed;xatomicptr LastPeer,Client;} rfnode;
struct rffixture {nnfixture* Base;xredisclusterrefresh* Refresh;rfnode Nodes[3];xatomic32 Action;};
static bool rfCommand(redispeer* P,xredisreply* Reply,ptr Data)
{
    rfnode* N=Data;rffixture* F=N->Owner;xredisreplyvalue Root,Name,Sub;
    redisPeerRequire(xredisReplyGet(Reply,0,&Root) && xredisReplyGet(Reply,Root.FirstChild,&Name),"refresh fixture command");
    if(!redisPeerName(Name,"CLUSTER"))return nnCommand(P,Reply,&F->Base->Nodes[N->Index]);
    redisPeerRequire(xredisReplyGet(Reply,Name.Next,&Sub),"refresh command subcommand");bool Shards=redisPeerName(Sub,"SHARDS");redisPeerRequire(Shards || redisPeerName(Sub,"SLOTS"),"fixed metadata command only");
    xrtAtomic32FetchAdd(Shards?&N->Shards:&N->Slots,1,XMEMORY_RELAXED);uint32 Mode=xrtAtomic32Load(&N->Mode,XMEMORY_ACQUIRE);uint32 Target=xrtAtomic32Load(&N->Target,XMEMORY_ACQUIRE);redisPeerRequire(Target<3,"refresh fixture target");
    if(Mode==RF_DROP)xrtNetStreamAbort(P->Stream);
    else if(Mode==RF_BAD)redisPeerSend(P,"+not-a-topology\r\n",17);
    else if(Mode==RF_DENIED)redisPeerSend(P,"-NOPERM metadata\r\n",18);
    else {char Wire[350];int Count;
        if(Shards)Count=snprintf(Wire,sizeof(Wire),"*1\r\n%%2\r\n$5\r\nslots\r\n*2\r\n:0\r\n:16383\r\n$5\r\nnodes\r\n*1\r\n%%7\r\n$2\r\nid\r\n$1\r\na\r\n$8\r\nendpoint\r\n$0\r\n\r\n$2\r\nip\r\n$9\r\n127.0.0.1\r\n$18\r\nreplication-offset\r\n:0\r\n$4\r\nport\r\n:%u\r\n$4\r\nrole\r\n$6\r\nmaster\r\n$6\r\nhealth\r\n$6\r\nonline\r\n",(unsigned)F->Base->Peers[Target].Address.Port);
        else Count=snprintf(Wire,sizeof(Wire),"*1\r\n*3\r\n:0\r\n:16383\r\n*2\r\n$0\r\n\r\n:%u\r\n",(unsigned)F->Base->Peers[Target].Address.Port);
        redisPeerRequire(Count>0 && (size_t)Count<sizeof(Wire),"refresh wire bounds");if(Mode==RF_HOLD)P->Blocked=true;redisPeerSend(P,Wire,(size_t)Count);}
    xrtAtomicPtrStore(&N->LastPeer,P,XMEMORY_RELEASE);xrtAtomic32FetchAdd(&N->Queries,1,XMEMORY_RELEASE);return true;
}
static void rfInstall(xnetworker* W,ptr Data)
{(void)W;rfnode* N=Data;N->Owner->Base->Peers[N->Index].Hook=(redispeerhook){rfCommand,N};xrtAtomic32Store(&N->Installed,1,XMEMORY_RELEASE);}
static bool rfAuthorize(const xredisclusteraddress* A,ptr Data)
{rffixture* F=Data;bool Allowed=nnAuthorize(A,F->Base);uint32 Action=xrtAtomic32Exchange(&F->Action,0,XMEMORY_ACQ_REL);if(Action==1)xredisClusterRefreshInvalidate(F->Refresh);else if(Action==2)xredisClusterRefreshClose(F->Refresh);return Allowed;}
static bool rfConnect(const xredisclusteraddress* A,xnetengine* E,size_t Slot,uint64 Attempt,int64 Timeout,ptr Data,xredisclient** Out)
{
    rffixture* F=Data;bool OK=nnConnect(A,E,Slot,Attempt,Timeout,F->Base,Out);if(OK){size_t I=0;while(I<3 && A->Port!=F->Base->Peers[I].Address.Port)++I;redisPeerRequire(I<3,"refresh native identity");xredisClientRetain(*Out);xredisclient* Old=xrtAtomicPtrExchange(&F->Nodes[I].Client,*Out,XMEMORY_ACQ_REL);xredisClientRelease(Old);}return OK;
}
static void rfDrop(ptr Data){nnDrop(((rffixture*)Data)->Base);}
static inline rffixture* rfStart(size_t Capacity)
{
    rffixture* F=xrtCalloc(1,sizeof(*F));redisPeerRequire(F!=NULL,"refresh fixture allocation");F->Base=nnStart();
    for(size_t I=0;I<3;++I){F->Nodes[I].Owner=F;F->Nodes[I].Index=I;xrtAtomic32Store(&F->Nodes[I].Target,(uint32)I,XMEMORY_RELAXED);redisPeerRequire(xrtNetEnginePost(F->Base->Peers[I].Engine,1,rfInstall,&F->Nodes[I]),"refresh hook publication");redisPeerWait(&F->Nodes[I].Installed,1);}
    xredisclusternodesconfig C;nnConfig(F->Base,&C);C.MaxNodes=Capacity;C.Authorize=rfAuthorize;C.Connect=rfConnect;C.Drop=rfDrop;C.Data=F;nnCreate(F->Base,&C);return F;
}
static inline xredisclusterrefresh* rfCreate(rffixture* F,size_t Count,xredisclusterrefreshconfig* Config)
{
    xredisclusterrefreshconfig K;if(Config)K=*Config;else xredisClusterRefreshConfigInit(&K);K.RetryDelay=1;xredisclusteraddress A[3];for(size_t I=0;I<Count;++I){A[I]=nnAddress(F->Base,I);A[I].Slot=0;}
    redisPeerRequire(xredisClusterRefreshCreate(F->Base->Cache,A,Count,&K,&F->Refresh)==XREDIS_ADMISSION_ACCEPTED,"refresh coordinator create");return F->Refresh;
}
static inline xfuture* rfBegin(rffixture* F,bool Force)
{xfuture* Out=NULL;redisPeerRequire(xredisClusterRefreshStart(F->Refresh,Force,&Out)==XREDIS_ADMISSION_ACCEPTED,"refresh cohort start");return Out;}
static inline const xredisclusterrefreshresult* rfDrive(rffixture* F,xfuture* Future)
{
    double Due=xrtTimer()+15;
    while(xrtFutureState(Future)==XFUTURE_PENDING){redisPeerRequire(xrtTimer()<Due,"refresh drive watchdog");xredisclusterrefreshstep S=xredisClusterRefreshStep(F->Refresh);redisPeerRequire(S!=XREDIS_REFRESH_STEP_ERROR && S!=XREDIS_REFRESH_BUSY,"serialized refresh driver");
        if(S==XREDIS_REFRESH_WAIT){double Wake=xredisClusterRefreshNextWake(F->Refresh);int64 Ms=(int64)ceil((Wake-xrtTimer())*1000);if(Ms<1)Ms=1;if(Ms>10)Ms=10;xfuture* N=xredisClusterRefreshPending(F->Refresh);if(N){(void)xrtFutureWaitFor(N,Ms);xrtFutureDestroy(N);}else xrtSleep((uint32)Ms);}}
    redisPeerRequire(xrtFutureState(Future)==XFUTURE_RESOLVED && xrtFutureValue(Future),"typed immutable refresh outcome");return xrtFutureValue(Future);
}
static inline xredisclusterrefreshinfo rfInfo(rffixture* F)
{xredisclusterrefreshinfo I={0};I.Size=sizeof(I);redisPeerRequire(xredisClusterRefreshInfo(F->Refresh,&I),"refresh info");return I;}
static inline void rfHeld(rffixture* F,size_t Node)
{
    double Due=xrtTimer()+10;while(!xrtAtomic32Load(&F->Nodes[Node].Queries,XMEMORY_ACQUIRE)){redisPeerRequire(xrtTimer()<Due && xredisClusterRefreshStep(F->Refresh)!=XREDIS_REFRESH_STEP_ERROR,"refresh held query");xfuture* N=xredisClusterRefreshPending(F->Refresh);if(N){(void)xrtFutureWaitFor(N,1);xrtFutureDestroy(N);}else xrtSleep(1);}
}
static inline void rfRelease(rffixture* F,size_t Node)
{redispeer* P=xrtAtomicPtrLoad(&F->Nodes[Node].LastPeer,XMEMORY_ACQUIRE);redisPeerRequire(P!=NULL,"held refresh peer");redisPeerReleaseBlocked(P);}
static inline void rfClose(rffixture* F)
{xfuture* Closed=xredisClusterRefreshClosed(F->Refresh);redisPeerRequire(xredisClusterRefreshClose(F->Refresh) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"refresh logical close");xrtFutureDestroy(Closed);}
static inline void rfStop(rffixture* F)
{
    rfClose(F);nnClose(F->Base);xredisClusterRefreshRelease(F->Refresh);xredisClusterNodesRelease(F->Base->Cache);
    for(size_t I=0;I<3;++I)xredisClientRelease(xrtAtomicPtrLoad(&F->Nodes[I].Client,XMEMORY_ACQUIRE));
    nnStop(F->Base);xrtFree(F);
}
#endif
