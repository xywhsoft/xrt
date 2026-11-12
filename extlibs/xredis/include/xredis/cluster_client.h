#ifndef XREDIS_CLUSTER_CLIENT_H
#define XREDIS_CLUSTER_CLIENT_H
#include <xredis/cluster_refresh.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT) && !defined(XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH)
#error "Cluster client requires metadata refresh and maintained nodes"
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT)
typedef struct xredisclusterclient xredisclusterclient;
typedef enum xredisclusterclientstatus {
    XREDIS_CLUSTER_CLIENT_COMPLETE,XREDIS_CLUSTER_CLIENT_TIMEOUT,
    XREDIS_CLUSTER_CLIENT_CANCELLED,XREDIS_CLUSTER_CLIENT_CLOSED,
    XREDIS_CLUSTER_CLIENT_MEMORY,XREDIS_CLUSTER_CLIENT_INTERNAL,XREDIS_CLUSTER_CLIENT_LIMIT,
    XREDIS_CLUSTER_CLIENT_RESOURCE
} xredisclusterclientstatus;
typedef struct xredisclusterclientconfig {
    size_t Size,MaxRequests,MaxCommandBytes,MaxStorageBytes,MaxSteps;
    int64 RefreshMin,RefreshRetryMax;
    xredisclusterrefreshconfig Refresh;xredisclusterrouteconfig Route;
} xredisclusterclientconfig;
/* COMPLETE is a terminal route, including server/uncertain transport failure.
 * Inspect Route.Status and Route.Query.Status/Delivery. Native and every Route
 * view borrow this result Future. Native can remain pending after a stop;
 * RouteObserved/Route never change afterwards. No outcome authorizes replay. */
typedef struct xredisclusterclientresult {
    size_t Size;xredisclusterclientstatus Status;uint64 Generation;
    double QueueSeconds,TotalSeconds;bool RouteObserved,RefreshObserved;
    xredisclusterrefreshstatus RefreshStatus;xredisclusterrouteresult Route;
    xfuture* Native;
} xredisclusterclientresult;
typedef struct xredisclusterclientready {
    xredisclusterclientstatus Status;uint64 Generation,Epoch;
} xredisclusterclientready;
typedef struct xredisclusterclientinfo {
    size_t Size;bool Closing,Closed,Dirty;
    size_t Requests,APIs,CommandBytes,StorageBytes,HeapEntries;
    uint64 Accepted,Completed,Rejected,Generation,Epoch,Refreshes,RefreshCompleted,RefreshFailures,Moved;
    xredisclusterrefreshstatus LastRefresh;xredisclusterclientstatus StopStatus;double NextRefresh;
} xredisclusterclientinfo;
XRT_EXTERN_C_BEGIN
/* Defaults:1024 active/reserved requests,16MiB encoded commands,64MiB request
 * controller reservations,64 request phases/turn plus one metadata phase,
 * 100ms..10s metadata refresh backoff.
 * Requests<=65536,byte caps<=1GiB,steps1..4096,delays1..60000ms.
 * Route.GetPool/Data/Tls must remain zero: exact transport comes from the query
 * origin. Route.Timeout is the default Call timeout; a Call supplies its own.
 * Node/native/Future limits remain independent. Snapshot costs are separate. */
XREDIS_API bool xredisClusterClientConfigInit(xredisclusterclientconfig* Config);
/* Copies explicit seeds through a private refresh coordinator; retains shared
 * Nodes and pins the progress Engine. Nodes may use another Engine. Failure
 * preserves output/Nodes/factory ownership. No policy/I/O inline. Once accepted,
 * one existing worker drives bootstrap, routes, watch notifications and timers.
 * Metadata transport failures retry with bounded backoff until Close. */
XREDIS_API xredisadmission xredisClusterClientCreate(xnetengine* Engine,uint64 Affinity,
    xredisclusternodes* Nodes,const xredisclusteraddress* Seeds,size_t Count,
    const xredisclusterclientconfig* Config,xredisclusterclient** Client);
XREDIS_API bool xredisClusterClientRetain(xredisclusterclient* Client);
XREDIS_API void xredisClusterClientRelease(xredisclusterclient* Client);
/* Thread-safe nonblocking admission, explicit slot/all-keys responsibility.
 * Timeout-1 uses Route.Timeout;0 is due;maximum24h. One entry deadline includes reservation/allocation,
 * queue/bootstrap and all native redirects. Warm admission copies inputs once
 * into its route; bootstrap queues add a cold copy. No accepted business replay
 * except validated matching-slot MOVED/ASK. Parent cancellation promptly stops
 * only this operation. Outputs change only on ACCEPTED; overload consumes none.
 * Caller-retained result/route/native storage leaves client quotas at logical
 * publication and stays caller-owned. Do not wait on this Engine's worker. */
XREDIS_API xredisadmission xredisClusterClientCall(xredisclusterclient* Client,
    uint16 Slot,const xbytesview* Arguments,size_t Count,int64 Timeout,xfuture** Future);
/* Ready resolves on the first valid map, or typed stop; metadata failures are
 * visible in Info while bootstrap retries. Acquired views are retained Maps.
 * MOVED from the current generation invalidates once per generation and starts
 * coalesced full-map refresh; ASK never changes the shared map. Old valid maps
 * remain usable during failed refresh. */
XREDIS_API xfuture* xredisClusterClientReady(xredisclusterclient* Client);
XREDIS_API xredisclusterstatus xredisClusterClientSnapshot(xredisclusterclient* Client,
    xredisclustermap** Map,bool* Stale);
XREDIS_API bool xredisClusterClientInvalidate(xredisclusterclient* Client);
XREDIS_API bool xredisClusterClientInfo(xredisclusterclient* Client,xredisclusterclientinfo* Info);
/* Nonblocking seal; stops this client's routes/refresh, retires its own watches,
 * posts/timer and Engine pin before Closed. Shared Nodes is never closed here.
 * Its owner separately closes Nodes and joins Engines for native physical FIFO
 * retirement/factory release. Pending native work can outlive parent results.
 * Close before final Release; keep code loaded through all result destruction. */
XREDIS_API bool xredisClusterClientClose(xredisclusterclient* Client);
XREDIS_API xfuture* xredisClusterClientClosed(xredisclusterclient* Client);
XRT_EXTERN_C_END
#endif
#endif
