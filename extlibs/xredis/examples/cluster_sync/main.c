#include <xredis/cluster_client_sync.h>
#include "../../tests/support/cluster_client_peer.h"
int main(void)
{
    rffixture* F=rfStart(3);xredisclusterclient* C=cfCreate(F,NULL);
    xbytesview A[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("managed\0route")};
    xredisclusterwait W;
    redisPeerRequire(xredisClusterClientCallSync(C,1,A,2,5000,&W)==XREDIS_ADMISSION_ACCEPTED,"sync admission");
    redisPeerRequire(W.Wait==XWAIT_OK,"native wait");const xredisclusterclientresult* R=xrtFutureValue(W.Future);
    redisPeerRequire(R && R->Status==XREDIS_CLUSTER_CLIENT_COMPLETE && R->RouteObserved && R->Route.Query.Status==XREDIS_REQUEST_OK,"sync route result");
    cfStop(F,C);xredisreplyvalue V;redisPeerRequire(xredisReplyGet(R->Route.Query.Reply,0,&V) && V.Data.Size==13 && !memcmp(V.Data.Data,"managed\0route",13),"retained binary reply");xrtFutureDestroy(W.Future);puts("[PASS] explicit-slot synchronous Cluster example");return 0;
}
