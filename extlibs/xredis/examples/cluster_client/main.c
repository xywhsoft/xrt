#include <xredis.h>
#include "replica_policy.h"
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
    uint16 Port=0;
    xredispool* Pool=NULL;
    redisPeerRequire(appReplicaPool(C,F->Base->Cache,1,5000,&Pool,&Port)==XREDIS_CLUSTER_POOL_READY &&
        Port==F->Base->Peers[1].Address.Port,"APP chooses an advertised replica");
    nnReady(Pool,1);
    const xbytesview Args[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("sample")};
    xfuture* Replica=NULL;
    redisPeerRequire(xredisSharedPoolCallAsync(Pool,Args,2,5000,&Replica)==XREDIS_ADMISSION_ACCEPTED &&
        xrtFutureWaitFor(Replica,5000)==XWAIT_OK,"send directly to the selected READONLY pool");
    const xredisrequestresult* Read=xrtFutureValue(Replica);
    redisPeerRequire(Read && Read->Status==XREDIS_REQUEST_OK && Read->Delivery==XREDIS_REPLIED,
        "native replica read result");
    xredisPoolRelease(Pool);
    cfStop(F,C);xrtFutureDestroy(Primary);xrtFutureDestroy(Replica);
    puts("Automatic metadata bootstrap, primary routing, APP replica selection and shared node cache close");return 0;
}
