#include "../../tests/support/sentinel_pool_peer.h"
#include "seed_policy.h"
static void spExample(bool Replicas)
{
    spfixture F;spStart(&F,0,false,false);F.Replicas=Replicas;xredissentinelpoolconfig K;spConfig(&F,&K);xredispool* P=spCreate(&F,1,NULL,&K);
    xfuture* First=spAcquire(P,5000);spPing(spLease(First));xrtAtomic32Store(&F.Current,1,XMEMORY_RELEASE);
    redisPeerRequire(xredisSentinelPoolRefresh(P)==XREDIS_ADMISSION_ACCEPTED,"request verified refresh");
    xfuture* Next=spAcquire(P,5000);spPing(spLease(Next));redisPeerRequire(spInfo(P).Generation==2,"automatic generation replacement");spReturn(spLease(Next),false);
    spClose(&F,P);spStop(&F);xredisPoolRelease(P);xrtFutureDestroy(First);xrtFutureDestroy(Next);printf("Native pool rediscovered and replaced its %s generation\n",Replicas?"replica":"master");
}
static xredissentinelstatus appPoolSeed(xnetengine* Engine, size_t Index, int64 Timeout,
    ptr Context, xredisclient** Client)
{
    spfixture* Peers = Context;
    if (Index && !xrtAtomic32Load(&Peers->AppBackupReady, XMEMORY_ACQUIRE))
        return XREDIS_SENTINEL_UNREACHABLE;
    return spSeed(Engine, Index, Timeout, Peers, Client);
}

static bool appRefreshSeed(spfixture* Peers, xbytesview Service, uint16 ApprovedPort)
{
    xredisclientinitconfig Init;
    xredisClientInitConfigInit(&Init);
    Init.Mode = XREDIS_INIT_RESP3;
    Init.Timeout = 2000;
    xredisclient* Seed = NULL;
    if (!xredisClientConnectInit(Peers->Seed.Base.Engine, &Peers->Seed.Base.Address, 0, NULL, &Init, &Seed))
        return false;
    bool Listed = appSeedRead(Seed, Service, ApprovedPort);
    xfuture* Closed = xredisClientClosed(Seed);
    xredisClientClose(Seed);
    xredisClientRelease(Seed);
    bool Retired = Closed && xrtFutureWaitFor(Closed, 2000) == XWAIT_OK;
    xrtFutureDestroy(Closed);
    if (Listed && Retired) xrtAtomic32Store(&Peers->AppBackupReady, 1, XMEMORY_RELEASE);
    return Listed && Retired;
}

static void spSeedExample(void)
{
    spfixture F;spStart(&F,0,false,false);
    spnode Peer={0};redisConnectPeerStart(&Peer.Base);
    Peer.Root=&F;Peer.Base.Hook=(redispeerhook){spCommand,&Peer};F.SeedPeer=&Peer;
    xredissentinelpoolconfig K;
    spConfig(&F,&K);
    K.Discovery.SeedCount=2;
    K.Discovery.ConnectSeed=appPoolSeed;
    xredispool* P=spCreate(&F,1,NULL,&K);
    xfuture* First=spAcquire(P,5000);
    spPing(spLease(First));
    redisPeerRequire(appRefreshSeed(&F,K.Discovery.Service,Peer.Base.Address.Port),
        "activate the approved backup while the pool serves the first lease");
    xrtAtomic32Store(&F.SeedOffline,1,XMEMORY_RELEASE);
    spReturn(spLease(First),true);
    xfuture* Next=spAcquire(P,5000);
    spPing(spLease(Next));
    redisPeerRequire(spInfo(P).PreferredSeed==1,"reconnected using the APP-selected backup");
    spClose(&F,P);redisConnectPeerStop(&Peer.Base);spStop(&F);
    xredisPoolRelease(P);xrtFutureDestroy(First);xrtFutureDestroy(Next);
    puts("Native pool recovered using a Sentinel seed selected by the APP");
}
int main(void)
{spExample(false);spExample(true);spSeedExample();return 0;}
