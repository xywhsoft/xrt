#include <xredis/cluster_command_sync.h>
#include "../../tests/support/cluster_command_peer.h"
int main(void)
{
    rffixture* F=qpStart();xredisclusterclient* C=cfCreate(F,NULL);xrediscommandcatalog* K=qpCatalogue(1,NULL);
    xbytesview A[]={XRT_BYTES_LITERAL("GET"),XRT_BYTES_LITERAL("owned\0{22179}")};xredisclusterwait W;
    redisPeerRequire(xredisClusterClientCallCommandSync(C,K,A,2,XREDIS_CLUSTER_COMMAND_NO_SLOT,5000,&W)==XREDIS_CLUSTER_COMMAND_ACCEPTED,"sync command admission");
    xredisCommandCatalogRelease(K);redisPeerRequire(W.Wait==XWAIT_OK,"sync command wait");cfStop(F,C);qpValue(W.Future);xrtFutureDestroy(W.Future);puts("[PASS] metadata-routed synchronous Cluster example");return 0;
}
