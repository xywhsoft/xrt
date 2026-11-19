#include "../../tests/support/sentinel_peer.h"
#include "replica_policy.h"
static void sentinelExample(bool Replica)
{
    sentinelfixture F;sentinelFixtureStart(&F,0,false,XREDIS_INIT_RESP3);
    F.Replicas=Replica;
    sentinelMaster(&F,Replica?ROLE_REPLICA_REPLY:ROLE_MASTER_REPLY,false,false);
    char Wire[200]="*1\r\n";
    if(Replica){
        sentinelReplicaRow(Wire+4,sizeof(Wire)-4,true,roleClientPeerInit(&F.Masters[0]->Peer)->Peer.Address.Port,"slave");
        sentinelSeed(&F,Wire,false,false);
    }else sentinelAddressSeed(&F,0);
    xredissentinelconfig K;
    sentinelConfig(&F,&K);
    xredisclient* C=NULL;
    if(Replica){
        appreplicapick Pick;
        xnetengine* Engine=roleClientPeerInit(&F.Seeds[0]->Peer)->Peer.Engine;
        redisPeerRequire(appReplicaPick(Engine,&K,8,(xredissentinelendpoint){0},xrtTimer()+5,&Pick,&C),"APP selects and verifies connected replica");
    }else{
        xredissentineldiscovery* Discovery=sentinelCreate(&F,&K);
        xredissentinelresult Result=sentinelDrive(Discovery);
        redisPeerRequire(Result.Status==XREDIS_SENTINEL_OK &&
            xredisSentinelDiscoveryTakeClient(Discovery,&C),"verified master discovery");
        xredisSentinelDiscoveryDestroy(Discovery);
    }
    xfuture* Ping=roleClientPing(C,5000);const xredisrequestresult* Reply=roleClientWait(Ping);redisPeerRequire(Reply->Status==XREDIS_REQUEST_OK,"native endpoint PING");xrtFutureDestroy(Ping);xfuture* Closed=xredisClientClosed(C);xredisClientClose(C);redisPeerRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"data endpoint close");xrtFutureDestroy(Closed);xredisClientRelease(C);sentinelFixtureStop(&F);printf("Discovered and ROLE-verified %s; transferred Client completes PING\n",Replica?"replica":"master");
}

int main(void)
{sentinelExample(false);sentinelExample(true);return 0;}
