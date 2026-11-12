#include <xredis.h>
#include "../../tests/support/cluster_client_peer.h"
int main(void)
{
    rffixture* F=rfStart(3);xredisclusterclient* C=cfCreate(F,NULL);xfuture* Result=cfEcho(C,5000);const xredisclusterclientresult* V=cfWait(Result);
    if(V->Status!=XREDIS_CLUSTER_CLIENT_COMPLETE || V->Route.Query.Status!=XREDIS_REQUEST_OK)return 1;
    cfStop(F,C);xrtFutureDestroy(Result);puts("Automatic metadata bootstrap and binary route, one shared Nodes cache, explicit client/cache close");return 0;
}
