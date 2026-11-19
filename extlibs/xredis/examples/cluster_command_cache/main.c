#include "app_catalog.h"
#include "../../tests/support/cluster_command_peer.h"

int main(void)
{
    rffixture* Fixture = qpStart();
    xredisclusterclient* Client = cfCreate(Fixture, NULL);
    xrediscommandcatalog* Catalogue = NULL;
    redisPeerRequire(appCatalogRefresh(Client, 1, 5000, NULL, &Catalogue),
        "authorized primary returned a valid command catalog");

    const xbytesview Args[] = {XRT_BYTES_LITERAL("GET"), XRT_BYTES_LITERAL("owned\0{22179}")};
    xrediscommandkeyresult Keys = {.Size = sizeof(Keys)};
    redisPeerRequire(xredisCommandCatalogEvaluate(Catalogue, Args, 2, NULL, 0, &Keys) == XREDIS_KEYS_OK,
        "APP resolves the binary key to its slot");
    xfuture* Business = NULL;
    redisPeerRequire(xredisClusterClientCall(Client, Keys.Slot, Args, 2, 5000, &Business) ==
        XREDIS_ADMISSION_ACCEPTED, "send on the explicit slot");
    xredisCommandCatalogRelease(Catalogue);
    qpValue(Business);
    cfStop(Fixture, Client);
    qpValue(Business);
    xrtFutureDestroy(Business);
    puts("[PASS] APP cached COMMAND and routed a binary-key GET through an explicit slot");
    return 0;
}
