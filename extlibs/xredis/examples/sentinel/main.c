#include "../../tests/support/sentinel_peer.h"
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
    xredissentinelconfig K;sentinelConfig(&F,&K);xredissentineldiscovery* D=Replica?sentinelReplicaCreate(&F,&K,8):sentinelCreate(&F,&K);xredissentinelresult R=sentinelDrive(D);redisPeerRequire(R.Status==XREDIS_SENTINEL_OK,"verified endpoint discovery");
    xredisclient* C=NULL;redisPeerRequire(xredisSentinelDiscoveryTakeClient(D,&C),"native client transfer");xredisSentinelDiscoveryDestroy(D);
    xfuture* Ping=roleClientPing(C,5000);const xredisrequestresult* Reply=roleClientWait(Ping);redisPeerRequire(Reply->Status==XREDIS_REQUEST_OK,"native endpoint PING");xrtFutureDestroy(Ping);xfuture* Closed=xredisClientClosed(C);xredisClientClose(C);redisPeerRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"data endpoint close");xrtFutureDestroy(Closed);xredisClientRelease(C);sentinelFixtureStop(&F);printf("Discovered and ROLE-verified %s; transferred Client completes PING\n",Replica?"replica":"master");
}

int main(void)
{sentinelExample(false);sentinelExample(true);return 0;}
