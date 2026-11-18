#include "../../tests/support/sentinel_pool_peer.h"
static void spExample(bool Replicas)
{
    spfixture F;spStart(&F,0,false,false);F.Replicas=Replicas;xredissentinelpoolconfig K;spConfig(&F,&K);xredispool* P=spCreate(&F,1,NULL,&K);
    xfuture* First=spAcquire(P,5000);spPing(spLease(First));xrtAtomic32Store(&F.Current,1,XMEMORY_RELEASE);
    redisPeerRequire(xredisSentinelPoolRefresh(P)==XREDIS_ADMISSION_ACCEPTED,"request verified refresh");
    xfuture* Next=spAcquire(P,5000);spPing(spLease(Next));redisPeerRequire(spInfo(P).Generation==2,"automatic generation replacement");spReturn(spLease(Next),false);
    spClose(&F,P);spStop(&F);xredisPoolRelease(P);xrtFutureDestroy(First);xrtFutureDestroy(Next);printf("Native pool rediscovered and replaced its %s generation\n",Replicas?"replica":"master");
}
static void spSeedExample(void)
{
    spfixture F;spStart(&F,0,false,false);
    spnode Peer={0};redisConnectPeerStart(&Peer.Base);
    Peer.Root=&F;Peer.Base.Hook=(redispeerhook){spCommand,&Peer};F.SeedPeer=&Peer;
    xredissentinelpoolconfig K;spConfig(&F,&K);
    xredissentinelendpoint Initial={XRT_BYTES_LITERAL("127.0.0.1"),F.Seed.Base.Address.Port};
    xredissentinelpoolseedconfig Seeds;
    redisPeerRequire(xredisSentinelPoolSeedConfigInit(&Seeds),"seed config");
    Seeds.Seeds=&Initial;Seeds.Connect=spSeedEndpoint;Seeds.MaxSeeds=2;
    xredispool* P=NULL;
    redisPeerRequire(xredisSentinelPoolCreateWithSeeds(F.Seed.Base.Engine,0,1,NULL,&K,&Seeds,&P)==XREDIS_ADMISSION_ACCEPTED,
        "pool with endpoint seed factory");
    xrtAtomicPtrStore(&F.Pool,P,XMEMORY_RELEASE);
    xfuture* First=spAcquire(P,5000);
    spPing(spLease(First));
    xredissentinelpoolseedinfo I={.Size=sizeof(I)};
    redisPeerRequire(xredisSentinelPoolSeedInfo(P,&I) && I.Configured==1 && I.Known==2,"learned one Sentinel");
    xrtAtomic32Store(&F.SeedOffline,1,XMEMORY_RELEASE);
    spReturn(spLease(First),true);
    xfuture* Next=spAcquire(P,5000);
    spPing(spLease(Next));
    redisPeerRequire(spInfo(P).PreferredSeed==1,"reconnected using learned Sentinel");
    spClose(&F,P);redisConnectPeerStop(&Peer.Base);spStop(&F);
    xredisPoolRelease(P);xrtFutureDestroy(First);xrtFutureDestroy(Next);
    puts("Native pool recovered using a learned Sentinel seed");
}
int main(void)
{spExample(false);spExample(true);spSeedExample();return 0;}
