#ifndef XREDIS_SENTINEL_PEER_H
#define XREDIS_SENTINEL_PEER_H
#include "role_client_peer.h"
#include <xredis/sentinel.h>
#include <math.h>
typedef struct sentinelpeer {
    roleclientpeer Peer;redisinitpeerstep Steps[2];char Response[200];bool Started;
} sentinelpeer;
typedef struct sentinelfixture {
    sentinelpeer *Seeds[4],*Masters[3];size_t SeedUsed,MasterUsed,SeedCount,MasterCount;
    size_t Order[16],OrderCount;unsigned Factory;bool Secure; xredisinitmode Mode;
    xredissentinelstatus SeedFailure,MasterFailure;
    xredisclient* Clients[8];size_t ClientCount;
} sentinelfixture;
static inline void sentinelPeerStart(sentinelfixture* F,sentinelpeer* P,bool Seed,cstr Response,bool Hold,bool Abort)
{
    size_t N=0;
    if(F->Mode!=XREDIS_INIT_RESP2)P->Steps[N++]=(redisinitpeerstep){{XRT_BYTES_LITERAL("HELLO"),XRT_BYTES_LITERAL("3")},2,XRT_BYTES_LITERAL(REDIS_INIT_HELLO_REPLY),false,false};
    if(Seed)P->Steps[N++]=(redisinitpeerstep){{XRT_BYTES_LITERAL("SENTINEL"),XRT_BYTES_LITERAL("GET-MASTER-ADDR-BY-NAME"),XRT_BYTES_LITERAL("cache")},3,{(cbytes)Response,strlen(Response)},Hold,Abort};
    else P->Steps[N++]=(redisinitpeerstep){{XRT_BYTES_LITERAL("ROLE")},1,{(cbytes)Response,strlen(Response)},Hold,Abort};
    roleClientPeerStart(&P->Peer,F->Secure,F->Factory!=0,P->Steps,N);P->Started=true;
}
static inline void sentinelFixtureStart(sentinelfixture* F,unsigned Factory,bool Secure,xredisinitmode Mode)
{memset(F,0,sizeof(*F));F->Factory=Factory;F->Secure=Secure;F->Mode=Mode;}
static inline void sentinelMaster(sentinelfixture* F,cstr Reply,bool Hold,bool Abort)
{redisPeerRequire(F->MasterCount<3,"master fixture cap");sentinelpeer* P=calloc(1,sizeof(*P));redisPeerRequire(P!=NULL,"master fixture storage");F->Masters[F->MasterCount++]=P;sentinelPeerStart(F,P,false,Reply,Hold,Abort);}
static inline void sentinelSeed(sentinelfixture* F,cstr Reply,bool Hold,bool Abort)
{redisPeerRequire(F->SeedCount<4,"seed fixture cap");sentinelpeer* P=F->Seeds[F->SeedCount];if(!P){P=calloc(1,sizeof(*P));redisPeerRequire(P!=NULL,"seed fixture storage");F->Seeds[F->SeedCount]=P;}++F->SeedCount;sentinelPeerStart(F,P,true,Reply,Hold,Abort);}
static inline void sentinelAddressSeed(sentinelfixture* F,size_t Master)
{
    sentinelpeer* P=calloc(1,sizeof(*P));redisPeerRequire(P!=NULL,"seed address fixture storage");F->Seeds[F->SeedCount]=P;unsigned Port=roleClientPeerInit(&F->Masters[Master]->Peer)->Peer.Address.Port;
    snprintf(P->Response,sizeof(P->Response),"*2\r\n$9\r\n127.0.0.1\r\n$%u\r\n%u\r\n",Port>=10000?5:Port>=1000?4:Port>=100?3:Port>=10?2:1,Port);sentinelSeed(F,P->Response,false,false);
}
static inline xredissentinelstatus sentinelConnect(sentinelfixture* F,sentinelpeer* P,xnetengine* Engine,int64 Timeout,xredisclient** C)
{
    xredisclientinitconfig Init;xredisClientInitConfigInit(&Init);Init.Mode=F->Mode;Init.Timeout=Timeout;
    xredisclientconfig Config;xredisClientConfigInit(&Config);Config.ReadBytes=1;Config.ReadFrames=1;
    redisinitpeer* I=roleClientPeerInit(&P->Peer);
#if ROLE_PEER_TLS
    if(F->Secure){xredisclienttlsconfig T;redisTlsPeerConfig(&P->Peer.TLS,&T);T.WriteBytes=3;bool OK;
        if(F->Factory==1)OK=xredisClientTlsDial(Engine,P->Peer.TLS.Dial.Resolver,XRT_STR_LITERAL("redis.fixture"),I->Peer.Address.Port,0,&Config,&Init,&T,C);
        else if(F->Factory==2){xnetaddrlist* A=xrtNetAddrListCreate(&I->Peer.Address,1);if(!A)return XREDIS_SENTINEL_MEMORY;OK=xredisClientTlsDialAddresses(Engine,A,0,&Config,&Init,&T,C);xrtNetAddrListDestroy(A);}
        else OK=xredisClientTlsConnect(Engine,&I->Peer.Address,0,&Config,&Init,&T,C);
        return OK?XREDIS_SENTINEL_OK:XREDIS_SENTINEL_MEMORY;}
#endif
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
    if(F->Factory==1)return xredisClientDial(Engine,P->Peer.Dial.Resolver,XRT_STR_LITERAL("redis.fixture"),I->Peer.Address.Port,0,&Config,&Init,NULL,C)?XREDIS_SENTINEL_OK:XREDIS_SENTINEL_MEMORY;
    if(F->Factory==2){xnetaddrlist* A=xrtNetAddrListCreate(&I->Peer.Address,1);if(!A)return XREDIS_SENTINEL_MEMORY;bool OK=xredisClientDialAddresses(Engine,A,0,&Config,&Init,NULL,C);xrtNetAddrListDestroy(A);return OK?XREDIS_SENTINEL_OK:XREDIS_SENTINEL_MEMORY;}
#endif
    return xredisClientConnectInit(Engine,&I->Peer.Address,0,&Config,&Init,C)?XREDIS_SENTINEL_OK:XREDIS_SENTINEL_MEMORY;
}
static inline void sentinelPreviousClosed(sentinelfixture* F)
{
    if(!F->ClientCount)return;
    xredisclientinfo I={0};I.Size=sizeof(I);redisPeerRequire(xredisClientInfo(F->Clients[F->ClientCount-1],&I) && I.State==XREDIS_CLIENT_CLOSED && !I.Requests && !I.InitBytes && !I.CommandBytes,"previous candidate physically retired before factory");
}
static inline xredissentinelstatus sentinelRemember(sentinelfixture* F,xredissentinelstatus S,xredisclient** C)
{if(S==XREDIS_SENTINEL_OK){redisPeerRequire(F->ClientCount<8 && xredisClientRetain(*C),"fixture client observer");F->Clients[F->ClientCount++]=*C;}return S;}
static inline xredissentinelstatus sentinelSeedConnect(xnetengine* E,size_t Seed,int64 Timeout,ptr Data,xredisclient** C)
{
    sentinelfixture* F=Data;sentinelPreviousClosed(F);redisPeerRequire(Timeout>0 && F->OrderCount<16,"bounded seed callback");F->Order[F->OrderCount++]=Seed;
    if(F->SeedFailure)return F->SeedFailure;
    redisPeerRequire(F->SeedUsed<F->SeedCount,"fresh seed connection per query");return sentinelRemember(F,sentinelConnect(F,F->Seeds[F->SeedUsed++],E,Timeout,C),C);
}
static inline xredissentinelstatus sentinelMasterConnect(xnetengine* E,const xredissentinelendpoint* Endpoint,int64 Timeout,ptr Data,xredisclient** C)
{
    sentinelfixture* F=Data;sentinelPreviousClosed(F);redisPeerRequire(Timeout>0 && Endpoint->Host.Size==9 && !memcmp(Endpoint->Host.Data,"127.0.0.1",9),"copied queried endpoint");
    if(F->MasterFailure)return F->MasterFailure;
    redisPeerRequire(F->MasterUsed<F->MasterCount,"fresh master candidate");sentinelpeer* P=F->Masters[F->MasterUsed++];redisPeerRequire(Endpoint->Port==roleClientPeerInit(&P->Peer)->Peer.Address.Port,"queried target port");return sentinelRemember(F,sentinelConnect(F,P,E,Timeout,C),C);
}
static inline void sentinelConfig(sentinelfixture* F,xredissentinelconfig* K)
{xredisSentinelConfigInit(K);K->Service=(xbytesview)XRT_BYTES_LITERAL("cache");K->ConnectSeed=sentinelSeedConnect;K->ConnectMaster=sentinelMasterConnect;K->Data=F;K->Protocol=F->Mode==XREDIS_INIT_RESP2?XREDIS_RESP2:XREDIS_RESP3;K->SeedTimeout=2000;K->RetryDelay=10;}
static inline xredissentineldiscovery* sentinelCreate(sentinelfixture* F,const xredissentinelconfig* K)
{xredissentineldiscovery* D=NULL;redisPeerRequire(xredisSentinelDiscoveryCreate(roleClientPeerInit(&F->Seeds[0]->Peer)->Peer.Engine,K,&D)==XREDIS_ADMISSION_ACCEPTED,"discovery create");return D;}
static inline xredissentinelresult sentinelDrive(xredissentineldiscovery* D)
{
    double End=xrtTimer()+12;
    for(;;){redisPeerRequire(xrtTimer()<End,"discovery terminal deadline");xredissentinelstep S=xredisSentinelDiscoveryStep(D);redisPeerRequire(S!=XREDIS_SENTINEL_STEP_ERROR,"discovery step");if(S==XREDIS_SENTINEL_DONE)break;
        if(S==XREDIS_SENTINEL_WAIT){double Due=xredisSentinelDiscoveryNextWake(D);int64 Ms=(int64)ceil((Due-xrtTimer())*1000);if(Ms<1)Ms=1;if(Ms>10)Ms=10;xfuture* P=xredisSentinelDiscoveryPending(D);if(P){(void)xrtFutureWaitFor(P,Ms);xrtFutureDestroy(P);}else xrtSleep((uint32)Ms);}}
    xredissentinelresult R={0};R.Size=sizeof(R);redisPeerRequire(xredisSentinelDiscoveryResult(D,&R) && xredisSentinelDiscoveryNextWake(D)==0,"terminal result");return R;
}
static inline void sentinelFixtureStop(sentinelfixture* F)
{for(size_t N=0;N<F->ClientCount;++N){xfuture* C=xredisClientClosed(F->Clients[N]);redisPeerRequire(xrtFutureWaitFor(C,10000)==XWAIT_OK,"fixture native physical retirement");xrtFutureDestroy(C);xredisClientRelease(F->Clients[N]);}for(size_t I=0;I<F->MasterCount;++I){roleClientPeerStop(&F->Masters[I]->Peer);free(F->Masters[I]);}for(size_t I=0;I<F->SeedCount;++I){roleClientPeerStop(&F->Seeds[I]->Peer);free(F->Seeds[I]);}}
#endif
