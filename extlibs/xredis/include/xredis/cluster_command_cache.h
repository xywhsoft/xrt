#ifndef XREDIS_CLUSTER_COMMAND_CACHE_H
#define XREDIS_CLUSTER_COMMAND_CACHE_H
#include <xredis/cluster_command.h>
#include <xredis/cluster_client_sync.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND_CACHE) && (!defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND) || !defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT_SYNC))
#error "Cluster command cache requires metadata routing and synchronous Cluster calls"
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND_CACHE)
typedef enum xredisclustercatalogstatus {
    XREDIS_CLUSTER_CATALOG_ERROR=-1,
    XREDIS_CLUSTER_CATALOG_READY=1,XREDIS_CLUSTER_CATALOG_ABSENT,XREDIS_CLUSTER_CATALOG_STALE,
    XREDIS_CLUSTER_CATALOG_BUSY,XREDIS_CLUSTER_CATALOG_CLOSED,XREDIS_CLUSTER_CATALOG_OVERLOADED,
    XREDIS_CLUSTER_CATALOG_WAIT_FAILED,XREDIS_CLUSTER_CATALOG_REQUEST_FAILED,XREDIS_CLUSTER_CATALOG_TIMEOUT,
    XREDIS_CLUSTER_CATALOG_MALFORMED,XREDIS_CLUSTER_CATALOG_LIMIT,XREDIS_CLUSTER_CATALOG_MEMORY
} xredisclustercatalogstatus;
XRT_EXTERN_C_BEGIN
/* Fetch the full COMMAND catalogue from the authorised primary route for Slot.
 * Use an origin whose command definitions represent this cluster. COMMAND uses
 * the shared client's managed ACL identity and reply limits. One synchronous
 * caller fetches/builds at a time. BUSY performs no I/O. Invoke from a control
 * thread outside both Engines. Timeout and its default match CallSync, with
 * one original deadline through catalogue validation and publication.
 * If admitted, Native receives the same owned Future and actual wait result
 * even on failure. Earlier refusals leave Native unchanged. Destroy that
 * Future separately. Limits are copied; NULL uses the usual catalogue defaults.
 * A failed refresh preserves the prior snapshot and its validity. Invalidating
 * during refresh prevents that reply from being published. Close seals publish.
 * Caller owns Client through return. Outputs must be disjoint from Client,
 * live map, current catalogue and Limits. */
XREDIS_API xredisclustercatalogstatus xredisClusterClientRefreshCommandCatalog(xredisclusterclient* Client,
    uint16 Slot,const xrediscommandkeylimits* Limits,int64 Timeout,xredisclusterwait* Native);
/* READY transfers an immutable reference. Other statuses leave output
 * unchanged. This snapshot can outlive refresh, Close and client destruction. */
XREDIS_API xredisclustercatalogstatus xredisClusterClientCommandCatalog(xredisclusterclient* Client,
    xrediscommandcatalog** Catalogue);
/* Retain the old storage while invalidating its availability. A fresh,
 * successfully committed refresh restores availability. */
XREDIS_API bool xredisClusterClientInvalidateCommandCatalog(xredisclusterclient* Client);
/* Route with one retained snapshot for the duration of admission. ABSENT or
 * STALE yields CATALOG_UNAVAILABLE without network work. A call that already
 * acquired a snapshot may proceed across concurrent replacement/invalidation.
 * The original entry deadline includes snapshot acquisition. */
XREDIS_API xredisclustercommandadmission xredisClusterClientCallCachedCommand(xredisclusterclient* Client,
    const xbytesview* Arguments,size_t Count,uint16 KeylessSlot,int64 Timeout,xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif
