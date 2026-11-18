#include <xredis.h>
#include "../../tests/support/cluster_client_peer.h"
int main(void)
{
    rffixture* F=rfStart(3);
    xrtAtomic32Store(&F->Nodes[0].Replicas,2,XMEMORY_RELEASE);
    xredisclusterclient* C=cfCreate(F,NULL);
    xfuture* Primary=cfEcho(C,5000);
    const xredisclusterclientresult* Result=cfWait(Primary);
    redisPeerRequire(Result->Status==XREDIS_CLUSTER_CLIENT_COMPLETE && Result->Route.Query.Status==XREDIS_REQUEST_OK,
        "primary route after metadata bootstrap");
    xredisclusteraddress Address=nnAddress(F->Base,1);
    Address.Reason=XREDIS_CLUSTER_REPLICA_READ;
    xredispool* Pool=NULL;
    redisPeerRequire(xredisClusterNodesGetPool(&Address,5000,F->Base->Cache,&Pool)==XREDIS_CLUSTER_POOL_READY,
        "replica prewarm");
    nnReady(Pool,1);xredisPoolRelease(Pool);
    const xbytesview Args[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("sample")};
    xfuture* Replica=NULL;
    redisPeerRequire(xredisClusterClientCallReplica(C,1,Args,2,5000,&Replica)==XREDIS_ADMISSION_ACCEPTED,
        "stale-tolerant route admission");
    Result=cfWait(Replica);
    redisPeerRequire(Result->Status==XREDIS_CLUSTER_CLIENT_COMPLETE && Result->Route.Query.Status==XREDIS_REQUEST_OK &&
        Result->Route.AttemptAddress.Port==Address.Port && Result->Route.AttemptAddress.Reason==Address.Reason,
        "replica route after READONLY");
    cfStop(F,C);xrtFutureDestroy(Primary);xrtFutureDestroy(Replica);
    puts("Automatic metadata bootstrap, primary and replica routes, and shared node cache close");return 0;
}
