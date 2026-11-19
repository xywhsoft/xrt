#ifndef XREDIS_SENTINEL_POOL_PEER_H
#define XREDIS_SENTINEL_POOL_PEER_H
#include "pool_connect_peer.h"
#include <xredis/sentinel_pool.h>
#include <xredis/dial.h>
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
#include "tls_peer.h"
#define SP_TLS 1
#else
#define SP_TLS 0
#endif
typedef struct spfixture spfixture;
typedef struct spnode {redisconnectpeer Base;spfixture* Root;unsigned Kind;} spnode;
struct spfixture {
    spnode Seed,A,B;
    spnode* SeedPeer;
    xfuture* Closed[512];size_t ClosedCount;
    bool ReapClosed;
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
    xnetresolver* Resolver;
#endif
    xatomic32 Current,Queries,Roles,Calls,Drops,QueryMode,Stale,FenceRejects,Notifies;
    xatomic32 PeerQueries,SeedOffline,PeerAttempts,AppBackupReady;
    xatomicptr Pool,Watched;unsigned Factory;bool Secure,Resp2,CloseFromNotify,Replicas;
#if SP_TLS
    redistlspeer TLS;
#endif
};
static void spPeerReply(redispeer* Peer,spfixture* F)
{
    redisPeerRequire(F->SeedPeer!=NULL,"peer-list fixture configured");
    xrtAtomic32FetchAdd(&F->PeerQueries,1,XMEMORY_RELEASE);
    unsigned Count=5;
    unsigned Other=F->SeedPeer->Base.Address.Port;
    unsigned Ports[]={F->Seed.Base.Address.Port,Other,Other,F->B.Base.Address.Port,F->A.Base.Address.Port};
    char Response[512];
    int Length=snprintf(Response,sizeof(Response),"*%u\r\n",Count);
    for(unsigned I=0;I<Count;++I){
        const char* Flags=I==3?"sentinel,s_down,disconnected":"sentinel";
        int N=snprintf(Response+Length,sizeof(Response)-(size_t)Length,"%s\r\n+ip\r\n+127.0.0.1\r\n+port\r\n+%u\r\n+flags\r\n+%s\r\n",
            F->Resp2?"*6":"%3",Ports[I],Flags);
        redisPeerRequire(N>0 && (size_t)(Length+N)<sizeof(Response),"bounded peer list");
        Length+=N;
    }
    redisPeerSend(Peer,Response,(size_t)Length);
}
static bool spCommand(redispeer* Peer,xredisreply* Reply,ptr Data)
{
    spnode* N=Data;spfixture* F=N->Root;xredisreplyvalue Root,Name={0};redisPeerRequire(xredisReplyGet(Reply,0,&Root) && xredisReplyGet(Reply,Root.FirstChild,&Name),"Sentinel pool peer command");
    if(redisPeerName(Name,"HELLO"))return redisConnectPeerCommand(Peer,Reply,&N->Base);
    if(redisPeerName(Name,"SENTINEL")){
        redisPeerRequire(N->Kind==0 && Root.ChildCount==3,"seed query cardinality");xredisreplyvalue Sub,Service;xredisReplyGet(Reply,Name.Next,&Sub);xredisReplyGet(Reply,Sub.Next,&Service);
        redisPeerRequire(redisPeerName(Service,"cache"),"snapshotted service");
        if(redisPeerName(Sub,"SENTINELS")){spPeerReply(Peer,F);return true;}
        redisPeerRequire(redisPeerName(Sub,F->Replicas?"REPLICAS":"GET-MASTER-ADDR-BY-NAME"),"topology query");uint32 Mode=xrtAtomic32Load(&F->QueryMode,XMEMORY_ACQUIRE);xrtAtomic32FetchAdd(&F->Queries,1,XMEMORY_RELEASE);
        switch(Mode){
        case 1:redisPeerSend(Peer,"_\r\n",3);break;case 2:redisPeerSend(Peer,"+bad\r\n",6);break;
        case 3:redisPeerSend(Peer,"-NOPERM synthetic\r\n",19);break;case 4:xrtNetStreamAbort(Peer->Stream);break;case 5:break;
        case 6:redisPeerSend(Peer,"*0\r\n",4);break;
        default:{
            bool Reverse=xrtAtomic32Load(&F->Current,XMEMORY_ACQUIRE)!=0;
            unsigned Port=Reverse?F->B.Base.Address.Port:F->A.Base.Address.Port;
            unsigned Other=Reverse?F->A.Base.Address.Port:F->B.Base.Address.Port;
            char Response[200];int L;
            if(F->Replicas){
                bool Both=Mode==7;
                const char* Row=F->Resp2?"*6":"%3";
                L=snprintf(Response,sizeof(Response),"*%u\r\n%s\r\n+ip\r\n+127.0.0.1\r\n+port\r\n+%u\r\n+flags\r\n+slave\r\n",Both?2u:1u,Row,Port);
                redisPeerRequire(L>0&&(size_t)L<sizeof(Response),"first candidate fixture row");
                if(Both)L+=snprintf(Response+L,sizeof(Response)-(size_t)L,
                    "%s\r\n+ip\r\n+127.0.0.1\r\n+port\r\n+%u\r\n+flags\r\n+slave\r\n",Row,Other);
            }else L=snprintf(Response,sizeof(Response),"*2\r\n+127.0.0.1\r\n+%u\r\n",Port);
            redisPeerRequire(L>0&&(size_t)L<sizeof(Response),"bounded discovery fixture reply");
            redisPeerSend(Peer,Response,(size_t)L);break;
        }}
        return true;
    }
    if(redisPeerName(Name,"ROLE")){
        redisPeerRequire(N->Kind!=0 && Root.ChildCount==1,"target role command");xrtAtomic32FetchAdd(&F->Roles,1,XMEMORY_RELEASE);
        bool Replica=(xrtAtomic32Load(&F->Stale,XMEMORY_ACQUIRE)!=0)!=F->Replicas;
        const char* Text=Replica?"*5\r\n+slave\r\n+127.0.0.1\r\n:6379\r\n+connected\r\n:0\r\n":"*3\r\n+master\r\n:0\r\n*0\r\n";redisPeerSend(Peer,Text,strlen(Text));return true;
    }
    return redisConnectPeerCommand(Peer,Reply,&N->Base);
}
static inline void spStart(spfixture* F,unsigned Factory,bool Secure,bool Resp2)
{
    (void)redisConnectFactoryConfig;memset(F,0,sizeof(*F));F->Factory=Factory;F->Secure=Secure;F->Resp2=Resp2;spnode* Nodes[]={&F->Seed,&F->A,&F->B};
    for(unsigned I=0;I<3;++I){redisConnectPeerStart(&Nodes[I]->Base);Nodes[I]->Root=F;Nodes[I]->Kind=I;Nodes[I]->Base.Hook=(redispeerhook){spCommand,Nodes[I]};}
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
    F->Resolver=xrtNetResolverCreate(NULL);redisPeerRequire(F->Resolver!=NULL,"pool resolver");
#endif
#if SP_TLS
    if(Secure){redisTlsPeerStart(&F->TLS,false,false,0);for(unsigned I=0;I<3;++I)Nodes[I]->Base.TLS=&F->TLS.Options;}
#else
    redisPeerRequire(!Secure,"TLS test module required");
#endif
}
static inline xredissentinelstatus spConnect(spfixture* F,xnetengine* E,const xnetaddr* Address,int64 Timeout,xredisclient** C)
{
    if(F->ReapClosed){
        size_t Live=0;
        for(size_t I=0;I<F->ClosedCount;++I){
            xfuture* Closed=F->Closed[I];
            if(xrtFutureState(Closed)==XFUTURE_RESOLVED)xrtFutureDestroy(Closed);
            else F->Closed[Live++]=Closed;
        }
        F->ClosedCount=Live;
    }
    xredisclientinitconfig I;xredisClientInitConfigInit(&I);I.Timeout=Timeout;I.Mode=F->Resp2?XREDIS_INIT_RESP2:XREDIS_INIT_RESP3;
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
    xredisclientdialconfig D;xredisClientDialConfigInit(&D);D.Family=XNET_FAMILY_IPV4;
#endif
    uint32 Attempt=xrtAtomic32FetchAdd(&F->Calls,1,XMEMORY_RELAXED);uint64 Affinity=Attempt%2;bool OK;
#if SP_TLS
    if(F->Secure){xredisclienttlsconfig T;redisTlsPeerConfig(&F->TLS,&T);T.Session.ServerName=(xstrview)XRT_STR_LITERAL("redis.fixture");T.WriteBytes=3;T.Dial.Family=XNET_FAMILY_IPV4;
        if(F->Factory==1)OK=xredisClientTlsDial(E,F->Resolver,XRT_STR_LITERAL("localhost"),Address->Port,Affinity,NULL,&I,&T,C);
        else if(F->Factory==2){xnetaddrlist* A=xrtNetAddrListCreate(Address,1);if(!A)return XREDIS_SENTINEL_MEMORY;OK=xredisClientTlsDialAddresses(E,A,Affinity,NULL,&I,&T,C);xrtNetAddrListDestroy(A);}
        else OK=xredisClientTlsConnect(E,Address,Affinity,NULL,&I,&T,C);
    }else
#endif
    {
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
        if(F->Factory==1)OK=xredisClientDial(E,F->Resolver,XRT_STR_LITERAL("localhost"),Address->Port,Affinity,NULL,&I,&D,C);
        else if(F->Factory==2){xnetaddrlist* A=xrtNetAddrListCreate(Address,1);if(!A)return XREDIS_SENTINEL_MEMORY;OK=xredisClientDialAddresses(E,A,Affinity,NULL,&I,&D,C);xrtNetAddrListDestroy(A);}
        else
#endif
        OK=xredisClientConnectInit(E,Address,Affinity,NULL,&I,C);
    }
    if(OK){redisPeerRequire(F->ClosedCount<512,"bounded fixture observer list");F->Closed[F->ClosedCount++]=xredisClientClosed(*C);}
    return OK?XREDIS_SENTINEL_OK:XREDIS_SENTINEL_MEMORY;
}
static inline xredissentinelstatus spSeed(xnetengine* E,size_t Seed,int64 Timeout,ptr Data,xredisclient** C)
{
    spfixture* F=Data;
    if(!Seed){
        if(xrtAtomic32Load(&F->SeedOffline,XMEMORY_ACQUIRE))return XREDIS_SENTINEL_UNREACHABLE;
        return spConnect(F,E,&F->Seed.Base.Address,Timeout,C);
    }
    redisPeerRequire(Seed==1 && F->SeedPeer,"configured backup seed index");
    xrtAtomic32FetchAdd(&F->PeerAttempts,1,XMEMORY_RELEASE);
    return spConnect(F,E,&F->SeedPeer->Base.Address,Timeout,C);
}
static inline xredissentinelstatus spMaster(xnetengine* E,const xredissentinelendpoint* A,int64 Timeout,ptr Data,xredisclient** C)
{spfixture* F=Data;redisPeerRequire(A->Host.Size==9 && !memcmp(A->Host.Data,"127.0.0.1",9),"announced target host");xnetaddr Address;redisPeerRequire(xrtNetAddrParse(&Address,"127.0.0.1",A->Port),"target numeric address");return spConnect(F,E,&Address,Timeout,C);}
static inline void spNotify(xredispool* P,const xredissentinelpoolinfo* I,ptr Data)
{
    spfixture* F=Data;xrtAtomic32FetchAdd(&F->Notifies,1,XMEMORY_RELEASE);
    if(I->Phase==XREDIS_SENTINEL_POOL_RECONFIGURING){xredislease* L=xrtAtomicPtrLoad(&F->Watched,XMEMORY_ACQUIRE);
        if(L){xfuture* Out=(xfuture*)(uintptr_t)1;xbytesview Ping=XRT_BYTES_LITERAL("PING");redisPeerRequire(xredisLeaseCallAsync(L,&Ping,1,1000,&Out)==XREDIS_ADMISSION_CLOSED && Out==(xfuture*)(uintptr_t)1,"aggregate fence blocks still-active old lease before physical close");xrtAtomic32FetchAdd(&F->FenceRejects,1,XMEMORY_RELEASE);}
        if(F->CloseFromNotify)xredisPoolClose(P);
    }
}
static inline void spDrop(ptr Data)
{
    spfixture* F=Data;for(size_t N=0;N<F->ClosedCount;++N){redisPeerRequire(xrtFutureState(F->Closed[N])==XFUTURE_RESOLVED,"context drop preceded seed/master physical close");xrtFutureDestroy(F->Closed[N]);F->Closed[N]=NULL;}
    xredispool* P=xrtAtomicPtrLoad(&F->Pool,XMEMORY_ACQUIRE);if(P){xredispoolinfo I={0};I.Size=sizeof(I);redisPeerRequire(xredisPoolInfo(P,&I) && !I.Live && !I.Idle && !I.Leased && !I.Pending && !I.Requests && !I.CommandBytes,"context drop pool resource baseline");}
    xrtAtomic32FetchAdd(&F->Drops,1,XMEMORY_RELEASE);
}
static inline void spConfig(spfixture* F,xredissentinelpoolconfig* K)
{xredisSentinelPoolConfigInit(K);K->Discovery.Service=(xbytesview)XRT_BYTES_LITERAL("cache");K->Discovery.ConnectSeed=spSeed;K->Discovery.ConnectMaster=spMaster;K->Discovery.Data=F;K->Discovery.Protocol=F->Resp2?XREDIS_RESP2:XREDIS_RESP3;K->Discovery.SeedTimeout=2000;K->Discovery.RetryDelay=10;K->RetryMin=5;K->RetryMax=20;K->Notify=spNotify;K->Drop=spDrop;}
static inline xredispool* spCreate(spfixture* F,size_t Count,const xredispoolconfig* Options,const xredissentinelpoolconfig* K)
{
    xredispool* P=NULL;
    xredisadmission A=F->Replicas?xredisSentinelReplicaPoolCreate(F->Seed.Base.Engine,0,Count,Options,K,spMaster,8,&P):
        xredisSentinelPoolCreate(F->Seed.Base.Engine,0,Count,Options,K,&P);
    redisPeerRequire(A==XREDIS_ADMISSION_ACCEPTED,"Sentinel pool create");
    xrtAtomicPtrStore(&F->Pool,P,XMEMORY_RELEASE);return P;
}
static inline const void* spWait(xfuture* F)
{redisPeerRequire(F && xrtFutureWaitFor(F,10000)==XWAIT_OK && xrtFutureValue(F),"Sentinel pool typed wait");return xrtFutureValue(F);}
static inline xfuture* spAcquire(xredispool* P,int64 Timeout)
{xfuture* F=NULL;redisPeerRequire(xredisPoolAcquireAsync(P,Timeout,&F)==XREDIS_ADMISSION_ACCEPTED,"Sentinel pool acquire");return F;}
static inline xredislease* spLease(xfuture* F)
{const xredisleaseresult* R=spWait(F);if(R->Status!=XREDIS_REQUEST_OK)fprintf(stderr,"lease status=%d\n",R->Status);redisPeerRequire(R->Status==XREDIS_REQUEST_OK && R->Lease,"verified native lease");return R->Lease;}
static inline void spPing(xredislease* L)
{xfuture* F=NULL;xbytesview Echo[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("ha\0echo")};redisPeerRequire(xredisLeaseCall(L,Echo,2,1000,&F)==XREDIS_ADMISSION_ACCEPTED && ((const xredisrequestresult*)spWait(F))->Status==XREDIS_REQUEST_OK,"native binary call");xrtFutureDestroy(F);}
static inline void spReturn(xredislease* L,bool Abort)
{xfuture* F=xredisLeaseClosed(L);redisPeerRequire(Abort?xredisLeaseAbort(L):xredisLeaseReturn(L),"lease return/abort");(void)spWait(F);xrtFutureDestroy(F);}
static inline xredissentinelpoolinfo spInfo(xredispool* P)
{xredissentinelpoolinfo I={0};I.Size=sizeof(I);redisPeerRequire(xredisSentinelPoolInfo(P,&I),"Sentinel pool snapshot");return I;}
static inline void spClose(spfixture* F,xredispool* P)
{xfuture* Closed=xredisPoolClosed(P);redisPeerRequire(xredisPoolClose(P) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"Sentinel physical close");xredissentinelpoolinfo I=spInfo(P);redisPeerRequire(I.Phase==XREDIS_SENTINEL_POOL_CLOSED && !I.Connect.Connecting && !I.Connect.Waiting && xrtAtomic32Load(&F->Drops,XMEMORY_ACQUIRE)==1,"Sentinel close/accounting/drop");xrtFutureDestroy(Closed);}
static inline void spStop(spfixture* F)
{
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
    redisPeerRequire(xrtNetResolverDestroy(F->Resolver),"resolver retirement");
#endif
    redisConnectPeerStop(&F->B.Base);redisConnectPeerStop(&F->A.Base);redisConnectPeerStop(&F->Seed.Base);
#if SP_TLS
    if(F->Secure)redisTlsPeerStop(&F->TLS);
#endif
}
#endif
