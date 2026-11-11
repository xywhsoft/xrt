#include "../../tests/support/sentinel_peer.h"
int main(void)
{
    sentinelfixture F;sentinelFixtureStart(&F,0,false,XREDIS_INIT_RESP3);sentinelMaster(&F,ROLE_MASTER_REPLY,false,false);sentinelAddressSeed(&F,0);
    xredissentinelconfig K;sentinelConfig(&F,&K);xredissentineldiscovery* D=sentinelCreate(&F,&K);xredissentinelresult R=sentinelDrive(D);redisPeerRequire(R.Status==XREDIS_SENTINEL_OK,"verified master discovery");
    xredisclient* C=NULL;redisPeerRequire(xredisSentinelDiscoveryTakeClient(D,&C),"native client transfer");xredisSentinelDiscoveryDestroy(D);
    xfuture* Ping=roleClientPing(C,5000);const xredisrequestresult* Reply=roleClientWait(Ping);redisPeerRequire(Reply->Status==XREDIS_REQUEST_OK,"native master PING");xrtFutureDestroy(Ping);xfuture* Closed=xredisClientClosed(C);xredisClientClose(C);redisPeerRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"master close");xrtFutureDestroy(Closed);xredisClientRelease(C);sentinelFixtureStop(&F);puts("Discovered and ROLE-verified master; transferred native client completes PING");return 0;
}
