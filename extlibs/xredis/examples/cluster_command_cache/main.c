#include <xredis/cluster_command_cache.h>
#include "../../tests/support/cluster_command_peer.h"
int main(void)
{
    rffixture* Fixture=qpStart();
    xredisclusterclient* Client=cfCreate(Fixture,NULL);
    xredisclusterwait Metadata;
    redisPeerRequire(xredisClusterClientRefreshCommandCatalog(Client,1,NULL,5000,&Metadata)==XREDIS_CLUSTER_CATALOG_READY,
        "authorized primary returned a valid command catalog");
    xrtFutureDestroy(Metadata.Future);
    const xbytesview Args[]={XRT_BYTES_LITERAL("GET"),XRT_BYTES_LITERAL("owned\0{22179}")};
    xfuture* Business=NULL;
    redisPeerRequire(xredisClusterClientCallCachedCommand(Client,Args,2,XREDIS_CLUSTER_COMMAND_NO_SLOT,5000,&Business)
        ==XREDIS_CLUSTER_COMMAND_ACCEPTED,"route using cached command catalog");
    qpValue(Business);
    cfStop(Fixture,Client);
    qpValue(Business);
    xrtFutureDestroy(Business);
    puts("[PASS] fetched and cached COMMAND, then routed a binary-key GET");
    return 0;
}
