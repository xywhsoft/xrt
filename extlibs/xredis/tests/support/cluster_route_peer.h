#ifndef XREDIS_TEST_CLUSTER_ROUTE_PEER_H
#define XREDIS_TEST_CLUSTER_ROUTE_PEER_H
#include "pool_peer.h"
#include <math.h>
enum {CR_ECHO,CR_MOVED,CR_ASK,CR_WRONG_SLOT,CR_BAD_REDIRECT,CR_DROP,CR_HOLD};
typedef struct crfixture crfixture;
typedef struct crnode {
    crfixture* Fixture; size_t Index;
    xatomic32 Mode,Target,Queries,Prefixes,Denied;
    xatomicptr RedirectHost;
    bool Armed;
} crnode;
struct crfixture {
    redispoolpeer Peer; xredispool* Pools[3]; crnode Nodes[3]; redispeerhook Hooks[3];
    xatomic32 Calls,WaitFirst,Force,Delay;
    cstr Alias;
    xredisclusteraddress History[256]; unsigned char Hosts[256][64]; int64 Budgets[256];
};
static bool crCommand(redispeer* P,xredisreply* Reply,ptr Data)
{
    crnode* N=Data;xredisreplyvalue Root,Name;
    redisPeerRequire(xredisReplyGet(Reply,0,&Root) && xredisReplyGet(Reply,Root.FirstChild,&Name),"route peer command");
    if(redisPeerName(Name,"HELLO"))return redisPoolPeerCommand(P,Reply,NULL);
    if(redisPeerName(Name,"ASKING")){
        redisPeerRequire(!N->Armed,"route ASKING/query adjacency");N->Armed=true;
        if(xrtAtomic32Load(&N->Denied,XMEMORY_ACQUIRE))redisPeerSend(P,"-NOPERM ASKING\r\n",16);
        else redisPeerSend(P,"+OK\r\n",5);
        xrtAtomic32FetchAdd(&N->Prefixes,1,XMEMORY_RELEASE);return true;
    }
    N->Armed=false;uint32 Mode=xrtAtomic32Load(&N->Mode,XMEMORY_ACQUIRE);bool Handled=true;
    if(Mode==CR_MOVED || Mode==CR_ASK || Mode==CR_WRONG_SLOT){
        uint32 Target=xrtAtomic32Load(&N->Target,XMEMORY_ACQUIRE);redisPeerRequire(Target<3,"route target fixture");
        cstr Host=xrtAtomicPtrLoad(&N->RedirectHost,XMEMORY_ACQUIRE);if(!Host)Host="127.0.0.1";
        char Wire[160];int Size=snprintf(Wire,sizeof(Wire),"-%s %u %s:%u\r\n",Mode==CR_ASK?"ASK":"MOVED",Mode==CR_WRONG_SLOT?2u:1u,Host,(unsigned)N->Fixture->Peer.Peers[Target].Address.Port);
        redisPeerRequire(Size>0 && (size_t)Size<sizeof(Wire),"route redirect fixture encoding");redisPeerSend(P,Wire,(size_t)Size);
    }else if(Mode==CR_BAD_REDIRECT)redisPeerSend(P,"-MOVED 1 bad\r\n",14);
    else if(Mode==CR_DROP)xrtNetStreamAbort(P->Stream);
    else {if(Mode==CR_HOLD)P->Blocked=true;Handled=false;}
    xrtAtomic32FetchAdd(&N->Queries,1,XMEMORY_RELEASE);return Handled;
}
static inline crfixture* crStart(void)
{
    crfixture* F=xrtCalloc(1,sizeof(*F));redisPeerRequire(F!=NULL,"route fixture allocation");redisPoolPeerStart(&F->Peer,3);
    for(size_t I=0;I<3;++I){F->Nodes[I].Fixture=F;F->Nodes[I].Index=I;F->Hooks[I]=(redispeerhook){crCommand,&F->Nodes[I]};
        xrtAtomicPtrStore(&F->Peer.Peers[I].Hook,&F->Hooks[I],XMEMORY_RELEASE);
        redisPeerRequire(xredisSharedPoolCreate(F->Peer.Peers[0].Engine,I%2u,&F->Peer.Clients[I],1,NULL,&F->Pools[I])==XREDIS_ADMISSION_ACCEPTED,"route shared node pool");}
    return F;
}
static inline void crMode(crfixture* F,size_t Node,unsigned Mode,size_t Target)
{xrtAtomic32Store(&F->Nodes[Node].Target,(uint32)Target,XMEMORY_RELEASE);xrtAtomic32Store(&F->Nodes[Node].Mode,Mode,XMEMORY_RELEASE);}
static xredisclusterpoolstatus crGetPool(const xredisclusteraddress* A,int64 Remaining,ptr Data,xredispool** Out)
{
    crfixture* F=Data;uint32 Call=xrtAtomic32FetchAdd(&F->Calls,1,XMEMORY_ACQ_REL);
    redisPeerRequire(Remaining>0 && Remaining<=86400000,"route original remaining budget");
    if(Call<256){F->History[Call]=*A;redisPeerRequire(A->Host.Size<=64,"route fixture host");memcpy(F->Hosts[Call],A->Host.Data,A->Host.Size);F->History[Call].Host.Data=F->Hosts[Call];F->Budgets[Call]=Remaining;}
    uint32 Force=xrtAtomic32Load(&F->Force,XMEMORY_ACQUIRE);if(Force)return (xredisclusterpoolstatus)Force;
    uint32 Delay=xrtAtomic32Load(&F->Delay,XMEMORY_ACQUIRE);if(Delay)xrtSleep(Delay); /* Deliberately slow negative fixture. */
    if(Call<xrtAtomic32Load(&F->WaitFirst,XMEMORY_ACQUIRE))return XREDIS_CLUSTER_POOL_WAIT;
    if(A->Tls || !((A->Host.Size==9 && !memcmp(A->Host.Data,"127.0.0.1",9)) ||
        (F->Alias && A->Host.Size==strlen(F->Alias) && !memcmp(A->Host.Data,F->Alias,A->Host.Size))))return XREDIS_CLUSTER_POOL_DENIED;
    for(size_t I=0;I<3;++I)if(A->Port==F->Peer.Peers[I].Address.Port){xredisPoolRetain(F->Pools[I]);*Out=F->Pools[I];return XREDIS_CLUSTER_POOL_READY;}
    return XREDIS_CLUSTER_POOL_DENIED;
}
static inline xredisclustertopology* crTopology(uint16 Port,cstr Host,uint16 First,uint16 Last)
{
    char Wire[400];int N=snprintf(Wire,sizeof(Wire),"*1\r\n*3\r\n:%u\r\n:%u\r\n*2\r\n$%zu\r\n%s\r\n:%u\r\n",(unsigned)First,(unsigned)Last,strlen(Host),Host,(unsigned)Port);
    redisPeerRequire(N>0 && (size_t)N<sizeof(Wire),"route topology fixture encoding");xredisreplyreader* Reader=NULL;xredisreply* Reply=NULL;size_t Used=0;
    redisPeerRequire(xredisReplyReaderCreate(NULL,&Reader) && xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Wire,(size_t)N},&Used,&Reply)==XREDIS_REPLY_READY && Used==(size_t)N,"route topology fixture parse");
    xredisclustertopology* T=NULL;redisPeerRequire(xredisClusterTopologyCreate(Reply,XREDIS_CLUSTER_LEGACY_SLOTS,NULL,&T)==XREDIS_CLUSTER_VALUE,"route topology fixture snapshot");xredisReplyRelease(Reply);xredisReplyReaderDestroy(Reader);return T;
}
static inline void crConfig(crfixture* F,xredisclusterrouteconfig* C)
{xredisClusterRouteConfigInit(C);C->GetPool=crGetPool;C->Data=F;}
static inline xredisclusterrouteresult crDrive(xredisclusterroute* R)
{
    double End=xrtTimer()+15;
    for(;;){redisPeerRequire(xrtTimer()<End,"route drive deadline");xredisclusterroutestep S=xredisClusterRouteStep(R);redisPeerRequire(S!=XREDIS_CLUSTER_ROUTE_STEP_ERROR,"route step");if(S==XREDIS_CLUSTER_ROUTE_DONE)break;
        if(S==XREDIS_CLUSTER_ROUTE_WAIT){double Due=xredisClusterRouteNextWake(R);int64 Ms=(int64)ceil((Due-xrtTimer())*1000);if(Ms<1)Ms=1;if(Ms>10)Ms=10;
            xfuture* P=xredisClusterRoutePending(R);if(P){(void)xrtFutureWaitFor(P,Ms);xrtFutureDestroy(P);}else xrtSleep((uint32)Ms);}}
    xredisclusterrouteresult Result={0};Result.Size=sizeof(Result);redisPeerRequire(xredisClusterRouteResult(R,&Result) && xredisClusterRouteNextWake(R)==0,"route immutable terminal result");return Result;
}
static inline void crStop(crfixture* F)
{
    for(size_t I=0;I<3;++I){xfuture* Closed=xredisPoolClosed(F->Pools[I]);redisPeerRequire(xredisPoolClose(F->Pools[I]) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"route pool physical close");
        xredispoolinfo P={0};P.Size=sizeof(P);redisPeerRequire(xredisPoolInfo(F->Pools[I],&P) && !P.Requests && !P.CommandBytes,"route pool physical quotas");xrtFutureDestroy(Closed);xredisPoolRelease(F->Pools[I]);}
    redisPoolPeerStop(&F->Peer);xrtFree(F);
}
#endif
