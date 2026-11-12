#include <xredis.h>
#include "../../tests/support/cluster_nodes_peer.h"
int main(void)
{
    nnfixture* F=nnStart();xredisclusternodesconfig C;nnConfig(F,&C);xredisclusternodes* Cache=nnCreate(F,&C);
    xredisclusteraddress A=nnAddress(F,0);xredispool* Pool=NULL;
    if(xredisClusterNodesGetPool(&A,5000,Cache,&Pool)!=XREDIS_CLUSTER_POOL_READY)return 1;
    nnReady(Pool,1);xfuture* Reply=nnEcho(Pool);xredisPoolRelease(Pool);nnClose(F);xredisClusterNodesRelease(Cache);nnStop(F);
    if(((const xredisrequestresult*)xrtFutureValue(Reply))->Status!=XREDIS_REQUEST_OK)return 2;
    xrtFutureDestroy(Reply);puts("Copied node identity -> coalesced maintained pool -> retained native reply, physical close");return 0;
}
