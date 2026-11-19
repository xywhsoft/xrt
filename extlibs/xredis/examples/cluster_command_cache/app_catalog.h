#ifndef XREDIS_EXAMPLE_APP_CATALOG_H
#define XREDIS_EXAMPLE_APP_CATALOG_H
#include <xredis/cluster_client_sync.h>
#include <xredis/command_keys.h>
#include <xrt/time.h>

/* One APP control thread owns the pointer and any replacements. Commands
 * may share a retained immutable snapshot across their own admission calls. */
static inline bool appCatalogRefresh(xredisclusterclient* Client, uint16 Slot, int64 Timeout,
    const xrediscommandkeylimits* Limits, xrediscommandcatalog** Current)
{
    if (Timeout <= 0) return false;
    double Deadline = xrtTimer() + (double)Timeout / 1000.0;
    const xbytesview Command[] = {XRT_BYTES_LITERAL("COMMAND")};
    xredisclusterwait Native;
    if (xredisClusterClientCallSync(Client, Slot, Command, 1, Timeout, &Native) != XREDIS_ADMISSION_ACCEPTED)
        return false;
    const xredisclusterclientresult* Result = Native.Wait == XWAIT_OK ? xrtFutureValue(Native.Future) : NULL;
    xrediscommandcatalog* Next = NULL;
    bool Ready = Result && Result->Status == XREDIS_CLUSTER_CLIENT_COMPLETE &&
        Result->RouteObserved && Result->Route.Status == XREDIS_CLUSTER_ROUTE_COMPLETE &&
        Result->Route.RequestObserved && Result->Route.Query.Status == XREDIS_REQUEST_OK &&
        Result->Route.Query.Reply && xrtTimer() < Deadline &&
        xredisCommandCatalogCreate(Result->Route.Query.Reply, Limits, &Next) == XREDIS_KEYS_OK &&
        xrtTimer() < Deadline;
    xrtFutureDestroy(Native.Future);
    if (!Ready) {
        xredisCommandCatalogRelease(Next);
        return false;
    }
    xrediscommandcatalog* Previous = *Current;
    *Current = Next;
    xredisCommandCatalogRelease(Previous);
    return true;
}
#endif
