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
    spnode Seed,A,B;xfuture* Closed[512];size_t ClosedCount;
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
    xnetresolver* Resolver;
#endif
    xatomic32 Current,Queries,Roles,Calls,Drops,QueryMode,Stale,FenceRejects,Notifies;
    xatomicptr Pool,Watched;unsigned Factory;bool Secure,Resp2,CloseFromNotify;
#if SP_TLS
    redistlspeer TLS;
#endif
};
static bool spCommand(redispeer* Peer,xredisreply* Reply,ptr Data)
{
    spnode* N=Data;spfixture* F=N->Root;xredisreplyvalue Root,Name;redisPeerRequire(xredisReplyGet(Reply,0,&Root) && xredisReplyGet(Reply,Root.FirstChild,&Name),"Sentinel pool peer command");
    if(redisPeerName(Name,"HELLO"))return redisConnectPeerCommand(Peer,Reply,&N->Base);
    if(redisPeerName(Name,"SENTINEL")){
        redisPeerRequire(N->Kind==0 && Root.ChildCount==3,"seed query cardinality");xredisreplyvalue Sub,Service;xredisReplyGet(Reply,Name.Next,&Sub);xredisReplyGet(Reply,Sub.Next,&Service);
        redisPeerRequire(redisPeerName(Sub,"GET-MASTER-ADDR-BY-NAME") && redisPeerName(Service,"cache"),"snapshotted service query");xrtAtomic32FetchAdd(&F->Queries,1,XMEMORY_RELEASE);
        switch(xrtAtomic32Load(&F->QueryMode,XMEMORY_ACQUIRE)){
        case 1:redisPeerSend(Peer,"_\r\n",3);break;case 2:redisPeerSend(Peer,"+bad\r\n",6);break;
        case 3:redisPeerSend(Peer,"-NOPERM synthetic\r\n",19);break;case 4:xrtNetStreamAbort(Peer->Stream);break;case 5:break;
        default:{unsigned Port=xrtAtomic32Load(&F->Current,XMEMORY_ACQUIRE)?F->B.Base.Address.Port:F->A.Base.Address.Port;char Response[100];int L=snprintf(Response,sizeof(Response),"*2\r\n+127.0.0.1\r\n+%u\r\n",Port);redisPeerSend(Peer,Response,(size_t)L);break;}}
        return true;
    }
    if(redisPeerName(Name,"ROLE")){
        redisPeerRequire(N->Kind!=0 && Root.ChildCount==1,"target role command");xrtAtomic32FetchAdd(&F->Roles,1,XMEMORY_RELEASE);
        const char* Text=xrtAtomic32Load(&F->Stale,XMEMORY_ACQUIRE)?"*5\r\n+slave\r\n+127.0.0.1\r\n:6379\r\n+connected\r\n:0\r\n":"*3\r\n+master\r\n:0\r\n*0\r\n";redisPeerSend(Peer,Text,strlen(Text));return true;
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
{spfixture* F=Data;redisPeerRequire(Seed==0,"one seed index");return spConnect(F,E,&F->Seed.Base.Address,Timeout,C);}
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
{xredispool* P=NULL;redisPeerRequire(xredisSentinelPoolCreate(F->Seed.Base.Engine,0,Count,Options,K,&P)==XREDIS_ADMISSION_ACCEPTED,"Sentinel pool create");xrtAtomicPtrStore(&F->Pool,P,XMEMORY_RELEASE);return P;}
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
